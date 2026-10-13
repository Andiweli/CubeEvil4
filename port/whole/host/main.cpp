// Linux launcher for the whole-program port (32-bit host test build).
//
//   re4_host                  run the game (re4_game_main)
//   re4_host --check-modules  load all REL module libraries and resolve every
//                             cross-module import, then exit
//   re4_host --self-test      call through a generated cross-module trampoline
//
// Module libraries are looked up in <directory of re4_host>/modules. The
// extracted disc files are read from --data DIR, else $RE4_DATA_DIR, else
// <directory of re4_host>/data (layout: port/whole/runtime/dvd.cpp).
#include "re4_runtime.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void crashHandler(int sig)
{
    void* frames[64];
    int n = backtrace(frames, 64);
    fprintf(stderr, "[re4] fatal signal %d (%s), backtrace:\n", sig, strsignal(sig));
    backtrace_symbols_fd(frames, n, 2);
    _exit(128 + sig);
}

static int checkModules()
{
    int failed = 0;
    int count = 0;
    for (const Re4ModuleEntry* e = kRe4Modules; e->name; e++) {
        if (!re4_module_load(e->name)) {
            failed++;
        }
        count++;
    }
    int unresolved = 0;
    for (const Re4ModuleEntry* e = kRe4Modules; e->name; e++) {
        char fn[128];
        snprintf(fn, sizeof(fn), "re4_module_check_imports_%s", e->name);
        void* handle = re4_module_load(e->name);
        if (!handle) {
            continue;
        }
        int (*check)() = (int (*)()) dlsym(handle, fn);
        if (!check) {
            re4_log("module %s: %s missing", e->name, fn);
            unresolved++;
            continue;
        }
        unresolved += check();
    }
    re4_log("check-modules: %d modules, %d failed to load, %d unresolved imports, %d stubs",
            count, failed, unresolved, re4_stub_count());
    return failed || unresolved ? 1 : 0;
}

// Loads two test modules; the caller reaches the target only through generated
// cross-module trampolines, with arguments in registers and on the stack.
static int selfTest()
{
    if (!re4_module_load("re4test_target") || !re4_module_load("re4test_caller")) {
        return 1;
    }
    int (*run)() = (int (*)()) dlsym(re4_module_load("re4test_caller"), "re4test_run");
    int result = run ? run() : 4;
    re4_log("self-test: trampolines %s (code %d)", result == 0 ? "ok" : "FAILED", result);
    return result;
}

int main(int argc, char** argv)
{
    char exe[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len > 0) {
        exe[len] = 0;
        char dir[PATH_MAX + 16];
        const char* exeDir = dirname(exe);
        snprintf(dir, sizeof(dir), "%s/modules", exeDir);
        re4_set_module_dir(dir);
        snprintf(dir, sizeof(dir), "%s/data", exeDir);
        re4_set_data_dir(dir);
    }
    if (const char* env = getenv("RE4_DATA_DIR")) {
        re4_set_data_dir(env);
    }
    if (argc > 2 && strcmp(argv[1], "--data") == 0) {
        re4_set_data_dir(argv[2]);
        argc -= 2;
        argv += 2;
    }
    signal(SIGSEGV, crashHandler);
    signal(SIGBUS, crashHandler);
    signal(SIGILL, crashHandler);
    signal(SIGFPE, crashHandler);
    signal(SIGABRT, crashHandler);

    // _exit: the game never ends on the GameCube, so its static destructors
    // (EventMgr, ...) were never meant to run.
    if (argc > 1 && strcmp(argv[1], "--check-modules") == 0) {
        _exit(checkModules());
    }
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        _exit(selfTest());
    }
    re4_log("starting re4_game_main");
    int ret = re4_game_main();
    re4_log("re4_game_main returned %d", ret);
    _exit(ret);
}
