// The GX renderer of the core mode (gx_gl.cpp, gx_gl_shader.cpp, gx_texture.cpp)
// on a headless GLES3 context: no disc data, runs in the cloud.
//
//   - the GX texture decoder against the app's checked TPL reader for every
//     format (I4 I8 IA4 IA8 RGB565 RGB5A3 RGBA8 C4 C8 C14X2 CMPR, sizes that
//     are not whole tiles)
//   - chunks as the core's decoder writes them (re4_gx_frame.h), with the
//     register encodings of the SDK: clear by display copy, vertex colours,
//     orientation (GameCube row 0 = EFB row 0), culling, a texture through
//     the TEV with nearest sampling, konstant colours, alpha test, blending,
//     depth test, a directional light, texture coordinates generated from the
//     position, an EFB copy used as a texture, a paletted texture, linear fog
//   - shader generation: random TEV setups (inputs, compare modes, scales,
//     swap tables, konst selections, indirect stages) all compile
#include "gx_chunk_builder.h"
#include "gx_gl.h"
#include "gx_selfcheck.h"
#include "gx_texture.h"
#include "re4_gx_frame.h"
#include "texture.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <array>
#include <vector>

namespace {

int gFailures = 0;

void Check(bool ok, const char *what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    gFailures++;
  }
}

void Require(bool ok, const char *what) {
  if (!ok) {
    std::fprintf(stderr, "FATAL: %s\n", what);
    std::exit(1);
  }
}

void Context() {
  auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
  EGLDisplay display = getDisplay ? getDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)
                                  : eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint major{}, minor{};
  Require(eglInitialize(display, &major, &minor), "EGL initialise");
  const EGLint attributes[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE};
  EGLConfig config{};
  EGLint count{};
  eglChooseConfig(display, attributes, &config, 1, &count);
  eglBindAPI(EGL_OPENGL_ES_API);
  const EGLint contextAttributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};
  EGLContext context = eglCreateContext(display, count ? config : nullptr, EGL_NO_CONTEXT, contextAttributes);
  Require(context != EGL_NO_CONTEXT, "EGL context");
  Require(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context), "EGL current");
}

// --- texture decoder against the TPL reader ---------------------------------------

void Be16(std::vector<std::uint8_t> &b, std::size_t at, unsigned v) {
  b[at] = static_cast<std::uint8_t>(v >> 8);
  b[at + 1] = static_cast<std::uint8_t>(v);
}
void Be32(std::vector<std::uint8_t> &b, std::size_t at, std::uint32_t v) {
  Be16(b, at, v >> 16);
  Be16(b, at + 2, v & 0xFFFF);
}

void TestDecoder() {
  std::mt19937 rng(7);
  const unsigned formats[] = {0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 14};
  for (unsigned format : formats) {
    const unsigned w = 13, h = 9;
    const std::size_t bytes = re4gx::GxTextureLevelBytes(format, w, h);
    const bool paletted = re4gx::GxIsPaletted(format);
    const unsigned entries = paletted ? (format == 10 ? 64 : re4gx::GxPaletteEntries(format)) : 0;
    // TPL: header (12), descriptor (8), texture header (36), palette header (12), palette, texels
    const std::size_t texHeader = 0x20, palHeader = 0x60, palData = 0x80;
    const std::size_t texData = (palData + entries * 2 + 31) / 32 * 32;
    std::vector<std::uint8_t> tpl(texData + bytes);
    Be32(tpl, 0, 0x0020AF30);
    Be32(tpl, 4, 1);
    Be32(tpl, 8, 12);
    Be32(tpl, 12, static_cast<std::uint32_t>(texHeader));
    Be32(tpl, 16, paletted ? static_cast<std::uint32_t>(palHeader) : 0);
    Be16(tpl, texHeader, h);
    Be16(tpl, texHeader + 2, w);
    Be32(tpl, texHeader + 4, format);
    Be32(tpl, texHeader + 8, static_cast<std::uint32_t>(texData));
    if (paletted) {
      Be16(tpl, palHeader, entries);
      Be32(tpl, palHeader + 4, 2); // RGB5A3
      Be32(tpl, palHeader + 8, static_cast<std::uint32_t>(palData));
      for (unsigned i = 0; i < entries; i++)
        Be16(tpl, palData + i * 2, rng() & 0xFFFF);
    }
    for (std::size_t i = 0; i < bytes; i++)
      tpl[texData + i] = static_cast<std::uint8_t>(rng());
    if (format == 10) // indices inside the 64-entry palette
      for (std::size_t i = 0; i < bytes; i += 2) {
        tpl[texData + i] &= 0xC0;
        tpl[texData + i + 1] &= 63;
      }
    re4::Texture ref = re4::DecodeTpl(tpl, 0, 0);
    std::vector<std::uint8_t> mine;
    const bool ok = re4gx::GxDecodeTexture(tpl.data() + texData, bytes, format, w, h,
                                           paletted ? tpl.data() + palData : nullptr, entries, 2, mine);
    char what[96];
    std::snprintf(what, sizeof(what), "decoder format %u equals the TPL reader", format);
    Check(ok && mine == ref.rgba, what);
  }
}

// --- chunks ------------------------------------------------------------------------

using namespace re4gx::chunk;

GLuint gReadFbo = 0;

// RGBA at EFB pixel (x, y) (GameCube coordinates, scale 1).
std::vector<std::uint8_t> EfbPixel(re4gx::GxRenderer &r, int x, int y) {
  glBindFramebuffer(GL_FRAMEBUFFER, gReadFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r.EfbColor(), 0);
  std::vector<std::uint8_t> p(4);
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
  return p;
}

std::vector<std::uint8_t> TexPixel(GLuint tex, int x, int y) {
  glBindFramebuffer(GL_FRAMEBUFFER, gReadFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  std::vector<std::uint8_t> p(4);
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
  return p;
}

bool Near(const std::vector<std::uint8_t> &p, int r, int g, int b, int tolerance = 2) {
  return std::abs(p[0] - r) <= tolerance && std::abs(p[1] - g) <= tolerance && std::abs(p[2] - b) <= tolerance;
}

void Print(const char *what, const std::vector<std::uint8_t> &p) {
  std::fprintf(stderr, "  %s: %d %d %d %d\n", what, p[0], p[1], p[2], p[3]);
}

const std::uint8_t kRed[4] = {255, 0, 0, 255};
const std::uint8_t kGreen[4] = {0, 255, 0, 255};
const std::uint8_t kBlue[4] = {0, 0, 255, 255};
const std::uint8_t kBlack[4] = {0, 0, 0, 255};
const std::uint8_t kWhite[4] = {255, 255, 255, 255};

void TestBasics(re4gx::GxRenderer &r) {
  // frame 1: clear to blue by a display copy, then a green quad top left, a
  // red triangle bottom right, display copy
  {
    Builder b;
    Baseline(b);
    DisplayCopy(b, true, kBlue);
    Quad(b, 10, 20, 110, 70, 0, kGreen);
    std::vector<std::uint8_t> v;
    V(v, 500, 400, 0, kRed);
    V(v, 600, 400, 0, kRed);
    V(v, 500, 500, 0, kRed);
    b.Draw(RE4_GX_V_COLOR0, 16, v, 3);
    DisplayCopy(b, false, kBlue);
    r.Run(b.Chunk());
  }
  auto inside = EfbPixel(r, 50, 40), outside = EfbPixel(r, 300, 300), tri = EfbPixel(r, 520, 420);
  Check(Near(inside, 0, 255, 0), "vertex colour: the green quad at GameCube rows 20..70 (EFB rows 20..70)");
  Check(Near(outside, 0, 0, 255), "clear by display copy: blue elsewhere");
  Check(Near(tri, 255, 0, 0), "a triangle with clockwise screen winding is drawn (no culling)");
  if (!Near(inside, 0, 255, 0) || !Near(outside, 0, 0, 255))
    Print("inside", inside), Print("outside", outside);
  unsigned w = 0, h = 0;
  GLuint xfb = r.Xfb(&w, &h);
  Check(xfb != 0 && w == 640 && h == 528, "display copy: an XFB of the copy size");
  Check(xfb && Near(TexPixel(xfb, 50, 40), 0, 255, 0), "display copy: the XFB holds the EFB (row 0 = top)");
  Check(r.Stats().draws == 2 && r.Stats().displayCopies == 2, "two draws, two display copies");

  // a held picture (RE4_GX_CHUNK_HOLD: the VI stays on the XFB before, as
  // with SYS_SCREEN_STOP): drawn and cleared, the XFB keeps the last one
  {
    Builder b;
    Baseline(b);
    b.flags |= RE4_GX_CHUNK_XFB | RE4_GX_CHUNK_HOLD;
    Quad(b, 10, 20, 110, 70, 0, kRed);
    DisplayCopy(b, true, kBlack);
    r.Run(b.Chunk());
  }
  Check(Near(TexPixel(r.Xfb(nullptr, nullptr), 50, 40), 0, 255, 0) && r.Stats().heldCopies == 1,
        "held picture: the XFB keeps the picture before");
  Check(Near(EfbPixel(r, 50, 40), 0, 0, 0), "held picture: the display copy still clears the EFB");

  // VISetBlack (RE4_GX_CHUNK_BLACK): the chunk runs, the screen shows black
  // until a chunk without the flag
  {
    GLuint tex = 0, fbo = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenFramebuffers(1, &fbo);
    auto shown = [&](re4gx::GxRenderer &rr) {
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
      glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      rr.Present(0, 0, 64, 64);
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      std::uint8_t px[4] = {0, 0, 0, 0};
      glReadPixels(5, 58, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);  // near the top left: the green quad
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      return std::array<int, 3>{px[0], px[1], px[2]};
    };
    for (int black = 1; black >= 0; black--) {
      Builder b;
      Baseline(b);
      b.flags |= RE4_GX_CHUNK_XFB | (black ? RE4_GX_CHUNK_BLACK : 0);
      Quad(b, 0, 0, 640, 448, 0, kGreen);
      DisplayCopy(b, true, kBlack);
      r.Run(b.Chunk());
      auto px = shown(r);
      if (black) {
        Check(px[0] == 0 && px[1] == 0 && px[2] == 0, "VISetBlack: the screen shows black");
        Check(Near(TexPixel(r.Xfb(nullptr, nullptr), 50, 40), 0, 255, 0), "VISetBlack: the XFB holds the picture");
      } else {
        Check(px[1] > 200 && px[0] < 40, "black lifted: the picture shows again");
      }
      if ((black && (px[0] || px[1] || px[2])) || (!black && px[1] <= 200))
        std::fprintf(stderr, "  shown: %d %d %d\n", px[0], px[1], px[2]);
    }
    // every present counts (the GPU measurement, RE4_GPU_BENCH); with the timer extension its
    // GPU time arrives some presents later
    {
      const unsigned before = r.Stats().presents;
      for (int i = 0; i < 3; i++) {
        shown(r);
        glFinish();
      }
      Check(r.Stats().presents == before + 3, "every present counted");
      const char *ext = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
      if (ext && std::strstr(ext, "GL_EXT_disjoint_timer_query")) {
        Check(r.Stats().presentTimed > 0, "the presents' GPU time measured");
        Check(r.Stats().gpuTimed > 0, "the chunks' GPU time measured");
        std::fprintf(stderr, "  present: %u timed, %.3f ms each\n", r.Stats().presentTimed,
                     r.Stats().presentTimed ? r.Stats().presentMicros / 1000.0 / r.Stats().presentTimed : 0.0);
      } else {
        std::fprintf(stderr, "  present: no timer extension here, GPU time not measured\n");
      }
      // every chunk is checked against the GPU (fences); after glFinish it has caught up
      unsigned counted = 0;
      for (unsigned n : r.Stats().behindCount)
        counted += n;
      Check(r.Stats().behindChecks == r.Stats().chunks && counted == r.Stats().behindChecks,
            "every chunk checked against the GPU");
      Builder b;
      Baseline(b);
      Quad(b, 0, 0, 640, 448, 0, kGreen);
      const unsigned long long sum = r.Stats().behindSum;
      const unsigned zero = r.Stats().behindCount[0];
      r.Run(b.Chunk());
      Check(r.Stats().behindSum == sum && r.Stats().behindCount[0] == zero + 1,
            "after glFinish the GPU is behind by no picture");
    }
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
  }

  // culling: hardware mode 1 culls back faces; GX front faces are clockwise
  // on screen (y down)
  {
    Builder b;
    Baseline(b);
    b.Bp(0x00, (1u << 4) | 1u << 14);
    DisplayCopy(b, true, kBlack);
    std::vector<std::uint8_t> cw, ccw;
    V(cw, 100, 100, 0, kRed); // clockwise on screen: right, then down
    V(cw, 200, 100, 0, kRed);
    V(cw, 100, 200, 0, kRed);
    V(ccw, 400, 100, 0, kGreen); // counter-clockwise
    V(ccw, 400, 200, 0, kGreen);
    V(ccw, 500, 100, 0, kGreen);
    b.Draw(RE4_GX_V_COLOR0, 16, cw, 3);
    b.Draw(RE4_GX_V_COLOR0, 16, ccw, 3);
    r.Run(b.Chunk());
  }
  Check(Near(EfbPixel(r, 120, 120), 255, 0, 0), "culling back faces keeps a clockwise (front) triangle");
  Check(Near(EfbPixel(r, 420, 120), 0, 0, 0), "culling back faces drops a counter-clockwise triangle");

  // depth: the nearer quad wins in either order (LEQUAL, depth 0 near)
  {
    Builder b;
    Baseline(b);
    DisplayCopy(b, true, kBlack);
    Quad(b, 0, 0, 100, 100, -0.25f, kRed);  // nearer (depth 0.25)
    Quad(b, 0, 0, 100, 100, -0.75f, kGreen); // farther, drawn later
    Quad(b, 200, 0, 300, 100, -0.75f, kGreen);
    Quad(b, 200, 0, 300, 100, -0.25f, kRed);
    r.Run(b.Chunk());
  }
  Check(Near(EfbPixel(r, 50, 50), 255, 0, 0) && Near(EfbPixel(r, 250, 50), 255, 0, 0),
        "depth test: the nearer quad wins in either order");

  // blending: src alpha over dst; alpha test discards below the reference
  {
    Builder b;
    Baseline(b);
    DisplayCopy(b, true, kBlack);
    Quad(b, 0, 0, 100, 100, 0, kBlue);
    const std::uint8_t half[4] = {255, 0, 0, 128};
    b.Bp(0x41, 1 | 1 << 3 | 1 << 4 | 5 << 5 | 4 << 8); // SRCALPHA, INVSRCALPHA
    Quad(b, 0, 0, 100, 100, 0, half);
    b.Bp(0x41, 1 << 3 | 1 << 4);
    b.Bp(0xF3, 100 | 6u << 16 | 0u << 19 | 0u << 22); // alpha >= 100 AND never -> discard all
    const std::uint8_t low[4] = {0, 255, 0, 50};
    Quad(b, 200, 0, 300, 100, 0, low);
    b.Bp(0xF3, 40 | 6u << 16 | 7u << 19); // alpha >= 40 AND always
    Quad(b, 400, 0, 500, 100, 0, low);
    r.Run(b.Chunk());
  }
  auto blended = EfbPixel(r, 50, 50);
  Check(Near(blended, 128, 0, 127, 3), "blending: half red over blue");
  if (!Near(blended, 128, 0, 127, 3))
    Print("blended", blended);
  Check(Near(EfbPixel(r, 250, 50), 0, 0, 0), "alpha test: a failing compare discards");
  Check(Near(EfbPixel(r, 450, 50), 0, 255, 0), "alpha test: a passing compare draws");
}

void TestTexture(re4gx::GxRenderer &r) {
  // a 4x4 RGBA8 texture: texel (x, y) = (x * 60, y * 60, 200), sampled nearest
  // over a 64x64 quad at (100, 100); the TEV takes the texture colour
  std::vector<std::uint8_t> tex(64);
  for (int i = 0; i < 16; i++) {
    const int x = i % 4, y = i / 4;
    tex[i * 2] = 255;                                   // A
    tex[i * 2 + 1] = static_cast<std::uint8_t>(x * 60); // R
    tex[32 + i * 2] = static_cast<std::uint8_t>(y * 60); // G
    tex[32 + i * 2 + 1] = 200;                           // B
  }
  Builder b;
  Baseline(b);
  DisplayCopy(b, true, kBlack);
  b.Bp(0x00, 1u | 1u << 4); // one texture coordinate
  b.Xf(0x103F, {1});
  b.Xf(0x1040, {5u << 7}); // texgen 0: regular, from TEX0, ST
  b.Bp(0x28, 0 | 0 << 3 | 1 << 6 | 0 << 7 | 0x3F7u << 12);
  b.Bp(0xC0, 8u | 15u << 4 | 15u << 8 | 15u << 12 | 1u << 19); // colour = texture
  b.Bp(0x80, 0);                                           // clamp, nearest
  b.Bp(0x84, 0);
  b.Bp(0x88, 3 | 3 << 10 | 6u << 20);
  b.Bp(0x30, 3);
  b.Bp(0x31, 3);
  b.Texture(0, 0x1234, 0, 0x100000, 4, 4, 6, tex);
  std::vector<std::uint8_t> v;
  VT(v, 100, 100, 0, kWhite, 0, 0);
  VT(v, 164, 100, 0, kWhite, 1, 0);
  VT(v, 164, 164, 0, kWhite, 1, 1);
  VT(v, 100, 164, 0, kWhite, 0, 1);
  b.Draw(RE4_GX_V_COLOR0 | RE4_GX_V_TEX0, 24, v, 4, 0x80);
  // the same texture again by key, without data, over another quad
  b.Texture(0, 0x1234, 0, 0x100000, 4, 4, 6, {});
  std::vector<std::uint8_t> v2;
  VT(v2, 300, 100, 0, kWhite, 0, 0);
  VT(v2, 364, 100, 0, kWhite, 1, 0);
  VT(v2, 364, 164, 0, kWhite, 1, 1);
  VT(v2, 300, 164, 0, kWhite, 0, 1);
  b.Draw(RE4_GX_V_COLOR0 | RE4_GX_V_TEX0, 24, v2, 4, 0x80);
  r.Run(b.Chunk());
  auto t00 = EfbPixel(r, 104, 104), t30 = EfbPixel(r, 160, 104), t03 = EfbPixel(r, 104, 160);
  Check(Near(t00, 0, 0, 200) && Near(t30, 180, 0, 200) && Near(t03, 0, 180, 200),
        "texture: texel (0,0) top left, x to the right, y down");
  if (!Near(t00, 0, 0, 200) || !Near(t30, 180, 0, 200) || !Near(t03, 0, 180, 200))
    Print("t00", t00), Print("t30", t30), Print("t03", t03);
  Check(Near(EfbPixel(r, 360, 160), 180, 180, 200), "texture: bound again by key without data");
  Check(r.Stats().missingTextures == 0, "no missing textures");
}

void TestTev(re4gx::GxRenderer &r) {
  Builder b;
  Baseline(b);
  DisplayCopy(b, true, kBlack);
  // konstant colour K1 = (10, 20, 30, 40), stage 0 colour = KONST (kcsel 0x0D)
  b.Bp(0xE2, 10 | 40u << 12 | 1u << 23);  // K1 RA
  b.Bp(0xE3, 30 | 20u << 12 | 1u << 23);  // K1 BG
  b.Bp(0xF6, (0 | 1 << 2) | 0x0Du << 4);  // stage 0 kcsel K1
  b.Bp(0xC0, 14u | 15u << 4 | 15u << 8 | 15u << 12 | 1u << 19);
  Quad(b, 0, 0, 50, 50, 0, kWhite);
  // lerp: a = red vertex colour, b = C0 (0, 0, 200), c = HALF (128): ~half way
  b.Bp(0xE2, 0 | 255u << 12);       // C0 RA (register, not konst)
  b.Bp(0xE3, 200 | 0u << 12);       // C0 BG
  b.Bp(0xC0, 15u | 13u << 4 | 2u << 8 | 10u << 12 | 1u << 19);
  Quad(b, 100, 0, 150, 50, 0, kRed);
  // compare mode RGB8_GT: d = 0 + (a > b ? c : 0) per component; a = ras (255,0,0), b = HALF, c = ONE
  b.Bp(0xC0, 15u | 12u << 4 | 13u << 8 | 10u << 12 | 3u << 16 | 0u << 18 | 3u << 20 | 1u << 19);
  Quad(b, 200, 0, 250, 50, 0, kRed);
  // scale x2 and subtract: d (ras = 100,100,100) - a(0) ... (d - lerp(0, 0, x)) * 2 = 200
  const std::uint8_t grey[4] = {100, 100, 100, 255};
  b.Bp(0xC0, 10u | 15u << 4 | 15u << 8 | 15u << 12 | 1u << 18 | 1u << 20 | 1u << 19);
  Quad(b, 300, 0, 350, 50, 0, grey);
  r.Run(b.Chunk());
  auto konst = EfbPixel(r, 25, 25), lerp = EfbPixel(r, 125, 25), cmp = EfbPixel(r, 225, 25), scaled = EfbPixel(r, 325, 25);
  Check(Near(konst, 10, 20, 30, 0), "TEV: konstant colour K1");
  Check(Near(lerp, 128, 0, 100, 1), "TEV: lerp(a, b, half) with the hardware rounding");
  Check(Near(cmp, 255, 0, 0, 0), "TEV: compare RGB8_GT");
  Check(Near(scaled, 200, 200, 200, 0), "TEV: subtract with scale 2");
  if (!Near(lerp, 128, 0, 100, 1))
    Print("lerp", lerp);
  if (!Near(cmp, 255, 0, 0, 0))
    Print("compare", cmp);
}

void TestLighting(re4gx::GxRenderer &r) {
  // one light at +z far away (directional), white; material from the vertex
  // (white), ambient register dark grey; normals towards / away from the light
  Builder b;
  Baseline(b);
  DisplayCopy(b, true, kBlack);
  b.Xf(0x100A, {0x202020FFu});                                   // ambient 0
  b.Xf(0x100E, {1u | 1u << 1 | 1u << 2 | 2u << 7 | 2u << 9});   // vertex material, lighting, light 0, clamp, dir
  std::vector<std::uint32_t> light(16, 0);
  light[3] = 0xFFFFFFFFu;
  light[4] = Bits(1.0f);
  light[7] = Bits(1.0f);
  light[12] = Bits(100000.0f); // position far along +z
  b.Xf(0x600, light);
  auto quad = [&](float x0, float nz) {
    std::vector<std::uint8_t> v;
    for (int i = 0; i < 4; i++) {
      const float p[3] = {i == 1 || i == 2 ? x0 + 50 : x0, i >= 2 ? 50.0f : 0.0f, 0.0f};
      const float n[3] = {0, 0, nz};
      v.insert(v.end(), reinterpret_cast<const std::uint8_t *>(p), reinterpret_cast<const std::uint8_t *>(p) + 12);
      v.insert(v.end(), reinterpret_cast<const std::uint8_t *>(n), reinterpret_cast<const std::uint8_t *>(n) + 12);
      v.insert(v.end(), kWhite, kWhite + 4);
    }
    b.Draw(RE4_GX_V_NORMAL | RE4_GX_V_COLOR0, 28, v, 4, 0x80);
  };
  quad(0, 1.0f);
  quad(100, -1.0f);
  r.Run(b.Chunk());
  Check(Near(EfbPixel(r, 25, 25), 255, 255, 255), "lighting: a normal towards the light is lit fully");
  Check(Near(EfbPixel(r, 125, 25), 32, 32, 32), "lighting: a normal away from the light gets the ambient only");
}

// viaXfb: read the result from a display copy (a multisampled EFB has the
// picture in its samples until a copy resolves it)
void TestCopyAndPalette(re4gx::GxRenderer &r, bool viaXfb = false) {
  // draw red into the EFB, copy 32x32 at (0, 0) to a texture at 0x200000
  // (RGBA8), then draw a quad with that copy as texture map 0
  Builder b;
  Baseline(b);
  DisplayCopy(b, true, kBlack);
  Quad(b, 0, 0, 32, 32, 0, kRed);
  b.Bp(0x49, 0);
  b.Bp(0x4A, 31 | 31u << 10);
  b.Bp(0x4D, 32);
  b.Bp(0x4B, 0x200000 >> 5);
  b.Bp(0x52, 6u << 4 | 2u << 15); // RGBA8 to a texture
  b.Bp(0x4A, 639 | 527u << 10);
  b.Bp(0x00, 1u | 1u << 4);
  b.Xf(0x103F, {1});
  b.Xf(0x1040, {5u << 7});
  b.Bp(0x28, 0 | 0 << 3 | 1 << 6 | 0 << 7 | 0x3F7u << 12);
  b.Bp(0xC0, 8u | 15u << 4 | 15u << 8 | 15u << 12 | 1u << 19);
  b.Bp(0x88, 31 | 31 << 10 | 6u << 20);
  b.Bp(0x30, 31);
  b.Bp(0x31, 31);
  b.Texture(0, 0x200000ull | 1ull << 32, RE4_GX_TEX_COPY, 0x200000, 32, 32, 6, {});
  std::vector<std::uint8_t> v;
  VT(v, 300, 300, 0, kWhite, 0, 0);
  VT(v, 340, 300, 0, kWhite, 1, 0);
  VT(v, 340, 340, 0, kWhite, 1, 1);
  VT(v, 300, 340, 0, kWhite, 0, 1);
  b.Draw(RE4_GX_V_COLOR0 | RE4_GX_V_TEX0, 24, v, 4, 0x80);
  // a C8 texture with an RGB565 palette: index i -> palette entry i
  std::vector<std::uint8_t> palette(512);
  for (int i = 0; i < 256; i++) {
    palette[i * 2] = i == 5 ? 0x07 : 0xF8; // entry 5: green (0x07E0), the rest red (0xF800)
    palette[i * 2 + 1] = i == 5 ? 0xE0 : 0x00;
  }
  const std::uint32_t tlutAt = 0x200 << 9;
  {
    const std::uint32_t at = b.Blob(palette.data(), palette.size());
    const std::uint32_t p[3] = {tlutAt, 512, at};
    b.Op(RE4_GX_OP_TLUT, p, 3);
  }
  std::vector<std::uint8_t> c8(32, 5); // 8x4 texels, all index 5
  b.Bp(0x88, 7 | 3 << 10 | 9u << 20);
  b.Bp(0x98, 0x200 | 1u << 10);
  b.Bp(0x30, 7);
  b.Bp(0x31, 3);
  b.Texture(0, 0x5555, 0, 0x300000, 8, 4, 9, c8, 0x200 | 1u << 10);
  std::vector<std::uint8_t> v2;
  VT(v2, 400, 300, 0, kWhite, 0, 0);
  VT(v2, 440, 300, 0, kWhite, 1, 0);
  VT(v2, 440, 340, 0, kWhite, 1, 1);
  VT(v2, 400, 340, 0, kWhite, 0, 1);
  b.Draw(RE4_GX_V_COLOR0 | RE4_GX_V_TEX0, 24, v2, 4, 0x80);
  if (viaXfb) {
    b.Bp(0x49, 0);
    b.Bp(0x4A, 639 | 527u << 10);
    DisplayCopy(b, false, kBlack);
  }
  r.Run(b.Chunk());
  auto pixel = [&](int x, int y) { return viaXfb ? TexPixel(r.Xfb(nullptr, nullptr), x, y) : EfbPixel(r, x, y); };
  Check(r.Stats().textureCopies == 1, "one EFB copy to a texture");
  Check(Near(pixel(320, 320), 255, 0, 0), "EFB copy used as a texture");
  Check(Near(pixel(420, 320), 0, 255, 0), "paletted texture: C8 through an RGB565 palette");
}

void TestFog(re4gx::GxRenderer &r) {
  // linear fog, orthographic: ze = A * depth - C; A = 1, C = 0 -> factor = depth
  Builder b;
  Baseline(b);
  DisplayCopy(b, true, kBlack);
  const auto fogWord = [](float f) {
    const std::uint32_t v = Bits(f);
    return ((v >> 12) & 0x7FF) | ((v >> 23) & 0xFF) << 11 | (v >> 31) << 19;
  };
  b.Bp(0xEE, fogWord(1.0f));
  b.Bp(0xF1, fogWord(0.0f) | 1u << 20 | 2u << 21); // orthographic, linear
  b.Bp(0xF2, 0xFFFFFF);                              // white fog
  Quad(b, 0, 0, 50, 50, -0.5f, kBlack);              // depth 0.5 -> half fog
  r.Run(b.Chunk());
  auto p = EfbPixel(r, 25, 25);
  Check(Near(p, 128, 128, 128, 2), "fog: linear, half way at depth 0.5");
  if (!Near(p, 128, 128, 128, 2))
    Print("fog", p);
}

// Random TEV setups: every generated shader compiles and links.
void TestShaderVariety(re4gx::GxRenderer &r) {
  std::mt19937 rng(42);
  const unsigned before = r.Stats().shaderErrors;
  for (int round = 0; round < 60; round++) {
    Builder b;
    Baseline(b);
    const unsigned stages = 1 + rng() % 16, ind = rng() % 3;
    b.Bp(0x00, (2u + 0) | 2u << 4 | (stages - 1) << 10 | ind << 16 | (rng() % 3) << 14);
    b.Xf(0x103F, {2});
    b.Xf(0x1040, {static_cast<std::uint32_t>(5u << 7 | (rng() & 2))});
    b.Xf(0x1041, {static_cast<std::uint32_t>(6u << 7 | (rng() & 6))});
    b.Xf(0x1012, {static_cast<std::uint32_t>(rng() & 1)});
    b.Xf(0x1009, {2});
    b.Xf(0x100E, {static_cast<std::uint32_t>(rng() & 0x7FFF)});
    b.Xf(0x100F, {static_cast<std::uint32_t>(rng() & 0x7FFF)});
    b.Xf(0x1010, {static_cast<std::uint32_t>(rng() & 0x7FFF)});
    b.Xf(0x1011, {static_cast<std::uint32_t>(rng() & 0x7FFF)});
    for (unsigned s = 0; s < stages; s++) {
      b.Bp(0xC0 + 2 * s, rng() & 0xFFFFFF);
      b.Bp(0xC1 + 2 * s, rng() & 0xFFFFFF);
      b.Bp(0x10 + s, ind ? (rng() & 0x1FFFFF) : 0);
    }
    for (int i = 0; i < 8; i++) {
      b.Bp(0x28 + i, rng() & 0xFFFFFF);
      b.Bp(0xF6 + i, rng() & 0xFFFFFF);
    }
    b.Bp(0x27, rng() & 0xFFFFFF);
    b.Bp(0xF3, rng() & 0xFFFFFF);
    b.Bp(0xF1, rng() & 0xFFFFFF);
    std::vector<std::uint8_t> v;
    for (int i = 0; i < 3; i++) {
      const float p[3] = {static_cast<float>(i * 10), static_cast<float>(i * 7), 0};
      const float n[3] = {0, 0, 1};
      const float st[4] = {0.1f, 0.2f, 0.3f, 0.4f};
      v.insert(v.end(), reinterpret_cast<const std::uint8_t *>(p), reinterpret_cast<const std::uint8_t *>(p) + 12);
      v.insert(v.end(), reinterpret_cast<const std::uint8_t *>(n), reinterpret_cast<const std::uint8_t *>(n) + 12);
      v.insert(v.end(), kRed, kRed + 4);
      v.insert(v.end(), kGreen, kGreen + 4);
      v.insert(v.end(), reinterpret_cast<const std::uint8_t *>(st), reinterpret_cast<const std::uint8_t *>(st) + 16);
    }
    b.Draw(RE4_GX_V_NORMAL | RE4_GX_V_COLOR0 | RE4_GX_V_COLOR1 | RE4_GX_V_TEX0 | (RE4_GX_V_TEX0 << 1), 48, v, 3);
    r.Run(b.Chunk());
  }
  Check(r.Stats().shaderErrors == before, "random TEV setups: every shader compiles");
  std::fprintf(stderr, "  shaders built: %u\n", r.Stats().programs);
}

} // namespace

// The game's brightness option (main_sub.cpp Bg_brightness_set): the seven
// copy filter taps (BP 0x53/0x54, 1/64 each) of the display copy sum to 64 at
// the normal setting; more brightens, less darkens the copied picture.
void TestCopyFilter(re4gx::GxRenderer &r) {
  const std::uint8_t grey[4] = {100, 100, 100, 255};
  struct {
    unsigned taps[7];
    int expect;
    const char *what;
  } cases[] = {{{8, 8, 10, 12, 10, 8, 8}, 100, "copy filter: sum 64 (normal brightness) keeps the colour"},
               {{10, 10, 12, 15, 12, 10, 10}, 123, "copy filter: sum 79 brightens by 79/64"},
               {{6, 6, 8, 9, 8, 6, 6}, 77, "copy filter: sum 49 darkens by 49/64"},
               {{20, 20, 25, 30, 25, 20, 20}, 250, "copy filter: sum 160 brightens and stays in range"}};
  for (const auto &c : cases) {
    Builder b;
    Baseline(b);
    b.Bp(0x53, c.taps[0] | c.taps[1] << 6 | c.taps[2] << 12 | c.taps[3] << 18);
    b.Bp(0x54, c.taps[4] | c.taps[5] << 6 | c.taps[6] << 12);
    DisplayCopy(b, true, kBlack);
    Quad(b, 10, 20, 110, 70, 0, grey);
    DisplayCopy(b, false, kBlack);
    r.Run(b.Chunk());
    auto px = TexPixel(r.Xfb(nullptr, nullptr), 50, 40);
    Check(Near(px, c.expect, c.expect, c.expect), c.what);
    if (!Near(px, c.expect, c.expect, c.expect))
      Print("xfb", px);
  }
  // back to the normal filter for the tests after
  Builder b;
  Baseline(b);
  b.Bp(0x53, 8 | 8 << 6 | 10 << 12 | 12 << 18);
  b.Bp(0x54, 10 | 8 << 6 | 8 << 12);
  DisplayCopy(b, true, kBlack);
  r.Run(b.Chunk());
}

// The gameplay autofocus (BP 0x5A, the core's marker; runtime/dof.cpp): a
// perspective scene (near 100, far 1,000,000 as the game's): far behind, a
// red / blue edge at x = 160 (20 m away); at the centre a grey card 1 m
// away (the focus); right of it a near green / white edge at 1.5 m. The
// marker softens the far edge, the near one stays sharp; with nothing near at
// the centre (looking into the distance) the picture stays as it is.
void QuadAt(Builder &b, float sx0, float sy0, float sx1, float sy1, float distance, const std::uint8_t c[4]) {
  // screen pixels through the viewport of Baseline (centre 320, 264; half 320 x 264)
  auto x = [&](float sx) { return (sx - 320.0f) / 320.0f * distance; };
  auto y = [&](float sy) { return (264.0f - sy) / 264.0f * distance; };
  std::vector<std::uint8_t> v;
  V(v, x(sx0), y(sy0), -distance, c);
  V(v, x(sx1), y(sy0), -distance, c);
  V(v, x(sx1), y(sy1), -distance, c);
  V(v, x(sx0), y(sy1), -distance, c);
  b.Draw(RE4_GX_V_COLOR0, 16, v, 4, 0x80);
}

void AutofocusScene(Builder &b, bool nearCentre, bool mark) {
  Baseline(b);
  DisplayCopy(b, true, kBlack);
  const float n = 100.0f, f = 1000000.0f;
  b.XfF(0x1020, {1.0f, 0.0f, 1.0f, 0.0f, -n / (f - n), -f * n / (f - n)});
  b.Xf(0x1026, {0});
  const std::uint8_t red[4] = {255, 0, 0, 128}, blue[4] = {0, 0, 255, 128}, grey[4] = {128, 128, 128, 255};
  QuadAt(b, 0, 0, 160, 528, 20000.0f, red);
  QuadAt(b, 160, 0, 640, 528, 20000.0f, blue);
  if (nearCentre)
    QuadAt(b, 280, 224, 360, 304, 1000.0f, grey);
  QuadAt(b, 440, 400, 480, 480, 1500.0f, kGreen);
  QuadAt(b, 480, 400, 520, 480, 1500.0f, kWhite);
  if (mark)
    b.Bp(0x5A, 1);
  DisplayCopy(b, false, kBlack);
}

void TestAutofocus(re4gx::GxRenderer &r, bool multisampled = false) {
  char tag[64];
  std::snprintf(tag, sizeof(tag), "autofocus%s%s", multisampled ? " (multisampled)" : "",
                r.HalfDof() ? "" : " (full resolution)");
  auto run = [&](bool nearCentre, bool mark) {
    Builder b;
    AutofocusScene(b, nearCentre, mark);
    r.Run(b.Chunk());
    return r.Xfb(nullptr, nullptr);
  };
  char what[160];
  const unsigned before = r.Stats().autofocus;
  GLuint xfb = run(true, false);
  auto sharpL = TexPixel(xfb, 159, 100), sharpR = TexPixel(xfb, 160, 100);
  std::snprintf(what, sizeof(what), "%s: without the marker the far edge is sharp", tag);
  Check(Near(sharpL, 255, 0, 0) && Near(sharpR, 0, 0, 255), what);
  xfb = run(true, true);
  std::snprintf(what, sizeof(what), "%s: one blur pass for the marker", tag);
  Check(r.Stats().autofocus == before + 1, what);
  auto l = TexPixel(xfb, 159, 100), rr = TexPixel(xfb, 160, 100);
  std::snprintf(what, sizeof(what), "%s: the far edge behind the focus gets soft", tag);
  Check(l[2] > 20 && l[0] < 240 && rr[0] > 20 && rr[2] < 240, what);
  if (!(l[2] > 20 && l[0] < 240 && rr[0] > 20 && rr[2] < 240)) {
    Print("left of the edge", l);
    Print("right of the edge", rr);
  }
  std::snprintf(what, sizeof(what), "%s: away from the edge the background keeps its colour", tag);
  Check(Near(TexPixel(xfb, 80, 100), 255, 0, 0) && Near(TexPixel(xfb, 600, 100), 0, 0, 255), what);
  std::snprintf(what, sizeof(what), "%s: the focused card stays as it is", tag);
  Check(Near(TexPixel(xfb, 320, 264), 128, 128, 128), what);
  auto g = TexPixel(xfb, 479, 440), w = TexPixel(xfb, 480, 440);
  std::snprintf(what, sizeof(what), "%s: the near edge in front of the focus stays sharp", tag);
  Check(Near(g, 0, 255, 0) && Near(w, 255, 255, 255), what);
  // the blurred background right next to it hardly takes its colour (llvmpipe: 0 at the full
  // resolution, 8 at the half one, 20 when the half one's centre texels in front counted too)
  auto beside = TexPixel(xfb, 520, 440), below = TexPixel(xfb, 500, 480);
  std::snprintf(what, sizeof(what), "%s: the background next to the sharp near edge keeps its colour", tag);
  Check(Near(beside, 0, 0, 255, 12) && Near(below, 0, 0, 255, 12), what);
  if (!(Near(beside, 0, 0, 255, 12) && Near(below, 0, 0, 255, 12))) {
    Print("beside the white card", beside);
    Print("below the white card", below);
  }
  if (!multisampled) {
    std::snprintf(what, sizeof(what), "%s: the EFB alpha stays", tag);
    Check(EfbPixel(r, 159, 100)[3] == 128, what);
  }
  // the focus follows its measurements step by step (the Retroid with MSAA, 19.10.2026: thin
  // branches went soft for one picture): one picture without the card at the centre (measured
  // far) changes nothing, the far edge stays soft; after three the focus is far, the picture sharp
  xfb = run(false, true);
  l = TexPixel(xfb, 159, 100);
  rr = TexPixel(xfb, 160, 100);
  std::snprintf(what, sizeof(what), "%s: one picture measured far leaves the focus (the far edge stays soft)", tag);
  Check(l[2] > 20 && l[0] < 240 && rr[0] > 20 && rr[2] < 240, what);
  run(false, true);
  xfb = run(false, true);
  l = TexPixel(xfb, 159, 100);
  rr = TexPixel(xfb, 160, 100);
  std::snprintf(what, sizeof(what), "%s: looking into the distance for three pictures leaves the picture sharp", tag);
  Check(Near(l, 255, 0, 0) && Near(rr, 0, 0, 255), what);
  if (!(Near(l, 255, 0, 0) && Near(rr, 0, 0, 255))) {
    Print("left of the edge", l);
    Print("right of the edge", rr);
  }
  // a picture without the marker in between: the next autofocus starts anew, at once
  xfb = run(false, true);
  run(true, false);
  xfb = run(true, true);
  l = TexPixel(xfb, 159, 100);
  rr = TexPixel(xfb, 160, 100);
  std::snprintf(what, sizeof(what), "%s: after a picture without the autofocus the focus is measured anew", tag);
  Check(l[2] > 20 && l[0] < 240 && rr[0] > 20 && rr[2] < 240, what);
  // standing (Retroid, 17.10.2026): Leon (a card 1 m away) covers the centre,
  // behind him a green / white wall 3 m away, the far edge at 20 m. Without
  // his depth in the marker he is the focus and the wall goes soft; with it
  // (1.5 m) the focus is the wall around him: the wall stays sharp, the far
  // edge gets soft, Leon stays as he is
  auto standing = [&](std::uint32_t player) {
    run(true, false);  // each case on its own: the autofocus starts anew
    Builder b;
    Baseline(b);
    DisplayCopy(b, true, kBlack);
    const float n = 100.0f, f = 1000000.0f;
    b.XfF(0x1020, {1.0f, 0.0f, 1.0f, 0.0f, -n / (f - n), -f * n / (f - n)});
    b.Xf(0x1026, {0});
    const std::uint8_t red[4] = {255, 0, 0, 255}, blue[4] = {0, 0, 255, 255}, grey[4] = {128, 128, 128, 255};
    QuadAt(b, 0, 0, 160, 528, 20000.0f, red);
    QuadAt(b, 160, 0, 640, 528, 20000.0f, blue);
    QuadAt(b, 200, 120, 260, 408, 3000.0f, kGreen);
    QuadAt(b, 260, 120, 440, 408, 3000.0f, kWhite);
    QuadAt(b, 280, 224, 360, 304, 1000.0f, grey);
    b.Bp(0x5A, player);
    DisplayCopy(b, false, kBlack);
    r.Run(b.Chunk());
    return r.Xfb(nullptr, nullptr);
  };
  xfb = standing(1);
  g = TexPixel(xfb, 259, 150);
  w = TexPixel(xfb, 260, 150);
  std::snprintf(what, sizeof(what), "%s: Leon as the focus softens the wall behind him (the problem)", tag);
  Check(!(Near(g, 0, 255, 0) && Near(w, 255, 255, 255)), what);
  xfb = standing(1500);
  g = TexPixel(xfb, 259, 150);
  w = TexPixel(xfb, 260, 150);
  std::snprintf(what, sizeof(what), "%s: with Leon's depth the wall around him is the focus and stays sharp", tag);
  Check(Near(g, 0, 255, 0) && Near(w, 255, 255, 255), what);
  if (!(Near(g, 0, 255, 0) && Near(w, 255, 255, 255))) {
    Print("wall green", g);
    Print("wall white", w);
  }
  l = TexPixel(xfb, 159, 100);
  rr = TexPixel(xfb, 160, 100);
  std::snprintf(what, sizeof(what), "%s: with Leon's depth the far edge behind the wall gets soft", tag);
  Check(l[2] > 20 && l[0] < 240 && rr[0] > 20 && rr[2] < 240, what);
  std::snprintf(what, sizeof(what), "%s: Leon himself stays sharp", tag);
  Check(Near(TexPixel(xfb, 320, 264), 128, 128, 128), what);
  // all the picture is Leon or nearer: no focus, no blur
  xfb = standing(30000);
  l = TexPixel(xfb, 159, 100);
  rr = TexPixel(xfb, 160, 100);
  std::snprintf(what, sizeof(what), "%s: nothing behind Leon: the picture stays sharp", tag);
  Check(Near(l, 255, 0, 0) && Near(rr, 0, 0, 255), what);
  // back to the orthographic projection of the other tests
  Builder b;
  Baseline(b);
  DisplayCopy(b, true, kBlack);
  r.Run(b.Chunk());
}

// The game draws on after an EFB copy (GPU step d2: on the tiles the copy ends the GPU's pass,
// the EFB is drawn back after it): what lies in front before the copy still hides what is drawn
// behind it after, a draw at exactly the same depth with EQUAL still passes, and the colours
// drawn before stay.
void TestPassBreak(re4gx::GxRenderer &r, const char *tag) {
  char what[160];
  Builder b;
  Baseline(b);
  DisplayCopy(b, false, kBlack);
  const unsigned restores = r.Stats().efbRestores;
  Quad(b, 0, 0, 640, 528, -1.0f, kBlue);     // background, depth 1
  Quad(b, 100, 100, 300, 300, -0.5f, kGreen);  // in front, depth 0.5
  // a copy of 32x32 at (0, 0) to a texture: the pass ends
  b.Bp(0x49, 0);
  b.Bp(0x4A, 31 | 31u << 10);
  b.Bp(0x4D, 32);
  b.Bp(0x4B, 0x200000 >> 5);
  b.Bp(0x52, 6u << 4 | 2u << 15);
  b.Bp(0x4A, 639 | 527u << 10);
  Quad(b, 200, 200, 400, 400, -0.75f, kRed);  // behind the green one, depth 0.75
  b.Bp(0x40, 1 | 2 << 1 | 1 << 4);            // EQUAL
  Quad(b, 100, 100, 150, 150, -0.5f, kWhite);  // the green one's depth exactly
  b.Bp(0x40, 1 | 3 << 1 | 1 << 4);
  DisplayCopy(b, false, kBlack);
  r.Run(b.Chunk());
  GLuint xfb = r.Xfb(nullptr, nullptr);
  auto front = TexPixel(xfb, 250, 250), behind = TexPixel(xfb, 350, 350), equal = TexPixel(xfb, 120, 120),
       before = TexPixel(xfb, 50, 450);
  std::snprintf(what, sizeof(what), "%s: after a copy, the depth drawn before still hides what lies behind", tag);
  Check(Near(front, 0, 255, 0), what);
  std::snprintf(what, sizeof(what), "%s: after a copy, what lies in front of the background is drawn", tag);
  Check(Near(behind, 255, 0, 0), what);
  std::snprintf(what, sizeof(what), "%s: after a copy, a draw at exactly the same depth passes EQUAL", tag);
  Check(Near(equal, 255, 255, 255), what);
  std::snprintf(what, sizeof(what), "%s: after a copy, the colour drawn before stays", tag);
  Check(Near(before, 0, 0, 255), what);
  if (!(Near(front, 0, 255, 0) && Near(behind, 255, 0, 0) && Near(equal, 255, 255, 255) && Near(before, 0, 0, 255))) {
    Print("green in front", front);
    Print("red behind", behind);
    Print("white EQUAL", equal);
    Print("blue before", before);
  }
  if (r.TiledMsaa()) {
    // the copy (and the display copy) end passes: drawn back before the draws after them
    std::snprintf(what, sizeof(what), "%s: the EFB drawn back after the copy", tag);
    Check(r.Stats().efbRestores >= restores + 1, what);
  }
}

// The uniform stream moves on to its next quarter only between chunks: a fence set while a
// picture was drawn changed the multisampled pixels around it on the Adreno 650 (thin branches
// paler for one picture, 20.10.2026). Chunks of 700 draws, each with its own position matrix
// (one XF block in the stream per draw, about 2.3 MB a chunk), switch before they start; one of
// 1400 draws (more than a quarter) has to switch inside. Every draw lands where its matrix says.
void TestUniformStream(re4gx::GxRenderer &r, const char *tag) {
  char what[160];
  auto chunk = [&](unsigned draws, bool reset) {
    Builder b;
    if (reset)
      Baseline(b);
    else
      b.flags = 0;
    DisplayCopy(b, true, kBlack);
    for (unsigned i = 0; i < draws; i++) {
      // a 4x4 square on a grid of 40 x 33 places, the matrix moves it there
      const float x = static_cast<float>(i % 40) * 16.0f, y = static_cast<float>(i / 40 % 33) * 16.0f;
      b.XfF(0, {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, 0});
      Quad(b, 4, 4, 8, 8, 0, (i & 1) ? kGreen : kRed);
    }
    b.XfF(0, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0});
    DisplayCopy(b, false, kBlack);
    const re4gx::GxStats before = r.Stats();
    r.Run(b.Chunk());
    const re4gx::GxStats &s = r.Stats();
    // the first and the last square of the chunk, and the black between
    const GLuint xfb = r.Xfb(nullptr, nullptr);
    const unsigned last = draws - 1;
    const int lx = static_cast<int>(last % 40) * 16 + 6, ly = static_cast<int>(last / 40 % 33) * 16 + 6;
    const bool ok = xfb && Near(TexPixel(xfb, 6, 6), 255, 0, 0) && Near(TexPixel(xfb, 2, 2), 0, 0, 0) &&
                    Near(TexPixel(xfb, lx, ly), (last & 1) ? 0 : 255, (last & 1) ? 255 : 0, 0);
    return std::make_pair(ok, std::make_pair(s.streamSwitches - before.streamSwitches,
                                             s.streamSplits - before.streamSplits));
  };
  unsigned switches = 0, splits = 0;
  bool right = true;
  for (int i = 0; i < 10; i++) {
    const auto res = chunk(700, i == 0);
    right = right && res.first;
    switches += res.second.first;
    splits += res.second.second;
  }
  std::snprintf(what, sizeof(what), "%s: uniform stream, chunks of 700 draws drawn right", tag);
  Check(right, what);
  std::snprintf(what, sizeof(what), "%s: uniform stream switches between chunks (%u), none inside (%u)", tag,
                switches, splits);
  Check(switches >= 8 && splits == 0, what);
  const auto big = chunk(1400, false);
  std::snprintf(what, sizeof(what), "%s: uniform stream, a chunk larger than a quarter drawn right", tag);
  Check(big.first, what);
  std::snprintf(what, sizeof(what), "%s: uniform stream, a chunk larger than a quarter switches inside (%u)", tag,
                big.second.second);
  Check(big.second.second >= 1, what);
  // and back to chunks that fit: from a quarter's start again, none inside
  const auto after = chunk(700, false);
  std::snprintf(what, sizeof(what), "%s: uniform stream, after the large chunk one of 700 draws right, none inside",
                tag);
  Check(after.first && after.second.first == 1 && after.second.second == 0, what);
}

// The menu's picture options (M11): a multisampled EFB is resolved before
// every copy out of it (display copy, texture copy); a slanted edge comes out
// between the two colours; anisotropic filtering does not change a picture
// without mipmaps.
void TestOptions(bool poison, bool tiled, bool halfDof = true, int lose = 0) {
  int maxSamples = 1;
  float maxAnisotropy = 1.0f;
  re4gx::GxRenderer::Limits(&maxSamples, &maxAnisotropy);
  std::fprintf(stderr, "  options: up to %d samples, anisotropy up to %.0f, MSAA %s%s\n", maxSamples, maxAnisotropy,
               tiled ? "on the tiles where the GPU can" : "in own buffers",
               lose == 2   ? ", everything lost at a pass's end, the depth never stored"
               : lose == 1 ? ", everything lost at a pass's end"
                           : "");
  re4gx::GxRenderer r;
  re4gx::GxOptions o;
  o.samples = 4;
  o.anisotropy = 8.0f;
  o.invalidate = poison;
  o.poison = poison;
  o.tiledMsaa = tiled;
  o.halfDof = halfDof;
  o.loseTiles = lose;
  r.SetOptions(o);
  std::string error;
  Require(r.Init(1, &error), error.c_str());
  // MSAA on the tiles exactly where the GPU has both extensions (GPU step d)
  const char *ext = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
  const bool can = ext && std::strstr(ext, "GL_EXT_multisampled_render_to_texture2") && maxSamples > 1;
  // (d2: not when the device never stores the resolved depth; the self-test at Init finds it)
  Check(r.TiledMsaa() == (tiled && can && lose < 2), "options: MSAA on the tiles where asked for and the GPU can");
  if (tiled && can && lose == 2)
    Check(r.Samples() > 1, "options: the depth never stored on the tiles: own buffers instead");
  Check(r.Samples() == (maxSamples >= 4 ? 4 : 1) || (r.Samples() > 1 && r.Samples() <= maxSamples),
        "options: the samples asked for, as far as the device goes");
  Check(r.Anisotropy() == std::min(8.0f, maxAnisotropy), "options: the anisotropy, as far as the device goes");
  {
    Builder b;
    Baseline(b);
    DisplayCopy(b, true, kBlue);
    Quad(b, 10, 20, 110, 70, 0, kGreen);
    std::vector<std::uint8_t> v;  // a slanted edge from (200, 100) to (300, 160)
    V(v, 200, 100, 0, kRed);
    V(v, 300, 160, 0, kRed);
    V(v, 200, 160, 0, kRed);
    b.Draw(RE4_GX_V_COLOR0, 16, v, 3);
    DisplayCopy(b, false, kBlue);
    r.Run(b.Chunk());
  }
  GLuint xfb = r.Xfb(nullptr, nullptr);
  Check(xfb && Near(TexPixel(xfb, 50, 40), 0, 255, 0), "multisampled: the display copy holds the picture");
  Check(xfb && Near(TexPixel(xfb, 300, 300), 0, 0, 255), "multisampled: the clear colour elsewhere");
  if (r.Samples() > 1 && xfb) {
    int between = 0;
    for (int x = 200; x < 300; x++) {
      const int y = 100 + (x - 200) * 60 / 100;  // on the edge
      auto px = TexPixel(xfb, x, y);
      between += px[0] > 20 && px[0] < 235 && px[2] > 20 && px[2] < 235;
    }
    Check(between > 20, "multisampled: the slanted edge is smoothed");
  }
  TestCopyAndPalette(r, true);  // EFB copy to a texture (resolved first), palette
  Check(r.HalfDof() == halfDof, "options: the depth of field's resolution as asked for");
  TestAutofocus(r, r.Samples() > 1);
  TestPassBreak(r, r.TiledMsaa() ? (lose ? "on the tiles, everything lost" : "on the tiles") : "multisampled");
  if (!poison && !lose)
    TestUniformStream(r, r.TiledMsaa() ? "on the tiles" : "multisampled");
  Check(glGetError() == GL_NO_ERROR, "options: no GL error");
  r.Release();
}

// 60 FPS (experimental): the in-between picture of two chunks (GxRenderer::Interpolate). A green
// card 1000 units away (80 pixels wide) moved by its position matrix (dx), a red one moved by its
// vertices (as the game skins its figures), both in perspective; between A (dx -50: 16 pixels to
// the left, red at 430..470) and B (dx +50, red at 450..490) the in-between picture shows them
// halfway, Run(B) afterwards draws B as without it (the state put back), t = 1 draws B, and a
// camera that turns 90 degrees is a cut.
void InterpScene(Builder &b, std::uint32_t sequence, float dx, float red0, float turn, Re4GxChunk *out) {
  Baseline(b);
  b.flags = sequence == 1 ? RE4_GX_CHUNK_RESET | RE4_GX_CHUNK_XFB : RE4_GX_CHUNK_XFB;
  DisplayCopy(b, true, kBlack);
  const float n = 100.0f, f = 1000000.0f;
  b.XfF(0x1020, {1.0f, 0.0f, 1.0f, 0.0f, -n / (f - n), -f * n / (f - n)});
  b.Xf(0x1026, {0});
  const float c = std::cos(turn), s = std::sin(turn);
  b.XfF(0, {c, 0, s, dx, 0, 1, 0, 0, -s, 0, c, 0});
  QuadAt(b, 280, 224, 360, 304, 1000.0f, kGreen);
  b.XfF(0, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0});
  QuadAt(b, red0, 380, red0 + 40, 420, 1000.0f, kRed);
  DisplayCopy(b, false, kBlack);
  *out = b.Chunk();
  out->sequence = sequence;
}

std::vector<std::uint8_t> XfbPixels(re4gx::GxRenderer &r) {
  unsigned w = 0, h = 0;
  const GLuint xfb = r.Xfb(&w, &h);
  std::vector<std::uint8_t> px(static_cast<std::size_t>(w) * h * 4);
  if (!xfb)
    return px;
  glBindFramebuffer(GL_FRAMEBUFFER, gReadFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, xfb, 0);
  glReadPixels(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h), GL_RGBA, GL_UNSIGNED_BYTE, px.data());
  return px;
}

void TestInterpolation() {
  std::string error;
  re4gx::GxRenderer r, plain;
  Require(r.Init(1, &error), error.c_str());
  Require(plain.Init(1, &error), error.c_str());
  r.SetRecording(true);
  Builder ba, bb, bc;
  Re4GxChunk a, b, c;
  InterpScene(ba, 1, -50.0f, 430.0f, 0.0f, &a);
  InterpScene(bb, 2, 50.0f, 450.0f, 0.0f, &b);
  InterpScene(bc, 3, 50.0f, 450.0f, 3.14159265f / 2, &c);
  r.Run(a);
  const GLuint xfbA = r.Xfb(nullptr, nullptr);
  Check(Near(TexPixel(xfbA, 272, 264), 0, 255, 0) && Near(TexPixel(xfbA, 435, 400), 255, 0, 0),
        "interpolation: picture A (green card at 264..344, red card at 430..470)");
  const bool shown = r.Interpolate(a, b, 0.5f);
  Check(shown, "interpolation: the in-between picture of A and B is shown");
  const GLuint xfb = r.Xfb(nullptr, nullptr);
  // A covers 264..344, B 296..376, the in-between picture 280..360
  auto mid = TexPixel(xfb, 320, 264), left = TexPixel(xfb, 272, 264), right = TexPixel(xfb, 368, 264);
  Check(Near(mid, 0, 255, 0) && Near(left, 0, 0, 0) && Near(right, 0, 0, 0),
        "interpolation: the card moved by its matrix halfway (280..360)");
  // red: A 430..470, B 450..490, in between 440..480
  auto red = TexPixel(xfb, 460, 400), redA = TexPixel(xfb, 435, 400), redB = TexPixel(xfb, 485, 400);
  Check(Near(red, 255, 0, 0) && Near(redA, 0, 0, 0) && Near(redB, 0, 0, 0),
        "interpolation: the card moved by its vertices halfway (440..480)");
  if (!Near(mid, 0, 255, 0) || !Near(red, 255, 0, 0))
    Print("middle", mid), Print("left", left), Print("right", right), Print("red", red);
  const auto &st = r.Stats();
  Check(st.interpPictures == 1 && st.interpMatched == 2 && st.interpVertexBlends == 1,
        "interpolation: both draws matched, one with blended vertices");
  if (st.interpMatched != 2 || st.interpVertexBlends != 1)
    std::fprintf(stderr, "  in-between: %u pictures, %u draws, %u matched, %u vertex blends\n", st.interpPictures,
                 st.interpDraws, st.interpMatched, st.interpVertexBlends);
  // Run(B) after it: the same picture as without the in-between one
  r.Run(b);
  plain.Run(a);
  plain.Run(b);
  const auto withPx = XfbPixels(r), withoutPx = XfbPixels(plain);
  Check(withPx == withoutPx && !withPx.empty(), "interpolation: Run(B) afterwards draws B as without it");
  Check(Near(TexPixel(r.Xfb(nullptr, nullptr), 368, 264), 0, 255, 0), "interpolation: picture B (green card at 296..376)");
  // a camera that turns by 90 degrees: a cut, no in-between picture
  Check(!r.Interpolate(b, c, 0.5f) && r.Stats().interpCuts == 1, "interpolation: a turn of 90 degrees is a cut");
  r.Run(c);
  // t = 1: the in-between picture is B (within rounding)
  re4gx::GxRenderer one;
  Require(one.Init(1, &error), error.c_str());
  one.SetRecording(true);
  one.Run(a);
  Check(one.Interpolate(a, b, 1.0f), "interpolation at t = 1: shown");
  const auto onePx = XfbPixels(one);
  unsigned differ = 0;
  for (std::size_t i = 0; i < onePx.size() && i < withoutPx.size(); i++)
    differ += std::abs(static_cast<int>(onePx[i]) - static_cast<int>(withoutPx[i])) > 2;
  Check(onePx.size() == withoutPx.size() && differ == 0, "interpolation at t = 1: the picture of B");
  // not recorded, or not the next chunk: nothing drawn
  Check(!plain.Interpolate(a, b, 0.5f), "interpolation: nothing without notes of the picture before");
  Check(!one.Interpolate(a, c, 0.5f), "interpolation: nothing between chunks that do not follow each other");
  Check(glGetError() == GL_NO_ERROR, "interpolation: no GL error");
  r.Release();
  plain.Release();
  one.Release();
}

int main() {
  Context();
  glGenFramebuffers(1, &gReadFbo);
  TestDecoder();
  // the GPU check of the movie colours as the app runs it at the start (gx_selfcheck.cpp)
  {
    int wrong = -1;
    const std::string line = re4gx::MovieColourCheck(&wrong);
    std::fprintf(stderr, "%s\n", line.c_str());
    Check(wrong == 0, "GPU check: the movie colours as the GameCube computes them");
  }
  // twice: as the app runs it, and with the copy targets, the blur and the focus painted magenta
  // where the GPU may forget them (GPU step b): every picture must stay the same
  for (int poison = 0; poison < 2; poison++) {
    if (poison)
      std::fprintf(stderr, "again with forgotten contents painted magenta:\n");
    re4gx::GxRenderer renderer;
    re4gx::GxOptions o;
    o.invalidate = poison != 0;
    o.poison = poison != 0;
    renderer.SetOptions(o);
    std::string error;
    Require(renderer.Init(1, &error), error.c_str());
    TestBasics(renderer);
    TestTexture(renderer);
    TestTev(renderer);
    TestLighting(renderer);
    TestCopyAndPalette(renderer);
    TestFog(renderer);
    TestShaderVariety(renderer);
    TestCopyFilter(renderer);
    TestAutofocus(renderer);
    TestPassBreak(renderer, "single-sampled");
    if (!poison)
      TestUniformStream(renderer, "single-sampled");
    Check(glGetError() == GL_NO_ERROR, "no GL error");
    renderer.Release();
    {
      // the depth of field as before GPU step e: the blur at the full resolution
      re4gx::GxRenderer full;
      o.halfDof = false;
      full.SetOptions(o);
      Require(full.Init(1, &error), error.c_str());
      TestAutofocus(full);
      Check(glGetError() == GL_NO_ERROR, "full-resolution depth of field: no GL error");
      full.Release();
    }
    TestOptions(poison != 0, true);
  }
  TestInterpolation();
  // MSAA in its own buffers (the devices without the extensions, RE4_GX_MSAA_TILES=0)
  TestOptions(false, false);
  // the depth of field's blur at the full resolution as before GPU step e (GxOptions::halfDof)
  TestOptions(false, false, false);
  // MSAA on the tiles on a GPU that loses colour and depth at every pass's end (the Adreno 650
  // lost the depth): the EFB drawn back (GPU step d2) keeps every picture; both resolutions of
  // the depth of field (its full one draws back into the EFB on the tiles)
  TestOptions(false, true, true, 1);
  TestOptions(true, true, false, 1);
  // ... and one that never stores the resolved depth either (the Adreno 650, it seems): the
  // self-test at Init takes own buffers, every picture right
  TestOptions(false, true, true, 2);
  std::fprintf(stderr, "gx_gl_check: %d failed\n", gFailures);
  return gFailures ? 1 : 0;
}
