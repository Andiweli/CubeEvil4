// Original r101 ladder through actual JNI A input, pause and EGL recreation.
#define main previous_bridge_test_main
#include "test_android_bridge_gl.cpp"
#undef main
int main(int argc, char **argv) {
  using namespace bridge_test;
  try {
    if (argc != 8) throw std::runtime_error("Usage: m28_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00");
    Egl egl;
    Jni jni;
    Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env, nullptr, string(argv[5]));
    Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env, nullptr, string(argv[6]), string(argv[7]));
    auto create = [&] {
      Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env, nullptr,
          string(argv[1]), string(argv[2]), string(argv[3]), string(argv[4]), string("r101"), 2801);
      frozenPixels(jni);
      require(javaException.empty() && actorGpuReady && roomLadders, "Production r101 ladder owner ready");
    };
    create();
    const auto &source = roomFlow->ladders().front();
    constexpr double pi = 3.14159265358979323846;
    auto feet = source.position;
    feet[0] += std::sin(source.heading) * 600;
    feet[2] += std::cos(source.heading) * 600;
    require(walkProbe->place(feet), "Actual input ladder approach is capsule-valid");
    leon->setPlacement(walkProbe->position(), float(source.heading + pi));
    controls(jni); frozenPixels(jni);
    controls(jni, re4::Bottom); tick(jni);
    require(leon->ladderActive(), "Native A edge with SAT-clipped front starts the ladder");
    controls(jni); ticks(jni, 50);
    pause(jni);
    const auto savedFeet = leon->feet();
    const auto savedTicks = leon->ladderState().ticks;
    const auto savedBones = leon->rig().bones();
    const auto savedView = re4::SceneFollowState();
    frozenPixels(jni);
    require(leon->feet() == savedFeet && leon->rig().bones() == savedBones,
            "Paused ladder frame does not move");
    egl.release(); egl.fresh(); create();
    require(paused && leon->ladderActive() && leon->ladderState().ticks == savedTicks &&
            leon->feet() == savedFeet && leon->rig().bones() == savedBones &&
            sameFollowState(re4::SceneFollowState(), savedView),
            "EGL recreation preserves ladder animation, position, camera and pause");
    controls(jni, re4::Start); frozenPixels(jni);
    controls(jni); frozenPixels(jni);
    unsigned remaining = 0;
    while (leon->ladderActive() && remaining++ < 400) tick(jni);
    require(!leon->ladderActive() && !leon->ladderLandingFailed() && leon->feet()[1] > 8500,
            "Production update resumes and lands on the tower floor");
    leon->setPlacement(walkProbe->position(), float(source.heading + 2 * pi));
    controls(jni); frozenPixels(jni);
    controls(jni, re4::Bottom); tick(jni);
    require(leon->action() == re4::PlayerAction::LadderDown, "Native A starts checked return");
    controls(jni); remaining = 0;
    while (leon->ladderActive() && remaining++ < 400) tick(jni);
    require(!leon->ladderActive() && !leon->ladderLandingFailed() && std::abs(leon->feet()[1]) < 100,
            "Production return lands on genuine ground");
    std::cout << "M28 production ladder JNI/GLES PASS: " << checks << " checks\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n'; return 1;
  }
}
