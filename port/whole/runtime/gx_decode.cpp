// The command decoder of the emulated graphics processor (GP) and the queue of
// decoded chunks to the renderer. Plan: port/docs/gx-plan.md (step G2);
// what a renderer gets: re4_gx_frame.h.
//
// The GP works through the command stream at once, as each command completes
// (gx_fifo.cpp feeds the bytes): vertex arrays, display lists, matrices,
// textures and palettes are read while the game still has them as they were
// when it sent the command. It keeps the register state (CP, XF, BP, the
// palette part of TMEM) and, while a renderer takes chunks, writes the
// decoded work into the current chunk.
//
// Byte order. On the GameCube everything the GP reads is big-endian. In the
// port:
//   - command streams are big-endian: the FIFO (re4_gx_put_* stores
//     big-endian), display lists recorded with GXBeginDisplayList, display
//     lists from the disc (the Formatumwandler leaves primitive streams as
//     they are);
//   - display lists the game builds with CPU stores (effect grids, the sprite
//     quad) are host order; the game registers them (re4_gx_host_order_list,
//     patch 0031) and the decoder reads them so;
//   - vertex arrays and matrices loaded by index are host order: the
//     Formatumwandler turned positions, normals and texture coordinates of
//     the disc to host order, the game's CPU code writes its buffers
//     (skinned positions, effect grids, matrices) in host order; bytes
//     (colours, u8/s8 data) are the same either way;
//   - texels and palette entries stay big-endian (the renderer decodes them).
#include "gx_internal.h"
#include "re4_gx_frame.h"
#include "re4_runtime.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include <vector>

namespace {

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

inline u16 rd16(const u8* p, bool be)
{
    return be ? (u16) (p[0] << 8 | p[1]) : (u16) (p[1] << 8 | p[0]);
}

inline u32 rd32(const u8* p, bool be)
{
    return be ? (u32) p[0] << 24 | (u32) p[1] << 16 | (u32) p[2] << 8 | p[3]
              : (u32) p[3] << 24 | (u32) p[2] << 16 | (u32) p[1] << 8 | p[0];
}

inline float asFloat(u32 v)
{
    float f;
    memcpy(&f, &v, 4);
    return f;
}

inline u32 rotl(u32 v, int s)
{
    return v << s | v >> (32 - s);
}

inline u32 fmix(u32 h)
{
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

// 64-bit content hash (two 32-bit lanes: fast on ARM32).
u64 hashBytes(const u8* p, size_t n, u64 seed)
{
    u32 h1 = (u32) seed ^ 0x9E3779B9u;
    u32 h2 = (u32) (seed >> 32) ^ 0x7F4A7C15u;
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        u32 a, b;
        memcpy(&a, p + i, 4);
        memcpy(&b, p + i + 4, 4);
        h1 = rotl(h1 ^ (a * 0xCC9E2D51u), 15) * 5 + 0xE6546B64u;
        h2 = rotl(h2 ^ (b * 0x1B873593u), 13) * 5 + 0x561CCD1Bu;
    }
    for (; i < n; i++) {
        h1 = rotl(h1 ^ (p[i] * 0xCC9E2D51u), 15) * 5 + 0xE6546B64u;
    }
    h1 = fmix(h1 ^ (u32) n);
    h2 = fmix(h2 + h1);
    return (u64) h2 << 32 | h1;
}

u64 hashWords(const u32* w, int n)
{
    return hashBytes((const u8*) w, (size_t) n * 4, 0x5245344758ull);
}

// --- GP state -----------------------------------------------------------------

struct Gp {
    u32 vcdLo, vcdHi;
    u32 vatA[8], vatB[8], vatC[8];
    u32 arrayBase[16], arrayStride[16];
    u32 matIndexA, matIndexB;
    u32 bp[0x100];
    u32 bpMask = 0xFFFFFF;
    u32 xfMem[0x800];  // XF memory 0x0000-0x07FF (matrices, lights)
    u32 xfReg[0x58];   // XF registers 0x1000-0x1057
};
Gp gGp;

const u32 kTlutBytes = 0x80000;  // the TLUT half of TMEM (BP 0x65 address << 9)
u8 gTlut[kTlutBytes];
bool gTlutLoaded[kTlutBytes >> 9];

struct Stats {
    u32 bytes, commands, bp, cp, xf, xfIndexed, draws, vertices, calls, unknown, dlBytes;
    u32 xfbCopies, texCopies, tlutLoads, textures, textureBytes, chunks, dropped, hostLists, skipped, held;
};
Stats gStats;
u32 gUnknownLogged;

// The last commands of the stream (depth 0), logged once when an unknown
// command shows that the stream lost its place: which command was read with
// a wrong length, and in what vertex format.
struct Recent {
    u8 op;
    u32 bytes, count, vertexSize, vcdLo, vcdHi, vatA;
};
Recent gRecent[16];
u32 gRecentAt;
bool gDesyncLogged;

void remember(u8 op, size_t bytes, u32 count, u32 vertexSize)
{
    Recent& r = gRecent[gRecentAt++ % 16];
    r.op = op;
    r.bytes = (u32) bytes;
    r.count = count;
    r.vertexSize = vertexSize;
    r.vcdLo = gGp.vcdLo;
    r.vcdHi = gGp.vcdHi;
    r.vatA = gGp.vatA[op & 7];
}

void logRecent(const u8* p, size_t n)
{
    if (gDesyncLogged) {
        return;
    }
    gDesyncLogged = true;
    re4_log("GX: the command stream lost its place; the commands before (oldest first):");
    for (u32 i = 0; i < 16; i++) {
        const Recent& r = gRecent[(gRecentAt + i) % 16];
        if (r.bytes == 0) {
            continue;
        }
        if (r.op >= 0x80 && r.op < 0xC0) {
            re4_log("GX:   draw 0x%02x: %u vertices of %u bytes (%u bytes), VCD %08x %08x, VAT A %08x", r.op, r.count,
                    r.vertexSize, r.bytes, r.vcdLo, r.vcdHi, r.vatA);
        } else {
            re4_log("GX:   command 0x%02x (%u bytes)", r.op, r.bytes);
        }
    }
    char bytes[3 * 16 + 1] = {0};
    for (size_t i = 0; i < n && i < 16; i++) {
        snprintf(bytes + 3 * i, 4, "%02x ", p[i]);
    }
    re4_log("GX:   then: %s", bytes);
}

// Display lists the game builds with CPU stores, in host order (patch 0031).
struct HostList {
    u32 size;
    u64 hash;
};
std::unordered_map<uintptr_t, HostList> gHostLists;

// EFB copies to a texture leave this marker at their destination: the
// renderer has the pixels, a texture bound there later is that copy as long
// as the marker is intact (memory the game reloads loses it).
const u8 kCopyMagic[8] = {'R', 'E', '4', 'E', 'F', 'B', 'C', 'P'};
u32 gCopySerial;

// --- chunks -------------------------------------------------------------------

struct Chunk {
    std::vector<u32> ops;
    std::vector<u8> vertices;
    std::vector<u16> indices;
    std::vector<u8> blob;
    Re4GxChunk view;
    // ops.size() right after the last draw record (0: none); a draw that
    // follows it with nothing in between may join it (draw())
    size_t lastDrawEnd = 0;

    void clear()
    {
        ops.clear();
        vertices.clear();
        indices.clear();
        blob.clear();
        memset(&view, 0, sizeof(view));
        lastDrawEnd = 0;
    }
};

const int kSlots = 4;
// kPending: ended with a display copy, waits for the game to put it on screen
// (VISetNextFrameBuffer before VIFlush) or not (SYS_SCREEN_STOP holds the old
// picture while an event or a room is set up)
enum SlotState { kFree, kFilling, kPending, kReady, kReading };
Chunk gSlots[kSlots];
SlotState gSlotState[kSlots];
std::deque<int> gReady;
std::mutex gMutex;
std::condition_variable gCv;
std::atomic<bool> gWanted(false);
// re4_core_gx_notify: the core process's server takes a chunk as soon as it is queued
std::atomic<void (*)(void)> gNotify(nullptr);
std::atomic<long long> gLastPoll(0);
const long long kStallMs = 1000;

int gCurrent = -1;  // slot being filled (core thread); -1: not recording
int gPending = -1;  // slot waiting for the game's VIFlush (kPending); -1: none
u32 gViNext;        // physical address of the XFB the VI shows next (0: never set)
bool gViBlack;      // the VI outputs black (VISetBlack, from VIFlush on): RE4_GX_CHUNK_BLACK
u32 gSequence;
bool gNeedReset = true;

// Textures the renderer has: key -> last chunk that used it, bytes (as sent,
// and as the renderer holds them decoded: RGBA8, gx_texture.cpp).
struct Sent {
    u32 lastChunk;
    u32 bytes;
    u32 decoded;
};
std::unordered_map<u64, Sent> gSent;
u64 gSentBytes;
u64 gDecodedBytes;
const u32 kForgetAfter = 600;           // chunks (10-20 s)
const u64 kSentBudget = 96u << 20;      // texture bytes the renderer keeps at most
// Decoded: a movie (game/sofdec.cpp) brings two new textures every frame, its
// Y8 and C4:4 planes (1.4 MB decoded at 512 x 448); without this limit the
// renderer would hold some 400 MB of past frames before kForgetAfter.
const u64 kDecodedBudget = 256u << 20;

// Per chunk: texture descriptor -> key (hashing once per chunk), and what each
// map is bound to.
struct Bound {
    u64 key;  // texels (with the palette for CI formats: added at each bind)
    u32 flags;
    u32 dataBytes;
    u32 address;
};
std::unordered_map<u64, Bound> gTexMemo;
// A register bindTextures() reads, a palette or an EFB copy changed since the
// last draw, or a chunk began: the maps are bound again.
bool gTexturesDirty = true;
u64 gMapKey[8];
u32 gMapFlags[8];
bool gMapValid[8];

long long nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

Chunk* chunk()
{
    return gCurrent >= 0 ? &gSlots[gCurrent] : nullptr;
}

void emit(u32 op, const u32* payload, u32 n)
{
    Chunk* c = chunk();
    c->ops.push_back(op | n << 8);
    c->ops.insert(c->ops.end(), payload, payload + n);
}

u32 blobAppend(const u8* data, u32 n)
{
    Chunk* c = chunk();
    u32 at = (u32) c->blob.size();
    c->blob.insert(c->blob.end(), data, data + n);
    while (c->blob.size() & 31) {
        c->blob.push_back(0);  // the next item 32-byte aligned
    }
    return at;
}

void emitXfRange(u32 address, u32 count)
{
    std::vector<u32> payload(count + 1);
    payload[0] = address;
    for (u32 i = 0; i < count; i++) {
        u32 a = address + i;
        payload[1 + i] = a < 0x800 ? gGp.xfMem[a] : gGp.xfReg[a - 0x1000];
    }
    emit(RE4_GX_OP_XF, payload.data(), count + 1);
}

void emitTlutRange(u32 offset, u32 bytes)
{
    u32 at = blobAppend(gTlut + offset, bytes);
    const u32 payload[3] = {offset, bytes, at};
    emit(RE4_GX_OP_TLUT, payload, 3);
}

void emitFullState()
{
    emit(RE4_GX_OP_BP_STATE, gGp.bp, 0x100);
    emitXfRange(0x000, 0x100);
    emitXfRange(0x400, 0x60);
    emitXfRange(0x500, 0x100);
    emitXfRange(0x600, 0x80);
    emitXfRange(0x1000, 0x58);
    for (u32 b = 0; b < (kTlutBytes >> 9);) {
        if (!gTlutLoaded[b]) {
            b++;
            continue;
        }
        u32 e = b;
        while (e < (kTlutBytes >> 9) && gTlutLoaded[e]) {
            e++;
        }
        emitTlutRange(b << 9, (e - b) << 9);
        b = e;
    }
}

void forgetTextures()
{
    if (gSequence % 64 != 0 && gSentBytes <= kSentBudget && gDecodedBytes <= kDecodedBudget) {
        return;
    }
    std::vector<std::pair<u32, u64>> old;
    for (auto& e : gSent) {
        old.push_back(std::make_pair(e.second.lastChunk, e.first));
    }
    std::sort(old.begin(), old.end());
    for (auto& o : old) {
        bool stale = o.first + kForgetAfter < gSequence;
        if (!stale && gSentBytes <= kSentBudget && gDecodedBytes <= kDecodedBudget) {
            break;
        }
        auto it = gSent.find(o.second);
        gSentBytes -= it->second.bytes;
        gDecodedBytes -= it->second.decoded;
        gSent.erase(it);
        const u32 payload[2] = {(u32) o.second, (u32) (o.second >> 32)};
        emit(RE4_GX_OP_FORGET, payload, 2);
    }
}

// Starts a chunk in a free slot (gMutex held). False when none is free.
bool beginLocked()
{
    int slot = -1;
    for (int i = 0; i < kSlots; i++) {
        if (gSlotState[i] == kFree) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return false;
    }
    gSlotState[slot] = kFilling;
    gCurrent = slot;
    Chunk* c = chunk();
    c->clear();
    c->view.sequence = ++gSequence;
    gTexMemo.clear();
    memset(gMapValid, 0, sizeof(gMapValid));
    gTexturesDirty = true;
    if (gNeedReset) {
        gNeedReset = false;
        c->view.flags |= RE4_GX_CHUNK_RESET;
        gSent.clear();
        gSentBytes = 0;
        gDecodedBytes = 0;
        emitFullState();
    } else {
        forgetTextures();
    }
    return true;
}

// Queues the pending chunk for the renderer (gMutex held). A picture the VI
// does not switch to (the game skipped VISetNextFrameBuffer, Render_swap with
// SYS_SCREEN_STOP) is drawn but not shown: RE4_GX_CHUNK_HOLD.
void publishPendingLocked()
{
    if (gPending < 0) {
        return;
    }
    if (!gWanted.load()) {
        // the renderer went away meanwhile: nobody takes it
        gSlotState[gPending] = kFree;
        gStats.dropped++;
        gPending = -1;
        return;
    }
    Re4GxChunk& v = gSlots[gPending].view;
    if (gViNext != 0 && gViNext != v.xfbAddress) {
        v.flags |= RE4_GX_CHUNK_HOLD;
        gStats.held++;
    }
    if (gViBlack) {
        v.flags |= RE4_GX_CHUNK_BLACK;
    }
    gSlotState[gPending] = kReady;
    gReady.push_back(gPending);
    gPending = -1;
    gStats.chunks++;
    gCv.notify_all();
    if (void (*notify)(void) = gNotify.load()) {
        notify();
    }
}

void dropQueueLocked()
{
    for (int s : gReady) {
        gSlotState[s] = kFree;
        gStats.dropped++;
    }
    if (gPending >= 0) {
        gSlotState[gPending] = kFree;
        gStats.dropped++;
        gPending = -1;
    }
    gReady.clear();
    gNeedReset = true;
}

// Timing for the log: game time per chunk (display copy to display copy) and
// the part of it the core waited for a free slot (the renderer is behind).
long long gBoundaryAt;
long long gFrameMs, gWaitMs;
u32 gTimedChunks;
long long gIdleAt;                    // re4_idle_ns() at the last log line
double gFifoBytes, gFifoVertices;     // since the last log line (FIFO: written by the CPU)
double gListBytes;                    // display list bytes read since the last log line

// A display copy ends the chunk: queue it, start the next one if a renderer
// still takes chunks. Waits while every slot is queued or being read.
void chunkBoundary(u32 xfb, u32 width, u32 height)
{
    const long long start = nowMs();
    if (gBoundaryAt && gWanted.load()) {
        gFrameMs += start - gBoundaryAt;
        if (++gTimedChunks == 600) {
            // idle: asleep for the next retrace (time to spare); what is neither is the
            // game's own work plus this decoder
            const long long idle = re4_idle_ns();
            re4_log("GX core: 600 chunks, %.1f ms each, of it %.1f ms waiting for the renderer, %.1f ms idle, "
                    "%.0f KB FIFO, %.0f KB display lists, %.0f vertices",
                    gFrameMs / 600.0, gWaitMs / 600.0, (idle - gIdleAt) / 1e6 / 600.0, gFifoBytes / 1024.0 / 600.0,
                    gListBytes / 1024.0 / 600.0, gFifoVertices / 600.0);
            gIdleAt = idle;
            gTimedChunks = 0;
            gFrameMs = gWaitMs = 0;
            gFifoBytes = gFifoVertices = gListBytes = 0;
        }
    }
    gBoundaryAt = start;
    struct WaitTimer {
        long long from;
        ~WaitTimer() { gWaitMs += nowMs() - from; }
    } waited = {start};
    std::unique_lock<std::mutex> lock(gMutex);
    if (gCurrent >= 0) {
        Chunk* c = chunk();
        Re4GxChunk& v = c->view;
        v.version = RE4_GX_FRAME_VERSION;
        v.flags |= RE4_GX_CHUNK_XFB;
        v.ops = c->ops.data();
        v.opWords = (u32) c->ops.size();
        v.vertices = c->vertices.data();
        v.vertexBytes = (u32) c->vertices.size();
        v.indices = c->indices.data();
        v.indexCount = (u32) c->indices.size();
        v.blob = c->blob.data();
        v.blobBytes = (u32) c->blob.size();
        v.xfbAddress = xfb;
        v.xfbWidth = width;
        v.xfbHeight = height;
        // a copy without VIFlush since the last one: decided now
        publishPendingLocked();
        gSlotState[gCurrent] = kPending;
        gPending = gCurrent;
        gCurrent = -1;
    }
    if (!gWanted.load()) {
        if (!gReady.empty() || gPending >= 0) {
            dropQueueLocked();
        }
        gNeedReset = true;
        return;
    }
    while (!beginLocked()) {
        if (!gWanted.load() || nowMs() - gLastPoll.load() > kStallMs) {
            // the renderer stopped taking chunks: drop the queue, record again
            // (from a full state) once it asks for chunks
            gWanted.store(false);
            dropQueueLocked();
            return;
        }
        gCv.wait_for(lock, std::chrono::milliseconds(20));
    }
}

// --- vertex formats -------------------------------------------------------------

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

float component(const u8* p, unsigned format, bool be, float scale)
{
    switch (format) {
    case 0:
        return p[0] * scale;
    case 1:
        return (signed char) p[0] * scale;
    case 2:
        return rd16(p, be) * scale;
    case 3:
        return (short) rd16(p, be) * scale;
    default:
        return asFloat(rd32(p, be));
    }
}

inline u8 x5(u32 v)
{
    return (u8) (v << 3 | v >> 2);
}

inline u8 x6(u32 v)
{
    return (u8) (v << 2 | v >> 4);
}

void color(const u8* p, unsigned format, bool be, u8 out[4])
{
    u32 v;
    switch (format) {
    case 0:  // RGB565
        v = rd16(p, be);
        out[0] = x5(v >> 11);
        out[1] = x6((v >> 5) & 63);
        out[2] = x5(v & 31);
        out[3] = 255;
        break;
    case 1:  // RGB888
    case 2:  // RGB888x
        out[0] = p[0];
        out[1] = p[1];
        out[2] = p[2];
        out[3] = 255;
        break;
    case 3:  // RGBA4444
        v = rd16(p, be);
        out[0] = (u8) ((v >> 12) * 17);
        out[1] = (u8) (((v >> 8) & 15) * 17);
        out[2] = (u8) (((v >> 4) & 15) * 17);
        out[3] = (u8) ((v & 15) * 17);
        break;
    case 4:  // RGBA6666, 24 bits
        v = (u32) p[0] << 16 | (u32) p[1] << 8 | p[2];
        out[0] = x6(v >> 18);
        out[1] = x6((v >> 12) & 63);
        out[2] = x6((v >> 6) & 63);
        out[3] = x6(v & 63);
        break;
    default:  // RGBA8888
        out[0] = p[0];
        out[1] = p[1];
        out[2] = p[2];
        out[3] = p[3];
        break;
    }
}

// The vertex format of one draw: what the stream holds per vertex and where
// each attribute goes in the chunk's vertex layout.
struct Format {
    u32 inSize;
    bool pnmtx, texmtx[8], anyMatrix;
    u32 posMode, posFormat, posComps;
    float posScale;
    u32 nrmMode, nrmFormat;
    bool nbt, nbt3;
    float nrmScale;
    u32 clrMode[2], clrFormat[2];
    u32 texMode[8], texFormat[8], texComps[8];
    float texScale[8];
    u32 layout, stride;
    u32 offNormal, offBt, offColor[2], offTex[8], offMatrix;
    u8 defaults[9];
    // derived for the vertex loop (draw()): bytes of each attribute's element,
    // the texture coordinates and texture matrix indices present, in order
    u32 posBytes, nrmCs, nrmComps, clrBytes[2], texCs[8], texBytes[8];
    u8 texList[8], texCount;
    u8 mtxList[8], mtxCount;
};

void describe(u32 vat, Format& f)
{
    const Gp& g = gGp;
    u32 lo = g.vcdLo, hi = g.vcdHi, a = g.vatA[vat], b = g.vatB[vat], c = g.vatC[vat];
    memset(&f, 0, sizeof(f));
    f.pnmtx = lo & 1;
    f.anyMatrix = f.pnmtx;
    u32 size = f.pnmtx;
    for (int i = 0; i < 8; i++) {
        f.texmtx[i] = (lo >> (1 + i)) & 1;
        f.anyMatrix |= f.texmtx[i];
        size += f.texmtx[i];
    }
    f.posMode = (lo >> 9) & 3;
    f.posComps = (a & 1) ? 3 : 2;
    f.posFormat = (a >> 1) & 7;
    f.posScale = 1.0f / (float) (1u << ((a >> 4) & 31));
    size += f.posMode == 1 ? f.posComps * componentSize(f.posFormat) : indexSize(f.posMode);
    f.nrmMode = (lo >> 11) & 3;
    f.nbt = (a >> 9) & 1;
    f.nrmFormat = (a >> 10) & 7;
    f.nrmScale = componentSize(f.nrmFormat) == 1 ? 1.0f / 64 : 1.0f / 16384;
    f.nbt3 = f.nbt && (a >> 31);
    if (f.nrmMode == 1) {
        size += (f.nbt ? 9 : 3) * componentSize(f.nrmFormat);
    } else if (f.nrmMode) {
        size += indexSize(f.nrmMode) * (f.nbt3 ? 3 : 1);
    }
    for (int k = 0; k < 2; k++) {
        f.clrMode[k] = (lo >> (13 + 2 * k)) & 3;
        f.clrFormat[k] = k == 0 ? (a >> 14) & 7 : (a >> 18) & 7;
        size += f.clrMode[k] == 1 ? colorSize(f.clrFormat[k]) : indexSize(f.clrMode[k]);
    }
    const u32 comps[8] = {(a >> 21) & 1, b & 1, (b >> 9) & 1, (b >> 18) & 1,
                          (b >> 27) & 1, (c >> 5) & 1, (c >> 14) & 1, (c >> 23) & 1};
    const u32 formats[8] = {(a >> 22) & 7, (b >> 1) & 7, (b >> 10) & 7, (b >> 19) & 7,
                            (b >> 28) & 7, (c >> 6) & 7, (c >> 15) & 7, (c >> 24) & 7};
    const u32 shifts[8] = {(a >> 25) & 31, (b >> 4) & 31, (b >> 13) & 31, (b >> 22) & 31,
                           c & 31, (c >> 9) & 31, (c >> 18) & 31, (c >> 27) & 31};
    for (int i = 0; i < 8; i++) {
        f.texMode[i] = (hi >> (2 * i)) & 3;
        f.texComps[i] = comps[i] ? 2 : 1;
        f.texFormat[i] = formats[i];
        f.texScale[i] = 1.0f / (float) (1u << shifts[i]);
        size += f.texMode[i] == 1 ? f.texComps[i] * componentSize(f.texFormat[i]) : indexSize(f.texMode[i]);
    }
    f.inSize = size;

    // output layout (re4_gx_frame.h)
    u32 at = 12;
    if (f.nrmMode) {
        f.layout |= RE4_GX_V_NORMAL;
        f.offNormal = at;
        at += 12;
        if (f.nbt) {
            f.layout |= RE4_GX_V_BINORMAL_TANGENT;
            f.offBt = at;
            at += 24;
        }
    }
    for (int k = 0; k < 2; k++) {
        if (f.clrMode[k]) {
            f.layout |= k == 0 ? RE4_GX_V_COLOR0 : RE4_GX_V_COLOR1;
            f.offColor[k] = at;
            at += 4;
        }
    }
    for (int i = 0; i < 8; i++) {
        if (f.texMode[i]) {
            f.layout |= RE4_GX_V_TEX0 << i;
            f.offTex[i] = at;
            at += 8;
        }
    }
    if (f.anyMatrix) {
        f.layout |= RE4_GX_V_MATRIX;
        f.offMatrix = at;
        at += 12;
    }
    f.stride = at;
    u32 ia = g.matIndexA, ib = g.matIndexB;
    f.defaults[0] = ia & 63;
    f.defaults[1] = (ia >> 6) & 63;
    f.defaults[2] = (ia >> 12) & 63;
    f.defaults[3] = (ia >> 18) & 63;
    f.defaults[4] = (ia >> 24) & 63;
    f.defaults[5] = ib & 63;
    f.defaults[6] = (ib >> 6) & 63;
    f.defaults[7] = (ib >> 12) & 63;
    f.defaults[8] = (ib >> 18) & 63;

    f.posBytes = f.posComps * componentSize(f.posFormat);
    f.nrmCs = componentSize(f.nrmFormat);
    f.nrmComps = f.nbt ? 9 : 3;
    for (int k = 0; k < 2; k++) {
        f.clrBytes[k] = colorSize(f.clrFormat[k]);
    }
    for (int i = 0; i < 8; i++) {
        f.texCs[i] = componentSize(f.texFormat[i]);
        f.texBytes[i] = f.texComps[i] * f.texCs[i];
        if (f.texMode[i]) {
            f.texList[f.texCount++] = (u8) i;
        }
        if (f.texmtx[i]) {
            f.mtxList[f.mtxCount++] = (u8) i;
        }
    }
}

// The vertex formats as the CP registers describe them now: worked out once
// after a change of VCD, VAT or MATINDEX (cpWrite), not for every draw
// command (describe() for each draw and each size check was a few percent of
// the core's time).
Format gFormat[8];
bool gFormatValid[8];

const Format& format(u32 vat)
{
    if (!gFormatValid[vat]) {
        describe(vat, gFormat[vat]);
        gFormatValid[vat] = true;
    }
    return gFormat[vat];
}

unsigned vertexSize(u32 vat)
{
    return format(vat).inSize;
}

// An indexed attribute: the element `index` of array `array` (host order),
// `bytes` long; NULL when outside memory.
const u8 kZero[64] = {0};

// An indexed array as host memory, resolved once per draw (element() went
// through re4gx::memory for every attribute of every vertex: a tenth of the
// core's time in the village).
struct Array {
    const u8* base;  // null: the array is not memory
    u32 limit;       // bytes from base that are memory
    u32 stride;
};

Array array(u32 index)
{
    Array a;
    a.stride = gGp.arrayStride[index];
    a.base = re4gx::memorySpan(gGp.arrayBase[index], &a.limit);
    return a;
}

inline const u8* at(const Array& a, u32 index, u32 bytes)
{
    const u32 offset = index * a.stride;
    return a.base && offset < a.limit && bytes <= a.limit - offset ? a.base + offset : nullptr;
}

// One float into the vertex being written (out is zero where nothing is put)
inline void put(u8* out, float v)
{
    memcpy(out, &v, 4);
}

// The components of one attribute, converted as component() does, by a
// function chosen once per draw for the format, the count and the byte order
// (instead of a switch for every component of every vertex).
typedef void (*Reader)(u8* out, const u8* src, float scale);

template <unsigned Format, bool BE>
inline float convert(const u8* p, float scale)
{
    if (Format == 0) {
        return p[0] * scale;
    }
    if (Format == 1) {
        return (signed char) p[0] * scale;
    }
    if (Format == 2) {
        return rd16(p, BE) * scale;
    }
    if (Format == 3) {
        return (short) rd16(p, BE) * scale;
    }
    return asFloat(rd32(p, BE));
}

template <unsigned Format, unsigned Comps, bool BE>
void readComponents(u8* out, const u8* src, float scale)
{
    const unsigned cs = Format <= 1 ? 1 : Format <= 3 ? 2 : 4;
    for (unsigned k = 0; k < Comps; k++) {
        put(out + 4 * k, convert<Format, BE>(src + k * cs, scale));
    }
}

template <unsigned Format, bool BE>
Reader readerFor(unsigned comps)
{
    switch (comps) {
    case 1:
        return readComponents<Format, 1, BE>;
    case 2:
        return readComponents<Format, 2, BE>;
    case 3:
        return readComponents<Format, 3, BE>;
    default:
        return readComponents<Format, 9, BE>;
    }
}

// format: 0 u8, 1 s8, 2 u16, 3 s16, 4 and above f32 (componentSize, component)
Reader readerFor(unsigned format, unsigned comps, bool be)
{
    switch (format) {
    case 0:
        return be ? readerFor<0, true>(comps) : readerFor<0, false>(comps);
    case 1:
        return be ? readerFor<1, true>(comps) : readerFor<1, false>(comps);
    case 2:
        return be ? readerFor<2, true>(comps) : readerFor<2, false>(comps);
    case 3:
        return be ? readerFor<3, true>(comps) : readerFor<3, false>(comps);
    default:
        return be ? readerFor<4, true>(comps) : readerFor<4, false>(comps);
    }
}

// Decodes `count` vertices at `data` into the chunk, appends the indices.
void draw(u8 op, u32 count, const u8* data, bool be)
{
    gStats.draws++;
    gStats.vertices += count;
    gFifoVertices += count;
    Chunk* c = chunk();
    if (!c || count == 0) {
        return;
    }
    u32 vat = op & 7;
    const Format& f = format(vat);
    // the arrays this format reads
    Array pos = {}, nrm = {}, clr[2] = {}, tex[8] = {};
    if (f.posMode >= 2) {
        pos = array(0);
    }
    if (f.nrmMode >= 2) {
        nrm = array(1);
    }
    for (int k = 0; k < 2; k++) {
        if (f.clrMode[k] >= 2) {
            clr[k] = array(2 + k);
        }
    }
    for (u32 j = 0; j < f.texCount; j++) {
        const u32 i = f.texList[j];
        if (f.texMode[i] >= 2) {
            tex[i] = array(4 + i);
        }
    }
    // the readers: direct data in the stream's byte order, arrays in host order
    const bool posDirect = f.posMode == 1, nrmDirect = f.nrmMode == 1;
    const Reader posRead = readerFor(f.posFormat, f.posComps, posDirect && be);
    const Reader nrmRead = readerFor(f.nrmFormat, f.nrmComps, nrmDirect && be);
    const Reader nrmPartRead = readerFor(f.nrmFormat, 3, false);  // one of three indexed parts (NBT3)
    Reader texRead[8];
    for (u32 j = 0; j < f.texCount; j++) {
        const u32 i = f.texList[j];
        texRead[i] = readerFor(f.texFormat[i], f.texComps[i], f.texMode[i] == 1 && be);
    }
    u32 base = (u32) c->vertices.size();
    c->vertices.resize(base + (size_t) count * f.stride);
    u8* out = c->vertices.data() + base;
    const u8* in = data;
    const u32 posIndex = indexSize(f.posMode), nrmIndex = indexSize(f.nrmMode);
    for (u32 v = 0; v < count; v++, out += f.stride) {
        if (f.anyMatrix) {  // the matrix indices: the defaults, then those of the vertex
            u8* matrix = out + f.offMatrix;
            memcpy(matrix, f.defaults, 9);
            if (f.pnmtx) {
                matrix[0] = *in++ & 63;
            }
            for (u32 j = 0; j < f.mtxCount; j++) {
                matrix[1 + f.mtxList[j]] = *in++ & 63;
            }
        }
        // position (the vertex bytes start as zeros: what is not there stays 0)
        {
            const u8* src = nullptr;
            if (f.posMode == 1) {
                src = in;
                in += f.posBytes;
            } else if (f.posMode) {
                u32 index = f.posMode == 2 ? *in : rd16(in, be);
                in += posIndex;
                if ((f.posMode == 2 && index == 0xFF) || (f.posMode == 3 && index == 0xFFFF)) {
                    gStats.skipped++;  // GX skips such a vertex; kept here (rare)
                }
                src = at(pos, index, f.posBytes);
            }
            if (src) {
                posRead(out, src, f.posScale);
            }
        }
        // normal (binormal, tangent)
        if (f.nrmMode) {
            // normal, then binormal and tangent: the three sit one after the
            // other from offNormal (offBt = offNormal + 12)
            u8* o = out + f.offNormal;
            const u32 cs = f.nrmCs, comps = f.nrmComps;
            if (f.nrmMode == 1) {
                nrmRead(o, in, f.nrmScale);
                in += comps * cs;
            } else if (f.nbt3) {
                // three indices: normal, binormal, tangent, each the part of its element
                for (u32 part = 0; part < 3; part++) {
                    u32 index = f.nrmMode == 2 ? *in : rd16(in, be);
                    in += nrmIndex;
                    if (const u8* src = at(nrm, index, 9 * cs)) {
                        nrmPartRead(o + 12 * part, src + part * 3 * cs, f.nrmScale);
                    }
                }
            } else {
                u32 index = f.nrmMode == 2 ? *in : rd16(in, be);
                in += nrmIndex;
                if (const u8* src = at(nrm, index, comps * cs)) {
                    nrmRead(o, src, f.nrmScale);
                }
            }
        }
        for (int k = 0; k < 2; k++) {
            if (!f.clrMode[k]) {
                continue;
            }
            u8 rgba[4] = {0, 0, 0, 255};
            if (f.clrMode[k] == 1) {
                color(in, f.clrFormat[k], be, rgba);
                in += f.clrBytes[k];
            } else {
                u32 index = f.clrMode[k] == 2 ? *in : rd16(in, be);
                in += indexSize(f.clrMode[k]);
                const u8* src = at(clr[k], index, f.clrBytes[k]);
                color(src ? src : kZero, f.clrFormat[k], false, rgba);
            }
            memcpy(out + f.offColor[k], rgba, 4);
        }
        for (u32 j = 0; j < f.texCount; j++) {
            const u32 i = f.texList[j];
            const u8* src = nullptr;
            if (f.texMode[i] == 1) {
                src = in;
                in += f.texBytes[i];
            } else {
                u32 index = f.texMode[i] == 2 ? *in : rd16(in, be);
                in += indexSize(f.texMode[i]);
                src = at(tex[i], index, f.texBytes[i]);
            }
            if (src) {
                texRead[i](out + f.offTex[i], src, f.texScale[i]);
            }
        }
    }

    // indices, relative to the first vertex: written in place (one insert per
    // triangle was a few percent)
    u32 first = (u32) c->indices.size();
    u32 primitive = RE4_GX_TRIANGLES;
    std::vector<u16>& ix = c->indices;
    const u32 kind = op & 0xF8;
    size_t added = 0;
    switch (kind) {
    case 0x80:
    case 0x88:
        added = (size_t) (count / 4) * 6;
        break;
    case 0x90:
        added = (size_t) (count / 3) * 3;
        break;
    case 0x98:
    case 0xA0:
        added = count >= 3 ? (size_t) (count - 2) * 3 : 0;
        break;
    case 0xA8:
        added = (size_t) (count / 2) * 2;
        break;
    case 0xB0:
        added = count >= 2 ? (size_t) (count - 1) * 2 : 0;
        break;
    default:
        added = count;
        break;
    }
    ix.resize(first + added);
    u16* w = ix.data() + first;
    switch (kind) {
    case 0x80:  // quads
    case 0x88:  // quads (second encoding)
        for (u32 q = 0; q + 3 < count; q += 4) {
            w[0] = (u16) q;
            w[1] = (u16) (q + 1);
            w[2] = (u16) (q + 2);
            w[3] = (u16) q;
            w[4] = (u16) (q + 2);
            w[5] = (u16) (q + 3);
            w += 6;
        }
        break;
    case 0x90:  // triangles
        for (u32 i = 0; i + 2 < count; i += 3) {
            w[0] = (u16) i;
            w[1] = (u16) (i + 1);
            w[2] = (u16) (i + 2);
            w += 3;
        }
        break;
    case 0x98:  // triangle strip: every second triangle flipped back
        for (u32 i = 2; i < count; i++) {
            u32 k = i - 2;
            w[0] = (u16) ((k & 1) ? k + 1 : k);
            w[1] = (u16) ((k & 1) ? k : k + 1);
            w[2] = (u16) i;
            w += 3;
        }
        break;
    case 0xA0:  // triangle fan
        for (u32 i = 2; i < count; i++) {
            w[0] = 0;
            w[1] = (u16) (i - 1);
            w[2] = (u16) i;
            w += 3;
        }
        break;
    case 0xA8:  // lines
        primitive = RE4_GX_LINES;
        for (u32 i = 0; i + 1 < count; i += 2) {
            w[0] = (u16) i;
            w[1] = (u16) (i + 1);
            w += 2;
        }
        break;
    case 0xB0:  // line strip
        primitive = RE4_GX_LINES;
        for (u32 i = 1; i < count; i++) {
            w[0] = (u16) (i - 1);
            w[1] = (u16) i;
            w += 2;
        }
        break;
    default:  // points
        primitive = RE4_GX_POINTS;
        for (u32 i = 0; i < count; i++) {
            *w++ = (u16) i;
        }
        break;
    }
    Re4GxDraw d;
    d.primitive = primitive | (u32) (op & 0xF8) << 8 | vat << 16;
    d.layout = f.layout;
    d.stride = f.stride;
    d.vertexOffset = base;
    d.vertexCount = count;
    d.indexOffset = first;
    d.indexCount = (u32) ix.size() - first;
    d.matrices[0] = f.defaults[0] | f.defaults[1] << 8 | f.defaults[2] << 16 | (u32) f.defaults[3] << 24;
    d.matrices[1] = f.defaults[4] | f.defaults[5] << 8 | f.defaults[6] << 16 | (u32) f.defaults[7] << 24;
    d.matrices[2] = f.defaults[8];
    c->view.vertexCount += count;
    // A draw right after another one (no state op between them) with the same
    // primitive class, vertex layout and default matrices joins it: one
    // renderer draw instead of one per GXBegin (a forest frame of r100 has
    // about 16 000 GXBegins and 470 such runs). The triangle order stays.
    const u32 words = sizeof(Re4GxDraw) / 4;
    if (c->lastDrawEnd && c->ops.size() == c->lastDrawEnd) {
        Re4GxDraw last;
        memcpy(&last, &c->ops[c->lastDrawEnd - words], sizeof(last));
        if ((last.primitive & 0xFF) == primitive && last.layout == d.layout && last.stride == d.stride &&
            memcmp(last.matrices, d.matrices, sizeof(d.matrices)) == 0 &&
            last.vertexOffset + last.vertexCount * last.stride == base && last.indexOffset + last.indexCount == first &&
            last.vertexCount + count <= 0x10000) {
            for (size_t i = first; i < ix.size(); i++) {
                ix[i] = (u16) (ix[i] + last.vertexCount);
            }
            last.vertexCount += count;
            last.indexCount += d.indexCount;
            memcpy(&c->ops[c->lastDrawEnd - words], &last, sizeof(last));
            return;
        }
    }
    emit(RE4_GX_OP_DRAW, (const u32*) &d, words);
    c->lastDrawEnd = c->ops.size();
    c->view.draws++;
}

// --- textures -------------------------------------------------------------------

void tileShape(u32 format, u32* w, u32* h, u32* bytes)
{
    *bytes = 32;
    switch (format) {
    case 0:   // I4
    case 8:   // C4
    case 14:  // CMPR
        *w = 8;
        *h = 8;
        break;
    case 1:  // I8
    case 2:  // IA4
    case 9:  // C8
        *w = 8;
        *h = 4;
        break;
    case 6:  // RGBA8: two 32-byte halves
        *w = 4;
        *h = 4;
        *bytes = 64;
        break;
    default:  // IA8, RGB565, RGB5A3, C14X2
        *w = 4;
        *h = 4;
        break;
    }
}

u32 textureBytes(u32 format, u32 width, u32 height, u32 levels)
{
    u32 tw, th, tb, total = 0;
    tileShape(format, &tw, &th, &tb);
    for (u32 l = 0; l < levels; l++) {
        total += ((width + tw - 1) / tw) * ((height + th - 1) / th) * tb;
        width = width > 1 ? width >> 1 : 1;
        height = height > 1 ? height >> 1 : 1;
    }
    return total;
}

u32 mipChain(u32 width, u32 height)
{
    u32 n = 1;
    for (u32 m = width > height ? width : height; m > 1; m >>= 1) {
        n++;
    }
    return n;
}

bool isCi(u32 format)
{
    return format == 8 || format == 9 || format == 10;
}

u32 tlutEntries(u32 format)
{
    return format == 8 ? 16 : format == 9 ? 256 : 16384;
}

u32 mapRegister(u32 map, u32 offset)
{
    return (map < 4 ? 0x80 + map : 0xA0 + (map - 4)) + offset;
}

u32 gLoggedMissing;

void bindMap(u32 map)
{
    const u32* bp = gGp.bp;
    u32 mode0 = bp[mapRegister(map, 0x00)];
    u32 mode1 = bp[mapRegister(map, 0x04)];
    u32 image0 = bp[mapRegister(map, 0x08)];
    u32 image3 = bp[mapRegister(map, 0x14)];
    u32 tlut = bp[mapRegister(map, 0x18)];
    Re4GxTexture t;
    memset(&t, 0, sizeof(t));
    t.map = map;
    t.address = (image3 & 0x1FFFFF) << 5;
    t.width = (image0 & 0x3FF) + 1;
    t.height = ((image0 >> 10) & 0x3FF) + 1;
    t.format = (image0 >> 20) & 0xF;
    bool mip = ((mode0 >> 5) & 3) != 0;  // HW min filter with a mip mode
    u32 maxLod = (mode1 >> 8) & 0xFF;
    t.levels = 1;
    if (mip) {
        t.levels = (maxLod + 15) / 16 + 1;
        u32 chain = mipChain(t.width, t.height);
        t.levels = t.levels > chain ? chain : t.levels;
    }
    t.tlut = isCi(t.format) ? tlut & 0xFFF : 0;
    const u32 desc[3] = {t.address, image0 & 0xFFFFFF, t.levels};
    u64 dkey = hashWords(desc, 3);
    auto memo = gTexMemo.find(dkey);
    Bound bound;
    if (memo != gTexMemo.end()) {
        bound = memo->second;
    } else {
        bound.key = 0;
        bound.flags = 0;
        bound.address = t.address;
        bound.dataBytes = textureBytes(t.format, t.width, t.height, t.levels);
        const u8* marker = re4gx::memory(t.address, 32);
        u32 serial = 0;
        if (marker && memcmp(marker, kCopyMagic, 8) == 0) {
            u32 check;
            memcpy(&serial, marker + 8, 4);
            memcpy(&check, marker + 12, 4);
            if (check != (serial ^ t.address ^ 0x5A5A5A5Au)) {
                serial = 0;
            }
        }
        if (serial) {
            bound.flags = RE4_GX_TEX_COPY;
            bound.key = (u64) serial << 32 | t.address;
            bound.dataBytes = 0;
        } else {
            const u8* data = re4gx::memory(t.address, bound.dataBytes);
            if (!data) {
                if (gLoggedMissing++ < 8) {
                    re4_log("GX: texture map %u at 0x%08x (%ux%u format %u) is not in memory", map, t.address,
                            t.width, t.height, t.format);
                }
                bound.dataBytes = 0;
            } else {
                const u32 shape[4] = {t.format, t.width, t.height, t.levels};
                bound.key = hashBytes(data, bound.dataBytes, hashWords(shape, 4)) | 1;
            }
        }
        gTexMemo[dkey] = bound;
    }
    if (isCi(t.format) && bound.key != 0 && !(bound.flags & RE4_GX_TEX_COPY)) {
        // the palette as TMEM holds it now (a load may change it any time)
        u32 offset = (t.tlut & 0x3FF) << 9;
        u32 bytes = tlutEntries(t.format) * 2;
        if (offset + bytes > kTlutBytes) {
            bytes = kTlutBytes - offset;
        }
        bound.key ^= hashBytes(gTlut + offset, bytes, (t.tlut >> 10) & 3) & ~1ull;
    }
    if (gMapValid[map] && gMapKey[map] == bound.key && gMapFlags[map] == bound.flags) {
        auto s = gSent.find(bound.key);
        if (s != gSent.end()) {
            s->second.lastChunk = gSequence;
        }
        return;
    }
    gMapValid[map] = true;
    gMapKey[map] = bound.key;
    gMapFlags[map] = bound.flags;
    t.keyLo = (u32) bound.key;
    t.keyHi = (u32) (bound.key >> 32);
    t.flags = bound.flags;
    if (bound.key != 0 && !(bound.flags & RE4_GX_TEX_COPY)) {
        auto s = gSent.find(bound.key);
        if (s == gSent.end()) {
            const u8* data = re4gx::memory(t.address, bound.dataBytes);
            t.dataOffset = blobAppend(data, bound.dataBytes);
            t.dataBytes = bound.dataBytes;
            Sent sent;
            sent.lastChunk = gSequence;
            sent.bytes = bound.dataBytes;
            sent.decoded = 0;
            for (u32 l = 0, w = t.width, h = t.height; l < t.levels; l++) {
                sent.decoded += w * h * 4;
                w = w > 1 ? w >> 1 : 1;
                h = h > 1 ? h >> 1 : 1;
            }
            gSent[bound.key] = sent;
            gSentBytes += bound.dataBytes;
            gDecodedBytes += sent.decoded;
            gStats.textures++;
            gStats.textureBytes += bound.dataBytes;
            chunk()->view.textureBytes += bound.dataBytes;
        } else {
            s->second.lastChunk = gSequence;
        }
    }
    emit(RE4_GX_OP_TEXTURE, (const u32*) &t, sizeof(t) / 4);
}

// The texture maps the TEV stages and the indirect stages of the next draw
// read (BP genmode, TEV orders 0x28-0x2F, RAS1_IREF 0x27). Nothing to do while
// none of that changed since the last draw (gTexturesDirty): the same maps
// with the same keys (bindMap per draw was a few percent of the core's time).
void bindTextures()
{
    if (!gTexturesDirty) {
        return;
    }
    gTexturesDirty = false;
    const u32* bp = gGp.bp;
    u32 genmode = bp[0x00];
    u32 used = 0;
    u32 stages = ((genmode >> 10) & 15) + 1;
    for (u32 s = 0; s < stages; s++) {
        u32 word = bp[0x28 + s / 2];
        u32 shift = (s & 1) ? 12 : 0;
        if ((word >> (shift + 6)) & 1) {
            used |= 1u << ((word >> shift) & 7);
        }
    }
    u32 indirect = (genmode >> 16) & 7;
    for (u32 k = 0; k < indirect && k < 4; k++) {
        used |= 1u << ((bp[0x27] >> (6 * k)) & 7);
    }
    for (u32 map = 0; map < 8; map++) {
        if (used & (1u << map)) {
            bindMap(map);
        }
    }
}

// --- registers -------------------------------------------------------------------

void cpWrite(u8 reg, u32 value)
{
    gStats.cp++;
    if (reg == 0x30 || reg == 0x40 || reg == 0x50 || reg == 0x60) {
        memset(gFormatValid, 0, sizeof(gFormatValid));  // every format reads these
    } else if (reg >= 0x70 && reg < 0xA0) {
        gFormatValid[reg & 7] = false;
    }
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

u32* xfSlot(u32 address)
{
    if (address < 0x800) {
        return &gGp.xfMem[address];
    }
    if (address >= 0x1000 && address < 0x1058) {
        return &gGp.xfReg[address - 0x1000];
    }
    return nullptr;
}

void xfWrite(u32 address, const u8* data, u32 count, bool be)
{
    bool changed = false;
    if (address + count <= 0x800) {  // XF memory (matrices, lights): the common case
        u32* slot = &gGp.xfMem[address];
        for (u32 i = 0; i < count; i++) {
            u32 v = rd32(data + 4 * i, be);
            changed |= slot[i] != v;
            slot[i] = v;
        }
    } else {
        for (u32 i = 0; i < count; i++) {
            u32 v = rd32(data + 4 * i, be);
            u32* slot = xfSlot(address + i);
            if (slot && *slot != v) {
                *slot = v;
                changed = true;
            }
        }
    }
    if (changed && chunk()) {
        Chunk* c = chunk();
        const size_t at = c->ops.size();
        c->ops.resize(at + 2 + count);
        u32* w = c->ops.data() + at;
        w[0] = RE4_GX_OP_XF | (count + 1) << 8;
        w[1] = address;
        for (u32 i = 0; i < count; i++) {
            w[2 + i] = rd32(data + 4 * i, be);
        }
    }
}

// XF loads from the indexed arrays 12..15 (GXLoadPosMtxIndx ...): matrices
// the CPU wrote, host order.
void xfIndexed(u8 op, u32 word)
{
    gStats.xfIndexed++;
    u32 array = 12 + ((op >> 3) & 3);
    u32 index = word >> 16;
    u32 count = ((word >> 12) & 15) + 1;
    u32 address = word & 0xFFF;
    const u8* src = at(::array(array), index, count * 4);
    if (src) {
        xfWrite(address, src, count, false);
    }
}

void tlutLoad()
{
    gStats.tlutLoads++;
    gTexturesDirty = true;
    u32 source = (gGp.bp[0x64] & 0x1FFFFF) << 5;
    u32 offset = (gGp.bp[0x65] & 0x3FF) << 9;
    u32 bytes = ((gGp.bp[0x65] >> 10) & 0x7FF) * 32;
    if (offset + bytes > kTlutBytes) {
        bytes = kTlutBytes - offset;
    }
    const u8* src = re4gx::memory(source, bytes);
    if (!src || bytes == 0) {
        return;
    }
    memcpy(gTlut + offset, src, bytes);
    for (u32 b = offset >> 9; b < ((offset + bytes + 511) >> 9); b++) {
        gTlutLoaded[b] = true;
    }
    if (chunk()) {
        emitTlutRange(offset, bytes);
    }
}

// 0x5A: not a GameCube register, the port's marker of the app's gameplay
// autofocus (runtime/dof.cpp): sent each time like the registers that act
bool isAction(u8 reg)
{
    return reg == 0x52 || reg == 0x55 || reg == 0x56 || reg == 0x5A || (reg >= 0x60 && reg <= 0x63) ||
           reg == 0x66 || reg == 0x67;
}

void efbCopy(u32 control)
{
    u32 dest = (gGp.bp[0x4B] & 0x1FFFFF) << 5;
    u32 width = (gGp.bp[0x4A] & 0x3FF) + 1;
    u32 height = ((gGp.bp[0x4A] >> 10) & 0x3FF) + 1;
    if (control & (1u << 14)) {
        gStats.xfbCopies++;
        chunkBoundary(dest, width, height);
        return;
    }
    gStats.texCopies++;
    gTexturesDirty = true;
    for (auto it = gTexMemo.begin(); it != gTexMemo.end();) {
        if (it->second.address == dest) {
            it = gTexMemo.erase(it);
        } else {
            ++it;
        }
    }
    u8* marker = re4gx::memory(dest, 32);
    if (marker) {
        u32 serial = ++gCopySerial;
        u32 check = serial ^ dest ^ 0x5A5A5A5Au;
        memcpy(marker, kCopyMagic, 8);
        memcpy(marker + 8, &serial, 4);
        memcpy(marker + 12, &check, 4);
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
    u32 old = gGp.bp[reg];
    u32 now = (old & ~gGp.bpMask) | (value & gGp.bpMask);
    gGp.bp[reg] = now;
    gGp.bpMask = 0xFFFFFF;
    // genmode, RAS1_IREF, TEV orders, the texture map registers (0x80-0xBF)
    if (now != old && (reg == 0x00 || (reg >= 0x27 && reg <= 0x2F) || (reg >= 0x80 && reg <= 0xBF))) {
        gTexturesDirty = true;
    }
    switch (reg) {
    case 0x45:  // PE_DONE: everything before it is drawn
        re4gx::peFinish();
        return;
    case 0x47:  // PE_TOKEN: the token register, no interrupt
        re4gx::peToken((u16) now, false);
        return;
    case 0x48:  // PE_TOKEN_INT
        re4gx::peToken((u16) now, true);
        return;
    case 0x64:  // TLUT source, loaded with 0x65
        return;
    case 0x65:
        tlutLoad();
        return;
    }
    if (chunk() && (now != old || isAction(reg))) {
        const u32 payload = (u32) reg << 24 | now;
        emit(RE4_GX_OP_BP, &payload, 1);
    }
    if (reg == 0x52) {
        efbCopy(now);
    }
}

// --- the command stream ---------------------------------------------------------

size_t command(const u8* p, size_t n, size_t* need, bool be, int depth);

void callDisplayList(u32 address, u32 size, int depth)
{
    gStats.calls++;
    if (depth > 0) {
        return;  // the GP does not nest display lists
    }
    const u8* list = re4gx::listMemory(address, size);
    if (!list) {
        if (gUnknownLogged++ < 8) {
            re4_log("GX: display list at 0x%08x (%u bytes) is not in memory", address, size);
        }
        return;
    }
    gStats.dlBytes += size;
    gListBytes += size;
    bool be = true;
    auto host = gHostLists.find((uintptr_t) list);
    if (host != gHostLists.end() && host->second.size == size && hashBytes(list, size, 0) == host->second.hash) {
        be = false;
    }
    size_t at = 0;
    while (at < size) {
        size_t need = 0;
        size_t used = command(list + at, size - at, &need, be, depth + 1);
        if (used == 0) {
            break;  // a command cut off at the end of the list
        }
        at += used;
    }
}

// One command at p (n bytes available): its length, or 0 when it is cut off
// (*need: the bytes it takes).
size_t command(const u8* p, size_t n, size_t* need, bool be, int depth)
{
    *need = 1;
    if (n == 0) {
        return 0;
    }
    u8 op = p[0];
    switch (op) {
    case 0x00:  // NOP
        return 1;
    case 0x08:  // CP register
        *need = 6;
        if (n < 6) {
            return 0;
        }
        gStats.commands++;
        cpWrite(p[1], rd32(p + 2, be));
        return 6;
    case 0x10: {  // XF registers / memory: (count - 1) << 16 | address, then the words
        *need = 5;
        if (n < 5) {
            return 0;
        }
        u32 head = rd32(p + 1, be);
        size_t count = (head >> 16) + 1;
        *need = 5 + 4 * count;
        if (n < *need) {
            return 0;
        }
        gStats.commands++;
        gStats.xf++;
        xfWrite(head & 0xFFFF, p + 5, (u32) count, be);
        return *need;
    }
    case 0x20:
    case 0x28:
    case 0x30:
    case 0x38:  // XF loads from the indexed arrays 12..15
        *need = 5;
        if (n < 5) {
            return 0;
        }
        gStats.commands++;
        xfIndexed(op, rd32(p + 1, be));
        return 5;
    case 0x40:  // call a display list
        *need = 9;
        if (n < 9) {
            return 0;
        }
        gStats.commands++;
        callDisplayList(rd32(p + 1, be), rd32(p + 5, be), depth);
        return 9;
    case 0x44:  // metrics
    case 0x48:  // invalidate the vertex cache
        gStats.commands++;
        return 1;
    case 0x61:  // BP register
        *need = 5;
        if (n < 5) {
            return 0;
        }
        gStats.commands++;
        bpWrite(rd32(p + 1, be));
        return 5;
    default:
        break;
    }
    if (op >= 0x80 && op < 0xC0) {  // draw: primitive | vertex format, count, vertices
        *need = 3;
        if (n < 3) {
            return 0;
        }
        u32 count = rd16(p + 1, be);
        *need = 3 + (size_t) count * vertexSize(op & 7);
        if (n < *need) {
            return 0;
        }
        gStats.commands++;
        if (chunk()) {
            bindTextures();
        }
        draw(op, count, p + 3, be);
        return *need;
    }
    if (depth == 0) {
        logRecent(p, n);
    }
    if (gUnknownLogged++ < 8) {
        re4_log("GX: unknown command 0x%02x, skipped", op);
    }
    gStats.unknown++;
    return 1;
}

// The GP's input from the FIFO: worked through as soon as a command is
// complete.
std::vector<u8> gStream;
size_t gRead;
size_t gNeed = 1;

}  // namespace

namespace re4gx {

void drain()
{
    for (;;) {
        size_t need = 1;
        const size_t avail = gStream.size() - gRead;
        const u8 op = avail ? gStream[gRead] : 0;
        const bool isDraw = op >= 0x80 && op < 0xC0 && avail >= 3;
        const u32 count = isDraw ? rd16(gStream.data() + gRead + 1, true) : 0;
        size_t used = command(gStream.data() + gRead, avail, &need, true, 0);
        if (used == 0) {
            gNeed = need;
            break;
        }
        remember(op, used, count, isDraw ? vertexSize(op & 7) : 0);
        gRead += used;
    }
    if (gRead == gStream.size()) {
        gStream.clear();
        gRead = 0;
    } else if (gRead >= 4096) {
        gStream.erase(gStream.begin(), gStream.begin() + (long) gRead);
        gRead = 0;
    }
}

void feed(const uint8_t* bytes, size_t n)
{
    gStats.bytes += (u32) n;
    gFifoBytes += (double) n;
    gStream.insert(gStream.end(), bytes, bytes + n);
    if (gStream.size() - gRead >= gNeed) {
        drain();
    }
}

}  // namespace re4gx

extern "C" {

void re4_gx_mark(void)
{
    // the chunk being recorded carries the mark; the next one starts over, so
    // that the app can keep it (and those after it) as files of their own
    if (Chunk* c = chunk()) {
        c->view.flags |= RE4_GX_CHUNK_MARK;
    }
    gNeedReset = true;
}

void re4_gx_host_order_list(const void* list, unsigned int size)
{
    HostList h;
    h.size = size;
    h.hash = hashBytes((const u8*) list, size, 0);
    gHostLists[(uintptr_t) list] = h;
    gStats.hostLists++;
}

const Re4GxChunk* re4_core_gx_acquire(void)
{
    std::lock_guard<std::mutex> lock(gMutex);
    gLastPoll.store(nowMs());
    gWanted.store(true);
    if (gReady.empty()) {
        return nullptr;
    }
    int slot = gReady.front();
    gReady.pop_front();
    gSlotState[slot] = kReading;
    return &gSlots[slot].view;
}

void re4_core_gx_release(const Re4GxChunk* chunk)
{
    std::lock_guard<std::mutex> lock(gMutex);
    for (int i = 0; i < kSlots; i++) {
        if (&gSlots[i].view == chunk && gSlotState[i] == kReading) {
            gSlotState[i] = kFree;
        }
    }
    gCv.notify_all();
}

// VI (runtime/vi.cpp): the XFB the game switches the picture to
// (VISetNextFrameBuffer, virtual or physical address) ...
void re4_gx_vi_next(const void* fb)
{
    std::lock_guard<std::mutex> lock(gMutex);
    gViNext = (u32) (uintptr_t) fb & 0x03FFFFE0;  // as BP 0x4B holds a copy destination
}

// VISetBlack as VIFlush applies it (vi.cpp): the pictures from now on are
// black on screen until it is lifted
void re4_gx_vi_black(int black)
{
    std::lock_guard<std::mutex> lock(gMutex);
    gViBlack = black != 0;
}

// ... and VIFlush, which ends the game's frame (Render_swap): the display copy
// made before it is shown, or held when the VI stays on the other XFB.
void re4_gx_vi_flush(void)
{
    re4gx::drain();
    std::lock_guard<std::mutex> lock(gMutex);
    publishPendingLocked();
}

void re4_core_gx_notify(void (*notify)(void))
{
    gNotify.store(notify);
}

void re4_core_gx_detach(void)
{
    std::lock_guard<std::mutex> lock(gMutex);
    gWanted.store(false);
    dropQueueLocked();
    gCv.notify_all();
}

// Tests (runtime_tests.cpp, gx_probe.cpp): what the GP has seen.
void re4_gx_stats(unsigned out[11])
{
    re4gx::drain();
    const u32 v[11] = {gStats.bytes, gStats.commands, gStats.bp, gStats.cp, gStats.xf, gStats.xfIndexed,
                       gStats.draws, gStats.vertices, gStats.calls, gStats.unknown, gStats.dlBytes};
    memcpy(out, v, sizeof(v));
}

// More counters: xfb copies, texture copies, TLUT loads, textures sent, texture
// bytes sent, chunks, chunks dropped, host-order lists registered, skipped
// vertices.
void re4_gx_stats2(unsigned out[9])
{
    re4gx::drain();
    const u32 v[9] = {gStats.xfbCopies, gStats.texCopies, gStats.tlutLoads, gStats.textures, gStats.textureBytes,
                      gStats.chunks, gStats.dropped, gStats.hostLists, gStats.skipped};
    memcpy(out, v, sizeof(v));
}

unsigned re4_gx_bp(unsigned reg)
{
    re4gx::drain();
    return gGp.bp[reg & 0xFF];
}

unsigned re4_gx_xf(unsigned address)
{
    re4gx::drain();
    u32* slot = xfSlot(address);
    return slot ? *slot : 0;
}

}  // extern "C"
