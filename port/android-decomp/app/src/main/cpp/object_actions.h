#pragma once
#include "hit_geometry.h"
#include "leon_preview.h"
#include "room_objects.h"
#include "item_effects.h"
#include "room_items.h"
#include <optional>

namespace re4 {
// Selected original cEmBox and non-rolling cEmBarrel runtimes. Room event
// callbacks, treasure chests and rolling barrels remain separate runtimes.
struct ObjectDrop {
  std::size_t recordIndex{};
  unsigned sourceId{}, id{}, count{}, areaNo{}, itemFlag{}, autoFlag{};
  unsigned effect{}, playerType{}, controlFlags{};
  CollisionVec position{}; // Original ITA position, or ETS +50 Y if ctrl bit0 off.
  std::array<float, 3> rotation{};
  double radius{}; // Zero means the original auto-area default, 1500.
  std::array<std::uint8_t, 156> raw{};
};
struct ObjectActionObject {
  RoomObjectPlacement placement;
  unsigned boxType{};
  bool explosive{}, hasItem{};
  Matrix34 matrix{};
  std::vector<AuthoredHitShape> shapes;
  ObjectDrop item;
};
struct ObjectActionState {
  unsigned slot{}, hp{1000};
  bool broken{}, itemActive{}, itemTaken{};
  unsigned knifeSerial{};
  bool hasKnifeSerial{};
  unsigned itemId{}, itemCount{};
  bool itemResolved{};
  unsigned bombWait{};
  unsigned shotSerial{};
  bool hasShotSerial{};
};
struct ObjectInventoryItem {
  unsigned id{}, count{};
};
struct ObjectActionsSnapshot {
  std::uint16_t room{};
  std::uint64_t profile{}, checksum{};
  std::vector<ObjectActionState> states;
  std::vector<ObjectInventoryItem> inventory;
  unsigned shotSerial{};
  bool hasShotSerial{};
  std::uint16_t randomState{0x6e41};
  unsigned noDropCount{}, noRecoveryDropCount{}, eventSerial{};
  std::uint64_t tick{};
};
struct ObjectActionHit {
  std::size_t index{};
  unsigned slot{};
  AuthoredHitResult hit;
};
struct ObjectPickup {
  std::size_t index{};
  unsigned slot{}, id{}, count{};
};
struct ObjectBreakEvent {
  unsigned serial{}, slot{}, objectId{}, boxType{}, breakKind{};
  std::size_t index{};
  bool explosive{};
  unsigned soundBlock{}, soundNo{};
  std::array<float, 3> position{}, rotation{};
};
struct ObjectBlast {
  std::size_t index{};
  unsigned slot{}, serial{}, weaponNo{0x13};
  CollisionVec position{};
  float radius{6000};
  unsigned origin{}; // 0 room objects/traps, 1 player subweapons.
};
// Source random-item branch for normal Leon carrying the currently playable
// handgun. Other carried weapon classes need their GetDropBullet branches.
struct ObjectDropContext {
  unsigned health{1200}, gameLevel{5}, stage{1}, handgunAmmo{};
  unsigned shotgunAmmo{}, tmpAmmo{}, bowAmmo{}, recoveryPoints{}, greenHerbs{};
  bool mercenaries{};
};
struct ObjectPlayerBlastHit { bool inner{}; unsigned baseDamage{}; };
std::vector<unsigned> ObjectRandomDropModelIds(bool mercenaries = false);

class ObjectActions {
public:
  ObjectActions(std::uint16_t room, const RoomArchive &archive,
                const std::vector<RoomObjectPlacement> &placements,
                const SatCollision *effectCollision = nullptr,
                ScenarioState *scenario = nullptr);
  const std::vector<ObjectActionObject> &objects() const { return objects_; }
  const std::vector<ObjectActionState> &states() const { return states_; }
  const std::vector<ObjectInventoryItem> &inventory() const { return inventory_; }
  // Source ITA taken flags are authoritative across rooms and restored
  // per-room snapshots. They do not replace the intact container state.
  bool itemVisible(std::size_t index) const;
  bool itemModelVisible(std::size_t index) const;
  std::vector<ItemEffectSource> effectSources() const;
  void setDropContext(const ObjectDropContext &context);
  // Called once per original 30 Hz tick, before stepping the player. Barrel
  // blast damage follows two later ticks; radial box/chain checks run here.
  std::vector<ObjectBlast> tick();
  void applyExplosion(const ObjectBlast &blast);
  std::vector<ObjectBreakEvent> drainEvents();
  std::optional<ObjectPlayerBlastHit>
  playerBlastHit(const ObjectBlast &blast, const CollisionVec &parts0World,
                 bool infiniteLauncherEquipped=false) const;
  std::uint16_t room() const { return room_; }
  std::uint64_t profile() const { return profile_; }
  std::optional<ObjectActionHit>
  target(const CollisionVec &from, const CollisionVec &to, unsigned weaponNo = 1,
         float maxLenSq = 1e16f) const;
  // One source weapon hit breaks this subset. Serial numbers prevent one
  // shot/knife attack from being consumed twice by render-frame callbacks.
  // The bounded adapter commits damage in the player's after-tick callback;
  // the GC cEmBox instead polls its queued damage in the next EmMgr pass.
  std::optional<ObjectActionHit> shoot(const PlayerShot &shot,
                                      float maxLenSq = 1e16f);
  bool applyShot(const ObjectActionHit &hit, unsigned serial, unsigned weaponNo = 1);
  bool applyKnife(const ObjectActionHit &hit, unsigned serial);
  std::vector<ObjectActionHit> knife(const PlayerMeleeEvent &event);
  std::optional<std::size_t>
  pickupTarget(const CollisionVec &feet, double heading,
               const std::optional<CollisionVec> &clippedFront = {}) const;
  std::optional<ObjectPickup>
  pickup(const CollisionVec &feet, double heading, bool actionPressed,
         const std::optional<CollisionVec> &clippedFront = {});
  // Commit the previously validated, frozen item-examination target only.
  std::optional<ObjectPickup> takePickup(std::size_t index);
  // Transfer collected ammo to the existing weapon runtime, or consume a
  // future inventory item. Failed requests leave the inventory unchanged.
  bool consumeItem(unsigned id, unsigned count);
  // Supplies collected in an inactive room remain usable. Checksummed state
  // is validated before mutation; invalid/insufficient requests return false.
  static bool consumeSnapshotItem(ObjectActionsSnapshot &snapshot, unsigned id,
                                  unsigned count);
  ObjectActionsSnapshot snapshot() const;
  void restore(const ObjectActionsSnapshot &snapshot);
  std::string describe() const;

private:
  std::uint16_t room_{};
  std::uint64_t profile_{};
  const SatCollision *effectCollision_{};
  ScenarioState *scenario_{};
  std::shared_ptr<const RoomItemArchive> itemModels_;
  std::vector<ObjectActionObject> objects_;
  std::vector<ObjectActionState> states_;
  std::vector<ObjectInventoryItem> inventory_;
  unsigned shotSerial_{};
  bool hasShotSerial_{};
  std::uint16_t randomState_{0x6e41};
  unsigned noDropCount_{}, noRecoveryDropCount_{}, eventSerial_{};
  std::uint64_t tick_{};
  ObjectDropContext dropContext_;
  std::vector<ObjectBreakEvent> events_;
  void breakObject(std::size_t index, unsigned breakKind = 0);
  void resolveRandomDrop(std::size_t index);
  unsigned randomByte();
};
} // namespace re4
