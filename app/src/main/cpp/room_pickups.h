#pragma once
#include "archive.h"
#include "room_flow.h"
#include "room_items.h"
#include "room_state.h"
#include "item_effects.h"
#include <optional>
#include <set>

namespace re4 {
struct RoomPickupRecord {
  RoomDoor trigger;
  unsigned id{}, count{}, itemFlag{}, autoFlag{}, controlFlags{};
  CollisionVec position{};
  std::array<float, 3> rotation{};
  bool hasModel{}, runtime{}, runtimeTaken{};
  bool eventEnabled{true}; // SceSetItemEvent disables model and pickup.
  unsigned lifetimeTicks{};
  unsigned effectType{};
  std::array<float,3> effectOffset{};
};
struct RoomPickupResult {
  std::size_t index{};
  unsigned area{}, id{}, count{}, pesetas{};
};
// Original sceAtSetItem waiting-type0 branch, area/facing/EAT checks, native
// MSB-first taken flags and itemInfo defaults. Enemy and box prerequisites,
// shoot-down items, event callbacks and inventory case layout are separate
// runtimes. Loading never turns those waiting records into free pickups.
class RoomPickups {
public:
  RoomPickups(std::uint16_t room, const RoomArchive &archive,
              ScenarioState &state, const SatCollision *effect = nullptr);
  const std::vector<RoomPickupRecord> &items() const { return items_; }
  std::vector<RoomItemPlacement> renderPlacements() const;
  bool visible(int index) const;
  std::optional<std::size_t> target(
      const CollisionVec &feet, double heading,
      const std::optional<CollisionVec> &clippedFront = {}) const;
  std::optional<RoomPickupResult> take(std::size_t index);
  void setAuthoredAreaEnabled(unsigned area, bool enabled);
  // SceAtCreateItemAt: auto-area and ordinary source default quantities.
  std::size_t addRuntimeDrop(unsigned id,unsigned count,const CollisionVec &position,int effect=-1);
  std::vector<RoomPickupRecord> runtimeDrops() const;
  // SceAtCreateItemAt only allocates SAVE_ITEM for literal item types5/7.
  std::vector<RoomPickupRecord> persistentRuntimeDrops() const;
  void restoreRuntimeDrops(const std::vector<RoomPickupRecord> &saved);
  void tick();
  std::vector<ItemEffectSource> effectSources() const;
  std::string describe() const;
private:
  std::uint16_t room_{};
  ScenarioState *state_{};
  const SatCollision *effect_{};
  std::vector<RoomPickupRecord> items_;
  unsigned records_{}, waiting_{}, unsupported_{};
  std::set<unsigned> modelIds_;
  std::shared_ptr<const RoomItemArchive> models_;
  bool taken(const RoomPickupRecord &item) const;
  bool lineVisible(const RoomPickupRecord &item, const CollisionVec &feet) const;
};
} // namespace re4
