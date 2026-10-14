#include "gx_stream.h"
#include <stdexcept>
#include <utility>
namespace re4 {
std::vector<GxPrimitive> DecodeGxStream(const std::vector<std::uint8_t> &b,
                                        std::size_t offset, std::size_t size,
                                        const GxLimits &limits) {
  if (offset > b.size() || size > b.size() - offset)
    throw std::runtime_error("GX: Stream bounds");
  const auto end = offset + size;
  auto u16 = [&](std::size_t p) {
    return static_cast<std::uint16_t>((std::uint16_t(b[p]) << 8) | b[p + 1]);
  };
  std::vector<GxPrimitive> result;
  std::size_t corners = 0, triangles = 0;
  const std::size_t stride = limits.colored ? 8 : 6;
  while (offset < end) {
    auto op = b[offset++];
    if (!op)
      continue; // GX_NOP and zero alignment padding.
    // commonModelTrans configures VAT 0 only. Never guess another VAT or
    // register state.
    if (op != 0x80 && op != 0x90 && op != 0x98 && op != 0xa0 && op != 0xa8 &&
        op != 0xb0 && op != 0xb8)
      throw std::runtime_error("GX: unsupported command/VAT");
    if (end - offset < 2)
      throw std::runtime_error("GX: Truncated vertex count");
    auto count = u16(offset);
    offset += 2;
    if (!count || (op == 0x80 && count % 4) || (op == 0x90 && count % 3) ||
        ((op == 0x98 || op == 0xa0) && count < 3) ||
        (op == 0xa8 && count % 2) || (op == 0xb0 && count < 2))
      throw std::runtime_error("GX: invalid primitive count");
    if (std::size_t(count) > (end - offset) / stride)
      throw std::runtime_error("GX: Truncated vertex references");
    std::size_t nt = op == 0x80                   ? count / 4 * 2
                     : op == 0x90                 ? count / 3
                     : (op == 0x98 || op == 0xa0) ? count - 2
                                                  : 0;
    if (result.size() >= limits.commands || corners > limits.corners ||
        count > limits.corners - corners || triangles > limits.triangles ||
        nt > limits.triangles - triangles)
      throw std::runtime_error("GX: Decoding budget exceeded");
    GxPrimitive p;
    p.opcode = op;
    p.vertices.reserve(count);
    p.triangles.reserve(nt);
    for (unsigned i = 0; i < count; ++i) {
      GxVertexRef v;
      v.position = u16(offset);
      v.normal = u16(offset + 2);
      v.color = limits.colored ? u16(offset + 4) : 0;
      v.texcoord = u16(offset + (limits.colored ? 6 : 4));
      if (v.position >= limits.positions || v.normal >= limits.normals ||
          (limits.colored && v.color >= limits.colors) ||
          v.texcoord >= limits.texcoords)
        throw std::runtime_error("GX: Attribute index outside array bounds");
      p.vertices.push_back(v);
      offset += stride;
    }
    if (op == 0x80)
      for (std::uint32_t i = 0; i < count; i += 4) {
        p.triangles.push_back({i, i + 1, i + 2});
        p.triangles.push_back({i, i + 2, i + 3});
      }
    else if (op == 0x90)
      for (std::uint32_t i = 0; i < count; i += 3)
        p.triangles.push_back({i, i + 1, i + 2});
    else if (op == 0x98)
      for (std::uint32_t i = 2; i < count; ++i)
        p.triangles.push_back(
            i % 2 ? std::array<std::uint32_t, 3>{i - 1, i - 2, i}
                  : std::array<std::uint32_t, 3>{i - 2, i - 1, i});
    else if (op == 0xa0)
      for (std::uint32_t i = 2; i < count; ++i)
        p.triangles.push_back({0, i - 1, i});
    corners += count;
    triangles += nt;
    result.push_back(std::move(p));
  }
  return result;
}
} // namespace re4
