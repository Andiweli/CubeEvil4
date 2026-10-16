// EXI, the GameCube's external interface (M10): the functions of the SDK's
// EXIBios.c the memory card library uses (lock, select, immediate and DMA
// transfers, probe, attach, the device's interrupt), at C level. EXIBios.c
// drives hardware registers at 0xCC006800 the port cannot trap, so the
// semantics are kept here instead: the same channel states (SDK bit values),
// lock queue, select rules and return values.
//
// Devices: channel 0 device 0 is memory card slot A (runtime/memcard.cpp),
// slot B (channel 1) is empty; the IPL/RTC (channel 0 device 1) and the
// other devices answer nothing.
//
// Transfers happen at once. What the hardware signals by interrupt comes
// later, as there: the end of a DMA (or of an immediate transfer with a
// callback) and the card's "done" interrupt after a page program or erase
// are queued and delivered at the interrupt points (re4_exi_deliver, from
// vi.cpp re4_os_idle and os_thread.cpp OSSleepThread), each in interrupt
// context. The card library chains its transfers in these handlers (a
// block read is 16 DMAs, a block write 64 page programs), so the delivery
// runs until nothing is pending. The card interrupt must not come before
// the library has unlocked the channel (CARDBios.c __CARDExiHandler locks
// it), which the queue guarantees.
#include "dolphin/exi.h"
#include "re4_runtime.h"

#include <string.h>

extern "C" {
int re4_memcard_present(int chan);
u32 re4_memcard_id(int chan);
void re4_memcard_select(int chan);
void re4_memcard_deselect(int chan);
void re4_memcard_write(int chan, const void* data, int length);
void re4_memcard_read(int chan, void* data, int length);
void re4_memcard_flush(int force);
long long re4_alarm_deliver(void);
}

namespace {

// EXIBios.c
const u32 kStateDma = 1, kStateImm = 2, kStateSelected = 4, kStateAttached = 8, kStateLocked = 16;
const int kChannels = 3;

struct Channel {
    EXICallback exiCallback = 0;
    EXICallback tcCallback = 0;
    EXICallback extCallback = 0;
    u32 state = 0;
    u32 dev = 0;
    int items = 0;
    struct {
        u32 dev;
        EXICallback callback;
    } queue[3] = {};
    bool tcPending = false;   // end of a transfer with a callback
    bool exiPending = false;  // the device's interrupt
};

Channel gChannel[kChannels];
bool gInHandler;  // an EXI handler is running (no delivery from inside it)

// One handler in interrupt context. The guard covers the handler only: the
// end of the interrupt may switch to another thread (a woken one of higher
// priority, or away from the one that is going to sleep), which must be able
// to deliver the rest.
void handler(EXICallback cb, s32 chan)
{
    re4_os_interrupt_begin();
    gInHandler = true;
    cb(chan, 0);
    gInHandler = false;
    re4_os_interrupt_end();
}

bool cardSlot(s32 chan, u32 dev)
{
    return chan == 0 && dev == 0 && re4_memcard_present(0);
}

bool present(s32 chan)
{
    if (chan == 2) {
        return true;  // the internal channel (AD16, unused)
    }
    return chan >= 0 && chan < 2 && re4_memcard_present(chan);
}

bool transfer(s32 chan, void* buf, s32 len, u32 type)
{
    Channel& c = gChannel[chan];
    if (!(c.state & kStateSelected) || len <= 0) {
        return false;
    }
    if (!cardSlot(chan, c.dev)) {
        if (type == EXI_READ) {
            memset(buf, 0xFF, (size_t) len);  // nobody answers
        }
        return true;
    }
    if (type == EXI_WRITE) {
        re4_memcard_write(chan, buf, len);
    } else if (type == EXI_READ) {
        re4_memcard_read(chan, buf, len);
    } else {
        // read-write: the card answers with what it shifts out meanwhile
        re4_memcard_write(chan, buf, len);
    }
    return true;
}

}  // namespace

extern "C" {

// The card's interrupt (memcard.cpp): delivered with the next interrupts.
void re4_exi_raise(int chan)
{
    if (chan >= 0 && chan < kChannels) {
        gChannel[chan].exiPending = true;
    }
}

// Pending EXI interrupts (and due alarms) run now, in interrupt context, until
// none is left; the card file is synced a second after its last write.
void re4_exi_deliver(void)
{
    if (gInHandler) {
        return;
    }
    for (int round = 0; round < 100000; round++) {
        bool any = false;
        for (int chan = 0; chan < kChannels; chan++) {
            Channel& c = gChannel[chan];
            if (c.tcPending) {
                c.tcPending = false;
                any = true;
                EXICallback cb = c.tcCallback;
                c.tcCallback = 0;
                c.state &= ~(kStateDma | kStateImm);
                if (cb) {
                    handler(cb, chan);
                }
            }
            if (c.exiPending) {
                c.exiPending = false;
                any = true;
                if (c.exiCallback) {
                    handler(c.exiCallback, chan);
                }
            }
        }
        if (!any) {
            break;
        }
    }
    re4_alarm_deliver();
    re4_memcard_flush(0);
}

void EXIInit(void) {}

EXICallback EXISetExiCallback(s32 chan, EXICallback callback)
{
    if (chan < 0 || chan >= kChannels) {
        return 0;
    }
    EXICallback old = gChannel[chan].exiCallback;
    gChannel[chan].exiCallback = callback;
    return old;
}

BOOL EXILock(s32 chan, u32 dev, EXICallback unlockedCallback)
{
    if (chan < 0 || chan >= kChannels) {
        return FALSE;
    }
    Channel& c = gChannel[chan];
    if (c.state & kStateLocked) {
        if (unlockedCallback) {
            for (int i = 0; i < c.items; i++) {
                if (c.queue[i].dev == dev) {
                    return FALSE;
                }
            }
            if (c.items < 3) {
                c.queue[c.items].callback = unlockedCallback;
                c.queue[c.items].dev = dev;
                c.items++;
            }
        }
        return FALSE;
    }
    c.state |= kStateLocked;
    c.dev = dev;
    return TRUE;
}

BOOL EXIUnlock(s32 chan)
{
    if (chan < 0 || chan >= kChannels) {
        return FALSE;
    }
    Channel& c = gChannel[chan];
    if (!(c.state & kStateLocked)) {
        return FALSE;
    }
    c.state &= ~kStateLocked;
    if (c.items > 0) {
        EXICallback cb = c.queue[0].callback;
        c.items--;
        memmove(&c.queue[0], &c.queue[1], sizeof(c.queue[0]) * (size_t) c.items);
        cb(chan, 0);
    }
    return TRUE;
}

BOOL EXISelect(s32 chan, u32 dev, u32 frequency)
{
    (void) frequency;
    if (chan < 0 || chan >= kChannels) {
        return FALSE;
    }
    Channel& c = gChannel[chan];
    if ((c.state & kStateSelected) ||
        (chan != 2 && ((dev == 0 && !(c.state & kStateAttached) && !present(chan)) || !(c.state & kStateLocked) ||
                       c.dev != dev))) {
        return FALSE;
    }
    c.state |= kStateSelected;
    if (cardSlot(chan, dev)) {
        re4_memcard_select(chan);
    }
    return TRUE;
}

int EXISelectSD(s32 chan, u32 dev, u32 freq)
{
    return EXISelect(chan, dev, freq);
}

BOOL EXIDeselect(s32 chan)
{
    if (chan < 0 || chan >= kChannels) {
        return FALSE;
    }
    Channel& c = gChannel[chan];
    if (!(c.state & kStateSelected)) {
        return FALSE;
    }
    c.state &= ~kStateSelected;
    if (cardSlot(chan, c.dev)) {
        re4_memcard_deselect(chan);
    }
    if (chan != 2 && c.dev == 0) {
        return present(chan) ? TRUE : FALSE;
    }
    return TRUE;
}

BOOL EXIImm(s32 chan, void* buffer, s32 length, u32 type, EXICallback callback)
{
    if (chan < 0 || chan >= kChannels || length <= 0 || length > 4) {
        return FALSE;
    }
    Channel& c = gChannel[chan];
    if ((c.state & (kStateDma | kStateImm)) || !transfer(chan, buffer, length, type)) {
        return FALSE;
    }
    c.state |= kStateImm;
    if (callback) {
        c.tcCallback = callback;
        c.tcPending = true;
    }
    return TRUE;
}

BOOL EXISync(s32 chan)
{
    if (chan < 0 || chan >= kChannels) {
        return FALSE;
    }
    Channel& c = gChannel[chan];
    if (!(c.state & kStateSelected)) {
        return FALSE;
    }
    // an immediate transfer without callback ends here; with one (and a DMA)
    // its interrupt still comes
    if ((c.state & kStateImm) && !c.tcPending) {
        c.state &= ~kStateImm;
    }
    return chan == 2 || present(chan) ? TRUE : FALSE;
}

BOOL EXIImmEx(s32 chan, void* buffer, s32 length, u32 type)
{
    u8* p = (u8*) buffer;
    while (length > 0) {
        s32 n = length < 4 ? length : 4;
        if (!EXIImm(chan, p, n, type, 0) || !EXISync(chan)) {
            return FALSE;
        }
        p += n;
        length -= n;
    }
    return TRUE;
}

BOOL EXIDma(s32 chan, void* buffer, s32 length, u32 type, EXICallback callback)
{
    if (chan < 0 || chan >= kChannels) {
        return FALSE;
    }
    Channel& c = gChannel[chan];
    if ((c.state & (kStateDma | kStateImm)) || !transfer(chan, buffer, length, type)) {
        return FALSE;
    }
    c.state |= kStateDma;
    c.tcCallback = callback;
    c.tcPending = true;  // the DMA's end, as an interrupt
    return TRUE;
}

BOOL EXIProbe(s32 chan)
{
    return present(chan) ? TRUE : FALSE;
}

s32 EXIProbeEx(s32 chan)
{
    return present(chan) ? 1 : -1;
}

BOOL EXIAttach(s32 chan, EXICallback callback)
{
    if (chan < 0 || chan >= 2) {
        return FALSE;
    }
    Channel& c = gChannel[chan];
    if ((c.state & kStateAttached) || !present(chan)) {
        return FALSE;
    }
    c.extCallback = callback;
    c.state |= kStateAttached;
    return TRUE;
}

BOOL EXIDetach(s32 chan)
{
    if (chan < 0 || chan >= 2) {
        return FALSE;
    }
    Channel& c = gChannel[chan];
    if (!(c.state & kStateAttached)) {
        return TRUE;
    }
    if ((c.state & kStateLocked) && c.dev == 0) {
        return FALSE;
    }
    c.state &= ~kStateAttached;
    return TRUE;
}

u32 EXIGetState(s32 chan)
{
    return chan >= 0 && chan < kChannels ? gChannel[chan].state : 0;
}

s32 EXIGetID(s32 chan, u32 dev, u32* id)
{
    if (!id) {
        return 0;
    }
    if (chan >= 0 && chan < 2 && dev == 0 && present(chan)) {
        *id = re4_memcard_id(chan);
        return 1;
    }
    *id = 0;
    return 0;
}

void EXIProbeReset(void) {}

}  // extern "C"
