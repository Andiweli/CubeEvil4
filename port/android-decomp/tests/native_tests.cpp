#include "input.h"
#include "ports/rnd.h"
#include "texture.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
static void check(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
static void put32(std::vector<std::uint8_t> &b, unsigned o, unsigned v) {
  for (unsigned i = 0; i < 4; i++)
    b[o + i] = static_cast<std::uint8_t>(v >> (24 - i * 8));
}
static std::vector<std::uint8_t> tpl(unsigned format) {
  std::vector<std::uint8_t> b(160);
  put32(b, 0, 0x20af30);
  put32(b, 4, 1);
  put32(b, 8, 12);
  put32(b, 12, 32);
  b[33] = 1;
  b[35] = 1;
  put32(b, 36, format);
  put32(b, 40, 96);
  return b;
}
int main(int argc, char **argv) {
  try {
    re4::InputMapper mapper;
    re4::RawInput r;
    r.buttons = re4::Bottom;
    auto f = mapper.poll(r);
    check((f.pressed & re4::Confirm) != 0, "confirm press");
    check(mapper.poll(r).pressed == 0, "held repeat");
    r.buttons = 0;
    check(mapper.poll(r).released & re4::Confirm, "confirm release");
    r.lt = .6f;
    check(mapper.poll(r).held & re4::Aim, "analog aim");
    r.lt = .45f;
    check(mapper.poll(r).held & re4::Aim, "trigger hysteresis");
    r.lt = .3f;
    check(!(mapper.poll(r).held & re4::Aim), "trigger release");
    r = {};
    r.buttons = re4::Lt | re4::Rt;
    check((mapper.poll(r).held & (re4::Aim | re4::Fire)) ==
              (re4::Aim | re4::Fire),
          "digital triggers");
    r = {};
    r.lx = .1f;
    check(mapper.poll(r).lx == 0, "deadzone");
    r.lx = 1;
    r.ly = 1;
    f = mapper.poll(r);
    check(std::abs(f.lx * f.lx + f.ly * f.ly - 1) < .0001f, "radial clamp");
    r.lx = std::numeric_limits<float>::quiet_NaN();
    r.ly = 0;
    check(mapper.poll(r).lx == 0, "NaN input");
    r = {};
    r.lx = r.rx = 1;
    f = mapper.poll(r);
    check(f.lx == 1 && f.rx == -1,
          "physical right maps RS yaw once, LS unchanged");
    check(f.held & re4::MenuRight, "RS correction does not reverse menu input");
    r.rx = -1;
    check(mapper.poll(r).rx == 1, "physical left maps opposite RS yaw");
    r = {};
    r.rx = .5f;
    f = mapper.poll(r);
    check(std::abs(f.rx + (.5f - .15f) / .85f) < .000001f,
          "RS reversal preserves radial deadzone magnitude");
    r.rx = r.ry = 1;
    f = mapper.poll(r);
    check(f.rx < 0 && f.ry > 0 &&
              std::abs(f.rx * f.rx + f.ry * f.ry - 1) < .0001f,
          "RS diagonal preserves length and natural up while reversing horizontal");
    r.rx = .1f;
    r.ry = 0;
    f = mapper.poll(r);
    check(f.rx == 0 && !std::signbit(f.rx),
          "RS deadzone stays positive neutral zero");
    r.rx = std::numeric_limits<float>::quiet_NaN();
    check(mapper.poll(r).rx == 0, "RS NaN stays neutral");
    r.rx = std::numeric_limits<float>::infinity();
    r.ry = -1;
    f = mapper.poll(r);
    check(f.rx == 0 && f.ry == -1,
          "RS invalid horizontal preserves vertical input");
    r = {};
    r.buttons = re4::Up | re4::Down;
    check(!(mapper.poll(r).held & (re4::MenuUp | re4::MenuDown)),
          "opposite directions");
    re4::InputBuffer buffer;
    r.buttons = re4::Left;
    buffer.submit(r);
    r.buttons = 0;
    buffer.submit(r);
    f = buffer.consume();
    check((f.pressed & re4::Reload) && (f.released & re4::Reload) && !f.held,
          "short press retained");
    check(buffer.consume().pressed == 0, "edges consumed once");
    buffer.reset();
    check(buffer.consume().held == 0, "reset input");
    r = {};
    r.rx = 1;
    buffer.submit(r);
    check(buffer.consume().rx == -1 && buffer.consume().rx == -1,
          "buffer consumers do not invert RS yaw again");
    buffer.submit(r);
    check(buffer.consume().rx == -1,
          "repeated raw RS input keeps corrected direction");
    buffer.reset();
    check(buffer.consume().rx == 0, "reset clears corrected RS input");
    buffer.reset();
    r = {};
    r.lt = 1; // A held weapon trigger must not hide resistance edges.
    for (float x : {-.4f, .4f, -.4f, .4f}) {
      r.lx = x;
      buffer.submit(r);
    }
    f = buffer.consume();
    check(f.resistancePresses == 8,
          "four small LS reversals retained between renders");
    check(!(f.held & (re4::MenuLeft | re4::MenuRight)),
          "resistance independent of menu threshold");
    check(buffer.consume().resistancePresses == 0, "resistance consumed once");
    buffer.submit(r);
    check(buffer.consume().resistancePresses == 0,
          "held LS never repeats resistance");
    for (float x : {.34f, .28f, .34f}) {
      r.lx = x;
      buffer.submit(r);
    }
    check(buffer.consume().resistancePresses == 0,
          "LS resistance hysteresis rejects threshold noise");
    r.lx = 0;
    buffer.submit(r);
    r.lx = .4f;
    buffer.submit(r);
    check(buffer.consume().resistancePresses == 2,
          "neutral permits next same-direction flick");
    buffer.reset();
    r = {};
    r.lx = -1;
    r.buttons = re4::DLeft;
    buffer.submit(r);
    check(buffer.consume().resistancePresses == 2,
          "same LS/dpad direction not doubled");
    r = {};
    r.buttons = re4::Bottom;
    buffer.submit(r);
    check(buffer.consume().resistancePresses == 1,
          "aliased confirm/interact count once");
    buffer.reset();
    check(buffer.consume().resistancePresses == 0,
          "lifecycle reset drops resistance queue");
    r = {};
    r.ly = r.ry = 1;
    buffer.submit(r);
    f = buffer.consume();
    check(f.ly == 1 && f.ry == 1,
          "movement and camera keep positive physical up");
    RndInit(0x0d37);
    std::uint16_t seed = 0x0d37;
    for (unsigned i = 0; i < 10000; i++) {
      unsigned lo = seed / 2 % 256, hi = (seed / 2 + seed / 256) % 256,
               next = hi * 256 + lo;
      if (next == seed)
        next += 257;
      seed = static_cast<std::uint16_t>(next);
      check(Rnd() == static_cast<std::uint8_t>(next / 256),
            "original RNG recurrence");
    }
    RndInit(0x0d37);
    for (int i = 0; i < 1000; i++) {
      float a = fRand0_1(), b = fRand1_1();
      check(a >= 0 && a < 1 && b >= -1 && b < 1, "RNG float range");
    }
    u32 effect = 0;
    for (int i = 0; i < 100; i++) {
      float a = fRandSeed0_1(&effect);
      check(a >= 0 && a < 1, "effect RNG range");
    }
    for (unsigned format : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 14u})
      check(re4::DecodeTpl(tpl(format)).rgba.size() == 4, "padded tile");
    auto red = tpl(4);
    red[96] = 0xf8;
    auto t = re4::DecodeTpl(red);
    check(t.rgba[0] == 255 && t.rgba[1] == 0 && t.rgba[3] == 255, "RGB565");
    auto rgba = tpl(6);
    rgba[96] = 128;
    rgba[97] = 7;
    rgba[128] = 8;
    rgba[129] = 9;
    t = re4::DecodeTpl(rgba);
    check(t.rgba == std::vector<std::uint8_t>({7, 8, 9, 128}), "RGBA8 planes");
    auto cmpr = tpl(14);
    cmpr[96] = 0xf8;
    cmpr[98] = 0x07;
    cmpr[99] = 0xe0;
    t = re4::DecodeTpl(cmpr);
    check(t.rgba[0] == 255 && t.rgba[1] == 0, "CMPR endpoint");
    cmpr[96] = cmpr[97] = 0;
    cmpr[98] = cmpr[99] = 255;
    cmpr[100] = 0xc0;
    t = re4::DecodeTpl(cmpr);
    check(t.rgba == std::vector<std::uint8_t>({127, 127, 127, 0}),
          "CMPR transparent texel preserves endpoint mean RGB");
    cmpr[96] = 0xf8;
    cmpr[97] = 0;
    cmpr[98] = 7;
    cmpr[99] = 0xe0;
    cmpr[100] = 0x80;
    t = re4::DecodeTpl(cmpr);
    check(t.rgba == std::vector<std::uint8_t>({159, 95, 0, 255}),
          "CMPR 5/8 interpolation");
    cmpr[100] = 0xc0;
    t = re4::DecodeTpl(cmpr);
    check(t.rgba == std::vector<std::uint8_t>({95, 159, 0, 255}),
          "CMPR 3/8 interpolation");
    auto intensity4 = tpl(0);
    intensity4[35] = 2;
    intensity4[96] = 0x18;
    t = re4::DecodeTpl(intensity4);
    check(t.rgba[0] == 17 && t.rgba[3] == 17 && t.rgba[7] == 136,
          "I4 intensity also supplies alpha");
    auto intensity8 = tpl(1);
    intensity8[35] = 3;
    intensity8[96] = 127;
    intensity8[97] = 0;
    intensity8[98] = 255;
    t = re4::DecodeTpl(intensity8);
    check(t.rgba[0] == 127 && t.rgba[3] == 127 && t.rgba[7] == 0 &&
              t.rgba[11] == 255,
          "I8 intensity alpha boundaries");
    for (unsigned format : {8u, 9u, 10u}) {
      auto p = tpl(format);
      put32(p, 16, 68);
      p[69] = 1;
      put32(p, 72, 1);
      put32(p, 76, 80);
      p[80] = 0xf8;
      t = re4::DecodeTpl(p);
      check(t.rgba[0] == 255, "palette decode");
      p[96] = 255;
      bool rejected = false;
      try {
        re4::DecodeTpl(p);
      } catch (const std::exception &) {
        rejected = true;
      }
      check(rejected, "palette bounds");
    }
    auto mip = tpl(4);
    mip.resize(256);
    mip[33] = 8;
    mip[35] = 8;
    mip[66] = 1;
    for (unsigned i = 96; i < 224; i += 2)
      mip[i] = 0xf8;
    for (unsigned i = 224; i < 256; i += 2) {
      mip[i] = 7;
      mip[i + 1] = 0xe0;
    }
    auto chain = re4::DecodeTplMipmapped(mip);
    check(chain.mips.size() == 1 && chain.mips[0].width == 4 &&
              chain.mips[0].height == 4 && chain.rgba[0] == 255 &&
              chain.mips[0].rgba[1] == 255,
          "Stored mip offset and colors");
    auto rejectsMip = [&](std::vector<std::uint8_t> bad) {
      try {
        re4::DecodeTplMipmapped(bad);
      } catch (const std::exception &) {
        return true;
      }
      return false;
    };
    auto partial = mip;
    partial[66] = 2;
    auto partialChain = re4::DecodeTplMipmapped(partial);
    check(partialChain.maxLOD == 2 && partialChain.mips.size() == 1,
          "Exact resource end clamps a nonempty original chain");
    auto bad = mip;
    bad.resize(255);
    check(rejectsMip(bad), "Truncated stored mip rejected");
    bad = mip;
    bad[66] = 4;
    check(rejectsMip(bad), "LOD exceeds natural chain");
    bad = mip;
    bad[65] = 2;
    check(rejectsMip(bad), "Reversed LOD interval");
    bad = mip;
    bad[35] = 7;
    check(rejectsMip(bad), "Non-power-of-two mip rejected");
    bad = mip;
    put32(bad, 4, 2);
    put32(bad, 20, 68);
    put32(bad, 76,
          224); // Next image starts where the requested mip would begin.
    check(rejectsMip(bad), "Mipmap cannot borrow next image bytes");
    bad = mip;
    bad[65] = bad[66] = 1;
    check(re4::DecodeTplMipmapped(bad).mips.empty(),
          "Equal LOD disables mipmaps");
    auto narrow = tpl(0);
    narrow.resize(224);
    narrow[33] = 1;
    narrow[35] = 8;
    narrow[66] = 3;
    chain = re4::DecodeTplMipmapped(narrow);
    check(chain.mips.size() == 3 && chain.mips[2].width == 1 &&
              chain.mips[2].height == 1,
          "One-pixel dimension remains clamped and tiles stay padded");
    bool rejected = false;
    red.resize(110);
    try {
      re4::DecodeTpl(red);
    } catch (const std::exception &) {
      rejected = true;
    }
    check(rejected, "truncated tile");
    for (int i = 1; i < argc; i++) {
      auto original = re4::LoadTpl(argv[i]);
      check(!original.rgba.empty(), "original TPL");
      std::cout << "Original TPL: " << argv[i] << " " << original.width << "x"
                << original.height << " format=" << original.format << "\n";
      if (i == 1) {
        std::ofstream out("texture-preview.ppm", std::ios::binary);
        out << "P6\n" << original.width << " " << original.height << "\n255\n";
        for (std::size_t p = 0; p < original.rgba.size(); p += 4)
          out.write(reinterpret_cast<const char *>(&original.rgba[p]), 3);
      }
    }
    std::cout << "Native input, RNG and texture contracts: PASS\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
