#pragma once
#include "scene.h"
namespace re4 {
struct RoomShadowData {
  // Only source SHD receivers; they never enter the ordinary world mesh.
  std::vector<float> vertices; // position, normal: six floats per corner.
  unsigned receivers{};
};
std::shared_ptr<const RoomShadowData> ReadRoomShadows(const RoomArchive &room);
} // namespace re4
