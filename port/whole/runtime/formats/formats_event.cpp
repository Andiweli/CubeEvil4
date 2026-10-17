// Formatumwandler: room cameras and events.
//
//   CAM  room / core camera data "B402".."B404" (cam_ctrl.h, CameraControl::calcAddr)
//   EVS  event bundle (event.cpp EvsHeader / EvsEntry): EVD files back to back
//   EVD  event file (event.h EvtHeader, EvtBinEntry, EvtPacket; event.cpp
//        Event::ExePacket_*): header, packet stream, table of named assets
//
// The assets of an EVD are converted by what the packets do with them (camera,
// motion and shape curves, lights, fog / focus curves, effects, messages,
// models and textures), the rest by the name's extension. In a bundle (EVS)
// an asset no packet of its own event names takes what another event does
// with that name: the game keeps the first event's copy for all of them.
#include "formats.h"

#include "cam_ctrl.h"
#include "event.h"
#include "re4_runtime.h"

#include <stdlib.h>
#include <string.h>

using namespace re4fmt;

static_assert(sizeof(CAM_FILE_HEADER) == 0x10, "CAM_FILE_HEADER layout");
static_assert(sizeof(CUT_INFO) == 0x10, "CUT_INFO layout");
static_assert(sizeof(AREA_DATA) == 0x30, "AREA_DATA layout");
static_assert(sizeof(CAMERA_DATA) == 0x34, "CAMERA_DATA layout");
static_assert(sizeof(LERP_DATA) == 0x10, "LERP_DATA layout");
static_assert(sizeof(EvtHeader) == 0x50, "EvtHeader layout");
static_assert(sizeof(EvtBinEntry) == 0x40, "EvtBinEntry layout");
static_assert(offsetof(EvtPacket, mod) == 0x10, "EvtPacket layout");

namespace re4fmt {

// ---- CAM ----------------------------------------------------------------------

namespace {

// The key arrays of the cuts: every 16-bit unit is swapped once, however many
// cuts share (or overlap in) an array. Plain C memory: this file is compiled
// with the game's flags and prelude, which the NDK's libc++ headers do not
// take (std::vector broke the Android build).
struct CamArrays {
    Blob& b;
    u32 base;
    u8* done;  // per 16-bit unit from `base`
    u32 units;
    unsigned outside;

    CamArrays(Blob& blob, u32 from) : b(blob), base(from), outside(0)
    {
        units = blob.size > from ? (blob.size - from) / 2 + 1 : 1;
        done = (u8*) calloc(units, 1);
    }
    ~CamArrays() { free(done); }
    // An array the game reads lies in the block; one that points elsewhere (a
    // stale pointer the camera tool left in a field that cut never uses) is
    // left as it is and reported, the rest of the block is still converted.
    bool fresh(u32 off, u32 bytes, unsigned cutNo, const char* what)
    {
        // offset 0 (relocated to the header by calcAddr): the version text
        // must stay as it is, the game reads those bytes as they are
        if (!done || off < base + sizeof(CAM_FILE_HEADER) || off > b.size || bytes > b.size - off) {
            if (done && off >= base + sizeof(CAM_FILE_HEADER) && outside++ < 4) {
                re4_log("formats: %s: cut %u: %s at 0x%x outside the %u-byte block, left as it is", b.name, cutNo,
                        what, (unsigned) off, (unsigned) b.size);
            }
            return false;
        }
        u32 u = (off - base) / 2;
        for (u32 i = 0; i < bytes / 2; i++) {
            if (u + i >= units || done[u + i]) {
                return false;
            }
        }
        for (u32 i = 0; i < bytes / 2; i++) {
            done[u + i] = 1;
        }
        return true;
    }
    void words(u32 off, u32 count, unsigned cutNo, const char* what)
    {
        for (u32 i = 0; i < count && b.ok; i++) {
            if (fresh(off + i * 4, 4, cutNo, what)) {
                swap32(b, off + i * 4);
            } else if (off + i * 4 >= b.size) {
                break;  // reported once
            }
        }
    }
    void halves(u32 off, u32 count, unsigned cutNo, const char* what)
    {
        for (u32 i = 0; i < count && b.ok; i++) {
            if (fresh(off + i * 2, 2, cutNo, what)) {
                swap16(b, off + i * 2);
            } else if (off + i * 2 >= b.size) {
                break;
            }
        }
    }
};

}  // namespace

void convertCameraAt(Blob& b, u32 base)
{
    if (!fits(b, base, sizeof(CAM_FILE_HEADER), "CAM_FILE_HEADER")) {
        return;
    }
    const u8* h = b.data + base;
    // cam_ctrl.cpp cameraDataVersion: B402..B404 are relocated (calcAddr);
    // B400, B401 and anything else the game uses as it is (the CAM of the
    // core archive is 32 bytes without a version)
    if (h[0] != 'B' || h[1] != '4' || h[2] != '0' || h[3] < '2' || h[3] > '4') {
        re4_log("formats: %s: camera data %02x %02x %02x %02x (not relocated by the game), left as it is", b.name,
                h[0], h[1], h[2], h[3]);
        return;
    }
    u32 nCut = h[offsetof(CAM_FILE_HEADER, nCdat)];
    u32 nArea = h[offsetof(CAM_FILE_HEADER, nAdat)];
    u32 nLerp = h[offsetof(CAM_FILE_HEADER, nLdat)];
    u32 info = base + sizeof(CAM_FILE_HEADER);
    u32 area = info + nArea * sizeof(CUT_INFO);
    u32 cut = area + nArea * sizeof(AREA_DATA);
    u32 lerp = cut + nCut * sizeof(CAMERA_DATA);
    if (!fits(b, info, lerp + nLerp * sizeof(LERP_DATA) - info, "camera tables")) {
        return;
    }
    if (nArea && (be32(b, info + offsetof(CUT_INFO, pAdat)) & 0x80000000u)) {
        return;  // relocated already
    }
    Seen seen;
    for (u32 i = 0; i < nArea; i++) {
        u32 o = info + i * sizeof(CUT_INFO);
        RE4_SWAP(b, o, CUT_INFO, pAdat);
        RE4_SWAP(b, o, CUT_INFO, pCdat);
    }
    for (u32 i = 0; i < nArea && b.ok; i++) {
        u32 o = area + i * sizeof(AREA_DATA);
        u32 n = be32(b, o + offsetof(AREA_DATA, nVer));
        u32 ver = be32(b, o + offsetof(AREA_DATA, pVer));
        RE4_SWAP(b, o, AREA_DATA, Dir);
        RE4_SWAP(b, o, AREA_DATA, Height);
        RE4_SWAP(b, o, AREA_DATA, Y);
        RE4_SWAP(b, o, AREA_DATA, nVer);
        RE4_SWAP(b, o, AREA_DATA, pVer);
        if (seen.first(base + ver)) {
            swap32(b, base + ver, n * 3);
        }
    }
    // The key arrays, as many entries as the game reads: nPoint keys for the
    // fixed, pan, rail and motion cuts (cam_ctrl.cpp), but a shoulder camera
    // cut (Id 8, CameraQuasiFPS::setAreaData) always reads 24 positions
    // (near and far point of 4 x 3 sites) and 12 targets, rolls and angles
    // once nPoint is not 0, whatever nPoint says. The frame times only for the
    // motion cuts (Id 6, 7: HermiteExport); the other cuts never read pFrame,
    // and r100 has cuts whose pFrame points outside the file.
    CamArrays keys(b, base);
    for (u32 i = 0; i < nCut && b.ok; i++) {
        typedef CAMERA_DATA C;
        u32 o = cut + i * sizeof(C);
        u32 n = be32(b, o + offsetof(C, nPoint));
        u32 frame = be32(b, o + offsetof(C, pFrame));
        u32 pos = be32(b, o + offsetof(C, pCampos));
        u32 target = be32(b, o + offsetof(C, pTarget));
        u32 roll = be32(b, o + offsetof(C, pRoll));
        u32 fovy = be32(b, o + offsetof(C, pFovy));
        s8 id = (s8) b.data[o + offsetof(C, Id)];
        swap32(b, o + offsetof(C, offset), 3);
        RE4_SWAP(b, o, C, pFrame);
        RE4_SWAP(b, o, C, floor_ratio);
        RE4_SWAP(b, o, C, nPoint);
        swap32(b, o + offsetof(C, pCampos), 4);
        if (n > 0x1000) {
            fits(b, o, 0xFFFFFFFFu, "camera key count");
            continue;
        }
        u32 nPos = n, nKey = n;
        if (id == 8 && n != 0) {
            nPos = 24;
            nKey = 12;
        }
        if (id == 6 || id == 7) {
            keys.halves(base + frame, n, i, "frame times");
        }
        keys.words(base + pos, nPos * 3, i, "positions");
        keys.words(base + target, nKey * 3, i, "targets");
        keys.words(base + roll, nKey, i, "rolls");
        keys.words(base + fovy, nKey, i, "angles");
    }
    for (u32 i = 0; i < nLerp; i++) {
        RE4_SWAP(b, lerp + i * sizeof(LERP_DATA), LERP_DATA, InterFrame);
    }
}

void convertCamera(Blob& b, const Part&)
{
    convertCameraAt(b, 0);
}

// ---- EVD ----------------------------------------------------------------------

// What the packets do with assets, by name: the kind each named asset gets.
// The game keeps one registration per name for all events of a room
// (DatTbl::SetDat: a name already there is not taken again), so in a bundle
// (EVS) the copy of the first event serves the packets of every other one:
// the marks of the whole bundle apply to each of its events. Plain C memory
// (see CamArrays).
struct EvdMarks {
    struct Mark {
        char name[0x31];
        int kind;  // AssetKind
    };
    Mark* m;
    u32 n, cap;

    EvdMarks() : m(0), n(0), cap(0) {}
    ~EvdMarks() { free(m); }
    // the name a packet gives at `str` (up to 0x30 bytes, as the game's strcmp reads it)
    void add(Blob& b, u32 str, int kind)
    {
        if (str >= b.size) {
            return;
        }
        if (n == cap) {
            u32 more = cap ? cap * 2 : 64;
            Mark* g = (Mark*) realloc(m, more * sizeof(Mark));
            if (!g) {
                return;
            }
            m = g;
            cap = more;
        }
        u32 len = 0;
        while (len < 0x30 && str + len < b.size && b.data[str + len]) {
            len++;
        }
        memcpy(m[n].name, b.data + str, len);
        m[n].name[len] = 0;
        m[n].kind = kind;
        n++;
    }
};

namespace {

enum AssetKind {
    kAssetUnknown,
    kAssetBin,
    kAssetTpl,
    kAssetCurve,  // MotionData layout: motions, camera and shape curves
    kAssetLit,
    kAssetFog,    // DatFog: two HERMITE_1_FIX curves (words)
    kAssetFocus,  // DatFocus: two curves and two levels (words)
    kAssetEff,
    kAssetMdt,
};

struct Asset {
    char name[0x31];
    u32 ofs;
    AssetKind kind;
};

AssetKind kindByName(const char* name)
{
    const char* dot = strrchr(name, '.');
    if (!dot) {
        return kAssetUnknown;
    }
    struct {
        const char* ext;
        AssetKind kind;
    } const ext[] = {
        { ".bin", kAssetBin }, { ".tpl", kAssetTpl }, { ".fcv", kAssetCurve }, { ".lit", kAssetLit },
        { ".fcs", kAssetFocus }, { ".fog", kAssetFog }, { ".eff", kAssetEff }, { ".mdt", kAssetMdt },
    };
    for (const auto& e : ext) {
        if (strcasecmp(dot, e.ext) == 0) {
            return e.kind;
        }
    }
    return kAssetUnknown;
}

// The marks on an event's asset table, in packet order: a later packet's kind wins.
void applyMarks(const EvdMarks& marks, Asset* a, u32 nBin)
{
    for (u32 k = 0; k < marks.n; k++) {
        for (u32 i = 0; i < nBin; i++) {
            if (strcmp(a[i].name, marks.m[k].name) == 0) {
                a[i].kind = (AssetKind) marks.m[k].kind;
            }
        }
    }
}

// The assets a packet names (EvtPacket at `p`, its type `id`).
void packetMarks(Blob& b, u32 p, u32 id, EvdMarks& out)
{
    u32 str = p + 0x10;
    switch (id) {
    case EvpTpCam:
        out.add(b, str, kAssetCurve);
        break;
    case EvpTpMot:
    case EvpTpShp:
        out.add(b, p + offsetof(EvtPacket, mod.bin), kAssetCurve);
        break;
    case EvpTpLit:
        out.add(b, str, kAssetLit);
        break;
    case EvpTpFog:
        out.add(b, str, kAssetFog);
        break;
    case EvpTpFocus:
        out.add(b, str, kAssetFocus);
        break;
    case EvpTpSetEff:
        out.add(b, str, kAssetEff);
        break;
    case EvpTpSetMdt:
        out.add(b, str, kAssetMdt);
        break;
    case EvpTpSetOm:
        out.add(b, p + offsetof(EvtPacket, mod.bin), kAssetBin);
        out.add(b, p + offsetof(EvtPacket, mod.tpl), kAssetTpl);
        break;
    case EvpTpSetParts:
        out.add(b, p + offsetof(EvtPacket, parts.bin), kAssetBin);
        out.add(b, p + offsetof(EvtPacket, parts.tpl), kAssetTpl);
        break;
    default:
        break;
    }
}

// The packet stream of the (still big-endian) EVD at `base`: what it does
// with its assets, without changing anything.
void scanEvd(Blob& b, u32 base, EvdMarks& out)
{
    typedef EvtHeader H;
    if (base + sizeof(H) > b.size || memcmp(b.data + base, "event", 5) != 0) {
        return;
    }
    u32 pac = be32(b, base + offsetof(H, pacOfs));
    u32 pacSize = be32(b, base + offsetof(H, pacSize));
    if ((pac & 0x80000000u) || pac > b.size - base || pacSize > b.size - base - pac) {
        return;
    }
    u32 p = base + pac;
    u32 stop = p + pacSize;
    while (p + 0x10 <= stop) {
        u32 id = be32(b, p + offsetof(EvtPacket, id));
        s16 size = (s16) be16(b, p + offsetof(EvtPacket, size));
        if (size < 0x10 || (u32) size > stop - p) {
            break;
        }
        packetMarks(b, p, id, out);
        p += (u32) size;
        if (id == EvpTpEndPac) {
            break;
        }
    }
}

}  // namespace

void convertEvdAt(Blob& b, u32 base, u32 end, const EvdMarks* bundle)
{
    typedef EvtHeader H;
    if (!fits(b, base, sizeof(H), "EvtHeader")) {
        return;
    }
    if (memcmp(b.data + base, "event", 5) != 0) {
        fits(b, base, 0xFFFFFFFFu, "EVD tag");
        return;
    }
    u32 pac = be32(b, base + offsetof(H, pacOfs));
    u32 pacSize = be32(b, base + offsetof(H, pacSize));
    u32 nBin = be32(b, base + offsetof(H, nBin));
    u32 bins = be32(b, base + offsetof(H, binOfs));
    if (pac & 0x80000000u) {
        return;
    }
    if (!fits(b, base + pac, pacSize, "EVD packets") || nBin > 1024 ||
        !fits(b, base + bins, nBin * sizeof(EvtBinEntry), "EvtBinEntry")) {
        return;
    }
    RE4_SWAP(b, base, H, sndFlag);
    RE4_SWAP(b, base, H, pacOfs);
    RE4_SWAP(b, base, H, pacSize);
    RE4_SWAP(b, base, H, nBin);
    RE4_SWAP(b, base, H, binOfs);
    static Asset assets[1024];
    for (u32 i = 0; i < nBin; i++) {
        u32 e = base + bins + i * sizeof(EvtBinEntry);
        memcpy(assets[i].name, b.data + e, 0x30);
        assets[i].name[0x30] = 0;
        assets[i].ofs = be32(b, e + offsetof(EvtBinEntry, ofs));
        assets[i].kind = kindByName(assets[i].name);
        RE4_SWAP(b, e, EvtBinEntry, ofs);
    }
    if (bundle) {
        applyMarks(*bundle, assets, nBin);
    }
    EvdMarks own;
    // packets: header words, then the numeric parameters of their type
    u32 p = base + pac;
    u32 stop = p + pacSize;
    while (p + 0x10 <= stop && b.ok) {
        u32 id = be32(b, p + offsetof(EvtPacket, id));
        s16 size = (s16) be16(b, p + offsetof(EvtPacket, size));
        if (size < 0x10 || (u32) size > stop - p) {
            fits(b, p, 0xFFFFFFFFu, "EvtPacket size");
            break;
        }
        swap32(b, p, 2);
        swap16(b, p + offsetof(EvtPacket, cut), 4);
        u32 q = p + 0x10;
        u32 room = (u32) size - 0x10;
        switch (id) {
        case EvpTpPos:  // pos[3], rot[3], partsNo
            if (room >= 0x34) {
                swap32(b, p + offsetof(EvtPacket, pos.pos), 7);
            }
            break;
        case EvpTpEsp:  // type
            if (room >= 0x10) {
                swap32(b, p + offsetof(EvtPacket, esp.type));
            }
            break;
        case EvpTpCamDammy:
        case EvpTpStr:
        case EvpTpSe:
        case EvpTpMes:
        case EvpTpFunc:
        case EvpTpFade:  // val: no, arg, time
            swap32(b, q, room / 4 < 3 ? room / 4 : 3);
            break;
        default:  // names (packetMarks), or no parameters
            break;
        }
        packetMarks(b, p, id, own);
        p += (u32) size;
        if (id == EvpTpEndPac) {
            break;
        }
    }
    applyMarks(own, assets, nBin);
    // assets: each extends to the next one (or the end of the file)
    Seen seen;
    for (u32 i = 0; i < nBin && b.ok; i++) {
        u32 o = base + assets[i].ofs;
        u32 next = end;
        for (u32 k = 0; k < nBin; k++) {
            u32 ok = base + assets[k].ofs;
            if (ok > o && ok < next) {
                next = ok;
            }
        }
        if (!seen.first(o) || o >= next) {
            continue;
        }
        char label[200];
        snprintf(label, sizeof(label), "%s %s", b.name, assets[i].name);
        Blob a = blob(b.data + o, next - o, label);
        Part part = { label, 0, 0, 0, 0, 0 };
        switch (assets[i].kind) {
        case kAssetBin:
            convertBin(a, part);
            break;
        case kAssetTpl:
            convertTpl(a, part);
            break;
        case kAssetCurve:
            convertMotion(a, part);
            break;
        case kAssetLit:
            convertLit(a, part);
            break;
        case kAssetFog:
            swap32(a, 0, 0x808 / 4);
            break;
        case kAssetFocus:
            swap32(a, 0, 0x810 / 4);
            break;
        case kAssetEff:
            convertEffect(a, part);
            break;
        case kAssetMdt:
            convertMdt(a, part);
            break;
        default:
            convertNamedFile(a, part, assets[i].name);
            break;
        }
        if (!a.ok) {
            b.ok = false;
        }
    }
}

void convertEvd(Blob& b, const Part&)
{
    convertEvdAt(b, 0, b.size, 0);
}

void convertEvdFile(Blob& b, const Part& part)
{
    convertEvdAt(b, 0, b.size, 0);
    // event models (EvpTpSetOm / SetParts) point into the file: the bridge
    // finds them by their offset (re4_formats_locate, entry -2)
    if (b.size >= 0x50 && memcmp(b.data, "event", 5) == 0) {
        noteArchive(b.data, b.size, b.name, part, 0, 0, true);
    }
}

// EVS: s32 count, u32 table offset; {u32 offset, u32} per event.
void convertEvs(Blob& b, const Part&)
{
    if (!fits(b, 0, 8, "EvsHeader")) {
        return;
    }
    u32 n = be32(b, 0);
    u32 tbl = be32(b, 4);
    if (n > 256 || !fits(b, tbl, n * 8, "EvsEntry")) {
        return;
    }
    u32 offs[256];
    for (u32 i = 0; i < n; i++) {
        offs[i] = be32(b, tbl + i * 8);
    }
    // what every event does with its assets, before any is converted
    EvdMarks marks;
    for (u32 i = 0; i < n; i++) {
        scanEvd(b, offs[i], marks);
    }
    swap32(b, 0, 2);
    swap32(b, tbl, n * 2);
    for (u32 i = 0; i < n && b.ok; i++) {
        u32 next = b.size;
        for (u32 k = 0; k < n; k++) {
            if (offs[k] > offs[i] && offs[k] < next) {
                next = offs[k];
            }
        }
        if (tbl > offs[i] && tbl < next) {
            next = tbl;
        }
        convertEvdAt(b, offs[i], next, &marks);
    }
}

}  // namespace re4fmt
