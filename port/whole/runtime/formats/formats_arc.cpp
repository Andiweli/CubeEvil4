// Formatumwandler: archives with a table of contents (src/game/read.cpp
// GetDataExt, card.cpp CardArc): u32 count, u32 offset of an embedded REL
// module (0 = none), two pad words, u32 offset of every entry, then a 4-byte
// tag per entry ("TPL", "MDT", "BIN" ...). Each entry is converted by its tag.
#include "formats.h"

#include "re4_runtime.h"

#include <string.h>

using namespace re4fmt;

namespace re4fmt {

namespace {

struct TagRule {
    const char* tag;
    Converter convert;
};

const TagRule kTags[] = {
    { "TPL", convertTpl },
    { "MDT", convertMdt },
    { "UWF", convertIdLayout },
    { "EFF", convertEffect },
    { "TEX", convertTexSet },
    { "VIB", convertVibration },
    // models and motions
    { "BIN", convertBin },
    { "FCV", convertMotion },
    { "SEQ", convertSeq },
    // room data (game.cpp gameRoomInit, read.cpp ReadAreaData)
    { "CNS", convertCons },
    { "SMD", convertSmd },
    { "SMX", convertSmx },
    { "LIT", convertLit },
    { "SAT", convertSat },
    { "EAT", convertSat },
    { "AEV", convertSceAreas },
    { "ITA", convertSceItems },
    { "SHD", convertShd },
    { "EAR", convertEffArea },
    { "SAR", convertLightArea },
    { "ITM", convertItemModels },
    { "ETM", convertEtcArchive },
    { "ETS", convertEtcList },
    { "CAM", convertCamera },
    { "BLK", convertBlock },
    { "EVS", convertEvs },
    { "EVD", convertEvd },
    { "FSE", convertFloorSe },
    { "ESE", convertSeArea },
    { "DSE", convertDoorSe },
    { "STB", convertSoundInfo },
    { "RTP", convertRoute },
    { "OSD", convertUnread },
    { "EMI", convertEmi },
};

// Logs an unknown tag once.
void reportTag(const char* tag, const char* where)
{
    static char seen[256][4];
    static u32 n;
    for (u32 i = 0; i < n; i++) {
        if (memcmp(seen[i], tag, 4) == 0) {
            return;
        }
    }
    if (n < 256) {
        memcpy(seen[n++], tag, 4);
    }
    re4_log("formats: archive entry \"%.4s\" (first in %s): no converter, left big-endian", tag, where);
}

}  // namespace

bool looksLikeArchive(Blob& b)
{
    if (b.size < 0x20) {
        return false;
    }
    u32 count = be32(b, 0);
    u32 rel = be32(b, 4);
    if (count == 0 || count > 0x1000 || 0x10 + count * 8 > b.size || be32(b, 8) || be32(b, 12)) {
        return false;
    }
    if (rel != 0 && (rel > b.size || rel % 32)) {
        return false;
    }
    u32 first = be32(b, 0x10);
    return first >= 0x10 + count * 8 && first <= b.size;
}

void convertArchive(Blob& b, const Part& part)
{
    if (!looksLikeArchive(b)) {
        fits(b, 0, 0xFFFFFFFFu, "archive header");
        return;
    }
    u32 count = be32(b, 0);
    u32 rel = be32(b, 4);
    u32 end = rel ? rel : b.size;
    u32 offs[0x1000];
    for (u32 i = 0; i < count; i++) {
        offs[i] = be32(b, 0x10 + i * 4);
    }
    swap32(b, 0, 4 + count);
    noteArchive(b.data, b.size, b.name, part, offs, count);
    Seen seen;
    for (u32 i = 0; i < count && b.ok; i++) {
        u32 o = offs[i];
        const char* entryTag = (const char*) b.data + 0x10 + count * 4 + i * 4;
        // An entry whose offset a later entry repeats has no data of its own
        // (a motion without sequence, a model without texture ...): the data
        // there belongs to the last entry with that offset and is converted
        // by its tag only.
        bool empty = false;
        for (u32 k = i + 1; k < count && !empty; k++) {
            empty = offs[k] == o;
        }
        if (empty || !seen.first(o)) {
            continue;
        }
        u32 stop = end;
        for (u32 k = 0; k < count; k++) {
            if (offs[k] > o && offs[k] < stop) {
                stop = offs[k];
            }
        }
        if (o > stop || !fits(b, o, stop - o, "archive entry")) {
            return;
        }
        if (stop == o) {
            noteEntry(b.data + o, entryTag, kEntryConverted, b.data, (int) i);  // empty entry
            continue;
        }
        char tag[5];
        memcpy(tag, b.data + 0x10 + count * 4 + i * 4, 4);
        tag[4] = 0;
        char label[160];
        snprintf(label, sizeof(label), "%s #%u %s", b.name, (unsigned) i, tag);
        Blob e = blob(b.data + o, stop - o, label);
        Converter conv = 0;
        for (const TagRule& t : kTags) {
            if (strcmp(t.tag, tag) == 0) {
                conv = t.convert;
            }
        }
        if (conv) {
            conv(e, part);
            noteEntry(b.data + o, entryTag, e.ok ? kEntryConverted : kEntryIncomplete, b.data, (int) i);
            if (!e.ok) {
                // what bounds the entry: the next entry (its tag) or the archive end
                int next = -1;
                for (u32 k = 0; k < count; k++) {
                    if (offs[k] == stop) {
                        next = (int) k;
                    }
                }
                if (next >= 0) {
                    re4_log("formats: %s: conversion incomplete (entry 0x%x..0x%x, next entry #%d %.4s, archive "
                            "end 0x%x)",
                            label, (unsigned) o, (unsigned) stop, next,
                            (const char*) b.data + 0x10 + count * 4 + next * 4, (unsigned) end);
                } else {
                    re4_log("formats: %s: conversion incomplete (entry 0x%x..0x%x = archive end)", label,
                            (unsigned) o, (unsigned) stop);
                }
            }
        } else {
            reportTag(tag, b.name);
            noteEntry(b.data + o, entryTag, kEntryNoConverter, b.data, (int) i);
        }
    }
    // The embedded REL module: OSLink converts its header too, but the game
    // reads bssSize before linking (read.cpp).
    if (rel != 0 && b.ok) {
        re4_rel_header_to_host(b.data + rel);
    }
}

// Codec messages (ss_term.cpp opArc): an archive 0x400 bytes into the file;
// the game reads nothing before it.
void convertCodecArchive(Blob& b, const Part& part)
{
    if (!fits(b, 0x400, 0x20, "codec archive")) {
        return;
    }
    Blob arc = blob(b.data + 0x400, b.size - 0x400, b.name);
    Part inner = part;
    inner.fileOfs = part.fileOfs + 0x400;
    convertArchive(arc, inner);
    b.ok = arc.ok;
}

}  // namespace re4fmt
