// Formatumwandler: room scenery and collision.
//
//   SMD  scroll models (scroll.h cSmd): placements + BIN / TPL / motion tables
//   SMX  scroll object parameters (scroll.h cSmxData / cSmxWork)
//   SHD  shadow objects (shadow.h ShdHeader) + their BIN models
//   ETM  room etc archive (EtcModel.cpp EtcArc): named BIN/TPL/FCV/SEQ/EFF files
//   ITM  item model pack (item_model.cpp): id, BIN and TPL tables
//   SAT / EAT  collision (atari.h cSatFile / cSatHeader / cSatBlock)
#include "formats.h"

#include "atari.h"
#include "re4_runtime.h"
#include "scroll.h"
#include "shadow.h"

#include <string.h>

using namespace re4fmt;

static_assert(sizeof(cSmdWork) == 0x48, "cSmdWork layout");
static_assert(offsetof(cSmd, work) == 0x10, "cSmd layout");
static_assert(sizeof(cSmxWork) == 0x90, "cSmxWork layout");
static_assert(sizeof(cSmxData) == 0x10, "cSmxData layout");
static_assert(sizeof(ShdEntry) == 0x48, "ShdEntry layout");
static_assert(offsetof(ShdHeader, entry) == 0x10, "ShdHeader layout");
static_assert(sizeof(cSatFile) == 0x14, "cSatFile layout");
static_assert(sizeof(SAT_POLY) == 0x14, "SAT_POLY layout");
static_assert(offsetof(cSatBlock, idx) == 0x24, "cSatBlock layout");

namespace re4fmt {

namespace {

// An offset table at `tbl` (u32 offsets from the table, data right after
// it): the entry count is implied by the smallest offset.
u32 tableCount(Blob& b, u32 tbl, u32 max)
{
    // A table offset at the very end of the block is an empty table: the SMDs
    // of r100 end with their motion table offset and use no motion.
    if (tbl == 0 || tbl == b.size || !fits(b, tbl, 4, "offset table")) {
        return 0;
    }
    u32 limit = 0xFFFFFFFFu;
    u32 n = 0;
    while (n < max && tbl + n * 4 + 4 <= b.size && n * 4 < limit) {
        u32 o = be32(b, tbl + n * 4);
        if (o == 0 || o % 4 != 0 || tbl + o >= b.size) {
            break;
        }
        if (o < limit) {
            limit = o;
        }
        n++;
    }
    return n;
}

// The section after `start` among the known starts (models, textures,
// motions), else the end of the block: bounds a model's last section.
struct Sections {
    u32 start[1024];
    u32 n;
    Sections() : n(0) {}
    void add(u32 o)
    {
        if (n < 1024) {
            start[n++] = o;
        }
    }
    u32 end(u32 o, u32 size) const
    {
        u32 e = size;
        for (u32 i = 0; i < n; i++) {
            if (start[i] > o && start[i] < e) {
                e = start[i];
            }
        }
        return e;
    }
};

// Converts the BIN / TPL / motion tables at the given offsets (0 = none).
void convertModelTables(Blob& b, u32 binTbl, u32 tplTbl, u32 motTbl, u32 floor)
{
    u32 nBin = tableCount(b, binTbl, 256);
    u32 nTpl = tableCount(b, tplTbl, 256);
    u32 nMot = tableCount(b, motTbl, 256);
    Sections s;
    s.add(binTbl);
    s.add(tplTbl);
    s.add(motTbl);
    u32 bin[256], tpl[256], mot[256];
    for (u32 i = 0; i < nBin; i++) {
        bin[i] = binTbl + be32(b, binTbl + i * 4);
        s.add(bin[i]);
    }
    for (u32 i = 0; i < nTpl; i++) {
        tpl[i] = tplTbl + be32(b, tplTbl + i * 4);
        s.add(tpl[i]);
    }
    for (u32 i = 0; i < nMot; i++) {
        mot[i] = motTbl + be32(b, motTbl + i * 4);
        s.add(mot[i]);
    }
    if (binTbl) {
        swap32(b, binTbl, nBin);
    }
    if (tplTbl) {
        swap32(b, tplTbl, nTpl);
    }
    if (motTbl) {
        swap32(b, motTbl, nMot);
    }
    (void) floor;
    Seen seen;
    for (u32 i = 0; i < nBin && b.ok; i++) {
        if (seen.first(bin[i])) {
            convertBinAt(b, bin[i], s.end(bin[i], b.size));
        }
    }
    for (u32 i = 0; i < nTpl && b.ok; i++) {
        if (seen.first(tpl[i])) {
            convertTplAt(b, tpl[i]);
        }
    }
    for (u32 i = 0; i < nMot && b.ok; i++) {
        if (seen.first(mot[i])) {
            convertMotionAt(b, mot[i]);
        }
    }
}

}  // namespace

// SMD: header, optional group counts, placements, then the tables.
void convertSmd(Blob& b, const Part&)
{
    if (!fits(b, 0, 0x14, "cSmd")) {
        return;
    }
    u32 nModel = be16(b, offsetof(cSmd, nModel));
    u32 binTbl = be32(b, offsetof(cSmd, BinTblOfs));
    u32 tplTbl = be32(b, offsetof(cSmd, TplTblOfs));
    u32 motTbl = be32(b, offsetof(cSmd, MotTblOfs));
    u32 works = 0x10;
    swap16(b, offsetof(cSmd, nModel));
    swap32(b, offsetof(cSmd, BinTblOfs), 3);
    if (b.data[offsetof(cSmd, Flag)] & 1) {  // group count table in front of the works
        u32 nGroup = be32(b, 0x10);
        if (!fits(b, 0x14, nGroup * 4, "SMD group counts")) {
            return;
        }
        swap32(b, 0x10, 1 + nGroup);
        works = 0x14 + nGroup * 4;
    }
    if (!fits(b, works, nModel * sizeof(cSmdWork), "cSmdWork")) {
        return;
    }
    for (u32 i = 0; i < nModel; i++) {
        u32 o = works + i * sizeof(cSmdWork);
        swap32(b, o, 9);  // Pos, Ang, Scale
        RE4_SWAP(b, o, cSmdWork, Radius);
        RE4_SWAP(b, o, cSmdWork, Flag);
        RE4_SWAP(b, o, cSmdWork, flags);
    }
    convertModelTables(b, binTbl, tplTbl, motTbl, works + nModel * sizeof(cSmdWork));
}

// SMX: header, cSmxWork records. Free (0x74 bytes, copied to the scroll
// object's work) is read by the scroll mover of the object type (obj02.cpp):
// type 1 ObjScrRotWork (Vec, flag byte), type 2 ObjScrSwingWork (13 floats).
void convertSmx(Blob& b, const Part&)
{
    u32 n = b.size >= sizeof(cSmxData) ? b.data[offsetof(cSmxData, nData)] : 0;
    if (!fits(b, 0, sizeof(cSmxData) + n * sizeof(cSmxWork), "cSmxWork")) {
        return;
    }
    swap32(b, offsetof(cSmxData, Dummy10), 3);
    static u32 reported;
    for (u32 i = 0; i < n; i++) {
        u32 o = sizeof(cSmxData) + i * sizeof(cSmxWork);
        u8 type = b.data[o + offsetof(cSmxWork, Id)];
        RE4_SWAP(b, o, cSmxWork, LitSelectMask);
        RE4_SWAP(b, o, cSmxWork, Flag);
        RE4_SWAP(b, o, cSmxWork, MaterialColor);
        RE4_SWAP(b, o, cSmxWork, SpecularColor);
        RE4_SWAP(b, o, cSmxWork, TexU);
        RE4_SWAP(b, o, cSmxWork, TexV);
        u32 f = o + offsetof(cSmxWork, Free);
        if (type == 1) {
            swap32(b, f, 3);
        } else if (type == 2) {
            swap32(b, f, 13);
        } else if (type < 32 && !(reported & (1u << type))) {
            bool zero = true;
            for (u32 k = 0; k < sizeof(((cSmxWork*) 0)->Free); k++) {
                zero = zero && b.data[f + k] == 0;
            }
            if (!zero) {
                reported |= 1u << type;
                re4_log("formats: SMX object type %u (first in %s): work area has no field list, left big-endian",
                        type, b.name);
            }
        }
    }
}

// SHD: header, ShdEntry records, model offset table (from itself) and models.
void convertShd(Blob& b, const Part&)
{
    if (!fits(b, 0, offsetof(ShdHeader, entry), "ShdHeader")) {
        return;
    }
    u32 n = be16(b, offsetof(ShdHeader, num));
    u32 tbl = be32(b, offsetof(ShdHeader, tblOfs));
    RE4_SWAP(b, 0, ShdHeader, num);
    RE4_SWAP(b, 0, ShdHeader, tblOfs);
    if (!fits(b, offsetof(ShdHeader, entry), n * sizeof(ShdEntry), "ShdEntry")) {
        return;
    }
    for (u32 i = 0; i < n; i++) {
        swap32(b, offsetof(ShdHeader, entry) + i * sizeof(ShdEntry), 9);  // pos, rot, scale
    }
    convertModelTables(b, tbl, 0, 0, 0);
}

// ETM (EtcModel.cpp EtcArc / EtcArcFile): u32 count, files from 0x20, each
// u32 size (to the next file), name at 0x20, data at 0x40. Converted by the
// name's extension.
void convertEtcArchive(Blob& b, const Part& part)
{
    u32 n = be32(b, 0);
    swap32(b, 0);
    u32 f = 0x20;
    for (u32 i = 0; i < n && b.ok; i++) {
        if (!fits(b, f, 0x40, "EtcArcFile")) {
            return;
        }
        u32 size = be32(b, f);
        swap32(b, f);
        if (size < 0x40 || !fits(b, f, size, "EtcArcFile data")) {
            return;
        }
        char name[0x21];
        memcpy(name, b.data + f + 0x20, 0x20);
        name[0x20] = 0;
        char label[200];
        snprintf(label, sizeof(label), "%s %s", b.name, name);
        Blob e = blob(b.data + f + 0x40, size - 0x40, label);
        convertNamedFile(e, part, name);
        f += size;
    }
}

// ITM (item_model.cpp ItemModelPack): x0, offsets of the id table (u32 count,
// {u16 id, u16, u32} records), the BIN table and the TPL table (u32 count,
// offsets from the table).
void convertItemModels(Blob& b, const Part&)
{
    if (!fits(b, 0, 16, "ItemModelPack")) {
        return;
    }
    u32 ids = be32(b, 4);
    u32 bins = be32(b, 8);
    u32 tpls = be32(b, 12);
    swap32(b, 0, 4);
    u32 n = be32(b, ids);
    if (!fits(b, ids + 4, n * 8, "ItemModelId")) {
        return;
    }
    swap32(b, ids);
    for (u32 i = 0; i < n; i++) {
        swap16(b, ids + 4 + i * 8, 2);
        swap32(b, ids + 4 + i * 8 + 4);
    }
    Sections s;
    s.add(ids);
    s.add(bins);
    s.add(tpls);
    u32 nb = be32(b, bins), nt = be32(b, tpls);
    if (!fits(b, bins + 4, nb * 4, "item BIN table") || !fits(b, tpls + 4, nt * 4, "item TPL table")) {
        return;
    }
    for (u32 i = 0; i < nb; i++) {
        s.add(bins + be32(b, bins + 4 + i * 4));
    }
    for (u32 i = 0; i < nt; i++) {
        s.add(tpls + be32(b, tpls + 4 + i * 4));
    }
    Seen seen;
    for (u32 i = 0; i < nb && b.ok; i++) {
        u32 o = bins + be32(b, bins + 4 + i * 4);
        if (seen.first(o)) {
            convertBinAt(b, o, s.end(o, b.size));
        }
    }
    for (u32 i = 0; i < nt && b.ok; i++) {
        u32 o = tpls + be32(b, tpls + 4 + i * 4);
        if (seen.first(o)) {
            convertTplAt(b, o);
        }
    }
    swap32(b, bins, 1 + nb);
    swap32(b, tpls, 1 + nt);
}

// ---- SAT / EAT ----------------------------------------------------------------

namespace {

// One block and the chain behind it (cSat::blockInit): Vec min, Vec size,
// four halfwords, u32 relative link to the next block, then either a child
// block (flag bit0) or the polygon indices of the three groups.
u32 convertSatBlocks(Blob& b, u32 o, int depth)
{
    u32 count = 0;
    while (b.ok && depth < 64) {
        if (!fits(b, o, offsetof(cSatBlock, idx), "cSatBlock")) {
            break;
        }
        u32 nFloor = be16(b, o + offsetof(cSatBlock, m_nFloor));
        u32 nSlope = be16(b, o + offsetof(cSatBlock, m_nSlope));
        u32 nWall = be16(b, o + offsetof(cSatBlock, m_nWall));
        u32 flag = be16(b, o + offsetof(cSatBlock, m_Flag));
        u32 next = be32(b, o + offsetof(cSatBlock, m_pList));
        if (next & 0x80000000u) {
            break;  // relocated already
        }
        swap32(b, o, 6);
        swap16(b, o + offsetof(cSatBlock, m_nFloor), 4);
        RE4_SWAP(b, o, cSatBlock, m_pList);
        count++;
        if (flag & 1) {
            count += convertSatBlocks(b, o + offsetof(cSatBlock, idx), depth + 1);
        } else {
            swap16(b, o + offsetof(cSatBlock, idx), nFloor + nSlope + nWall);
        }
        if (next == 0) {
            break;
        }
        o += next;
    }
    return count;
}

void convertSatFile(Blob& b, u32 o)
{
    typedef cSatFile F;
    if (!fits(b, o, sizeof(F), "cSatFile")) {
        return;
    }
    u32 nVtx = be16(b, o + offsetof(F, m_nVertex));
    u32 nNrm = be16(b, o + offsetof(F, m_nNormal));
    u32 nEdge = be16(b, o + offsetof(F, m_nEdge));
    u32 nPoly = be16(b, o + offsetof(F, m_nPolygon));
    u32 nBlock = be16(b, o + offsetof(F, m_nBlock));
    swap16(b, o + offsetof(F, m_nVertex), 9);
    u32 v = o + sizeof(F);
    u32 vecs = (nVtx + nNrm + nEdge) * 3;
    if (!fits(b, v, vecs * 4 + nPoly * sizeof(SAT_POLY), "SAT tables")) {
        return;
    }
    swap32(b, v, vecs);
    u32 p = v + vecs * 4;
    for (u32 i = 0; i < nPoly; i++) {
        swap16(b, p + i * sizeof(SAT_POLY), 8);
        RE4_SWAP(b, p + i * sizeof(SAT_POLY), SAT_POLY, m_Status);
    }
    u32 blocks = convertSatBlocks(b, p + nPoly * sizeof(SAT_POLY), 0);
    if (blocks != nBlock && b.ok) {
        re4_log("formats: %s: SAT at 0x%x has %u blocks in its chain, header says %u", b.name, (unsigned) o,
                (unsigned) blocks, (unsigned) nBlock);
    }
}

}  // namespace

// SAT / EAT entry: one cSatFile (version 0xFF) or a cSatHeader (bit7) with
// offsets of several (cSatHeader::getSat).
void convertSat(Blob& b, const Part&)
{
    if (b.size < 4) {
        return;
    }
    u8 version = b.data[0];
    if (version != 0xFF && version != 0x20 && !(version & 0x80)) {
        // not a collision file the game reads (sat.cpp accepts 0x20 and 0xFF;
        // the two SAT entries of the core archive, which no code looks up)
        re4_log("formats: %s: SAT version 0x%02x, left as it is", b.name, (unsigned) version);
        return;
    }
    if (version != 0xFF && (version & 0x80)) {
        u32 n = 0;
        u32 first = 0xFFFFFFFFu;
        while (4 + n * 4 + 4 <= b.size && 4 + n * 4 < first) {
            u32 o = be32(b, 4 + n * 4);
            if (o == 0 || o >= b.size) {
                break;
            }
            first = o < first ? o : first;
            n++;
        }
        u32 offs[256];
        n = n > 256 ? 256 : n;
        for (u32 i = 0; i < n; i++) {
            offs[i] = be32(b, 4 + i * 4);
        }
        swap32(b, 4, n);
        Seen seen;
        for (u32 i = 0; i < n && b.ok; i++) {
            if (seen.first(offs[i])) {
                convertSatFile(b, offs[i]);
            }
        }
    } else {
        convertSatFile(b, 0);
    }
}

}  // namespace re4fmt
