// Actual GLES3 skinning, framebuffer and room/actor state validation.
// Original game data are supplied by the developer, never bundled here.
#include "actor_gl.h"
#include "archive.h"
#include "sat.h"
#include "scene_gl.h"
#include "walk_probe.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool okay, const char *message) {
  if (!okay)
    throw std::runtime_error(message);
}
std::vector<unsigned char> pixels() {
  std::vector<unsigned char> p(960 * 540 * 4);
  glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
  check(glGetError() == GL_NO_ERROR, "Actor framebuffer read");
  return p;
}
void save(const std::string &path, const std::vector<unsigned char> &p) {
  std::ofstream f(path, std::ios::binary);
  f << "P6\n960 540\n255\n";
  for (int y = 539; y >= 0; --y)
    for (int x = 0; x < 960; ++x)
      f.write(reinterpret_cast<const char *>(p.data() + (y * 960 + x) * 4), 3);
  check(bool(f), "Actor framebuffer export");
}
double capture(const re4::LeonPreview &actor) {
  // DrawActorGl leaves its own program and VAO bound. This test target links
  // transform-feedback varyings into that exact production vertex shader.
  const auto &mesh = actor.mesh();
  const auto &bones = actor.rig().bones();
  const auto &bind = actor.rig().bindCenters();
  auto count = actor.skin().size();
  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer);
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, count * 6 * sizeof(float), nullptr,
               GL_STREAM_READ);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer);
  glEnable(GL_RASTERIZER_DISCARD);
  glBeginTransformFeedback(GL_TRIANGLES);
  glDrawArrays(GL_TRIANGLES, 0, GLsizei(count));
  glEndTransformFeedback();
  glDisable(GL_RASTERIZER_DISCARD);
  check(glGetError() == GL_NO_ERROR, "Actor transform feedback");
  auto result = static_cast<const float *>(
      glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
                       count * 6 * sizeof(float), GL_MAP_READ_BIT));
  check(result != nullptr, "Actor transform-feedback mapping");
  re4::SmdPlacement p;
  p.scale = {1, 1, 1};
  p.rotation[1] = actor.heading();
  auto placement = re4::PlacementMatrix(p);
  double maximum = 0;
  for (std::size_t i = 0; i < count; ++i) {
    std::array<double, 3> position{}, normal{};
    const auto &skin = actor.skin()[i];
    for (unsigned j = 0; j < 3; ++j)
      for (unsigned row = 0; row < 3; ++row) {
        auto id = skin.position.joints[j];
        double transformed = bones[id][row * 4 + 3];
        for (unsigned col = 0; col < 3; ++col)
          transformed += double(bones[id][row * 4 + col]) *
                         (mesh.vertices[i * 3 + col] - bind[id][col]);
        position[row] += double(skin.position.rates[j]) * transformed;
        id = skin.normal.joints[j];
        for (unsigned col = 0; col < 3; ++col)
          normal[row] += double(skin.normal.rates[j]) *
                         bones[id][row * 4 + col] * mesh.normals[i * 3 + col];
      }
    for (unsigned row = 0; row < 3; ++row) {
      double n = 0;
      for (unsigned col = 0; col < 3; ++col)
        n += placement[row * 4 + col] * normal[col];
      check(std::isfinite(result[i * 6 + row]) &&
                std::isfinite(result[i * 6 + 3 + row]),
            "Non-finite GPU skin");
      maximum =
          std::max(maximum, std::abs(result[i * 6 + row] - position[row]));
      maximum = std::max(maximum, std::abs(result[i * 6 + 3 + row] - n));
    }
  }
  check(glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER) == GL_TRUE,
        "Actor capture corruption");
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
  glDeleteBuffers(1, &buffer);
  check(maximum < .02,
        "GPU skin differs from independent double-precision evaluation");
  return maximum;
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 5)
      throw std::runtime_error(
          "Usage: actor_gl_check PL00.drs WEP01.drs R300.dat OUTPUT.ppm");
    setenv("EGL_PLATFORM", "surfaceless", 1);
    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major, minor;
    check(eglInitialize(display, &major, &minor), "Actor EGL initialize");
    check(eglBindAPI(EGL_OPENGL_ES_API), "Actor EGL API");
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
    EGLConfig config;
    EGLint n;
    check(eglChooseConfig(display, attrs, &config, 1, &n) && n,
          "Actor EGL config");
    EGLint ca[]{EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    auto context = eglCreateContext(display, config, EGL_NO_CONTEXT, ca);
    EGLint pa[]{EGL_WIDTH, 960, EGL_HEIGHT, 540, EGL_NONE};
    auto surface = eglCreatePbufferSurface(display, config, pa);
    check(eglMakeCurrent(display, surface, surface, context),
          "Actor EGL current");
    re4::LeonPreview actor(argv[1], argv[2]);
    GLint budget = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &budget);
    check(budget >= 8192, "GLES3 joint UBO below minimum budget");
    re4::CreateActorGl(actor);
    float camera[16]{1.f / 1100, 0, 0,           0, 0, 1.f / 1300, 0, 0,
                     0,          0, -1.f / 5000, 0, 0, -.6f,       0, 1};
    std::array<float, 3> center{};
    auto draw = [&](bool normals) {
      glViewport(0, 0, 960, 540);
      glDepthMask(GL_TRUE);
      glClearColor(.035f, .055f, .075f, 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      re4::DrawActorGl(camera, center, 1, actor, normals);
      check(glGetError() == GL_NO_ERROR, "Actor draw error");
      return pixels();
    };
    double maximum = 0;
    unsigned changed = 0;
    auto idle = draw(false);
    save(argv[4], idle);
    for (unsigned clip = 0; clip < 3; ++clip) {
      actor.rig().sample(clip, actor.rig().maxFrame(clip) * .375f);
      auto pose = draw(false);
      maximum = std::max(maximum, capture(actor));
      auto normal = draw(true);
      check(normal != pose, "Actor normal view absent");
      check(draw(false) == pose, "Actor texture restore");
      changed += pose != idle;
      save(std::string(argv[4]) + ".clip-" + std::to_string(clip) + ".ppm",
           pose);
    }
    check(changed >= 2, "Actor locomotion does not change framebuffer");
    std::cout << "GPU skin PASS: 3 poses, " << actor.skin().size()
              << " corners, separate position/normal weights, max difference "
              << maximum << " game units\n";
    re4::DestroyActorGl();
    re4::CreateActorGl(actor);
    check(draw(false) != idle, "Actor recreate pose lost");
    re4::DestroyActorGl();

    auto archive = re4::RoomArchive::Load(argv[3]);
    auto copy = [&](const char *tag) {
      auto e = archive.find(tag);
      check(e != nullptr, "Actor room asset absent");
      return archive.copy(e - archive.entries().data());
    };
    re4::Smd smd(copy("SMD"));
    re4::Smx smx(copy("SMX"));
    re4::Sat sat(copy("SAT"));
    re4::SatCollision world(sat);
    re4::WalkProbe walker(world);
    check(walker.reset(), "Actor room spawn");
    auto room = re4::BuildStaticScene(smd);
    re4::AttachSceneTextures(smd, room);
    re4::ApplySceneSmx(smx, room);
    re4::CreateSceneGl(room);
    re4::CreateActorGl(actor);
    re4::SceneFollowDistance(4000);
    actor.reset();
    actor.update(walker.position(), 0, 0, false, 0);
    re4::SceneProbe probe;
    probe.collision = &world;
    probe.feet = walker.position();
    probe.showBody = false;
    probe.groundTriangle = world.triangle(walker.floorHit()->polygon);
    auto follow = [&](bool normals, bool showActor) {
      re4::DrawSceneGl(960, 540, 0, 0, 0, normals, &probe,
                       showActor ? &actor : nullptr, 0);
      return pixels();
    };
    auto without = follow(false, false), with = follow(false, true);
    unsigned visible = 0;
    for (std::size_t i = 0; i < with.size(); i += 4)
      visible += !std::equal(with.begin() + i, with.begin() + i + 3,
                             without.begin() + i);
    check(visible > 100, "Leon not visible in original room");
    check(follow(true, true) != with, "Room/actor normal view unchanged");
    check(follow(false, true) == with, "Room/actor texture restore");
    save(std::string(argv[4]) + ".room.ppm", with);
    for (unsigned tick = 0; tick < 120; ++tick) {
      auto old = walker.position();
      walker.step(.4, -.5, re4::SceneCameraYaw(), tick > 60, 1. / 30);
      auto now = walker.position();
      double dx = now[0] - old[0], dz = now[2] - old[2];
      actor.update(now, std::hypot(dx, dz), float(std::atan2(dx, dz)),
                   tick > 60, 1. / 30);
      probe.feet = now;
      probe.groundTriangle = world.triangle(walker.floorHit()->polygon);
      re4::DrawSceneGl(960, 540, 0, 0, 0, false, &probe, &actor, 1. / 30);
      check(glGetError() == GL_NO_ERROR, "Animated room GL state/error");
    }
    // The source close shoulder camera may crop Leon beside a wall. It must
    // continue submitting the source mesh; distance alone cannot hide it.
    // Exact synthetic close-wall/occlusion coverage lives in the dedicated
    // follow_camera_gl_check rather than relying on this moving-room endpoint.
    check(follow(false, true) != follow(false, false),
          "Animated Leon remains visible with ordinary room depth");
    save(std::string(argv[4]) + ".walk.ppm", pixels());
    re4::DestroySceneGl();
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(display, surface);
    eglDestroyContext(display, context);
    eglTerminate(display);
    std::cout << "Actor framebuffer PASS: original textures, moving "
                 "silhouette, normals/restore, GPU cleanup; "
              << visible << " visible room pixels, 120 moving room frames\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
