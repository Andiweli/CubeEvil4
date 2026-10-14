#include "enemy_spawn.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using Bytes = std::vector<std::uint8_t>;
namespace {
void half(Bytes &b, std::size_t p, unsigned n) {
  b.at(p) = n >> 8;
  b.at(p + 1) = n;
}
void rejects(std::size_t length) {
  bool rejected = false;
  try {
    re4::EnemySpawns records(Bytes(length, 0));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);
}
} // namespace
int main(int argc, char **argv) {
  rejects(0);
  rejects(31);
  rejects(re4::EnemySpawns::FileBytes - 1);
  rejects(re4::EnemySpawns::FileBytes + 1);
  rejects(256 * 32);
  rejects(1024 * 1024);

  Bytes b(re4::EnemySpawns::FileBytes, 0);
  // Last real record tests complete decoding at the exact file boundary.
  constexpr std::size_t p = 254 * 32;
  b[p] = 0x0d;
  b[p + 1] = 0x12;
  b[p + 2] = 0;
  b[p + 3] = 0x1b;
  b[p + 4] = 0x81;
  b[p + 5] = 0x23;
  b[p + 6] = 0x45;
  b[p + 7] = 0x67;
  half(b, p + 8, 0xffff);
  b[p + 10] = 0x34;
  b[p + 11] = 2;
  half(b, p + 12, 0x8000);
  half(b, p + 14, 0x7fff);
  half(b, p + 16, 0xffff);
  half(b, p + 18, 0x8000);
  half(b, p + 20, 0x2000);
  half(b, p + 22, 0xffff);
  half(b, p + 24, 0x0301);
  half(b, p + 26, 0xfffe);
  b[p + 28] = 0xde;
  b[p + 29] = 0xad;
  b[p + 30] = 0xbe;
  b[p + 31] = 0xef;
  re4::EnemySpawns records(b);
  assert(records.records().size() == 255);
  const auto *s = records.find(254);
  assert(s && s->listIndex == 254 && s->recordOffset == p);
  assert(s->alive() && !s->alreadySet());
  assert(s->beFlags == 0x0d && s->id == 0x12 && s->type == 0);
  assert(s->setNo == 0x1b && s->emsetNo == 0x34 && s->character == 2);
  assert(s->flags == 0x81234567 && s->hp == 65535 && s->room == 0x301);
  assert((s->position == re4::CollisionVec{-327680, 327670, -10}));
  assert(std::abs(s->rotation[0] + 6.283185307179586) < 1e-12);
  assert(std::abs(s->heading - 1.5707963267948966) < 1e-12);
  assert(std::abs(s->rotation[2] + 3.14159265358979323846 / 16384) < 1e-12);
  assert(s->guardRadius == -2000);
  assert(s->raw[28] == 0xde && s->raw[31] == 0xef);
  assert(!records.find(255));
  assert(!records.find(std::numeric_limits<std::size_t>::max()));
  const auto room = records.forRoom(0x301);
  assert(room.size() == 1 && room[0] == s);
  assert(records.forRoom(0x100).empty());
  assert(records.forRoom(0).size() == 254);
  b[p] = 2;
  b[p + 31] = 0;
  assert(s->alive() && s->raw[31] == 0xef); // Owns decoded data/raw bytes.
  re4::EnemySpawns inactive(b);
  assert(!inactive.find(254)->alive() && inactive.find(254)->alreadySet());
  assert(inactive.find(254)->hp == 65535); // Never silently clamps authored HP.
  std::cout << "EnemySpawns exact BE records, bounds and ownership PASS\n";
  if (argc == 2) {
    std::ifstream in(argv[1], std::ios::binary | std::ios::ate);
    assert(in && in.tellg() == std::streampos(re4::EnemySpawns::FileBytes));
    Bytes real(re4::EnemySpawns::FileBytes);
    in.seekg(0);
    assert(in.read(reinterpret_cast<char *>(real.data()), real.size()));
    re4::EnemySpawns original(real);
    const auto *first = original.find(0);
    assert(first && first->room == 0x100 && first->id == 0x12 &&
           first->type == 0);
    assert(!first->alive() && first->hp == 500 && first->flags == 0x20000024);
    assert((first->position == re4::CollisionVec{-77380, 860, -36350}));
    assert(std::abs(first->heading - 1.1756045263158583) < 1e-12);
    const auto *house = original.find(0x25);
    assert(house && house->room == 0x100 && house->id == 0x12 &&
           house->type == 0);
    assert(!house->alive() && !house->alreadySet());
    assert(house->setNo == 0x1b && house->flags == 0x24 && house->hp == 1000);
    assert(house->character == 1 && house->guardRadius == 0);
    assert((house->position == re4::CollisionVec{-83950, 830, -32960}));
    assert(std::abs(house->heading - 0.0005752427954571154) < 1e-15);
    const auto entries = original.forRoom(0x100);
    std::size_t village = 0;
    for (auto entry : entries)
      if (entry->id == 0x12) {
        assert(
            !entry->alive()); // The room script, not ESL alone, activates them.
        ++village;
      }
    assert(village == 12);
    for (const auto &entry : original.records())
      for (std::size_t k = 0; k < entry.raw.size(); ++k)
        assert(entry.raw[k] == real[entry.recordOffset + k]);
    std::cout
        << "Real GC ESL00: 255 records, 12 inactive r100 village Ganados, "
           "original male house record 0x25 PASS\n";
  }
}
