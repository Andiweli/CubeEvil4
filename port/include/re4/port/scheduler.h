#pragma once
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace re4::port {

// Native identities, never serialized into the original 32-bit data layouts.
struct TaskHandle {
    std::uint64_t runtime = 0;
    std::uint64_t generation = 0;
    std::uint8_t slot = 0;
};
inline bool operator==(TaskHandle a, TaskHandle b) {
    return a.runtime == b.runtime && a.generation == b.generation && a.slot == b.slot;
}
struct ModuleToken {
    std::uint64_t runtime = 0;
    std::uint64_t generation = 0;
};
inline bool operator==(ModuleToken a, ModuleToken b) {
    return a.runtime == b.runtime && a.generation == b.generation;
}

using TaskEntry = std::function<void()>;
// Bind the actual callback type. In particular, pointer arguments never pass
// through int, and an integer callback receives an integer, not a cast pointer.
template<class Function, class... Args>
TaskEntry bindTask(Function function, Args... args) {
    if constexpr (std::is_pointer_v<Function>)
        if (!function) throw std::invalid_argument("Null typed callback");
    return [function, args...] { std::invoke(function, args...); };
}

struct TaskState {
    std::uint8_t status;
    std::uint8_t priority; // Original OS priority; does not reorder main slots.
    std::uint8_t kind;
    std::uint16_t sleepVisits;
    std::uint16_t suspendCount;
    void* model;
};

struct BackgroundBudget {
    std::uint32_t workUnits = 1024 * 1024;
    // Zero disables the clock limit, useful for deterministic verification.
    std::chrono::nanoseconds timeLimit = std::chrono::milliseconds(2);
};
enum class BackgroundOutcome { Completed, Cancelled, Failed };
// Runs on the owner stack after task stack/captures have been released. Must
// not throw or resubmit into the releasing slot. Idle is published AFTER this
// callback returns; it publishes owned results and never runs on a worker.
using BackgroundCompletion = std::function<void(BackgroundOutcome)>;

class NativeScheduler final {
public:
    // A single owner thread, with independent stacks; no worker thread per task.
    explicit NativeScheduler(std::size_t stackBytes = 256 * 1024);
    ~NativeScheduler();
    NativeScheduler(const NativeScheduler&) = delete;
    NativeScheduler& operator=(const NativeScheduler&) = delete;

    // Main callbacks must exit/chain, as in the original; unexpected return is
    // reported as an error. Scenario callbacks may return (original OT cleanup).
    std::optional<TaskHandle> exec(int slot, TaskEntry entry, ModuleToken module = {});
    std::optional<TaskHandle> execScenario(int level, TaskEntry entry,
        std::uint8_t kind = 0, int orderingPriority = 2, void* model = nullptr,
        ModuleToken module = {});
    void runMain(); // Slots 0,1,2,3, once per visit.
    std::optional<TaskHandle> execBackground(TaskEntry entry, ModuleToken module = {},
        BackgroundCompletion completion = {});
    // Frame tail, after gameplay and DVD/audio watchers (original main.cpp).
    void runBackground(BackgroundBudget budget = {});
    // Required in bounded decoder/poll loops; preserves the entire task stack.
    void backgroundCheckpoint(std::uint32_t workUnits = 1);
    [[noreturn]] void exitBackground();
    bool killBackground();
    bool backgroundActive() const noexcept;
    // The following requests/status are the ONLY cross-thread APIs. The runtime
    // must outlive callers. Generation checks prevent cancelling a replacement.
    void requestBackgroundSuspend() noexcept;
    bool requestBackgroundCancel(TaskHandle task) noexcept;
    void runScenarios(); // Reverse OT, LIFO in each bucket, restart at head.
    void visit(TaskHandle task); // Equivalent of TaskSchedulerMain for one task.
    void sleep(int visits);
    [[noreturn]] void exit();
    [[noreturn]] void chain(TaskEntry entry, ModuleToken module = {});
    bool kill(TaskHandle task);
    void killScenarios(); // ScenarioTaskAllOff: slots 5..17, generation-safe.
    void clearTasks(); // Owner frame boundary: release all stackful tasks on reset.
    bool detachScenario(TaskHandle task); // SceTaskDelete adapter: unlink, keep stack.
    bool suspend(TaskHandle task);
    bool signal(TaskHandle task);
    bool setKind(TaskHandle task, std::uint8_t kind);
    bool setPriority(TaskHandle task, std::uint8_t priority);
    std::optional<TaskState> state(TaskHandle task) const;
    std::optional<TaskHandle> current() const;
    std::size_t allocatedStacks() const;

    void setGameGates(bool suspend, bool diedemo);
    void setScenarioGates(bool itemOrChapterHold, bool scenarioStopped);
    ModuleToken createModuleLifetime();
    // False while any owned task is on the active call chain. On true, all its
    // suspended/pending tasks are destroyed and future entries are rejected.
    // Only after true may a registry release code/data. No loader is simulated.
    bool invalidateModule(ModuleToken module);
    // A nested module call can sleep on a task owned by a different module.
    // Borrowed lifetimes live in scheduler metadata, so abrupt cancellation or
    // TaskChain cannot leak a stack-local RAII pin. No function-pointer casts.
    void callModule(ModuleToken module, const TaskEntry &entry, void *context = nullptr);
    void *moduleCallContext() const;
    bool canInvalidateModules(const std::vector<ModuleToken> &modules) const;
    bool invalidateModules(const std::vector<ModuleToken> &modules);
    bool pauseModule(ModuleToken module);
    bool resumeModule(ModuleToken module);

    static NativeScheduler& active();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
