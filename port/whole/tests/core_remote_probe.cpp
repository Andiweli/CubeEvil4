// The core process's test entry (host/coreproc.cpp with RE4_CORE_TEST_ENTRY,
// Linux; the check is tests/core_remote_test.cpp). It runs instead of the
// game, as its boot thread, and gives the app's side something to receive:
//
//   remote  one game frame after the other (PADRead, then a GX frame of the
//           port's GX probe), the sound of 40 AI blocks (left 1000, right
//           -1000) at the start; it watches the controller (the A-and-stick
//           state the check sends first, B taps, X switches the rumble motor
//           on), the depth of field switch and the debug menu request, and
//           ends with START (return 7), one log line with what it saw
//   crash   a crash on purpose (the app must notice the core process's end)
//
// The memory card: RE4_REMOTE_TEST_CARD (a new, formatted card when missing).
#include "re4_runtime.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace {

struct PadStatus {  // PADStatus
    unsigned short button;
    signed char stickX, stickY, substickX, substickY;
    unsigned char triggerLeft, triggerRight, analogA, analogB;
    signed char err;
};

}  // namespace

extern "C" {
unsigned PADRead(PadStatus* status);
void PADControlMotor(long chan, unsigned cmd);
int re4_dof_enabled(void);
int re4_debug_menu_request(void);
void re4probe_gx_produce(void* fifo, void* xfb, int frames, int init);
void AIInit(unsigned char* stack);
void AIInitDMA(unsigned start, unsigned length);
void AIStartDMA(void);
void re4_ai_block(void);
void re4_memcard_open(const char* path);
}

extern "C" __attribute__((visibility("default"))) int re4_coreproc_test_entry(void)
{
    const char* what = getenv("RE4_CORE_TEST_ENTRY");
    if (what && strcmp(what, "crash") == 0) {
        re4_log("remote-probe: a crash on purpose");
        volatile int* nowhere = 0;
        *nowhere = 1;
        return 1;
    }
    if (const char* card = getenv("RE4_REMOTE_TEST_CARD")) {
        re4_memcard_open(card);
    }
    // the sound: the DSP leaves right before left in each frame (runtime/ai.cpp)
    short* block = (short*) (unsigned long) 0x82A00000u;
    for (int i = 0; i < 160; i++) {
        block[i * 2] = -1000;
        block[i * 2 + 1] = 1000;
    }
    AIInit(0);
    AIInitDMA(0x82A00000u, 160 * 4);
    AIStartDMA();
    for (int i = 0; i < 40; i++) {
        re4_ai_block();
    }
    void* fifo = (void*) (unsigned long) 0x82800000u;
    void* xfb = (void*) (unsigned long) 0x82830000u;
    unsigned frames = 0, padAt = 0, bFrames = 0, buttons = 0, dofChanges = 0, debugMenu = 0;
    int dof = re4_dof_enabled();
    for (; frames < 60000; frames++) {
        PadStatus pads[4];
        PADRead(pads);
        const PadStatus& p = pads[0];
        if (!padAt && p.button == 0x0100 && p.stickX == 50 && p.stickY == -20 && p.triggerRight == 200 &&
            p.analogA == 0xFF) {
            padAt = frames + 1;
        }
        if (p.button & 0x0200) {
            bFrames++;
        }
        buttons |= p.button;
        PADControlMotor(0, (p.button & 0x0400) ? 1 : 0);
        const int d = re4_dof_enabled();
        if (d != dof) {
            dofChanges++;
            dof = d;
        }
        if (re4_debug_menu_request()) {
            debugMenu++;
        }
        re4probe_gx_produce(fifo, xfb, 1, frames == 0);
        if (p.button & 0x1000) {
            break;
        }
        usleep(3000);
    }
    re4_log("remote-probe: frames %u, pad at %u, B frames %u, buttons 0x%04x, dof changes %u (now %d), debug menu %u",
            frames, padAt, bFrames, buttons, dofChanges, dof, debugMenu);
    return 7;
}
