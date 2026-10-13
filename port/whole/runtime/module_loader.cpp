// REL module loader: every original REL module is a shared library
// libre4_<name>.so. Modules call functions of sibling modules through
// generated trampolines that end up in re4_resolve_import.
#include "re4_runtime.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

struct Loaded {
    const char* name;
    void* handle;
};

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

extern "C" const char* re4_module_name(unsigned id)
{
    for (const Re4ModuleEntry* e = kRe4Modules; e->name; e++) {
        if (e->id == id) {
            return e->name;
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
    snprintf(path, sizeof(path), "%s/libre4_%s.so", gModuleDir, name);
    // RTLD_LOCAL: each module keeps its own copies of the units the original
    // RELs duplicated (em_wrap.cpp, st2.cpp, ...), exactly as on the GameCube.
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        re4_log("module %s: %s", name, dlerror());
        return 0;
    }
    gLoaded[gLoadedCount].name = strdup(name);
    gLoaded[gLoadedCount].handle = handle;
    gLoadedCount++;
    return handle;
}

extern "C" void re4_module_unload(const char* name)
{
    int i = findLoaded(name);
    if (i < 0) {
        return;
    }
    dlclose(gLoaded[i].handle);
    free((void*) gLoaded[i].name);
    gLoaded[i] = gLoaded[--gLoadedCount];
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
