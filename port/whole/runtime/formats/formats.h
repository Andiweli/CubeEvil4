#ifndef RE4_FORMATS_H
#define RE4_FORMATS_H

// Format converters: disc data is big-endian, the original code reads it in
// place as structures. Every loaded part is converted to host byte order once,
// right after the game's DVD queue finished it (patch 0015) or the Yz2
// decoder unpacked it. See port/docs/whole-program-status.md, "Formatumwandler".
//
// The converters are compiled with the game's flags and headers: structure
// sizes and field offsets come from the original code. Only what the CPU reads
// is converted; texel data, display lists and audio samples stay big-endian
// for the renderer (M6) and the audio layer (M8).

#include "types.h"

#include <stddef.h>

namespace re4fmt {

// A block of disc data being converted. Every access is checked against
// `size`; a structure that does not fit is logged and clears `ok`, the
// converter then stops instead of touching memory outside the block.
struct Blob {
    u8* data;
    u32 size;
    const char* name;
    bool ok;
};

Blob blob(void* data, u32 size, const char* name);
bool fits(Blob& b, u32 off, u32 len, const char* what);

// Big-endian reads (before the conversion); 0 and a log line when outside.
u32 be32(Blob& b, u32 off);
u16 be16(Blob& b, u32 off);
// Host-order reads (after the conversion).
u32 rd32(Blob& b, u32 off);
u16 rd16(Blob& b, u32 off);

void swap16(Blob& b, u32 off, u32 count = 1);
void swap32(Blob& b, u32 off, u32 count = 1);
// One structure field: `size` bytes made of `elem`-byte elements (1, 2, 4).
void swapField(Blob& b, u32 base, u32 off, u32 size, u32 elem);

// Where a part comes from; converters that need more than the bytes get it.
struct Part {
    const char* name;  // lower-case path on the disc
    int nest;          // nesting depth of the part table (0 = top)
    int index;         // part number at that depth
    u32 type;          // DvdHeader::type
    u32 sndType;       // DvdHeader::sndType
    u32 fileOfs;       // DvdHeader::ofs: offset of the part in the file
};

typedef void (*Converter)(Blob& b, const Part& part);

void convertWords(Blob& b, const Part& part);  // file made of 32-bit words only
void convertSizeTbl(Blob& b, const Part& part);

// formats_snd.cpp
void convertStrHed(Blob& b, const Part& part);
void convertDoorseHed(Blob& b, const Part& part);
void convertBgmTbl(Blob& b, const Part& part);
void convertSoundBlock(Blob& b, const Part& part);  // DvdHeader type 1 (TRANS_SND_BLK)

// formats_tex.cpp
void convertTplAt(Blob& b, u32 base);
void convertTpl(Blob& b, const Part& part);
void convertFont(Blob& b, const Part& part);

// formats_mes.cpp
u32 headerWords(Blob& b, u32 from);  // offset table ends at the first block
void convertMdt(Blob& b, const Part& part);

// formats_arc.cpp
bool looksLikeArchive(Blob& b);
void convertArchive(Blob& b, const Part& part);

// Remembers converted offsets of one blob, so a structure that two tables
// point at is converted once. Fixed size: the converters run before the game
// heaps exist and must not allocate.
struct Seen {
    enum { kMax = 4096 };
    u32 off[kMax];
    u32 n;
    bool full;
    Seen() : n(0), full(false) {}
    bool first(u32 o);  // true the first time `o` is seen
};

}  // namespace re4fmt

// Field swaps by the original structure definition.
#define RE4_SWAP(blob, base, T, f) \
    re4fmt::swapField(blob, base, offsetof(T, f), sizeof(((T*) 0)->f), sizeof(((T*) 0)->f))
#define RE4_SWAP_ARRAY(blob, base, T, f) \
    re4fmt::swapField(blob, base, offsetof(T, f), sizeof(((T*) 0)->f), sizeof(((T*) 0)->f[0]))

#endif
