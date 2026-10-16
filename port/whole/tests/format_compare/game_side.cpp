// re4_format_compare, game side: compiled with the game's flags and headers
// (re4_game_flags). Converts a copy of the big-endian input with the
// Formatumwandler (port/whole/runtime/formats) exactly as the game's loader
// does, then reports the fields through the original structure definitions.
//
// app_side.cpp reports the same keys from the app's checked big-endian
// readers. Where the app derives a value (a float from a fixed-point
// halfword, the count of an array bounded by the next section, a normalised
// light parent), this side computes it from the converted fields the same
// way; each such rule names the app function it follows.
#include "types.h"
#include "atari.h"
#include "cam_ctrl.h"
#include "event.h"
#include "global.h"
#include "light.h"
#include "model.h"
#include "room_jmp.h"
#include "sce_at.h"
#include "scroll.h"

#include "formats.h"
#include "facts.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

using namespace re4fmt;

static_assert(SCEAT_ID_DOOR == 1 && SCEAT_ID_SAVE == 8 && SCEAT_ID_LADDER == 0x10, "area ids (room_flow.cpp)");
static_assert(EvpTpCam == 6 && EvpTpCamDammy == 8 && EvpTpPos == 9 && EvpTpEsp == 13 && EvpTpEndPac == 27,
              "packet ids (event_data.cpp)");

namespace {

// ---- reading converted data -------------------------------------------------

struct View {
    const u8* data;
    u32 size;

    bool has(u32 off, u32 len) const { return off <= size && len <= size - off; }
    u32 w(u32 off) const  // host-order word (converted)
    {
        u32 v = 0;
        if (has(off, 4)) {
            memcpy(&v, data + off, 4);
        }
        return v;
    }
    u16 h(u32 off) const
    {
        u16 v = 0;
        if (has(off, 2)) {
            memcpy(&v, data + off, 2);
        }
        return v;
    }
    f32 f(u32 off) const
    {
        f32 v = 0;
        if (has(off, 4)) {
            memcpy(&v, data + off, 4);
        }
        return v;
    }
    u8 b(u32 off) const { return off < size ? data[off] : 0; }
    u32 beW(u32 off) const  // a word left big-endian
    {
        return has(off, 4) ? (u32) data[off] << 24 | (u32) data[off + 1] << 16 | (u32) data[off + 2] << 8 | data[off + 3]
                           : 0;
    }
    u16 beH(u32 off) const { return has(off, 2) ? (u16) (data[off] << 8 | data[off + 1]) : 0; }
    template <class T> const T* at(u32 off) const { return has(off, sizeof(T)) ? (const T*) (data + off) : 0; }
};

unsigned hashFloat(unsigned h, f32 v)
{
    return re4cmp_hash_u32(h, re4cmp_float_bits(v));
}

u32 ptrOfs(const void* p)  // an offset the file stores in a pointer field
{
    return (u32) (unsigned long) p;
}

// "a.b" for the file names of assets and archive entries in keys
void tagKey(char* out, const u8* tag)
{
    for (int i = 0; i < 3; i++) {
        u8 c = tag[i];
        out[i] = c >= 32 && c < 127 && c != '.' ? (char) c : '_';
    }
    out[3] = 0;
}

// ---- models (BIN) -----------------------------------------------------------

// smd.cpp sectionCount: the bytes from field `field` to the next BIN section
// (or `end`), divided by `stride`.
u32 sectionCount(const View& v, u32 base, u32 end, u32 field, u32 stride, u32 version)
{
    u32 relStart = v.w(base + field);
    if (relStart < 64) {
        return 0;
    }
    u32 start = base + relStart;
    u32 stop = end;
    static const u32 kFields[] = { 0, 12, 16, 20, 28, 44, 48, 52, 64, 68 };
    for (u32 k = 0; k < 10; k++) {
        if (kFields[k] >= 64 && version != 0x20030818) {
            continue;
        }
        u32 rel = v.w(base + kFields[k]);
        if (rel && !(rel & 0x80000000u) && rel <= end - base && base + rel > start && base + rel < stop) {
            stop = base + rel;
        }
    }
    return stop > start ? (stop - start) / stride : 0;
}

void modelFacts(const Re4Out& o, const View& v, u32 base, u32 end)
{
    const cModelData* d = v.at<cModelData>(base);
    if (!d) {
        return;
    }
    u32 version = d->version;
    o.u(version, "version");
    o.u(d->nParts, "parts");
    o.u(d->weight_palette_num, "wpal");
    o.u(d->weight_ext_num, "wext");
    o.u(d->displist_num, "lists");
    o.u(d->nVtx, "vtx");
    o.u(d->shift, "shift");
    o.u(d->flags, "flags");
    // parts records (ModelDataHead): bytes, then the centre
    u32 head = base + ptrOfs(d->pHead);
    for (u32 j = 0; j < d->nParts; j++) {
        const ModelDataHead* p = v.at<ModelDataHead>(head + j * sizeof(ModelDataHead));
        if (!p) {
            break;
        }
        o.u(p->partsNo, "joint.%u.att", (unsigned) j);
        o.u(p->parentNo, "joint.%u.parent", (unsigned) j);
        o.f(p->center.x, "joint.%u.c.0", (unsigned) j);
        o.f(p->center.y, "joint.%u.c.1", (unsigned) j);
        o.f(p->center.z, "joint.%u.c.2", (unsigned) j);
    }
    // positions: s16 x, y, z, matrix (smd.cpp: value * 2^-shift)
    f32 scale = ldexpf(1.0f, -(int) d->shift);
    u32 vtx = base + ptrOfs(d->vtxOrig);
    unsigned hash = RE4CMP_HASH_START;
    for (u32 i = 0; i < d->nVtx; i++) {
        for (u32 k = 0; k < 3; k++) {
            f32 value = (int) (s16) v.h(vtx + i * 8 + k * 2) * scale;
            hash = hashFloat(hash, value);
            if (i < RE4CMP_LIST) {
                o.f(value, "pos.%u.%u", (unsigned) i, (unsigned) k);
            }
        }
    }
    o.u(d->nVtx, "pos.n");
    o.u(hash, "pos.hash");
    // normals: s8 / 64 (flags bit29) or s16 / 16384
    bool small = (d->flags & 0x20000000u) != 0;
    u32 nrm = base + ptrOfs(d->nrmOrig);
    hash = RE4CMP_HASH_START;
    for (u32 i = 0; i < d->nNrm; i++) {
        for (u32 k = 0; k < 3; k++) {
            f32 value = small ? (int) (s8) v.b(nrm + i * 4 + k) / 64.f : (int) (s16) v.h(nrm + i * 8 + k * 2) / 16384.f;
            hash = hashFloat(hash, value);
            if (i < RE4CMP_LIST) {
                o.f(value, "nrm.%u.%u", (unsigned) i, (unsigned) k);
            }
        }
    }
    o.u(d->nNrm, "nrm.n");
    o.u(hash, "nrm.hash");
    // texture coordinates: u16 pairs up to the next section; s16 / 256 with
    // flags bit31, else u16 / 32768
    bool colored = (d->flags & 0x80000000u) != 0;
    u32 nUv = sectionCount(v, base, end, 16, 4, version);
    u32 uv = base + ptrOfs(d->pTex);
    hash = RE4CMP_HASH_START;
    for (u32 i = 0; i < nUv; i++) {
        for (u32 k = 0; k < 2; k++) {
            u16 word = v.h(uv + i * 4 + k * 2);
            f32 value = colored ? (int) (s16) word / 256.f : word / 32768.f;
            hash = hashFloat(hash, value);
            if (i < RE4CMP_LIST) {
                o.f(value, "uv.%u.%u", (unsigned) i, (unsigned) k);
            }
        }
    }
    o.u(nUv, "uv.n");
    o.u(hash, "uv.hash");
    // vertex colours: RGBA bytes (stay as they are)
    u32 nClr = colored ? sectionCount(v, base, end, 12, 4, version) : 0;
    u32 clr = base + ptrOfs(d->pClr);
    o.u(nClr, "clr.n");
    o.u(nClr && v.has(clr, nClr * 4) ? re4cmp_hash(RE4CMP_HASH_START, v.data + clr, nClr * 4) : 0, "clr.hash");
    // primitive parts: header fields, the GX stream (stays big-endian)
    u32 part = base + ptrOfs(d->pParts);
    hash = RE4CMP_HASH_START;
    for (u32 i = 0; i < d->displist_num; i++) {
        const cPolyHeader* ph = v.at<cPolyHeader>(part);
        if (!ph) {
            break;
        }
        o.u(ph->buff_size, "dl.%u.size", (unsigned) i);
        o.u(ph->nPoly, "dl.%u.npoly", (unsigned) i);
        o.u(ph->flag, "dl.%u.flag", (unsigned) i);
        o.u(ph->material_tex, "dl.%u.tex", (unsigned) i);
        if (v.has(part + sizeof(cPolyHeader), ph->buff_size)) {
            hash = re4cmp_hash(hash, v.data + part + sizeof(cPolyHeader), ph->buff_size);
        }
        part += sizeof(cPolyHeader) + ph->buff_size;
    }
    o.u(hash, "dl.hash");
}

// Skinning palette of a standalone BIN (model_skin.cpp SkinnedModel). The
// weight records are declared in trans.cpp: cWeightExt (u16 ids[3], u16
// count, u8 percent[3], pad) and cWeight (u8 ids[3], u8 count, u8
// percent[3], pad).
void skinFacts(const Re4Out& o, const View& v, u32 base)
{
    const cModelData* d = v.at<cModelData>(base);
    const ModelDataHead* h0 = v.at<ModelDataHead>(base + ptrOfs(d->pHead));
    bool ext = d->weight_ext_num > 255;
    u32 count = ext ? d->weight_ext_num : d->weight_palette_num;
    u32 stride = ext ? 12 : 8;
    u32 table = base + ptrOfs(d->pWeight);
    u32 vtx = base + ptrOfs(d->vtxOrig);
    u32 nrm = base + ptrOfs(d->nrmOrig);
    bool small = (d->flags & 0x20000000u) != 0;
    unsigned hash = RE4CMP_HASH_START;
    if (count == 0 && d->nParts == 1 && h0 && h0->partsNo == 0 && h0->parentNo == 0xFF) {
        // commonModelTrans's unweighted rigid branch: one weight, joint 0
        o.u(1, "skin.n");
        o.u(1, "skin.0.n");
        o.u(0, "skin.0.j0");
        o.f(1.0f, "skin.0.r0");
        hash = re4cmp_hash_u32(hash, 1);
        hash = re4cmp_hash_u32(hash, 0);
        hash = hashFloat(hash, 1.0f);
        o.u(hash, "skin.hash");
        unsigned zero = RE4CMP_HASH_START;
        for (u32 i = 0; i < d->nVtx; i++) {
            zero = re4cmp_hash_u32(zero, 0);
        }
        o.u(d->nVtx, "pw.n");
        o.u(zero, "pw.hash");
        zero = RE4CMP_HASH_START;
        for (u32 i = 0; i < d->nNrm; i++) {
            zero = re4cmp_hash_u32(zero, 0);
        }
        o.u(d->nNrm, "nw.n");
        o.u(zero, "nw.hash");
        o.u(0, "blend.n");
        return;
    }
    o.u(count, "skin.n");
    for (u32 i = 0; i < count; i++) {
        u32 p = table + i * stride;
        u32 n = ext ? v.h(p + 6) : v.b(p + 3);
        f32 total = 0;
        hash = re4cmp_hash_u32(hash, n);
        if (i < RE4CMP_LIST) {
            o.u(n, "skin.%u.n", (unsigned) i);
        }
        for (u32 j = 0; j < n && j < 3; j++) {
            u32 joint = ext ? v.h(p + j * 2) : v.b(p + j);
            f32 rate = j + 1 == n ? 1.f - total : v.b(p + (ext ? 8 : 4) + j) * .01f;
            total += rate;
            hash = re4cmp_hash_u32(hash, joint);
            hash = hashFloat(hash, rate);
            if (i < RE4CMP_LIST) {
                o.u(joint, "skin.%u.j%u", (unsigned) i, (unsigned) j);
                o.f(rate, "skin.%u.r%u", (unsigned) i, (unsigned) j);
            }
        }
    }
    o.u(hash, "skin.hash");
    // matrix index of every position (s16 x4: the fourth) and normal
    hash = RE4CMP_HASH_START;
    for (u32 i = 0; i < d->nVtx; i++) {
        hash = re4cmp_hash_u32(hash, v.h(vtx + i * 8 + 6));
    }
    o.u(d->nVtx, "pw.n");
    o.u(hash, "pw.hash");
    hash = RE4CMP_HASH_START;
    for (u32 i = 0; i < d->nNrm; i++) {
        hash = re4cmp_hash_u32(hash, small ? v.b(nrm + i * 4 + 3) : v.h(nrm + i * 8 + 6));
    }
    o.u(d->nNrm, "nw.n");
    o.u(hash, "nw.hash");
    // motion blend table: u32 count, four halfwords each
    u32 blend = d->version == 0x20030818 ? d->blendTbl : 0;
    u32 nBlend = blend ? v.w(base + blend) : 0;
    o.u(nBlend, "blend.n");
    for (u32 i = 0; i < nBlend && i < RE4CMP_LIST; i++) {
        for (u32 k = 0; k < 4; k++) {
            o.u(v.h(base + blend + 4 + i * 8 + k * 2), "blend.%u.%u", (unsigned) i, (unsigned) k);
        }
    }
}

bool isModel(const View& v, u32 base)
{
    if (!v.has(base, sizeof(cModelData) - 8)) {
        return false;
    }
    u32 version = v.at<cModelData>(base) ? v.at<cModelData>(base)->version : v.w(base + 0x3C);
    return version == 0x20010801 || version == 0x20030817 || version == 0x20030818;
}

void binFacts(const Re4Out& o, const View& v)
{
    if (isModel(v, 0)) {
        modelFacts(o, v, 0, v.size);
        skinFacts(o, v, 0);
    }
}

// ---- SMD / SMX ----------------------------------------------------------------

void smdFacts(const Re4Out& o, const View& v)
{
    const cSmd* s = v.at<cSmd>(0);
    if (!s) {
        return;
    }
    o.u(s->Version, "version");
    o.u(s->Flag, "flags");
    o.u(s->nModel, "count");
    u32 works = 0x10;
    if (s->Flag & 1) {
        u32 n = s->grp.nGroup;
        o.u(n, "groups.n");
        for (u32 i = 0; i < n; i++) {
            o.u(v.w(offsetof(cSmd, grp.num) + i * 4), "groups.%u", (unsigned) i);
        }
        works = 0x14 + n * 4;
    } else {
        o.u(0, "groups.n");
    }
    o.u(works, "works");
    // the tables as far as the placements use them (smd.cpp: SmdSetParam
    // substitutes index 0 for 0xFF on active BIN/TPL links)
    u32 bins = 0, tpls = 0, mots = 0;
    for (u32 i = 0; i < s->nModel; i++) {
        const cSmdWork* w = v.at<cSmdWork>(works + i * sizeof(cSmdWork));
        if (!w) {
            break;
        }
        const f32* f = &w->Pos.x;
        for (u32 k = 0; k < 3; k++) {
            o.f(f[k], "place.%u.pos.%u", (unsigned) i, (unsigned) k);
            o.f(f[3 + k], "place.%u.rot.%u", (unsigned) i, (unsigned) k);
            o.f(f[6 + k], "place.%u.scale.%u", (unsigned) i, (unsigned) k);
        }
        o.u(w->BinId, "place.%u.bin", (unsigned) i);
        o.u(w->TplId, "place.%u.tpl", (unsigned) i);
        o.u(w->MotId, "place.%u.mot", (unsigned) i);
        o.u(w->WorkNo, "place.%u.work", (unsigned) i);
        o.u(w->flags, "place.%u.flags", (unsigned) i);
        if (w->WorkNo == 0xFF) {
            continue;
        }
        if (!(w->flags & 0x10)) {
            u32 b = (w->BinId == 0xFF ? 0 : w->BinId) + 1;
            u32 t = (w->TplId == 0xFF ? 0 : w->TplId) + 1;
            bins = b > bins ? b : bins;
            tpls = t > tpls ? t : tpls;
        }
        if (!(w->flags & 0x40) && w->MotId != 0xFF) {
            mots = (u32) w->MotId + 1 > mots ? w->MotId + 1 : mots;
        }
    }
    u32 tbl[3] = { s->BinTblOfs, s->TplTblOfs, s->MotTblOfs };
    u32 counts[3] = { bins, tpls, mots };
    const char* names[3] = { "bins", "tpls", "mots" };
    u32 offs[3][256];
    for (u32 t = 0; t < 3; t++) {
        o.u(counts[t], "%s.n", names[t]);
        for (u32 i = 0; i < counts[t] && i < 256; i++) {
            offs[t][i] = tbl[t] + v.w(tbl[t] + i * 4);
            o.u(offs[t][i], "%s.%u", names[t], (unsigned) i);
        }
    }
    for (u32 i = 0; i < bins && i < 256; i++) {
        u32 base = offs[0][i];
        u32 end = v.size;
        for (u32 t = 0; t < 3; t++) {
            if (tbl[t] > base && tbl[t] < end) {
                end = tbl[t];
            }
            for (u32 k = 0; k < counts[t] && k < 256; k++) {
                if (offs[t][k] > base && offs[t][k] < end) {
                    end = offs[t][k];
                }
            }
        }
        modelFacts(o.sub("model.%u.", (unsigned) i), v, base, end);
    }
}

void smxFacts(const Re4Out& o, const View& v)
{
    const cSmxData* x = v.at<cSmxData>(0);
    if (!x) {
        return;
    }
    o.u(x->nData, "n");
    for (u32 i = 0; i < x->nData; i++) {
        const cSmxWork* w = v.at<cSmxWork>(sizeof(cSmxData) + i * sizeof(cSmxWork));
        if (!w) {
            break;
        }
        Re4Out r = o.sub("rec.%u.", (unsigned) i);
        r.u(w->ModelNo, "work");
        r.u(w->Id, "type");
        r.u(w->OtType, "order");
        r.u(w->CullMode, "cull");
        r.u(w->LitSelectMask, "lmask");
        r.u(w->Flag, "flags");
        r.u(w->MaterialColor, "mat");
        r.u(w->SpecularColor, "spec");
        r.f(w->TexU, "u");
        r.f(w->TexV, "v");
        // the scroll mover's work area: floats for types 1 and 2 (obj02.cpp)
        if (w->Id == 1 || w->Id == 2) {
            u32 n = w->Id == 1 ? 3 : 13;
            for (u32 k = 0; k < n; k++) {
                f32 value;
                memcpy(&value, w->Free + k * 4, 4);
                r.f(value, "free.%u", (unsigned) k);
            }
        } else {
            r.u(re4cmp_hash(RE4CMP_HASH_START, w->Free, sizeof(w->Free)), "free.hash");
        }
    }
}

// ---- SAT / EAT ------------------------------------------------------------------

void satFacts(const Re4Out& o, const View& v)
{
    u32 begin = 0;
    u8 version = v.b(0);
    if (version != 0xFF && (version & 0x80)) {
        begin = v.w(4);  // cSatHeader: offsets of the files; sat.cpp reads the first
        o.u((begin - 4) / 4, "slots");
    } else {
        o.u(0, "slots");
    }
    const cSatFile* f = v.at<cSatFile>(begin);
    if (!f) {
        return;
    }
    u32 p = begin + sizeof(cSatFile);
    const struct {
        const char* name;
        u32 n;
    } arrays[3] = { { "vtx", f->m_nVertex }, { "nrm", f->m_nNormal }, { "edge", f->m_nEdge } };
    for (u32 a = 0; a < 3; a++) {
        unsigned hash = RE4CMP_HASH_START;
        for (u32 i = 0; i < arrays[a].n * 3; i++) {
            hash = hashFloat(hash, v.f(p + i * 4));
        }
        o.u(arrays[a].n, "%s.n", arrays[a].name);
        o.u(hash, "%s.hash", arrays[a].name);
        p += arrays[a].n * 12;
    }
    unsigned hash = RE4CMP_HASH_START;
    for (u32 i = 0; i < f->m_nPolygon; i++) {
        const SAT_POLY* q = v.at<SAT_POLY>(p + i * sizeof(SAT_POLY));
        if (!q) {
            break;
        }
        u32 fields[9] = { q->v[0], q->v[1], q->v[2], q->m_Normal, q->e[0], q->e[1], q->e[2], q->m_Dummy7, q->m_Status };
        static const char* const kNames[9] = { "v0", "v1", "v2", "n", "e0", "e1", "e2", "res", "attr" };
        for (u32 k = 0; k < 9; k++) {
            hash = re4cmp_hash_u32(hash, fields[k]);
            if (i < RE4CMP_LIST) {
                o.u(fields[k], "poly.%u.%s", (unsigned) i, kNames[k]);
            }
        }
    }
    o.u(f->m_nPolygon, "poly.n");
    o.u(hash, "poly.hash");
    o.u(f->m_nFloor, "floor");
    o.u(f->m_nSlope, "slope");
    o.u(f->m_nWall, "wall");
    // block chain (sat.cpp: depth-first over children and m_pList links)
    u32 blockStart = p + f->m_nPolygon * sizeof(SAT_POLY);
    u32 cap = (u32) f->m_nBlock + 2;
    u32* pending = (u32*) malloc(cap * 2 * sizeof(u32));
    u32* leaves = pending + cap;
    u32 nPending = 0;
    u32 nLeaves = 0;
    u32 visited = 0;
    u32 refs = 0;
    if (f->m_nBlock) {
        pending[nPending++] = blockStart;
    }
    while (nPending && visited < f->m_nBlock + 1) {
        u32 b = pending[--nPending];
        const cSatBlock* blk = v.at<cSatBlock>(b);
        if (!blk) {
            break;
        }
        visited++;
        if (blk->m_Flag & 1) {
            if (nPending < cap) {
                pending[nPending++] = b + offsetof(cSatBlock, idx);
            }
        } else {
            refs += (u32) blk->m_nFloor + blk->m_nSlope + blk->m_nWall;
            if (nLeaves < cap) {
                leaves[nLeaves++] = b;
            }
        }
        u32 next = ptrOfs(blk->m_pList);
        if (next && nPending < cap) {
            pending[nPending++] = b + next;
        }
    }
    o.u(visited, "blocks");
    o.u(refs, "refs");
    // wall polygons in the order of the leaf blocks' addresses
    for (u32 i = 1; i < nLeaves; i++) {
        for (u32 k = i; k > 0 && leaves[k - 1] > leaves[k]; k--) {
            u32 t = leaves[k];
            leaves[k] = leaves[k - 1];
            leaves[k - 1] = t;
        }
    }
    static u8 seen[0x2000];
    memset(seen, 0, sizeof(seen));
    hash = RE4CMP_HASH_START;
    u32 nWall = 0;
    for (u32 i = 0; i < nLeaves; i++) {
        const cSatBlock* blk = v.at<cSatBlock>(leaves[i]);
        u32 start = (u32) blk->m_nFloor + blk->m_nSlope;
        for (u32 j = 0; j < blk->m_nWall; j++) {
            u32 poly = v.h(leaves[i] + offsetof(cSatBlock, idx) + (start + j) * 2);
            if (poly < 0x2000 && !seen[poly]) {
                seen[poly] = 1;
                hash = re4cmp_hash_u32(hash, poly);
                nWall++;
            }
        }
    }
    o.u(nWall, "wallorder.n");
    o.u(hash, "wallorder.hash");
    free(pending);
}

// ---- MAP (the map screen's rooms, Sscrn ss_map.cpp mapRoomNum / mapBinAddr / mapHitAddr)

void mapFacts(const Re4Out& o, const View& v)
{
    u32 n = v.w(0);
    if (n < 2 || n > 64 || !v.has(16, n * 4)) {
        return;
    }
    o.u(n, "count");
    for (u32 i = 0; i < n; i++) {
        u32 ofs = v.w(16 + i * 4);
        u32 end = v.size;
        for (u32 k = 0; k < n; k++) {
            u32 other = v.w(16 + k * 4);
            if (other > ofs && other < end) {
                end = other;
            }
        }
        if (ofs >= v.size) {
            return;
        }
        View part = { v.data + ofs, end - ofs };
        if (i + 2 < n) {
            binFacts(o.sub("model%u.", (unsigned) i), part);
        } else {
            satFacts(o.sub("floor%u.", (unsigned) (i - (n - 2))), part);
        }
    }
}

// ---- AEV (room_flow.cpp RoomFlow) ------------------------------------------------

void areaFacts(const Re4Out& o, const AREA_HIT_DATA& a)
{
    o.u(a.be_flag, "a.en");
    o.u(a.type, "a.type");
    o.f(a.xz4.floor, "a.floor");
    o.f(a.xz4.height, "a.height");
    o.f(a.xz4.radius, "a.radius");
    for (u32 j = 0; j < 4; j++) {
        o.f(a.xz4.p[j].x, "a.p%u.x", (unsigned) j);
        o.f(a.xz4.p[j].z, "a.p%u.z", (unsigned) j);
    }
}

void aevFacts(const Re4Out& o, const View& v)
{
    u32 n = v.h(6);  // SceAtFileHead: magic, u16 version, u16 count
    o.u(n, "count");
    for (u32 i = 0; i < n; i++) {
        const SCE_AT_DATA* r = v.at<SCE_AT_DATA>(0x10 + i * sizeof(SCE_AT_DATA));
        if (!r) {
            break;
        }
        u8 id = r->id;
        if (id != SCEAT_ID_DOOR && id != SCEAT_ID_LADDER && id != SCEAT_ID_SAVE) {
            continue;
        }
        Re4Out a = o.sub("rec.%u.", (unsigned) i);
        a.u(id, "kind");
        a.u(r->no, "no");
        a.u(r->be_flg, "en");
        a.u(r->hit_type, "hit");
        a.u(r->trg_type, "trg");
        a.u(r->target_type, "tgt");
        a.u(r->task_level, "lvl");
        a.u(ptrOfs(r->pParam), "param");
        a.u(ptrOfs((const void*) r->pFunc), "func");
        a.u(r->priority, "prio");
        a.u(r->kind, "skind");
        a.u(r->waiting_type, "wtype");
        a.u(r->waiting_no, "wno");
        a.i(r->hit_dir_ang, "hdir");
        a.i(r->hit_open_ang, "hopen");
        a.u(ptrOfs(r->pParent), "parent");
        a.u(*(const u8*) &r->country, "country");
        areaFacts(a, r->area);
        if (id == SCEAT_ID_SAVE) {
            continue;
        }
        if (id == SCEAT_ID_LADDER) {
            const SCE_AT_DATA_LADDER& l = r->ladder;
            a.f(l.pos.x, "dest.0");
            a.f(l.pos.y, "dest.1");
            a.f(l.pos.z, "dest.2");
            a.f(l.ang_y, "head");
            a.i(l.height, "level");
            a.u(l.type, "ltype");
            a.u(l.pos_set, "posset");
            a.u(l.cam_no, "cam0");
            a.u(l.cam_no2, "cam1");
            a.u(l.cam_no3, "cam2");
            continue;
        }
        const SCE_AT_DATA_DOOR& d = r->door;
        a.f(d.next_pos.x, "dest.0");
        a.f(d.next_pos.y, "dest.1");
        a.f(d.next_pos.z, "dest.2");
        a.f(d.next_ang_y, "head");
        a.u((u32) d.next_stage_no << 8 | d.next_room_no, "room");
        a.u(d.key_id, "key");
        a.u(d.key_flg, "keyflg");
        a.u(ptrOfs((const void*) d.pExitFunc), "exitfn");
        a.u(d.next_part_no, "part");
        a.u(d.fade_eff, "fade");
        a.u(ptrOfs(d.pExitParam), "exitparam");
    }
}

// ---- LIT / CAM (room_lighting.cpp) -----------------------------------------------

u32 color(const GXColor& c)
{
    return (u32) c.r << 24 | (u32) c.g << 16 | (u32) c.b << 8 | c.a;
}

void litFacts(const Re4Out& o, const View& v)
{
    const cLit* lit = v.at<cLit>(0);
    if (!lit) {
        return;
    }
    u32 version = lit->Version;
    o.u(version, "version");
    o.u(lit->CutNum, "cuts.n");
    for (u32 c = 0; c < lit->CutNum; c++) {
        u32 ofs = v.w(4 + c * 4);
        Re4Out cut = o.sub("cut.%u.", (unsigned) c);
        if (ofs == 0) {
            cut.u(0, "present");
            continue;
        }
        const cLightEnv* e = v.at<cLightEnv>(ofs);
        if (!e) {
            continue;
        }
        cut.u(1, "present");
        cut.u(color(e->AmbientScr), "amb.scr");
        cut.u(color(e->AmbientEm), "amb.act");
        cut.u(color(e->AmbientEsp), "amb.eff");
        cut.u(e->Fog.Type, "fog.type");
        cut.f(e->Fog.Start, "fog.start");
        cut.f(e->Fog.End, "fog.end");
        cut.u(color(e->Fog.Color), "fog.clr");
        cut.u(e->MirrorFog.Type, "mfog.type");
        cut.u(color(e->MirrorFog.Color), "mfog.clr");
        if (e->MirrorFog.Type & 7) {
            cut.f(e->MirrorFog.Start, "mfog.start");
            cut.f(e->MirrorFog.End, "mfog.end");
        }
        cut.i(e->FocusZ, "focus.depth");
        cut.u(e->FocusLevel, "focus.level");
        cut.u(e->FocusMode, "focus.mode");
        cut.u(e->blur_rate, "blur.rate");
        cut.u(e->blur_type, "blur.type");
        cut.i(e->blur_power, "blur.power");
        cut.f(e->lod_bias, "lodbias");
        cut.u(e->nLight, "lights.n");
        for (u32 l = 0; l < e->nLight; l++) {
            const cLightWork* w = v.at<cLightWork>(ofs + sizeof(cLightEnv) + l * sizeof(cLightWork));
            if (!w) {
                break;
            }
            Re4Out li = cut.sub("light.%u.", (unsigned) l);
            if ((w->BeFlag & 3) != 3) {
                li.u(0, "active");  // room_lighting.cpp keeps only the basic bytes of these
                continue;
            }
            li.u(w->BeFlag, "flags");
            li.u(w->Type, "type");
            li.u(w->Id, "id");
            // cLit::versionUp: lights of old files also on screen kind 0x40
            u32 mask = w->EnableMask;
            if (version <= 0x29 && (mask & 1)) {
                mask |= 0x40;
            }
            li.u(mask, "mask");
            // cLight::calcParent detaches invalid parent kinds
            u32 parent = w->ParentType, parentNo = w->ParentNo;
            if (parent > 4) {
                parent = 0;
                parentNo = 1;
            }
            li.u(parent, "parent");
            li.u(parentNo, "parentno");
            li.u(w->Kind, "kind");
            li.u(w->Attribute, "attr");
            li.f(w->Pos.x, "pos.0");
            li.f(w->Pos.y, "pos.1");
            li.f(w->Pos.z, "pos.2");
            li.f(w->Radius, "radius");
            li.u(color(w->Col), "clr");
            li.f(w->Intensity, "intensity");
            const LIT_TYPE04_FREE& s = w->spot;
            if (w->Type >= 3 && w->Type <= 6) {
                li.f(s.Normal.x, "dir.0");
                li.f(s.Normal.y, "dir.1");
                li.f(s.Normal.z, "dir.2");
            } else {
                li.f(s.Normal.x, "dir.0");
            }
            if (w->Type == 3 || w->Type == 6) {
                const LIT_TYPE03_FREE* t = (const LIT_TYPE03_FREE*) &w->spot;
                li.f(t->CutOff, "cutoff");
                li.f(t->Edge, "edge");
            } else if (w->Type == 5) {
                const LIT_TYPE05_FREE* t = (const LIT_TYPE05_FREE*) &w->spot;
                li.u(t->Flag, "dflags");
                li.f(t->Edge, "edge");
            } else if (w->Type == 4) {
                const f32 att[6] = { s.A0, s.A1, s.A2, s.K0, s.K1, s.K2 };
                for (u32 k = 0; k < 6; k++) {
                    li.f(att[k], "att.%u", (unsigned) k);
                }
            }
            if (w->Id == 4) {
                const LIT04_MOVE_FREE* sh = (const LIT04_MOVE_FREE*) &w->sub;
                li.u(sh->Flag, "sh.flags");
                li.u(sh->Kind, "sh.kind");
                li.u(sh->Tex_no, "sh.tex");
                li.i(sh->Ang_x, "sh.ax");
                li.i(sh->Ang_y, "sh.ay");
                li.u(sh->Fovy, "sh.fovy");
                li.u(sh->SelfShadowLevel, "sh.self");
                li.u(sh->SoftShadowLevel, "sh.soft");
                li.u(sh->Fovy_sub, "sh.fovysub");
                if (sh->Flag & 2) {
                    li.f(sh->Lit_pos.x, "sh.src.0");
                    li.f(sh->Lit_pos.y, "sh.src.1");
                    li.f(sh->Lit_pos.z, "sh.src.2");
                }
            }
        }
    }
}

void camFacts(const Re4Out& o, const View& v)
{
    const CAM_FILE_HEADER* h = v.at<CAM_FILE_HEADER>(0);
    if (!h) {
        return;
    }
    u32 n = 0;
    for (u32 i = 0; i < h->nAdat; i++) {
        const CUT_INFO* c = v.at<CUT_INFO>(sizeof(CAM_FILE_HEADER) + i * sizeof(CUT_INFO));
        u32 p = c ? ptrOfs(c->pAdat) : 0;
        const AREA_DATA* a = p ? v.at<AREA_DATA>(p) : 0;
        if (!a || !(a->Be_flag & 1)) {
            continue;
        }
        Re4Out r = o.sub("area.%u.", (unsigned) n++);
        r.u(a->Be_flag, "flags");
        r.u((u8) a->No, "no");
        r.u(a->Attr, "attr");
        r.u(a->Type_char, "char");
        if (a->Attr & 0x40) {
            r.f(a->Dir, "dir");
        }
        r.f(a->Height, "height");
        r.f(a->Y, "floor");
        u32 ver = ptrOfs(a->pVer);
        unsigned hash = RE4CMP_HASH_START;
        for (s32 k = 0; k < a->nVer * 3; k++) {
            hash = hashFloat(hash, v.f(ver + k * 4));
        }
        r.u(a->nVer, "v.n");
        r.u(hash, "v.hash");
    }
    o.u(n, "areas.n");
}

// ---- motion curves (event_data.cpp EventCurve) ------------------------------------

void curveFacts(const Re4Out& o, const View& v)
{
    // MotionData: u16 maxFrame, u8 count, u16 kind[n] (odd address), u8
    // index[n], pad to 4, u32 size, u32 offsets[n]
    u32 n = v.b(2);
    u32 table = (3 + n * 3 + 3) & ~3u;
    o.u(v.h(0) & 0x3FFF, "max");
    o.u(n, "ch.n");
    static const u8 kWidths[16][3] = { { 4, 4, 4 }, { 4, 2, 2 }, { 4, 1, 1 }, { 0, 0, 0 }, { 2, 4, 4 }, { 2, 2, 2 },
                                       { 2, 1, 1 }, { 0, 0, 0 }, { 1, 4, 4 }, { 1, 2, 2 }, { 1, 1, 1 }, { 0, 0, 0 },
                                       { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 4, 0, 0 } };
    for (u32 c = 0; c < n; c++) {
        u16 kind = v.h(3 + c * 2);
        Re4Out ch = o.sub("ch.%u.", (unsigned) c);
        ch.u(kind, "kind");
        ch.u(v.b(3 + n * 2 + c), "idx");
        u32 p = v.w(table + 4 + c * 4);
        const u8* w = kWidths[kind >> 12];
        for (u32 axis = 0; axis < 3; axis++) {
            u32 count = v.h(p);  // counts and frames are converted
            p += 2;
            unsigned fh = RE4CMP_HASH_START, vh = RE4CMP_HASH_START;
            for (u32 k = 0; k < count; k++) {
                fh = re4cmp_hash_u32(fh, v.h(p));
                p += 2;
            }
            for (u32 k = 0; k < count; k++) {  // key values stay big-endian
                for (u32 x = 0; x < 3; x++) {
                    u32 value = w[x] == 4 ? v.beW(p) : w[x] == 2 ? v.beH(p) : w[x] == 1 ? v.b(p) : 0;
                    if (w[x]) {
                        vh = re4cmp_hash_u32(vh, value);
                    }
                    p += w[x];
                }
            }
            ch.u(count, "ax%u.n", (unsigned) axis);
            ch.u(fh, "ax%u.fh", (unsigned) axis);
            ch.u(vh, "ax%u.vh", (unsigned) axis);
        }
    }
}

// ---- conversion -------------------------------------------------------------------

struct Copy {
    u8* data;
    u32 size;
    explicit Copy(const unsigned char* src, unsigned n) : data((u8*) malloc(n + 64)), size(n)
    {
        memcpy(data, src, n);
        memset(data + n, 0, 64);
    }
    ~Copy() { free(data); }
};

const char* const kIncomplete = "!incomplete";

void entryFacts(const Re4Out& o, const char* tag, const View& e)
{
    if (strcmp(tag, "SMD") == 0) {
        smdFacts(o, e);
    } else if (strcmp(tag, "SMX") == 0) {
        smxFacts(o, e);
    } else if (strcmp(tag, "SAT") == 0 || strcmp(tag, "EAT") == 0) {
        satFacts(o, e);
    } else if (strcmp(tag, "AEV") == 0) {
        aevFacts(o, e);
    } else if (strcmp(tag, "LIT") == 0) {
        litFacts(o, e);
    } else if (strcmp(tag, "CAM") == 0) {
        camFacts(o, e);
    } else if (strcmp(tag, "BIN") == 0) {
        binFacts(o, e);
    } else if (strcmp(tag, "MAP") == 0) {
        mapFacts(o, e);
    } else if (strcmp(tag, "FCV") == 0) {
        curveFacts(o, e);
    }
}

}  // namespace

namespace {

// The facts of a converted archive at `data`.
void archiveFacts(u8* data, u32 size, Re4Facts* out);

}  // namespace

extern "C" int re4cmp_game_archive(const unsigned char* data, unsigned size, const char* name, Re4Facts* out)
{
    Copy c(data, size);
    Blob b = blob(c.data, size, name);
    Part part = { name, 0, 0, 0, 0, 0 };
    if (!looksLikeArchive(b)) {
        return 0;
    }
    convertArchive(b, part);
    archiveFacts(c.data, size, out);
    return 1;
}

// op/opNN.das: the whole part through the codec converter, the archive read at +0x400
extern "C" int re4cmp_game_codec(const unsigned char* data, unsigned size, const char* name, Re4Facts* out)
{
    Copy c(data, size);
    Blob b = blob(c.data, size, name);
    Part part = { name, 0, 0, 0, 0, 0 };
    if (size <= 0x420) {
        return 0;
    }
    Blob arc = blob(c.data + 0x400, size - 0x400, name);
    if (!looksLikeArchive(arc)) {
        return 0;
    }
    u32 count = be32(arc, 0);
    convertCodecArchive(b, part);
    u32 host = 0;
    memcpy(&host, c.data + 0x400, 4);
    if (!b.ok || host != count) {
        return 0;  // the archive was not converted: its offsets are still big-endian
    }
    archiveFacts(c.data + 0x400, size - 0x400, out);
    return 1;
}

namespace {

void archiveFacts(u8* data, u32 size, Re4Facts* out)
{
    Re4Out o = re4cmp_out(out);
    View v = { data, size };
    u32 count = v.w(0);
    u32 rel = v.w(4);
    u32 end = rel ? rel : size;
    o.u(count, "arc.count");
    u32 occurrence[4096];
    for (u32 i = 0; i < count; i++) {
        const u8* tag = data + 0x10 + count * 4 + i * 4;
        char key[4];
        tagKey(key, tag);
        u32 ofs = v.w(0x10 + i * 4);
        o.s(key, "arc.%u.tag", (unsigned) i);
        o.u(ofs, "arc.%u.offset", (unsigned) i);
        occurrence[i] = 0;
        for (u32 k = 0; k < i; k++) {
            if (memcmp(data + 0x10 + count * 4 + k * 4, tag, 3) == 0) {
                occurrence[i]++;
            }
        }
        u32 stop = end;
        for (u32 k = 0; k < count; k++) {
            u32 other = v.w(0x10 + k * 4);
            if (other > ofs && other < stop) {
                stop = other;
            }
        }
        Re4Out e = o.sub("%s%u.", key, (unsigned) occurrence[i]);
        // an entry whose offset a later entry repeats has no data of its own
        bool empty = false;
        for (u32 k = i + 1; k < count && !empty; k++) {
            empty = v.w(0x10 + k * 4) == ofs;
        }
        char noted[5] = { 0 };
        int status = entryStatus(data + ofs, noted);
        for (u32 k = 0; k < 4; k++) {
            noted[k] = noted[k] >= 0x20 && noted[k] < 0x7F ? noted[k] : 0;
        }
        char info[160];
        snprintf(info, sizeof(info), "#%u %s @0x%lx, 0x%lx bytes, %s%s%s", (unsigned) i, key, (unsigned long) ofs,
                 (unsigned long) (stop > ofs ? stop - ofs : 0),
                 empty                          ? "leer (Daten gehören dem nächsten Eintrag)"
                 : status == kEntryConverted    ? "umgewandelt"
                 : status == kEntryIncomplete   ? "unvollständig"
                 : status == kEntryNoConverter  ? "kein Umwandler"
                                                : "nicht notiert",
                 status && !empty ? " als " : "", status && !empty ? noted : "");
        o.s(info, "!entry.%s%u.", key, (unsigned) occurrence[i]);
        if (empty) {
            e.u(1, "empty");
            continue;
        }
        if (status == kEntryIncomplete) {
            e.u(1, kIncomplete);
        }
        View ev = { data + ofs, stop > ofs ? stop - ofs : 0 };
        entryFacts(e, key, ev);
    }
}

}  // namespace

extern "C" int re4cmp_game_esl(const unsigned char* data, unsigned size, const char* name, Re4Facts* out)
{
    Copy c(data, size);
    Blob b = blob(c.data, size, name);
    Part part = { name, 0, 0, 0, 0, 0 };
    convertEmList(b, part);
    Re4Out o = re4cmp_out(out);
    const double kRotation = 3.14159265358979323846 / 16384.0;  // enemy_spawn.cpp RotationUnit
    for (u32 i = 0; i < size / sizeof(EM_LIST) && i < 255; i++) {
        const EM_LIST* e = (const EM_LIST*) (c.data + i * sizeof(EM_LIST));
        Re4Out r = o.sub("rec.%u.", (unsigned) i);
        r.u(e->be_flag, "be");
        r.u(e->id, "id");
        r.u(e->type, "type");
        r.u(e->set, "set");
        r.u(e->flag, "flag");
        r.u(e->hp, "hp");
        r.u(e->emset_no, "emset");
        r.u(e->Character, "char");
        for (u32 k = 0; k < 3; k++) {
            r.d(e->pos[k] * 10.0, "pos.%u", (unsigned) k);
            r.d(e->rot[k] * kRotation, "rot.%u", (unsigned) k);
        }
        r.u(e->room, "room");
        r.d(e->Guard_r * 1000.0, "guard");
    }
    return b.ok ? 1 : 0;
}

extern "C" int re4cmp_game_roominfo(const unsigned char* data, unsigned size, const char* name, Re4Facts* out)
{
    Copy c(data, size);
    Blob b = blob(c.data, size, name);
    Part part = { name, 0, 0, 0, 0, 0 };
    convertRoomInfo(b, part);
    Re4Out o = re4cmp_out(out);
    View v = { c.data, size };
    u32 stages = v.w(0);
    o.u(stages, "stages");
    static u32 point[0x10000];  // room_flow.cpp: start points numbered per room
    memset(point, 0, sizeof(point));
    u32 n = 0;
    for (u32 s = 0; s < stages && s < 64; s++) {
        u32 ofs = v.w(4 + s * 4);
        if (ofs == 0) {
            continue;
        }
        u32 count = v.w(ofs);
        for (u32 i = 0; i < count; i++) {
            const CRoomInfo* r = v.at<CRoomInfo>(ofs + 4 + i * sizeof(CRoomInfo));
            if (!r) {
                break;
            }
            Re4Out e = o.sub("start.%u.", (unsigned) n++);
            e.u(r->flag, "flags");
            e.u(r->roomNo, "room");
            e.u(point[r->roomNo]++, "point");
            e.f(r->pos.x, "pos.0");
            e.f(r->pos.y, "pos.1");
            e.f(r->pos.z, "pos.2");
            e.f(r->angle, "head");
            const char* texts[3] = { r->name, r->person, r->person2 };
            const char* names[3] = { "name", "person", "person2" };
            for (u32 k = 0; k < 3; k++) {
                u32 t = ptrOfs(texts[k]);
                char text[513] = "";
                if (t != 0 && t < size) {
                    u32 len = 0;
                    while (len < 512 && t + len < size && c.data[t + len]) {
                        len++;
                    }
                    memcpy(text, c.data + t, len);
                    text[len] = 0;
                }
                e.s(text, "%s", names[k]);
            }
        }
    }
    return b.ok ? 1 : 0;
}

namespace {

// event_data.cpp's string(): a name of at most n bytes, terminated inside them
void fixedString(char* out, const char* in, u32 n)
{
    u32 len = 0;
    while (len < n && in[len]) {
        len++;
    }
    memcpy(out, in, len);
    out[len] = 0;
}

bool hasExt(const char* name, const char* ext)
{
    const char* dot = strrchr(name, '.');
    return dot && strcasecmp(dot, ext) == 0;
}

}  // namespace

extern "C" int re4cmp_game_evd(const unsigned char* data, unsigned size, const char* name, Re4Facts* out)
{
    Copy c(data, size);
    Blob b = blob(c.data, size, name);
    convertEvdAt(b, 0, size);
    Re4Out o = re4cmp_out(out);
    View v = { c.data, size };
    const EvtHeader* h = v.at<EvtHeader>(0);
    if (!h) {
        return 0;
    }
    char room[16], no[16], full[32];
    fixedString(room, h->room, 8);
    fixedString(no, h->no, 12);
    snprintf(full, sizeof(full), "%s%s", room, no);
    o.s(full, "name");
    o.u((u32) h->sndFlag, "snd");
    o.u(h->pacOfs, "pac.start");
    o.u(h->pacSize, "pac.size");
    // assets: each extends to the next one or the end of the file
    u32 nBin = (u32) h->nBin;
    o.u(nBin, "a.n");
    for (u32 i = 0; i < nBin; i++) {
        const EvtBinEntry* e = v.at<EvtBinEntry>(h->binOfs + i * sizeof(EvtBinEntry));
        if (!e) {
            break;
        }
        char an[0x31];
        fixedString(an, e->name, 0x30);
        u32 ext = size - e->ofs;
        for (u32 k = 0; k < nBin; k++) {
            u32 other = v.w(h->binOfs + k * sizeof(EvtBinEntry) + offsetof(EvtBinEntry, ofs));
            if (other > e->ofs && other - e->ofs < ext) {
                ext = other - e->ofs;
            }
        }
        o.s(an, "a.%u.name", (unsigned) i);
        o.u(e->ofs, "a.%u.ofs", (unsigned) i);
        o.u(ext, "a.%u.ext", (unsigned) i);
        View av = { c.data + e->ofs, ext };
        Re4Out ao = o.sub("asset.%s.", an);
        if (hasExt(an, ".fcv")) {
            curveFacts(ao, av);
        } else if (hasExt(an, ".lit")) {
            litFacts(ao, av);
        } else if (hasExt(an, ".bin")) {
            binFacts(ao, av);
        }
    }
    // packets (event.cpp Event::ExePacket_*)
    u32 p = h->pacOfs;
    u32 stop = h->pacOfs + h->pacSize;
    u32 n = 0;
    u32 cuts[4096];
    u32 nCut = 0;
    unsigned long total = 0;
    while (p + 0x10 <= stop && v.has(p, 0x10)) {
        const EvtPacket* k = v.at<EvtPacket>(p);
        s16 len = k->size;
        if (len < 0x10) {
            break;
        }
        Re4Out pk = o.sub("pk.%u.", (unsigned) n++);
        pk.u((u32) k->id, "type");
        pk.u(k->flag, "flags");
        pk.i(k->cut, "cut");
        pk.i(k->frame, "frame");
        pk.u((u32) len, "len");
        pk.u(p, "off");
        switch (k->id) {
        case EvpTpCamDammy:
        case EvpTpStr:
        case EvpTpSe:
        case EvpTpMes:
        case EvpTpFunc:
        case EvpTpFade:
            for (u32 x = 0; x < 3 && 16 + (x + 1) * 4 <= (u32) len; x++) {
                pk.i((s32) v.w(p + 16 + x * 4), "v%u", (unsigned) x);
            }
            break;
        case EvpTpPos:
            for (u32 x = 0; x < 7; x++) {
                pk.i((s32) v.w(p + offsetof(EvtPacket, pos.pos) + x * 4), "p%u", (unsigned) x);
            }
            break;
        case EvpTpEsp:
            pk.i(k->esp.type, "esp");
            break;
        default:
            break;
        }
        // cut lengths: a camera curve's maxFrame + 1, or the dummy camera's frames
        if (k->id == EvpTpCam || k->id == EvpTpCamDammy) {
            u32 frames = 0;
            if (k->id == EvpTpCamDammy) {
                frames = v.w(p + 16);
            } else {
                char cn[0x31];
                fixedString(cn, (const char*) c.data + p + 16, 0x30);
                for (u32 i = 0; i < nBin; i++) {
                    const EvtBinEntry* e = v.at<EvtBinEntry>(h->binOfs + i * sizeof(EvtBinEntry));
                    if (e && strncmp(e->name, cn, 0x30) == 0) {
                        frames = (u32) v.h(e->ofs) + 1;
                        break;
                    }
                }
            }
            if (nCut < 4096) {
                cuts[nCut++] = frames;
            }
            total += frames;
        }
        p += (u32) len;
        if (k->id == EvpTpEndPac) {
            break;
        }
    }
    o.u(n, "packets.n");
    o.u(nCut, "cuts.n");
    for (u32 i = 0; i < nCut; i++) {
        o.u(cuts[i], "cut.%u", (unsigned) i);
    }
    o.u(total, "total");
    if (!b.ok) {
        o.u(1, kIncomplete);
    }
    return 1;
}
