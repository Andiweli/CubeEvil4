#pragma once
#include "animation.h"
#include "hit_geometry.h"
#include "room_objects.h"
#include "room_state.h"
#include "room_jumps.h"
#include "walk_probe.h"
#include <optional>

namespace re4 {
class LeonPreview;
class WalkProbe;
class RoomLadders;
enum class InteriorDoorPhase { Closed, HandOpening, KickOpening, Open, Closing, Shock,
                               ScriptOpen, ScriptClose };
struct InteriorDoor {
  RoomObjectPlacement placement;
  unsigned type{}, hp{1000}, lockHp{};
  float yaw{}; // Offset from authored closed yaw; entire leaf moves at its hinge.
  InteriorDoorPhase phase = InteriorDoorPhase::Closed;
  unsigned timer{};
  unsigned openTimer{}; // Source kick Open_timer=0x96, original 30-Hz ticks.
  bool mirrored{}, hit14{}, hit17{};
  bool scriptLocked{}, scriptInitialized{}; // FREE_EMDOOR::Be_flg bit0 / Rno2.
  float scriptAngle{}; // Preserve the original absolute float ang.y across script ticks.
  std::unique_ptr<AnimationRig> motion;
};
struct InteriorWindow {
  RoomObjectPlacement placement;
  unsigned hp{1000}, hpType{};
  bool protectedByEvent{}, field{}, lowSat{};
  unsigned knifeSerial{};
  bool hasKnifeSerial{};
  std::uint32_t fenceFlags{}; // Original 0x40000000 player / 0x20000000 enemy disable bits.
  float width{}, height{};
};
struct InteriorDoorOffer {
  std::size_t index{};
  unsigned slot{};
  bool front{}, kick{}, blocked{};
};
struct InteriorWindowOffer {
  std::size_t index{};
  unsigned slot{}, direction{};
  CollisionVec start{}, landing{};
  float heading{};
  // cPlayer::actionSelect uses fanceOn for an already broken same-height
  // window. Only intact panes / direction2 use ExeWindowEvent's glass FCV.
  std::optional<FenceOffer> brokenFence;
};
struct InteriorHit {
  enum Kind { DoorLeaf, DoorLock, Window } kind;
  std::size_t index{};
  unsigned slot{};
  AuthoredHitResult hit;
};
struct InteriorRack {
  RoomObjectPlacement placement, base;
  std::array<float, 4> range{}; // Original north/east/south/west setRange.
  float height{};
  bool pushing{}, stopped{};
  unsigned direction{};
  std::unique_ptr<AnimationRig> motion;
};
struct InteriorPushOffer {
  std::size_t index{};
  unsigned slot{}, direction{};
  float heading{};
};
struct InteriorEvent {
  enum Kind { Open, Kick, BlockedKick, Close, LockBroken, WindowCrack,
              WindowBroken, ProtectedWindow, PushStart, PushLoop, PushStop,
              WindowVaultSound, ScriptDoorVoice } kind;
  unsigned slot{};
  int soundBlock = -1;
  unsigned soundNo{};
  CollisionVec position{};
  // cEmWindow::SetBreakEsp EST choice. No sequence means no shard request.
  int effectSequence = -1;
  unsigned effectObject{};
  std::array<float, 3> effectRotation{};
};
const char *InteriorEventLabel(InteriorEvent::Kind kind);
// cEmWindow::DmCk + GetWepSizeGroup for the playable source weapon subset.
// Live protected/event flags and attack serials are owned by RoomInteractions.
unsigned WindowWeaponHitPoints(unsigned hp, unsigned hpType, unsigned weaponNo);

// Shared source-backed ET00/07/25 windows and ET03/0D interior doors in rooms
// carrying their original assets: emDoorActEvtCk/plemDoorOpen/Kick,
// Open/Open2/Close, right-hand lock, cEmWindow's three vault directions,
// DmCk/SetBreakModel and r101 furniture pushing. Window shard requests carry
// their original EST into the Efm05 backend. Campaign callbacks, attached
// window dust, door pane destruction and lock-fall particles remain separate.
class RoomInteractions {
public:
  RoomInteractions(std::uint16_t room, const RoomArchive &archive,
                   const std::vector<RoomObjectPlacement> &placements,
                   const std::string &playerDrs, const RoomSaveState &state,
                   const SatCollision *effect = nullptr);
  const std::vector<InteriorDoor> &doors() const { return doors_; }
  const std::vector<InteriorWindow> &windows() const { return windows_; }
  const std::vector<InteriorRack> &racks() const { return racks_; }
  const RoomSaveState &state() const { return state_; }
  std::uint16_t room() const { return room_; }
  void syncVillageState(const RoomSaveState &state);
  void setVillageHouseRack(WalkProbe &probe);
  void breakVillageWindow(unsigned slot, WalkProbe &probe);
  const std::vector<AnimationBytes> &windowMotions() const { return windowMotions_; }
  void setLadders(const RoomLadders *ladders) { ladders_=ladders; }
  void recordBrokenObject(unsigned slot) { state_.setObject(slot, 1); }
  void setSceneryBodies(std::vector<ObjectBodyBox> bodies, WalkProbe *probe = nullptr) {
    sceneryBodies_ = std::move(bodies); rebuildCollision(nullptr, probe);
  }
  std::optional<InteriorDoorOffer> doorOffer(const CollisionVec &feet,
                                            double heading) const;
  bool interact(LeonPreview &player, WalkProbe &probe);
  std::optional<InteriorWindowOffer> windowOffer(const LeonPreview &player,
                                                const WalkProbe &probe) const;
  std::optional<InteriorPushOffer> pushOffer(const LeonPreview &player,
                                            const WalkProbe &probe) const;
  // Once per original player/world tick, before Leon's MotionMove. Pausing
  // or rebuilding EGL never advances this owner or replaces its flags.
  void tick(LeonPreview &player, WalkProbe &probe, double forward = 0,
             bool allowPush = true);
  // Original r100_WindowBreakCk then r100_DoorCk, once per source world tick.
  // Flags are the actual Room_flg words written by authored AEV areas3/9;
  // foundPlayer is W->ems[0..2], precisely ESL3/4/5. Return value requests
  // their original flag0x80 only when WindowBreakCk sets RSF4.
  bool tickR100Scripts(RoomSaveState &save, SourceFlags<4> &roomFlags,
                      const std::array<bool,3> &foundPlayer);
  bool r100DoorMessageEnabled() const { return r100DoorMessageEnabled_; }
  bool windowFenceEnabled(unsigned slot, unsigned kind) const;
  const SatCollision *collision() const { return collision_.get(); }
  bool visible(int slot, int variant, int lockParent = -1) const;
  std::optional<Matrix34> delta(int slot, int lockParent = -1) const;
  std::optional<InteriorHit> target(const CollisionVec &from,
                                    const CollisionVec &to,
                                    unsigned weaponNo = 1) const;
  // Pure laser classification; never changes hp, flags, serials or RNG.
  bool damageable(const InteriorHit &hit, unsigned weaponNo = 1) const;
  bool damage(const InteriorHit &hit, unsigned weaponNo, unsigned serial);
  // cObjLadder::breakWindow: reset animation's Free bit0 breaks intact
  // windows within 2000 of the original rung-derived top.
  bool breakWindowsAtLadderTop(const CollisionVec &top);
  // A ladder dismount crossing the physical pane must break it before Leon
  // enters the house. Excludes windows protected by a live room event.
  bool breakWindowsCrossed(const CollisionVec &previous, const CollisionVec &current);
  std::vector<InteriorEvent> drainEvents();
  std::string describe() const;
private:
  const RoomLadders *ladders_{};
  std::uint16_t room_{};
  RoomSaveState state_;
  const SatCollision *effect_{};
  std::vector<InteriorDoor> doors_;
  std::vector<InteriorWindow> windows_;
  std::vector<InteriorRack> racks_;
  std::vector<ObjectBodyBox> sceneryBodies_;
  std::array<unsigned,64> shotSerialBySlot_{};
  std::array<bool,64> hasShotSerialBySlot_{};
  std::vector<AnimationBytes> windowMotions_;
  std::array<std::vector<RootMotionStep>, 3> windowPaths_;
  void tickWindow(LeonPreview &player);
  std::optional<std::size_t> activeRack_;
  std::optional<std::size_t> activeDoor_;
  bool activeFront_{};
  std::unique_ptr<Sat> collisionMesh_;
  std::unique_ptr<SatCollision> collision_;
  std::vector<InteriorEvent> events_;
  std::array<unsigned, 64> knifeSerial_{};
  std::array<bool, 64> hasKnifeSerial_{};
  std::uint16_t randomState_{0x6e41};
  std::uint64_t ticks_{};
  enum class R100DoorTask { OpenUntilFirstLook, WaitForFound, CloseUntilExit, Done };
  R100DoorTask r100DoorTask_ = R100DoorTask::OpenUntilFirstLook;
  unsigned villageDoorCloseTicks_{};
  bool villageDoorCompleted_{};
  bool r100WindowsEnabled_{}, r100WindowTaskDone_{}, r100DoorFound_{};
  bool r100CloseSleepPending_{}, r100DoorMessageEnabled_{};
  unsigned r100CloseSleeps_{};
  unsigned randomByte();
  bool rackBlocks(const InteriorDoor &door) const;
  void tickPush(LeonPreview &player, WalkProbe &probe, double forward, bool allowPush);
  bool rackMoveClear(std::size_t index, const RoomObjectPlacement &next,
                     const WalkProbe &probe) const;
  void rebuildCollision(const LeonPreview *player, WalkProbe *probe);
  void breakLock(InteriorDoor &door, bool shot);
  void breakWindow(InteriorWindow &window, const CollisionVec &from,
                   unsigned size, unsigned breakType);
  void event(InteriorEvent::Kind kind, unsigned slot, int block, unsigned number,
             const std::array<float, 3> &position);
};
} // namespace re4
