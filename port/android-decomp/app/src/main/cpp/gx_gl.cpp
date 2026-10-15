#include "gx_gl.h"

#include "gx_gl_shader.h"
#include "gx_texture.h"
#include "re4_gx_frame.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#define GXLOG(...) __android_log_print(ANDROID_LOG_INFO, "RE4GX", __VA_ARGS__)
#else
#define GXLOG(...) (std::fprintf(stderr, "[RE4GX] " __VA_ARGS__), std::fputc('\n', stderr))
#endif

namespace re4gx {
namespace {

constexpr int kEfbWidth = 640;
constexpr int kEfbHeight = 528;
constexpr unsigned kTlutBytes = 0x80000;

float AsFloat(std::uint32_t v) {
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}

std::uint64_t Fnv(const void *data, std::size_t n, std::uint64_t h = 1469598103934665603ull) {
  const auto *p = static_cast<const std::uint8_t *>(data);
  for (std::size_t i = 0; i < n; i++) {
    h ^= p[i];
    h *= 1099511628211ull;
  }
  return h;
}

int Signed11(std::uint32_t v) { return (v & 0x400) ? static_cast<int>(v & 0x7FF) - 2048 : static_cast<int>(v & 0x7FF); }

GLuint Compile(GLenum type, const std::string &source, std::string *log) {
  GLuint s = glCreateShader(type);
  const char *text = source.c_str();
  glShaderSource(s, 1, &text, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char buffer[4096];
    glGetShaderInfoLog(s, sizeof(buffer), nullptr, buffer);
    if (log)
      *log = buffer;
    glDeleteShader(s);
    return 0;
  }
  return s;
}

GLuint Link(const std::string &vs, const std::string &fs, std::string *log) {
  GLuint v = Compile(GL_VERTEX_SHADER, vs, log);
  if (!v)
    return 0;
  GLuint f = Compile(GL_FRAGMENT_SHADER, fs, log);
  if (!f) {
    glDeleteShader(v);
    return 0;
  }
  GLuint p = glCreateProgram();
  glAttachShader(p, v);
  glAttachShader(p, f);
  glLinkProgram(p);
  glDeleteShader(v);
  glDeleteShader(f);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char buffer[4096];
    glGetProgramInfoLog(p, sizeof(buffer), nullptr, buffer);
    if (log)
      *log = buffer;
    glDeleteProgram(p);
    return 0;
  }
  return p;
}

struct Program {
  GLuint id = 0;
  GLint mtx = -1;
};

struct KeyHash {
  std::size_t operator()(const ShaderKey &k) const { return static_cast<std::size_t>(Fnv(&k, sizeof(k))); }
};
struct KeyEqual {
  bool operator()(const ShaderKey &a, const ShaderKey &b) const { return std::memcmp(&a, &b, sizeof(a)) == 0; }
};

// A texture as the renderer keeps it: decoded RGBA8 with its levels.
struct Texture {
  GLuint id = 0;
  unsigned width = 0, height = 0, levels = 1;
};

// An EFB copy to a texture: the texels as the matching texture format reads
// them (8-bit copies as I8: value in all four channels; two-channel copies as
// IA8; colour copies as their colour), and for paletted reads the raw value.
struct Copy {
  GLuint id = 0;
  unsigned width = 0, height = 0;
  unsigned serial = 0;
  unsigned kind = 0; // 0 colour, 1 one channel (value in .r), 2 two channels (.a high, .r low)
};

} // namespace

struct GxRenderer::Impl {
  int scale = 1;
  GxStats stats;

  // register state
  std::uint32_t bp[256] = {};
  std::uint32_t xfMem[0x800] = {};
  std::uint32_t xfReg[0x58] = {};
  std::vector<std::uint8_t> tlut = std::vector<std::uint8_t>(kTlutBytes);
  std::int32_t tevColor[4][4] = {};
  std::int32_t tevKonst[4][4] = {};
  // XF block rows (vec4) that changed since the last upload
  int dirtyLo = 0, dirtyHi = static_cast<int>(sizeof(XfBlock) / 16);

  // GL objects
  GLuint efbFbo = 0, efbColor = 0, efbDepth = 0;
  GLuint xfbFbo = 0, xfbTex = 0;
  unsigned xfbWidth = 0, xfbHeight = 0;
  bool haveXfb = false;
  GLuint vao = 0, vbo = 0, ibo = 0;
  GLuint xfUbo = 0, pxUbo = 0;
  GLuint copyProgram = 0, presentProgram = 0, paletteProgram = 0;
  GLint copyMode = -1, copyRect = -1, copySize = -1, copyHalf = -1;
  GLint paletteSize = -1, paletteKind = -1, paletteFormat = -1;
  GLuint quadVao = 0;
  GLuint whiteTex = 0;
  std::unordered_map<ShaderKey, Program, KeyHash, KeyEqual> programs;
  std::unordered_map<std::uint64_t, Texture> textures;
  std::unordered_map<std::uint32_t, Copy> copies;  // by destination address
  std::unordered_map<std::uint64_t, Texture> paletted; // copies read through a palette
  std::unordered_map<std::uint64_t, GLuint> samplers;

  // the texture bound to each map
  struct Bound {
    GLuint id = 0;
    unsigned width = 1, height = 1, levels = 1;
  } bound[8];

  // per-draw caches
  const Program *current = nullptr;
  XfBlock xf = {};
  PixelBlock px = {};
  PixelBlock pxSent = {};
  bool pxValid = false;
  std::uint32_t copySerial = 0;

  // --- state -----------------------------------------------------------------

  void BpWrite(std::uint32_t word, bool action) {
    const unsigned reg = word >> 24;
    const std::uint32_t value = word & 0xFFFFFF;
    bp[reg] = value;
    if (reg >= 0xE0 && reg <= 0xE7) {
      const unsigned i = (reg - 0xE0) / 2;
      std::int32_t *dst = (value >> 23) & 1 ? tevKonst[i] : tevColor[i];
      if ((reg & 1) == 0) { // RA
        dst[0] = Signed11(value);
        dst[3] = Signed11(value >> 12);
      } else { // BG
        dst[2] = Signed11(value);
        dst[1] = Signed11(value >> 12);
      }
    }
    if (action && reg == 0x52)
      EfbCopy(value);
  }

  void Dirty(int lo, int hi) {
    dirtyLo = std::min(dirtyLo, lo);
    dirtyHi = std::max(dirtyHi, hi);
  }

  // The XF block rows an XF address touches.
  void DirtyAddress(std::uint32_t a) {
    constexpr int kNormal = 64, kPost = 96, kLight = 160, kRegs = 200; // row offsets in XfBlock
    if (a < 0x100)
      Dirty(static_cast<int>(a / 4), static_cast<int>(a / 4) + 1);
    else if (a >= 0x400 && a < 0x460)
      Dirty(kNormal + static_cast<int>((a - 0x400) / 3), kNormal + static_cast<int>((a - 0x400) / 3) + 1);
    else if (a >= 0x500 && a < 0x600)
      Dirty(kPost + static_cast<int>((a - 0x500) / 4), kPost + static_cast<int>((a - 0x500) / 4) + 1);
    else if (a >= 0x600 && a < 0x680)
      Dirty(kLight + static_cast<int>((a - 0x600) / 16) * 5, kLight + static_cast<int>((a - 0x600) / 16) * 5 + 5);
    else if (a >= 0x1000)
      Dirty(kRegs, static_cast<int>(sizeof(XfBlock) / 16));
  }

  void XfWrite(std::uint32_t address, const std::uint32_t *words, std::uint32_t n) {
    for (std::uint32_t i = 0; i < n; i++) {
      const std::uint32_t a = address + i;
      if (a < 0x800)
        xfMem[a] = words[i];
      else if (a >= 0x1000 && a < 0x1058)
        xfReg[a - 0x1000] = words[i];
      else
        continue;
      DirtyAddress(a);
    }
  }

  void FillXf() {
    for (int r = 0; r < 64; r++)
      for (int c = 0; c < 4; c++)
        xf.posTex[r][c] = AsFloat(xfMem[r * 4 + c]);
    for (int r = 0; r < 32; r++) {
      for (int c = 0; c < 3; c++)
        xf.normal[r][c] = AsFloat(xfMem[0x400 + r * 3 + c]);
      xf.normal[r][3] = 0.0f;
    }
    for (int r = 0; r < 64; r++)
      for (int c = 0; c < 4; c++)
        xf.post[r][c] = AsFloat(xfMem[0x500 + r * 4 + c]);
    for (int l = 0; l < 8; l++) {
      const std::uint32_t *w = xfMem + 0x600 + l * 16;
      const std::uint32_t color = w[3];
      for (int c = 0; c < 4; c++)
        xf.light[l][0][c] = static_cast<float>((color >> (24 - 8 * c)) & 255) / 255.0f;
      for (int part = 0; part < 4; part++) {
        for (int c = 0; c < 3; c++)
          xf.light[l][1 + part][c] = AsFloat(w[4 + part * 3 + c]);
        xf.light[l][1 + part][3] = 0.0f;
      }
    }
    const std::uint32_t colorRegs[4] = {xfReg[0x0C], xfReg[0x0D], xfReg[0x0A], xfReg[0x0B]};
    for (int i = 0; i < 4; i++)
      for (int c = 0; c < 4; c++)
        xf.matAmb[i][c] = static_cast<float>((colorRegs[i] >> (24 - 8 * c)) & 255) / 255.0f;
    xf.viewport[0][0] = AsFloat(xfReg[0x1A]);
    xf.viewport[0][1] = AsFloat(xfReg[0x1B]);
    xf.viewport[0][2] = AsFloat(xfReg[0x1C]);
    xf.viewport[0][3] = AsFloat(xfReg[0x1D]);
    xf.viewport[1][0] = AsFloat(xfReg[0x1E]);
    xf.viewport[1][1] = AsFloat(xfReg[0x1F]);
    xf.viewport[1][2] = kEfbWidth;
    xf.viewport[1][3] = kEfbHeight;
    for (int i = 0; i < 6; i++)
      xf.proj[i / 4][i % 4] = AsFloat(xfReg[0x20 + i]);
    xf.proj[1][2] = static_cast<float>(xfReg[0x26]);
  }

  // --- textures ----------------------------------------------------------------

  GLuint Sampler(std::uint32_t mode0, std::uint32_t mode1, unsigned levels) {
    const std::uint64_t key = (static_cast<std::uint64_t>(mode1 & 0xFFFF) << 32) | (mode0 & 0x1FF) |
                              (static_cast<std::uint64_t>(levels > 1) << 48);
    auto it = samplers.find(key);
    if (it != samplers.end())
      return it->second;
    GLuint s = 0;
    glGenSamplers(1, &s);
    static const GLenum wrap[4] = {GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT, GL_REPEAT};
    glSamplerParameteri(s, GL_TEXTURE_WRAP_S, wrap[mode0 & 3]);
    glSamplerParameteri(s, GL_TEXTURE_WRAP_T, wrap[(mode0 >> 2) & 3]);
    glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, (mode0 >> 4) & 1 ? GL_LINEAR : GL_NEAREST);
    const unsigned minFilter = (mode0 >> 5) & 7;
    static const GLenum minTable[8] = {GL_NEAREST,
                                       GL_NEAREST_MIPMAP_NEAREST,
                                       GL_NEAREST_MIPMAP_LINEAR,
                                       GL_NEAREST,
                                       GL_LINEAR,
                                       GL_LINEAR_MIPMAP_NEAREST,
                                       GL_LINEAR_MIPMAP_LINEAR,
                                       GL_LINEAR};
    GLenum min = minTable[minFilter];
    if (levels <= 1)
      min = (minFilter >= 4) ? GL_LINEAR : GL_NEAREST;
    glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, min);
    glSamplerParameterf(s, GL_TEXTURE_MIN_LOD, static_cast<float>(mode1 & 0xFF) / 16.0f);
    glSamplerParameterf(s, GL_TEXTURE_MAX_LOD, static_cast<float>((mode1 >> 8) & 0xFF) / 16.0f);
    samplers[key] = s;
    return s;
  }

  void Upload(const Re4GxTexture &t, const Re4GxChunk &chunk) {
    const std::uint64_t key = static_cast<std::uint64_t>(t.keyHi) << 32 | t.keyLo;
    if (t.dataBytes == 0 || t.dataOffset + t.dataBytes > chunk.blobBytes)
      return;
    Texture tex;
    tex.width = t.width;
    tex.height = t.height;
    tex.levels = t.levels ? t.levels : 1;
    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const std::uint8_t *data = chunk.blob + t.dataOffset;
    std::size_t left = t.dataBytes;
    const std::uint8_t *palette = nullptr;
    unsigned entries = 0;
    if (GxIsPaletted(t.format)) {
      const unsigned offset = (t.tlut & 0x3FF) << 9;
      palette = tlut.data() + offset;
      entries = std::min<unsigned>(GxPaletteEntries(t.format), (kTlutBytes - offset) / 2);
    }
    unsigned w = t.width, h = t.height, levels = 0;
    std::vector<std::uint8_t> rgba;
    for (unsigned l = 0; l < tex.levels; l++) {
      const std::size_t bytes = GxTextureLevelBytes(t.format, w, h);
      if (bytes > left)
        break;
      GxDecodeTexture(data, bytes, t.format, w, h, palette, entries, (t.tlut >> 10) & 3, rgba);
      glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(l), GL_RGBA8, static_cast<GLsizei>(w), static_cast<GLsizei>(h),
                   0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
      levels++;
      data += bytes;
      left -= bytes;
      w = std::max(1u, w / 2);
      h = std::max(1u, h / 2);
    }
    tex.levels = std::max(1u, levels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(tex.levels - 1));
    auto old = textures.find(key);
    if (old != textures.end())
      glDeleteTextures(1, &old->second.id);
    textures[key] = tex;
    stats.textures++;
  }

  // A texture copy read through a palette (C4/C8/C14X2 over a copy): the copy's
  // values looked up in TMEM's palette, on the GPU.
  GLuint PalettedCopy(const Copy &copy, const Re4GxTexture &t) {
    const unsigned offset = (t.tlut & 0x3FF) << 9;
    const unsigned entries = std::min<unsigned>(GxPaletteEntries(t.format), (kTlutBytes - offset) / 2);
    const std::uint64_t key =
        Fnv(tlut.data() + offset, entries * 2, (static_cast<std::uint64_t>(copy.serial) << 32) ^ t.address ^ t.tlut);
    auto it = paletted.find(key);
    if (it != paletted.end())
      return it->second.id;
    // the palette as a 256 x (entries / 256) RGBA texture
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(entries) * 4);
    std::vector<std::uint8_t> idx(entries);
    for (unsigned i = 0; i < entries; i++)
      idx[i] = static_cast<std::uint8_t>(i);
    // decode the palette entries through the C8 path (one entry per texel)
    for (unsigned i = 0; i < entries; i++) {
      std::vector<std::uint8_t> one;
      std::uint8_t tile[32] = {};
      tile[0] = 0;
      GxDecodeTexture(tile, 32, kTexC8, 1, 1, tlut.data() + offset + i * 2, 1, (t.tlut >> 10) & 3, one);
      std::memcpy(rgba.data() + i * 4, one.data(), 4);
    }
    const unsigned pw = std::min(entries, 256u), ph = std::max(1u, entries / 256);
    GLuint pal = 0;
    glGenTextures(1, &pal);
    glBindTexture(GL_TEXTURE_2D, pal);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(pw), static_cast<GLsizei>(ph), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    Texture out;
    out.width = copy.width;
    out.height = copy.height;
    glGenTextures(1, &out.id);
    glBindTexture(GL_TEXTURE_2D, out.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(copy.width), static_cast<GLsizei>(copy.height), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, out.id, 0);
    SaveAndPlainState();
    glViewport(0, 0, static_cast<GLsizei>(copy.width), static_cast<GLsizei>(copy.height));
    glUseProgram(paletteProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, copy.id);
    glBindSampler(0, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pal);
    glBindSampler(1, 0);
    glUniform2f(paletteSize, static_cast<float>(pw), static_cast<float>(ph));
    glUniform1i(paletteKind, static_cast<GLint>(copy.kind));
    glUniform1i(paletteFormat, static_cast<GLint>(t.format));
    glBindVertexArray(quadVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &pal);
    glBindFramebuffer(GL_FRAMEBUFFER, efbFbo);
    current = nullptr;
    paletted[key] = out;
    return out.id;
  }

  void BindTexture(const Re4GxTexture &t, const Re4GxChunk &chunk) {
    const unsigned map = t.map & 7;
    Bound &b = bound[map];
    b.id = whiteTex;
    b.width = t.width ? t.width : 1;
    b.height = t.height ? t.height : 1;
    b.levels = 1;
    if (t.flags & RE4_GX_TEX_COPY) {
      auto it = copies.find(t.address);
      if (it == copies.end()) {
        stats.missingTextures++;
        return;
      }
      b.id = GxIsPaletted(t.format) ? PalettedCopy(it->second, t) : it->second.id;
      return;
    }
    const std::uint64_t key = static_cast<std::uint64_t>(t.keyHi) << 32 | t.keyLo;
    if (key == 0)
      return;
    if (t.dataBytes)
      Upload(t, chunk);
    auto it = textures.find(key);
    if (it == textures.end()) {
      stats.missingTextures++;
      return;
    }
    b.id = it->second.id;
    b.levels = it->second.levels;
  }

  void Forget(std::uint64_t key) {
    auto it = textures.find(key);
    if (it != textures.end()) {
      glDeleteTextures(1, &it->second.id);
      textures.erase(it);
    }
  }

  void DropTextures() {
    for (auto &t : textures)
      glDeleteTextures(1, &t.second.id);
    textures.clear();
    for (auto &t : paletted)
      glDeleteTextures(1, &t.second.id);
    paletted.clear();
  }

  // --- GL state ---------------------------------------------------------------

  // GL state for the renderer's own passes (copies, palettes, presenting)
  void SaveAndPlainState() {
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
  }

  void ApplyState(unsigned primitive) {
    glBindFramebuffer(GL_FRAMEBUFFER, efbFbo);
    glViewport(0, 0, kEfbWidth * scale, kEfbHeight * scale);
    // scissor: registers in EFB pixels + 342, minus the scissor offset (BP 0x59, in 2 pixels)
    const int xoff = static_cast<int>(bp[0x59] & 0x3FF) * 2, yoff = static_cast<int>((bp[0x59] >> 10) & 0x3FF) * 2;
    const int x0 = std::max(0, static_cast<int>((bp[0x20] >> 12) & 0x7FF) - xoff);
    const int y0 = std::max(0, static_cast<int>(bp[0x20] & 0x7FF) - yoff);
    const int x1 = std::min(kEfbWidth, static_cast<int>((bp[0x21] >> 12) & 0x7FF) - xoff + 1);
    const int y1 = std::min(kEfbHeight, static_cast<int>(bp[0x21] & 0x7FF) - yoff + 1);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x0 * scale, y0 * scale, std::max(0, x1 - x0) * scale, std::max(0, y1 - y0) * scale);
    // depth
    const std::uint32_t z = bp[0x40];
    if (z & 1) {
      static const GLenum func[8] = {GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS};
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(func[(z >> 1) & 7]);
      glDepthMask((z >> 4) & 1 ? GL_TRUE : GL_FALSE);
    } else {
      glDisable(GL_DEPTH_TEST);
      glDepthMask(GL_FALSE);
    }
    // blending, colour and alpha update
    const std::uint32_t c = bp[0x41];
    const bool alphaFormat = (bp[0x43] & 7) == 1; // RGBA6_Z24 keeps alpha
    glColorMask((c >> 3) & 1, (c >> 3) & 1, (c >> 3) & 1, ((c >> 4) & 1) && alphaFormat);
    if ((c >> 11) & 1) { // subtract: dst - src
      glEnable(GL_BLEND);
      glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
      glBlendFunc(GL_ONE, GL_ONE);
    } else if (c & 1) {
      static const GLenum src[8] = {GL_ZERO,      GL_ONE,           GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
                                    GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA};
      static const GLenum dst[8] = {GL_ZERO,      GL_ONE,           GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
                                    GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA};
      glEnable(GL_BLEND);
      glBlendEquation(GL_FUNC_ADD);
      glBlendFunc(src[(c >> 8) & 7], dst[(c >> 5) & 7]);
    } else if ((c >> 1) & 1) { // logic operations: the common ones
      const unsigned op = (c >> 12) & 15;
      glEnable(GL_BLEND);
      glBlendEquation(GL_FUNC_ADD);
      if (op == 0) // clear
        glBlendFunc(GL_ZERO, GL_ZERO);
      else if (op == 5) // noop
        glBlendFunc(GL_ZERO, GL_ONE);
      else if (op == 15) // set: approximated
        glBlendFunc(GL_ONE, GL_ONE);
      else // copy and the rest
        glBlendFunc(GL_ONE, GL_ZERO);
    } else {
      glDisable(GL_BLEND);
    }
    // culling: only triangles (hardware 1 = back, 2 = front, 3 = all)
    const unsigned cull = (bp[0x00] >> 14) & 3;
    if (primitive != RE4_GX_TRIANGLES || cull == 0) {
      glDisable(GL_CULL_FACE);
    } else {
      glEnable(GL_CULL_FACE);
      glCullFace(cull == 1 ? GL_BACK : cull == 2 ? GL_FRONT : GL_FRONT_AND_BACK);
    }
    // lines and points: width in 1/6 pixel
    const float lineWidth = static_cast<float>(bp[0x22] & 0xFF) / 6.0f * static_cast<float>(scale);
    glLineWidth(std::max(1.0f, lineWidth));
  }

  // --- draws --------------------------------------------------------------------

  ShaderKey MakeKey(const Re4GxDraw &d) {
    ShaderKey k;
    std::memset(&k, 0, sizeof(k));
    k.layout = d.layout;
    k.numChans = xfReg[0x09] & 3;
    k.numTexGens = std::min(8u, xfReg[0x3F] & 15);
    for (int i = 0; i < 4; i++)
      k.chan[i] = i % 2 < static_cast<int>(k.numChans) ? xfReg[0x0E + i] & 0x7FFF : 0;
    k.dualTex = xfReg[0x12] & 1;
    for (unsigned i = 0; i < k.numTexGens; i++) {
      k.texGen[i] = xfReg[0x40 + i] & 0x3FFFF;
      k.postGen[i] = k.dualTex ? xfReg[0x50 + i] & 0x13F : 0;
    }
    k.orthographic = xfReg[0x26] & 1;
    const std::uint32_t gen = bp[0x00];
    k.numStages = ((gen >> 10) & 15) + 1;
    k.numIndStages = (gen >> 16) & 7;
    for (unsigned s = 0; s < k.numStages; s++) {
      k.colorEnv[s] = bp[0xC0 + 2 * s] & 0xFFFFFF;
      k.alphaEnv[s] = bp[0xC1 + 2 * s] & 0xFFFFFF;
      const std::uint32_t o = bp[0x28 + s / 2] >> ((s & 1) ? 12 : 0);
      k.order[s] = (o & 7) | ((o >> 3) & 7) << 3 | ((o >> 6) & 1) << 6 | ((o >> 7) & 7) << 7;
      const std::uint32_t ks = bp[0xF6 + s / 2];
      k.konst[s] = ((s & 1) ? (ks >> 14) & 31 : (ks >> 4) & 31) | ((s & 1) ? (ks >> 19) & 31 : (ks >> 9) & 31) << 8;
      k.indCmd[s] = k.numIndStages ? bp[0x10 + s] & 0x1FFFFF : 0;
      if ((o >> 6) & 1)
        k.texMaps |= 1u << (o & 7);
    }
    for (int t = 0; t < 4; t++)
      k.swap[t] = (bp[0xF6 + 2 * t] & 3) | ((bp[0xF6 + 2 * t] >> 2) & 3) << 2 | (bp[0xF7 + 2 * t] & 3) << 4 |
                  ((bp[0xF7 + 2 * t] >> 2) & 3) << 6;
    if (k.numIndStages) {
      k.indRef = bp[0x27] & 0xFFFFFF;
      for (unsigned s = 0; s < k.numIndStages && s < 4; s++)
        k.texMaps |= 1u << ((k.indRef >> (6 * s)) & 7);
    }
    k.alphaTest = (bp[0xF3] >> 16) & 0xFF;
    k.fog = (bp[0xF1] >> 20) & 15;
    return k;
  }

  const Program *ProgramFor(const ShaderKey &k) {
    auto it = programs.find(k);
    if (it != programs.end())
      return it->second.id ? &it->second : nullptr;
    Program p;
    std::string log;
    const std::string vs = GxVertexShader(k), fs = GxFragmentShader(k);
    p.id = Link(vs, fs, &log);
    if (!p.id) {
      stats.shaderErrors++;
      if (stats.shaderErrors <= 4)
        GXLOG("shader failed: %s\n--- vertex ---\n%s\n--- fragment ---\n%s", log.c_str(), vs.c_str(), fs.c_str());
    } else {
      stats.programs++;
      GLuint xfIndex = glGetUniformBlockIndex(p.id, "XF");
      if (xfIndex != GL_INVALID_INDEX)
        glUniformBlockBinding(p.id, xfIndex, kXfBinding);
      GLuint pxIndex = glGetUniformBlockIndex(p.id, "PX");
      if (pxIndex != GL_INVALID_INDEX)
        glUniformBlockBinding(p.id, pxIndex, kPixelBinding);
      glUseProgram(p.id);
      for (int m = 0; m < 8; m++) {
        char name[8];
        std::snprintf(name, sizeof(name), "s%d", m);
        GLint loc = glGetUniformLocation(p.id, name);
        if (loc >= 0)
          glUniform1i(loc, m);
      }
      p.mtx = glGetUniformLocation(p.id, "uMtx");
      current = nullptr;
    }
    auto &slot = programs[k];
    slot = p;
    return slot.id ? &slot : nullptr;
  }

  void FillPixel(const ShaderKey &k) {
    std::memcpy(px.colors, tevColor, sizeof(tevColor));
    std::memcpy(px.konst, tevKonst, sizeof(tevKonst));
    px.alphaRef[0] = static_cast<std::int32_t>(bp[0xF3] & 0xFF);
    px.alphaRef[1] = static_cast<std::int32_t>((bp[0xF3] >> 8) & 0xFF);
    const auto fogFloat = [](std::uint32_t v) {
      const std::uint32_t bits = ((v >> 19) & 1) << 31 | ((v >> 11) & 0xFF) << 23 | (v & 0x7FF) << 12;
      return AsFloat(bits);
    };
    px.fog[0][0] = fogFloat(bp[0xEE]);
    px.fog[0][1] = fogFloat(bp[0xF1]);
    px.fog[0][2] = static_cast<float>(bp[0xEF] & 0xFFFFFF);
    px.fog[0][3] = static_cast<float>(bp[0xF0] & 31);
    px.fog[1][0] = static_cast<float>((bp[0xF2] >> 16) & 255);
    px.fog[1][1] = static_cast<float>((bp[0xF2] >> 8) & 255);
    px.fog[1][2] = static_cast<float>(bp[0xF2] & 255);
    px.fog[1][3] = 0.0f;
    for (int i = 0; i < 8; i++) {
      px.coordSize[i][0] = static_cast<float>((bp[0x30 + 2 * i] & 0xFFFF) + 1);
      px.coordSize[i][1] = static_cast<float>((bp[0x31 + 2 * i] & 0xFFFF) + 1);
      px.coordSize[i][2] = px.coordSize[i][3] = 0.0f;
      const unsigned mode0 = bp[(i < 4 ? 0x80 : 0xA0) + (i & 3)];
      int bias = static_cast<int>((mode0 >> 9) & 0xFF);
      if (bias & 0x80)
        bias -= 256;
      px.mapSize[i][0] = static_cast<float>(bound[i].width);
      px.mapSize[i][1] = static_cast<float>(bound[i].height);
      px.mapSize[i][2] = static_cast<float>(bias) / 32.0f;
      px.mapSize[i][3] = 0.0f;
    }
    for (int s = 0; s < 4; s++) {
      const std::uint32_t r = bp[0x25 + s / 2] >> ((s & 1) ? 8 : 0);
      px.indScale[s][0] = static_cast<std::int32_t>(r & 15);
      px.indScale[s][1] = static_cast<std::int32_t>((r >> 4) & 15);
      px.indScale[s][2] = px.indScale[s][3] = 0;
    }
    for (int m = 0; m < 3; m++) {
      const std::uint32_t a = bp[0x06 + 3 * m], b = bp[0x07 + 3 * m], c = bp[0x08 + 3 * m];
      const int scale = static_cast<int>(((a >> 22) & 3) | ((b >> 22) & 3) << 2 | ((c >> 22) & 3) << 4);
      px.indMtx[2 * m][0] = Signed11(a);
      px.indMtx[2 * m][1] = Signed11(b);
      px.indMtx[2 * m][2] = Signed11(c);
      px.indMtx[2 * m][3] = 17 - scale;
      px.indMtx[2 * m + 1][0] = Signed11(a >> 11);
      px.indMtx[2 * m + 1][1] = Signed11(b >> 11);
      px.indMtx[2 * m + 1][2] = Signed11(c >> 11);
      px.indMtx[2 * m + 1][3] = 17 - scale;
    }
    (void)k;
  }

  void Draw(const Re4GxDraw &d, const Re4GxChunk &chunk) {
    if (d.indexCount == 0 || d.vertexOffset + static_cast<std::uint64_t>(d.vertexCount) * d.stride > chunk.vertexBytes ||
        d.indexOffset + d.indexCount > chunk.indexCount)
      return;
    const ShaderKey k = MakeKey(d);
    const Program *p = ProgramFor(k);
    if (!p)
      return;
    const unsigned primitive = d.primitive & 0xFF;
    ApplyState(primitive);
    const float pointSize = static_cast<float>((bp[0x22] >> 8) & 0xFF) / 6.0f * static_cast<float>(scale);
    if (xf.proj[1][3] != pointSize)
      Dirty(207, 208);
    if (dirtyLo < dirtyHi) {
      FillXf();
      xf.proj[1][3] = pointSize;
      glBindBuffer(GL_UNIFORM_BUFFER, xfUbo);
      glBufferSubData(GL_UNIFORM_BUFFER, dirtyLo * 16, (dirtyHi - dirtyLo) * 16,
                      reinterpret_cast<const std::uint8_t *>(&xf) + dirtyLo * 16);
      dirtyLo = static_cast<int>(sizeof(XfBlock) / 16);
      dirtyHi = 0;
    }
    FillPixel(k);
    if (!pxValid || std::memcmp(&px, &pxSent, sizeof(px)) != 0) {
      glBindBuffer(GL_UNIFORM_BUFFER, pxUbo);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PixelBlock), &px);
      pxSent = px;
      pxValid = true;
    }
    if (current != p) {
      glUseProgram(p->id);
      current = p;
    }
    if (p->mtx >= 0)
      glUniform3ui(p->mtx, d.matrices[0], d.matrices[1], d.matrices[2]);
    for (int m = 0; m < 8; m++) {
      if (!(k.texMaps & (1u << m)))
        continue;
      glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(m));
      glBindTexture(GL_TEXTURE_2D, bound[m].id ? bound[m].id : whiteTex);
      const std::uint32_t mode0 = bp[(m < 4 ? 0x80 : 0xA0) + (m & 3)], mode1 = bp[(m < 4 ? 0x84 : 0xA4) + (m & 3)];
      glBindSampler(static_cast<GLuint>(m), Sampler(mode0, mode1, bound[m].levels));
    }
    // vertex attributes at the draw's vertices
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (GLuint a = 0; a < 15; a++)
      glDisableVertexAttribArray(a);
    const auto at = [&](unsigned offset) {
      return reinterpret_cast<const void *>(static_cast<std::uintptr_t>(d.vertexOffset + offset));
    };
    const GLsizei stride = static_cast<GLsizei>(d.stride);
    unsigned off = 0;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, at(off));
    off += 12;
    if (d.layout & RE4_GX_V_NORMAL) {
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, at(off));
      off += 12;
      if (d.layout & RE4_GX_V_BINORMAL_TANGENT) {
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, at(off));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, at(off + 12));
        off += 24;
      }
    }
    for (int c = 0; c < 2; c++) {
      if (d.layout & (c == 0 ? RE4_GX_V_COLOR0 : RE4_GX_V_COLOR1)) {
        glEnableVertexAttribArray(4 + c);
        glVertexAttribPointer(4 + c, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, at(off));
        off += 4;
      }
    }
    for (int t = 0; t < 8; t++) {
      if (d.layout & (RE4_GX_V_TEX0 << t)) {
        glEnableVertexAttribArray(6 + t);
        glVertexAttribPointer(6 + t, 2, GL_FLOAT, GL_FALSE, stride, at(off));
        off += 8;
      }
    }
    if (d.layout & RE4_GX_V_MATRIX) {
      glEnableVertexAttribArray(14);
      glVertexAttribIPointer(14, 3, GL_UNSIGNED_INT, stride, at(off));
    }
    const GLenum mode = primitive == RE4_GX_LINES ? GL_LINES : primitive == RE4_GX_POINTS ? GL_POINTS : GL_TRIANGLES;
    glDrawElements(mode, static_cast<GLsizei>(d.indexCount), GL_UNSIGNED_SHORT,
                   reinterpret_cast<const void *>(static_cast<std::uintptr_t>(d.indexOffset) * 2));
    stats.draws++;
    if (mode == GL_TRIANGLES)
      stats.triangles += d.indexCount / 3;
  }

  // --- EFB copies -------------------------------------------------------------------

  // Copy format and conversion of BP 0x52 (and the EFB pixel format): mode for
  // the copy shader (0 RGBA8, 1 RGB565, 2 RGB5A3, 3 intensity I, 4 intensity IA,
  // 5 R, 6 G, 7 B, 8 A, 9 RG, 10 GB, 11 RA, 12 depth high byte, 13 depth middle,
  // 14 depth low, 15 depth 16 bit, 16 depth 24 bit) and the kind of the result.
  static void CopyMode(std::uint32_t control, bool depth, int *mode, unsigned *kind) {
    const unsigned format = ((control >> 4) & 7) | ((control >> 3) & 1) << 3;
    const bool intensity = (control >> 15) & 1;
    *kind = 0;
    if (depth) {
      switch (format) {
      case 0x9: *mode = 13; *kind = 1; return;  // Z8M
      case 0xA: *mode = 14; *kind = 1; return;  // Z8L
      case 0xB:                                 // Z16
      case 0x3:
      case 0xC: *mode = 15; *kind = 2; return;  // Z16L (approx.)
      case 0x6: *mode = 16; *kind = 0; return;  // Z24X8
      default: *mode = 12; *kind = 1; return;   // Z4, Z8
      }
    }
    if (intensity) {
      if (format == 2 || format == 3) {
        *mode = 4;
        *kind = 2;
      } else {
        *mode = 3;
        *kind = 1;
      }
      return;
    }
    switch (format) {
    case 0x0:
    case 0x1:
    case 0x8: *mode = 5; *kind = 1; return; // R4, R8
    case 0x2:
    case 0x3: *mode = 11; *kind = 2; return; // RA4, RA8
    case 0x4: *mode = 1; return;
    case 0x5: *mode = 2; return;
    case 0x7: *mode = 8; *kind = 1; return; // A8
    case 0x9: *mode = 6; *kind = 1; return; // G8
    case 0xA: *mode = 7; *kind = 1; return; // B8
    case 0xB: *mode = 9; *kind = 2; return; // RG8
    case 0xC: *mode = 10; *kind = 2; return; // GB8
    default: *mode = 0; return;             // RGBA8 (and YUVA8)
    }
  }

  void EfbCopy(std::uint32_t control) {
    const int x = static_cast<int>(bp[0x49] & 0x3FF), y = static_cast<int>((bp[0x49] >> 10) & 0x3FF);
    const int w = static_cast<int>(bp[0x4A] & 0x3FF) + 1, h = static_cast<int>((bp[0x4A] >> 10) & 0x3FF) + 1;
    const std::uint32_t dest = (bp[0x4B] & 0x1FFFFF) << 5;
    const bool toXfb = (control >> 14) & 1;
    const bool half = !toXfb && ((control >> 9) & 1);
    const bool depth = !toXfb && (bp[0x43] & 7) == 3;
    int mode = 0;
    unsigned kind = 0;
    if (!toXfb)
      CopyMode(control, depth, &mode, &kind);
    const int ow = half ? std::max(1, w / 2) : w, oh = half ? std::max(1, h / 2) : h;
    GLuint target = 0;
    if (toXfb) {
      if (!xfbTex || xfbWidth != static_cast<unsigned>(w) || xfbHeight != static_cast<unsigned>(h)) {
        if (xfbTex)
          glDeleteTextures(1, &xfbTex);
        glGenTextures(1, &xfbTex);
        glBindTexture(GL_TEXTURE_2D, xfbTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w * scale, h * scale, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        xfbWidth = static_cast<unsigned>(w);
        xfbHeight = static_cast<unsigned>(h);
      }
      target = xfbTex;
      haveXfb = true;
      stats.displayCopies++;
    } else {
      Copy &c = copies[dest];
      if (!c.id || c.width != static_cast<unsigned>(ow) || c.height != static_cast<unsigned>(oh)) {
        if (c.id)
          glDeleteTextures(1, &c.id);
        glGenTextures(1, &c.id);
        glBindTexture(GL_TEXTURE_2D, c.id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ow * scale, oh * scale, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        c.width = static_cast<unsigned>(ow);
        c.height = static_cast<unsigned>(oh);
      }
      c.serial = ++copySerial;
      c.kind = kind;
      target = c.id;
      stats.textureCopies++;
      // copies read through a palette are made anew from this copy
      for (auto it = paletted.begin(); it != paletted.end();) {
        glDeleteTextures(1, &it->second.id);
        it = paletted.erase(it);
      }
    }
    // render the EFB region into the target
    glBindFramebuffer(GL_FRAMEBUFFER, xfbFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
    SaveAndPlainState();
    const int tw = toXfb ? w : ow, th = toXfb ? h : oh;
    glViewport(0, 0, tw * scale, th * scale);
    glUseProgram(copyProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depth ? efbDepth : efbColor);
    glBindSampler(0, 0);
    glUniform1i(copyMode, toXfb ? 0 : mode);
    glUniform4f(copyRect, static_cast<float>(x * scale), static_cast<float>(y * scale), static_cast<float>(w * scale),
                static_cast<float>(h * scale));
    glUniform2f(copySize, static_cast<float>(kEfbWidth * scale), static_cast<float>(kEfbHeight * scale));
    glUniform1i(copyHalf, half ? 1 : 0);
    glBindVertexArray(quadVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    current = nullptr;
    glBindFramebuffer(GL_FRAMEBUFFER, efbFbo);
    // clear the copied region: colour if colour/alpha update is on, depth if
    // depth update is on (GXCopyDisp/GXCopyTex set the registers for it)
    if ((control >> 11) & 1) {
      const std::uint32_t c = bp[0x41];
      const bool color = (c >> 3) & 1, alpha = (c >> 4) & 1, zupdate = (bp[0x40] >> 4) & 1;
      glEnable(GL_SCISSOR_TEST);
      glScissor(x * scale, y * scale, w * scale, h * scale);
      glColorMask(color, color, color, alpha);
      glDepthMask(zupdate ? GL_TRUE : GL_FALSE);
      const float r = static_cast<float>(bp[0x4F] & 255) / 255.0f, a = static_cast<float>((bp[0x4F] >> 8) & 255) / 255.0f;
      const float b = static_cast<float>(bp[0x50] & 255) / 255.0f, g = static_cast<float>((bp[0x50] >> 8) & 255) / 255.0f;
      glClearColor(r, g, b, a);
      glClearDepthf(static_cast<float>(bp[0x51] & 0xFFFFFF) / 16777215.0f);
      GLbitfield mask = 0;
      if (color || alpha)
        mask |= GL_COLOR_BUFFER_BIT;
      if (zupdate)
        mask |= GL_DEPTH_BUFFER_BIT;
      if (mask)
        glClear(mask);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glDepthMask(GL_TRUE);
    }
  }

  // --- programs of the renderer itself --------------------------------------------------

  bool MakeUtilityPrograms(std::string *error) {
    static const char kQuadVs[] = "#version 300 es\n"
                                  "out vec2 vUv;\n"
                                  "void main() {\n"
                                  "  vec2 p = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));\n"
                                  "  vUv = p;\n"
                                  "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
                                  "}\n";
    // EFB to a copy: the source rectangle (EFB texels), optionally the 2x2 box
    // filter, then the conversion of the copy format
    static const char kCopyFs[] =
        "#version 300 es\nprecision highp float;\nprecision highp int;\n"
        "uniform sampler2D src;\nuniform int mode;\nuniform vec4 rect;\nuniform vec2 size;\nuniform int halfScale;\n"
        "in vec2 vUv;\nout vec4 o;\n"
        "vec4 fetch(vec2 p) { return texture(src, p / size); }\n"
        "void main() {\n"
        "  vec2 p = rect.xy + vUv * rect.zw;\n"
        "  vec4 c = halfScale != 0 ? 0.25 * (fetch(p + vec2(-0.5, -0.5)) + fetch(p + vec2(0.5, -0.5)) + fetch(p + "
        "vec2(-0.5, 0.5)) + fetch(p + vec2(0.5, 0.5))) : fetch(p);\n"
        "  float y = clamp(0.257 * c.r + 0.504 * c.g + 0.098 * c.b + 16.0 / 255.0, 0.0, 1.0);\n"
        "  uint z = uint(c.r * 16777215.0);\n"
        "  float zh = float((z >> 16) & 255u) / 255.0, zm = float((z >> 8) & 255u) / 255.0, zl = float(z & 255u) / "
        "255.0;\n"
        "  if (mode == 0) o = c;\n"
        "  else if (mode == 1) o = vec4(floor(c.rgb * vec3(31.0, 63.0, 31.0) + 0.5) / vec3(31.0, 63.0, 31.0), 1.0);\n"
        "  else if (mode == 2) o = c.a >= 0.999 ? vec4(floor(c.rgb * 31.0 + 0.5) / 31.0, 1.0) : "
        "vec4(floor(c * vec4(15.0, 15.0, 15.0, 7.0) + 0.5) / vec4(15.0, 15.0, 15.0, 7.0));\n"
        "  else if (mode == 3) o = vec4(y);\n"
        "  else if (mode == 4) o = vec4(y, y, y, c.a);\n"
        "  else if (mode == 5) o = vec4(c.r);\n"
        "  else if (mode == 6) o = vec4(c.g);\n"
        "  else if (mode == 7) o = vec4(c.b);\n"
        "  else if (mode == 8) o = vec4(c.a);\n"
        "  else if (mode == 9) o = vec4(c.g, c.g, c.g, c.r);\n"
        "  else if (mode == 10) o = vec4(c.b, c.b, c.b, c.g);\n"
        "  else if (mode == 11) o = vec4(c.r, c.r, c.r, c.a);\n"
        "  else if (mode == 12) o = vec4(zh);\n"
        "  else if (mode == 13) o = vec4(zm);\n"
        "  else if (mode == 14) o = vec4(zl);\n"
        "  else if (mode == 15) o = vec4(zm, zm, zm, zh);\n"
        "  else o = vec4(zh, zm, zl, 1.0);\n"
        "}\n";
    // a copy read through a palette: the copy's value (one channel: .r; two
    // channels: .a high byte, .r low byte) indexes the palette texture
    static const char kPaletteFs[] =
        "#version 300 es\nprecision highp float;\nprecision highp int;\n"
        "uniform sampler2D src;\nuniform sampler2D pal;\nuniform vec2 palSize;\nuniform int kind;\nuniform int "
        "format;\n"
        "in vec2 vUv;\nout vec4 o;\n"
        "void main() {\n"
        "  vec4 c = texture(src, vUv);\n"
        "  int index = kind == 2 ? (int(round(c.a * 255.0)) << 8 | int(round(c.r * 255.0))) & 16383 : "
        "int(round(c.r * 255.0));\n"
        "  if (format == 8) index = index >> 4;\n"
        "  int w = int(palSize.x);\n"
        "  o = texelFetch(pal, ivec2(index % w, index / w), 0);\n"
        "}\n";
    static const char kPresentFs[] = "#version 300 es\nprecision highp float;\n"
                                     "uniform sampler2D src;\nin vec2 vUv;\nout vec4 o;\n"
                                     "void main() { o = vec4(texture(src, vec2(vUv.x, 1.0 - vUv.y)).rgb, 1.0); }\n";
    std::string log;
    copyProgram = Link(kQuadVs, kCopyFs, &log);
    if (!copyProgram) {
      *error = "copy shader: " + log;
      return false;
    }
    glUseProgram(copyProgram);
    glUniform1i(glGetUniformLocation(copyProgram, "src"), 0);
    copyMode = glGetUniformLocation(copyProgram, "mode");
    copyRect = glGetUniformLocation(copyProgram, "rect");
    copySize = glGetUniformLocation(copyProgram, "size");
    copyHalf = glGetUniformLocation(copyProgram, "halfScale");
    paletteProgram = Link(kQuadVs, kPaletteFs, &log);
    if (!paletteProgram) {
      *error = "palette shader: " + log;
      return false;
    }
    glUseProgram(paletteProgram);
    glUniform1i(glGetUniformLocation(paletteProgram, "src"), 0);
    glUniform1i(glGetUniformLocation(paletteProgram, "pal"), 1);
    paletteSize = glGetUniformLocation(paletteProgram, "palSize");
    paletteKind = glGetUniformLocation(paletteProgram, "kind");
    paletteFormat = glGetUniformLocation(paletteProgram, "format");
    presentProgram = Link(kQuadVs, kPresentFs, &log);
    if (!presentProgram) {
      *error = "present shader: " + log;
      return false;
    }
    glUseProgram(presentProgram);
    glUniform1i(glGetUniformLocation(presentProgram, "src"), 0);
    return true;
  }
};

GxRenderer::GxRenderer() : impl_(new Impl) {}

GxRenderer::~GxRenderer() = default;

bool GxRenderer::Init(int scale, std::string *error) {
  Impl &m = *impl_;
  std::string dummy;
  if (!error)
    error = &dummy;
  m.scale = std::max(1, scale);
  const int w = kEfbWidth * m.scale, h = kEfbHeight * m.scale;
  glGenTextures(1, &m.efbColor);
  glBindTexture(GL_TEXTURE_2D, m.efbColor);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenTextures(1, &m.efbDepth);
  glBindTexture(GL_TEXTURE_2D, m.efbDepth);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1, &m.efbFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, m.efbFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m.efbColor, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m.efbDepth, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    *error = "EFB framebuffer incomplete";
    return false;
  }
  glViewport(0, 0, w, h);
  glClearColor(0, 0, 0, 1);
  glClearDepthf(1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glGenFramebuffers(1, &m.xfbFbo);
  glGenVertexArrays(1, &m.vao);
  glGenVertexArrays(1, &m.quadVao);
  glGenBuffers(1, &m.vbo);
  glGenBuffers(1, &m.ibo);
  glBindVertexArray(m.vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
  glBindVertexArray(0);
  glGenBuffers(1, &m.xfUbo);
  glBindBuffer(GL_UNIFORM_BUFFER, m.xfUbo);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(XfBlock), nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, kXfBinding, m.xfUbo);
  glGenBuffers(1, &m.pxUbo);
  glBindBuffer(GL_UNIFORM_BUFFER, m.pxUbo);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(PixelBlock), nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, kPixelBinding, m.pxUbo);
  const std::uint8_t white[4] = {255, 255, 255, 255};
  glGenTextures(1, &m.whiteTex);
  glBindTexture(GL_TEXTURE_2D, m.whiteTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  for (auto &b : m.bound)
    b.id = m.whiteTex;
  return m.MakeUtilityPrograms(error);
}

void GxRenderer::Run(const Re4GxChunk &chunk) {
  Impl &m = *impl_;
  if (chunk.version != RE4_GX_FRAME_VERSION)
    return;
  m.stats.chunks++;
  if (chunk.flags & RE4_GX_CHUNK_RESET) {
    m.stats.resets++;
    m.DropTextures();
  }
  // this chunk's vertices and indices
  glBindVertexArray(m.vao);
  glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
  glBufferData(GL_ARRAY_BUFFER, chunk.vertexBytes ? chunk.vertexBytes : 4, chunk.vertices, GL_STREAM_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, chunk.indexCount ? chunk.indexCount * 2 : 4, chunk.indices, GL_STREAM_DRAW);
  glBindFramebuffer(GL_FRAMEBUFFER, m.efbFbo);
  // state other GL code of the app may have left
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  glDisable(GL_DITHER);
  glFrontFace(GL_CCW);
  glBindBufferBase(GL_UNIFORM_BUFFER, kXfBinding, m.xfUbo);
  glBindBufferBase(GL_UNIFORM_BUFFER, kPixelBinding, m.pxUbo);
  m.current = nullptr;
  std::uint32_t at = 0;
  while (at < chunk.opWords) {
    const std::uint32_t head = chunk.ops[at];
    const std::uint32_t op = head & 0xFF, n = head >> 8;
    const std::uint32_t *p = chunk.ops + at + 1;
    if (at + 1 + n > chunk.opWords)
      break;
    switch (op) {
    case RE4_GX_OP_BP:
      if (n >= 1)
        m.BpWrite(p[0], true);
      break;
    case RE4_GX_OP_BP_STATE:
      for (std::uint32_t r = 0; r < n && r < 256; r++)
        m.BpWrite(r << 24 | (p[r] & 0xFFFFFF), false);
      break;
    case RE4_GX_OP_XF:
      if (n >= 1)
        m.XfWrite(p[0], p + 1, n - 1);
      break;
    case RE4_GX_OP_TLUT:
      if (n >= 3 && p[0] < kTlutBytes && p[2] + p[1] <= chunk.blobBytes)
        std::memcpy(m.tlut.data() + p[0], chunk.blob + p[2], std::min<std::uint32_t>(p[1], kTlutBytes - p[0]));
      break;
    case RE4_GX_OP_TEXTURE:
      if (n * 4 >= sizeof(Re4GxTexture)) {
        Re4GxTexture t;
        std::memcpy(&t, p, sizeof(t));
        m.BindTexture(t, chunk);
      }
      break;
    case RE4_GX_OP_FORGET:
      if (n >= 2)
        m.Forget(static_cast<std::uint64_t>(p[1]) << 32 | p[0]);
      break;
    case RE4_GX_OP_DRAW:
      if (n * 4 >= sizeof(Re4GxDraw)) {
        Re4GxDraw d;
        std::memcpy(&d, p, sizeof(d));
        m.Draw(d, chunk);
      }
      break;
    default:
      break;
    }
    at += 1 + n;
  }
  // leave GL as the app's other renderers expect it
  glBindVertexArray(0);
  for (GLuint unit = 0; unit < 8; unit++)
    glBindSampler(unit, 0);
  glActiveTexture(GL_TEXTURE0);
  glUseProgram(0);
  m.current = nullptr;
  m.SaveAndPlainState();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint GxRenderer::Xfb(unsigned *width, unsigned *height) const {
  if (width)
    *width = impl_->xfbWidth;
  if (height)
    *height = impl_->xfbHeight;
  return impl_->haveXfb ? impl_->xfbTex : 0;
}

bool GxRenderer::Present(int x, int y, int w, int h) {
  Impl &m = *impl_;
  if (!m.haveXfb)
    return false;
  m.SaveAndPlainState();
  glViewport(x, y, w, h);
  glUseProgram(m.presentProgram);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m.xfbTex);
  glBindSampler(0, 0);
  glBindVertexArray(m.quadVao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
  glUseProgram(0);
  m.current = nullptr;
  return true;
}

GLuint GxRenderer::EfbColor() const { return impl_->efbColor; }

int GxRenderer::Scale() const { return impl_->scale; }

const GxStats &GxRenderer::Stats() const { return impl_->stats; }

void GxRenderer::Release() {
  Impl &m = *impl_;
  m.DropTextures();
  for (auto &c : m.copies)
    glDeleteTextures(1, &c.second.id);
  m.copies.clear();
  for (auto &p : m.programs)
    if (p.second.id)
      glDeleteProgram(p.second.id);
  m.programs.clear();
  for (auto &s : m.samplers)
    glDeleteSamplers(1, &s.second);
  m.samplers.clear();
  const GLuint textures[] = {m.efbColor, m.efbDepth, m.xfbTex, m.whiteTex};
  glDeleteTextures(4, textures);
  const GLuint fbos[] = {m.efbFbo, m.xfbFbo};
  glDeleteFramebuffers(2, fbos);
  const GLuint buffers[] = {m.vbo, m.ibo, m.xfUbo, m.pxUbo};
  glDeleteBuffers(4, buffers);
  const GLuint vaos[] = {m.vao, m.quadVao};
  glDeleteVertexArrays(2, vaos);
  glDeleteProgram(m.copyProgram);
  glDeleteProgram(m.paletteProgram);
  glDeleteProgram(m.presentProgram);
  impl_.reset(new Impl);
}

} // namespace re4gx
