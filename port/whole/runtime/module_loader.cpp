// REL module loader: every original REL module is a shared library
// libre4_<name>.so. Modules call functions of sibling modules through
// generated trampolines that end up in re4_resolve_import.
#include "re4_runtime.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__ANDROID__) && __SIZEOF_POINTER__ != 4
#include <android/dlext.h>
#include <sys/mman.h>
#endif

namespace {

struct Loaded {
    const char* name;
    void* handle;
    void* range;          // Android, 64 bit: the reserved range of the window it lies in
    unsigned long rangeSize;
};

#if defined(__ANDROID__) && __SIZEOF_POINTER__ != 4
// 64 bit on Android (milestone 12): nothing places the libraries below 4 GB
// as the Linux loader does (host/loader.c). Each module gets a reserved range
// of the window (re4_window_map) and the linker loads it there. The largest
// module spans 2 MB (x86_64 -O0, libre4_t_esp.so); a module that does not fit
// is tried once more in a range four times as large.
const unsigned long kModuleRange = 8ul << 20;

void* openInWindow(const char* path, void** range, unsigned long* rangeSize)
{
    for (unsigned long size = kModuleRange; size <= 4 * kModuleRange; size *= 4) {
        void* r = re4_window_map(size, PROT_NONE);
        if (!r) {
            return 0;
        }
        android_dlextinfo info;
        memset(&info, 0, sizeof(info));
        info.flags = ANDROID_DLEXT_RESERVED_ADDRESS;
        info.reserved_addr = r;
        info.reserved_size = size;
        if (void* handle = android_dlopen_ext(path, RTLD_NOW | RTLD_LOCAL, &info)) {
            *range = r;
            *rangeSize = size;
            return handle;
        }
        munmap(r, size);
    }
    return 0;
}
#endif

char gModuleDir[1024] = ".";
Loaded gLoaded[128];
int gLoadedCount;

int findLoaded(const char* name)
{
    for (int i = 0; i < gLoadedCount; i++) {
        if (strcmp(gLoaded[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

}  // namespace

extern "C" void re4_set_module_dir(const char* dir)
{
    snprintf(gModuleDir, sizeof(gModuleDir), "%s", dir);
}

// Modules added at run time (re4_host --runtime-test registers its test REL).
Re4ModuleEntry gExtraModules[8];
int gExtraCount;

extern "C" void re4_module_register(unsigned id, const char* name)
{
    if (gExtraCount < (int) (sizeof(gExtraModules) / sizeof(gExtraModules[0]))) {
        gExtraModules[gExtraCount].id = id;
        gExtraModules[gExtraCount].name = name;
        gExtraCount++;
    }
}

extern "C" const char* re4_module_name(unsigned id)
{
    for (const Re4ModuleEntry* e = kRe4Modules; e->name; e++) {
        if (e->id == id) {
            return e->name;
        }
    }
    for (int i = 0; i < gExtraCount; i++) {
        if (gExtraModules[i].id == id) {
            return gExtraModules[i].name;
        }
    }
    return 0;
}

extern "C" void* re4_module_load(const char* name)
{
    int i = findLoaded(name);
    if (i >= 0) {
        return gLoaded[i].handle;
    }
    if (gLoadedCount == (int) (sizeof(gLoaded) / sizeof(gLoaded[0]))) {
        re4_log("module %s: too many modules loaded", name);
        return 0;
    }
    char path[1200];
    if (gModuleDir[0]) {
        snprintf(path, sizeof(path), "%s/libre4_%s.so", gModuleDir, name);
    } else {
        // Android: libraries stay uncompressed in the APK; the linker finds
        // them by name in the app's library path.
        snprintf(path, sizeof(path), "libre4_%s.so", name);
    }
    // RTLD_LOCAL: each module keeps its own copies of the units the original
    // RELs duplicated (em_wrap.cpp, st2.cpp, ...), exactly as on the GameCube.
    void* range = 0;
    unsigned long rangeSize = 0;
#if defined(__ANDROID__) && __SIZEOF_POINTER__ != 4
    void* handle = openInWindow(path, &range, &rangeSize);
#else
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    if (!handle) {
        re4_log("module %s: %s", name, dlerror());
        return 0;
    }
    if (void* prolog = dlsym(handle, "_prolog")) {  // REL modules (module_crt.cpp)
        re4_check_game_address(path, prolog);
    }
    gLoaded[gLoadedCount].name = strdup(name);
    gLoaded[gLoadedCount].handle = handle;
    gLoaded[gLoadedCount].range = range;
    gLoaded[gLoadedCount].rangeSize = rangeSize;
    gLoadedCount++;
    return handle;
}

extern "C" int re4_module_is_loaded(const char* name)
{
    return findLoaded(name) >= 0;
}

extern "C" void re4_module_unload(const char* name)
{
    int i = findLoaded(name);
    if (i < 0) {
        return;
    }
    dlclose(gLoaded[i].handle);
#if defined(__ANDROID__) && __SIZEOF_POINTER__ != 4
    // the linker leaves the range mapped (or reserved): back to the window
    if (gLoaded[i].range) {
        munmap(gLoaded[i].range, gLoaded[i].rangeSize);
    }
#endif
    free((void*) gLoaded[i].name);
    // Keep load order: the newest module wins in re4_try_resolve_import.
    memmove(&gLoaded[i], &gLoaded[i + 1], (gLoadedCount - i - 1) * sizeof(gLoaded[0]));
    gLoadedCount--;
    // Trampolines of the other modules may still point into the unloaded one;
    // they resolve again on their next call.
    for (int k = 0; k < gLoadedCount; k++) {
        char fn[160];
        snprintf(fn, sizeof(fn), "re4_module_reset_imports_%s", gLoaded[k].name);
        if (void (*reset)() = (void (*)()) dlsym(gLoaded[k].handle, fn)) {
            reset();
        }
    }
}

extern "C" void* re4_try_resolve_import(const char* symbol)
{
    // Newest module first: a stage module that was just linked provides the
    // rooms its siblings register.
    for (int i = gLoadedCount - 1; i >= 0; i--) {
        if (void* p = dlsym(gLoaded[i].handle, symbol)) {
            return p;
        }
    }
    return 0;
}

extern "C" void* re4_resolve_import(const char* importer, const char* symbol)
{
    void* p = re4_try_resolve_import(symbol);
    if (!p) {
        re4_log("module %s calls %s, but no loaded module provides it", importer, symbol);
        abort();
    }
    return p;
}
