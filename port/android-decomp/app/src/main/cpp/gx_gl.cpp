#include "gx_gl.h"

#include "gx_gl_shader.h"
#include "gx_texture.h"
#include "re4_gx_frame.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

#include <dlfcn.h>

#ifndef GL_MAP_PERSISTENT_BIT_EXT
#define GL_MAP_PERSISTENT_BIT_EXT 0x0040
#endif
#ifndef GL_MAP_COHERENT_BIT_EXT
#define GL_MAP_COHERENT_BIT_EXT 0x0080
#endif
#ifndef GL_TIME_ELAPSED_EXT
#define GL_TIME_ELAPSED_EXT 0x88BF
#endif
#ifndef GL_GPU_DISJOINT_EXT
#define GL_GPU_DISJOINT_EXT 0x8FBB
#endif

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

GLuint Link(const std::string &vs, const std::string &fs, std::string *log, bool retrievable = false) {
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
  if (retrievable)
    glProgramParameteri(p, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
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

// Linked programs kept on disk (GxRenderer::SetProgramCache): one file per
// shader pair, named by a hash of both sources and the driver; a binary the
// driver no longer takes is linked anew.
struct ProgramFile {
  char magic[4];
  std::uint32_t format, bytes;
  std::uint64_t check; // a second hash of the sources
};

GLuint LoadProgram(const std::string &path, std::uint64_t check) {
  FILE *f = std::fopen(path.c_str(), "rb");
  if (!f)
    return 0;
  ProgramFile head{};
  std::vector<std::uint8_t> binary;
  bool ok = std::fread(&head, sizeof(head), 1, f) == 1 && std::memcmp(head.magic, "RE4P", 4) == 0 &&
            head.check == check && head.bytes > 0 && head.bytes < (64u << 20);
  if (ok) {
    binary.resize(head.bytes);
    ok = std::fread(binary.data(), 1, binary.size(), f) == binary.size();
  }
  std::fclose(f);
  if (!ok)
    return 0;
  GLuint p = glCreateProgram();
  glProgramBinary(p, head.format, binary.data(), static_cast<GLsizei>(binary.size()));
  GLint linked = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &linked);
  if (!linked) {
    glDeleteProgram(p);
    return 0;
  }
  return p;
}

void StoreProgram(const std::string &path, std::uint64_t check, GLuint p) {
  GLint bytes = 0;
  glGetProgramiv(p, GL_PROGRAM_BINARY_LENGTH, &bytes);
  if (bytes <= 0)
    return;
  std::vector<std::uint8_t> binary(static_cast<std::size_t>(bytes));
  GLenum format = 0;
  GLsizei written = 0;
  glGetProgramBinary(p, bytes, &written, &format, binary.data());
  if (written <= 0)
    return;
  const std::string temp = path + ".part";
  FILE *f = std::fopen(temp.c_str(), "wb");
  if (!f)
    return;
  ProgramFile head{{'R', 'E', '4', 'P'}, format, static_cast<std::uint32_t>(written), check};
  const bool ok = std::fwrite(&head, sizeof(head), 1, f) == 1 &&
                  std::fwrite(binary.data(), 1, static_cast<std::size_t>(written), f) == static_cast<std::size_t>(written);
  if (std::fclose(f) == 0 && ok)
    std::rename(temp.c_str(), path.c_str());
  else
    std::remove(temp.c_str());
}

struct Program {
  GLuint id = 0;
  GLint mtx = -1;
  mutable std::uint32_t lastMtx[3] = {~0u, ~0u, ~0u}; // uMtx as last set
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
  bool holdXfb = false;  // this chunk's display copy is not shown (RE4_GX_CHUNK_HOLD)
  GLuint vao = 0, vbo = 0, ibo = 0;
  GLuint xfUbo = 0, pxUbo = 0;
  GLuint copyProgram = 0, presentProgram = 0, paletteProgram = 0;
  GLint presentRows = -1;
  GLint copyMode = -1, copyRect = -1, copySize = -1, copyHalf = -1, copyGain = -1;
  GLint paletteSize = -1, paletteKind = -1, paletteFormat = -1;
  GLuint quadVao = 0;
  GLuint whiteTex = 0;
  std::unordered_map<ShaderKey, Program, KeyHash, KeyEqual> programs;
  std::unordered_map<std::uint64_t, Texture> textures;
  std::unordered_map<std::uint32_t, Copy> copies;  // by destination address
  std::unordered_map<std::uint64_t, Texture> paletted; // copies read through a palette
  std::unordered_map<std::uint64_t, GLuint> samplers;

  // --- what GL already has (per-draw work is skipped while it holds) --------
  bool keyDirty = true;              // a register that shapes the shader changed
  std::uint32_t keyLayout = ~0u;     // vertex layout of the cached key
  ShaderKey key = {};
  const Program *keyProgram = nullptr;
  bool pxDirty = true;               // a register of the PX block changed
  bool xfRegsDirty = true;           // colours, viewport or projection changed
  std::uint32_t stateSig[10] = {};   // ApplyState's registers as last applied
  std::string programCache;          // folder of linked programs ("" = none)
  std::string driverTag;             // GL renderer and version: binaries only for this driver
  bool stateValid = false;
  GLuint unitTex[8] = {}, unitSampler[8] = {};
  bool unitsValid = false;
  bool vaoBound = false;
  std::uint32_t attribMask = 0;      // enabled vertex attributes of vao
  std::uint32_t attribLayout = ~0u, attribStride = 0, attribBase = ~0u;
  // glDrawElementsBaseVertex (GLES 3.2 / OES / EXT): one attribute setup for
  // all draws of a layout; null: pointers per draw
  using DrawBaseVertex = void (*)(GLenum, GLsizei, GLenum, const void *, GLint);
  DrawBaseVertex drawBaseVertex = nullptr;

  // Other passes (uploads, copies, palettes, presenting) changed GL state.
  void Touched() {
    stateValid = false;
    unitsValid = false;
    vaoBound = false;
    current = nullptr;
  }

  // the texture bound to each map
  struct Bound {
    GLuint id = 0;
    unsigned width = 1, height = 1, levels = 1;
  } bound[8];

  // --- uniform stream ------------------------------------------------------
  // Every changed XF or PX block goes to a fresh place of one big buffer and is
  // bound there (glBindBufferRange), as Dolphin does: the driver never has to
  // update a block the GPU still reads. glBufferSubData on the two small
  // blocks did that about 1200 times in a village frame (r101); mobile drivers
  // copy the whole buffer for each such update. Persistent mapping
  // (EXT_buffer_storage) when the driver has it, else an unsynchronised map per
  // block; a fence guards each quarter of the ring before it is written again.
  // RE4_GX_UNIFORM_STREAM=0: the two small buffers as before.
  static constexpr unsigned kStreamParts = 4;
  static constexpr std::size_t kStreamPart = std::size_t(4) << 20;
  GLuint stream = 0;
  std::uint8_t *streamMap = nullptr; // persistent mapping (null: a map per block)
  std::size_t streamPos = 0, streamAlign = 256;
  unsigned streamPart = 0;
  GLsync streamFence[kStreamParts] = {};

  // Room for `bytes` in the stream: its offset.
  std::size_t StreamAlloc(std::size_t bytes) {
    std::size_t at = (streamPos + streamAlign - 1) / streamAlign * streamAlign;
    if (at + bytes > (streamPart + 1) * kStreamPart) {
      // the next quarter, once the GPU is done with what used it last
      streamFence[streamPart] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
      streamPart = (streamPart + 1) % kStreamParts;
      if (GLsync fence = streamFence[streamPart]) {
        if (glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0) == GL_TIMEOUT_EXPIRED) {
          stats.streamWaits++;
          glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 2000000000ull);
        }
        glDeleteSync(fence);
        streamFence[streamPart] = nullptr;
      }
      at = streamPart * kStreamPart;
    }
    streamPos = at + bytes;
    return at;
  }

  // A uniform block to its binding: in the stream, or into the small buffer.
  void UploadBlock(GLuint binding, GLuint buffer, const void *data, std::size_t offset, std::size_t bytes,
                   std::size_t blockBytes) {
    stats.uniformUploads++;
    if (!stream) {
      glBindBuffer(GL_UNIFORM_BUFFER, buffer);
      glBufferSubData(GL_UNIFORM_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(bytes),
                      static_cast<const std::uint8_t *>(data) + offset);
      stats.uniformBytes += static_cast<unsigned>(bytes);
      return;
    }
    // the stream holds whole blocks
    const std::size_t at = StreamAlloc(blockBytes);
    if (streamMap) {
      std::memcpy(streamMap + at, data, blockBytes);
    } else {
      glBindBuffer(GL_UNIFORM_BUFFER, stream);
      void *dst = glMapBufferRange(GL_UNIFORM_BUFFER, static_cast<GLintptr>(at), static_cast<GLsizeiptr>(blockBytes),
                                   GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
      if (dst) {
        std::memcpy(dst, data, blockBytes);
        glUnmapBuffer(GL_UNIFORM_BUFFER);
      } else {
        glBufferSubData(GL_UNIFORM_BUFFER, static_cast<GLintptr>(at), static_cast<GLsizeiptr>(blockBytes), data);
      }
    }
    glBindBufferRange(GL_UNIFORM_BUFFER, binding, stream, static_cast<GLintptr>(at),
                      static_cast<GLsizeiptr>(blockBytes));
    stats.uniformBytes += static_cast<unsigned>(blockBytes);
  }

  void StreamInit() {
    const char *off = std::getenv("RE4_GX_UNIFORM_STREAM");
    if (off && *off == '0')
      return;
    GLint align = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &align);
    streamAlign = static_cast<std::size_t>(std::max(16, align));
    const std::size_t size = kStreamParts * kStreamPart;
    glGenBuffers(1, &stream);
    glBindBuffer(GL_UNIFORM_BUFFER, stream);
    const char *ext = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
    using BufferStorage = void (*)(GLenum, GLsizeiptr, const void *, GLbitfield);
    BufferStorage storage = nullptr;
    if (ext && std::strstr(ext, "GL_EXT_buffer_storage")) {
      storage = reinterpret_cast<BufferStorage>(dlsym(RTLD_DEFAULT, "glBufferStorageEXT"));
      // not exported by every GLES library: then through EGL (loaded with the context)
      using GetProc = void *(*)(const char *);
      if (!storage)
        if (auto get = reinterpret_cast<GetProc>(dlsym(RTLD_DEFAULT, "eglGetProcAddress")))
          storage = reinterpret_cast<BufferStorage>(get("glBufferStorageEXT"));
    }
    const char *persistent = std::getenv("RE4_GX_PERSISTENT");
    if (storage && !(persistent && *persistent == '0')) {
      const GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_COHERENT_BIT_EXT;
      storage(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(size), nullptr, flags);
      streamMap = static_cast<std::uint8_t *>(glMapBufferRange(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(size), flags));
    }
    if (!streamMap) {
      glDeleteBuffers(1, &stream);
      glGenBuffers(1, &stream);
      glBindBuffer(GL_UNIFORM_BUFFER, stream);
      glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(size), nullptr, GL_DYNAMIC_DRAW);
    }
    GXLOG("GX uniform stream: %u MB, %s, offsets aligned to %u", static_cast<unsigned>(size >> 20),
          streamMap ? "persistent mapping" : "a map per block", static_cast<unsigned>(streamAlign));
  }

  void StreamRelease() {
    for (GLsync &f : streamFence) {
      if (f)
        glDeleteSync(f);
      f = nullptr;
    }
    if (stream) {
      if (streamMap) {
        glBindBuffer(GL_UNIFORM_BUFFER, stream);
        glUnmapBuffer(GL_UNIFORM_BUFFER);
      }
      glDeleteBuffers(1, &stream);
    }
    stream = 0;
    streamMap = nullptr;
  }

  // --- GPU time of each chunk (EXT_disjoint_timer_query) -----------------------
  static constexpr int kTimerQueries = 4;
  GLuint timerQuery[kTimerQueries] = {};
  bool timerPending[kTimerQueries] = {};
  int timerNext = 0;
  bool timerOn = false;

  void TimerInit() {
    const char *ext = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
    timerOn = ext && std::strstr(ext, "GL_EXT_disjoint_timer_query");
    if (timerOn)
      glGenQueries(kTimerQueries, timerQuery);
  }

  // Results that are ready go to the stats; then the query of this chunk starts.
  void TimerBegin() {
    if (!timerOn)
      return;
    GLint disjoint = 0;
    glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
    for (int i = 0; i < kTimerQueries; i++) {
      if (!timerPending[i])
        continue;
      GLuint ready = 0;
      glGetQueryObjectuiv(timerQuery[i], GL_QUERY_RESULT_AVAILABLE, &ready);
      if (!ready)
        continue;
      GLuint ns = 0;
      glGetQueryObjectuiv(timerQuery[i], GL_QUERY_RESULT, &ns);
      timerPending[i] = false;
      if (!disjoint) {
        stats.gpuMicros += ns / 1000;
        stats.gpuTimed++;
      }
    }
    if (timerPending[timerNext]) {
      timerNext = -1; // all still running: this chunk is not timed
      return;
    }
    glBeginQuery(GL_TIME_ELAPSED_EXT, timerQuery[timerNext]);
  }

  void TimerEnd() {
    if (!timerOn)
      return;
    if (timerNext >= 0) {
      glEndQuery(GL_TIME_ELAPSED_EXT);
      timerPending[timerNext] = true;
    }
    timerNext = 0;
    while (timerNext < kTimerQueries && timerPending[timerNext])
      timerNext++;
    if (timerNext == kTimerQueries)
      timerNext = 0;
  }

  // per-draw caches
  const Program *current = nullptr;
  XfBlock xf = {};
  PixelBlock px = {};
  PixelBlock pxSent = {};
  bool pxValid = false;
  std::uint32_t copySerial = 0;

  // --- state -----------------------------------------------------------------

  // The copy filter (BP 0x53/0x54, GXSetCopyFilter): seven vertical taps in
  // 1/64 that the GameCube applies to EFB copies. Their sum scales the colour:
  // the game's brightness option (main_sub.cpp Bg_brightness_set) raises or
  // lowers it around 64. The vertical blur of the taps (the deflicker filter
  // for the TV picture) is left out: the picture stays sharp.
  float CopyFilterGain() const {
    const std::uint32_t c0 = bp[0x53], c1 = bp[0x54];
    unsigned sum = 0;
    for (unsigned i = 0; i < 4; i++)
      sum += (c0 >> (6 * i)) & 63;
    for (unsigned i = 0; i < 3; i++)
      sum += (c1 >> (6 * i)) & 63;
    return sum == 0 ? 1.0f : static_cast<float>(sum) / 64.0f;
  }

  void BpWrite(std::uint32_t word, bool action) {
    const unsigned reg = word >> 24;
    const std::uint32_t value = word & 0xFFFFFF;
    if (bp[reg] != value || !action) {
      // registers the generated shader depends on (MakeKey)
      if (reg == 0x00 || (reg >= 0x10 && reg <= 0x1F) || (reg >= 0x27 && reg <= 0x2F) ||
          (reg >= 0xC0 && reg <= 0xDF) || reg == 0xF1 || reg == 0xF3 || (reg >= 0xF6 && reg <= 0xFD))
        keyDirty = true;
      // registers of the PX block (FillPixel)
      if ((reg >= 0x06 && reg <= 0x0E) || reg == 0x25 || reg == 0x26 || (reg >= 0x30 && reg <= 0x3F) ||
          (reg >= 0x80 && reg <= 0x87) || (reg >= 0xA0 && reg <= 0xA7) || (reg >= 0xE0 && reg <= 0xE7) ||
          (reg >= 0xEE && reg <= 0xF3))
        pxDirty = true;
    }
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
      if (a < 0x800) {
        xfMem[a] = words[i];
        XfMemToBlock(a);
      } else if (a >= 0x1000 && a < 0x1058) {
        const std::uint32_t r = a - 0x1000;
        xfReg[r] = words[i];
        if (r == 0x09 || (r >= 0x0E && r <= 0x12) || r == 0x26 || r >= 0x3F)
          keyDirty = true;
        xfRegsDirty = true;
      } else {
        continue;
      }
      DirtyAddress(a);
    }
  }

  // One word of XF memory into the XF block (FillXf for all of it).
  void XfMemToBlock(std::uint32_t a) {
    if (a < 0x100) {
      xf.posTex[a / 4][a % 4] = AsFloat(xfMem[a]);
    } else if (a >= 0x400 && a < 0x460) {
      xf.normal[(a - 0x400) / 3][(a - 0x400) % 3] = AsFloat(xfMem[a]);
    } else if (a >= 0x500 && a < 0x600) {
      xf.post[(a - 0x500) / 4][(a - 0x500) % 4] = AsFloat(xfMem[a]);
    } else if (a >= 0x600 && a < 0x680) {
      const unsigned l = (a - 0x600) / 16, w = (a - 0x600) % 16;
      if (w == 3) {
        for (int c = 0; c < 4; c++)
          xf.light[l][0][c] = static_cast<float>((xfMem[a] >> (24 - 8 * c)) & 255) / 255.0f;
      } else if (w >= 4) {
        xf.light[l][1 + (w - 4) / 3][(w - 4) % 3] = AsFloat(xfMem[a]);
      }
    }
  }

  // The XF registers of the block: material and ambient colours, viewport, projection.
  void FillXfRegs() {
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
    xfRegsDirty = false;
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
    Touched(); // binds the new texture
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
    Touched(); // its own pass
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
    pxDirty = true; // the map's size is in the PX block
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
      // a texture of the copy's width but another height reads the copy's
      // rows one to one, as the texels lie in memory (the heat haze over the
      // fires of r101 copies rows 56-447 and reads 336 of them as 256x168):
      // texel coordinates go by the copy's size, not by the texture's; scaled
      // to it, the rows below the picture (black) showed as a dark band
      if (it->second.width == b.width && it->second.height) {
        b.height = it->second.height;
      }
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
    Touched(); // a deleted texture is unbound
    auto it = textures.find(key);
    if (it != textures.end()) {
      glDeleteTextures(1, &it->second.id);
      textures.erase(it);
    }
  }

  void DropTextures() {
    Touched();
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
    // the registers below, as last applied: nothing to do while they hold
    const std::uint32_t sig[10] = {bp[0x20],     bp[0x21],     bp[0x59],       bp[0x40] & 0x1F, bp[0x41] & 0xFFFF,
                                   bp[0x43] & 7, (bp[0x00] >> 14) & 3, bp[0x22] & 0xFF, primitive, bp[0x42] & 0x1FF};
    if (stateValid && std::memcmp(sig, stateSig, sizeof(sig)) == 0)
      return;
    std::memcpy(stateSig, sig, sizeof(sig));
    stateValid = true;
    stats.stateChanges++;
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
    const bool alphaWrite = ((c >> 4) & 1) && alphaFormat, colorWrite = (c >> 3) & 1;
    glColorMask(colorWrite, colorWrite, colorWrite, alphaWrite);
    // destination alpha (GXSetDstAlpha, BP 0x42): the EFB alpha gets this
    // constant instead of the TEV alpha, which still drives the blending; the
    // shader writes it (PX alphaRef.z) unless the colour blending needs the
    // TEV alpha (colour written with SRC_ALPHA factors), then the alpha blend
    // factors make it (0 only: other values would need dual-source blending;
    // counted in Stats().dstAlphaApprox)
    const bool dstAlpha = alphaWrite && ((bp[0x42] >> 8) & 1);
    const int dstValue = static_cast<int>(bp[0x42] & 0xFF);
    int shaderAlpha = -1;
    if ((c >> 11) & 1) { // subtract: dst - src
      glEnable(GL_BLEND);
      glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, dstAlpha ? GL_FUNC_ADD : GL_FUNC_REVERSE_SUBTRACT);
      glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, dstAlpha ? GL_ZERO : GL_ONE);
      if (dstAlpha)
        shaderAlpha = dstValue;
    } else if (c & 1) {
      static const GLenum src[8] = {GL_ZERO,      GL_ONE,           GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
                                    GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA};
      static const GLenum dst[8] = {GL_ZERO,      GL_ONE,           GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
                                    GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA};
      const unsigned sf = (c >> 8) & 7, df = (c >> 5) & 7;
      glEnable(GL_BLEND);
      glBlendEquation(GL_FUNC_ADD);
      if (!dstAlpha) {
        glBlendFunc(src[sf], dst[df]);
      } else if (!colorWrite || (sf != 4 && sf != 5 && df != 4 && df != 5)) {
        glBlendFuncSeparate(src[sf], dst[df], GL_ONE, GL_ZERO); // TEV alpha not needed: the shader writes it
        shaderAlpha = dstValue;
      } else {
        glBlendFuncSeparate(src[sf], dst[df], GL_ZERO, GL_ZERO);
        if (dstValue != 0)
          stats.dstAlphaApprox++;
      }
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
      if (dstAlpha && op != 0 && op != 5 && op != 15)
        shaderAlpha = dstValue;
    } else {
      glDisable(GL_BLEND);
      if (dstAlpha)
        shaderAlpha = dstValue;
    }
    if (px.alphaRef[2] != shaderAlpha) {
      px.alphaRef[2] = shaderAlpha;
      pxDirty = true;
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
    std::string cachePath;
    std::uint64_t check = 0;
    if (!programCache.empty()) {
      std::uint64_t h = Fnv(driverTag.data(), driverTag.size());
      h = Fnv(vs.data(), vs.size(), h);
      h = Fnv(fs.data(), fs.size(), h ^ 0x5Au);
      check = Fnv(fs.data(), fs.size(), Fnv(vs.data(), vs.size(), 0x9E3779B97F4A7C15ull));
      char name[32];
      std::snprintf(name, sizeof(name), "/%016llx.bin", static_cast<unsigned long long>(h));
      cachePath = programCache + name;
      p.id = LoadProgram(cachePath, check);
      if (p.id)
        stats.programsLoaded++;
    }
    if (!p.id) {
      p.id = Link(vs, fs, &log, !cachePath.empty());
      if (p.id && !cachePath.empty())
        StoreProgram(cachePath, check, p.id);
    }
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

  // Vertex attribute pointers of vao for a layout (re4_gx_frame.h) and stride,
  // the first vertex at byte `base` of vbo; only changed enables are switched.
  void SetAttributes(std::uint32_t layout, std::uint32_t stride, std::uint32_t base) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    const GLsizei s = static_cast<GLsizei>(stride);
    const auto at = [&](unsigned offset) { return reinterpret_cast<const void *>(static_cast<std::uintptr_t>(base + offset)); };
    std::uint32_t mask = 1;
    unsigned off = 0;
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, s, at(off));
    off += 12;
    if (layout & RE4_GX_V_NORMAL) {
      mask |= 1u << 1;
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, s, at(off));
      off += 12;
      if (layout & RE4_GX_V_BINORMAL_TANGENT) {
        mask |= 3u << 2;
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, s, at(off));
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, s, at(off + 12));
        off += 24;
      }
    }
    for (int c = 0; c < 2; c++) {
      if (layout & (c == 0 ? RE4_GX_V_COLOR0 : RE4_GX_V_COLOR1)) {
        mask |= 1u << (4 + c);
        glVertexAttribPointer(4 + c, 4, GL_UNSIGNED_BYTE, GL_TRUE, s, at(off));
        off += 4;
      }
    }
    for (int t = 0; t < 8; t++) {
      if (layout & (RE4_GX_V_TEX0 << t)) {
        mask |= 1u << (6 + t);
        glVertexAttribPointer(6 + t, 2, GL_FLOAT, GL_FALSE, s, at(off));
        off += 8;
      }
    }
    if (layout & RE4_GX_V_MATRIX) {
      mask |= 1u << 14;
      glVertexAttribIPointer(14, 3, GL_UNSIGNED_INT, s, at(off));
    }
    for (GLuint a = 0; a < 15; a++) {
      const std::uint32_t bit = 1u << a;
      if ((mask & bit) && !(attribMask & bit))
        glEnableVertexAttribArray(a);
      else if (!(mask & bit) && (attribMask & bit))
        glDisableVertexAttribArray(a);
    }
    attribMask = mask;
    attribLayout = layout;
    attribStride = stride;
    attribBase = base;
  }

  void Draw(const Re4GxDraw &d, const Re4GxChunk &chunk) {
    if (d.indexCount == 0 || d.vertexOffset + static_cast<std::uint64_t>(d.vertexCount) * d.stride > chunk.vertexBytes ||
        d.indexOffset + d.indexCount > chunk.indexCount)
      return;
    // the shader: the one of the last draw while no register that shapes it changed
    if (keyDirty || d.layout != keyLayout) {
      key = MakeKey(d);
      keyProgram = ProgramFor(key);
      keyLayout = d.layout;
      keyDirty = false;
    }
    const Program *p = keyProgram;
    if (!p)
      return;
    const ShaderKey &k = key;
    const unsigned primitive = d.primitive & 0xFF;
    ApplyState(primitive);
    const float pointSize = static_cast<float>((bp[0x22] >> 8) & 0xFF) / 6.0f * static_cast<float>(scale);
    if (xf.proj[1][3] != pointSize) {
      xf.proj[1][3] = pointSize;
      Dirty(207, 208);
    }
    if (xfRegsDirty)
      FillXfRegs();
    if (dirtyLo < dirtyHi) {
      UploadBlock(kXfBinding, xfUbo, &xf, static_cast<std::size_t>(dirtyLo) * 16,
                  static_cast<std::size_t>(dirtyHi - dirtyLo) * 16, sizeof(XfBlock));
      dirtyLo = static_cast<int>(sizeof(XfBlock) / 16);
      dirtyHi = 0;
    }
    if (pxDirty) {
      FillPixel(k);
      pxDirty = false;
      if (!pxValid || std::memcmp(&px, &pxSent, sizeof(px)) != 0) {
        UploadBlock(kPixelBinding, pxUbo, &px, 0, sizeof(PixelBlock), sizeof(PixelBlock));
        pxSent = px;
        pxValid = true;
      }
    }
    if (current != p) {
      glUseProgram(p->id);
      current = p;
      stats.programSwitches++;
    }
    if (p->mtx >= 0 && (p->lastMtx[0] != d.matrices[0] || p->lastMtx[1] != d.matrices[1] ||
                        p->lastMtx[2] != d.matrices[2])) {
      glUniform3ui(p->mtx, d.matrices[0], d.matrices[1], d.matrices[2]);
      std::memcpy(p->lastMtx, d.matrices, sizeof(p->lastMtx));
    }
    if (!unitsValid) {
      for (int m = 0; m < 8; m++)
        unitTex[m] = unitSampler[m] = ~0u;
      unitsValid = true;
    }
    for (int m = 0; m < 8; m++) {
      if (!(k.texMaps & (1u << m)))
        continue;
      const GLuint id = bound[m].id ? bound[m].id : whiteTex;
      if (unitTex[m] != id) {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(m));
        glBindTexture(GL_TEXTURE_2D, id);
        unitTex[m] = id;
        stats.textureBinds++;
      }
      const std::uint32_t mode0 = bp[(m < 4 ? 0x80 : 0xA0) + (m & 3)], mode1 = bp[(m < 4 ? 0x84 : 0xA4) + (m & 3)];
      const GLuint sampler = Sampler(mode0, mode1, bound[m].levels);
      if (unitSampler[m] != sampler) {
        glBindSampler(static_cast<GLuint>(m), sampler);
        unitSampler[m] = sampler;
        stats.textureBinds++;
      }
    }
    // vertex attributes: with a base vertex the same setup serves every draw
    // of a layout and stride (the vertex block starts at base + n * stride)
    if (!vaoBound) {
      glBindVertexArray(vao);
      vaoBound = true;
    }
    std::uint32_t base = d.vertexOffset;
    GLint baseVertex = 0;
    if (drawBaseVertex && d.stride) {
      base = d.vertexOffset % d.stride;
      baseVertex = static_cast<GLint>(d.vertexOffset / d.stride);
    }
    if (d.layout != attribLayout || d.stride != attribStride || base != attribBase) {
      SetAttributes(d.layout, d.stride, base);
      stats.attributeSetups++;
    }
    const GLenum mode = primitive == RE4_GX_LINES ? GL_LINES : primitive == RE4_GX_POINTS ? GL_POINTS : GL_TRIANGLES;
    const void *indices = reinterpret_cast<const void *>(static_cast<std::uintptr_t>(d.indexOffset) * 2);
    if (drawBaseVertex)
      drawBaseVertex(mode, static_cast<GLsizei>(d.indexCount), GL_UNSIGNED_SHORT, indices, baseVertex);
    else
      glDrawElements(mode, static_cast<GLsizei>(d.indexCount), GL_UNSIGNED_SHORT, indices);
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
    Touched(); // its own pass
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
    if (toXfb && holdXfb) {
      // the VI stays on the picture before (RE4_GX_CHUNK_HOLD, SYS_SCREEN_STOP):
      // nothing to show, only the clear of the EFB
      stats.heldCopies++;
      SaveAndPlainState();
      glBindFramebuffer(GL_FRAMEBUFFER, efbFbo);
      ClearAfterCopy(control, x, y, w, h);
      return;
    }
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
    glUniform1f(copyGain, depth ? 1.0f : CopyFilterGain());
    glBindVertexArray(quadVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    current = nullptr;
    glBindFramebuffer(GL_FRAMEBUFFER, efbFbo);
    ClearAfterCopy(control, x, y, w, h);
  }

  // clear the copied region: colour if colour/alpha update is on, depth if
  // depth update is on (GXCopyDisp/GXCopyTex set the registers for it)
  void ClearAfterCopy(std::uint32_t control, int x, int y, int w, int h) {
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
        "uniform float gain;\n"
        "in vec2 vUv;\nout vec4 o;\n"
        "vec4 fetch(vec2 p) { return texture(src, p / size); }\n"
        "void main() {\n"
        "  vec2 p = rect.xy + vUv * rect.zw;\n"
        "  vec4 c = halfScale != 0 ? 0.25 * (fetch(p + vec2(-0.5, -0.5)) + fetch(p + vec2(0.5, -0.5)) + fetch(p + "
        "vec2(-0.5, 0.5)) + fetch(p + vec2(0.5, 0.5))) : fetch(p);\n"
        "  if (mode < 12) c.rgb = min(c.rgb * gain, vec3(1.0));\n"
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
    // rows: XFB texture rows shown (offset, span; 0..1), row 0 at the top
    static const char kPresentFs[] = "#version 300 es\nprecision highp float;\n"
                                     "uniform sampler2D src;\nuniform vec2 rows;\nin vec2 vUv;\nout vec4 o;\n"
                                     "void main() { o = vec4(texture(src, vec2(vUv.x, rows.x + (1.0 - vUv.y) * rows.y)).rgb, 1.0); }\n";
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
    copyGain = glGetUniformLocation(copyProgram, "gain");
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
    presentRows = glGetUniformLocation(presentProgram, "rows");
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
  // glDrawElementsBaseVertex: GLES 3.2, or the OES / EXT extension
  // (RE4_GX_BASEVERTEX=0 keeps the per-draw attribute setup)
  {
    const char *off = std::getenv("RE4_GX_BASEVERTEX");
    const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    const char *ext = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
    int major = 0, minor = 0;
    if (version)
      std::sscanf(version, "OpenGL ES %d.%d", &major, &minor);
    void *f = nullptr;
    if (!(off && *off == '0')) {
      if (major > 3 || (major == 3 && minor >= 2))
        f = dlsym(RTLD_DEFAULT, "glDrawElementsBaseVertex");
      if (!f && ext && std::strstr(ext, "GL_OES_draw_elements_base_vertex"))
        f = dlsym(RTLD_DEFAULT, "glDrawElementsBaseVertexOES");
      if (!f && ext && std::strstr(ext, "GL_EXT_draw_elements_base_vertex"))
        f = dlsym(RTLD_DEFAULT, "glDrawElementsBaseVertexEXT");
    }
    m.drawBaseVertex = reinterpret_cast<Impl::DrawBaseVertex>(f);
  }
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
  // the EFB as every display copy of the game leaves it: cleared to the copy
  // clear colour, black with alpha 0 (bio4_GXSetCopyClear). The shadow maps
  // (shadow.cpp) draw only the casters' alpha and rely on that 0 around them;
  // alpha 255 here made a single dumped frame shade the whole light cone.
  glClearColor(0, 0, 0, 0);
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
  m.StreamInit();
  m.TimerInit();
  return m.MakeUtilityPrograms(error);
}

void GxRenderer::Run(const Re4GxChunk &chunk) {
  Impl &m = *impl_;
  if (chunk.version != RE4_GX_FRAME_VERSION)
    return;
  m.stats.chunks++;
  m.Touched();
  m.holdXfb = (chunk.flags & RE4_GX_CHUNK_HOLD) != 0;
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
  if (m.stream) {
    // both blocks again into the stream at the first draw: the places bound
    // last may since have been written over
    m.Dirty(0, static_cast<int>(sizeof(XfBlock) / 16));
    m.pxDirty = true;
    m.pxValid = false;
  } else {
    glBindBufferBase(GL_UNIFORM_BUFFER, kXfBinding, m.xfUbo);
    glBindBufferBase(GL_UNIFORM_BUFFER, kPixelBinding, m.pxUbo);
  }
  m.TimerBegin();
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
  m.TimerEnd();
  // leave GL as the app's other renderers expect it
  glBindVertexArray(0);
  for (GLuint unit = 0; unit < 8; unit++)
    glBindSampler(unit, 0);
  glActiveTexture(GL_TEXTURE0);
  glUseProgram(0);
  m.Touched();
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

bool GxRenderer::Present(int x, int y, int w, int h, unsigned top, unsigned rows) {
  Impl &m = *impl_;
  if (!m.haveXfb)
    return false;
  m.SaveAndPlainState();
  glViewport(x, y, w, h);
  glUseProgram(m.presentProgram);
  const float lines = static_cast<float>(m.xfbHeight ? m.xfbHeight : 1);
  if (rows == 0 || top + rows > m.xfbHeight)
    glUniform2f(m.presentRows, 0.0f, 1.0f);
  else
    glUniform2f(m.presentRows, static_cast<float>(top) / lines, static_cast<float>(rows) / lines);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m.xfbTex);
  glBindSampler(0, 0);
  glBindVertexArray(m.quadVao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
  glUseProgram(0);
  m.Touched();
  return true;
}

GLuint GxRenderer::EfbColor() const { return impl_->efbColor; }

int GxRenderer::Scale() const { return impl_->scale; }

const GxStats &GxRenderer::Stats() const { return impl_->stats; }

bool GxRenderer::BaseVertex() const { return impl_->drawBaseVertex != nullptr; }

bool GxRenderer::SetProgramCache(const std::string &folder) {
  Impl &m = *impl_;
  m.programCache.clear();
  GLint formats = 0;
  glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &formats);
  if (folder.empty() || formats <= 0)
    return false;
  const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
  const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
  m.driverTag = std::string(renderer ? renderer : "") + "|" + (version ? version : "");
  m.programCache = folder;
  return true;
}

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
  m.StreamRelease();
  if (m.timerOn)
    glDeleteQueries(Impl::kTimerQueries, m.timerQuery);
  const GLuint vaos[] = {m.vao, m.quadVao};
  glDeleteVertexArrays(2, vaos);
  glDeleteProgram(m.copyProgram);
  glDeleteProgram(m.paletteProgram);
  glDeleteProgram(m.presentProgram);
  impl_.reset(new Impl);
}

} // namespace re4gx
