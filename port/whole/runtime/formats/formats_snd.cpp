// Formatumwandler: sound system files (src/game/snd.cpp SndInit, snd_sub3.cpp).
#include "formats.h"

#include "re4_runtime.h"
#include "snd.h"
#include "snd_drv.h"

using namespace re4fmt;

static_assert(sizeof(SND_RIT) == 0x10, "SND_RIT layout");
static_assert(sizeof(SND_SHD) == 0x7C, "SND_SHD layout");
static_assert(sizeof(SND_SIT) == 0x18, "SND_SIT layout");
static_assert(sizeof(WTFILEHEADER) == 0x18, "WTFILEHEADER layout");
static_assert(sizeof(WTREGION) == 0x18, "WTREGION layout");
static_assert(sizeof(WTART) == 0x50, "WTART layout");
static_assert(sizeof(WTSAMPLE) == 0x10, "WTSAMPLE layout");
static_assert(sizeof(WTADPCM) == 0x2E, "WTADPCM layout");

namespace re4fmt {

// Stream header (SHD): eight words, then the u16 ADPCM coefficients and states.
static void convertShd(Blob& b, u32 o)
{
    RE4_SWAP(b, o, SND_SHD, flag);
    RE4_SWAP(b, o, SND_SHD, samples);
    RE4_SWAP(b, o, SND_SHD, nibbles);
    RE4_SWAP(b, o, SND_SHD, rate);
    RE4_SWAP(b, o, SND_SHD, start_nbl);
    RE4_SWAP(b, o, SND_SHD, lptop_nbl);
    RE4_SWAP(b, o, SND_SHD, lpend_nbl);
    RE4_SWAP(b, o, SND_SHD, offset);
    RE4_SWAP_ARRAY(b, o, SND_SHD, coef);
    RE4_SWAP_ARRAY(b, o, SND_SHD, coefR);
    RE4_SWAP_ARRAY(b, o, SND_SHD, gain);
    RE4_SWAP_ARRAY(b, o, SND_SHD, ps);
    RE4_SWAP_ARRAY(b, o, SND_SHD, yn1);
    RE4_SWAP_ARRAY(b, o, SND_SHD, yn2);
    RE4_SWAP_ARRAY(b, o, SND_SHD, lps);
    RE4_SWAP_ARRAY(b, o, SND_SHD, lyn1);
    RE4_SWAP_ARRAY(b, o, SND_SHD, lyn2);
}

static void convertRit(Blob& b, u32 o)
{
    RE4_SWAP(b, o, SND_RIT, str_no);
    RE4_SWAP(b, o, SND_RIT, flag);
}

// Stream block (Snd_str_blk_init): rit_num, offset of the stream header
// offset table, offset of the RIT array (both from the block start).
static void convertStreamBlock(Blob& b, u32 o)
{
    u32 rits = be32(b, o);
    u32 sbh = o + be32(b, o + 4);
    u32 rit = o + be32(b, o + 8);
    swap32(b, o, 4);
    if (!b.ok || !fits(b, rit, rits * sizeof(SND_RIT), "RIT array")) {
        return;
    }
    for (u32 i = 0; i < rits; i++) {
        convertRit(b, rit + i * sizeof(SND_RIT));
    }
    // Header offset table (relative to its own start): it ends where the
    // first header begins.
    u32 end = b.size - sbh;
    for (u32 i = 0; i * 4 < end && b.ok; i++) {
        u32 v = be32(b, sbh + i * 4);
        if (v != 0 && v < end) {
            end = v;
        }
    }
    u32 count = end / 4;
    Seen seen;
    for (u32 i = 0; i < count && b.ok; i++) {
        u32 v = be32(b, sbh + i * 4);
        swap32(b, sbh + i * 4);
        if (v != 0 && seen.first(v) && fits(b, sbh + v, sizeof(SND_SHD), "stream header")) {
            convertShd(b, sbh + v);
        }
    }
}

// bgm/bio4str.hed: offsets of the stream blocks (SndMem.str_file[]), up to
// the first block.
void convertStrHed(Blob& b, const Part&)
{
    u32 first = b.size;
    for (u32 i = 0; i * 4 < first && b.ok; i++) {
        u32 v = be32(b, i * 4);
        if (v != 0 && v < first) {
            first = v;
        }
    }
    Seen seen;
    for (u32 i = 0; i * 4 < first && b.ok; i++) {
        u32 v = be32(b, i * 4);
        swap32(b, i * 4);
        if (v != 0 && seen.first(v)) {
            convertStreamBlock(b, v);
        }
    }
}

// bgm/doorse.hed (SndDoorTbl): u32 file offsets at file_ofs, u16 count at
// num_ofs and u16 values after it.
void convertDoorseHed(Blob& b, const Part&)
{
    u32 files = be32(b, offsetof(SndDoorTbl, file_ofs));
    u32 num = be32(b, offsetof(SndDoorTbl, num_ofs));
    if (!b.ok || files > num || num > b.size) {
        fits(b, num, 0xFFFFFFFFu, "door table layout");
        return;
    }
    swap32(b, 0, files / 4);
    swap32(b, files, (num - files) / 4);
    swap16(b, num, (b.size - num) / 2);
}

// bgm/bgmtbl.dat (SndBgmTbl): offsets, the per-room tables (SndBgmRoom, all
// 32-bit) up to list_ofs, then the u16 room id list.
void convertBgmTbl(Blob& b, const Part&)
{
    u32 list = be32(b, offsetof(SndBgmTbl, list_ofs));
    if (!b.ok || list > b.size || list % 4) {
        fits(b, list, 0xFFFFFFFFu, "room id list");
        return;
    }
    swap32(b, 0, list / 4);
    swap16(b, list, (b.size - list) / 2);
}

// Wavetable (snd_iss3.cpp, the SDK's SYN): WTFILEHEADER with six offsets from
// its start, then the tables. A table ends where the next one begins.
static void convertWavetable(Blob& b, u32 w, u32 end)
{
    if (!fits(b, w, sizeof(WTFILEHEADER), "WTFILEHEADER") || end < w + sizeof(WTFILEHEADER)) {
        fits(b, w, 0xFFFFFFFFu, "wavetable");
        return;
    }
    enum { kPerc, kMelodic, kRegions, kArts, kSamples, kAdpcm, kTables };
    u32 ofs[kTables];
    for (int i = 0; i < kTables; i++) {
        ofs[i] = be32(b, w + i * 4);
    }
    swap32(b, w, kTables);
    Seen seen;
    for (int i = 0; i < kTables && b.ok; i++) {
        u32 start = w + ofs[i];
        u32 stop = end;
        for (int k = 0; k < kTables; k++) {
            if (w + ofs[k] > start && w + ofs[k] < stop) {
                stop = w + ofs[k];
            }
        }
        if (ofs[i] == 0 || start >= stop || !seen.first(start) || !fits(b, start, stop - start, "wavetable table")) {
            continue;
        }
        u32 span = stop - start;
        switch (i) {
        case kPerc:
        case kMelodic:
            swap16(b, start, span / 2);  // WTINST: u16 region indices
            break;
        case kRegions:
            for (u32 o = start; o + sizeof(WTREGION) <= stop; o += sizeof(WTREGION)) {
                RE4_SWAP(b, o, WTREGION, fineTune);
                RE4_SWAP(b, o, WTREGION, attn);
                RE4_SWAP(b, o, WTREGION, loopStart);
                RE4_SWAP(b, o, WTREGION, loopLength);
                RE4_SWAP(b, o, WTREGION, articulationIndex);
                RE4_SWAP(b, o, WTREGION, sampleIndex);
            }
            break;
        case kArts:
            swap32(b, start, span / sizeof(WTART) * (sizeof(WTART) / 4));  // all s32
            break;
        case kSamples:
            for (u32 o = start; o + sizeof(WTSAMPLE) <= stop; o += sizeof(WTSAMPLE)) {
                RE4_SWAP(b, o, WTSAMPLE, format);
                RE4_SWAP(b, o, WTSAMPLE, sampleRate);
                RE4_SWAP(b, o, WTSAMPLE, offset);
                RE4_SWAP(b, o, WTSAMPLE, length);
                RE4_SWAP(b, o, WTSAMPLE, adpcmIndex);
            }
            break;
        case kAdpcm:
            swap16(b, start, span / sizeof(WTADPCM) * (sizeof(WTADPCM) / 2));  // all u16
            break;
        }
    }
}

static void convertSit(Blob& b, u32 o)
{
    RE4_SWAP(b, o, SND_SIT, note);
    RE4_SWAP(b, o, SND_SIT, pitch_l);
    RE4_SWAP(b, o, SND_SIT, pitch_h);
    RE4_SWAP(b, o, SND_SIT, flag);
}

// ISS block (Snd_iss_blk_init): u32 SIT count, offsets (from the block) of the
// wavetable, the SIT array and the sequence table (0 = none). Sequences are
// byte streams (snd_seq2.cpp); their table is u32 count + u32 offsets.
static void convertIssBlock(Blob& b, u32 o)
{
    u32 sits = be32(b, o);
    u32 wt = be32(b, o + 4);
    u32 sit = be32(b, o + 8);
    u32 seq = be32(b, o + 12);
    swap32(b, o, 4);
    if (!b.ok) {
        return;
    }
    if (fits(b, o + sit, sits * sizeof(SND_SIT), "SIT array")) {
        for (u32 i = 0; i < sits; i++) {
            convertSit(b, o + sit + i * sizeof(SND_SIT));
        }
    }
    if (seq != 0) {
        u32 n = be32(b, o + seq);
        swap32(b, o + seq, n + 1);
    }
    if (wt != 0) {
        u32 end = b.size;
        if (sit > wt && o + sit < end) {
            end = o + sit;
        }
        if (seq > wt && o + seq < end) {
            end = o + seq;
        }
        convertWavetable(b, o + wt, end);
    }
}

// DvdHeader type 1 (TRANS_SND_BLK), the MRAM half of a sound bank (the
// samples follow as type 2 into ARAM). SndBlkInit: BGM banks (sndType 3)
// start with the ISS block, the others with a 0x20-byte header whose first
// word is the offset of the ISS block.
void convertSoundBlock(Blob& b, const Part& part)
{
    u32 iss = 0;
    if (part.sndType != 3) {
        iss = be32(b, 0);
        if (iss < 4 || iss % 4 || iss > b.size) {
            fits(b, iss, 0xFFFFFFFFu, "ISS block offset");
            return;
        }
        swap32(b, 0, iss / 4);
    }
    convertIssBlock(b, iss);
}

}  // namespace re4fmt
