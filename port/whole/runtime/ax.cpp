// AX, the audio DSP interface (SDK libax), without sound output yet (M8).
//
// The game's sound driver (snd_*.cpp, the Capcom sound library) runs in the
// AX audio-frame callback every 5 ms (snd_main.cpp cb_audio_frame): it starts
// requested sounds, frees the voices whose AX voice stopped, refills streams
// by the play position of their voices and runs the MIDI sequencer. Without
// that tick nothing ever finishes: voice works stay taken ("STR SND_VOICE L is
// not available"), a voice stream never becomes ready and the radio call
// (ss_term.cpp OpeMesMove) waits for it forever.
//
// This replacement keeps what the driver sees of the hardware:
//   - the 64 voices with the SDK's allocation rules (AXAlloc.c: a free voice,
//     else the oldest voice of the lowest priority below the requested one,
//     whose drop callback runs first),
//   - the parameter setters (AXVPB.c) writing the voice's AXPB,
//   - the DSP's playback: every audio frame each running voice moves on by
//     160 output samples (32 kHz) times its sample-rate ratio, in the address
//     unit of its format (ADPCM nibbles with the frame headers skipped, PCM16
//     and PCM8 samples), loops at its end address or stops there (state 0),
//   - the audio-frame callback, 200 times per second of game time: delivered
//     with the video retraces (vi.cpp), 3 or 4 per field.
// Mixing the samples into sound output is milestone 8 (it reads the same
// voices). Effects (AXFX), the mixer (MIX), the synth (SYN) and the
// sequencer (SEQ) stay generated stubs until then: no notes, no effects.
#include "dolphin/ax.h"

#include "re4_runtime.h"

#include <string.h>

namespace {

const u32 kPriorities = AX_PRIORITY_STACKS;  // 0 = free, 1..31
const u32 kFrameSamples = 160;               // 5 ms at 32 kHz

AXVPB gVoices[AX_MAX_VOICES];
// per priority: a doubly linked list, head = newest (AXAlloc.c __AXPushStackHead)
AXVPB* gHead[kPriorities];
AXVPB* gTail[kPriorities];
AXCallback gCallback;
bool gInit;
u32 gFieldAccumulator;
u32 gFrames;

void unlink(AXVPB* p)
{
    u32 prio = p->priority;
    AXVPB* prev = (AXVPB*) p->prev;
    AXVPB* next = (AXVPB*) p->next;
    if (prev) {
        prev->next = next;
    } else {
        gHead[prio] = next;
    }
    if (next) {
        next->prev = prev;
    } else {
        gTail[prio] = prev;
    }
    p->next = p->prev = 0;
}

void pushHead(AXVPB* p, u32 prio)
{
    p->priority = prio;
    p->prev = 0;
    p->next = gHead[prio];
    if (gHead[prio]) {
        ((AXVPB*) gHead[prio])->prev = p;
    } else {
        gTail[prio] = p;
    }
    gHead[prio] = p;
}

// AXVPB.c __AXSetPBDefault
void setDefault(AXVPB* p)
{
    p->pb.state = 0;
    p->pb.itd.flag = 0;
    p->sync = 0x2000A4;
    p->updateMS = p->updateCounter = 0;
    p->updateWrite = p->updateData;
    for (int i = 0; i < 5; i++) {
        p->pb.update.updNum[i] = 0;
    }
    p->pb.lpf.on = 0;
}

void init()
{
    if (gInit) {
        return;
    }
    gInit = true;
    memset(gVoices, 0, sizeof(gVoices));
    memset(gHead, 0, sizeof(gHead));
    memset(gTail, 0, sizeof(gTail));
    for (u32 i = 0; i < AX_MAX_VOICES; i++) {
        AXVPB* p = &gVoices[i];
        p->index = i;
        setDefault(p);
        pushHead(p, 0);
    }
}

u32 addr32(u16 hi, u16 lo)
{
    return ((u32) hi << 16) | lo;
}

// Play position of an address in samples (ADPCM: 14 samples per 8-byte frame
// of 16 nibbles, the first two nibbles are the frame header).
u32 toSample(u32 addr, u16 format)
{
    if (format == 0) {
        u32 nibble = addr & 15;
        return (addr >> 4) * 14 + (nibble < 2 ? 0 : nibble - 2);
    }
    return addr;
}

u32 toAddr(u32 sample, u16 format)
{
    if (format == 0) {
        return (sample / 14) * 16 + 2 + sample % 14;
    }
    return sample;
}

// One audio frame of the DSP for one running voice.
void play(AXVPB* p)
{
    AXPB& pb = p->pb;
    u32 ratio = pb.srcSelect == 2 ? 0x10000 : addr32(pb.src.ratioHi, pb.src.ratioLo);
    u32 total = pb.src.currentAddressFrac + kFrameSamples * ratio;
    pb.src.currentAddressFrac = (u16) total;
    u32 n = total >> 16;
    if (n == 0) {
        return;
    }
    u16 format = pb.addr.format;
    u32 cur = toSample(addr32(pb.addr.currentAddressHi, pb.addr.currentAddressLo), format);
    u32 end = toSample(addr32(pb.addr.endAddressHi, pb.addr.endAddressLo), format);
    u32 pos = cur + n;
    if (pos > end) {
        if (pb.addr.loopFlag) {
            u32 loop = toSample(addr32(pb.addr.loopAddressHi, pb.addr.loopAddressLo), format);
            u32 length = end >= loop ? end - loop + 1 : 0;
            pos = length ? loop + (pos - end - 1) % length : loop;
        } else {
            pos = end;
            pb.state = 0;  // the DSP stops the voice at its end
        }
    }
    u32 a = toAddr(pos, format);
    pb.addr.currentAddressHi = (u16) (a >> 16);
    pb.addr.currentAddressLo = (u16) a;
}

}  // namespace

extern "C" {

// One audio frame: the DSP plays the running voices, then the game's callback.
void re4_ax_frame(void)
{
    if (!gInit) {
        return;
    }
    for (u32 prio = 1; prio < kPriorities; prio++) {
        for (AXVPB* p = gHead[prio]; p; p = (AXVPB*) p->next) {
            if (p->pb.state == 1) {
                play(p);
            }
            p->sync = 0;
            p->depop = 0;
        }
    }
    gFrames++;
    if (gCallback) {
        gCallback();
    }
}

// A video field went by (vi.cpp): the audio frames of that time, 200 per
// second (a field is 1001/60000 s).
void re4_ax_field(void)
{
    if (!gInit || !gCallback) {
        return;
    }
    gFieldAccumulator += 200 * 1001;
    while (gFieldAccumulator >= 60000) {
        gFieldAccumulator -= 60000;
        re4_ax_frame();
    }
}

unsigned re4_ax_frames(void)
{
    return gFrames;
}

void AXInit(void)
{
    init();
}

void AXInitEx(u32 outputBufferMode)
{
    (void) outputBufferMode;
    init();
}

void AXQuit(void)
{
    gCallback = 0;
}

AXCallback AXRegisterCallback(AXCallback callback)
{
    AXCallback old = gCallback;
    gCallback = callback;
    return old;
}

void AXRegisterAuxACallback(void (*callback)(void*, void*), void* context)
{
    (void) callback;
    (void) context;
}

void AXRegisterAuxBCallback(void (*callback)(void*, void*), void* context)
{
    (void) callback;
    (void) context;
}

void AXSetMode(u32 mode)
{
    (void) mode;
}

u32 AXGetMode(void)
{
    return 0;
}

// The driver's statistics only (snd_main.cpp Snd_dev_voice_ck).
u32 AXGetMaxDspCycles(void)
{
    return 486000000 / 4 / 400;  // OS_BUS_CLOCK / 400, as __AXVPBInit sets it
}

u32 AXGetDspCycles(void)
{
    return 0;
}

void AXSetMaxDspCycles(u32 cycles)
{
    (void) cycles;
}

// AXAlloc.c
AXVPB* AXAcquireVoice(u32 priority, void (*callback)(void*), u32 userContext)
{
    init();
    if (priority == 0 || priority >= kPriorities) {
        return 0;
    }
    AXVPB* p = gTail[0];
    if (p) {
        unlink(p);
    } else {
        // the oldest voice of the lowest priority below the requested one
        for (u32 i = 1; i < priority && !p; i++) {
            p = gTail[i];
        }
        if (p) {
            unlink(p);
            if (p->pb.state == 1) {
                p->depop = 1;
            }
            if (p->callback) {
                p->callback(p);
            }
        }
    }
    if (p) {
        pushHead(p, priority);
        p->callback = callback;
        p->userContext = userContext;
        setDefault(p);
    }
    return p;
}

void AXFreeVoice(AXVPB* p)
{
    if (!p || p->priority == 0) {
        return;
    }
    unlink(p);
    if (p->pb.state == 1) {
        p->depop = 1;
    }
    setDefault(p);
    pushHead(p, 0);
}

void AXSetVoicePriority(AXVPB* p, u32 priority)
{
    if (!p || p->priority == 0 || priority == 0 || priority >= kPriorities) {
        return;
    }
    unlink(p);
    pushHead(p, priority);
}

// AXVPB.c: the setters (the sync flags only matter to the real DSP)
void AXSetVoiceSrcType(AXVPB* p, u32 type)
{
    switch (type) {
    case AX_SRC_TYPE_NONE:
        p->pb.srcSelect = 2;
        break;
    case AX_SRC_TYPE_LINEAR:
        p->pb.srcSelect = 1;
        break;
    case AX_SRC_TYPE_4TAP_8K:
    case AX_SRC_TYPE_4TAP_12K:
    case AX_SRC_TYPE_4TAP_16K:
        p->pb.srcSelect = 0;
        p->pb.coefSelect = (u16) (type - AX_SRC_TYPE_4TAP_8K);
        break;
    }
}

void AXSetVoiceState(AXVPB* p, u16 state)
{
    p->pb.state = state;
    if (state == 0) {
        p->depop = 1;
    }
}

void AXSetVoiceType(AXVPB* p, u16 type)
{
    p->pb.type = type;
}

void AXSetVoiceMix(AXVPB* p, AXPBMIX* mix)
{
    p->pb.mix = *mix;
}

void AXSetVoiceDpop(AXVPB* p, AXPBDPOP* dpop)
{
    p->pb.dpop = *dpop;
}

void AXSetVoiceVe(AXVPB* p, AXPBVE* ve)
{
    p->pb.ve = *ve;
}

void AXSetVoiceVeDelta(AXVPB* p, s16 delta)
{
    p->pb.ve.currentDelta = delta;
}

void AXSetVoiceFir(AXVPB* p, AXPBFIR* fir)
{
    p->pb.fir = *fir;
}

void AXSetVoiceAddr(AXVPB* p, AXPBADDR* addr)
{
    p->pb.addr = *addr;
    // AXSetVoiceAddr: PCM voices get the fixed ADPCM gain block
    if (addr->format == 10 || addr->format == 25) {
        memset(&p->pb.adpcm, 0, sizeof(p->pb.adpcm));
        p->pb.adpcm.gain = addr->format == 10 ? 0x0800 : 0x0100;
    }
}

void AXSetVoiceLoop(AXVPB* p, u16 loop)
{
    p->pb.addr.loopFlag = loop;
}

void AXSetVoiceLoopAddr(AXVPB* p, u32 addr)
{
    p->pb.addr.loopAddressHi = (u16) (addr >> 16);
    p->pb.addr.loopAddressLo = (u16) addr;
}

void AXSetVoiceEndAddr(AXVPB* p, u32 addr)
{
    p->pb.addr.endAddressHi = (u16) (addr >> 16);
    p->pb.addr.endAddressLo = (u16) addr;
}

void AXSetVoiceCurrentAddr(AXVPB* p, u32 addr)
{
    p->pb.addr.currentAddressHi = (u16) (addr >> 16);
    p->pb.addr.currentAddressLo = (u16) addr;
}

void AXSetVoiceAdpcm(AXVPB* p, AXPBADPCM* adpcm)
{
    p->pb.adpcm = *adpcm;
}

void AXSetVoiceSrc(AXVPB* p, AXPBSRC* src)
{
    p->pb.src = *src;
}

void AXSetVoiceSrcRatio(AXVPB* p, f32 ratio)
{
    u32 r = (u32) (65536.0f * ratio);
    if (r > 0x40000) {
        r = 0x40000;
    }
    p->pb.src.ratioHi = (u16) (r >> 16);
    p->pb.src.ratioLo = (u16) r;
}

void AXSetVoiceAdpcmLoop(AXVPB* p, AXPBADPCMLOOP* adpcmloop)
{
    p->pb.adpcmLoop = *adpcmloop;
}

void AXSetVoiceLpf(AXVPB* p, AXPBLPF* lpf)
{
    p->pb.lpf = *lpf;
}

void AXSetVoiceLpfCoefs(AXVPB* p, u16 a0, u16 b0)
{
    p->pb.lpf.a0 = a0;
    p->pb.lpf.b0 = b0;
}

}  // extern "C"
