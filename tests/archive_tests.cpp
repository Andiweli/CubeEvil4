#include "archive.h"
#include <iostream>
#include <random>
#include <stdexcept>
static void check(bool v) {
  if (!v)
    throw std::runtime_error("Archive contract failed");
}
static void put(std::vector<std::uint8_t> &b, unsigned p, std::uint32_t v) {
  for (unsigned j = 0; j < 4; j++)
    b[p + j] = std::uint8_t(v >> (24 - j * 8));
}
static std::vector<std::uint8_t> fixture() {
  std::vector<std::uint8_t> b(96);
  put(b, 0, 3);
  put(b, 16, 48);
  put(b, 20, 80);
  put(b, 24, 64);
  unsigned p = 28;
  for (auto t : {"LIT", "SMD", "LIT"}) {
    for (unsigned j = 0; j < 3; j++)
      b[p + j] = t[j];
    p += 4;
  }
  return b;
}
static void reject(std::vector<std::uint8_t> b) {
  bool error = false;
  try {
    re4::RoomArchive a(std::move(b));
  } catch (const std::runtime_error &) {
    error = true;
  }
  check(error);
}
int main() {
  try {
    auto b = fixture();
    re4::RoomArchive a(b);
    check(a.entries().size() == 3);
    check(a.find("LIT", 0)->offset == 48);
    check(a.find("LIT", 1)->offset == 64);
    check(a.find("LIT", 2) == nullptr);
    check(!a.find("LI", 0) && !a.find("LIT", -1));
    check(a.entries()[0].extent == 16 && a.entries()[1].extent == 16);
    check(a.copy(0).size() == 16);
    check(a.describe().find("LIT[1]") != std::string::npos);
    b = fixture();
    b[31] = 255;
    re4::RoomArchive fourth(b);
    check(fourth.find("LIT", 0)->offset == 48); // original ignores byte 4
    b = fixture();
    put(b, 20, 48);
    re4::RoomArchive alias(b);
    check(alias.entries()[0].extent == 16 && alias.entries()[1].extent == 16);
    b = fixture();
    put(b, 24, 96);
    re4::RoomArchive empty(b);
    check(empty.copy(2).empty());
    b = fixture();
    put(b, 0, 0xffffffff);
    reject(b);
    b = fixture();
    put(b, 16, 39);
    reject(b);
    b = fixture();
    put(b, 16, 97);
    reject(b);
    b = fixture();
    b.resize(15);
    reject(b);
    b = fixture();
    put(b, 0, 4097);
    reject(b);
    b = fixture();
    put(b, 0, 0);
    re4::RoomArchive zero(b);
    check(zero.entries().empty());
    bool error = false;
    try {
      a.copy(3);
    } catch (const std::out_of_range &) {
      error = true;
    }
    check(error);
    std::mt19937 rng(0x415243);
    for (unsigned i = 0; i < 3000; i++) {
      b.resize(rng() % 256);
      for (auto &v : b)
        v = std::uint8_t(rng());
      try {
        re4::RoomArchive fuzz(b);
      } catch (const std::runtime_error &) {
      }
    }
    std::cout << "Archive BE tables, repeated tags, aliases, bounds and "
                 "malformed data: PASS\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
