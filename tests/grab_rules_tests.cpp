#include "grab_rules.h"
#include "input.h"
#include "ports/rnd.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void inputGroups() {
  using namespace re4;
  InputMapper mapper;
  RawInput raw;
  auto poll = [&] { return GrabPressCount(mapper.poll(raw).pressed); };
  assert(poll() == 0);
  raw.buttons = Bottom;
  assert(poll() == 1 && poll() == 0);
  raw.buttons = 0;
  assert(poll() == 0);
  raw.buttons = Bottom | Right;
  assert(poll() == 1); // One original A/B trigger group, two physical buttons.
  raw = {};
  poll();
  raw.lx = -1;
  assert(poll() == 2 && poll() == 0); // Original horizontal group is duplicated.
  raw.lx = 1;
  assert(poll() == 2);
  raw = {};
  poll();
  raw.ly = 1;
  assert(poll() == 1 && poll() == 0);
  raw = {};
  poll();
  raw.lx = -1;
  raw.ly = 1;
  raw.buttons = Bottom;
  assert(poll() == 3);
  raw = {};
  poll();
  raw.buttons = Left | Top | Lb | Rb | Lt | Rt;
  raw.rx = raw.ry = 1;
  assert(poll() == 0); // No weapon trigger, X/Y or right-stick resistance count.
  raw = {};
  poll();
  raw.buttons = DLeft | DRight;
  assert(poll() == 0); // Original contradictory directions are cancelled.
  raw.buttons = DLeft;
  assert(poll() == 2);
  assert(GrabPressCount(MenuUp | MenuDown | MenuLeft | MenuRight | Confirm |
                        Interact | Cancel | Run) == 3);
}
void hitBox() {
  using re4::GrabBoxContains;
  assert(GrabBoxContains({0, 0, 0}));
  assert(GrabBoxContains({399.999, 500, 900}));
  assert(GrabBoxContains({-399.999, -500, 0}));
  assert(!GrabBoxContains({400, 0, 100}));
  assert(!GrabBoxContains({-400, 0, 100}));
  assert(!GrabBoxContains({0, 500.001, 100}));
  assert(!GrabBoxContains({0, -500.001, 100}));
  assert(!GrabBoxContains({0, 0, -.001}));
  assert(!GrabBoxContains({0, 0, 900.001}));
  assert(!GrabBoxContains({std::numeric_limits<double>::quiet_NaN(), 0, 100}));
  assert(!GrabBoxContains({0, std::numeric_limits<double>::infinity(), 100}));
}
void alignment() {
  using re4::AlignCatch;
  constexpr double pi = 3.14159265358979323846;
  auto front = AlignCatch({0, 0, 0}, 0, {0, 0, 1000}, 0);
  assert(std::abs(front.enemyTurn) < .000001);
  assert(std::abs(front.playerTurn + pi) < .000001);
  assert(std::abs(front.enemyAdjustment[0] - 90) < .0001);
  assert(std::abs(front.enemyAdjustment[2] - 264.91) < .0001);
  for (unsigned axis = 0; axis < 3; ++axis)
    assert(front.enemyAdjustment[axis] == -front.playerAdjustment[axis]);
  // Rotate and translate the whole configuration: the actor-local catch
  // offset must stay invariant, despite distinct old headings and heights.
  for (unsigned step = 0; step < 64; ++step) {
    const double yaw = -pi + step * 2 * pi / 64;
    const double sine = std::sin(yaw), cosine = std::cos(yaw);
    re4::CollisionVec enemy{12000, 100, -22000};
    re4::CollisionVec player{enemy[0] + sine * 1000, 140,
                             enemy[2] + cosine * 1000};
    auto result = AlignCatch(enemy, float(yaw - .7), player, float(yaw + .4));
    const double enemyFinal = yaw - .7 + result.enemyTurn;
    const double playerFinal = yaw + .4 + result.playerTurn;
    assert(std::abs(std::sin(enemyFinal - yaw)) < .00001);
    assert(std::abs(std::sin(playerFinal - yaw - pi)) < .00001);
    assert(std::cos(enemyFinal - yaw) > .999999);
    assert(std::cos(playerFinal - yaw - pi) > .999999);
    for (unsigned axis = 0; axis < 3; ++axis)
      assert(result.enemyAdjustment[axis] == -result.playerAdjustment[axis]);
    assert(result.enemyAdjustment[1] == 20);
    // The final paired positions retain a rotated(-180,0,470.18) separation.
    const double dx = enemy[0] + result.enemyAdjustment[0] -
                      (player[0] + result.playerAdjustment[0]);
    const double dz = enemy[2] + result.enemyAdjustment[2] -
                      (player[2] + result.playerAdjustment[2]);
    const double localX = std::cos(playerFinal) * dx - std::sin(playerFinal) * dz;
    const double localZ = std::sin(playerFinal) * dx + std::cos(playerFinal) * dz;
    assert(std::abs(localX + 180) < .01 && std::abs(localZ - 470.18) < .01);
  }
  for (const auto &bad :
       {re4::CollisionVec{NAN, 0, 0}, re4::CollisionVec{0, INFINITY, 0},
        re4::CollisionVec{1e9, 0, 0}}) {
    bool rejected = false;
    try {
      (void)AlignCatch(bad, 0, {0, 0, 1000}, 0);
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    assert(rejected);
  }
}
void lifeOracle() {
  using namespace re4;
  assert(OriginalLeonLife == 1200);
  assert(GrabHoldDamage(0) == 10 && GrabHoldDamage(0x24) == 6 &&
         GrabHoldDamage(0x24, true) == 10);
  assert(GrabFailedReleaseDamage(0) == 180 &&
         GrabFailedReleaseDamage(4) == 91 &&
         GrabFailedReleaseDamage(4, true) == 180);
  // Frozen independent source oracle: exact player branch of upstream
  // em_sub.cpp::LifeDownSet2, commit 9b76ff003081d6ece8b838453fb8310743a972c7,
  // compiled with original rnd.cpp recurrence and no-op event/debug stubs.
  // Rank 5, no armour; all 65536 RNG seeds, seven HP boundaries, five damage
  // values and both keep-one flags = 4,587,520 independently evaluated cases.
  std::uint64_t hash = 1469598103934665603ull;
  auto append = [&](unsigned value) {
    for (unsigned byte = 0; byte < 4; ++byte) {
      hash ^= (value >> (byte * 8)) & 255;
      hash *= 1099511628211ull;
    }
  };
  unsigned cases = 0;
  for (unsigned seed = 0; seed < 65536; ++seed)
    for (unsigned hp : {0u, 1u, 50u, 199u, 300u, 301u, 1200u})
      for (unsigned damage : {0u, 6u, 180u, 1200u, 9999u})
        for (bool keep : {false, true}) {
          RndInit(static_cast<std::uint16_t>(seed));
          append(GrabPlayerLifeDown(hp, damage, keep));
          append(RndState());
          ++cases;
        }
  assert(cases == 4587520 && hash == 0x0ea5b178252da983ull);
  RndInit(0x4e19);
  unsigned hp = OriginalLeonLife;
  for (unsigned frame = 0; frame < 35; ++frame)
    hp = GrabPlayerLifeDown(hp, GrabHoldDamage(0x24), true);
  assert(hp == 990); // Actual original ESL 0x25 flags 0x24, neutral rank 5.
  hp = GrabPlayerLifeDown(hp, GrabFailedReleaseDamage(0), true);
  assert(hp == 810);
  bool rejected = false;
  const auto random = RndState();
  try {
    (void)GrabPlayerLifeDown(65535, 6, true);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected && RndState() == random);
}
} // namespace
int main() {
  inputGroups();
  hitBox();
  alignment();
  lifeOracle();
  std::cout << "Original grab rules PASS: physical press groups, strict box, "
               "paired placement/yaw, original 1200 HP and 4,587,520 "
               "source-oracle life/RNG cases; 35 actual-spawn hold ticks "
               "990 HP then 810 HP fail\n";
}
