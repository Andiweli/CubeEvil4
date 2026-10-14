#pragma once
#include "collision_query.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace re4 {
struct ScenarioState;
// GC disk records, never native GC pointers. Original: room_jmp.h /
// room_jmp.cpp.
struct RoomStart {
  std::uint16_t room{}, flags{};
  std::size_t point{}, recordOffset{};
  CollisionVec position{};
  double heading{};
  std::string name, person, person2;
  std::array<std::uint8_t, 32> raw{};
  bool valid() const { return (flags & 1) != 0; }
};
class RoomInfo {
public:
  explicit RoomInfo(const std::vector<std::uint8_t> &bytes);
  const std::vector<RoomStart> &starts() const { return starts_; }
  // The original point numbering is retained, including invalid records.
  const RoomStart *find(std::uint16_t room, std::size_t point = 0) const;
  std::size_t stageCount() const { return stages_; }

private:
  std::vector<RoomStart> starts_;
  std::size_t stages_{};
};

// Original AREA_HIT_DATA; points retain the editor's four-edge ordering.
struct DoorArea {
  std::uint8_t enabled{}, type{};
  double floor{}, height{}, radius{};
  std::array<std::array<double, 2>, 4> points{};
  std::array<std::uint8_t, 48> raw{};
};
struct RoomDoor {
  std::uint16_t sourceRoom{}, destinationRoom{};
  std::size_t recordIndex{};
  std::uint8_t no{}, enabled{}, hitType{}, triggerType{}, targetType{};
  std::uint8_t priority{}, taskLevel{}, kind{}, waitingType{}, waitingNo{};
  std::uint8_t country{}, keyId{}, keyFlag{}, destinationPart{}, fadeEffect{};
  std::int8_t hitDirection{}, hitOpenAngle{};
  std::uint32_t parentPointer{}, functionPointer{}, parameterPointer{};
  std::uint32_t exitFunctionPointer{}, exitParameterPointer{};
  DoorArea area;
  CollisionVec destination{};
  double destinationHeading{};
  // Unmodified 0x9c disk record, exposed for independent validation.
  std::array<std::uint8_t, 156> raw{};
};
struct RoomLadder {
  RoomDoor trigger; // Identical source area/facing/flags, not a room transition.
  CollisionVec position{};
  double heading{};
  int level{};
  unsigned type{}, posSet{};
  std::array<unsigned, 3> cameraCuts{};
};
struct DoorSelection {
  const RoomDoor *door{};
  bool available{}, activated{};
  std::string reason;
};
class RoomFlow {
public:
  RoomFlow(std::uint16_t sourceRoom, const std::vector<std::uint8_t> &aev);
  const std::vector<RoomDoor> &doors() const { return doors_; }
  const std::vector<RoomLadder> &ladders() const { return ladders_; }
  const std::vector<RoomDoor> &typewriters() const { return typewriters_; }
  std::uint16_t sourceRoom() const { return sourceRoom_; }
  std::size_t recordCount() const { return count_; }
  const RoomDoor *find(std::uint8_t no) const;
  // Reproduces the unparented original area test. The caller can supply the
  // front point after SAT clipping, as sceAtCheck_main does. Feet use GC units;
  // actor heading is radians, +Z at zero (not the inspection camera yaw).
  static bool hit(const RoomDoor &door, const CollisionVec &feet,
                  double heading,
                  const std::optional<CollisionVec> &clippedFront = {});
  // Availability is deliberately bounded to audited script-unmodified door0:
  // r300/r301 and the forward forest exit r100->r101. The latter keeps the
  // authored held-action trigger; r100_DoorCk controls the house leaf, not it.
  // Other parsed doors remain unavailable until their callbacks are ported.
  static std::string unavailableReason(const RoomDoor &door);
  DoorSelection
  select(const CollisionVec &feet, double heading, bool actionPressed,
         bool actionHeld,
         const std::optional<CollisionVec> &clippedFront = {},
         const ScenarioState *state=nullptr,unsigned part=0,unsigned entryTicks=120) const;

private:
  std::uint16_t sourceRoom_{};
  std::size_t count_{};
  std::vector<RoomDoor> doors_;
  std::vector<RoomLadder> ladders_;
  std::vector<RoomDoor> typewriters_;
};
} // namespace re4
