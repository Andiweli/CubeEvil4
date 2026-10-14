#pragma once
#include "leon_preview.h"
#include "enemy_spawn.h"
#include <memory>
namespace re4 {
// The first five values retain the M19 fixture indices. DownIdle is a logical
// state: original DownWakeWait/Die_Cramp keep the ended fall motion.
enum class EnemyMotion {
  Idle, Walk, HitHead, HitBody, Death,
  Catch, NeckHang, ReleaseFailed, ThrowOff, Wake, DownIdle,
  BlowFrontFaceUp, BlowFrontFaceDown, BlowBackFaceUp, BlowBackFaceDown,
  BlowDeathFaceUp, BlowDeathFaceDown, WakeFaceDown,
  HeadLeft, HeadRight,
  DownHitFaceUp, DownHitFaceDown, DownDeathFaceUp, DownDeathFaceDown,
  BodyAlt, BodyRear, AxeAttack, HoeAttack, HoeAttackHigh, HoeAttackLow,
  RoomEvent,
  HouseTurn,HouseWalk,
  Find,Turn180,Turn180Alternate,
  FlashStart,FlashRecover,FlashFinish,
  Dash,
  RoofBack,RoofFront,RoofLand,
  BlowAirUp,BlowAirDown,BlowLandUp,BlowLandDown,
  DownWakeWait = DownIdle, WakeFaceUp = Wake
};
const char *EnemyMotionLabel(EnemyMotion motion);
struct EnemyMotionSource {
  unsigned motionEntry{}, sequenceEntry{}, blendFrames{};
  std::uint16_t flags{};
};
struct GanadoAppearance {
  unsigned type{}, weaponType{};
  std::uint32_t flags{};
  unsigned enemyId=0x12;
};
// Em12Set / em10WeaponInit: exact original village types and held tools.
GanadoAppearance GanadoAppearanceForSpawn(const EnemySpawn &spawn);
// The original EM12 village types0/1/3/4, hands, axe/sickle/hoe and cEm10 motions.
// The caller owns encounter state, HP, placement, SAT movement and target AI.
class EnemyPreview {
public:
  // EmSetFromList2 uses R100's original ESL global list index 0x25.
  explicit EnemyPreview(const std::string &enemyDrs, unsigned emsetNo = 0x25,
                        bool melee = false, bool leftHandedAxe = false);
  EnemyPreview(const std::string &enemyDrs, const EnemySpawn &spawn,
                bool melee = true);
  ~EnemyPreview();
  EnemyPreview(const EnemyPreview &) = delete;
  EnemyPreview &operator=(const EnemyPreview &) = delete;
  // Root factor 0 selects the original default (death face-up/no mirror=2,
  // other falls=1). A source Dm_Blow selection may supply its TmpF explicitly.
  void start(EnemyMotion motion, bool mirrored = false, double rootScale = 0,
             std::optional<unsigned> blendFrames = {}, unsigned startFrame = 0);
  RootMotionStep advance(double playbackRate = 1);
  void sample(EnemyMotion motion, float frame);
  // r100Cliff's room ARC2e/2f/30, selected by setEvtMotion(...,0,0,0).
  // This appends one no-SEQ action without changing the normal EM12 indices.
  void installRoomEventMotion(const std::vector<std::uint8_t> &motion);
  void prepareHouseScareMotions(unsigned walkType);
  // em10InitRtnSet Walk_type and em10SetWalkMotion's found armed kind3.
  void setOriginalWalking(unsigned walkType,bool found);
  std::optional<unsigned> roomEventAction() const { return roomEventAction_; }
  float opacity() const { return opacity_; }
  void setOpacity(float opacity);
  static EnemyMotionSource source(EnemyMotion motion, unsigned emsetNo = 0x25);
  EnemyMotion motion() const { return motion_; }
  bool mirrored() const { return mirrored_; }
  bool hasMeleeMotions() const { return melee_; }
  bool holdsAxe() const { return axe_ && !weaponDropped_; }
  bool holdsWeapon() const { return appearance_.weaponType && !weaponDropped_; }
  unsigned type() const { return appearance_.type; }
  unsigned weaponType() const { return appearance_.weaponType; }
  unsigned weaponBinEntry() const;
  unsigned weaponParentPart() const { return appearance_.flags&0x01000000u?16:10; }
  EnemyMotionSource motionSource(EnemyMotion motion) const;
  void startChainsawCatch(unsigned phase);
  void dropWeapon() { weaponDropped_ = true; }
  void restoreWeapon() { weaponDropped_ = false; }
  // em10WeaponSet's mirrored hand16 / local offsets, before actor placement.
  Matrix34 weaponMatrix() const;
  bool openHands() const;
  bool visiblePart(unsigned sourceBinEntry) const;
  // The M19 diagnostic mesh keeps relaxed hands and its original counts.
  const StaticScene &mesh() const { return mesh_; }
  const std::vector<SkinCorner> &skin() const { return skin_; }
  // One GPU owner contains both original hand variants; SceneDraw.work is
  // the source BIN entry. Filter using visiblePart without another upload.
  const StaticScene &renderMesh() const { return renderMesh_; }
  const std::vector<SkinCorner> &renderSkin() const { return renderSkin_; }
  AnimationRig &rig() { return *rig_; }
  const AnimationRig &rig() const { return *rig_; }
  std::string describe() const;

private:
  EnemyPreview(const std::string &enemyDrs, unsigned emsetNo, bool melee,
                GanadoAppearance appearance);
  StaticScene mesh_;
  std::vector<SkinCorner> skin_;
  StaticScene renderMesh_;
  std::vector<SkinCorner> renderSkin_;
  std::unique_ptr<AnimationRig> rig_;
  EnemyMotion motion_{EnemyMotion::Idle};
  bool mirrored_{};
  unsigned emsetNo_{};
  double blowRootScale_ = 1;
  bool melee_{};
  bool blowDownPose_{};
  bool axe_{}, weaponDropped_{};
  GanadoAppearance appearance_{};
  std::optional<unsigned> roomEventAction_;
  std::array<unsigned,3> chainsawActions_{};
  std::optional<unsigned> houseTurnAction_,houseWalkAction_;
  AnimationBytes houseTurnSource_,houseAltWalkSource_;
  unsigned houseWalkType_{};
  std::array<AnimationBytes,3> ordinaryWalkSources_;
  std::array<std::optional<unsigned>,3> ordinaryWalkActions_;
  unsigned walkingClip() const;
  std::array<AnimationBytes,3> discoverySources_;
  std::array<std::optional<unsigned>,3> discoveryActions_;
  std::array<unsigned,3> flashActions_{};
  AnimationBytes dashSource_;
  std::optional<unsigned> dashAction_;
  std::array<AnimationBytes,7> roofSources_;
  std::array<std::optional<unsigned>,7> roofActions_;
  unsigned ordinaryWalkType_{};
  bool originalWalking_{},originalFound_{};
  float opacity_=1;
};
} // namespace re4
