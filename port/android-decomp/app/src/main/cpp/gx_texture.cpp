#include "gx_texture.h"

namespace re4gx {
namespace {

struct Tile {
  unsigned w, h, bytes;
};

Tile TileOf(unsigned format) {
  switch (format) {
  case kTexI4:
  case kTexC4:
  case kTexCMPR:
    return {8, 8, 32};
  case kTexI8:
  case kTexIA4:
  case kTexC8:
    return {8, 4, 32};
  case kTexRGBA8:
    return {4, 4, 64};
  default: // IA8, RGB565, RGB5A3, C14X2
    return {4, 4, 32};
  }
}

inline std::uint8_t X3(unsigned v) { return static_cast<std::uint8_t>((v << 5) | (v << 2) | (v >> 1)); }
inline std::uint8_t X4(unsigned v) { return static_cast<std::uint8_t>((v << 4) | v); }
inline std::uint8_t X5(unsigned v) { return static_cast<std::uint8_t>((v << 3) | (v >> 2)); }
inline std::uint8_t X6(unsigned v) { return static_cast<std::uint8_t>((v << 2) | (v >> 4)); }

inline void Put(std::uint8_t *p, unsigned r, unsigned g, unsigned b, unsigned a) {
  p[0] = static_cast<std::uint8_t>(r);
  p[1] = static_cast<std::uint8_t>(g);
  p[2] = static_cast<std::uint8_t>(b);
  p[3] = static_cast<std::uint8_t>(a);
}

void Rgb565(std::uint8_t *p, unsigned v) { Put(p, X5(v >> 11), X6((v >> 5) & 63), X5(v & 31), 255); }

void Rgb5a3(std::uint8_t *p, unsigned v) {
  if (v & 0x8000)
    Put(p, X5((v >> 10) & 31), X5((v >> 5) & 31), X5(v & 31), 255);
  else
    Put(p, X4((v >> 8) & 15), X4((v >> 4) & 15), X4(v & 15), X3((v >> 12) & 7));
}

// IA8: the high byte is alpha, the low byte intensity.
void Ia8(std::uint8_t *p, unsigned v) { Put(p, v & 255, v & 255, v & 255, v >> 8); }

void Palette(std::uint8_t *p, unsigned index, const std::uint8_t *tlut, unsigned entries, unsigned format) {
  unsigned v = 0;
  if (tlut && index < entries)
    v = static_cast<unsigned>(tlut[index * 2] << 8 | tlut[index * 2 + 1]);
  switch (format) {
  case kTlutRGB565:
    Rgb565(p, v);
    break;
  case kTlutRGB5A3:
    Rgb5a3(p, v);
    break;
  default:
    Ia8(p, v);
    break;
  }
}

// 5/8 + 3/8 blend of the CMPR palette (as the hardware rounds it).
inline unsigned Blend(unsigned a, unsigned b) { return (a * 5 + b * 3) >> 3; }

void CmprBlock(const std::uint8_t *s, std::uint8_t *out, unsigned stride) {
  unsigned c0 = static_cast<unsigned>(s[0] << 8 | s[1]);
  unsigned c1 = static_cast<unsigned>(s[2] << 8 | s[3]);
  std::uint8_t colors[4][4];
  Rgb565(colors[0], c0);
  Rgb565(colors[1], c1);
  for (int k = 0; k < 3; k++) {
    if (c0 > c1) {
      colors[2][k] = static_cast<std::uint8_t>(Blend(colors[0][k], colors[1][k]));
      colors[3][k] = static_cast<std::uint8_t>(Blend(colors[1][k], colors[0][k]));
    } else {
      colors[2][k] = static_cast<std::uint8_t>((colors[0][k] + colors[1][k]) / 2);
      colors[3][k] = colors[2][k];
    }
  }
  colors[2][3] = 255;
  colors[3][3] = c0 > c1 ? 255 : 0;
  for (unsigned y = 0; y < 4; y++) {
    unsigned bits = s[4 + y];
    for (unsigned x = 0; x < 4; x++) {
      unsigned i = (bits >> (6 - 2 * x)) & 3;
      std::uint8_t *p = out + y * stride + x * 4;
      for (int k = 0; k < 4; k++)
        p[k] = colors[i][k];
    }
  }
}

} // namespace

bool GxIsPaletted(unsigned format) { return format == kTexC4 || format == kTexC8 || format == kTexC14X2; }

unsigned GxPaletteEntries(unsigned format) {
  return format == kTexC4 ? 16 : format == kTexC8 ? 256 : 16384;
}

std::size_t GxTextureLevelBytes(unsigned format, unsigned width, unsigned height) {
  Tile t = TileOf(format);
  return static_cast<std::size_t>((width + t.w - 1) / t.w) * ((height + t.h - 1) / t.h) * t.bytes;
}

bool GxDecodeTexture(const std::uint8_t *data, std::size_t bytes, unsigned format, unsigned width,
                     unsigned height, const std::uint8_t *tlut, unsigned tlutEntries,
                     unsigned tlutFormat, std::vector<std::uint8_t> &rgba) {
  rgba.assign(static_cast<std::size_t>(width) * height * 4, 0);
  const Tile t = TileOf(format);
  const unsigned tilesX = (width + t.w - 1) / t.w, tilesY = (height + t.h - 1) / t.h;
  const bool known = format <= kTexRGBA8 || format == kTexC4 || format == kTexC8 || format == kTexC14X2 ||
                     format == kTexCMPR;
  if (!known || !data || bytes < static_cast<std::size_t>(tilesX) * tilesY * t.bytes)
    return false;
  const unsigned stride = width * 4;
  std::vector<std::uint8_t> tile(static_cast<std::size_t>(t.w) * t.h * 4);
  const std::uint8_t *s = data;
  for (unsigned ty = 0; ty < tilesY; ty++) {
    for (unsigned tx = 0; tx < tilesX; tx++, s += t.bytes) {
      // the tile into a w*h RGBA block, then the visible part out
      std::uint8_t *o = tile.data();
      const unsigned n = t.w * t.h;
      switch (format) {
      case kTexI4:
        for (unsigned i = 0; i < n; i++) {
          unsigned v = (s[i / 2] >> ((i & 1) ? 0 : 4)) & 15;
          Put(o + i * 4, X4(v), X4(v), X4(v), X4(v));
        }
        break;
      case kTexI8:
        for (unsigned i = 0; i < n; i++)
          Put(o + i * 4, s[i], s[i], s[i], s[i]);
        break;
      case kTexIA4:
        for (unsigned i = 0; i < n; i++) {
          unsigned iv = X4(s[i] & 15), a = X4(s[i] >> 4);
          Put(o + i * 4, iv, iv, iv, a);
        }
        break;
      case kTexIA8:
        for (unsigned i = 0; i < n; i++)
          Ia8(o + i * 4, static_cast<unsigned>(s[i * 2] << 8 | s[i * 2 + 1]));
        break;
      case kTexRGB565:
        for (unsigned i = 0; i < n; i++)
          Rgb565(o + i * 4, static_cast<unsigned>(s[i * 2] << 8 | s[i * 2 + 1]));
        break;
      case kTexRGB5A3:
        for (unsigned i = 0; i < n; i++)
          Rgb5a3(o + i * 4, static_cast<unsigned>(s[i * 2] << 8 | s[i * 2 + 1]));
        break;
      case kTexRGBA8: // 32 bytes of A R pairs, then 32 bytes of G B pairs
        for (unsigned i = 0; i < n; i++)
          Put(o + i * 4, s[i * 2 + 1], s[32 + i * 2], s[32 + i * 2 + 1], s[i * 2]);
        break;
      case kTexC4:
        for (unsigned i = 0; i < n; i++)
          Palette(o + i * 4, (s[i / 2] >> ((i & 1) ? 0 : 4)) & 15, tlut, tlutEntries, tlutFormat);
        break;
      case kTexC8:
        for (unsigned i = 0; i < n; i++)
          Palette(o + i * 4, s[i], tlut, tlutEntries, tlutFormat);
        break;
      case kTexC14X2:
        for (unsigned i = 0; i < n; i++)
          Palette(o + i * 4, static_cast<unsigned>(s[i * 2] << 8 | s[i * 2 + 1]) & 0x3FFF, tlut, tlutEntries,
                  tlutFormat);
        break;
      case kTexCMPR: // four 4x4 DXT1 blocks: top left, top right, bottom left, bottom right
        for (unsigned b = 0; b < 4; b++)
          CmprBlock(s + b * 8, o + ((b >> 1) * 4 * t.w + (b & 1) * 4) * 4, t.w * 4);
        break;
      }
      for (unsigned y = 0; y < t.h; y++) {
        unsigned py = ty * t.h + y;
        if (py >= height)
          break;
        for (unsigned x = 0; x < t.w; x++) {
          unsigned px = tx * t.w + x;
          if (px >= width)
            break;
          std::uint8_t *d = rgba.data() + static_cast<std::size_t>(py) * stride + px * 4;
          const std::uint8_t *q = o + (y * t.w + x) * 4;
          d[0] = q[0];
          d[1] = q[1];
          d[2] = q[2];
          d[3] = q[3];
        }
      }
    }
  }
  return true;
}

} // namespace re4gx
