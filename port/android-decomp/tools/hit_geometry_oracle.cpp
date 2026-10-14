// Driver for verbatim functions extracted from the frozen upstream source by
// test_hit_geometry.py. It links independent upstream SDK C math, never the
// production geometry or motion64 library.
#include "re4_host_stub.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

// Minimal host adaptation of include/em.h. emLineAtCk and HitParts are
// extracted verbatim; only storage/pointer access differ on this host.
enum YARARE_ATARI_FLAG {
  YAT_FLAG_ON = 1, YAT_FLAG_X_AXIS = 2, YAT_FLAG_Z_AXIS = 4,
  YAT_FLAG_CUBE = 8, YAT_FLAG_HANDGUN_MUSHI = 16,
  YAT_FLAG_THROUGH = 32, YAT_FLAG_NO_MARK = 64,
  YAT_FLAG_NO_SCR_BOMB_CK = 128, YAT_FLAG_DMPOS = 16384
};
struct YARARE_INFO {
  Vec offset, cross;
  f32 radius, height, extent;
  u16 flag;
  s16 parts_no;
  f32 len, c_dis;
  YARARE_INFO *pList;
};
struct OracleEnemy : cCoord {
  YARARE_INFO hitInfo{};
  std::vector<cCoord> joints;
  cCoord *getPartsPtr(int index) { return &joints.at(index); }
};

int emLineCapsuleCrossCk(Vec *, Vec *, Vec *, Vec *, Vec *, f32);
int emLineCubeCrossCk(Vec *, Vec *, Mtx, Vec *, Vec *, f32, f32, f32);
int emLinePolyCrossCk(Vec *, Vec *, Vec *, Vec *);
int LineSphereCrossCk(Vec *, Vec *, Vec *, f32, Vec *);
YARARE_INFO *emLineAtCk(OracleEnemy *, Vec *, Vec *, f32, int);

// Same documented scalar-host replacement used by upstream's motion helper.
// GameCube paired-single/frsqrte assembly is not executed on this host.
f32 SQRTF(f32 value) {
  return value <= .00001f ? 0.f : sqrtf(value);
}
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
void vector(Vec &value) {
  if (!(std::cin >> value.x >> value.y >> value.z))
    throw std::runtime_error("oracle: incomplete vector");
}
float number() {
  float value;
  if (!(std::cin >> value))
    throw std::runtime_error("oracle: incomplete scalar");
  return value;
}
} // namespace
int main() {
  try {
    char kind;
    while (std::cin >> kind) {
      Vec from{}, to{}, hit{};
      vector(from);
      vector(to);
      int result;
      unsigned partIndex = 0;
      float lenSq = 0;
      if (kind == 'C') {
        Vec top{}, bottom{};
        vector(top);
        vector(bottom);
        result = emLineCapsuleCrossCk(&from, &to, &top, &bottom, &hit,
                                       number());
      } else if (kind == 'B') {
        Mtx matrix{};
        for (auto &row : matrix)
          for (auto &value : row)
            value = number();
        Vec offset{};
        vector(offset);
        const auto width = number(), height = number(), extent = number();
        result = emLineCubeCrossCk(&from, &to, matrix, &offset, &hit, width,
                                    height, extent);
      } else if (kind == 'P') {
        Vec vertices[4]{};
        for (auto &value : vertices)
          vector(value);
        result = emLinePolyCrossCk(&from, &to, vertices, &hit);
      } else if (kind == 'S') {
        Vec center{};
        vector(center);
        result = LineSphereCrossCk(&from, &to, &center, number(), &hit);
      } else if (kind == 'A') {
        const auto maxLenSq = number();
        const auto weaponNo = static_cast<unsigned>(number());
        OracleEnemy enemy{};
        for (auto &row : enemy.mat)
          for (auto &value : row)
            value = number();
        const auto jointCount = static_cast<unsigned>(number());
        const auto shapeCount = static_cast<unsigned>(number());
        if (jointCount > 248 || shapeCount == 0 || shapeCount > 256)
          throw std::runtime_error("oracle: invalid authored shape count");
        enemy.joints.resize(jointCount);
        for (auto &joint : enemy.joints) {
          Mtx raw;
          for (auto &row : raw)
            for (auto &value : row)
              value = number();
          PSMTXConcat(enemy.mat, raw, joint.mat);
        }
        std::vector<YARARE_INFO> shapes(shapeCount);
        for (unsigned index = 0; index < shapeCount; ++index) {
          auto &shape = shapes[index];
          vector(shape.offset);
          shape.radius = number();
          shape.height = number();
          shape.extent = number();
          shape.flag = static_cast<u16>(number());
          shape.parts_no = static_cast<s16>(number());
          shape.pList = index + 1 < shapeCount ? &shapes[index + 1] : nullptr;
        }
        enemy.hitInfo = shapes.front();
        auto *part = emLineAtCk(&enemy, &from, &to, maxLenSq, weaponNo);
        result = part ? 1 : 0;
        if (part) {
          partIndex = part == &enemy.hitInfo ? 0 : unsigned(part - shapes.data());
          hit = part->cross;
          lenSq = part->len;
        }
      } else {
        throw std::runtime_error("oracle: unknown case type");
      }
      // The literal source can return nonfinite points for degenerate
      // projections. Native code must reject those rather than use a NaN hit.
      const bool rejected = result &&
          (!std::isfinite(hit.x) || !std::isfinite(hit.y) ||
           !std::isfinite(hit.z) || !std::isfinite(lenSq));
      word(rejected ? 2 : result ? 1 : 0);
      floating(result && !rejected ? hit.x : 0);
      floating(result && !rejected ? hit.y : 0);
      floating(result && !rejected ? hit.z : 0);
      word(result && !rejected ? partIndex : 0);
      floating(result && !rejected ? lenSq : 0);
    }
    if (!std::cin.eof() || !std::cout.good())
      throw std::runtime_error("oracle: stream error");
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
