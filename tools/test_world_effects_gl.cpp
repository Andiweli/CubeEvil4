#include "archive.h"
#include "world_effects_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
constexpr int W = 128, H = 128;
unsigned checks;
void require(bool value, const char *message) {
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}
struct Egl {
  EGLDisplay display;
  EGLConfig config{};
  EGLContext context = EGL_NO_CONTEXT;
  EGLSurface surface;
  Egl() {
    setenv("EGL_PLATFORM", "surfaceless", 1);
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major, minor;
    require(eglInitialize(display, &major, &minor), "EGL initialize");
    require(eglBindAPI(EGL_OPENGL_ES_API), "EGL GLES");
    EGLint attrs[]{EGL_SURFACE_TYPE,
                   EGL_PBUFFER_BIT,
                   EGL_RENDERABLE_TYPE,
                   EGL_OPENGL_ES3_BIT,
                   EGL_RED_SIZE,
                   8,
                   EGL_GREEN_SIZE,
                   8,
                   EGL_BLUE_SIZE,
                   8,
                   EGL_ALPHA_SIZE,
                   8,
                   EGL_DEPTH_SIZE,
                   24,
                   EGL_NONE};
    EGLint n;
    require(eglChooseConfig(display, attrs, &config, 1, &n) && n, "EGL config");
    EGLint size[]{EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE};
    surface = eglCreatePbufferSurface(display, config, size);
    require(surface != EGL_NO_SURFACE, "EGL surface");
    fresh();
  }
  void fresh() {
    if (context != EGL_NO_CONTEXT) {
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      require(eglDestroyContext(display, context), "old EGL destroy");
    }
    EGLint attrs[]{EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
    require(context != EGL_NO_CONTEXT &&
                eglMakeCurrent(display, surface, surface, context),
            "EGL fresh context");
  }
  ~Egl() {
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
  }
};
void put(WaterRecord &r, unsigned p, unsigned value) {
  for (unsigned k = 0; k < 4; ++k)
    r.bytes.at(p + k) = std::uint8_t(value >> (24 - k * 8));
}
void real(WaterRecord &r, unsigned p, float value) {
  unsigned bits;
  std::memcpy(&bits, &value, 4);
  put(r, p, bits);
}
WorldEffectsData fixture() {
  WorldEffectsData data;
  data.sprites.signature = 24;
  data.sequences = {3};
  WaterAnimation a;
  a.id = 1;
  a.width = a.height = 2;
  a.loop = 1;
  a.pattern = {0, 1};
  a.duration = {1, 1};
  for (unsigned k = 0; k < 2; ++k) {
    Texture t;
    t.width = t.height = 2;
    for (unsigned j = 0; j < 4; ++j) {
      t.rgba.push_back(k ? 255 : 0);
      t.rgba.push_back(0);
      t.rgba.push_back(k ? 0 : 255);
      t.rgba.push_back(255);
    }
    a.images.push_back(t);
  }
  data.sprites.animations = {a};
  WaterRecord r;
  r.bytes[0] = 1;
  r.bytes[1] = 0;
  r.bytes[7] = 0xfe;
  put(r, 8, 1);
  real(r, 48, 1);
  real(r, 136, 1.6f);
  real(r, 140, 1.6f);
  real(r, 152, 1);
  for (unsigned k = 0; k < 4; ++k) {
    r.bytes[156 + k] = 255;
    real(r, 160 + k * 4, 1);
  }
  WaterTarget target;
  target.sequence = 3;
  target.records = {r};
  data.sprites.targets = {target};
  return data;
}
constexpr float identity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
void clear(float depth = 1) {
  glViewport(0, 0, W, H);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_TRUE);
  glClearColor(.1f, .2f, .3f, .5f);
  glClearDepthf(depth);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
std::vector<std::uint8_t> pixels() {
  std::vector<std::uint8_t> p(W * H * 4);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
  require(!glGetError(), "framebuffer read");
  return p;
}
unsigned difference(const std::vector<std::uint8_t> &a,
                    const std::vector<std::uint8_t> &b) {
  unsigned count = 0;
  for (std::size_t i = 0; i < a.size(); i += 4)
    if (!std::equal(a.begin() + i, a.begin() + i + 4, b.begin() + i))
      ++count;
  return count;
}
void preview(const char *name, const std::vector<std::uint8_t> &p) {
  const char *prefix = std::getenv("RE4_EFFECT_PREVIEW_PREFIX");
  if (!prefix)
    return;
  std::ofstream stream(std::string(prefix) + name + ".ppm", std::ios::binary);
  stream << "P6\n" << W << ' ' << H << "\n255\n";
  for (int y = H - 1; y >= 0; --y)
    for (int x = 0; x < W; ++x)
      stream.write(reinterpret_cast<const char *>(p.data() + (y * W + x) * 4),
                   3);
  require(bool(stream), "actual source effect preview saved");
}
GLuint sentinelVao{}, sentinelBuffer{}, sentinelTexture{};
void sentinel() {
  glGenVertexArrays(1, &sentinelVao);
  glGenBuffers(1, &sentinelBuffer);
  glGenTextures(1, &sentinelTexture);
  glBindVertexArray(sentinelVao);
  glBindBuffer(GL_ARRAY_BUFFER, sentinelBuffer);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sentinelTexture);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, sentinelTexture);
  glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_GREATER);
  glDepthMask(GL_TRUE);
  glEnable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);
  glBlendFuncSeparate(GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ZERO);
  glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
}
void checkState() {
  GLint v;
  GLboolean b, mask[4];
  glGetIntegerv(GL_CURRENT_PROGRAM, &v);
  require(v == 0, "program restored");
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &v);
  require(v == int(sentinelVao), "VAO restored");
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &v);
  require(v == int(sentinelBuffer), "array buffer restored");
  glGetIntegerv(GL_ACTIVE_TEXTURE, &v);
  require(v == GL_TEXTURE3, "active texture restored");
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &v);
  require(v == int(sentinelTexture), "active unit binding restored");
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &v);
  require(v == int(sentinelTexture), "unit zero binding restored");
  glActiveTexture(GL_TEXTURE3);
  require(!glIsEnabled(GL_DEPTH_TEST), "depth enable restored");
  glGetIntegerv(GL_DEPTH_FUNC, &v);
  require(v == GL_GREATER, "depth function restored");
  glGetBooleanv(GL_DEPTH_WRITEMASK, &b);
  require(b, "depth write mask restored");
  require(glIsEnabled(GL_BLEND) && glIsEnabled(GL_CULL_FACE),
          "blend/cull enable restored");
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &v);
  require(v == GL_FUNC_REVERSE_SUBTRACT, "RGB blend equation restored");
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &v);
  require(v == GL_FUNC_SUBTRACT, "alpha blend equation restored");
  glGetIntegerv(GL_BLEND_SRC_RGB, &v);
  require(v == GL_DST_ALPHA, "RGB source blend restored");
  glGetIntegerv(GL_BLEND_DST_RGB, &v);
  require(v == GL_ONE_MINUS_DST_ALPHA, "RGB destination blend restored");
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &v);
  require(v == GL_ONE, "alpha source blend restored");
  glGetIntegerv(GL_BLEND_DST_ALPHA, &v);
  require(v == GL_ZERO, "alpha destination blend restored");
  glGetBooleanv(GL_COLOR_WRITEMASK, mask);
  require(mask[0] && !mask[1] && mask[2] && mask[3],
          "color write mask restored");
  require(!glGetError(), "state restoration clean");
}
} // namespace
int main(int argc, char **argv) {
  try {
    Egl egl;
    auto data = fixture();
    WorldEffectCamera camera;
    sentinel();
    CreateWorldEffectsGl(data);
    checkState();
    clear();
    sentinel(); // Sentinel creation does not touch framebuffer.
    DrawWorldEffectsGl(identity, {}, 1, camera, 0);
    checkState();
    auto phase0 = pixels();
    require(phase0[(H / 2 * W + W / 2) * 4] == 255,
            "world sprite rendered to RGB");
    require(phase0[(H / 2 * W + W / 2) * 4 + 3] == 128,
            "world sprite preserves destination alpha");
    clear();
    DrawWorldEffectsGl(identity, {}, 1, camera, 1);
    auto phase1 = pixels();
    require(difference(phase0, phase1) > 10000,
            "ANM changes original visible frame");
    auto saved = SaveWorldEffectsState();
    require(saved && saved->frames == 201, "clock includes original pre-run");
    clear();
    DrawWorldEffectsGl(identity, {}, 1, camera, 1);
    require(pixels() == phase1, "paused drawing preserves exact pixels");
    clear(.25f);
    auto covered = pixels();
    DrawWorldEffectsGl(identity, {}, 1, camera, 1);
    require(pixels() == covered, "room depth occludes sprite completely");
    clear();
    DrawWorldEffectsGl(identity, {}, 1, camera, 0);
    require(pixels() == phase0, "backward scene clock deterministic reset");
    ForgetWorldEffectsGl();
    egl.fresh();
    CreateWorldEffectsGl(data);
    RestoreWorldEffectsState(saved);
    clear();
    DrawWorldEffectsGl(identity, {}, 1, camera, 1);
    require(pixels() == phase1,
            "lost EGL context preserves restored visible phase");
    DestroyWorldEffectsGl();
    WorldEffectsData empty;
    CreateWorldEffectsGl(empty);
    RestoreWorldEffectsState(saved);
    require(SaveWorldEffectsState()->signature == 0 &&
                SaveWorldEffectsState()->sprites.empty(),
            "r400 snapshot does not contaminate empty SAT inspector");
    DestroyWorldEffectsGl();
    auto otherRoom = data;
    otherRoom.sprites.signature = 25;
    CreateWorldEffectsGl(otherRoom);
    RestoreWorldEffectsState(saved);
    require(SaveWorldEffectsState()->frames == 200 &&
                SaveWorldEffectsState()->signature == 25,
            "foreign scene snapshot leaves fresh source phase");
    DestroyWorldEffectsGl();
    if (argc == 3 || argc == 4) {
      auto core = RoomArchive::LoadDrs(argv[1]);
      auto *e = core.find("EFF");
      auto room = RoomArchive::Load(argv[2]);
      auto original = BuildRoomWorldEffects(
          room, 0x400, core.copy(e - core.entries().data()));
      require(original.triggerBanks.size() == 1,
              "original effect data and explosive bank decoded");
      CreateWorldEffectsGl(original);
      clear();
      camera.eye = {-7972, 2000, 8000};
      DrawWorldEffectsGl(identity, {-7972, 250, -318}, 1.f / 5000, camera, 0);
      auto a = pixels();
      auto snap = SaveWorldEffectsState();
      DrawWorldEffectsGl(identity, {-7972, 250, -318}, 1.f / 5000, camera, 23);
      require(SaveWorldEffectsState()->frames == 223,
              "original animation clock uploaded");
      RestoreWorldEffectsState(snap);
      clear();
      DrawWorldEffectsGl(identity, {-7972, 250, -318}, 1.f / 5000, camera, 0);
      require(pixels() == a,
              "original resource snapshot reproduces GPU pixels");
      require(TriggerWorldEffectsGl(0x2d, {-7972, 250, -318}, {}, 77),
              "original ET2D break triggers loaded texture bank");
      clear();
      DrawWorldEffectsGl(identity, {-7972, 1500, -318}, 1.f / 6000, camera, 8);
      auto explosion = pixels();
      preview("-et2d-explosion", explosion);
      auto exploded = SaveWorldEffectsState();
      require(exploded->instances.size() == 1 &&
                  exploded->instances[0].state.frames == 8,
              "original explosion relative clock rendered");
      clear();
      DrawWorldEffectsGl(identity, {-7972, 1500, -318}, 1.f / 6000, camera, 8);
      require(pixels() == explosion, "paused explosion pixel phase exact");
      clear(0);
      auto coveredExplosion = pixels();
      DrawWorldEffectsGl(identity, {-7972, 1500, -318}, 1.f / 6000, camera, 8);
      require(pixels() == coveredExplosion,
              "opaque room depth occludes original explosion");
      ForgetWorldEffectsGl();
      egl.fresh();
      CreateWorldEffectsGl(original);
      RestoreWorldEffectsState(exploded);
      clear();
      DrawWorldEffectsGl(identity, {-7972, 1500, -318}, 1.f / 6000, camera, 8);
      require(pixels() == explosion,
              "live explosion survives lost EGL context");
      DestroyWorldEffectsGl();
      if (argc == 4) {
        auto village =
            BuildRoomWorldEffects(RoomArchive::Load(argv[3]), 0x101,
                                  core.copy(e - core.entries().data()));
        require(village.originalPyre, "r101 original EST0 loaded to GLES");
        camera.eye = {-3400, 2200, 12000};
        camera.forward = {0, 0, -1};
        CreateWorldEffectsGl(village);
        const std::array<float, 3> center{-3400, 1200, 2650};
        clear();
        auto background = pixels();
        sentinel();
        DrawWorldEffectsGl(identity, center, 1.f / 3000, camera, 0);
        checkState();
        auto pyre0 = pixels();
        preview("-r101-pyre-0", pyre0);
        require(difference(pyre0, background) > 128,
                "actual original fire textures visibly draw");
        auto start = SaveWorldEffectsState();
        require(start->streams[1].frames == 0,
                "GLES startup does not warm explicit EstSet");
        clear();
        DrawWorldEffectsGl(identity, center, 1.f / 3000, camera, 90);
        auto pyre90 = pixels();
        preview("-r101-pyre-90", pyre90);
        require(difference(pyre0, pyre90) > 128,
                "source pyre animation and emitters visibly advance");
        auto active = SaveWorldEffectsState();
        clear();
        DrawWorldEffectsGl(identity, center, 1.f / 3000, camera, 90);
        require(pixels() == pyre90,
                "paused pyre preserves original source pixels");
        clear(0);
        auto behind = pixels();
        DrawWorldEffectsGl(identity, center, 1.f / 3000, camera, 90);
        require(pixels() == behind,
                "original flame and smoke respect room depth");
        clear();
        DrawWorldEffectsGl(identity, center, 1.f / 3000, camera, 120);
        auto next = pixels();
        RestoreWorldEffectsState(active);
        clear();
        DrawWorldEffectsGl(identity, center, 1.f / 3000, camera, 120);
        require(pixels() == next,
                "source fire replay including Esp0b camera offsets exact");
        ForgetWorldEffectsGl();
        egl.fresh();
        CreateWorldEffectsGl(village);
        RestoreWorldEffectsState(active);
        clear();
        DrawWorldEffectsGl(identity, center, 1.f / 3000, camera, 90);
        require(pixels() == pyre90,
                "lost EGL context restores two independent pyre clocks");
        DestroyWorldEffectsGl();
      }
    }
    std::cout << "world_effects_gl_check: " << checks
              << " GLES3 checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
