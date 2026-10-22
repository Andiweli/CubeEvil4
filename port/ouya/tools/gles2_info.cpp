// What a GLES 2 device can do, as the GX renderer of the OUYA app (port/ouya, stage O2 of
// port/docs/ouya-machbarkeit.md) needs to know it: limits, shader precision, the frame buffer
// formats it takes, how many vertex uniforms a shader may really index. No disc data; runs as a
// plain program (adb shell on the OUYA, Linux with Mesa: MESA_GLES_VERSION_OVERRIDE=2.0).
//
//   gles2_info            all of it as "key: value" lines on stdout
//
// Built for the OUYA by port/scripts/ouya-gpu-info.ps1 (NDK r23c, android-16).
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef GL_DEPTH_COMPONENT16_NONLINEAR_NV
#define GL_DEPTH_COMPONENT16_NONLINEAR_NV 0x8E2C
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

namespace {

void Line(const char *key, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Line(const char *key, const char *fmt, ...) {
  std::printf("%s: ", key);
  va_list ap;
  va_start(ap, fmt);
  std::vprintf(fmt, ap);
  va_end(ap);
  std::printf("\n");
  std::fflush(stdout);
}

bool Has(const char *list, const char *name) {
  if (!list)
    return false;
  const std::size_t n = std::strlen(name);
  for (const char *at = std::strstr(list, name); at; at = std::strstr(at + n, name))
    if ((at == list || at[-1] == ' ') && (at[n] == ' ' || at[n] == '\0'))
      return true;
  return false;
}

void ClearErrors() {
  while (glGetError() != GL_NO_ERROR) {
  }
}

GLuint Compile(GLenum type, const std::string &source, std::string *log) {
  GLuint s = glCreateShader(type);
  const char *text = source.c_str();
  glShaderSource(s, 1, &text, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char buffer[2048] = {};
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
  glBindAttribLocation(p, 0, "aPos");
  glLinkProgram(p);
  glDeleteShader(v);
  glDeleteShader(f);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char buffer[2048] = {};
    glGetProgramInfoLog(p, sizeof(buffer), nullptr, buffer);
    if (log)
      *log = buffer;
    glDeleteProgram(p);
    return 0;
  }
  return p;
}

std::string OneLine(std::string s) {
  for (char &c : s)
    if (c == '\n' || c == '\r')
      c = ' ';
  if (s.size() > 300)
    s.resize(300);
  return s;
}

const char kQuadVs[] = "attribute vec2 aPos;\nvoid main() { gl_Position = vec4(aPos, 0.0, 1.0); }\n";

void Quad() {
  static const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// A colour target of w x h (RGBA, unsigned bytes) bound as the frame buffer.
struct Target {
  GLuint tex = 0, fbo = 0;
  bool Make(int w, int h) {
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glViewport(0, 0, w, h);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  }
  void Free() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
  }
};

void Limits() {
  struct {
    GLenum name;
    const char *key;
  } ints[] = {{GL_MAX_VERTEX_ATTRIBS, "max_vertex_attribs"},
              {GL_MAX_VERTEX_UNIFORM_VECTORS, "max_vertex_uniform_vectors"},
              {GL_MAX_FRAGMENT_UNIFORM_VECTORS, "max_fragment_uniform_vectors"},
              {GL_MAX_VARYING_VECTORS, "max_varying_vectors"},
              {GL_MAX_TEXTURE_IMAGE_UNITS, "max_texture_image_units"},
              {GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, "max_vertex_texture_image_units"},
              {GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, "max_combined_texture_image_units"},
              {GL_MAX_TEXTURE_SIZE, "max_texture_size"},
              {GL_MAX_RENDERBUFFER_SIZE, "max_renderbuffer_size"},
              {GL_SUBPIXEL_BITS, "subpixel_bits"},
              {GL_NUM_COMPRESSED_TEXTURE_FORMATS, "compressed_texture_formats"},
              {GL_NUM_SHADER_BINARY_FORMATS, "shader_binary_formats"}};
  for (auto &i : ints) {
    GLint v = -1;
    glGetIntegerv(i.name, &v);
    Line(i.key, "%d", v);
  }
  GLint dims[2] = {};
  glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims);
  Line("max_viewport_dims", "%d x %d", dims[0], dims[1]);
  GLfloat range[2] = {};
  glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
  Line("point_size_range", "%g .. %g", range[0], range[1]);
  glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, range);
  Line("line_width_range", "%g .. %g", range[0], range[1]);
  const char *ext = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
  if (Has(ext, "GL_EXT_texture_filter_anisotropic")) {
    GLfloat most = 0;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &most);
    Line("max_anisotropy", "%g", most);
  }
  if (Has(ext, "GL_OES_get_program_binary")) {
    GLint n = 0;
    glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS_OES, &n);
    Line("program_binary_formats", "%d", n);
  }
  GLint bits = 0;
  glGetIntegerv(GL_DEPTH_BITS, &bits);
  Line("default_framebuffer_depth_bits", "%d", bits);
  ClearErrors();
}

void Precision() {
  const struct {
    GLenum shader;
    const char *name;
  } shaders[] = {{GL_VERTEX_SHADER, "vertex"}, {GL_FRAGMENT_SHADER, "fragment"}};
  const struct {
    GLenum type;
    const char *name;
  } types[] = {{GL_LOW_FLOAT, "lowp_float"},   {GL_MEDIUM_FLOAT, "mediump_float"}, {GL_HIGH_FLOAT, "highp_float"},
               {GL_LOW_INT, "lowp_int"},       {GL_MEDIUM_INT, "mediump_int"},     {GL_HIGH_INT, "highp_int"}};
  for (auto &s : shaders)
    for (auto &t : types) {
      GLint range[2] = {-1, -1}, precision = -1;
      glGetShaderPrecisionFormat(s.shader, t.type, range, &precision);
      char key[64];
      std::snprintf(key, sizeof(key), "precision_%s_%s", s.name, t.name);
      Line(key, "range %d %d, precision %d", range[0], range[1], precision);
    }
  ClearErrors();
}

// The integers a fragment shader keeps exactly: for k = 8..24, whether 2^k + 1 stays odd (the
// value through a uniform and its parity through mod, so the compiler can neither fold it nor
// cancel big + 1 - big), in the given precision. TEV values reach 65 280 in the GLES 3
// renderer's integer arithmetic.
void FragmentIntegers(const char *precision) {
  const std::string fs = std::string("precision ") + precision +
                         " float;\n"
                         "uniform float one;\n"
                         "void main() {\n"
                         "  float k = floor(gl_FragCoord.x) + 8.0;\n"
                         "  float odd = exp2(k) * one + one;\n"
                         "  float halved = floor(odd * 0.5);\n"
                         "  float parity = odd - 2.0 * halved;\n"
                         "  gl_FragColor = vec4(parity > 0.5 ? 1.0 : 0.0, 0.0, 0.0, 1.0);\n"
                         "}\n";
  std::string log;
  GLuint p = Link(kQuadVs, fs, &log);
  char key[64];
  std::snprintf(key, sizeof(key), "fragment_%s_exact_integers", precision);
  if (!p) {
    Line(key, "shader failed: %s", OneLine(log).c_str());
    return;
  }
  Target t;
  t.Make(17, 1);
  glUseProgram(p);
  glUniform1f(glGetUniformLocation(p, "one"), 1.0f);
  Quad();
  unsigned char px[17 * 4] = {};
  glReadPixels(0, 0, 17, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
  int exact = -1;
  for (int i = 0; i < 17; i++)
    if (px[i * 4] > 128)
      exact = i + 8;
    else
      break;
  Line(key, exact < 0 ? "below 2^8" : "up to 2^%d + 1", exact);
  t.Free();
  glDeleteProgram(p);
}

// The texture coordinate a fragment shader computes with (a dependent read) against the one it
// passes on unchanged: a 2048-texel texture with a step every texel, repeated, each of 64 pixels
// sampling the centre of texel 20 + its column, n repeats further on; how many read the wrong
// texel.
void TextureCoordinates() {
  const int size = 2048;
  std::vector<unsigned char> ramp(static_cast<std::size_t>(size) * 4);
  for (int i = 0; i < size; i++) {
    ramp[i * 4] = static_cast<unsigned char>(i & 255);
    ramp[i * 4 + 1] = static_cast<unsigned char>(i >> 8);
    ramp[i * 4 + 2] = 0;
    ramp[i * 4 + 3] = 255;
  }
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, ramp.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  const char vs[] = "attribute vec2 aPos;\nuniform float repeats;\nvarying vec2 vUv;\n"
                    "void main() {\n"
                    "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
                    "  vUv = vec2(repeats + (20.0 + (aPos.x * 0.5 + 0.5) * 64.0) / 2048.0, 0.5);\n"
                    "}\n";
  for (int dependent = 0; dependent < 2; dependent++) {
    const std::string fs = std::string("precision mediump float;\nuniform sampler2D s;\nuniform float scale;\n"
                                       "varying vec2 vUv;\n"
                                       "void main() { gl_FragColor = texture2D(s, ") +
                           (dependent ? "vUv * scale" : "vUv") + "); }\n";
    std::string log;
    GLuint p = Link(vs, fs, &log);
    if (!p) {
      Line("texcoord", "shader failed: %s", OneLine(log).c_str());
      continue;
    }
    glUseProgram(p);
    glUniform1i(glGetUniformLocation(p, "s"), 0);
    glUniform1f(glGetUniformLocation(p, "scale"), 1.0f);
    std::string results;
    for (int repeats : {0, 4, 16, 64}) {
      Target t;
      t.Make(64, 1);
      glBindTexture(GL_TEXTURE_2D, tex);
      glUniform1f(glGetUniformLocation(p, "repeats"), static_cast<float>(repeats));
      Quad();
      unsigned char px[64 * 4] = {};
      glReadPixels(0, 0, 64, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
      int wrong = 0;
      for (int i = 0; i < 64; i++) {
        const int texel = px[i * 4] | px[i * 4 + 1] << 8;
        if (texel != 20 + i)
          wrong++;
      }
      char part[48];
      std::snprintf(part, sizeof(part), "%s%d repeats: %d wrong", results.empty() ? "" : ", ", repeats, wrong);
      results += part;
      t.Free();
    }
    Line(dependent ? "texcoord_computed_in_fragment" : "texcoord_passed_through", "%s", results.c_str());
    glDeleteProgram(p);
  }
  glDeleteTextures(1, &tex);
}

// How many vec4 a vertex shader can index dynamically (the XF block of the GLES 3 renderer has
// 208), and with which a draw still works.
void VertexUniforms() {
  int best = 0;
  std::string lastLog;
  for (int n : {64, 128, 160, 192, 208, 224, 240, 248, 252, 256, 512, 1024}) {
    char vs[512];
    std::snprintf(vs, sizeof(vs),
                  "attribute vec2 aPos;\nuniform vec4 rows[%d];\nuniform float pick;\nvarying vec4 vColor;\n"
                  "void main() {\n"
                  "  int i = int(pick);\n"
                  "  vColor = rows[i] + rows[i + 1] * 0.0;\n"
                  "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
                  "}\n",
                  n);
    const char fs[] = "precision mediump float;\nvarying vec4 vColor;\nvoid main() { gl_FragColor = vColor; }\n";
    std::string log;
    GLuint p = Link(vs, fs, &log);
    if (!p) {
      lastLog = log;
      break;
    }
    // the last two rows must read back
    std::vector<float> rows(static_cast<std::size_t>(n) * 4, 0.0f);
    rows[(n - 2) * 4 + 1] = 1.0f;
    rows[(n - 2) * 4 + 3] = 1.0f;
    glUseProgram(p);
    glUniform4fv(glGetUniformLocation(p, "rows"), n, rows.data());
    glUniform1f(glGetUniformLocation(p, "pick"), static_cast<float>(n - 2));
    Target t;
    t.Make(4, 4);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    Quad();
    unsigned char px[4] = {};
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    t.Free();
    glDeleteProgram(p);
    if (!(px[0] < 16 && px[1] > 240)) {
      lastLog = "linked, but row " + std::to_string(n - 2) + " read " + std::to_string(px[0]) + " " +
                std::to_string(px[1]) + " " + std::to_string(px[2]);
      break;
    }
    best = n;
  }
  Line("vertex_uniform_array_works_up_to", "%d vec4%s%s", best, lastLog.empty() ? "" : "; next: ",
       OneLine(lastLog).c_str());
  ClearErrors();
}

// Varyings a program may really use (vec4 each).
void Varyings() {
  int best = 0;
  std::string lastLog;
  for (int n = 4; n <= 32; n++) {
    std::string vs = "attribute vec2 aPos;\n", fs = "precision mediump float;\n", vbody, fbody = "  vec4 s = vec4(0.0);\n";
    for (int i = 0; i < n; i++) {
      vs += "varying vec4 v" + std::to_string(i) + ";\n";
      fs += "varying vec4 v" + std::to_string(i) + ";\n";
      vbody += "  v" + std::to_string(i) + " = vec4(aPos, float(" + std::to_string(i) + "), 1.0);\n";
      fbody += "  s += v" + std::to_string(i) + ";\n";
    }
    vs += "void main() {\n" + vbody + "  gl_Position = vec4(aPos, 0.0, 1.0);\n}\n";
    fs += "void main() {\n" + fbody + "  gl_FragColor = s * 0.001;\n}\n";
    std::string log;
    GLuint p = Link(vs, fs, &log);
    if (!p) {
      lastLog = log;
      break;
    }
    glDeleteProgram(p);
    best = n;
  }
  Line("varyings_link_up_to", "%d vec4%s%s", best, lastLog.empty() ? "" : "; next: ", OneLine(lastLog).c_str());
}

// Frame buffers like the EFB: an RGBA texture of 640x528 with each kind of depth.
void FrameBuffers() {
  const char *ext = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
  struct Depth {
    const char *name;
    bool texture;
    GLenum format, type;
  } depths[] = {{"renderbuffer DEPTH_COMPONENT16", false, GL_DEPTH_COMPONENT16, 0},
                {"renderbuffer DEPTH_COMPONENT24_OES", false, GL_DEPTH_COMPONENT24_OES, 0},
                {"renderbuffer DEPTH_COMPONENT16_NONLINEAR_NV", false, GL_DEPTH_COMPONENT16_NONLINEAR_NV, 0},
                {"renderbuffer DEPTH24_STENCIL8_OES", false, GL_DEPTH24_STENCIL8_OES, 0},
                {"texture DEPTH_COMPONENT/UNSIGNED_SHORT", true, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT},
                {"texture DEPTH_COMPONENT/UNSIGNED_INT", true, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT}};
  int index = 0;
  for (auto &d : depths) {
    ClearErrors();
    Target t;
    t.Make(640, 528);
    GLuint rb = 0, tex = 0;
    if (d.texture) {
      glGenTextures(1, &tex);
      glBindTexture(GL_TEXTURE_2D, tex);
      glTexImage2D(GL_TEXTURE_2D, 0, d.format, 640, 528, 0, GL_DEPTH_COMPONENT, d.type, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
    } else {
      glGenRenderbuffers(1, &rb);
      glBindRenderbuffer(GL_RENDERBUFFER, rb);
      glRenderbufferStorage(GL_RENDERBUFFER, d.format, 640, 528);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb);
    }
    const GLenum error = glGetError();
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    GLint bits = 0;
    if (status == GL_FRAMEBUFFER_COMPLETE)
      glGetIntegerv(GL_DEPTH_BITS, &bits);
    char key[32];
    std::snprintf(key, sizeof(key), "efb_depth_%d", index++);
    Line(key, "%s: %s, error 0x%x, depth bits %d", d.name,
         status == GL_FRAMEBUFFER_COMPLETE ? "complete" : "INCOMPLETE", error, bits);
    if (rb)
      glDeleteRenderbuffers(1, &rb);
    if (tex)
      glDeleteTextures(1, &tex);
    t.Free();
  }
  // NPOT: a 640x528 texture clamped, repeated, with mipmaps (the GameCube's copies are such)
  struct Npot {
    const char *key;
    GLenum wrap, min;
    bool mipmaps;
  } npots[] = {{"npot_clamp_samples", GL_CLAMP_TO_EDGE, GL_LINEAR, false},
               {"npot_repeat_samples", GL_REPEAT, GL_LINEAR, false},
               {"npot_mirror_samples", GL_MIRRORED_REPEAT, GL_LINEAR, false},
               {"npot_clamp_mipmaps_samples", GL_CLAMP_TO_EDGE, GL_LINEAR_MIPMAP_LINEAR, true}};
  std::string log;
  GLuint p = Link(kQuadVs, "precision mediump float;\nuniform sampler2D s;\n"
                           "void main() { gl_FragColor = texture2D(s, gl_FragCoord.xy * 0.37); }\n",
                  &log);
  for (auto &n : npots) {
    ClearErrors();
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    std::vector<unsigned char> white(640 * 528 * 4, 255);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 640, 528, 0, GL_RGBA, GL_UNSIGNED_BYTE, white.data());
    if (n.mipmaps)
      glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, n.wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, n.wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, n.min);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    Target t;
    t.Make(8, 8);
    glUseProgram(p);
    glUniform1i(glGetUniformLocation(p, "s"), 0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    Quad();
    unsigned char px[4] = {};
    glReadPixels(3, 3, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    Line(n.key, "%s (read %d)", px[0] > 240 ? "white: works" : "black: incomplete", px[0]);
    t.Free();
    glDeleteTextures(1, &tex);
  }
  glDeleteProgram(p);
  // a mipmapped power-of-two texture with only some of its levels (GX textures stop early)
  {
    ClearErrors();
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    std::vector<unsigned char> white(64 * 64 * 4, 255);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, white.data());
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, white.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    GLuint q = Link(kQuadVs, "precision mediump float;\nuniform sampler2D s;\n"
                             "void main() { gl_FragColor = texture2D(s, gl_FragCoord.xy * 0.01); }\n",
                    &log);
    Target t;
    t.Make(8, 8);
    glUseProgram(q);
    glUniform1i(glGetUniformLocation(q, "s"), 0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    Quad();
    unsigned char px[4] = {};
    glReadPixels(3, 3, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    Line("partial_mipmaps_sample", "%s (read %d)", px[0] > 240 ? "white: works" : "black: incomplete", px[0]);
    t.Free();
    glDeleteProgram(q);
    glDeleteTextures(1, &tex);
  }
  // reading the frame buffer's colour in the shader (blending and logic operations exactly)
  if (Has(ext, "GL_NV_shader_framebuffer_fetch")) {
    GLuint q = Link(kQuadVs, "#extension GL_NV_shader_framebuffer_fetch : require\nprecision mediump float;\n"
                             "void main() { gl_FragColor = vec4(1.0) - gl_LastFragColor; }\n",
                    &log);
    Line("framebuffer_fetch_nv", "%s", q ? "compiles" : ("no: " + OneLine(log)).c_str());
    if (q)
      glDeleteProgram(q);
  }
  Line("ext_depth_texture", "%s", Has(ext, "GL_OES_depth_texture") ? "yes" : "no");
  ClearErrors();
}

} // namespace

int main() {
  EGLDisplay display = EGL_NO_DISPLAY;
#ifndef __ANDROID__
  auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
  if (getDisplay)
    display = getDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
#endif
  if (display == EGL_NO_DISPLAY)
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint major = 0, minor = 0;
  if (!eglInitialize(display, &major, &minor)) {
    Line("error", "eglInitialize failed (0x%x)", eglGetError());
    return 1;
  }
  Line("egl_version", "%s", eglQueryString(display, EGL_VERSION));
  Line("egl_vendor", "%s", eglQueryString(display, EGL_VENDOR));
  Line("egl_extensions", "%s", eglQueryString(display, EGL_EXTENSIONS));
  // the configs a window could get: colour and depth bits
  {
    EGLint count = 0;
    eglGetConfigs(display, nullptr, 0, &count);
    std::vector<EGLConfig> configs(static_cast<std::size_t>(count > 0 ? count : 0));
    eglGetConfigs(display, configs.data(), count, &count);
    std::string all;
    for (EGLint i = 0; i < count && i < 64; i++) {
      EGLint r = 0, g = 0, b = 0, a = 0, d = 0, s = 0, samples = 0, type = 0, renderable = 0;
      eglGetConfigAttrib(display, configs[i], EGL_RED_SIZE, &r);
      eglGetConfigAttrib(display, configs[i], EGL_GREEN_SIZE, &g);
      eglGetConfigAttrib(display, configs[i], EGL_BLUE_SIZE, &b);
      eglGetConfigAttrib(display, configs[i], EGL_ALPHA_SIZE, &a);
      eglGetConfigAttrib(display, configs[i], EGL_DEPTH_SIZE, &d);
      eglGetConfigAttrib(display, configs[i], EGL_STENCIL_SIZE, &s);
      eglGetConfigAttrib(display, configs[i], EGL_SAMPLES, &samples);
      eglGetConfigAttrib(display, configs[i], EGL_SURFACE_TYPE, &type);
      eglGetConfigAttrib(display, configs[i], EGL_RENDERABLE_TYPE, &renderable);
      if (!(renderable & EGL_OPENGL_ES2_BIT))
        continue;
      char part[64];
      std::snprintf(part, sizeof(part), "%s%d%d%d%d d%d s%d x%d%s", all.empty() ? "" : ", ", r, g, b, a, d, s,
                    samples, (type & EGL_WINDOW_BIT) ? " win" : "");
      all += part;
    }
    Line("egl_configs_es2", "%d: %s", count, all.c_str());
  }
  eglBindAPI(EGL_OPENGL_ES_API);
  const EGLint pbufferConfig[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                                  EGL_RED_SIZE,        8,                  EGL_GREEN_SIZE,   8,
                                  EGL_BLUE_SIZE,       8,                  EGL_NONE};
  const EGLint anyConfig[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE};
  EGLConfig config = nullptr;
  EGLint count = 0;
  EGLSurface surface = EGL_NO_SURFACE;
  if (eglChooseConfig(display, pbufferConfig, &config, 1, &count) && count > 0) {
    const EGLint size[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    surface = eglCreatePbufferSurface(display, config, size);
  }
  if (surface == EGL_NO_SURFACE) {
    count = 0;
    eglChooseConfig(display, anyConfig, &config, 1, &count);
  }
  const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  EGLContext context = eglCreateContext(display, count ? config : nullptr, EGL_NO_CONTEXT, contextAttributes);
  if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context)) {
    Line("error", "no GLES 2 context (0x%x)", eglGetError());
    return 1;
  }
  Line("surface", "%s", surface == EGL_NO_SURFACE ? "none (surfaceless)" : "pbuffer 16x16");
  Line("gl_vendor", "%s", reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
  Line("gl_renderer", "%s", reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
  Line("gl_version", "%s", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
  Line("glsl_version", "%s", reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
  Line("gl_extensions", "%s", reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)));
  Limits();
  Precision();
  FragmentIntegers("mediump");
  {
    // highp in fragment shaders is optional in GLES 2
    std::string log;
    GLuint p = Link(kQuadVs, "precision highp float;\nvoid main() { gl_FragColor = vec4(1.0); }\n", &log);
    Line("fragment_highp", "%s", p ? "compiles" : ("no: " + OneLine(log)).c_str());
    if (p) {
      glDeleteProgram(p);
      FragmentIntegers("highp");
    }
  }
  TextureCoordinates();
  VertexUniforms();
  Varyings();
  FrameBuffers();
  Line("done", "ok");
  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(display, context);
  if (surface != EGL_NO_SURFACE)
    eglDestroySurface(display, surface);
  eglTerminate(display);
  return 0;
}
