#pragma once
#include "collision_query.h"
#include "scene.h"
#include <optional>

namespace re4 {
using HitVector = std::array<float, 3>;
enum AuthoredHitFlag : std::uint16_t {
  HitOn = 1, HitXAxis = 2, HitZAxis = 4, HitCube = 8,
  HitIgnoreHandguns = 16, HitThrough = 32, HitNoMark = 64,
  HitNoScenarioBomb = 128, HitDamagePosition = 16384
};
// Original YARARE_INFO geometry. partsNo is 1-based; zero uses the model.
// For capsules height extends from offset along Y, or flagged X/Z.
// For cubes radius/extent are half-width/depth, height extends from offset Y.
struct AuthoredHitShape {
  HitVector offset{};
  float radius{}, height{}, extent{};
  std::uint16_t flags{};
  std::int16_t partsNo{};
};
struct AuthoredHitResult {
  std::size_t partIndex{};
  float lenSq{};
  HitVector cross{};
};
// Literal float ports of em_sub.cpp/sub2.cpp; strict source boundaries and
// ordered capsule caps/cube faces are retained rather than replaced by a
// generic physics primitive. Invalid/unbounded inputs throw.
std::optional<HitVector> HitLineCapsule(const HitVector &from,
                                       const HitVector &to,
                                       const HitVector &top,
                                       const HitVector &bottom, float radius);
std::optional<HitVector> HitLineCube(const HitVector &from, const HitVector &to,
                                    const Matrix34 &matrix,
                                    const HitVector &offset, float width,
                                    float height, float extent);
std::optional<AuthoredHitResult> HitAuthoredShapes(
    const std::vector<AuthoredHitShape> &shapes,
    const std::vector<Matrix34> &rawJointMatrices,
    const Matrix34 &actorPlacement, const CollisionVec &from,
    const CollisionVec &to, float maxLenSq, unsigned weaponNo = 1);
// em10_R0_Init's ordinary, non-chainsaw/non-parasite Ganado hit set. The
// final source record is present but disabled. Armour changes damage rules,
// not these basic posed capsules.
std::vector<AuthoredHitShape> NormalGanadoHitShapes();
} // namespace re4
