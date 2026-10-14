#pragma once
#include "collision_query.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace re4 {
// Original GC EM_LIST from etc/emleonNN.esl. These are editor records, not
// cEm pointers and not permission to skip the room's scenario spawn rules.
struct EnemySpawn {
  std::size_t listIndex{}, recordOffset{};
  std::uint8_t beFlags{}, id{}, type{}, setNo{}, emsetNo{}, character{};
  std::uint32_t flags{};
  std::uint16_t hp{}, room{};
  CollisionVec position{}, rotation{};
  double heading{}, guardRadius{};
  std::array<std::uint8_t, 32> raw{};
  bool alive() const { return (beFlags & 1) != 0; }
  bool alreadySet() const { return (beFlags & 2) != 0; }
};

class EnemySpawns {
public:
  static constexpr std::size_t RecordBytes = 32;
  // Actual GC ESL files contain entries 0x00..0xfe; 0xff is the event enemy
  // sentinel used by EmSetEvent, and has no record in these assets.
  static constexpr std::size_t RecordCount = 255;
  static constexpr std::size_t FileBytes = RecordBytes * RecordCount;
  explicit EnemySpawns(const std::vector<std::uint8_t> &bytes);
  const std::vector<EnemySpawn> &records() const { return records_; }
  const EnemySpawn *find(std::size_t listIndex) const;
  // Includes inactive, already-set and empty records. Callers must preserve
  // those semantics; the original campaign additionally checks death flags.
  std::vector<const EnemySpawn *> forRoom(std::uint16_t room) const;

private:
  std::vector<EnemySpawn> records_;
};
} // namespace re4
