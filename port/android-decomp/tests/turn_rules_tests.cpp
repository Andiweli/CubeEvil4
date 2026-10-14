#include "input.h"
#include "turn_rules.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

namespace source_oracle {
// Arithmetic/order transcribed independently from frozen game/pad.cpp and
// game/player.cpp, with numeric Key masks instead of the platform enums.
unsigned directions(float x, float y) {
  unsigned result = 0;
  float ang = -std::atan2(x, y);
  if (x * x + y * y > 30.f * 30.f) {
    if (ang < 0.f) {
      if (ang > -(70.f * .017453292f))
        result |= 1;
      if (ang < -(110.f * .017453292f))
        result |= 2;
      if (ang < -(30.f * .017453292f) && ang > -(150.f * .017453292f))
        result |= 4;
    } else {
      if (ang < 70.f * .017453292f)
        result |= 1;
      if (ang > 110.f * .017453292f)
        result |= 2;
      if (ang > 30.f * .017453292f && ang < 150.f * .017453292f)
        result |= 8;
    }
  }
  return result;
}
float limit(float value) {
  while (value >= 3.1415927f)
    value -= 6.2831855f;
  while (value < -3.1415927f)
    value += 6.2831855f;
  return value;
}
float steer(float value, re4::Locomotion state, unsigned key) {
  if (state == re4::Locomotion::Run) {
    if (key & 4)
      value -= .0418879f * 1.1f;
    else if (key & 8)
      value += .0418879f * 1.1f;
  } else if (state == re4::Locomotion::Walk || state == re4::Locomotion::Back) {
    if (key & 4)
      value -= .0418879f;
    if (key & 8)
      value += .0418879f;
  }
  return limit(value);
}
} // namespace source_oracle
int main() {
  using namespace re4;
  unsigned checks = 0;
  for (int x = -72; x <= 72; ++x)
    for (int y = -72; y <= 72; ++y) {
      assert(GameCubeStickDirections(float(x), float(y)) ==
             source_oracle::directions(float(x), float(y)));
      ++checks;
      const auto keys = GameCubeStickDirections(float(x), float(y));
      assert(!(keys & MoveForward) || !(keys & MoveBack));
      assert(!(keys & MoveLeft) || !(keys & MoveRight));
      // Values outside the physical full-radius stick are saturated by the
      // platform mapper. Every GC-range point inside that circle must survive
      // the mapper and inverse adapter exactly, including the radius30 edge.
      if (x * x + y * y <= 72 * 72) {
        InputMapper mapper;
        RawInput raw;
        raw.lx = float(x) / 72;
        raw.ly = float(y) / 72;
        const auto mapped = mapper.poll(raw);
        assert(GameCubeMappedDirections(mapped.lx, mapped.ly) == keys);
        ++checks;
      }
    }
  // Source angular boundaries are strict; sample both sides of every edge.
  for (float radius : {29.f, 30.f, 30.01f, 40.f, 72.f})
    for (float degree :
         {-150.01f, -150.f,  -149.99f, -110.01f, -110.f,  -109.99f, -70.01f,
          -70.f,    -69.99f, -30.01f,  -30.f,    -29.99f, 0.f,      29.99f,
          30.f,     30.01f,  69.99f,   70.f,     70.01f,  109.99f,  110.f,
          110.01f,  149.99f, 150.f,    150.01f}) {
      const float angle = degree * .017453292f;
      const float x = std::sin(angle) * radius;
      const float y = std::cos(angle) * radius;
      assert(GameCubeStickDirections(x, y) == source_oracle::directions(x, y));
      ++checks;
    }
  // Independent axes are not the original sectors: shallow 20deg turns walk
  // straight, while 45deg combines walking and full fixed-speed steering.
  assert(GameCubeMappedDirections(.3, .9) == MoveForward);
  assert(GameCubeMappedDirections(.5, .5) == (MoveForward | MoveRight));
  assert(GameCubeMappedDirections(.9, .3) == MoveRight);
  assert(GameCubeMappedDirections(.5, -.5) == (MoveBack | MoveRight));
  assert(GameCubeMappedDirections(-.5, -.5) == (MoveBack | MoveLeft));
  assert(GameCubeMappedDirections(0, .30) == 0);
  assert(GameCubeMappedDirections(0, .33) == MoveForward);
  assert(GameCubeMappedDirections(0, 0) == 0);
  assert(GameCubeLocomotion(MoveForward | MoveRight, false) ==
         Locomotion::Walk);
  assert(GameCubeLocomotion(MoveForward | MoveLeft, true) == Locomotion::Run);
  assert(GameCubeLocomotion(MoveBack | MoveRight, true) == Locomotion::Back);
  assert(GameCubeLocomotion(MoveLeft, false) == Locomotion::TurnLeft);
  assert(GameCubeLocomotion(MoveRight, false) == Locomotion::TurnRight);
  assert(GameCubeLocomotion(0, true) == Locomotion::Idle);
  for (unsigned state = 0; state <= unsigned(Locomotion::QuickTurn); ++state)
    for (unsigned key = 0; key < 16; ++key)
      for (int sample = -2048; sample <= 2048; ++sample) {
        const float heading = sample * .01f;
        assert(GameCubeSteer(heading, Locomotion(state), key) ==
               source_oracle::steer(heading, Locomotion(state), key));
        ++checks;
      }
  assert(GameCubeLimitAngle(3.1415927f) == -3.1415927f);
  assert(GameCubeLimitAngle(-3.1415927f) == -3.1415927f);
  // The handgun draw transfers the captured camera heading in four equal
  // increments while preserving actor heading + remaining view direction.
  for (float initial : {-1.0471976f, -.4f, 0.f, .4f, 1.0471976f}) {
    float remaining = initial, heading = 0;
    for (unsigned frame = 0; frame < 4; ++frame) {
      float oracleRemaining = remaining;
      const float expected = oracleRemaining / (4.f - frame);
      oracleRemaining -= expected;
      const auto delta = GameCubeDrawTurn(frame, remaining);
      assert(delta == expected && remaining == oracleRemaining);
      heading += delta;
      assert(std::abs(heading + remaining - initial) < 1e-6f);
      ++checks;
    }
    assert(remaining == 0 && std::abs(heading - initial) < 1e-6f);
    assert(GameCubeDrawTurn(4, remaining) == 0);
  }
  for (float bad : {std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::quiet_NaN()}) {
    bool rejected = false;
    try {
      (void)GameCubeMappedDirections(bad, 0);
    } catch (...) {
      rejected = true;
    }
    assert(rejected);
  }
  std::cout << "Original GC turn rules PASS: " << checks
            << " source sector/fixed-steering/wrap/draw checks; original "
               "threshold restored after platform radial dead zone\n";
}
