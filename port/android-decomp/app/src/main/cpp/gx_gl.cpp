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
#ifndef GL_MAX_SAMPLES_EXT
#define GL_MAX_SAMPLES_EXT 0x8D57
#endif
#ifndef GL_TEXTURE_SAMPLES_EXT
#define GL_TEXTURE_SAMPLES_EXT 0x8D6C
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

// `name` in the extension string `ext` as a whole word
bool HasExtension(const char *ext, const char *name) {
  if (!ext)
    return false;
  const std::size_t n = std::strlen(name);
  for (const char *at = std::strstr(ext, name); at; at = std::strstr(at + n, name))
    if ((at == ext || at[-1] == ' ') && (at[n] == ' ' || at[n] == '\0'))
      return true;
  return false;
}

// A function of a GLES extension: not exported by every GLES library, then through EGL (loaded
// with the context)
void *GlExtension(const char *name) {
  void *f = dlsym(RTLD_DEFAULT, name);
  using GetProc = void *(*)(const char *);
  if (!f)
    if (auto get = reinterpret_cast<GetProc>(dlsym(RTLD_DEFAULT, "eglGetProcAddress")))
      f = get(name);
  return f;
}

constexpr int kEfbWidth = 640;
constexpr int kEfbHeight = 528;
// GL_EXT_texture_filter_anisotropic (not in the GLES 3.0 headers)
constexpr GLenum kMaxAnisotropy = 0x84FE, kMaxAnisotropyLimit = 0x84FF;
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
  // multisampled EFB (GxOptions.samples > 1): the game draws into it; before
  // a copy out of the EFB the region is resolved into efbColor / efbDepth
  GLuint msaaFbo = 0, msaaColor = 0, msaaDepth = 0;
  // or the samples in the GPU's tile memory, on efbColor / efbDepth themselves (TiledMsaa)
  bool tiledMsaa = false;
  // GPU step d2: the second set of EFB textures on the tiles; each new pass draws into the other
  // set and first draws the EFB back from the one before (UseEfb)
  GLuint spareFbo = 0, spareColor = 0, spareDepth = 0;
  GLuint restoreProgram = 0;
  bool efbLeft = false;         // another framebuffer was bound since the EFB's
  int wantSamples = 1;          // the samples asked for (own buffers when the tiles fail their self-test)
  bool restorePending = false;  // the EFB is to be drawn back before it is drawn into

  GxOptions options;
  int samples = 1;
  float anisotropy = 1.0f;
  GLuint xfbFbo = 0, xfbTex = 0;
  unsigned xfbWidth = 0, xfbHeight = 0;
  bool haveXfb = false;
  bool holdXfb = false;  // this chunk's display copy is not shown (RE4_GX_CHUNK_HOLD)
  bool viBlack = false;  // the VI shows black (RE4_GX_CHUNK_BLACK, VISetBlack): Present draws black
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
  // the gameplay autofocus (Autofocus): made at its first marker
  GLuint focusProgram = 0, blurProgram = 0;
  GLint focusRegion = -1, focusSize = -1, focusZ = -1, focusPlayer = -1;
  GLint blurRegion = -1, blurSize = -1, blurZ = -1, blurRadius = -1;
  GLuint blurTex = 0, focusTex = 0, blurFbo = 0, focusFbo = 0;
  // GPU step e: the blur at half the resolution into halfTex, mixed into the EFB at the full one
  // (colorFbo: the EFB's colour without its depth, which the mix reads)
  GLuint halfProgram = 0, mixProgram = 0, halfTex = 0, halfFbo = 0, colorFbo = 0;
  GLint halfRegion = -1, halfSize = -1, halfZ = -1, halfRadius = -1;
  GLint mixRegion = -1, mixSize = -1, mixZ = -1;
  bool autofocusFailed = false;
  // the last perspective projection (XF 0x1024/0x1025, type 0 at 0x1026):
  // the scene's, for the distances behind the depths
  float perspA = 0.0f, perspB = 0.0f;
  bool havePerspective = false;

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
    if (ext && std::strstr(ext, "GL_EXT_buffer_storage"))
      storage = reinterpret_cast<BufferStorage>(GlExtension("glBufferStorageEXT"));
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

  // --- GPU time (EXT_disjoint_timer_query): the chunks and the presents -----------
  // A few queries in turn for each kind; a result is read when it is ready (frames later), and a
  // chunk or present whose queries are all still running is not timed. The two kinds never nest:
  // Present runs after Run.
  struct TimerPool {
    static constexpr int kQueries = 4;
    GLuint query[kQueries] = {};
    bool pending[kQueries] = {};
    int next = 0;

    // Results that are ready go to micros / timed (left out when the GPU's clock was disjoint).
    void Collect(unsigned long long *micros, unsigned *timed) {
      GLint disjoint = 0;
      glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
      for (int i = 0; i < kQueries; i++) {
        if (!pending[i])
          continue;
        GLuint ready = 0;
        glGetQueryObjectuiv(query[i], GL_QUERY_RESULT_AVAILABLE, &ready);
        if (!ready)
          continue;
        GLuint ns = 0;
        glGetQueryObjectuiv(query[i], GL_QUERY_RESULT, &ns);
        pending[i] = false;
        if (!disjoint) {
          *micros += ns / 1000;
          (*timed)++;
        }
      }
    }

    void Begin() {
      if (pending[next]) {
        next = -1; // all still running: this one is not timed
        return;
      }
      glBeginQuery(GL_TIME_ELAPSED_EXT, query[next]);
    }

    void End() {
      if (next >= 0) {
        glEndQuery(GL_TIME_ELAPSED_EXT);
        pending[next] = true;
      }
      next = 0;
      while (next < kQueries && pending[next])
        next++;
      if (next == kQueries)
        next = 0;
    }
  };
  TimerPool chunkTimer, presentTimer;
  bool timerOn = false;

  // --- how far the GPU is behind (fences, GxStats::behind*) ------------------------
  // The timer of a chunk can read far more than a picture's interval while the pictures keep
  // coming (Retroid, 2x MSAA with the autofocus, 19.10.2026: 450 ms at 30 pictures/s); a fence
  // after every chunk tells, independent of the timer, whether the GPU keeps up: after the next
  // chunk the fences not reached yet are chunks it has not finished.
  static constexpr int kFences = 8;
  GLsync fence[kFences] = {};
  int fenceNext = 0;

  void FenceAfterChunk() {
    unsigned behind = 0;
    for (GLsync &f : fence) {
      if (!f)
        continue;
      const GLenum r = glClientWaitSync(f, 0, 0);
      if (r == GL_TIMEOUT_EXPIRED) {
        behind++;
      } else {
        glDeleteSync(f);
        f = nullptr;
      }
    }
    stats.behindSum += behind;
    stats.behindChecks++;
    stats.behindCount[std::min(behind, 8u)]++;
    // kFences chunks behind: the oldest is no longer looked at (counted above)
    if (fence[fenceNext])
      glDeleteSync(fence[fenceNext]);
    fence[fenceNext] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    fenceNext = (fenceNext + 1) % kFences;
  }

  void FenceRelease() {
    for (GLsync &f : fence) {
      if (f)
        glDeleteSync(f);
      f = nullptr;
    }
  }

  void TimerInit() {
    const char *ext = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
    timerOn = ext && std::strstr(ext, "GL_EXT_disjoint_timer_query");
    if (timerOn) {
      glGenQueries(TimerPool::kQueries, chunkTimer.query);
      glGenQueries(TimerPool::kQueries, presentTimer.query);
    }
  }

  // Results that are ready go to the stats; then the query of this chunk starts.
  void TimerBegin() {
    if (!timerOn)
      return;
    chunkTimer.Collect(&stats.gpuMicros, &stats.gpuTimed);
    chunkTimer.Begin();
  }

  void TimerEnd() {
    if (timerOn)
      chunkTimer.End();
  }

  void PresentTimerBegin() {
    if (!timerOn)
      return;
    presentTimer.Collect(&stats.presentMicros, &stats.presentTimed);
    presentTimer.Begin();
  }

  void PresentTimerEnd() {
    if (timerOn)
      presentTimer.End();
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
    if (action && reg == 0x5A)
      Autofocus(value);
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
    bool projection = false;
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
        if (r >= 0x20 && r <= 0x26)
          projection = true;
        xfRegsDirty = true;
      } else {
        continue;
      }
      DirtyAddress(a);
    }
    if (projection && xfReg[0x26] == 0) {
      perspA = AsFloat(xfReg[0x24]);
      perspB = AsFloat(xfReg[0x25]);
      havePerspective = true;
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

  // the frame buffer the game draws into
  GLuint DrawFbo() const { return msaaFbo ? msaaFbo : efbFbo; }

  // GPU step d (19.10.2026): MSAA on the tiles (EXT_multisampled_render_to_texture2). The EFB's
  // own textures take `want` samples: the GPU keeps them in its tile memory and writes only the
  // resolved colour and depth out when a pass ends (an EFB copy, the autofocus, the picture's end);
  // the next pass reads them back into every sample, and Resolve has nothing to do. Separate
  // multisampled buffers instead were written out, resolved with a blit and read in again at every
  // copy (the Retroid, 19.10.2026: MSAA +74 % work, with the depth of field +114 %). What was
  // drawn before a copy keeps its resolved colour and depth, not its samples, after it. False:
  // the device cannot (efbFbo has its plain textures again). Not used unless asked for
  // (GxOptions::tiledMsaa): the extension leaves the samples undefined after a pass, and the
  // Adreno 650 then had the colour but not the depth (R3 capture, 19.10.2026). Since d2 a second
  // set of textures (spareFbo) and UseEfb draw the EFB back at the start of every new pass.
  bool TiledMsaa(int want) {
    const char *ext = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
    if (!HasExtension(ext, "GL_EXT_multisampled_render_to_texture") ||
        !HasExtension(ext, "GL_EXT_multisampled_render_to_texture2"))
      return false;
    using Attach = void (*)(GLenum, GLenum, GLenum, GLuint, GLint, GLsizei);
    const auto attach = reinterpret_cast<Attach>(GlExtension("glFramebufferTexture2DMultisampleEXT"));
    GLint most = 1;
    glGetIntegerv(GL_MAX_SAMPLES_EXT, &most);
    want = std::min(want, static_cast<int>(most));
    if (!attach || want < 2)
      return false;
    while (glGetError() != GL_NO_ERROR) {
    }
    glBindFramebuffer(GL_FRAMEBUFFER, efbFbo);
    attach(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, efbColor, 0, want);
    attach(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, efbDepth, 0, want);
    GLint got = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_SAMPLES_EXT, &got);
    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE && glGetError() == GL_NO_ERROR &&
              got > 1;
    // d2: the second set, the same samples
    if (ok) {
      const int w = kEfbWidth * scale, h = kEfbHeight * scale;
      MakeEfbTextures(w, h, &spareColor, &spareDepth);
      glGenFramebuffers(1, &spareFbo);
      glBindFramebuffer(GL_FRAMEBUFFER, spareFbo);
      attach(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, spareColor, 0, want);
      attach(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, spareDepth, 0, want);
      GLint spare = 0;
      glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_SAMPLES_EXT, &spare);
      ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE && glGetError() == GL_NO_ERROR &&
           spare == got;
      glBindFramebuffer(GL_FRAMEBUFFER, efbFbo);
    }
    if (ok) {
      samples = got;
      tiledMsaa = true;
      return true;
    }
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, efbColor, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, efbDepth, 0);
    const GLuint spareTextures[] = {spareColor, spareDepth};
    glDeleteTextures(2, spareTextures);
    glDeleteFramebuffers(1, &spareFbo);
    spareFbo = spareColor = spareDepth = 0;
    while (glGetError() != GL_NO_ERROR) {
    }
    return false;
  }

  // The EFB's colour (RGBA8, linear) and depth (24 bits, nearest) textures.
  static void MakeEfbTextures(int w, int h, GLuint *color, GLuint *depth) {
    glGenTextures(1, color);
    glBindTexture(GL_TEXTURE_2D, *color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenTextures(1, depth);
    glBindTexture(GL_TEXTURE_2D, *depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

  // Every binding of a framebuffer to draw into (GL_FRAMEBUFFER) goes through here. MSAA on the
  // tiles (d2): the EFB bound again after another framebuffer starts a new pass, and the
  // extension left its samples undefined; it is drawn back before it is next drawn into
  // (UseEfb). Until then it stays as it is, and copies, the autofocus and the tests read it.
  void BindFbo(GLuint fbo) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (fbo != efbFbo) {
      efbLeft = true;
      return;
    }
    if (efbLeft && tiledMsaa)
      restorePending = true;
    efbLeft = false;
  }

  // Before the EFB is drawn into or partly cleared (bound): with MSAA on the tiles after a pass
  // ended (d2), the new pass draws into the other set of textures and first draws the EFB back
  // into it from the set before (colour and depth, the same into every sample; what was drawn
  // before keeps its resolved colour and depth at the edges, not its samples). The GPU need not
  // load anything for the new pass (glInvalidateFramebuffer): the restore draws over all of it.
  void UseEfb() {
    if (!restorePending)
      return;
    restorePending = false;
    std::swap(efbFbo, spareFbo);
    std::swap(efbColor, spareColor);
    std::swap(efbDepth, spareDepth);
    glBindFramebuffer(GL_FRAMEBUFFER, efbFbo);
    SaveAndPlainState();
    const GLenum both[2] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT};
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 2, both);
    if (options.loseTiles >= 1) {
      glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
      glClearDepthf(0.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    if (options.loseTiles >= 2) {
      // a device that never writes the resolved depth into the texture: 0 there
      GLuint fbo = 0;
      glGenFramebuffers(1, &fbo);
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, spareDepth, 0);
      glClearDepthf(0.0f);
      glClear(GL_DEPTH_BUFFER_BIT);
      glBindFramebuffer(GL_FRAMEBUFFER, efbFbo);
      glDeleteFramebuffers(1, &fbo);
    }
    glViewport(0, 0, kEfbWidth * scale, kEfbHeight * scale);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glUseProgram(restoreProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, spareColor);
    glBindSampler(0, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, spareDepth);
    glBindSampler(1, 0);
    glBindVertexArray(quadVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_DEPTH_TEST);
    Touched();
    stats.efbRestores++;
  }

  // MSAA in separate multisampled buffers (msaaFbo), resolved into efbColor / efbDepth before
  // every copy; false when the device cannot (single-sampled then).
  bool OwnMsaa(int want) {
    const int w = kEfbWidth * scale, h = kEfbHeight * scale;
    while (glGetError() != GL_NO_ERROR) {
    }
    glGenRenderbuffers(1, &msaaColor);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaColor);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, want, GL_RGBA8, w, h);
    glGenRenderbuffers(1, &msaaDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaDepth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, want, GL_DEPTH_COMPONENT24, w, h);
    GLint got = 0;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &got);
    glGenFramebuffers(1, &msaaFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaColor);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaDepth);
    const bool ok =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE && glGetError() == GL_NO_ERROR && got > 1;
    if (ok) {
      samples = got;
      glViewport(0, 0, w, h);
      glClearColor(0, 0, 0, 0);
      glClearDepthf(1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else {
      glDeleteFramebuffers(1, &msaaFbo);
      glDeleteRenderbuffers(1, &msaaColor);
      glDeleteRenderbuffers(1, &msaaDepth);
      msaaFbo = msaaColor = msaaDepth = 0;
    }
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, efbFbo);
    return ok;
  }

  // MSAA on the tiles given up: the EFB's textures plain again, the second set gone.
  void DropTiles() {
    glBindFramebuffer(GL_FRAMEBUFFER, efbFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, efbColor, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, efbDepth, 0);
    const GLuint spareTextures[] = {spareColor, spareDepth};
    glDeleteTextures(2, spareTextures);
    glDeleteFramebuffers(1, &spareFbo);
    spareFbo = spareColor = spareDepth = 0;
    tiledMsaa = false;
    samples = 1;
    efbLeft = restorePending = false;
  }

  // GPU step d2 on this device (once, at Init): a pass on the tiles draws red at depth 0.25 into
  // an 8x8 corner and ends; what the device kept of it is read back (the resolved colour and
  // depth in the EFB's textures); the next pass draws the EFB back (UseEfb), then green at depth
  // 0.5 with LESS (must stay red: the depth came back) and blue at depth 0.25 with EQUAL (must
  // pass: the same depth exactly). False (logged): MSAA on the tiles does not keep the EFB here.
  // The Retroid's Adreno 650 showed Leon without hair with d and d2 (19.10.2026).
  bool TilesSelfTest() {
    static const char kVs[] = "#version 300 es\n"
                              "void main() {\n"
                              "  vec2 p = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));\n"
                              "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
                              "}\n";
    static const char kDrawFs[] = "#version 300 es\nprecision highp float;\n"
                                  "uniform vec4 color;\nuniform float depth;\nout vec4 o;\n"
                                  "void main() { o = color; gl_FragDepth = depth; }\n";
    static const char kProbeFs[] = "#version 300 es\nprecision highp float;\n"
                                   "uniform sampler2D color;\nuniform highp sampler2D depth;\nout vec4 o;\n"
                                   "void main() {\n"
                                   "  ivec2 p = ivec2(gl_FragCoord.xy);\n"
                                   "  o = vec4(texelFetch(color, p, 0).rgb, texelFetch(depth, p, 0).r);\n"
                                   "}\n";
    constexpr int n = 8;
    std::string log;
    const GLuint draw = Link(kVs, kDrawFs, &log), probe = Link(kVs, kProbeFs, &log);
    GLuint probeTex = 0, probeFbo = 0;
    glGenTextures(1, &probeTex);
    glBindTexture(GL_TEXTURE_2D, probeTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, n, n, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenFramebuffers(1, &probeFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, probeFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, probeTex, 0);
    if (!draw || !probe || glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      GXLOG("MSAA on the tiles, self-test not possible (%s); own buffers", log.c_str());
      glDeleteProgram(draw);
      glDeleteProgram(probe);
      glDeleteTextures(1, &probeTex);
      glDeleteFramebuffers(1, &probeFbo);
      return false;
    }
    SaveAndPlainState();
    glBindVertexArray(quadVao);
    for (int i = 0; i < 2; i++)
      glBindSampler(static_cast<GLuint>(i), 0);
    glUseProgram(probe);
    glUniform1i(glGetUniformLocation(probe, "color"), 0);
    glUniform1i(glGetUniformLocation(probe, "depth"), 1);
    glUseProgram(draw);
    const GLint color = glGetUniformLocation(draw, "color"), depth = glGetUniformLocation(draw, "depth");
    auto quad = [&](int x, int w, float r, float g, float b, float z, GLenum func) {
      glViewport(x, 0, w, n);
      glDepthFunc(func);
      glUniform4f(color, r, g, b, 1.0f);
      glUniform1f(depth, z);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    };
    // the corner's pixels as the EFB's textures hold them now (rgb, depth in a)
    std::uint8_t px[n * n * 4];
    auto read = [&]() {
      BindFbo(probeFbo);
      glDisable(GL_DEPTH_TEST);
      glViewport(0, 0, n, n);
      glUseProgram(probe);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, efbColor);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, efbDepth);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glReadPixels(0, 0, n, n, GL_RGBA, GL_UNSIGNED_BYTE, px);
      glUseProgram(draw);
    };
    auto at = [&](int x, int c) { return static_cast<int>(px[(4 * n + x) * 4 + c]); };
    // the first pass: red at depth 0.25
    efbLeft = false;
    BindFbo(efbFbo);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    quad(0, n, 1.0f, 0.0f, 0.0f, 0.25f, GL_ALWAYS);
    read();
    const bool colorKept = at(2, 0) > 240 && at(2, 1) < 16;
    const int storedDepth = at(2, 3);  // 0.25: 64
    const bool depthKept = std::abs(storedDepth - 64) <= 2;
    // the next pass: drawn back, then green behind (LESS) and blue at the same depth (EQUAL)
    BindFbo(efbFbo);
    UseEfb();
    glUseProgram(draw);
    glBindVertexArray(quadVao);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    quad(0, n / 2, 0.0f, 1.0f, 0.0f, 0.5f, GL_LESS);
    quad(n / 2, n / 2, 0.0f, 0.0f, 1.0f, 0.25f, GL_EQUAL);
    read();
    const bool behindHidden = at(1, 0) > 240 && at(1, 1) < 16;
    const bool equalPassed = at(6, 2) > 240 && at(6, 0) < 16;
    const bool ok = behindHidden && equalPassed;
    GXLOG("MSAA on the tiles, self-test: the device keeps the colour %s, the depth %s (read %d of 255, 64 "
          "expected); the EFB drawn back: what lies behind %s, the same depth %s -> %s",
          colorKept ? "yes" : "NO", depthKept ? "yes" : "NO", storedDepth,
          behindHidden ? "hidden" : "NOT hidden (the depth lost)", equalPassed ? "passes EQUAL" : "FAILS EQUAL",
          ok ? "MSAA on the tiles" : "own buffers instead");
    glDeleteProgram(draw);
    glDeleteProgram(probe);
    glDeleteTextures(1, &probeTex);
    glDeleteFramebuffers(1, &probeFbo);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    Touched();
    stats.efbRestores = 0;
    return ok;
  }

  // a multisampled EFB: the region (EFB pixels times the scale, with a pixel
  // around it for the copy filter) into the single-sample textures the
  // copies read
  void Resolve(int x, int y, int w, int h, bool depth) {
    if (!msaaFbo)
      return;
    const int fw = kEfbWidth * scale, fh = kEfbHeight * scale;
    const int x0 = std::max(0, x - scale), y0 = std::max(0, y - scale);
    const int x1 = std::min(fw, x + w + scale), y1 = std::min(fh, y + h + scale);
    if (x1 <= x0 || y1 <= y0)
      return;
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, efbFbo);
    glBlitFramebuffer(x0, y0, x1, y1, x0, y0, x1, y1, depth ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo);
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
    // anisotropic filtering (the menu's option): only where the game filters
    // linearly through mipmaps; point-sampled effects keep their look
    if (anisotropy > 1.0f && levels > 1 && ((mode0 >> 4) & 1) && minFilter >= 5 && minFilter <= 6)
      glSamplerParameterf(s, kMaxAnisotropy, anisotropy);
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
    BindFbo(fbo);
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
    BindFbo(DrawFbo());
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

  // The colour of the bound framebuffer is about to be drawn over completely (or only that part
  // is read later): the GPU may forget what it held. A tile-based GPU (Adreno, Mali) otherwise
  // reads it into its tiles before drawing (GxOptions::invalidate). Poisoned: magenta instead.
  void ForgetColor() {
    if (!options.invalidate)
      return;
    if (options.poison) {
      glDisable(GL_SCISSOR_TEST);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      return;
    }
    const GLenum color = GL_COLOR_ATTACHMENT0;
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &color);
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
    BindFbo(DrawFbo());
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
    UseEfb();
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
      BindFbo(DrawFbo());
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
    Resolve(x * scale, y * scale, w * scale, h * scale, depth);
    BindFbo(xfbFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
    SaveAndPlainState();
    // the copy draws over all of its target (the viewport is the texture)
    ForgetColor();
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
    BindFbo(DrawFbo());
    ClearAfterCopy(control, x, y, w, h);
  }

  // The app's gameplay autofocus (BP 0x5A: the core marks the place,
  // port/whole/runtime/dof.cpp, in the game's filter table after the scene
  // and before the interface): the blur of the first Android port
  // (depth_of_field_gl.cpp, RE4-Decomp) on the EFB. The distance at the centre
  // of the picture (median of nine depths) is the focus; what lies well
  // behind it gets soft (at most 60 %), nearer parts stay sharp, and only taps
  // behind the focus are mixed in, so the sharp foreground does not bleed into
  // the background. A focus beyond 6 m (looking into the distance) leaves the
  // picture as it is. Distances from the depths through the scene's
  // perspective projection and the XF viewport (GL depth = (zRange * (A + B /
  // distance) + zFar) / 2^24-1, see the vertex shader); the picture is the
  // XF viewport (the game's 512x448 at the top left of the EFB). `player`
  // (the marker's value): how deep Leon and his weapon reach into the
  // picture; only what lies deeper can be the focus (standing, the centre is
  // often on Leon): the nine depths at the centre, or, when fewer than five
  // of them lie behind Leon, 35 around it (15 % across, 12 % up and down);
  // none behind him: no blur.
  void Autofocus(std::uint32_t player) {
    if (autofocusFailed || !havePerspective)
      return;
    Touched(); // its own passes
    const int fw = kEfbWidth * scale, fh = kEfbHeight * scale;
    if (!blurProgram && !MakeAutofocus(fw, fh)) {
      autofocusFailed = true;
      GXLOG("gameplay autofocus could not be set up; left out");
      BindFbo(DrawFbo());
      return;
    }
    // the picture: the XF viewport, in EFB pixels
    const float halfW = AsFloat(xfReg[0x1A]), halfH = -AsFloat(xfReg[0x1B]);
    const float cx = AsFloat(xfReg[0x1D]) - 342.0f, cy = AsFloat(xfReg[0x1E]) - 342.0f;
    int x0 = static_cast<int>(std::lround(cx - halfW)), y0 = static_cast<int>(std::lround(cy - halfH));
    int x1 = static_cast<int>(std::lround(cx + halfW)), y1 = static_cast<int>(std::lround(cy + halfH));
    x0 = std::max(0, x0);
    y0 = std::max(0, y0);
    x1 = std::min(kEfbWidth, x1);
    y1 = std::min(kEfbHeight, y1);
    if (!(halfW > 0.0f && halfH > 0.0f) || x1 - x0 < 16 || y1 - y0 < 16) {
      x0 = y0 = 0;
      x1 = kEfbWidth;
      y1 = kEfbHeight;
    }
    const float rx = static_cast<float>(x0 * scale), ry = static_cast<float>(y0 * scale);
    const float rw = static_cast<float>((x1 - x0) * scale), rh = static_cast<float>((y1 - y0) * scale);
    float zRange = AsFloat(xfReg[0x1C]);
    if (!(std::fabs(zRange) > 1.0f))
      zRange = 16777215.0f;
    const float zFar = AsFloat(xfReg[0x1F]);
    // only the picture is read (the focus, the blur's taps stay inside it)
    Resolve(x0 * scale, y0 * scale, (x1 - x0) * scale, (y1 - y0) * scale, false);
    Resolve(x0 * scale, y0 * scale, (x1 - x0) * scale, (y1 - y0) * scale, true);
    SaveAndPlainState();
    glDepthMask(GL_FALSE);
    glBindVertexArray(quadVao);
    for (int i = 0; i < 3; i++)
      glBindSampler(i, 0);
    // the focus into one texel: distance (16 bits in r, g), strength (b)
    BindFbo(focusFbo);
    ForgetColor();  // its one texel is drawn over
    glViewport(0, 0, 1, 1);
    glUseProgram(focusProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, efbDepth);
    glUniform4f(focusRegion, rx, ry, rw, rh);
    glUniform2f(focusSize, static_cast<float>(fw), static_cast<float>(fh));
    glUniform4f(focusZ, perspA, perspB, zRange, zFar);
    glUniform1f(focusPlayer, static_cast<float>(player));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    // the first port's tap distance: 1.5 pixels of a 480-line picture
    const float radius = std::min(4.0f, std::max(1.0f, rh / 480.0f * 1.5f));
    if (options.halfDof && halfProgram) {
      // GPU step e: the blur at half the resolution (a quarter of the pixels, each of them the
      // four EFB texels around its centre and the taps behind the focus)
      const int hx0 = x0 * scale / 2, hy0 = y0 * scale / 2;
      const int hx1 = (x1 * scale + 1) / 2, hy1 = (y1 * scale + 1) / 2;
      BindFbo(halfFbo);
      ForgetColor();  // only the picture's region is drawn, and the mix reads only it
      glViewport(hx0, hy0, hx1 - hx0, hy1 - hy0);
      glUseProgram(halfProgram);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, efbColor);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, efbDepth);
      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D, focusTex);
      glUniform4f(halfRegion, rx, ry, rw, rh);
      glUniform2f(halfSize, static_cast<float>(fw), static_cast<float>(fh));
      glUniform4f(halfZ, perspA, perspB, zRange, zFar);
      glUniform1f(halfRadius, radius);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      // mixed into the EFB at the full resolution: each pixel's own distance says how much of the
      // blur (sharp in front of the focus: nothing drawn), blended over the picture; colour only,
      // alpha and depth stay. The mix reads the depth texture, so not through efbFbo, whose depth
      // it is (the multisampled EFB has its own). On the tiles (d2) the EFB's textures alternate:
      // colorFbo takes the current one, which the next pass draws back
      BindFbo(msaaFbo ? msaaFbo : colorFbo);
      if (!msaaFbo)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, efbColor, 0);
      glViewport(x0 * scale, y0 * scale, (x1 - x0) * scale, (y1 - y0) * scale);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
      glEnable(GL_BLEND);
      glBlendEquation(GL_FUNC_ADD);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glUseProgram(mixProgram);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, halfTex);
      glUniform4f(mixRegion, rx, ry, rw, rh);
      glUniform2f(mixSize, static_cast<float>(fw), static_cast<float>(fh));
      glUniform4f(mixZ, perspA, perspB, zRange, zFar);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glDisable(GL_BLEND);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      BindFbo(DrawFbo());
      current = nullptr;
      stats.autofocus++;
      return;
    }
    // the blurred picture into blurTex
    BindFbo(blurFbo);
    ForgetColor();  // only the picture's region is drawn, and only it is copied back
    glViewport(x0 * scale, y0 * scale, (x1 - x0) * scale, (y1 - y0) * scale);
    glUseProgram(blurProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, efbColor);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, efbDepth);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, focusTex);
    glUniform4f(blurRegion, rx, ry, rw, rh);
    glUniform2f(blurSize, static_cast<float>(fw), static_cast<float>(fh));
    glUniform4f(blurZ, perspA, perspB, zRange, zFar);
    glUniform1f(blurRadius, radius);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    // back into the EFB: colour only, alpha and depth stay
    BindFbo(DrawFbo());
    UseEfb();
    glViewport(x0 * scale, y0 * scale, (x1 - x0) * scale, (y1 - y0) * scale);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glUseProgram(copyProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blurTex);
    glUniform1i(copyMode, 0);
    glUniform4f(copyRect, rx, ry, rw, rh);
    glUniform2f(copySize, static_cast<float>(fw), static_cast<float>(fh));
    glUniform1i(copyHalf, 0);
    glUniform1f(copyGain, 1.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    current = nullptr;
    stats.autofocus++;
  }

  bool MakeAutofocus(int fw, int fh) {
    static const char kQuadVs[] = "#version 300 es\n"
                                  "out vec2 vUv;\n"
                                  "void main() {\n"
                                  "  vec2 p = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));\n"
                                  "  vUv = p;\n"
                                  "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
                                  "}\n";
    // region: the picture in EFB texels (x, y, w, h); size: the EFB texture;
    // zmap: projection A, B, viewport zRange, zFar
    static const char kDistance[] =
        "uniform highp sampler2D depths;\nuniform vec4 region;\nuniform vec2 size;\nuniform vec4 zmap;\n"
        "float distanceAt(vec2 t) {\n"
        "  float q = (texture(depths, t / size).r * 16777215.0 - zmap.w) / zmap.z + zmap.x;\n"
        "  return q < 0.0 ? zmap.y / q : 1.0e9;\n"
        "}\n";
    const std::string focusFs =
        std::string("#version 300 es\nprecision highp float;\nprecision highp int;\n") + kDistance +
        "uniform float player;\nout vec4 o;\n"
        "void main() {\n"
        "  float z[35];\n"
        "  for (int i = 0; i < 35; ++i) z[i] = 0.0;\n"
        "  int n = 0;\n"
        "  vec2 c = region.xy + 0.5 * region.zw;\n"
        "  for (int y = -1; y <= 1; ++y)\n"
        "    for (int x = -1; x <= 1; ++x) {\n"
        "      float d = distanceAt(c + vec2(float(x), float(y)) * 0.006 * region.zw);\n"
        "      if (d > player) z[n++] = d;\n"
        "    }\n"
        "  if (n < 5) {\n"
        "    n = 0;\n"
        "    for (int y = -2; y <= 2; ++y)\n"
        "      for (int x = -3; x <= 3; ++x) {\n"
        "        float d = distanceAt(c + vec2(float(x) * 0.05, float(y) * 0.06) * region.zw);\n"
        "        if (d > player) z[n++] = d;\n"
        "      }\n"
        "  }\n"
        "  if (n == 0) { o = vec4(0.0); return; }\n"
        "  for (int i = 0; i < 34; ++i)\n"
        "    for (int j = i + 1; j < 35; ++j)\n"
        "      if (j < n && z[j] < z[i]) { float t = z[i]; z[i] = z[j]; z[j] = t; }\n"
        "  float f = z[n / 2];\n"
        "  uint d = uint(clamp(round(f), 0.0, 65535.0));\n"
        "  float amount = 1.0 - smoothstep(3000.0, 6000.0, f);\n"
        "  o = vec4(float(d >> 8u) / 255.0, float(d & 255u) / 255.0, amount, 1.0);\n"
        "}\n";
    const std::string blurFs =
        std::string("#version 300 es\nprecision highp float;\nprecision highp int;\n") + kDistance +
        "uniform sampler2D frame;\nuniform sampler2D focus;\nuniform float radius;\nout vec4 o;\n"
        "void main() {\n"
        "  vec2 t = gl_FragCoord.xy;\n"
        "  vec4 original = texelFetch(frame, ivec2(t), 0);\n"
        "  vec4 f = texelFetch(focus, ivec2(0), 0);\n"
        "  float focal = round(f.r * 255.0) * 256.0 + round(f.g * 255.0);\n"
        "  float start = focal + max(800.0, focal * 0.25), end = focal + max(2500.0, focal);\n"
        "  float amount = 0.6 * f.b * smoothstep(start, end, distanceAt(t));\n"
        "  if (amount <= 0.0) { o = original; return; }\n"
        "  vec3 sum = original.rgb * 2.0;\n"
        "  float weight = 2.0;\n"
        "  const vec2 taps[8] = vec2[8](vec2(-1, 0), vec2(1, 0), vec2(0, -1), vec2(0, 1), vec2(-0.707, -0.707),\n"
        "                               vec2(0.707, 0.707), vec2(-0.707, 0.707), vec2(0.707, -0.707));\n"
        "  vec2 lo = region.xy + 0.5, hi = region.xy + region.zw - 0.5;\n"
        "  for (int i = 0; i < 8; ++i) {\n"
        "    vec2 p = clamp(t + taps[i] * radius, lo, hi);\n"
        "    if (distanceAt(p) >= start) { sum += texture(frame, p / size).rgb; weight += 1.0; }\n"
        "  }\n"
        "  o = vec4(mix(original.rgb, sum / weight, amount), original.a);\n"
        "}\n";
    // GPU step e: the blur above at half the resolution, without the mix. Its centre is the four
    // EFB texels under the texel (together weighted as the one above), the taps as above, and only
    // what lies behind the focus counts, the centre too: the sharp foreground does not bleed into
    // the background through the coarser texels. Alpha: something behind the focus was there
    // (1, the colour as it is) or not (0, black), so the mix can leave the latter out.
    const std::string halfFs =
        std::string("#version 300 es\nprecision highp float;\nprecision highp int;\n") + kDistance +
        "uniform sampler2D frame;\nuniform sampler2D focus;\nuniform float radius;\nout vec4 o;\n"
        "void main() {\n"
        "  vec2 t = gl_FragCoord.xy * 2.0;\n"
        "  vec4 f = texelFetch(focus, ivec2(0), 0);\n"
        "  if (f.b <= 0.0) { o = vec4(0.0); return; }\n"
        "  float focal = round(f.r * 255.0) * 256.0 + round(f.g * 255.0);\n"
        "  float start = focal + max(800.0, focal * 0.25);\n"
        "  vec2 lo = region.xy + 0.5, hi = region.xy + region.zw - 0.5;\n"
        "  vec3 sum = vec3(0.0);\n"
        "  float weight = 0.0;\n"
        "  for (int i = 0; i < 4; ++i) {\n"
        "    vec2 p = clamp(t + vec2(float(i & 1) - 0.5, float(i >> 1) - 0.5), lo, hi);\n"
        "    if (distanceAt(p) >= start) { sum += texture(frame, p / size).rgb * 0.5; weight += 0.5; }\n"
        "  }\n"
        "  const vec2 taps[8] = vec2[8](vec2(-1, 0), vec2(1, 0), vec2(0, -1), vec2(0, 1), vec2(-0.707, -0.707),\n"
        "                               vec2(0.707, 0.707), vec2(-0.707, 0.707), vec2(0.707, -0.707));\n"
        "  for (int i = 0; i < 8; ++i) {\n"
        "    vec2 p = clamp(t + taps[i] * radius, lo, hi);\n"
        "    if (distanceAt(p) >= start) { sum += texture(frame, p / size).rgb; weight += 1.0; }\n"
        "  }\n"
        "  o = weight > 0.0 ? vec4(sum / weight, 1.0) : vec4(0.0);\n"
        "}\n";
    // ... and the mix at the full resolution: each pixel's strength from its own distance, the
    // blur from halfTex (linear, inside the region; its colour divided by its alpha leaves the
    // texels without a background out), blended over the picture by the strength, less where
    // the texels around are mostly foreground
    const std::string mixFs =
        std::string("#version 300 es\nprecision highp float;\nprecision highp int;\n") + kDistance +
        "uniform sampler2D blurred;\nuniform sampler2D focus;\nout vec4 o;\n"
        "void main() {\n"
        "  vec2 t = gl_FragCoord.xy;\n"
        "  vec4 f = texelFetch(focus, ivec2(0), 0);\n"
        "  if (f.b <= 0.0) discard;\n"
        "  float focal = round(f.r * 255.0) * 256.0 + round(f.g * 255.0);\n"
        "  float start = focal + max(800.0, focal * 0.25), end = focal + max(2500.0, focal);\n"
        "  float amount = 0.6 * f.b * smoothstep(start, end, distanceAt(t));\n"
        "  if (amount <= 0.0) discard;\n"
        "  vec2 lo = region.xy + 1.0, hi = region.xy + region.zw - 1.0;\n"
        "  vec4 b = texture(blurred, clamp(t, lo, hi) / size);\n"
        "  if (b.a <= 0.0) discard;\n"
        "  o = vec4(b.rgb / b.a, amount * min(1.0, b.a * 4.0));\n"
        "}\n";
    std::string log;
    focusProgram = Link(kQuadVs, focusFs, &log);
    blurProgram = focusProgram ? Link(kQuadVs, blurFs, &log) : 0;
    halfProgram = blurProgram ? Link(kQuadVs, halfFs, &log) : 0;
    mixProgram = halfProgram ? Link(kQuadVs, mixFs, &log) : 0;
    if (!mixProgram) {
      GXLOG("autofocus shader: %s", log.c_str());
      return false;
    }
    glUseProgram(halfProgram);
    glUniform1i(glGetUniformLocation(halfProgram, "frame"), 0);
    glUniform1i(glGetUniformLocation(halfProgram, "depths"), 1);
    glUniform1i(glGetUniformLocation(halfProgram, "focus"), 2);
    halfRegion = glGetUniformLocation(halfProgram, "region");
    halfSize = glGetUniformLocation(halfProgram, "size");
    halfZ = glGetUniformLocation(halfProgram, "zmap");
    halfRadius = glGetUniformLocation(halfProgram, "radius");
    glUseProgram(mixProgram);
    glUniform1i(glGetUniformLocation(mixProgram, "blurred"), 0);
    glUniform1i(glGetUniformLocation(mixProgram, "depths"), 1);
    glUniform1i(glGetUniformLocation(mixProgram, "focus"), 2);
    mixRegion = glGetUniformLocation(mixProgram, "region");
    mixSize = glGetUniformLocation(mixProgram, "size");
    mixZ = glGetUniformLocation(mixProgram, "zmap");
    glUseProgram(focusProgram);
    glUniform1i(glGetUniformLocation(focusProgram, "depths"), 0);
    focusRegion = glGetUniformLocation(focusProgram, "region");
    focusSize = glGetUniformLocation(focusProgram, "size");
    focusZ = glGetUniformLocation(focusProgram, "zmap");
    focusPlayer = glGetUniformLocation(focusProgram, "player");
    glUseProgram(blurProgram);
    glUniform1i(glGetUniformLocation(blurProgram, "frame"), 0);
    glUniform1i(glGetUniformLocation(blurProgram, "depths"), 1);
    glUniform1i(glGetUniformLocation(blurProgram, "focus"), 2);
    blurRegion = glGetUniformLocation(blurProgram, "region");
    blurSize = glGetUniformLocation(blurProgram, "size");
    blurZ = glGetUniformLocation(blurProgram, "zmap");
    blurRadius = glGetUniformLocation(blurProgram, "radius");
    glGenTextures(1, &blurTex);
    glBindTexture(GL_TEXTURE_2D, blurTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fw, fh, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenTextures(1, &focusTex);
    glBindTexture(GL_TEXTURE_2D, focusTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &blurFbo);
    BindFbo(blurFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blurTex, 0);
    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glGenFramebuffers(1, &focusFbo);
    BindFbo(focusFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, focusTex, 0);
    ok = ok && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    // GPU step e: the half-resolution blur (linear: the mix reads between its texels) and the EFB's
    // colour without its depth
    glGenTextures(1, &halfTex);
    glBindTexture(GL_TEXTURE_2D, halfTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fw / 2, fh / 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &halfFbo);
    BindFbo(halfFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, halfTex, 0);
    ok = ok && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glGenFramebuffers(1, &colorFbo);
    BindFbo(colorFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, efbColor, 0);
    ok = ok && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!ok)
      GXLOG("autofocus framebuffers incomplete");
    return ok;
  }

  // clear the copied region: colour if colour/alpha update is on, depth if
  // depth update is on (GXCopyDisp/GXCopyTex set the registers for it)
  void ClearAfterCopy(std::uint32_t control, int x, int y, int w, int h) {
    if ((control >> 11) & 1) {
      const std::uint32_t c = bp[0x41];
      const bool color = (c >> 3) & 1, alpha = (c >> 4) & 1, zupdate = (bp[0x40] >> 4) & 1;
      if (color || alpha || zupdate)
        UseEfb();  // MSAA on the tiles: the EFB back first, the clear is only the copy's region
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
    // d2: the EFB drawn back from the other set of textures, colour and depth texel for texel
    // (the depth as sampled, no arithmetic: the 24 bits come back as they were)
    static const char kRestoreFs[] = "#version 300 es\nprecision highp float;\n"
                                     "uniform sampler2D color;\nuniform highp sampler2D depth;\nout vec4 o;\n"
                                     "void main() {\n"
                                     "  ivec2 p = ivec2(gl_FragCoord.xy);\n"
                                     "  o = texelFetch(color, p, 0);\n"
                                     "  gl_FragDepth = texelFetch(depth, p, 0).r;\n"
                                     "}\n";
    restoreProgram = Link(kQuadVs, kRestoreFs, &log);
    if (!restoreProgram) {
      *error = "EFB restore shader: " + log;
      return false;
    }
    glUseProgram(restoreProgram);
    glUniform1i(glGetUniformLocation(restoreProgram, "color"), 0);
    glUniform1i(glGetUniformLocation(restoreProgram, "depth"), 1);
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
  // once per process: the GPU and what step d (MSAA on the tiles) needs of it
  static bool described = false;
  if (!described) {
    described = true;
    const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    const char *ext = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
    auto has = [ext](const char *name) { return HasExtension(ext, name) ? "yes" : "no"; };
    GXLOG("GPU: %s, %s; EXT_multisampled_render_to_texture %s, ..._texture2 %s, "
          "EXT_shader_framebuffer_fetch %s, QCOM_tiled_rendering %s",
          renderer ? renderer : "?", version ? version : "?", has("GL_EXT_multisampled_render_to_texture"),
          has("GL_EXT_multisampled_render_to_texture2"), has("GL_EXT_shader_framebuffer_fetch"),
          has("GL_QCOM_tiled_rendering"));
    // all of them, in pieces a log line holds
    const std::string all = ext ? ext : "";
    const std::size_t piece = 800, pieces = (all.size() + piece - 1) / piece;
    for (std::size_t i = 0; i < pieces; i++)
      GXLOG("GPU extensions (%zu/%zu): %s", i + 1, pieces, all.substr(i * piece, piece).c_str());
  }
  const int w = kEfbWidth * m.scale, h = kEfbHeight * m.scale;
  Impl::MakeEfbTextures(w, h, &m.efbColor, &m.efbDepth);
  glGenFramebuffers(1, &m.efbFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, m.efbFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m.efbColor, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m.efbDepth, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    *error = "EFB framebuffer incomplete";
    return false;
  }
  // the menu's options, as far as the device goes
  {
    int maxSamples = 1;
    float maxAnisotropy = 1.0f;
    Limits(&maxSamples, &maxAnisotropy);
    m.anisotropy = std::max(1.0f, std::min(m.options.anisotropy, maxAnisotropy));
    const int want = std::min(m.options.samples, maxSamples);
    // RE4_GX_DOF_HALF=0 / 1: the depth of field's blur at the full or half resolution (experiments)
    const char *halfDof = std::getenv("RE4_GX_DOF_HALF");
    if (halfDof && (*halfDof == '0' || *halfDof == '1'))
      m.options.halfDof = *halfDof == '1';
    // RE4_GX_MSAA_TILES=1 / 0: on the tiles or not, whatever the options say (experiments)
    const char *tiles = std::getenv("RE4_GX_MSAA_TILES");
    const bool tiled = tiles && (*tiles == '0' || *tiles == '1') ? *tiles == '1' : m.options.tiledMsaa;
    m.wantSamples = want;
    if (want > 1 && tiled)
      m.TiledMsaa(want);
    if (want > 1 && !m.tiledMsaa)
      m.OwnMsaa(want);
  }
  glViewport(0, 0, w, h);
  // the EFB as every display copy of the game leaves it: cleared to the copy
  // clear colour, black with alpha 0 (bio4_GXSetCopyClear). The shadow maps
  // (shadow.cpp) draw only the casters' alpha and rely on that 0 around them;
  // alpha 255 here made a single dumped frame shade the whole light cone.
  glClearColor(0, 0, 0, 0);
  glClearDepthf(1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (m.spareFbo) {
    // d2: the other set the same, the first pass draws the EFB back from one of them
    glBindFramebuffer(GL_FRAMEBUFFER, m.spareFbo);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, m.efbFbo);
  }
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
  if (!m.MakeUtilityPrograms(error))
    return false;
  // d2: does this device keep the EFB over a pass's end, drawn back? Else own buffers
  if (m.tiledMsaa && !m.TilesSelfTest()) {
    m.DropTiles();
    m.OwnMsaa(m.wantSamples);
  }
  // both sets (or the plain EFB) as the game's display copies leave them
  for (GLuint fbo : {m.efbFbo, m.spareFbo}) {
    if (!fbo)
      continue;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 0);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, m.DrawFbo());
  m.efbLeft = m.restorePending = false;
  return true;
}

void GxRenderer::Run(const Re4GxChunk &chunk) {
  Impl &m = *impl_;
  if (chunk.version != RE4_GX_FRAME_VERSION)
    return;
  m.stats.chunks++;
  m.Touched();
  m.holdXfb = (chunk.flags & RE4_GX_CHUNK_HOLD) != 0;
  if (chunk.flags & RE4_GX_CHUNK_XFB)
    m.viBlack = (chunk.flags & RE4_GX_CHUNK_BLACK) != 0;
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
  // the app drew the screen since the last chunk: a new pass
  m.efbLeft = true;
  m.BindFbo(m.DrawFbo());
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
  m.FenceAfterChunk();
  // leave GL as the app's other renderers expect it
  glBindVertexArray(0);
  for (GLuint unit = 0; unit < 8; unit++)
    glBindSampler(unit, 0);
  glActiveTexture(GL_TEXTURE0);
  glUseProgram(0);
  m.Touched();
  m.SaveAndPlainState();
  m.BindFbo(0);
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
  m.stats.presents++;
  m.PresentTimerBegin();
  m.SaveAndPlainState();
  if (m.viBlack) {
    // VISetBlack: the GameCube's video output is black, whatever the XFB holds
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    m.Touched();
    m.PresentTimerEnd();
    return true;
  }
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
  m.PresentTimerEnd();
  return true;
}

GLuint GxRenderer::EfbColor() const { return impl_->efbColor; }

void GxRenderer::SetOptions(const GxOptions &options) { impl_->options = options; }

int GxRenderer::Samples() const { return impl_->samples; }

float GxRenderer::Anisotropy() const { return impl_->anisotropy; }

bool GxRenderer::Invalidates() const { return impl_->options.invalidate; }

bool GxRenderer::TiledMsaa() const { return impl_->tiledMsaa; }

bool GxRenderer::HalfDof() const { return impl_->options.halfDof; }

void GxRenderer::Limits(int *maxSamples, float *maxAnisotropy) {
  GLint samples = 1;
  glGetIntegerv(GL_MAX_SAMPLES, &samples);
  *maxSamples = std::max(1, static_cast<int>(samples));
  *maxAnisotropy = 1.0f;
  const char *ext = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
  if (ext && std::strstr(ext, "GL_EXT_texture_filter_anisotropic")) {
    GLfloat most = 1.0f;
    glGetFloatv(kMaxAnisotropyLimit, &most);
    *maxAnisotropy = std::max(1.0f, static_cast<float>(most));
  }
}

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
  glDeleteProgram(m.restoreProgram);
  const GLuint spareTextures[] = {m.spareColor, m.spareDepth};
  glDeleteTextures(2, spareTextures);
  glDeleteFramebuffers(1, &m.spareFbo);
  const GLuint textures[] = {m.efbColor, m.efbDepth, m.xfbTex, m.whiteTex};
  glDeleteTextures(4, textures);
  const GLuint fbos[] = {m.efbFbo, m.xfbFbo, m.msaaFbo};
  glDeleteFramebuffers(3, fbos);
  const GLuint renderbuffers[] = {m.msaaColor, m.msaaDepth};
  glDeleteRenderbuffers(2, renderbuffers);
  const GLuint buffers[] = {m.vbo, m.ibo, m.xfUbo, m.pxUbo};
  glDeleteBuffers(4, buffers);
  m.StreamRelease();
  m.FenceRelease();
  if (m.timerOn) {
    glDeleteQueries(Impl::TimerPool::kQueries, m.chunkTimer.query);
    glDeleteQueries(Impl::TimerPool::kQueries, m.presentTimer.query);
  }
  const GLuint vaos[] = {m.vao, m.quadVao};
  glDeleteVertexArrays(2, vaos);
  glDeleteProgram(m.copyProgram);
  glDeleteProgram(m.paletteProgram);
  glDeleteProgram(m.presentProgram);
  glDeleteProgram(m.focusProgram);
  glDeleteProgram(m.blurProgram);
  glDeleteProgram(m.halfProgram);
  glDeleteProgram(m.mixProgram);
  const GLuint halfTextures[] = {m.halfTex};
  glDeleteTextures(1, halfTextures);
  const GLuint halfFbos[] = {m.halfFbo, m.colorFbo};
  glDeleteFramebuffers(2, halfFbos);
  const GLuint autofocusTextures[] = {m.blurTex, m.focusTex};
  glDeleteTextures(2, autofocusTextures);
  const GLuint autofocusFbos[] = {m.blurFbo, m.focusFbo};
  glDeleteFramebuffers(2, autofocusFbos);
  impl_.reset(new Impl);
}

} // namespace re4gx
