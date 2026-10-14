// Production GLES3 skinning and draw selection for original knife and kick assets.
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
    check(eglInitialize(display, &major, &minor), "Melee EGL initialization");
    check(eglBindAPI(EGL_OPENGL_ES_API), "Melee EGL ES API");
    const EGLint attributes[]{EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                              EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                              EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                              EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE};
    EGLint count{};
    check(eglChooseConfig(display, attributes, &config, 1, &count) && count,
          "Melee EGL configuration");
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
          "Melee EGL current context");
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
  check(glGetError() == GL_NO_ERROR, "Melee framebuffer read");
  return result;
}
void save(const std::string &path, const std::vector<unsigned char> &image) {
  std::ofstream stream(path, std::ios::binary);
  stream << "P6\n" << width << ' ' << height << "\n255\n";
  for (int y = height - 1; y >= 0; --y)
    for (int x = 0; x < width; ++x)
      stream.write(reinterpret_cast<const char *>(
          image.data() + (y * width + x) * 4), 3);
  check(bool(stream), "Melee framebuffer export");
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
        "Melee GPU owner dimensions");
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
  check(glGetError() == GL_NO_ERROR, "Melee transform feedback");
  const auto *observed = static_cast<const float *>(glMapBufferRange(
      GL_TRANSFORM_FEEDBACK_BUFFER, 0, bytes, GL_MAP_READ_BIT));
  check(observed != nullptr, "Melee transform-feedback mapping");
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
              "Melee GPU non-finite vertex");
      maximum = std::max(maximum, std::abs(observed[corner * 9 + row] - local[row]));
      maximum = std::max(maximum,
          std::abs(observed[corner * 9 + 3 + row] - worldNormal[row]));
      maximum = std::max(maximum,
          std::abs(observed[corner * 9 + 6 + row] - world[row]));
    }
  }
  check(glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER) == GL_TRUE,
        "Melee transform-feedback corruption");
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
  glDeleteBuffers(1, &buffer);
  check(maximum < .025, "Melee raw-part/skin/world GPU mismatch");
  maximumDifference = std::max(maximumDifference, maximum);
  ++captures;
}
void checkSubmissions(const re4::StaticScene &mesh,
                      const re4::EnemyPreview *enemy = nullptr,
                      const re4::LeonPreview *leon = nullptr) {
  std::size_t expected{}, observed{};
  std::vector<bool> coveredVertices(mesh.vertices.size() / 3);
  for (const auto &draw : mesh.draws)
    if ((!enemy || enemy->visiblePart(draw.work)) &&
        (!leon || leon->visiblePart(draw.work)))
      expected += draw.count;
  for (const auto &submission : submissions) {
    check(submission.first >= 0 && submission.count > 0,
          "Melee production draw range");
    const auto first = std::size_t(submission.first);
    const auto last = first + std::size_t(submission.count);
    check(last <= mesh.vertices.size() / 3, "Melee draw beyond GPU owner");
    for (auto vertex = first; vertex < last; ++vertex) {
      check(!coveredVertices[vertex], "Melee vertex submitted more than once");
      coveredVertices[vertex] = true;
    }
    observed += submission.count;
    std::size_t covered{};
    for (const auto &draw : mesh.draws) {
      const auto begin = std::max(first, draw.first);
      const auto end = std::min(last, draw.first + draw.count);
      if (begin < end) {
        check((!enemy || enemy->visiblePart(draw.work)) &&
                  (!leon || leon->visiblePart(draw.work)),
              "Hidden original hand variant submitted");
        covered += end - begin;
      }
    }
    check(covered == std::size_t(submission.count), "Melee unmapped draw range");
  }
  check(observed == expected && observed != 0,
        "Melee selected production vertex count");
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
          "Melee CPU pose/state/inventory changed during context recreation");
  }
};
} // namespace

extern "C" void __real_glDrawArrays(GLenum, GLint, GLsizei);
extern "C" void __wrap_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  if (recordDraws) {
    check(mode == GL_TRIANGLES, "Melee production primitive");
    submissions.push_back({first, count});
  }
  __real_glDrawArrays(mode, first, count);
}

int main(int argc, char **argv) {
  try {
    if (argc != 4 && argc != 5)
      throw std::runtime_error(
          "Usage: knife_render_gl_check PL00.drs WEP01.drs EM12.drs [OUTPUT.ppm]");
    Egl egl;
    re4::LeonPreview leon(argv[1], argv[2], argv[3], true);
    re4::EnemyPreview enemy(argv[3], 0x25, true);
    check(leon.hasMeleeMotions() && enemy.hasMeleeMotions() && leon.weapon(),
          "Melee source motion owners missing");
    check(leon.renderSkin().size() > leon.skin().size() &&
              leon.renderMesh().included == leon.mesh().included + 1,
          "Original PL00 BIN9 knife not appended");
    bool hasBlade = false;
    for (const auto &draw : leon.renderMesh().draws)
      hasBlade |= draw.work == 9 && draw.count > 0;
    check(hasBlade, "PL00 knife source draw boundary missing");
    const re4::Sat sat = floorFixture();
    const re4::SatCollision world(sat);
    re4::WalkProbe probe(world);
    check(probe.place({-700, 0, 0}), "Melee test floor placement");
    leon.setPlacement(probe.position(), .731f);
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
    bounds(leon.renderMesh(), leon.renderSkin(), leon.rig(), leon.placement());
    bounds(enemy.renderMesh(), enemy.renderSkin(), enemy.rig(), placement);
    for (unsigned row = 0; row < 3; ++row)
      center[row] = float((maximum[row] + minimum[row]) / 2);
    const auto extent = std::max({maximum[0] - minimum[0],
        (maximum[1] - minimum[1]) * double(width) / height, 100.}) * 1.08;
    const float camera[16]{float(2 / extent), 0, 0, 0,
        0, float(2 * width / (extent * height)), 0, 0,
        0, 0, float(-1 / std::max(extent, 10000.)), 0, 0, 0, 0, 1};
    auto upload = [&] {
      re4::CreateActorGl(leon);
      re4::CreateWeaponGl(*leon.weapon());
      re4::CreateEnemyGl(enemy);
      check(glGetError() == GL_NO_ERROR, "Melee independent GPU uploads");
    };
    upload();
    auto frame = [&] {
      glViewport(0, 0, width, height);
      glDepthMask(GL_TRUE);
      glClearColor(.035f, .055f, .075f, 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      submissions.clear(); recordDraws = true;
      re4::DrawActorGl(camera, center, 1, leon, false);
      recordDraws = false;
      checkSubmissions(leon.renderMesh(), nullptr, &leon);
      GLint bodyProgram{};
      glGetIntegerv(GL_CURRENT_PROGRAM, &bodyProgram);
      capture(leon.renderMesh(), leon.renderSkin(), leon.rig(), leon.placement());
      GLint weaponProgram{};
      submissions.clear(); recordDraws = true;
      if (leon.weaponVisible())
        re4::DrawWeaponGl(camera, center, 1, *leon.weapon(),
            leon.rig().jointMatrices().at(10), leon.placement(), false);
      recordDraws = false;
      if (leon.weaponVisible()) {
        checkSubmissions(leon.weapon()->mesh());
        glGetIntegerv(GL_CURRENT_PROGRAM, &weaponProgram);
        check(bodyProgram != weaponProgram, "Melee shared GPU program");
        capture(leon.weapon()->mesh(), leon.weapon()->skin(),
            leon.weapon()->rig(), re4::WeaponAttachmentMatrix(
                leon.rig().jointMatrices().at(10), leon.placement()));
      } else check(submissions.empty(), "Melee hidden gun submitted");
      submissions.clear(); recordDraws = true;
      re4::DrawEnemyGl(camera, center, 1, enemy, placement, false);
      recordDraws = false;
      checkSubmissions(enemy.renderMesh(), &enemy);
      GLint enemyProgram{};
      glGetIntegerv(GL_CURRENT_PROGRAM, &enemyProgram);
      check(bodyProgram && enemyProgram && bodyProgram != enemyProgram &&
                (!weaponProgram || weaponProgram != enemyProgram),
            "Melee GPU owners not independent");
      capture(enemy.renderMesh(), enemy.renderSkin(), enemy.rig(), placement);
      check(glGetError() == GL_NO_ERROR, "Melee production draw error");
      return pixels();
    };
    check(!leon.bladeVisible() && leon.weaponVisible(), "Idle knife/gun state");
    frame();
    const auto magazine = leon.magazine(), reserve = leon.reserve();
    re4::PlayerActionInput input;
    input.knifeHeld = true;
    auto step = [&](unsigned count) {
      for (unsigned t = 0; t < count; ++t)
        leon.step(probe, 0, 0, false, false, 1. / 30, input);
    };
    step(4);
    check(leon.action() == re4::PlayerAction::KnifeDraw && !leon.bladeVisible() &&
              !leon.weaponVisible(), "Knife became visible before source frame4");
    frame();
    step(2);
    check(leon.bladeVisible() && !leon.weaponVisible(),
          "Source draw frame4 did not reveal knife");
    frame();
    unsigned ticks{};
    while (leon.action() != re4::PlayerAction::KnifeReady && ticks++ < 3000)
      step(1);
    check(leon.action() == re4::PlayerAction::KnifeReady && leon.bladeVisible(),
          "Source knife draw did not reach ready");
    step(30); frame();
    input.fireHeld = true;
    step(1);
    check(leon.action() == re4::PlayerAction::KnifeSlash &&
              leon.bladeVisible() && !leon.weaponVisible(), "Knife slash state");
    step(6);
    check(leon.action() == re4::PlayerAction::KnifeSlash,
          "Source knife slash ended before authored hit window");
    const auto slashing = frame();
    const CpuSnapshot snapshot(leon, enemy);
    const bool blade = leon.bladeVisible();
    const auto renderCount = leon.renderSkin().size();
    egl.drop(); re4::ForgetActorGl(); egl.create(); upload();
    snapshot.verify(leon, enemy);
    check(blade == leon.bladeVisible() && renderCount == leon.renderSkin().size(),
          "Knife state/geometry changed across context recreation");
    check(frame() == slashing, "Knife framebuffer changed after context recreation");
    if (argc == 5) save(argv[4], slashing);
    input.fireHeld = false;
    ticks = 0;
    while (leon.action() == re4::PlayerAction::KnifeSlash && ticks++ < 3000)
      step(1);
    check(leon.action() == re4::PlayerAction::KnifeReady,
          "Knife slash did not return to ready");
    step(40);
    // Exercise production GPU's original cMot3 base/+/- poses for both triplets.
    for (const auto clips : {std::array<unsigned, 3>{21,22,23},
                             std::array<unsigned, 3>{24,25,26}})
      for (float rate : {-1.f,-.35f,0.f,.35f,1.f}) {
        leon.rig().sampleBlend3(clips, rate, 7);
        frame();
      }
    // Restart the runtime stance after explicit pose sampling, then sheath.
    leon.rig().startBlend3({21,22,23}, 0, 3);
    input.knifeHeld = false;
    step(1);
    check(leon.action() == re4::PlayerAction::KnifeHolster,
          "Knife release did not start source sheath");
    frame();
    step(6);
    check(!leon.bladeVisible(), "Sheath frame4 did not hide knife");
    frame();
    ticks = 0;
    while (leon.action() == re4::PlayerAction::KnifeHolster && ticks++ < 3000)
      step(1);
    check(leon.action() == re4::PlayerAction::Footwork && leon.weaponVisible() &&
              !leon.bladeVisible(), "Sheath did not restore gun/footwork");
    check(leon.magazine() == magazine && leon.reserve() == reserve && !leon.shots(),
          "Knife consumed ammunition");
    leon.beginKick({leon.feet()[0] + 500, leon.feet()[1], leon.feet()[2] + 500});
    step(17);
    check(leon.action() == re4::PlayerAction::Kick && !leon.weaponVisible() &&
              !leon.bladeVisible(), "Source front kick weapon selection");
    frame();
    for (unsigned clip : {11u,12u,13u,14u,15u,16u,17u,20u,21u,22u,23u,24u,25u})
      for (bool mirror : {false,true}) {
        enemy.rig().reset();
        enemy.start(static_cast<re4::EnemyMotion>(clip), mirror);
        for (unsigned t = 0; t < 9; ++t) enemy.advance();
        check(enemy.mirrored() == (mirror && clip != 22 && clip != 23) && !enemy.openHands(),
              "Melee fall mirror/hand selection");
        frame();
        for (unsigned t = 0; t < enemy.rig().cycle(clip).sequenceFrames + 16; ++t)
          enemy.advance();
        check(enemy.rig().status().ended, "Melee fall/wake did not end");
        if (clip < 17 || (clip >= 20 && clip < 24)) {
          const auto heldPose = enemy.rig().jointMatrices();
          enemy.start(re4::EnemyMotion::DownWakeWait);
          for (unsigned t = 0; t < 10; ++t) enemy.advance();
          check(enemy.rig().jointMatrices() == heldPose && !enemy.openHands(),
                "Melee ended fall/down wait pose changed");
        }
        frame();
      }
    re4::DestroyActorGl();
    check(glGetError() == GL_NO_ERROR, "Melee GPU cleanup");
    std::cout << "PASS original production blade/hands + kick/blow/wake GLES "
              << checks << " checks, " << captures << " complete owner captures, maxDelta="
              << maximumDifference << ", playerRenderCorners=" << leon.renderSkin().size()
              << ", enemyRenderCorners=" << enemy.renderSkin().size() << '\n';
  } catch (const std::exception &e) {
    recordDraws = false;
    std::cerr << e.what() << '\n';
    return 1;
  }
}
