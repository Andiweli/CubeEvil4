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
 * is taken over by the platform (re4_mem1_init, RE4_MEM1_PLACEHOLDER). */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
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

static void reserveLowMemory(void)
{
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) {
        return;
    }
    unsigned long used[512][2];
    int n = 0;
    char line[512];
    while (n < 512 && fgets(line, sizeof(line), f)) {
        unsigned long a, b;
        if (sscanf(line, "%lx-%lx", &a, &b) == 2) {
            used[n][0] = a;
            used[n][1] = b;
            n++;
        }
    }
    fclose(f);
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

int main(int argc, char** argv)
{
    char exe[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len <= 0) {
        fprintf(stderr, "re4_host: cannot find its own path\n");
        return 1;
    }
    exe[len] = 0;
    reserveLowMemory();
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
    return hostMain(argc, argv);
}
