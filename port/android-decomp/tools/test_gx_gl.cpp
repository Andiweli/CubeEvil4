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
#include "gx_gl.h"
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

// A chunk as the core's decoder writes it.
struct Builder {
  std::vector<std::uint32_t> ops;
  std::vector<std::uint8_t> vertices;
  std::vector<std::uint16_t> indices;
  std::vector<std::uint8_t> blob;
  std::uint32_t bp[256] = {};
  std::uint32_t flags = 0;
  Re4GxChunk view{};

  void Op(std::uint32_t op, const std::uint32_t *p, std::uint32_t n) {
    ops.push_back(op | n << 8);
    ops.insert(ops.end(), p, p + n);
  }
  void Bp(unsigned reg, std::uint32_t value) {
    bp[reg] = value & 0xFFFFFF;
    const std::uint32_t w = reg << 24 | (value & 0xFFFFFF);
    Op(RE4_GX_OP_BP, &w, 1);
  }
  void Xf(std::uint32_t address, std::vector<std::uint32_t> words) {
    words.insert(words.begin(), address);
    Op(RE4_GX_OP_XF, words.data(), static_cast<std::uint32_t>(words.size()));
  }
  void XfF(std::uint32_t address, std::vector<float> values) {
    std::vector<std::uint32_t> w(values.size());
    std::memcpy(w.data(), values.data(), values.size() * 4);
    Xf(address, w);
  }
  std::uint32_t Blob(const void *data, std::size_t n) {
    std::uint32_t at = static_cast<std::uint32_t>(blob.size());
    blob.insert(blob.end(), static_cast<const std::uint8_t *>(data), static_cast<const std::uint8_t *>(data) + n);
    while (blob.size() & 31)
      blob.push_back(0);
    return at;
  }
  // vertices: floats/bytes as the layout says; one triangle list or quads
  void Draw(std::uint32_t layout, std::uint32_t stride, const std::vector<std::uint8_t> &v, unsigned count,
            unsigned gxPrimitive = 0x90, std::uint32_t primitive = RE4_GX_TRIANGLES) {
    Re4GxDraw d{};
    d.primitive = primitive | gxPrimitive << 8;
    d.layout = layout;
    d.stride = stride;
    d.vertexOffset = static_cast<std::uint32_t>(vertices.size());
    d.vertexCount = count;
    d.indexOffset = static_cast<std::uint32_t>(indices.size());
    vertices.insert(vertices.end(), v.begin(), v.end());
    if (gxPrimitive == 0x80) {
      for (unsigned q = 0; q + 3 < count; q += 4) {
        const std::uint16_t t[6] = {static_cast<std::uint16_t>(q), static_cast<std::uint16_t>(q + 1),
                                    static_cast<std::uint16_t>(q + 2), static_cast<std::uint16_t>(q),
                                    static_cast<std::uint16_t>(q + 2), static_cast<std::uint16_t>(q + 3)};
        indices.insert(indices.end(), t, t + 6);
      }
    } else {
      for (unsigned i = 0; i < count; i++)
        indices.push_back(static_cast<std::uint16_t>(i));
    }
    d.indexCount = static_cast<std::uint32_t>(indices.size()) - d.indexOffset;
    d.matrices[0] = 0 | 60u << 8 | 60u << 16 | 60u << 24; // PNMTX0, identity texture matrices
    d.matrices[1] = 60u | 60u << 8 | 60u << 16 | 60u << 24;
    d.matrices[2] = 60u;
    Op(RE4_GX_OP_DRAW, reinterpret_cast<const std::uint32_t *>(&d), sizeof(d) / 4);
  }
  void Texture(unsigned map, std::uint64_t key, std::uint32_t flags, std::uint32_t address, unsigned w, unsigned h,
               unsigned format, const std::vector<std::uint8_t> &data, std::uint32_t tlut = 0) {
    Re4GxTexture t{};
    t.map = map;
    t.keyLo = static_cast<std::uint32_t>(key);
    t.keyHi = static_cast<std::uint32_t>(key >> 32);
    t.flags = flags;
    t.address = address;
    t.width = w;
    t.height = h;
    t.format = format;
    t.levels = 1;
    t.tlut = tlut;
    if (!data.empty()) {
      t.dataOffset = Blob(data.data(), data.size());
      t.dataBytes = static_cast<std::uint32_t>(data.size());
    }
    Op(RE4_GX_OP_TEXTURE, reinterpret_cast<const std::uint32_t *>(&t), sizeof(t) / 4);
  }
  const Re4GxChunk &Chunk() {
    view = Re4GxChunk{};
    view.version = RE4_GX_FRAME_VERSION;
    view.flags = flags;
    view.ops = ops.data();
    view.opWords = static_cast<std::uint32_t>(ops.size());
    view.vertices = vertices.data();
    view.vertexBytes = static_cast<std::uint32_t>(vertices.size());
    view.indices = indices.data();
    view.indexCount = static_cast<std::uint32_t>(indices.size());
    view.blob = blob.data();
    view.blobBytes = static_cast<std::uint32_t>(blob.size());
    return view;
  }
};

std::uint32_t Bits(float f) {
  std::uint32_t v;
  std::memcpy(&v, &f, 4);
  return v;
}

// The state GXInit leaves (the parts these tests need) plus a 2D setup:
// orthographic projection over the EFB, identity position matrix, one colour
// channel from the vertex colour, one TEV stage passing the rasterized colour.
void Baseline(Builder &b) {
  std::vector<std::uint32_t> state(256, 0);
  b.flags = RE4_GX_CHUNK_RESET;
  // genmode: 1 colour channel, 1 TEV stage, no texture coordinates
  state[0x00] = 1u << 4;
  // TEV stage 0: colour = rasterized colour, alpha = rasterized alpha, clamped
  state[0xC0] = 10u | 15u << 4 | 15u << 8 | 15u << 12 | 1u << 19;
  state[0xC1] = 5u << 4 | 7u << 7 | 7u << 10 | 7u << 13 | 1u << 19;
  state[0x28] = 0x3F7u << 12 | 0u; // stage 0: map 0, coord 0, no texture, colour channel 0
  // swap table 0 identity (r g b a), the others like GXInit
  state[0xF6] = 0 | 1 << 2;
  state[0xF7] = 2 | 3 << 2;
  for (int t = 1; t < 4; t++) {
    state[0xF6 + 2 * t] = 0 | 1 << 2;
    state[0xF7 + 2 * t] = 2 | 3 << 2;
  }
  state[0xF3] = 7u << 16 | 7u << 19; // alpha test always
  state[0x40] = 1 | 3 << 1 | 1 << 4; // depth on, LEQUAL, update
  state[0x41] = 1 << 3 | 1 << 4;     // colour and alpha update
  state[0x43] = 1;                   // RGBA6_Z24 (alpha kept)
  state[0x20] = 342 | 342u << 12;    // scissor 0,0 .. 639,527
  state[0x21] = (342 + 527) | (342u + 639) << 12;
  state[0x59] = 171 | 171u << 10;
  for (int i = 0; i < 8; i++) {
    state[0x30 + 2 * i] = 63; // rasterizer size 64 x 64
    state[0x31 + 2 * i] = 63;
  }
  state[0x49] = 0;
  state[0x4A] = 639 | 527u << 10; // copy rectangle: the whole EFB
  for (int i = 0; i < 256; i++)
    b.bp[i] = state[i];
  b.Op(RE4_GX_OP_BP_STATE, state.data(), 256);
  // XF: identity PNMTX0 (rows 0-2) and identity texture matrix 60 (rows 60-62)
  b.XfF(0, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0});
  b.XfF(60 * 4, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0});
  b.XfF(0x400, {1, 0, 0, 0, 1, 0, 0, 0, 1}); // normal matrix 0
  b.XfF(0x500 + 61 * 4, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0});
  b.Xf(0x1009, {1});                // one colour channel
  b.Xf(0x100E, {1});                // colour 0 from the vertex, no lighting
  b.Xf(0x1010, {1});                // alpha 0 likewise
  b.Xf(0x103F, {0});                // no texture coordinates
  // viewport over the EFB, depth 0..1
  b.XfF(0x101A, {320.0f, -264.0f, 16777215.0f, 342.0f + 320.0f, 342.0f + 264.0f, 16777215.0f});
  // orthographic projection: x 0..640, y 0..528 (top down), z 0 .. -1 as depth 0..1
  b.XfF(0x1020, {1.0f / 320.0f, -1.0f, -2.0f / 528.0f, 1.0f, -1.0f, -1.0f});
  b.Xf(0x1026, {1});
}

// a vertex: position, colour
void V(std::vector<std::uint8_t> &v, float x, float y, float z, const std::uint8_t c[4]) {
  const float p[3] = {x, y, z};
  const auto *pb = reinterpret_cast<const std::uint8_t *>(p);
  v.insert(v.end(), pb, pb + 12);
  v.insert(v.end(), c, c + 4);
}

// a vertex: position, colour, s, t
void VT(std::vector<std::uint8_t> &v, float x, float y, float z, const std::uint8_t c[4], float s, float t) {
  V(v, x, y, z, c);
  const float st[2] = {s, t};
  const auto *pb = reinterpret_cast<const std::uint8_t *>(st);
  v.insert(v.end(), pb, pb + 8);
}

void Quad(Builder &b, float x0, float y0, float x1, float y1, float z, const std::uint8_t c[4]) {
  std::vector<std::uint8_t> v;
  V(v, x0, y0, z, c);
  V(v, x1, y0, z, c);
  V(v, x1, y1, z, c);
  V(v, x0, y1, z, c);
  b.Draw(RE4_GX_V_COLOR0, 16, v, 4, 0x80);
}

void DisplayCopy(Builder &b, bool clear, const std::uint8_t color[4]) {
  b.Bp(0x4F, color[0] | color[3] << 8); // clear colour A R
  b.Bp(0x50, color[2] | color[1] << 8); // G B
  b.Bp(0x51, 0xFFFFFF);
  b.Bp(0x4B, 0x80460000u >> 5 & 0x1FFFFF);
  b.Bp(0x52, (clear ? 1u << 11 : 0) | 1u << 14);
}

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

// The menu's picture options (M11): a multisampled EFB is resolved before
// every copy out of it (display copy, texture copy); a slanted edge comes out
// between the two colours; anisotropic filtering does not change a picture
// without mipmaps.
void TestOptions() {
  int maxSamples = 1;
  float maxAnisotropy = 1.0f;
  re4gx::GxRenderer::Limits(&maxSamples, &maxAnisotropy);
  std::fprintf(stderr, "  options: up to %d samples, anisotropy up to %.0f\n", maxSamples, maxAnisotropy);
  re4gx::GxRenderer r;
  re4gx::GxOptions o;
  o.samples = 4;
  o.anisotropy = 8.0f;
  r.SetOptions(o);
  std::string error;
  Require(r.Init(1, &error), error.c_str());
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
  Check(glGetError() == GL_NO_ERROR, "options: no GL error");
  r.Release();
}

int main() {
  Context();
  glGenFramebuffers(1, &gReadFbo);
  TestDecoder();
  re4gx::GxRenderer renderer;
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
  Check(glGetError() == GL_NO_ERROR, "no GL error");
  renderer.Release();
  TestOptions();
  std::fprintf(stderr, "gx_gl_check: %d failed\n", gFailures);
  return gFailures ? 1 : 0;
}
