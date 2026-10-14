#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace re4 {
struct SmxRecord {
  std::uint8_t work{}, type{}, order{}, cull{};
  std::uint32_t lightMask{}, flags{}, material{}, specular{};
  std::array<float, 2> uvSpeed{};
  std::array<std::uint8_t, 120> objectWork{};
  std::array<std::uint8_t, 3> materialRGB() const;
};
class Smx {
public:
  explicit Smx(const std::vector<std::uint8_t> &bytes);
  std::vector<SmxRecord> records;
  const SmxRecord *find(unsigned work) const;
};
} // namespace re4
