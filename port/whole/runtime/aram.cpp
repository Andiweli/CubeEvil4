// Auxiliary RAM (ARAM, the GameCube's 16 MB audio/streaming memory) and its
// DMA queue (ARQ). ARAM is a separate address space (0 .. 16 MB) that the CPU
// reaches only by DMA; the port keeps it in a host buffer and copies
// immediately, then runs the request's callback as the DMA interrupt would.
#include "re4_runtime.h"

#include <stdlib.h>
#include <string.h>

namespace {

typedef unsigned u32;

typedef void (*ARQCallback)(u32 request);
struct ARQRequest {
    ARQRequest* next;
    u32 owner;
    u32 type;
    u32 priority;
    u32 source;
    u32 dest;
    u32 length;
    ARQCallback callback;
};

const u32 kAramSize = 0x01000000;
// The first 16 KB belong to the OS/DSP (AR_ARAM_USR_BASE in the SDK).
const u32 kAramUserBase = 0x4000;

unsigned char* gAram;
bool gInit;
u32 gStackPointer;
u32* gBlockLength;
int gFreeBlocks;

unsigned char* aram()
{
    if (!gAram) {
        gAram = (unsigned char*) calloc(kAramSize, 1);
    }
    return gAram;
}

// Main-memory side of a DMA. The hardware takes a physical address and drops
// the cache-region bits; callers pass cached (0x80...) addresses, physical ones,
// or, in the port, plain host pointers to buffers outside MEM1 (static arrays
// of libre4_main).
unsigned char* mainMemory(u32 addr)
{
    if (addr < RE4_MEM1_SIZE) {
        return (unsigned char*) (unsigned long) (addr + RE4_MEM1_BASE);
    }
    if (addr >= 0xC0000000u && addr < 0xC0000000u + RE4_MEM1_SIZE) {
        return (unsigned char*) (unsigned long) (addr - 0x40000000u);
    }
    return (unsigned char*) (unsigned long) addr;
}

bool dma(u32 type, u32 mainAddr, u32 aramAddr, u32 length)
{
    if (aramAddr >= kAramSize || length > kAramSize - aramAddr) {
        re4_log("ARAM: DMA outside ARAM (type %u, main %08x, aram %08x, %u bytes)", type, mainAddr,
                aramAddr, length);
        return false;
    }
    unsigned char* mem = mainMemory(mainAddr);
    if (type == 0) {
        memcpy(aram() + aramAddr, mem, length);
    } else {
        memcpy(mem, aram() + aramAddr, length);
        // the converter's notes follow the data (event files are loaded to
        // ARAM and copied to MEM1 when the event plays)
        re4_formats_aram_copied(mem, aram() + aramAddr, length);
    }
    return true;
}

}  // namespace

extern "C" {

unsigned char* re4_aram(void)
{
    return aram();
}

u32 ARInit(u32* stackIndexAddr, u32 numEntries)
{
    if (gInit) {
        return gStackPointer;
    }
    aram();
    gBlockLength = stackIndexAddr;
    gFreeBlocks = (int) numEntries;
    gStackPointer = kAramUserBase;
    gInit = true;
    return gStackPointer;
}

int ARCheckInit(void)
{
    return gInit;
}

void ARReset(void)
{
    gInit = false;
}

// Stack allocator: blocks are handed out upwards and freed in reverse order.
u32 ARAlloc(u32 length)
{
    u32 block = gStackPointer;
    gStackPointer += length;
    if (gBlockLength && gFreeBlocks > 0) {
        *gBlockLength++ = length;
        gFreeBlocks--;
    }
    return block;
}

u32 ARFree(u32* length)
{
    if (gBlockLength) {
        gBlockLength--;
        if (length) {
            *length = *gBlockLength;
        }
        gStackPointer -= *gBlockLength;
        gFreeBlocks++;
    }
    return gStackPointer;
}

u32 ARGetBaseAddress(void)
{
    return kAramUserBase;
}

u32 ARGetSize(void)
{
    return kAramSize;
}

u32 ARGetInternalSize(void)
{
    return kAramSize;
}

u32 ARGetDMAStatus(void)
{
    return 0;
}

void ARStartDMA(u32 type, u32 mainAddr, u32 aramAddr, u32 length)
{
    dma(type, mainAddr, aramAddr, length);
}

void ARQInit(void) {}

void ARQReset(void) {}

void ARQPostRequest(ARQRequest* request, u32 owner, u32 type, u32 priority, u32 source, u32 dest,
                    u32 length, ARQCallback callback)
{
    request->next = 0;
    request->owner = owner;
    request->type = type;
    request->priority = priority;
    request->source = source;
    request->dest = dest;
    request->length = length;
    request->callback = callback;
    if (type == 0) {
        dma(type, source, dest, length);
    } else {
        dma(type, dest, source, length);
    }
    if (callback) {
        callback((u32) (unsigned long) request);
    }
}

void ARQRemoveRequest(ARQRequest* request)
{
    (void) request;
}

void ARQRemoveOwnerRequest(u32 owner)
{
    (void) owner;
}

void ARQFlushQueue(void) {}

}  // extern "C"
