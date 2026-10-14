#include "grab_rules.h"
#include "input.h"
#include "ports/rnd.h"
#include <cmath>
#include <stdexcept>

namespace re4 {
namespace {
constexpr float pi = 3.1415927f, pi2 = 6.2831855f;
float limitAngle(float angle) {
  angle = std::fmod(angle, pi2);
  if (angle >= pi)
    angle -= pi2;
  if (angle < -pi)
    angle += pi2;
  return angle;
}
// Original sub2.cpp Muku2, full PI turn limit. Callers pass limited angles.
float turnTo(float from, float to) {
  float delta = to - from;
  if (delta < 0)
    delta += pi2;
  if (delta < pi)
    return delta;
  return delta - pi2;
}
void validPoint(const CollisionVec &point) {
  for (auto value : point)
    if (!std::isfinite(value) || std::abs(value) > 1e8)
      throw std::runtime_error("Grab: invalid position");
}
} // namespace

unsigned GrabPressCount(std::uint64_t pressed) {
  // pl_sub.cpp PlGachaMove: 0xF, 0xC0000000, 0x0C000000.
  // pad.cpp Key_type_tbl[0] duplicates main left/right in bits26/27;
  // the third mask is not a right-stick mask despite the source comment.
  unsigned count = 0;
  if (pressed & (MenuUp | MenuDown | MenuLeft | MenuRight))
    ++count;
  if (pressed & (Confirm | Interact | Cancel | Run))
    ++count;
  if (pressed & (MenuLeft | MenuRight))
    ++count;
  return count;
}
unsigned GrabPlayerLifeDown(unsigned life, unsigned damage, bool keepOne,
                            unsigned rank) {
  // The original life is signed16 and damage signed32. Limit this selected
  // branch to its real nonnegative life/damage domain before conversions.
  if (life > 32767 || damage > 32767 || rank > 10)
    throw std::runtime_error("Grab: invalid health value");
  const auto high = Rnd();
  const auto low = Rnd();
  (void)high;
  (void)low; // LifeDownSet2 consumes both even when random spread is zero.
  if (!life)
    return 0;
  // Same float operation order and truncation as original LifeDownSet2.
  const float rate=float(rank)*.1f+.5f;
  damage=unsigned(int(float(damage)*rate));
  if(rank<=2 && life>300) keepOne=true;
  // Rank5 scaling is exactly1, no armour. Survival consumes its random
  // byte even when the caller already requested the keep-one flag.
  if (life > 300 && (Rnd() & 3) == 0)
    keepOne = true;
  const unsigned applied = damage < life ? damage : life;
  life -= applied;
  if (!life && keepOne)
    life = 1;
  return life;
}
unsigned GrabHoldDamage(std::uint32_t enemyFlags, bool hardMode) {
  // em10_R1_NeckHang: neutral village type0 has em10GetPower=1.
  return (enemyFlags & 4) && !hardMode ? 10 / 2 + 1 : 10;
}
unsigned GrabFailedReleaseDamage(std::uint32_t playerFlags, bool hardMode) {
  // plem10_NeckHang deliberately tests PLAYER flag4 and player type0.
  return (playerFlags & 4) && !hardMode ? 180 / 2 + 1 : 180;
}
bool GrabBoxContains(const CollisionVec &local) {
  for (auto value : local)
    if (!std::isfinite(value))
      return false;
  return local[0] > -400 && local[0] < 400 && local[1] >= -500 &&
         local[1] <= 500 && local[2] >= 0 && local[2] <= 900;
}
CatchAlignment AlignCatch(const CollisionVec &enemyFeet, float enemyHeading,
                          const CollisionVec &playerFeet,
                          float playerHeading,float offsetX,float offsetZ) {
  validPoint(enemyFeet);
  validPoint(playerFeet);
  if (!std::isfinite(enemyHeading) || !std::isfinite(playerHeading))
    throw std::runtime_error("Grab: invalid direction");
  enemyHeading = limitAngle(enemyHeading);
  playerHeading = limitAngle(playerHeading);
  // Original positions and intermediate matrix arithmetic are F32.
  const float ex = float(enemyFeet[0]), ey = float(enemyFeet[1]),
              ez = float(enemyFeet[2]);
  const float px = float(playerFeet[0]), py = float(playerFeet[1]),
              pz = float(playerFeet[2]);
  const float enemyTarget = limitAngle(std::atan2(px - ex, pz - ez));
  const float playerTarget = limitAngle(std::atan2(ex - px, ez - pz));
  const float enemyFinal =
      limitAngle(enemyHeading + limitAngle(enemyTarget - enemyHeading));
  const float playerFinal =
      limitAngle(playerHeading + limitAngle(playerTarget - playerHeading));
  CatchAlignment result;
  result.enemyTurn = turnTo(enemyHeading, enemyFinal);
  result.playerTurn = turnTo(playerHeading, playerFinal);
  // EmCatchPLSet builds the matrix from PLAYER final yaw, not enemy yaw.
  const float yaw = limitAngle(playerHeading + result.playerTurn);
  const float cosine = std::cos(yaw), sine = std::sin(yaw);
  const float worldX = cosine * offsetX + sine * offsetZ + px;
  const float worldZ = -sine * offsetX + cosine * offsetZ + pz;
  const float dx = (worldX - ex) * .5f, dy = (py - ey) * .5f,
              dz = (worldZ - ez) * .5f;
  result.enemyAdjustment = {dx, dy, dz};
  result.playerAdjustment = {-dx, -dy, -dz};
  return result;
}
} // namespace re4
