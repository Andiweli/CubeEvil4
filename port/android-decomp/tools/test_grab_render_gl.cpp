// Production GLES3 skinning and model selection for the paired front grab.
// Supply locally extracted original assets, or explicit synthetic fixtures.
// Build actor_gl.cpp with RE4_VERIFY_ACTOR_GL/RE4_VERIFY_ENEMY_GL and link
// --wrap=glDrawArrays. No test hook is compiled into the Android application.
#include "actor_gl.h"
#include "walk_probe.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
constexpr int width = 960, height = 540;
unsigned checks{}, captures{};
double maximumDifference{};
struct Submission { GLint first; GLsizei count; };
bool recordDraws{};
std::vector<Submission> submissions;
void check(bool condition, const char *message) {
  if (!condition)
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
    EGLint major{}, minor{};
    check(eglInitialize(display, &major, &minor), "Grab EGL initialization");
    check(eglBindAPI(EGL_OPENGL_ES_API), "Grab EGL ES API");
    const EGLint attributes[]{EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                              EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                              EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                              EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE};
    EGLint count{};
    check(eglChooseConfig(display, attributes, &config, 1, &count) && count,
          "Grab EGL configuration");
    create();
  }
  void create() {
    const EGLint contextAttributes[]{EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    const EGLint surfaceAttributes[]{EGL_WIDTH, width, EGL_HEIGHT, height,
                                     EGL_NONE};
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    surface = eglCreatePbufferSurface(display, config, surfaceAttributes);
    check(context != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE &&
              eglMakeCurrent(display, surface, surface, context),
          "Grab EGL current context");
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
  ~Egl() { drop(); eglTerminate(display); }
};
void half(std::vector<std::uint8_t> &bytes, std::size_t offset, unsigned value) {
  bytes.at(offset) = value >> 8;
  bytes.at(offset + 1) = value;
}
void word(std::vector<std::uint8_t> &bytes, std::size_t offset, unsigned value) {
  half(bytes, offset, value >> 16);
  half(bytes, offset + 2, value);
}
re4::Sat floorFixture() {
  const std::vector<re4::CollisionVec> vertices{{-100000, 0, -100000},
      {100000, 0, -100000}, {100000, 0, 100000}, {-100000, 0, 100000}};
  std::vector<std::uint8_t> bytes(20 + vertices.size() * 12 + 12 + 12 + 40);
  bytes[0] = 32;
  half(bytes, 2, vertices.size());
  half(bytes, 4, 1);
  half(bytes, 6, 1);
  half(bytes, 10, 2);
  half(bytes, 12, 2);
  std::size_t offset = 20;
  auto floating = [&](float value) {
    unsigned bits;
    std::memcpy(&bits, &value, sizeof(bits));
    word(bytes, offset, bits);
    offset += 4;
  };
  for (auto point : vertices)
    for (auto value : point)
      floating(float(value));
  for (float value : {0.f, 1.f, 0.f})
    floating(value);
  offset += 12;
  const unsigned indices[]{0, 2, 1, 0, 3, 2};
  for (unsigned triangle = 0; triangle < 2; ++triangle) {
    for (unsigned corner = 0; corner < 3; ++corner)
      half(bytes, offset + corner * 2, indices[triangle * 3 + corner]);
    offset += 20;
  }
  return re4::Sat(bytes);
}
std::vector<unsigned char> pixels() {
  std::vector<unsigned char> result(width * height * 4);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
  check(glGetError() == GL_NO_ERROR, "Grab framebuffer read");
  return result;
}
void save(const std::string &path, const std::vector<unsigned char> &image) {
  std::ofstream stream(path, std::ios::binary);
  stream << "P6\n" << width << ' ' << height << "\n255\n";
  for (int y = height - 1; y >= 0; --y)
    for (int x = 0; x < width; ++x)
      stream.write(reinterpret_cast<const char *>(
          image.data() + (y * width + x) * 4), 3);
  check(bool(stream), "Grab framebuffer export");
}
std::array<double, 3> skinPoint(const re4::StaticScene &mesh,
    const std::vector<re4::SkinCorner> &skin, const re4::AnimationRig &rig,
    std::size_t corner) {
  std::array<double, 3> result{};
  const auto &raw = rig.jointMatrices();
  const auto &bind = rig.bindCenters();
  for (unsigned influence = 0; influence < 3; ++influence) {
    const auto joint = skin.at(corner).position.joints[influence];
    for (unsigned row = 0; row < 3; ++row) {
      double point = raw.at(joint)[row * 4 + 3];
      for (unsigned col = 0; col < 3; ++col)
        point += double(raw[joint][row * 4 + col]) *
            (mesh.vertices.at(corner * 3 + col) - double(bind.at(joint)[col]));
      result[row] += double(skin[corner].position.rates[influence]) * point;
    }
  }
  return result;
}
void capture(const re4::StaticScene &mesh,
    const std::vector<re4::SkinCorner> &skin, const re4::AnimationRig &rig,
    const re4::Matrix34 &placement) {
  // Draw*Gl leaves the production program, VAO and UBO bound. Capture every
  // uploaded vertex, including the currently hidden authored hand variant.
  check(!skin.empty() && skin.size() == mesh.vertices.size() / 3 &&
            rig.jointMatrices().size() == rig.bindCenters().size() &&
            rig.jointMatrices().size() <= 128,
        "Grab GPU owner dimensions");
  const auto bytes = skin.size() * 9 * sizeof(float);
  GLuint buffer{};
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer);
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bytes, nullptr, GL_STREAM_READ);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer);
  glEnable(GL_RASTERIZER_DISCARD);
  glBeginTransformFeedback(GL_TRIANGLES);
  glDrawArrays(GL_TRIANGLES, 0, GLsizei(skin.size()));
  glEndTransformFeedback();
  glDisable(GL_RASTERIZER_DISCARD);
  check(glGetError() == GL_NO_ERROR, "Grab transform feedback");
  const auto *observed = static_cast<const float *>(glMapBufferRange(
      GL_TRANSFORM_FEEDBACK_BUFFER, 0, bytes, GL_MAP_READ_BIT));
  check(observed != nullptr, "Grab transform-feedback mapping");
  const auto &raw = rig.jointMatrices();
  double maximum = 0;
  for (std::size_t corner = 0; corner < skin.size(); ++corner) {
    const auto local = skinPoint(mesh, skin, rig, corner);
    std::array<double, 3> normal{}, world{}, worldNormal{};
    for (unsigned influence = 0; influence < 3; ++influence) {
      const auto joint = skin[corner].normal.joints[influence];
      for (unsigned row = 0; row < 3; ++row)
        for (unsigned col = 0; col < 3; ++col)
          normal[row] += double(skin[corner].normal.rates[influence]) *
              raw.at(joint)[row * 4 + col] * mesh.normals.at(corner * 3 + col);
    }
    for (unsigned row = 0; row < 3; ++row) {
      world[row] = placement[row * 4 + 3];
      for (unsigned col = 0; col < 3; ++col) {
        world[row] += double(placement[row * 4 + col]) * local[col];
        worldNormal[row] += double(placement[row * 4 + col]) * normal[col];
      }
      for (unsigned offset : {0u, 3u, 6u})
        check(std::isfinite(observed[corner * 9 + offset + row]),
              "Grab GPU non-finite vertex");
      maximum = std::max(maximum, std::abs(observed[corner * 9 + row] - local[row]));
      maximum = std::max(maximum,
          std::abs(observed[corner * 9 + 3 + row] - worldNormal[row]));
      maximum = std::max(maximum,
          std::abs(observed[corner * 9 + 6 + row] - world[row]));
    }
  }
  check(glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER) == GL_TRUE,
        "Grab transform-feedback corruption");
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
  glDeleteBuffers(1, &buffer);
  check(maximum < .025, "Grab raw-part/skin/world GPU mismatch");
  maximumDifference = std::max(maximumDifference, maximum);
  ++captures;
}
void checkSubmissions(const re4::StaticScene &mesh,
                      const re4::EnemyPreview *enemy = nullptr) {
  std::size_t expected{}, observed{};
  std::vector<bool> coveredVertices(mesh.vertices.size() / 3);
  for (const auto &draw : mesh.draws)
    if (!enemy || enemy->visiblePart(draw.work))
      expected += draw.count;
  for (const auto &submission : submissions) {
    check(submission.first >= 0 && submission.count > 0,
          "Grab production draw range");
    const auto first = std::size_t(submission.first);
    const auto last = first + std::size_t(submission.count);
    check(last <= mesh.vertices.size() / 3, "Grab draw beyond GPU owner");
    for (auto vertex = first; vertex < last; ++vertex) {
      check(!coveredVertices[vertex], "Grab vertex submitted more than once");
      coveredVertices[vertex] = true;
    }
    observed += submission.count;
    std::size_t covered{};
    for (const auto &draw : mesh.draws) {
      const auto begin = std::max(first, draw.first);
      const auto end = std::min(last, draw.first + draw.count);
      if (begin < end) {
        check(!enemy || enemy->visiblePart(draw.work),
              "Hidden original hand variant submitted");
        covered += end - begin;
      }
    }
    check(covered == std::size_t(submission.count), "Grab unmapped draw range");
  }
  check(observed == expected && observed != 0,
        "Grab selected production vertex count");
}
struct CpuSnapshot {
  std::vector<re4::Matrix34> player, weapon, enemy;
  re4::AnimationStatus playerState{}, weaponState{}, enemyState{};
  std::array<double, 3> feet{};
  float heading{};
  re4::PlayerAction action{};
  re4::EnemyMotion motion{};
  unsigned life{}, magazine{}, reserve{}, shots{};
  bool openHands{}, mirrored{}, weaponVisible{};
  CpuSnapshot(const re4::LeonPreview &leon, const re4::EnemyPreview &ganado)
      : player(leon.rig().jointMatrices()),
        weapon(leon.weapon()->rig().jointMatrices()),
        enemy(ganado.rig().jointMatrices()), playerState(leon.rig().status()),
        weaponState(leon.weapon()->rig().status()), enemyState(ganado.rig().status()),
        feet(leon.feet()), heading(leon.heading()), action(leon.action()),
        motion(ganado.motion()), life(leon.life()), magazine(leon.magazine()),
        reserve(leon.reserve()), shots(leon.shots()), openHands(ganado.openHands()),
        mirrored(ganado.mirrored()), weaponVisible(leon.weaponVisible()) {}
  void verify(const re4::LeonPreview &leon, const re4::EnemyPreview &ganado) const {
    const auto same = [](const re4::AnimationStatus &a,
                          const re4::AnimationStatus &b) {
      return a.curveFrame == b.curveFrame && a.sequenceFrame == b.sequenceFrame &&
             a.state == b.state && a.ended == b.ended;
    };
    check(player == leon.rig().jointMatrices() &&
              weapon == leon.weapon()->rig().jointMatrices() &&
              enemy == ganado.rig().jointMatrices() &&
              same(playerState, leon.rig().status()) &&
              same(weaponState, leon.weapon()->rig().status()) &&
              same(enemyState, ganado.rig().status()) &&
              feet == leon.feet() && heading == leon.heading() &&
              action == leon.action() && motion == ganado.motion() &&
              life == leon.life() && magazine == leon.magazine() &&
              reserve == leon.reserve() && shots == leon.shots() &&
              openHands == ganado.openHands() && mirrored == ganado.mirrored() &&
              weaponVisible == leon.weaponVisible(),
          "Grab CPU pose/state/inventory changed during context recreation");
  }
};
} // namespace

extern "C" void __real_glDrawArrays(GLenum, GLint, GLsizei);
extern "C" void __wrap_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  if (recordDraws) {
    check(mode == GL_TRIANGLES, "Grab production primitive");
    submissions.push_back({first, count});
  }
  __real_glDrawArrays(mode, first, count);
}

int main(int argc, char **argv) {
  try {
    if (argc != 4 && argc != 5)
      throw std::runtime_error(
          "Usage: grab_render_gl_check PL00.drs WEP01.drs EM12.drs [OUTPUT.ppm]");
    Egl egl;
    re4::LeonPreview leon(argv[1], argv[2], argv[3]);
    re4::EnemyPreview enemy(argv[3]);
    check(enemy.renderSkin().size() > enemy.skin().size() &&
              enemy.renderMesh().included == enemy.mesh().included + 2,
          "Grab open-hand vertices not appended to the GPU owner");
    const re4::Sat sat = floorFixture();
    const re4::SatCollision world(sat);
    re4::WalkProbe probe(world);
    check(probe.place({-700, 0, 0}), "Grab authored test floor placement");
    leon.setPlacement(probe.position(), .731f);
    check(leon.hasGrabMotions() && leon.weapon() && leon.weaponVisible(),
          "Paired original player/enemy clips unavailable");
    re4::SmdPlacement enemyPlacement;
    enemyPlacement.position = {700, 0, 0};
    enemyPlacement.rotation[1] = -.412f;
    enemyPlacement.scale = {1.03f, 1.03f, 1.03f};
    const auto placement = re4::PlacementMatrix(enemyPlacement);
    std::array<float, 3> center{};
    std::array<double, 3> minimum{}, maximum{};
    minimum.fill(std::numeric_limits<double>::infinity());
    maximum.fill(-std::numeric_limits<double>::infinity());
    auto bounds = [&](const re4::StaticScene &mesh,
        const std::vector<re4::SkinCorner> &skin, const re4::AnimationRig &rig,
        const re4::Matrix34 &matrix) {
      for (std::size_t i = 0; i < skin.size(); ++i) {
        const auto local = skinPoint(mesh, skin, rig, i);
        for (unsigned row = 0; row < 3; ++row) {
          double point = matrix[row * 4 + 3];
          for (unsigned col = 0; col < 3; ++col)
            point += double(matrix[row * 4 + col]) * local[col];
          minimum[row] = std::min(minimum[row], point);
          maximum[row] = std::max(maximum[row], point);
        }
      }
    };
    bounds(leon.mesh(), leon.skin(), leon.rig(), leon.placement());
    bounds(enemy.renderMesh(), enemy.renderSkin(), enemy.rig(), placement);
    for (unsigned row = 0; row < 3; ++row)
      center[row] = float((maximum[row] + minimum[row]) / 2);
    const auto extent = std::max({maximum[0] - minimum[0],
        (maximum[1] - minimum[1]) * double(width) / height, 100.}) * .8;
    const float camera[16]{float(2 / extent), 0, 0, 0,
        0, float(2 * width / (extent * height)), 0, 0,
        0, 0, float(-1 / std::max(extent, 10000.)), 0, 0, 0, 0, 1};
    auto upload = [&] {
      re4::CreateActorGl(leon);
      re4::CreateWeaponGl(*leon.weapon());
      re4::CreateEnemyGl(enemy);
      check(glGetError() == GL_NO_ERROR, "Grab three owning GPU uploads");
    };
    upload();
    auto frame = [&](bool feedback) {
      glViewport(0, 0, width, height);
      glDepthMask(GL_TRUE);
      glClearColor(.035f, .055f, .075f, 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      submissions.clear();
      recordDraws = true;
      re4::DrawActorGl(camera, center, 1, leon, false);
      recordDraws = false;
      checkSubmissions(leon.mesh());
      GLint bodyProgram{};
      glGetIntegerv(GL_CURRENT_PROGRAM, &bodyProgram);
      if (feedback)
        capture(leon.mesh(), leon.skin(), leon.rig(), leon.placement());
      GLint weaponProgram{};
      submissions.clear();
      recordDraws = true;
      if (leon.weaponVisible())
        re4::DrawWeaponGl(camera, center, 1, *leon.weapon(),
            leon.rig().jointMatrices().at(10), leon.placement(), false);
      recordDraws = false;
      if (leon.weaponVisible()) {
        checkSubmissions(leon.weapon()->mesh());
        glGetIntegerv(GL_CURRENT_PROGRAM, &weaponProgram);
        check(bodyProgram != weaponProgram, "Grab weapon shares body program");
        if (feedback)
          capture(leon.weapon()->mesh(), leon.weapon()->skin(),
              leon.weapon()->rig(), re4::WeaponAttachmentMatrix(
                  leon.rig().jointMatrices().at(10), leon.placement()));
      } else {
        check(submissions.empty(), "Held/recovering weapon was submitted");
      }
      submissions.clear();
      recordDraws = true;
      re4::DrawEnemyGl(camera, center, 1, enemy, placement, false);
      recordDraws = false;
      checkSubmissions(enemy.renderMesh(), &enemy);
      GLint enemyProgram{};
      glGetIntegerv(GL_CURRENT_PROGRAM, &enemyProgram);
      check(bodyProgram && enemyProgram && bodyProgram != enemyProgram &&
                (!weaponProgram || weaponProgram != enemyProgram),
            "Grab three GPU programs are not independent");
      if (feedback)
        capture(enemy.renderMesh(), enemy.renderSkin(), enemy.rig(), placement);
      check(glGetError() == GL_NO_ERROR, "Grab production scene draw");
      return pixels();
    };
    const auto idle = frame(true);
    auto advance = [&](unsigned count) {
      for (unsigned i = 0; i < count; ++i) {
        enemy.advance();
        leon.step(probe, 0, 0, false, false, 1. / 30);
      }
    };
    leon.beginGrab(.2f, {40, 0, -25});
    enemy.start(re4::EnemyMotion::NeckHang);
    advance(10);
    check(!leon.weaponVisible() && leon.action() == re4::PlayerAction::Grabbed &&
              enemy.openHands(), "Grab hold/hand/weapon state");
    const auto held = frame(true);
    const CpuSnapshot snapshot(leon, enemy);
    egl.drop();
    re4::ForgetActorGl();
    egl.create();
    upload();
    snapshot.verify(leon, enemy);
    check(frame(true) == held, "Held framebuffer changed after context recreation");
    if (argc == 5)
      save(argv[4], held);

    leon.releaseGrab(false);
    enemy.start(re4::EnemyMotion::ReleaseFailed);
    advance(8);
    check(leon.action() == re4::PlayerAction::GrabRelease &&
              !leon.weaponVisible() && enemy.openHands(),
          "Failed release/hand/weapon state");
    frame(true);
    unsigned ticks{};
    while (leon.action() == re4::PlayerAction::GrabRelease && ticks++ < 3000)
      advance(1);
    check(leon.action() == re4::PlayerAction::Recover && !leon.weaponVisible(),
          "Failed release did not enter hidden-weapon recovery");
    enemy.start(re4::EnemyMotion::Idle);
    frame(true);
    ticks = 0;
    while (leon.action() == re4::PlayerAction::Recover && ticks++ < 3000)
      advance(1);
    check(leon.action() == re4::PlayerAction::Footwork && leon.weaponVisible(),
          "Weapon did not return after original get-up");
    frame(true);

    leon.beginGrab(0, {});
    enemy.start(re4::EnemyMotion::NeckHang);
    advance(8);
    leon.releaseGrab(true);
    enemy.start(re4::EnemyMotion::ThrowOff);
    advance(8);
    check(leon.action() == re4::PlayerAction::GrabEscape &&
              !leon.weaponVisible() && enemy.openHands(),
          "Successful escape/hand/weapon state");
    frame(true);
    ticks = 0;
    while ((!enemy.rig().status().ended || leon.grabbed()) && ticks++ < 3000)
      advance(1);
    check(enemy.rig().status().ended && !leon.grabbed() && leon.weaponVisible(),
          "Original successful escape/ThrowOff did not end");
    enemy.start(re4::EnemyMotion::DownWakeWait);
    check(!enemy.openHands(), "Down wait retained grab hand model");
    frame(true);
    enemy.start(re4::EnemyMotion::Wake);
    advance(8);
    check(!enemy.openHands(), "Original Wake retained grab hand model");
    frame(true);
    enemy.start(re4::EnemyMotion::Wake, true);
    advance(8);
    check(enemy.mirrored(), "Original mirrored Wake missing");
    frame(true);
    check(!idle.empty(), "Grab framebuffer missing");
    re4::DestroyActorGl();
    check(glGetError() == GL_NO_ERROR, "Grab three GPU teardown");
    std::cout << "Grab production GLES PASS: " << checks << " checks, "
              << captures << " full GPU owner captures, max world/skin error "
              << maximumDifference << "; enemy render corners "
              << enemy.renderSkin().size() << ", hand variants selected; "
                 "hold/fail/recover/escape/wake/context preserved\n";
  } catch (const std::exception &error) {
    recordDraws = false;
    std::cerr << error.what() << '\n';
    return 1;
  }
}
