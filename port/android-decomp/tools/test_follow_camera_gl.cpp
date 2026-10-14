// Production SceneGl close-wall regression with original PL00/WEP01 assets.
// Synthetic SAT planes isolate camera collision; no game assets are bundled.
#include "actor_gl.h"
#include "follow_camera.h"
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
constexpr int width = 960, height = 540;
unsigned checks{};
void check(bool okay, const char *message) {
  ++checks;
  if (!okay)
    throw std::runtime_error(message);
}
struct Egl {
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLContext context = EGL_NO_CONTEXT;
  EGLSurface surface = EGL_NO_SURFACE;
  Egl() {
    setenv("EGL_PLATFORM", "surfaceless", 1);
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major{}, minor{}, count{};
    check(eglInitialize(display, &major, &minor), "Camera EGL initialize");
    check(eglBindAPI(EGL_OPENGL_ES_API), "Camera EGL API");
    const EGLint attributes[]{EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                              EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                              EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                              EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE};
    EGLConfig config{};
    check(eglChooseConfig(display, attributes, &config, 1, &count) && count,
          "Camera EGL config");
    const EGLint contextAttributes[]{EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    const EGLint surfaceAttributes[]{EGL_WIDTH, width, EGL_HEIGHT, height,
                                     EGL_NONE};
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    surface = eglCreatePbufferSurface(display, config, surfaceAttributes);
    check(context != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE &&
              eglMakeCurrent(display, surface, surface, context),
          "Camera EGL current");
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
using Pixels = std::vector<unsigned char>;
Pixels pixels(int w = width, int h = height) {
  Pixels result(w * h * 4);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
  check(glGetError() == GL_NO_ERROR, "Camera framebuffer read");
  return result;
}
unsigned difference(const Pixels &a, const Pixels &b) {
  unsigned result{};
  for (std::size_t i = 0; i < a.size(); i += 4)
    result += !std::equal(a.begin() + i, a.begin() + i + 3, b.begin() + i);
  return result;
}
void save(const std::string &path, const Pixels &image) {
  std::ofstream output(path, std::ios::binary);
  output << "P6\n" << width << ' ' << height << "\n255\n";
  for (int y = height - 1; y >= 0; --y)
    for (int x = 0; x < width; ++x)
      output.write(reinterpret_cast<const char *>(image.data() +
                                                  (y * width + x) * 4), 3);
  check(bool(output), "Camera framebuffer export");
}
re4::Sat fixture(double wallZ, double heading) {
  std::vector<std::uint8_t> bytes(20);
  bytes[0] = 0x20;
  re4::Sat sat(bytes);
  sat.vertices = {{-8000, 0, -8000}, {8000, 0, -8000},
                   {8000, 0, 8000}, {-8000, 0, 8000},
                   {-8000, 0, float(wallZ)}, {8000, 0, float(wallZ)},
                   {8000, 5000, float(wallZ)}, {-8000, 5000, float(wallZ)}};
  const double c = std::cos(heading), s = std::sin(heading);
  for (unsigned i = 4; i < 8; ++i) {
    const auto x = sat.vertices[i][0], z = sat.vertices[i][2];
    sat.vertices[i][0] = float(c * x + s * z);
    sat.vertices[i][2] = float(-s * x + c * z);
  }
  sat.normals = {{0, 1, 0}, {float(s), 0, float(c)}};
  sat.edges = {{0, 0, 0}};
  for (const auto &vertices : {std::array<unsigned, 3>{0, 1, 2},
                               std::array<unsigned, 3>{0, 2, 3},
                               std::array<unsigned, 3>{4, 5, 6},
                               std::array<unsigned, 3>{4, 6, 7}}) {
    re4::SatPolygon polygon;
    for (unsigned i = 0; i < 3; ++i)
      polygon.vertices[i] = vertices[i];
    polygon.normal = sat.polygons.size() < 2 ? 0 : 1;
    polygon.attributes = sat.polygons.size() < 2 ? 0x10 : 0x400;
    sat.polygons.push_back(polygon);
  }
  sat.floor = sat.wall = 2;
  return sat;
}
re4::StaticScene scenery(const re4::Sat &sat) {
  re4::StaticScene scene;
  scene.minimum = {-12000, 0, -12000};
  scene.maximum = {12000, 5000, 12000};
  for (unsigned index = 0; index < sat.polygons.size(); ++index) {
    re4::SceneDraw draw;
    draw.first = scene.vertices.size() / 3;
    draw.count = 3;
    draw.zMode = 0;
    draw.blendMode = 4;
    draw.materialRGB = index < 2 ? std::array<std::uint8_t, 3>{65, 85, 65}
                                : std::array<std::uint8_t, 3>{180, 45, 35};
    const auto &polygon = sat.polygons[index];
    for (unsigned corner = 0; corner < 3; ++corner) {
      const auto &v = sat.vertices[polygon.vertices[corner]];
      scene.vertices.insert(scene.vertices.end(), v.begin(), v.end());
      const auto &n = sat.normals[polygon.normal];
      scene.normals.insert(scene.normals.end(), n.begin(), n.end());
      scene.uv.insert(scene.uv.end(), {0, 0});
      scene.colors.insert(scene.colors.end(), {255, 255, 255, 255});
    }
    scene.draws.push_back(draw);
  }
  scene.triangles = sat.polygons.size();
  return scene;
}
void poseIsOptical(const re4::FollowCameraPose &pose) {
  double right{}, up{}, forward{}, magnitude{};
  for (unsigned i = 0; i < 3; ++i) {
    const double d = pose.target[i] - pose.eye[i];
    right += d * pose.right[i];
    up += d * pose.up[i];
    forward += d * pose.look[i];
    magnitude += pose.look[i] * pose.look[i];
  }
  check(std::abs(right) < 1e-7 && std::abs(up) < 1e-7 && forward > 0 &&
            std::abs(magnitude - 1) < 1e-8,
        "Camera uses resolved eye and explicit optical target");
}
std::array<float, 16> opticalMatrix(const re4::FollowCameraPose &pose) {
  std::array<double, 16> view{}, projection{};
  for (unsigned axis = 0; axis < 3; ++axis) {
    view[axis] = pose.right[axis];
    view[4 + axis] = pose.up[axis];
    view[8 + axis] = -pose.look[axis];
    view[3] -= view[axis] * pose.eye[axis];
    view[7] -= view[4 + axis] * pose.eye[axis];
    view[11] -= view[8 + axis] * pose.eye[axis];
  }
  view[15] = 1;
  constexpr double pi = 3.14159265358979323846, near = 100, far = 200000;
  const auto f = 1 / std::tan(pose.fovDegrees * pi / 360);
  projection[0] = f / (double(width) / height);
  projection[5] = f;
  projection[10] = -(far + near) / (far - near);
  projection[11] = -2 * far * near / (far - near);
  projection[14] = -1;
  std::array<float, 16> result{};
  for (unsigned row = 0; row < 4; ++row)
    for (unsigned column = 0; column < 4; ++column)
      for (unsigned k = 0; k < 4; ++k)
        result[column * 4 + row] +=
            float(projection[row * 4 + k] * view[k * 4 + column]);
  return result;
}
Pixels actorFrame(const re4::LeonPreview &actor,
                  const re4::FollowCameraPose &pose, bool clearance,
                  bool body = true, int w = width, int h = height) {
  const auto camera = opticalMatrix(pose);
  glViewport(0, 0, w, h);
  glDepthMask(GL_TRUE);
  glClearColor(.035f, .055f, .075f, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (body)
    re4::DrawActorGl(camera.data(), {}, 1, actor, false,
                     clearance ? &pose.eye : nullptr);
  if (actor.weapon() && actor.weaponVisible())
    re4::DrawWeaponGl(camera.data(), {}, 1, *actor.weapon(),
                      actor.rig().jointMatrices().at(10), actor.placement(),
                      false);
  return pixels(w, h);
}
void headClearance(re4::LeonPreview &actor, const std::string &prefix) {
  actor.reset();
  actor.setPlacement({0, 0, 0}, 0);
  re4::CreateActorGl(actor);
  re4::CreateWeaponGl(*actor.weapon());
  auto sat = fixture(-111, 0);
  re4::SatCollision collision(sat);
  const auto close = re4::PoseFollowCamera({}, &collision, {}, 0,
                                          false, 0, 0, double(width) / height);
  const auto source = actorFrame(actor, close, false);
  const auto clear = actorFrame(actor, close, true);
  const auto weaponOnly = actorFrame(actor, close, true, false);
  check(difference(source, clear) > 10000,
        "Near head attachment obscures the closest camera site");
  check(difference(clear, weaponOnly) > 100,
        "Head clearance hides Leon's torso/arms as well as his head");
  check(actorFrame(actor, close, true) == clear,
        "Head coverage pattern drifts on paused rendering");
  save(prefix + "-head-unadapted.ppm", source);
  save(prefix + "-head-cleared.ppm", clear);
  const auto normal = re4::PoseFollowCamera({}, nullptr, {}, 0);
  check(actorFrame(actor, normal, true) == actorFrame(actor, normal, false),
        "Head adaptation changes unobstructed shoulder rendering");
  // A caller with the opposite face convention must get its exact GL state
  // back, while production actor winding still gives the same visible mesh.
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  glFrontFace(GL_CW);
  check(actorFrame(actor, close, true) == clear,
        "Actor culling inherits a room pass's face convention");
  GLint face{}, winding{};
  glGetIntegerv(GL_CULL_FACE_MODE, &face);
  glGetIntegerv(GL_FRONT_FACE, &winding);
  check(glIsEnabled(GL_CULL_FACE) && face == GL_FRONT && winding == GL_CW,
        "Actor culling leaks into subsequent scene passes");
  glDisable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  // Full world rotation keeps the head bound attached to the animated head,
  // rather than a fixed global axis. Each degree is actually rasterized.
  constexpr double pi = 3.14159265358979323846;
  unsigned minimum{};
  for (unsigned degree = 0; degree < 360; ++degree) {
    const auto heading = degree * pi / 180;
    actor.setPlacement({0, 0, 0}, float(heading));
    auto rotatedSat = fixture(-111, actor.heading());
    re4::SatCollision rotatedWorld(rotatedSat);
    const auto pose = re4::PoseFollowCamera(
        {}, &rotatedWorld, {}, actor.heading(), false, 0, 0,
        double(width) / height);
    const auto adapted = actorFrame(actor, pose, true, true, 320, 180);
    const auto unadapted = actorFrame(actor, pose, false, true, 320, 180);
    const auto changed = difference(adapted, unadapted);
    check(changed > 1000, "Rotating Leon loses near-head clearance");
    minimum = degree ? std::min(minimum, changed) : changed;
  }
  actor.setPlacement({0, 0, 0}, 0);
  Pixels rateReference;
  for (unsigned hz : {30u, 60u, 120u}) {
    re4::FollowCameraState state;
    for (unsigned tick = 0; tick < hz; ++tick)
      re4::AdvanceFollowCamera(state, .2, .15, 1. / hz);
    const auto pose = re4::PoseFollowCamera(
        state, &collision, {}, 0, false, 0, 0, double(width) / height);
    const auto frame = actorFrame(actor, pose, true);
    if (rateReference.empty())
      rateReference = frame;
    else
      check(frame == rateReference,
            "Near-head coverage depends on render refresh rate");
  }
  std::cout << "Head clearance PASS: closest source site, unaffected outer "
               "profile, preserved arms/weapon, caller culling restored, "
            << "360 rasterized headings (minimum " << minimum
            << " cleared pixels), identical 30/60/120 Hz frames\n";
  re4::DestroyActorGl();
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 4)
      throw std::runtime_error(
          "Usage: follow_camera_gl_check PL00.drs WEP01.drs OUTPUT_PREFIX");
    Egl egl;
    re4::LeonPreview actor(argv[1], argv[2]);
    headClearance(actor, argv[3]);
    unsigned minimumVisible = width * height;
    for (double heading : {0., -.35, .35}) {
      for (double wallZ : {-800., -400., -300., -150., -111.}) {
        auto sat = fixture(wallZ, heading);
        re4::SatCollision collision(sat);
        re4::WalkProbe walker(collision);
        check(walker.place({0, 0, 0}), "Camera floor fixture valid");
        actor.reset();
        actor.setPlacement({0, 0, 0}, float(heading));
        re4::CreateSceneGl(scenery(sat));
        re4::CreateActorGl(actor);
        re4::CreateWeaponGl(*actor.weapon());
        re4::SceneFollowHeading(actor.heading());
        re4::SceneProbe probe;
        probe.collision = &collision;
        probe.feet = {0, 0, 0};
        probe.showBody = false;
        auto draw = [&](bool body) {
          re4::DrawSceneGl(width, height, 0, 0, 0, false, &probe,
                           body ? &actor : nullptr, 0);
          return pixels();
        };
        const auto without = draw(false), with = draw(true);
        const auto pose = re4::SceneFollowPose();
        check(pose.distance < 1000, "Close-wall case reaches former body-hide threshold");
        const auto expected = re4::PoseFollowCamera(
            {}, &collision, {0, 0, 0}, actor.heading(), false, 0, 0,
            double(width) / height);
        for (unsigned i = 0; i < 3; ++i)
          check(std::abs(expected.eye[i] - pose.eye[i]) < 1e-7 &&
                    std::abs(expected.target[i] - pose.target[i]) < 1e-7,
                "Production camera retains source-resolved common profile");
        poseIsOptical(pose);
        const auto visible = difference(with, without);
        std::cout << "Close wall z=" << wallZ << " heading=" << heading
                  << " distance=" << pose.distance << " Leon+weapon pixels="
                  << visible << '\n';
        check(visible > 100, "Original Leon disappears at a close wall");
        minimumVisible = std::min(minimumVisible, visible);
        check(draw(true) == with, "Paused camera/body framebuffer drifts");
        if (heading == 0)
          save(std::string(argv[3]) + "-wall-" + std::to_string(int(-wallZ)) +
                   ".ppm", with);
        // Both the camera and Leon remain on the player's side of the SAT
        // wall, with normal scene depth testing active throughout the draw.
        const double localZ = std::sin(heading) * pose.eye[0] +
                              std::cos(heading) * pose.eye[2];
        check(localZ > wallZ && localZ <= -110 + 1e-5,
              "Close camera moved through the wall or through its authored close site");
        check(glIsEnabled(GL_DEPTH_TEST), "Camera draw disabled ordinary depth test");
        // The same close profile survives the original handgun draw/ready
        // curves. Use actual player ticks rather than directly replacing a rig.
        re4::PlayerActionInput input;
        input.aimHeld = true;
        for (unsigned tick = 0; tick < 40; ++tick)
          actor.step(walker, 0, 0, false, false, 1. / 30, input);
        check(actor.action() == re4::PlayerAction::Aim,
              "Camera regression reaches original handgun ready action");
        probe.feet = walker.position();
        const auto aimWithout = draw(false), aimWith = draw(true);
        check(difference(aimWith, aimWithout) > 100,
              "Original Leon/handgun disappears when aiming beside a wall");
        poseIsOptical(re4::SceneFollowPose());
        if (heading == 0 && wallZ == -800)
          save(std::string(argv[3]) + "-wall-800-aim.ppm", aimWith);
        re4::DestroySceneGl();
      }
    }
    // A render-only opaque barrier proves body rendering has no X-ray override.
    // With no matching SAT it stays between the desired eye and Leon, so both
    // source mesh and attached handgun must be naturally hidden by depth.
    auto occluder = fixture(-700, 0);
    actor.reset();
    actor.setPlacement({0, 0, 0}, 0);
    re4::CreateSceneGl(scenery(occluder));
    re4::CreateActorGl(actor);
    re4::CreateWeaponGl(*actor.weapon());
    re4::SceneFollowHeading(0);
    re4::SceneProbe probe;
    probe.showBody = false;
    re4::DrawSceneGl(width, height, 0, 0, 0, false, &probe, nullptr, 0);
    const auto opaque = pixels();
    re4::DrawSceneGl(width, height, 0, 0, 0, false, &probe, &actor, 0);
    check(pixels() == opaque, "Leon or weapon rendered through an opaque wall");
    re4::DestroySceneGl();
    std::cout << "Production SceneGl close-wall PASS: original PL00/WEP01, "
              << "15 movement/ready collision pairs, minimum " << minimumVisible
              << " visible actor pixels, opaque-wall depth and paused repeat · "
              << checks << " checks\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
