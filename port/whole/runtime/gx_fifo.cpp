// The GX FIFO of the port and the front end of the emulated graphics
// processor (GP). Plan: port/docs/gx-plan.md (steps G1, G2).
//
// The SDK's GX library (src/lib/GX*.c, compiled unchanged but for patches 0030
// and 0031) and the game's inline vertex functions store every command in the
// write-gather pipe; whole_prepare.py turns those stores into re4_gx_put_*.
// Where the bytes go depends on the CPU FIFO the library attached, which it
// tells the hardware through the register blocks kept here:
//   - linked to the GP (CP enable register bit 4, GXSetCPUFifo with the GP
//     FIFO): the command stream of the GP, decoded at once (gx_decode.cpp);
//   - not linked (a display list in the making, GXBeginDisplayList): memory at
//     the PI write pointer, as the pipe writes it on the GameCube; the
//     register keeps the 32-byte steps GXEndDisplayList reads the size from.
//
// What the GP reports back are the PE interrupts "draw done" (GXSetDrawDone,
// BP 0x45) and "token" (GXSetDrawSync, BP 0x48): pending until the CPU waits
// (OSSleepThread) or the next video retrace, the moments the GameCube's GP
// would have caught up.
#include "../include/re4_gx_fifo.h"
#include "gx_internal.h"
#include "re4_runtime.h"

#include <atomic>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace {

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

// The register blocks (16-bit CP/PE/MEM, 32-bit PI), indexed as the SDK does.
u16 gCp[0x100];
u16 gPe[0x100];
u32 gPi[0x100];
u16 gMem[0x100];

bool linked()
{
    return (gCp[1] >> 4) & 1;  // __GXFifoLink: cpEnable bit 4
}

// --- memory as the GP sees it ---------------------------------------------------

// Port memory outside MEM1 (static data of the libraries: the game's sprite
// display list, the SDK's default texture) gets physical addresses in a window
// above MEM1, 1 MB at a time (two windows from each first use, for data that
// crosses a window border): the SDK's pointer-to-physical conversions call
// re4_gx_physical (patch 0031) instead of masking the pointer.
const u32 kAliasBase = 0x03000000;
const u32 kAliasWindow = 0x00100000;
const int kAliasSlots = 16;
// host address of each window, 0 = free: written by the game's thread, read
// also by the GP thread (gx_thread.cpp)
std::atomic<uintptr_t> gAlias[kAliasSlots];

inline uintptr_t alias(int s)
{
    return gAlias[s].load(std::memory_order_relaxed);
}
bool gAliasFullLogged;

u32 aliasOf(uintptr_t pointer)
{
    uintptr_t window = pointer & ~(uintptr_t) (kAliasWindow - 1);
    for (int s = 0; s < kAliasSlots; s++) {
        if (alias(s) == window) {
            return kAliasBase + (u32) s * kAliasWindow + (u32) (pointer - window);
        }
    }
    for (int s = 0; s + 1 < kAliasSlots; s += 2) {
        if (alias(s) == 0 && alias(s + 1) == 0) {
            gAlias[s + 1].store(window + kAliasWindow, std::memory_order_relaxed);
            gAlias[s].store(window, std::memory_order_relaxed);
            return kAliasBase + (u32) s * kAliasWindow + (u32) (pointer - window);
        }
    }
    if (!gAliasFullLogged) {
        gAliasFullLogged = true;
        re4_log("GX: no physical window left for port memory at %p", (void*) pointer);
    }
    return 0;
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
    u32 end = (gPi[4] & 0x03FFFFFF) + 4;  // DisplayListFifo.top = list + size - 4
    for (int i = 0; i < n; i++) {
        u8* at = gWrite < end ? re4gx::memory(gWrite, 1) : nullptr;
        if (!at) {
            gOverflow = true;
            break;
        }
        *at = bytes[i];
        gWrite++;
    }
    // the pipe writes whole 32-byte bursts; GXEndDisplayList reads the size
    // from here after GXFlush has pushed 32 bytes of NOPs behind the list
    gPi[5] = (gWrite & 0x03FFFFE0) | (gOverflow ? 1u << 26 : 0);
    gPiSet = gPi[5];
}

inline void put(const u8* bytes, int n)
{
    if (linked()) {
        if (re4gx::gThreaded) {
            re4gx::ringPut(bytes, (size_t) n);
        } else {
            re4gx::feed(bytes, (size_t) n);
        }
    } else {
        toMemory(bytes, n);
    }
}

// --- the PE -------------------------------------------------------------------

// raised by the GP (the GP thread, gx_thread.cpp), taken by the game's thread
std::atomic<bool> gPendingFinish{false};
std::atomic<bool> gPendingToken{false};
std::atomic<u16> gToken{0};
bool gDelivering;

}  // namespace

namespace re4gx {

uint8_t* memory(uint32_t physical, uint32_t size)
{
    if (physical < RE4_MEM1_SIZE) {
        if (size > RE4_MEM1_SIZE - physical) {
            return nullptr;
        }
        return (uint8_t*) (uintptr_t) (RE4_MEM1_BASE + physical);
    }
    if (physical >= kAliasBase && physical < kAliasBase + kAliasSlots * kAliasWindow) {
        u32 s = (physical - kAliasBase) / kAliasWindow;
        u32 offset = (physical - kAliasBase) % kAliasWindow;
        if (alias(s) == 0) {
            return nullptr;
        }
        u32 available = kAliasWindow - offset;
        if (s + 1 < kAliasSlots && alias(s + 1) == alias(s) + kAliasWindow) {
            available += kAliasWindow;
        }
        if (size > available) {
            return nullptr;
        }
        return (uint8_t*) (alias(s) + offset);
    }
    return nullptr;
}

uint8_t* memorySpan(uint32_t physical, uint32_t* available)
{
    *available = 0;
    if (physical < RE4_MEM1_SIZE) {
        *available = RE4_MEM1_SIZE - physical;
        return (uint8_t*) (uintptr_t) (RE4_MEM1_BASE + physical);
    }
    if (physical >= kAliasBase && physical < kAliasBase + kAliasSlots * kAliasWindow) {
        u32 s = (physical - kAliasBase) / kAliasWindow;
        u32 offset = (physical - kAliasBase) % kAliasWindow;
        if (alias(s) == 0) {
            return nullptr;
        }
        u32 span = kAliasWindow - offset;
        if (s + 1 < kAliasSlots && alias(s + 1) == alias(s) + kAliasWindow) {
            span += kAliasWindow;
        }
        *available = span;
        return (uint8_t*) (alias(s) + offset);
    }
    return nullptr;
}

uint8_t* listMemory(uint32_t address, uint32_t size)
{
    if (address >= RE4_MEM1_BASE && address - RE4_MEM1_BASE < RE4_MEM1_SIZE) {
        return memory(address - RE4_MEM1_BASE, size);
    }
    if (address < kAliasBase + kAliasSlots * kAliasWindow) {
        return memory(address, size);  // physical
    }
    // a pointer to port memory outside MEM1 (it fits the word: 32-bit hosts,
    // and LP64 hosts where the game's libraries and stacks lie below 4 GB,
    // milestone 12 way A; GXCallDisplayList writes the pointer itself, e.g.
    // the sprite quad g_EspCommonDisplayList): only the game's own; any other
    // value is a stream that lost its place, and reading there would crash
    if (size == 0) {
        return nullptr;
    }
    const uint8_t* list = (const uint8_t*) (uintptr_t) address;
    // on the GP thread (gx_thread.cpp) only the game's libraries: the check of
    // the threads' stacks belongs to the game's thread
    int (*valid)(const void*) = re4gx::gThreaded ? re4_game_library_ptr : re4_valid_port_ptr;
    if (!valid(list) || !valid(list + size - 1)) {
        return nullptr;
    }
    return (uint8_t*) list;
}

void peFinish()
{
    gPendingFinish.store(true);
    if (gThreaded) {
        raised();
    }
}

void peToken(uint16_t token, bool interrupt)
{
    if (interrupt) {
        gToken.store(token);
        gPendingToken.store(true);
        if (gThreaded) {
            raised();
        }
    } else {
        __atomic_store_n(&gPe[7], token, __ATOMIC_RELAXED);  // the GP thread: the game may read it meanwhile
    }
}

bool interruptPending()
{
    return gPendingFinish.load() || gPendingToken.load();
}

}  // namespace re4gx

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

// each with a put() of its own: one call less for every value the game puts
// (a call between exported functions goes through the PLT)
void re4_gx_put_s8(signed char v)
{
    const u8 b = (u8) v;
    put(&b, 1);
}

void re4_gx_put_s16(short v)
{
    const u8 b[2] = {(u8) ((u16) v >> 8), (u8) v};
    put(b, 2);
}

void re4_gx_put_s32(int v)
{
    const u32 u = (u32) v;
    const u8 b[4] = {(u8) (u >> 24), (u8) (u >> 16), (u8) (u >> 8), (u8) u};
    put(b, 4);
}

void re4_gx_put_f32(float v)
{
    u32 u;
    memcpy(&u, &v, 4);
    const u8 b[4] = {(u8) (u >> 24), (u8) (u >> 16), (u8) (u >> 8), (u8) u};
    put(b, 4);
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

unsigned int re4_gx_physical(const void* pointer)
{
    uintptr_t p = (uintptr_t) pointer;
    if (p >= RE4_MEM1_BASE && p - RE4_MEM1_BASE < RE4_MEM1_SIZE) {
        return (unsigned int) (p - RE4_MEM1_BASE);
    }
    if (p < RE4_MEM1_SIZE) {
        return (unsigned int) p;  // a physical address already (or NULL)
    }
    return aliasOf(p);
}

void* re4_gx_cached(unsigned int physical)
{
    physical &= 0x03FFFFFF;
    if (physical >= kAliasBase) {
        return re4gx::memory(physical, 0);
    }
    return (void*) (uintptr_t) (RE4_MEM1_BASE + physical);
}

unsigned int re4_gx_efb_peek(unsigned short x, unsigned short y, int z)
{
    (void) x;
    (void) y;
    return z ? 0x00FFFFFF : 0;  // depth far, colour black
}

namespace {

void deliverRaised()
{
    if (gPendingToken.exchange(false)) {
        gPe[7] = gToken.load();
        re4_os_hw_interrupt(0x12);  // __OS_INTERRUPT_PI_PE_TOKEN
    }
    if (gPendingFinish.exchange(false)) {
        re4_os_hw_interrupt(0x13);  // __OS_INTERRUPT_PI_PE_FINISH
    }
}

}  // namespace

void re4_gx_deliver_interrupts(void)
{
    if (gDelivering) {
        return;
    }
    gDelivering = true;
    // the GP catches up first: on the game's thread what has arrived, on the GP
    // thread (gx_thread.cpp) everything sent so far
    if (re4gx::gThreaded) {
        re4gx::sync();
    } else {
        re4gx::drain();
    }
    deliverRaised();
    gDelivering = false;
}

void re4_gx_deliver_pending(void)
{
    if (gDelivering) {
        return;
    }
    gDelivering = true;
    // the GP thread goes on meanwhile: what it raised so far (the game's thread
    // waits for it only in the dispatcher's idle, re4_gx_idle_wait)
    if (re4gx::gThreaded) {
        re4gx::flush();  // what the GP thread has not been handed yet (it may be "draw done")
    } else {
        re4gx::drain();
    }
    deliverRaised();
    gDelivering = false;
}

}  // extern "C"
