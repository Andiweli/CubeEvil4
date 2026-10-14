#include "room_items.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <stdexcept>

using Bytes = std::vector<std::uint8_t>;
namespace {
void half(Bytes &bytes, std::size_t offset, unsigned value) {
  bytes.at(offset) = value >> 8;
  bytes.at(offset + 1) = value;
}
void word(Bytes &bytes, std::size_t offset, std::uint32_t value) {
  half(bytes, offset, value >> 16);
  half(bytes, offset + 2, value);
}
void rejects(const std::function<void()> &action) {
  bool rejected = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);
}
Bytes pack() {
  Bytes bytes(240);
  word(bytes, 0, 3);
  word(bytes, 4, 16);
  word(bytes, 8, 40);
  word(bytes, 12, 192);
  word(bytes, 16, 2);
  half(bytes, 20, 4);
  half(bytes, 28, 6);
  word(bytes, 40, 2);
  word(bytes, 44, 64 - 40);
  word(bytes, 48, 128 - 40);
  word(bytes, 192, 2);
  word(bytes, 196, 216 - 192);
  word(bytes, 200, 228 - 192);
  bytes[64] = 0xab;
  bytes[128] = 0xcd;
  word(bytes, 216, 0x0020af30);
  word(bytes, 228, 0x0020af30);
  bytes[239] = 0xef;
  return bytes;
}
} // namespace
int main() {
  const auto bytes = pack();
  const re4::RoomItemArchive items(bytes);
  assert(items.assets().size() == 2 && !items.find(7));
  const auto *ammo = items.find(4), *herb = items.find(6);
  assert(ammo && ammo->binOffset == 64 && ammo->binExtent == 64);
  assert(ammo->tplOffset == 216 && ammo->tplExtent == 12);
  assert(herb && herb->binOffset == 128 && herb->binExtent == 64);
  assert(herb->tplOffset == 228 && herb->tplExtent == 12);
  assert(items.bin(4).front() == 0xab && items.bin(6).front() == 0xcd);
  assert(items.tpl(6).back() == 0xef);
  rejects([&] { items.bin(5); });
  rejects([&] { items.tpl(5); });
  for (std::size_t length = 0; length < bytes.size(); ++length)
    rejects([&] {
      re4::RoomItemArchive truncated(Bytes(bytes.begin(), bytes.begin() + length));
    });
  for (unsigned field : {4u, 8u, 12u})
    for (unsigned offset : {0u, 15u, 17u, 0xfffffffcu}) {
      auto malformed = bytes;
      word(malformed, field, offset);
      rejects([&] { re4::RoomItemArchive invalid(malformed); });
    }
  for (unsigned field : {16u, 40u, 192u})
    for (unsigned count : {1u, 3u, 257u, 0xffffffffu}) {
      auto malformed = bytes;
      word(malformed, field, count);
      rejects([&] { re4::RoomItemArchive invalid(malformed); });
    }
  for (unsigned id : {4u, 256u, 0xffffu}) {
    auto malformed = bytes;
    half(malformed, 28, id);
    rejects([&] { re4::RoomItemArchive invalid(malformed); });
  }
  for (unsigned field : {44u, 48u, 196u, 200u})
    for (unsigned offset : {0u, 4u, 11u, 0xffffffffu}) {
      auto malformed = bytes;
      word(malformed, field, offset);
      rejects([&] { re4::RoomItemArchive invalid(malformed); });
    }
  auto malformed = bytes;
  word(malformed, 0, 2);
  rejects([&] { re4::RoomItemArchive invalid(malformed); });
  malformed = bytes;
  word(malformed, 228, 0);
  rejects([&] { re4::RoomItemArchive invalid(malformed); });
  malformed = bytes;
  word(malformed, 48, 64 - 40); // Both BIN records point at one file.
  rejects([&] { re4::RoomItemArchive invalid(malformed); });
  malformed = bytes;
  word(malformed, 48, 192 - 40); // BIN points into the TPL offset table.
  rejects([&] { re4::RoomItemArchive invalid(malformed); });
  malformed = bytes;
  word(malformed, 48, 100 - 40); // Truncates the preceding BIN below64bytes.
  rejects([&] { re4::RoomItemArchive invalid(malformed); });
  malformed = bytes;
  word(malformed, 8, 16); // Offset table overlaps the ID table.
  rejects([&] { re4::RoomItemArchive invalid(malformed); });
  Bytes empty(36);
  word(empty, 0, 3);
  word(empty, 4, 16);
  word(empty, 8, 24);
  word(empty, 12, 32);
  assert(re4::RoomItemArchive(empty).assets().empty());
  // Source r215/r22b/r313/... carry the valid zero-ID table at32, with
  // null BIN/TPL tables. cItmSys::DataLoad executes no loop iterations.
  Bytes nullTables(64);word(nullTables,0,3);word(nullTables,4,32);
  assert(re4::RoomItemArchive(nullTables).assets().empty());
  word(nullTables,32,1);
  rejects([&] { re4::RoomItemArchive invalid(nullTables); });
  std::cout << "Room ITM table-relative offsets, IDs, extents and bounds PASS\n";
}
