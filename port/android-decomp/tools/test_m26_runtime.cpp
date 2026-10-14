// Actual production JNI, GLES and PCM lifecycle. Original disc assets are
// developer-supplied; this executable adds no testing hooks to the Android app.
#define main previous_bridge_test_main
#include "test_android_bridge_gl.cpp"
#undef main

namespace m26_test {
using namespace bridge_test;
unsigned arrayReads{}, arrayGets{}, arrayReleases{}, drawCalls{};
struct ShortArray {
  std::vector<jshort> storage;
  bool unavailable{};
  explicit ShortArray(std::size_t size) : storage(size + 2, jshort(0x5a5a)) {
    std::fill(storage.begin() + 1, storage.end() - 1, jshort(0x1234));
  }
  jshortArray handle() { return reinterpret_cast<jshortArray>(this); }
  bool intact() const {
    return storage.front() == 0x5a5a && storage.back() == 0x5a5a;
  }
  bool silent() const {
    return std::all_of(storage.begin() + 1, storage.end() - 1,
                       [](jshort x) { return x == 0; });
  }
};
jsize JNICALL arrayLength(JNIEnv *, jarray array) {
  ++arrayReads;
  return jsize(reinterpret_cast<ShortArray *>(array)->storage.size() - 2);
}
jshort *JNICALL shortElements(JNIEnv *, jshortArray array, jboolean *copy) {
  ++arrayGets;
  if (copy) *copy = JNI_FALSE;
  auto &value = *reinterpret_cast<ShortArray *>(array);
  return value.unavailable ? nullptr : value.storage.data() + 1;
}
void JNICALL releaseShorts(JNIEnv *, jshortArray array, jshort *data, jint mode) {
  ++arrayReleases;
  require(data == reinterpret_cast<ShortArray *>(array)->storage.data() + 1 &&
              mode == 0,
          "Audio JNI releases the acquired array with committed PCM");
}
struct RuntimeJni : Jni {
  RuntimeJni() {
    table.GetArrayLength = arrayLength;
    table.GetShortArrayElements = shortElements;
    table.ReleaseShortArrayElements = releaseShorts;
  }
};
bool renderPcm(RuntimeJni &jni, bool expectSound) {
  ShortArray pcm(2048);
  const auto releases = arrayReleases;
  require(Java_com_ast_re4decomp_NativeBridge_renderAudio(
              &jni.env, nullptr, pcm.handle()) == 1024 &&
              pcm.intact() && arrayReleases == releases + 1,
          "Actual renderAudio writes exactly one bounded stereo PCM buffer");
  require(expectSound ? !pcm.silent() : pcm.silent(),
          expectSound ? "Original cue produces nonzero JNI PCM"
                      : "Paused native mixer writes zero PCM");
  return !pcm.silent();
}
void audioBounds(RuntimeJni &jni) {
  const auto gets = arrayGets, releases = arrayReleases, reads = arrayReads;
  require(Java_com_ast_re4decomp_NativeBridge_renderAudio(
              &jni.env, nullptr, nullptr) == 0 && arrayReads == reads,
          "Null PCM array is rejected before JNI array access");
  for (std::size_t length : {0u, 1u, 3u, 32769u, 32770u}) {
    ShortArray array(length);
    require(Java_com_ast_re4decomp_NativeBridge_renderAudio(
                &jni.env, nullptr, array.handle()) == 0 && array.intact(),
            "Empty, odd or oversized PCM buffer is rejected safely");
  }
  require(arrayGets == gets && arrayReleases == releases,
          "Rejected array bounds never acquire or release sample storage");
  ShortArray unavailable(16);
  unavailable.unavailable = true;
  require(Java_com_ast_re4decomp_NativeBridge_renderAudio(
              &jni.env, nullptr, unavailable.handle()) == 0 &&
              arrayGets == gets + 1 && arrayReleases == releases,
          "JNI element allocation failure returns without touching PCM");
  ShortArray maximum(32768);
  require(Java_com_ast_re4decomp_NativeBridge_renderAudio(
              &jni.env, nullptr, maximum.handle()) == 16384 && maximum.intact(),
          "Largest accepted JNI stereo buffer preserves both guard samples");
}
bool sameDestruction(const re4::DestructionEffectsState &a,
                     const re4::DestructionEffectsState &b) {
  if (std::tie(a.signature, a.frames, a.serial, a.random) !=
          std::tie(b.signature, b.frames, b.serial, b.random) ||
      a.events.size() != b.events.size() ||
      a.emitters.size() != b.emitters.size() || a.models.size() != b.models.size())
    return false;
  for (std::size_t i = 0; i < a.events.size(); ++i)
    if (std::tie(a.events[i].slot, a.events[i].serial) !=
        std::tie(b.events[i].slot, b.events[i].serial)) return false;
  for (std::size_t i = 0; i < a.emitters.size(); ++i) {
    const auto &x = a.emitters[i], &y = b.emitters[i];
    if (std::tie(x.record, x.slot, x.age, x.next, x.seed, x.placement) !=
        std::tie(y.record, y.slot, y.age, y.next, y.seed, y.placement)) return false;
  }
  for (std::size_t i = 0; i < a.models.size(); ++i) {
    const auto &x = a.models[i], &y = b.models[i];
    if (std::tie(x.record, x.slot, x.age, x.serial, x.seed, x.position, x.rotation,
                 x.angularSpeed, x.burstCenter, x.color, x.sizeX, x.sizeY,
                 x.sizeMultiplier, x.sizeStep, x.matrix) !=
        std::tie(y.record, y.slot, y.age, y.serial, y.seed, y.position, y.rotation,
                 y.angularSpeed, y.burstCenter, y.color, y.sizeX, y.sizeY,
                 y.sizeMultiplier, y.sizeStep, y.matrix) ||
        x.parts.size() != y.parts.size()) return false;
    for (std::size_t k = 0; k < x.parts.size(); ++k) {
      const auto &p = x.parts[k], &q = y.parts[k];
      if (std::tie(p.position, p.world, p.rotation, p.speed, p.angularSpeed, p.flag) !=
          std::tie(q.position, q.world, q.rotation, q.speed, q.angularSpeed, q.flag))
        return false;
    }
  }
  return true;
}
bool sameWorld(const std::shared_ptr<const re4::WorldEffectsState> &a,
               const std::shared_ptr<const re4::WorldEffectsState> &b) {
  if (!a || !b) return !a && !b;
  if (!sameEffectsState(a, b) || a->acceptedEvents != b->acceptedEvents ||
      a->eventRandom != b->eventRandom || a->streams.size() != b->streams.size() ||
      a->instances.size() != b->instances.size()) return false;
  auto same = [](const re4::EnvironmentState &x, const re4::EnvironmentState &y) {
    return sameEffectsState(std::make_shared<re4::EnvironmentState>(x),
                            std::make_shared<re4::EnvironmentState>(y));
  };
  for (std::size_t i = 0; i < a->streams.size(); ++i)
    if (!same(a->streams[i], b->streams[i])) return false;
  for (std::size_t i = 0; i < a->instances.size(); ++i) {
    const auto &x = a->instances[i], &y = b->instances[i];
    if (std::tie(x.bank, x.eventSerial, x.startFrame, x.position, x.rotation) !=
        std::tie(y.bank, y.eventSerial, y.startFrame, y.position, y.rotation) ||
        !same(x.state, y.state)) return false;
  }
  return true;
}
void finish(RuntimeJni &jni, const std::string &destination,
            const std::string &path) {
  const auto request = token(jni, destination);
  Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
      &jni.env, nullptr, jlong(request), string(path), string(""));
  walkFrame = {};
  draw(jni);
  require(roomRequest.token == 0 && actorGpuReady,
          "Production GL draw commits the completed request");
}
std::size_t findShootable(bool explosive, bool random) {
  require(bool(objectActions), "Live source room owns its object action runtime");
  for (std::size_t i = 0; i < objectActions->objects().size(); ++i) {
    const auto &object = objectActions->objects()[i];
    if (object.explosive != explosive || objectActions->states()[i].broken ||
        (random && object.item.sourceId != 0x1001 && object.item.sourceId != 0x1002))
      continue;
    try { (void)containerShot(i, 1000); return i; }
    catch (const std::runtime_error &) {}
  }
  throw std::runtime_error("No source EAT-clear requested container ray");
}
void shootObject(std::size_t index, unsigned serial) {
  const auto shot = containerShot(index, serial);
  const auto hit = objectActions->target(shot.from, shot.to);
  require(hit && hit->index == index,
          "Actual authored shot targets its original ETS collision shape");
  if (encounter) encounter->tick(leon->feet(), &shot);
  else objectActions->shoot(shot);
  require(objectActions->states()[index].broken,
          "Production shot dispatch breaks the intended original container");
  afterWorldTick(&shot);
  const auto events = destruction->state().events.size();
  afterWorldTick(nullptr);
  require(destruction->state().events.size() == events,
          "Drained break event cannot respawn fragment ownership on another callback");
}
bool lookAtContainer(std::size_t index) {
  const auto &object = objectActions->objects()[index];
  constexpr double pi = 3.14159265358979323846;
  for (double distance : {7500., 6500., 8500., 5000., 3500.})
    for (unsigned side = 0; side < 36; ++side) {
      const auto angle = side * pi / 18;
      re4::CollisionVec feet{object.placement.position[0] + std::sin(angle) * distance,
                             object.placement.position[1],
                             object.placement.position[2] + std::cos(angle) * distance};
      if (!walkProbe->place(feet)) continue;
      feet = walkProbe->position();
      const auto heading = float(std::atan2(object.placement.position[0] - feet[0],
                                            object.placement.position[2] - feet[2]));
      const auto pose = re4::PoseFollowCamera({}, collisionWorld.get(), feet, heading);
      const re4::CollisionVec target{object.placement.position[0],
                                     object.placement.position[1] + 700,
                                     object.placement.position[2]};
      if (collisionWorld->line(pose.eye, target, re4::SatSubset::All, 0)) continue;
      leon->setPlacement(feet, heading);
      re4::SceneFollowHeading(heading);
      return true;
    }
  return false;
}
void unpause(RuntimeJni &jni) {
  if (paused) {
    controls(jni, re4::Start);
    frozenPixels(jni);
    controls(jni);
    frozenPixels(jni);
  }
  require(!paused, "Actual Start action resumes runtime");
}
} // namespace m26_test

extern "C" void __real_glDrawArrays(GLenum, GLint, GLsizei);
extern "C" void __wrap_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  ++m26_test::drawCalls;
  __real_glDrawArrays(mode, first, count);
}

int main(int argc, char **argv) {
  using namespace m26_test;
  try {
    if (argc != 11)
      throw std::runtime_error("Usage: m26_runtime_check PL00 WEP01 R300 R301 "
                               "roominfo OUTPUT EM12 emleon00 R400 R101");
    Egl egl;
    RuntimeJni jni;
    const auto player = std::string(argv[1]);
    const auto core = player.substr(0, player.find_last_of('/')) + "/../etc/core.das";
    Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env, nullptr, string(core));
    Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env, nullptr,
                                                     string(argv[7]), string(argv[8]));
    auto create = [&](jlong session) {
      Java_com_ast_re4decomp_NativeBridge_createScene(
          &jni.env, nullptr, string(argv[3]), string(argv[1]), string(argv[2]),
          string(argv[5]), string("r300"), session);
      require(javaException.empty(), "Production session creation succeeds");
    };
    create(2601);
    draw(jni);
    Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env, nullptr, JNI_TRUE);
    require(activeRoomId == 0x300 && leon && roomInfo &&
                soundMixer.hasCue(2, 0) && soundMixer.hasCue(2, 2),
            "Initial session loads original player/weapon sound banks");
    Java_com_ast_re4decomp_NativeBridge_requestVillageTest(&jni.env, nullptr);
    const auto villageToken = token(jni, "r101");
    auto *const previousPlayer = leon.get();
    egl.release();
    Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
        &jni.env, nullptr, jlong(villageToken), string(argv[10]), string(""));
    require(activeRoomId == 0x300 && leon.get() == previousPlayer,
            "Worker village completion cannot mutate live GL room");
    egl.current();
    walkFrame = {};
    draw(jni);
    const auto *start = roomInfo->find(0x101, 1);
    require(start && activeRoomId == 0x101 && walking && actorGpuReady &&
                roomRequest.token == 0 && leon->hasMeleeMotions() &&
                std::abs(walkProbe->position()[0] - start->position[0]) < .01 &&
                std::abs(walkProbe->position()[2] - start->position[2]) < .01 &&
                std::abs(leon->heading() - start->heading) < 1e-5,
            "Actual village request commits original r101 point 1 and player profile");
    require(previewWorldEffects && previewWorldEffects->originalPyre &&
                previewObjects && objectActions && previewItems && destruction &&
                destructionData && !destructionData->meshes.empty() &&
                soundMixer.hasCue(6, 0x58) && pyreVoice,
            "r101 owns original pyre, object/drop/fragment geometry and fire audio");
    savePhase(argv[6], "r101-start");
    const auto randomBox = findShootable(false, true);
    const auto beforeRandom = objectActions->snapshot().randomState;
    shootObject(randomBox, 1001);
    require(objectActions->states()[randomBox].itemResolved &&
                objectActions->snapshot().randomState != beforeRandom &&
                !destruction->state().events.empty(),
            "Source random ITA box resolves its drop once and routes authored fragments");
    const auto &drop = objectActions->states()[randomBox];
    require(drop.itemCount
                ? drop.itemActive && std::any_of(
                      previewItems->placements.begin(), previewItems->placements.end(),
                      [&](const auto &p) { return p.index == randomBox && p.id == drop.itemId; })
                : !drop.itemActive,
            "Resolved random drop selects its preloaded original ITM model or no-drop branch");
    renderPcm(jni, true);
    const auto villageSnapshot = objectActions->snapshot();
    Java_com_ast_re4decomp_NativeBridge_requestGanadoTest(&jni.env, nullptr);
    finish(jni, "r400", argv[9]);
    require(activeRoomId == 0x400 && encounter && objectActions && destruction &&
                soundMixer.hasCue(6, 0x56) &&
                objectRoomStates.at(0x101).checksum == villageSnapshot.checksum,
            "Combat room commit retains source village drop cache and installs explosion audio");
    const auto red = findShootable(true, false);
    require(objectActions->objects()[red].placement.id == 0x2d &&
                lookAtContainer(red),
            "Red ET2D barrel has an original SAT-valid visible approach");
    const auto frameBefore = destruction->state().frames;
    const auto eventBefore = objectActions->snapshot().eventSerial;
    const auto voicesBefore = soundMixer.voiceCount();
    shootObject(red, 2001);
    require(objectActions->states()[red].bombWait == 2 &&
                destruction->state().frames == frameBefore &&
                soundMixer.voiceCount() > voicesBefore &&
                !re4::SaveWorldEffectsState()->acceptedEvents.empty(),
            "ET2D after-tick callback routes source fragments, explosion sprites and sound once");
    controls(jni);
    leon->suspendClock();
    tick(jni);
    require(objectActions->states()[red].bombWait == 1,
            "Original barrel blast waits through the first subsequent world tick");
    tick(jni);
    require(objectActions->states()[red].bombWait == 0 &&
                objectActions->snapshot().eventSerial >= eventBefore + 2,
            "Original delayed barrel blast becomes due on the second world tick");
    ticks(jni, 8);
    require(!destruction->instances().empty(),
            "Original Efm05 burst produces genuine posed fragment mesh instances");
    const auto instances = destruction->instances();
    require(std::any_of(instances.begin(), instances.end(), [&](const auto &instance) {
              return instance.slot == objectActions->objects()[red].placement.slot &&
                     destructionData->meshes.at(instance.mesh).efm == 0x44;
            }),
            "Shot ET2D slot emits its own original metal Efm44 fragments");
    renderPcm(jni, true);
    pause(jni);
    const auto pausedEffects = destruction->state();
    const auto pausedObjects = objectActions->snapshot();
    const auto pausedWorld = re4::SaveWorldEffectsState();
    const auto pausedVoices = soundMixer.voiceCount();
    const auto actorState = actionSnapshot();
    const auto withStart = drawCalls;
    const auto withFragments = frozenPixels(jni);
    const auto withCalls = drawCalls - withStart;
    auto parkedFragments = std::move(destruction);
    const auto withoutStart = drawCalls;
    const auto withoutFragments = frozenPixels(jni);
    const auto withoutCalls = drawCalls - withoutStart;
    destruction = std::move(parkedFragments);
    require(withCalls > withoutCalls &&
                pixelChanges(withFragments, withoutFragments) > 20,
            "Production SceneGL submits visible original dynamic fragment instances");
    std::cout << "Original dynamic fragments: " << withCalls - withoutCalls
              << " production draws, " << pixelChanges(withFragments, withoutFragments)
              << " framebuffer pixels\n";
    frozenPixels(jni);
    savePhase(argv[6], "r400-fragments");
    renderPcm(jni, false);
    audioBounds(jni);
    for (unsigned i = 0; i < 4; ++i) draw(jni);
    require(sameDestruction(pausedEffects, destruction->state()) &&
                sameWorld(pausedWorld, re4::SaveWorldEffectsState()) &&
                objectActions->snapshot().checksum == pausedObjects.checksum &&
                soundMixer.voiceCount() == pausedVoices && actorState == actionSnapshot(),
            "Pause freezes source fragment/world/drop/player state and audio voices");
    auto *const fragmentOwner = destruction.get();
    auto *const objectOwner = objectActions.get();
    auto *const enemyOwner = encounter.get();
    egl.fresh();
    create(2601);
    draw(jni);
    require(activeRoomId == 0x400 && paused && destruction.get() == fragmentOwner &&
                objectActions.get() == objectOwner && encounter.get() == enemyOwner &&
                sameDestruction(pausedEffects, destruction->state()) &&
                sameWorld(pausedWorld, re4::SaveWorldEffectsState()) &&
                soundMixer.voiceCount() == pausedVoices,
            "Same-session GL recreation preserves original CPU owners, effects and audio");
    Java_com_ast_re4decomp_NativeBridge_requestVillageTest(&jni.env, nullptr);
    failNextUniformAllocation = true;
    const auto failedBefore = failedUniformAllocations;
    finish(jni, "r101", argv[10]);
    require(failedUniformAllocations == failedBefore + 1 && activeRoomId == 0x400 &&
                paused && destruction.get() == fragmentOwner &&
                objectActions.get() == objectOwner && encounter.get() == enemyOwner &&
                sameDestruction(pausedEffects, destruction->state()) &&
                objectActions->snapshot().checksum == pausedObjects.checksum &&
                sameWorld(pausedWorld, re4::SaveWorldEffectsState()) &&
                soundMixer.voiceCount() == pausedVoices && soundMixer.hasCue(6, 0x56),
            "Failed destination GPU upload restores source ownership/effects/audio bank");
    renderPcm(jni, false);
    unpause(jni);
    require(soundMixer.play(6, 0x56) != 0,
            "Rollback retains the playable original prior-room explosion cue");
    renderPcm(jni, true);
    pause(jni);
    egl.fresh();
    create(2602);
    draw(jni);
    require(activeRoomId == 0x300 && !paused && objectRoomStates.empty() &&
                !objectActions && !destruction && soundMixer.hasCue(2, 0) &&
                soundMixer.hasCue(2, 2) && soundMixer.voiceCount() == 0,
            "Fresh session clears prior owners/voices and reloads static original banks");
    const re4::PlayerShot freshShot{1, {}, {0, 0, 1000}};
    afterWorldTick(&freshShot);
    renderPcm(jni, true);
    const auto activeVoices = soundMixer.voiceCount();
    Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env, nullptr, JNI_FALSE);
    renderPcm(jni, false);
    require(soundMixer.voiceCount() == activeVoices,
            "Actual UI audioActive false freezes existing cue voices");
    Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env, nullptr, JNI_TRUE);
    renderPcm(jni, true);
    std::cout << "M26 production JNI/GLES/PCM runtime PASS: " << checks
              << " checks; original r101 point1/pyre/random drop, actual ET2D "
                 "two-tick blast/fragments/sound, paused/context/rollback owners, "
                 "fresh-session banks and bounded JNI PCM\n";
    re4::DestroySceneGl();
  } catch (const std::exception &e) {
    std::cerr << "M26 production runtime FAIL: " << e.what() << '\n';
    return 1;
  }
}
