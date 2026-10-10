#pragma once
#include "re4/port/consumers.h"
#include "re4/port/contracts.h"
#include "re4/port/modules.h"
#include "re4_runtime_types.h"
#include <array>
#include <thread>

namespace re4::port {
class MissingGameConsumer final : public std::runtime_error {
  public:
    explicit MissingGameConsumer(const std::string &function)
        : std::runtime_error("Original consumer unavailable: " + function) {}
};

// Required shared consumers of the original ScenarioRoomInit/ScenarioMove/StageSet.
// No default success behavior. Production registration requires the actual engine.
// Test doubles must be described as isolated adapter tests.
class SharedScenarioConsumers {
  public:
    virtual ~SharedScenarioConsumers() = default;
    virtual void SceAtSetSaveItem() = 0;
    virtual void SceExecInitCondition() = 0;
    virtual void SceExecCheckCondition() = 0;
    virtual void SubCharInit(int, Vec *, float) = 0;
    virtual void SubCharCtrl(int, int) = 0;
    virtual void EmSetFromList() = 0;
    virtual void SceAtRoomSet() = 0;
    virtual void SubMissionCheck() = 0;
    virtual void SceAtCheckMoveScrAt() = 0;
    virtual int scenarioCheckEventCancel() = 0;
    virtual void EventRun() = 0;
    virtual void MessageStageInit() = 0;
};

// Native arena boundaries use size_t, never GameCube absolute addresses. Heap 13
// is a selector for the current heap, not a fourteenth heap. Leases pin storage.
class GameplayHeaps final {
  public:
    struct Allocation {
        std::shared_ptr<void> owner;
        void *data = nullptr;
        std::size_t bytes = 0;
        explicit operator bool() const { return data != nullptr; }
    };
    explicit GameplayHeaps(std::size_t bytes = 8 * 1024 * 1024);
    ~GameplayHeaps();
    GameplayHeaps(const GameplayHeaps &) = delete;
    GameplayHeaps &operator=(const GameplayHeaps &) = delete;
    Allocation allocate(std::size_t bytes, int heap = 13, bool clear = false);
    bool replace(int from, int to);
    bool setCurrent(int heap);
    int current() const;
    void suspend(int heap);
    void resume(int heap);
    bool active(int heap) const;
    void reset(); // New arena; outstanding leases keep the retired arena alive.
    std::size_t liveBytes() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Platform adapter, separate from original PadRead. API axis values normalized
// by Android's actual MotionRange. Original PADClamp supplies game deadzones.
class ControllerInput final : public PlatformGamepads {
  public:
    void connect(int device, std::uint8_t port, bool analogLeft = true, bool analogRight = true);
    void disconnect(int device);
    bool button(int device, int androidKeyCode, bool down);
    bool axes(int device, float x, float y, float z, float rz, float hatX, float hatY,
              float leftTrigger, float rightTrigger);
    void clearHeld();
    OriginalPadSample sample(std::uint8_t port) override;
    void rumble(std::uint8_t port, bool enabled) override;
    bool requestedRumble(std::uint8_t port) const;
    // A requested motor state is not a claim of available Android haptic hardware.
  private:
    struct Port {
        int device = -1;
        std::uint16_t buttons = 0, hats = 0;
        float x = 0, y = 0, z = 0, rz = 0, lt = 0, rt = 0;
        bool l2 = false, r2 = false, motor = false;
        bool analogLeft = true, analogRight = true;
    };
    std::array<Port, 4> ports_{};
    const std::thread::id owner_ = std::this_thread::get_id();
    void checkOwner() const;
    Port *find(int device);
};

struct OriginalContext;
class PadRuntime final {
  public:
    explicit PadRuntime(PlatformGamepads &);
    ~PadRuntime();
    void tick(int systemVcnt = 2);
    void clear();
    const JOY &joy(std::uint8_t port = 0) const;
    const KEY &key() const;
    NativeGameState &state();
    void stopKeys(std::uint64_t permitted);

  private:
    std::unique_ptr<OriginalContext> original_;
};

enum class ModuleOperation {
    Activate,
    Deactivate,
    Stop,
    Restart,
    ResetRoom,
    RetireRoom,
    ReleaseRoom
};
enum class OperationStatus { Pending, Complete, Cancelled, Failed };
struct FrameOperation {
    OperationStatus status = OperationStatus::Pending;
    std::string error;
};
class GameRuntime final {
  public:
    GameRuntime(std::shared_ptr<const ImportedFiles>, PlatformGamepads &, std::uint8_t disc = 0,
                std::function<std::uint32_t()> seconds = {});
    ~GameRuntime();
    GameRuntime(const GameRuntime &) = delete;
    GameRuntime &operator=(const GameRuntime &) = delete;
    void initialize(std::uint16_t screenWidth = 640, std::uint16_t screenHeight = 480);
    void pause();
    void resume();
    bool paused() const;
    void tick();
    std::uint32_t displayFrame(std::int64_t nanoseconds);
    NativeGameState &state();
    const KEY &key() const;
    const JOY &joy(std::uint8_t port = 0) const;
    GameplayHeaps &heaps();
    EngineLoader &loader();
    ModuleRuntime &modules();
    EngineConsumers &engineConsumers();
    std::shared_ptr<FrameOperation> queueRoomRetirement(std::uint16_t room);
    void
    retireRoom(std::uint16_t room); // Stackful boundary adapter, never a replacement room script.
    void invokeRoomEntry(std::uint16_t room, bool init);
    void runOpeningTask(); // Diagnostic invocation of actual R120Event; no replacement intro.
    void bindConsumers(SharedScenarioConsumers *); // Borrowed, must outlive all tasks.
    std::shared_ptr<FrameOperation> queueModule(ModuleOperation, std::uint32_t id);
    void awaitModule(ModuleOperation, std::uint32_t id); // Stackful, task only.
    std::optional<TaskHandle> sceExec(int level, TaskEntry, std::uint8_t kind = 0, int priority = 2,
                                      void *model = nullptr, ModuleToken module = {});
    void scenarioRoomInit();
    void scenarioMove();
    void stageSet();
    void doorFlagInit();
    void initGameTime();
    void setGameTime();
    std::uint32_t gameTime(std::uint32_t *, std::uint32_t *, std::uint32_t *);
    // Real GameTask reset/dispatch. Missing gameDebug/gameInit/etc fail explicitly;
    // this entry cannot pretend to complete the unported full initialization.
    std::optional<TaskHandle> startOriginalGameTask(int slot = 0);
    int enemyListNumber(std::uint16_t room);
    std::string enemyListName(std::uint32_t index);
    void readEnemyList(int mode = 1); // Actual original readEmList; stackful task required.
    const std::vector<std::string> &trace() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
std::string verifyGameRuntime(const std::string &fixtureDirectory);
} // namespace re4::port
