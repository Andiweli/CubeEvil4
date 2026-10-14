#include "hit_geometry.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using re4::AuthoredHitShape;
using re4::CollisionVec;
using re4::HitVector;
using re4::Matrix34;
using re4::SatQueryMode;
constexpr Matrix34 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
unsigned checks = 0;
void check(bool value) {
  if (!value)
    std::cerr << "Failed geometry check " << checks + 1 << '\n';
  assert(value);
  ++checks;
}
void point(const std::optional<HitVector> &value, HitVector expected,
           float tolerance = .00001f) {
  check(bool(value));
  for (unsigned i = 0; i < 3; ++i)
    check(std::abs((*value)[i] - expected[i]) <= tolerance);
}
template <class Function> void rejects(Function action) {
  bool rejected = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected);
}
void half(std::vector<std::uint8_t> &bytes, std::size_t offset, unsigned value) {
  bytes.at(offset) = value >> 8;
  bytes.at(offset + 1) = value;
}
void word(std::vector<std::uint8_t> &bytes, std::size_t offset, unsigned value) {
  half(bytes, offset, value >> 16);
  half(bytes, offset + 2, value);
}
// A single vertical EAT/SAT polygon isolates the manager's exclusion rules.
re4::Sat wallFixture(std::uint32_t attributes) {
  std::vector<std::uint8_t> bytes(20 + 36 + 12 + 12 + 20);
  bytes[0] = 32;
  half(bytes, 2, 3);
  half(bytes, 4, 1);
  half(bytes, 6, 1);
  half(bytes, 10, 1);
  half(bytes, 16, 1);
  std::size_t offset = 20;
  for (float value : {0.f, -100.f, -100.f, 0.f, 100.f, -100.f,
                     0.f, 0.f, 100.f, 1.f, 0.f, 0.f}) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    word(bytes, offset, bits);
    offset += 4;
  }
  offset += 12;
  half(bytes, offset + 2, 1);
  half(bytes, offset + 4, 2);
  word(bytes, offset + 16, attributes);
  return re4::Sat(bytes);
}
void primitiveGoldens() {
  const HitVector top{0, 4, 0}, bottom{0, 0, 0};
  // These exact axis-aligned results follow the original float expressions.
  point(re4::HitLineCapsule({-5, 2, 0}, {5, 2, 0}, top, bottom, 1),
        {-1, 2, 0});
  point(re4::HitLineCapsule({5, 2, 0}, {-5, 2, 0}, top, bottom, 1),
        {1, 2, 0});
  point(re4::HitLineCapsule({0, 10, 0}, {0, -10, 0}, top, bottom, 1),
        {0, 5, 0});
  point(re4::HitLineCapsule({0, -10, 0}, {0, 10, 0}, top, bottom, 1),
        {0, -1, 0});
  point(re4::HitLineCapsule({.5f, 2, 0}, {5, 2, 0}, top, bottom, 1),
        {.5f, 2, 0});
  point(re4::HitLineCapsule({0, 4.5f, 0}, {0, 10, 0}, top, bottom, 1),
        {0, 4.5f, 0});
  check(!re4::HitLineCapsule({-5, 2, 1}, {5, 2, 1}, top, bottom, 1));
  check(!re4::HitLineCapsule({-5, 5, 1}, {5, 5, 1}, top, bottom, 1));
  check(!re4::HitLineCapsule({-5, 2, 0}, {-2, 2, 0}, top, bottom, 1));
  check(!re4::HitLineCapsule({2, 2, 0}, {2, 2, 0}, top, bottom, 1));
  point(re4::HitLineCapsule({-5, 0, 0}, {5, 0, 0}, bottom, bottom, 1),
        {-1, 0, 0});
  // The source checks top cap first, preserving its result for downward axes.
  point(re4::HitLineCapsule({0, -10, 0}, {0, 10, 0}, bottom, top, 1),
        {0, -1, 0});
  const HitVector origin{0, 0, 0};
  point(re4::HitLineCube({0, 2, 5}, {0, 2, -5}, identity, origin, 1, 4, 1),
        {0, 2, 1});
  point(re4::HitLineCube({5, 2, 0}, {-5, 2, 0}, identity, origin, 1, 4, 1),
        {1, 2, 0});
  point(re4::HitLineCube({0, 2, -5}, {0, 2, 5}, identity, origin, 1, 4, 1),
        {0, 2, -1});
  point(re4::HitLineCube({-5, 2, 0}, {5, 2, 0}, identity, origin, 1, 4, 1),
        {-1, 2, 0});
  point(re4::HitLineCube({0, 10, 0}, {0, -10, 0}, identity, origin, 1, 4, 1),
        {0, 4, 0});
  point(re4::HitLineCube({0, -10, 0}, {0, 10, 0}, identity, origin, 1, 4, 1),
        {0, 0, 0});
  // Unlike capsules, the original ordered quad check has no inside-origin hit.
  check(!re4::HitLineCube({0, 2, 0}, {5, 2, 0}, identity, origin, 1, 4, 1));
  check(!re4::HitLineCube({0, 2, 5}, {0, 2, 1}, identity, origin, 1, 4, 1));
  check(!re4::HitLineCube({0, 2, 1}, {0, 2, -5}, identity, origin, 1, 4, 1));
  check(!re4::HitLineCube({0, 2, 5}, {0, 2, -5}, identity, origin, 0, 4, 1));
}
void authoredGoldens() {
  auto joint = identity, placement = identity;
  joint[3] = 3;
  placement[3] = 10;
  AuthoredHitShape sphere{{0, 0, 0}, 1, 0, 0, re4::HitOn, 1};
  auto hit = re4::HitAuthoredShapes({sphere}, {joint}, placement,
                                   {0, 0, 0}, {20, 0, 0}, 400);
  check(hit && hit->partIndex == 0 && hit->lenSq == 144 &&
        hit->cross == HitVector({12, 0, 0}));
  sphere.partsNo = 0;
  hit = re4::HitAuthoredShapes({sphere}, {joint}, placement,
                              {0, 0, 0}, {20, 0, 0}, 400);
  check(hit && hit->lenSq == 81 && hit->cross[0] == 9);
  // Raw joint matrices already contain the hierarchy; model placement once.
  sphere.partsNo = 2;
  auto second = identity;
  second[3] = 6;
  hit = re4::HitAuthoredShapes({sphere}, {joint, second}, placement,
                              {0, 0, 0}, {20, 0, 0}, 400);
  check(hit && hit->cross[0] == 15);
  sphere.partsNo = 0;
  auto farther = sphere;
  farther.offset[0] = 4;
  hit = re4::HitAuthoredShapes({farther, sphere}, {}, identity,
                              {-10, 0, 0}, {10, 0, 0}, 400);
  check(hit && hit->partIndex == 1 && hit->lenSq == 81);
  hit = re4::HitAuthoredShapes({sphere, sphere}, {}, identity,
                              {-10, 0, 0}, {10, 0, 0}, 81);
  check(hit && hit->partIndex == 1); // Equal distance replaces preceding part.
  check(!re4::HitAuthoredShapes({sphere}, {}, identity,
                                {-10, 0, 0}, {10, 0, 0}, 80.99f));
  sphere.flags = re4::HitOn | re4::HitIgnoreHandguns;
  for (unsigned weapon : {1u, 2u, 3u, 4u, 0x11u, 0x26u, 0x2bu})
    check(!re4::HitAuthoredShapes({sphere}, {}, identity,
                                  {-10, 0, 0}, {10, 0, 0}, 400, weapon));
  check(bool(re4::HitAuthoredShapes({sphere}, {}, identity,
                                    {-10, 0, 0}, {10, 0, 0}, 400, 5)));
  sphere.flags = re4::HitThrough | re4::HitDamagePosition;
  check(!re4::HitAuthoredShapes({sphere}, {}, identity,
                                {-10, 0, 0}, {10, 0, 0}, 400));
  // Original radius uses the matrix's transformed local Z vector, not X/Y.
  auto scaled = identity;
  scaled[0] = 2;
  scaled[5] = 3;
  scaled[10] = 4;
  sphere.flags = re4::HitOn;
  hit = re4::HitAuthoredShapes({sphere}, {}, scaled,
                              {-10, 0, 0}, {10, 0, 0}, 400);
  check(hit && hit->cross[0] == -4 && hit->lenSq == 36);
  AuthoredHitShape alongX{{0, 0, 0}, 1, 4, 0,
                          re4::HitOn | re4::HitXAxis, 0};
  Matrix34 yaw90{0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0};
  hit = re4::HitAuthoredShapes({alongX}, {}, yaw90,
                              {-5, 0, -2}, {5, 0, -2}, 100);
  check(hit && std::abs(hit->cross[0] + 1) < .00001f &&
        std::abs(hit->cross[2] + 2) < .00001f);
  auto alongZ = alongX;
  alongZ.flags = re4::HitOn | re4::HitZAxis;
  hit = re4::HitAuthoredShapes({alongZ}, {}, identity,
                              {-5, 0, 2}, {5, 0, 2}, 100);
  check(hit && std::abs(hit->cross[0] + 1) < .00001f);
  AuthoredHitShape cube{{3, 0, 0}, 1, 4, 1,
                        re4::HitOn | re4::HitCube, 0};
  hit = re4::HitAuthoredShapes({cube}, {}, placement,
                              {20, 2, 0}, {0, 2, 0}, 400);
  check(hit && hit->cross == HitVector({14, 2, 0}) && hit->lenSq == 36);
  const auto normal = re4::NormalGanadoHitShapes();
  check(normal.size() == 11 && normal[0].partsNo == 5 &&
        normal[0].radius == 160 && normal[0].height == 110);
  check(normal[1].partsNo == 2 && normal[1].radius == 210 &&
        normal[1].offset[1] == -30 && normal[1].height == 290);
  check(normal[2].partsNo == 0x14 && normal[2].radius == 150 &&
        normal[3].partsNo == 0x18 && normal[3].radius == 150);
  check(normal[6].radius == 170 && normal[7].radius == 170 &&
        normal[8].partsNo == 8 && normal[9].partsNo == 0x0e);
  check(normal[10].partsNo == 5 && normal[10].radius == 300 &&
        !(normal[10].flags & re4::HitOn));
}
void managerFilters() {
  using Query = re4::SatCollision;
  check(!Query::accepts(0x400, 0));
  check(Query::accepts(0x400, 0, 0, SatQueryMode::Effect));
  check(!Query::accepts(0x400000, 0, 0x400000, SatQueryMode::Effect));
  check(Query::accepts(0x400000, 0, 0, SatQueryMode::Effect));
  check(!Query::accepts(0x400000, 0x800, 0, SatQueryMode::Effect));
  check(Query::accepts(0x400000, 0x400, 0, SatQueryMode::Effect));
  check(!Query::accepts(0x4000, 0x400, 0, SatQueryMode::Effect));
  check(Query::accepts(0x4000, 0x800, 0, SatQueryMode::Effect));
  check(Query::accepts(0x8000, 0x800, 0, SatQueryMode::Effect));
  check(!Query::accepts(0x8000, 0x800));
  check(Query::accepts(0x400000, 0x800));
  auto eat = wallFixture(0x400);
  Query sight(eat), effect(eat, SatQueryMode::Effect);
  check(!sight.line({5, 0, 0}, {-5, 0, 0}, re4::SatSubset::All, 0));
  auto hit = effect.line({5, 0, 0}, {-5, 0, 0}, re4::SatSubset::All, 0,
                         0x400000);
  check(hit && hit->fraction == .5 && hit->attributes == 0x01000400);
  check(!effect.line({5, 0, 0}, {-5, 0, 0}, re4::SatSubset::All, 0, 0x400));
  // Clip the authored hit distance at the wall: enemy behind it cannot qualify.
  AuthoredHitShape sphere{{-3, 0, 0}, 1, 0, 0, re4::HitOn, 0};
  check(!re4::HitAuthoredShapes({sphere}, {}, identity,
                                {5, 0, 0}, {-10, 0, 0}, 25));
  sphere.offset[0] = 3;
  check(bool(re4::HitAuthoredShapes({sphere}, {}, identity,
                                    {5, 0, 0}, {-10, 0, 0}, 25)));
  rejects([&] { Query invalid(eat, static_cast<SatQueryMode>(99)); });
}
void malformed() {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();
  for (float invalid : {nan, inf, -1.f, 1000001.f}) {
    rejects([&] { re4::HitLineCapsule({0, 0, 0}, {5, 0, 0},
                                      {0, 4, 0}, {0, 0, 0}, invalid); });
    rejects([&] { re4::HitLineCube({0, 0, 5}, {0, 0, -5}, identity,
                                   {0, 0, 0}, invalid, 4, 1); });
  }
  for (float invalid : {nan, inf, 10000001.f, -10000001.f}) {
    rejects([&] { re4::HitLineCapsule({invalid, 0, 0}, {5, 0, 0},
                                      {0, 4, 0}, {0, 0, 0}, 1); });
    auto matrix = identity;
    matrix[5] = invalid;
    rejects([&] { re4::HitLineCube({0, 0, 5}, {0, 0, -5}, matrix,
                                   {0, 0, 0}, 1, 4, 1); });
  }
  AuthoredHitShape shape{{0, 0, 0}, 1, 4, 0, re4::HitOn, 0};
  auto query = [&](const std::vector<AuthoredHitShape> &shapes,
                   const std::vector<Matrix34> &joints = {}) {
    re4::HitAuthoredShapes(shapes, joints, identity,
                           {-5, 2, 0}, {5, 2, 0}, 100);
  };
  for (int index : {-1, 1, 249}) {
    shape.partsNo = index;
    rejects([&] { query({shape}); });
  }
  shape.partsNo = 0;
  shape.flags = 0x200;
  rejects([&] { query({shape}); });
  shape.flags = re4::HitOn;
  rejects([&] { query(std::vector<AuthoredHitShape>(257, shape)); });
  rejects([&] { query({shape}, std::vector<Matrix34>(249, identity)); });
  shape.offset[0] = nan;
  rejects([&] { query({shape}); });
  shape.offset[0] = 0;
  shape.height = -1;
  rejects([&] { query({shape}); });
  shape.height = 4;
  auto matrix = identity;
  matrix[0] = nan;
  rejects([&] { query({shape}, {matrix}); });
  for (float limit : {nan, inf, -1.f, 1.1e16f})
    rejects([&] { re4::HitAuthoredShapes({shape}, {}, identity,
                                        {-5, 2, 0}, {5, 2, 0}, limit); });
  rejects([&] { re4::HitAuthoredShapes({shape}, {}, identity,
                                      {-5, 2, 0}, {5, 2, 0}, 100, 0x2e); });
  rejects([&] { re4::HitAuthoredShapes({shape}, {}, identity,
                                      {NAN, 2, 0}, {5, 2, 0}, 100); });
  check(!re4::HitAuthoredShapes({}, {}, identity,
                                {-5, 2, 0}, {5, 2, 0}, 100));
}
void realEat(const char *path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
    throw std::runtime_error("EAT fixture missing");
  stream.seekg(0, std::ios::end);
  const auto size = stream.tellg();
  check(size > 20 && size < 32 * 1024 * 1024);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  stream.seekg(0);
  stream.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
  check(bool(stream));
  re4::Sat eat(bytes);
  re4::SatCollision world(eat, SatQueryMode::Effect);
  check(eat.offsetSlots == 1 && eat.polygons.size() > 1000);
  unsigned rays = 0;
  for (std::size_t i = 0; i < eat.polygons.size() && rays < 64; ++i) {
    if (!re4::SatCollision::accepts(eat.polygons[i].attributes, 0, 0x400000,
                                   SatQueryMode::Effect))
      continue;
    const auto triangle = world.triangle(i);
    CollisionVec center{}, normal{};
    for (unsigned axis = 0; axis < 3; ++axis)
      center[axis] = (triangle[0][axis] + triangle[1][axis] +
                      triangle[2][axis]) / 3;
    const auto a = triangle[0], b = triangle[1], c = triangle[2];
    normal = {(b[1] - a[1]) * (c[2] - a[2]) -
                  (b[2] - a[2]) * (c[1] - a[1]),
              (b[2] - a[2]) * (c[0] - a[0]) -
                  (b[0] - a[0]) * (c[2] - a[2]),
              (b[0] - a[0]) * (c[1] - a[1]) -
                  (b[1] - a[1]) * (c[0] - a[0])};
    const double length = std::sqrt(normal[0] * normal[0] +
                                     normal[1] * normal[1] +
                                     normal[2] * normal[2]);
    if (length < .01)
      continue;
    CollisionVec from = center, to = center;
    for (unsigned axis = 0; axis < 3; ++axis) {
      from[axis] += normal[axis] / length * 50;
      to[axis] -= normal[axis] / length * 50;
    }
    const auto hit = world.line(from, to, re4::SatSubset::All, 0, 0x400000);
    check(hit && hit->fraction <= .50001 && !(hit->attributes & 0x400000));
    ++rays;
  }
  check(rays == 64);
  std::cout << "Original EAT " << path << ": " << eat.polygons.size()
            << " polygons / " << rays << " clipped rays PASS\n";
}
} // namespace
int main(int argc, char **argv) {
  primitiveGoldens();
  authoredGoldens();
  managerFilters();
  malformed();
  for (int i = 1; i < argc; ++i)
    realEat(argv[i]);
  std::cout << "Original authored capsule/cube, 1-based posed parts, nearest "
               "ties, EAT filters and bounded malformed input: "
            << checks << " checks PASS\n";
}
