#include "sat.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <stdexcept>
using Bytes = std::vector<std::uint8_t>;
void h(Bytes &b, int p, unsigned v) {
  b[p] = v >> 8;
  b[p + 1] = v;
}
void u(Bytes &b, int p, unsigned v) {
  h(b, p, v >> 16);
  h(b, p + 2, v);
}
void f(Bytes &b, int p, float v) {
  unsigned bits;
  std::memcpy(&bits, &v, 4);
  u(b, p, bits);
}
Bytes fixture() {
  Bytes b(138);
  b[0] = 32;
  h(b, 2, 3);
  h(b, 4, 1);
  h(b, 6, 1);
  h(b, 10, 1);
  h(b, 12, 1);
  h(b, 18, 1);
  f(b, 32, 2);
  f(b, 48, 3);
  f(b, 60, 1);
  h(b, 82, 1);
  h(b, 84, 2);
  u(b, 96, 0x12345678);
  h(b, 124, 1);
  return b;
}
void rejects(Bytes b) {
  bool fail = false;
  try {
    re4::Sat s(b);
  } catch (const std::runtime_error &) {
    fail = true;
  }
  assert(fail);
}
int main() {
  auto b = fixture();
  re4::Sat s(b);
  assert(s.polygons.size() == 1 && s.blocks == 1 && s.blockReferences == 1);
  assert(s.polygons[0].attributes == 0x12345678);
  auto scene = s.scene();
  assert(scene.triangles == 1 && scene.vertices.size() == 9 &&
         scene.colors[0] == 60);
  assert(scene.maximum[0] == 2 && scene.maximum[1] == 3);
  assert(scene.draws[0].vertexColors && scene.draws[0].texture == -1);
  for (std::size_t n = 0; n < b.size(); ++n) {
    auto q = b;
    q.resize(n);
    rejects(q);
  }
  auto q = b;
  h(q, 80, 3);
  rejects(q);
  q = b;
  h(q, 86, 1);
  rejects(q);
  q = b;
  h(q, 88, 1);
  rejects(q);
  q = b;
  h(q, 136, 1);
  rejects(q);
  q = b;
  h(q, 12, 0);
  rejects(q);
  q = b;
  u(q, 20, 0x7fc00000);
  rejects(q);
  q = b;
  u(q, 132, 36);
  rejects(q);
  q = b;
  h(q, 88, 0xe000); // Edge outline bits must not be mistaken for index bits.
  assert(re4::Sat(q).polygons[0].edges[0] == 0xe000);
  q = b;
  h(q, 12, 0);
  h(q, 14, 1);
  assert(re4::Sat(q).scene().colors[0] == 240);
  q = b;
  h(q, 12, 0);
  h(q, 16, 1);
  assert(re4::Sat(q).scene().colors[0] == 70);
  q = b;
  h(q, 18, 2);
  rejects(q); // Declared node count.
  q = b;
  h(q, 130, 1);
  rejects(q); // Child beyond truncated leaf.
  Bytes wrapped(146);
  wrapped[0] = 128;
  u(wrapped, 4, 8);
  std::copy(b.begin(), b.end(), wrapped.begin() + 8);
  assert(re4::Sat(wrapped).polygons.size() == 1);
  Bytes variants(288);
  variants[0] = 128;
  u(variants, 4, 12);
  u(variants, 8, 150);
  std::copy(b.begin(), b.end(), variants.begin() + 12);
  std::copy(b.begin(), b.end(), variants.begin() + 150);
  assert(re4::Sat(variants).offsetSlots == 2);
  u(variants, 8, 149);
  rejects(variants); // Variant 0 cannot read through next variant.
  u(wrapped, 4, 7);
  rejects(wrapped);
  std::cout << "SAT bounds, references, groups, attributes and scene PASS\n";
}
