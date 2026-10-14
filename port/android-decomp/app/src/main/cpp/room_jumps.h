#pragma once
#include "animation.h"
#include "walk_probe.h"
namespace re4 {
struct JumpSample { RootMotionStep root; float frame{}, pelvisY{}; };
struct JumpDownOffer {
  SatHit wall;
  CollisionVec alignment{}, landing{};
  float heading{};
};
struct JumpPlayback {
  CollisionVec base{}, feet{};
  float heading{}, targetHeading{};
  unsigned adjustment{10}, ticks{}, phase{}; // 0 fall, 1 landing, 2 complete.
  bool failed{};
};
// Original actWallCheck: normal-aligned second ray and +/-350 shoulder rays.
// Only source attributes 0x100010 enable jumping. The immutable FCV replay
// then proves a real lower floor/capsule exit; no synthetic ground is used.
std::optional<JumpDownOffer> FindJumpDown(const SatCollision &world,
    const WalkProbe &probe, float heading, const std::vector<JumpSample> &path,
    const CollisionVec &landingTravel);
JumpPlayback StartJumpPlacement(const CollisionVec &feet, float heading,
                                const JumpDownOffer &offer);
void AdvanceJumpPlacement(JumpPlayback &state, const RootMotionStep &root,
                          float frame);
struct JumpFallOffer {
  SatHit wall;
  CollisionVec direction{}, landing{};
  float heading{}, heightAdjustment{};
};
struct JumpFallPlayback {
  CollisionVec feet{}, previousPelvis{};
  float heading{}, heightAdjustment{};
  unsigned ticks{}, phase{}; // 0 jump, 1 landing, 2 complete.
  bool failed{};
};
// cPlayer::jumpCheck: both +/-300 shoulder rays, length600, common SAT
// bit0x80000. Its height comes from the original floor probe3800 beyond.
// The authored PL5e/60 replay also proves the post-landing scenery exit.
std::optional<JumpFallOffer> FindJumpFall(const SatCollision &world,
    const WalkProbe &probe, float heading, const std::vector<JumpSample> &path,
    const CollisionVec &landingTravel);
struct FenceSample { RootMotionStep root; float frame{}; };
struct FenceOffer {
  SatHit wall;
  CollisionVec alignment{}, landing{};
  float heading{};
  bool wide{};
  bool adjustSides{true}; // r3 bit0: broken-window fanceOn disables fanceAdjust.
};
struct FencePlayback {
  CollisionVec base{}, feet{};
  float heading{}, targetHeading{};
  unsigned adjustment{10}, ticks{};
  bool adjustSides{true};
};
// Source actWallCheck + fanceCheck + fanceWidthCheck. Only SAT bit0x20
// enables this action, with clear shoulders and a real +/-300-unit floor.
std::optional<FenceOffer> FindFence(const SatCollision &world,
    const WalkProbe &probe, float heading, const CollisionVec &parts0World,
    const std::array<std::vector<FenceSample>, 2> &paths);
FencePlayback StartFencePlacement(const CollisionVec &feet, float heading,
                                  const FenceOffer &offer);
void AdvanceFencePlacement(const SatCollision &world, FencePlayback &state,
                           const RootMotionStep &root, float frame);
} // namespace re4
