// Compiled with the game's flags and headers (re4_game_flags) for the runtime
// test "stream": the sound driver's own stream code (snd_str*.cpp) plays a
// stream file of the synthetic disc (tests/runtime_tests.cpp writes it). The
// driver refills the stream in the audio-frame callback, i.e. inside an
// interrupt; it starts a DVD read or an ARAM DMA and marks it busy only
// afterwards (snd_str2.cpp), so the platform must not complete them inside the
// start call. Fields are delivered as vi.cpp does: the audio frames of one
// video field between re4_os_interrupt_begin and re4_os_interrupt_end.
#define SND_SDK_NO_GX
#include "snd_drv.h"

#include <string.h>

void SceSleep(int ctr);
int SndStrStatusCk(u32 snd_id, u32 status);

extern "C" {
void re4_os_interrupt_begin(void);
void re4_os_interrupt_end(void);
void re4_ai_field(void);
unsigned char* re4_aram(void);
}

namespace {

const u32 kAramRing = 0x400000;  // the stream's ARAM ring (as Snd_str_aram_adrs_set gets it)

u8 sBuffer[0x8000] __attribute__((aligned(32)));

// byte `off` of the test stream (runtime_tests.cpp writes the same)
u8 pattern(u32 off)
{
    return (u8) (off * 7 + (off >> 15) * 13 + 3);
}

void field()
{
    re4_os_interrupt_begin();
    re4_ai_field();
    re4_os_interrupt_end();
}

int gArqDone;
void arqDone(u32)
{
    gArqDone++;
}

}  // namespace

// `name`: a mono ADPCM stream file of `blocks` 32 KB blocks (bytes: pattern()).
// mode 0 plays it once to its end as SndStrPlayBlock does (request ready, wait
// for status 2, request play); mode 1 stops it (request 8, as SndStrStopBlock
// and the radio's OpeSndStrStop) after 20 fields of play; mode 2 stops it
// while it is still buffering; mode 3 stops it like mode 1 and waits for the
// end the way SndStrStopBlock does (SceSleep loop outside a scenario task). The fields until ready
// and until the stream work is gone are returned; the result is a bit mask of
// failed checks (0 = passed).
extern "C" int re4probe_stream_test(const char* name, int blocks, int mode, int* readyField, int* endField)
{
    int failed = 0;
    *readyField = -1;
    *endField = -1;

    // ARQ: posted inside an interrupt handler, the callback waits for its end
    {
        static u8 src[64] __attribute__((aligned(32)));
        static ARQRequest req;
        memset(src, 0x5A, sizeof(src));
        gArqDone = 0;
        re4_os_interrupt_begin();
        ARQPostRequest(&req, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32) src, 0x300000, sizeof(src), arqDone);
        if (gArqDone != 0 || re4_aram()[0x300000] != 0x5A) {
            failed |= 0x1;
        }
        re4_os_interrupt_end();
        if (gArqDone != 1) {
            failed |= 0x2;
        }
        ARQPostRequest(&req, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32) src, 0x300000, sizeof(src), arqDone);
        if (gArqDone != 2) {  // elsewhere at once (the game's queues spin on it)
            failed |= 0x4;
        }
    }

    Snd_system_init();
    Snd_str_buff[0] = sBuffer;
    Snd_str_aram_adrs_set(0, kAramRing);
    memset(re4_aram() + kAramRing, 0, 8 * 0x8000);

    static SND_SHD shd;
    static SND_RIT rit;
    memset(&shd, 0, sizeof(shd));
    memset(&rit, 0, sizeof(rit));
    shd.flag = 0x2;                  // mono, no loop
    shd.nibbles = blocks * 0x10000;  // 32 KB = 64 K nibbles a block
    shd.samples = blocks * 0x10000 / 16 * 14;
    shd.rate = 128000;  // 4x the output rate: a quick test
    shd.lptop_nbl = 2;
    shd.lpend_nbl = shd.nibbles - 1;
    rit.ch = 0;
    rit.poly = 0;
    rit.vol = 100;
    rit.pan = 64;
    rit.pl_id = 0;
    rit.span = -1;
    rit.svol = -1;

    u32 id = Snd_str_init(&shd, &rit, 0, (char*) name, 0);
    if (id == 0) {
        AXRegisterCallback(0);
        return failed | 0x8;
    }
    SND_STR* str = &Snd_str_work[0];
    const u8* ring = re4_aram() + kAramRing;
    const int kLimit = 4000;
    int f = 0;
    Snd_str_req(id, 1, 0, 0);
    if (mode == 2) {
        // one field of buffering (the long stream needs about 5), then stop
        field();
        if (Snd_str_get_status(id) & 0x2) {
            failed |= 0x400;
        }
        Snd_str_req(id, 8, 0, 0);
        f = 1;
    } else {
        for (; f < kLimit; f++) {
            if (Snd_str_get_status(id) & 0x2) {
                break;
            }
            field();
        }
        if (f == kLimit) {
            AXRegisterCallback(0);
            return failed | 0x10;
        }
        *readyField = f;
        // the first ring block holds the start of the file
        for (int i = 0; i < 0x8000; i++) {
            if (ring[i] != pattern(i)) {
                failed |= 0x20;
                break;
            }
        }
        Snd_str_req(id, 2, 0, 0);
    }
    int played = 0;
    int start = f;
    for (; f < kLimit; f++) {
        Snd_str_work_close_check();  // the game frame's part
        if (Snd_str_end_check(id) == 0) {
            break;
        }
        if (str->be_flag & 0x10) {
            played = 1;
        }
        if (str->be_flag & 0x8000) {
            failed |= 0x40;  // DVD error: starvation or a failed read
        }
        if (mode == 1 && f == start + 20) {
            Snd_str_req(id, 8, 0, 0);
        }
        if (mode == 3 && f == start + 20) {
            // the radio screen's stop (SndStrStopBlock): request, then
            // SceSleep(1) until it no longer plays, outside a scenario task;
            // only the interrupts delivered while it waits (patch 0035) move
            // the fade on
            Snd_str_req(id, 8, 0, 0);
            int spins = 0;
            do {
                SceSleep(1);
            } while (SndStrStatusCk(id, 0x10) != 0 && ++spins < 2000);
            if (spins >= 2000) {
                failed |= 0x1000;
            }
        }
        field();
    }
    if (f == kLimit) {
        AXRegisterCallback(0);
        return failed | 0x80;
    }
    *endField = f;
    if (played != (mode != 2)) {
        failed |= 0x100;
    }
    if (mode == 0) {
        // the whole file went through the ring: the last block landed in its slot
        const u8* slot = ring + (blocks - 1) % 8 * 0x8000;
        for (int i = 0; i < 0x8000; i++) {
            if (slot[i] != pattern((blocks - 1) * 0x8000 + i)) {
                failed |= 0x200;
                break;
            }
        }
    }
    if (str->dvd_busy || str->dma_busy || str->ax_voice_l) {
        failed |= 0x800;  // the work was released idle
    }
    AXRegisterCallback(0);  // the driver's tick ends with the test
    return failed;
}
