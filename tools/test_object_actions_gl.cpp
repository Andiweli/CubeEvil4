#include "object_actions.h"
#include "destruction_effects.h"
#include "scene_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using Bytes = std::vector<std::uint8_t>;
std::vector<GLint> submitted;
unsigned checks;
struct FragmentCapture {
  bool seen{};
  std::array<float, 16> model{};
  std::array<float, 9> normal{};
  std::array<float, 3> color{};
  float alpha{};
  GLboolean depthWrite{};
  GLint depthFunction{}, blendSource{}, blendDestination{};
} fragmentCapture;
void require(bool value, const char *message) {
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}
void half(Bytes &b, std::size_t p, unsigned value) {
  b.at(p) = value >> 8;
  b.at(p + 1) = value;
}
void word(Bytes &b, std::size_t p, unsigned value) {
  half(b, p, value >> 16);
  half(b, p + 2, value);
}
void real(Bytes &b, std::size_t p, float value) {
  unsigned bits;
  std::memcpy(&bits, &value, 4);
  word(b, p, bits);
}
re4::RoomArchive room() {
  Bytes b(32 + 16 + 2 * 156);
  word(b, 0, 1);
  word(b, 16, 32);
  word(b, 20, 0x49544100);
  word(b, 32, 0x49544100);
  half(b, 36, 0x105);
  half(b, 38, 2);
  for (unsigned i = 0; i < 2; ++i) {
    const auto p = 48 + i * 156;
    b[p + 0x34] = 3;
    b[p + 0x35] = 3;
    b[p + 0x36] = i;
    b[p + 0x37] = 1;
    b[p + 0x38] = 8;
    b[p + 0x39] = 1;
    b[p + 0x44] = 8;
    b[p + 0x46] = 2;
    b[p + 0x47] = 20 + i;
    half(b, p + 0x78, i ? 6 : 0x1005);
  }
  return re4::RoomArchive(b);
}
std::vector<re4::RoomObjectPlacement> placements() {
  Bytes b(16 + 80);
  half(b, 0, 2);
  for (unsigned i = 0; i < 2; ++i) {
    half(b, 16 + i * 40, 1);
    half(b, 18 + i * 40, 20 + i);
    real(b, 52 + i * 40, i * 2000.f);
  }
  return re4::ReadRoomObjectPlacements(b);
}
re4::StaticScene mesh() {
  re4::StaticScene scene;
  for (unsigned i = 0; i < 6; ++i) {
    const auto x = -1100 + i * 400.f;
    scene.vertices.insert(scene.vertices.end(),
                            {x - 250, 1100, 0, x + 250, 1100, 0, x, 1800, 0});
    scene.normals.insert(scene.normals.end(), {0, 0, -1, 0, 0, -1, 0, 0, -1});
    scene.uv.insert(scene.uv.end(), {0, 0, 1, 0, .5f, 1});
    scene.colors.insert(scene.colors.end(), 12, 255);
    re4::SceneDraw draw;
    draw.first = i * 3;
    draw.count = 3;
    draw.blendMode = 4;
    draw.object = 0;
    // Deliberately identical materials/order objects: missing ownership in
    // batching would merge these and reveal or hide neighboring geometry.
    draw.roomObjectSlot = i < 2 ? int(20 + i) : -1;
    draw.roomItemIndex = i == 2 || i == 3 ? 0 : -1;
    draw.roomItemId = i == 2 ? 4 : i == 3 ? 6 : -1;
    draw.destructionMesh = i == 5 ? 0 : -1;
    scene.draws.push_back(draw);
  }
  scene.minimum = {-1500, 1000, -1};
  scene.maximum = {1500, 1900, 1};
  scene.triangles = 6;
  return scene;
}
void draw(const re4::SceneProbe *probe, const std::vector<GLint> &expected) {
  submitted.clear();
  fragmentCapture = {};
  re4::DrawSceneGl(320, 180, 0, 0, 0, false, probe, nullptr, 0);
  glFinish();
  require(glGetError() == GL_NO_ERROR, "production object visibility GL error");
  std::sort(submitted.begin(), submitted.end());
  require(submitted == expected, "production batching/visibility mismatch");
}
bool close(float a, float b) { return std::abs(a - b) < .0001f; }
} // namespace
extern "C" void __real_glDrawArrays(GLenum mode, GLint first, GLsizei count);
extern "C" void __wrap_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  if (mode == GL_TRIANGLES && count == 3)
    submitted.push_back(first);
  if (mode == GL_TRIANGLES && count == 3 && first == 15) {
    fragmentCapture.seen = true;
    GLint program{};
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetUniformfv(program, glGetUniformLocation(program, "modelDelta"),
                   fragmentCapture.model.data());
    glGetUniformfv(program, glGetUniformLocation(program, "normalDelta"),
                   fragmentCapture.normal.data());
    glGetUniformfv(program, glGetUniformLocation(program, "materialRGB"),
                   fragmentCapture.color.data());
    glGetUniformfv(program, glGetUniformLocation(program, "materialAlpha"),
                   &fragmentCapture.alpha);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &fragmentCapture.depthWrite);
    glGetIntegerv(GL_DEPTH_FUNC, &fragmentCapture.depthFunction);
    glGetIntegerv(GL_BLEND_SRC_RGB, &fragmentCapture.blendSource);
    glGetIntegerv(GL_BLEND_DST_RGB, &fragmentCapture.blendDestination);
  }
  __real_glDrawArrays(mode, first, count);
}
int main() {
  try {
    setenv("EGL_PLATFORM", "surfaceless", 1);
    const auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major, minor;
    require(eglInitialize(display, &major, &minor), "EGL initialize");
    require(eglBindAPI(EGL_OPENGL_ES_API), "EGL GLES");
    EGLConfig config;
    EGLint number;
    const EGLint attributes[]{EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                             EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                             EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                             EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE};
    require(eglChooseConfig(display, attributes, &config, 1, &number) && number,
            "EGL config");
    const EGLint contextAttributes[]{EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    auto context = eglCreateContext(display, config, EGL_NO_CONTEXT,
                                     contextAttributes);
    const EGLint surfaceAttributes[]{EGL_WIDTH, 320, EGL_HEIGHT, 180, EGL_NONE};
    const auto surface = eglCreatePbufferSurface(display, config,
                                                 surfaceAttributes);
    require(context != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE &&
                eglMakeCurrent(display, surface, surface, context),
            "EGL current");
    re4::ObjectActions actions(0x400, room(), placements());
    const auto scene = mesh();
    re4::CreateSceneGl(scene);
    re4::SceneFollowHeading(0);
    re4::SceneProbe probe;
    probe.objects = &actions;
    probe.showBody = false;
    draw(&probe, {0, 3, 12}); // Intact containers, hidden variants/fragments.
    require(actions.shoot({1, {0, 175, -1000}, {0, 175, 3000}}).has_value(),
            "source crate shot");
    draw(&probe, {3, 6, 12}); // Exactly ammo4 appears; herb6 variant stays hidden.
    const auto dropped = actions.snapshot();
    const auto view = re4::SaveSceneView();
    require(actions.pickup({0, 0, -550}, 0, true).has_value(), "source pickup");
    draw(&probe, {3, 12}); // Pickup disappears; neighboring model survives.
    probe.objects = nullptr;
    draw(&probe, {0, 3, 12}); // Inspector has no pickups or inactive fragments.
    probe.objects = &actions;
    actions.restore(dropped);
    draw(&probe, {3, 6, 12});
    // A real context loss must not revive broken objects or lose visible drops.
    re4::ForgetSceneGl();
    require(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                           EGL_NO_CONTEXT) && eglDestroyContext(display, context),
            "EGL context loss");
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    require(context != EGL_NO_CONTEXT &&
                eglMakeCurrent(display, surface, surface, context),
            "EGL context recovery");
    re4::CreateSceneGl(scene);
    re4::RestoreSceneView(view);
    draw(&probe, {3, 6, 12});
    require(actions.pickup({0, 0, -550}, 0, true).has_value(), "recovered pickup");
    draw(&probe, {3, 12});
    // A synthetic one-part effect isolates the production affine adapter.
    // It is test geometry only; game effects use the original EFM BIN parts.
    re4::DestructionEffectsData data;
    data.signature = 0x1234;
    data.meshes.resize(1);
    data.models.push_back({1, 0, 1});
    data.records.resize(1);
    data.records[0].model = 0;
    re4::DestructionEffectsSimulation fragments(data);
    auto effectState = fragments.state();
    effectState.serial = 1;
    effectState.models.resize(1);
    auto &model = effectState.models[0];
    model.serial = 1;
    model.sizeX = 2;
    model.sizeY = 3;
    model.color = {128, 64, 255, 255};
    model.parts.resize(1);
    model.parts[0].world = {3000, 4500, 6000};
    model.parts[0].rotation[2] = 1.57079635f;
    require(fragments.restore(effectState), "fragment state fixture");
    probe.destruction = &fragments;
    draw(&probe, {3, 12, 15});
    require(fragmentCapture.seen, "active original-model fragment submission");
    const auto &matrix = fragmentCapture.model;
    require(close(matrix[1], 2) && close(matrix[4], -3) && close(matrix[10], 2) &&
                close(matrix[12], -.9f) && close(matrix[13], 3050.f / 1500.f) &&
                close(matrix[14], 4),
            "part-local to native-world to normalized VBO transform");
    const auto &normal = fragmentCapture.normal;
    require(close(normal[1], .5f) && close(normal[3], -1.f / 3) &&
                close(normal[8], .5f),
            "fragment nonuniform inverse-transpose normals");
    require(close(fragmentCapture.color[0], 128.f / 255) &&
                close(fragmentCapture.color[1], 64.f / 255) &&
                close(fragmentCapture.color[2], 1) && close(fragmentCapture.alpha, 1),
            "source fragment material color and alpha");
    require(fragmentCapture.depthWrite == GL_TRUE &&
                fragmentCapture.depthFunction == GL_LEQUAL &&
                fragmentCapture.blendSource == GL_SRC_ALPHA &&
                fragmentCapture.blendDestination == GL_ONE_MINUS_SRC_ALPHA,
            "opaque fragment ordinary depth and original blending");
    effectState.models[0].color[3] = 128;
    require(fragments.restore(effectState), "fragment fade state fixture");
    draw(&probe, {3, 12, 15});
    require(close(fragmentCapture.alpha, 128.f / 255) &&
                fragmentCapture.depthWrite == GL_FALSE &&
                fragmentCapture.depthFunction == GL_LEQUAL,
            "source invisible_factor fade keeps depth testing without writes");
    require(fragments.state().frames == effectState.frames &&
                fragments.state().serial == effectState.serial,
            "rendering does not advance fragment simulation");
    probe.destruction = nullptr;
    draw(&probe, {3, 12});
    re4::DestroySceneGl();
    require(glGetError() == GL_NO_ERROR, "production resource release");
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
    std::cout << "Production GLES ownership, item variants, fragment transforms/"
                 "normals/color/fade, batching and context recovery: "
              << checks << " checks PASS\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
