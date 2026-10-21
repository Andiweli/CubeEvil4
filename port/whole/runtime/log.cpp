// Logging for the port runtime and the generated stubs.
// The last lines also stay in a small ring for the app's diagnostic overlay
// (re4_core_log_tail).
#include "re4_runtime.h"

#include <atomic>
#include <mutex>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace {

const int kTailLines = 24;
const int kTailWidth = 120;
std::mutex gTailLock;
char gTail[kTailLines][kTailWidth];
int gTailNext;
int gTailCount;

void remember(const char* fmt, va_list ap)
{
    char line[kTailWidth];
    vsnprintf(line, sizeof(line), fmt, ap);
    for (char* c = line; *c; c++) {
        if (*c == '\n' || *c == '\r') {
            *c = ' ';
        }
    }
    std::lock_guard<std::mutex> lock(gTailLock);
    memcpy(gTail[gTailNext], line, sizeof(line));
    gTailNext = (gTailNext + 1) % kTailLines;
    if (gTailCount < kTailLines) {
        gTailCount++;
    }
}

}  // namespace

extern "C" void re4_log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list copy;
    va_copy(copy, ap);
    remember(fmt, copy);
    va_end(copy);
#ifdef __ANDROID__
    // RE4_LOG_STDERR=1: also on stderr, one line per call (the core probe
    // host/probe.cpp started over adb keeps the whole log with its output)
    static const bool toStderr = getenv("RE4_LOG_STDERR") && strcmp(getenv("RE4_LOG_STDERR"), "1") == 0;
    if (toStderr) {
        char line[1024];
        va_list again;
        va_copy(again, ap);
        vsnprintf(line, sizeof(line), fmt, again);
        va_end(again);
        fprintf(stderr, "[re4] %s\n", line);
    }
    __android_log_vprint(ANDROID_LOG_INFO, "re4", fmt, ap);
#else
    fputs("[re4] ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
#endif
    va_end(ap);
}

// The remembered lines, oldest first, '\n'-separated; returns the length.
extern "C" int re4_log_tail(char* out, int size)
{
    if (!out || size <= 0) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(gTailLock);
    int len = 0;
    out[0] = 0;
    for (int i = 0; i < gTailCount; i++) {
        const char* line = gTail[(gTailNext - gTailCount + i + kTailLines) % kTailLines];
        int n = snprintf(out + len, (size_t) (size - len), "%s\n", line);
        if (n < 0 || n >= size - len) {
            break;
        }
        len += n;
    }
    return len;
}

// The game's own log lines (pLog->err / warn / mes, db_log.cpp, patch 0076): on
// the GameCube they went to the screen and the console. Here each message
// (by its format string) is logged the first 3 times, then every 1000th time
// with its count, so that a message of every frame does not flood the log.
extern "C" void re4_game_log(const char* format, const char* text)
{
    struct Seen {
        const char* format;
        unsigned count;
    };
    static Seen seen[512];
    static int used;
    static std::mutex lock;
    unsigned count = 4;  // a full table: not logged
    {
        std::lock_guard<std::mutex> guard(lock);
        int i = 0;
        while (i < used && seen[i].format != format) {
            i++;
        }
        if (i == used && used < 512) {
            seen[used++] = {format, 0};
        }
        if (i < used) {
            count = ++seen[i].count;
        }
    }
    if (count <= 3) {
        re4_log("game: %s", text);
    } else if (count % 1000 == 0) {
        re4_log("game: %s (%u times)", text, count);
    }
}

// An allocation of the game that found no room (main_mem.cpp, patch 0081):
// heap number, size, the flag of mem_alloc (1 the game reports it itself, 0 a
// try with a fallback such as cDataSwap's ARAM, -1 Debug_alloc) and the place
// ("card.cpp(2190)"). Each place the first 3 times, then every 1000th time;
// re4_heap_fail_count for the state trace.
static std::atomic<unsigned> gHeapFails{0};

extern "C" void re4_heap_failed(int heap, unsigned size, int flag, const char* where)
{
    unsigned n = ++gHeapFails;
    struct Seen {
        char where[32];
        unsigned count;
    };
    static Seen seen[256];
    static int used;
    static std::mutex lock;
    unsigned count = 4;
    {
        std::lock_guard<std::mutex> guard(lock);
        const char* w = where ? where : "?";
        int i = 0;
        while (i < used && strncmp(seen[i].where, w, sizeof(seen[i].where) - 1) != 0) {
            i++;
        }
        if (i == used && used < 256) {
            snprintf(seen[used].where, sizeof(seen[used].where), "%s", w);
            seen[used++].count = 0;
        }
        if (i < used) {
            count = ++seen[i].count;
        }
    }
    if (count <= 3 || count % 1000 == 0) {
        re4_log("heap: no room for 0x%x bytes in heap %d (%s, flag %d; %u failures so far)", size, heap,
                where ? where : "?", flag, n);
    }
}

extern "C" unsigned re4_heap_fail_count(void)
{
    return gHeapFails.load();
}

extern "C" void re4_stub_hit(const char* const* names, unsigned char* seen, int index)
{
    if (!seen[index]) {
        seen[index] = 1;
        re4_log("stub called: %s", names[index]);
    }
}
