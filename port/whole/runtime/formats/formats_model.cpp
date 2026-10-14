// Formatumwandler: models (BIN, game/model.cpp cModelData), motions (FCV,
// game/motion.cpp MotionData), motion sequences (SEQ) and the light path block
// that the core archive also tags "BIN" (game/lightPath.cpp).
//
// What the CPU reads becomes host order. What stays big-endian:
// - the primitive streams after each cPolyHeader (GX display lists, renderer),
// - vertex colours (bytes),
// - the key values of motion curves: motion.cpp reads them byte-wise as
//   big-endian (FccF32/FccS16, patch 0018). Counts and frame numbers of the
//   curves are read directly and are converted.
// Vertex positions, normals and texture coordinates become host order: the
// CPU reads positions and normals (CalcSk1, shapes, bounding box); the
// renderer reads all three arrays in host order as well.
#include "formats.h"

#include "model.h"
#include "re4_runtime.h"

using namespace re4fmt;

static_assert(sizeof(cModelData) == 0x48, "cModelData layout");
static_assert(sizeof(cPolyHeader) == 0x20, "cPolyHeader layout");
static_assert(sizeof(ModelDataHead) == 0x10, "ModelDataHead layout");
static_assert(offsetof(cModelData, version) == 0x3C, "cModelData version");

namespace re4fmt {

namespace {

const u32 kBinVersions[] = { 0x20010801, 0x20030817, 0x20030818 };

// End of the section starting at `start`: the nearest other section start
// after it, else `end` (the model's extent).
u32 sectionEnd(const u32* starts, int n, u32 start, u32 end)
{
    u32 stop = end;
    for (int i = 0; i < n; i++) {
        if (starts[i] > start && starts[i] < stop) {
            stop = starts[i];
        }
    }
    return stop;
}

// Byte size of one axis key array for Fcc key layout `fmt` (motion.cpp
// Fcc_next_axis_addr); 0 for layouts the game has no reader for.
u32 fccKeySize(u32 fmt)
{
    static const u8 size[16] = { 12, 8, 6, 0, 10, 6, 4, 0, 9, 5, 3, 0, 0, 0, 0, 4 };
    return size[fmt & 15];
}

}  // namespace

bool isBin(Blob& b, u32 base)
{
    if (base > b.size || b.size - base < 0x40) {
        return false;  // a probe, not an error
    }
    u32 v = be32(b, base + offsetof(cModelData, version));
    for (u32 k : kBinVersions) {
        if (v == k) {
            return true;
        }
    }
    return false;
}

// One model at `base`; `end` bounds its sections (the next entry or the end
// of the block).
void convertBinAt(Blob& b, u32 base, u32 end)
{
    typedef cModelData D;
    if (!isBin(b, base)) {
        fits(b, base, 0xFFFFFFFFu, "BIN version");
        return;
    }
    if (be32(b, base + offsetof(D, pClr)) & 0x80000000u) {
        return;  // relocated already (calcModelAddr ran): converted before
    }
    u32 version = be32(b, base + offsetof(D, version));
    bool tables = version == 0x20030818;  // calcModelAddr: version > 0x20030817
    u32 head = be32(b, base + offsetof(D, pHead));
    u32 clr = be32(b, base + offsetof(D, pClr));
    u32 tex = be32(b, base + offsetof(D, pTex));
    u32 wgt = be32(b, base + offsetof(D, pWeight));
    u32 parts = be32(b, base + offsetof(D, pParts));
    u32 flags = be32(b, base + offsetof(D, flags));
    u32 shape = be32(b, base + offsetof(D, shapeOfs));
    u32 vtx = be32(b, base + offsetof(D, vtxOrig));
    u32 nrm = be32(b, base + offsetof(D, nrmOrig));
    u32 nParts = b.data[base + offsetof(D, nParts)];
    u32 nWeight = b.data[base + offsetof(D, weight_palette_num)];
    u32 nWeightExt = be16(b, base + offsetof(D, weight_ext_num));
    u32 nList = be16(b, base + offsetof(D, displist_num));
    u32 nVtx = be16(b, base + offsetof(D, nVtx));
    u32 nNrm = be16(b, base + offsetof(D, nNrm));
    u32 blend = tables && fits(b, base, sizeof(D), "BIN tables") ? be32(b, base + offsetof(D, blendTbl)) : 0;
    u32 flip = tables ? be32(b, base + offsetof(D, flipTbl)) : 0;
    if (!b.ok) {
        return;
    }
    // header: the eleven words, the halfwords, the version (and the two
    // table offsets of 0x20030818)
    swap32(b, base + 0x00, 6);  // pHead, two words, pClr, pTex, pWeight
    RE4_SWAP(b, base, D, displist_num);
    swap32(b, base + offsetof(D, pParts), 3);  // pParts, flags, nTex
    RE4_SWAP(b, base, D, weight_ext_num);
    swap32(b, base + offsetof(D, shapeOfs), 3);  // shapeOfs, vtxOrig, nrmOrig
    RE4_SWAP(b, base, D, nVtx);
    RE4_SWAP(b, base, D, nNrm);
    RE4_SWAP(b, base, D, version);
    if (tables) {
        RE4_SWAP(b, base, D, blendTbl);
        RE4_SWAP(b, base, D, flipTbl);
    }

    // parts records: the word x0 (partsNo, parentNo and two bytes as one
    // union, a byte view that patch 0016 lays out for host-order words), then
    // the position
    if (nParts && fits(b, base + head, nParts * sizeof(ModelDataHead), "BIN parts records")) {
        swap32(b, base + head, nParts * sizeof(ModelDataHead) / 4);
    }
    // vertex positions s16 x/y/z + s16 matrix index, normals s16 x4 or s8 x4
    if (nVtx) {
        swap16(b, base + vtx, nVtx * 4);
    }
    if (nNrm && !(flags & 0x20000000)) {
        swap16(b, base + nrm, nNrm * 4);
    }
    // texture coordinates: u16 pairs up to the next section (the count is
    // implied by the display lists)
    u32 starts[11] = { head, clr, tex, wgt, parts, shape, vtx, nrm, blend, flip, 0 };
    if (tex >= 0x40 && base + tex < end) {
        u32 stop = sectionEnd(starts, 10, tex, end - base);
        swap16(b, base + tex, (stop - tex) / 2);
    }
    // skinning weights: cWeightExt (u16 ids, u16 count, u8 rates) or cWeight (bytes)
    if (nWeightExt > 0xFF && fits(b, base + wgt, nWeightExt * 12, "BIN extended weights")) {
        for (u32 i = 0; i < nWeightExt; i++) {
            swap16(b, base + wgt + i * 12, 4);
        }
    }
    (void) nWeight;
    // primitive parts: header (bytes, buffer size, polygon count) + GX stream
    u32 p = base + parts;
    for (u32 i = 0; i < nList && b.ok; i++) {
        if (!fits(b, p, sizeof(cPolyHeader), "cPolyHeader")) {
            break;
        }
        u32 size = be32(b, p + offsetof(cPolyHeader, buff_size));
        RE4_SWAP(b, p, cPolyHeader, buff_size);
        RE4_SWAP(b, p, cPolyHeader, nPoly);
        if (!fits(b, p + sizeof(cPolyHeader), size, "primitive stream")) {
            break;
        }
        p += sizeof(cPolyHeader) + size;
    }
    // shape (morph) table: u32 count, {u32 offset, s32 entries} per shape,
    // delta lists of s16 vertex index, dx, dy, dz (shape.cpp)
    if (shape != 0 && fits(b, base + shape, 4, "shape table")) {
        u32 t = base + shape + 4;
        u32 n = be32(b, base + shape);
        swap32(b, base + shape);
        if (fits(b, t, n * 8, "shape entries")) {
            for (u32 i = 0; i < n && b.ok; i++) {
                u32 ofs = be32(b, t + i * 8);
                u32 cnt = be32(b, t + i * 8 + 4);
                swap32(b, t + i * 8, 2);
                swap16(b, t + ofs, cnt * 4);
            }
        }
    }
    // motion blend table (u32 count, 4 halfwords each) and x-flip table
    // (u32 count, halfwords); motion.cpp reads the counts as words
    if (blend != 0 && fits(b, base + blend, 4, "blend table")) {
        u32 n = be32(b, base + blend);
        swap32(b, base + blend);
        swap16(b, base + blend + 4, n * 4);
    }
    if (flip != 0 && fits(b, base + flip, 4, "flip table")) {
        u32 n = be32(b, base + flip);
        swap32(b, base + flip);
        swap16(b, base + flip + 4, n);
    }
}

// lightPath.h cLightPathHeader: u8 count, three pad bytes, u32 offsets of
// byte strings (brightness 0..200, 0xFF ends a path).
bool isLightPath(Blob& b, u32 base, u32 size)
{
    if (size < 8 || b.data[base + 1] || b.data[base + 2] || b.data[base + 3]) {
        return false;
    }
    u32 n = b.data[base];
    if (n == 0 || 4 + n * 4 > size) {
        return false;
    }
    for (u32 i = 0; i < n; i++) {
        u32 o = be32(b, base + 4 + i * 4);
        if (o != 0 && (o < 4 + n * 4 || o >= size)) {
            return false;
        }
    }
    return true;
}

void convertLightPathAt(Blob& b, u32 base)
{
    swap32(b, base + 4, b.data[base]);
}

// BIN archive entries: a model, or (core archive) the light path block.
void convertBin(Blob& b, const Part&)
{
    if (isBin(b, 0)) {
        convertBinAt(b, 0, b.size);
    } else if (isLightPath(b, 0, b.size)) {
        convertLightPathAt(b, 0);
    } else {
        fits(b, 0, 0xFFFFFFFFu, "BIN (neither a model nor a light path)");
    }
}

// MotionData at `base`: u16 maxFrame, u8 joints, u16 kind[n] (unaligned),
// u8 parts[n], pad to 4, u32 size, u32 key block offsets[n]; per joint three
// axes of u16 count, u16 frames[count], keys (left big-endian).
void convertMotionAt(Blob& b, u32 base)
{
    if (!fits(b, base, 3, "MotionData")) {
        return;
    }
    u32 n = b.data[base + 2];
    u32 table = (base + 3 + n * 3 + 3) & ~3u;
    if (!fits(b, base, table - base + 4 + n * 4, "MotionData header")) {
        return;
    }
    u32 kinds[256];
    u32 offs[256];
    for (u32 i = 0; i < n; i++) {
        kinds[i] = be16(b, base + 3 + i * 2);
        offs[i] = be32(b, table + 4 + i * 4);
    }
    if (n && (offs[0] & 0x80000000u)) {
        return;  // relocated already (MotionSetCore ran)
    }
    swap16(b, base);         // maxFrame
    swap16(b, base + 3, n);  // kind words (odd address)
    swap32(b, table, 1 + n);
    Seen seen;
    for (u32 i = 0; i < n && b.ok; i++) {
        if (!seen.first(offs[i])) {
            continue;
        }
        u32 key = fccKeySize(kinds[i] >> 12);
        u32 p = base + offs[i];
        for (int axis = 0; axis < 3 && b.ok; axis++) {
            u32 count = be16(b, p);
            swap16(b, p, 1 + count);
            p += 2 + count * 2 + count * key;
        }
    }
}

void convertMotion(Blob& b, const Part&)
{
    convertMotionAt(b, 0);
}

// Sequence table: u16 count, u8 flags, u8 pad, {u16 frame, u8 se, u8 free}.
void convertSeqAt(Blob& b, u32 base)
{
    u32 n = be16(b, base);
    swap16(b, base);
    if (fits(b, base + 4, n * 4, "sequence keys")) {
        for (u32 i = 0; i < n; i++) {
            swap16(b, base + 4 + i * 4);
        }
    }
}

void convertSeq(Blob& b, const Part&)
{
    convertSeqAt(b, 0);
}

}  // namespace re4fmt
