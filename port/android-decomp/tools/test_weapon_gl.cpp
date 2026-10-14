// Production two-instance GLES skinning and original hand attachment.
// Pass locally extracted assets; none are included in this test or repository.
#include "actor_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks;
bool failWeaponUniform;
unsigned injectedFailures;
void check(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
  ++checks;
}
struct Egl {
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLContext context = EGL_NO_CONTEXT;
  EGLSurface surface = EGL_NO_SURFACE;
  Egl() {
    setenv("EGL_PLATFORM", "surfaceless", 1);
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major, minor;
    check(eglInitialize(display, &major, &minor), "Weapon EGL initialize");
    check(eglBindAPI(EGL_OPENGL_ES_API), "Weapon EGL API");
    const EGLint configAttributes[]{EGL_SURFACE_TYPE,
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
    EGLint count;
    check(eglChooseConfig(display, configAttributes, &config, 1, &count) &&
              count,
          "Weapon EGL config");
    const EGLint contextAttributes[]{EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context =
        eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    const EGLint surfaceAttributes[]{EGL_WIDTH, 960, EGL_HEIGHT, 540, EGL_NONE};
    surface = eglCreatePbufferSurface(display, config, surfaceAttributes);
    check(context != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE &&
              eglMakeCurrent(display, surface, surface, context),
          "Weapon EGL current");
  }
  ~Egl() {
    if (display != EGL_NO_DISPLAY) {
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (surface != EGL_NO_SURFACE)
        eglDestroySurface(display, surface);
      if (context != EGL_NO_CONTEXT)
        eglDestroyContext(display, context);
      eglTerminate(display);
    }
  }
};
std::vector<unsigned char> pixels() {
  std::vector<unsigned char> result(960 * 540 * 4);
  glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
  check(glGetError() == GL_NO_ERROR, "Weapon framebuffer read");
  return result;
}
void save(const std::string &path, const std::vector<unsigned char> &image) {
  std::ofstream output(path, std::ios::binary);
  output << "P6\n960 540\n255\n";
  for (int y = 539; y >= 0; --y)
    for (int x = 0; x < 960; ++x)
      output.write(
          reinterpret_cast<const char *>(image.data() + (y * 960 + x) * 4), 3);
  check(bool(output), "Weapon framebuffer export");
}
re4::Matrix34 placement(const re4::LeonPreview &actor) {
  re4::SmdPlacement value;
  value.scale = {1, 1, 1};
  value.rotation[1] = actor.heading();
  for (unsigned i = 0; i < 3; ++i)
    value.position[i] = float(actor.feet()[i]);
  return re4::PlacementMatrix(value);
}
double capture(const re4::WeaponPreview &weapon, const re4::Matrix34 &hand,
               const re4::Matrix34 &world) {
  // DrawWeaponGl leaves its actual shader, VAO and UBO bound. The test target
  // enables three production-shader varyings: local position, world normal,
  // and fully attached world position. The body check retains its two varyings.
  const auto count = weapon.skin().size();
  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer);
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, count * 9 * sizeof(float), nullptr,
               GL_STREAM_READ);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer);
  glEnable(GL_RASTERIZER_DISCARD);
  glBeginTransformFeedback(GL_TRIANGLES);
  glDrawArrays(GL_TRIANGLES, 0, GLsizei(count));
  glEndTransformFeedback();
  glDisable(GL_RASTERIZER_DISCARD);
  check(glGetError() == GL_NO_ERROR, "Weapon transform feedback");
  const auto *observed = static_cast<const float *>(
      glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
                       count * 9 * sizeof(float), GL_MAP_READ_BIT));
  check(observed != nullptr, "Weapon transform-feedback mapping");
  const auto &mesh = weapon.mesh();
  const auto &bones = weapon.rig().bones();
  const auto &bind = weapon.rig().bindCenters();
  double maximum = 0;
  for (std::size_t i = 0; i < count; ++i) {
    std::array<double, 3> local{}, normal{}, inHand{}, inHandNormal{},
        attached{}, attachedNormal{};
    for (unsigned influence = 0; influence < 3; ++influence)
      for (unsigned row = 0; row < 3; ++row) {
        const auto &skin = weapon.skin()[i];
        auto joint = skin.position.joints[influence];
        double point = bones[joint][row * 4 + 3];
        for (unsigned col = 0; col < 3; ++col)
          point += double(bones[joint][row * 4 + col]) *
                   (mesh.vertices[i * 3 + col] - double(bind[joint][col]));
        local[row] += double(skin.position.rates[influence]) * point;
        joint = skin.normal.joints[influence];
        for (unsigned col = 0; col < 3; ++col)
          normal[row] += double(skin.normal.rates[influence]) *
                         bones[joint][row * 4 + col] *
                         mesh.normals[i * 3 + col];
      }
    // Evaluate the parenting chain separately in double precision rather
    // than using the production concatenation helper or a Leon skin palette.
    for (unsigned row = 0; row < 3; ++row) {
      inHand[row] = hand[row * 4 + 3];
      for (unsigned col = 0; col < 3; ++col) {
        inHand[row] += double(hand[row * 4 + col]) * local[col];
        inHandNormal[row] += double(hand[row * 4 + col]) * normal[col];
      }
    }
    for (unsigned row = 0; row < 3; ++row) {
      attached[row] = world[row * 4 + 3];
      for (unsigned col = 0; col < 3; ++col) {
        attached[row] += double(world[row * 4 + col]) * inHand[col];
        attachedNormal[row] += double(world[row * 4 + col]) * inHandNormal[col];
      }
      check(std::isfinite(observed[i * 9 + row]) &&
                std::isfinite(observed[i * 9 + row + 3]) &&
                std::isfinite(observed[i * 9 + row + 6]),
            "Non-finite attached weapon GPU result");
      maximum = std::max(maximum, std::abs(observed[i * 9 + row] - local[row]));
      maximum = std::max(
          maximum, std::abs(observed[i * 9 + row + 3] - attachedNormal[row]));
      maximum = std::max(maximum,
                         std::abs(observed[i * 9 + row + 6] - attached[row]));
    }
  }
  check(glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER) == GL_TRUE,
        "Weapon transform-feedback corruption");
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
  glDeleteBuffers(1, &buffer);
  check(maximum < .02,
        "Weapon GPU differs from independent parenting/skin chain");
  return maximum;
}
} // namespace

// GNU/Linux host-only allocation fault; production Android has no test hook.
extern "C" void __real_glBufferData(GLenum, GLsizeiptr, const void *, GLenum);
extern "C" void __wrap_glBufferData(GLenum target, GLsizeiptr size,
                                    const void *data, GLenum usage) {
  if (target == GL_UNIFORM_BUFFER && failWeaponUniform) {
    failWeaponUniform = false;
    ++injectedFailures;
    __real_glBufferData(target, -1, data, usage);
  } else {
    __real_glBufferData(target, size, data, usage);
  }
}

int main(int argc, char **argv) {
  try {
    if (argc != 4)
      throw std::runtime_error(
          "Usage: weapon_gl_check PL00.drs WEP01.drs OUTPUT.ppm");
    Egl egl;
    re4::LeonPreview actor(argv[1], argv[2]);
    re4::WeaponPreview weapon(argv[2]);
    actor.setPlacement({1730, -241, 880}, .731f);
    re4::CreateActorGl(actor);
    re4::CreateWeaponGl(weapon);
    const float camera[16]{1.f / 1100, 0, 0,           0, 0, 1.f / 1300, 0, 0,
                           0,          0, -1.f / 5000, 0, 0, -.6f,       0, 1};
    const std::array<float, 3> center{1730, -241, 880};
    auto draw = [&](const re4::WeaponPreview &gun, bool showWeapon,
                    bool normals) {
      glViewport(0, 0, 960, 540);
      glDepthMask(GL_TRUE);
      glClearColor(.035f, .055f, .075f, 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      re4::DrawActorGl(camera, center, 1, actor, normals);
      if (showWeapon)
        re4::DrawWeaponGl(camera, center, 1, gun, actor.rig().bones().at(10),
                          placement(actor), normals);
      check(glGetError() == GL_NO_ERROR, "Body/weapon GPU draw");
      return pixels();
    };
    auto body = draw(weapon, false, false), armed = draw(weapon, true, false);
    unsigned visible = 0;
    for (std::size_t i = 0; i < body.size(); i += 4)
      visible += !std::equal(body.begin() + i, body.begin() + i + 3,
                             armed.begin() + i);
    check(visible > 20, "Original attached WEP01 model is not visible");
    save(argv[3], armed);
    check(draw(weapon, true, true) != armed, "Body/weapon normals view absent");
    check(draw(weapon, true, false) == armed, "Body/weapon texture restore");
    double maximum = 0;
    unsigned samples = 0, changed = 0;
    for (unsigned actorClip = 0; actorClip < 3; ++actorClip) {
      actor.rig().sample(actorClip, actor.rig().maxFrame(actorClip) * .375f);
      for (bool empty : {false, true})
        for (auto motion : {re4::WeaponMotion::Idle, re4::WeaponMotion::Fire,
                            re4::WeaponMotion::Reload}) {
          re4::WeaponPreview gun(argv[2]);
          const auto maximumFrame = gun.maxFrame(motion, empty);
          std::vector<float> frames{0, 3, maximumFrame * .375f, maximumFrame};
          if (motion == re4::WeaponMotion::Reload) {
            frames.push_back(14);
            frames.push_back(31);
            frames.push_back(39);
          }
          std::vector<unsigned char> first;
          for (auto frame : frames) {
            gun.sample(motion, frame, empty);
            auto image = draw(gun, true, false);
            if (first.empty())
              first = image;
            else
              changed += image != first;
            maximum = std::max(maximum, capture(gun, actor.rig().bones().at(10),
                                                placement(actor)));
            ++samples;
          }
        }
    }
    check(changed >= 6, "Local original fire/reload motion has no GPU effect");
    auto before = draw(weapon, true, false);
    failWeaponUniform = true;
    bool failed = false;
    try {
      re4::CreateWeaponGl(weapon);
    } catch (const std::exception &) {
      failed = true;
    }
    check(failed && !failWeaponUniform && injectedFailures == 1,
          "Actual weapon GPU allocation failure was not exercised");
    check(draw(weapon, true, false) == before,
          "Weapon GPU failure damaged the existing body/weapon instances");
    re4::DestroyActorGl();
    re4::CreateActorGl(actor);
    re4::CreateWeaponGl(weapon);
    check(draw(weapon, true, false) == before,
          "Body/weapon GPU recreate pose lost");
    re4::DestroyActorGl();
    std::cout << "Weapon GPU PASS: " << samples << " poses, "
              << weapon.skin().size() << " corners, max attached difference "
              << maximum << " units; " << visible << " visible pixels; "
              << "two-instance state, GPU rollback, cleanup/recreate; "
              << checks << " checks\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Weapon GPU FAIL: " << error.what() << '\n';
    return 1;
  }
}
