#include "room_objects.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

using Bytes = std::vector<std::uint8_t>;
namespace {
void half(Bytes &b, std::size_t p, unsigned value) {
  b.at(p) = value >> 8;
  b.at(p + 1) = value;
}
void word(Bytes &b, std::size_t p, std::uint32_t value) {
  half(b, p, value >> 16);
  half(b, p + 2, value);
}
void real(Bytes &b, std::size_t p, float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  word(b, p, bits);
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
Bytes placements() {
  Bytes bytes(16 + 80);
  half(bytes, 0, 2);
  half(bytes, 16, 3);
  half(bytes, 18, 63);
  for (unsigned k = 0; k < 3; ++k) {
    real(bytes, 32 + k * 4, float(k + 1) * -.25f);
    real(bytes, 44 + k * 4, float(k + 1) * 1234.5f);
  }
  half(bytes, 56, 0);
  half(bytes, 58, 0);
  real(bytes, 84, -12345.5f);
  return bytes;
}
Bytes archive() {
  Bytes bytes(32 + 64 + 4 + 64 + 3);
  word(bytes, 0, 2);
  word(bytes, 32, 68);
  const char first[] = "et0300.bin";
  std::copy(first, first + sizeof(first), bytes.begin() + 64);
  bytes[96] = 0xaa;
  bytes[99] = 0xbb;
  word(bytes, 100, 67);
  const char second[] = "et0300.tpl";
  std::copy(second, second + sizeof(second), bytes.begin() + 132);
  bytes[164] = 0xcc;
  bytes[166] = 0xdd;
  return bytes;
}
} // namespace
int main() {
  const auto bytes = placements();
  const auto list = re4::ReadRoomObjectPlacements(bytes);
  assert(list.size() == 2);
  assert(list[0].id == 3 && list[0].slot == 63);
  assert(list[0].recordOffset == 16);
  assert((list[0].rotation == std::array<float, 3>{-.25f, -.5f, -.75f}));
  assert((list[0].position == std::array<float, 3>{1234.5f, 2469.f, 3703.5f}));
  assert(list[1].position[0] == -12345.5f);
  assert(
      std::equal(list[0].raw.begin(), list[0].raw.end(), bytes.begin() + 16));
  assert(
      std::equal(list[1].raw.begin(), list[1].raw.end(), bytes.begin() + 56));
  for (std::size_t length = 0; length < bytes.size(); ++length)
    rejects([&] {
      re4::ReadRoomObjectPlacements(
          Bytes(bytes.begin(), bytes.begin() + length));
    });
  for (unsigned slot : {64u, 0xffffu}) {
    auto malformed = bytes;
    half(malformed, 18, slot);
    rejects([&] { re4::ReadRoomObjectPlacements(malformed); });
  }
  for (unsigned id : {104u, 0xffffu}) {
    auto malformed = bytes;
    half(malformed, 16, id);
    rejects([&] { re4::ReadRoomObjectPlacements(malformed); });
  }
  auto malformed = bytes;
  half(malformed, 58, 63);
  rejects([&] { re4::ReadRoomObjectPlacements(malformed); });
  malformed = bytes;
  half(malformed, 0, 65);
  rejects([&] { re4::ReadRoomObjectPlacements(malformed); });
  for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity(), 10000001.f}) {
    malformed = bytes;
    real(malformed, 44, invalid);
    rejects([&] { re4::ReadRoomObjectPlacements(malformed); });
  }
  assert(re4::ReadRoomObjectPlacements(Bytes(16)).empty());

  const auto packed = archive();
  const re4::RoomObjectArchive files(packed);
  assert(files.assets().size() == 2);
  const auto *first = files.find("et0300.bin");
  assert(first && first->headerOffset == 32 && first->dataOffset == 96);
  assert(first->extent == 4);
  assert(files.copy("et0300.bin") == (Bytes{0xaa, 0, 0, 0xbb}));
  assert(files.copy("et0300.tpl") == (Bytes{0xcc, 0, 0xdd}));
  assert(!files.find("ET0300.bin")); // GetEtcAddr uses exact strcmp.
  rejects([&] { files.copy("missing.bin"); });
  for (std::size_t length = 0; length < packed.size(); ++length)
    rejects([&] {
      re4::RoomObjectArchive truncated(
          Bytes(packed.begin(), packed.begin() + length));
    });
  for (unsigned size : {0u, 63u, 0xffffffffu}) {
    malformed = packed;
    word(malformed, 32, size);
    rejects([&] { re4::RoomObjectArchive invalid(malformed); });
  }
  malformed = packed;
  std::fill(malformed.begin() + 64, malformed.begin() + 96, 'x');
  rejects([&] { re4::RoomObjectArchive invalid(malformed); });
  malformed = packed;
  std::copy(malformed.begin() + 64, malformed.begin() + 96,
            malformed.begin() + 132);
  rejects([&] { re4::RoomObjectArchive invalid(malformed); });
  malformed = packed;
  word(malformed, 0, 4097);
  rejects([&] { re4::RoomObjectArchive invalid(malformed); });
  assert(re4::RoomObjectArchive(Bytes(32)).assets().empty());

  // Merging a new layer preserves existing SMD/environment ownership and
  // texture/draw indices while adding complete world-space attributes.
  re4::StaticScene scene;
  scene.vertices = {1, 2, 3, 1, 3, 4, 2, 3, 4};
  scene.normals.assign(9, 1);
  scene.uv.assign(6, .5f);
  scene.colors.assign(12, 255);
  scene.triangles = scene.texturedTriangles = scene.included = 1;
  scene.minimum = {1, 2, 3};
  scene.maximum = {2, 3, 4};
  scene.textures.resize(1);
  re4::SceneDraw old;
  old.first = 0;
  old.count = 3;
  old.object = 91;
  old.work = 27;
  old.texture = 0;
  old.sky = 2;
  scene.draws.push_back(old);
  re4::RoomObjectLayer layer;
  layer.mesh = scene;
  layer.mesh.vertices[0] = -10;
  layer.mesh.minimum[0] = -10;
  layer.mesh.maximum[2] = 20;
  layer.mesh.draws[0].object = 63;
  layer.mesh.draws[0].work = 0xfe;
  layer.mesh.draws[0].sky = -1;
  layer.mesh.draws[0].alphaTexture = 0;
  layer.mesh.draws[0].roomObjectSlot = 63;
  layer.mesh.draws[0].roomItemIndex = 12;
  layer.mesh.draws[0].roomItemId = 6;
  re4::AppendRoomObjectScene(scene, layer);
  assert(scene.draws.size() == 2 && scene.draws[0].sky == 2);
  assert(scene.draws[0].work == 27 && scene.draws[0].texture == 0);
  const auto &added = scene.draws[1];
  assert(added.first == 3 && added.object == 155 && added.work == 0xfe);
  assert(added.texture == 1 && added.alphaTexture == 1 && added.sky == -1);
  assert(scene.draws[0].roomObjectSlot == -1 &&
         scene.draws[0].roomItemIndex == -1 && scene.draws[0].roomItemId == -1);
  assert(added.roomObjectSlot == 63 && added.roomItemIndex == 12 &&
         added.roomItemId == 6);
  assert(scene.minimum[0] == -10 && scene.maximum[2] == 20);
  assert(scene.vertices.size() == 18 && scene.normals.size() == 18);
  assert(scene.uv.size() == 12 && scene.colors.size() == 24);
  assert(scene.triangles == 2 && scene.texturedTriangles == 2);
  std::cout << "Room ETS/ETM bounds, transforms and scene merging PASS\n";
}
