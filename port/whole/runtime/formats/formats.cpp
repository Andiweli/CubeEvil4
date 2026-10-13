// Formatumwandler: entry points and dispatch (see formats.h).
//
//   re4_formats_part_loaded  the game's DVD queue finished one part of a file
//                            (src/game/dvd.cpp cDvdQueue::readMain, patch 0015)
//   re4_formats_unpacked     yz2Decode_Decode unpacked a stream (runtime/yz2_decode.cpp)
//
// A part is chosen by its DvdHeader type first (sound blocks and samples have
// their own types), then by file name. Unknown files are logged once and left
// as they are.
#include "formats.h"

#include "dvd.h"
#include "re4_runtime.h"

#include <ctype.h>
#include <fnmatch.h>
#include <string.h>

namespace re4fmt {

Blob blob(void* data, u32 size, const char* name)
{
    Blob b;
    b.data = (u8*) data;
    b.size = size;
    b.name = name;
    b.ok = true;
    return b;
}

bool fits(Blob& b, u32 off, u32 len, const char* what)
{
    if (off <= b.size && len <= b.size - off) {
        return true;
    }
    if (b.ok) {
        re4_log("formats: %s: %s at 0x%x (+0x%x) outside the %u-byte block, conversion stopped", b.name, what,
                (unsigned) off, (unsigned) len, (unsigned) b.size);
    }
    b.ok = false;
    return false;
}

u32 be32(Blob& b, u32 off)
{
    if (!fits(b, off, 4, "32-bit read")) {
        return 0;
    }
    const u8* p = b.data + off;
    return ((u32) p[0] << 24) | ((u32) p[1] << 16) | ((u32) p[2] << 8) | p[3];
}

u16 be16(Blob& b, u32 off)
{
    if (!fits(b, off, 2, "16-bit read")) {
        return 0;
    }
    const u8* p = b.data + off;
    return (u16) ((p[0] << 8) | p[1]);
}

u32 rd32(Blob& b, u32 off)
{
    u32 v = 0;
    if (fits(b, off, 4, "32-bit read")) {
        memcpy(&v, b.data + off, 4);
    }
    return v;
}

u16 rd16(Blob& b, u32 off)
{
    u16 v = 0;
    if (fits(b, off, 2, "16-bit read")) {
        memcpy(&v, b.data + off, 2);
    }
    return v;
}

void swap16(Blob& b, u32 off, u32 count)
{
    u32 len = count > b.size / 2 ? 0xFFFFFFFFu : count * 2;
    if (count != 0 && fits(b, off, len, "16-bit field")) {
        re4_swap16_words(b.data + off, len);
    }
}

void swap32(Blob& b, u32 off, u32 count)
{
    u32 len = count > b.size / 4 ? 0xFFFFFFFFu : count * 4;
    if (count != 0 && fits(b, off, len, "32-bit field")) {
        re4_swap32_words(b.data + off, len);
    }
}

void swapField(Blob& b, u32 base, u32 off, u32 size, u32 elem)
{
    if (elem == 2) {
        swap16(b, base + off, size / 2);
    } else if (elem == 4) {
        swap32(b, base + off, size / 4);
    }
}

bool Seen::first(u32 o)
{
    for (u32 i = 0; i < n; i++) {
        if (off[i] == o) {
            return false;
        }
    }
    if (n < (u32) kMax) {
        off[n++] = o;
    } else if (!full) {
        full = true;
        re4_log("formats: more than %d shared structures in one block", (int) kMax);
    }
    return true;
}

void convertWords(Blob& b, const Part&)
{
    swap32(b, 0, b.size / 4);
}

// etc/sizetbl.dat (cDvd::SizeTableRead; the game keeps the pointer but never
// reads it): u32 count, then per file char name[8], u32 size, u32 pad.
void convertSizeTbl(Blob& b, const Part&)
{
    u32 count = be32(b, 0);
    if (!fits(b, 4, count * 16, "size table")) {
        return;
    }
    swap32(b, 0);
    for (u32 i = 0; i < count; i++) {
        swap32(b, 4 + i * 16 + 8, 2);
    }
}

}  // namespace re4fmt

using namespace re4fmt;

namespace {

// Plain text: nothing to convert.
void text(Blob&, const Part&)
{
}

// Yz2-packed: converted after unpacking (re4_formats_unpacked).
void packed(Blob&, const Part&)
{
}

struct Rule {
    const char* pattern;  // fnmatch pattern on the lower-case path
    Converter convert;
    const char* what;
};

const Rule kRules[] = {
    { "bgm/bio4str.hed", convertStrHed, "stream block headers" },
    { "bgm/bio4midi.hed", convertWords, "BGM file offsets" },
    { "bgm/doorse.hed", convertDoorseHed, "door SE file table" },
    { "bgm/bgmtbl.dat", convertBgmTbl, "room BGM table" },
    { "etc/sizetbl.dat", convertSizeTbl, "file size table" },
    { "font/*.fnt", convertFont, "message font" },
    { "*.tpl", convertTpl, "texture palette" },
    { "st*/r*.das", packed, "room archive (Yz2)" },
    { "*.das", convertArchive, "archive" },
    { "*.drs", convertArchive, "archive" },
    { "ss/*.dat", convertArchive, "archive (screens)" },
    { "debug/*.txt", text, "text" },
    { "rel/*.rel", text, "REL module (OSLink converts its header)" },
    { "debug/roominfo.dat", convertRoomInfo, "room jump table" },
};

// Logs a file once (hash table of names already reported).
bool firstReport(const char* name)
{
    static u32 seen[1024];
    u32 h = 2166136261u;
    for (const char* p = name; *p; p++) {
        h = (h ^ (u8) *p) * 16777619u;
    }
    h |= 1;
    for (u32 i = 0; i < 1024; i++) {
        u32 slot = (h + i) & 1023;
        if (seen[slot] == h) {
            return false;
        }
        if (seen[slot] == 0) {
            seen[slot] = h;
            return true;
        }
    }
    return false;
}

void normalise(const char* in, char* out, size_t size)
{
    size_t n = 0;
    for (const char* p = in; *p && n + 1 < size; p++) {
        char c = *p == '\\' ? '/' : (char) tolower((unsigned char) *p);
        out[n++] = c;
    }
    out[n] = 0;
    // SN host reads prefix the PC folder (cDvdQueue::Initialize).
    const char* host = "d:/bio4/data/";
    size_t hl = strlen(host);
    if (strncmp(out, host, hl) == 0) {
        memmove(out, out + hl, n - hl + 1);
    }
    while (out[0] == '/') {
        memmove(out, out + 1, strlen(out));
    }
}

char g_packedName[64];

}  // namespace

extern "C" void re4_formats_part_loaded(const char* rawName, int nest, int index, const void* header, unsigned addr)
{
    const DvdHeader* h = (const DvdHeader*) header;
    char name[96];
    normalise(rawName, name, sizeof(name));
    Part part = { name, nest, index, h->type, h->sndType, h->ofs };
    if (h->size == 0) {
        return;
    }
    bool aram = h->type == TRANS_SND_PCM || h->type == TRANS_ARAM;
    u8* data = aram ? re4_aram() + addr : (u8*) addr;
    char label[128];
    snprintf(label, sizeof(label), "%s part %d.%d", name, nest, index);
    Blob b = blob(data, h->size, label);

    if (h->type == TRANS_SND_PCM) {
        return;  // DSP-ADPCM samples: read by the audio layer (M8)
    }
    if (h->type == TRANS_SND_BLK) {
        convertSoundBlock(b, part);
        return;
    }
    for (const Rule& r : kRules) {
        if (fnmatch(r.pattern, name, 0) == 0) {
            if (r.convert == packed) {
                snprintf(g_packedName, sizeof(g_packedName), "%s", name);
            }
            r.convert(b, part);
            if (firstReport(name)) {
                re4_log("formats: %s: %s%s", label, r.what, b.ok ? "" : " (incomplete)");
            }
            return;
        }
    }
    if (firstReport(name)) {
        re4_log("formats: %s (type %u, %u bytes at %s 0x%08x): no converter, left big-endian", label,
                (unsigned) h->type, (unsigned) h->size, aram ? "ARAM" : "MRAM", (unsigned) addr);
    }
}

// Room archives are the only Yz2 user (read.cpp decodeData); they unpack to
// an archive with a table of contents.
extern "C" void re4_formats_unpacked(void* dst, unsigned size)
{
    static char label[96];
    snprintf(label, sizeof(label), "%s (unpacked)", g_packedName[0] ? g_packedName : "yz2 stream");
    Blob b = blob(dst, size, label);
    Part part = { label, 0, 0, TRANS_MRAM, 0, 0 };
    convertArchive(b, part);
    re4_log("formats: %s: archive, %u bytes%s", label, size, b.ok ? "" : " (incomplete)");
}
