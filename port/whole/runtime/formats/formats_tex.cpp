// Formatumwandler: TPL texture palettes (include/tpl.h) and message fonts
// (.fnt, src/game/mes.cpp MessageControl::setupFont).
#include "formats.h"

#include "re4_runtime.h"
#include "tpl.h"

using namespace re4fmt;

static_assert(sizeof(TEXPalette) == 0x0C, "TEXPalette layout");
static_assert(sizeof(TEXDescriptor) == 0x08, "TEXDescriptor layout");
static_assert(sizeof(TEXHeader) == 0x24, "TEXHeader layout");
static_assert(sizeof(CLUTHeader) == 0x0C, "CLUTHeader layout");

namespace re4fmt {

static const u32 kTplVersion = 0x0020AF30;

// Headers only: texels and CLUT entries are read by the GPU (renderer, M6)
// and stay big-endian.
static void convertTexHeader(Blob& b, u32 o)
{
    if (!fits(b, o, sizeof(TEXHeader), "TEXHeader")) {
        return;
    }
    RE4_SWAP(b, o, TEXHeader, height);
    RE4_SWAP(b, o, TEXHeader, width);
    RE4_SWAP(b, o, TEXHeader, format);
    RE4_SWAP(b, o, TEXHeader, data);
    RE4_SWAP(b, o, TEXHeader, wrapS);
    RE4_SWAP(b, o, TEXHeader, wrapT);
    RE4_SWAP(b, o, TEXHeader, minFilter);
    RE4_SWAP(b, o, TEXHeader, magFilter);
    RE4_SWAP(b, o, TEXHeader, LODBias);
}

static void convertClutHeader(Blob& b, u32 o)
{
    if (!fits(b, o, sizeof(CLUTHeader), "CLUTHeader")) {
        return;
    }
    RE4_SWAP(b, o, CLUTHeader, numEntries);
    RE4_SWAP(b, o, CLUTHeader, format);
    RE4_SWAP(b, o, CLUTHeader, data);
}

// A TPL at `base` inside `b`; header offsets are relative to `base`.
void convertTplAt(Blob& b, u32 base)
{
    if (be32(b, base + offsetof(TEXPalette, version)) != kTplVersion) {
        fits(b, base, 0xFFFFFFFFu, "TPL version");
        return;
    }
    u32 count = be32(b, base + offsetof(TEXPalette, numDescriptors));
    u32 desc = be32(b, base + offsetof(TEXPalette, descriptorArray));
    RE4_SWAP(b, base, TEXPalette, version);
    RE4_SWAP(b, base, TEXPalette, numDescriptors);
    RE4_SWAP(b, base, TEXPalette, descriptorArray);
    if (!fits(b, base + desc, count * sizeof(TEXDescriptor), "TEXDescriptor array")) {
        return;
    }
    Seen seen;
    for (u32 i = 0; i < count && b.ok; i++) {
        u32 d = base + desc + i * sizeof(TEXDescriptor);
        u32 tex = be32(b, d + offsetof(TEXDescriptor, textureHeader));
        u32 clut = be32(b, d + offsetof(TEXDescriptor, CLUTHeader));
        RE4_SWAP(b, d, TEXDescriptor, textureHeader);
        RE4_SWAP(b, d, TEXDescriptor, CLUTHeader);
        // Offset 0 means "none"; headers can be shared between descriptors.
        if (tex != 0 && seen.first(base + tex)) {
            convertTexHeader(b, base + tex);
        }
        if (clut != 0 && seen.first(base + clut)) {
            convertClutHeader(b, base + clut);
        }
    }
}

void convertTpl(Blob& b, const Part&)
{
    convertTplAt(b, 0);
}

// .fnt: u32 offset of the TPL, u32 offset of the u8 glyph width table, padding
// to 0x20 (all words).
void convertFont(Blob& b, const Part&)
{
    u32 tpl = be32(b, 0);
    swap32(b, 0, 8);
    if (b.ok) {
        convertTplAt(b, tpl);
    }
}

}  // namespace re4fmt
