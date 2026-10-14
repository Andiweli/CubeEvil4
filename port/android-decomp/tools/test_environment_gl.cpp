#include "archive.h"
#include "environment_gl.h"
#include "room_flow.h"
#include "scene_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace re4;
namespace {
constexpr int W = 960, H = 540;
unsigned checks;
void require(bool b, const char *text) {
  ++checks;
  if (!b)
    throw std::runtime_error(text);
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
      require(eglDestroyContext(display, context), "Old EGL destroy");
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
std::vector<std::uint8_t> pixels() {
  std::vector<std::uint8_t> p(W * H * 4);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
  require(!glGetError(), "Framebuffer read");
  return p;
}
void ppm(const std::string &path, const std::vector<std::uint8_t> &p) {
  std::ofstream f(path, std::ios::binary);
  f << "P6\n" << W << ' ' << H << "\n255\n";
  for (int y = H - 1; y >= 0; --y)
    for (int x = 0; x < W; ++x)
      f.write(reinterpret_cast<const char *>(p.data() + (y * W + x) * 4), 3);
  require(bool(f), "PPM write");
}
std::vector<std::uint8_t> target(unsigned t, const EnvironmentData &data) {
  GLint read;
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, EnvironmentTexture(t), 0);
  require(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) ==
              GL_FRAMEBUFFER_COMPLETE,
          "Generated target complete");
  std::vector<std::uint8_t> p(data.targets[t].width * data.targets[t].height *
                              4);
  glReadPixels(0, 0, data.targets[t].width, data.targets[t].height, GL_RGBA,
               GL_UNSIGNED_BYTE, p.data());
  glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(read));
  glDeleteFramebuffers(1, &fbo);
  require(!glGetError(), "Generated target read");
  return p;
}
unsigned difference(const std::vector<std::uint8_t> &a,
                    const std::vector<std::uint8_t> &b) {
  unsigned n = 0;
  for (std::size_t i = 0; i < a.size(); i += 4)
    if (!std::equal(a.begin() + i, a.begin() + i + 4, b.begin() + i))
      ++n;
  return n;
}
StaticScene load(const char *path, unsigned id,
                 const std::vector<std::uint8_t> &core) {
  auto arc = RoomArchive::Load(path);
  auto copy = [&](const char *tag, unsigned occurrence = 0) {
    auto e = arc.find(tag, int(occurrence));
    return arc.copy(e - arc.entries().data());
  };
  Smd s(copy("SMD"));
  Smx x(copy("SMX"));
  std::unique_ptr<Smd> common;
  if (arc.find("SMD", 1))
    common = std::make_unique<Smd>(copy("SMD", 1), s.commonRequirements());
  auto scene = BuildStaticScene(s, -1, common.get());
  AttachSceneTextures(s, scene, common.get());
  ApplySceneSmx(x, scene);
  auto env = BuildRoomEnvironment(arc, s, x, std::uint16_t(id), core);
  require(env.warnings.empty(), "Original water source matched");
  AttachSceneEnvironment(env, scene);
  return scene;
}
void addQuad(StaticScene &s, float left, float right, float low, float high,
             float z, unsigned work, unsigned texture, bool sky = false,
             bool water = false) {
  SceneDraw d;
  d.first = s.vertices.size() / 3;
  d.count = 6;
  d.work = work;
  d.texture = int(texture);
  d.blendMode = 4;
  d.sky = sky ? 0 : -1;
  d.water = water ? 0 : -1;
  const float p[18]{left, low, z, right, low,  z, right, high, z,
                    left, low, z, right, high, z, left,  high, z};
  s.vertices.insert(s.vertices.end(), std::begin(p), std::end(p));
  const float uv[12]{0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0};
  s.uv.insert(s.uv.end(), std::begin(uv), std::end(uv));
  for (unsigned k = 0; k < 6; ++k) {
    s.normals.insert(s.normals.end(), {0, 0, 1});
    s.colors.insert(s.colors.end(), {255, 255, 255, 255});
  }
  s.draws.push_back(d);
}
Texture color(std::array<std::uint8_t, 4> c) {
  Texture t;
  t.width = t.height = 1;
  t.rgba.assign(c.begin(), c.end());
  return t;
}
void synthetic() {
  StaticScene s;
  s.minimum = {-1000, -1000, -20000};
  s.maximum = {1000, 1000, 600000};
  s.textures = {color({0, 0, 255, 255}), color({255, 0, 0, 255}),
                color({0, 255, 0, 255})};
  SkyObject sky;
  sky.work = 1;
  sky.placement.scale = {1, 1, 1};
  s.environment.sky = {sky};
  s.environment.signature = 220;
  WaterAnimation a;
  a.width = a.height = 1;
  a.pattern = {0};
  a.duration = {1};
  a.loop = 1;
  a.images = {s.textures[1]};
  s.environment.animations = {a};
  WaterTarget t;
  t.records.clear();
  s.environment.targets = {t};
  s.environment.surfaces = {{3, 0, 255, 0, 0, 255, false}};
  // Constant target tests depth independently of the animation generator.
  addQuad(s, -800000, 800000, -800000, 800000, 600000, 1, 0, true);
  // The neutral original shoulder eye is (-500,1765,-1190), looking at
  // (0,1340,1480). Keep a foreground panel near that view center and a wider
  // water panel farther away, with sky visible around both. The former
  // 4000-unit orbit fixture put the new 1291-unit shoulder eye almost inside
  // the foreground and let the water cover the entire framebuffer.
  addQuad(s, -3000, 3000, -2000, 3000, 5000, 3, 1, false, true);
  addQuad(s, -600, 600, 900, 1900, 1000, 2, 2);
  SceneProbe probe;
  probe.height = 1800;
  probe.showBody = false;
  // A static esp08 record supplies a constant red target. This tests
  // production depth and background projection independently of animation.
  WaterRecord r;
  r.bytes[0] = 1;
  r.bytes[1] = 8;
  r.bytes[7] = 253;
  r.bytes[159] = 255;
  r.bytes[156] = r.bytes[157] = r.bytes[158] = 255;
  r.bytes[8] = 0;
  r.bytes[9] = 1;
  r.bytes[10] = 0x80;
  auto write = [&](unsigned p, float f) {
    std::uint32_t u;
    std::memcpy(&u, &f, 4);
    for (unsigned k = 0; k < 4; ++k)
      r.bytes[p + k] = std::uint8_t(u >> (24 - k * 8));
  };
  write(12, 256);
  write(16, 224);
  write(48, 1);
  write(136, 600);
  write(140, 600);
  write(152, 1);
  for (unsigned p = 160; p <= 172; p += 4)
    write(p, 1);
  s.environment.targets[0].records = {r};
  CreateSceneGl(s);
  SceneFollowHeading(0);
  auto view = SaveSceneView();
  DrawSceneGl(W, H, 0, 0, 0, false, &probe, nullptr, 0);
  auto p = pixels();
  auto at = [&](int x, int y, unsigned c) { return p[(y * W + x) * 4 + c]; };
  require(at(W / 2, H / 2, 1) > 240 && at(W / 2, H / 2, 0) < 10,
          "Opaque foreground hides water");
  unsigned blue = 0, water = 0;
  for (std::size_t i = 0; i < p.size(); i += 4) {
    blue += p[i + 2] > 240 && p[i] < 10;
    water += p[i] > 240 && p[i + 1] < 10;
  }
  require(blue > 1000, "Sky beyond original 200000 far clip visible");
  require(water > 1000, "Water visible outside foreground");
  DestroySceneGl();
  // Generated FBO bottom/top must be flipped when used as a room texture.
  Texture gradient;
  gradient.width = gradient.height = 2;
  gradient.rgba = {255, 0, 0,   255, 255, 0, 0,   255,
                   0,   0, 255, 255, 0,   0, 255, 255};
  s.environment.animations[0].images = {gradient};
  s.environment.animations[0].width = s.environment.animations[0].height = 2;
  // The UV orientation check uses the inspection camera and its own centered
  // panel; it does not inherit the shoulder-view depth fixture above.
  s.vertices.clear();
  s.normals.clear();
  s.colors.clear();
  s.uv.clear();
  s.draws.clear();
  addQuad(s, -1500, 1500, -1000, 2500, 0, 3, 1, false, true);
  s.minimum = {-1500, -1000, 0};
  s.maximum = {1500, 2500, 0};
  s.environment.sky.clear();
  CreateSceneGl(s);
  view = SaveSceneView();
  view.yaw = 0;
  view.pitch = 0;
  view.distance = 3.3f;
  RestoreSceneView(view);
  DrawSceneGl(W, H, 0, 0, 0, false, nullptr, nullptr, 0);
  p = pixels();
  require(p[(H * 2 / 3 * W + W / 2) * 4] > p[(H * 2 / 3 * W + W / 2) * 4 + 2],
          "Generated room texture upper edge uses authored top row");
  require(p[(H / 3 * W + W / 2) * 4 + 2] > p[(H / 3 * W + W / 2) * 4],
          "Generated room texture lower edge uses authored bottom row");
  DestroySceneGl();
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 6)
      throw std::runtime_error("Usage: environment_gl_check CORE.das R100.dat "
                               "R300.dat R301.dat OUTPUT-PREFIX");
    Egl egl;
    synthetic();
    auto arc = RoomArchive::LoadDrs(argv[1]);
    auto e = arc.find("EFF");
    auto core = arc.copy(e - arc.entries().data());
    unsigned ids[]{0x100, 0x300, 0x301};
    for (unsigned room = 0; room < 3; ++room) {
      auto scene = load(argv[2 + room], ids[room], core);
      CreateSceneGl(scene);
      DrawSceneGl(W, H, 0, 0, 0, false, nullptr, nullptr, 0);
      auto before = target(0, scene.environment);
      DrawEnvironmentTargets(1800);
      auto after = target(0, scene.environment);
      auto delta = difference(before, after);
      require(delta > 10, "Original generated water animates");
      auto view = SaveSceneView();
      view.animationFrames = 1800;
      RestoreSceneView(view);
      DrawSceneGl(W, H, 0, 0, 0, false, nullptr, nullptr, 0);
      auto image = pixels();
      ppm(std::string(argv[5]) + "-r" +
              (room == 0   ? "100"
               : room == 1 ? "300"
                           : "301") +
              ".ppm",
          image);
      // Inspect the production follow view at an authored debug room start.
      auto infoPath =
          std::filesystem::path(argv[1]).parent_path().parent_path() /
          "Debug/roominfo.dat";
      std::ifstream infoFile(infoPath, std::ios::binary);
      std::vector<std::uint8_t> infoBytes{
          std::istreambuf_iterator<char>(infoFile), {}};
      RoomInfo info(infoBytes);
      const auto *start = info.find(std::uint16_t(ids[room]));
      require(start && start->valid(), "Original debug room start");
      SceneProbe probe;
      probe.feet = start->position;
      probe.showBody = false;
      SceneFollowHeading(float(start->heading));
      DrawSceneGl(W, H, 0, 0, 0, false, &probe, nullptr, 0);
      const auto followImage = pixels();
      const auto follow = SaveSceneView();
      for (unsigned i = 0; i < 3; ++i)
        DrawSceneGl(W, H, 0, 0, 0, false, &probe, nullptr, 0);
      require(pixels() == followImage,
              "Paused original follow view keeps exact framebuffer");
      ppm(std::string(argv[5]) + "-walk-r" +
              (room == 0   ? "100"
               : room == 1 ? "300"
                           : "301") +
              ".ppm",
          followImage);
      ForgetSceneGl();
      egl.fresh();
      CreateSceneGl(scene);
      RestoreSceneView(follow);
      DrawSceneGl(W, H, 0, 0, 0, false, &probe, nullptr, 0);
      require(pixels() == followImage,
              "Actual EGL context loss restores original follow image");
      RestoreSceneView(view);
      DrawSceneGl(W, H, 0, 0, 0, false, nullptr, nullptr, 0);
      require(pixels() == image, "Follow/inspection view restore");
      auto saved = SaveSceneView();
      for (unsigned i = 0; i < 3; ++i)
        DrawSceneGl(W, H, 0, 0, 0, false, nullptr, nullptr, 0);
      require(pixels() == image, "Pause keeps exact framebuffer");
      ForgetSceneGl();
      egl.fresh();
      CreateSceneGl(scene);
      RestoreSceneView(saved);
      DrawSceneGl(W, H, 0, 0, 0, false, nullptr, nullptr, 0);
      require(pixels() == image,
              "Actual EGL context loss restores sky/water exact image");
      require(target(0, scene.environment) == after,
              "Context resume retains effect target state");
      DestroySceneGl();
      std::cout << std::hex << ids[room] << std::dec
                << " generated change=" << delta
                << " pixels; pause/context PASS\n";
    }
    std::cout << "Environment GLES3 PASS: " << checks << " checks; "
              << glGetString(GL_VERSION) << '\n';
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
