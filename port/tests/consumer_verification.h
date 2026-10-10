#pragma once
#include "re4/port/game_runtime.h"
#include "re4_scheduler_original.h"
#include <cstring>
namespace re4::port::tests {
// Explicit isolated byte fixtures, not a reconstructed room or an event package from the game.
inline ResourceLease bytes(std::vector<u8> value) {
    auto owner = std::make_shared<const std::vector<u8>>(std::move(value));
    return {owner, owner->data(), static_cast<u32>(owner->size())};
}
inline void word(std::vector<u8> &b, std::size_t n, u32 value) {
    for (int i = 0; i < 4; ++i)
        b.at(n + std::size_t(i)) = u8(value >> (24 - i * 8));
}
inline ResourceLease eventBytes(const char *no = "s00") {
    std::vector<u8> b(0x50, 0);
    std::memcpy(b.data(), "event", 5);
    std::memcpy(b.data() + 0x20, "r120", 5);
    std::memcpy(b.data() + 0x28, no, std::strlen(no) + 1);
    word(b, 0x40, 0x50);
    word(b, 0x4c, 0x50);
    return bytes(std::move(b));
}
template <class Checks> void consumerTests(Checks &c, GameRuntime &game) {
    auto &engine = game.engineConsumers();
    std::vector<u8> bin(0x60, 0);
    word(bin, 0, 0x40);
    word(bin, 0x3c, 0x20030818);
    bin[0x19] = 2;
    bin[0x41] = 255;
    bin[0x51] = 0;
    word(bin, 0x44, 0x3f800000); // Real IEEE1 in isolated BIN header fixture.
    auto model = engine.model(bytes(bin), "fixture.bin");
    auto &cpu = engine.model(model);
    c.test(cpu.nParts == 2 && cpu.pList->pParent == &cpu && cpu.pList->pList->pParent == cpu.pList,
           "original setPartsParent over checked native BIN joints");
    c.test(cpu.pList->pos.x == 1 && cpu.getPartsPtr(1) == cpu.pList->pList,
           "original joint lookup and BE float adapter");
    c.rejects([&] { cpu.getPartsPtr(2); }, "original out-of-range joint error retained");
    cpu.be_flag |= 2;
    c.test(cpu.isTrans() == 1, "original model visibility");
    cpu.setNoSuspend(1);
    c.test((cpu.be_flag & 0x800) != 0, "original event no-suspend model bit");
    cpu.setNoSuspend(0);
    c.test((cpu.be_flag & 0x800) == 0, "original model event reset");
    cpu.pos = {1, 2, 3};
    cpu.pList->world = {7, 8, 9};
    cpu.pList->world_old = {4, 5, 6};
    cpu.updateOldPos();
    c.test(cpu.pos_old.z == 3 && cpu.pList->world_old2.z == 6 && cpu.pList->world_old.z == 9,
           "original model position history");
    auto bad = bin;
    bad[0x51] = 1;
    c.rejects([&] { engine.model(bytes(bad), "cycle.bin"); }, "cyclic model parent rejected");
    bad = bin;
    word(bad, 0, 0xfffffff0);
    c.rejects([&] { engine.model(bytes(bad), "outside.bin"); }, "BIN offset overflow rejected");
    bad = bin;
    bad[0x41] = 7;
    c.rejects([&] { engine.model(bytes(bad), "parent.bin"); }, "BIN parent bounds");
    bad = bin;
    word(bad, 0x3c, 0);
    c.rejects([&] { engine.model(bytes(bad), "version.bin"); }, "unknown BIN version fails closed");
    std::vector<u8> message(32, 0);
    word(message, 8, 12);
    word(message, 16, 1);
    word(message, 20, 12);
    message[24] = 0;
    message[25] = 0x80;
    engine.bindMessage(1, bytes(message));
    c.test(engine.messageCount(1) == 1 && engine.message(1, 0).data[1] == 0x80,
           "actual MessageData::getMesNum/getAddr over native lookup, original BE text retained");
    c.test(!engine.message(1, -1).data && !engine.message(1, 1).data,
           "native message index validation");
    bad = message;
    word(bad, 8, 0xfffffff0);
    c.rejects([&] { engine.bindMessage(1, bytes(bad)); },
              "incomplete English MDT remains an error");
    c.rejects([&] { engine.bindMessage(1, bytes(message), 0); },
              "additional language not inferred");
    std::vector<u8> cam(16, 0);
    std::memcpy(cam.data(), "B404", 4);
    engine.bindCamera(bytes(cam));
    c.test(engine.camera().version == 4 && engine.camera().areas == 0,
           "checked CAM view, no camera decisions fabricated");
    engine.cameraState.areaNo = 7;
    engine.cameraState.cameraNo = 9;
    c.test(engine.cameraState.CurrentAreaNo() == 7 && engine.cameraState.CurrentCameraNo() == 9,
           "original camera state getters");
    engine.cameraState.Disable();
    c.test(engine.cameraState.r0 == 0 && (engine.cameraState.m_system_flag & 8),
           "original camera disable");
    engine.cameraState.AreaCheckOnOff(1);
    c.test(!(engine.cameraState.m_system_flag & 8), "original camera area-check enable");
    bad = cam;
    bad[5] = 255;
    c.rejects([&] { engine.bindCamera(bytes(bad)); }, "camera table bounds");
    int calls = 0;
    u32 param = 0;
    engine.setInvocationBoundary(
        [](const TaskEntry &call) { call(); }); // Isolated module boundary fixture.
    engine.SetFunc("evt_r120s00_func", [&](EventCpu *e, u32 p) {
        ++calls;
        param = p;
        c.test(e->GetFuncType() == 3, "original callback func type");
    });
    auto evt = engine.event(eventBytes());
    engine.invoke(evt, 3, -1);
    c.test(calls == 1 && param == 0xffffffff, "actual Event::ExeFunc argument width/order");
    auto copied = EventFunction{};
    engine.GetFunc(&copied, "evt_r120s00_func");
    engine.SetFunc("evt_r120s00_func", [](EventCpu *, u32) {
        throw std::runtime_error("binding must remain original");
    });
    engine.invoke(evt, 3, 0);
    c.test(calls == 2, "original DatTbl duplicate-name binding retained");
    engine.event(evt).StatusFlag = 0x20000;
    engine.invoke(evt, 0, 0);
    c.test(calls == 2, "original event NoFunc bit suppresses callback");
    engine.event(evt).StatusFlag = 0x08000000;
    engine.invoke(evt, 1, 0);
    c.test(calls == 2, "original event cancellation bit suppresses update callback");
    c.test(MessageIndexCpu{}.getSpaceWidth() == 8, "original English space width");
    auto evd = std::vector<u8>(0x50, 0);
    std::memcpy(evd.data(), "event", 5);
    std::memcpy(evd.data() + 0x20, "r120", 5);
    std::memcpy(evd.data() + 0x28, "s00", 4);
    word(evd, 0x40, 0xfffffff0);
    word(evd, 0x44, 32);
    c.rejects([&] { engine.event(bytes(evd)); }, "EVD packet bounds reject integer overflow");
    word(evd, 0x40, 0x50);
    word(evd, 0x44, 0);
    word(evd, 0x48, 0xffffffff);
    c.rejects([&] { engine.event(bytes(evd)); }, "EVD bin count overflow rejected");
    word(evd, 0x48, 0);
    word(evd, 0x4c, 0x50);
    std::memset(evd.data() + 0x20, 'x', 8);
    c.rejects([&] { engine.event(bytes(evd)); },
              "EVD bounded original callback name rejected before strcpy");
    engine.clear();
    c.rejects([&] { engine.model(model); }, "model reference invalid before storage release");
    c.rejects([&] { engine.event(evt); }, "event reference invalid before storage release");
    c.rejects([&] { copied(nullptr, 0); }, "copied callback rejects retired room");
    c.test(engine.modelCount() == 0, "room model registry released");
    bool foreign = false;
    std::thread thread([&] {
        try {
            engine.modelCount();
        } catch (const std::logic_error &) {
            foreign = true;
        }
    });
    thread.join();
    c.test(foreign, "consumer owner thread enforced");
}
template <class Checks> void openingTests(Checks &c, GameRuntime &game) {
    auto &scheduler = game.loader().scheduler();
    for (int round = 0; round < 12; ++round) {
        game.initialize();
        auto activate = game.queueModule(ModuleOperation::Activate, 74);
        game.tick();
        c.test(activate->status == OperationStatus::Complete &&
                   game.modules().room(0x120).has_value(),
               "reviewed original r120 entry registered");
        std::string failure;
        try {
            game.invokeRoomEntry(0x120, true);
        } catch (const MissingGameConsumer &e) {
            failure = e.what();
        }
        c.test(failure.empty() && game.texRender().count()==1 &&
                   game.texRender().target(0).GetWSize()==256 && game.texRender().bufferBytes()==256*256*4,
               "actual R120Init completes CPU TexRender initialization");
        c.test(game.modules().roomWorkLease(0x120).size >= 0x388 && original::TaskStatus(17) != 0,
               "actual r120 native work and original R120Event scheduled");
        auto &engine = game.engineConsumers();
        auto evt = engine.event(eventBytes());
        auto copied = EventFunction{};
        engine.GetFunc(&copied, "evt_r120s00_func");
        auto flagsBefore = game.state().Scenario_flg[1];
        engine.invoke(evt, 3, 0); // Actual original cancel callback only, not Event::Init/Run.
        c.test((game.state().Scenario_flg[SCF_R120_EVENT_CANCEL / 32] &
                (0x80000000u >> (SCF_R120_EVENT_CANCEL % 32))) != 0,
               "actual r120 cancel callback persists original scenario bit");
        c.test(game.state().Scenario_flg[1] == (flagsBefore | 0x10),
               "original cancel bit 59 is exactly Scenario_flg[1] mask 0x10");
        // Harness visits the original scenario OT explicitly: production calls it
        // from ScenarioMove/gameMainLoop, which is not yet reachable from GameTask.
        game.tick(); scheduler.runScenarios();
        failure.clear();
        try { game.tick(); scheduler.runScenarios(); } catch(const MissingGameConsumer &e) { failure=e.what(); }
        c.test(failure=="Original consumer unavailable: systemVISetBlack/presentation adapter",
               "original opening advances to actual VI dependency");
        game.tick(); // System-owned failed-task cleanup survives the invalidated caller.
        c.test(game.modules().info(74).phase == ModulePhase::Inactive &&
                   scheduler.allocatedStacks() == 0,
               "failed original opening kills borrowers and releases work at frame boundary");
        c.test(game.texRender().count()==0 && !game.texRender().bufferBytes(),
               "failed original opening releases TexRender buffers");
        c.rejects([&] { engine.event(evt); },
                  "failed original room init invalidates event references");
        c.rejects([&] { copied(nullptr, 0); },
                  "failed original room init invalidates copied callback");
    }
    game.initialize();
    auto task = game.sceExec(18, [&] { game.runOpeningTask(); });
    scheduler.runScenarios();
    c.test(task && scheduler.state(*task).has_value(),
           "actual R120Event preserves first SceSleep stack");
    std::string failure;
    try {
        scheduler.runScenarios();
    } catch (const MissingGameConsumer &e) {
        failure = e.what();
    }
    c.test(failure == "Original consumer unavailable: systemVISetBlack/presentation adapter",
           "actual opening task cannot silently skip presentation/video");
    game.initialize();
    auto activate=game.queueModule(ModuleOperation::Activate,74); game.tick();
    c.test(activate->status==OperationStatus::Complete,"room generation cleanup fixture activation");
    game.queueFailedRoomRetirement(0x120);
    game.modules().deactivateId(74); game.modules().activateId(74);
    game.tick();
    c.test(game.modules().info(74).phase==ModulePhase::Active,
           "queued failure retirement cannot retire a newly linked room generation");
    game.initialize();
}
template <class Checks> void retirementTests(Checks &c, GameRuntime &game) {
    auto &scheduler = game.loader().scheduler();
    game.initialize();
    for (int n = 0; n < 20; ++n) {
        auto room = u16(n % 2 ? 0x328 : 0x10d);
        game.state().room_id_prev = n ? game.state().room_id : 0;
        game.state().room_id = room;
        bool done = false;
        auto task = scheduler.exec(1, [&] {
            game.stageSet();
            done = true;
            scheduler.exit();
        });
        for (int i = 0; i < 5000 && !done; ++i) {
            game.tick();
            std::this_thread::yield();
        }
        c.test(task && done,
               "initialized original rooms can cross stage heaps after central retirement");
        game.scenarioRoomInit(); // Existing consumer-order fixture; actual original R10d/R328
                                 // allocation.
        auto before = game.modules().room(room)->main;
        auto metadata =
            std::find_if(originalModuleCatalog().rooms.begin(), originalModuleCatalog().rooms.end(),
                         [room](const auto &entry) { return entry.id == room; });
        auto borrower = scheduler.exec(2, game.modules().bind(before),
                                       game.modules().lifetime(metadata->fileIndex));
        c.test(borrower.has_value(), "original room callback pending at transition");
        // Do not visit it: original empty Main returns instead of TaskExit.
        auto retire = game.queueRoomRetirement(room);
        game.tick();
        c.test(retire->status == OperationStatus::Complete && !scheduler.state(*borrower),
               "retirement invalidates pending main borrower before work release");
        c.rejects([&] { game.modules().invoke(before); },
                  "retirement invalidates original room callback epoch");
    }
    game.initialize();
}
template <class Checks> void automaticTransitionTests(Checks &c, GameRuntime &game) {
    auto &scheduler = game.loader().scheduler();
    game.initialize();
    std::optional<ModuleCallback> previous;
    std::optional<TaskHandle> borrower;
    std::optional<ConsumerHandle> reference;
    for (int n = 0; n < 20; ++n) {
        auto room = u16(n % 2 ? 0x328 : 0x10d);
        game.state().room_id_prev = n ? game.state().room_id : 0;
        game.state().room_id = room;
        bool done = false;
        auto task = scheduler.exec(1, [&] {
            game.stageSet();
            done = true;
            scheduler.exit();
        });
        for (int i = 0; i < 5000 && !done; ++i) {
            game.tick();
            std::this_thread::yield();
        }
        c.test(task && done,
               "shared StageSet adapter retires initialized old room before heap reuse");
        if (previous) {
            c.rejects([&] { game.modules().invoke(*previous); },
                      "automatic stage change invalidates old callback");
            c.test(!scheduler.state(*borrower),
                   "automatic stage change kills sleeping module borrower");
            c.rejects([&] { game.engineConsumers().event(*reference); },
                      "automatic stage change invalidates resource references");
        }
        game.scenarioRoomInit(); // Explicit existing isolated shared-consumer fixture.
        previous = game.modules().room(room)->main;
        reference =
            game.engineConsumers().event(eventBytes()); // Isolated reference lifetime fixture.
        auto nested = [&] {
            scheduler.sleep(200);
            scheduler.exit();
        };
        borrower = scheduler.exec(2, [&, room] { game.modules().callRoomEntry(room, nested); });
        game.tick();
        c.test(borrower && scheduler.state(*borrower).has_value(),
               "module borrower stack lives until central transition");
    }
    auto released = game.queueModule(ModuleOperation::ReleaseRoom, game.state().room_id);
    game.tick();
    c.test(released->status == OperationStatus::Complete,
           "same-module room release completes at frame boundary");
    auto callback = game.modules().room(game.state().room_id)->init;
    game.scenarioRoomInit();
    c.test(game.modules().roomWorkLease(game.state().room_id).size == 1,
           "same-module room can initialize again without another prolog");
    c.rejects([&] { game.modules().invoke(*previous); },
              "same-module release changes callback epoch");
    c.test(callback.epoch != previous->epoch,
           "same-module native callback generations remain full width");
    auto resetReference = game.engineConsumers().event(eventBytes());
    auto reset = game.queueModule(ModuleOperation::ResetRoom, game.state().room_id);
    game.tick();
    c.test(reset->status == OperationStatus::Complete,
           "explicit ResetRoom uses shared safe retirement");
    c.rejects([&] { game.engineConsumers().event(resetReference); },
              "explicit ResetRoom invalidates event references");
    game.scenarioRoomInit();
    auto unloadReference = game.engineConsumers().event(eventBytes());
    auto unload = game.queueModule(ModuleOperation::Deactivate, 101);
    game.tick();
    c.test(unload->status == OperationStatus::Complete &&
               game.modules().info(101).phase == ModulePhase::Inactive,
           "direct active-room module unload uses shared safe retirement");
    c.rejects([&] { game.engineConsumers().event(unloadReference); },
              "direct module unload invalidates event references");
    game.initialize();
}
template <class Checks> void callbackStackTests(Checks &c, GameRuntime &game) {
    auto &scheduler = game.loader().scheduler();
    game.initialize();
    auto activated = game.queueModule(ModuleOperation::Activate, 74);
    game.tick();
    c.test(activated->status == OperationStatus::Complete, "callback stack fixture module active");
    auto &engine = game.engineConsumers();
    engine.setInvocationBoundary(
        [&](const TaskEntry &call) { game.modules().callRoomEntry(0x120, call); });
    int reached = 0;
    engine.SetFunc("evt_r120s00_func", [&](EventCpu *, u32) {
        int local = 73;
        original::TaskSleep(1);
        reached = local;
        original::TaskSleep(1);
        reached = local + 1;
    }); // Isolated callback, actual Event::ExeFunc and TaskSleep wrappers.
    auto event = engine.event(eventBytes());
    auto task = game.sceExec(5, [&] {
        engine.invoke(event, 1, 0);
        scheduler.exit();
    });
    scheduler.runScenarios();
    c.test(task && scheduler.state(*task) && reached == 0,
           "event callback has a sleeping native stack");
    c.rejects([&] { engine.clear(); },
              "consumer clear refuses storage release while callback sleeps");
    auto stopped = game.queueModule(ModuleOperation::Stop, 74);
    game.tick();
    scheduler.runScenarios();
    c.test(stopped->status == OperationStatus::Complete && reached == 0,
           "module stop preserves sleeping event callback");
    auto restarted = game.queueModule(ModuleOperation::Restart, 74);
    game.tick();
    scheduler.runScenarios();
    c.test(restarted->status == OperationStatus::Complete && reached == 73,
           "event callback resumes exact local state after module restore");
    scheduler.runScenarios();
    c.test(reached == 74 && !scheduler.state(*task),
           "event callback completes after nested sleeps");
    auto cancelled = game.sceExec(5, [&] {
        engine.invoke(event, 1, 0);
        scheduler.exit();
    });
    scheduler.runScenarios();
    auto retired = game.queueRoomRetirement(0x120);
    game.tick();
    c.test(cancelled && retired->status == OperationStatus::Complete &&
               !scheduler.state(*cancelled),
           "retirement kills callback before invocation ledger release");
    c.rejects([&] { engine.event(event); }, "cancelled callback package reference invalidated");
    game.initialize();
    EventFunction copied;
    {
        EngineConsumers owner;
        owner.SetFunc("fixture", [](EventCpu *, u32) {});
        owner.GetFunc(&copied, "fixture");
    }
    c.rejects([&] { copied(nullptr, 0); }, "callback copy rejects destroyed consumer owner");
}
} // namespace re4::port::tests
