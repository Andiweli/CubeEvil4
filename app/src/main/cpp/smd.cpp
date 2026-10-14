#include "smd.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>
namespace re4 {
namespace {
class Reader {
public:
  const std::vector<std::uint8_t> &b;
  std::size_t begin = 0, end = std::numeric_limits<std::size_t>::max();
  void range(std::size_t p, std::size_t n) const {
    if (p < begin || p > std::min(end, b.size()) ||
        n > std::min(end, b.size()) - p)
      throw std::runtime_error("SMD: Range outside subfile bounds");
  }
  std::uint16_t u16(std::size_t p) const {
    range(p, 2);
    return (std::uint16_t(b[p]) << 8) | b[p + 1];
  }
  std::uint32_t u32(std::size_t p) const {
    return (std::uint32_t(u16(p)) << 16) | u16(p + 2);
  }
  float f32(std::size_t p) const {
    auto word = u32(p);
    float f;
    std::memcpy(&f, &word, 4);
    if (!std::isfinite(f))
      throw std::runtime_error("SMD: invalid transform");
    return f;
  }
  std::size_t relative(std::size_t base, std::uint32_t delta,
                       std::size_t size) const {
    // Reject relocated pointers; checked subtraction also works on a 32-bit
    // host.
    if (delta & 0x80000000u)
      throw std::runtime_error("SMD: Pointer already relocated");
    range(base, delta);
    auto result = base + delta;
    range(result, size);
    return result;
  }
};
} // namespace
Smd::Smd(const std::vector<std::uint8_t> &bytes, SmdResourceCounts required) {
  if (required.bins > 256 || required.palettes > 256 || required.motions > 256)
    throw std::runtime_error("SMD: Resource index limit");
  if (bytes.size() > 64u * 1024 * 1024)
    throw std::runtime_error("SMD: Size limit");
  Reader r{bytes};
  r.range(0, 16);
  version = bytes[0];
  flags = bytes[1];
  if (version != 0x40)
    throw std::runtime_error("SMD: unsupported version");
  const auto count = r.u16(2);
  workOffset = 16;
  logicalWorkCount = count;
  if (flags & 1) {
    auto n = r.u32(16);
    if (n > 4096)
      throw std::runtime_error("SMD: Group limit");
    r.range(20, std::size_t(n) * 4);
    workOffset = 20 + std::size_t(n) * 4;
    for (std::uint32_t i = 0; i < n; ++i) {
      auto v = r.u32(20 + i * 4);
      if (v > 65535 || logicalWorkCount > 65535 - v)
        throw std::runtime_error("SMD: Logical work limit");
      groups.push_back(v);
      logicalWorkCount += v;
    }
  }
  r.range(workOffset, std::size_t(count) * 0x48);
  unsigned bins = required.bins, tpls = required.palettes,
           motions = required.motions;
  for (unsigned i = 0; i < count; ++i) {
    auto p = workOffset + i * 0x48;
    SmdPlacement w;
    for (unsigned k = 0; k < 3; ++k) {
      w.position[k] = r.f32(p + 4 * k);
      w.rotation[k] = r.f32(p + 12 + 4 * k);
      w.scale[k] = r.f32(p + 24 + 4 * k);
    }
    w.bin = bytes[p + 0x24];
    w.tpl = bytes[p + 0x25];
    w.motion = bytes[p + 0x26];
    w.work = bytes[p + 0x27];
    w.flags = r.u32(p + 0x44);
    placements.push_back(w);
    if (w.work == 0xff)
      continue;
    if (!(w.flags & 0x10)) {
      // SmdSetParam substitutes index 0 for 0xff on active BIN/TPL links.
      bins = std::max(bins, unsigned(w.bin == 0xff ? 0 : w.bin) + 1);
      tpls = std::max(tpls, unsigned(w.tpl == 0xff ? 0 : w.tpl) + 1);
    }
    if (!(w.flags & 0x40) && w.motion != 0xff)
      motions = std::max(motions, unsigned(w.motion) + 1);
  }
  auto table = [&](unsigned header, unsigned n, std::vector<std::size_t> &out) {
    auto base = r.u32(header);
    if (!n)
      return;
    if (base < workOffset + std::size_t(count) * 0x48)
      throw std::runtime_error("SMD: Table overlaps placements");
    r.range(base, std::size_t(n) * 4);
    for (unsigned i = 0; i < n; ++i) {
      auto delta = r.u32(base + i * 4);
      if (delta < n * 4)
        throw std::runtime_error("SMD: Payload overlaps table");
      out.push_back(r.relative(base, delta, 1));
    }
  };
  table(4, bins, binOffsets);
  table(8, tpls, tplOffsets);
  table(12, motions, motionOffsets);
  std::size_t paletteBytes = 0;
  for (auto base : tplOffsets) {
    auto end = bytes.size();
    for (auto header : {4u, 8u, 12u})
      if (r.u32(header) > base)
        end = std::min(end, std::size_t(r.u32(header)));
    for (const auto *offsets : {&binOffsets, &tplOffsets, &motionOffsets})
      for (auto t : *offsets)
        if (t > base)
          end = std::min(end, t);
    r.range(base, end - base);
    if (end - base > 64u * 1024 * 1024 - paletteBytes)
      throw std::runtime_error("SMD: Palette budget");
    paletteBytes += end - base;
    palettes.emplace_back(bytes.begin() + base, bytes.begin() + end);
  }
  std::size_t remainingCorners = 2000000, remainingTriangles = 2000000,
              remainingCommands = 131072, remainingUV = 2000000,
              remainingColors = 2000000, remainingNormals = 2000000;
  for (auto base : binOffsets) {
    auto end = bytes.size();
    for (auto header : {4u, 8u, 12u}) {
      auto t = r.u32(header);
      if (t > base)
        end = std::min(end, std::size_t(t));
    }
    for (const auto *offsets : {&binOffsets, &tplOffsets, &motionOffsets})
      for (auto t : *offsets)
        if (t > base)
          end = std::min(end, t);
    Reader r{bytes, base, end};
    r.range(base, 0x40);
    SmdModel m;
    m.offset = base;
    m.parts = bytes[base + 0x19];
    m.weightPalette = bytes[base + 0x18];
    m.weightExt = r.u16(base + 0x2a);
    m.displayLists = r.u16(base + 0x1a);
    m.vertices = r.u16(base + 0x38);
    m.shift = bytes[base + 0x28];
    m.version = r.u32(base + 0x3c);
    if (m.version != 0x20010801 && m.version != 0x20030817 &&
        m.version != 0x20030818)
      throw std::runtime_error("SMD: unsupported BIN version");
    if (m.shift > 30)
      throw std::runtime_error("SMD: invalid vertex shift");
    // Attribute bounds use the next known BIN section. UV/color counts are
    // upper bounds including alignment padding, not invented exact counts.
    auto sectionCount = [&](unsigned field, std::size_t stride) {
      auto relStart = r.u32(base + field);
      if (relStart < 64)
        return std::size_t(0);
      auto start = r.relative(base, relStart, 0);
      auto stop = end;
      for (auto f : {0u, 12u, 16u, 20u, 28u, 44u, 48u, 52u}) {
        auto rel = r.u32(base + f);
        if (rel && !(rel & 0x80000000u) && rel <= end - base &&
            base + rel > start)
          stop = std::min(stop, base + rel);
      }
      if (m.version == 0x20030818) {
        r.range(base, 72);
        for (auto f : {64u, 68u}) {
          auto rel = r.u32(base + f);
          if (rel && !(rel & 0x80000000u) && rel <= end - base &&
              base + rel > start)
            stop = std::min(stop, base + rel);
        }
      }
      return (stop - start) / stride;
    };
    auto head = r.relative(base, r.u32(base), std::size_t(m.parts) * 16);
    for (unsigned i = 0; i < m.parts; ++i) {
      SmdJoint joint;
      joint.attachment = bytes[head + i * 16];
      joint.parent = bytes[head + i * 16 + 1];
      for (unsigned k = 0; k < 3; ++k)
        joint.center[k] = r.f32(head + i * 16 + 4 + k * 4);
      m.joints.push_back(joint);
    }
    m.positions.resize(m.vertices);
    if (m.vertices > sectionCount(48, 8))
      throw std::runtime_error("GX: Vertex array overlaps BIN section");
    const auto v =
        r.relative(base, r.u32(base + 0x30), std::size_t(m.vertices) * 8);
    const float scale = std::ldexp(1.0f, -int(m.shift));
    for (unsigned i = 0; i < m.vertices; ++i)
      for (unsigned k = 0; k < 3; ++k) {
        auto word = r.u16(v + i * 8 + k * 2);
        auto signedWord = word >= 0x8000 ? int(word) - 65536 : int(word);
        float value = signedWord * scale;
        m.positions[i][k] = value;
        if (!i)
          m.minimum[k] = m.maximum[k] = value;
        else {
          m.minimum[k] = std::min(m.minimum[k], value);
          m.maximum[k] = std::max(m.maximum[k], value);
        }
      }
    auto part = r.relative(base, r.u32(base + 0x1c), m.displayLists ? 32 : 0);
    if (m.displayLists && (part & 31))
      throw std::runtime_error("SMD: BIN display list is not aligned");
    const auto flags = r.u32(base + 32);
    m.sourceFlags=flags;
    const bool colored = (flags & 0x80000000u) != 0;
    const auto normals = r.u16(base + 58);
    const auto normalStride = (flags & 0x20000000u) ? 4u : 8u;
    (void)r.relative(base, r.u32(base + 52),
                     std::size_t(normals) * normalStride);
    if (normals > sectionCount(52, normalStride))
      throw std::runtime_error("GX: Normal array overlaps BIN section");

    if (normals > remainingNormals)
      throw std::runtime_error("SMD: Normal budget");
    remainingNormals -= normals;
    auto normalStart =
        r.relative(base, r.u32(base + 52), std::size_t(normals) * normalStride);
    m.normals.reserve(normals);
    for (unsigned i = 0; i < normals; ++i) {
      std::array<float, 3> normal{};
      for (unsigned k = 0; k < 3; ++k) {
        if (normalStride == 4) {
          auto raw = bytes[normalStart + i * 4 + k];
          normal[k] = (raw >= 128 ? int(raw) - 256 : int(raw)) / 64.f;
        } else {
          auto raw = r.u16(normalStart + i * 8 + k * 2);
          normal[k] = (raw >= 32768 ? int(raw) - 65536 : int(raw)) / 16384.f;
        }
      }
      m.normals.push_back(normal);
    }
    GxLimits limits;
    limits.positions = m.vertices;
    limits.normals = normals;
    limits.colored = colored;
    limits.colors = colored ? sectionCount(12, 4) : 0;
    limits.texcoords = sectionCount(16, 4);
    m.colored = colored;
    m.vertexAmbient = (flags & 0x40000000u) != 0;
    if (limits.colors > remainingColors)
      throw std::runtime_error("SMD: Color budget");
    remainingColors -= limits.colors;
    if (limits.colors) {
      auto colorStart = r.relative(base, r.u32(base + 12), limits.colors * 4);
      m.colors.reserve(limits.colors);
      for (std::size_t i = 0; i < limits.colors; ++i) {
        std::array<std::uint8_t, 4> color;
        std::copy_n(bytes.begin() + colorStart + i * 4, 4, color.begin());
        m.colors.push_back(color);
      }
    }
    if (limits.texcoords > remainingUV)
      throw std::runtime_error("SMD: UV-Budget");
    remainingUV -= limits.texcoords;
    if (limits.texcoords) {
      auto uv = r.relative(base, r.u32(base + 16), limits.texcoords * 4);
      for (std::size_t i = 0; i < limits.texcoords; ++i) {
        std::array<float, 2> pair{};
        for (unsigned k = 0; k < 2; ++k) {
          auto word = r.u16(uv + i * 4 + k * 2);
          pair[k] =
              colored ? (word >= 0x8000 ? int(word) - 65536 : int(word)) / 256.f
                      : word / 32768.f;
        }
        m.texcoords.push_back(pair);
      }
    }
    for (unsigned i = 0; i < m.displayLists; ++i) {
      r.range(part, 32);
      std::array<std::uint8_t, 32> header;
      std::copy_n(bytes.begin() + part, 32, header.begin());
      m.materialHeaders.push_back(header);
      auto size = r.u32(part + 24);
      r.range(part + 32, size);
      limits.corners = remainingCorners;
      limits.triangles = remainingTriangles;
      limits.commands = remainingCommands;
      auto primitives = DecodeGxStream(bytes, part + 32, size, limits);
      for (auto &primitive : primitives) {
        primitive.displayList = i;
        remainingCorners -= primitive.vertices.size();
        remainingTriangles -= primitive.triangles.size();
        --remainingCommands;
        m.primitives.push_back(std::move(primitive));
      }
      m.streamBytes += size;
      part += 32 + size;
    }
    models.push_back(m);
  }
}
SmdResourceCounts Smd::commonRequirements() const {
  // SmdSetParam always appends the first common TPL, even for local BINs.
  SmdResourceCounts out{0, 1, 0};
  for (const auto &w : placements) {
    if (w.work == 0xff)
      continue;
    if (w.flags & 0x10)
      out.bins = std::max(out.bins, unsigned(w.bin == 0xff ? 0 : w.bin) + 1);
    if ((w.flags & 0x40) && w.motion != 0xff)
      out.motions = std::max(out.motions, unsigned(w.motion) + 1);
  }
  return out;
}
std::string Smd::describe() const {
  std::size_t vertices = 0, lists = 0, streams = 0, common = 0;
  for (const auto &m : models) {
    vertices += m.vertices;
    lists += m.displayLists;
    streams += m.streamBytes;
  }
  for (const auto &w : placements)
    if (w.work != 0xff && (w.flags & 0x10))
      ++common;
  std::size_t corners = 0, triangles = 0, commands = 0;
  for (const auto &m : models)
    for (const auto &p : m.primitives) {
      corners += p.vertices.size();
      triangles += p.triangles.size();
      ++commands;
    }
  std::ostringstream s;
  s << placements.size() << " base placements · " << groups.size()
    << " groups · " << logicalWorkCount << " logical works\n"
    << models.size() << " local BIN models · " << vertices << " Vertices · "
    << lists << " display-list blocks · " << streams << " stream bytes\n"
    << tplOffsets.size() << " local TPL references · " << motionOffsets.size()
    << " local motion references · " << common
    << " common model references (resource source separate)\n"
    << "GX: " << commands << " Primitive · " << corners
    << " vertex references · " << triangles
    << " triangles (including degenerates)\n"
    << "Attribute indices and UV values verified; local geometry, before "
       "placement transform.\n";
  return s.str();
}
} // namespace re4
