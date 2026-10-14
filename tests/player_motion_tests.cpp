#include "leon_preview.h"
#include "turn_rules.h"
#include "walk_probe.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

namespace {
using V = re4::CollisionVec;
void half(std::vector<std::uint8_t> &b, std::size_t p, unsigned v) {
  b.at(p) = v >> 8;
  b.at(p + 1) = v;
}
void word(std::vector<std::uint8_t> &b, std::size_t p, unsigned v) {
  half(b, p, v >> 16);
  half(b, p + 2, v);
}
// Broad floor and a solid wall at X=0, in original millimetre coordinates.
re4::Sat fixture() {
  std::vector<V> vertices{{-100000, 0, -100000}, {100000, 0, -100000},
                          {100000, 0, 100000},   {-100000, 0, 100000},
                          {0, 0, -100000},       {0, 20000, -100000},
                          {0, 0, 100000}};
  std::vector<std::uint8_t> b(20 + vertices.size() * 12 + 24 + 12 + 60);
  b[0] = 32;
  half(b, 2, vertices.size());
  half(b, 4, 2);
  half(b, 6, 1);
  half(b, 10, 3);
  half(b, 12, 2);
  half(b, 16, 1);
  std::size_t offset = 20;
  for (auto point : vertices)
    for (auto value : point) {
      float f = float(value);
      unsigned bits;
      std::memcpy(&bits, &f, 4);
      word(b, offset, bits);
      offset += 4;
    }
  for (float f : {0.f, 1.f, 0.f, 1.f, 0.f, 0.f}) {
    unsigned bits;
    std::memcpy(&bits, &f, 4);
    word(b, offset, bits);
    offset += 4;
  }
  offset += 12;
  unsigned ids[9]{0, 2, 1, 0, 3, 2, 4, 5, 6};
  for (unsigned j = 0; j < 3; ++j) {
    for (unsigned k = 0; k < 3; ++k)
      half(b, offset + k * 2, ids[j * 3 + k]);
    half(b, offset + 6, j == 2 ? 1 : 0);
    offset += 20;
  }
  return re4::Sat(b);
}
void finite(const re4::LeonPreview &actor) {
  for (const auto &bone : actor.rig().bones())
    for (auto f : bone)
      assert(std::isfinite(f));
}
void sourceMotion(const char *player, const char *weapon,
                  const re4::SatCollision &world) {
  re4::LeonPreview actor(player, weapon);
  const auto &rig = actor.rig();
  assert(std::abs(rig.cycle(1).translation[2] - 1624.87255859375) < .001);
  assert(std::abs(rig.cycle(2).translation[2] - 2934.1171875) < .001);
  assert(std::abs(rig.cycle(3).translation[2] + 1378.320068359375) < .001);
  assert(rig.cycle(1).sequenceFrames == 28 &&
         rig.cycle(2).sequenceFrames == 21 &&
         rig.cycle(6).sequenceFrames == 23);
  assert(std::abs(rig.cycle(4).yaw - 3.1415) < .001); // Physical left.
  assert(std::abs(rig.cycle(5).yaw + 3.1415) < .001); // Physical right.
  for (unsigned clip : {1u, 2u, 3u}) {
    actor.reset();
    auto state = static_cast<re4::Locomotion>(clip);
    actor.rig().tick(state); // First authored sample starts at sequence zero.
    double z = 0;
    auto cycle = actor.rig().cycle(clip);
    for (unsigned i = 0; i < cycle.sequenceFrames * 20; ++i) {
      auto delta = actor.rig().tick(state);
      z += delta.translation[2];
      assert(std::abs(delta.translation[2]) < 500);
      assert(!delta.ended);
      finite(actor);
    }
    assert(std::abs(z - cycle.translation[2] * 20) < .1);
  }
  // Scene-frame frequency cannot change the 30-Hz authored displacement/pose.
  re4::LeonPreview hz30(player, weapon), hz60(player, weapon);
  re4::WalkProbe p30(world), p60(world);
  assert(p30.place({10000, 0, 0}) && p60.place({10000, 0, 0}));
  hz30.setPlacement(p30.position(), 0);
  hz60.setPlacement(p60.position(), 0);
  for (unsigned i = 0; i < 300; ++i)
    hz30.step(p30, .75, 1, i >= 150, false, 1. / 30);
  for (unsigned i = 0; i < 600; ++i)
    hz60.step(p60, .75, 1, i >= 300, false, 1. / 60);
  assert(p30.position() == p60.position());
  assert(hz30.heading() == hz60.heading());
  assert(hz30.rig().bones() == hz60.rig().bones());
  // Original GC movement is digital once its stick sector is active. Partial
  // and full deflection play the same authored gait, with identical stride.
  re4::WalkProbe analog(world);
  assert(analog.place({10000, 0, 0}));
  actor.setPlacement(analog.position(), 0);
  actor.step(analog, 0, .5, false, false, 1. / 30);
  for (unsigned i = 0; i < 56; ++i)
    actor.step(analog, 0, .5, false, false, 1. / 30);
  assert(std::abs(analog.position()[2] - rig.cycle(1).translation[2] * 2) < .1);
  re4::LeonPreview partialTurn(player, weapon), fullTurn(player, weapon);
  re4::WalkProbe partialProbe(world), fullProbe(world);
  assert(partialProbe.place({20000, 0, 0}) && fullProbe.place({20000, 0, 0}));
  partialTurn.setPlacement(partialProbe.position(), 0);
  fullTurn.setPlacement(fullProbe.position(), 0);
  for (unsigned i = 0; i < 20; ++i) {
    partialTurn.step(partialProbe, .5, .5, false, false, 1. / 30);
    fullTurn.step(fullProbe, 1, 1, false, false, 1. / 30);
  }
  assert(partialProbe.position() == fullProbe.position());
  assert(partialTurn.heading() == fullTurn.heading());
  assert(partialTurn.rig().bones() == fullTurn.rig().bones());
  float sourceHeading = 0;
  for (unsigned i = 0; i < 20; ++i)
    sourceHeading = re4::GameCubeSteer(sourceHeading, re4::Locomotion::Walk,
                                       re4::MoveForward | re4::MoveRight);
  assert(partialTurn.heading() == sourceHeading);
  // A shallow forward diagonal is outside the source's ±30deg side sector.
  actor.setPlacement(analog.position(), 0);
  for (unsigned i = 0; i < 30; ++i)
    actor.step(analog, .3, .9, false, false, 1. / 30);
  assert(actor.state() == re4::Locomotion::Walk && actor.heading() == 0);
  // A press arriving on a zero-dt render survives until the simulation tick.
  actor.setPlacement(analog.position(), 0);
  actor.step(analog, 0, -1, true, true, 0);
  assert(actor.state() == re4::Locomotion::Idle);
  actor.step(analog, 0, -1, true, false, 1. / 30);
  assert(actor.state() == re4::Locomotion::QuickTurn);
  for (unsigned i = 0; i < 70; ++i) {
    actor.step(analog, 0, -1, true, false, 1. / 30);
    finite(actor);
  }
  assert(actor.state() == re4::Locomotion::Back);
  assert(std::abs(std::abs(actor.heading()) - 3.1415) < .001);
  // Source keyConfigTypeA grants one further tick for B-before-stick-back.
  actor.setPlacement(analog.position(), 0);
  actor.step(analog, 0, 0, true, true, 1. / 30);
  actor.step(analog, 0, -1, true, false, 1. / 30);
  assert(actor.state() == re4::Locomotion::QuickTurn);
  actor.setPlacement(analog.position(), 0);
  actor.step(analog, 0, 0, true, true, 1. / 30);
  actor.step(analog, 0, 0, true, false, 1. / 30);
  actor.step(analog, 0, -1, true, false, 1. / 30);
  assert(actor.state() == re4::Locomotion::Back);
  // Suspending discards a queued edge and elapsed fraction, preserving pose.
  actor.setPlacement(analog.position(), 0);
  actor.step(analog, 0, -1, true, true, 0);
  auto pose = actor.rig().bones();
  actor.suspendClock();
  assert(actor.rig().bones() == pose && actor.heading() == 0);
  actor.step(analog, 0, -1, true, false, 1. / 30);
  assert(actor.state() == re4::Locomotion::Back);
  // Collision rejects root translation while the intended animation continues.
  // Holding forward against the wall must not switch to idle/deadlock.
  re4::WalkProbe blocked(world);
  assert(blocked.place({2000, 0, 0}));
  actor.setPlacement(blocked.position(), -1.57079632679f);
  for (unsigned i = 0; i < 180; ++i)
    actor.step(blocked, 0, 1, true, false, 1. / 30);
  assert(blocked.position()[0] >= 299.99 && blocked.position()[0] < 300.2);
  assert(blocked.wallHit() && actor.state() == re4::Locomotion::Run);
  double x = blocked.position()[0];
  for (unsigned i = 0; i < 30; ++i)
    actor.step(blocked, 0, -1, false, false, 1. / 30);
  assert(blocked.position()[0] > x + 1000);
  // In-place authored turning does not require accepted walking displacement.
  actor.setPlacement(blocked.position(), 0);
  for (unsigned i = 0; i < 15; ++i)
    actor.step(blocked, 1, 0, false, false, 1. / 30);
  assert(actor.state() == re4::Locomotion::TurnRight && actor.heading() < -.5);
  actor.setPlacement(blocked.position(), 0);
  for (unsigned i = 0; i < 15; ++i)
    actor.step(blocked, -1, 0, false, false, 1. / 30);
  assert(actor.state() == re4::Locomotion::TurnLeft && actor.heading() > .5);
  // In-place turning also retains its full authored clip speed at half stick.
  partialTurn.setPlacement(partialProbe.position(), 0);
  fullTurn.setPlacement(fullProbe.position(), 0);
  for (unsigned i = 0; i < 15; ++i) {
    partialTurn.step(partialProbe, .5, 0, false, false, 1. / 30);
    fullTurn.step(fullProbe, 1, 0, false, false, 1. / 30);
  }
  assert(partialTurn.heading() == fullTurn.heading());
  assert(partialTurn.rig().bones() == fullTurn.rig().bones());
  // Behind a +Z-facing actor the camera's right row is (-1,0,0).
  // Physical right input must therefore steer forward travel toward -X.
  re4::WalkProbe steering(world);
  assert(steering.place({20000, 0, 0}));
  actor.setPlacement(steering.position(), 0);
  auto beforeSteering = steering.position();
  for (unsigned i = 0; i < 30; ++i)
    actor.step(steering, 1, 1, false, false, 1. / 30);
  auto afterSteering = steering.position();
  V cameraRight{-1, 0, 0};
  double screenRight = 0;
  for (unsigned i = 0; i < 3; ++i)
    screenRight += (afterSteering[i] - beforeSteering[i]) * cameraRight[i];
  assert(screenRight > 100 && actor.heading() < 0 &&
         afterSteering[2] > beforeSteering[2]);
  // ready00 captures the shoulder look; ready10 transfers it into Leon over
  // four original draw samples. The camera can use the remainder to preserve
  // its world direction while the actor adopts it, including wrapped headings.
  for (float heading : {0.f, 3.f, -3.f}) {
    actor.setPlacement(steering.position(), heading);
    re4::PlayerActionInput aim;
    aim.aimHeld = aim.hasCameraLook = true;
    aim.cameraYaw = .8;
    aim.cameraPitch = .3;
    for (unsigned tick = 0; tick < 5; ++tick) {
      actor.step(steering, 0, 0, false, false, 1. / 30, aim);
      const auto expected = re4::GameCubeLimitAngle(heading + .8f);
      const auto worldView = re4::GameCubeLimitAngle(
          actor.heading() + actor.drawCameraYawRemaining());
      assert(std::abs(re4::GameCubeLimitAngle(worldView - expected)) < 1e-6f);
      // Original CameraControl::getCameraPitch() is zero. Exploration look
      // pitch must not leak into the newly raised weapon.
      assert(actor.aimRate() == 0.f);
    }
    assert(actor.drawCameraYawRemaining() == 0);
    assert(std::abs(re4::GameCubeLimitAngle(actor.heading() - heading) - .8f) <
           1e-6f);
  }
  std::cout
      << "Original Leon root motion PASS: source distances, 20 loop seams, "
         "30/60 Hz, source digital stride/sector steering, quickturn "
         "edge/window/held/completion, suspend, wall block/retreat, physical "
         "left/right steering, source four-frame shoulder draw transfer\n";
}
} // namespace
int main(int argc, char **argv) {
  auto sat = fixture();
  re4::SatCollision world(sat);
  re4::WalkProbe probe(world);
  assert(probe.place({2000, 100, 0}));
  auto old = probe.position();
  assert(!probe.place({0, 0, 0}) && probe.position() == old);
  assert(!probe.place({2000, 251, 0}) && probe.position() == old);
  assert(!probe.place({200000, 0, 0}) && probe.position() == old);
  assert(!probe.place({NAN, 0, 0}) && probe.position() == old);
  probe.moveDelta({-10000, 0, 0});
  assert(probe.position()[0] >= 299.99 && probe.wallHit());
  probe.moveDelta({1000, 0, 0});
  assert(probe.position()[0] >= 1299.99 && !probe.wallHit());
  std::cout << "Root capsule PASS: authored place, rejection rollback, "
               "continuous block and retreat\n";
  if (argc == 3)
    sourceMotion(argv[1], argv[2], world);
  else
    assert(argc == 1);
}
