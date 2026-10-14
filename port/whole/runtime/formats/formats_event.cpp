// Formatumwandler: room cameras and events.
//
//   CAM  room / core camera data "B402".."B404" (cam_ctrl.h, CameraControl::calcAddr)
//   EVS  event bundle (event.cpp EvsHeader / EvsEntry): EVD files back to back
//   EVD  event file (event.h EvtHeader, EvtBinEntry, EvtPacket; event.cpp
//        Event::ExePacket_*): header, packet stream, table of named assets
//
// The assets of an EVD are converted by what the packets do with them (camera,
// motion and shape curves, lights, fog / focus curves, effects, messages,
// models and textures), the rest by the name's extension.
#include "formats.h"

#include "cam_ctrl.h"
#include "event.h"
#include "re4_runtime.h"

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

void convertCameraAt(Blob& b, u32 base)
{
    if (!fits(b, base, sizeof(CAM_FILE_HEADER), "CAM_FILE_HEADER")) {
        return;
    }
    const u8* h = b.data + base;
    if (h[0] != 'B' || h[1] != '4' || h[2] != '0' || h[3] < '0' || h[3] > '9') {
        fits(b, base, 0xFFFFFFFFu, "camera data version");
        return;
    }
    if (h[3] <= '1') {
        re4_log("formats: %s: camera data B40%c (not relocated by the game), left big-endian", b.name, h[3]);
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
    for (u32 i = 0; i < nCut && b.ok; i++) {
        typedef CAMERA_DATA C;
        u32 o = cut + i * sizeof(C);
        u32 n = be32(b, o + offsetof(C, nPoint));
        u32 frame = be32(b, o + offsetof(C, pFrame));
        u32 pos = be32(b, o + offsetof(C, pCampos));
        u32 target = be32(b, o + offsetof(C, pTarget));
        u32 roll = be32(b, o + offsetof(C, pRoll));
        u32 fovy = be32(b, o + offsetof(C, pFovy));
        swap32(b, o + offsetof(C, offset), 3);
        RE4_SWAP(b, o, C, pFrame);
        RE4_SWAP(b, o, C, floor_ratio);
        RE4_SWAP(b, o, C, nPoint);
        swap32(b, o + offsetof(C, pCampos), 4);
        if (seen.first(base + frame)) {
            swap16(b, base + frame, n);
        }
        if (seen.first(base + pos)) {
            swap32(b, base + pos, n * 3);
        }
        if (seen.first(base + target)) {
            swap32(b, base + target, n * 3);
        }
        if (seen.first(base + roll)) {
            swap32(b, base + roll, n);
        }
        if (seen.first(base + fovy)) {
            swap32(b, base + fovy, n);
        }
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

void mark(Asset* a, u32 n, const u8* name, AssetKind kind)
{
    for (u32 i = 0; i < n; i++) {
        if (strncmp(a[i].name, (const char*) name, 0x30) == 0) {
            a[i].kind = kind;
        }
    }
}

}  // namespace

void convertEvdAt(Blob& b, u32 base, u32 end)
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
        const u8* str = b.data + q;
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
        case EvpTpCam:
            mark(assets, nBin, str, kAssetCurve);
            break;
        case EvpTpMot:
        case EvpTpShp:
            mark(assets, nBin, b.data + p + offsetof(EvtPacket, mod.bin), kAssetCurve);
            break;
        case EvpTpLit:
            mark(assets, nBin, str, kAssetLit);
            break;
        case EvpTpFog:
            mark(assets, nBin, str, kAssetFog);
            break;
        case EvpTpFocus:
            mark(assets, nBin, str, kAssetFocus);
            break;
        case EvpTpSetEff:
            mark(assets, nBin, str, kAssetEff);
            break;
        case EvpTpSetMdt:
            mark(assets, nBin, str, kAssetMdt);
            break;
        case EvpTpSetOm:
            mark(assets, nBin, b.data + p + offsetof(EvtPacket, mod.bin), kAssetBin);
            mark(assets, nBin, b.data + p + offsetof(EvtPacket, mod.tpl), kAssetTpl);
            break;
        case EvpTpSetParts:
            mark(assets, nBin, b.data + p + offsetof(EvtPacket, parts.bin), kAssetBin);
            mark(assets, nBin, b.data + p + offsetof(EvtPacket, parts.tpl), kAssetTpl);
            break;
        default:  // names only, or no parameters
            break;
        }
        p += (u32) size;
        if (id == EvpTpEndPac) {
            break;
        }
    }
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
    convertEvdAt(b, 0, b.size);
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
        convertEvdAt(b, offs[i], next);
    }
}

}  // namespace re4fmt
