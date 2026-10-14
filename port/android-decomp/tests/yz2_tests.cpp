#include "ports/yz2.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

static void check(bool b, const char *why) {
  if (!b)
    throw std::runtime_error(why);
}
static void put32(std::vector<std::uint8_t> &b, unsigned at,
                  std::uint32_t value) {
  for (unsigned i = 0; i < 4; i++)
    b[at + i] = std::uint8_t(value >> (24 - i * 8));
}
static std::vector<std::uint8_t> literal(unsigned byte) {
  std::vector<std::uint8_t> b(36);
  const char *header = "0\t1\n";
  std::copy(header, header + 4, b.begin());
  unsigned symbol = 1024 + byte, base = 768 * 26 + (symbol - 768) * 25;
  put32(b, 32, base * 131072u);
  return b;
}
static void rejected(const std::vector<std::uint8_t> &b,
                     std::size_t budget = 64 * 1024 * 1024) {
  bool threw = false;
  try {
    (void)re4::DecodeYz2(b, budget);
  } catch (const std::runtime_error &) {
    threw = true;
  }
  check(threw, "invalid stream accepted");
}
int main() {
  try {
    for (unsigned i = 0; i < 256; i++) {
      auto r = re4::DecodeYz2(literal(i));
      check(r.bytes.size() == 1 && r.bytes[0] == i, "literal decoding");
      check(r.consumed == 4 && r.streamOffset == 32, "arithmetic flush bounds");
    }
    auto b = literal(65);
    rejected(b, 0);
    b.resize(35);
    rejected(b);
    b = literal(65);
    b[2] = '0';
    rejected(b);
    b = literal(65);
    b[1] = ':';
    rejected(b);
    b = literal(65);
    put32(b, 32, 0);
    rejected(b); // dictionary reference before any output
    b = literal(65);
    put32(b, 32, 0xffffffff);
    check(re4::DecodeYz2(b).bytes[0] == 255, "top arithmetic interval");
    std::vector<std::uint8_t> wrapped(1060);
    for (unsigned i = 0; i < 8; i++)
      put32(wrapped, i * 4, 0xcab6be20);
    put32(wrapped, 32, 0);
    put32(wrapped, 36, 36);
    put32(wrapped, 44, 1024);
    put32(wrapped, 64, 0xffffffff);
    b = literal(90);
    std::copy(b.begin(), b.end(), wrapped.begin() + 1024);
    auto r = re4::DecodeYz2(wrapped);
    check(r.bytes[0] == 90 && r.payloadOffset == 1024, "DVD MRAM part");
    b = wrapped;
    put32(b, 44, 0xffffffe0);
    rejected(b);
    b = wrapped;
    put32(b, 36, 37);
    rejected(b);
    b = wrapped;
    put32(b, 0, 0xcab6be21);
    rejected(b);
    b = wrapped;
    put32(b, 64, 0);
    put32(b, 68, 36);
    put32(b, 76, 1024);
    put32(b, 96, 0xffffffff);
    rejected(b);
    b = wrapped;
    for (unsigned p = 64; p < 1024; p += 32)
      put32(b, p, 0xfffffffe);
    rejected(b);
    // Deterministic malformed-input stress: strict output cap, every accepted
    // run completes.
    std::mt19937 rng(0x595a32);
    for (unsigned i = 0; i < 2000; i++) {
      b = literal(0);
      b[2] = 'f';
      b.resize(100);
      for (unsigned j = 32; j < 100; j++)
        b[j] = std::uint8_t(rng());
      try {
        (void)re4::DecodeYz2(b, 4096);
      } catch (const std::runtime_error &) {
      }
    }
    std::cout << "Yz2 literals, container, budgets, references and malformed "
                 "streams: PASS\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
