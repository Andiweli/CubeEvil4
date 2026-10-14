#pragma once
#include "animation.h"
#include "ladder_motion.h"
#include "room_jumps.h"
#include "collision_query.h"
#include "follow_camera.h"
#include "weapon_sway.h"
#include "player_eyes.h"
#include "weapon_action_source.h"
#include <functional>
#include <optional>
#include <utility>
namespace re4 {
class WalkProbe;
class WeaponPreview;
struct InteriorWindowOffer;
struct ObjectPlayerBlastHit;
struct SkinCorner {
  BoneWeight position, normal;
};
enum class PlayerAction {
  Footwork,
  Draw,
  Aim,
  Fire,
  Reload,
  Holster,
  Grabbed,
  GrabRelease,
  GrabEscape,
  Recover,
  Dead,
  KnifeDraw,
  KnifeReady,
  KnifeSlash,
  KnifeHolster,
  Kick,
  BlastBlow,
  BlastStagger,
  BlastRecover,
  BlastDead,
  DoorOpen,
  DoorKick,
  DoorBlockedKick,
  LadderUp,
  LadderDown,
  JumpDown,
  JumpLanding,
  PushTakeHold,
  Push,
  PushStopped,
  PushRelease,
  WindowVault,
  ItemExam,
  LadderTip,
  LadderReset,
  FenceVault,
  JumpFall,
  JumpFallLanding,
  Hurt,
  HurtDead,
  DogRescue,
  BearTrapBite,
  RoomScript
};
const char *PlayerActionLabel(PlayerAction action);
struct PlayerActionInput {
  bool aimHeld{}, fireHeld{}, firePressed{}, reloadPressed{};
  bool aimPressed{}, knifePressed{};
  // Mapped RS X: physical right is negative; aim adapts it to camera-right.
  // Pitch is positive for physical RS up, independent of horizontal mapping.
  double yaw{}, pitch{};
  double gyroYaw{},gyroPitch{}; // Screen-relative angular velocity in rad/s.
  double scopeZoom{};
  // Platform shoulder-view state, sampled before a weapon draw. The source
  // handgun folds cameraYaw into ang.y over draw frames 0..4. The original
  // CameraControl::getCameraPitch returns zero; cameraPitch is retained for
  // input compatibility and never seeds gun/knife aiming.
  bool hasCameraLook{};
  double cameraYaw{}, cameraPitch{};
  unsigned resistancePresses{}; // Physical edges accumulated between frames.
  std::uint64_t
      resistPressed{}; // Universal action edges, counted once per tick.
  bool knifeHeld{}, interactPressed{};
};
struct PlayerShot {
  unsigned serial{};
  std::array<double, 3> from{}, to{};
  unsigned weaponItemId = 0x21;
  unsigned powerLevel{}; // Source inventory firepower nibble, zero-based0..6.
  bool flightImpact{}; // A projectile owner resolved this finite flight segment.
  unsigned weaponNumber() const { return OriginalWeaponNumber(weaponItemId); }
  bool projectile() const { const auto n=weaponNumber();return !flightImpact && (n==13 || n==14 || n==19 || n==22 || n==23 || n==28); }
  // Original shotgun center plus twelve ring pellets; one trigger/ammo debit.
  std::vector<std::array<CollisionVec, 2>> pellets{};
};
using WindowCameraPose = ActionCameraPose;
enum class PlayerMeleeKind { Knife, Kick };
struct PlayerMeleeEvent {
  PlayerMeleeKind kind = PlayerMeleeKind::Knife;
  unsigned serial{}; // One authored attack, shared by its per-frame checks.
  std::vector<std::pair<CollisionVec, CollisionVec>> rays;
  unsigned flag{}; // Knife: hitCheck weak=4/first=8; kick: source priority 0xA.
  std::array<CollisionVec, 2> centers{};
  double radius{};
};
struct PlayerSequenceSound {
  unsigned number{}, sub{}, part{};
  CollisionVec position{}, rootPosition{};
};
struct PlayerWeaponSound {
  unsigned block{},number{},part{};
  CollisionVec position{};
  std::uint64_t tick{};
  float frame{};
};
using PlayerSimulationTick = std::function<void(const PlayerShot *)>;
// Original Leon assets and authored root-motion locomotion in the prototype.
// This does not install cPlayer's combat, inventory or scenario state machine.
class LeonPreview {
public:
  LeonPreview(const std::string &playerDrs, const std::string &weaponDrs,
              const std::string &enemyDrs = {}, bool enableMelee = false);
  ~LeonPreview();
  void reset();
  void update(const std::array<double, 3> &feet, double distance, float heading,
              bool run, double seconds);
  void setPlacement(const std::array<double, 3> &feet, float heading);
  void suspendClock();
  void inheritInventory(const LeonPreview &previous);
  void installHandgun(const std::string &weaponDrs);
  void installShotgun(const std::string &weaponDrs);
  bool equipWeapon(unsigned itemId);
  unsigned equippedWeaponId() const;
  unsigned magazineCapacity() const;
  bool setWeaponAmmo(unsigned itemId, unsigned loaded, unsigned reserve);
  bool setWeaponPowerLevel(unsigned itemId, unsigned level);
  unsigned weaponPowerLevel(unsigned itemId) const;
  unsigned handgunMagazine() const;
  unsigned handgunReserve() const;
  unsigned shotgunMagazine() const;
  unsigned shotgunReserve() const;
  std::optional<std::pair<unsigned,unsigned>> weaponAmmo(unsigned itemId) const;
  bool canEquip(unsigned itemId) const;
  unsigned addShotgunAmmo(unsigned count);
  unsigned fireTicks() const { return fireTicks_; }
  // Transfers collected handgun rounds into the prototype's aggregate
  // reserve; returns accepted rounds so an inventory item is never lost.
  // Source item04 stacks have 50 rounds. Their case layout is separate;
  // this aggregate stays within bulletNumTotal's unsigned 16-bit range.
  unsigned addHandgunAmmo(unsigned count);
  // itemUse(0x06) calls healing(600); a full or dead player consumes nothing.
  unsigned heal(unsigned amount);
  bool hasGrabMotions() const { return hasGrabMotions_; }
  bool hasMeleeMotions() const { return hasMeleeMotions_; }
  bool knifeActive() const {
    return action_ == PlayerAction::KnifeDraw ||
           action_ == PlayerAction::KnifeReady ||
           action_ == PlayerAction::KnifeSlash ||
           action_ == PlayerAction::KnifeHolster;
  }
  bool bladeVisible() const { return bladeVisible_; }
  bool visiblePart(unsigned work) const {
    const bool mineGrip=(equippedWeaponId()==0x36 || equippedWeaponId()==0xab) && aiming();
    if(work>=1300 && work<1559)return mineGrip && work==1300+weaponProfile_;
    if(work==1190)return unarmed_;
    if(work>=600 && work<859 && work!=618) return !unarmed_ && !mineGrip && work==600+weaponProfile_;
    if(work>=900 && work<1159) return leftHandNow_==0 && work==900+weaponProfile_;
    if(work==618) return leftHandNow_==2;
    if(work==20) return leftHandNow_==4;
    if(work==21) return leftHandNow_==5;
    return work != 9 || bladeVisible_;
  }
  const std::optional<PlayerMeleeEvent> &lastMeleeEvent() const {
    return meleeEvent_;
  }
  void beginKick(const CollisionVec &target);
  bool doorActive() const {
    return action_ == PlayerAction::DoorOpen || action_ == PlayerAction::DoorKick ||
           action_ == PlayerAction::DoorBlockedKick;
  }
  // emdoor.cpp::plemDoorOpen / plemDoorKick, original PL_ARC motions.
  // The room owner supplies the checked door alignment and starts its own
  // leaf motion. This action owns Leon until its original motion ends.
  void beginDoor(bool kick, bool blocked, const CollisionVec &alignment,
                 float heading, unsigned frame = 0);
  unsigned doorTicks() const { return doorTicks_; }
  bool pushActive() const {
    return action_ == PlayerAction::PushTakeHold || action_ == PlayerAction::Push ||
           action_ == PlayerAction::PushStopped || action_ == PlayerAction::PushRelease;
  }
  void beginPush(float heading);
  void stopPush();
  bool ladderActive() const {
    return action_ == PlayerAction::LadderUp || action_ == PlayerAction::LadderDown;
  }
  const LadderPlayback &ladderState() const { return ladderPlayback_; }
  const CollisionVec &ladderTravel(bool up) const { return ladderTravel_[up ? 0 : 1]; }
  // M28 fixed nine-level tower profile, independently replayed from real FCVs
  // for landing validation. Other levels remain outside this room adapter.
  void beginLadder(WalkProbe &probe, const CollisionVec &alignment,
                   float heading, int level);
  void installObjectLadderMotions(const std::vector<AnimationBytes> &clips);
  void beginObjectLadder(WalkProbe &probe, const CollisionVec &alignment,
                        float heading, unsigned rungs, unsigned type,
                        const CollisionVec &landing);
  void beginLadderProp(WalkProbe &probe, const CollisionVec &alignment,
                       float heading, unsigned motionIndex, bool flip,
                       bool reset);
  bool ladderPropActive() const {
    return action_ == PlayerAction::LadderTip || action_ == PlayerAction::LadderReset;
  }
  unsigned ladderPropTicks() const { return ladderPropTicks_; }
  bool objectLadderActive() const { return ladderActive() && objectLadder_; }
  const ObjectLadderPlayback &objectLadderState() const { return objectLadderPlayback_; }
  std::vector<LadderSound> drainLadderSounds();
  std::vector<PlayerSequenceSound> drainSequenceSounds();
  std::vector<PlayerWeaponSound> drainWeaponSounds();
  bool ladderLandingFailed() const { return ladderLandingFailed_; }
  bool jumpActive() const { return action_==PlayerAction::JumpDown || action_==PlayerAction::JumpLanding || jumpFallActive(); }
  bool jumpFallActive() const { return action_==PlayerAction::JumpFall || action_==PlayerAction::JumpFallLanding; }
  std::optional<JumpFallOffer> jumpFallOffer(const WalkProbe &probe) const;
  void beginJumpFall(WalkProbe &probe, const JumpFallOffer &offer);
  const JumpFallPlayback &jumpFallState() const { return jumpFallPlayback_; }
  void installWindowMotions(const std::vector<AnimationBytes> &clips);
  void beginWindow(WalkProbe &probe, const InteriorWindowOffer &offer);
  bool windowActive() const { return action_ == PlayerAction::WindowVault; }
  unsigned windowTicks() const { return windowTicks_; }
  unsigned windowSlot() const { return windowSlot_; }
  unsigned windowDirection() const { return windowDirection_; }
  std::optional<WindowCameraPose> windowCamera() const;
  std::optional<ActionCameraPose> actionCamera() const;
  // sceAtGetItem freezes the player and displays itemExam; it does not
  // substitute a made-up bend/pickup FCV. The inventory owner accepts or
  // cancels the transaction before endItemExam restores normal control.
  bool beginItemExam();
  void endItemExam(bool accepted);
  bool itemExamActive() const { return action_ == PlayerAction::ItemExam; }
  void installDogTrapMotions(const std::vector<AnimationBytes> &clips);
  bool dogTrapActive() const {
    return action_==PlayerAction::DogRescue || action_==PlayerAction::BearTrapBite;
  }
  void beginDogRescue(WalkProbe &probe,const CollisionVec &alignment,float heading);
  bool beginBearTrapBite(WalkProbe &probe,const CollisionVec &trap);
  void setBearTrapCameraOrigin(const CollisionVec &trap);
  // em2aTrap1CamMove blends from the owning room camera at 0.1 each 30-Hz tick.
  float actionCameraBlend() const { return action_==PlayerAction::BearTrapBite?.1f:1.f; }
  unsigned dogTrapTicks() const { return dogTrapTicks_; }
  unsigned installRoomScriptMotion(const AnimationBytes &clip);
  void beginRoomScriptMotion(WalkProbe &probe,const CollisionVec &alignment,
      float heading,unsigned clip,unsigned blendFrames=10,unsigned flags=1);
  void endRoomScriptMotion();
  bool roomScriptActive() const { return action_==PlayerAction::RoomScript; }
  bool roomScriptMotionEnded() const {
    const auto status=rig_->status();
    return roomScriptActive() && (status.ended || (status.state&3));
  }
  // cPlLeon::setLeftHand: radio type2 uses actual PL_ARC16/TPL11;
  // type63 restores oldLhandNo and updates old/now in the original order.
  void setRoomScriptLeftHand(unsigned type);
  unsigned leftHandType() const { return leftHandNow_; }
  void setRoomScriptEyeMode(unsigned mode);
  unsigned eyeMode() const { return eyeMode_; }
  const PlayerEyeState &eyeState() const { return eyeState_; }
  const JumpPlayback &jumpState() const { return jumpPlayback_; }
  std::optional<JumpDownOffer> jumpOffer(const WalkProbe &probe) const;
  void beginJump(WalkProbe &probe, const JumpDownOffer &offer,
                  const FollowCameraPose *view = nullptr);
  bool fenceActive() const { return action_ == PlayerAction::FenceVault; }
  bool traversalActive() const {
    return ladderActive() || ladderPropActive() || fenceActive() ||
        jumpActive() || windowActive();
  }
  std::optional<FenceOffer> fenceOffer(const WalkProbe &probe) const;
  void beginFence(WalkProbe &probe, const FenceOffer &offer);
  // cPlayer::actionSelect: broken windows with break direction0/1 use
  // fanceOn r3=5 (narrow PL_ARC55/56, no fanceAdjust), not ExeWindowEvent.
  std::optional<FenceOffer> brokenWindowOffer(const WalkProbe &probe,
      const CollisionVec &alignment, float heading) const;
  void beginBrokenWindow(WalkProbe &probe, const FenceOffer &offer);
  const FencePlayback &fenceState() const { return fencePlayback_; }
  bool grabbed() const {
    return action_ == PlayerAction::Grabbed ||
           action_ == PlayerAction::GrabRelease ||
           action_ == PlayerAction::GrabEscape ||
           action_ == PlayerAction::Recover;
  }
  bool alive() const { return life_ != 0; }
  bool blastActive() const {
    return action_ == PlayerAction::BlastBlow ||
           action_ == PlayerAction::BlastStagger ||
           action_ == PlayerAction::BlastRecover ||
           action_ == PlayerAction::BlastDead;
  }
  bool hurtActive() const { return action_==PlayerAction::Hurt || action_==PlayerAction::HurtDead; }
  // Selected original Em10AtkTbl[0]/EmAtkHitCk -> PlSetDamage -> damageNormal.
  // damageType0..5 is the original attack sweep's front/back/height result.
  // Unarmoured Leon; the caller supplies the source attack power.
  bool receiveGanadoAxeHit(const CollisionVec &attacker, unsigned damage,
                          unsigned damageType);
  void setDamageDifficulty(unsigned rank);
  unsigned damageDifficulty() const { return damageRank_; }
  CollisionVec parts0World() const;
  // cAtariInfo::getSpeedVector: part0 X/Z plus model-rotated offset0/-200/0;
  // for scenery, Y is deliberately actorY+radius400 (not part0 Y).
  CollisionVec sceneryBodyCenter() const;
  // Normal Leon, configured source rank: original PlBombHitCk life loss and
  // PL_DM8/9 authored motions. Root placement uses the existing SAT adapter;
  // original splash/dust, face and sound effects remain separate runtimes.
  bool applyBlast(const ObjectPlayerBlastHit &hit, const CollisionVec &center,
                  WalkProbe &probe);
  unsigned life() const { return life_; }
  bool restoreLife(unsigned value) { if(!value || value>MaxLife)return false;life_=value;return true; }
  // Development policy requested until the full campaign is playable. Keep
  // genuine hit/grab/blast reactions; only health loss and fatal HP are blocked.
  void setUnlimitedHealth(bool value) { unlimitedHealth_ = value; if (value) life_ = MaxLife; }
  bool unlimitedHealth() const { return unlimitedHealth_; }
  void setUnlimitedAmmo(bool value);
  bool unlimitedAmmo() const {return unlimitedAmmo_;}
  void setRenderInterpolation(bool value);
  CollisionVec renderFeet() const;
  float renderHeading() const;
  Matrix34 renderPlacement() const;
  const std::vector<Matrix34> &renderJointMatrices() const;
  static constexpr unsigned MaxLife = 1200;
  bool weaponVisible() const;
  unsigned damageWait() const { return damageWait_; }
  void holdBinoculars(bool held) {damageWait_=held?0x80:0;}
  void beginGrab(float turn, const std::array<double, 3> &adjustment);
  void receiveContinuousFlame();
  void beginChainsawGrab(float turn,const CollisionVec &adjustment);
  void endChainsawGrab(bool escaped);
  void releaseGrab(bool escaped);
  void finishGrab();
  void followCatch(const std::array<double, 3> &partnerDelta);
  unsigned takeResistPresses();
  void takeDamage(unsigned damage, bool keepOne);
  void setFatalLife();
  // Left-stick forward/back and turning; camera control remains independent.
  void step(WalkProbe &probe, double turn, double forward, bool runHeld,
            bool runPressed, double seconds);
  void step(WalkProbe &probe, double turn, double forward, bool runHeld,
            bool runPressed, double seconds, const PlayerActionInput &action,
            const PlayerSimulationTick &afterTick = {},
            const std::function<void()> &beforeTick = {});
  const StaticScene &mesh() const { return mesh_; }
  const std::vector<SkinCorner> &skin() const { return skin_; }
  const StaticScene &renderMesh() const { return renderMesh_; }
  const std::vector<SkinCorner> &renderSkin() const { return renderSkin_; }
  const AnimationRig &rig() const { return *rig_; }
  AnimationRig &rig() { return *rig_; }
  const std::array<double, 3> &feet() const { return feet_; }
  float heading() const { return heading_; }
  Locomotion state() const { return state_; }
  PlayerAction action() const { return action_; }
  bool weaponAttackImplemented() const;
  bool aiming() const {
    return action_ == PlayerAction::Draw || action_ == PlayerAction::Aim ||
           action_ == PlayerAction::Fire || action_ == PlayerAction::Reload;
  }
  float aimRate() const { return aimRate_; }
  float scopeZoom() const { return scopeZoom_; }
  bool usesScope() const;
  float waistYaw() const { return waistYaw_; }
  const WeaponSwayState &weaponSwayState() const { return weaponSway_; }
  std::uint16_t weaponRandomState() const { return weaponRandom_; }
  float weaponAimYaw() const { return waistTarget_; }
  float drawCameraYawRemaining() const { return drawCameraYawRemaining_; }
  unsigned magazine() const { return magazine_; }
  unsigned reserve() const { return reserve_; }
  unsigned shots() const { return shots_; }
  const WeaponPreview *weapon() const { return weapon_; }
  Matrix34 placement() const;
  std::array<double, 3> muzzle() const;
  std::array<double, 3> aimDirection() const;
  // Original handgun projectile ray from the solved recoil pose.
  // Real shots retain the source's two fRand1_1()*200 components.
  PlayerShot shotRay(bool spread = false) const;
  // cObjWep::getMarkerPos: weapon parts0, its own local pose and WEP01
  // marker {262,-24,32}; independent of the projectile origin/spread.
  PlayerShot laserRay() const;
  bool scopeReady() const;
  ActionCameraPose scopeCamera() const;
  bool laserReady() const;
  std::string describe() const;
  std::string motionInfo() const;

private:
  StaticScene mesh_;
  std::vector<SkinCorner> skin_;
  StaticScene renderMesh_;
  std::vector<SkinCorner> renderSkin_;
  std::unique_ptr<AnimationRig> rig_;
  std::array<std::unique_ptr<WeaponPreview>, 259> weapons_;
  WeaponPreview *weapon_{};
  unsigned weaponProfile_{};
  bool unarmed_{};
  std::array<unsigned,7> unarmedClips_{};
  unsigned unarmedKnifeHolster_{};
  std::array<std::array<unsigned, 2>, 259> weaponAmmo_{{{10,60},{10,20},{6,0}}};
  std::array<unsigned, 259> weaponPowerLevels_{};
  std::array<std::array<unsigned, 25>, 259> weaponClips_{};
  std::array<unsigned,7> injuredClips_{};
  bool injuredLocomotion_{};
  void syncLocomotion();
  void installWeapon(const std::string &weaponDrs, unsigned profile);
  bool prepareWeapon(unsigned itemId);
  static unsigned weaponSlot(unsigned id) { return id==0x21?0:id==0x23?1:id==0x2c?2:id<256?id+3:259; }
  std::string playerDrs_;
  unsigned leftHandNow_=4,leftHandOld_=4;
  unsigned eyeMode_{};
  PlayerEyeState eyeState_;
  void eyeTick();
  unsigned fireReadyTicks() const;
  bool hasRounds() const;
  unsigned activeClip(unsigned index) const { return unarmed_ && index<7?unarmedClips_[index]:unarmed_ && index==17?unarmedKnifeHolster_:weaponClips_[weaponProfile_][index]; }
  std::array<unsigned, 3> activeBlend(const std::array<unsigned, 3> &clips) const {
    return {activeClip(clips[0]),activeClip(clips[1]),activeClip(clips[2])};
  }
  std::array<double, 3> feet_{};
  float heading_ = 3.7415927f;
  Locomotion state_ = Locomotion::Idle;
  double motionAccumulator_{};
  bool quickTurnPending_{};
  bool quickTurning_{};
  unsigned runPressTicks_{};
  PlayerAction action_ = PlayerAction::Footwork;
  bool firePending_{}, reloadPending_{}, neutralGate_{}, reloadCommitted_{};
  // Original gun r2 fire -> r2 set/r3 4 retains the authored recoil motion.
  // It must not install an aim idle between a single shot and weapon down.
  bool fireRecoilTail_{}, fireAimPending_{};
  unsigned burstShots_{};
  bool throwReleased_{}, rifleCycling_{};
  PlayerAction queuedGunAction_{PlayerAction::Footwork};
  unsigned magazine_ = 10, reserve_ = 60, shots_{};
  bool unlimitedHealth_{}, unlimitedAmmo_{}, renderInterpolation_{};
  unsigned damageRank_{5};
  CollisionVec previousRenderFeet_{};
  float previousRenderHeading_{};
  bool previousRenderFootwork_{};
  bool previousRenderTraversal_{};
  std::vector<Matrix34> previousRenderJoints_;
  mutable std::vector<Matrix34> interpolatedRenderJoints_;
  AttachedMotionCamera previousRenderAttached_{};
  unsigned life_ = MaxLife, damageWait_{}, resistPending_{}, grabTicks_{};
  bool hasGrabMotions_{}, hasMeleeMotions_{}, chainsawGrab_{};
  std::array<unsigned,2> chainsawClips_{};
  unsigned continuousFlameTicks_{};
  std::array<unsigned, 4> blastClips_{};
  std::array<unsigned, 3> doorClips_{};
  std::array<unsigned, 6> ladderClips_{};
  std::array<unsigned, 2> jumpClips_{};
  std::array<unsigned, 2> jumpFallClips_{};
  std::vector<JumpSample> jumpFallPath_;
  CollisionVec jumpFallLandingTravel_{};
  JumpFallPlayback jumpFallPlayback_;
  CollisionVec jumpFallRollback_{};
  float jumpFallRollbackHeading_{};
  std::array<unsigned,5> hurtClips_{};
  std::array<unsigned,2> dogTrapClips_{};
  bool dogTrapMotions_{};
  unsigned dogTrapTicks_{};
  float trapCatchTurn_{};
  CollisionVec trapCameraOrigin_{};
  unsigned hurtTicks_{}, hurtType_{};
  bool hurtMotionPending_{};
  std::vector<PlayerSequenceSound> sequenceSounds_;
  std::vector<PlayerWeaponSound> weaponSounds_;
  std::uint64_t nativeTicks_{};
  void emitWeaponSound(unsigned block,unsigned number,unsigned part);
  ActionCameraPose jumpView_{};
  CollisionVec jumpViewLocalEye_{};
  const SatCollision *jumpCameraWorld_{};
  unsigned jumpCameraPart_{1};
  bool jumpCameraActive_{};
  std::array<unsigned, 2> fenceClips_{};
  std::array<std::vector<FenceSample>, 2> fencePaths_;
  FencePlayback fencePlayback_{};
  CollisionVec fenceRollbackFeet_{};
  float fenceRollbackHeading_{};
  std::array<unsigned, 4> pushClips_{};
  float pushHeading_{};
  std::vector<JumpSample> jumpPath_;
  std::vector<AnimationBytes> windowMotions_;
  std::vector<AnimationBytes> objectLadderMotions_;
  std::array<unsigned, 4> objectLadderClips_{};
  std::array<unsigned, 3> ladderPropClips_{};
  unsigned ladderPropTicks_{}, ladderPropClip_{};
  bool ladderPropReset_{}, ladderPropType_{};
  ObjectLadderPlayback objectLadderPlayback_{};
  bool objectLadder_{};
  CollisionVec objectLadderLanding_{};
  unsigned windowClipBase_{}, windowTicks_{}, windowSlot_{}, windowDirection_{};
  CollisionVec windowLanding_{};
  CollisionVec windowStart_{};
  float windowStartHeading_{};
  void windowTick(WalkProbe &probe);
  CollisionVec jumpLandingTravel_{}, jumpRollbackFeet_{};
  float jumpRollbackHeading_{};
  JumpPlayback jumpPlayback_;
  std::array<CollisionVec, 2> ladderTravel_{};
  LadderPlayback ladderPlayback_;
  CollisionVec ladderRollbackFeet_{};
  float ladderRollbackHeading_{};
  bool ladderLandingFailed_{};
  std::vector<LadderSound> ladderSounds_;
  CollisionVec doorAdjustment_{};
  float doorHeading_{};
  unsigned doorTicks_{};
  double blastAirHeight_{};
  bool bladeVisible_{}, knifeFinishing_{}, knifeToGun_{};
  bool knifeFirePending_{};
  unsigned meleeSerial_{}, kickTicks_{};
  CollisionVec previousBladeTip_{}, kickTarget_{};
  std::optional<PlayerMeleeEvent> meleeEvent_;
  float catchTurn_{};
  std::array<double, 3> catchAdjustment_{}, partnerDelta_{};
  float aimRate_{}, waistYaw_{}, waistTarget_{}, aimAcceleration_{};
  WeaponSwayState weaponSway_{};
  mutable std::uint16_t weaponRandom_{};
  float drawCameraYawRemaining_{};
  unsigned aimBlendTicks_{};
  unsigned fireTicks_{};
  bool returningFromReload_{};
  float scopeZoom_{};
  ScopeSwayState scopeSway_{};
  bool rifleReloadScope_{};
  std::optional<PlayerShot> scopeShot_;
  bool rocketDiscarding_{},rocketDiscarded_{};
  std::array<float,3> scopeEyeLocal_{};
  float weaponFrame_{};
  bool weaponNewFrame_{}, weaponEmptyVariant_{};
  void resetMotion();
  void beginAction(PlayerAction action);
  bool actionTick(const PlayerActionInput &input, bool movementHeld);
  void startFence(const WalkProbe &probe, const FenceOffer &offer);
  void aimTick(const PlayerActionInput &input);
  void weaponSway(unsigned flags);
  void weaponTick();
  void grabTick(WalkProbe &probe);
  void blastTick(WalkProbe &probe);
  void doorTick(WalkProbe &probe);
  void pushTick(WalkProbe &probe, bool forward);
  void ladderTick(WalkProbe &probe);
  void ladderPropTick(WalkProbe &probe);
  void fenceTick(WalkProbe &probe, bool movementHeld);
  void jumpTick(WalkProbe &probe);
  void jumpFallTick(WalkProbe &probe);
  void hurtTick(WalkProbe &probe, bool movementHeld);
  void dogTrapTick(WalkProbe &probe);
  void roomScriptTick(WalkProbe &probe);
  void emitSequenceSound();
  bool meleeTick(WalkProbe &probe, const PlayerActionInput &input, double turn,
                 double forward);
  void knifeHitCheck();
};
} // namespace re4
