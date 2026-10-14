// Actual JNI/render loop: carry a weapon from r100, close r101 radio, keep playing.
#define RE4_K16_ENTRY previous_k16_fix1_main
#include "test_k16_runtime.cpp"
#undef RE4_K16_ENTRY

int main(int argc, char **argv) {
  using namespace save_test;
  try {
    if (argc != 2) throw std::runtime_error("Usage: k17_fix1_runtime_check FIXTURE_DISC_ROOT");
    const auto root = std::filesystem::path(argv[1]), files = root / "files";
    const auto js = [&](const std::filesystem::path &p) { return string(p.string().c_str()); };
    Egl egl;
    SaveJni jni;
    constexpr jlong session = 17501;
    Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env, nullptr, js(files / "etc/core.das"));
    Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env, nullptr, js(files / "em/em12.drs"), js(files / "etc/emleon00.esl"));
    Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env, nullptr, js(files / "em/wep02.drs"), js(files / "em/wep07.drs"));
    Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env, nullptr, JNI_TRUE, session);
    Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env, nullptr, JNI_TRUE, 0, session);
    Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(&jni.env, nullptr, JNI_TRUE);
    Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env, nullptr, js(root / "decoded/r100.dat"), js(files / "em/pl00.drs"), js(files / "em/wep01.drs"), js(files / "Debug/roominfo.dat"), string("r100"), session);
    frozenPixels(jni);
    unpause(jni);
    controls(jni);
    require(javaException.empty() && leon && actorGpuReady && activeRoomId == 0x100, "Original r100 starts through production JNI");
    inventoryCase = re4::InventoryCase(3);
    require(inventoryCase.add(0x7f, 1), "Original XL case-size owner accompanies the test inventory");
    Java_com_ast_re4decomp_NativeBridge_giveTestWeapon(&jni.env, nullptr, 0x2e);
    frozenPixels(jni);
    require(leon->equippedWeaponId() == 0x2e && leon->leftHandType() == 0, "Rifle with weapon-owned left hand is equipped before r101");
    const auto *start = roomInfo->find(0x101, 0);
    require(start && start->valid(), "Original r101 entry position exists");
    requestRoom(0x101, 0, start->position, float(start->heading));
    const auto transfer = roomRequest;
    Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(&jni.env, nullptr, jlong(transfer.token), js(root / "decoded/r101.dat"), string(""));
    frozenPixels(jni);
    require(activeRoomId == 0x101 && leon->equippedWeaponId() == 0x2e && r100EventPhase == 12 && villageRadioIntro,
            "Real r100 -> r101 transfer retains equipped rifle and starts entry radio12");

    const auto closeRadio = [&] {
      for (unsigned tick = 0; tick < 180 && r100EventPhase == 12; ++tick) ticks(jni, 1);
      require(r100EventPhase == 13 && r100RadioTerm && leon->leftHandType() == 2, "Real radio animation enters original terminal");
      ticks(jni, 35);
      controls(jni, re4::Start);
      ticks(jni, 1);
      controls(jni);
      ticks(jni, 3);
      require(!paused && !r100RadioTerm && !eventUiActive.load() && r100EventPhase == 0 && !leon->roomScriptActive(),
              "START closes radio through actual JNI/GLES with no rendering pause or event lock");
      require(glGetError() == GL_NO_ERROR && actorGpuReady, "Room/player GPU owners survive radio completion");
    };
    closeRadio();
    require(leon->leftHandType() == 0 && entryCheckpoint && entryCheckpoint->room == 0x101,
            "Entry radio restores rifle hand and commits original r101 checkpoint");

    constexpr unsigned ids[]{0x23, 0x21, 0x40, 0x25, 0x27, 3, 0x29, 0x2a, 0x37, 0x2c, 0x94, 0x2d, 0x2e, 0x2f, 0x30, 0x3e, 0x34, 0x36, 0x35, 0x17, 0x6d, 0x52, 1, 2, 0xe};
    Java_com_ast_re4decomp_NativeBridge_setCheatFlags(&jni.env, nullptr, 3);
    for (unsigned id : ids) {
      inventoryCase = re4::InventoryCase(3);
      require(inventoryCase.add(0x7f, 1), "Repeated radio checkpoint retains original case-size owner");
      Java_com_ast_re4decomp_NativeBridge_giveTestWeapon(&jni.env, nullptr, jint(id));
      frozenPixels(jni);
      require(leon->equippedWeaponId() == id, "Each selectable weapon equips through real JNI");
      const auto hand = leon->leftHandType();
      beginR100Radio(12);
      closeRadio();
      require(leon->leftHandType() == hand, "Each selectable weapon restores its previous grip after radio");
      for (float horizontal : {-1.f, 1.f}) {
        controls(jni);
        const auto initial = leon->placement();
        const auto camera = re4::SceneFollowPose();
        Java_com_ast_re4decomp_NativeBridge_input(&jni.env, nullptr, 0, horizontal, 0, 0, 0, 0, 0);
        ticks(jni, 10);
        const auto matrix = leon->placement();
        double right = 0;
        for (unsigned axis = 0; axis < 3; ++axis)
          right += (matrix[axis * 4 + 2] - initial[axis * 4 + 2]) * camera.right[axis];
        require(horizontal * right > .02 && !paused, "Physical JNI left-stick input turns toward the requested screen side after radio");
      }
      controls(jni);
      const auto worldTicks = enemyGroup->worldTicks();
      ticks(jni, 3);
      require(enemyGroup->worldTicks() == worldTicks + 3 && !paused, "Live village simulation advances after radio and turn inputs");
    }
    std::cout << "K17 Fix1 production JNI/GLES r100->r101, radio/25 weapons, physical LS and resumed world PASS: " << checks << " checks\n";
  } catch (const std::exception &e) {
    std::cerr << "K17 Fix1 runtime regression: " << e.what() << '\n';
    return 1;
  }
}
