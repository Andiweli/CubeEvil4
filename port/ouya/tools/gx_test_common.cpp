// gx_test_common.h: contexts, pictures, the GLES 3 reference, compact to float vertices.
#include "gx_test_common.h"

#include "gx_gl.h"
#include "re4_gx_frame.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ouyatest {

bool MakeContext(int major, std::string *error) {
  auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
  EGLDisplay display = getDisplay ? getDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)
                                  : eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint ma = 0, mi = 0;
  if (!eglInitialize(display, &ma, &mi)) {
    *error = "eglInitialize failed";
    return false;
  }
  const EGLint attributes[] = {EGL_RENDERABLE_TYPE, major >= 3 ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_ES2_BIT, EGL_NONE};
  EGLConfig config{};
  EGLint count = 0;
  eglChooseConfig(display, attributes, &config, 1, &count);
  eglBindAPI(EGL_OPENGL_ES_API);
  const EGLint contextAttributes[] = {EGL_CONTEXT_MAJOR_VERSION, major, EGL_NONE};
  EGLContext context = eglCreateContext(display, count ? config : nullptr, EGL_NO_CONTEXT, contextAttributes);
  if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) {
    *error = "no GLES context (Mesa packages: port/tools/whole_setup_linux.sh)";
    return false;
  }
  return true;
}

void WritePpm(const std::string &path, const Picture &p) {
  FILE *f = std::fopen(path.c_str(), "wb");
  if (!f)
    return;
  std::fprintf(f, "P6\n%u %u\n255\n", p.width, p.height);
  std::vector<std::uint8_t> row(static_cast<std::size_t>(p.width) * 3);
  for (unsigned y = 0; y < p.height; y++) {
    for (unsigned x = 0; x < p.width; x++)
      for (int c = 0; c < 3; c++)
        row[x * 3 + c] = p.rgba[(static_cast<std::size_t>(y) * p.width + x) * 4 + c];
    std::fwrite(row.data(), 1, row.size(), f);
  }
  std::fclose(f);
}

Difference Compare(const Picture &a, const Picture &b, int tolerance, bool alpha) {
  Difference d;
  if (a.width != b.width || a.height != b.height || a.rgba.size() != b.rgba.size()) {
    d.sizeDiffers = true;
    return d;
  }
  const std::size_t n = static_cast<std::size_t>(a.width) * a.height;
  for (std::size_t i = 0; i < n; i++) {
    unsigned most = 0;
    for (int c = 0; c < (alpha ? 4 : 3); c++)
      most = std::max(most, static_cast<unsigned>(std::abs(a.rgba[i * 4 + c] - b.rgba[i * 4 + c])));
    d.worst = std::max(d.worst, most);
    if (most > static_cast<unsigned>(tolerance))
      d.pixels++;
  }
  return d;
}

Picture DifferencePicture(const Picture &a, const Picture &b) {
  Picture d = a;
  if (a.rgba.size() != b.rgba.size())
    return d;
  // red: the colour's difference, green: the alpha's (16 times)
  for (std::size_t i = 0; i < a.rgba.size(); i += 4) {
    int colour = 0;
    for (int c = 0; c < 3; c++)
      colour = std::max(colour, std::abs(a.rgba[i + c] - b.rgba[i + c]));
    d.rgba[i] = static_cast<std::uint8_t>(std::min(255, 16 * colour));
    d.rgba[i + 1] = static_cast<std::uint8_t>(std::min(255, 16 * std::abs(a.rgba[i + 3] - b.rgba[i + 3])));
    d.rgba[i + 2] = 0;
    d.rgba[i + 3] = 255;
  }
  return d;
}

namespace {

Picture Read(GLuint texture, unsigned width, unsigned height) {
  Picture p;
  if (!texture)
    return p;
  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  p.width = width;
  p.height = height;
  p.rgba.resize(static_cast<std::size_t>(width) * height * 4);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, p.rgba.data());
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &fbo);
  return p;
}

} // namespace

struct Reference::Impl {
  re4gx::GxRenderer renderer;
};

Reference::Reference() : impl_(new Impl) {}
Reference::~Reference() = default;

bool Reference::Init(std::string *error) { return impl_->renderer.Init(1, error); }

void Reference::Run(const Re4GxChunk &chunk) { impl_->renderer.Run(chunk); }

Picture Reference::Efb() const { return Read(impl_->renderer.EfbColor(), 640, 528); }

Picture Reference::Xfb() const {
  unsigned w = 0, h = 0;
  const GLuint xfb = impl_->renderer.Xfb(&w, &h);
  return Read(xfb, w, h);
}

unsigned Reference::ShaderErrors() const { return impl_->renderer.Stats().shaderErrors; }

namespace {

// one component of a compact attribute as the float vertex has it (gx_probe.cpp floatBits)
float Component(const std::uint8_t *p, std::uint32_t type, std::uint32_t fraction, bool normal) {
  const float scale = normal ? 0.0f : std::ldexp(1.0f, -static_cast<int>(fraction));
  switch (type) {
  case RE4_GX_CT_U8:
    return normal ? static_cast<float>(p[0]) * (1.0f / 64) : static_cast<float>(p[0]) * scale;
  case RE4_GX_CT_S8:
    return normal ? static_cast<float>(static_cast<std::int8_t>(p[0])) * (1.0f / 64)
                  : static_cast<float>(static_cast<std::int8_t>(p[0])) * scale;
  case RE4_GX_CT_U16: {
    std::uint16_t h;
    std::memcpy(&h, p, 2);
    return normal ? static_cast<float>(h) * (1.0f / 16384) : static_cast<float>(h) * scale;
  }
  case RE4_GX_CT_S16: {
    std::int16_t h;
    std::memcpy(&h, p, 2);
    return normal ? static_cast<float>(h) * (1.0f / 16384) : static_cast<float>(h) * scale;
  }
  default: {
    float f;
    std::memcpy(&f, p, 4);
    return f;
  }
  }
}

void PutFloats(std::vector<std::uint8_t> &out, const std::uint8_t *vertex, std::uint32_t word, std::uint32_t floats,
               bool normal) {
  const std::uint32_t type = (word >> 8) & 15, comps = (word >> 12) & 15, fraction = (word >> 16) & 31;
  const std::uint32_t size = type == RE4_GX_CT_U8 || type == RE4_GX_CT_S8 ? 1 : type <= RE4_GX_CT_S16 ? 2 : 4;
  for (std::uint32_t k = 0; k < floats; k++) {
    float v = 0.0f;
    if ((word & RE4_GX_CA_PRESENT) && k < comps)
      v = Component(vertex + (word & 0xFF) + k * size, type, fraction, normal);
    const auto *b = reinterpret_cast<const std::uint8_t *>(&v);
    out.insert(out.end(), b, b + 4);
  }
}

} // namespace

void ToFloatVertices(const Re4GxChunk &chunk, FloatChunk &storage, Re4GxChunk &out) {
  storage.ops.clear();
  storage.vertices.clear();
  std::uint32_t words[RE4_GX_COMPACT_ATTRS] = {};
  for (std::uint32_t at = 0; at < chunk.opWords;) {
    const std::uint32_t op = chunk.ops[at] & 0xFF, n = chunk.ops[at] >> 8;
    const std::uint32_t *payload = chunk.ops + at + 1;
    if (at + 1 + n > chunk.opWords)
      break;
    if (op == RE4_GX_OP_COMPACT_FORMAT && n == RE4_GX_COMPACT_ATTRS) {
      std::memcpy(words, payload, sizeof(words));
    } else if (op == RE4_GX_OP_DRAW && n * 4 >= sizeof(Re4GxDraw)) {
      Re4GxDraw d;
      std::memcpy(&d, payload, sizeof(d));
      const std::uint8_t *v = chunk.vertices + d.vertexOffset;
      const std::uint32_t start = static_cast<std::uint32_t>(storage.vertices.size());
      if (!(d.layout & RE4_GX_V_COMPACT)) {
        storage.vertices.insert(storage.vertices.end(), v, v + static_cast<std::size_t>(d.vertexCount) * d.stride);
      } else {
        for (std::uint32_t i = 0; i < d.vertexCount; i++, v += d.stride) {
          PutFloats(storage.vertices, v, words[RE4_GX_CA_POSITION], 3, false);
          if (d.layout & RE4_GX_V_NORMAL)
            PutFloats(storage.vertices, v, words[RE4_GX_CA_NORMAL], (d.layout & RE4_GX_V_BINORMAL_TANGENT) ? 9 : 3,
                      true);
          for (int k = 0; k < 2; k++)
            if (d.layout & (RE4_GX_V_COLOR0 << k)) {
              const std::uint8_t *c = v + (words[RE4_GX_CA_COLOR0 + k] & 0xFF);
              storage.vertices.insert(storage.vertices.end(), c, c + 4);
            }
          for (int t = 0; t < 8; t++)
            if (d.layout & (RE4_GX_V_TEX0 << t))
              PutFloats(storage.vertices, v, words[RE4_GX_CA_TEX0 + t], 2, false);
          if (d.layout & RE4_GX_V_MATRIX) {
            std::uint8_t m[12] = {};
            for (int slot = 0; slot < 9; slot++)
              m[slot] = static_cast<std::uint8_t>(d.matrices[slot / 4] >> (8 * (slot % 4)));
            const std::uint32_t w = words[RE4_GX_CA_MATRIX], slots = (w >> 16) & 0x1FF;
            const std::uint8_t *own = v + (w & 0xFF);
            for (int slot = 0, j = 0; slot < 9; slot++)
              if (slots & (1u << slot))
                m[slot] = own[j++];
            storage.vertices.insert(storage.vertices.end(), m, m + 12);
          }
        }
        const std::uint32_t bytes = static_cast<std::uint32_t>(storage.vertices.size()) - start;
        d.stride = d.vertexCount ? bytes / d.vertexCount : 0;
        d.layout &= ~static_cast<std::uint32_t>(RE4_GX_V_COMPACT);
      }
      d.vertexOffset = start;
      storage.ops.push_back(op | n << 8);
      const auto *dw = reinterpret_cast<const std::uint32_t *>(&d);
      storage.ops.insert(storage.ops.end(), dw, dw + sizeof(d) / 4);
      for (std::uint32_t k = sizeof(d) / 4; k < n; k++)
        storage.ops.push_back(payload[k]);
      at += 1 + n;
      continue;
    }
    if (op != RE4_GX_OP_COMPACT_FORMAT)
      storage.ops.insert(storage.ops.end(), chunk.ops + at, chunk.ops + at + 1 + n);
    at += 1 + n;
  }
  out = chunk;
  out.ops = storage.ops.data();
  out.opWords = static_cast<std::uint32_t>(storage.ops.size());
  out.vertices = storage.vertices.data();
  out.vertexBytes = static_cast<std::uint32_t>(storage.vertices.size());
}

} // namespace ouyatest
