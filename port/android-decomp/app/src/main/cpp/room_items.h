#pragma once
#include "archive.h"
#include "scene.h"
#include <map>
#include <string>
#include <vector>

namespace re4 {
class ObjectActions;
struct RoomItemAsset {
  unsigned id{};
  std::size_t binOffset{}, binExtent{}, tplOffset{}, tplExtent{};
};
// Original item_model.cpp: ITM id records (8 bytes) and separate BIN/TPL
// offset tables. Offsets are relative to their own table, not the ITM header.
class RoomItemArchive {
public:
  explicit RoomItemArchive(std::vector<std::uint8_t> bytes);
  const std::vector<RoomItemAsset> &assets() const { return assets_; }
  const RoomItemAsset *find(unsigned id) const;
  std::vector<std::uint8_t> bin(unsigned id) const;
  std::vector<std::uint8_t> tpl(unsigned id) const;
  // Unanimated ITM parts retain the source BIN rest translations. Item
  // effect0x2d attaches to those parts, rather than the world-space glow.
  std::vector<EnvironmentMatrix> effectParts(unsigned id,
      const std::array<float,3> &position,const std::array<float,3> &rotation) const;

private:
  std::vector<std::uint8_t> bytes_;
  std::vector<RoomItemAsset> assets_;
  mutable std::map<unsigned,std::vector<std::array<float,3>>> effectCenters_;
};
struct RoomItemPlacement {
  unsigned index{}, id{};
  std::array<float, 3> position{}, rotation{};
};
struct RoomItemLayer {
  StaticScene mesh;
  std::size_t rendered{}, uniqueModels{}, owners{};
  std::vector<RoomItemPlacement> placements;
  std::string describe() const;
};
// Creates the real room ITM models once. Each draw retains the stable
// ObjectActions index plus ITM id so only the active, untaken, resolved model
// becomes visible. Random-item boxes preload their source-defined variants;
// this does not resolve their drop or advance the game RNG during loading.
RoomItemLayer BuildRoomItems(const RoomArchive &room,
                             const std::vector<RoomItemPlacement> &placements,
                             bool variants=false);
RoomItemLayer BuildRoomItems(const RoomArchive &room,
                             const ObjectActions &actions);
void AppendRoomItemScene(StaticScene &scene, const RoomItemLayer &items);
} // namespace re4
