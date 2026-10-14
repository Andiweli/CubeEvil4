#pragma once
#include "collision_query.h"
#include "room_state.h"
#include <vector>
#include <map>

namespace re4 {
struct ScenarioCollisionPiece {
  unsigned number{}, enabled{}, control{}, flags{};
  std::uint32_t sightAttributes{}, effectAttributes{};
  float floor{}, height{};
  std::array<std::array<float,2>,4> points{};
};
// sceAtSetScrAt / SceAtSetEnable: authored, unparented AEV type0xB pieces.
// Parent-attached pieces require their live model owner and are counted below.
class ScenarioCollision {
public:
  ScenarioCollision(std::uint16_t room,const std::vector<std::uint8_t> &aev);
  const std::vector<ScenarioCollisionPiece> &pieces() const {return pieces_;}
  unsigned parentedCount() const {return parented_;}
  bool enabled(const ScenarioCollisionPiece &piece,const RoomSaveState &state) const;
  Sat geometry(const RoomSaveState &state,SatQueryMode mode) const;
  // First install and live RSF10 changes rebuild only the owned additional pieces.
  bool sync(const RoomSaveState &state,SatCollision *sight,SatCollision *effect);
  // Native SceAtSetEnable for a script-owned authored SCR_AT area.
  void setEnabled(unsigned number,bool enabled);
private:
  std::uint16_t room_{};
  std::vector<ScenarioCollisionPiece> pieces_;
  unsigned parented_{};
  bool installed_{},afterTruck_{};
  std::map<unsigned,bool> overrides_;
};
} // namespace re4
