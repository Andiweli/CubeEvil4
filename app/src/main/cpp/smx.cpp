#include "smx.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace re4 {
Smx::Smx(const std::vector<std::uint8_t> &bytes) {
  if (bytes.size() < 16 || bytes[0] != 0x10)
    throw std::runtime_error("SMX: invalid header or version");
  auto count = bytes[1];
  if (bytes.size() < 16 + std::size_t(count) * 0x90)
    throw std::runtime_error("SMX: Truncated records");
  auto word = [&](std::size_t p) {
    return (std::uint32_t(bytes[p]) << 24) |
           (std::uint32_t(bytes[p + 1]) << 16) |
           (std::uint32_t(bytes[p + 2]) << 8) | bytes[p + 3];
  };
  for (unsigned i = 0; i < count; ++i) {
    auto p = 16 + i * 0x90;
    SmxRecord r;
    r.work = bytes[p];
    r.type = bytes[p + 1];
    r.order = bytes[p + 2];
    r.cull = bytes[p + 3];
    r.lightMask = word(p + 4);
    r.flags = word(p + 8);
    r.material = word(p + 12);
    r.specular = word(p + 0x84);
    std::copy_n(bytes.begin() + p + 16, 120, r.objectWork.begin());
    for (unsigned k = 0; k < 2; ++k) {
      auto bits = word(p + 0x88 + k * 4);
      std::memcpy(&r.uvSpeed[k], &bits, 4);
      if (!std::isfinite(r.uvSpeed[k]))
        throw std::runtime_error("SMX: invalid UV speed");
    }
    records.push_back(r);
  }
}
const SmxRecord *Smx::find(unsigned work) const {
  if (work > 0xf9)
    return nullptr; // smxInit rejects IDs > 0xf9; setObj skips 0xfe.
  for (const auto &r : records)
    if (r.work == work)
      return &r;
  return nullptr; // smxInit uses the first matching record.
}
std::array<std::uint8_t, 3> SmxRecord::materialRGB() const {
  if ((material & 0xffffff00u) == 0)
    return {255, 255, 255};
  return {std::uint8_t(material >> 24), std::uint8_t(material >> 16),
          std::uint8_t(material >> 8)};
}
} // namespace re4
