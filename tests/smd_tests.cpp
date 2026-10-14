#include "smd.h"
#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <stdexcept>
using Bytes = std::vector<std::uint8_t>;
void put(Bytes &b, std::size_t p, std::uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    b[p + i] = v >> (24 - i * 8);
}
Bytes fixture() {
  Bytes b(320);
  b[0] = 0x40;
  b[3] = 1;
  put(b, 4, 96);
  put(b, 8, 100);
  put(b, 12, 320);
  for (auto p : {40, 44, 48})
    put(b, p, 0x3f800000);
  b[54] = 0xff;
  b[55] = 0;
  put(b, 96, 32);
  put(b, 100, 188);
  const unsigned base = 128;
  put(b, base, 64);
  b[base + 25] = 1;
  b[base + 27] = 1;
  put(b, base + 28, 128);
  b[base + 40] = 1;
  put(b, base + 48, 96);
  b[base + 57] = 2;
  put(b, base + 60, 0x20010801);
  // Two s16 vertices at 224, stride 8: (-2,4,6), (8,-10,12), scaled by 1/2.
  const int values[] = {-2, 4, 6, 0, 8, -10, 12, 0};
  for (unsigned i = 0; i < 8; ++i) {
    auto v = static_cast<std::uint16_t>(values[i]);
    b[224 + i * 2] = v >> 8;
    b[225 + i * 2] = v;
  }
  put(b, 256 + 24, 0);
  return b;
}
void reject(Bytes b) {
  bool caught = false;
  try {
    re4::Smd s(b);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  assert(caught);
}
int main() {
  auto b = fixture();
  re4::Smd s(b);
  assert(s.placements.size() == 1 && s.logicalWorkCount == 1);
  assert(s.binOffsets[0] == 128 && s.tplOffsets[0] == 288 &&
         s.motionOffsets.empty());
  assert(s.models[0].minimum[0] == -1 && s.models[0].minimum[1] == -5 &&
         s.models[0].maximum[2] == 6);
  auto uvFixture = b;
  put(uvFixture, 128 + 16, 112); // UVs between vertices and displaylist header.
  put(uvFixture, 240, 0x8000ffff);
  re4::Smd unsignedUv(uvFixture);
  assert(unsignedUv.models[0].texcoords[0][0] == 1.f);
  assert(unsignedUv.models[0].texcoords[0][1] == 65535.f / 32768.f);
  put(uvFixture, 128 + 32, 0x80000000);
  re4::Smd signedUv(uvFixture);
  assert(signedUv.models[0].texcoords[0][0] == -128.f);
  assert(signedUv.models[0].texcoords[0][1] == -1.f / 256);

  auto colorFixture = uvFixture;
  put(colorFixture, 128 + 12,
      88); // Two RGBA entries between joint data and positions.
  put(colorFixture, 216, 0x12345678);
  put(colorFixture, 220, 0xff800040);
  put(colorFixture, 128 + 32, 0xc0000000);
  re4::Smd vertexColor(colorFixture);
  assert(vertexColor.models[0].vertexAmbient &&
         vertexColor.models[0].colors.size() == 2);
  assert((vertexColor.models[0].colors[0] ==
          std::array<std::uint8_t, 4>{0x12, 0x34, 0x56, 0x78}));
  assert((vertexColor.models[0].colors[1] ==
          std::array<std::uint8_t, 4>{255, 128, 0, 64}));
  put(colorFixture, 128 + 12, 200);
  reject(colorFixture);
  auto normalFixture = b;
  put(normalFixture, 128 + 52,
      80); // Two padded S16 normal records before positions.
  normalFixture[187] = 2;
  put(normalFixture, 208, 0xc0004000);
  put(normalFixture, 212, 0x20001234);
  put(normalFixture, 216, 0x80007fff);
  put(normalFixture, 220, 0x0000ffff);
  re4::Smd signedNormals(normalFixture);
  assert(
      (signedNormals.models[0].normals[0] == std::array<float, 3>{-1, 1, .5f}));
  assert(signedNormals.models[0].normals[1][0] == -2 &&
         signedNormals.models[0].normals[1][1] == 32767.f / 16384);
  put(normalFixture, 128 + 32, 0x20000000);
  put(normalFixture, 208, 0xc04020ab);
  put(normalFixture, 212, 0x807f00ff);
  re4::Smd byteNormals(normalFixture);
  assert(
      (byteNormals.models[0].normals[0] == std::array<float, 3>{-1, 1, .5f}));
  assert(byteNormals.models[0].normals[1][0] == -2 &&
         byteNormals.models[0].normals[1][1] == 127.f / 64);
  normalFixture[187] = 5;
  reject(normalFixture);
  auto inactive = b;
  inactive[55] = 0xff;
  re4::Smd noDemand(inactive);
  assert(noDemand.models.empty() && noDemand.palettes.empty());
  re4::Smd externalDemand(inactive, {1, 1, 0});
  assert(externalDemand.models.size() == 1 &&
         externalDemand.palettes.size() == 1);
  assert(externalDemand.models[0].positions == s.models[0].positions);
  bool rejectedDemand = false;
  try {
    re4::Smd badDemand(inactive, {257, 0, 0});
  } catch (const std::runtime_error &) {
    rejectedDemand = true;
  }
  assert(rejectedDemand);
  rejectedDemand = false;
  try {
    re4::Smd badTable(inactive, {2, 1, 0});
  } catch (const std::runtime_error &) {
    rejectedDemand = true;
  }
  assert(rejectedDemand);
  auto original = b;
  re4::Smd again(b);
  assert(b == original);
  // A group count contributes logical works, not extra 72-byte records.
  auto g = b;
  g[1] = 1;
  put(g, 16, 1);
  put(g, 20, 7);
  std::copy(b.begin() + 16, b.begin() + 88, g.begin() + 24);
  re4::Smd grouped(g);
  assert(grouped.logicalWorkCount == 8 && grouped.placements.size() == 1 &&
         grouped.workOffset == 24);
  auto x = b;
  put(x, 128 + 48, 0x80000000);
  reject(x);
  x = b;
  put(x, 128 + 48, 160);
  reject(x); // BIN vertex array may not cross into TPL.
  x = b;
  put(x, 256 + 24, 0xffffffff);
  reject(x);
  x = b;
  x[168] = 31;
  reject(x);
  x = b;
  put(x, 16, 0x7fc00000);
  reject(x);
  x = b;
  put(x, 96, 1);
  reject(x);
  x = b;
  x[0] = 0x31;
  reject(x);
  for (std::size_t n = 0; n < 288; ++n) {
    x = b;
    x.resize(n);
    reject(x);
  }
  // Common references must not be misread as local tables.
  x = b;
  put(x, 84, 0x50);
  x[54] = 0;
  re4::Smd common(x);
  assert(common.models.empty() && common.tplOffsets.empty() &&
         common.motionOffsets.empty());
  std::uint32_t seed = 0x4a2f18;
  for (unsigned i = 0; i < 3000; ++i) {
    x = b;
    seed = seed * 1664525u + 1013904223u;
    auto p = seed % x.size();
    seed = seed * 1664525u + 1013904223u;
    x[p] ^= seed >> 24;
    try {
      re4::Smd fuzz(x);
    } catch (const std::runtime_error &) {
    }
  }
  std::cout
      << "SMD bounds, groups, vertices and common-reference contracts PASS\n";
}
