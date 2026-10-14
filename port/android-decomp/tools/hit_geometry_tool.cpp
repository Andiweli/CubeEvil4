// Text input C(capsule), B(box), A(authored linked shapes). Each result is
// big-endian >I3fIf: state 0 miss/1 hit/2 guarded nonfinite, XYZ, index, lenSq.
// test_hit_geometry.py defines and independently exercises the input fields.
#include "hit_geometry.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void word(std::uint32_t value) {
  for (int shift = 24; shift >= 0; shift -= 8)
    std::cout.put(static_cast<char>(value >> shift));
}
void floating(float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  word(bits);
}
std::array<float, 3> point() {
  std::array<float, 3> value{};
  if (!(std::cin >> value[0] >> value[1] >> value[2]))
    throw std::runtime_error("hit geometry tool: incomplete point");
  return value;
}
float number() {
  float value;
  if (!(std::cin >> value))
    throw std::runtime_error("hit geometry tool: incomplete scalar");
  return value;
}
} // namespace
int main() {
  try {
    char kind;
    while (std::cin >> kind) {
      const auto from = point(), to = point();
      std::optional<std::array<float, 3>> hit;
      unsigned partIndex = 0;
      float lenSq = 0;
      bool rejected = false;
      try {
        if (kind == 'C') {
          const auto top = point(), bottom = point();
          hit = re4::HitLineCapsule(from, to, top, bottom, number());
        } else if (kind == 'B') {
          re4::Matrix34 matrix{};
          for (auto &value : matrix)
            value = number();
          const auto offset = point();
          const auto width = number(), height = number(), extent = number();
          hit = re4::HitLineCube(from, to, matrix, offset, width, height, extent);
        } else if (kind == 'A') {
          const auto maxLenSq = number();
          const auto weaponNo = static_cast<unsigned>(number());
          re4::Matrix34 placement{};
          for (auto &value : placement)
            value = number();
          const auto jointCount = static_cast<unsigned>(number());
          const auto shapeCount = static_cast<unsigned>(number());
          if (jointCount > 248 || shapeCount == 0 || shapeCount > 256)
            throw std::runtime_error("hit geometry tool: invalid shape count");
          std::vector<re4::Matrix34> joints(jointCount);
          for (auto &joint : joints)
            for (auto &value : joint)
              value = number();
          std::vector<re4::AuthoredHitShape> shapes(shapeCount);
          for (auto &shape : shapes) {
            shape.offset = point();
            shape.radius = number();
            shape.height = number();
            shape.extent = number();
            shape.flags = static_cast<std::uint16_t>(number());
            shape.partsNo = static_cast<std::int16_t>(number());
          }
          auto result = re4::HitAuthoredShapes(
              shapes, joints, placement, {from[0], from[1], from[2]},
              {to[0], to[1], to[2]}, maxLenSq, weaponNo);
          if (result) {
            hit = result->cross;
            partIndex = static_cast<unsigned>(result->partIndex);
            lenSq = result->lenSq;
          }
        } else {
          throw std::runtime_error("hit geometry tool: unknown case type");
        }
      } catch (const std::runtime_error &error) {
        // Only this defined native guard corresponds to a nonfinite source
        // result. Input/other exceptions remain fatal test failures.
        if (std::string(error.what()) != "Hit geometry: invalid coordinate")
          throw;
        rejected = true;
      }
      word(rejected ? 2 : hit ? 1 : 0);
      for (unsigned component = 0; component < 3; ++component)
        floating(hit ? (*hit)[component] : 0.f);
      word(partIndex);
      floating(lenSq);
    }
    if (!std::cin.eof() || !std::cout.good())
      throw std::runtime_error("hit geometry tool: stream error");
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
