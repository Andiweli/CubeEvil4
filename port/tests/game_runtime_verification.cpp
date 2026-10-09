#include "re4/port/game_runtime.h"
#include "re4/port/sha256.h"
#include "re4_scheduler_original.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

namespace re4::port {
namespace {
struct Checks {
    int count = 0;
    void test(bool condition, const char *why) {
        ++count;
        if (!condition)
            throw std::runtime_error(why);
    }
    template <class F> void rejects(F callback, const char *why) {
        bool rejected = false;
        try {
            callback();
        } catch (const std::exception &) {
            rejected = true;
        }
        test(rejected, why);
    }
};
// Explicit isolated consumer-order fixture. No actor/event/trigger implementation.
struct ConsumerOrder final : SharedScenarioConsumers {
    std::vector<std::string> calls;
    void SceAtSetSaveItem() override { calls.push_back("saved-items"); }
    void SceExecInitCondition() override { calls.push_back("init-conditions"); }
    void SceExecCheckCondition() override { calls.push_back("check-conditions"); }
    void SubCharInit(int, Vec *, float) override { calls.push_back("partner-init"); }
    void SubCharCtrl(int, int) override { calls.push_back("partner-control"); }
    void EmSetFromList() override { calls.push_back("enemy-list"); }
    void SceAtRoomSet() override { calls.push_back("areas"); }
    void SubMissionCheck() override { calls.push_back("missions"); }
    void SceAtCheckMoveScrAt() override { calls.push_back("moving-areas"); }
    int scenarioCheckEventCancel() override {
        calls.push_back("cancel-check");
        return 0;
    }
    void EventRun() override { calls.push_back("events"); }
    void MessageStageInit() override { calls.push_back("stage-messages"); }
};
std::shared_ptr<ImportedFiles> fixture(const std::string &directory) {
    auto root = std::filesystem::u8path(directory);
    std::filesystem::create_directories(root / "blobs");
    std::vector<u8> bytes(0x1fe0, 0x5a); // Bounded short-file adapter test, no enemy positions.
    auto hash = sha256(bytes.data(), bytes.size());
    std::ofstream blob(root / "blobs" / hash, std::ios::binary);
    blob.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    blob.close();
    std::ofstream catalog(root / "catalog.tsv");
    catalog << "RE4-IMPORT-CATALOG-1\n";
    for (int n = 0; n <= 10; ++n)
        catalog << "0\t" << n + 1 << "\t8160\tetc/emleon" << (n < 10 ? "0" : "") << n
                << ".esl\t" << hash << '\n';
    catalog.close();
    return std::make_shared<ImportedFiles>(directory, (root / "catalog.tsv").u8string());
}
void inputTests(Checks &c) {
    ControllerInput input;
    PadRuntime pad(input);
    pad.tick();
    c.test(pad.joy().err == -1, "disconnect original PAD error");
    input.connect(9, 0);
    pad.tick();
    c.test(pad.joy().on == 0 && pad.key().on == 0, "neutral original pad");
    const std::pair<int, u16> mapping[] = {
        {19, JOY_UP}, {20, JOY_DOWN}, {21, JOY_LEFT}, {22, JOY_RIGHT}, {96, JOY_A},     {97, JOY_B},
        {99, JOY_X},  {100, JOY_Y},   {102, JOY_L},   {103, JOY_Z},    {108, JOY_START}};
    for (const auto &[key, bit] : mapping) {
        c.test(input.button(9, key, true), "mapped Android key");
        pad.tick();
        c.test(pad.joy().on == bit && pad.joy().trg == bit && pad.joy().rep == bit,
               "original press/trg/repeat");
        c.test((pad.key().on >> 32) == 0, "preserve original u32 KEY cursor");
        c.test(input.button(9, key, false), "mapped release");
        pad.tick();
        c.test(pad.joy().on == 0 && pad.joy().rel == bit && pad.key().rel != 0, "original release");
    }
    c.test(!input.button(9, 109, true) && !input.button(9, 106, true), "unmapped GC-less buttons");
    input.button(9, 96, true);
    pad.tick();
    c.test((pad.key().on & 0x80000000U) != 0, "original KEY_A mapping");
    for (int i = 0; i < 11; ++i) {
        pad.tick();
        c.test(pad.joy().rep == 0, "original repeat delay");
    }
    pad.tick();
    c.test(pad.joy().rep == JOY_A, "24-retrace repeat threshold");
    input.button(9, 96, false);
    pad.tick();
    input.axes(9, 0.15f, 0, 0, 0, 0, 0, 30.0f / 255, 0);
    pad.tick();
    c.test(pad.joy().stickX == 0 && pad.joy().triggerLeft == 0, "SDK deadzones unchanged");
    input.axes(9, 1, -1, -1, 1, 0, 0, 1, 1);
    pad.tick();
    c.test(pad.joy().stickX == 40 && pad.joy().stickY == 40, "SDK octagonal main-stick clamp");
    c.test(pad.joy().substickX == -31 && pad.joy().substickY == -31, "SDK octagonal C-stick clamp");
    c.test(pad.key().triggerLeft == 150 && pad.key().triggerRight == 150, "SDK trigger clamp");
    c.test((pad.joy().on & (JOY_L | JOY_R)) == (JOY_L | JOY_R), "analog shoulder gates");
    input.clearHeld();
    pad.tick();
    input.axes(9, 0, 0, 0, 0, -1, 0, 0, 0);
    input.button(9, 21, true);
    pad.tick();
    c.test(pad.joy().on == JOY_LEFT && pad.joy().trg == JOY_LEFT, "hat + key merge once");
    input.axes(9, 0, 0, 0, 0, 0, 0, 0, 0);
    pad.tick();
    c.test(pad.joy().on == JOY_LEFT && pad.joy().trg == 0, "held dpad key survives hat release");
    input.button(9, 22, true);
    pad.tick();
    c.test((pad.key().on & ((1U << 2) | (1U << 3))) == 0,
           "original opposite direction cancellation");
    input.clearHeld();
    pad.tick();
    input.button(9, 104, true);
    input.axes(9, 0, 0, 0, 0, 0, 0, 0.2f, 0);
    pad.tick();
    c.test(pad.joy().triggerLeft == 21, "digital trigger event cannot override real analog axis");
    input.connect(9, 0, false, false);
    input.button(9, 104, true);
    pad.tick();
    c.test(pad.joy().triggerLeft == 150, "digital fallback for controller without analog trigger");
    pad.stopKeys(0);
    pad.tick();
    c.test(pad.key().on == 0 && pad.key().triggerLeft == 0, "original key stop");
    input.disconnect(9);
    pad.tick();
    c.test(pad.joy().on == 0 && pad.key().on == 0, "disconnect clears held");
    input.connect(10, 0);
    pad.state().Stop_flg = 0;
    pad.tick();
    c.test(!input.button(9, 96, true), "stale device events rejected");
    c.rejects([&] { input.axes(10, std::numeric_limits<float>::quiet_NaN(), 0, 0, 0, 0, 0, 0, 0); },
              "reject NaN input");
    c.rejects([&] { input.connect(1, 4); }, "reject fifth gamepad");
    pad.clear();
    pad.tick();
    c.test(pad.key().on == 0, "repeated typed input reset");
}
void heapTests(Checks &c) {
    GameplayHeaps heaps(4096);
    auto system = heaps.allocate(65, 13, true);
    c.test(reinterpret_cast<std::uintptr_t>(system.data) % 32 == 0, "native allocation alignment");
    c.test(static_cast<u8 *>(system.data)[64] == 0 && heaps.liveBytes() == 96,
           "bounded clear/alignment accounting");
    c.test(heaps.replace(0, 1) && heaps.setCurrent(1), "system to game tail handoff");
    auto game = heaps.allocate(128);
    std::memset(game.data, 0x73, 128);
    c.test(heaps.replace(1, 2) && heaps.setCurrent(2), "game to stage tail handoff");
    auto stage = heaps.allocate(256);
    c.test(heaps.replace(2, 3) && heaps.setCurrent(3), "stage to module tail handoff");
    auto module = heaps.allocate(128);
    heaps.suspend(3);
    c.test(!heaps.setCurrent(3), "suspended heap cannot become current");
    c.rejects([&] { heaps.allocate(1); }, "suspended allocation rejected");
    heaps.resume(3);
    c.test(heaps.setCurrent(3), "heap resume");
    c.rejects([&] { heaps.allocate(SIZE_MAX); }, "overflow allocation");
    c.rejects([&] { heaps.allocate(4096); }, "bounded allocation");
    c.rejects([&] { heaps.setCurrent(13); }, "heap 13 is allocation selector only");
    c.test(!heaps.replace(1, 2), "live target lease cannot be overwritten");
    heaps.reset();
    c.test(heaps.liveBytes() == 0, "new arena excludes retired leases");
    c.test(static_cast<u8 *>(game.data)[127] == 0x73, "retired arena lease remains valid");
    auto fresh = heaps.allocate(128);
    c.test(fresh.data != game.data, "fresh arena does not alias pinned old data");
}
} // namespace

std::string verifyGameRuntime(const std::string &directory) {
    Checks c;
    inputTests(c);
    heapTests(c);
    ConsumerOrder consumers;
    ControllerInput input;
    input.connect(9, 0);
    u32 now = 500;
    GameRuntime game(fixture(directory), input, 0, [&] { return now; });
    game.initialize();
    c.test(game.state().room_id == 0x120 && game.state().RoomNo_next == 0x120,
           "original boot room");
    c.test(game.state().debug_mode == 1 && game.state().debug_disp == -1 &&
               game.state().game_mode == 5,
           "original boot settings");
    c.test(game.state().pl_type == 0 && game.state().game_country == 1,
           "original Leon / English init");
    game.state().play_time = 3661;
    game.initGameTime();
    u32 h = 0, m = 0, s = 0;
    c.test(game.gameTime(&h, &m, &s) == 3661 && h == 1 && m == 1 && s == 1,
           "original game time split");
    game.doorFlagInit();
    for (int bit : {0x96, 0x98, 0x9a, 0x9b, 0x9d, 0xa3, 0x72, 0x7a, 0x7f, 0x80, 0x84, 0x89})
        c.test((game.state().Scenario_flg[bit / 32] & (0x80000000U >> (bit % 32))) != 0,
               "original door flags ABI safe");
    auto &scheduler = game.loader().scheduler();
    auto old = game.sceExec(18, [] { original::SceSleep(20); });
    c.rejects([&] { game.scenarioRoomInit(); },
              "missing original saved-item consumer explicit failure");
    c.test(old && !scheduler.state(*old), "original room init kills scenario tasks");
    game.bindConsumers(&consumers);
    auto activation = game.queueModule(ModuleOperation::Activate, 78); // Real st1_2 / r10d.
    c.test(activation->status == OperationStatus::Pending, "module request deferred");
    game.tick();
    c.test(activation->status == OperationStatus::Complete, "module activation at frame boundary");
    game.state().room_id = 0x10d;
    game.scenarioRoomInit();
    c.test(consumers.calls ==
               std::vector<std::string>{"saved-items", "init-conditions", "enemy-list", "areas"},
           "isolated original room-init consumer order");
    c.test(game.modules().roomWorkLease(0x10d).size == 1, "actual original R10dInit allocation");
    c.test(game.heaps().liveBytes() >= 32, "original room work uses shared gameplay heap");
    consumers.calls.clear();
    std::vector<int> taskTrace;
    auto task = game.sceExec(18, [&] {
        int local = 7;
        taskTrace.push_back(local);
        original::SceSleep(1);
        local += 5;
        taskTrace.push_back(local);
    });
    game.scenarioMove();
    c.test(taskTrace == std::vector<int>{7}, "original scenario pass enters stackful task");
    game.pause();
    now += 500;
    game.tick();
    c.test(taskTrace.size() == 1, "pause retains task stack");
    c.test(game.gameTime(nullptr, nullptr, nullptr) == 3661,
           "paused wall time excluded from original play time");
    auto frames = game.state().Frame_cnt;
    c.test(game.displayFrame(900000000000LL) == 0 && game.state().Frame_cnt == frames,
           "paused VI ignores wall time");
    game.resume();
    game.scenarioMove();
    c.test(taskTrace == std::vector<int>{7, 12} && task && !scheduler.state(*task),
           "scenario resumes exact local state");
    c.test(consumers.calls == std::vector<std::string>{"check-conditions", "cancel-check", "events",
                                                       "missions", "moving-areas",
                                                       "check-conditions", "cancel-check", "events",
                                                       "missions", "moving-areas"},
           "isolated original scenario-move order");
    auto previous = game.modules().room(0x10d)->main;
    bool transitioned = false;
    auto main = scheduler.exec(2, [&] {
        game.awaitModule(ModuleOperation::Stop, 78);
        game.awaitModule(ModuleOperation::Restart, 78);
        transitioned = true;
        scheduler.exit();
    });
    for (int i = 0; i < 8 && !transitioned; ++i)
        game.tick();
    c.test(main && transitioned, "stackful frame-boundary stop/restart");
    game.modules().invoke(previous);
    auto ticket = game.queueModule(ModuleOperation::ResetRoom, 0x10d);
    game.tick();
    c.test(ticket->status == OperationStatus::Complete, "room reset at boundary");
    c.rejects([&] { game.modules().invoke(previous); }, "stale room callback rejected");
    std::shared_ptr<FrameOperation> cancelled;
    auto doomed = scheduler.exec(1, [&] {
        cancelled = game.queueModule(ModuleOperation::Deactivate, 78);
        scheduler.sleep(2);
        scheduler.exit();
    });
    game.tick();
    c.test(doomed && scheduler.kill(*doomed), "cancel requesting task");
    game.tick();
    c.test(cancelled->status == OperationStatus::Cancelled &&
               game.modules().info(78).phase == ModulePhase::Active,
           "cancelled requester cannot mutate module");
    game.initialize();
    c.test(game.state().Frame_cnt == 0 && game.state().room_id == 0x120 &&
               scheduler.allocatedStacks() == 0,
           "repeated native/original boot reset");
    for (int hz : {60, 90, 120}) {
        game.initialize();
        game.displayFrame(0);
        for (int i = 1; i <= hz; ++i)
            game.displayFrame(1000000000LL * i / hz);
        c.test(game.state().Frame_cnt == 30, "30 Hz independent of Android display rate");
    }
    game.initialize();
    game.startOriginalGameTask();
    c.rejects([&] { game.tick(); },
              "original GameTask fails at missing shared debug camera consumer");
    c.test(game.state().Rno0 == 0 && game.state().Rno1 == 0 && game.state().Rno2 == 0 &&
               game.state().Rno3 == 0,
           "actual original GameTask initial state executed");
    game.initialize();
    c.test(game.enemyListNumber(0x120) == 0 && game.enemyListNumber(0x100) == 0 &&
               game.enemyListNumber(0x315) == 7,
           "original universal ESL selection");
    c.test(game.enemyListName(0) == "etc/emleon00.esl" && game.enemyListName(11) == "...no string",
           "original ESL names and bounds");
    c.test(!game.modules().room(0x120), "r120 callback is honestly unavailable");
    auto start = game.queueModule(ModuleOperation::Activate, 74);
    game.tick();
    c.test(start->status == OperationStatus::Complete,
           "original st1_0 registration component activated");
    c.rejects([&] { game.scenarioRoomInit(); },
              "actual r120 initializer remains a named dependency blocker");
    game.initialize();
    consumers.calls.clear();
    game.state().em_list_no = -1;
    game.state().Em_list[8191] = 0x71;
    bool stageDone = false;
    auto stageTask = scheduler.exec(1, [&] {
        game.stageSet();
        stageDone = true;
        scheduler.exit();
    });
    for (int i = 0; i < 5000 && !stageDone; ++i) {
        game.tick();
        std::this_thread::yield();
    }
    c.test(stageTask && stageDone, "original StageSet and readEmList stackful flow");
    c.test(consumers.calls == std::vector<std::string>{"stage-messages"},
           "isolated message consumer in original stage order");
    c.test(game.state().em_list_no == 0 && game.state().Em_list[0] == 0x5a &&
               game.state().Em_list[8159] == 0x5a && game.state().Em_list[8191] == 0x71,
           "original ESL short-read preserves untouched destination tail");
    game.state().room_id_prev = game.state().room_id;
    game.state().room_id = 0x328;
    stageDone = false;
    stageTask = scheduler.exec(1, [&] {
        game.stageSet();
        stageDone = true;
        scheduler.exit();
    });
    for (int i = 0; i < 5000 && !stageDone; ++i) {
        game.tick();
        std::this_thread::yield();
    }
    c.test(stageTask && stageDone && game.modules().info(74).phase == ModulePhase::Inactive &&
               game.modules().info(101).phase == ModulePhase::Active,
           "original StageSet cross-stage relink at frame boundaries");
    game.scenarioRoomInit();
    c.test(game.modules().roomWorkLease(0x328).size == 1,
           "actual R328Init after original cross-stage flow with isolated consumers");
    auto pinnedWork = game.modules().roomWorkLease(0x328);
    game.state().room_id_prev = game.state().room_id;
    game.state().room_id = 0x10d;
    auto blockedSwitch = scheduler.exec(1, [&] {
        game.stageSet();
        scheduler.exit();
    });
    bool blocked = false;
    for (int i = 0; i < 100 && !blocked; ++i) {
        try {
            game.tick();
        } catch (const std::logic_error &failure) {
            blocked = std::string(failure.what()) ==
                      "MemReplaceHeap: native live-resource boundary prevents handoff";
            if (!blocked)
                throw;
        }
    }
    c.test(blockedSwitch && blocked && *pinnedWork.data == 0,
           "initialized room stage-switch stops explicitly before unsafe heap reuse");
    game.initialize();
    c.test(pinnedWork.data && *pinnedWork.data == 0 &&
               game.modules().info(101).phase == ModulePhase::Inactive,
           "external original room-work lease survives reset while callback invalidates");
    // Kill a sleeping original ESL task repeatedly. More than the 16 DVD queue
    // entries are exercised; abandoned requests must not accumulate.
    for (int n = 0; n < 20; ++n) {
        game.state().em_list_no = -1;
        auto reader = scheduler.exec(1, [&] {
            game.readEnemyList(0); // Original proc=0 => asynchronous mode 0x10.
            scheduler.exit();
        });
        game.tick();
        c.test(reader && scheduler.kill(*reader), "cancel sleeping original readEmList task");
        for (int i = 0; i < 4; ++i) {
            game.tick();
            std::this_thread::yield();
        }
    }
    bool recovered = false;
    game.state().em_list_no = -1;
    auto reader = scheduler.exec(1, [&] {
        game.readEnemyList(0);
        recovered = true;
        scheduler.exit();
    });
    for (int i = 0; i < 5000 && !recovered; ++i) {
        game.tick();
        std::this_thread::yield();
    }
    c.test(reader && recovered && game.state().Em_list[8159] == 0x5a,
           "original readEmList completes after repeated cancellation without queue leak");
    game.initialize();
    c.test(scheduler.allocatedStacks() == 0, "all stack owners released on reset");
    std::ostringstream out;
    out << "{\"pointer_bits\":" << sizeof(void *) * 8 << ",\"checks\":" << c.count
        << ",\"failed\":0,"
           "\"kind\":\"original_scalar_pad_scenario_bodies_and_isolated_consumer_order_tests\","
           "\"trace\":[\"systemWorkInit:r120,Leon,English,mode5\",\"pad:original-clamp,trg,rel,"
           "repeat,u32-key-cursor\","
           "\"heaps:bounded,lease-pinned,tail-handoff\",\"scenario:original-order,stack-sleep,"
           "pause,resume\","
           "\"modules:deferred,stop,restart,stale,cancel\",\"VI:60,90,120=>30\",\"GameTask:blocked-"
           "gameDebug\",\"r120:blocked-R120Init\"]}";
    return out.str();
}
} // namespace re4::port
