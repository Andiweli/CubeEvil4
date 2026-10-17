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

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#define LOW_START 0x00010000UL
#define LOW_END 0x83000000UL

#define MEM1_START 0x80000000UL

static int reserve(unsigned long from, unsigned long to)
{
    if (to <= from) {
        return 1;
    }
    void* p = mmap((void*) from, to - from, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == (void*) from) {
        return 1;
    }
    if (p != MAP_FAILED) {
        munmap(p, to - from);  /* the kernel treated the address as a hint */
    }
    return 0;
}

/* The mappings of the process, sorted by address (/proc/self/maps). Without
 * stdio: on 64 bit nothing may call malloc before reserveHighMemory, or its
 * brk heap would start above 4 GB. *stack: start of the main thread's stack. */
static int readMaps(unsigned long used[][2], int max, unsigned long* stack)
{
    static char text[256 << 10];
    int fd = open("/proc/self/maps", O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    size_t len = 0;
    ssize_t r;
    while (len < sizeof(text) - 1 && (r = read(fd, text + len, sizeof(text) - 1 - len)) > 0) {
        len += (size_t) r;
    }
    close(fd);
    text[len] = 0;
    int n = 0;
    for (char* line = text; *line && n < max;) {
        char* end = strchr(line, '\n');
        if (end) {
            *end = 0;
        }
        char* rest;
        unsigned long a = strtoul(line, &rest, 16);
        if (*rest == '-') {
            used[n][0] = a;
            used[n][1] = strtoul(rest + 1, 0, 16);
            if (stack && strstr(rest, "[stack]")) {
                *stack = a;
            }
            n++;
        }
        if (!end) {
            break;
        }
        line = end + 1;
    }
    return n;
}

static void reserveLowMemory(void)
{
    static unsigned long used[1024][2];
    int n = readMaps(used, 1024, 0);
    /* MEM1 first: the platform maps over this placeholder only when the
       whole range was free. */
    if (reserve(MEM1_START, LOW_END)) {
        setenv("RE4_MEM1_PLACEHOLDER", "1", 1);
    }
    unsigned long cursor = LOW_START;
    for (int i = 0; i < n && cursor < MEM1_START; i++) {
        if (used[i][1] <= cursor) {
            continue;
        }
        if (used[i][0] > cursor) {
            reserve(cursor, used[i][0] < MEM1_START ? used[i][0] : MEM1_START);
        }
        cursor = used[i][1];
    }
    reserve(cursor, MEM1_START);
}

#if __SIZEOF_POINTER__ == 8
#define HIGH_START 0x100000000UL

/* Every free range from 4 GB up to the main thread's stack. mmap without an
 * address hands out space below mmap_base only, which lies at least 128 MB
 * under the stack; 64 MB under the stack stay free for it to grow. Runs before
 * the first malloc: the brk heap would grow behind re4_host, above 4 GB; with
 * that range taken, malloc takes its memory with mmap, inside the window. */
static void reserveHighMemory(void)
{
    static unsigned long used[1024][2];
    unsigned long stack = 0;
    int n = readMaps(used, 1024, &stack);
    if (!stack) {
        fprintf(stderr, "re4_host: no [stack] in /proc/self/maps\n");
        return;
    }
    unsigned long top = stack - (64UL << 20);
    unsigned long cursor = HIGH_START;
    unsigned long failed = 0;
    for (int i = 0; i <= n && cursor < top; i++) {
        unsigned long next = i < n ? used[i][0] : top;
        if (i < n && used[i][1] <= cursor) {
            continue;
        }
        if (next > top) {
            next = top;
        }
        if (next > cursor && !reserve(cursor, next)) {
            fprintf(stderr, "re4_host: cannot reserve 0x%lx..0x%lx\n", cursor, next);
            failed++;
        }
        if (i < n) {
            cursor = used[i][1];
        }
    }
    if (failed) {
        fprintf(stderr, "re4_host: %lu ranges above 4 GB not reserved: memory there may reach the game\n",
                failed);
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
    void* probe = malloc(64);
    if ((unsigned long) probe >= HIGH_START) {
        fprintf(stderr, "re4_host: malloc returns %p, above 4 GB\n", probe);
        return 1;
    }
    free(probe);
#endif
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
