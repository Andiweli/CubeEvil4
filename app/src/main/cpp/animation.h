#pragma once
#include "model_skin.h"
#include <memory>
#include <optional>
namespace re4 {
// The first three values retain the M16 preview/reference clip indices.
enum class Locomotion { Idle, Walk, Run, Back, TurnLeft, TurnRight, QuickTurn };
const char *LocomotionLabel(Locomotion state);
struct RootMotionStep {
  std::array<double, 3> translation{};
  double yaw{};
  bool ended{};
};
struct MotionCycle {
  std::array<double, 3> translation{};
  double yaw{};
  unsigned sequenceFrames{};
};
struct AnimationStatus {
  float curveFrame{};
  float sequenceFrame{};
  std::uint16_t state{}; // Original Mot_state: 1/2 loop, 4/8 ended.
  bool ended{};
};
struct SequenceSound { std::uint8_t se{}, free{}; };
struct AnimationBytes {
  std::vector<std::uint8_t> motion, sequence;
  // Weapon motions run without pAttachCam. Some original weapon FCVs
  // alias unused camera keys with a different skeletal key format.
  bool attachCameraCurves{true};
};
struct AttachedMotionCamera {
  std::array<float, 3> eye{}, target{};
  float roll{}, fovRadians{};
  unsigned type{};
  bool hasEye{}, hasTarget{}, hasRoll{}, hasFov{};
};
// Presentation between two already solved 30-Hz poses. Uses the original
// quaternion conversion/Slerp math and preserves per-axis scale. It never
// evaluates FCV/SEQ, updates Hokan history or changes simulation clocks.
Matrix34 InterpolateMotionMatrix(const Matrix34 &previous,
                                 const Matrix34 &current, double fraction);
// Original MotionMove, IK and quaternion blend, hosted on native 64-bit types.
// Single owning runtime thread; the scene preview supplies placement/collision.
class AnimationRig {
public:
  // cModel::modelInit's authored rest hierarchy, without a fabricated FCV.
  explicit AnimationRig(const SkinnedModel &body);
  AnimationRig(const SkinnedModel &body,
               const std::array<AnimationBytes, 3> &clips);
  // One to 1024 owned FCVs; SEQ is optional, including weapon rigs.
  AnimationRig(const SkinnedModel &body,
               const std::vector<AnimationBytes> &clips);
  ~AnimationRig();
  AnimationRig(const AnimationRig &) = delete;
  AnimationRig &operator=(const AnimationRig &) = delete;
  void reset();
  // Event::MotClear/ExePacket_Mot: MotionClear(model,1) restores the
  // bind positions and zero angles while retaining callback part scales.
  void clearEventMotion(bool rebuildPose = true);
  void update(Locomotion state, double seconds);
  // One original 30-Hz evaluation, returning model-space root displacement.
  // The caller applies collision and placement; bones remain model-local.
  RootMotionStep tick(Locomotion state, double playbackRate = 1);
  // Stationary actions retain original MotionMove, Hokan and event timing.
  // Clips may omit SEQ; the original linear FCV frame sequence is then used.
  void startAction(unsigned clip, unsigned blendFrames, unsigned frame = 0,
                   bool loop = false);
  // Accept the original action flags (e.g. holster=5, reload=1, fire=4),
  // suppressing only root placement flag 1; the caller owns actor placement.
  void startActionFlags(unsigned clip, unsigned blendFrames, unsigned frame,
                        std::uint16_t sourceFlags);
  AnimationStatus advanceAction(double playbackRate = 1);
  // One original MotionGetSpeed + MotionMove action tick. Apply this root
  // delta externally once; MotionMove's own placement flag remains disabled.
  RootMotionStep advanceRootAction(double playbackRate = 1,
                                   bool rebuildWorld = false);
  // cEm10::Dm_Blow extracts/adds root explicitly, then MotionMove extracts
  // it again. Hokan's second extraction updates history and can be nonzero.
  // Both additions use the unchanged actor matrix; return their local sum
  // for one external placement, plus the source kick's
  // temporary root-part height stretch and subsequent partsWorldCalc.
  RootMotionStep advanceBlowAction(double playbackRate = 1,
                                  double firstTranslationScale = 1,
                                  bool stretchRoot = true);
  // cMotBase extracts speed after MotionMove while its first ten ticks own
  // placement. This mutates the original speed history without another pose.
  RootMotionStep rootMotionSpeed();
  AnimationStatus status() const;
  float curveFrame() const;
  bool crossedFrame(float frame) const;
  std::uint8_t sequenceFree() const;
  // cPlayer::seqSeCtrl consumes Se after the one original motion tick.
  std::optional<SequenceSound> consumeSequenceSound();
  // Owned original left/right mapping; 0xffff keeps an individual part.
  // Empty disables mirroring. Entries must cover exactly the owner's parts.
  void setFlipTable(const std::vector<std::uint16_t> &parts);
  // Original cMot3 order: base m0, positive-side m1, negative-side m2.
  void startBlend3(const std::array<unsigned, 3> &clips, float rate,
                   unsigned blendFrames, unsigned frame = 0, bool loop = true,
                   bool additive = false);
  void setBlend3Rate(float rate);
  // Rate used by the last solved pose, before a later cMot3 control update.
  float posedBlend3Rate() const;
  // Explicit curve-frame blend sampling for independent reference validation.
  void sampleBlend3(const std::array<unsigned, 3> &clips, float rate,
                    float frame);
  // Original cPlBody half/half/counterrotation; caller owns waist smoothing.
  // Zero removes the twist. It requires the player's spine/hip parts 1..3.
  // Refreshes raw attachment matrices without another motion/Hokan evaluation.
  void setWaistYaw(float radians);
  // Original event callbacks write pList[part].scale. Rebuild the existing
  // local/world hierarchy without consuming a motion frame or Hokan history.
  void setPartScale(unsigned part, const std::array<float, 3> &scale);
  std::array<float, 3> partScale(unsigned part) const;
  // EM2A stretches wire endpoints by writing cParts::pos, while retaining
  // the original inverse bind matrices and the mesh's authored coordinates.
  void setPartPosition(unsigned part, const std::array<float, 3> &position);
  std::array<float, 3> partPosition(unsigned part) const;
  void setPartRotation(unsigned part,const std::array<float,3> &rotation);
  std::array<float,3> partRotation(unsigned part) const;
  float sequenceFrame() const;
  MotionCycle cycle(unsigned clip) const;
  // Owned event clips can be appended without changing active MOTION_INFO,
  // solved bones, blend history or existing clip addresses.
  unsigned appendActions(const std::vector<AnimationBytes> &clips);
  unsigned clipCount() const;
  // Opt in for event owners with original attach-camera FCV channels.
  void enableAttachedCamera(bool enabled);
  AttachedMotionCamera attachedCamera() const;
  void setLocomotionClips(const std::array<unsigned, 7> &clips);
  const std::vector<Matrix34> &bones() const;
  // Raw posed parts matrices in the owner's model space, before inverse bind.
  // Hand attachment uses jointMatrices()[10], not a skinning palette matrix.
  const std::vector<Matrix34> &jointMatrices() const;
  const std::vector<std::array<float, 3>> &bindCenters() const;
  // Explicit curve-frame sampling for reference validation (without SEQ).
  void sample(unsigned clip, float frame);
  float maxFrame(unsigned clip) const;

private:
  AnimationRig(const SkinnedModel &body,
               const std::vector<AnimationBytes> &clips, bool staticPose);
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace re4
