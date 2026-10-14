#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace re4 {
struct GxVertexRef {
  std::uint16_t position{}, normal{}, color{}, texcoord{};
};
struct GxPrimitive {
  std::uint8_t opcode{};
  std::uint32_t displayList{};
  std::vector<GxVertexRef> vertices;
  // Corner indices into vertices, preserving separate attribute indices/seams.
  // Strip degenerates are retained; alternating winding still advances.
  std::vector<std::array<std::uint32_t, 3>> triangles;
};
struct GxLimits {
  std::size_t positions{}, normals{}, colors{}, texcoords{};
  std::size_t corners = 2000000, triangles = 2000000, commands = 131072;
  bool colored{};
};
std::vector<GxPrimitive> DecodeGxStream(const std::vector<std::uint8_t> &bytes,
                                        std::size_t offset, std::size_t size,
                                        const GxLimits &limits);
} // namespace re4
