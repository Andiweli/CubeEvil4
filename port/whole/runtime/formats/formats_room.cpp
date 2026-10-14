// Formatumwandler: the room tables of the room archive (game/game.cpp
// gameRoomInit and friends) and the enemy lists etc/*.esl.
//
//   CNS  pool sizes (cons.cpp ConsRoom)          ETS  etc model list (EtcModel.cpp)
//   EMI  enemy route points (embarrel.h)         SAR  light areas (light_area.cpp)
//   EAR  effect areas (espgen.h SstArea)         FSE  floor SE areas (flr_at.h)
//   ESE  SE areas (snd.h SeAtHead)               DSE  door SE table (snd.cpp)
//   STB  room sound header (snd.h SND_INFO)      BLK  streaming blocks (block.h)
//   RTP  route table (route_ck.h)                OSD  (stored, never read)
//   AEV/ITA  scenario / item areas (sce_at.h)    *.esl  EM_LIST[255] (global.h)
//
// Structures declared in a .cpp of the game are described here by offsets,
// each with its source; the ones from headers are swapped field by field.
#include "formats.h"

#include "area.h"
#include "block.h"
#include "embarrel.h"
#include "espgen.h"
#include "etc_model.h"
#include "flr_at.h"
#include "global.h"
#include "re4_runtime.h"
#include "route_ck.h"
#include "sce_at.h"
#include "snd.h"

#include <string.h>

using namespace re4fmt;

static_assert(sizeof(AREA_HIT_DATA) == 0x30, "AREA_HIT_DATA layout");
static_assert(sizeof(ETS_DATA) == 0x28, "ETS_DATA layout");
static_assert(sizeof(EmiEntry) == 0x40, "EmiEntry layout");
static_assert(sizeof(SstAreaEnt) == 0x98, "SstAreaEnt layout");
static_assert(sizeof(FLR_AT_DATA) == 0x84, "FLR_AT_DATA layout");
static_assert(sizeof(FLR_AT_HEADER) == 0x10, "FLR_AT_HEADER layout");
static_assert(sizeof(SeAtHead) == 0x10, "SeAtHead layout");
static_assert(sizeof(SE_AT_DATA) == 0x2C, "SE_AT_DATA layout");
static_assert(sizeof(SND_INFO) == 0x240, "SND_INFO layout");
static_assert(sizeof(REVERB_INFO) == 0x20, "REVERB_INFO layout");
static_assert(sizeof(BlockHeader) == 0x18, "BlockHeader layout");
static_assert(sizeof(BlockArea) == 0x38, "BlockArea layout");
static_assert(sizeof(RTP) == 0x18, "RTP layout");
static_assert(sizeof(RTP_POINT) == 0x10, "RTP_POINT layout");
static_assert(sizeof(RTP_LINE) == 4, "RTP_LINE layout");
static_assert(sizeof(SCE_AT_DATA) == 0x9C, "SCE_AT_DATA layout");
static_assert(sizeof(SCE_AT_ITEM) == 0x9C, "SCE_AT_ITEM layout");
static_assert(sizeof(EM_LIST) == 0x20, "EM_LIST layout");
// SCE_AT_DATA derives from SCE_AT_UNIT (not standard layout): the payload
// union follows the 0x5C-byte unit.
static const u32 kSceUnitSize = 0x5C;
static_assert(sizeof(SCE_AT_UNIT) == kSceUnitSize, "SCE_AT_UNIT layout");

namespace re4fmt {

// AREA_HIT_DATA (area.h): be_flag, type, pad halfword, eleven floats (every
// union member is made of floats).
void convertArea(Blob& b, u32 o)
{
    if (fits(b, o, sizeof(AREA_HIT_DATA), "AREA_HIT_DATA")) {
        RE4_SWAP(b, o, AREA_HIT_DATA, pad);
        swap32(b, o + offsetof(AREA_HIT_DATA, xz4), 11);
    }
}

namespace {

// u32 count followed by `count` records of `size` bytes from `first`.
u32 recordCount(Blob& b, u32 countOfs, bool half, u32 first, u32 size, const char* what)
{
    u32 n = half ? be16(b, countOfs) : be32(b, countOfs);
    if (!fits(b, first, n * size, what)) {
        return 0;
    }
    return n;
}

}  // namespace

// CNS (cons.cpp ConsRoom): u32 num, u32 bits[(num >> 5) + 1], u32 values[num].
void convertCons(Blob& b, const Part&)
{
    u32 n = be32(b, 0);
    u32 words = 1 + (n >> 5) + 1 + n;
    if (fits(b, 0, words * 4, "ConsRoom")) {
        swap32(b, 0, words);
    }
}

// ETS (EtcModel.cpp EtcList): u16 num at 0, ETS_DATA records from 0x10.
void convertEtcList(Blob& b, const Part&)
{
    u32 n = recordCount(b, 0, true, 0x10, sizeof(ETS_DATA), "ETS_DATA");
    swap16(b, 0);
    for (u32 i = 0; i < n; i++) {
        u32 o = 0x10 + i * sizeof(ETS_DATA);
        RE4_SWAP(b, o, ETS_DATA, id);
        RE4_SWAP(b, o, ETS_DATA, no);
        swap32(b, o + offsetof(ETS_DATA, ang), 3);
        swap32(b, o + offsetof(ETS_DATA, pos), 3);
    }
}

// EMI (embarrel.h EmiData): int n, pad, EmiEntry records from 8.
void convertEmi(Blob& b, const Part&)
{
    u32 n = recordCount(b, 0, false, offsetof(EmiData, entry), sizeof(EmiEntry), "EmiEntry");
    swap32(b, 0);
    for (u32 i = 0; i < n; i++) {
        u32 o = offsetof(EmiData, entry) + i * sizeof(EmiEntry);
        swap32(b, o + offsetof(EmiEntry, pos), 3);
        RE4_SWAP(b, o, EmiEntry, rotY);
    }
}

// SAR (light_area.cpp LightAreaHed): u32 num, pad to 0x10, 0xD8-byte
// LightAreaData: four bytes, AREA_HIT_DATA at 4, u32 flag at 0x34, bytes.
void convertLightArea(Blob& b, const Part&)
{
    const u32 kSize = 0xD8;
    u32 n = recordCount(b, 0, false, 0x10, kSize, "LightAreaData");
    swap32(b, 0);
    for (u32 i = 0; i < n; i++) {
        u32 o = 0x10 + i * kSize;
        convertArea(b, o + 4);
        swap32(b, o + 0x34);
    }
}

// EAR (espgen.h SstArea): u32 num, u32 version, pad, SstAreaEnt records.
void convertEffArea(Blob& b, const Part&)
{
    u32 n = recordCount(b, 0, false, offsetof(SstArea, ent), sizeof(SstAreaEnt), "SstAreaEnt");
    swap32(b, 0, 2);
    for (u32 i = 0; i < n; i++) {
        u32 o = offsetof(SstArea, ent) + i * sizeof(SstAreaEnt);
        convertArea(b, o + offsetof(SstAreaEnt, area));
        RE4_SWAP(b, o, SstAreaEnt, flag);
    }
}

// FSE (flr_at.h): "FSE" header, FLR_AT_DATA records from 0x10, payload by type.
void convertFloorSe(Blob& b, const Part&)
{
    u32 n = recordCount(b, offsetof(FLR_AT_HEADER, num), true, sizeof(FLR_AT_HEADER), sizeof(FLR_AT_DATA),
                        "FLR_AT_DATA");
    RE4_SWAP(b, 0, FLR_AT_HEADER, version);
    RE4_SWAP(b, 0, FLR_AT_HEADER, num);
    RE4_SWAP(b, 0, FLR_AT_HEADER, padd2);
    RE4_SWAP(b, 0, FLR_AT_HEADER, padd3);
    for (u32 i = 0; i < n; i++) {
        u32 o = sizeof(FLR_AT_HEADER) + i * sizeof(FLR_AT_DATA);
        convertArea(b, o + offsetof(FLR_AT_DATA, area));
        if (b.data[o + offsetof(FLR_AT_DATA, type)] == 2) {  // BGM volume control
            u32 p = o + offsetof(FLR_AT_DATA, bgmctrl);
            RE4_SWAP_ARRAY(b, p, FLR_AT_BGM_VOL, time);
            RE4_SWAP(b, p, FLR_AT_BGM_VOL, str_blk);
            RE4_SWAP(b, p, FLR_AT_BGM_VOL, str_no);
            RE4_SWAP(b, p, FLR_AT_BGM_VOL, str_fade_time);
        }
    }
}

// ESE (snd.h SeAtHead): "ESE" header, SE_AT_DATA records from 0x10.
void convertSeArea(Blob& b, const Part&)
{
    u32 n = recordCount(b, offsetof(SeAtHead, num), true, sizeof(SeAtHead), sizeof(SE_AT_DATA), "SE_AT_DATA");
    RE4_SWAP(b, 0, SeAtHead, version);
    RE4_SWAP(b, 0, SeAtHead, num);
    for (u32 i = 0; i < n; i++) {
        u32 o = sizeof(SeAtHead) + i * sizeof(SE_AT_DATA);
        RE4_SWAP(b, o, SE_AT_DATA, flag);
        swap32(b, o + offsetof(SE_AT_DATA, pos), 3);
        swap16(b, o + offsetof(SE_AT_DATA, x10), 10);  // x10 .. rnd_interval
    }
}

// DSE (snd.cpp SndDoorSe): u32 num, {u16 room, u16 door[5]} records.
void convertDoorSe(Blob& b, const Part&)
{
    u32 n = recordCount(b, 0, false, 4, 12, "SndDoorRoom");
    swap32(b, 0);
    swap16(b, 4, n * 6);
}

// STB (snd.h SND_INFO): two REVERB_INFO, four tables of 32 offsets; the
// offsets lead to SND_COMBINE_TBL (bytes) and SND_TBL_INFO (u32 num, f32
// scale, {f32 dist, u16 flag, u16 val} records; snd.cpp scales dist in place).
void convertSoundInfo(Blob& b, const Part&)
{
    if (!fits(b, 0, sizeof(SND_INFO), "SND_INFO")) {
        return;
    }
    for (u32 r = 0; r < 2; r++) {
        u32 o = r * sizeof(REVERB_INFO);
        swap16(b, o, 4);
        swap32(b, o + offsetof(REVERB_INFO, Delay), 6);
    }
    u32 tables[3][32];
    for (u32 i = 0; i < 32; i++) {
        tables[0][i] = be32(b, offsetof(SND_INFO, offset_vol) + i * 4);
        tables[1][i] = be32(b, offsetof(SND_INFO, offset_pitch) + i * 4);
        tables[2][i] = be32(b, offsetof(SND_INFO, offset_filter) + i * 4);
    }
    swap32(b, offsetof(SND_INFO, offset_tbl), 4 * 32);
    Seen seen;
    for (u32 t = 0; t < 3; t++) {
        for (u32 i = 0; i < 32 && b.ok; i++) {
            u32 o = tables[t][i];
            if (o == 0 || !seen.first(o) || !fits(b, o, 8, "SND_TBL_INFO")) {
                continue;
            }
            u32 n = be32(b, o);
            if (!fits(b, o + 8, n * 8, "SND_TBL_DATA")) {
                return;
            }
            swap32(b, o, 2);
            for (u32 k = 0; k < n; k++) {
                swap32(b, o + 8 + k * 8);
                swap16(b, o + 8 + k * 8 + 4, 2);
            }
        }
    }
}

// BLK (block.h): header, BlockLink (bytes), BlockArea, BlockConnect (bytes).
void convertBlock(Blob& b, const Part&)
{
    typedef BlockHeader H;
    if (!fits(b, 0, sizeof(H), "BlockHeader")) {
        return;
    }
    u32 nArea = be16(b, offsetof(H, nArea));
    u32 area = be32(b, offsetof(H, ofsArea));
    RE4_SWAP(b, 0, H, version);
    RE4_SWAP(b, 0, H, nArea);
    RE4_SWAP(b, 0, H, nConnect);
    swap32(b, offsetof(H, ofsLink), 3);
    if (!fits(b, area, nArea * sizeof(BlockArea), "BlockArea")) {
        return;
    }
    for (u32 i = 0; i < nArea; i++) {
        u32 o = area + i * sizeof(BlockArea);
        RE4_SWAP(b, o, BlockArea, tag);
        convertArea(b, o + offsetof(BlockArea, area));
    }
}

// RTP (route_ck.h): header, points (Vec, two halfwords), lines (two
// halfwords), next-hop table (bytes). Offsets are from the header.
void convertRoute(Blob& b, const Part&)
{
    if (!fits(b, 0, sizeof(RTP), "RTP")) {
        return;
    }
    u32 nPoint = be16(b, offsetof(RTP, nPoint));
    u32 nLine = be16(b, offsetof(RTP, nLine));
    u32 point = be32(b, offsetof(RTP, pPoint));
    u32 line = be32(b, offsetof(RTP, pLine));
    RE4_SWAP(b, 0, RTP, ID);
    swap16(b, offsetof(RTP, mapped), 4);
    swap32(b, offsetof(RTP, pPoint), 3);
    if (fits(b, point, nPoint * sizeof(RTP_POINT), "RTP_POINT")) {
        for (u32 i = 0; i < nPoint; i++) {
            u32 o = point + i * sizeof(RTP_POINT);
            swap32(b, o, 3);
            swap16(b, o + offsetof(RTP_POINT, offLine), 2);
        }
    }
    if (fits(b, line, nLine * sizeof(RTP_LINE), "RTP_LINE")) {
        swap16(b, line, nLine * 2);
    }
}

// OSD: read.cpp stores the pointer in pG->pOsd; no code reads it.
void convertUnread(Blob&, const Part&)
{
}

// AEV / ITA (sce_at.h, sce_at.cpp SceAtFileHead): magic, u16 version, u16
// num, pad; SCE_AT_DATA records (0x9C) from 0x10.
namespace {

void convertSceUnit(Blob& b, u32 o)
{
    typedef SCE_AT_UNIT U;
    RE4_SWAP(b, o, U, tag);
    convertArea(b, o + offsetof(U, area));
    RE4_SWAP(b, o, U, pParam);
    swap32(b, o + offsetof(U, pFunc));
    RE4_SWAP(b, o, U, pParent);
    RE4_SWAP(b, o, U, parts_no);
}

void reportPayload(u8 id, const char* where)
{
    static u32 reported;
    if (!(reported & (1u << (id & 31)))) {
        reported |= 1u << (id & 31);
        re4_log("formats: area type %u (first in %s): payload has no field list, left big-endian", id, where);
    }
}

void convertScePayload(Blob& b, u32 o, u8 id, const char* where)
{
    u32 p = o + kSceUnitSize;
    switch (id) {
    case SCEAT_ID_NORMAL:
    case SCEAT_ID_ITEM_PARENT:
        swap32(b, p, 16);  // model pointers (runtime)
        break;
    case SCEAT_ID_DOOR:
        swap32(b, p + offsetof(SCE_AT_DATA_DOOR, next_pos), 4);  // next_pos, next_ang_y
        swap32(b, p + offsetof(SCE_AT_DATA_DOOR, pExitFunc));
        RE4_SWAP(b, p, SCE_AT_DATA_DOOR, pExitParam);
        break;
    case SCEAT_ID_ITEM: {
        typedef SCE_AT_DATA_ITEM I;
        swap32(b, p + offsetof(I, item_pos), 3);
        RE4_SWAP(b, p, I, pModel);
        swap32(b, p + offsetof(I, eff_offset), 3);
        swap16(b, p + offsetof(I, item_id), 4);  // item_id, item_flg, item_num, auto_item_flg
        RE4_SWAP(b, p, I, save_no);
        swap32(b, p + offsetof(I, radius), 4);   // radius, ang_x, ang_y, open_ang
        break;
    }
    case SCEAT_ID_FLG:
        RE4_SWAP(b, p, SCE_AT_DATA_FLG, flg_no);
        break;
    case SCEAT_ID_MES:
        RE4_SWAP(b, p, SCE_AT_DATA_MES, mes_type);
        RE4_SWAP(b, p, SCE_AT_DATA_MES, mes_no);
        RE4_SWAP(b, p, SCE_AT_DATA_MES, se_no);
        break;
    case SCEAT_ID_SAVE:
        RE4_SWAP(b, p, SCE_AT_DATA_SAVE, term_no);
        break;
    case SCEAT_ID_SHD_DISP:
        RE4_SWAP(b, p, SCE_AT_DATA_SHD_DISP, shd_no);
        break;
    case SCEAT_ID_DAMAGE:
        RE4_SWAP(b, p, SCE_AT_DATA_DAMAGE, dmg_timer);
        RE4_SWAP(b, p, SCE_AT_DATA_DAMAGE, dmg_vol);
        RE4_SWAP(b, p, SCE_AT_DATA_DAMAGE, dmg_ang);
        break;
    case SCEAT_ID_SCR_AT: {
        typedef SCE_AT_DATA_SCR_AT S;
        RE4_SWAP(b, p, S, pSat);
        RE4_SWAP(b, p, S, pEat);
        swap32(b, p + offsetof(S, sat_attr), 4);  // sat_attr, eat_attr, ctrl_flag, sat_flag
        break;
    }
    case SCEAT_ID_CAM_CTRL:
        swap32(b, p + offsetof(SCE_AT_DATA_CAM_CTRL, pos), 4);
        RE4_SWAP(b, p, SCE_AT_DATA_CAM_CTRL, radius);
        RE4_SWAP(b, p, SCE_AT_DATA_CAM_CTRL, out_range);
        break;
    case SCEAT_ID_FIELD_INFO:
        swap32(b, p, 12);  // id, pParent, free[10]
        break;
    case SCEAT_ID_SKEY:
        swap32(b, p + offsetof(SCE_AT_DATA_SKEY, pParam));
        swap32(b, p + offsetof(SCE_AT_DATA_SKEY, pFunc));
        break;
    case SCEAT_ID_LADDER:
        swap32(b, p + offsetof(SCE_AT_DATA_LADDER, pos), 4);
        break;
    case SCEAT_ID_USE:
        RE4_SWAP(b, p, SCE_AT_DATA_USE, use_id);
        break;
    case SCEAT_ID_HIDE:
        swap32(b, p + offsetof(SCE_AT_DATA_HIDE, hide_area), 8 + 3 + 1);  // hide_area, pos, func
        break;
    case SCEAT_ID_POS_JUMP:
        swap32(b, p + offsetof(SCE_AT_DATA_POS_JUMP, dest_pos), 4);
        break;
    case SCEAT_ID_EXEC:  // the unit fields only (sceAtFunc_exec, SceAtDataSet_exec)
        break;
    default: {
        bool zero = true;
        for (u32 k = 0; k < 0x40; k++) {
            zero = zero && b.data[p + k] == 0;
        }
        if (!zero) {
            reportPayload(id, where);
        }
        break;
    }
    }
}

void convertSceFile(Blob& b, const char* magic, u16 version)
{
    const u32 kHead = 0x10;
    if (!fits(b, 0, kHead, "SceAtFileHead")) {
        return;
    }
    if (memcmp(b.data, magic, 4) != 0 || be16(b, 4) != version) {
        fits(b, 0, 0xFFFFFFFFu, "SceAt magic / version");
        return;
    }
    u32 n = recordCount(b, 6, true, kHead, sizeof(SCE_AT_DATA), "SCE_AT_DATA");
    swap16(b, 4, 2);
    for (u32 i = 0; i < n; i++) {
        u32 o = kHead + i * sizeof(SCE_AT_DATA);
        convertSceUnit(b, o);
        convertScePayload(b, o, b.data[o + offsetof(SCE_AT_UNIT, id)], b.name);
    }
}

}  // namespace

void convertSceAreas(Blob& b, const Part&)
{
    convertSceFile(b, "AEV", 0x104);
}

void convertSceItems(Blob& b, const Part&)
{
    convertSceFile(b, "ITA", 0x105);
}

// etc/*.esl (stage.cpp readEmList): EM_LIST records (global.h).
void convertEmList(Blob& b, const Part&)
{
    u32 n = b.size / sizeof(EM_LIST);
    for (u32 i = 0; i < n; i++) {
        u32 o = i * sizeof(EM_LIST);
        RE4_SWAP(b, o, EM_LIST, flag);
        RE4_SWAP(b, o, EM_LIST, hp);
        RE4_SWAP_ARRAY(b, o, EM_LIST, pos);
        RE4_SWAP_ARRAY(b, o, EM_LIST, rot);
        RE4_SWAP(b, o, EM_LIST, room);
        RE4_SWAP(b, o, EM_LIST, Guard_r);
    }
}

}  // namespace re4fmt
