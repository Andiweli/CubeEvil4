#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace re4 {
// Byte-wise disk reads. A GameCube file offset is never a native pointer.
class AssetReader {
public:
  const std::vector<std::uint8_t> &bytes;
  void range(std::size_t p, std::size_t n) const {
    if (p > bytes.size() || n > bytes.size() - p)
      throw std::runtime_error("Game data: Truncated range");
  }
  std::uint16_t u16(std::size_t p) const {
    range(p, 2);
    return (std::uint16_t(bytes[p]) << 8) | bytes[p + 1];
  }
  std::uint32_t u32(std::size_t p) const {
    return (std::uint32_t(u16(p)) << 16) | u16(p + 2);
  }
  float f32(std::size_t p) const {
    auto bits = u32(p);
    float v;
    std::memcpy(&v, &bits, 4);
    if (!std::isfinite(v))
      throw std::runtime_error("Game data: non-finite value");
    return v;
  }
};
} // namespace re4
