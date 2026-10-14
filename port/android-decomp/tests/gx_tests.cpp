#include "gx_stream.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
using Bytes = std::vector<std::uint8_t>;
Bytes stream(std::uint8_t op, unsigned count, bool color) {
  Bytes b{0, op, static_cast<std::uint8_t>(count >> 8),
          static_cast<std::uint8_t>(count)};
  for (unsigned i = 0; i < count; ++i) {
    b.push_back(0);
    b.push_back(i);
    b.push_back(0);
    b.push_back(i + 1);
    if (color) {
      b.push_back(0);
      b.push_back(i + 2);
    }
    b.push_back(0);
    b.push_back(i + 3);
  }
  b.push_back(0);
  return b;
}
void reject(const Bytes &b, const re4::GxLimits &l) {
  bool caught = false;
  try {
    (void)re4::DecodeGxStream(b, 0, b.size(), l);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  assert(caught);
}
int main() {
  re4::GxLimits l;
  l.positions = l.normals = l.colors = l.texcoords = 256;
  l.colored = true;
  auto b = stream(0x98, 5, true);
  auto ps = re4::DecodeGxStream(b, 0, b.size(), l);
  assert(ps.size() == 1);
  assert((ps[0].triangles == std::vector<std::array<std::uint32_t, 3>>{
                                 {0, 1, 2}, {2, 1, 3}, {2, 3, 4}}));
  assert(ps[0].vertices[2].position == 2 && ps[0].vertices[2].normal == 3 &&
         ps[0].vertices[2].color == 4 && ps[0].vertices[2].texcoord == 5);
  // Degenerate strip indices remain present and winding advances across them.
  auto d = b;
  d[4 + 8 * 2 + 1] = 1;
  auto deg = re4::DecodeGxStream(d, 0, d.size(), l);
  assert(deg[0].triangles == ps[0].triangles);
  b = stream(0x80, 4, true);
  ps = re4::DecodeGxStream(b, 0, b.size(), l);
  assert((ps[0].triangles ==
          std::vector<std::array<std::uint32_t, 3>>{{0, 1, 2}, {0, 2, 3}}));
  b = stream(0xa0, 4, true);
  ps = re4::DecodeGxStream(b, 0, b.size(), l);
  assert((ps[0].triangles ==
          std::vector<std::array<std::uint32_t, 3>>{{0, 1, 2}, {0, 2, 3}}));
  l.colored = false;
  b = stream(0x90, 3, false);
  ps = re4::DecodeGxStream(b, 0, b.size(), l);
  assert(ps[0].triangles.size() == 1 && ps[0].vertices[2].texcoord == 5);
  for (auto op : {0xa8, 0xb0, 0xb8}) {
    b = stream(op, 4, false);
    ps = re4::DecodeGxStream(b, 0, b.size(), l);
    assert(ps[0].vertices.size() == 4 && ps[0].triangles.empty());
  }
  b = stream(0x90, 3, false);
  auto original = b;
  ps = re4::DecodeGxStream(b, 0, b.size(), l);
  assert(b == original);
  for (std::size_t n = 1; n + 1 < b.size(); ++n) {
    auto x = b;
    x.resize(n);
    if (n > 1)
      reject(x, l);
  }
  auto x = b;
  x[1] = 0x91;
  reject(x, l);
  x = b;
  x[1] = 0x61;
  reject(x, l);
  for (auto op : {0x80, 0x90, 0x98, 0xa0, 0xa8, 0xb0, 0xb8}) {
    x = stream(op, 0, false);
    reject(x, l);
  }
  x = stream(0x80, 3, false);
  reject(x, l);
  x = stream(0xa8, 3, false);
  reject(x, l);
  for (unsigned attribute = 0; attribute < 3; ++attribute) {
    auto bounded = l;
    if (!attribute)
      bounded.positions = 2;
    else if (attribute == 1)
      bounded.normals = 2;
    else
      bounded.texcoords = 2;
    reject(b, bounded);
  }
  auto bounded = l;
  bounded.corners = 2;
  reject(b, bounded);
  bounded = l;
  bounded.triangles = 0;
  reject(b, bounded);
  bounded = l;
  bounded.commands = 0;
  reject(b, bounded);
  l.colored = true;
  x = stream(0x90, 3, true);
  bounded = l;
  bounded.colors = 2;
  reject(x, bounded);
  std::uint32_t seed = 0x5910a;
  for (unsigned i = 0; i < 5000; ++i) {
    auto f = x;
    seed = seed * 1664525u + 1013904223u;
    auto p = seed % f.size();
    seed = seed * 1664525u + 1013904223u;
    f[p] ^= seed >> 24;
    try {
      (void)re4::DecodeGxStream(f, 0, f.size(), l);
    } catch (const std::runtime_error &) {
    }
  }
  std::cout
      << "GX formats, topology, winding, attributes, bounds and budgets PASS\n";
}
