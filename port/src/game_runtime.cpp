#include "runtime_original.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <utility>

namespace re4::port {
SharedScenarioConsumers &OriginalContext::required(const char *name) {
    trace.push_back(name);
    if (!consumers)
        throw MissingGameConsumer(name);
    return *consumers;
}
void OriginalContext::reset() {
    global = NativeGameState{};
    system = System{};
    for (auto &joy : Joy)
        joy = JOY{};
    Key = KEY{};
    for (auto &p : Pad_data)
        p = PADStatus{};
    ResetBits = ConnectedBits = 0;
    Vib_level = 0;
    un_stop_mask = 0;
    allocations.clear();
    pUser_name = nullptr;
    pSUB = nullptr;
    SceSys = Scenario{};
    SceSys.context = this;
    RoomData = RoomFacade{};
    RoomData.context = this;
    cMes.context = EvtMgr.context = Dvd.context = this;
    for (auto &entry : ItemEventTbl)
        entry = nullptr;
    reads.clear();
    nextRead = 1;
    r120_work = nullptr;
    trace.clear();
}
void *OriginalContext::mem_calloc(u32 bytes, const char *, int, int flag, int heap) {
    if (!heaps || flag != 1)
        throw std::logic_error("systemWorkInit allocation contract");
    auto allocation = heaps->allocate(bytes, heap, true);
    auto *ptr = allocation.data;
    allocations.push_back(std::move(allocation));
    return ptr;
}
void OriginalContext::TaskSleep(int visits) {
    if (visits)
        runtime->loader().scheduler().sleep(visits);
}
void OriginalContext::ScenarioTaskAllOff() {
    runtime->loader().scheduler().killScenarios();
    SceSys.event_cancel_enable = 0;
    // NativeScheduler owns typed OT links and clears them when killing each slot.
}
void OriginalContext::ScenarioInit() {
    ScenarioTaskAllOff();
    SceSys = Scenario{};
    SceSys.context = this; // Typed reset instead of pointer-bearing memclr.
}
void OriginalContext::Scenario::scheduler() {
    auto &scheduler = context->runtime->loader().scheduler();
    scheduler.setScenarioGates(m_item_get != 0 || chapterEnd != 0,
                               nativeFlag(&context->global.Stop_flg, SPF_SCE) != 0);
    scheduler.runScenarios();
}
void OriginalContext::MessageFacade::stageInit() {
    context->required("cMes.stageInit").MessageStageInit();
}
void OriginalContext::MessageFacade::gameInit() { context->runtime->messages().gameInit(); }
void OriginalContext::SubScreenGameInit() { runtime->subscreen().gameInit(); }
void OriginalContext::EventFacade::Run() { context->required("EvtMgr.Run").EventRun(); }
namespace {
const RoomMetadata &roomMeta(u16 id) {
    const auto &rooms = originalModuleCatalog().rooms;
    auto it = std::find_if(rooms.begin(), rooms.end(), [&](const auto &r) { return r.id == id; });
    if (it == rooms.end())
        throw std::invalid_argument("unregistered original room");
    return *it;
}
} // namespace
int OriginalContext::RoomFacade::checkRelRead(u16 room) {
    return m_RelNo != roomMeta(room).fileIndex;
}
void OriginalContext::RoomFacade::stopRelData() {
    if (!linkedModule)
        return;
    context->runtime->awaitModule(ModuleOperation::Stop, linkedModule);
}
void OriginalContext::RoomFacade::linkRelData(u16 room) {
    const auto &meta = roomMeta(room);
    if (linkedModule && linkedModule != meta.module)
        context->runtime->awaitModule(ModuleOperation::Deactivate, linkedModule);
    context->runtime->awaitModule(ModuleOperation::Activate, meta.module);
    linkedModule = meta.module;
    linkedRoom = room;
    m_RelNo = meta.fileIndex;
}
void OriginalContext::RoomFacade::execInitFunc(u16 room) {
    auto callbacks = context->runtime->modules().room(room);
    if (!callbacks)
        throw MissingGameConsumer(roomMeta(room).init);
    context->trace.push_back(roomMeta(room).init);
    context->runtime->modules().invoke(callbacks->init);
}
void OriginalContext::RoomFacade::execMainFunc(u16 room) {
    auto callbacks = context->runtime->modules().room(room);
    if (!callbacks)
        throw MissingGameConsumer(roomMeta(room).main);
    context->trace.push_back(roomMeta(room).main);
    context->runtime->modules().invoke(callbacks->main);
}
int OriginalContext::DvdReadN(const char *path, void *destination, int a, int b, int c, int mode,
                              const char *, int) {
    if (destination != global.Em_list || a || b || c || (mode != 0x11 && mode != 0x10))
        throw std::invalid_argument("readEmList bounded DvdReadN contract");
    if (nextRead == INT32_MAX)
        throw std::overflow_error("native read IDs");
    auto id = nextRead++;
    auto request = runtime->loader().request(path, static_cast<u16>(mode));
    auto caller = runtime->loader().scheduler().current();
    if (!caller) {
        runtime->loader().cancel(request);
        throw std::logic_error("bounded DvdReadN requires task");
    }
    reads.emplace(
        id, ReadInto{request, static_cast<u8 *>(destination), sizeof(global.Em_list), *caller});
    trace.push_back(std::string("DvdReadN:") + path);
    return id;
}
void OriginalContext::collectCancelledReads() {
    auto &loader = runtime->loader();
    for (auto it = reads.begin(); it != reads.end();) {
        if (loader.scheduler().state(it->second.caller)) {
            ++it;
            continue;
        }
        auto status = loader.status(it->second.request);
        if (status == LoadStatus::Pending || status == LoadStatus::Reading)
            loader.cancel(it->second.request);
        std::shared_ptr<const LoadedFile> discarded;
        auto result = loader.readCheck(it->second.request, discarded);
        if (result == 0)
            ++it;
        else
            it = reads.erase(it);
    }
}
int OriginalContext::DvdFacade::ReadCheck(int id, int *result, void *, void *) {
    auto it = context->reads.find(id);
    if (it == context->reads.end())
        throw std::invalid_argument("stale native DvdReadN ID");
    auto &loader = context->runtime->loader();
    auto status = loader.status(it->second.request);
    if (status == LoadStatus::Pending || status == LoadStatus::Reading)
        return 0;
    std::shared_ptr<const LoadedFile> file;
    auto completed = loader.readCheck(it->second.request, file);
    int bytes = completed == 1 && file && file->bytes.size() <= it->second.capacity
                    ? static_cast<int>(file->bytes.size())
                    : 0;
    if (bytes)
        std::memcpy(it->second.destination, file->bytes.data(), file->bytes.size());
    if (result)
        *result = bytes;
    context->reads.erase(it);
    return 1; // Completed original read; result==0 selects the original zero-list fallback.
}

struct GameRuntime::Impl {
    EngineLoader loader;
    std::shared_ptr<GameplayHeaps> heaps = std::make_shared<GameplayHeaps>();
    struct HeapMemory final : ModuleRuntime::Memory {
        std::shared_ptr<GameplayHeaps> heaps;
        const std::uint64_t identity;
        static std::uint64_t id() {
            static std::atomic<std::uint64_t> identities{0x8000000000000000ULL};
            auto value = identities.fetch_add(1);
            if (!value)
                throw std::overflow_error("native heap provider identity");
            return value;
        }
        explicit HeapMemory(std::shared_ptr<GameplayHeaps> h)
            : heaps(std::move(h)), identity(id()) {}
        MemoryReservation reserve(u32 bytes) override {
            auto block = heaps->allocate(bytes, 13, true);
            return {std::move(block.owner), static_cast<u8 *>(block.data), bytes, identity};
        }
        void release(MemoryReservation &memory) override {
            if (memory.runtime != identity)
                throw std::invalid_argument("foreign module heap allocation");
            memory = {};
        }
    };
    std::shared_ptr<HeapMemory> moduleMemory = std::make_shared<HeapMemory>(heaps);
    ModuleRuntime modules;
    EngineConsumers engineConsumers;
    TexRenderRuntime texRender;
    GraphicsState graphics;
    OriginalContext original;
    MessageRuntime messages;
    SubScreenRuntime subscreen;
    FrameViAdapter vi;
    const std::thread::id owner = std::this_thread::get_id();
    bool initialized = false, paused = true, stepping = false;
    std::vector<TaskHandle> gameTasks;
    std::vector<u32> activated;
    struct Pending {
        ModuleOperation operation;
        u32 id;
        std::optional<TaskHandle> caller;
        std::shared_ptr<FrameOperation> result;
        u64 roomEpoch=0; // System cleanup must not retire a newly linked generation.
    };
    std::vector<Pending> pending;
    std::vector<Pending> settled; // Pins await records outside cancellable task stacks.
    Impl(GameRuntime *runtime, std::shared_ptr<const ImportedFiles> files, PlatformGamepads &pads,
         u8 disc)
        : loader(files, disc), modules(loader, moduleMemory),
          engineConsumers(&loader.scheduler()), texRender([this](u32 bytes){ return moduleMemory->reserve(bytes); }),
          original(pads), messages(loader,*heaps,original.global,original.system.language,files,disc),
          subscreen(loader,*heaps,original.global,original.system.language,std::move(files),disc), vi([runtime] { runtime->tick(); }) {
        original.runtime = runtime;
        engineConsumers.setAdaOverrideProvider([this]{return DbgFlagChk(original.pG,DBG_ADA_OMAKE_EV);});
        original.heaps = heaps.get();
        modules.bindRoomEntry(0x120, [this](bool init) {
            original.RoomData.m_RelNo = roomMeta(0x120).fileIndex;
            original.RoomData.linkedModule = roomMeta(0x120).module;
            original.RoomData.linkedRoom = 0x120;
            if (!init) {
                original.R120Main();
                return;
            }
            original.r120_work =
                static_cast<OriginalContext::R120Work *>(ModuleContext::active().roomWork(0x120));
            engineConsumers.setInvocationBoundary(
                [this](const TaskEntry &call) { modules.callRoomEntry(0x120, call); });
            try {
                original.R120Init();
            } catch (...) {
                // System-owned cleanup survives cancellation of the failing caller.
                pending.push_back(
                    {ModuleOperation::RetireRoom, 0x120, {}, std::make_shared<FrameOperation>(), modules.info(roomMeta(0x120).module).epoch});
                throw;
            }
        });
        original.seconds = [] {
            return static_cast<u32>(std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count());
        };
        original.reset();
    }
    void check(bool boundary = false) const {
        if (owner != std::this_thread::get_id())
            throw std::logic_error("game runtime owner thread");
        if (boundary &&
            (const_cast<EngineLoader &>(loader).scheduler().current() ||
             const_cast<EngineLoader &>(loader).scheduler().moduleCallContext() || stepping))
            throw std::logic_error("game lifecycle requires frame boundary");
    }
    void cancelPending() {
        for (auto &p : pending) {
            p.result->status = OperationStatus::Cancelled;
            p.result->error = "runtime reset";
        }
        pending.clear();
        settled.clear();
    }
    void retire(u16 id, bool deactivate) {
        const auto &meta = roomMeta(id);
        loader.scheduler().killScenarios();
        modules.quiesceRoom(id); // Kill owned and borrowed stacks before consumer storage.
        loader.cancelRoom();
        loader.cancelAll();
        engineConsumers.clear();
        texRender.clear();
        original.ScenarioInit();
        original.r120_work = nullptr;
        if (deactivate)
            modules.deactivateId(meta.module);
        else
            modules.resetRoom(id);
        if (deactivate && original.RoomData.linkedModule == meta.module) {
            original.RoomData.linkedModule = 0;
            original.RoomData.m_RelNo = 0;
            original.RoomData.linkedRoom = 0;
        }
        original.trace.push_back("retire-room:" + std::to_string(id));
    }
    void apply() {
        settled.erase(std::remove_if(settled.begin(), settled.end(),
                                     [this](const auto &p) {
                                         return !p.caller || !loader.scheduler().state(*p.caller);
                                     }),
                      settled.end());
        auto batch = std::move(pending);
        pending.clear();
        for (auto &p : batch) {
            if(p.roomEpoch && modules.info(roomMeta(static_cast<u16>(p.id)).module).epoch!=p.roomEpoch) {
                p.result->status=OperationStatus::Cancelled;
                p.result->error="failed-room generation superseded";
                continue;
            }
            if (p.caller && !loader.scheduler().state(*p.caller)) {
                p.result->status = OperationStatus::Cancelled;
                p.result->error = "requesting task invalidated";
                continue;
            }
            try {
                switch (p.operation) {
                case ModuleOperation::Activate: {
                    auto phase = modules.info(p.id).phase;
                    // A new link is a fresh prolog, distinct from temporary restart.
                    if (phase == ModulePhase::Stopped) {
                        if (original.RoomData.linkedModule == p.id && original.RoomData.linkedRoom)
                            retire(original.RoomData.linkedRoom, true);
                        else
                            modules.deactivateId(p.id);
                    }
                    if (std::find(activated.begin(), activated.end(), p.id) == activated.end())
                        activated.push_back(p.id);
                    modules.activateId(p.id);
                    break;
                }
                case ModuleOperation::Deactivate:
                    if (original.RoomData.linkedModule == p.id && original.RoomData.linkedRoom)
                        retire(original.RoomData.linkedRoom, true);
                    else
                        modules.deactivateId(p.id);
                    break;
                case ModuleOperation::Stop:
                    modules.stopId(p.id);
                    break;
                case ModuleOperation::Restart:
                    modules.restartId(p.id);
                    break;
                case ModuleOperation::ResetRoom:
                case ModuleOperation::RetireRoom:
                case ModuleOperation::ReleaseRoom: {
                    if (p.id > UINT16_MAX)
                        throw std::invalid_argument("room ID truncation");
                    retire(static_cast<u16>(p.id), p.operation == ModuleOperation::RetireRoom);
                    break;
                }
                }
                p.result->status = OperationStatus::Complete;
            } catch (const std::exception &e) {
                p.result->status = OperationStatus::Failed;
                p.result->error = e.what();
            }
            if (p.caller)
                settled.push_back(std::move(p));
        }
    }
};
GameRuntime::GameRuntime(std::shared_ptr<const ImportedFiles> files, PlatformGamepads &pads,
                         u8 disc, std::function<u32()> seconds)
    : impl_(std::make_unique<Impl>(this, std::move(files), pads, disc)) {
    if (seconds)
        impl_->original.seconds = std::move(seconds);
}
GameRuntime::~GameRuntime() {
    impl_->check(true);
    impl_->loader.scheduler().clearTasks();
    impl_->engineConsumers.clear();
    impl_->texRender.clear();
    impl_->cancelPending();
    impl_->messages.clear();
    impl_->subscreen.clear();
}
void GameRuntime::initialize(u16 width, u16 height) {
    impl_->check(true);
    if (!width || !height)
        throw std::invalid_argument("screen dimensions");
    impl_->loader.cancelRoom();
    impl_->loader.cancelAll();
    impl_->loader.scheduler().clearTasks();
    impl_->engineConsumers.clear();
    impl_->texRender.clear();
    for (auto task : impl_->gameTasks)
        impl_->loader.scheduler().kill(task);
    impl_->gameTasks.clear();
    impl_->cancelPending();
    for (auto id : impl_->activated)
        if (impl_->modules.info(id).phase != ModulePhase::Inactive)
            impl_->modules.deactivateId(id);
    impl_->activated.clear();
    impl_->messages.clear();
    impl_->subscreen.clear();
    impl_->heaps->reset();
    impl_->original.reset();
    impl_->original.Rmode = {width, height};
    impl_->original.systemWorkInit();
    // Native arena tail handoff. GC SystemMemInit's absolute memory map is not copied.
    if (!impl_->heaps->replace(0, 1) || !impl_->heaps->setCurrent(1))
        throw std::runtime_error("game heap handoff");
    impl_->subscreen.initializeInventory(); // Original Title_task prerequisite; start items remain in gameInit.
    impl_->original.ScenarioInit();
    impl_->original.PadInit();
    impl_->original.InitGameTime();
    impl_->vi.reset();
    impl_->paused = false;
    impl_->initialized = true;
}
void GameRuntime::pause() {
    impl_->check(true);
    if (!impl_->initialized || impl_->paused)
        return;
    impl_->original.SetGameTime();
    impl_->paused = true;
    impl_->vi.reset();
    for (u8 i = 0; i < 4; ++i)
        impl_->original.pads.rumble(i, false);
}
void GameRuntime::resume() {
    impl_->check(true);
    if (!impl_->initialized)
        throw std::logic_error("uninitialized game runtime");
    if (!impl_->paused)
        return;
    impl_->original.InitGameTime();
    impl_->paused = false;
    impl_->vi.reset();
}
bool GameRuntime::paused() const {
    impl_->check();
    return impl_->paused;
}
void GameRuntime::tick() {
    impl_->check(true);
    if (!impl_->initialized)
        throw std::logic_error("uninitialized game runtime");
    if (impl_->paused)
        return;
    impl_->stepping = true;
    try {
        impl_->apply();
        impl_->original.collectCancelledReads();
        impl_->messages.collectCancelledReads();
        impl_->original.PadRead();
        ++impl_->original.global.Frame_cnt;
        impl_->loader.scheduler().setGameGates(
            nativeFlag(impl_->original.global.Status_flg, STA_SUSPEND) != 0,
            nativeFlag(impl_->original.global.Status_flg, STA_DIEDEMO) != 0);
        impl_->loader.frame();
        impl_->stepping = false;
    } catch (...) {
        impl_->stepping = false;
        throw;
    }
}
u32 GameRuntime::displayFrame(std::int64_t ns) {
    impl_->check(true);
    if (impl_->paused)
        return 0;
    return impl_->vi.onDisplayFrame(ns);
}
NativeGameState &GameRuntime::state() {
    impl_->check();
    return impl_->original.global;
}
const KEY &GameRuntime::key() const {
    impl_->check();
    return impl_->original.Key;
}
const JOY &GameRuntime::joy(u8 port) const {
    impl_->check();
    if (port >= 4)
        throw std::out_of_range("game Joy port");
    return impl_->original.Joy[port];
}
GameplayHeaps &GameRuntime::heaps() {
    impl_->check();
    return *impl_->heaps;
}
EngineLoader &GameRuntime::loader() {
    impl_->check();
    return impl_->loader;
}
ModuleRuntime &GameRuntime::modules() {
    impl_->check();
    return impl_->modules;
}
EngineConsumers &GameRuntime::engineConsumers() {
    impl_->check();
    return impl_->engineConsumers;
}
TexRenderRuntime &GameRuntime::texRender() { impl_->check(); return impl_->texRender; }
GraphicsState &GameRuntime::graphics() { impl_->check(); return impl_->graphics; }
MessageRuntime &GameRuntime::messages() { impl_->check(); return impl_->messages; }
SubScreenRuntime &GameRuntime::subscreen() { impl_->check(); return impl_->subscreen; }
std::shared_ptr<FrameOperation> GameRuntime::queueRoomRetirement(u16 id) {
    return queueModule(ModuleOperation::RetireRoom, id);
}
void GameRuntime::queueFailedRoomRetirement(u16 id) {
    impl_->check();
    impl_->pending.push_back({ModuleOperation::RetireRoom, id, {}, std::make_shared<FrameOperation>(),
                            impl_->modules.info(roomMeta(id).module).epoch});
}
void GameRuntime::retireRoom(u16 id) { awaitModule(ModuleOperation::RetireRoom, id); }
void GameRuntime::invokeRoomEntry(u16 id, bool init) {
    impl_->check();
    auto entry = impl_->modules.room(id);
    if (!entry)
        throw MissingGameConsumer(roomMeta(id).init);
    impl_->modules.invoke(init ? entry->init : entry->main);
}
void GameRuntime::runOpeningTask() {
    impl_->check();
    impl_->original.R120Event();
}
void GameRuntime::bindConsumers(SharedScenarioConsumers *consumer) {
    impl_->check(true);
    if (impl_->loader.scheduler().allocatedStacks())
        throw std::logic_error("consumer rebinding while tasks live");
    impl_->original.consumers = consumer;
}
std::shared_ptr<FrameOperation> GameRuntime::queueModule(ModuleOperation operation, u32 id) {
    impl_->check();
    auto result = std::make_shared<FrameOperation>();
    impl_->pending.push_back({operation, id, impl_->loader.scheduler().current(), result});
    return result;
}
void GameRuntime::awaitModule(ModuleOperation operation, u32 id) {
    impl_->check();
    if (!impl_->loader.scheduler().current())
        throw std::logic_error("module await requires stackful task");
    // Only a borrowed pointer lives on the task stack. The runtime ledger owns
    // the record until this task exits/is killed; abrupt stack release leaks no RAII pin.
    auto *ticket = queueModule(operation, id).get();
    while (ticket->status == OperationStatus::Pending)
        impl_->loader.scheduler().sleep(1);
    if (ticket->status != OperationStatus::Complete)
        throw std::runtime_error(ticket->error);
}
std::optional<TaskHandle> GameRuntime::sceExec(int level, TaskEntry entry, u8 kind, int priority,
                                               void *model, ModuleToken module) {
    impl_->check();
    return impl_->loader.scheduler().execScenario(level, std::move(entry), kind, priority, model,
                                                  module);
}
void GameRuntime::scenarioRoomInit() {
    impl_->check();
    impl_->original.ScenarioRoomInit();
}
void GameRuntime::scenarioMove() {
    impl_->check();
    impl_->original.ScenarioMove();
}
void GameRuntime::stageSet() {
    impl_->check();
    auto &room = impl_->original.RoomData;
    if (room.linkedModule && room.linkedRoom != impl_->original.global.room_id) {
        const auto &next = roomMeta(impl_->original.global.room_id);
        awaitModule(next.module == room.linkedModule ? ModuleOperation::ReleaseRoom
                                                     : ModuleOperation::RetireRoom,
                    room.linkedRoom);
    }
    impl_->original.StageSet();
}
void GameRuntime::doorFlagInit() {
    impl_->check();
    impl_->original.DoorFlagInit();
}
void GameRuntime::initGameTime() {
    impl_->check();
    impl_->original.InitGameTime();
}
void GameRuntime::setGameTime() {
    impl_->check();
    impl_->original.SetGameTime();
}
u32 GameRuntime::gameTime(u32 *h, u32 *m, u32 *s) {
    impl_->check();
    if (impl_->paused) {
        auto sec = impl_->original.global.play_time;
        impl_->original.SecToTime(sec, h, m, s);
        return sec;
    }
    return impl_->original.GetGameTime(h, m, s);
}
std::optional<TaskHandle> GameRuntime::startOriginalGameTask(int slot) {
    impl_->check();
    auto task = impl_->loader.scheduler().exec(slot, [this] { impl_->original.GameTask(); });
    if (task)
        impl_->gameTasks.push_back(*task);
    return task;
}
int GameRuntime::enemyListNumber(u16 room) {
    impl_->check();
    return impl_->original.checkEmListNo(room);
}
std::string GameRuntime::enemyListName(u32 index) {
    impl_->check();
    return impl_->original.getEmListName(index);
}
void GameRuntime::readEnemyList(int mode) {
    impl_->check();
    if (mode != 0 && mode != 1)
        throw std::invalid_argument("original ESL read mode");
    if (!impl_->loader.scheduler().current())
        throw std::logic_error("readEmList requires stackful task");
    impl_->original.readEmList(mode);
}
const std::vector<std::string> &GameRuntime::trace() const {
    impl_->check();
    return impl_->original.trace;
}
} // namespace re4::port
