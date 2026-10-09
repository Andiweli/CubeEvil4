#include "re4/port/scheduler.h"
#include "re4_scheduler_original.h"
#include "stack_context.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <exception>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace re4::port::original {
thread_local TASK Task[TASK_NUM];
static thread_local TASK mainTask;
thread_local TASK* CTASK_MAIN = &mainTask; // Native sentinel, never a wire address.
thread_local TASK* pCTask = CTASK_MAIN;
thread_local cSceSys SceSys;
void TaskSleep(int ctr) { if (ctr != 0) NativeScheduler::active().sleep(ctr); }
[[noreturn]] void TaskExit() { NativeScheduler::active().exit(); }
void SceTaskDelete(TASK* task) {
    auto& scheduler = NativeScheduler::active();
    const auto handle = scheduler.current();
    if (!handle || task != &Task[handle->slot])
        throw std::logic_error("SceTaskDelete adapter requires the current task");
    scheduler.detachScenario(*handle);
}
std::optional<TaskHandle> iTaskExec(TaskEntry entry, ModuleToken module, BackgroundCompletion completion) {
    return NativeScheduler::active().execBackground(std::move(entry), module, std::move(completion));
}
void iTaskScheduler(BackgroundBudget budget) { NativeScheduler::active().runBackground(budget); }
void iTaskSuspend() { NativeScheduler::active().requestBackgroundSuspend(); }
void iTaskKill() { NativeScheduler::active().killBackground(); }
[[noreturn]] void iTaskExit() { NativeScheduler::active().exitBackground(); }
int iTaskStatus() { return NativeScheduler::active().backgroundActive() ? 1 : 0; }
} // namespace re4::port::original

namespace re4::port {
namespace {
thread_local NativeScheduler* activeScheduler = nullptr;
std::atomic<std::uint64_t> nextRuntime{1};
}
struct NativeScheduler::Impl {
    struct Slot {
        Impl* owner = nullptr;
        int index = 0;
        std::uint64_t generation = 0;
        ModuleToken module;
        TaskEntry entry;
        TaskEntry replacement;
        std::unique_ptr<detail::StackContext> context;
        std::exception_ptr failure;
        bool executing = false;
        bool linked = false;
        bool scenarioEntry = false;
        bool executedThisPass = false;
        bool releasing = false;
        BackgroundCompletion completion;
        BackgroundOutcome outcome = BackgroundOutcome::Cancelled;
    };
    detail::ContextDomain domain;
    const std::thread::id thread = std::this_thread::get_id();
    const std::uint64_t identity = nextRuntime.fetch_add(1);
    std::uint64_t nextGeneration = 1;
    std::uint64_t nextModule = 1;
    const std::size_t stackBytes;
    std::array<Slot, 18> slots;
    std::array<std::vector<TaskHandle>, 16> order;
    std::vector<ModuleToken> modules;
    std::vector<ModuleToken> directModules;
    bool gameSuspend = false, diedemo = false;
    bool scenarioHold = false, scenarioStop = false, inScenarioPass = false;
    int current = -1;
    std::atomic<std::uint64_t> backgroundGeneration{0}, runningBackground{0};
    std::atomic<std::uint64_t> pauseBackground{0}, cancelBackground{0};
    BackgroundBudget backgroundBudget;
    std::uint64_t usedBackgroundUnits = 0;
    std::chrono::steady_clock::time_point backgroundDeadline;

    explicit Impl(std::size_t bytes) : stackBytes(bytes) {
        if (activeScheduler) throw std::logic_error("Only one original task domain per owner thread");
        if (bytes < 64 * 1024 || bytes > 16 * 1024 * 1024)
            throw std::invalid_argument("Native stack size must be 64 KiB..16 MiB");
        if (!identity) throw std::overflow_error("Scheduler identity exhausted");
        for (int i = 0; i < 18; ++i) {
            slots[i].owner = this;
            slots[i].index = i;
            original::Task[i] = original::TASK{};
            original::Task[i].Task_no = static_cast<std::uint8_t>(i);
        }
        original::pCTask = original::CTASK_MAIN;
    }
    void checkThread() const {
        if (thread != std::this_thread::get_id())
            throw std::logic_error("Scheduler contexts cannot migrate between threads");
    }
    bool moduleValid(ModuleToken module) const {
        if (module == ModuleToken{}) return true; // Resident core code.
        return module.runtime == identity && std::find(modules.begin(), modules.end(), module) != modules.end();
    }
    TaskHandle handle(const Slot& slot) const {
        return {identity, slot.generation, static_cast<std::uint8_t>(slot.index)};
    }
    Slot* resolve(TaskHandle task) {
        if (task.runtime != identity || task.slot >= slots.size()) return nullptr;
        auto& slot = slots[task.slot];
        return slot.generation == task.generation && task.generation && slot.entry ? &slot : nullptr;
    }
    const Slot* resolve(TaskHandle task) const { return const_cast<Impl*>(this)->resolve(task); }
    void unlink(Slot& slot) {
        if (!slot.linked) return;
        const auto task = handle(slot);
        for (auto& bucket : order)
            bucket.erase(std::remove(bucket.begin(), bucket.end(), task), bucket.end());
        slot.linked = false;
    }
    void release(Slot& slot) {
        // Always executed by a different, still-live stack after control returns.
        if (slot.executing) throw std::logic_error("Attempted to free an active stack");
        slot.releasing = true;
        const auto outcome = slot.outcome;
        auto completion = std::move(slot.completion);
        // libc++ may COPY a small callable during std::function's move, leaving
        // the source populated. Explicitly clear to release captured buffers
        // exactly once on both Android ABIs, not just MSVC hosts.
        slot.completion = {};
        unlink(slot);
        slot.context.reset();
        slot.entry = {};
        slot.replacement = {};
        slot.failure = {};
        original::Task[slot.index].Status = original::TASK_NONE;
        original::Task[slot.index].suspend_cnt = 0;
        if (completion) {
            try { completion(outcome); } catch (...) { std::terminate(); }
        }
        completion = {};
        // Publish idle only AFTER result/cancellation publication and capture
        // destruction. An acquire observer seeing idle cannot see an unfinished
        // completion callback. Keep the slot reserved throughout that callback.
        if (slot.index == 4) backgroundGeneration.store(0, std::memory_order_release);
        slot.releasing = false;
    }
    static void start(void* argument) {
        auto& slot = *static_cast<Slot*>(argument);
        auto& task = original::Task[slot.index];
        try {
            slot.entry();
            // cSceSys::scheduler explicitly removes returned scenario tasks.
            // Main TaskExec_hook instead leaves its parent suspended on return:
            // such a callback violates the original main-task contract. Report
            // it instead of silently inventing TaskExit or deadlocking Android.
            if (!slot.scenarioEntry)
                throw std::logic_error("Main task returned without TaskExit or TaskChain");
        } catch (...) {
            // Exceptions unwind only this task's own frames, then are reported
            // on its caller's stack. No exception crosses an assembly switch.
            slot.failure = std::current_exception();
            slot.outcome = BackgroundOutcome::Failed;
        }
        task.Status = original::TASK_NONE;
        task.suspend_cnt = 0;
        slot.context->yield();
        std::terminate();
    }
    void visit(Slot& slot) {
        auto& task = original::Task[slot.index];
        if (slot.executing) throw std::logic_error("A task cannot resume itself or an active ancestor");
        // The checks, exact status switch, and 16-bit decrement follow the
        // original TaskSchedulerMain. Sleep counts visits, not wall-clock time.
        if (gameSuspend && !task.isNoSuspend()) return;
        if (diedemo && !task.isDiedemoMove()) return;
        switch (task.Status) {
        case original::TASK_EXEC:
            slot.context = std::make_unique<detail::StackContext>(stackBytes, start, &slot);
            task.Status = original::TASK_RUN;
            break;
        case original::TASK_SLEEP:
            --task.SleepCtr;
            if (task.SleepCtr != 0) return;
            task.Status = original::TASK_RUN;
            break;
        case original::TASK_RUN:
            break;
        default: return; // TASK_SUSPEND bit prevents all switch cases.
        }
        const int parent = current;
        auto* parentTask = original::pCTask;
        current = slot.index;
        original::pCTask = &task;
        slot.executing = true;
        if (slot.index == 4) runningBackground.store(slot.generation, std::memory_order_release);
        slot.context->resume();
        if (slot.index == 4) runningBackground.store(0, std::memory_order_release);
        slot.executing = false;
        current = parent;
        original::pCTask = parentTask;
        if (task.Status == original::TASK_EXEC) {
            // TaskChain: discard the old stack before starting the replacement
            // on a future visit. Preserve kind, model, priority and OT position.
            slot.context.reset();
            slot.entry = std::move(slot.replacement);
            slot.replacement = {};
        } else if (task.Status == original::TASK_NONE) {
            const auto failure = slot.failure;
            release(slot);
            if (failure) std::rethrow_exception(failure);
        }
    }
};

NativeScheduler::NativeScheduler(std::size_t bytes) : impl_(std::make_unique<Impl>(bytes)) {
    activeScheduler = this;
}
NativeScheduler::~NativeScheduler() {
    if (impl_->thread != std::this_thread::get_id() || impl_->current != -1) std::terminate();
    for (auto& slot : impl_->slots) impl_->release(slot);
    original::pCTask = original::CTASK_MAIN;
    activeScheduler = nullptr;
}
NativeScheduler& NativeScheduler::active() {
    if (!activeScheduler) throw std::logic_error("No original task domain on this thread");
    return *activeScheduler;
}
std::optional<TaskHandle> NativeScheduler::exec(int index, TaskEntry entry, ModuleToken module) {
    impl_->checkThread();
    if (index < 0 || index >= 18) throw std::out_of_range("Original task slot");
    if (index == 4) throw std::logic_error("Slot 4 requires execBackground and bounded checkpoints");
    if (!entry) throw std::invalid_argument("Null task entry");
    if (!impl_->moduleValid(module)) return std::nullopt;
    auto& slot = impl_->slots[index];
    if (original::TaskStatus(index) != 0 || slot.entry || slot.releasing) return std::nullopt;
    if (impl_->nextGeneration == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Task generation exhausted");
    slot.entry = std::move(entry);
    slot.generation = impl_->nextGeneration++;
    slot.module = module;
    slot.scenarioEntry = false;
    auto& task = original::Task[index];
    task.Status = original::TASK_EXEC;
    task.Priority = 0xF;
    task.setKind(6);
    return impl_->handle(slot);
}
std::optional<TaskHandle> NativeScheduler::execScenario(int level, TaskEntry entry,
    std::uint8_t kind, int priority, void* model, ModuleToken module) {
    impl_->checkThread();
    if (!entry) throw std::invalid_argument("Null scenario entry");
    if (!impl_->moduleValid(module)) return std::nullopt;
    if (level == 0) {
        // A direct invocation shares its caller's lifetime and stack. Cross-
        // module direct calls must remain resident until call-graph ownership
        // has been integrated with the real module registry.
        if (impl_->current >= 0 && !(module == ModuleToken{}) && !(module == impl_->slots[impl_->current].module))
            throw std::logic_error("Direct scenario call cannot change its task's module lifetime");
        original::SetTaskModelPtr(model, nullptr);
        if (impl_->current >= 0) { entry(); return std::nullopt; }
        impl_->directModules.push_back(module);
        try { entry(); } catch (...) { impl_->directModules.pop_back(); throw; }
        impl_->directModules.pop_back();
        return std::nullopt;
    }
    if (priority < 0 || priority >= 16) throw std::out_of_range("Scenario ordering priority");
    // Preserve the unsigned comparison and the original auto-allocation loop:
    // slots 17..6, NOT slot 5 (reserved for item/chapter tasks).
    if (static_cast<std::uint32_t>(level) > 17) {
        level = 0;
        for (int i = 17; i > 5; --i) if (original::TaskStatus(i) == 0) { level = i; break; }
    }
    if (level < 5 || level > 17) return std::nullopt;
    auto handle = exec(level, std::move(entry), module);
    if (!handle) return std::nullopt;
    auto& slot = impl_->slots[level];
    try { impl_->order[priority].insert(impl_->order[priority].begin(), *handle); }
    catch (...) { impl_->release(slot); throw; }
    slot.linked = true;
    slot.scenarioEntry = true;
    auto& task = original::Task[level];
    task.Priority = 0xE;
    original::SetTaskModelPtr(model, &task);
    if (kind != 0) task.setKind(kind);
    else if (!original::SceSys.checkCTaskRange()) task.setKind(1);
    else task.setKind(original::pCTask->getKind());
    return handle;
}
void NativeScheduler::runMain() {
    impl_->checkThread();
    if (impl_->current != -1) throw std::logic_error("TaskScheduler requires its main caller");
    for (int i = 0; i < 4; ++i) if (impl_->slots[i].entry) impl_->visit(impl_->slots[i]);
}
std::optional<TaskHandle> NativeScheduler::execBackground(TaskEntry entry, ModuleToken module,
    BackgroundCompletion completion) {
    impl_->checkThread();
    if (!entry) throw std::invalid_argument("Null background entry");
    auto& slot = impl_->slots[4];
    if (slot.entry || slot.releasing || !impl_->moduleValid(module)) return std::nullopt;
    if (impl_->nextGeneration == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Task generation exhausted");
    slot.entry = std::move(entry);
    slot.generation = impl_->nextGeneration++;
    slot.module = module;
    slot.scenarioEntry = false;
    slot.completion = std::move(completion);
    slot.outcome = BackgroundOutcome::Cancelled;
    auto& task = original::Task[4];
    task = original::TASK{};
    task.Task_no = 4;
    task.Status = original::TASK_EXEC;
    task.Priority = 0; // iTaskExec sets Priority to the previous inactive flag.
    task.setKind(6);
    impl_->pauseBackground.store(0, std::memory_order_relaxed);
    impl_->cancelBackground.store(0, std::memory_order_relaxed);
    impl_->backgroundGeneration.store(slot.generation, std::memory_order_release);
    return impl_->handle(slot);
}
void NativeScheduler::runBackground(BackgroundBudget budget) {
    impl_->checkThread();
    if (impl_->current != -1) throw std::logic_error("iTaskScheduler requires the frame tail");
    if (!budget.workUnits || budget.timeLimit.count() < 0 || budget.timeLimit > std::chrono::seconds(1))
        throw std::invalid_argument("Invalid background budget (maximum one second)");
    auto& slot = impl_->slots[4];
    if (!slot.entry) return;
    if (impl_->cancelBackground.load(std::memory_order_acquire) == slot.generation) {
        impl_->release(slot); return;
    }
    // Original iTaskScheduler clears the retrace suspension every frame.
    original::Task[4].Status &= ~original::TASK_SUSPEND;
    impl_->pauseBackground.store(0, std::memory_order_relaxed);
    impl_->backgroundBudget = budget;
    impl_->usedBackgroundUnits = 0;
    impl_->backgroundDeadline = std::chrono::steady_clock::now() + budget.timeLimit;
    impl_->visit(slot);
}
void NativeScheduler::backgroundCheckpoint(std::uint32_t units) {
    impl_->checkThread();
    if (impl_->current != 4) throw std::logic_error("Background checkpoint requires slot 4");
    auto& slot = impl_->slots[4];
    if (impl_->cancelBackground.load(std::memory_order_acquire) == slot.generation) {
        slot.outcome = BackgroundOutcome::Cancelled;
        exit();
    }
    impl_->usedBackgroundUnits += units;
    const auto budget = impl_->backgroundBudget;
    if (impl_->usedBackgroundUnits >= budget.workUnits ||
        impl_->pauseBackground.load(std::memory_order_acquire) == slot.generation ||
        (budget.timeLimit.count() && std::chrono::steady_clock::now() >= impl_->backgroundDeadline)) {
        original::Task[4].Status |= original::TASK_SUSPEND;
        slot.context->yield();
        // Also observe requests arriving while the stack was suspended.
        if (impl_->cancelBackground.load(std::memory_order_acquire) == slot.generation) {
            slot.outcome = BackgroundOutcome::Cancelled;
            exit();
        }
    }
}
[[noreturn]] void NativeScheduler::exitBackground() {
    impl_->checkThread();
    if (impl_->current != 4) throw std::logic_error("iTaskExit requires slot 4");
    impl_->slots[4].outcome = BackgroundOutcome::Completed;
    exit();
}
bool NativeScheduler::killBackground() {
    impl_->checkThread();
    auto& slot = impl_->slots[4];
    if (!slot.entry) return false;
    return kill(impl_->handle(slot));
}
bool NativeScheduler::backgroundActive() const noexcept {
    return impl_->backgroundGeneration.load(std::memory_order_acquire) != 0;
}
void NativeScheduler::requestBackgroundSuspend() noexcept {
    const auto generation = impl_->runningBackground.load(std::memory_order_acquire);
    if (!generation) return;
    auto prior = impl_->pauseBackground.load(std::memory_order_acquire);
    while (prior < generation && !impl_->pauseBackground.compare_exchange_weak(
        prior, generation, std::memory_order_release, std::memory_order_acquire)) {}
}
bool NativeScheduler::requestBackgroundCancel(TaskHandle task) noexcept {
    if (task.runtime != impl_->identity || task.slot != 4 || !task.generation ||
        impl_->backgroundGeneration.load(std::memory_order_acquire) != task.generation) return false;
    // Only publish the observed generation, never target the next occupant.
    auto prior = impl_->cancelBackground.load(std::memory_order_acquire);
    // Monotonic generations: a late old request cannot overwrite a newer one.
    while (prior < task.generation && !impl_->cancelBackground.compare_exchange_weak(
        prior, task.generation, std::memory_order_release, std::memory_order_acquire)) {}
    return true;
}
void NativeScheduler::runScenarios() {
    impl_->checkThread();
    if (impl_->inScenarioPass) throw std::logic_error("Recursive scenario ordering pass");
    impl_->inScenarioPass = true;
    try {
        for (int i = 5; i < 18; ++i) impl_->slots[i].executedThisPass = impl_->scenarioHold && i != 5;
        while (!impl_->scenarioStop) {
            std::optional<TaskHandle> next;
            for (int priority = 15; priority >= 0 && !next; --priority) {
                for (const auto handle : impl_->order[priority]) {
                    const auto* slot = impl_->resolve(handle);
                    if (slot && !slot->executedThisPass) { next = handle; break; }
                }
            }
            if (!next) break;
            auto* slot = impl_->resolve(*next);
            slot->executedThisPass = true;
            impl_->visit(*slot);
            // Start at the head after every task, as cSceSys::scheduler does.
        }
    } catch (...) { impl_->inScenarioPass = false; throw; }
    impl_->inScenarioPass = false;
}
void NativeScheduler::visit(TaskHandle handle) {
    impl_->checkThread();
    if (handle.slot == 4) throw std::logic_error("Slot 4 requires a frame-tail budget");
    if (auto* slot = impl_->resolve(handle)) impl_->visit(*slot);
}
void NativeScheduler::sleep(int visits) {
    impl_->checkThread();
    if (visits == 0) return;
    if (impl_->current < 0) throw std::logic_error("TaskSleep requires a task stack");
    auto& slot = impl_->slots[impl_->current];
    auto& task = original::Task[slot.index];
    task.SleepCtr = static_cast<std::uint16_t>(visits);
    task.Status = (task.Status & original::TASK_SUSPEND) | original::TASK_SLEEP;
    slot.context->yield();
}
[[noreturn]] void NativeScheduler::exit() {
    impl_->checkThread();
    if (impl_->current < 0) throw std::logic_error("TaskExit requires a task stack");
    auto& slot = impl_->slots[impl_->current];
    impl_->unlink(slot);
    original::Task[slot.index].Status = original::TASK_NONE;
    original::Task[slot.index].suspend_cnt = 0;
    slot.context->yield();
    std::terminate();
}
[[noreturn]] void NativeScheduler::chain(TaskEntry entry, ModuleToken module) {
    impl_->checkThread();
    if (impl_->current < 0 || !entry) throw std::logic_error("TaskChain requires a task and non-null entry");
    auto& slot = impl_->slots[impl_->current];
    if (slot.index == 4) throw std::logic_error("Background work cannot TaskChain");
    if (module == ModuleToken{}) module = slot.module;
    if (!impl_->moduleValid(module)) throw std::logic_error("TaskChain targets an expired module");
    slot.replacement = std::move(entry);
    slot.module = module;
    original::Task[slot.index].Status = original::TASK_EXEC;
    slot.context->yield();
    std::terminate();
}
bool NativeScheduler::kill(TaskHandle task) {
    impl_->checkThread();
    auto* slot = impl_->resolve(task);
    if (!slot) return false;
    slot->outcome = BackgroundOutcome::Cancelled;
    if (slot->executing) {
        if (slot->index == impl_->current) exit();
        return false; // Never destroy a suspended caller stack of the current task.
    }
    impl_->release(*slot);
    return true;
}
bool NativeScheduler::detachScenario(TaskHandle task) {
    impl_->checkThread();
    auto* slot = impl_->resolve(task);
    if (!slot) return false;
    impl_->unlink(*slot);
    return true;
}
bool NativeScheduler::suspend(TaskHandle task) {
    impl_->checkThread();
    if (!impl_->resolve(task)) return false;
    original::TaskSuspend(task.slot);
    return true;
}
bool NativeScheduler::signal(TaskHandle task) {
    impl_->checkThread();
    if (!impl_->resolve(task)) return false;
    original::TaskSignal(task.slot);
    return true;
}
bool NativeScheduler::setKind(TaskHandle task, std::uint8_t kind) {
    impl_->checkThread();
    if (!impl_->resolve(task)) return false;
    original::Task[task.slot].setKind(kind);
    return true;
}
bool NativeScheduler::setPriority(TaskHandle task, std::uint8_t priority) {
    impl_->checkThread();
    if (!impl_->resolve(task)) return false;
    original::Task[task.slot].Priority = priority;
    return true;
}
std::optional<TaskState> NativeScheduler::state(TaskHandle task) const {
    impl_->checkThread();
    if (!impl_->resolve(task)) return std::nullopt;
    auto& value = original::Task[task.slot];
    return TaskState{value.Status, value.Priority, value.getKind(), value.SleepCtr, value.suspend_cnt, value.pModel};
}
std::optional<TaskHandle> NativeScheduler::current() const {
    impl_->checkThread();
    if (impl_->current < 0) return std::nullopt;
    return impl_->handle(impl_->slots[impl_->current]);
}
std::size_t NativeScheduler::allocatedStacks() const {
    impl_->checkThread();
    std::size_t count = 0;
    for (const auto& slot : impl_->slots) if (slot.context) ++count;
    return count;
}
void NativeScheduler::setGameGates(bool suspend, bool diedemo) {
    impl_->checkThread(); impl_->gameSuspend = suspend; impl_->diedemo = diedemo;
}
void NativeScheduler::setScenarioGates(bool hold, bool stop) {
    impl_->checkThread(); impl_->scenarioHold = hold; impl_->scenarioStop = stop;
}
ModuleToken NativeScheduler::createModuleLifetime() {
    impl_->checkThread();
    if (impl_->nextModule == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Module lifetime exhausted");
    ModuleToken token{impl_->identity, impl_->nextModule++};
    impl_->modules.push_back(token);
    return token;
}
bool NativeScheduler::invalidateModule(ModuleToken module) {
    impl_->checkThread();
    if (module == ModuleToken{} || !impl_->moduleValid(module)) return false;
    if (std::find(impl_->directModules.begin(), impl_->directModules.end(), module) != impl_->directModules.end()) return false;
    for (const auto& slot : impl_->slots) if (slot.entry && slot.module == module && slot.executing) return false;
    // Retire before releasing captures: a capture destructor cannot submit new
    // work into a lifetime that is already being invalidated.
    impl_->modules.erase(std::remove(impl_->modules.begin(), impl_->modules.end(), module), impl_->modules.end());
    for (auto& slot : impl_->slots) if (slot.entry && slot.module == module) impl_->release(slot);
    return true;
}
} // namespace re4::port
