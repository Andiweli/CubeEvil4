// The GX FIFO of the port and the front end of the emulated graphics
// processor (GP). Plan: port/docs/gx-plan.md (step G1).
//
// The SDK's GX library (src/lib/GX*.c, compiled unchanged but for patch 0030)
// and the game's inline vertex functions store every command in the
// write-gather pipe; whole_prepare.py turns those stores into re4_gx_put_*.
// Where the bytes go depends on the CPU FIFO the library attached, which it
// tells the hardware through the register blocks kept here:
//   - linked to the GP (CP enable register bit 4, GXSetCPUFifo with the GP
//     FIFO): the command stream of the GP, decoded at once;
//   - not linked (a display list in the making, GXBeginDisplayList): memory at
//     the PI write pointer, as the pipe writes it on the GameCube; the
//     register keeps the 32-byte steps GXEndDisplayList reads the size from.
//
// The GP works through the stream as it arrives. What it reports back are the
// PE interrupts "draw done" (GXSetDrawDone, BP 0x45) and "token" (GXSetDrawSync,
// BP 0x48): pending until the CPU waits (OSSleepThread) or the next video
// retrace, the moments the GameCube's GP would have caught up.
#include "../include/re4_gx_fifo.h"
#include "re4_runtime.h"

#include <stdio.h>
#include <string.h>
#include <vector>

namespace {

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

// The register blocks (16-bit CP/PE/MEM, 32-bit PI), indexed as the SDK does.
u16 gCp[0x100];
u16 gPe[0x100];
u32 gPi[0x100];
u16 gMem[0x100];

const u32 kPhysicalMask = 0x03FFFFFF;

bool linked()
{
    return (gCp[1] >> 4) & 1;  // __GXFifoLink: cpEnable bit 4
}

u8* host(u32 physical)
{
    return (u8*) (unsigned long) (RE4_MEM1_BASE + (physical & kPhysicalMask));
}

u32 be32(const u8* p)
{
    return (u32) p[0] << 24 | (u32) p[1] << 16 | (u32) p[2] << 8 | p[3];
}

u16 be16(const u8* p)
{
    return (u16) (p[0] << 8 | p[1]);
}

// --- display list recording (unlinked CPU FIFO) -----------------------------

u32 gPiSet = 0xFFFFFFFF;  // PI write pointer as this file last wrote it
u32 gWrite;               // byte-exact physical write position
bool gOverflow;

void toMemory(const u8* bytes, int n)
{
    if (gPi[5] != gPiSet) {
        // the library attached another FIFO (GXSetCPUFifo): start at its pointer
        gWrite = gPi[5] & 0x03FFFFE0;
        gOverflow = false;
    }
    u32 end = (gPi[4] & kPhysicalMask) + 4;  // DisplayListFifo.top = list + size - 4
    for (int i = 0; i < n; i++) {
        if (gWrite >= end || (gWrite & kPhysicalMask) >= RE4_MEM1_SIZE) {
            gOverflow = true;
            break;
        }
        *host(gWrite++) = bytes[i];
    }
    // the pipe writes whole 32-byte bursts; GXEndDisplayList reads the size
    // from here after GXFlush has pushed 32 bytes of NOPs behind the list
    gPi[5] = (gWrite & 0x03FFFFE0) | (gOverflow ? 1u << 26 : 0);
    gPiSet = gPi[5];
}

// --- the GP ----------------------------------------------------------------

struct Stats {
    u32 bytes, commands, bp, cp, xf, xfIndexed, draws, vertices, calls, unknown, dlBytes;
};
Stats gStats;

struct Gp {
    u32 vcdLo, vcdHi;
    u32 vatA[8], vatB[8], vatC[8];
    u32 arrayBase[16], arrayStride[16];
    u32 matIndexA, matIndexB;
    u32 bp[0x100];
    u32 bpMask = 0xFFFFFF;
};
Gp gGp;

bool gPendingFinish;
bool gPendingToken;
u16 gToken;
bool gDelivering;
u32 gUnknownLogged;

unsigned componentSize(unsigned format)
{
    return format <= 1 ? 1 : format <= 3 ? 2 : 4;  // u8 s8 / u16 s16 / f32
}

unsigned colorSize(unsigned format)
{
    static const unsigned kSize[8] = {2, 3, 4, 2, 3, 4, 4, 4};  // 565 888 888x 4444 6666 8888
    return kSize[format & 7];
}

unsigned indexSize(unsigned mode)
{
    return mode == 2 ? 1 : mode == 3 ? 2 : 0;  // direct (1) is sized by the format
}

// Bytes of one vertex of format `vat` (CP VCD and VAT registers).
unsigned vertexSize(const Gp& g, unsigned vat)
{
    u32 lo = g.vcdLo, hi = g.vcdHi, a = g.vatA[vat], b = g.vatB[vat], c = g.vatC[vat];
    unsigned size = lo & 1;  // position matrix index
    for (int i = 0; i < 8; i++) {
        size += (lo >> (1 + i)) & 1;  // texture matrix indices
    }
    unsigned position = (lo >> 9) & 3;
    size += position == 1 ? ((a & 1) ? 3 : 2) * componentSize((a >> 1) & 7) : indexSize(position);
    unsigned normal = (lo >> 11) & 3;
    if (normal) {
        bool nbt = (a >> 9) & 1;
        if (normal == 1) {
            size += (nbt ? 9 : 3) * componentSize((a >> 10) & 7);
        } else {
            size += indexSize(normal) * (nbt && (a >> 31) ? 3 : 1);
        }
    }
    for (int k = 0; k < 2; k++) {
        unsigned mode = (lo >> (13 + 2 * k)) & 3;
        size += mode == 1 ? colorSize(k == 0 ? (a >> 14) & 7 : (a >> 18) & 7) : indexSize(mode);
    }
    const unsigned elements[8] = {(a >> 21) & 1, b & 1, (b >> 9) & 1, (b >> 18) & 1,
                                  (b >> 27) & 1, (c >> 5) & 1, (c >> 14) & 1, (c >> 23) & 1};
    const unsigned formats[8] = {(a >> 22) & 7, (b >> 1) & 7, (b >> 10) & 7, (b >> 19) & 7,
                                 (b >> 28) & 7, (c >> 6) & 7, (c >> 15) & 7, (c >> 24) & 7};
    for (int i = 0; i < 8; i++) {
        unsigned mode = (hi >> (2 * i)) & 3;
        size += mode == 1 ? (elements[i] ? 2 : 1) * componentSize(formats[i]) : indexSize(mode);
    }
    return size;
}

void cpWrite(u8 reg, u32 value)
{
    gStats.cp++;
    if (reg == 0x30) {
        gGp.matIndexA = value;
    } else if (reg == 0x40) {
        gGp.matIndexB = value;
    } else if (reg == 0x50) {
        gGp.vcdLo = value;
    } else if (reg == 0x60) {
        gGp.vcdHi = value;
    } else if ((reg & 0xF8) == 0x70) {
        gGp.vatA[reg & 7] = value;
    } else if ((reg & 0xF8) == 0x80) {
        gGp.vatB[reg & 7] = value;
    } else if ((reg & 0xF8) == 0x90) {
        gGp.vatC[reg & 7] = value;
    } else if ((reg & 0xF0) == 0xA0) {
        gGp.arrayBase[reg & 15] = value;
    } else if ((reg & 0xF0) == 0xB0) {
        gGp.arrayStride[reg & 15] = value;
    }
}

void bpWrite(u32 word)
{
    gStats.bp++;
    u8 reg = (u8) (word >> 24);
    u32 value = word & 0xFFFFFF;
    if (reg == 0xFE) {  // the mask for the next write
        gGp.bpMask = value;
        return;
    }
    gGp.bp[reg] = (gGp.bp[reg] & ~gGp.bpMask) | (value & gGp.bpMask);
    gGp.bpMask = 0xFFFFFF;
    switch (reg) {
    case 0x45:  // PE_DONE: everything before it is drawn
        gPendingFinish = true;
        break;
    case 0x47:  // PE_TOKEN: the token register, no interrupt
        gPe[7] = (u16) value;
        break;
    case 0x48:  // PE_TOKEN_INT
        gToken = (u16) value;
        gPendingToken = true;
        break;
    }
}

size_t command(const u8* p, size_t n, int depth);

void callDisplayList(u32 address, u32 size, int depth)
{
    gStats.calls++;
    if (depth > 0 || (address & kPhysicalMask) + size > RE4_MEM1_SIZE) {
        return;  // the GP does not nest display lists
    }
    gStats.dlBytes += size;
    const u8* list = host(address);
    size_t at = 0;
    while (at < size) {
        size_t used = command(list + at, size - at, depth + 1);
        if (used == 0) {
            break;  // a command cut off at the end of the list
        }
        at += used;
    }
}

// One command at p (n bytes available): its length, or 0 when incomplete.
size_t command(const u8* p, size_t n, int depth)
{
    if (n == 0) {
        return 0;
    }
    u8 op = p[0];
    switch (op) {
    case 0x00:  // NOP
        return 1;
    case 0x08:  // CP register
        if (n < 6) {
            return 0;
        }
        gStats.commands++;
        cpWrite(p[1], be32(p + 2));
        return 6;
    case 0x10: {  // XF registers / memory: (count - 1) << 16 | address, then the words
        if (n < 5) {
            return 0;
        }
        u32 head = be32(p + 1);
        size_t count = (head >> 16) + 1;
        if (n < 5 + 4 * count) {
            return 0;
        }
        gStats.commands++;
        gStats.xf++;
        return 5 + 4 * count;
    }
    case 0x20:
    case 0x28:
    case 0x30:
    case 0x38:  // XF loads from the indexed arrays 12..15
        if (n < 5) {
            return 0;
        }
        gStats.commands++;
        gStats.xfIndexed++;
        return 5;
    case 0x40:  // call a display list
        if (n < 9) {
            return 0;
        }
        gStats.commands++;
        callDisplayList(be32(p + 1), be32(p + 5), depth);
        return 9;
    case 0x44:  // metrics
    case 0x48:  // invalidate the vertex cache
        gStats.commands++;
        return 1;
    case 0x61:  // BP register
        if (n < 5) {
            return 0;
        }
        gStats.commands++;
        bpWrite(be32(p + 1));
        return 5;
    default:
        break;
    }
    if (op >= 0x80 && op < 0xC0) {  // draw: primitive | vertex format, count, vertices
        if (n < 3) {
            return 0;
        }
        u16 count = be16(p + 1);
        size_t size = 3 + (size_t) count * vertexSize(gGp, op & 7);
        if (n < size) {
            return 0;
        }
        gStats.commands++;
        gStats.draws++;
        gStats.vertices += count;
        return size;
    }
    if (gUnknownLogged++ < 8) {
        re4_log("GX: unknown FIFO command 0x%02x, skipped", op);
    }
    gStats.unknown++;
    return 1;
}

// The GP's input. The CPU only appends; the GP works through what has
// arrived when it matters (drain): before its interrupts are delivered, when
// the buffer grows large, and for the tests. A command cut off at the end
// waits for the rest of its bytes.
std::vector<u8> gStream;
size_t gRead;
const size_t kDrainAt = 256 * 1024;

void drain()
{
    for (;;) {
        size_t used = command(gStream.data() + gRead, gStream.size() - gRead, 0);
        if (used == 0) {
            break;
        }
        gRead += used;
    }
    if (gRead == gStream.size()) {
        gStream.clear();
        gRead = 0;
    } else if (gRead > 0) {
        gStream.erase(gStream.begin(), gStream.begin() + (long) gRead);
        gRead = 0;
    }
}

void toGp(const u8* bytes, int n)
{
    gStats.bytes += n;
    gStream.insert(gStream.end(), bytes, bytes + n);
    if (gStream.size() >= kDrainAt) {
        drain();
    }
}

void put(const u8* bytes, int n)
{
    if (linked()) {
        toGp(bytes, n);
    } else {
        toMemory(bytes, n);
    }
}

}  // namespace

extern "C" {

void re4_gx_put_u8(unsigned char v)
{
    put(&v, 1);
}

void re4_gx_put_u16(unsigned short v)
{
    const u8 b[2] = {(u8) (v >> 8), (u8) v};
    put(b, 2);
}

void re4_gx_put_u32(unsigned int v)
{
    const u8 b[4] = {(u8) (v >> 24), (u8) (v >> 16), (u8) (v >> 8), (u8) v};
    put(b, 4);
}

void re4_gx_put_s8(signed char v)
{
    re4_gx_put_u8((unsigned char) v);
}

void re4_gx_put_s16(short v)
{
    re4_gx_put_u16((unsigned short) v);
}

void re4_gx_put_s32(int v)
{
    re4_gx_put_u32((unsigned int) v);
}

void re4_gx_put_f32(float v)
{
    u32 bits;
    memcpy(&bits, &v, 4);
    re4_gx_put_u32(bits);
}

void* re4_gx_register_block(unsigned int paddr)
{
    switch (paddr) {
    case 0xC000000:
        return gCp;
    case 0xC001000:
        return gPe;
    case 0xC003000:
        return gPi;
    case 0xC004000:
        return gMem;
    }
    re4_log("GX: unknown register block 0x%08x", paddr);
    return gMem;
}

unsigned int re4_gx_efb_peek(unsigned short x, unsigned short y, int z)
{
    (void) x;
    (void) y;
    return z ? 0x00FFFFFF : 0;  // depth far, colour black
}

void re4_gx_deliver_interrupts(void)
{
    if (gDelivering) {
        return;
    }
    gDelivering = true;
    drain();
    if (gPendingToken) {
        gPendingToken = false;
        gPe[7] = gToken;
        re4_os_hw_interrupt(0x12);  // __OS_INTERRUPT_PI_PE_TOKEN
    }
    if (gPendingFinish) {
        gPendingFinish = false;
        re4_os_hw_interrupt(0x13);  // __OS_INTERRUPT_PI_PE_FINISH
    }
    gDelivering = false;
}

// Tests (runtime_tests.cpp): what the GP has seen.
void re4_gx_stats(unsigned out[11])
{
    drain();
    const u32 v[11] = {gStats.bytes, gStats.commands, gStats.bp, gStats.cp, gStats.xf, gStats.xfIndexed,
                       gStats.draws, gStats.vertices, gStats.calls, gStats.unknown, gStats.dlBytes};
    memcpy(out, v, sizeof(v));
}

unsigned re4_gx_bp(unsigned reg)
{
    drain();
    return gGp.bp[reg & 0xFF];
}

}  // extern "C"
