#include "scheduler_verification.h"
#include "re4/port/scheduler.h"
#include "re4_scheduler_original.h"
#include <array>
#include <cfenv>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace re4::port {
namespace {
void expect(CheckGroup& group, bool condition, const char* message) {
    ++group.checks;
    if (!condition) { ++group.failures; group.diagnostics.emplace_back(message); }
}
TaskHandle required(std::optional<TaskHandle> value) {
    if (!value) throw std::runtime_error("Scheduler test could not allocate an expected slot");
    return *value;
}
// Test probes terminate via the actual native TaskExit adapter. Real game
// callbacks already call TaskExit/TaskChain or have an infinite sleep loop.
std::optional<TaskHandle> mainTask(NativeScheduler& scheduler, int slot, TaskEntry entry, ModuleToken module = {}) {
    if (!entry) return scheduler.exec(slot, {}, module);
    return scheduler.exec(slot, [entry = std::move(entry)] { entry(); original::TaskExit(); }, module);
}
template<class Action> bool rejected(Action action) {
    try { action(); } catch (const std::exception&) { return true; }
    return false;
}
void trace(CheckGroup& group, const std::vector<int>& values) {
    std::ostringstream out;
    out << "TRACE " << group.name << ':';
    for (int value : values) out << ' ' << value;
    group.diagnostics.push_back(out.str());
}
struct StackProbe {
    CheckGroup* group;
    std::vector<int> events;
    std::thread::id owner = std::this_thread::get_id();
    void* pointer;
};
void nestedOriginalSleep(StackProbe* probe, int& local, const int* originalAddress) {
    const int innerLocal = 73;
    probe->events.push_back(11);
    original::SceSleep(2); // Verbatim original wrapper -> original range check -> native TaskSleep.
    expect(*probe->group, innerLocal == 73 && local == 19 && &local == originalAddress, "nested frame/local/address not retained");
    ++local;
    probe->events.push_back(12);
    original::SceSleep(1);
    expect(*probe->group, innerLocal == 73 && local == 20, "second nested sleep lost locals");
}
void typedStackTask(StackProbe* probe) {
    expect(*probe->group, probe == probe->pointer, "callback argument pointer was truncated");
    expect(*probe->group, std::this_thread::get_id() == probe->owner, "task executed on a different OS thread");
    int local = 19;
    alignas(64) std::array<int, 16> storage{};
    storage[7] = 912;
    expect(*probe->group, reinterpret_cast<std::uintptr_t>(storage.data()) % 64 == 0, "native stack alignment");
    probe->events.push_back(10);
    original::SceSleep(0);
    nestedOriginalSleep(probe, local, &local);
    expect(*probe->group, local == 20 && storage[7] == 912, "outer stack frame not retained");
    expect(*probe->group, std::this_thread::get_id() == probe->owner, "sleep resumed on another OS thread");
    probe->events.push_back(13);
    original::SceExit(); // Verbatim unlink + TaskExit: must never return.
}
#if defined(__ANDROID__)
// Pin all eight ABI-preserved FP registers. A second task has different values
// in these same registers while suspended. This detects the ARM32 softfp trap.
void registerProbe(CheckGroup& group, double seed) {
    register double a asm("d8") = seed + 1;
    register double b asm("d9") = seed + 2;
    register double c asm("d10") = seed + 3;
    register double d asm("d11") = seed + 4;
    register double e asm("d12") = seed + 5;
    register double f asm("d13") = seed + 6;
    register double g asm("d14") = seed + 7;
    register double h asm("d15") = seed + 8;
    for (int i = 0; i < 64; ++i) {
        asm volatile("" : "+w"(a), "+w"(b), "+w"(c), "+w"(d), "+w"(e), "+w"(f), "+w"(g), "+w"(h));
        original::TaskSleep(1);
        asm volatile("" : "+w"(a), "+w"(b), "+w"(c), "+w"(d), "+w"(e), "+w"(f), "+w"(g), "+w"(h));
        expect(group, a == seed+1 && b == seed+2 && c == seed+3 && d == seed+4 &&
            e == seed+5 && f == seed+6 && g == seed+7 && h == seed+8, "callee-saved FP registers not retained");
    }
}
#endif
} // namespace

void checkSchedulerStack(CheckGroup& group) {
    NativeScheduler scheduler;
    auto probe = std::make_unique<StackProbe>();
    probe->group = &group; probe->pointer = probe.get();
    if (sizeof(void*) == 8)
        expect(group, reinterpret_cast<std::uintptr_t>(probe.get()) > std::numeric_limits<std::uint32_t>::max(), "64-bit pointer test did not exercise high address bits");
    const auto task = required(scheduler.execScenario(5, bindTask(typedStackTask, probe.get())));
    expect(group, scheduler.allocatedStacks() == 0, "stack allocated before TASK_EXEC visit");
    scheduler.runScenarios();
    expect(group, probe->events == std::vector<int>({10,11}), "first sleep did not yield at nested call");
    expect(group, scheduler.state(task)->status == 2 && scheduler.state(task)->sleepVisits == 2, "first sleep state/count");
    scheduler.runScenarios();
    expect(group, probe->events == std::vector<int>({10,11}) && scheduler.state(task)->sleepVisits == 1, "Sleep(2) resumed too early");
    scheduler.runScenarios();
    expect(group, probe->events == std::vector<int>({10,11,12}), "first continuation incorrect");
    scheduler.runScenarios();
    expect(group, probe->events == std::vector<int>({10,11,12,13}) && !scheduler.state(task), "SceExit did not release/unlink");
    expect(group, scheduler.allocatedStacks() == 0, "completed original scenario leaked stack");
    trace(group, probe->events);

    // Child scenario pass invoked from a main task: both parent and child stacks
    // remain intact, with the immediate caller restored after every child yield.
    int parentLocal = 0;
    required(mainTask(scheduler, 2, [&] {
        int parent = 41;
        const auto parentHandle = *scheduler.current();
        required(scheduler.execScenario(6, [&] {
            expect(group, !scheduler.kill(parentHandle), "child killed its active caller stack");
            int child = 83;
            original::SceSleep(1);
            expect(group, child == 83, "nested scenario child frame lost");
        }));
        scheduler.runScenarios();
        expect(group, scheduler.current() == std::optional<TaskHandle>(parentHandle), "parent task identity not restored");
        original::TaskSleep(1);
        scheduler.runScenarios();
        expect(group, parent == 41, "parent frame lost across child/parent yields");
        parentLocal = parent;
    }));
    scheduler.runMain(); scheduler.runMain();
    expect(group, parentLocal == 41 && scheduler.allocatedStacks() == 0, "nested scheduler pass incomplete");

    const int originalRound = std::fegetround();
    const auto fpTask = required(mainTask(scheduler, 0, [&] {
        expect(group, std::fesetround(FE_DOWNWARD) == 0, "cannot set task FP rounding");
        original::TaskSleep(1);
        expect(group, std::fegetround() == FE_DOWNWARD, "task FP control state not preserved");
    }));
    scheduler.visit(fpTask);
    expect(group, std::fegetround() == originalRound, "task changed caller FP environment");
    scheduler.visit(fpTask);
    expect(group, std::fegetround() == originalRound, "completed task changed caller FP environment");
    expect(group, rejected([&] { scheduler.sleep(1); }), "TaskSleep outside task accepted");
    original::SceSleep(7); // Actual original no-op outside scenario range.
    expect(group, original::SceSys.checkCTaskRange() == 0, "main sentinel considered a scenario task");
    int nonScenario = 0;
    required(mainTask(scheduler, 1, [&] { original::SceSleep(3); ++nonScenario; }));
    scheduler.runMain();
    expect(group, nonScenario == 1, "SceSleep incorrectly yielded outside slots 5..17");
#if defined(__ANDROID__)
    required(mainTask(scheduler, 0, [&] { registerProbe(group, 100); }));
    required(mainTask(scheduler, 1, [&] { registerProbe(group, 900); }));
    for (int i = 0; i <= 64; ++i) scheduler.runMain();
    expect(group, scheduler.allocatedStacks() == 0, "FP register probes leaked stacks");
#endif
}

void checkSchedulerOrder(CheckGroup& group) {
    NativeScheduler scheduler;
    std::vector<int> events;
    for (int i = 3; i >= 0; --i) {
        const auto task = required(mainTask(scheduler, i, [&, i] { events.push_back(i); }));
        scheduler.setPriority(task, static_cast<std::uint8_t>(20 - i));
    }
    scheduler.runMain();
    expect(group, events == std::vector<int>({0,1,2,3}), "OS priorities reordered original main slots");
    required(scheduler.execScenario(6, [&] { events.push_back(6); }, 1, 2));
    required(scheduler.execScenario(7, [&] { events.push_back(7); }, 1, 2));
    required(scheduler.execScenario(8, [&] {
        events.push_back(8);
        required(scheduler.execScenario(9, [&] { events.push_back(9); }, 1, 15));
    }, 1, 10));
    scheduler.runScenarios();
    expect(group, events == std::vector<int>({0,1,2,3,8,9,7,6}), "reverse OT/LIFO/head restart incorrect");
    trace(group, events);
    const auto autoTask = required(scheduler.execScenario(18, [] {}));
    expect(group, autoTask.slot == 17, "auto scenario allocation did not start at slot 17");
    scheduler.kill(autoTask);
    std::vector<TaskHandle> allocated;
    for (int i = 17; i >= 6; --i) {
        auto next = required(scheduler.execScenario(18, [] {}));
        expect(group, next.slot == i, "auto scenario allocation order");
        allocated.push_back(next);
    }
    expect(group, !scheduler.execScenario(18, [] {}), "auto scenario allocation consumed reserved slot 5");
    for (auto handle : allocated) scheduler.kill(handle);

    std::vector<int> held;
    auto five = required(scheduler.execScenario(5, [&] { held.push_back(5); }, 6));
    auto six = required(scheduler.execScenario(6, [&] { held.push_back(6); }, 6));
    scheduler.setScenarioGates(true, false); scheduler.runScenarios();
    expect(group, held == std::vector<int>({5}) && !scheduler.state(five) && scheduler.state(six).has_value(), "item/chapter hold exception for slot 5");
    scheduler.setScenarioGates(false, true); scheduler.runScenarios();
    expect(group, held == std::vector<int>({5}), "SPF_SCE gate ignored");
    scheduler.setScenarioGates(false, false); scheduler.runScenarios();
    expect(group, held == std::vector<int>({5,6}), "scenario did not resume after hold");
    int steps = 0;
    auto gate = required(scheduler.execScenario(6, [&] { ++steps; original::SceSleep(2); ++steps; }, 1));
    scheduler.runScenarios();
    scheduler.setGameGates(true, false); scheduler.runScenarios();
    expect(group, steps == 1 && scheduler.state(gate)->sleepVisits == 2, "STA_SUSPEND decremented held sleep");
    scheduler.setKind(gate, 3); scheduler.runScenarios();
    expect(group, scheduler.state(gate)->sleepVisits == 1, "kind bit 2 did not bypass STA_SUSPEND");
    scheduler.setGameGates(false, true); scheduler.runScenarios();
    expect(group, scheduler.state(gate)->sleepVisits == 1, "STA_DIEDEMO failed to hold task");
    scheduler.setKind(gate, 7); scheduler.runScenarios();
    expect(group, steps == 2 && !scheduler.state(gate), "kind bit 4 did not bypass STA_DIEDEMO");
    scheduler.setGameGates(false, false);
    std::uint8_t inherited = 0;
    required(scheduler.execScenario(6, [&] {
        auto child = required(scheduler.execScenario(7, [&] {}, 0));
        inherited = scheduler.state(child)->kind;
    }, 5));
    scheduler.runScenarios();
    expect(group, inherited == 5, "scenario kind was not inherited from caller");
    expect(group, rejected([&] { mainTask(scheduler, 4, [] {}); }), "preemptive ISR slot silently treated as cooperative");
    expect(group, !scheduler.execScenario(1, [] {}), "invalid scenario slot accepted");
    expect(group, rejected([&] { scheduler.execScenario(6, [] {}, 1, 16); }), "invalid OT priority accepted");
}

void checkSchedulerLifecycle(CheckGroup& group) {
    NativeScheduler scheduler;
    std::vector<int> events;
    auto pending = required(mainTask(scheduler, 0, [&] { events.push_back(99); }));
    expect(group, !mainTask(scheduler, 0, [] {}), "busy slot overwritten");
    expect(group, scheduler.kill(pending) && !scheduler.kill(pending), "pending kill/stale reference");
    auto task = required(mainTask(scheduler, 0, [&] {
        int local = 27;
        events.push_back(1);
        original::TaskSleep(2);
        expect(group, local == 27, "suspend/resume lost local");
        events.push_back(2);
        scheduler.chain([&] { events.push_back(3); scheduler.exit(); });
    }));
    scheduler.runMain();
    scheduler.suspend(task); scheduler.suspend(task);
    scheduler.runMain();
    expect(group, scheduler.state(task)->status == 0x82 && scheduler.state(task)->suspendCount == 2 && scheduler.state(task)->sleepVisits == 2, "counted suspend state");
    scheduler.signal(task); scheduler.runMain();
    expect(group, scheduler.state(task)->suspendCount == 1 && scheduler.state(task)->sleepVisits == 2, "first signal resumed counted suspension");
    scheduler.signal(task); scheduler.runMain();
    expect(group, scheduler.state(task)->status == 2 && scheduler.state(task)->sleepVisits == 1, "final signal did not resume countdown");
    scheduler.runMain();
    expect(group, events == std::vector<int>({1,2}) && scheduler.state(task)->status == 1 && scheduler.allocatedStacks() == 0, "TaskChain started early or kept old stack");
    scheduler.runMain();
    expect(group, events == std::vector<int>({1,2,3}) && !scheduler.state(task), "chained task did not execute/exit");
    expect(group, !scheduler.suspend(task) && !scheduler.signal(task), "stale suspend/signal accepted");
    trace(group, events);
    auto replacement = required(mainTask(scheduler, 0, [] {}));
    expect(group, !(replacement == task) && !scheduler.kill(task) && scheduler.state(replacement).has_value(), "slot reuse accepted stale handle");
    scheduler.kill(replacement);
    int selfKillReturned = 0;
    required(mainTask(scheduler, 0, [&] { scheduler.kill(*scheduler.current()); ++selfKillReturned; }));
    scheduler.runMain();
    expect(group, selfKillReturned == 0 && scheduler.allocatedStacks() == 0, "self-kill returned or freed its active stack");
    int pendingRuns = 0;
    auto heldPending = required(mainTask(scheduler, 0, [&] { ++pendingRuns; }));
    scheduler.suspend(heldPending); scheduler.runMain();
    expect(group, pendingRuns == 0 && scheduler.state(heldPending)->status == 0x81 && scheduler.allocatedStacks() == 0, "suspended TASK_EXEC started");
    scheduler.signal(heldPending); scheduler.signal(heldPending); scheduler.runMain();
    expect(group, pendingRuns == 1 && !scheduler.state(heldPending), "pending TASK_EXEC did not resume after signal");

    int afterKill = 0, automaticDestructors = 0;
    struct OriginalAbruptLifetime { int* count; ~OriginalAbruptLifetime() { ++*count; } };
    auto sleeping = required(mainTask(scheduler, 1, [&] {
        OriginalAbruptLifetime lifetime{&automaticDestructors};
        original::TaskSleep(1); ++afterKill;
    }));
    scheduler.runMain(); scheduler.suspend(sleeping); scheduler.kill(sleeping); scheduler.runMain();
    expect(group, afterKill == 0 && automaticDestructors == 0 && scheduler.allocatedStacks() == 0, "OSCancelThread-style abrupt cancellation changed or stack leaked");
    auto owned = std::make_shared<int>(5);
    std::weak_ptr<int> resource = owned;
    auto capture = required(mainTask(scheduler, 1, [owned] { original::TaskSleep(1); }));
    owned.reset(); scheduler.runMain(); scheduler.kill(capture);
    expect(group, resource.expired(), "port-owned entry captures leaked on cancellation");

    auto negative = required(mainTask(scheduler, 0, [&] { original::TaskSleep(-1); ++afterKill; }));
    scheduler.runMain();
    expect(group, scheduler.state(negative)->sleepVisits == 65535, "negative sleep lost original u16 conversion");
    scheduler.runMain(); expect(group, scheduler.state(negative)->sleepVisits == 65534, "u16 sleep decrement incorrect");
    scheduler.kill(negative);
    auto wrap = required(mainTask(scheduler, 0, [&] { original::TaskSleep(65536); }));
    scheduler.runMain(); scheduler.runMain();
    expect(group, scheduler.state(wrap)->sleepVisits == 65535, "zero-after-u16-conversion sleep did not wrap on visit");
    scheduler.kill(wrap);
    int integerArgument = 0;
    required(mainTask(scheduler, 1, bindTask(+[](int* out, int value) { *out = value; }, &integerArgument, -23)));
    scheduler.runMain(); expect(group, integerArgument == -23, "typed integer callback argument");
    required(mainTask(scheduler, 1, [] { throw std::runtime_error("intentional task failure"); }));
    expect(group, rejected([&] { scheduler.runMain(); }) && scheduler.allocatedStacks() == 0 && !scheduler.current(), "task exception crossed context or left active stack");
    for (int i = 0; i < 512; ++i) {
        auto stress = required(mainTask(scheduler, 0, [] { original::TaskSleep(1); }));
        scheduler.runMain();
        expect(group, scheduler.allocatedStacks() == 1 && scheduler.kill(stress) && scheduler.allocatedStacks() == 0, "stack reuse/cancellation stress failure");
    }
    bool wrongThreadRejected = false;
    std::thread other([&] { wrongThreadRejected = rejected([&] { scheduler.runMain(); }); });
    other.join(); expect(group, wrongThreadRejected, "scheduler accepted another owner thread");
    expect(group, original::GetStackSize(2) == 0x3000 && original::GetStackSize(0) == 0x2000 && original::GetStackSize(17) == 0x1800, "original stack-size function changed");
    expect(group, rejected([&] { mainTask(scheduler, 0, {}); }), "empty callback accepted");
    expect(group, rejected([&] { bindTask(static_cast<void(*)()>(nullptr)); }), "null typed callback accepted");
    required(scheduler.exec(0, [] {}));
    expect(group, rejected([&] { scheduler.runMain(); }) && scheduler.allocatedStacks() == 0,
        "unexpected main return was silently treated as original TaskExit");
}

void checkSchedulerModules(CheckGroup& group) {
    NativeScheduler scheduler;
    auto module = scheduler.createModuleLifetime();
    auto pending = required(mainTask(scheduler, 0, [] {}, module));
    auto sleeper = required(scheduler.execScenario(6, [] { original::SceSleep(1); }, 1, 2, nullptr, module));
    scheduler.runScenarios();
    expect(group, scheduler.allocatedStacks() == 1, "module sleeper did not get a real stack");
    expect(group, scheduler.invalidateModule(module), "quiescent module refused invalidation");
    expect(group, !scheduler.state(pending) && !scheduler.state(sleeper) && scheduler.allocatedStacks() == 0, "module invalidation left task references/stacks");
    expect(group, !mainTask(scheduler, 0, [] {}, module) && !scheduler.invalidateModule(module), "expired module token accepted");
    auto nextModule = scheduler.createModuleLifetime();
    expect(group, !(nextModule == module), "module generation reused");
    auto replacement = required(mainTask(scheduler, 0, [] {}, nextModule));
    expect(group, !scheduler.kill(pending) && scheduler.state(replacement).has_value(), "old module handle killed new occupant");
    scheduler.kill(replacement);
    bool parentProtected = false, childProtected = false;
    auto parent = required(mainTask(scheduler, 2, [&] {
        parentProtected = !scheduler.invalidateModule(nextModule);
        required(scheduler.execScenario(7, [&] { childProtected = !scheduler.invalidateModule(nextModule); }, 1, 2, nullptr, nextModule));
        scheduler.runScenarios();
        original::TaskSleep(1);
    }, nextModule));
    scheduler.runMain();
    expect(group, parentProtected && childProtected, "module was invalidated while its code was on the active call chain");
    expect(group, scheduler.invalidateModule(nextModule) && !scheduler.state(parent), "suspended parent module could not be cancelled safely");
    scheduler.runMain(); scheduler.runScenarios();
    auto directModule = scheduler.createModuleLifetime();
    bool directProtected = false;
    scheduler.execScenario(0, [&] { directProtected = !scheduler.invalidateModule(directModule); }, 0, 2, nullptr, directModule);
    expect(group, directProtected && scheduler.invalidateModule(directModule), "direct module callback lifetime was not protected");
    auto outer = scheduler.createModuleLifetime();
    auto inner = scheduler.createModuleLifetime();
    bool nestedDirectProtected = false;
    scheduler.execScenario(0, [&] {
        scheduler.execScenario(0, [&] { nestedDirectProtected = !scheduler.invalidateModule(outer); }, 0, 2, nullptr, inner);
    }, 0, 2, nullptr, outer);
    expect(group, nestedDirectProtected && scheduler.invalidateModule(outer) && scheduler.invalidateModule(inner), "nested direct caller module was not protected");
    auto retiring = scheduler.createModuleLifetime();
    bool reentryRejected = false;
    struct OnRelease { std::function<void()> action; ~OnRelease() { action(); } };
    auto owned = std::make_shared<OnRelease>();
    owned->action = [&] { reentryRejected = !mainTask(scheduler, 3, [] {}, retiring); };
    required(mainTask(scheduler, 0, [owned] {}, retiring)); owned.reset();
    expect(group, scheduler.invalidateModule(retiring) && reentryRejected, "retiring module accepted reentrant work during capture release");
    TaskHandle crossDomain;
    {
        std::thread other([&] {
            NativeScheduler separate;
            crossDomain = required(separate.exec(0, [] {}));
        });
        other.join();
    }
    expect(group, !scheduler.kill(crossDomain), "handle from another runtime was accepted");
    group.diagnostics.push_back("TRACE scheduler_modules: pending-cancel sleep-cancel expired-reject ancestor-protect direct-protect foreign-reject");
}
} // namespace re4::port
