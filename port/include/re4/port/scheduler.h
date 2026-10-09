#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

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
    void runMain(); // Slots 0,1,2,3, once per visit; slot 4 is deliberately rejected.
    void runScenarios(); // Reverse OT, LIFO in each bucket, restart at head.
    void visit(TaskHandle task); // Equivalent of TaskSchedulerMain for one task.
    void sleep(int visits);
    [[noreturn]] void exit();
    [[noreturn]] void chain(TaskEntry entry, ModuleToken module = {});
    bool kill(TaskHandle task);
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

    static NativeScheduler& active();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
