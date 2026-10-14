#pragma once
#include "archive.h"
#include "scene.h"
#include "room_flow.h"
#include <cstdint>
#include <string>

namespace re4 {
// cBlock::roomInit / cBlockUnit::checkBlockLoadToMram, block.cpp.
// Keep the immutable data resident, but display only the original MRAM set
// selected by cBlock::checkBlockArea / checkBlockConnect. ARAM neighbours
// are cached data, not visible scroll objects.
struct RoomBlockLayer {
  StaticScene mesh;
  unsigned count{};
  std::size_t sourceBytes{};
  struct Area {DoorArea volume;unsigned priority{};int number=-1;};
  std::vector<Area> areas;
  std::vector<std::uint32_t> visibleSets;
  // Per block: the placement matrix of each SMD work (core mode compares the
  // game's scroll objects with them).
  std::vector<std::vector<Matrix34>> placements;
  int selectedArea=-1;
  std::uint32_t visibleMask(const CollisionVec &feet);
  std::string describe() const;
};
RoomBlockLayer BuildRoomBlocks(const RoomArchive &room, const Smx *smx,
                               const std::string &decodedRoomPath,
                               std::uint16_t roomId);
} // namespace re4
