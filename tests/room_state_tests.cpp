#include "room_state.h"
#include "walk_probe.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
namespace {
template <class F> bool rejects(F f) {
  try { f(); return false; } catch (const std::exception &) { return true; }
}
void half(std::vector<std::uint8_t> &b, unsigned p, unsigned x) {
  b.at(p) = x >> 8; b.at(p + 1) = x;
}
void scalar(std::vector<std::uint8_t> &b, unsigned p, float x) {
  std::uint32_t bits;
  std::memcpy(&bits, &x, 4);
  half(b, p, bits >> 16); half(b, p + 2, bits);
}
re4::Sat floorFixture() {
  // Dynamic ETS walls cannot add/remove this independently authored floor.
  std::vector<std::uint8_t> bytes(20 + 4 * 12 + 12 + 12 + 40);
  bytes[0] = 32;
  half(bytes, 2, 4); half(bytes, 4, 1); half(bytes, 6, 1);
  half(bytes, 10, 2); half(bytes, 12, 2);
  unsigned p = 20;
  for (const auto v : {std::array<float,3>{-30,0,-30}, {30,0,-30},
                       {30,0,30}, {-30,0,30}})
    for (auto x : v) { scalar(bytes, p, x); p += 4; }
  scalar(bytes, p + 4, 1); p += 24;
  for (auto v : {std::array<unsigned,3>{0,2,1}, {0,3,2}}) {
    for (unsigned k = 0; k < 3; ++k) half(bytes, p + 2 * k, v[k]);
    p += 20;
  }
  return re4::Sat(bytes);
}
} // namespace
int main() {
  re4::SourceFlags<8> flags;
  for (unsigned i : {0u,31u,32u,63u,64u,127u,255u}) flags.set(i);
  assert(flags.words()[0] == 0x80000001u && flags.words()[1] == 0x80000001u);
  assert(flags.words()[2] == 0x80000000u && flags.words()[3] == 1 && flags.words()[7] == 1);
  flags.set(31, false);
  assert(flags.test(0) && !flags.test(31) && flags.test(255));
  const auto before = flags.words();
  assert(rejects([&]{flags.set(256);}) && flags.words() == before);
  assert(rejects([&]{flags.test(std::numeric_limits<unsigned>::max());}));
  re4::ScenarioState state;
  state.scenario.set(18); state.key.set(63);
  state.transient.set(0); state.transient.set(63); state.transient.set(64);
  state.transient.set(127); state.transient.clearFrameWords();
  assert(state.transient.test(0) && state.transient.test(63));
  assert(!state.transient.test(64) && !state.transient.test(127));
  auto &village = state.room(0x101);
  assert(re4::VillageProfile(village).hiddenLadders);
  assert(re4::VillageProfile(village).protectedWindows);
  village.room.set(8);
  assert(!re4::VillageProfile(village).hiddenLadders && !re4::VillageProfile(village).protectedWindows);
  village.room.set(8, false); village.room.set(7);
  assert(re4::VillageProfile(village).hiddenLadders && !re4::VillageProfile(village).protectedWindows);
  village.setObject(11, 6); village.setObject(8, 1);
  assert(village.object(11, 2) && village.object(11, 4) && !village.object(11, 1));
  assert(state.scenario.test(18) && state.key.test(63) && !state.room(0x400).object(8, 1));
  assert(rejects([&]{village.setObject(64, 1);}) && rejects([&]{village.object(0, 0);}));
  assert(rejects([&]{state.room(0x800);}) && rejects([&]{village.room.set(32);}));
  const auto copy = state;
  state.room(0x101).setObject(8, 1, false);
  assert(copy.room(0x101).object(8, 1) && !state.room(0x101).object(8, 1));
  using Triangle = std::array<std::array<float,3>,3>;
  const std::vector<Triangle> plane{{{{5,0,-10},{5,10,-10},{5,10,10}}},
                                   {{{5,0,-10},{5,10,10},{5,0,10}}}};
  const auto mesh = re4::Sat::DynamicWalls(plane);
  const re4::SatCollision wall(mesh);
  assert(mesh.floor == 0 && mesh.wall == 2 && !wall.ground({5,2,0}, 3, 3));
  const auto floor = floorFixture();
  const re4::SatCollision world(floor);
  re4::WalkProbe probe(world, {1,4,10,1,2});
  probe.setObjectCollision(&wall);
  assert(probe.place({10,0,0}));
  probe.moveDelta({-8,0,0});
  assert(probe.position()[0] >= 5.99 && probe.position()[0] < 6.1 && probe.wallHit());
  const auto blocked = probe.position();
  assert(!probe.place({5.5,0,0}) && probe.position() == blocked);
  probe.moveDelta({-1,0,4});
  assert(probe.position()[0] >= 5.99 && probe.position()[2] > 3.9);
  probe.setObjectCollision(nullptr);
  probe.moveDelta({-4,0,0});
  assert(probe.position()[0] < 2.1 && probe.position()[1] == 0);
  auto bad = plane;
  bad[0][0][0] = std::numeric_limits<float>::quiet_NaN();
  assert(rejects([&]{re4::Sat::DynamicWalls(bad);}));
  bad = plane; bad[0][2] = bad[0][1];
  assert(rejects([&]{re4::Sat::DynamicWalls(bad);}));
  bad = plane; for(auto &v : bad[0]) v[1] = 0;
  assert(rejects([&]{re4::Sat::DynamicWalls(bad);}));
  assert(re4::Sat::DynamicWalls({}).polygons.empty());
  assert(rejects([&]{re4::Sat::DynamicWalls(std::vector<Triangle>(1025, plane[0]));}));
  std::cout << "MSB source flags, separate RSF/ETC/frame words, copy isolation and dynamic wall/body contracts PASS\n";
}
