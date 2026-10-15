// Compiled with the game's flags and headers (re4_game_flags) for the runtime
// test "gx": the SDK's GX library (src/lib/GX*.c, patch 0030) and the game's
// inline vertex functions (gx.h, rewritten by whole_prepare.py) feed the GX
// FIFO of the port (runtime/gx_fifo.cpp). Checks that the bytes arrive as the
// GameCube's graphics processor would read them: register encodings, vertex
// sizes (a command stream that lost its framing shows up as unknown commands),
// display lists recorded into memory and called, the PE interrupts.
#include "types.h"
#include "gx.h"

#include <string.h>

extern "C" {
void* GXInit(void* base, u32 size);
void GXBeginDisplayList(void* list, u32 size);
u32 GXEndDisplayList(void);
void GXSetDrawSync(u16 token);
typedef void (*GXDrawSyncCallback)(u16 token);
GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb);
void GXFlush(void);
void re4_gx_stats(unsigned out[11]);
unsigned re4_gx_bp(unsigned reg);
void re4_gx_deliver_interrupts(void);
}

namespace {

enum { kBytes, kCommands, kBp, kCp, kXf, kXfIndexed, kDraws, kVertices, kCalls, kUnknown, kDlBytes };

u16 gSyncToken;
int gSyncCalls;

void onSync(u16 token)
{
    gSyncToken = token;
    gSyncCalls++;
}

void stats(unsigned out[11])
{
    re4_gx_stats(out);
}

}  // namespace

// `fifo` and `list`: 32-byte aligned MEM1 memory (0x10000 and 0x400 bytes).
// Returns a bit mask of failed checks (0 = passed).
extern "C" int re4probe_gx_test(void* fifo, void* list)
{
    int failed = 0;
    unsigned s0[11], s1[11];
    GXInit(fifo, 0x10000);
    stats(s0);
    if (s0[kBp] < 30 || s0[kXf] == 0 || s0[kUnknown] != 0) {
        failed |= 0x1;  // GXInit programs every register
    }
    // BP 0x41 (cmode0): blend on, source SRCALPHA (4) bits 8-10, destination
    // INVSRCALPHA (5) bits 5-7
    GXSetBlendMode(1, 4, 5, 0);
    if ((re4_gx_bp(0x41) & 0xFE3) != (1 | 5 << 5 | 4 << 8)) {
        failed |= 0x2;
    }
    // a triangle of float positions (12 bytes a vertex)
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);                // GX_VA_POS, GX_DIRECT
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);    // XYZ, F32
    GXBegin(0x90, 0, 3);               // GX_TRIANGLES, GX_VTXFMT0
    GXPosition3f32(0.0f, 0.0f, 0.0f);
    GXPosition3f32(1.0f, 0.0f, 0.0f);
    GXPosition3f32(0.0f, 1.0f, 0.0f);
    // a quad of s16 positions, RGBA8 colours and float texture coordinates (18 bytes)
    GXSetVtxDesc(11, 1);               // GX_VA_CLR0
    GXSetVtxDesc(13, 1);               // GX_VA_TEX0
    GXSetVtxAttrFmt(1, 9, 1, 3, 0);    // XYZ, S16
    GXSetVtxAttrFmt(1, 11, 1, 5, 0);   // RGBA, RGBA8
    GXSetVtxAttrFmt(1, 13, 1, 4, 0);   // ST, F32
    GXBegin(0x80, 1, 4);               // GX_QUADS, GX_VTXFMT1
    for (int i = 0; i < 4; i++) {
        GXPosition3s16((s16) i, (s16) -i, 7);
        GXColor4u8(255, 128, 0, 255);
        GXTexCoord2f32(0.5f, 0.25f);
    }
    GXSetVtxDesc(11, 0);
    GXSetVtxDesc(13, 0);
    GXFlush();
    stats(s1);
    if (s1[kDraws] - s0[kDraws] != 2 || s1[kVertices] - s0[kVertices] != 7 || s1[kUnknown] != 0) {
        failed |= 0x4;
    }
    // a display list: recorded into memory, not drawn; then called
    memset(list, 0xEE, 0x400);
    GXBeginDisplayList(list, 0x400);
    GXBegin(0x90, 0, 3);
    GXPosition3f32(0.0f, 0.0f, 0.0f);
    GXPosition3f32(2.0f, 0.0f, 0.0f);
    GXPosition3f32(0.0f, 2.0f, 0.0f);
    u32 size = GXEndDisplayList();
    unsigned s2[11];
    stats(s2);
    const u8* dl = (const u8*) list;
    // the list may start with the vertex format the GXBegin flushed (CP writes)
    const u8* draw = (const u8*) memchr(dl, 0x90, size ? size : 1);
    if (size == 0 || size % 32 != 0 || size > 0x400 || draw == 0 || draw[1] != 0 || draw[2] != 3 ||
        s2[kDraws] != s1[kDraws]) {
        failed |= 0x8;
    }
    GXCallDisplayList(list, size);
    GXFlush();
    unsigned s3[11];
    stats(s3);
    if (s3[kCalls] - s2[kCalls] != 1 || s3[kDraws] - s2[kDraws] != 1 || s3[kVertices] - s2[kVertices] != 3 ||
        s3[kUnknown] != 0) {
        failed |= 0x10;
    }
    // PE interrupts: the draw-sync token reaches the callback, GXDrawDone returns
    gSyncCalls = 0;
    GXSetDrawSyncCallback(onSync);
    GXSetDrawSync(0xADEB);
    re4_gx_deliver_interrupts();
    if (gSyncCalls != 1 || gSyncToken != 0xADEB) {
        failed |= 0x20;
    }
    GXDrawDone();  // would wait for ever without the finish interrupt
    GXSetDrawSyncCallback(0);
    return failed;
}
