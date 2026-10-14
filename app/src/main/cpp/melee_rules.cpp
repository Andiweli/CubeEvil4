#include "melee_rules.h"
#include "ports/motion64/runtime.h"
#include "ports/rnd.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace re4 {
namespace {
using motion64::Mtx;
using motion64::Vec;
constexpr float pi = 3.1415927f, pi2 = 6.2831855f;
float limitAngle(float angle) {
  // Real actor headings are already limited. Bound malformed external
  // inputs before the original repeated subtraction to avoid long loops.
  if (!std::isfinite(angle) || std::abs(angle) > 10000)
    throw std::runtime_error("Melee: invalid direction");
  while (angle >= pi)
    angle -= pi2;
  while (angle < -pi)
    angle += pi2;
  return angle;
}
void validVector(const HitVector &point) {
  for (float value : point)
    if (!std::isfinite(value) || std::abs(value) > 1e7f)
      throw std::runtime_error("Melee: invalid coordinate");
}
HitVector narrow(const CollisionVec &point) {
  HitVector result{};
  for (unsigned i = 0; i < 3; ++i) {
    if (!std::isfinite(point[i]) || std::abs(point[i]) > 1e7)
      throw std::runtime_error("Melee: invalid position");
    result[i] = float(point[i]);
  }
  return result;
}
void validMatrix(const Matrix34 &matrix) {
  for (float value : matrix)
    if (!std::isfinite(value) || std::abs(value) > 1e7f)
      throw std::runtime_error("Melee: invalid part matrix");
}
void validSize(float value) {
  if (!std::isfinite(value) || value < 0 || value > 1e6f)
    throw std::runtime_error("Melee: invalid extent");
}
Vec vector(const HitVector &v) { return {v[0], v[1], v[2]}; }
HitVector vector(const Vec &v) { return {v.x, v.y, v.z}; }
float distanceSq(const Vec &a, const Vec &b) {
  return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) +
         (a.z - b.z) * (a.z - b.z);
}
Matrix34 concatenate(const Matrix34 &a, const Matrix34 &b) {
  Mtx first, second, result;
  std::memcpy(first, a.data(), sizeof(first));
  std::memcpy(second, b.data(), sizeof(second));
  motion64::PSMTXConcat(first, second, result);
  Matrix34 matrix;
  std::memcpy(matrix.data(), result, sizeof(result));
  validMatrix(matrix);
  return matrix;
}
bool ignoresHandgun(unsigned weapon) {
  switch (weapon) {
  case 1: case 2: case 3: case 4: case 0x11: case 0x26: case 0x2b:
    return true;
  default:
    return false;
  }
}
// at_sub.cpp137 AtSphereCapsuleCk. This deliberately retains sampled
// spheres and strict '<' boundaries rather than an analytic capsule test.
bool sphereCapsule(Vec center, float sphereRadius, Vec top, Vec bottom,
                   float capsuleRadius) {
  const float rr = (sphereRadius + capsuleRadius) *
                   (sphereRadius + capsuleRadius);
  if (distanceSq(center, top) < rr || distanceSq(center, bottom) < rr)
    return true;
  Vec direction;
  motion64::PSVECSubtract(&top, &bottom, &direction);
  float length = std::sqrt(direction.x * direction.x +
                           direction.y * direction.y +
                           direction.z * direction.z);
  // All selected normal Ganado capsules have a positive radius. Reject
  // singular/unbounded transforms before the original float->u32 cast.
  if (capsuleRadius < .01f || !std::isfinite(length) ||
      length / (capsuleRadius + capsuleRadius) > 65534)
    throw std::runtime_error("Melee: invalid capsule sample budget");
  unsigned count = unsigned(length / (capsuleRadius + capsuleRadius)) + 2;
  length = length / float(count);
  if (length < .01f)
    return false;
  motion64::PSVECNormalize(&direction, &direction);
  motion64::PSVECScale(&direction, &direction, length);
  Vec sample = bottom;
  --count;
  while (count-- != 0) {
    motion64::PSVECAdd(&sample, &direction, &sample);
    if (distanceSq(center, sample) < rr)
      return true;
  }
  return false;
}
float yaw(const HitVector &from, const HitVector &to) {
  return limitAngle(std::atan2(to[0] - from[0], to[2] - from[2]));
}
float turnedHeading(float oldHeading, const HitVector &from,
                    const HitVector &to) {
  // Original Muku limitPI, then ang.y += result and LIMIT_ANGLE. Retain
  // the separate float subtraction/addition rather than assigning yaw.
  const float turn = limitAngle(yaw(from, to) - oldHeading);
  return limitAngle(oldHeading + turn);
}
} // namespace

bool KickPromptGeometry(const CollisionVec &playerFeet, float playerHeading,
                        const CollisionVec &enemyFeet) {
  const auto player = narrow(playerFeet), enemy = narrow(enemyFeet);
  playerHeading = limitAngle(playerHeading);
  const float dx = enemy[0] - player[0], dz = enemy[2] - player[2];
  // emMove computes l_pl on XZ; height is separately checked by the prompt.
  if (dx * dx + dz * dz > 2250000.0f ||
      std::abs(enemy[1] - player[1]) > 700.0f)
    return false;
  return std::abs(limitAngle(yaw(player, enemy) - playerHeading)) <=
         1.0471976f;
}

std::optional<MeleeSphereHit> SphereAuthoredShapes(
    const std::vector<AuthoredHitShape> &shapes,
    const std::vector<Matrix34> &rawJointMatrices,
    const Matrix34 &actorPlacement, const CollisionVec &sphereCenter,
    float radius, unsigned weaponNo, std::optional<float> verticalRadius) {
  if (shapes.size() > 256 || rawJointMatrices.size() > 248 || weaponNo > 0x2d)
    throw std::runtime_error("Melee: Data budget or weapon index");
  validSize(radius);
  if (verticalRadius)
    validSize(*verticalRadius);
  validMatrix(actorPlacement);
  for (const auto &matrix : rawJointMatrices)
    validMatrix(matrix);
  const Vec center = vector(narrow(sphereCenter));
  std::optional<MeleeSphereHit> closest;
  float best = 1e16f;
  for (std::size_t i = 0; i < shapes.size(); ++i) {
    const auto &shape = shapes[i];
    validVector(shape.offset);
    for (float size : {shape.radius, shape.height, shape.extent})
      validSize(size);
    if (shape.partsNo < 0 || std::size_t(shape.partsNo) > rawJointMatrices.size() ||
        (shape.flags & ~(HitOn | HitXAxis | HitZAxis | HitCube |
                         HitIgnoreHandguns | HitThrough | HitNoMark |
                         HitNoScenarioBomb | HitDamagePosition)))
      throw std::runtime_error("Melee: invalid hit part");
    if (!(shape.flags & HitOn))
      continue;
    if (shape.flags & HitCube)
      throw std::runtime_error("Melee: Only original Ganado capsules supported");
    if ((shape.flags & HitIgnoreHandguns) && ignoresHandgun(weaponNo))
      continue;
    const Matrix34 matrix = shape.partsNo == 0
                                ? actorPlacement
                                : concatenate(actorPlacement,
                                    rawJointMatrices[shape.partsNo - 1]);
    Mtx m;
    std::memcpy(m, matrix.data(), sizeof(m));
    Vec bottom = vector(shape.offset), top = bottom;
    if (shape.flags & HitXAxis)
      top.x += shape.height;
    else if (shape.flags & HitZAxis)
      top.z += shape.height;
    else
      top.y += shape.height;
    motion64::PSMTXMultVec(m, &top, &top);
    motion64::PSMTXMultVec(m, &bottom, &bottom);
    validVector(vector(top));
    validVector(vector(bottom));
    Vec scaledRadius{0, 0, shape.radius};
    motion64::PSMTXMultVecSR(m, &scaledRadius, &scaledRadius);
    const float capRadius = motion64::PSVECMag(&scaledRadius);
    validSize(capRadius);
    if (!sphereCapsule(center, radius, top, bottom, capRadius))
      continue;
    Vec capsuleCenter;
    motion64::PSVECAdd(&top, &bottom, &capsuleCenter);
    motion64::PSVECScale(&capsuleCenter, &capsuleCenter, .5f);
    // emSphereAtCk's r2 gate deliberately uses the authored, unscaled
    // part radius; GetWepTargetListBomb narrows barrel-blast height to2000.
    if (verticalRadius && *verticalRadius != radius &&
        std::abs(capsuleCenter.y - center.y) >
            *verticalRadius + shape.radius)
      continue;
    const float lenSq = distanceSq(center, capsuleCenter);
    // Directionless emSphereAtCk uses >=: equal centers retain FIRST part.
    if (lenSq >= best)
      continue;
    best = lenSq;
    closest = MeleeSphereHit{i, lenSq, vector(capsuleCenter),
        std::uint16_t(shape.flags & ~HitDamagePosition)};
  }
  return closest;
}
bool MeleeSphereNeedsScenarioCheck(const MeleeSphereHit &hit, float radius) {
  validSize(radius);
  validVector(hit.center);
  if (!std::isfinite(hit.lenSq) || hit.lenSq < 0 || hit.lenSq > 1e16f)
    throw std::runtime_error("Melee: invalid hit distance");
  const float limit = radius * .3f;
  return !(hit.flags & HitNoScenarioBomb) && hit.lenSq > limit * limit;
}
unsigned KnifeDamage(bool head) { return head ? 120 : 100; }
unsigned KickDamage(bool head) { return head ? 240 : 200; }
bool KnifeBodyFlinches() { return Rnd() % 3 != 0; }

MeleeBlowSelection SelectMeleeBlow(const CollisionVec &enemyFeet,
                                 float enemyHeading,
                                 const CollisionVec &hitFrom, bool alive,
                                 bool parasite) {
  const auto enemy = narrow(enemyFeet), from = narrow(hitFrom);
  enemyHeading = limitAngle(enemyHeading);
  // Preserve both unconditional entry bytes before any branch. The first
  // also controls death-low TmpF=2 when the original mirror flag is1.
  const bool sourceRno3 = (Rnd() & 1) != 0;
  const bool highRoll = Rnd() % 10 > 6;
  const float angle = std::abs(limitAngle(yaw(enemy, from) - enemyHeading));
  MeleeBlowSelection result;
  result.mirrored = !sourceRno3;
  result.flags = result.mirrored ? 0x41 : 1;
  result.rootScale = 1;
  if (highRoll || alive) {
    const bool variant = (Rnd() & 1) != 0;
    if (angle < 1.5707964f) {
      result.motionEntry = variant || parasite ? 63 : 59;
      result.downFacing = !(variant || parasite);
      result.heading = turnedHeading(enemyHeading, enemy, from);
      result.fallSpeed = {0, -100, -100};
    } else {
      result.motionEntry = variant ? 71 : 73;
      result.downFacing = variant;
      result.heading = turnedHeading(enemyHeading, from, enemy);
      result.fallSpeed = {0, -100, 100};
    }
  } else {
    // Original && short-circuit: dead back hits skip this third byte.
    const bool deathHigh = angle < 1.5707964f && Rnd() % 10 > 6;
    result.motionEntry = deathHigh ? 61 : 55;
    result.downFacing = !deathHigh;
    result.rootScale = !deathHigh && sourceRno3 ? 2 : 1;
    result.heading = turnedHeading(enemyHeading, enemy, from);
    result.fallSpeed = {0, -200, -100};
  }
  result.sequenceEntry = result.motionEntry + 1;
  return result;
}
} // namespace re4
