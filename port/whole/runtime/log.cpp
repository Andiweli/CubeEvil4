// Logging for the port runtime and the generated stubs.
// The last lines also stay in a small ring for the app's diagnostic overlay
// (re4_core_log_tail).
#include "re4_runtime.h"

#include <mutex>
#include <stdarg.h>
#include <stdio.h>
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

extern "C" void re4_stub_hit(const char* const* names, unsigned char* seen, int index)
{
    if (!seen[index]) {
        seen[index] = 1;
        re4_log("stub called: %s", names[index]);
    }
}
