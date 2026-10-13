// Dolphin OS replacement: boot, console info, time, interrupts and the debug
// output. The heap allocator itself is the SDK's own OSAlloc.c/OSArena.c
// (src/lib), compiled unchanged into libre4_main.
#include "re4_runtime.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern "C" {
void OSSetArenaLo(void* lo);
void OSSetArenaHi(void* hi);
}

namespace {

typedef long long OSTime;

// OS_TIMER_CLOCK: a quarter of the 162 MHz bus clock.
const long long kTimerClock = 162000000 / 4;

struct CalendarTime {
    int sec, min, hour, mday, mon, year, wday, yday, msec, usec;
};

// OSReport text arrives in pieces ("FST Address = ", "\n"); the log gets lines.
char gReportLine[1024];
size_t gReportLen;

void reportText(const char* text)
{
    for (const char* p = text; *p; p++) {
        if (*p == '\n' || gReportLen == sizeof(gReportLine) - 1) {
            gReportLine[gReportLen] = 0;
            re4_log("OSReport: %s", gReportLine);
            gReportLen = 0;
            if (*p == '\n') {
                continue;
            }
        }
        gReportLine[gReportLen++] = *p;
    }
}

OSTime monotonicTicks()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (OSTime) ts.tv_sec * kTimerClock + (OSTime) ts.tv_nsec * kTimerClock / 1000000000;
}

// OSGetTime counts from 2000-01-01 00:00 (the GameCube epoch).
const time_t kEpoch2000 = 946684800;
OSTime gTimeOffset;
bool gTimeOffsetSet;

int gInterruptsEnabled = 1;
unsigned gProgressiveMode;
unsigned gSoundMode = 1;  // stereo

}  // namespace

extern "C" {

void OSInit(void)
{
    if (!re4_mem1_init()) {
        re4_log("OSInit: GameCube main memory is unavailable, cannot continue");
        abort();
    }
    OSSetArenaLo((void*) (unsigned long) RE4_MEM1_ARENA_LO);
    OSSetArenaHi((void*) (unsigned long) RE4_MEM1_FST_ADDR);
}

void OSReport(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    reportText(buf);
}

void OSPanic(const char* file, int line, const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    re4_log("OSPanic %s:%d: %s", file, line, buf);
    abort();
}

unsigned OSGetConsoleType(void)
{
    return RE4_CONSOLE_TYPE;
}

unsigned OSGetConsoleSimulatedMemSize(void)
{
    return RE4_MEM1_RETAIL_SIZE;
}

unsigned OSGetPhysicalMemSize(void)
{
    return RE4_MEM1_RETAIL_SIZE;
}

OSTime OSGetTime(void)
{
    if (!gTimeOffsetSet) {
        gTimeOffset = (OSTime) (time(0) - kEpoch2000) * kTimerClock - monotonicTicks();
        gTimeOffsetSet = true;
    }
    return monotonicTicks() + gTimeOffset;
}

unsigned OSGetTick(void)
{
    return (unsigned) OSGetTime();
}

void OSTicksToCalendarTime(OSTime ticks, CalendarTime* td)
{
    OSTime secs = ticks / kTimerClock;
    OSTime rest = ticks % kTimerClock;
    if (rest < 0) {
        rest += kTimerClock;
        secs--;
    }
    time_t t = (time_t) (secs + kEpoch2000);
    struct tm tm;
    gmtime_r(&t, &tm);
    td->sec = tm.tm_sec;
    td->min = tm.tm_min;
    td->hour = tm.tm_hour;
    td->mday = tm.tm_mday;
    td->mon = tm.tm_mon;
    td->year = tm.tm_year + 1900;
    td->wday = tm.tm_wday;
    td->yday = tm.tm_yday;
    td->msec = (int) (rest * 1000 / kTimerClock);
    td->usec = (int) (rest * 1000000 / kTimerClock % 1000);
}

OSTime OSCalendarTimeToTicks(CalendarTime* td)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_sec = td->sec;
    tm.tm_min = td->min;
    tm.tm_hour = td->hour;
    tm.tm_mday = td->mday;
    tm.tm_mon = td->mon;
    tm.tm_year = td->year - 1900;
    OSTime secs = (OSTime) timegm(&tm) - kEpoch2000;
    return secs * kTimerClock + (OSTime) td->msec * (kTimerClock / 1000) +
           (OSTime) td->usec * (kTimerClock / 1000000);
}

// There are no hardware interrupts; the game's interrupt-time work (the VI
// retrace callback, the interrupt task scheduler) is driven by the frame loop.
int OSDisableInterrupts(void)
{
    int old = gInterruptsEnabled;
    gInterruptsEnabled = 0;
    return old;
}

int OSEnableInterrupts(void)
{
    int old = gInterruptsEnabled;
    gInterruptsEnabled = 1;
    return old;
}

int OSRestoreInterrupts(int level)
{
    int old = gInterruptsEnabled;
    gInterruptsEnabled = level;
    return old;
}

// Soft-reset survival region (pRK): process memory survives anyway.
void OSSetSaveRegion(void* start, void* end)
{
    (void) start;
    (void) end;
}

void* OSSetErrorHandler(unsigned short error, void* handler)
{
    (void) error;
    (void) handler;
    return 0;
}

int DBIsDebuggerPresent(void)
{
    return 0;
}

void OSInitAlarm(void) {}

unsigned OSGetResetButtonState(void)
{
    return 0;
}

unsigned OSGetProgressiveMode(void)
{
    return gProgressiveMode;
}

void OSSetProgressiveMode(unsigned on)
{
    gProgressiveMode = on;
}

unsigned OSGetSoundMode(void)
{
    return gSoundMode;
}

void OSSetSoundMode(unsigned mode)
{
    gSoundMode = mode;
}

// Caches and the locked cache: coherent host memory, nothing to do.
void DCFlushRange(void*, unsigned) {}
void DCFlushRangeNoSync(void*, unsigned) {}
void DCInvalidateRange(void*, unsigned) {}
void DCStoreRange(void*, unsigned) {}
void DCStoreRangeNoSync(void*, unsigned) {}
void LCEnable(void) {}
void PPCSync(void) {}

}  // extern "C"
