/* The address ranges below 0x83000000 held free (host/loader.c, the Linux
 * loader of re4_host; host/coreproc.cpp, the core process): the game tells
 * its own data from offsets by bit 31 of an address, so its libraries have to
 * land above 0x80000000 as on Android. Every free gap below 0x83000000 gets an
 * inaccessible reservation; MEM1 (0x80000000-0x83000000) first, which the
 * platform then maps over (re4_mem1_init, RE4_MEM1_PLACEHOLDER). Without stdio
 * and malloc (on 64 bit nothing may call malloc before the high ranges are
 * reserved). Static functions for C and C++. */
#ifndef RE4_LOW_MEMORY_H
#define RE4_LOW_MEMORY_H

#include <fcntl.h>
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

#endif
