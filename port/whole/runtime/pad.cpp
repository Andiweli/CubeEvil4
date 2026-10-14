// Controllers (PAD). Real input comes with milestone 7; until then channel 0
// is a connected controller whose buttons come from a test script, channels
// 1-3 are empty (the title screen skips its logos when a second pad answers).
//
//   RE4_PAD_SCRIPT="START@300 A@400-3000/40"
//     BUTTONS@FRAME            hold for 3 frames from PADRead call FRAME
//     BUTTONS@FROM-TO/EVERY    the same every EVERY frames from FROM to TO
//     BUTTONS: A B X Y Z L R START UP DOWN LEFT RIGHT, joined with '+'
//
// The game calls PADRead once per frame (pad.cpp PadRead in the main loop).
#include "re4_runtime.h"

#include <stdlib.h>
#include <string.h>

namespace {

typedef unsigned short u16;
typedef unsigned int u32;
typedef signed char s8;
typedef unsigned char u8;

struct PADStatus {
    u16 button;
    s8 stickX;
    s8 stickY;
    s8 substickX;
    s8 substickY;
    u8 triggerLeft;
    u8 triggerRight;
    u8 analogA;
    u8 analogB;
    s8 err;
};
static_assert(sizeof(PADStatus) == 12, "PADStatus layout");

const s8 kNoController = -1;  // PAD_ERR_NO_CONTROLLER
const u32 kHold = 3;

struct Press {
    u16 buttons;
    u32 from;
    u32 to;
    u32 every;
};

Press gScript[64];
int gScriptLen;
u32 gFrame;

u16 buttonBit(const char* name, size_t len)
{
    static const struct {
        const char* name;
        u16 bit;
    } kNames[] = {
        { "LEFT", 0x0001 }, { "RIGHT", 0x0002 }, { "DOWN", 0x0004 }, { "UP", 0x0008 },
        { "Z", 0x0010 },    { "R", 0x0020 },     { "L", 0x0040 },    { "A", 0x0100 },
        { "B", 0x0200 },    { "X", 0x0400 },     { "Y", 0x0800 },    { "START", 0x1000 },
    };
    for (const auto& n : kNames) {
        if (strlen(n.name) == len && strncmp(n.name, name, len) == 0) {
            return n.bit;
        }
    }
    re4_log("pad: unknown button \"%.*s\" in RE4_PAD_SCRIPT", (int) len, name);
    return 0;
}

void parseScript(const char* s)
{
    while (*s && gScriptLen < 64) {
        while (*s == ' ' || *s == ',') {
            s++;
        }
        if (!*s) {
            break;
        }
        Press p = { 0, 0, 0, 0 };
        while (*s && *s != '@') {
            const char* e = s;
            while (*e && *e != '+' && *e != '@') {
                e++;
            }
            p.buttons |= buttonBit(s, (size_t) (e - s));
            s = *e == '+' ? e + 1 : e;
        }
        if (*s != '@') {
            re4_log("pad: RE4_PAD_SCRIPT entry without @FRAME ignored");
            break;
        }
        char* e;
        p.from = (u32) strtoul(s + 1, &e, 10);
        p.to = p.from;
        if (*e == '-') {
            p.to = (u32) strtoul(e + 1, &e, 10);
            if (*e == '/') {
                p.every = (u32) strtoul(e + 1, &e, 10);
            }
        }
        s = e;
        gScript[gScriptLen++] = p;
    }
    re4_log("pad: test script with %d entries", gScriptLen);
}

u16 scriptButtons(u32 frame)
{
    u16 b = 0;
    for (int i = 0; i < gScriptLen; i++) {
        const Press& p = gScript[i];
        if (frame < p.from || frame >= p.to + kHold) {
            continue;
        }
        u32 phase = frame - p.from;
        if (p.every ? phase % p.every < kHold : phase < kHold) {
            b |= p.buttons;
        }
    }
    return b;
}

}  // namespace

extern "C" {

int PADInit(void)
{
    if (const char* s = getenv("RE4_PAD_SCRIPT")) {
        if (gScriptLen == 0) {
            parseScript(s);
        }
    }
    return 1;
}

int PADReset(u32) { return 1; }
int PADRecalibrate(u32) { return 1; }
void PADSetAnalogMode(u32) {}
void PADClamp(PADStatus*) {}
void PADControlMotor(long, u32) {}

// Per-frame observers (the state trace, runtime/state_trace.cpp), called
// once per game frame with the frame number.
static void (*g_frameHook)(unsigned);

void re4_set_frame_hook(void (*hook)(unsigned))
{
    g_frameHook = hook;
}

u32 PADRead(PADStatus* status)
{
    if (g_frameHook) {
        g_frameHook(gFrame);
    }
    memset(status, 0, 4 * sizeof(PADStatus));
    status[0].button = scriptButtons(gFrame);
    if (status[0].button) {
        u16 old = gFrame ? scriptButtons(gFrame - 1) : 0;
        if (old != status[0].button) {
            re4_log("pad: frame %u buttons 0x%04x", gFrame, status[0].button);
        }
    }
    for (int i = 1; i < 4; i++) {
        status[i].err = kNoController;
    }
    gFrame++;
    return 0;
}

}  // extern "C"
