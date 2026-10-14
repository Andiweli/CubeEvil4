#include "room_flow.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
using Bytes = std::vector<std::uint8_t>;
using V = re4::CollisionVec;
namespace {
constexpr std::size_t rec = 16;
void half(Bytes &b, std::size_t p, unsigned x) {
  b.at(p) = x >> 8;
  b.at(p + 1) = x;
}
void word(Bytes &b, std::size_t p, std::uint32_t x) {
  half(b, p, x >> 16);
  half(b, p + 2, x);
}
void scalar(Bytes &b, std::size_t p, float f) {
  std::uint32_t bits;
  std::memcpy(&bits, &f, 4);
  word(b, p, bits);
}
Bytes aev(unsigned shape = 1) {
  Bytes b(16 + 156);
  b[0] = 'A';
  b[1] = 'E';
  b[2] = 'V';
  half(b, 4, 0x104);
  half(b, 6, 1);
  b[rec + 4] = 1;
  b[rec + 5] = shape;
  scalar(b, rec + 12, 1000);
  scalar(b, rec + 16, 1000);
  if (shape == 1) {
    constexpr float xy[] = {-1000, -1000, 1000, -1000, 1000, 1000, -1000, 1000};
    for (int k = 0; k < 8; ++k)
      scalar(b, rec + 20 + 4 * k, xy[k]);
  }
  b[rec + 0x34] = 3;
  b[rec + 0x35] = 1;
  b[rec + 0x37] = 1;
  b[rec + 0x38] = 8;
  b[rec + 0x39] = 1;
  b[rec + 0x44] = 2;
  b[rec + 0x49] = 45;
  scalar(b, rec + 0x5c, 100);
  scalar(b, rec + 0x60, -7000);
  scalar(b, rec + 0x64, 200);
  scalar(b, rec + 0x68, -2.4f);
  half(b, rec + 0x6c, 0x301);
  return b;
}
Bytes info() {
  // Four stage offsets; stage 3 contains two original 32-byte records.
  Bytes b(88 + 9);
  word(b, 0, 4);
  word(b, 16, 20);
  word(b, 20, 2);
  for (unsigned i = 0; i < 2; ++i) {
    std::size_t p = 24 + i * 32;
    half(b, p, i ? 0 : 1);
    half(b, p + 2, 0x300);
    scalar(b, p + 4, i ? 200 : 100);
    scalar(b, p + 8, -17000);
    scalar(b, p + 12, -40000);
    scalar(b, p + 16, -2.49f);
    word(b, p + 20, 88);
  }
  const char name[] = "original";
  std::copy_n(name, sizeof(name), b.begin() + 88);
  return b;
}
void fails(const std::function<void()> &f) {
  bool failed = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  assert(failed);
}
void badAev(const Bytes &b) {
  fails([&] { re4::RoomFlow flow(0x300, b); });
}
void badInfo(const Bytes &b) {
  fails([&] { re4::RoomInfo starts(b); });
}
} // namespace
int main() {
  // Fixed AEV ladders preserve disk data and signed levels; SceAtRoomSet
  // only normalizes action/priority when ACT_GO_UP/DOWN is absent.
  auto ladderBytes = aev();
  ladderBytes[rec + 0x35] = 0x10;
  ladderBytes[rec + 0x6c] = std::uint8_t(-9);
  ladderBytes[rec + 0x6d] = 0;
  ladderBytes[rec + 0x6e] = 1;
  re4::RoomFlow ladderFlow(0x101, ladderBytes);
  assert(ladderFlow.doors().empty() && ladderFlow.ladders().size() == 1);
  const auto &ladder = ladderFlow.ladders().front();
  assert(ladder.level == -9 && ladder.posSet == 1 && ladder.type == 0);
  assert(ladder.trigger.triggerType == 8 && ladder.trigger.priority == 2);
  assert(std::equal(ladder.trigger.raw.begin(), ladder.trigger.raw.end(),
                    ladderBytes.begin() + rec));
  ladderBytes[rec + 0x38] = 0x82;
  re4::RoomFlow normalized(0x400, ladderBytes);
  assert(normalized.ladders().front().trigger.triggerType == 0x88);
  assert(normalized.ladders().front().trigger.priority == 5);
  auto malformedLadder = ladderBytes;
  scalar(malformedLadder, rec + 0x68, INFINITY);
  badAev(malformedLadder);
  malformedLadder = ladderBytes;
  malformedLadder.pop_back();
  badAev(malformedLadder);
  auto b = aev();
  re4::RoomFlow flow(0x300, b);
  assert(flow.recordCount() == 1 && flow.doors().size() == 1);
  auto d = flow.find(0);
  assert(d && !flow.find(1) && d->destinationRoom == 0x301);
  assert(d->destination == V({100, -7000, 200}));
  assert(std::abs(d->destinationHeading + 2.4) < 1e-6);
  assert(std::equal(d->raw.begin(), d->raw.end(), b.begin() + 16));
  auto q = flow.select({0, 0, 0}, 0, false, true);
  assert(q.door == d && q.available && !q.activated && q.reason.empty());
  q = flow.select({0, 0, 0}, 0, true, true);
  assert(q.available && q.activated);
  assert(!flow.select({3000, 0, 0}, 0, true, true).door);
  assert(!flow.select({NAN, 0, 0}, 0, true, true).door);
  assert(!flow.select({0, 0, 0}, INFINITY, true, true).door);
  assert(!flow.select({0, 0, 0}, 0, true, true, V{NAN, 0, 0}).door);
  // Floor band and prism edge boundaries are the original area.cpp tests.
  assert(re4::RoomFlow::hit(*d, {0, -350, 0}, 0));
  assert(!re4::RoomFlow::hit(*d, {0, -350.01, 0}, 0));
  assert(!re4::RoomFlow::hit(*d, {0, 750, 0}, 0));
  assert(re4::RoomFlow::hit(*d, {1000, 0, 0}, 0));
  assert(!re4::RoomFlow::hit(*d, {1000.01, 0, 0}, 0));
  // Original front point: 550 units along +Z at heading 0. SAT may shorten it.
  assert(!re4::RoomFlow::hit(*d, {0, 0, 700}, 0));
  assert(re4::RoomFlow::hit(*d, {0, 0, 700}, 0, V{0, 250, 999}));
  auto c = aev(2);
  c[rec + 0x37] = 0;
  re4::RoomFlow cylinder(0x300, c);
  assert(cylinder.select({999, 0, 0}, 0, true, true).door);
  assert(!cylinder.select({1000, 0, 0}, 0, true, true).door);
  assert(!cylinder.select({0, 750, 0}, 0, true, true).door);
  // Signed degrees*2 and wrapping across +/-pi, independent of front point.
  c[rec + 0x37] = 2;
  c[rec + 0x48] = std::uint8_t(-45);
  re4::RoomFlow facing(0x300, c);
  assert(facing.find(0)->hitDirection == -45);
  assert(facing.select({0, 0, 0}, 0, true, true).door);
  assert(!facing.select({0, 0, 0}, .001, true, true).door);
  c[rec + 0x48] = 90;
  c[rec + 0x49] = 10;
  re4::RoomFlow wrapped(0x300, c);
  assert(wrapped.select({0, 0, 0}, -3.141592653589793 + .2, true, true).door);
  assert(!wrapped.select({0, 0, 0}, -3.141592653589793 + .5, true, true).door);
  // Parsed original doors outside the audited loop must remain unavailable.
  re4::RoomFlow unported(0x100, b);
  q = unported.select({0, 0, 0}, 0, true, true);
  assert(q.door && !q.available && !q.reason.empty());
  // r100's forward forest exit is an authored held-A area, independent of
  // the RSF gates which lock the first house's ETS leaf. Preserve held-input
  // activation; the K15 village graph also supports the original return door.
  auto forest = b;
  half(forest, rec + 0x6c, 0x101);
  forest[rec + 0x38] = 2;
  re4::RoomFlow forwardForest(0x100, forest);
  q = forwardForest.select({0, 0, 0}, 0, false, true);
  assert(q.door && q.available && q.activated);
  q = forwardForest.select({0, 0, 0}, 0, true, false);
  assert(q.available && q.activated);
  q = forwardForest.select({0, 0, 0}, 0, false, false);
  assert(q.available && !q.activated);
  half(forest, rec + 0x6c, 0x100);
  re4::RoomFlow reverseForest(0x101, forest);
  assert(reverseForest.select({0, 0, 0}, 0, true, true).available);
  half(forest, rec + 0x6c, 0x102);
  re4::RoomFlow wrongForest(0x100, forest);
  assert(!wrongForest.select({0, 0, 0}, 0, true, true).available);
  half(forest, rec + 0x6c, 0x101);
  forest[rec + 0x38] = 7;
  re4::RoomFlow wrongForestTrigger(0x100, forest);
  assert(!wrongForestTrigger.select({0, 0, 0}, 0, true, true).available);
  for (auto offset : {0x6eu, 0x52u, 0x46u, 0x3au, 0x45u, 0x74u}) {
    auto bad = b;
    bad[rec + offset] = 1;
    re4::RoomFlow refused(0x300, bad);
    assert(!refused.select({0, 0, 0}, 0, true, true).available);
  }
  for (auto offset : {0x3cu, 0x40u, 0x70u, 0x78u}) {
    auto bad = b;
    word(bad, rec + offset, 0x80001234);
    re4::RoomFlow refused(0x300, bad);
    assert(!refused.select({0, 0, 0}, 0, true, true).available);
  }
  auto parented = b;
  word(parented, rec + 0x4c, 0x80001234);
  re4::RoomFlow parent(0x300, parented);
  assert(!parent.select({0, 0, 0}, 0, true, true).door);
  auto unsupported = b;
  unsupported[rec + 0x38] = 2;
  re4::RoomFlow heldAction(0x300, unsupported);
  q = heldAction.select({0, 0, 0}, 0, false, true);
  assert(q.door && q.activated && !q.available);
  auto disabled = b;
  disabled[rec + 0x34] = 2;
  re4::RoomFlow off(0x300, disabled);
  assert(!off.select({0, 0, 0}, 0, true, true).door);
  auto eye = b;
  eye[rec + 5] = 3;
  re4::RoomFlow viewCone(0x300, eye);
  assert(!viewCone.select({0, 0, 0}, 0, true, true).door);
  auto tiny = b;
  tiny.resize(tiny.size() - 1);
  badAev(tiny);
  for (auto offset : {0u, 3u, 4u}) {
    auto bad = b;
    bad[offset] ^= 1;
    badAev(bad);
  }
  auto count = b;
  half(count, 6, 257);
  badAev(count);
  auto nan = b;
  scalar(nan, rec + 0x5c, NAN);
  badAev(nan);
  auto inf = b;
  scalar(inf, rec + 20, INFINITY);
  badAev(inf);
  auto huge = b;
  scalar(huge, rec + 8, 2e7f);
  badAev(huge);
  auto negative = b;
  scalar(negative, rec + 16, -1);
  badAev(negative);
  auto priority = b;
  priority[rec + 0x44] = 16;
  badAev(priority);
  auto duplicate = b;
  duplicate.insert(duplicate.end(), b.begin() + 16, b.end());
  half(duplicate, 6, 2);
  badAev(duplicate);
  // Original OT priority and source indices remain deterministic.
  duplicate[16 + 156 + 0x36] = 1;
  duplicate[16 + 156 + 0x44] = 3;
  re4::RoomFlow sorted(0x300, duplicate);
  assert(sorted.doors()[0].no == 1 && sorted.doors()[0].recordIndex == 1);
  assert(!sorted.select({0, 0, 0}, 0, true, true).available);
  // RoomInfo offsets and GC pointer-size independence.
  auto ri = info();
  re4::RoomInfo starts(ri);
  assert(starts.stageCount() == 4 && starts.starts().size() == 2);
  auto start = starts.find(0x300);
  assert(start && start->position == V({100, -17000, -40000}));
  assert(start->point == 0 && start->name == "original");
  assert(!starts.find(0x300, 1) && !starts.find(0x301));
  assert(starts.starts()[1].point == 1 && !starts.starts()[1].valid());
  assert(std::equal(start->raw.begin(), start->raw.end(), ri.begin() + 24));
  auto rbad = ri;
  word(rbad, 0, 0);
  badInfo(rbad);
  rbad = ri;
  word(rbad, 0, 17);
  badInfo(rbad);
  rbad = ri;
  word(rbad, 16, 19);
  badInfo(rbad);
  rbad = ri;
  word(rbad, 16, 0xfffffffcu);
  badInfo(rbad);
  rbad = ri;
  word(rbad, 20, 257);
  badInfo(rbad);
  rbad = ri;
  half(rbad, 26, 0x200);
  badInfo(rbad);
  rbad = ri;
  scalar(rbad, 28, NAN);
  badInfo(rbad);
  rbad = ri;
  word(rbad, 44, 24);
  badInfo(rbad);
  rbad = ri;
  word(rbad, 44, ri.size());
  badInfo(rbad);
  rbad = ri;
  rbad.back() = 'x';
  badInfo(rbad);
  rbad = ri;
  word(rbad, 12, 20);
  badInfo(rbad);
  rbad = ri;
  rbad.resize(87);
  badInfo(rbad);
  std::cout << "RoomInfo bounds / GC records, original quad/cylinder/front/"
               "facing tests, action edges and conservative door scope PASS\n";
}
