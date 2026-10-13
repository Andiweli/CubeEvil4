// Video interface: the retrace interrupt and its callbacks.
//
// The game counts frames in its VI post-retrace callback (postVSyncCallback:
// vsync_cnt, iTaskSuspend, the hang check) and paces the main loop by waiting
// for that count. The port has no interrupts; retraces are delivered at
// interrupt points instead: when every thread waits (the dispatcher's idle),
// in VIWaitForRetrace, in the main loop's vsync waits (patch 0012) and at the
// checkpoints of long background work. A retrace is due every NTSC field
// (1/59.94 s) of real time; RE4_VI_FAST=1 delivers the next one immediately
// whenever the game waits (tests, no pacing).
#include "re4_runtime.h"

#include <stdlib.h>
#include <time.h>

namespace {

typedef unsigned u32;
typedef void (*VIRetraceCallback)(u32 retraceCount);

const long long kFieldNs = 1000000000LL * 1001 / 60000;

VIRetraceCallback gPre;
VIRetraceCallback gPost;
u32 gRetraceCount;
long long gNextRetrace;
bool gStarted;
bool gFast;
bool gDelivering;
void* gNextFrameBuffer;
void* gCurrentFrameBuffer;

long long nowNs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long) ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void start()
{
    if (!gStarted) {
        gStarted = true;
        const char* fast = getenv("RE4_VI_FAST");
        gFast = fast && *fast && *fast != '0';
        gNextRetrace = nowNs() + kFieldNs;
    }
}

void deliver()
{
    gDelivering = true;
    gRetraceCount++;
    gCurrentFrameBuffer = gNextFrameBuffer;
    re4_os_interrupt_begin();
    if (gPre) {
        gPre(gRetraceCount);
    }
    if (gPost) {
        gPost(gRetraceCount);
    }
    re4_os_interrupt_end();
    gDelivering = false;
}

}  // namespace

extern "C" {

int re4_interrupt_point(void)
{
    if (!gStarted || gDelivering || gFast) {
        return 0;
    }
    long long now = nowNs();
    if (now < gNextRetrace) {
        return 0;
    }
    // Retraces missed while the CPU was busy are counted at once; the clock
    // then restarts from now instead of catching up further.
    int due = (int) ((now - gNextRetrace) / kFieldNs) + 1;
    if (due > 4) {
        due = 4;
    }
    gNextRetrace = now + kFieldNs - (now - gNextRetrace) % kFieldNs;
    for (int i = 0; i < due; i++) {
        deliver();
    }
    return due;
}

// Waits for the next interrupt and delivers it (the CPU idles until then).
void re4_os_idle(void)
{
    start();
    if (gDelivering) {
        return;
    }
    if (gFast) {
        deliver();
        return;
    }
    long long now = nowNs();
    if (now < gNextRetrace) {
        long long wait = gNextRetrace - now;
        struct timespec ts = {(time_t) (wait / 1000000000LL), (long) (wait % 1000000000LL)};
        nanosleep(&ts, 0);
    }
    re4_interrupt_point();
}

void re4_wait_interrupt(void)
{
    re4_os_idle();
}

void VIInit(void)
{
    start();
}

void VIWaitForRetrace(void)
{
    start();
    u32 count = gRetraceCount;
    while (gRetraceCount == count) {
        re4_os_idle();
    }
}

u32 VIGetRetraceCount(void)
{
    return gRetraceCount;
}

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb)
{
    VIRetraceCallback old = gPre;
    gPre = cb;
    return old;
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb)
{
    VIRetraceCallback old = gPost;
    gPost = cb;
    return old;
}

void VISetNextFrameBuffer(void* fb)
{
    gNextFrameBuffer = fb;
}

void* VIGetNextFrameBuffer(void)
{
    return gNextFrameBuffer;
}

void* VIGetCurrentFrameBuffer(void)
{
    return gCurrentFrameBuffer;
}

void VISetBlack(int black)
{
    (void) black;
}

void VIConfigure(const void* renderMode)
{
    (void) renderMode;
}

void VIFlush(void) {}

u32 VIGetTvFormat(void)
{
    return 0;  // VI_NTSC
}

u32 VIGetDTVStatus(void)
{
    return 0;  // no component cable: interlaced output
}

u32 VIGetNextField(void)
{
    return gRetraceCount & 1;
}

}  // extern "C"
