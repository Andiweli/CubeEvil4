#include "follow_camera.h"
#include "walk_probe.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using V = re4::CollisionVec;
void h(std::vector<std::uint8_t> &b, int p, unsigned v) {
  b[p] = v >> 8;
  b[p + 1] = v;
}
void u(std::vector<std::uint8_t> &b, int p, unsigned v) {
  h(b, p, v >> 16);
  h(b, p + 2, v);
}
// Two wide ground triangles, one wall at X=0. Original upward floor normals.
re4::Sat fixture() {
  std::vector<V> v{{-20, 0, -20}, {20, 0, -20}, {20, 0, 20}, {-20, 0, 20},
                   {0, 0, -20},   {0, 20, -20}, {0, 0, 20}};
  std::vector<std::uint8_t> b(20 + v.size() * 12 + 2 * 12 + 12 + 3 * 20);
  b[0] = 32;
  h(b, 2, v.size());
  h(b, 4, 2);
  h(b, 6, 1);
  h(b, 10, 3);
  h(b, 12, 2);
  h(b, 16, 1);
  int offset = 20;
  for (auto point : v)
    for (auto value : point) {
      float f = value;
      unsigned bits;
      std::memcpy(&bits, &f, 4);
      u(b, offset, bits);
      offset += 4;
    }
  float normals[6]{0, 1, 0, 1, 0, 0};
  for (auto f : normals) {
    unsigned bits;
    std::memcpy(&bits, &f, 4);
    u(b, offset, bits);
    offset += 4;
  }
  offset += 12;
  unsigned ids[9]{0, 2, 1, 0, 3, 2, 4, 5, 6};
  for (int j = 0; j < 3; ++j) {
    for (int k = 0; k < 3; ++k)
      h(b, offset + k * 2, ids[j * 3 + k]);
    h(b, offset + 6, j == 2 ? 1 : 0);
    offset += 20;
  }
  return re4::Sat(b);
}
int main() {
  auto sat = fixture();
  re4::SatCollision world(sat);
  auto floor = world.ground({5, 2, 0}, 1, 5);
  assert(floor && std::abs(floor->point[1]) < 1e-9 && floor->normal[1] == 1);
  assert(!world.ground({50, 0, 0}, 1, 5));
  assert(!world.line({1, 0, 0}, {2, 0, 0}));
  auto wall = world.line({5, 2, 0}, {-5, 2, 0}, re4::SatSubset::Walls);
  assert(wall && std::abs(wall->fraction - .5) < 1e-9);
  auto sweep = world.sweepCapsule({5, 0, 0}, {-10, 0, 0}, 1, 4);
  assert(sweep && std::abs(sweep->fraction - .4) < .02);
  assert(world.clearCapsule({5, 0, 0}, 1, 4));
  assert(!world.clearCapsule({.5, 0, 0}, 1, 4));
  // Parallel contact, retreat and thin-wall/high-speed crossing.
  assert(!world.sweepCapsule({1, 0, 0}, {0, 0, 5}, 1, 4));
  assert(!world.sweepCapsule({1, 0, 0}, {5, 0, 0}, 1, 4));
  assert(world.sweepCapsule({10, 0, 0}, {-1000, 0, 0}, 1, 4));
  assert(!re4::SatCollision::accepts(0x400000, 0x1000));
  assert(!re4::SatCollision::accepts(0x400, 0x1000));
  assert(re4::SatCollision::accepts(0x400, 0x8000));
  assert(!re4::SatCollision::accepts(0x8000, 0x800));
  assert(!re4::SatCollision::accepts(4, 0, 4));
  // Camera sphere: the previous walls-only sweep crossed this floor. A ray
  // would also let the near plane penetrate before the camera center hits.
  auto camera = re4::FollowCameraOffset(world, {5, 5, 0}, {0, -10, 0}, 1);
  assert(camera[1] >= -4 && camera[1] < 0);
  auto freeCamera = re4::FollowCameraOffset(world, {5, 5, 0}, {0, 10, 0}, 1);
  assert(freeCamera == V({0, 10, 0}));
  // At a wall a shoulder camera shortens horizontally, never down to the hip.
  auto shoulder = re4::ShoulderCameraOffset(nullptr, {5, 0, 0}, 4, 0, 1000);
  assert(shoulder == V({900, 0, 1000}));
  auto closeShoulder = re4::ShoulderCameraOffset(&world, {5, 0, 0}, 4,
                                                 3.14159265358979323846, 1000);
  assert(closeShoulder[1] == 0 && closeShoulder[0] > -5);
  assert(std::hypot(closeShoulder[0], closeShoulder[2]) < 1000);
  // Original SEE filter accepts camera-only collision that PL ignores.
  auto cameraOnly = fixture();
  cameraOnly.polygons[0].attributes = cameraOnly.polygons[1].attributes = 0x400;
  re4::SatCollision cameraWorld(cameraOnly);
  assert(!cameraWorld.ground({5, 2, 0}, 1, 5));
  auto seeHit = cameraWorld.sweepCapsule({5, 4, 0}, {0, -10, 0}, 1, 2,
                                         re4::SatSubset::All, 0x8000);
  assert(seeHit && seeHit->fraction < .5);
  re4::WalkProbe walker(world, {1, 4, 10, 1, 2});
  assert(walker.reset());
  const auto beforeLanding = walker.position();
  const auto clearLanding = walker.traversalLanding({5, .2, 0});
  assert(clearLanding && *clearLanding == V({5, 0, 0}));
  const auto contactLanding = walker.traversalLanding({.5, 0, 0});
  assert(contactLanding && (*contactLanding)[0] > 1 &&
         (*contactLanding)[0] - .5 <= walker.config().radius &&
         world.clearCapsule(*contactLanding, 1, 4));
  assert(!walker.traversalLanding({50, 0, 0}) &&
         !walker.traversalLanding({5, -1000, 0}) &&
         !walker.traversalLanding({NAN, 0, 0}));
  assert(walker.position() == beforeLanding); // Prediction never moves the live owner.
  auto start = walker.position();
  for (int i = 0; i < 200; ++i)
    walker.step(1, 0, 0, false, 1. / 60);
  auto moved = walker.position();
  assert(std::abs(moved[0] - start[0]) > 1);
  assert(world.ground(moved, 1, 1));
  for (int i = 0; i < 500; ++i)
    walker.step(1, 0, 0, true, .05);
  assert(walker.edgeBlocked());
  assert(walker.position()[0] <= 20);
  auto before = walker.position();
  walker.step(NAN, 0, 0, false, .1);
  assert(walker.position() == before);
  re4::WalkProbe left(world, {1, 4, 10, 1, 2});
  assert(left.reset());
  for (int i = 0; i < 180; ++i)
    left.step(-1, 0, 0, false, 1. / 60);
  assert(left.position()[0] >= .99 && left.position()[0] < 1.2 &&
         left.wallHit());
  // Slide tangentially at a wall without crossing it.
  double oldZ = left.position()[2];
  for (int i = 0; i < 12; ++i)
    left.step(-1, -.5, 0, false, 1. / 60);
  assert(left.position()[0] >= .99 && left.position()[2] > oldZ + .1);
  re4::WalkProbe hz30(world, {1, 4, 10, 1, 2}), hz60(world, {1, 4, 10, 1, 2});
  assert(hz30.reset() && hz60.reset());
  for (int i = 0; i < 30; ++i)
    hz30.step(1, 0, 0, false, 1. / 30);
  for (int i = 0; i < 60; ++i)
    hz60.step(1, 0, 0, false, 1. / 60);
  assert(std::abs(hz30.position()[0] - hz60.position()[0]) < 1e-7);
  auto filtered = fixture();
  filtered.polygons[0].attributes = filtered.polygons[1].attributes = 0x400000;
  re4::SatCollision disabled(filtered);
  assert(!disabled.ground({5, 2, 0}, 1, 5));
  assert(!disabled.spawn(1, 4));
  auto degenerate = fixture();
  degenerate.vertices.assign(degenerate.vertices.size(), {0, 0, 0});
  re4::SatCollision empty(degenerate);
  assert(!empty.line({0, 5, 0}, {0, -5, 0}));
  assert(!empty.spawn(1, 4));
  bool threw = false;
  try {
    world.ground({0, 0, 0}, -1, 2);
  } catch (const std::runtime_error &) {
    threw = true;
  }
  assert(threw);
  std::cout << "Collision floor/line, capsule sweep, thin wall, filters, edge "
               "guard and walker PASS\n";
}
