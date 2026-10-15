// GameCube texture formats (GX, tiled, big-endian) to RGBA8 for the GX
// renderer of the core mode (gx_gl.cpp, port/docs/gx-plan.md step G3).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace re4gx {

// GX texture formats (BP TX_SETIMAGE0 bits 20-23).
enum GxTexFormat : unsigned {
  kTexI4 = 0,
  kTexI8 = 1,
  kTexIA4 = 2,
  kTexIA8 = 3,
  kTexRGB565 = 4,
  kTexRGB5A3 = 5,
  kTexRGBA8 = 6,
  kTexC4 = 8,
  kTexC8 = 9,
  kTexC14X2 = 10,
  kTexCMPR = 14
};

// TLUT formats (BP TX_SETTLUT bits 10-11).
enum GxTlutFormat : unsigned { kTlutIA8 = 0, kTlutRGB565 = 1, kTlutRGB5A3 = 2 };

bool GxIsPaletted(unsigned format);
// Palette entries a paletted format indexes (16, 256, 16384).
unsigned GxPaletteEntries(unsigned format);
// Bytes of one mipmap level (whole tiles).
std::size_t GxTextureLevelBytes(unsigned format, unsigned width, unsigned height);

// One level at `data` (`bytes` available) into width*height RGBA8 texels, top
// row first. Paletted formats look up `tlut` (big-endian entries as TMEM holds
// them, `tlutEntries` of them; missing entries read as 0). False when the data
// is short or the format unknown (the texels are then left black).
bool GxDecodeTexture(const std::uint8_t *data, std::size_t bytes, unsigned format, unsigned width,
                     unsigned height, const std::uint8_t *tlut, unsigned tlutEntries,
                     unsigned tlutFormat, std::vector<std::uint8_t> &rgba);

} // namespace re4gx
