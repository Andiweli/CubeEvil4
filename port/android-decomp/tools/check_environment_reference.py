#!/usr/bin/env python3
"""Compile unchanged original cEsp move/color/ANM methods as an independent oracle.
No disc files are needed. CMake host build must already exist. Usage:
  python3 tools/check_environment_reference.py --build /path/to/host-build
"""
import argparse
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--build", required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / "upstream/re4/src/game/esp_sub.cpp").read_text()


def method(name):
    start = source.index("int cEsp::" + name + "()")
    body = source.index("{", start)
    level, end = 1, body + 1
    while level:
        level += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


pre = r'''
#include "environment.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
using f32 = float;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
struct Vec {
  float x, y, z;
};
struct Parent {
  float mat[3][4]{};
};
Parent world;
Parent *pEffParentWorld = &world;
void PSVECAdd(const Vec *a, const Vec *b, Vec *c) {
  Vec v{a->x + b->x, a->y + b->y, a->z + b->z};
  *c = v;
}
void PSVECScale(const Vec *a, Vec *b, float f) {
  Vec v{a->x * f, a->y * f, a->z * f};
  *b = v;
}
#define SQRTF std::sqrt
struct cAnm {
  u16 Frames{};
  u8 Data_num{}, Loop{};
  u8 Frame_cnt[256]{};
} currentAnm;
struct Logger {
  void err(int, int, const char *, unsigned) {
    throw std::runtime_error("original texture missing");
  }
} logger, *pLog = &logger;
int EspGetAnmAddr(unsigned, cAnm **out) {
  *out = &currentAnm;
  return 1;
}
class cEsp {
public:
  bool dead = false;
  Parent *parent = &world;
  u8 m_Release_time = 255;
  Vec m_Pos{}, m_Speed{}, m_Speed_plus{}, m_Ang{}, m_Ang_plus{};
  float m_D_speed{}, m_Size_mul{}, m_Size_plus{}, m_D_size_plus{}, m_Radius{},
      m_Size_base_x{}, m_Size_base_y{};
  float m_Col_r{}, m_Col_g{}, m_Col_b{}, m_Col_a{}, m_Col_d_r{}, m_Col_d_g{},
      m_Col_d_b{}, m_Col_d_a{};
  u8 m_Col_start_r{}, m_Col_start_g{}, m_Col_start_b{}, m_Col_start_a{},
      m_Blend_type{};
  u16 m_Pos_start_cnt{}, m_Size_start_cnt{}, m_Col_max_cnt{}, m_Col_start_cnt{},
      m_Life_max{}, m_Life_time{};
  unsigned m_Tex_id{}, m_MaskTex_id{}, m_Tool_flg{};
  u16 m_Anm_cnt{}, m_MaskAnm_cnt{};
  u8 m_Anm_rate{}, m_Ptn_no{}, m_MaskPtn_no{};
  void ApplyMatrix(float[3][4]) {
    throw std::runtime_error("unexpected parent");
  }
  int CommonMove();
  int ColorUpdate();
  int AnmMove();
};
void PushEsp(cEsp *p) { p->dead = true; }
'''
post = r'''
unsigned checks = 0;
float maxError = 0;
void require(bool b, const char *s) {
  ++checks;
  if (!b)
    throw std::runtime_error(s);
}
void equal(float a, float b) {
  maxError = std::max(maxError, std::abs(a - b));
  require(std::abs(a - b) <= .000001f, "original/port float mismatch");
}
void put(re4::WaterRecord &r, unsigned p, unsigned u) {
  for (unsigned k = 0; k < 4; ++k)
    r.bytes.at(p + k) = u8(u >> (24 - 8 * k));
}
void real(re4::WaterRecord &r, unsigned p, float f) {
  unsigned u;
  std::memcpy(&u, &f, 4);
  put(r, p, u);
}
void half(re4::WaterRecord &r, unsigned p, unsigned x) {
  r.bytes[p] = u8(x >> 8);
  r.bytes[p + 1] = u8(x);
}
int main() {
  try {
    for (unsigned sample = 0; sample < 512; ++sample) {
      re4::EnvironmentData data;
      data.signature = sample;
      re4::WaterAnimation animation;
      animation.width = animation.height = 128;
      animation.loop = sample % 3;
      animation.pattern = {0, 1, 2};
      animation.duration = {1, 2, 3};
      data.animations = {animation};
      currentAnm = {};
      currentAnm.Frames = 3;
      currentAnm.Data_num = 1;
      currentAnm.Loop = u8(sample % 3);
      for (unsigned k = 0; k < 3; ++k)
        currentAnm.Frame_cnt[3 + k] = u8(k + 1);
      re4::WaterRecord r;
      r.bytes[0] = 1;
      r.bytes[1] = 0;
      r.bytes[7] = 253;
      put(r, 8, 0x18000);
      real(r, 48, .97f);
      real(r, 136, 100);
      real(r, 140, 200);
      real(r, 148, .02f);
      real(r, 152, .98f);
      real(r, 12, 100);
      real(r, 16, 200);
      real(r, 36, 1.2f);
      real(r, 40, -.7f);
      real(r, 64, .13f);
      real(r, 68, .05f);
      real(r, 112, .17f);
      for (unsigned k = 0; k < 4; ++k) {
        r.bytes[156 + k] = u8(180 + 10 * k);
        real(r, 160 + k * 4, k == 3 ? .97f : 1.001f);
      }
      half(r, 176, sample % 5);
      half(r, 178, sample % 7);
      half(r, 180, sample % 13);
      half(r, 182, sample % 11);
      half(r, 184, sample % 2 ? 40 : 0);
      half(r, 186, sample % 8 == 0 ? 65530 : 0);
      r.bytes[189] = u8(sample % 256);
      r.bytes[194] = u8(sample % 6);
      re4::WaterTarget target;
      target.records = {r};
      data.targets = {target};
      re4::EnvironmentSimulation sim(data);
      const auto initial = sim.state().sprites.at(0);
      cEsp ref;
      auto vec = [](const auto &a) { return Vec{a[0], a[1], a[2]}; };
      ref.m_Pos = vec(initial.position);
      ref.m_Speed = vec(initial.speed);
      ref.m_Speed_plus = vec(initial.acceleration);
      ref.m_Ang = vec(initial.angle);
      ref.m_Ang_plus = vec(initial.angleSpeed);
      ref.m_D_speed = .97f;
      ref.m_Size_mul = 1;
      ref.m_Size_plus = .02f;
      ref.m_D_size_plus = .98f;
      ref.m_Size_base_x = 100;
      ref.m_Size_base_y = 200;
      ref.m_Col_r = ref.m_Col_start_r = 180;
      ref.m_Col_g = ref.m_Col_start_g = 190;
      ref.m_Col_b = ref.m_Col_start_b = 200;
      ref.m_Col_a = ref.m_Col_start_a = 210;
      ref.m_Col_d_r = ref.m_Col_d_g = ref.m_Col_d_b = 1.001f;
      ref.m_Col_d_a = .97f;
      ref.m_Col_max_cnt = u16(sample % 5);
      ref.m_Col_start_cnt = u16(sample % 7);
      ref.m_Pos_start_cnt = u16(sample % 13);
      ref.m_Size_start_cnt = u16(sample % 11);
      ref.m_Life_max = sample % 2 ? 40 : 0;
      ref.m_Life_time = u16(sample % 8 == 0 ? 65530 : 0);
      ref.m_Blend_type = u8(sample % 6);
      ref.m_Anm_rate = u8(int(std::int8_t(sample % 256)) + 32);
      for (unsigned frame = 0; frame < 256; ++frame) {
        if (!ref.CommonMove())
          ref.dead = true;
        else if (!ref.AnmMove())
          ref.dead = true;
        sim.tick();
        require(ref.dead == sim.state().sprites.empty(),
                "original/port death frame mismatch");
        if (ref.dead)
          break;
        auto &s = sim.state().sprites[0];
        equal(s.position[0], ref.m_Pos.x);
        equal(s.position[1], ref.m_Pos.y);
        equal(s.speed[0], ref.m_Speed.x);
        equal(s.speed[1], ref.m_Speed.y);
        equal(s.angle[2], ref.m_Ang.z);
        equal(s.sizeMultiplier, ref.m_Size_mul);
        equal(s.sizeStep, ref.m_Size_plus);
        equal(s.color[0], ref.m_Col_r);
        equal(s.color[1], ref.m_Col_g);
        equal(s.color[2], ref.m_Col_b);
        equal(s.color[3], ref.m_Col_a);
        require(s.age == ref.m_Life_time && s.pattern == ref.m_Ptn_no &&
                    s.animationCounter == ref.m_Anm_cnt,
                "original/port 8/16-bit animation counter mismatch");
      }
    }
    std::cout << "Unchanged original CommonMove/ColorUpdate/AnmMove PASS: "
              << checks << " checks, maximum float error=" << maxError << '\n';
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
'''
with tempfile.TemporaryDirectory(prefix="re4-env-original-") as tmp:
    cpp = Path(tmp) / "reference.cpp"
    exe = Path(tmp) / "reference"
    cpp.write_text("\n".join([pre, method("CommonMove"), method("ColorUpdate"),
                              method("AnmMove"), post]))
    subprocess.run([
        "g++", "-std=c++17", "-O2", "-fno-fast-math", "-ffp-contract=off",
        "-I" + str(root / "app/src/main/cpp"), str(cpp), "-Wl,--start-group",
        str(args.build.resolve() / "libre4_platform.a"),
        str(args.build.resolve() / "libre4_motion64.a"), "-Wl,--end-group",
        "-o", str(exe)
    ], check=True)
    subprocess.run([str(exe)], check=True)
