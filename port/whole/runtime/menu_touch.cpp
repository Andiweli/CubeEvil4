// Touch in the game's sub screens (wish of the project owner, 22.10.2026: pick the tabs and the
// items of the attache case with a finger), compiled with the game's flags into libre4_main. The
// app hands a tap on the picture over (re4_core_touch: u across the game's 512 x 448 picture, v
// down it, 0..1; CoreActivity.watchTap only passes taps beside the touch controls); at the next
// game frame (a frame hook in PADRead, before game/pad.cpp turns the pad into Key) it is used
// while a sub screen is open and dropped elsewhere.
#include "types.h"
#include "global.h"

#include <time.h>

extern GLOBAL_WK* pG;

extern "C" void re4_log(const char* fmt, ...);
extern "C" void re4_add_frame_hook(void (*hook)(unsigned));

namespace {

// the app's thread writes, the game's reads: the tap as float bits, counted
volatile u32 gTapU, gTapV, gTapSeq;
u32 gTapSeen;
bool gHooked;
unsigned gTaps, gUsed;  // the log

u32 bits(float f)
{
    u32 b;
    __builtin_memcpy(&b, &f, 4);
    return b;
}

float value(u32 b)
{
    float f;
    __builtin_memcpy(&f, &b, 4);
    return f;
}

void touchHook(unsigned frame)
{
    const u32 seq = __atomic_load_n(&gTapSeq, __ATOMIC_ACQUIRE);
    if (seq == gTapSeen) {
        return;
    }
    gTapSeen = seq;
    const float u = value(__atomic_load_n(&gTapU, __ATOMIC_RELAXED));
    const float v = value(__atomic_load_n(&gTapV, __ATOMIC_RELAXED));
    const bool sub = pG && pG->Rno0 == 3 && StaFlagChk(pG, STA_SUB_SCRN);
    gTaps++;
    if (gTaps <= 20 || gTaps % 100 == 0) {
        re4_log("touch: tap at (%.0f %.0f) of 512x448 in frame %u%s", (double) (u * 512.0f), (double) (v * 448.0f),
                frame, sub ? ", sub screen open" : ", no sub screen: not used");
    }
    if (!sub) {
        return;
    }
    gUsed++;
}

} // namespace

// The app (CoreBridge.tap): a tap on the picture, any thread.
extern "C" void re4_core_touch(float u, float v)
{
    if (!gHooked) {
        gHooked = true;
        re4_add_frame_hook(touchHook);
        re4_log("touch: taps on the picture reach the sub screens");
    }
    if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) {
        return;
    }
    __atomic_store_n(&gTapU, bits(u), __ATOMIC_RELAXED);
    __atomic_store_n(&gTapV, bits(v), __ATOMIC_RELAXED);
    __atomic_fetch_add(&gTapSeq, 1, __ATOMIC_RELEASE);
}
