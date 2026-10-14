#include "ambient_animals.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using Bytes = std::vector<std::uint8_t>;
void half(Bytes &bytes, std::size_t p, unsigned n) {
  bytes[p] = n >> 8;
  bytes[p + 1] = n;
}
Bytes fixture() {
  Bytes bytes(re4::EnemySpawns::FileBytes);
  for (unsigned i : {0u, 1u, 110u, 111u, 112u, 113u}) {
    const auto p = i * re4::EnemySpawns::RecordBytes;
    bytes[p] = 1;
    bytes[p + 1] = 0x28;
    bytes[p + 2] = i == 0 || i == 112;
    half(bytes, p + 8, 1000);
    half(bytes, p + 12, 100 + i);
    half(bytes, p + 14, i);
    half(bytes, p + 16, -100 - i);
    half(bytes, p + 20, 0x2000);
    half(bytes, p + 24, 0x400);
    bytes[p + 31] = i;
  }
  // A supported room contains other EM ids. None are chicken placeholders.
  bytes[2 * 32] = 1;
  bytes[2 * 32 + 1] = 0x16;
  half(bytes, 2 * 32 + 8, 1000);
  half(bytes, 2 * 32 + 24, 0x400);
  return bytes;
}
void reject(const Bytes &bytes) {
  bool rejected = false;
  try {
    (void)re4::VillageChickenSpawns(re4::EnemySpawns(bytes));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);
}
} // namespace
int main() {
  auto bytes = fixture();
  const auto original = re4::EnemySpawns(bytes);
  const auto selected = re4::VillageChickenSpawns(original);
  assert(selected.size() == 6);
  const std::array<unsigned, 6> indices{0, 1, 110, 111, 112, 113};
  for (unsigned i = 0; i < selected.size(); ++i) {
    const auto &s = selected[i];
    assert(s.listIndex == indices[i]);
    assert(s.alive() && !s.alreadySet());
    assert(s.position[0] == 1000 + 10 * indices[i]);
    assert(s.position[2] == -1000. - 10. * indices[i]);
    assert(s.heading == 3.14159265358979323846 / 2);
    assert(s.hp == 1000 && s.room == 0x400 && s.id == 0x28);
    assert(s.raw == original.find(indices[i])->raw);
  }
  bytes[0] = 0;
  assert(selected[0].alive()); // Source records have owning storage.
  reject(bytes);
  bytes = fixture();
  bytes[0] = 3;
  reject(bytes); // Already instantiated.
  bytes = fixture();
  bytes[2] = 2;
  reject(bytes); // Unsupported skin variant.
  bytes = fixture();
  bytes[8] = bytes[9] = 0;
  reject(bytes); // No live HP.
  bytes = fixture();
  half(bytes, 24, 0x100);
  reject(bytes);
  bytes = fixture();
  bytes[3 * 32] = 1;
  bytes[3 * 32 + 1] = 0x28;
  half(bytes, 3 * 32 + 8, 1);
  half(bytes, 3 * 32 + 24, 0x400);
  reject(bytes); // Extra active chicken never exceeds the profile's budget.
  reject(Bytes());
  reject(Bytes(re4::EnemySpawns::FileBytes + 1));
  const std::array<unsigned, 5> entries{8, 9, 20, 10, 11};
  const std::array<unsigned, 5> blends{30, 10, 10, 10, 10};
  const std::array<unsigned, 5> flags{5, 1, 1, 5, 5};
  for (unsigned i = 0; i < entries.size(); ++i) {
    const auto s = re4::ChickenMotionSource(re4::AnimalMotion(i));
    assert(s.motionEntry == entries[i] && s.blendFrames == blends[i] &&
           s.flags == flags[i]);
  }
  bool rejected = false;
  try {
    (void)re4::ChickenMotionSource(re4::AnimalMotion(255));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);
  std::cout << "r400 chicken ESL ownership, exact metadata and budgets PASS\n";
}
