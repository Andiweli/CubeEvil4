#pragma once
#include "leon_preview.h"
#include "weapon_action_source.h"

namespace re4 {
class GenericEventActor;
enum class WeaponMotion { Idle, Fire, Reload, RifleReloadDown, RifleBoltCycle };
enum class WeaponKind { HandgunWep01, HandgunWep02, ShotgunWep07, OriginalCarry };
// Original model parenting: actor placement * raw posed right-hand parts mat.
// The weapon's own inverse bind is applied independently during skinning.
Matrix34 WeaponAttachmentMatrix(const Matrix34 &hand,
                                const Matrix34 &actorPlacement);

// Original WEP01 model and its private FCV skeleton. Gameplay supplies the
// action frame and magazine variant; inventory, hit tests and effects are not
// implemented by this asset/pose owner.
class WeaponPreview {
public:
  explicit WeaponPreview(const std::string &weaponDrs,
                         WeaponKind kind = WeaponKind::HandgunWep01);
  WeaponPreview(const std::string &weaponDrs,unsigned itemId,const std::string &playerDrs);
  ~WeaponPreview();
  WeaponKind kind() const { return kind_; }
  unsigned itemId() const { return itemId_; }
  unsigned capacity() const;
  unsigned parentPart() const { return parentPart_; }
  unsigned number() const { return OriginalWeaponNumber(itemId_); }
  const WeaponActionSource &actions() const { return *OriginalWeaponAction(number()); }
  WeaponFamily family() const { return actions().family; }
  bool shotgun() const { return family()==WeaponFamily::Shotgun; }
  bool infinite() const { return itemId_==0x34 || itemId_==0x6d; }
  bool hasMuzzleEffects() const {
    return family()==WeaponFamily::Handgun || family()==WeaponFamily::Shotgun ||
        family()==WeaponFamily::Machine || family()==WeaponFamily::Matilda ||
        number()==9 || number()==13 || itemId_==0x36;
  }
  bool attackImplemented() const { return OriginalWeaponAction(number())!=nullptr; }
  const std::string &sourcePath() const { return sourcePath_; }
  // PlHandgunMove returns to the set state at PlShotFrameTbl[1/2][0]-2:
  // both untuned handgun profiles use 14-2, not the raw WEP02 table value.
  // PlShotgunMove uses its own authored recoil/pump threshold of frame40.
  unsigned fireReadyTicks() const { return actions().fireReady; }
  unsigned reloadCommitFrame() const { return actions().reloadCommit; }
  std::array<float, 3> markerOffset() const;
  const std::vector<std::uint8_t> &effects() const { return effects_; }
  void sample(WeaponMotion motion, float frame, bool empty = false);
  void sampleCarry(PlayerAction action,float frame,bool hasRounds=true);
  Matrix34 attachment(const std::vector<Matrix34> &playerParts,
      PlayerAction action,float frame,bool spare=false) const;
  std::vector<Matrix34> posedParts(const std::vector<Matrix34> &playerParts,
      PlayerAction action,float frame,bool spare=false) const;
  const GenericEventActor *secondary() const {return secondary_.get();}
  float maxFrame(WeaponMotion motion, bool empty = false) const;
  const StaticScene &mesh() const { return mesh_; }
  const std::vector<SkinCorner> &skin() const { return skin_; }
  const AnimationRig &rig() const { return *rig_; }
  AnimationRig &rig() { return *rig_; }
  WeaponMotion motion() const { return motion_; }
  float frame() const { return frame_; }
  bool empty() const { return empty_; }
  std::string describe() const;

private:
  static unsigned clipIndex(WeaponMotion motion, bool empty);
  StaticScene mesh_;
  std::vector<SkinCorner> skin_;
  std::unique_ptr<AnimationRig> rig_;
  WeaponKind kind_;
  unsigned itemId_{},parentPart_=10;
  std::string sourcePath_;
  std::vector<std::uint8_t> effects_;
  WeaponMotion motion_ = WeaponMotion::Idle;
  float frame_{};
  bool empty_{};
  bool staticPose_{};
  std::array<unsigned,3> carryClips_{}; // Draw, aim, down/back.
  std::vector<unsigned> parents_;
  std::unique_ptr<GenericEventActor> secondary_;
};
} // namespace re4
