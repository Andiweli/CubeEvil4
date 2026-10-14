#include "enemy_spawn.h"
#include <algorithm>
#include <stdexcept>

namespace re4 {
namespace {
std::uint16_t half(const std::array<std::uint8_t, 32> &b, std::size_t p) {
  return static_cast<std::uint16_t>((unsigned(b[p]) << 8) | b[p + 1]);
}
std::int32_t signedHalf(const std::array<std::uint8_t, 32> &b, std::size_t p) {
  const auto n = half(b, p);
  return n < 0x8000 ? n : static_cast<std::int32_t>(n) - 0x10000;
}
std::uint32_t word(const std::array<std::uint8_t, 32> &b, std::size_t p) {
  return (std::uint32_t(half(b, p)) << 16) | half(b, p + 2);
}
constexpr double RotationUnit = 3.14159265358979323846 / 16384.0;
} // namespace

EnemySpawns::EnemySpawns(const std::vector<std::uint8_t> &bytes) {
  if (bytes.size() != FileBytes)
    throw std::runtime_error("GC ESL must contain exactly 255 32-byte records");
  records_.reserve(RecordCount);
  for (std::size_t i = 0; i < RecordCount; ++i) {
    EnemySpawn s;
    s.listIndex = i;
    s.recordOffset = i * RecordBytes;
    std::copy_n(bytes.begin() + s.recordOffset, RecordBytes, s.raw.begin());
    s.beFlags = s.raw[0];
    s.id = s.raw[1];
    s.type = s.raw[2];
    s.setNo = s.raw[3];
    s.flags = word(s.raw, 4);
    s.hp = half(s.raw, 8);
    s.emsetNo = s.raw[10];
    s.character = s.raw[11];
    for (std::size_t k = 0; k < 3; ++k) {
      s.position[k] = signedHalf(s.raw, 12 + k * 2) * 10.0;
      s.rotation[k] = signedHalf(s.raw, 18 + k * 2) * RotationUnit;
    }
    s.heading = s.rotation[1];
    s.room = half(s.raw, 24);
    s.guardRadius = signedHalf(s.raw, 26) * 1000.0;
    records_.push_back(s);
  }
}

const EnemySpawn *EnemySpawns::find(std::size_t listIndex) const {
  return listIndex < records_.size() ? &records_[listIndex] : nullptr;
}

std::vector<const EnemySpawn *> EnemySpawns::forRoom(std::uint16_t room) const {
  std::vector<const EnemySpawn *> result;
  for (const auto &s : records_)
    if (s.room == room)
      result.push_back(&s);
  return result;
}
} // namespace re4
