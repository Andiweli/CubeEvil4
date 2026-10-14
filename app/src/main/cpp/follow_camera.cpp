#include "follow_camera.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace re4 {
namespace {
constexpr double pi = 3.14159265358979323846;
struct ShoulderSite {
  CollisionVec eye, close, target;
};
// Original g_transOfs[TRANS_DATA_LEON][0][up/mid/down]. Keep the same
// player-local profile in movement and ready states: the source's distinct
// weapon zoom is deliberately omitted to satisfy the requested fixed framing.
constexpr ShoulderSite sites[3]{
    {{-500, 885, -1050}, {-240, 1550, -150}, {0, 2585, 1390}},
    {{-500, 1765, -1190}, {-180, 1700, -110}, {0, 1340, 1480}},
    {{-500, 2420, -680}, {-200, 1870, -185}, {0, 780, 1250}}};
CollisionVec subtract(const CollisionVec &a, const CollisionVec &b) {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
double length(const CollisionVec &v) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}
CollisionVec normalize(const CollisionVec &v) {
  const double l = length(v);
  if (!std::isfinite(l) || l < 1e-9)
    throw std::runtime_error("Camera: invalid view vector");
  return {v[0] / l, v[1] / l, v[2] / l};
}
CollisionVec cross(const CollisionVec &a, const CollisionVec &b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}
CollisionVec interpolate(const CollisionVec &neutral, const CollisionVec &site,
                         double ratio) {
  return {neutral[0] + (site[0] - neutral[0]) * ratio,
          neutral[1] + (site[1] - neutral[1]) * ratio,
          neutral[2] + (site[2] - neutral[2]) * ratio};
}
CollisionVec transform(const CollisionVec &local, const CollisionVec &feet,
                       double heading) {
  const double c = std::cos(heading), s = std::sin(heading);
  return {feet[0] + c * local[0] + s * local[2], feet[1] + local[1],
          feet[2] - s * local[0] + c * local[2]};
}
void validateState(const FollowCameraState &state) {
  if (!std::isfinite(state.yawRatioRadians) ||
      !std::isfinite(state.verticalRatio) ||
      !std::isfinite(state.accumulator) ||
      !std::isfinite(state.releaseYawRadians) ||
      !std::isfinite(state.desiredYaw) || !std::isfinite(state.desiredVertical) ||
      !std::isfinite(state.desiredReleaseYaw) ||
      std::abs(state.desiredYaw) > pi / 3 + 1e-8 ||
      std::abs(state.desiredVertical) > 1 + 1e-8 ||
      std::abs(state.desiredReleaseYaw) > pi / 2 + 1e-8 ||
      std::abs(state.yawRatioRadians) > pi / 3 + 1e-8 ||
      std::abs(state.releaseYawRadians) > pi / 2 + 1e-8 ||
      std::abs(state.verticalRatio) > 1 + 1e-8 || state.accumulator < 0 ||
      state.accumulator >= 1 + 1e-8)
    throw std::runtime_error("Camera: invalid follow camera state");
}
CollisionVec resolveShoulderEye(const SatCollision &world,
                                const ShoulderSite &site,
                                const CollisionVec &feet, double heading,
                                double aspect) {
  // CameraQuasiFPS::hitCheck: one center line and both horizontal edges of
  // the near plane. cameraHitCheck uses this exact SEE flag and exclusion
  // mask. A swept sphere ignores neither the authored attribute exclusions
  // nor the near-plane footprint, and used to pull this view into Leon.
  constexpr double nearPlane = 100;
  constexpr std::uint32_t cameraMask = 0x1c2810;
  const auto delta = subtract(site.eye, site.close);
  const double total = length(delta);
  if (total < 1e-9)
    return transform(site.eye, feet, heading);
  const double width = nearPlane * std::tan(50 * pi / 360) *
                       std::max(1., aspect);
  auto edge = [&](const CollisionVec &eye) {
    auto side = normalize(cross(subtract(eye, site.target), {0, 1, 0}));
    for (auto &component : side)
      component *= width;
    return side;
  };
  const auto closeSide = edge(site.close), outerSide = edge(site.eye);
  auto ray = [&](const CollisionVec &from, const CollisionVec &to) {
    return world.line(transform(from, feet, heading),
                      transform(to, feet, heading), SatSubset::All, 0x8000,
                      cameraMask);
  };
  double fraction = 1;
  bool blocked = false;
  if (const auto hit = ray(site.close, site.eye)) {
    fraction = hit->fraction;
    blocked = true;
  }
  for (double sign : {-1., 1.}) {
    CollisionVec from = site.close, to = site.eye;
    for (unsigned i = 0; i < 3; ++i) {
      from[i] += sign * closeSide[i];
      to[i] += sign * outerSide[i];
    }
    if (const auto hit = ray(from, to)) {
      // The source removes the interpolated side offset from the hit and
      // adds its 100-unit near-plane vector toward the close point. This is
      // the same center-line fraction; clamp at the authored close endpoint.
      fraction = std::min(fraction,
                          std::max(0., hit->fraction - nearPlane / total));
      blocked = true;
    }
  }
  if (!blocked) {
    // Source fallback: a side wall at the preferred outer endpoint can miss
    // all three lengthwise rays. Test the near width in both directions.
    for (double sign : {-1., 1.}) {
      CollisionVec to = site.eye;
      for (unsigned i = 0; i < 3; ++i)
        to[i] += sign * outerSide[i];
      if (ray(site.eye, to)) {
        fraction = std::max(0., 1 - nearPlane / total);
        break;
      }
    }
  }
  CollisionVec eye = site.close;
  for (unsigned i = 0; i < 3; ++i)
    eye[i] += fraction * delta[i];
  return transform(eye, feet, heading);
}
} // namespace
ActionCameraPose PoseObjectLadderCamera(const Matrix34 &worldMatrix,
                                        ObjectLadderCamera action) {
  CollisionVec eye{}, target{};
  switch (action) {
  case ObjectLadderCamera::Climb:
    eye = {0, 300, -1800}; target = {0, 300, 0}; break;
  case ObjectLadderCamera::Tip:
    eye = {0, 2500, -500}; target = {0, 1000, 500}; break;
  case ObjectLadderCamera::Reset:
    eye = {0, 1300, -1000}; target = {0, 1800, 0}; break;
  default: throw std::runtime_error("Ladder: invalid action camera");
  }
  for (auto v : worldMatrix)
    if (!std::isfinite(v))
      throw std::runtime_error("Ladder: invalid camera matrix");
  const auto transformPoint = [&](const CollisionVec &point) {
    CollisionVec out{};
    for (unsigned row = 0; row < 3; ++row) {
      out[row] = worldMatrix[row * 4 + 3];
      for (unsigned column = 0; column < 3; ++column)
        out[row] += worldMatrix[row * 4 + column] * point[column];
    }
    return out;
  };
  return {transformPoint(eye), transformPoint(target), 0, 55};
}
CollisionVec FollowCameraOffset(const SatCollision &world,
                                const CollisionVec &target,
                                const CollisionVec &desired, double radius) {
  double length = 0;
  for (auto v : desired) {
    if (!std::isfinite(v))
      throw std::runtime_error("Camera: invalid offset");
    length += v * v;
  }
  length = std::sqrt(length);
  CollisionVec feet = target;
  feet[1] -= radius;
  auto hit = world.sweepCapsule(feet, desired, radius, 2 * radius,
                                SatSubset::All, 0x8000);
  double margin = std::min(20., radius * .2);
  double rate =
      hit ? std::max(0., hit->fraction - (length > 0 ? margin / length : 0))
          : 1;
  CollisionVec offset = desired;
  for (auto &v : offset)
    v *= rate;
  return offset;
}
CollisionVec ShoulderCameraOffset(const SatCollision *world,
                                  const CollisionVec &feet, double height,
                                  double yaw, double distance) {
  if (!std::isfinite(height) || height <= 0 || !std::isfinite(yaw) ||
      !std::isfinite(distance) || distance < 0)
    throw std::runtime_error("Camera: invalid shoulder parameters");
  auto pivot = feet;
  pivot[1] += height * .95;
  constexpr double shoulder = 900;
  CollisionVec offset{std::sin(yaw) * distance + std::cos(yaw) * shoulder, 0,
                      std::cos(yaw) * distance - std::sin(yaw) * shoulder};
  return world ? FollowCameraOffset(*world, pivot, offset) : offset;
}
void AdvanceFollowCamera(FollowCameraState &state, double mappedRightX,
                         double mappedRightY, double seconds) {
  validateState(state);
  if (!std::isfinite(mappedRightX) || !std::isfinite(mappedRightY) ||
      !std::isfinite(seconds) || seconds < 0)
    throw std::runtime_error("Camera: invalid follow camera controls");
  mappedRightX = std::clamp(mappedRightX, -1., 1.);
  mappedRightY = std::clamp(mappedRightY, -1., 1.);
  double remaining = std::min(seconds, .05) * 30;
  if (!remaining) return; // Pause/validation never initializes or consumes state.
  if (!state.smoothingInitialized) {
    state.desiredYaw = state.yawRatioRadians;
    state.desiredVertical = state.verticalRatio;
    state.desiredReleaseYaw = state.releaseYawRadians;
    state.smoothingInitialized = true;
  }
  const bool active = mappedRightX != 0 || mappedRightY != 0;
  if (active) {
    state.desiredYaw = mappedRightX * pi / 3;
    state.desiredVertical = mappedRightY;
    state.desiredReleaseYaw = 0;
  }
  while (remaining > 1e-10) {
    if (state.accumulator < 1e-10 && !active) {
      state.desiredYaw *= .9;
      state.desiredVertical *= .9;
      state.desiredReleaseYaw *= .9;
    }
    const double part = std::min(remaining, 1 - state.accumulator);
    const double retention = std::pow(.8, part);
    auto smooth = [&](double &visible, double desired) {
      visible = desired + (visible - desired) * retention;
    };
    smooth(state.yawRatioRadians, state.desiredYaw);
    smooth(state.verticalRatio, state.desiredVertical);
    smooth(state.releaseYawRadians, state.desiredReleaseYaw);
    remaining -= part;
    state.accumulator += part;
    if (state.accumulator >= 1 - 1e-10) state.accumulator = 0;
  }
}
FollowCameraPose PoseFollowCamera(const FollowCameraState &state,
                                  const SatCollision *world,
                                  const CollisionVec &feet,
                                  double actorHeading, bool aiming,
                                  double aimYaw, double aimRate, double aspect) {
  validateState(state);
  for (double value : {feet[0], feet[1], feet[2], actorHeading, aimYaw, aimRate,
                      aspect})
    if (!std::isfinite(value))
      throw std::runtime_error("Camera: invalid follow camera position");
  if (aspect <= 0 || aspect > 100)
    throw std::runtime_error("Camera: invalid aspect ratio");
  const double ratio =
      std::clamp(aiming ? aimRate : state.verticalRatio, -1., 1.);
  const auto &end = sites[ratio > 0 ? 0 : 2];
  ShoulderSite site{interpolate(sites[1].eye, end.eye, std::abs(ratio)),
                    interpolate(sites[1].close, end.close, std::abs(ratio)),
                    interpolate(sites[1].target, end.target, std::abs(ratio))};
  // Exact offsetCorrection condition from the source. The close endpoint is
  // extended only when the -320 Z crossing lies past the authored close point.
  auto closeDirection = subtract(site.close, site.eye);
  const double closeLength = length(closeDirection);
  if (closeLength > 1e-9 && std::abs(closeDirection[2]) > 1e-9) {
    closeDirection = normalize(closeDirection);
    const double crossing = (-320 - site.eye[2]) / closeDirection[2];
    if (crossing > closeLength)
      for (unsigned i = 0; i < 3; ++i)
        site.close[i] = site.eye[i] + closeDirection[i] * crossing;
  }
  const double heading =
      actorHeading + (aiming ? aimYaw
                            : state.yawRatioRadians + state.releaseYawRadians);
  FollowCameraPose result;
  result.eye = world ? resolveShoulderEye(*world, site, feet, heading, aspect)
                     : transform(site.eye, feet, heading);
  result.target = transform(site.target, feet, heading);
  // Always derive the view from the resolved eye and explicit target. A wall
  // shortening must not retain a stale Euler rotation pointing at the floor.
  result.look = normalize(subtract(result.target, result.eye));
  result.right = normalize(cross(result.look, {0, 1, 0}));
  result.up = normalize(cross(result.right, result.look));
  result.distance =
      std::hypot(result.eye[0] - feet[0], result.eye[2] - feet[2]);
  return result;
}
} // namespace re4
