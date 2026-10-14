#include "turn_rules.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace re4 {
std::uint8_t GameCubeStickDirections(float x, float y) {
  if (!std::isfinite(x) || !std::isfinite(y))
    throw std::runtime_error("Leon: invalid GC stick");
  // Frozen upstream game/pad.cpp PadRead: STICK_ON=30, -atan2f(x,y),
  // overlapping 70/110 degree forward/back and 30..150 degree turn sectors.
  constexpr float rad = .017453292f;
  std::uint8_t keys = 0;
  const float angle = -std::atan2(x, y);
  if (x * x + y * y > 30.f * 30.f) {
    if (angle < 0) {
      if (angle > -70.f * rad)
        keys |= MoveForward;
      if (angle < -110.f * rad)
        keys |= MoveBack;
      if (angle < -30.f * rad && angle > -150.f * rad)
        keys |= MoveRight;
    } else {
      if (angle < 70.f * rad)
        keys |= MoveForward;
      if (angle > 110.f * rad)
        keys |= MoveBack;
      if (angle > 30.f * rad && angle < 150.f * rad)
        keys |= MoveLeft;
    }
  }
  return keys;
}
std::uint8_t GameCubeMappedDirections(double x, double y) {
  if (!std::isfinite(x) || !std::isfinite(y))
    throw std::runtime_error("Leon: invalid platform stick");
  x = std::clamp(x, -1., 1.);
  y = std::clamp(y, -1., 1.);
  const double magnitude = std::hypot(x, y);
  if (magnitude == 0)
    return 0;
  const double radius = (.15 + .85 * std::min(magnitude, 1.)) * 72;
  // PADClamp stores signed integer stick axes. Quantize the adapter's
  // reconstructed axes likewise; tiny float radius errors must not turn the
  // exact source boundary (e.g.18²+24²=30²) into a held direction.
  return GameCubeStickDirections(float(std::round(x / magnitude * radius)),
                                float(std::round(y / magnitude * radius)));
}
Locomotion GameCubeLocomotion(std::uint8_t keys, bool runHeld) {
  if (keys & MoveForward)
    return runHeld ? Locomotion::Run : Locomotion::Walk;
  if (keys & MoveBack)
    return Locomotion::Back;
  if (keys & MoveRight)
    return Locomotion::TurnRight;
  if (keys & MoveLeft)
    return Locomotion::TurnLeft;
  return Locomotion::Idle;
}
float GameCubeLimitAngle(float heading) {
  if (!std::isfinite(heading))
    throw std::runtime_error("Leon: invalid GC angle");
  constexpr float pi = 3.1415927f, pi2 = 6.2831855f;
  // All callers feed one tick's heading (at most a few revolutions). Bound
  // malicious external input without a potentially unbounded float loop.
  if (std::abs(heading) > 64 * pi2)
    heading = std::fmod(heading, pi2);
  while (heading >= pi)
    heading -= pi2;
  while (heading < -pi)
    heading += pi2;
  return heading;
}
float GameCubeSteer(float heading, Locomotion state, std::uint8_t keys) {
  constexpr float speed = .0418879f;
  // game/player.cpp: run uses an else-if, walk/back two separate ifs.
  if (state == Locomotion::Run) {
    if (keys & MoveRight)
      heading -= speed * 1.1f;
    else if (keys & MoveLeft)
      heading += speed * 1.1f;
  } else if (state == Locomotion::Walk || state == Locomotion::Back) {
    if (keys & MoveRight)
      heading -= speed;
    if (keys & MoveLeft)
      heading += speed;
  }
  return GameCubeLimitAngle(heading);
}
float GameCubeDrawTurn(float sequenceFrame, float &remainingYaw) {
  if (!std::isfinite(sequenceFrame) || !std::isfinite(remainingYaw))
    throw std::runtime_error("Leon: invalid shoulder transition");
  if (sequenceFrame < 0 || sequenceFrame >= 4)
    return 0;
  const float delta = remainingYaw / (4.f - sequenceFrame);
  remainingYaw -= delta;
  return delta;
}
} // namespace re4
