#include "texture.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
namespace re4 {
namespace {
struct MipBoundary : std::runtime_error {
  MipBoundary() : std::runtime_error("TPL: Mipmap ends at resource bounds") {}
};
using Color = std::array<std::uint8_t, 4>;
struct View {
  const std::vector<std::uint8_t> &b;
  void require(std::size_t p, std::size_t n) const {
    if (p > b.size() || n > b.size() - p)
      throw std::runtime_error("Truncated TPL data");
  }
  unsigned u8(std::size_t p) const {
    require(p, 1);
    return b[p];
  }
  unsigned u16(std::size_t p) const { return (u8(p) << 8) | u8(p + 1); }
  std::uint32_t u32(std::size_t p) const { return (u16(p) << 16) | u16(p + 2); }
};
std::uint8_t expand(unsigned v, unsigned bits) {
  switch (bits) {
  case 3:
    return static_cast<std::uint8_t>((v << 5) | (v << 2) | (v >> 1));
  case 4:
    return static_cast<std::uint8_t>((v << 4) | v);
  case 5:
    return static_cast<std::uint8_t>((v << 3) | (v >> 2));
  case 6:
    return static_cast<std::uint8_t>((v << 2) | (v >> 4));
  default:
    throw std::runtime_error("Invalid channel precision");
  }
}
Color rgb565(unsigned v) {
  return {expand(v >> 11, 5), expand((v >> 5) & 63, 6), expand(v & 31, 5), 255};
}
Color rgb5a3(unsigned v) {
  if (v & 0x8000)
    return {expand((v >> 10) & 31, 5), expand((v >> 5) & 31, 5),
            expand(v & 31, 5), 255};
  return {expand((v >> 8) & 15, 4), expand((v >> 4) & 15, 4), expand(v & 15, 4),
          expand((v >> 12) & 7, 3)};
}
Color ia8(unsigned v) {
  auto i = static_cast<std::uint8_t>(v & 255);
  return {i, i, i, static_cast<std::uint8_t>(v >> 8)};
}
} // namespace
Texture DecodeTpl(const std::vector<std::uint8_t> &bytes, unsigned index,
                  unsigned mipLevel) {
  View v{bytes};
  if (v.u32(0) != 0x0020af30)
    throw std::runtime_error("No TPL archive");
  const auto count = v.u32(4), table = v.u32(8);
  if (count == 0 || count > 4096 || index >= count)
    throw std::runtime_error("Invalid TPL index");
  v.require(table, static_cast<std::size_t>(count) * 8);
  const auto hdr = v.u32(table + index * 8), pal = v.u32(table + index * 8 + 4);
  v.require(hdr, 36);
  Texture t;
  t.height = v.u16(hdr);
  t.width = v.u16(hdr + 2);
  t.format = v.u32(hdr + 4);
  t.wrapS = v.u32(hdr + 12);
  t.wrapT = v.u32(hdr + 16);
  if (t.wrapS > 2 || t.wrapT > 2)
    throw std::runtime_error("Invalid TPL wrap mode");
  if (t.width == 0 || t.height == 0 || t.width > 4096 || t.height > 4096)
    throw std::runtime_error("Invalid texture size");
  std::size_t data = v.u32(hdr + 8);
  const auto initialData = data;
  t.minLOD = v.u8(hdr + 33);
  t.maxLOD = v.u8(hdr + 34);
  unsigned natural = 0;
  for (unsigned n = std::max(t.width, t.height); n > 1; n >>= 1)
    ++natural;
  if (t.minLOD > t.maxLOD || t.maxLOD > natural)
    throw std::runtime_error("TPL: invalid mipmap bounds");
  unsigned last = t.minLOD == t.maxLOD ? 0 : t.maxLOD;
  if (mipLevel > last)
    throw std::runtime_error("TPL: Mipmap level outside chain bounds");
  if (last && ((t.width & (t.width - 1)) || (t.height & (t.height - 1))))
    throw std::runtime_error("TPL: Mipmap dimensions are not powers of two");
  unsigned tw = 4, th = 4, block = 32;
  switch (t.format) {
  case 0:
  case 8:
  case 14:
    tw = th = 8;
    break;
  case 1:
  case 2:
  case 9:
    tw = 8;
    th = 4;
    break;
  case 3:
  case 4:
  case 5:
  case 10:
    break;
  case 6:
    block = 64;
    break;
  default:
    throw std::runtime_error("Unknown GX texture format");
  }
  auto dataEnd = bytes.size();
  auto boundary = [&](std::size_t offset) {
    if (offset > initialData)
      dataEnd = std::min(dataEnd, offset);
  };
  boundary(table);
  for (unsigned i = 0; i < count; ++i) {
    auto h = v.u32(table + i * 8), p = v.u32(table + i * 8 + 4);
    v.require(h, 36);
    boundary(h);
    boundary(v.u32(h + 8));
    if (p) {
      v.require(p, 12);
      boundary(p);
      boundary(v.u32(p + 8));
    }
  }
  for (unsigned level = 0; level < mipLevel; ++level) {
    auto size = std::size_t((t.width + tw - 1) / tw) *
                ((t.height + th - 1) / th) * block;
    if (data > dataEnd || size > dataEnd - data)
      throw std::runtime_error("TPL: Truncated mipmap chain");
    data += size;
    t.width = std::max(t.width / 2, 1u);
    t.height = std::max(t.height / 2, 1u);
  }
  const auto tileCount = static_cast<std::size_t>((t.width + tw - 1) / tw) *
                         ((t.height + th - 1) / th);
  if (mipLevel && data <= dataEnd && tileCount * block > dataEnd - data &&
      (dataEnd - data) % 32 == 0)
    throw MipBoundary();
  if (data > dataEnd || tileCount * block > dataEnd - data)
    throw std::runtime_error(
        "TPL: Image data exceeds resource bounds: Image " +
        std::to_string(index) + " Level " + std::to_string(mipLevel) +
        " Offset " + std::to_string(data) + " end " + std::to_string(dataEnd) +
        " Bytes " + std::to_string(tileCount * block));
  v.require(data, tileCount * block);
  t.rgba.resize(static_cast<std::size_t>(t.width) * t.height * 4);
  unsigned paletteCount = 0, paletteFormat = 0;
  std::size_t paletteData = 0;
  if (t.format == 8 || t.format == 9 || t.format == 10) {
    if (!pal)
      throw std::runtime_error("TPL palette is missing");
    v.require(pal, 12);
    paletteCount = v.u16(pal);
    paletteFormat = v.u32(pal + 4);
    paletteData = v.u32(pal + 8);
    if (!paletteCount || paletteFormat > 2)
      throw std::runtime_error("Invalid TPL palette");
    v.require(paletteData, paletteCount * 2);
  }
  auto colorIndex = [&](unsigned i) {
    if (i >= paletteCount)
      throw std::runtime_error("TPL palette index outside palette bounds");
    unsigned c = v.u16(paletteData + i * 2);
    return paletteFormat == 0   ? ia8(c)
           : paletteFormat == 1 ? rgb565(c)
                                : rgb5a3(c);
  };
  auto put = [&](unsigned x, unsigned y, Color c) {
    if (x < t.width && y < t.height)
      std::copy(c.begin(), c.end(),
                t.rgba.begin() +
                    (static_cast<std::size_t>(y) * t.width + x) * 4);
  };
  std::size_t pos = data;
  for (unsigned by = 0; by < t.height; by += th)
    for (unsigned bx = 0; bx < t.width; bx += tw, pos += block) {
      if (t.format == 14) {
        for (unsigned s = 0; s < 4; s++) {
          std::size_t p = pos + s * 8;
          unsigned a = v.u16(p), b = v.u16(p + 2);
          std::array<Color, 4> c{rgb565(a), rgb565(b), Color{}, Color{}};
          for (unsigned k = 0; k < 3; k++) {
            c[2][k] = static_cast<std::uint8_t>(
                a > b ? (5 * c[0][k] + 3 * c[1][k]) / 8
                      : (c[0][k] + c[1][k]) / 2);
            c[3][k] = static_cast<std::uint8_t>(
                a > b ? (3 * c[0][k] + 5 * c[1][k]) / 8
                      : (c[0][k] + c[1][k]) / 2);
          }
          c[2][3] = 255;
          c[3][3] = a > b ? 255 : 0;
          for (unsigned y = 0; y < 4; y++)
            for (unsigned x = 0; x < 4; x++)
              put(bx + (s % 2) * 4 + x, by + (s / 2) * 4 + y,
                  c[(v.u8(p + 4 + y) >> (6 - 2 * x)) & 3]);
        }
        continue;
      }
      for (unsigned y = 0; y < th; y++)
        for (unsigned x = 0; x < tw; x++) {
          unsigned i = y * tw + x;
          Color c{};
          switch (t.format) {
          case 0: {
            auto n = expand((v.u8(pos + i / 2) >> (i % 2 ? 0 : 4)) & 15, 4);
            c = {n, n, n, n};
            break;
          }
          case 1: {
            auto n = static_cast<std::uint8_t>(v.u8(pos + i));
            c = {n, n, n, n};
            break;
          }
          case 2: {
            auto n = v.u8(pos + i);
            auto intensity = expand(n & 15, 4);
            c = {intensity, intensity, intensity, expand(n >> 4, 4)};
            break;
          }
          case 3:
            c = ia8(v.u16(pos + i * 2));
            break;
          case 4:
            c = rgb565(v.u16(pos + i * 2));
            break;
          case 5:
            c = rgb5a3(v.u16(pos + i * 2));
            break;
          case 6:
            c = {static_cast<std::uint8_t>(v.u8(pos + i * 2 + 1)),
                 static_cast<std::uint8_t>(v.u8(pos + 32 + i * 2)),
                 static_cast<std::uint8_t>(v.u8(pos + 32 + i * 2 + 1)),
                 static_cast<std::uint8_t>(v.u8(pos + i * 2))};
            break;
          case 8:
            c = colorIndex((v.u8(pos + i / 2) >> (i % 2 ? 0 : 4)) & 15);
            break;
          case 9:
            c = colorIndex(v.u8(pos + i));
            break;
          case 10:
            c = colorIndex(v.u16(pos + i * 2) & 0x3fff);
            break;
          }
          put(bx + x, by + y, c);
        }
    }
  return t;
}
Texture DecodeTplMipmapped(const std::vector<std::uint8_t> &bytes,
                           unsigned index) {
  auto t = DecodeTpl(bytes, index);
  if (t.minLOD != t.maxLOD)
    for (unsigned level = 1; level <= t.maxLOD; ++level) {
      try {
        auto image = DecodeTpl(bytes, index, level);
        t.mips.push_back({image.width, image.height, std::move(image.rgba)});
      } catch (const MipBoundary &) {
        // Some original images contain a shorter chain followed by GX-aligned
        // padding. Never read the following image to satisfy an overlarge
        // header LOD.
        if (t.mips.empty() || t.minLOD > t.mips.size())
          throw;
        break;
      }
    }
  return t;
}
Texture LoadTpl(const std::string &filename, unsigned index) {
  std::ifstream f(filename, std::ios::binary | std::ios::ate);
  if (!f)
    throw std::runtime_error("Original texture missing; import Disc 1 first");
  const auto size = f.tellg();
  if (size <= 0 || size > 64 * 1024 * 1024)
    throw std::runtime_error("Invalid TPL file size");
  std::vector<std::uint8_t> b(static_cast<std::size_t>(size));
  f.seekg(0);
  f.read(reinterpret_cast<char *>(b.data()), size);
  if (!f)
    throw std::runtime_error("Could not read TPL file");
  return DecodeTpl(b, index);
}
} // namespace re4
