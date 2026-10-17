// Linux launcher for the whole-program port (32-bit host test build),
// libre4_host.so: the loader re4_host (host/loader.c) keeps the low address
// range free and calls re4_host_main.
//
//   re4_host                  run the game (re4_game_main)
//   re4_host --check-modules  load all REL module libraries and resolve every
//                             cross-module import, then exit
//   re4_host --self-test      call through a generated cross-module trampoline
//   re4_host --runtime-test   OSLink, DVD and yz2 checks without disc data
//                             (tests/runtime_tests.cpp)
//   re4_host --core-test      the embedding interface of the Android app
//                             without disc data (tests/core_tests.cpp)
//   re4_host --movie-test FILE YUV WAV FIELDS
//                             a movie of the data folder through the CRI
//                             middleware (port/tools/test_whole_movie.py)
//   re4_host --memcard-format FILE
//                             a formatted, empty memory card in FILE, made by
//                             the SDK's card library (tests/card_probe.cpp;
//                             the PC check boots with it)
//   re4_host --gci-export CARD DIR
//                             every file of the card file CARD as a .gci in
//                             GameCube byte order into DIR (as the app's menu
//                             exports them; runtime/gci.cpp)
//   re4_host --gci-import CARD FILE.gci [--replace]
//                             a .gci onto the card file CARD
//
// kill -USR1 <pid> prints a backtrace of the running context and of every OS
// thread's last wait (with RE4_WAIT_TRACE=1): the hang search.
//
// Module libraries are looked up in <directory of re4_host>/modules. The
// extracted disc files are read from --data DIR, else $RE4_DATA_DIR, else
// <directory of re4_host>/data (layout: port/whole/runtime/dvd.cpp).
#include "re4_card_files.h"
#include "re4_runtime.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int re4_runtime_tests();
int re4_core_tests();

// Where an address lies: library and offset (for addr2line), nearest symbol.
static void printAddress(const char* what, void* address)
{
    Dl_info info;
    if (address && dladdr(address, &info) && info.dli_fname) {
        fprintf(stderr, "[re4]   %s %p = %s+0x%lx (%s)\n", what, address, info.dli_fname,
                (unsigned long) ((char*) address - (char*) info.dli_fbase), info.dli_sname ? info.dli_sname : "?");
    } else {
        fprintf(stderr, "[re4]   %s %p\n", what, address);
    }
}

// Game memory around a register that points into MEM1: what the code that
// crashed was working on (r229: a draw list entry with a broken function
// pointer). Only mapped pages are read (mincore), inside the handler.
static bool pageMapped(unsigned long a)
{
    unsigned char v;
    unsigned long page = (unsigned long) sysconf(_SC_PAGESIZE);
    return mincore((void*) (a & ~(page - 1)), 1, &v) == 0;
}

static void dumpAround(const char* reg, unsigned long v)
{
    if (v < RE4_MEM1_BASE || v >= (unsigned long) RE4_MEM1_BASE + RE4_MEM1_SIZE) {
        return;
    }
    unsigned long from = (v & ~0xFUL) - 0x40, to = (v & ~0xFUL) + 0x20;
    if (from < RE4_MEM1_BASE || to > (unsigned long) RE4_MEM1_BASE + RE4_MEM1_SIZE || !pageMapped(from) ||
        !pageMapped(to - 1)) {
        return;
    }
    fprintf(stderr, "[re4]   memory around %s 0x%08lx (host order):\n", reg, v);
    for (unsigned long a = from; a < to; a += 16) {
        const unsigned* w = (const unsigned*) a;
        fprintf(stderr, "[re4]     %08lx: %08x %08x %08x %08x%s\n", a, w[0], w[1], w[2], w[3],
                v >= a && v < a + 16 ? "  <-" : "");
    }
}

static void crashHandler(int sig, siginfo_t* si, void* context)
{
    void* frames[64];
    int n = backtrace(frames, 64);
    fprintf(stderr, "[re4] fatal signal %d (%s), fault address %p, backtrace:\n", sig, strsignal(sig),
            si ? si->si_addr : 0);
    // the faulting instruction (backtrace() does not unwind through the signal frame on ARM)
    const ucontext_t* uc = (const ucontext_t*) context;
#if defined(__arm__)
    printAddress("pc", (void*) uc->uc_mcontext.arm_pc);
    printAddress("lr", (void*) uc->uc_mcontext.arm_lr);
    const unsigned long* r = &uc->uc_mcontext.arm_r0;  // r0..r10, fp, ip, sp in a row
    static const char* const kNames[] = { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
                                          "r8", "r9", "r10", "fp", "ip", "sp" };
    fprintf(stderr, "[re4]   registers:");
    for (int i = 0; i < 14; i++) {
        fprintf(stderr, " %s=%08lx", kNames[i], r[i]);
    }
    fprintf(stderr, "\n");
    for (int i = 0; i < 14; i++) {
        dumpAround(kNames[i], r[i]);
    }
#elif defined(__i386__)
    printAddress("pc", (void*) uc->uc_mcontext.gregs[REG_EIP]);
    static const struct {
        const char* name;
        int reg;
    } kRegs[] = { { "eax", REG_EAX }, { "ebx", REG_EBX }, { "ecx", REG_ECX }, { "edx", REG_EDX },
                  { "esi", REG_ESI }, { "edi", REG_EDI }, { "ebp", REG_EBP }, { "esp", REG_ESP } };
    fprintf(stderr, "[re4]   registers:");
    for (const auto& k : kRegs) {
        fprintf(stderr, " %s=%08lx", k.name, (unsigned long) (unsigned) uc->uc_mcontext.gregs[k.reg]);
    }
    fprintf(stderr, "\n");
    for (const auto& k : kRegs) {
        dumpAround(k.name, (unsigned long) (unsigned) uc->uc_mcontext.gregs[k.reg]);
    }
#elif defined(__x86_64__)
    printAddress("pc", (void*) uc->uc_mcontext.gregs[REG_RIP]);
#else
    (void) uc;
#endif
    backtrace_symbols_fd(frames, n, 2);
    _exit(128 + sig);
}

// SIGUSR1 (kill -USR1 <pid>): backtrace of the running context without
// stopping, to find out where the game waits or loops.
static void traceHandler(int sig)
{
    void* frames[64];
    int n = backtrace(frames, 64);
    fprintf(stderr, "[re4] signal %d (%s), backtrace of the running context:\n", sig, strsignal(sig));
    backtrace_symbols_fd(frames, n, 2);
    re4_os_dump_threads(2);
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

extern "C" int re4probe_movie_play(const char* fname, const char* yuvPath, const char* wavPath, int maxFields);
extern "C" int re4probe_card_format(const char* path);
extern "C" void re4_memcard_open(const char* path);
extern "C" void re4_memcard_flush(int force);

// --gci-export / --gci-import: the card functions of the app's menu on a card file.
static int gciTool(int argc, char** argv)
{
    re4_memcard_open(argv[2]);
    char note[256] = "";
    if (strcmp(argv[1], "--gci-export") == 0) {
        static Re4CardFile files[127];
        int n = re4_card_list(files, 127);
        if (n < 0) {
            fprintf(stderr, "gci-export: %s: no formatted card\n", argv[2]);
            return 1;
        }
        int failed = 0;
        static unsigned char out[RE4_GCI_HEADER + 251 * RE4_CARD_BLOCK];
        for (int i = 0; i < n; i++) {
            char name[64], path[PATH_MAX];
            re4_gci_file_name(files[i].index, name, sizeof(name));
            int bytes = re4_gci_export(files[i].index, out, sizeof(out), note, sizeof(note));
            if (bytes < 0) {
                printf("gci-export: %s: FAILED (%s)\n", files[i].name, note);
                failed++;
                continue;
            }
            snprintf(path, sizeof(path), "%s/%s", argv[3], name);
            FILE* f = fopen(path, "wb");
            if (!f || fwrite(out, 1, (size_t) bytes, f) != (size_t) bytes) {
                printf("gci-export: %s: cannot write %s\n", files[i].name, path);
                failed++;
            } else {
                printf("gci-export: %s -> %s (%d bytes; %s)\n", files[i].name, path, bytes, note);
            }
            if (f) {
                fclose(f);
            }
        }
        return failed ? 1 : 0;
    }
    FILE* f = fopen(argv[3], "rb");
    if (!f) {
        fprintf(stderr, "gci-import: %s cannot be opened\n", argv[3]);
        return 1;
    }
    static unsigned char in[RE4_GCI_HEADER + 252 * RE4_CARD_BLOCK];
    size_t size = fread(in, 1, sizeof(in), f);
    fclose(f);
    bool replace = argc > 4 && strcmp(argv[4], "--replace") == 0;
    int r = re4_gci_import(in, (unsigned) size, replace, note, sizeof(note));
    printf("gci-import: %s: %s (%d)\n", argv[3], r == RE4_CARD_OK ? "ok" : note, r);
    re4_memcard_open(0);  // synced and closed
    return r == RE4_CARD_OK ? 0 : 1;
}

extern "C" __attribute__((visibility("default"))) int re4_host_main(int argc, char** argv)
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
    struct sigaction crash;
    memset(&crash, 0, sizeof(crash));
    crash.sa_sigaction = crashHandler;
    crash.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &crash, 0);
    sigaction(SIGBUS, &crash, 0);
    sigaction(SIGILL, &crash, 0);
    sigaction(SIGFPE, &crash, 0);
    sigaction(SIGABRT, &crash, 0);
    signal(SIGUSR1, traceHandler);

    // _exit: the game never ends on the GameCube, so its static destructors
    // (EventMgr, ...) were never meant to run.
    if (argc > 1 && strcmp(argv[1], "--check-modules") == 0) {
        _exit(checkModules());
    }
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        _exit(selfTest());
    }
    if (argc > 1 && strcmp(argv[1], "--runtime-test") == 0) {
        _exit(re4_runtime_tests() ? 1 : 0);
    }
    if (argc > 1 && strcmp(argv[1], "--core-test") == 0) {
        _exit(re4_core_tests() ? 1 : 0);
    }
    if (argc > 5 && strcmp(argv[1], "--movie-test") == 0) {
        // FILE YUV WAV FIELDS: a movie of the data folder through the CRI
        // middleware (tests/movie_probe.cpp, port/tools/test_whole_movie.py)
        int frames = re4probe_movie_play(argv[2], argv[3], argv[4], atoi(argv[5]));
        re4_log("movie-test: %d frames", frames);
        _exit(frames > 0 ? 0 : 1);
    }
    if (argc > 2 && strcmp(argv[1], "--memcard-format") == 0) {
        _exit(re4probe_card_format(argv[2]) ? 0 : 1);
    }
    if (argc > 3 && (strcmp(argv[1], "--gci-export") == 0 || strcmp(argv[1], "--gci-import") == 0)) {
        int r = gciTool(argc, argv);
        fflush(stdout);
        _exit(r);
    }
    // The game tests bit 31 of addresses of its own data (as on the GameCube).
    re4_check_game_address("libre4_main", (const void*) &re4_game_main);
    re4_log("starting re4_game_main");
    // The game runs as the boot thread of the Dolphin OS thread emulation.
    int ret = re4_os_run(re4_game_main);
    re4_log("re4_game_main returned %d", ret);
    _exit(ret);
}
