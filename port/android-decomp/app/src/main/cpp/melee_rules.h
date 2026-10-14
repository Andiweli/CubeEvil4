#pragma once
#include "hit_geometry.h"
#include <cstdint>
#include <optional>

namespace re4 {
// em10ActEvtSetKick: XZ range and separate height/facing limits. The caller
// checks living/stagger/player state and the SAT ray at height +1500.
bool KickPromptGeometry(const CollisionVec &playerFeet, float playerHeading,
                        const CollisionVec &enemyFeet);

struct MeleeSphereHit {
  std::size_t partIndex{};
  float lenSq{};
  HitVector center{};
  std::uint16_t flags{};
};
// Selected directionless emSphereAtCk/GetWepTargetListBomb branch. Uses the
// original sampled sphere/capsule check and first-nearest center ordering.
// The normal Ganado has capsules only; enabled cube shapes are rejected.
// Optional r2 retains source blast height clipping with the unscaled authored
// capsule radius; omitted means the existing spherical kick branch.
std::optional<MeleeSphereHit> SphereAuthoredShapes(
    const std::vector<AuthoredHitShape> &shapes,
    const std::vector<Matrix34> &rawJointMatrices,
    const Matrix34 &actorPlacement, const CollisionVec &center,
    float radius = 1200, unsigned weaponNo = 0x14,
    std::optional<float> verticalRadius = {});
// The caller casts the EAT ray from sphere center to selected capsule center.
bool MeleeSphereNeedsScenarioCheck(const MeleeSphereHit &hit,
                                  float radius = 1200);

// Normal Leon, village Ganado, no armour/parasite, rank5. These functions do
// not consume RNG; LifeDownSet2's two bytes remain caller-owned.
unsigned KnifeDamage(bool head);
unsigned KickDamage(bool head);
// Call only for a living ordinary body knife hit, AFTER LifeDownSet2.
// Head hits take the earlier Dm_Head path without this random byte.
bool KnifeBodyFlinches();

struct MeleeBlowSelection {
  unsigned motionEntry{}, sequenceEntry{}, blendFrames{3};
  std::uint16_t flags{};
  bool mirrored{};
  // Literal original Be_flg0x20; true selects raw105/106 wake, false107/108.
  bool downFacing{};
  float heading{}, rootScale{1};
  CollisionVec fallSpeed{};
};
// em10_R1_Dm_Blow entry selection, including living/dead and front/back RNG
// branches. Uses caller-owned Rnd stream, with the original byte order.
MeleeBlowSelection SelectMeleeBlow(const CollisionVec &enemyFeet,
                                 float enemyHeading,
                                 const CollisionVec &hitFrom, bool alive,
                                 bool parasite = false);
} // namespace re4
