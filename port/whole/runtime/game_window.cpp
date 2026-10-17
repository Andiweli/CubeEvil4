// The address window of the game on LP64 (milestone 12, way A: native 64 bit).
//
// The game keeps addresses in 32 bits (u32 fields, (u32) casts, 4-byte
// pointers of the fixed-form structures) and tells its own data from offsets by
// bit 31 (see re4_check_game_address). On ILP32 every address qualifies. On
// LP64 whatever the game can reach has to lie in 0x80000000..0xFFFFFFFF: MEM1
// at the start (mem1.cpp), above it the libraries, the stacks the game code
// runs on and memory the platform hands to the game. re4_window_map takes
// address space from 0x83000000 upwards; the Linux loader (host/loader.c)
// leaves only this window free, so the libraries land at its top.
#include "re4_runtime.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#if __SIZEOF_POINTER__ != 4
namespace {

const unsigned long kWindowStart = RE4_WINDOW_START;
const unsigned long kWindowEnd = RE4_WINDOW_END;

pthread_mutex_t gWindowLock = PTHREAD_MUTEX_INITIALIZER;
unsigned long gCursor = kWindowStart;

// The first free range of `size` bytes in [from, kWindowEnd), 0 if none
// (from /proc/self/maps, sorted by address).
unsigned long findFree(unsigned long from, unsigned long size)
{
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) {
        return 0;
    }
    unsigned long at = from;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        unsigned long a, b;
        if (sscanf(line, "%lx-%lx", &a, &b) != 2 || b <= at) {
            continue;
        }
        if (a >= at + size) {
            break;
        }
        at = b;  // overlaps: the range starts after this mapping at the earliest
        if (at + size > kWindowEnd) {
            break;
        }
    }
    fclose(f);
    return at + size <= kWindowEnd ? at : 0;
}

}  // namespace
#endif

extern "C" void* re4_window_map(unsigned long size, int prot)
{
    const unsigned long page = (unsigned long) sysconf(_SC_PAGESIZE);
    size = (size + page - 1) & ~(page - 1);
#if __SIZEOF_POINTER__ == 4
    void* p = mmap(0, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? 0 : p;
#else
    pthread_mutex_lock(&gWindowLock);
    void* result = 0;
    bool wrapped = false;
    for (int tries = 0; tries < 64 && !result; tries++) {
        unsigned long at = findFree(gCursor, size);
        if (!at) {
            if (wrapped) {
                break;
            }
            // freed stacks below the cursor
            wrapped = true;
            gCursor = kWindowStart;
            continue;
        }
        void* want = (void*) at;
        void* p = mmap(want, size, prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (p == want) {
            result = p;
            gCursor = at + size;
        } else {
            // taken in the meantime (another thread), or an old kernel that
            // treats the address as a hint
            if (p != MAP_FAILED) {
                munmap(p, size);
            }
            gCursor = at + page;
        }
    }
    pthread_mutex_unlock(&gWindowLock);
    if (!result) {
        re4_log("window: no room for %lu KB below 4 GB", size >> 10);
    }
    return result;
#endif
}

extern "C" int re4_in_window(const void* p)
{
#if __SIZEOF_POINTER__ == 4
    (void) p;
    return 1;  // every address fits
#else
    unsigned long a = (unsigned long) p;
    return a >= RE4_MEM1_BASE && a < RE4_WINDOW_END;
#endif
}

extern "C" void re4_check_window(const char* what, const void* p)
{
    if (!re4_in_window(p)) {
        re4_log("%s at %p, outside 0x%08x..0xFFFFFFFF: the game keeps its addresses in 32 bits", what, p,
                RE4_MEM1_BASE);
        abort();
    }
}
