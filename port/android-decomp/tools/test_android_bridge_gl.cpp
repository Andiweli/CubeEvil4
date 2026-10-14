// Exercise the production bridge itself with minimal JNI strings and real
// surfaceless GLES3. Include the bridge once so the check can assert its CPU
// ownership/placement/animation without adding test APIs to the Android app.
// Do not also compile android_bridge.cpp into this executable.
#include "../app/src/main/cpp/android_bridge.cpp"
#include "../app/src/main/cpp/weapon_preview.h"
#include <EGL/egl.h>
#include <cstdlib>
#include <iostream>
#include <tuple>
#include <vector>

namespace bridge_test {
unsigned checks;
bool failNextUniformAllocation;
unsigned failNthUniformAllocation;
unsigned failedUniformAllocations;
unsigned uniformAllocations;
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
  ++checks;
}
bool sameFollowState(const re4::FollowCameraState &a,
                     const re4::FollowCameraState &b) {
  return a.yawRatioRadians == b.yawRatioRadians &&
         a.verticalRatio == b.verticalRatio && a.accumulator == b.accumulator &&
         a.releaseYawRadians == b.releaseYawRadians &&
         a.desiredYaw == b.desiredYaw && a.desiredVertical == b.desiredVertical &&
         a.desiredReleaseYaw == b.desiredReleaseYaw &&
         a.smoothingInitialized == b.smoothingInitialized;
}
bool sameEffectsState(const std::shared_ptr<const re4::EnvironmentState> &a,
                      const std::shared_ptr<const re4::EnvironmentState> &b) {
  if (!a || !b)
    return !a && !b;
  if (std::tie(a->signature, a->frames, a->serial, a->random, a->skyAngles,
                a->recordCursor, a->sequenceSeed) !=
          std::tie(b->signature, b->frames, b->serial, b->random, b->skyAngles,
                   b->recordCursor, b->sequenceSeed) ||
      a->emitters.size() != b->emitters.size() ||
      a->sprites.size() != b->sprites.size())
    return false;
  for (std::size_t i = 0; i < a->emitters.size(); ++i) {
    const auto &x = a->emitters[i], &y = b->emitters[i];
    if (std::tie(x.target, x.record, x.next, x.interval, x.seed) !=
        std::tie(y.target, y.record, y.next, y.interval, y.seed))
      return false;
  }
  for (std::size_t i = 0; i < a->sprites.size(); ++i) {
    const auto &x = a->sprites[i], &y = b->sprites[i];
    if (std::tie(x.target, x.record, x.flags, x.pattern, x.animationCounter, x.age,
                  x.serial, x.position, x.speed, x.acceleration, x.angle,
                  x.angleSpeed, x.color, x.size, x.scroll, x.sizeMultiplier,
                  x.sizeStep) !=
        std::tie(y.target, y.record, y.flags, y.pattern, y.animationCounter, y.age,
                 y.serial, y.position, y.speed, y.acceleration, y.angle,
                 y.angleSpeed, y.color, y.size, y.scroll, y.sizeMultiplier,
                 y.sizeStep))
      return false;
  }
  return true;
}
bool sameVisualEffects(const re4::SceneViewState &a,
                       const re4::SceneViewState &b) {
  return sameEffectsState(a.environment, b.environment) &&
         sameEffectsState(a.worldEffects, b.worldEffects);
}
double dot(const re4::CollisionVec &a, const re4::CollisionVec &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
void checkShoulderCamera(const char *message) {
  const auto pose = re4::SceneFollowPose();
  re4::CollisionVec toTarget{};
  for (unsigned i = 0; i < 3; ++i)
    toTarget[i] = pose.target[i] - pose.eye[i];
  require(pose.fovDegrees == 50 &&
              std::abs(dot(pose.look, pose.look) - 1) < 1e-8 &&
              std::abs(dot(pose.right, pose.right) - 1) < 1e-8 &&
              std::abs(dot(pose.up, pose.up) - 1) < 1e-8 &&
              std::abs(dot(toTarget, pose.right)) < 1e-6 &&
              std::abs(dot(toTarget, pose.up)) < 1e-6 &&
              dot(toTarget, pose.look) > 0,
          message);
}
void checkWeaponProfile() {
  const auto pose = re4::SceneFollowPose();
  const auto action = leon->action();
  double localYaw = action == re4::PlayerAction::Draw
                        ? leon->drawCameraYawRemaining()
                        : leon->waistYaw();
  if (action == re4::PlayerAction::KnifeDraw ||
      action == re4::PlayerAction::KnifeReady ||
      action == re4::PlayerAction::KnifeSlash ||
      action == re4::PlayerAction::KnifeHolster)
    localYaw += re4::SceneFollowState().yawRatioRadians +
                re4::SceneFollowState().releaseYawRadians;
  const auto expected = re4::PoseFollowCamera(
      re4::SceneFollowState(), collisionWorld.get(), walkProbe->position(),
      leon->heading(), true, localYaw, leon->aimRate(), 16. / 9);
  for (unsigned i = 0; i < 3; ++i)
    require(std::abs(pose.eye[i] - expected.eye[i]) < 1e-6 &&
                std::abs(pose.target[i] - expected.target[i]) < 1e-6,
            "Production weapon phases retain common player-local shoulder profile");
  checkShoulderCamera("Resolved weapon view targets the same explicit shoulder focus");
}
struct String {
  std::string text;
};
std::vector<std::unique_ptr<String>> strings;
std::string javaException;
jstring string(const std::string &value) {
  strings.push_back(std::make_unique<String>(String{value}));
  return reinterpret_cast<jstring>(strings.back().get());
}
std::string text(jstring value) {
  require(value != nullptr, "JNI bridge returned a null string");
  return reinterpret_cast<String *>(value)->text;
}
const char *JNICALL getChars(JNIEnv *, jstring value, jboolean *copy) {
  if (copy)
    *copy = JNI_FALSE;
  return value ? reinterpret_cast<String *>(value)->text.c_str() : nullptr;
}
void JNICALL releaseChars(JNIEnv *, jstring, const char *) {}
jstring JNICALL newString(JNIEnv *, const char *value) {
  return string(value ? value : "");
}
jclass JNICALL findClass(JNIEnv *, const char *) {
  return reinterpret_cast<jclass>(std::uintptr_t(1));
}
jint JNICALL throwNew(JNIEnv *, jclass, const char *message) {
  javaException = message ? message : "Java exception";
  return JNI_OK;
}
struct Jni {
  JNINativeInterface_ table{};
  JNIEnv env{};
  Jni() {
    table.GetStringUTFChars = getChars;
    table.ReleaseStringUTFChars = releaseChars;
    table.NewStringUTF = newString;
    table.FindClass = findClass;
    table.ThrowNew = throwNew;
    env.functions = &table;
  }
};
struct Egl {
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLSurface surface = EGL_NO_SURFACE;
  EGLContext context = EGL_NO_CONTEXT;
  EGLConfig config{};
  Egl() {
    setenv("EGL_PLATFORM", "surfaceless", 1);
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major, minor;
    require(eglInitialize(display, &major, &minor), "Bridge EGL initialize");
    require(eglBindAPI(EGL_OPENGL_ES_API), "Bridge EGL API");
    const EGLint attrs[]{EGL_SURFACE_TYPE,
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
    EGLint count;
    require(eglChooseConfig(display, attrs, &config, 1, &count) && count,
            "Bridge EGL config");
    const EGLint pbuffer[]{EGL_WIDTH, 960, EGL_HEIGHT, 540, EGL_NONE};
    surface = eglCreatePbufferSurface(display, config, pbuffer);
    require(surface != EGL_NO_SURFACE, "Bridge EGL pbuffer");
    fresh();
  }
  void release() {
    require(
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT),
        "Bridge EGL release");
  }
  void current() {
    require(eglMakeCurrent(display, surface, surface, context),
            "Bridge EGL current");
  }
  void fresh() {
    if (context != EGL_NO_CONTEXT) {
      release();
      require(eglDestroyContext(display, context), "Bridge old EGL destroy");
    }
    const EGLint attrs[]{EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
    require(context != EGL_NO_CONTEXT, "Bridge new EGL context");
    current();
  }
  ~Egl() {
    if (display != EGL_NO_DISPLAY) {
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (context != EGL_NO_CONTEXT)
        eglDestroyContext(display, context);
      if (surface != EGL_NO_SURFACE)
        eglDestroySurface(display, surface);
      eglTerminate(display);
    }
  }
};
struct ActionSnapshot {
  re4::PlayerAction action;
  re4::Locomotion locomotion;
  unsigned magazine, reserve, shots;
  float heading, aimRate, waistYaw, curveFrame, sequenceFrame;
  std::array<double, 3> feet, muzzle, aimDirection;
  std::vector<re4::Matrix34> body, joints, gun, gunJoints;
  re4::WeaponMotion gunMotion;
  float gunFrame;
  bool gunEmpty;
  bool operator==(const ActionSnapshot &other) const {
    return action == other.action && locomotion == other.locomotion &&
           magazine == other.magazine && reserve == other.reserve &&
           shots == other.shots && heading == other.heading &&
           aimRate == other.aimRate && waistYaw == other.waistYaw &&
           curveFrame == other.curveFrame &&
           sequenceFrame == other.sequenceFrame && feet == other.feet &&
           muzzle == other.muzzle && aimDirection == other.aimDirection &&
           body == other.body && joints == other.joints && gun == other.gun &&
           gunJoints == other.gunJoints && gunMotion == other.gunMotion &&
           gunFrame == other.gunFrame && gunEmpty == other.gunEmpty;
  }
};
ActionSnapshot actionSnapshot() {
  require(leon && leon->weapon(), "Production player owns the original gun");
  const auto &weapon = *leon->weapon();
  return {leon->action(),
          leon->state(),
          leon->magazine(),
          leon->reserve(),
          leon->shots(),
          leon->heading(),
          leon->aimRate(),
          leon->waistYaw(),
          leon->rig().curveFrame(),
          leon->rig().sequenceFrame(),
          leon->feet(),
          leon->muzzle(),
          leon->aimDirection(),
          leon->rig().bones(),
          leon->rig().jointMatrices(),
          weapon.rig().bones(),
          weapon.rig().jointMatrices(),
          weapon.motion(),
          weapon.frame(),
          weapon.empty()};
}
struct EnemySnapshot {
  unsigned hp, maxHp, shots, hits, damage;
  std::uint16_t random;
  re4::EnemyMotion motion;
  bool mirrored, settled, head, critical;
  float heading, scale, curve, sequence;
  std::array<double, 3> feet;
  re4::Matrix34 placement;
  std::vector<re4::Matrix34> joints;
  bool operator==(const EnemySnapshot &other) const {
    return hp == other.hp && maxHp == other.maxHp && shots == other.shots &&
           hits == other.hits && damage == other.damage && random == other.random &&
           motion == other.motion && mirrored == other.mirrored &&
           settled == other.settled && head == other.head && critical == other.critical &&
           heading == other.heading && scale == other.scale && curve == other.curve &&
           sequence == other.sequence && feet == other.feet &&
           placement == other.placement && joints == other.joints;
  }
};
struct AnimalSnapshot {
  std::uint64_t ticks{};
  std::vector<std::array<std::uint8_t, 32>> sources;
  std::vector<re4::CollisionVec> feet;
  std::vector<float> headings, curveFrames, sequenceFrames;
  std::vector<re4::AnimalMotion> motions;
  std::vector<std::vector<re4::Matrix34>> bones, joints;
  bool operator==(const AnimalSnapshot &other) const {
    return ticks == other.ticks && sources == other.sources && feet == other.feet &&
           headings == other.headings && curveFrames == other.curveFrames &&
           sequenceFrames == other.sequenceFrames && motions == other.motions &&
           bones == other.bones && joints == other.joints;
  }
};
AnimalSnapshot animalSnapshot() {
  require(bool(ambientAnimals), "Production room owns original ambient animals");
  AnimalSnapshot result;
  result.ticks = ambientAnimals->ticks();
  for (const auto &animal : ambientAnimals->animals()) {
    result.sources.push_back(animal.source().raw);
    result.feet.push_back(animal.feet());
    result.headings.push_back(animal.heading());
    result.curveFrames.push_back(animal.rig().curveFrame());
    result.sequenceFrames.push_back(animal.rig().sequenceFrame());
    result.motions.push_back(animal.motion());
    result.bones.push_back(animal.rig().bones());
    result.joints.push_back(animal.rig().jointMatrices());
  }
  return result;
}
EnemySnapshot enemySnapshot() {
  require(encounter != nullptr, "Production bridge owns the original enemy");
  const auto &actor = encounter->actor();
  return {encounter->hp(), encounter->maxHp(), encounter->shots(),
          encounter->hits(), encounter->lastDamage(), encounter->randomState(),
          actor.motion(), actor.mirrored(), encounter->deadSettled(),
          encounter->lastHead(), encounter->lastCritical(), encounter->heading(),
          encounter->scale(), actor.rig().curveFrame(), actor.rig().sequenceFrame(),
          encounter->feet(), encounter->placement(), actor.rig().jointMatrices()};
}
void controls(Jni &jni, std::uint32_t buttons = 0, float lt = 0, float rt = 0,
              float rx = 0, float ry = 0) {
  Java_com_ast_re4decomp_NativeBridge_input(&jni.env, nullptr, buttons, 0, 0,
                                            rx, ry, lt, rt);
}
std::uint64_t token(Jni &jni, const std::string &destination = "r301") {
  auto request = text(Java_com_ast_re4decomp_NativeBridge_pendingRoomRequest(
      &jni.env, nullptr));
  auto tab = request.find('\t');
  require(tab != std::string::npos && request.substr(tab + 1) == destination,
          "Actual bridge published the expected room request");
  return std::stoull(request.substr(0, tab));
}
void draw(Jni &jni) {
  Java_com_ast_re4decomp_NativeBridge_resize(&jni.env, nullptr, 960, 540);
  Java_com_ast_re4decomp_NativeBridge_draw(&jni.env, nullptr);
  const auto error = glGetError();
  const auto failure = "Actual bridge GLES draw error " +
                       std::to_string(error) + " / " + graphics;
  require(error == GL_NO_ERROR, failure.c_str());
  require(javaException.empty(), "Unexpected Java exception in bridge");
}
void tick(Jni &jni) {
  // Drive the real render/update path at a reproducible 30-Hz boundary.
  // This adjusts only its host wall clock, not animation or placement logic.
  walkFrame =
      std::chrono::steady_clock::now() - std::chrono::microseconds(33334);
  seenInputGeneration = inputGeneration;
  draw(jni);
}
void ticks(Jni &jni, unsigned count) {
  while (count--)
    tick(jni);
}
void inspector(Jni &jni) {
  Java_com_ast_re4decomp_NativeBridge_requestInspector(&jni.env, nullptr);
  draw(jni);
}
void turn180(Jni &jni) {
  auto heading = leon->heading();
  Java_com_ast_re4decomp_NativeBridge_input(&jni.env, nullptr, re4::Right, 0,
                                            -1, 0, 0, 0, 0);
  tick(jni);
  Java_com_ast_re4decomp_NativeBridge_input(&jni.env, nullptr, 0, 0, 0, 0, 0, 0,
                                            0);
  for (unsigned i = 0; i < 25; ++i)
    tick(jni);
  auto angle = std::abs(
      std::remainder(leon->heading() - heading, 6.28318530717958647692f));
  require(angle > 2.9f && angle < 3.3f &&
              leon->state() == re4::Locomotion::Idle,
          "Actual B/back input performs original one-shot 180-degree turn");
}
void pause(Jni &jni) {
  if (!paused) {
    Java_com_ast_re4decomp_NativeBridge_input(&jni.env, nullptr, re4::Start, 0,
                                              0, 0, 0, 0, 0);
    draw(jni);
    Java_com_ast_re4decomp_NativeBridge_input(&jni.env, nullptr, 0, 0, 0, 0, 0,
                                              0, 0);
    draw(jni);
  }
  require(paused, "Actual bridge pause action works");
}
void save(const std::string &path) {
  std::vector<unsigned char> pixels(960 * 540 * 4);
  glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  require(glGetError() == GL_NO_ERROR, "Bridge framebuffer read");
  unsigned covered = 0;
  for (std::size_t i = 0; i < pixels.size(); i += 4)
    covered += pixels[i] > 30 || pixels[i + 1] > 30 || pixels[i + 2] > 30;
  require(covered > 1000, "Bridge room framebuffer contains geometry");
  std::ofstream out(path, std::ios::binary);
  out << "P6\n960 540\n255\n";
  for (int y = 539; y >= 0; --y)
    for (int x = 0; x < 960; ++x)
      out.write(
          reinterpret_cast<const char *>(pixels.data() + (y * 960 + x) * 4), 3);
  require(bool(out), "Bridge framebuffer export");
}
void savePhase(std::string path, const char *phase) {
  if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".ppm") == 0)
    path.resize(path.size() - 4);
  save(path + "-" + phase + ".ppm");
}
bool aimAtTorso(Jni &jni) {
  const auto shape = re4::NormalGanadoHitShapes().at(1);
  for (unsigned attempt = 0; attempt < 240; ++attempt) {
    const auto muzzle = leon->muzzle(), direction = leon->aimDirection();
    auto end = muzzle;
    for (unsigned k = 0; k < 3; ++k)
      end[k] += direction[k] * 30000;
    if (encounter->target(muzzle, end)) {
      controls(jni, 0, 1);
      return true;
    }
    auto center = shape.offset;
    center[1] += shape.height * .5f;
    const auto local = re4::TransformPoint(
        encounter->actor().rig().jointMatrices().at(shape.partsNo - 1), center);
    const auto target = re4::TransformPoint(encounter->placement(), local);
    const double dx = target[0] - muzzle[0], dy = target[1] - muzzle[1],
                 dz = target[2] - muzzle[2];
    const double yaw = std::remainder(std::atan2(dx, dz) -
        std::atan2(direction[0], direction[2]), 6.28318530717958647692);
    const double pitch = std::atan2(dy, std::hypot(dx, dz)) -
                         std::asin(std::clamp(direction[1], -1., 1.));
    // With the mapped RS axis, physical right now decreases world aim yaw
    // toward camera-right. Adjust real JNI controller input, never the actor.
    controls(jni, 0, 1, 0, float(std::clamp(-yaw * 8, -1., 1.)),
             float(std::clamp(pitch * 8, -1., 1.)));
    tick(jni);
  }
  std::cerr << "Ganado aim diagnostic: " << encounter->status()
            << " heading=" << leon->heading() << " aim=" << leon->aimRate()
            << "\n";
  return false;
}
void enemyDiagnostic(const char *phase) {
  const auto camera = re4::SaveSceneView();
  const auto player = leon->feet(), enemy = encounter->feet();
  const auto direction = leon->aimDirection();
  std::cerr << "Ganado " << phase << ": room=" << std::hex << activeRoomId << std::dec
            << " SMDmodels=" << previewSmd->models.size() << " player="
            << player[0] << ',' << player[1] << ',' << player[2] << " enemy="
            << enemy[0] << ',' << enemy[1] << ',' << enemy[2]
            << " heading=" << leon->heading() << " aimRate=" << leon->aimRate()
            << " ray=" << direction[0] << ',' << direction[1] << ',' << direction[2]
            << " camera=" << camera.yaw << ',' << camera.pitch << ','
            << re4::SceneCameraDistance() << " HP=" << encounter->hp() << '\n';
}
unsigned enemyVisibility(Jni &jni) {
  const auto enemy = enemySnapshot();
  const auto action = actionSnapshot();
  std::vector<std::uint8_t> visible(960 * 540 * 4), hidden(visible.size());
  // A zero host wall-clock step renders the exact pose without a simulation
  // tick. The scene, player, camera and original GPU data remain identical.
  walkFrame = {};
  draw(jni);
  glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, visible.data());
  auto owner = std::move(encounter);
  walkFrame = {};
  draw(jni);
  glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, hidden.data());
  encounter = std::move(owner);
  walkFrame = {};
  draw(jni);
  require(enemySnapshot() == enemy && actionSnapshot() == action,
          "Visibility comparison preserves exact enemy/player simulation state");
  unsigned pixels = 0;
  for (unsigned pixel = 0; pixel < 960 * 540; ++pixel) {
    const auto offset = pixel * 4;
    // The aim ray can shorten at the enemy. Count geometry coverage, rather
    // than treating that red diagnostic line as a visible actor.
    const bool redRay =
        (visible[offset] > 100 && visible[offset + 1] < 70 && visible[offset + 2] < 70) ||
        (hidden[offset] > 100 && hidden[offset + 1] < 70 && hidden[offset + 2] < 70);
    if (!redRay)
      pixels += !std::equal(visible.begin() + offset, visible.begin() + offset + 3,
                           hidden.begin() + offset);
  }
  return pixels;
}
void shootEnemy(Jni &jni, const std::string &capture = {}) {
  require(encounter && encounter->alive(), "JNI shooting fixture has living enemy");
  const unsigned previousHp = encounter->hp(), previousHits = encounter->hits();
  const unsigned previousShots = leon->shots(), previousMagazine = leon->magazine();
  for (unsigned attempt = 0; attempt < 3 && encounter->hits() == previousHits;
       ++attempt) {
    controls(jni, 0, 1);
    ticks(jni, 20);
    require(aimAtTorso(jni), "JNI right-stick aligns posed gun with original hit capsules");
    if (!capture.empty() && attempt == 0) {
      const auto visible = enemyVisibility(jni);
      std::cerr << "Ganado aimed visible geometry pixels=" << visible << '\n';
      require(visible >= 2000, "Aligned original enemy remains visibly framed beyond Leon's body");
      enemyDiagnostic("aim-target");
      savePhase(capture, "ganado-aim-target");
    }
    controls(jni, 0, 1, 1);
    ticks(jni, 3);
  }
  require(encounter->hits() > previousHits && encounter->hp() < previousHp &&
              encounter->lastDamage() > 0 && leon->shots() > previousShots &&
              leon->magazine() == previousMagazine - (leon->shots() - previousShots),
          "Actual RT shot spends ammunition and drains original enemy HP");
}
void knifeCameraMapping(Jni &jni, const char *player, const char *weapon,
                        const char *enemy) {
  require(paused && encounter && leon && leon->hasMeleeMotions() && actorGpuReady,
          "Knife camera check uses an existing paused M21 GPU owner");
  const auto savedEnemyState = enemySnapshot();
  const auto savedPlayerState = actionSnapshot();
  const auto savedView = re4::SaveSceneView();
  const auto savedInput = input;
  const auto savedFrame = frame;
  const auto savedGeneration = inputGeneration;
  const auto savedSeenGeneration = seenInputGeneration;
  const auto savedClock = walkFrame;
  const auto savedHud = hudInfo;
  auto savedEnemy = std::move(encounter);
  auto savedPlayer = std::move(leon);
  auto savedProbe = std::move(walkProbe);
  // Isolate controller/camera from enemy attacks without replacing its state.
  // The new actor has the same exact original assets and render-mesh owner;
  // no GPU allocation or world/actor position change is needed for the check.
  leon = std::make_unique<re4::LeonPreview>(player, weapon, enemy, true);
  leon->inheritInventory(*savedPlayer);
  walkProbe = std::make_unique<re4::WalkProbe>(*collisionWorld);
  require(walkProbe->place(savedProbe->position()),
          "Knife camera check places its independent probe on the same original floor");
  leon->setPlacement(walkProbe->position(), savedPlayer->heading());
  paused = false;
  controls(jni);
  ticks(jni, 2);
  // The original ready routine transfers camera yaw to the body over four
  // frames. Cancel before that transfer finishes: the rendered focus must
  // survive the return to footwork, rather than resetting behind the body.
  auto independentView = re4::SaveSceneView();
  independentView.followCamera = {0, 0, .5, 0};
  re4::RestoreSceneView(independentView);
  controls(jni, 0, 1, 0, .6f, -.3f);
  tick(jni);
  require(leon->action() == re4::PlayerAction::Draw &&
              std::abs(leon->drawCameraYawRemaining()) > .03,
          "Early draw fixture retains original incomplete four-frame yaw transfer");
  const auto earlyPose = re4::SceneFollowPose();
  const auto earlyFeet = walkProbe->position();
  const double earlyHeading = leon->heading() + leon->drawCameraYawRemaining();
  const double earlyPitch = leon->aimRate();
  controls(jni);
  tick(jni);
  const auto returnedPose = re4::SceneFollowPose();
  const auto returnedFeet = walkProbe->position();
  require(leon->action() == re4::PlayerAction::Footwork &&
              std::abs(std::remainder(leon->heading() +
                                         re4::SceneFollowState().releaseYawRadians -
                                         earlyHeading,
                                     6.28318530717958647692)) < 1e-6 &&
              std::abs(re4::SceneFollowState().verticalRatio - earlyPitch) < 1e-6,
          "Interrupted source draw carries unfinished world yaw and pitch into footwork");
  for (unsigned axis = 0; axis < 3; ++axis)
    require(std::abs((returnedPose.target[axis] - returnedFeet[axis]) -
                     (earlyPose.target[axis] - earlyFeet[axis])) < .001 &&
                std::abs((returnedPose.eye[axis] - returnedFeet[axis]) -
                         (earlyPose.eye[axis] - earlyFeet[axis])) < .001,
            "Interrupted draw preserves actual player-local eye and focus without a camera jump");
  const auto carriedYaw = re4::SceneFollowState().releaseYawRadians;
  ticks(jni, 2);
  require(std::abs(re4::SceneFollowState().releaseYawRadians) < std::abs(carriedYaw) &&
              std::abs(re4::SceneFollowState().releaseYawRadians) > std::abs(carriedYaw) * .85,
          "Returned draw view smoothly resumes neutral recenter without a snap");
  // A completed gun holster retains source residual waist yaw. Its view
  // hands that exact residual to the follower when the weapon view ends.
  controls(jni, 0, 1);
  ticks(jni, 18);
  controls(jni, 0, 1, 0, .5f);
  ticks(jni, 8);
  require(leon->action() == re4::PlayerAction::Aim &&
              std::abs(leon->waistYaw()) > .01,
          "Full holster fixture has a genuine source waist aim offset");
  controls(jni);
  bool completedHolster = false;
  for (unsigned tickIndex = 0; tickIndex < 40; ++tickIndex) {
    const auto beforeAction = leon->action();
    const auto beforePose = re4::SceneFollowPose();
    const auto beforeFeet = walkProbe->position();
    const double beforeHeading = leon->heading() + leon->waistYaw();
    const double beforePitch = leon->aimRate();
    tick(jni);
    if (leon->action() != re4::PlayerAction::Footwork)
      continue;
    require(beforeAction == re4::PlayerAction::Holster &&
                std::abs(std::remainder(leon->heading() +
                                           re4::SceneFollowState().releaseYawRadians -
                                           beforeHeading,
                                       6.28318530717958647692)) < 1e-6 &&
                std::abs(re4::SceneFollowState().verticalRatio - beforePitch) < 1e-6,
            "Completed holster preserves residual source waist yaw and view pitch");
    const auto afterPose = re4::SceneFollowPose();
    const auto afterFeet = walkProbe->position();
    for (unsigned axis = 0; axis < 3; ++axis)
      require(std::abs((afterPose.target[axis] - afterFeet[axis]) -
                       (beforePose.target[axis] - beforeFeet[axis])) < .001 &&
                  std::abs((afterPose.eye[axis] - afterFeet[axis]) -
                           (beforePose.eye[axis] - beforeFeet[axis])) < .001,
              "Completed holster hands the same actual optical eye and focus to footwork");
    completedHolster = true;
    break;
  }
  require(completedHolster, "Original gun holster reaches stationary footwork");
  re4::SceneFollowHeading(leon->heading());
  controls(jni, re4::Lb);
  ticks(jni, 18);
  require(leon->action() == re4::PlayerAction::KnifeReady &&
              leon->bladeVisible() && !leon->weaponVisible(),
          "Actual JNI L1 draws the original visible knife and hides the gun");
  const auto stationaryFeet = walkProbe->position();
  const auto rounds = leon->magazine(), shots = leon->shots();
  auto checkView = [&] {
    checkWeaponProfile();
  };
  checkView();
  for (float physicalX : {1.f, -1.f}) {
    const auto oldView = re4::SceneFollowPose();
    const double oldHeading = leon->heading() + leon->waistYaw();
    const re4::CollisionVec before{std::sin(oldHeading), 0, std::cos(oldHeading)};
    const auto cameraRight = oldView.right;
    controls(jni, re4::Lb, 0, 0, physicalX);
    ticks(jni, 30);
    const double newHeading = leon->heading() + leon->waistYaw();
    const re4::CollisionVec after{std::sin(newHeading), 0, std::cos(newHeading)};
    const double right = (after[0] - before[0]) * cameraRight[0] +
                         (after[2] - before[2]) * cameraRight[2];
    require(right * physicalX > .1 && walkProbe->position() == stationaryFeet,
            "Physical knife RS left/right moves toward actual camera-right");
    checkView();
  }
  for (float physicalY : {1.f, -1.f}) {
    const auto oldRate = leon->aimRate();
    controls(jni, re4::Lb, 0, 0, 0, physicalY);
    ticks(jni, 20);
    require((leon->aimRate() - oldRate) * physicalY > .2,
            "Physical RS-up raises knife aim and shoulder camera");
    checkView();
  }
  controls(jni, re4::Lb, 0, 1);
  tick(jni);
  require(leon->action() == re4::PlayerAction::KnifeSlash,
          "Actual JNI L1+RT starts the original knife slash");
  for (unsigned i = 0; i < 10; ++i) {
    tick(jni);
    checkView();
    require(leon->action() == re4::PlayerAction::KnifeSlash &&
                leon->magazine() == rounds && leon->shots() == shots,
            "Authored blade swing keeps camera stable and leaves ammo untouched");
  }
  paused = true;
  leon = std::move(savedPlayer);
  encounter = std::move(savedEnemy);
  walkProbe = std::move(savedProbe);
  input = savedInput;
  frame = savedFrame;
  inputGeneration = savedGeneration;
  seenInputGeneration = savedSeenGeneration;
  walkFrame = savedClock;
  hudInfo = savedHud;
  re4::RestoreSceneView(savedView);
  draw(jni);
  require(enemySnapshot() == savedEnemyState && actionSnapshot() == savedPlayerState &&
              re4::SaveSceneView().yaw == savedView.yaw &&
              re4::SaveSceneView().pitch == savedView.pitch &&
              sameFollowState(re4::SceneFollowState(), savedView.followCamera) &&
              sameVisualEffects(re4::SaveSceneView(), savedView),
          "Knife camera isolation restores exact paused encounter and gun state");
}
unsigned ambientVisibility(Jni &jni) {
  const auto before = animalSnapshot();
  std::vector<std::uint8_t> visible(960 * 540 * 4), hidden(visible.size());
  walkFrame = {};
  draw(jni);
  glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, visible.data());
  auto owner = std::move(ambientAnimals);
  walkFrame = {};
  draw(jni);
  glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, hidden.data());
  ambientAnimals = std::move(owner);
  walkFrame = {};
  draw(jni);
  require(animalSnapshot() == before,
          "Chicken framebuffer comparison preserves every original animal pose");
  unsigned pixels{};
  for (unsigned i = 0; i < 960 * 540; ++i)
    pixels += !std::equal(visible.begin() + i * 4, visible.begin() + i * 4 + 3,
                          hidden.begin() + i * 4);
  return pixels;
}
void ambientChecks(Jni &jni, Egl &egl, char **argv,
                   const std::function<void(jlong)> &create) {
  auto chicken = std::string(argv[7]);
  chicken.resize(chicken.find_last_of('/') + 1);
  chicken += "em28.drs";
  auto list = std::string(argv[8]);
  list.resize(list.find_last_of('/') + 1);
  list += "omake01.esl";
  Java_com_ast_re4decomp_NativeBridge_configureEnemy(
      &jni.env, nullptr, string(argv[7]), string(argv[8]));
  Java_com_ast_re4decomp_NativeBridge_configureAmbient(
      &jni.env, nullptr, string(chicken), string(list));
  Java_com_ast_re4decomp_NativeBridge_requestGanadoTest(&jni.env, nullptr);
  const auto request = token(jni, "r400");
  Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
      &jni.env, nullptr, static_cast<jlong>(request), string(argv[9]), string(""));
  const auto allocations = uniformAllocations;
  draw(jni);
  require(activeRoomId == 0x400 && ambientAnimals &&
              ambientAnimals->animals().size() == 6 &&
              uniformAllocations == allocations + 5 && actorGpuReady,
          "Production r400 arrival restores original six chickens and both variant GPUs");
  for (const auto &animal : ambientAnimals->animals())
    require(animal.source().id == 0x28 && animal.source().room == 0x400 &&
                animal.source().alive() && !animal.source().alreadySet() &&
                animal.variant() < 2,
            "Original active EM28 records retain source identity and model variants");
  const auto arrival = animalSnapshot();
  // Isolate the ambient motion check from the already validated melee chain.
  // The exact enemy owner is parked and then restored without a tick.
  auto parkedEnemy = std::move(encounter);
  controls(jni);
  ticks(jni, 45);
  require(ambientAnimals->ticks() >= arrival.ticks + 44 &&
              !(animalSnapshot() == arrival),
          "Actual bridge advances original chicken motion at game ticks");
  pause(jni);
  encounter = std::move(parkedEnemy);
  walkFrame = {};
  draw(jni);
  const auto frozen = animalSnapshot();
  ticks(jni, 8);
  require(animalSnapshot() == frozen,
          "Pause freezes all six chicken source placements, motion frames and bones");
  auto *const cpuOwner = ambientAnimals.get();
  const auto camera = re4::SaveSceneView();
  egl.fresh();
  const auto contextAllocations = uniformAllocations;
  create(202);
  draw(jni);
  require(ambientAnimals.get() == cpuOwner && animalSnapshot() == frozen &&
              uniformAllocations == contextAllocations + 5 &&
              sameFollowState(re4::SceneFollowState(), camera.followCamera) &&
              re4::SaveSceneView().followHeading == camera.followHeading &&
              sameVisualEffects(re4::SaveSceneView(), camera),
          "Context recreation retains six original animal CPUs and restores five actor GPUs");
  const auto player = actionSnapshot();
  const auto enemy = enemySnapshot();
  auto *const playerOwner = leon.get();
  auto *const enemyOwner = encounter.get();
  auto *const roomOwner = previewSmd.get();
  for (unsigned failingAllocation : {4u, 5u}) {
    Java_com_ast_re4decomp_NativeBridge_requestGanadoTest(&jni.env, nullptr);
    const auto failedRequest = token(jni, "r400");
    Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
        &jni.env, nullptr, static_cast<jlong>(failedRequest), string(argv[9]), string(""));
    const auto faults = failedUniformAllocations;
    failNthUniformAllocation = failingAllocation;
    draw(jni);
    require(failNthUniformAllocation == 0 && failedUniformAllocations == faults + 1,
            "Injected fault reaches each actual chicken-variant UBO allocation");
    require(ambientAnimals.get() == cpuOwner && previewSmd.get() == roomOwner &&
                leon.get() == playerOwner && encounter.get() == enemyOwner &&
                animalSnapshot() == frozen && actionSnapshot() == player &&
                enemySnapshot() == enemy && paused && actorGpuReady &&
                roomRequest.token == 0 &&
                sameFollowState(re4::SceneFollowState(), camera.followCamera) &&
                re4::SaveSceneView().followHeading == camera.followHeading &&
                sameVisualEffects(re4::SaveSceneView(), camera),
            "Chicken GPU failure rolls back exact live animals, combat, room, camera and all GPUs");
  }
  Java_com_ast_re4decomp_NativeBridge_configureAmbient(
      &jni.env, nullptr, string(chicken + ".missing"), string(list));
  Java_com_ast_re4decomp_NativeBridge_requestGanadoTest(&jni.env, nullptr);
  const auto missing = token(jni, "r400");
  Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
      &jni.env, nullptr, static_cast<jlong>(missing), string(argv[9]), string(""));
  draw(jni);
  require(ambientAnimals.get() == cpuOwner && animalSnapshot() == frozen &&
              actionSnapshot() == player && enemySnapshot() == enemy &&
              roomRequest.token == 0 && paused && actorGpuReady,
          "Missing chicken CPU asset preserves the existing live room and six animals");
  Java_com_ast_re4decomp_NativeBridge_configureAmbient(
      &jni.env, nullptr, string(chicken), string(list));
  // Changing a diagnostic render source must start its own animation clock.
  // Replaying a long full-room clock into the SAT mesh used to reject the
  // empty sprite profile and prevent returning to the normal view.
  unsigned worldRecords{};
  std::vector<unsigned> globalSequences;
  if (previewWorldEffects) {
    for (std::size_t i = 0; i < previewWorldEffects->sprites.targets.size(); ++i)
      if (previewWorldEffects->targetPrerun[i] != ~0u) {
        worldRecords += unsigned(previewWorldEffects->sprites.targets[i].records.size());
        globalSequences.push_back(previewWorldEffects->sequences[i]);
      }
  }
  require(previewWorldEffects && worldRecords == 13 &&
              previewWorldEffects->triggerBanks.size() == 1 &&
              globalSequences == std::vector<unsigned>({3, 13}),
          "Original r400 retains all thirteen supported fireplace-glow/light-shaft records in authored SST3/13");
  const auto beforeProfileView = re4::SaveSceneView();
  auto oldClockView = beforeProfileView;
  oldClockView.animationFrames = 36001;
  re4::RestoreSceneView(oldClockView);
  controls(jni, re4::Top);
  draw(jni);
  require(collisionView && re4::SaveSceneView().animationFrames == 0 &&
              sameFollowState(re4::SceneFollowState(), beforeProfileView.followCamera) &&
              actionSnapshot() == player && enemySnapshot() == enemy &&
              animalSnapshot() == frozen,
          "Long-clock source switch resets only the SAT animation profile and preserves the paused game");
  controls(jni);
  draw(jni);
  controls(jni, re4::Top);
  draw(jni);
  require(!collisionView && re4::SaveSceneView().animationFrames == 0 &&
              sameFollowState(re4::SceneFollowState(), beforeProfileView.followCamera) &&
              actionSnapshot() == player && enemySnapshot() == enemy &&
              animalSnapshot() == frozen && actorGpuReady &&
              re4::SaveSceneView().worldEffects &&
              re4::SaveSceneView().worldEffects->frames == 200,
          "Returning to original r400 restores its fireplace-glow/light-shaft profile without replaying the old clock");
  controls(jni);
  draw(jni);
  // Find a nearby, real SAT floor vantage for an authored chicken. Only the
  // host inspection actor is moved; no source animal is teleported or scaled.
  unsigned pixels{};
  for (const auto &animal : ambientAnimals->animals()) {
    if (!animal.onOriginalFloor())
      continue;
    for (const re4::CollisionVec offset :
         {re4::CollisionVec{0, 0, -2200}, {2200, 0, 0}, {0, 0, 2200},
          {-2200, 0, 0}, {1500, 0, -1500}, {-1500, 0, -1500}}) {
      auto position = animal.feet();
      for (unsigned axis = 0; axis < 3; ++axis)
        position[axis] += offset[axis];
      if (!walkProbe->place(position))
        continue;
      const auto feet = walkProbe->position();
      const double heading = std::atan2(animal.feet()[0] - feet[0],
                                        animal.feet()[2] - feet[2]);
      leon->setPlacement(feet, float(heading));
      re4::SceneFollowHeading(float(heading));
      pixels = ambientVisibility(jni);
      if (pixels >= 50)
        break;
    }
    if (pixels >= 50)
      break;
  }
  std::cerr << "Original EM28 visible framebuffer pixels=" << pixels << '\n';
  require(pixels >= 50,
          "Actual original chicken mesh and palette contribute visible GLES pixels");
  savePhase(argv[6], "r400-chickens");
}

unsigned objectInventoryCount(unsigned id) {
  return inventoryCase.count(id);
}
re4::PlayerShot containerShot(std::size_t index, unsigned serial) {
  const auto &object = objectActions->objects().at(index);
  for (float distance : {500.f, 800.f, 1200.f, 2000.f}) {
    for (const std::array<float, 2> direction :
         {std::array<float, 2>{0, 1}, {0, -1}, {1, 0}, {-1, 0}}) {
      const auto height = object.shapes.at(0).height * .5f;
      const auto from = re4::TransformPoint(
          object.matrix, {direction[0] * distance, height,
                          direction[1] * distance});
      const auto to = re4::TransformPoint(
          object.matrix, {-direction[0] * distance, height,
                          -direction[1] * distance});
      re4::PlayerShot shot{serial, {from[0], from[1], from[2]},
                           {to[0], to[1], to[2]}};
      const auto hit = objectActions->target(shot.from, shot.to);
      if (hit && hit->index == index)
        return shot;
    }
  }
  throw std::runtime_error("No EAT-clear authored container ray for original slot " +
                           std::to_string(object.placement.slot));
}
std::size_t breakContainer(unsigned id, unsigned &serial) {
  require(encounter && objectActions, "Container test uses live production encounter/object owners");
  for (std::size_t index = 0; index < objectActions->objects().size(); ++index) {
    if (objectActions->objects()[index].item.id != id ||
        objectActions->states()[index].broken)
      continue;
    re4::PlayerShot shot;
    try {
      shot = containerShot(index, ++serial);
    } catch (const std::runtime_error &) {
      continue;
    }
    const auto previousHp = encounter->hp();
    encounter->tick(leon->feet(), &shot);
    require(objectActions->states()[index].broken &&
                objectActions->states()[index].hp == 0 &&
                objectActions->states()[index].itemActive &&
                !objectActions->states()[index].itemTaken &&
                encounter->hp() == previousHp,
            "Actual encounter shot breaks nearest original container and reveals its ITA pickup");
    const auto saved = objectActions->snapshot();
    encounter->tick(leon->feet(), &shot);
    require(objectActions->snapshot().checksum == saved.checksum,
            "One authored shot serial cannot break or duplicate the same container twice");
    return index;
  }
  throw std::runtime_error("No unbroken original container with item " + std::to_string(id));
}
bool placeForPickup(std::size_t index) {
  const auto &item = objectActions->objects().at(index).item;
  constexpr double pi = 3.14159265358979323846;
  for (double distance : {750., 1050., 1300., 400., 1600.}) {
    for (unsigned direction = 0; direction < 24; ++direction) {
      const double angle = direction * pi / 12;
      auto position = item.position;
      position[0] += std::sin(angle) * distance;
      position[1] -= 50;
      position[2] += std::cos(angle) * distance;
      const auto floor = collisionWorld->ground(position, 500, 2000);
      if (!floor || !walkProbe->place(floor->point))
        continue;
      const auto feet = walkProbe->position();
      const auto heading = std::atan2(item.position[0] - feet[0],
                                       item.position[2] - feet[2]);
      auto center = feet, front = feet;
      center[1] += 250;
      front[1] += 250;
      front[0] += std::sin(heading) * 550;
      front[2] += std::cos(heading) * 550;
      if (const auto hit = collisionWorld->line(center, front, re4::SatSubset::All, 0))
        front = hit->point;
      const auto selected = objectActions->pickupTarget(feet, heading, front);
      if (!selected || *selected != index ||
          (roomFlow && roomFlow->select(feet, heading, false, false, front).door))
        continue;
      leon->setPlacement(feet, float(heading));
      re4::SceneFollowHeading(float(heading));
      return true;
    }
  }
  return false;
}
std::vector<std::uint8_t> frozenPixels(Jni &jni) {
  walkFrame = {};
  draw(jni);
  std::vector<std::uint8_t> pixels(960 * 540 * 4);
  glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  require(glGetError() == GL_NO_ERROR, "Actual item framebuffer read");
  return pixels;
}
unsigned pixelChanges(const std::vector<std::uint8_t> &a,
                       const std::vector<std::uint8_t> &b) {
  unsigned result{};
  for (std::size_t offset = 0; offset < a.size(); offset += 4)
    result += !std::equal(a.begin() + offset, a.begin() + offset + 3,
                          b.begin() + offset);
  return result;
}
void collectContainer(Jni &jni, std::size_t index, bool requireVisible,
                      const std::string &capture) {
  require(placeForPickup(index), "Authored ITA pickup has a real SAT-valid controller approach");
  require(leon->action() == re4::PlayerAction::Footwork,
          "Original item action starts in ordinary footwork");
  // Pause is released through the actual JNI button. The enemy is parked only
  // while inspecting this source pickup, then returned with its room pointer.
  auto parkedEnemy = std::move(encounter);
  if (paused) {
    controls(jni, re4::Start);
    frozenPixels(jni);
  }
  controls(jni, 0, 0, 0, 0, -1); // Physical down: inspect the ground pickup.
  ticks(jni, 2);
  const auto before = frozenPixels(jni);
  if (requireVisible)
    savePhase(capture, "r400-pickup-active");
  const auto player = actionSnapshot();
  const unsigned previousReserve = leon->reserve();
  const auto id = objectActions->objects()[index].item.id;
  const auto count = objectActions->objects()[index].item.count;
  controls(jni, re4::Bottom, 0, 0, 0, -1);
  frozenPixels(jni);
  require(pendingPickup.token && !pendingPickup.loose && pendingPickup.index == index &&
              paused && leon->itemExamActive() && !objectActions->states()[index].itemTaken,
          "Physical A opens the original item examination before committing its source container");
  Java_com_ast_re4decomp_NativeBridge_answerPickup(&jni.env, nullptr,
      static_cast<jlong>(pendingPickup.token), JNI_TRUE);
  const auto after = frozenPixels(jni);
  require(!pendingPickup.token && !leon->itemExamActive() && objectActions->inventory().empty(),
          "Confirmed original source-container item transfers into the single authoritative case");
  require(objectActions->states()[index].itemTaken &&
              !objectActions->states()[index].itemActive &&
              objectActions->states()[index].broken,
          "Physical A controller edge collects the original ITA item once");
  require(leon->rig().bones() == player.body &&
              leon->shots() == player.shots && leon->magazine() == player.magazine,
          "Pickup changes supplies without advancing paused-source bones or firing");
  if (id == 4)
    require(leon->reserve() == previousReserve + count && count == 20 &&
                objectInventoryCount(4) == previousReserve + count,
            "Original character-linked handgun pickup transfers exactly twenty rounds");
  else
    require(objectInventoryCount(id) >= count,
            "Non-ammo pickup appears in the production supply inventory");
  const auto changed = pixelChanges(before, after);
  if (requireVisible) {
    std::cerr << "Original ITM pickup visible framebuffer pixels=" << changed << '\n';
    require(changed >= 10, "Original room ITM pickup contributes visible pixels before collection");
    savePhase(capture, "r400-pickup-collected");
  }
  const auto collected = objectActions->snapshot();
  frozenPixels(jni); // Still-held A cannot manufacture another action edge.
  require(objectActions->snapshot().checksum == collected.checksum &&
              (id != 4 || leon->reserve() == previousReserve + count),
          "Held physical A cannot duplicate collected contents or ammunition");
  controls(jni);
  pause(jni);
  encounter = std::move(parkedEnemy);
  walkFrame = {};
  draw(jni);
}
void objectActionChecks(Jni &jni, Egl &egl, char **argv,
                        const std::function<void(jlong)> &create) {
  require(activeRoomId == 0x400 && objectActions && previewItems &&
              objectActions->objects().size() == 16 && previewItems->rendered == 13,
          "Production r400 owns thirteen wooden and three explosive source containers with thirteen ITM placements");
  unsigned serial = leon->shots() + 1000;
  const auto ammo = breakContainer(4, serial);
  collectContainer(jni, ammo, true, argv[6]);
  const auto green = breakContainer(6, serial);
  collectContainer(jni, green, false, argv[6]);
  // Full HP rejects the mailbox request without consuming the original herb.
  auto parkedEnemy = std::move(encounter);
  controls(jni);
  frozenPixels(jni);
  require(paused && leon->life() == re4::LeonPreview::MaxLife &&
              objectInventoryCount(6) == 1,
          "Paused herb fixture starts alive at full health with one collected green herb");
  const auto full = objectActions->snapshot();
  Java_com_ast_re4decomp_NativeBridge_requestUseGreenHerb(&jni.env, nullptr);
  frozenPixels(jni);
  require(leon->life() == 1200 && objectActions->snapshot().checksum == full.checksum,
          "Full-health native herb request consumes no inventory");
  require(leon->unlimitedHealth(), "Production defaults to unlimited testing health");
  leon->takeDamage(900, false);
  require(leon->life() == 1200, "Testing health blocks damage without consuming the herb");
  // Explicitly disable the development policy only inside the original
  // healing-helper regression. No Android UI can disable it.
  leon->setUnlimitedHealth(false);
  leon->takeDamage(900, false);
  require(leon->life() == 300, "Herb fixture applies real player damage helper");
  Java_com_ast_re4decomp_NativeBridge_requestUseGreenHerb(&jni.env, nullptr);
  Java_com_ast_re4decomp_NativeBridge_requestUseGreenHerb(&jni.env, nullptr);
  frozenPixels(jni);
  require(leon->life() == 900 && objectInventoryCount(6) == 0,
          "Paused coalesced native requests consume one green herb and restore exactly six hundred HP");
  frozenPixels(jni);
  Java_com_ast_re4decomp_NativeBridge_requestUseGreenHerb(&jni.env, nullptr);
  frozenPixels(jni);
  require(leon->life() == 900 && objectInventoryCount(6) == 0,
          "Consumed herb cannot heal again on another draw or request");
  pause(jni);
  encounter = std::move(parkedEnemy);
  const auto secondGreen = breakContainer(6, serial);
  collectContainer(jni, secondGreen, false, argv[6]);
  require(paused && leon->life() == 900 && objectInventoryCount(6) == 1,
          "Second original green herb is retained for source-room cache verification");
  const auto stable = objectActions->snapshot();
  auto *const owner = objectActions.get();
  const auto itemOwner = previewItems.get();
  const auto enemyOwner = encounter.get();
  const auto reserve = leon->reserve();
  const auto savedPlayer = actionSnapshot();
  const auto savedEnemy = enemySnapshot();
  const auto savedView = re4::SaveSceneView();
  egl.fresh();
  create(202);
  draw(jni);
  require(objectActions.get() == owner && previewItems.get() == itemOwner &&
              encounter.get() == enemyOwner && objectActions->snapshot().checksum == stable.checksum &&
              actionSnapshot() == savedPlayer && enemySnapshot() == savedEnemy &&
              sameFollowState(re4::SceneFollowState(), savedView.followCamera) && paused,
          "GL context recreation preserves collected ITA contents, supplies and borrowed encounter owner");
  // GPU rollback must return the exact previous object runtime; the encounter
  // then shoots another actual source container through that restored pointer.
  Java_com_ast_re4decomp_NativeBridge_requestGanadoTest(&jni.env, nullptr);
  const auto failed = token(jni, "r400");
  Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
      &jni.env, nullptr, static_cast<jlong>(failed), string(argv[9]), string(""));
  failNthUniformAllocation = 3;
  const auto faults = failedUniformAllocations;
  draw(jni);
  require(failNthUniformAllocation == 0 && failedUniformAllocations == faults + 1 &&
              objectActions.get() == owner && previewItems.get() == itemOwner &&
              encounter.get() == enemyOwner && paused && actorGpuReady &&
              objectActions->snapshot().checksum == stable.checksum &&
              actionSnapshot() == savedPlayer && enemySnapshot() == savedEnemy,
          "Third-GPU failure restores exact objects, pickups, supplies and encounter borrowed lifetimes");
  const auto extra = breakContainer(4, serial);
  require(objectActions->states()[extra].itemActive,
          "Rolled-back encounter targets the restored room's live object runtime");
  const auto beforeRestart = objectActions->snapshot();
  Java_com_ast_re4decomp_NativeBridge_requestGanadoTest(&jni.env, nullptr);
  const auto restarted = token(jni, "r400");
  Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
      &jni.env, nullptr, static_cast<jlong>(restarted), string(argv[9]), string(""));
  draw(jni);
  require(objectActions.get() != owner && encounter.get() != enemyOwner &&
              objectActions->snapshot().checksum == beforeRestart.checksum &&
              objectActions->states()[ammo].itemTaken &&
              objectActions->states()[green].itemTaken &&
              objectActions->states()[secondGreen].itemTaken &&
              objectActions->states()[extra].itemActive &&
              leon->reserve() == reserve && actorGpuReady && objectInventoryCount(6) == 1,
          "Combat restart restores source-container cache without respawning pickups or duplicating ammo");
  pause(jni);
  // A menu request received while a real room worker is pending must wait.
  // The completed r301 has no active ObjectActions; its herb remains owned
  // by the case, proving supplies travel with Leon across room changes.
  leon->setUnlimitedHealth(false);
  leon->takeDamage(900, false);
  require(leon->life() == 300, "Inactive-room herb fixture has missing health");
  const auto beforeLeave = objectActions->snapshot();
  Java_com_ast_re4decomp_NativeBridge_requestDoorTest(&jni.env, nullptr);
  const auto leave = token(jni);
  Java_com_ast_re4decomp_NativeBridge_requestUseGreenHerb(&jni.env, nullptr);
  frozenPixels(jni);
  require(activeRoomId == 0x400 && transferPending() && useGreenRequested &&
              objectInventoryCount(6) == 1 && leon->life() == 300 &&
              objectActions->snapshot().checksum == beforeLeave.checksum,
          "Pending room transfer retains herb mailbox without early health or inventory mutation");
  Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
      &jni.env, nullptr, static_cast<jlong>(leave), string(argv[4]), string(""));
  frozenPixels(jni);
  require(activeRoomId == 0x301 && !objectActions && !encounter &&
              !useGreenRequested && leon->life() == 900 &&
              objectRoomStates.count(0x400) &&
              objectRoomStates.at(0x400).checksum == beforeLeave.checksum && inventoryCase.count(6) == 0,
          "Completed room change consumes one carried case herb and heals once without mutating its source-container cache");
  const auto cachedConsumed = objectRoomStates.at(0x400);
  for (const auto &item : cachedConsumed.inventory)
    require(item.id != 6, "Inactive source cache records consumed green herb exactly");
  require(text(Java_com_ast_re4decomp_NativeBridge_inventory(&jni.env, nullptr)).find(
              "Green herb used") != std::string::npos,
          "Native supplies mailbox publishes the actual healing result for the UI");
  frozenPixels(jni);
  require(leon->life() == 900 && objectRoomStates.at(0x400).checksum == cachedConsumed.checksum,
          "Completed mailbox cannot repeat cached-room healing on another frame");
  pause(jni);
  Java_com_ast_re4decomp_NativeBridge_requestGanadoTest(&jni.env, nullptr);
  const auto returnRoom = token(jni, "r400");
  Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
      &jni.env, nullptr, static_cast<jlong>(returnRoom), string(argv[9]), string(""));
  frozenPixels(jni);
  require(activeRoomId == 0x400 && objectActions && encounter && actorGpuReady &&
              objectActions->snapshot().checksum == cachedConsumed.checksum &&
              objectInventoryCount(6) == 0 && objectActions->states()[secondGreen].itemTaken,
          "Reopening r400 validates inactive-cache checksum and cannot respawn the consumed herb");
  leon->setUnlimitedHealth(true);
  pause(jni);
  std::cerr << "Production r400 objects: " << objectActions->describe()
            << " reserve=" << leon->reserve() << '\n';
}
} // namespace bridge_test

// GNU ld wraps only this host executable. Every normal allocation still uses
// the real GLES implementation; one armed actor UBO allocation receives an
// invalid size, then rollback is free to recreate its GPU resources normally.
extern "C" void __real_glBufferData(GLenum, GLsizeiptr, const void *, GLenum);
extern "C" void __wrap_glBufferData(GLenum target, GLsizeiptr size,
                                    const void *data, GLenum usage) {
  bool fail = false;
  if (target == GL_UNIFORM_BUFFER) {
    ++bridge_test::uniformAllocations;
    if (bridge_test::failNextUniformAllocation) {
      bridge_test::failNextUniformAllocation = false;
      fail = true;
    } else if (bridge_test::failNthUniformAllocation) {
      fail = --bridge_test::failNthUniformAllocation == 0;
    }
  }
  if (fail) {
    ++bridge_test::failedUniformAllocations;
    __real_glBufferData(target, -1, data, usage);
  } else {
    __real_glBufferData(target, size, data, usage);
  }
}

int main(int argc, char **argv) {
  using namespace bridge_test;
  try {
    if (argc != 7 && argc != 10)
      throw std::runtime_error(
          "Usage: android_bridge_gl_check PL00.drs WEP01.drs R300.dat R301.dat "
          "roominfo.dat OUTPUT.ppm [EM12.drs emleon00.esl R400.dat]");
    Egl egl;
    Jni jni;
    auto corePath=std::string(argv[1]);
    corePath=corePath.substr(0,corePath.find_last_of('/'))+"/../etc/core.das";
    Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(corePath));
    require(!coreEnvironmentEff.empty(),"Production JNI loads primary CORE EFF");
    auto create = [&](jlong session) {
      Java_com_ast_re4decomp_NativeBridge_createScene(
          &jni.env, nullptr, string(argv[3]), string(argv[1]), string(argv[2]),
          string(argv[5]), string("r300"), session);
      require(javaException.empty(), "Initial createScene JNI failure");
    };
    create(101);
    draw(jni);
    require(activeRoomId == 0x300 && walking && leon && walkProbe && roomFlow &&
                roomInfo,
            "Production bridge initial room/player/flow");
    auto initialFeet = walkProbe->position();
    auto initialActor = leon.get();
    auto initialPose = leon->rig().bones();
    Java_com_ast_re4decomp_NativeBridge_requestDoorTest(&jni.env, nullptr);
    auto first = token(jni);
    egl.release();
    Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
        &jni.env, nullptr, static_cast<jlong>(first), string(argv[4]),
        string(""));
    require(activeRoomId == 0x300 && walking && leon.get() == initialActor &&
                walkProbe->position() == initialFeet &&
                leon->rig().bones() == initialPose,
            "Worker completion without EGL leaves live scene/actor untouched");
    require(roomRequest.token == first,
            "Completion waits for a current GL draw");
    egl.fresh();
    create(101);
    require(activeRoomId == 0x300 && roomRequest.token == first,
            "Context recreation preserves pending room transfer");
    draw(jni);
    require(activeRoomId == 0x301 && roomRequest.token == 0 && walking && leon,
            "Actual draw commits the verified destination room");
    auto start301 = roomInfo->find(0x301);
    require(
        start301 &&
            std::abs(walkProbe->position()[0] - start301->position[0]) < .01 &&
            std::abs(walkProbe->position()[2] - start301->position[2]) < .01,
        "Destination uses original r301 start");
    save(argv[6]);
    for (bool returnTo301 : {false, true}) {
      const std::uint16_t destination = returnTo301 ? 0x301 : 0x300;
      const auto *door = roomFlow->find(0);
      require(door && door->destinationRoom == destination,
              "Production source AEV supplies the allowlisted opposite room");
      const auto destinationFeet = door->destination;
      const auto destinationHeading = float(door->destinationHeading);
      turn180(jni);
      Java_com_ast_re4decomp_NativeBridge_input(&jni.env, nullptr, re4::Bottom,
                                                0, 0, 0, 0, 0, 0);
      tick(jni);
      auto actionToken = token(jni, roomName(destination));
      Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
          &jni.env, nullptr, static_cast<jlong>(actionToken),
          string(returnTo301 ? argv[4] : argv[3]), string(""));
      draw(jni);
      require(
          activeRoomId == destination && roomRequest.token == 0 && leon &&
              std::abs(walkProbe->position()[0] - destinationFeet[0]) < .01 &&
              std::abs(walkProbe->position()[2] - destinationFeet[2]) < .01 &&
              std::abs(std::remainder(leon->heading() - destinationHeading,
                                      6.28318530717958647692f)) < 1e-6,
          "Actual A/door selection commits original AEV destination/heading");
      for (unsigned i = 0; i < 3; ++i)
        tick(jni); // The same physical A remains held throughout the load.
      require(roomRequest.token == 0 && activeRoomId == destination,
              "Held A cannot retrigger a room transfer after commit");
      Java_com_ast_re4decomp_NativeBridge_input(&jni.env, nullptr, 0, 0, 0, 0,
                                                0, 0, 0);
      tick(jni);
    }
    pause(jni);
    auto feet = walkProbe->position();
    auto heading = leon->heading();
    auto pose = leon->rig().bones();
    auto sequence = leon->rig().sequenceFrame();
    auto cpuRoom = previewSmd.get();
    for (bool missingFile : {false, true}) {
      Java_com_ast_re4decomp_NativeBridge_requestDoorTest(&jni.env, nullptr);
      auto failed = token(jni);
      egl.release();
      Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
          &jni.env, nullptr, static_cast<jlong>(failed),
          string(missingFile ? std::string(argv[4]) + ".missing" : ""),
          string(missingFile ? "" : "Host test: missing Disc 2"));
      require(activeRoomId == 0x301 && walkProbe->position() == feet,
              "Failed completion also waits without modifying old room");
      egl.current();
      draw(jni);
      require(activeRoomId == 0x301 && previewSmd.get() == cpuRoom &&
                  roomRequest.token == 0 && walkProbe->position() == feet &&
                  leon && leon->heading() == heading &&
                  leon->rig().bones() == pose &&
                  leon->rig().sequenceFrame() == sequence && paused,
              "Decode/missing-file failure preserves room/position/pose/pause");
    }
    auto actor = leon.get();
    auto originalView = re4::SaveSceneView();
    originalView.pitch = .65f;
    originalView.followDistance = 9000;
    originalView.animationFrames = 47;
    originalView.followCamera = {.2, -.4, .25};
    re4::RestoreSceneView(originalView);
    draw(jni);
    originalView = re4::SaveSceneView();
    auto yaw = re4::SceneCameraYaw();
    egl.fresh();
    create(101); // The Activity's intent still names its initial r300.
    require(activeRoomId == 0x301 && previewSmd.get() == cpuRoom &&
                leon.get() == actor && walkProbe->position() == feet &&
                leon->heading() == heading && leon->rig().bones() == pose &&
                leon->rig().sequenceFrame() == sequence && paused,
            "Same-session GL recreation retains current room/player/pose");
    require(std::abs(std::remainder(re4::SceneCameraYaw() - yaw,
                                    6.28318530717958647692f)) < 1e-6,
            "Same-session recreation retains camera yaw");
    auto restoredView = re4::SaveSceneView();
    require(restoredView.pitch == originalView.pitch &&
                restoredView.followDistance == originalView.followDistance &&
                restoredView.distance == originalView.distance &&
                restoredView.animationFrames == originalView.animationFrames &&
                sameFollowState(restoredView.followCamera,
                                originalView.followCamera) &&
                restoredView.followHeading == originalView.followHeading &&
                sameVisualEffects(restoredView, originalView),
            "Same-session recreation retains complete follower/checkpoint/UV clock");
    draw(jni);
    Java_com_ast_re4decomp_NativeBridge_requestDoorTest(&jni.env, nullptr);
    auto oldToken = token(jni);
    egl.fresh();
    create(202);
    require(
        activeRoomId == 0x300 && roomRequest.token == 0,
        "New Activity session starts its requested room and clears transfer");
    Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
        &jni.env, nullptr, static_cast<jlong>(oldToken), string(argv[4]),
        string(""));
    draw(jni);
    require(
        activeRoomId == 0x300 && roomRequest.token == 0,
        "Late completion from previous session cannot replace current room");
    pause(jni);
    Java_com_ast_re4decomp_NativeBridge_requestDoorTest(&jni.env, nullptr);
    auto nextToken = token(jni);
    require(nextToken > oldToken,
            "Request tokens do not repeat across sessions");
    Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
        &jni.env, nullptr, static_cast<jlong>(oldToken), string(argv[4]),
        string(""));
    draw(jni);
    require(activeRoomId == 0x300 && roomRequest.token == nextToken,
            "Stale token cannot complete a newer request");
    const auto oldCpuRoom = previewSmd.get();
    const auto oldCpuActor = leon.get();
    const auto oldCpuFeet = walkProbe->position();
    const auto oldCpuHeading = leon->heading();
    const auto oldCpuPose = leon->rig().bones();
    const auto oldCpuPhase = leon->rig().sequenceFrame();
    auto faultView = re4::SaveSceneView();
    faultView.pitch = .7f;
    faultView.followDistance = 8000;
    faultView.animationFrames = 91;
    faultView.followCamera = {-.3, .2, .75};
    re4::RestoreSceneView(faultView);
    draw(jni);
    faultView = re4::SaveSceneView();
    Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
        &jni.env, nullptr, static_cast<jlong>(nextToken), string(argv[4]),
        string(""));
    failNextUniformAllocation = true;
    draw(jni);
    require(!failNextUniformAllocation && failedUniformAllocations == 1,
            "Host-only fault reached the actual actor GPU UBO upload");
    require(
        activeRoomId == 0x300 && previewSmd.get() == oldCpuRoom &&
            leon.get() == oldCpuActor && walkProbe->position() == oldCpuFeet &&
            leon->heading() == oldCpuHeading &&
            leon->rig().bones() == oldCpuPose &&
            leon->rig().sequenceFrame() == oldCpuPhase && paused &&
            actorGpuReady && roomRequest.token == 0,
        "Actor GPU failure restores old room/CPU actor/pose/phase/GPU/pause");
    auto recoveredView = re4::SaveSceneView();
    require(recoveredView.yaw == faultView.yaw &&
                recoveredView.pitch == faultView.pitch &&
                recoveredView.distance == faultView.distance &&
                recoveredView.followDistance == faultView.followDistance &&
                recoveredView.animationFrames == faultView.animationFrames &&
                sameFollowState(recoveredView.followCamera,
                                faultView.followCamera) &&
                recoveredView.followHeading == faultView.followHeading &&
                sameVisualEffects(recoveredView, faultView),
            "Actor GPU rollback restores full camera and animation clock");
    Java_com_ast_re4decomp_NativeBridge_requestDoorTest(&jni.env, nullptr);
    const auto recoveredToken = token(jni);
    require(recoveredToken > nextToken,
            "GPU failure permits a fresh room request");
    Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
        &jni.env, nullptr, static_cast<jlong>(recoveredToken), string(argv[4]),
        string(""));
    draw(jni);
    require(
        activeRoomId == 0x301 && roomRequest.token == 0 && leon &&
            actorGpuReady,
        "Good request commits after stale completion and actor GPU failure");

    // Drive the actual JNI mapping. Triggers are physical analog values;
    // no direct LeonPreview action calls are used by these integration checks.
    controls(jni);
    ticks(jni, 2);
    auto stationaryFeet = walkProbe->position();
    const auto followView = re4::SaveSceneView();
    const auto followHeading = leon->heading();
    for (float physicalX : {1.f, -1.f}) {
      re4::RestoreSceneView(followView);
      const auto beforeFollowFeet = walkProbe->position();
      controls(jni, 0, 0, 0, physicalX);
      ticks(jni, 6);
      const auto heldLook = re4::SceneFollowState();
      const auto feet = walkProbe->position();
      const auto idleDrift = std::hypot(
          std::hypot(feet[0] - beforeFollowFeet[0], feet[2] - beforeFollowFeet[2]),
          feet[1] - beforeFollowFeet[1]);
      // Original idle flags5 carry tiny authored root motion. The longer
      // bounded-look exercise reaches a different cycle phase (1.12 units in
      // six ticks on PL00), while actual locomotion would move hundreds.
      require(heldLook.yawRatioRadians * physicalX < -.7 &&
                  std::abs(heldLook.yawRatioRadians) <=
                      1.04719755119659774616 + 1e-8 &&
                  leon->heading() == followHeading &&
                  idleDrift < 2,
              "Physical RS sets original bounded shoulder yaw without turning Leon");
      ticks(jni, 24);
      const auto settledLook = re4::SceneFollowState();
      require(std::abs(settledLook.yawRatioRadians) > std::abs(heldLook.yawRatioRadians) &&
                  std::abs(settledLook.yawRatioRadians) <= 1.04719755119659774616,
              "Held physical RS smoothly approaches its bounded source yaw");
      checkShoulderCamera("Bounded shoulder yaw retains explicit optical target");
      controls(jni);
      ticks(jni, 2);
      const auto recenter = re4::SceneFollowState();
      // tick() uses the actual steady clock, so its elapsed time can cross
      // a partial 30-Hz boundary. Exact rate equivalence is checked by the
      // deterministic follow_camera_tests; this checks native input continuity.
      require(std::abs(recenter.yawRatioRadians) < std::abs(settledLook.yawRatioRadians) &&
                  std::abs(recenter.yawRatioRadians) > std::abs(settledLook.yawRatioRadians) * .85 &&
                  std::abs(recenter.desiredYaw) < std::abs(settledLook.desiredYaw),
              "Physical neutral smoothly recenters across the real clock boundary");
    }
    for (float physicalY : {1.f, -1.f}) {
      re4::RestoreSceneView(followView);
      controls(jni, 0, 0, 0, 0, physicalY);
      ticks(jni, 6);
      require(re4::SceneFollowState().verticalRatio * physicalY > .7,
              "Physical RS-up selects the original upper shoulder sites");
      checkShoulderCamera("Vertical shoulder look retains explicit optical target");
    }
    controls(jni);
    ticks(jni, 2);
    // Original idle flags5 retain their tiny authored root movement. Capture
    // the gun's stationary baseline after that footwork, before the draw.
    stationaryFeet = walkProbe->position();
    auto drawLook = followView;
    drawLook.followCamera = {0, 0, .5, 0};
    re4::RestoreSceneView(drawLook);
    const auto headingBeforeDraw = leon->heading();
    re4::InputMapper drawMapper;
    re4::RawInput drawRaw;
    drawRaw.rx = .4f;
    drawRaw.ry = -.3f;
    drawRaw.lt = 1;
    const auto drawMapped = drawMapper.poll(drawRaw);
    const double expectedDrawYaw = drawMapped.rx * 1.04719755119659774616 * .2;
    const double expectedDrawPitch = drawMapped.ry * .2;
    // First-frame RS+LT must sample the current C-stick before ready00,
    // then render the same sample exactly once at the 30-Hz boundary.
    controls(jni, 0, 1, 0, drawRaw.rx, drawRaw.ry);
    tick(jni);
    require(leon->action() == re4::PlayerAction::Draw &&
                std::abs(std::remainder(leon->heading() +
                                           leon->drawCameraYawRemaining() -
                                           headingBeforeDraw - expectedDrawYaw,
                                       6.28318530717958647692)) < .001 &&
                std::abs(leon->aimRate() - expectedDrawPitch) < .001 &&
                std::abs(re4::SceneFollowState().accumulator - .5) < .05,
            "Same-frame physical RS+LT seeds the new look before original draw and advances camera once");
    checkWeaponProfile();
    controls(jni, 0, 1);
    for (unsigned i = 0; i < 17; ++i) {
      tick(jni);
      checkWeaponProfile();
    }
    require(walking && !paused && leon->action() == re4::PlayerAction::Aim &&
                leon->magazine() == 10 && leon->reserve() == 60 &&
                walkProbe->position() == stationaryFeet,
            "Production LT draws original gun and reaches stationary aim");
    require(std::abs(std::remainder(leon->heading() - headingBeforeDraw - expectedDrawYaw,
                                    6.28318530717958647692)) < .001 &&
                std::abs(leon->aimRate() - expectedDrawPitch) < .001,
            "Original draw hands bounded look yaw and pitch to Leon without recenter snap");
    for (float physicalX : {1.f, -1.f}) {
      const auto initialDirection = leon->aimDirection();
      const auto cameraRight = re4::SceneFollowPose().right;
      controls(jni, 0, 1, 0, physicalX);
      ticks(jni, 30);
      const auto direction = leon->aimDirection();
      const double movedRight =
          (direction[0] - initialDirection[0]) * cameraRight[0] +
          (direction[2] - initialDirection[2]) * cameraRight[2];
      require(movedRight * physicalX > .1 &&
                  walkProbe->position() == stationaryFeet &&
                  leon->shots() == 0 && leon->magazine() == 10,
              "Physical RS left/right moves posed muzzle toward actual camera-right");
    }
    controls(jni, 0, 1);
    ticks(jni, 2);
    savePhase(argv[6], "aim");
    const auto aimCamera = re4::SaveSceneView();
    // ControllerActivity has already inverted Android's downward-positive Y
    // before passing it to the production JNI entry point.
    controls(jni, 0, 1, 0, 0, 1);
    ticks(jni, 20);
    require(leon->aimRate() > .3f && leon->aimDirection()[1] > .2 &&
                walkProbe->position() == stationaryFeet,
            "Production right-stick up raises the posed hand/muzzle");
    const auto movedAimCamera = re4::SaveSceneView();
    require(movedAimCamera.followDistance == aimCamera.followDistance &&
                movedAimCamera.distance == aimCamera.distance,
            "Aim input leaves common shoulder framing settings unchanged");
    checkWeaponProfile();
    controls(jni, 0, 1, 1);
    ticks(jni, 3);
    require(leon->action() == re4::PlayerAction::Fire &&
                leon->magazine() == 9 && leon->shots() == 1 &&
                leon->weapon()->motion() == re4::WeaponMotion::Fire,
            "Production RT spends one round and animates player and gun");
    checkWeaponProfile();
    controls(jni, re4::Start, 1, 1);
    draw(jni);
    require(paused, "Start pauses while both weapon triggers remain held");
    const auto pausedAction = actionSnapshot();
    controls(jni, 0, 1, 1);
    ticks(jni, 8);
    require(actionSnapshot() == pausedAction,
            "Pause freezes exact player/gun pose, frame and ammunition");
    checkWeaponProfile();
    controls(jni, re4::Start, 1, 1);
    draw(jni);
    require(!paused, "Start resumes with the same physical triggers held");
    controls(jni, 0, 1, 1);
    ticks(jni, 40);
    require(
        leon->magazine() == pausedAction.magazine &&
            leon->reserve() == pausedAction.reserve &&
            leon->shots() == pausedAction.shots,
        "Held RT after resume cannot spend extra ammunition before neutral");
    controls(jni);
    ticks(jni, 2);
    controls(jni, 0, 1, 1);
    ticks(jni, 8);
    require(leon->action() == re4::PlayerAction::Fire &&
                leon->magazine() == 8 && leon->shots() == 2,
            "Physical neutral re-arms the next original draw/shot");

    const auto beforeWeaponContext = actionSnapshot();
    const auto weaponContextActor = leon.get();
    egl.fresh();
    create(202);
    require(
        leon.get() == weaponContextActor && actorGpuReady &&
            actionSnapshot() == beforeWeaponContext,
        "Same-session context recreation preserves exact active gun/action");
    draw(jni);
    require(
        actionSnapshot() == beforeWeaponContext,
        "First recreated-context draw preserves action frame and ammunition");
    ticks(jni, 40);
    require(leon->magazine() == beforeWeaponContext.magazine &&
                leon->reserve() == beforeWeaponContext.reserve &&
                leon->shots() == beforeWeaponContext.shots,
            "Context recreation also requires neutral before held RT can fire");
    controls(jni);
    ticks(jni, 2);
    controls(jni, 0, 1);
    ticks(jni, 18);
    const auto beforeReload = actionSnapshot();
    controls(jni, re4::Left, 1);
    tick(jni);
    require(walking && leon->action() == re4::PlayerAction::Reload &&
                leon->weapon()->motion() == re4::WeaponMotion::Reload,
            "Production X starts original reload without entering inspector");
    checkWeaponProfile();
    controls(jni, 0, 1);
    ticks(jni, 20);
    require(leon->magazine() == beforeReload.magazine &&
                leon->reserve() == beforeReload.reserve,
            "Reload animation cannot refill before its original gun event");
    ticks(jni, 20);
    require(leon->magazine() == 10 &&
                leon->reserve() ==
                    beforeReload.reserve - (10 - beforeReload.magazine) &&
                leon->shots() == beforeReload.shots,
            "Original reload event transfers bounded reserve exactly once");
    ticks(jni, 18);
    require(leon->action() == re4::PlayerAction::Aim &&
                leon->magazine() + leon->reserve() ==
                    beforeReload.magazine + beforeReload.reserve,
            "Reload returns to aim and conserves total ammunition");
    checkWeaponProfile();

    controls(jni, 0, 1, 1);
    ticks(jni, 2);
    const auto beforeInspector = actionSnapshot();
    inspector(jni);
    require(!walking && actionSnapshot() == beforeInspector,
            "Explicit inspector request freezes the current weapon action");
    ticks(jni, 50);
    require(actionSnapshot() == beforeInspector,
            "Inspector holding LT/RT advances neither gun nor ammunition");
    inspector(jni);
    require(walking && actionSnapshot() == beforeInspector,
            "Returning from inspector preserves exact action/gun pose and ammo");
    controls(jni);
    ticks(jni, 25);
    require(leon->action() == re4::PlayerAction::Footwork,
            "Neutral input returns the production player to footwork");

    // The real AEV door carries a partially loaded magazine. New trigger
    // holds during loading must not turn the destination arrival into a shot.
    turn180(jni);
    controls(jni, re4::Bottom);
    tick(jni);
    const auto ammoDoorToken = token(jni, "r300");
    const auto beforeAmmoDoor = actionSnapshot();
    controls(jni, re4::Bottom, 1, 1);
    Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
        &jni.env, nullptr, static_cast<jlong>(ammoDoorToken), string(argv[3]),
        string(""));
    draw(jni);
    require(activeRoomId == 0x300 &&
                leon->action() == re4::PlayerAction::Footwork &&
                leon->magazine() == beforeAmmoDoor.magazine &&
                leon->reserve() == beforeAmmoDoor.reserve &&
                leon->shots() == beforeAmmoDoor.shots &&
                leon->weapon()->motion() == re4::WeaponMotion::Idle &&
                leon->weapon()->frame() == 0,
            "Actual door preserves ammunition and resets transient gun/action");
    ticks(jni, 30);
    require(roomRequest.token == 0 &&
                leon->magazine() == beforeAmmoDoor.magazine &&
                leon->shots() == beforeAmmoDoor.shots,
            "Held A/LT/RT after room arrival causes no transfer or extra shot");
    controls(jni);
    ticks(jni, 2);
    controls(jni, 0, 1);
    ticks(jni, 18);
    savePhase(argv[6], "arrival-aim");
    controls(jni, re4::Left, 1);
    tick(jni);
    controls(jni, 0, 1);
    ticks(jni, 15);
    require(leon->action() == re4::PlayerAction::Reload,
            "Weapon rollback starts with an active original reload pose");
    controls(jni, re4::Start, 1);
    draw(jni);
    require(paused, "Weapon rollback fixture is paused");
    controls(jni, 0, 1);
    draw(jni);
    const auto beforeWeaponFailure = actionSnapshot();
    const auto weaponFaultRoom = previewSmd.get();
    const auto weaponFaultActor = leon.get();
    const auto weaponFaultFeet = walkProbe->position();
    auto weaponFaultView = re4::SaveSceneView();
    weaponFaultView.pitch = .6f;
    weaponFaultView.followDistance = 8500;
    weaponFaultView.animationFrames = 131;
    weaponFaultView.followCamera = {.4, -.1, .5};
    re4::RestoreSceneView(weaponFaultView);
    draw(jni);
    weaponFaultView = re4::SaveSceneView();
    Java_com_ast_re4decomp_NativeBridge_requestDoorTest(&jni.env, nullptr);
    const auto weaponFaultToken = token(jni);
    Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
        &jni.env, nullptr, static_cast<jlong>(weaponFaultToken),
        string(argv[4]), string(""));
    const auto faultsBeforeGun = failedUniformAllocations;
    failNthUniformAllocation = 2;
    draw(jni);
    require(failNthUniformAllocation == 0 &&
                failedUniformAllocations == faultsBeforeGun + 1,
            "Host fault reaches second actor allocation, the actual gun UBO");
    require(
        activeRoomId == 0x300 && previewSmd.get() == weaponFaultRoom &&
            leon.get() == weaponFaultActor &&
            walkProbe->position() == weaponFaultFeet && paused &&
            actorGpuReady && roomRequest.token == 0 &&
            actionSnapshot() == beforeWeaponFailure,
        "Gun GPU failure rolls back exact room, actor/action/gun/ammo/pause");
    const auto weaponRecoveredView = re4::SaveSceneView();
    require(weaponRecoveredView.yaw == weaponFaultView.yaw &&
                weaponRecoveredView.pitch == weaponFaultView.pitch &&
                weaponRecoveredView.distance == weaponFaultView.distance &&
                weaponRecoveredView.followDistance ==
                    weaponFaultView.followDistance &&
                weaponRecoveredView.animationFrames ==
                    weaponFaultView.animationFrames &&
                sameFollowState(weaponRecoveredView.followCamera,
                                weaponFaultView.followCamera) &&
                weaponRecoveredView.followHeading == weaponFaultView.followHeading &&
                sameVisualEffects(weaponRecoveredView, weaponFaultView),
            "Gun GPU rollback restores full camera and room animation clock");
    draw(jni);
    require(actionSnapshot() == beforeWeaponFailure,
            "Recovered GPU redraw retains paused weapon pose and ammunition");
    savePhase(argv[6], "weapon-rollback");
    Java_com_ast_re4decomp_NativeBridge_requestDoorTest(&jni.env, nullptr);
    const auto afterGunFaultToken = token(jni);
    require(afterGunFaultToken > weaponFaultToken,
            "Gun GPU failure permits a fresh room request");
    Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
        &jni.env, nullptr, static_cast<jlong>(afterGunFaultToken),
        string(argv[4]), string(""));
    draw(jni);
    require(
        activeRoomId == 0x301 && actorGpuReady && roomRequest.token == 0 &&
            leon->action() == re4::PlayerAction::Footwork &&
            leon->magazine() == beforeWeaponFailure.magazine &&
            leon->reserve() == beforeWeaponFailure.reserve &&
            leon->shots() == beforeWeaponFailure.shots,
        "A verified room can commit after gun failure without changing ammo");
    savePhase(argv[6], "final");
    if (argc == 10) {
      Java_com_ast_re4decomp_NativeBridge_configureEnemy(
          &jni.env, nullptr, string(argv[7]), string(argv[8]));
      const auto beforeGanadoAmmo = actionSnapshot();
      controls(jni, 0, 1, 1);
      Java_com_ast_re4decomp_NativeBridge_requestGanadoTest(&jni.env, nullptr);
      const auto ganadoToken = token(jni, "r400");
      const auto previousRoom = previewSmd.get();
      egl.release();
      Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
          &jni.env, nullptr, static_cast<jlong>(ganadoToken), string(argv[9]),
          string(""));
      require(activeRoomId == 0x301 && previewSmd.get() == previousRoom &&
                  !encounter && roomRequest.token == ganadoToken,
              "Ganado worker completion remains CPU-only without EGL");
      egl.current();
      const auto beforeGanadoGpu = uniformAllocations;
      draw(jni);
      require(activeRoomId == 0x400 && encounter && actorGpuReady &&
                  effectWorld && roomRequest.token == 0 &&
                  uniformAllocations == beforeGanadoGpu + 3 &&
                  encounter->hp() == 1000 && encounter->maxHp() == 1000 &&
                  encounter->actor().rig().jointMatrices().size() == 34,
              "Actual r400 commit owns Leon, gun and original EM12 GPU data");
      std::ifstream esl(argv[8], std::ios::binary);
      std::vector<std::uint8_t> eslBytes(re4::EnemySpawns::FileBytes);
      require(bool(esl.read(reinterpret_cast<char *>(eslBytes.data()), eslBytes.size())),
              "Actual bridge ESL fixture readable");
      const re4::EnemySpawns spawns(eslBytes);
      const auto *originalEnemy = spawns.find(re4::EnemyEncounter::TestSpawn);
      require(previewObjects && previewObjects->rendered == 33 &&
                  previewObjects->unsupported == 0 && previewObjects->doors == 6 &&
                  previewObjects->windows == 5 && previewObjects->ladders == 4 &&
                  previewObjects->containers == 18 &&
                  previewObjects->mesh.triangles == 6234 &&
                  previewObjects->mesh.texturedTriangles == 6234,
              "Production r400 merges all audited ETS/ETM doors, windows, ladders and containers");
      require(originalEnemy && originalEnemy->id == 0x12 && originalEnemy->type == 0 &&
                  originalEnemy->hp == 1000 && originalEnemy->listIndex == 0x25,
              "Ganado test uses audited original R100 ESL actor");
      const auto playerStart = re4::EnemyEncounter::TerrainPlayerPosition();
      for (unsigned axis : {0u, 2u}) {
        require(std::abs(walkProbe->position()[axis] - playerStart[axis]) < .01,
                "Ganado test uses explicit r400 player placement");
        require(std::abs(encounter->feet()[axis] - re4::EnemyEncounter::TerrainEnemyPlacement().position[axis]) < .01,
                "Ganado test uses explicit r400 enemy placement");
      }
      require(std::abs(walkProbe->position()[1] - playerStart[1]) <= 250 &&
                  std::abs(encounter->feet()[1] - re4::EnemyEncounter::TerrainEnemyPlacement().position[1]) <= 250 &&
                  std::abs(std::remainder(encounter->heading() - re4::EnemyEncounter::TerrainEnemyPlacement().heading,
                                           6.28318530717958647692)) < 1e-6,
              "Explicit test heading and original r400 SAT-grounded heights verified");
      const auto initialEnemy = enemySnapshot();
      enemyDiagnostic("arrival");
      savePhase(argv[6], "ganado-arrival");
      ticks(jni, 25);
      require(leon->magazine() == beforeGanadoAmmo.magazine &&
                  leon->reserve() == beforeGanadoAmmo.reserve &&
                  leon->shots() == beforeGanadoAmmo.shots && encounter->hp() == 1000,
              "Held LT/RT during Ganado arrival cannot fire until physical neutral");
      controls(jni);
      ticks(jni, 2);
      controls(jni, 0, 1);
      ticks(jni, 18);
      require(leon->action() == re4::PlayerAction::Aim,
              "Actual LT draws original weapon in Ganado encounter");
      enemyDiagnostic("aim-initial");
      savePhase(argv[6], "ganado-aim-initial");
      shootEnemy(jni, argv[6]);
      enemyDiagnostic("hit");
      savePhase(argv[6], "ganado-hit");

      controls(jni, re4::Start, 1, 1);
      draw(jni);
      require(paused, "Start pauses actual Ganado encounter");
      controls(jni, 0, 1, 1);
      const auto pausedEnemy = enemySnapshot();
      const auto pausedEnemyAction = actionSnapshot();
      std::vector<std::uint8_t> withEnemy(960 * 540 * 4), withoutEnemy(withEnemy.size());
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, withEnemy.data());
      // Host-only visibility diagnostic: no simulation tick occurs while
      // paused, and the exact encounter object is restored immediately.
      auto hiddenEnemy = std::move(encounter);
      draw(jni);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, withoutEnemy.data());
      savePhase(argv[6], "ganado-hit-without-enemy");
      encounter = std::move(hiddenEnemy);
      draw(jni);
      unsigned enemyPixels = 0;
      for (unsigned pixel = 0; pixel < 960 * 540; ++pixel)
        enemyPixels += !std::equal(withEnemy.begin() + pixel * 4,
                                 withEnemy.begin() + pixel * 4 + 3,
                                 withoutEnemy.begin() + pixel * 4);
      std::cerr << "Ganado paused visible framebuffer pixels=" << enemyPixels << '\n';
      ticks(jni, 8);
      require(enemySnapshot() == pausedEnemy && actionSnapshot() == pausedEnemyAction,
              "Pause freezes enemy HP, RNG, placement and exact player/enemy poses");
      const auto contextEnemy = encounter.get();
      const auto enemyCamera = re4::SaveSceneView();
      egl.fresh();
      const auto contextAllocations = uniformAllocations;
      create(202);
      Java_com_ast_re4decomp_NativeBridge_configureEnemy(
          &jni.env, nullptr, string(argv[7]), string(argv[8]));
      require(encounter.get() == contextEnemy && actorGpuReady && paused &&
                  uniformAllocations == contextAllocations + 3 &&
                  enemySnapshot() == pausedEnemy && actionSnapshot() == pausedEnemyAction,
              "Context recreation restores three GPUs and retains enemy HP/pose/RNG");
      draw(jni);
      require(enemySnapshot() == pausedEnemy && actionSnapshot() == pausedEnemyAction &&
                  re4::SaveSceneView().yaw == enemyCamera.yaw &&
                  re4::SaveSceneView().pitch == enemyCamera.pitch &&
                  sameFollowState(re4::SceneFollowState(), enemyCamera.followCamera) &&
                  re4::SaveSceneView().followHeading == enemyCamera.followHeading &&
                  sameVisualEffects(re4::SaveSceneView(), enemyCamera),
              "First restored draw leaves paused enemy/action and camera unchanged");
      knifeCameraMapping(jni, argv[1], argv[2], argv[7]);
      controls(jni, re4::Start, 1, 1);
      draw(jni);
      controls(jni, 0, 1, 1);
      ticks(jni, 35);
      require(!paused && encounter->hp() == pausedEnemy.hp &&
                  leon->shots() == pausedEnemyAction.shots,
              "Pause/context resume requires neutral before held RT spends another round");
      const auto inspectedEnemy = enemySnapshot();
      const auto inspectedAction = actionSnapshot();
      inspector(jni);
      ticks(jni, 20);
      require(!walking && enemySnapshot() == inspectedEnemy &&
                  actionSnapshot() == inspectedAction,
              "Inspector freezes original enemy HP/RNG and both actor poses");
      inspector(jni);
      require(walking && enemySnapshot() == inspectedEnemy &&
                  actionSnapshot() == inspectedAction,
              "Returning from inspector preserves exact Ganado/action state");
      ticks(jni, 25);
      require(encounter->hp() == inspectedEnemy.hp &&
                  leon->shots() == inspectedAction.shots,
              "Inspector return also gates the physically held fire trigger");
      controls(jni);
      ticks(jni, 2);
      controls(jni, 0, 1);
      ticks(jni, 18);
      shootEnemy(jni);
      pause(jni);
      const auto rollbackEnemy = enemySnapshot();
      const auto rollbackAction = actionSnapshot();
      const auto rollbackEnemyOwner = encounter.get();
      const auto rollbackRoomOwner = previewSmd.get();
      const auto rollbackPlayerOwner = leon.get();
      const auto rollbackCamera = re4::SaveSceneView();
      const auto missingEnemy = std::string(argv[7]) + ".missing";
      Java_com_ast_re4decomp_NativeBridge_configureEnemy(
          &jni.env, nullptr, string(missingEnemy), string(argv[8]));
      Java_com_ast_re4decomp_NativeBridge_requestGanadoTest(&jni.env, nullptr);
      const auto enemyCpuToken = token(jni, "r400");
      Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
          &jni.env, nullptr, static_cast<jlong>(enemyCpuToken), string(argv[9]),
          string(""));
      draw(jni);
      require(activeRoomId == 0x400 && paused && roomRequest.token == 0 &&
                  encounter.get() == rollbackEnemyOwner &&
                  previewSmd.get() == rollbackRoomOwner && leon.get() == rollbackPlayerOwner &&
                  actorGpuReady && enemySnapshot() == rollbackEnemy &&
                  actionSnapshot() == rollbackAction,
              "Missing enemy CPU asset rolls back exact live HP/pose/RNG/ammunition");
      Java_com_ast_re4decomp_NativeBridge_configureEnemy(
          &jni.env, nullptr, string(argv[7]), string(argv[8]));
      Java_com_ast_re4decomp_NativeBridge_requestGanadoTest(&jni.env, nullptr);
      const auto enemyGpuToken = token(jni, "r400");
      Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
          &jni.env, nullptr, static_cast<jlong>(enemyGpuToken), string(argv[9]),
          string(""));
      const auto beforeEnemyFaults = failedUniformAllocations;
      failNthUniformAllocation = 3;
      draw(jni);
      require(failNthUniformAllocation == 0 &&
                  failedUniformAllocations == beforeEnemyFaults + 1,
              "Host-only fault reaches actual third actor allocation, EM12 UBO");
      require(activeRoomId == 0x400 && paused && roomRequest.token == 0 &&
                  encounter.get() == rollbackEnemyOwner &&
                  previewSmd.get() == rollbackRoomOwner && leon.get() == rollbackPlayerOwner &&
                  actorGpuReady && enemySnapshot() == rollbackEnemy &&
                  actionSnapshot() == rollbackAction &&
                  re4::SaveSceneView().yaw == rollbackCamera.yaw &&
                  re4::SaveSceneView().pitch == rollbackCamera.pitch &&
                  re4::SaveSceneView().animationFrames == rollbackCamera.animationFrames &&
                  sameFollowState(re4::SceneFollowState(), rollbackCamera.followCamera) &&
                  re4::SaveSceneView().followHeading == rollbackCamera.followHeading &&
                  sameVisualEffects(re4::SaveSceneView(), rollbackCamera),
              "Third GPU failure restores enemy HP/RNG/pose, player/action/ammo and camera");
      draw(jni);
      require(enemySnapshot() == rollbackEnemy && actionSnapshot() == rollbackAction,
              "Recovered three GPUs redraw the exact paused encounter");
      savePhase(argv[6], "ganado-rollback");
      controls(jni, re4::Start, 1, 1);
      draw(jni);
      controls(jni, 0, 1, 1);
      ticks(jni, 12);
      require(!paused && encounter.get() == rollbackEnemyOwner && actorGpuReady &&
                  encounter->hp() == rollbackEnemy.hp &&
                  encounter->randomState() == rollbackEnemy.random &&
                  leon->shots() == rollbackAction.shots &&
                  !(enemySnapshot() == rollbackEnemy),
              "Rolled-back enemy resumes animation while held RT remains gated");
      pause(jni);
      Java_com_ast_re4decomp_NativeBridge_requestGanadoTest(&jni.env, nullptr);
      const auto restartedEnemyToken = token(jni, "r400");
      Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
          &jni.env, nullptr, static_cast<jlong>(restartedEnemyToken), string(argv[9]),
          string(""));
      draw(jni);
      require(activeRoomId == 0x400 && encounter.get() != rollbackEnemyOwner &&
                  encounter->hp() == 1000 && encounter->randomState() == initialEnemy.random &&
                  actorGpuReady && !paused && roomRequest.token == 0 &&
                  leon->magazine() == rollbackAction.magazine &&
                  leon->reserve() == rollbackAction.reserve && leon->shots() == rollbackAction.shots,
              "Verified Ganado restart succeeds after CPU/third-GPU faults and preserves ammo");
      pause(jni);
      const auto beforeRecoveryEnemy = enemySnapshot();
      const auto beforeRecoveryAction = actionSnapshot();
      Java_com_ast_re4decomp_NativeBridge_configureEnemy(
          &jni.env, nullptr, string(missingEnemy), string(argv[8]));
      Java_com_ast_re4decomp_NativeBridge_requestGanadoTest(&jni.env, nullptr);
      const auto failedAgainToken = token(jni, "r400");
      Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
          &jni.env, nullptr, static_cast<jlong>(failedAgainToken), string(argv[9]),
          string(""));
      draw(jni);
      require(enemySnapshot() == beforeRecoveryEnemy &&
                  actionSnapshot() == beforeRecoveryAction && roomRequest.token == 0,
              "Repeated missing enemy request remains recoverable");
      Java_com_ast_re4decomp_NativeBridge_requestDoorTest(&jni.env, nullptr);
      const auto leaveGanadoToken = token(jni);
      Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
          &jni.env, nullptr, static_cast<jlong>(leaveGanadoToken), string(argv[4]),
          string(""));
      const auto beforeRecoveryGpu = uniformAllocations;
      draw(jni);
      require(activeRoomId == 0x301 && !encounter && actorGpuReady &&
                  roomRequest.token == 0 && uniformAllocations == beforeRecoveryGpu + 2 &&
                  leon->magazine() == beforeRecoveryAction.magazine &&
                  leon->reserve() == beforeRecoveryAction.reserve &&
                  leon->shots() == beforeRecoveryAction.shots,
              "Normal DoorTest recovers from broken enemy asset and preserves ammunition");
      savePhase(argv[6], "ganado-door-recovery");
      ambientChecks(jni, egl, argv, create);
      objectActionChecks(jni, egl, argv, create);
    }
    re4::DestroySceneGl();
    std::cout << "Production JNI/GLES bridge: " << checks
              << " checks PASS; real A doors, completion without EGL, CPU/GPU "
                 "rollback, stale tokens, context recreation, LT/RT/X gun "
                 "actions, pause/inspector/arrival neutral gates and gun GPU "
                 "rollback"
              << (argc == 10 ? "; actual EM12 HP/hits/RNG, ETS/ETM objects, visible EM28 chickens, source container/pickup/ammo/herb flow, five-GPU/context/CPU rollback and DoorTest recovery" : "")
              << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Production JNI/GLES bridge FAIL: " << e.what() << '\n';
    return 1;
  }
}
