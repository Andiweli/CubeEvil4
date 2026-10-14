#pragma once
#include "room_flow.h"
#include "ladder_motion.h"
#include "leon_preview.h"
#include "walk_probe.h"
#include "room_objects.h"
namespace re4 {
class RoomInteractions;
enum class LadderAction { Climb, Descend, Tip, Reset };
struct LadderPropSound { unsigned block{}, number{}; CollisionVec position{}; };
struct LadderOffer {
  unsigned no{};
  bool down{};
  CollisionVec alignment{}, landing{};
  float heading{};
  bool object{};
  unsigned rungs{}, type{};
  LadderAction action = LadderAction::Climb;
  bool flip{};
};
// M28 checked fixed AEV tower ladders, r101#5 / r400#10, nine levels.
// The top return is an explicit reciprocal Android action using the original
// down FCVs. It is not a fabricated negative AEV or the missing jump/event VM.
// ETS ET06/ET08 use obj13's separate climb FCVs and original body box.
// r101 house ladders remain under their genuine RSF8 event prerequisite;
// a dedicated post-house test profile may supply that state explicitly.
class RoomLadders {
public:
  explicit RoomLadders(const RoomFlow &flow);
  RoomLadders(const RoomFlow &flow, const RoomArchive &archive,
              const std::vector<RoomObjectPlacement> &placements,
              const RoomSaveState &state, const std::string &playerDrs);
  std::optional<LadderOffer> offer(const LeonPreview &player, const WalkProbe &probe,
                                 const std::optional<CollisionVec> &clippedFront = {}) const;
  bool interact(LeonPreview &player, WalkProbe &probe,
                const std::optional<CollisionVec> &clippedFront = {});
  // Borrow the live shared wall owner. Tip alignment disables SCA in the
  // source motion; validate its approach before allowing that transition.
  void setInteractions(const RoomInteractions *interactions) { interactions_=interactions; }
  // Original cObjLadder::move: advance the prop FCV/SEQ once per 30Hz world
  // tick, update collision and break windows at the reset Free-bit event.
  void tick(WalkProbe &probe, RoomInteractions *interactions = nullptr);
  std::optional<Matrix34> delta(int slot) const;
  std::vector<LadderPropSound> drainSounds();
  void setVillageHouse(bool enabled, WalkProbe &probe, RoomInteractions *interactions);
  std::size_t count() const { return ladders_.size() + objectLadders_.size(); }
  std::size_t fixedCount() const { return ladders_.size(); }
  const std::vector<RoomObjectPlacement> &objectLadders() const { return objectLadders_; }
  const std::vector<AnimationBytes> &objectMotions() const { return objectMotions_; }
  // Append these to the shared RoomInteractions body/wall layer: never
  // replace the doors/furniture collision borrower with a second owner.
  std::vector<ObjectBodyBox> objectBodies() const;
  // obj13::LadderNearCk: standing ladder tops within2000 reserve the
  // window transition for ladder actions. Falling/fallen props release it.
  bool permitsWindowCross(const CollisionVec &window) const;
  // Standalone shared-wall owner for checked rooms without interior doors.
  // Rooms with RoomInteractions must merge objectBodies into that owner.
  const SatCollision *collision() const { return collision_.get(); }
  std::string describe() const;
private:
  bool villageObjectsEnabled_=true;
  const RoomInteractions *interactions_{};
  std::vector<RoomLadder> ladders_;
  struct ObjectStep { RootMotionStep root; unsigned exitTicks{}; };
  std::vector<RoomObjectPlacement> objectLadders_;
  std::vector<AnimationBytes> objectMotions_;
  std::array<std::vector<ObjectStep>, 2> objectPaths_;
  struct ObjectRuntime {
    enum Phase { Standing, Falling, Fallen, Raising } phase = Standing;
    std::unique_ptr<SkinnedModel> model;
    std::unique_ptr<AnimationRig> motion;
    std::array<float,3> pivot{};
    CollisionVec motionPosition{};
    double motionYaw{};
    bool standingPosePending{};
    unsigned downWait{}, climbWait{}, resetWait{};
  };
  std::vector<ObjectRuntime> objectStates_;
  std::vector<LadderPropSound> sounds_;
  bool bodiesChanged_{};
  void rebuildCollision();
  std::unique_ptr<Sat> collisionMesh_;
  std::unique_ptr<SatCollision> collision_;
};
} // namespace re4
