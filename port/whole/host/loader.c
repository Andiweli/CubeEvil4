/* re4_host: Linux loader of the whole-program port.
 *
 * The game tells its own data from offsets and table links by bit 31 of an
 * address (the GameCube keeps everything at 0x80000000 and above): ordering
 * tables (libgpu.cpp ClearOTagR/DrawOTag) store links with bit 31 cleared. On
 * Android the libraries of a 32-bit app load at 0xC0000000 and above
 * (measured on the Retroid, port/docs/whole-program-status.md), and so do
 * those of i686 Linux. qemu-arm loads them at 0x40000000. This loader makes
 * every Linux target look like Android: it fills each free gap below
 * 0x83000000 with an inaccessible reservation, then loads the launcher library
 * libre4_host.so (host/main.cpp) and with it the game libraries, which can
 * only land above. The MEM1 part of the reservation (0x80000000-0x83000000)
 * is taken over by the platform (re4_mem1_init, RE4_MEM1_PLACEHOLDER).
 *
 * 64 bit (milestone 12, way A): the game keeps its addresses in 32 bits, so
 * everything it reaches has to lie below 4 GB as well. The loader also fills
 * every free range above 4 GB; the libraries, thread stacks and malloc's large
 * blocks then land in the window 0x83000000..0xFFFFFFFF (the libraries at its
 * top, re4_window_map takes stacks from its bottom). The launcher runs on a
 * thread of its own: the stack of the main thread lies far above 4 GB. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "low_memory.h"

#if __SIZEOF_POINTER__ == 8
#define HIGH_START 0x100000000UL

/* A free range, or as much of it as can be had: a range with a mapping the
 * process cannot see (qemu-user keeps its own memory out of the guest's
 * /proc/self/maps) is split until the pieces around it fit. Returns the number
 * of pages left out. */
static unsigned long gSplitBudget = 1UL << 16;

static unsigned long reserveSplit(unsigned long from, unsigned long to, int depth)
{
    if (to <= from || reserve(from, to)) {
        return 0;
    }
    unsigned long page = (unsigned long) sysconf(_SC_PAGESIZE);
    if (to - from <= page || depth > 40 || !gSplitBudget) {
        return (to - from) / page;
    }
    gSplitBudget--;
    unsigned long mid = from + (((to - from) / 2) & ~(page - 1));
    if (mid <= from) {
        mid = from + page;
    }
    return reserveSplit(from, mid, depth + 1) + reserveSplit(mid, to, depth + 1);
}

/* Every free range above 4 GB, up to the end of the user address space. mmap
 * without an address then has only the window left (on a native kernel it
 * hands out space below mmap_base, at least 128 MB under the stack; 64 MB
 * under the stack stay free for it to grow). Runs before the first malloc:
 * the brk heap would grow behind re4_host, above 4 GB; with that range taken,
 * malloc takes its memory with mmap, inside the window. Under qemu-user
 * (aarch64 in the cloud) the stack is the lowest mapping above 4 GB and the
 * C library, the program and a brk area lie above it: the ranges up to the
 * top count as well (malloc stays above 4 GB there, see main). */
/* The end of the user address space (TASK_SIZE: 47 bit less a page on
 * x86_64, 39 to 48 bit on arm64): the highest of these ends below which a
 * page can be mapped or is in use, and at least the end of the highest
 * mapping (qemu-user keeps the guest's program near the host's top). */
static unsigned long userTop(unsigned long used[][2], int n)
{
    static const int bits[] = {48, 47, 42, 39};
    unsigned long page = (unsigned long) sysconf(_SC_PAGESIZE);
    unsigned long top = 1UL << 39;
    for (unsigned t = 0; t < sizeof(bits) / sizeof(bits[0]); t++) {
        unsigned long end = (1UL << bits[t]) - page;
        void* want = (void*) (end - page);
        void* p = mmap(want, page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        if (p == want || (p == MAP_FAILED && errno == EEXIST)) {
            if (p == want) {
                munmap(p, page);
            }
            top = end;
            break;
        }
        if (p != MAP_FAILED) {
            munmap(p, page);
        }
    }
    for (int i = 0; i < n; i++) {
        if (used[i][1] > top && used[i][1] < (1UL << 56)) {
            top = used[i][1];
        }
    }
    return top;
}

static void reserveHighMemory(void)
{
    static unsigned long used[1024][2];
    unsigned long stack = 0;
    int n = readMaps(used, 1024, &stack);
    if (!stack) {
        fprintf(stderr, "re4_host: no [stack] in /proc/self/maps\n");
        return;
    }
    unsigned long top = userTop(used, n);
    /* a native kernel keeps the stack near the top and lets it grow down;
       qemu-user maps a fixed one far below the top */
    unsigned long keep_from = stack - (64UL << 20), keep_to = stack;
    if (top - stack > (1UL << 32)) {
        keep_from = keep_to = 0;
    }
    unsigned long cursor = HIGH_START;
    unsigned long left = 0;
    for (int i = 0; i <= n && cursor < top; i++) {
        unsigned long next = top;
        if (i < n) {
            if (used[i][1] <= cursor) {
                continue;
            }
            if (used[i][0] < top) {
                next = used[i][0];
            }
        }
        if (next > cursor) {
            /* leave room for the stack to grow */
            if (cursor < keep_to && next > keep_from) {
                left += reserveSplit(cursor, keep_from > cursor ? keep_from : cursor, 0);
                if (next > keep_to) {
                    left += reserveSplit(keep_to, next, 0);
                }
            } else {
                left += reserveSplit(cursor, next, 0);
            }
        }
        if (i < n) {
            cursor = used[i][1];
        }
    }
    if (left) {
        fprintf(stderr, "re4_host: %lu pages above 4 GB not reserved: memory there may reach the game\n", left);
    }
    /* one malloc arena: each further one takes 64 MB of the window */
    mallopt(M_ARENA_MAX, 1);
}

struct HostCall {
    int (*main)(int, char**);
    int argc;
    char** argv;
    int result;
};

static void* hostThread(void* arg)
{
    struct HostCall* call = (struct HostCall*) arg;
    call->result = call->main(call->argc, call->argv);
    return 0;
}

/* re4_host_main on a thread whose stack lies below 4 GB */
static int runLow(int (*hostMain)(int, char**), int argc, char** argv)
{
    struct HostCall call = {hostMain, argc, argv, 1};
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 64UL << 20);
    pthread_t thread;
    int err = pthread_create(&thread, &attr, hostThread, &call);
    pthread_attr_destroy(&attr);
    if (err != 0) {
        fprintf(stderr, "re4_host: cannot create the launcher thread (%d)\n", err);
        return 1;
    }
    /* signals from outside (SIGUSR1: backtrace of the running context) go to
       the launcher thread */
    sigset_t all;
    sigfillset(&all);
    pthread_sigmask(SIG_BLOCK, &all, 0);
    pthread_join(thread, 0);
    return call.result;
}
#endif

int main(int argc, char** argv)
{
    char exe[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len <= 0) {
        fprintf(stderr, "re4_host: cannot find its own path\n");
        return 1;
    }
    exe[len] = 0;
#if __SIZEOF_POINTER__ == 8
    reserveHighMemory();  /* first: reserveLowMemory uses malloc (setenv) */
#endif
    reserveLowMemory();
#if __SIZEOF_POINTER__ == 8
    /* malloc below 4 GB like everything else here. Under qemu-user (aarch64
       in the cloud) the C library's heap lies in qemu's brk area above 4 GB and
       grows there: as on Android, where malloc hands out memory above 4 GB as
       well. The game never gets such memory (re4_check_window stops at the
       places that hand memory to it); the run goes on with a note. */
    void* probe = malloc(64);
    if ((unsigned long) probe >= HIGH_START) {
        fprintf(stderr, "re4_host: malloc returns %p, above 4 GB (as on Android)\n", probe);
    }
    free(probe);
#endif
    if (getenv("RE4_LOADER_MAPS")) {
        /* the address space after the reservations (where qemu or a kernel
           puts things the loader did not expect) */
        FILE* maps = fopen("/proc/self/maps", "r");
        char line[512];
        while (maps && fgets(line, sizeof(line), maps)) {
            fputs(line, stderr);
        }
        if (maps) {
            fclose(maps);
        }
    }
    char lib[PATH_MAX + 32];
    snprintf(lib, sizeof(lib), "%s/libre4_host.so", dirname(exe));
    void* handle = dlopen(lib, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "re4_host: %s\n", dlerror());
        return 1;
    }
    int (*hostMain)(int, char**) = (int (*)(int, char**)) dlsym(handle, "re4_host_main");
    if (!hostMain) {
        fprintf(stderr, "re4_host: re4_host_main missing in %s\n", lib);
        return 1;
    }
#if __SIZEOF_POINTER__ == 8
    return runLow(hostMain, argc, argv);
#else
    return hostMain(argc, argv);
#endif
}
