#include "follow_camera.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using V = re4::CollisionVec;
unsigned checks{};
constexpr double pi = 3.14159265358979323846;
void checkAt(bool good, unsigned line) {
  ++checks;
  if (!good)
    throw std::runtime_error("Follow camera contract " + std::to_string(checks) +
                             " at line " + std::to_string(line));
}
#define check(good) checkAt((good), __LINE__)
bool near(double a, double b, double epsilon = 1e-8) {
  return std::abs(a - b) < epsilon;
}
void vectorNear(const V &a, const V &b) {
  for (unsigned i = 0; i < 3; ++i)
    if (!near(a[i], b[i]))
      throw std::runtime_error("Vector component " + std::to_string(i) +
                               ": " + std::to_string(a[i]) + " != " +
                               std::to_string(b[i]));
    else
      check(true);
}
double dot(const V &a, const V &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
V difference(const V &a, const V &b) {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
V rotated(const V &v, double heading) {
  return {std::cos(heading) * v[0] + std::sin(heading) * v[2], v[1],
          -std::sin(heading) * v[0] + std::cos(heading) * v[2]};
}
// A wide original-format SAT wall. Camera-only attributes make sure the SEE
// query is used, rather than accidentally treating this as player collision.
re4::Sat wall(double z, double lowX = -3000, double highX = 3000) {
  std::vector<std::uint8_t> bytes(20);
  bytes[0] = 0x20;
  re4::Sat result(bytes);
  result.vertices = {{float(lowX), 0, float(z)}, {float(highX), 0, float(z)},
                     {float(highX), 4000, float(z)},
                     {float(lowX), 4000, float(z)}};
  result.normals = {{0, 0, 1}};
  result.edges = {{0, 0, 0}};
  re4::SatPolygon a, b;
  a.vertices = {0, 1, 2};
  b.vertices = {0, 2, 3};
  a.attributes = b.attributes = 0x400;
  result.polygons = {a, b};
  result.wall = 2;
  return result;
}
void basis(const re4::FollowCameraPose &pose) {
  check(near(dot(pose.look, pose.look), 1));
  check(near(dot(pose.right, pose.right), 1));
  check(near(dot(pose.up, pose.up), 1));
  check(near(dot(pose.look, pose.right), 0));
  check(near(dot(pose.look, pose.up), 0));
  check(near(dot(pose.right, pose.up), 0));
  const auto target = difference(pose.target, pose.eye);
  // Actual resolved target stays on the optical axis, including at a wall.
  check(near(dot(target, pose.right), 0));
  check(near(dot(target, pose.up), 0));
  check(dot(target, pose.look) > 0);
}
} // namespace

int main() {
  // Independent transcription of obj13's three special camera functions.
  // A root part can be lower than actor.pos during mounting/dismounting; the
  // caller must transform the climb's 300-height profile by animated part0.
  const re4::Matrix34 rootWorld{0,0,1,1200, 0,1,0,-700, -1,0,0,4200};
  const auto climb = re4::PoseObjectLadderCamera(rootWorld, re4::ObjectLadderCamera::Climb);
  vectorNear(climb.eye, {-600,-400,4200});
  vectorNear(climb.target, {1200,-400,4200});
  check(climb.fovDegrees == 55 && climb.roll == 0);
  const auto tip = re4::PoseObjectLadderCamera(rootWorld, re4::ObjectLadderCamera::Tip);
  vectorNear(tip.eye, {700,1800,4200});
  vectorNear(tip.target, {1700,300,4200});
  check(tip.fovDegrees == 55 && tip.roll == 0);
  const auto reset = re4::PoseObjectLadderCamera(rootWorld, re4::ObjectLadderCamera::Reset);
  vectorNear(reset.eye, {200,600,4200});
  vectorNear(reset.target, {1200,1100,4200});
  check(reset.fovDegrees == 55 && reset.roll == 0);
  re4::FollowCameraState state;
  const auto neutral = re4::PoseFollowCamera(state, nullptr, {0, 0, 0}, 0);
  vectorNear(neutral.eye, {-500, 1765, -1190});
  vectorNear(neutral.target, {0, 1340, 1480});
  check(neutral.fovDegrees == 50);
  check(near(neutral.distance, std::hypot(500., 1190.)));
  basis(neutral);

  // Independent source constants catch table/site swaps. The platform keeps
  // this transition profile for weapon/knife as well by explicit user request.
  state.verticalRatio = 1;
  auto up = re4::PoseFollowCamera(state, nullptr, {0, 0, 0}, 0);
  vectorNear(up.eye, {-500, 885, -1050});
  vectorNear(up.target, {0, 2585, 1390});
  check(up.look[1] > 0);
  state.verticalRatio = -1;
  auto down = re4::PoseFollowCamera(state, nullptr, {0, 0, 0}, 0);
  vectorNear(down.eye, {-500, 2420, -680});
  vectorNear(down.target, {0, 780, 1250});
  check(down.look[1] < 0);
  state.verticalRatio = .5;
  auto half = re4::PoseFollowCamera(state, nullptr, {0, 0, 0}, 0);
  vectorNear(half.eye, {-500, 1325, -1120});
  vectorNear(half.target, {0, 1962.5, 1435});
  basis(half);

  state = {};
  re4::AdvanceFollowCamera(state, -1, -.5, 1. / 60);
  check(state.yawRatioRadians < 0 && state.yawRatioRadians > -pi / 3 * .2);
  check(state.verticalRatio < 0 && state.verticalRatio > -.1);
  re4::AdvanceFollowCamera(state, -1, -.5, 1. / 60);
  check(near(state.yawRatioRadians, -pi / 3 * .2));
  check(near(state.verticalRatio, -.1));
  const auto first = state;
  for (unsigned i = 0; i < 3000; ++i)
    re4::AdvanceFollowCamera(state, -1, -.5, 1. / 30);
  check(near(state.yawRatioRadians, -pi / 3));
  check(near(state.verticalRatio, -.5));
  check(state.desiredYaw == first.desiredYaw);
  const auto held = state;
  re4::AdvanceFollowCamera(state, 0, 0, 1. / 30);
  check(near(state.desiredYaw, held.desiredYaw * .9));
  check(near(state.yawRatioRadians, held.yawRatioRadians * .98));
  check(near(state.verticalRatio, held.verticalRatio * .98));
  re4::AdvanceFollowCamera(state, 1, .5, 1. / 120);
  check(state.yawRatioRadians > held.yawRatioRadians * .98);
  check(state.yawRatioRadians < 0); // Reversing is gradual, never a full flip.
  for (unsigned i = 0; i < 150; ++i)
    re4::AdvanceFollowCamera(state, 0, 0, 1. / 30);
  check(std::abs(state.yawRatioRadians) < .0001);
  check(std::abs(state.verticalRatio) < .0001);

  re4::FollowCameraState hz30, hz60, hz120;
  auto equal = [&](const auto &a, const auto &b) {
    check(near(a.yawRatioRadians, b.yawRatioRadians));
    check(near(a.verticalRatio, b.verticalRatio));
    check(near(a.releaseYawRadians, b.releaseYawRadians));
    check(near(a.desiredYaw, b.desiredYaw));
    check(near(a.desiredVertical, b.desiredVertical));
    check(near(a.accumulator, b.accumulator));
  };
  for (const auto input : {std::array<double, 2>{-.7,.4}, {0,0}, {.8,-.6}, {0,0}}) {
    for (unsigned i = 0; i < 30; ++i)
      re4::AdvanceFollowCamera(hz30, input[0], input[1], 1. / 30);
    for (unsigned i = 0; i < 60; ++i)
      re4::AdvanceFollowCamera(hz60, input[0], input[1], 1. / 60);
    for (unsigned i = 0; i < 120; ++i)
      re4::AdvanceFollowCamera(hz120, input[0], input[1], 1. / 120);
    equal(hz30, hz60); equal(hz30, hz120);
  }
  re4::AdvanceFollowCamera(hz30, -.2, 1, 1. / 60);
  auto checkpoint = hz30;
  re4::AdvanceFollowCamera(hz30, 0, 0, 0);
  equal(hz30, checkpoint);
  re4::AdvanceFollowCamera(hz30, -.2, 1, 1. / 60);
  re4::AdvanceFollowCamera(checkpoint, -.2, 1, 1. / 60);
  equal(hz30, checkpoint);

  // Movement, body rotation and weapon drawing use the same local framing;
  // no animated muzzle can move the view. Consuming look yaw into actor yaw
  // keeps eye/target unchanged in the world, including during the draw blend.
  state = {-.6, .3, 0};
  auto before = re4::PoseFollowCamera(state, nullptr, {40, 300, -200}, 1.2);
  auto handover = re4::PoseFollowCamera(state, nullptr, {40, 300, -200},
                                       1.2 - .4, true, -.2, .3);
  vectorNear(before.eye, handover.eye);
  vectorNear(before.target, handover.target);
  check(before.fovDegrees == handover.fovDegrees);
  basis(handover);
  for (double heading : {-pi, -2., -.8, 0., .5, 2., pi}) {
    auto pose = re4::PoseFollowCamera({}, nullptr, {400, 1200, -700}, heading);
    vectorNear(difference(pose.eye, {400, 1200, -700}),
               rotated(neutral.eye, heading));
    vectorNear(difference(pose.target, {400, 1200, -700}),
               rotated(neutral.target, heading));
    vectorNear(pose.look, rotated(neutral.look, heading));
    basis(pose);
  }
  auto neutralAim = re4::PoseFollowCamera({}, nullptr, {0, 0, 0}, 0, true);
  vectorNear(neutral.eye, neutralAim.eye);
  vectorNear(neutral.target, neutralAim.target);

  // A mode return carries the remaining aim/draw yaw into footwork without
  // changing the body transform or optical target. Fresh input replaces the
  // visual transition; otherwise the original neutral factor recenters it.
  auto beforeReturn =
      re4::PoseFollowCamera({}, nullptr, {0, 0, 0}, .7, true, .3, .4);
  state = {0, .4, 0, .3};
  auto afterReturn = re4::PoseFollowCamera(state, nullptr, {0, 0, 0}, .7);
  vectorNear(beforeReturn.eye, afterReturn.eye);
  vectorNear(beforeReturn.target, afterReturn.target);
  re4::AdvanceFollowCamera(state, 0, 0, 1. / 30);
  check(near(state.releaseYawRadians, .294));
  check(near(state.verticalRatio, .392));
  auto releaseCheckpoint = state;
  re4::AdvanceFollowCamera(state, 0, 0, 1. / 60);
  re4::AdvanceFollowCamera(releaseCheckpoint, 0, 0, 1. / 60);
  check(state.releaseYawRadians == releaseCheckpoint.releaseYawRadians);
  check(state.accumulator == releaseCheckpoint.accumulator);
  re4::AdvanceFollowCamera(state, -.2, 0, 1. / 60);
  check(state.releaseYawRadians > 0 && state.desiredReleaseYaw == 0);
  check(state.yawRatioRadians < 0 && state.yawRatioRadians > -.2 * pi / 3);

  auto scenery = wall(-800);
  re4::SatCollision collision(scenery);
  const auto close = re4::PoseFollowCamera({}, &collision, {0, 0, 0}, 0);
  check(close.eye[2] > -800 && close.eye[2] < -650);
  check(close.eye[1] >= 1700);
  check(close.distance < neutral.distance);
  vectorNear(close.target, neutral.target);
  basis(close);
  auto veryCloseScenery = wall(-300);
  re4::SatCollision closeCollision(veryCloseScenery);
  const auto veryClose =
      re4::PoseFollowCamera({}, &closeCollision, {0, 0, 0}, 0);
  check(veryClose.eye[2] > -300 && veryClose.eye[2] < -110);
  check(veryClose.eye[1] >= 1700);
  vectorNear(veryClose.target, neutral.target);
  basis(veryClose);

  // Independent source-derived broad-wall result: the side lines meet z=-800
  // and their center correction adds exactly ZNEAR along the close direction.
  // M24's 120-unit sphere produced a substantially different, shorter view.
  const double segmentLength = std::sqrt(320. * 320 + 65. * 65 + 1080. * 1080);
  const double nearWidth = 100 * std::tan(25 * pi / 180) * 4. / 3;
  const double sideCloseZ = -110 - nearWidth * 180 / std::hypot(1590., 180.);
  const double sideOuterZ = -1190 - nearWidth * 500 / std::hypot(2670., 500.);
  const double sourceFraction =
      (-800 - sideCloseZ) / (sideOuterZ - sideCloseZ) - 100. / segmentLength;
  vectorNear(close.eye, {-180 - sourceFraction * 320,
                         1700 + sourceFraction * 65,
                         -110 - sourceFraction * 1080});
  // The authored close site is the lower bound. No collision pushes the eye
  // through the close point or independently shifts the optical target.
  auto endpointScenery = wall(-110);
  re4::SatCollision endpointCollision(endpointScenery);
  auto endpoint = re4::PoseFollowCamera({}, &endpointCollision, {0, 0, 0}, 0);
  vectorNear(endpoint.eye, {-180, 1700, -110});
  vectorNear(endpoint.target, neutral.target);
  basis(endpoint);

  // Each source camera-ignore bit is excluded exactly, while camera-only
  // walls remain active. These exclusions used to be lost in the sphere test.
  for (unsigned ignored : {0x10u, 0x800u, 0x2000u, 0x40000u, 0x80000u,
                           0x100000u, 0x800000u}) {
    auto ignoredScenery = wall(-800);
    for (auto &polygon : ignoredScenery.polygons)
      polygon.attributes |= ignored;
    re4::SatCollision ignoredCollision(ignoredScenery);
    const auto unchanged =
        re4::PoseFollowCamera({}, &ignoredCollision, {0, 0, 0}, 0);
    vectorNear(unchanged.eye, neutral.eye);
    vectorNear(unchanged.target, neutral.target);
  }
  // Both edge lines must independently protect narrow wall edges missed by
  // the center. The real viewport aspect controls the near-plane width.
  for (const auto &range : {std::array<double, 2>{-455, -435},
                            std::array<double, 2>{-334, -314}}) {
    auto sideScenery = wall(-800, range[0], range[1]);
    re4::SatCollision sideCollision(sideScenery);
    const auto edgePose =
        re4::PoseFollowCamera({}, &sideCollision, {0, 0, 0}, 0);
    check(edgePose.eye[2] > -800 && edgePose.eye[2] < -650);
    vectorNear(edgePose.target, neutral.target);
    basis(edgePose);
  }
  auto widescreenScenery = wall(-800, -480, -455);
  re4::SatCollision widescreenCollision(widescreenScenery);
  const auto classic =
      re4::PoseFollowCamera({}, &widescreenCollision, {0, 0, 0}, 0);
  const auto widescreen = re4::PoseFollowCamera(
      {}, &widescreenCollision, {0, 0, 0}, 0, false, 0, 0, 16. / 9);
  vectorNear(classic.eye, neutral.eye);
  check(widescreen.eye[2] > -800 && widescreen.eye[2] < -650);
  vectorNear(widescreen.target, neutral.target);
  basis(widescreen);

  // Turning at a wall resolves eye and target in the same player-local frame.
  // Sample every degree: finite, forward-facing and no stale Euler look.
  for (unsigned angle = 0; angle < 360; ++angle) {
    const auto turned = re4::PoseFollowCamera(
        {}, &collision, {0, 0, 0}, angle * pi / 180, false, 0, 0, 16. / 9);
    basis(turned);
    check(std::isfinite(turned.distance));
    check(turned.eye[1] >= 1700 && turned.eye[1] <= 1765);
  }

  const double nan = std::numeric_limits<double>::quiet_NaN();
  unsigned errors{};
  for (unsigned i = 0; i < 8; ++i) {
    try {
      state = {};
      if (i == 0)
        re4::AdvanceFollowCamera(state, nan, 0, .01);
      else if (i == 1)
        re4::AdvanceFollowCamera(state, 0, 0, -.01);
      else if (i == 2)
        re4::PoseFollowCamera(state, nullptr, {nan, 0, 0}, 0);
      else if (i == 3) {
        state.accumulator = 2;
        re4::AdvanceFollowCamera(state, 0, 0, .01);
      } else if (i == 4) {
        state.yawRatioRadians = 2;
        re4::PoseFollowCamera(state, nullptr, {0, 0, 0}, 0);
      } else if (i == 5) {
        state.releaseYawRadians = 2;
        re4::PoseFollowCamera(state, nullptr, {0, 0, 0}, 0);
      } else {
        re4::PoseFollowCamera(state, nullptr, {0, 0, 0}, 0, false, 0, 0,
                              i == 6 ? 0 : nan);
      }
    } catch (const std::runtime_error &) {
      ++errors;
    }
  }
  check(errors == 8);
  std::cout << "Original shoulder sites, bounded look, 30/60 Hz recenter, "
               "stable draw framing, three-ray SEE/mask, viewport edges and "
               "checkpoint PASS · "
            << checks << " checks\n";
}
