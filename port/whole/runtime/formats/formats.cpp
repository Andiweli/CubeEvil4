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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

// A file inside a container (etc archive, event assets) by its extension.
void convertNamedFile(Blob& b, const Part& part, const char* name)
{
    struct Ext {
        const char* ext;
        Converter convert;
    };
    static const Ext kExt[] = {
        { ".bin", convertBin }, { ".tpl", convertTpl }, { ".fcv", convertMotion }, { ".seq", convertSeq },
        { ".eff", convertEffect }, { ".lit", convertLit }, { ".mdt", convertMdt }, { ".sat", convertSat },
        { ".eat", convertSat }, { ".esl", convertEmList }, { ".evd", convertEvd }, { ".cam", convertCamera },
    };
    const char* dot = strrchr(name, '.');
    for (const Ext& e : kExt) {
        if (dot && strcasecmp(dot, e.ext) == 0) {
            e.convert(b, part);
            return;
        }
    }
    static char seen[64][8];
    static u32 n;
    char ext[8] = "";
    snprintf(ext, sizeof(ext), "%s", dot ? dot : "(none)");
    for (u32 i = 0; i < n; i++) {
        if (strcmp(seen[i], ext) == 0) {
            return;
        }
    }
    if (n < 64) {
        memcpy(seen[n++], ext, sizeof(ext));
    }
    re4_log("formats: %s: no converter for \"%s\" files, left big-endian", b.name, ext);
}

// ---- coverage: what every archive entry became ----------------------------------

namespace {

struct EntryNote {
    const void* addr;
    char tag[4];
    int status;
};

struct ArchiveNote {
    const void* addr;
    char name[80];
};

const u32 kNotes = 8192;
EntryNote g_notes[kNotes];
ArchiveNote g_archives[64];
u32 g_nextArchive;

u32 slotOf(const void* addr)
{
    u32 h = (u32) (unsigned long) addr;
    h ^= h >> 13;
    h *= 0x5BD1E995u;
    return (h ^ (h >> 15)) % kNotes;
}

}  // namespace

void noteEntry(const void* addr, const char* tag, int status)
{
    u32 s = slotOf(addr);
    for (u32 i = 0; i < kNotes; i++) {
        EntryNote& e = g_notes[(s + i) % kNotes];
        if (e.addr == 0 || e.addr == addr) {
            e.addr = addr;
            memcpy(e.tag, tag, 4);
            e.status = status;
            return;
        }
    }
}

void noteArchive(const void* addr, const char* name)
{
    for (u32 i = 0; i < 64; i++) {
        if (g_archives[i].addr == addr) {
            snprintf(g_archives[i].name, sizeof(g_archives[i].name), "%s", name);
            return;
        }
    }
    ArchiveNote& a = g_archives[g_nextArchive++ % 64];
    a.addr = addr;
    snprintf(a.name, sizeof(a.name), "%s", name);
}

}  // namespace re4fmt

using namespace re4fmt;

// GetDataExt (read.cpp, patch 0025) reports every lookup: the tag, the entry
// it found and whether the converter turned that entry into host order. Each
// entry is reported once per load; RE4_COVERAGE_LOG=FILE also appends a
// tab-separated line (archive, tag, number, address, status).
extern "C" void re4_formats_get_data(const void* arc, const char* tag, int no, const void* entry)
{
    if (entry == 0) {
        return;  // not in this archive: nothing to convert
    }
    static const void* reported[4096];
    static u32 nReported;
    for (u32 i = 0; i < nReported; i++) {
        if (reported[i] == entry) {
            return;
        }
    }
    if (nReported < 4096) {
        reported[nReported++] = entry;
    }
    const char* status = "nein (unbekannter Eintrag)";
    u32 s = slotOf(entry);
    for (u32 i = 0; i < kNotes; i++) {
        const EntryNote& e = g_notes[(s + i) % kNotes];
        if (e.addr == 0) {
            break;
        }
        if (e.addr == entry) {
            status = e.status == kEntryConverted ? "ja"
                     : e.status == kEntryIncomplete ? "nein (unvollständig)"
                                                    : "nein (kein Umwandler)";
            break;
        }
    }
    const char* name = "?";
    for (u32 i = 0; i < 64; i++) {
        if (g_archives[i].addr == arc) {
            name = g_archives[i].name;
        }
    }
    re4_log("coverage: GetDataExt %.3s #%d in %s -> %p: umgewandelt %s", tag, no, name, entry, status);
    if (const char* path = getenv("RE4_COVERAGE_LOG")) {
        if (FILE* f = fopen(path, "a")) {
            fprintf(f, "%s\t%.3s\t%d\t%p\t%s\n", name, tag, no, entry, status);
            fclose(f);
        }
    }
}

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
    { "st*/r*_*.dat", convertArchive, "room block archive" },
    { "etc/*.esl", convertEmList, "enemy list" },
    { "*.evd", convertEvd, "event" },
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
