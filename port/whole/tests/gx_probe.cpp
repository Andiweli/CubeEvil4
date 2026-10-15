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
unsigned re4_gx_xf(unsigned address);
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
    // colours reach the registers with R in the high byte, whatever the host's
    // byte order (patch 0033): TEV colour, konstant colour, material colour,
    // light colour, fog colour
    {
        GXColor c = {10, 20, 30, 40};
        GXSetTevColor(1, c);  // GX_TEVREG0: BP 0xE2 (r, a), 0xE3 (b, g)
        bool tev = (re4_gx_bp(0xE2) & 0x7FF) == 10 && ((re4_gx_bp(0xE2) >> 12) & 0x7FF) == 40 &&
                   (re4_gx_bp(0xE3) & 0x7FF) == 30 && ((re4_gx_bp(0xE3) >> 12) & 0x7FF) == 20;
        GXSetTevKColor(2, c);  // GX_KCOLOR2: BP 0xE4/0xE5 with the konst bit
        bool konst = (re4_gx_bp(0xE4) & 0x7FF) == 10 && ((re4_gx_bp(0xE4) >> 12) & 0x7FF) == 40 &&
                     (re4_gx_bp(0xE5) & 0x7FF) == 30 && ((re4_gx_bp(0xE5) >> 12) & 0x7FF) == 20;
        GXSetChanMatColor(4, c);  // GX_COLOR0A0: XF 0x100C
        bool mat = re4_gx_xf(0x100C) == 0x0A141E28u;
        GXLightObj light;
        memset(&light, 0, sizeof(light));
        GXInitLightColor(&light, c);
        GXLoadLightObjImm(&light, 1);  // GX_LIGHT0: XF 0x600, colour word at +3
        bool lit = re4_gx_xf(0x603) == 0x0A141E28u;
        GXSetFog(2, 0.0f, 1.0f, 0.1f, 1.0f, c);  // fog colour: BP 0xF2 = r g b
        bool fog = (re4_gx_bp(0xF2) & 0xFFFFFF) == 0x0A141Eu;
        if (!tev || !konst || !mat || !lit || !fog) {
            failed |= 0x40;
        }
    }
    // GXSetTevOp builds its register words from tables (patch 0036: words as
    // the GameCube holds them, not bit-fields in the host's order)
    {
        GXSetTevOp(0, 0);  // GX_TEVSTAGE0, GX_MODULATE: BP 0xC0: a=ZERO b=TEXC c=RASC d=ZERO, clamp
        bool c0 = (re4_gx_bp(0xC0) & 0xFFFFFF) == 0x08F8AFu;
        bool a0 = (re4_gx_bp(0xC1) & 0xFFFFF0) == 0x08F2F0u;  // a=ZERO b=TEXA c=RASA d=ZERO
        GXSetTevOp(1, 3);  // GX_TEVSTAGE1, GX_REPLACE: BP 0xC2: d=TEXC; 0xC3: d=TEXA
        bool c1 = (re4_gx_bp(0xC2) & 0xFFFFFF) == 0x08FFF8u;
        bool a1 = (re4_gx_bp(0xC3) & 0xFFFFF0) == 0x08FFC0u;
        if (!c0 || !a0 || !c1 || !a1) {
            failed |= 0x80;
        }
    }
    return failed;
}

// --- G2: the decoder and the chunks for the renderer ----------------------------

#define RE4_GX_FRAME_IO_WRITE_ONLY  // game flags: no STL here
#include "../runtime/re4_gx_frame_io.h"

#include <stdio.h>
#include <stdlib.h>

extern "C" {
void GXLoadPosMtxIndx(u16 index, u32 id);
void re4_gx_put_u8(unsigned char v);
void re4_gx_put_u16(unsigned short v);
unsigned re4_gx_xf(unsigned address);
unsigned int re4_gx_physical(const void* pointer);
void re4_gx_host_order_list(const void* list, unsigned int size);
}

namespace {

// What a chunk holds, in op order.
struct Parsed {
    const Re4GxDraw* draws[16];
    int drawCount;
    const Re4GxTexture* textures[16];
    int textureBefore[16];  // index of the draw the texture op precedes
    int textureCount;
    const uint32_t* tluts[8];
    int tlutCount;
    const uint32_t* xf[64];  // payloads: address, words
    uint32_t xfWords[64];
    int xfCount;
    bool bpStateFirst;
    int unknown;
};

void parse(const Re4GxChunk* c, Parsed& p)
{
    memset(&p, 0, sizeof(p));
    uint32_t at = 0;
    bool first = true;
    while (at < c->opWords) {
        uint32_t head = c->ops[at];
        uint32_t op = head & 0xFF;
        uint32_t n = head >> 8;
        const uint32_t* payload = c->ops + at + 1;
        if (first) {
            p.bpStateFirst = op == RE4_GX_OP_BP_STATE && n == 0x100;
            first = false;
        }
        if (op == RE4_GX_OP_DRAW && p.drawCount < 16) {
            p.draws[p.drawCount++] = (const Re4GxDraw*) payload;
        } else if (op == RE4_GX_OP_TEXTURE && p.textureCount < 16) {
            p.textureBefore[p.textureCount] = p.drawCount;
            p.textures[p.textureCount++] = (const Re4GxTexture*) payload;
        } else if (op == RE4_GX_OP_TLUT && p.tlutCount < 8) {
            p.tluts[p.tlutCount++] = payload;
        } else if (op == RE4_GX_OP_XF && p.xfCount < 64) {
            p.xfWords[p.xfCount] = n - 1;
            p.xf[p.xfCount++] = payload;
        } else if (op < RE4_GX_OP_BP || op > RE4_GX_OP_BP_STATE) {
            p.unknown++;
        }
        at += 1 + n;
    }
}

uint32_t bits(f32 f)
{
    uint32_t v;
    memcpy(&v, &f, 4);
    return v;
}

const f32* vertexFloats(const Re4GxChunk* c, const Re4GxDraw* d, uint32_t vertex, uint32_t offset)
{
    return (const f32*) (c->vertices + d->vertexOffset + vertex * d->stride + offset);
}

bool near(f32 a, f32 b)
{
    return a - b < 0.0001f && b - a < 0.0001f;
}

u16 sStaticPalette[16] __attribute__((aligned(32)));

void copyDisp(void* xfb)
{
    GXSetDispCopySrc(0, 0, 640, 448);
    GXSetDispCopyDst(640, 448);
    GXCopyDisp(xfb, 1);
}

void triangle()
{
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXBegin(0x90, 0, 3);
    GXPosition3f32(0.0f, 0.0f, 0.0f);
    GXPosition3f32(1.0f, 0.0f, 0.0f);
    GXPosition3f32(0.0f, 1.0f, 0.0f);
}

}  // namespace

// `mem`: 64 KB of 32-byte aligned MEM1 scratch memory, `fifo`: 64 KB for the
// GX FIFO. A synthetic frame through the SDK's GX library; returns a bit mask
// of failed checks (0 = passed).
extern "C" int re4probe_gx_frame_test(void* fifo, void* mem)
{
    int failed = 0;
    u8* m = (u8*) mem;
    s16* posArr = (s16*) (m + 0x000);
    u8* clrArr = m + 0x040;
    f32* mtxArr = (f32*) (m + 0x080);
    u8* texRgb = m + 0x100;
    u8* texC4 = m + 0x120;
    u8* palette = m + 0x140;
    u8* hostList = m + 0x200;
    u8* copyDest = m + 0x1000;
    u8* xfb = m + 0x2000;
    memset(m, 0, 0x3000);

    GXInit(fifo, 0x10000);
    if (re4_core_gx_acquire() != 0) {  // attaches; nothing built yet
        failed |= 0x1;
    }
    copyDisp(xfb);  // the chunk starts at this frame boundary
    if (re4_core_gx_acquire() != 0) {
        failed |= 0x1;
    }

    // a position matrix, loaded directly
    f32 mtx[3][4] = {{1, 0, 0, 5}, {0, 1, 0, 6}, {0, 0, 1, 7}};
    GXLoadPosMtxImm(mtx, 0);

    // a textured quad: s16 positions (index8, host order, 2 fraction bits),
    // RGBA8 colours (index16), float texture coordinates (direct)
    for (int i = 0; i < 4; i++) {
        posArr[i * 3 + 0] = (s16) (4 * i);
        posArr[i * 3 + 1] = (s16) (-8 * i);
        posArr[i * 3 + 2] = 12;
        clrArr[i * 4 + 0] = (u8) (10 + i);
        clrArr[i * 4 + 1] = 20;
        clrArr[i * 4 + 2] = 30;
        clrArr[i * 4 + 3] = 255;
    }
    for (int i = 0; i < 32; i++) {
        texRgb[i] = (u8) (i * 7 + 1);
    }
    GXTexObj tex;
    GXInitTexObj(&tex, texRgb, 4, 4, 5, 0, 0, 0);  // RGB5A3, clamp
    GXLoadTexObj(&tex, 0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0, 0, 4);  // stage 0: coord 0, map 0, colour 0
    GXClearVtxDesc();
    GXSetVtxDesc(9, 2);   // position index8
    GXSetVtxDesc(11, 3);  // colour 0 index16
    GXSetVtxDesc(13, 1);  // texture coordinate 0 direct
    GXSetVtxAttrFmt(1, 9, 1, 3, 2);   // XYZ s16, 2 fraction bits
    GXSetVtxAttrFmt(1, 11, 1, 5, 0);  // RGBA8
    GXSetVtxAttrFmt(1, 13, 1, 4, 0);  // ST f32
    GXSetArray(9, posArr, 6);
    GXSetArray(11, clrArr, 4);
    GXBegin(0x80, 1, 4);
    for (int i = 0; i < 4; i++) {
        re4_gx_put_u8((u8) i);
        re4_gx_put_u16((u16) (3 - i));
        GXTexCoord2f32(0.25f * i, 0.5f);
    }

    // a triangle strip with the same texture: no new texture op
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXBegin(0x98, 0, 5);
    for (int i = 0; i < 5; i++) {
        GXPosition3f32((f32) i, (f32) (i & 1), 0.0f);
    }

    // a display list the CPU built in host order (like eff_sys.cpp EspInit)
    {
        memset(hostList, 0, 64);
        hostList[0] = 0x90;
        u16 count = 3;
        memcpy(hostList + 1, &count, 2);
        const f32 v[9] = {1.5f, 2.5f, 3.5f, -1.0f, 0.0f, 2.0f, 8.0f, 9.0f, 10.0f};
        memcpy(hostList + 3, v, sizeof(v));
        re4_gx_host_order_list(hostList, 64);
        GXCallDisplayList(hostList, 64);
    }

    // a position matrix loaded by index from a host-order array
    for (int k = 0; k < 24; k++) {
        mtxArr[k] = 0.5f * k;
    }
    GXSetArray(21, mtxArr, 48);  // GX_POS_MTX_ARRAY
    GXLoadPosMtxIndx(1, 3);      // matrix 1 to PNMTX1 (XF 12)

    // a palette and a C4 texture using it
    for (int i = 0; i < 32; i++) {
        palette[i] = (u8) (0x80 + i);
        texC4[i] = (u8) (i * 3);
    }
    GXTlutObj tlut;
    GXInitTlutObj(&tlut, palette, 2, 16);  // RGB5A3, 16 entries
    GXLoadTlut(&tlut, 0);
    GXTexObj ci;
    GXInitTexObjCI(&ci, texC4, 8, 8, 8, 0, 0, 0, 0);
    GXLoadTexObj(&ci, 0);
    triangle();

    // a palette in port memory outside MEM1
    for (int i = 0; i < 16; i++) {
        sStaticPalette[i] = (u16) (0x1234 + i);
    }
    GXTlutObj tlut2;
    GXInitTlutObj(&tlut2, sStaticPalette, 2, 16);
    GXLoadTlut(&tlut2, 1);

    // an EFB copy to a texture, then bound
    GXSetTexCopySrc(0, 0, 8, 8);
    GXSetTexCopyDst(8, 8, 6, 0);
    GXCopyTex(copyDest, 0);
    GXTexObj copied;
    GXInitTexObj(&copied, copyDest, 8, 8, 6, 0, 0, 0);
    GXLoadTexObj(&copied, 0);
    triangle();

    copyDisp(xfb);
    const Re4GxChunk* c1 = re4_core_gx_acquire();
    if (!c1 || c1->version != RE4_GX_FRAME_VERSION || c1->flags != (RE4_GX_CHUNK_RESET | RE4_GX_CHUNK_XFB) ||
        c1->xfbAddress != re4_gx_physical(xfb) || c1->xfbWidth != 640 || c1->xfbHeight != 448) {
        return failed | 0x2;
    }
    if (const char* dump = getenv("RE4_GX_TEST_DUMP")) {  // the renderer's host tools can draw it
        if (FILE* f = fopen(dump, "wb")) {
            re4gxio::Write(f, *c1, 0);
            fclose(f);
        }
    }
    Parsed p;
    parse(c1, p);
    if (!p.bpStateFirst || p.unknown != 0 || p.drawCount != 5) {
        failed |= 0x2;
    }
    // the matrix
    bool matrix = false;
    for (int i = 0; i < p.xfCount; i++) {
        if (p.xf[i][0] == 0 && p.xfWords[i] == 12 && p.xf[i][1 + 3] == bits(5.0f) && p.xf[i][1 + 7] == bits(6.0f) &&
            p.xf[i][1 + 11] == bits(7.0f) && p.xf[i][1] == bits(1.0f)) {
            matrix = true;
        }
    }
    if (!matrix) {
        failed |= 0x4;
    }
    // the quad
    if (p.drawCount >= 1) {
        const Re4GxDraw* d = p.draws[0];
        bool ok = (d->primitive & 0xFF) == RE4_GX_TRIANGLES && ((d->primitive >> 8) & 0xF8) == 0x80 &&
                  ((d->primitive >> 16) & 7) == 1 && d->layout == (RE4_GX_V_COLOR0 | RE4_GX_V_TEX0) &&
                  d->stride == 24 && d->vertexCount == 4 && d->indexCount == 6;
        static const u16 kQuad[6] = {0, 1, 2, 0, 2, 3};
        for (int i = 0; ok && i < 6; i++) {
            ok = c1->indices[d->indexOffset + i] == kQuad[i];
        }
        for (uint32_t v = 0; ok && v < 4; v++) {
            const f32* pos = vertexFloats(c1, d, v, 0);
            const u8* rgba = c1->vertices + d->vertexOffset + v * d->stride + 12;
            const f32* st = vertexFloats(c1, d, v, 16);
            ok = near(pos[0], (f32) v) && near(pos[1], -2.0f * v) && near(pos[2], 3.0f) && rgba[0] == 13 - v &&
                 rgba[1] == 20 && rgba[3] == 255 && near(st[0], 0.25f * v) && near(st[1], 0.5f);
        }
        if (!ok) {
            failed |= 0x8;
        }
    }
    // the RGB5A3 texture: once, before the quad, with its texels
    {
        bool ok = p.textureCount >= 1 && p.textureBefore[0] == 0;
        const Re4GxTexture* t = ok ? p.textures[0] : 0;
        ok = ok && t->map == 0 && t->format == 5 && t->width == 4 && t->height == 4 && t->levels == 1 &&
             t->dataBytes == 32 && t->flags == 0 && t->address == re4_gx_physical(texRgb) &&
             memcmp(c1->blob + t->dataOffset, texRgb, 32) == 0;
        // the strip and the list draw with it bound: no texture op until the C4 one
        ok = ok && p.textureCount >= 2 && p.textureBefore[1] == 3;
        if (!ok) {
            failed |= 0x10;
        }
    }
    // the strip: every second triangle flipped back
    if (p.drawCount >= 2) {
        const Re4GxDraw* d = p.draws[1];
        static const u16 kStrip[9] = {0, 1, 2, 2, 1, 3, 2, 3, 4};
        bool ok = d->indexCount == 9 && d->layout == 0 && d->stride == 12 && d->vertexCount == 5;
        for (int i = 0; ok && i < 9; i++) {
            ok = c1->indices[d->indexOffset + i] == kStrip[i];
        }
        if (!ok) {
            failed |= 0x20;
        }
    }
    // the host-order display list
    if (p.drawCount >= 3) {
        const Re4GxDraw* d = p.draws[2];
        bool ok = d->vertexCount == 3 && d->indexCount == 3;
        if (ok) {
            const f32* a = vertexFloats(c1, d, 0, 0);
            const f32* c = vertexFloats(c1, d, 2, 0);
            ok = near(a[0], 1.5f) && near(a[1], 2.5f) && near(a[2], 3.5f) && near(c[0], 8.0f) && near(c[2], 10.0f);
        }
        if (!ok) {
            failed |= 0x40;
        }
    }
    // the indexed matrix load
    for (int k = 0; k < 12; k++) {
        if (re4_gx_xf(12 + k) != bits(0.5f * (12 + k))) {
            failed |= 0x80;
        }
    }
    // palettes: the MEM1 one and the one outside MEM1
    {
        bool mem1 = false, outside = false;
        for (int i = 0; i < p.tlutCount; i++) {
            const uint32_t* t = p.tluts[i];
            if (t[1] >= 32 && memcmp(c1->blob + t[2], palette, 32) == 0) {  // the region size is loaded
                mem1 = true;
            }
            if (t[1] >= 32 && memcmp(c1->blob + t[2], sStaticPalette, 32) == 0) {
                outside = true;
            }
        }
        const Re4GxTexture* t = p.textureCount >= 2 ? p.textures[1] : 0;
        if (!mem1 || !t || t->format != 8 || t->width != 8 || t->dataBytes != 32 || t->tlut == 0 ||
            (t->tlut >> 10) != 2) {
            failed |= 0x100;
        }
        if (!outside) {
            failed |= 0x400;
        }
    }
    // the EFB copy bound as a texture
    {
        const Re4GxTexture* t = p.textureCount >= 3 ? p.textures[2] : 0;
        if (!t || p.textureBefore[2] != 4 || !(t->flags & RE4_GX_TEX_COPY) || t->dataBytes != 0 ||
            t->address != re4_gx_physical(copyDest)) {
            failed |= 0x200;
        }
    }

    // the next frame: the RGB5A3 texture again, sent before (no data)
    GXLoadTexObj(&tex, 0);
    triangle();
    copyDisp(xfb);
    const Re4GxChunk* c2 = re4_core_gx_acquire();
    if (!c2 || c2->flags != RE4_GX_CHUNK_XFB || c2->sequence != c1->sequence + 1) {
        failed |= 0x800;
    } else {
        Parsed q;
        parse(c2, q);
        const Re4GxTexture* t = q.textureCount == 1 ? q.textures[0] : 0;
        const Re4GxTexture* t1 = p.textures[0];
        if (!t || t->dataBytes != 0 || t->keyLo != t1->keyLo || t->keyHi != t1->keyHi || q.drawCount != 1) {
            failed |= 0x800;
        }
    }
    re4_core_gx_release(c1);
    if (c2) {
        re4_core_gx_release(c2);
    }
    re4_core_gx_detach();
    copyDisp(xfb);  // no renderer: nothing is built
    if (re4_core_gx_acquire() != 0) {
        failed |= 0x1000;
    }
    re4_core_gx_detach();
    return failed;
}

// `frames` frames of one triangle each, ended by a display copy (the queue
// test of runtime_tests.cpp runs the renderer side in another thread).
extern "C" void re4probe_gx_produce(void* fifo, void* xfb, int frames, int init)
{
    if (init) {
        GXInit(fifo, 0x10000);
    }
    for (int i = 0; i < frames; i++) {
        triangle();
        copyDisp(xfb);
    }
}
