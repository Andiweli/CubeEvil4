// Actual three-instance GLES skinning, original EM12 masks and GPU lifecycle.
// Supply locally extracted original assets; none are bundled by this test.
#include "actor_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
unsigned checks{}, injectedFailures{};
bool failUniform{}, recordEnemyTextures{};
std::vector<GLuint> enemyTextures;
void check(bool okay, const char *message) {
  if (!okay)
    throw std::runtime_error(message);
  ++checks;
}
struct Egl {
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLContext context = EGL_NO_CONTEXT;
  EGLSurface surface = EGL_NO_SURFACE;
  EGLConfig config{};
  Egl() {
    setenv("EGL_PLATFORM", "surfaceless", 1);
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major, minor;
    check(eglInitialize(display, &major, &minor), "Enemy EGL initialize");
    check(eglBindAPI(EGL_OPENGL_ES_API), "Enemy EGL API");
    const EGLint attributes[]{EGL_SURFACE_TYPE,
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
    EGLint count;
    check(eglChooseConfig(display, attributes, &config, 1, &count) && count,
          "Enemy EGL config");
    create();
  }
  void create() {
    const EGLint contextAttributes[]{EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context =
        eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    const EGLint surfaceAttributes[]{EGL_WIDTH, 960, EGL_HEIGHT, 540, EGL_NONE};
    surface = eglCreatePbufferSurface(display, config, surfaceAttributes);
    check(context != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE &&
              eglMakeCurrent(display, surface, surface, context),
          "Enemy EGL current");
  }
  void drop() {
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surface != EGL_NO_SURFACE)
      eglDestroySurface(display, surface);
    if (context != EGL_NO_CONTEXT)
      eglDestroyContext(display, context);
    surface = EGL_NO_SURFACE;
    context = EGL_NO_CONTEXT;
  }
  void recreate() {
    drop();
    create();
  }
  ~Egl() {
    if (display != EGL_NO_DISPLAY) {
      drop();
      eglTerminate(display);
    }
  }
};
std::vector<unsigned char> pixels() {
  std::vector<unsigned char> image(960 * 540 * 4);
  glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
  check(glGetError() == GL_NO_ERROR, "Enemy framebuffer read");
  return image;
}
unsigned different(const std::vector<unsigned char> &a,
                   const std::vector<unsigned char> &b) {
  unsigned count = 0;
  for (std::size_t i = 0; i < a.size(); i += 4)
    count += !std::equal(a.begin() + i, a.begin() + i + 3, b.begin() + i);
  return count;
}
void save(const std::string &path, const std::vector<unsigned char> &image) {
  std::ofstream out(path, std::ios::binary);
  out << "P6\n960 540\n255\n";
  for (int y = 539; y >= 0; --y)
    for (int x = 0; x < 960; ++x)
      out.write(
          reinterpret_cast<const char *>(image.data() + (y * 960 + x) * 4), 3);
  check(bool(out), "Enemy framebuffer export");
}
double capture(const re4::EnemyPreview &enemy, const re4::Matrix34 &placement) {
  // The exact production program/VAO/UBO remains bound after DrawEnemyGl.
  // Evaluate raw posed parts, inverse bind and world placement independently
  // in double precision; no production palette/parenting helper is reused.
  const auto count = enemy.skin().size();
  const auto bytes = count * 9 * sizeof(float);
  GLuint buffer;
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer);
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bytes, nullptr, GL_STREAM_READ);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer);
  glEnable(GL_RASTERIZER_DISCARD);
  glBeginTransformFeedback(GL_TRIANGLES);
  glDrawArrays(GL_TRIANGLES, 0, GLsizei(count));
  glEndTransformFeedback();
  glDisable(GL_RASTERIZER_DISCARD);
  check(glGetError() == GL_NO_ERROR, "Enemy transform feedback");
  const auto *observed = static_cast<const float *>(glMapBufferRange(
      GL_TRANSFORM_FEEDBACK_BUFFER, 0, bytes, GL_MAP_READ_BIT));
  check(observed != nullptr, "Enemy transform-feedback mapping");
  const auto &mesh = enemy.mesh();
  const auto &raw = enemy.rig().jointMatrices();
  const auto &bind = enemy.rig().bindCenters();
  check(raw.size() == 34 && bind.size() == 34,
        "Original normal EM12 raw-part count");
  double maximum = 0;
  for (std::size_t i = 0; i < count; ++i) {
    std::array<double, 3> local{}, normal{}, world{}, worldNormal{};
    const auto &skin = enemy.skin()[i];
    for (unsigned influence = 0; influence < 3; ++influence)
      for (unsigned row = 0; row < 3; ++row) {
        auto joint = skin.position.joints[influence];
        double point = raw.at(joint)[row * 4 + 3];
        for (unsigned col = 0; col < 3; ++col)
          point += double(raw[joint][row * 4 + col]) *
                   (mesh.vertices[i * 3 + col] - double(bind[joint][col]));
        local[row] += double(skin.position.rates[influence]) * point;
        joint = skin.normal.joints[influence];
        for (unsigned col = 0; col < 3; ++col)
          normal[row] += double(skin.normal.rates[influence]) *
                         raw.at(joint)[row * 4 + col] *
                         mesh.normals[i * 3 + col];
      }
    for (unsigned row = 0; row < 3; ++row) {
      world[row] = placement[row * 4 + 3];
      for (unsigned col = 0; col < 3; ++col) {
        world[row] += double(placement[row * 4 + col]) * local[col];
        worldNormal[row] += double(placement[row * 4 + col]) * normal[col];
      }
      for (unsigned offset : {0u, 3u, 6u})
        check(std::isfinite(observed[i * 9 + offset + row]),
              "Non-finite original enemy GPU result");
      maximum = std::max(maximum, std::abs(observed[i * 9 + row] - local[row]));
      maximum = std::max(
          maximum, std::abs(observed[i * 9 + 3 + row] - worldNormal[row]));
      maximum =
          std::max(maximum, std::abs(observed[i * 9 + 6 + row] - world[row]));
    }
  }
  check(glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER) == GL_TRUE,
        "Enemy transform-feedback corruption");
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
  glDeleteBuffers(1, &buffer);
  check(maximum < .025,
        "Enemy GPU differs from raw posed parts/weights/world placement");
  return maximum;
}
void alterMask(const re4::Texture &texture, GLuint name, int alpha) {
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, name);
  auto upload = [&](unsigned level, unsigned width, unsigned height,
                    const std::vector<std::uint8_t> &source) {
    auto data = source;
    if (alpha >= 0)
      for (std::size_t i = 3; i < data.size(); i += 4)
        data[i] = static_cast<std::uint8_t>(alpha);
    glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, width, height, GL_RGBA,
                    GL_UNSIGNED_BYTE, data.data());
  };
  upload(0, texture.width, texture.height, texture.rgba);
  for (std::size_t i = 0; i < texture.mips.size(); ++i) {
    const auto &mip = texture.mips[i];
    upload(i + 1, mip.width, mip.height, mip.rgba);
  }
  glActiveTexture(GL_TEXTURE0);
  check(glGetError() == GL_NO_ERROR, "Enemy original mask upload");
}
} // namespace

// Host-only GNU ld faults/capture. Android has no allocation or mask test hook.
extern "C" void __real_glBufferData(GLenum, GLsizeiptr, const void *, GLenum);
extern "C" void __wrap_glBufferData(GLenum target, GLsizeiptr size,
                                    const void *data, GLenum usage) {
  if (target == GL_UNIFORM_BUFFER && failUniform) {
    failUniform = false;
    ++injectedFailures;
    __real_glBufferData(target, -1, data, usage);
  } else {
    __real_glBufferData(target, size, data, usage);
  }
}
extern "C" void __real_glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei,
                                    GLint, GLenum, GLenum, const void *);
extern "C" void __wrap_glTexImage2D(GLenum target, GLint level, GLint internal,
                                    GLsizei width, GLsizei height, GLint border,
                                    GLenum format, GLenum type,
                                    const void *data) {
  __real_glTexImage2D(target, level, internal, width, height, border, format,
                      type, data);
  if (recordEnemyTextures && target == GL_TEXTURE_2D && level == 0) {
    GLint name;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &name);
    enemyTextures.push_back(GLuint(name));
  }
}

int main(int argc, char **argv) {
  try {
    if (argc != 5)
      throw std::runtime_error(
          "Usage: enemy_gl_check PL00.drs WEP01.drs EM12.drs OUTPUT.ppm");
    Egl egl;
    re4::LeonPreview leon(argv[1], argv[2]);
    re4::WeaponPreview gun(argv[2]);
    re4::EnemyPreview enemy(argv[3],
                            0x25); // Actual original global list index.
    check(enemy.mesh().included == 4 && enemy.mesh().textures.size() == 3 &&
              !enemy.mesh().fallbackTriangles &&
              !enemy.mesh().unresolvedAlphaTriangles,
          "Original EM12 body/head/hands and resolved material sources");
    leon.setPlacement({-86550, 830, -32960}, .731f);
    re4::SmdPlacement authored;
    authored.position = {-83950, 830, -32960};
    authored.rotation[1] = .0005752427954571154f;
    authored.scale = {1.03f, 1.03f, 1.03f};
    const auto placement = re4::PlacementMatrix(authored);
    const std::array<float, 3> center{-85350, 830, -32960};
    const float camera[16]{1.f / 2800, 0, 0,           0, 0, 1.f / 1500, 0, 0,
                           0,          0, -1.f / 6000, 0, 0, -.6f,       0, 1};
    re4::CreateActorGl(leon);
    re4::CreateWeaponGl(gun);
    recordEnemyTextures = true;
    re4::CreateEnemyGl(enemy);
    recordEnemyTextures = false;
    check(enemyTextures.size() == enemy.mesh().textures.size(),
          "Actual independent enemy texture uploads");
    auto draw = [&](bool body, bool weapon, bool ganado, bool normals = false) {
      glViewport(0, 0, 960, 540);
      glDepthMask(GL_TRUE);
      glClearColor(.035f, .055f, .075f, 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      if (body)
        re4::DrawActorGl(camera, center, 1, leon, normals);
      if (weapon)
        re4::DrawWeaponGl(camera, center, 1, gun,
                          leon.rig().jointMatrices().at(10), leon.placement(),
                          normals);
      if (ganado)
        re4::DrawEnemyGl(camera, center, 1, enemy, placement, normals);
      check(glGetError() == GL_NO_ERROR, "Three-instance original GPU draw");
      return pixels();
    };
    auto clear = draw(false, false, false);
    auto onlyEnemy = draw(false, false, true);
    const auto visible = different(clear, onlyEnemy);
    check(visible > 1000, "Original complete enemy model is not visible");
    auto all = draw(true, true, true);
    check(different(draw(true, true, false), all) > 1000,
          "Enemy GPU clobbered by body/weapon submission");
    check(different(draw(false, true, true), all) > 1000,
          "Body GPU clobbered by enemy submission");
    check(different(draw(true, false, true), all) > 5,
          "Gun GPU clobbered by enemy submission");
    check(draw(true, true, true, true) != all, "Enemy normal view absent");
    check(draw(true, true, true) == all, "Three-instance texture restore");
    save(argv[4], all);

    std::set<int> maskTextures;
    for (const auto &material : enemy.mesh().draws)
      if (material.alphaTest) {
        check(material.alphaTexture >= 0 &&
                  std::size_t(material.alphaTexture) < enemyTextures.size(),
              "Enemy original separate alpha texture missing");
        maskTextures.insert(material.alphaTexture);
      }
    // These exact body/head/hand BINs have no alpha-test materials. The
    // original shader branch must therefore ignore texture alpha, including
    // zero alpha; inventing a head/hand cutout loses legitimate geometry.
    check(maskTextures.empty(), "Type-0 EM12 introduces an unauthored mask");
    for (std::size_t i = 0; i < enemyTextures.size(); ++i)
      alterMask(enemy.mesh().textures[i], enemyTextures[i], 0);
    check(draw(false, false, true) == onlyEnemy,
          "Source alpha-off enemy materials unexpectedly discard texels");
    for (std::size_t i = 0; i < enemyTextures.size(); ++i)
      alterMask(enemy.mesh().textures[i], enemyTextures[i], -1);
    check(draw(false, false, true) == onlyEnemy,
          "Original enemy alpha textures/mips restore incorrectly");

    double maximum = 0;
    unsigned samples = 0, changed = 0;
    for (auto motion : {re4::EnemyMotion::Idle, re4::EnemyMotion::Walk,
                        re4::EnemyMotion::HitHead, re4::EnemyMotion::HitBody,
                        re4::EnemyMotion::Death}) {
      const auto clip = static_cast<unsigned>(motion);
      const auto last = enemy.rig().maxFrame(clip);
      check(last > 0, "Original enemy FCV extent missing");
      std::vector<unsigned char> first;
      for (float frame : {0.f, 3.f, last * .375f, last * .75f, last - .001f}) {
        enemy.sample(motion, frame);
        auto image = draw(true, true, true);
        if (first.empty())
          first = image;
        else
          changed += image != first;
        maximum = std::max(maximum, capture(enemy, placement));
        ++samples;
      }
      save(std::string(argv[4]) + "." + std::to_string(clip) + ".ppm", first);
      for (bool mirrored : {false, true}) {
        enemy.start(motion, mirrored);
        for (unsigned tick = 0; tick < 18; ++tick) {
          const auto step = enemy.advance();
          for (auto component : step.translation)
            check(std::isfinite(component),
                  "Enemy original root step is invalid");
          if (tick == 0 || tick == 5 || tick == 17) {
            draw(true, true, true);
            maximum = std::max(maximum, capture(enemy, placement));
            ++samples;
          }
        }
      }
    }
    check(changed >= 10, "Original enemy motions do not change GPU silhouette");
    enemy.start(re4::EnemyMotion::Death);
    bool dead = false;
    for (unsigned tick = 0; tick < 200; ++tick)
      if (enemy.advance().ended) {
        dead = true;
        break;
      }
    check(dead && enemy.rig().status().ended,
          "Original death sequence does not reach its terminal pose");
    auto corpse = draw(true, true, true);
    maximum = std::max(maximum, capture(enemy, placement));
    ++samples;
    save(std::string(argv[4]) + ".corpse.ppm", corpse);
    const auto rawPose = enemy.rig().jointMatrices();
    auto before = draw(true, true, true);
    for (bool bodyFailure : {false, true}) {
      failUniform = true;
      const auto previous = injectedFailures;
      bool failed = false;
      try {
        if (bodyFailure)
          re4::CreateActorGl(leon);
        else
          re4::CreateEnemyGl(enemy);
      } catch (const std::exception &) {
        failed = true;
      }
      check(failed && !failUniform && injectedFailures == previous + 1,
            "Actual body/enemy GPU allocation failure not exercised");
      check(draw(true, true, true) == before &&
                enemy.rig().jointMatrices() == rawPose,
            "GPU failure damaged existing body/gun/enemy pose or framebuffer");
    }
    re4::CreateActorGl(leon);
    check(draw(true, true, true) == draw(true, false, false),
          "Successful body replacement retains stale gun/enemy resources");
    re4::CreateWeaponGl(gun);
    re4::CreateEnemyGl(enemy);
    check(draw(true, true, true) == before,
          "Enemy GPU replacement lost original raw pose");
    re4::ForgetActorGl();
    egl.recreate();
    check(draw(true, true, true) == draw(false, false, false),
          "Forgotten context retains invalid actor GPU names");
    re4::CreateActorGl(leon);
    re4::CreateWeaponGl(gun);
    re4::CreateEnemyGl(enemy);
    check(draw(true, true, true) == before &&
              enemy.rig().jointMatrices() == rawPose,
          "Actual EGL context recreation lost body/gun/enemy pose");
    re4::DestroyActorGl();
    check(draw(true, true, true) == draw(false, false, false),
          "Actor teardown does not clear all three GPU instances");
    std::cout << "Enemy GPU PASS: " << samples << " poses, "
              << enemy.skin().size() << " corners, max raw/world difference "
              << maximum << " units; " << visible << " visible pixels; "
              << maskTextures.size()
              << " original alpha-test materials; "
                 "alpha-off branch, three-instance "
                 "rollback, replacement, context recreation; "
              << checks << " checks\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Enemy GPU FAIL: " << error.what() << '\n';
    return 1;
  }
}
