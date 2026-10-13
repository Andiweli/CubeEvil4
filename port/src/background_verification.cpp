#include "background_verification.h"
#include "re4/port/scheduler.h"
#include "re4/port/yz2.h"
#include "re4/port/async_file.h"
#include "re4_scheduler_original.h"
#include "dvd_adapter_probe.h"
#include "../tests/yz2_fixtures.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace re4::port::verification { thread_local DvdProbe Dvd; }
namespace re4::port {
namespace {
using namespace std::chrono_literals;
constexpr BackgroundBudget deterministic{4096, 0ns};
void expect(CheckGroup& group, bool condition, const char* label) {
    ++group.checks;
    if (!condition) { ++group.failures; group.diagnostics.emplace_back(label); }
}
template<class F> bool rejected(F action) {
    try { action(); } catch (const std::exception&) { return true; }
    return false;
}
TaskHandle required(std::optional<TaskHandle> task) {
    if (!task) throw std::runtime_error("Expected background allocation failed");
    return *task;
}
void finish(NativeScheduler& scheduler, BackgroundBudget budget = deterministic) {
    for (int frame = 0; frame < 20000 && scheduler.backgroundActive(); ++frame)
        original::iTaskScheduler(budget);
    if (scheduler.backgroundActive()) throw std::runtime_error("Background task failed to finish within test frame limit");
}
void nestedPause(NativeScheduler& scheduler, CheckGroup& group, int& local) {
    const auto* address = &local;
    const int nestedLocal = 47;
    for (int i = 0; i < 3; ++i) {
        ++local;
        original::iTaskSuspend();
        scheduler.backgroundCheckpoint();
        expect(group, address == &local && nestedLocal == 47 && local == i + 11,
            "Nested locals/address survive original iTaskSuspend checkpoints");
    }
}
bool terminal(ReadState state) {
    return state == ReadState::Complete || state == ReadState::Failed || state == ReadState::Cancelled;
}
void waitRead(const std::shared_ptr<FileRead>& job) {
    const auto end = std::chrono::steady_clock::now() + 5s;
    while (!terminal(job->state()) && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(1ms);
    if (!terminal(job->state())) throw std::runtime_error("I/O worker did not acknowledge within test timeout");
}
std::shared_ptr<FileRead> submit(AsyncFileReader& reader, const std::string& path, u32 limit = 64 * 1024 * 1024) {
    const auto end = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < end) {
        if (auto result = reader.read(path, limit)) return result;
        std::this_thread::yield();
    }
    throw std::runtime_error("I/O worker did not accept next request");
}
std::string executablePath() {
#if defined(_WIN32)
    char buffer[32768];
    const auto size = GetModuleFileNameA(nullptr, buffer, sizeof(buffer));
    if (!size || size >= sizeof(buffer)) throw std::runtime_error("Cannot locate test executable");
    return std::string(buffer, size);
#else
    // Actual Android ELF file, also available in the instrumentation process.
    return "/proc/self/exe";
#endif
}
}

void checkBackground(CheckGroup& group) {
    NativeScheduler scheduler;
    const auto owner = std::this_thread::get_id();
    int localResult = 0, completions = 0;
    BackgroundOutcome result = BackgroundOutcome::Failed;
    const auto task = required(original::iTaskExec([&] {
        int local = 10;
        nestedPause(scheduler, group, local);
        localResult = local;
        expect(group, std::this_thread::get_id() == owner, "Slot 4 remains on the engine owner");
        original::iTaskExit();
    }, {}, [&](BackgroundOutcome outcome) {
        expect(group, scheduler.backgroundActive() && !scheduler.allocatedStacks(),
            "Completion publishes on owner after stack release and before idle");
        expect(group, !original::iTaskExec([] {}), "Completion cannot replace its releasing slot");
        ++completions; result = outcome;
    }));
    expect(group, original::iTaskStatus() == 1 && scheduler.state(task)->status == original::TASK_EXEC,
        "iTaskExec publishes pending status before running");
    expect(group, scheduler.state(task)->priority == 0, "Original background priority zero");
    expect(group, !original::iTaskExec([] {}), "Only one background job can be active");
    scheduler.runMain();
    expect(group, scheduler.allocatedStacks() == 0 && localResult == 0, "Background never runs in main pass");
    for (int frame = 0; frame < 3; ++frame) {
        original::iTaskScheduler(deterministic);
        expect(group, scheduler.state(task)->status == (original::TASK_RUN | original::TASK_SUSPEND),
            "Retrace-style suspension retains RUN|SUSPEND status");
        expect(group, original::iTaskStatus() == 1 && !completions, "Suspension does not publish completion");
    }
    original::iTaskScheduler(deterministic);
    expect(group, localResult == 13 && result == BackgroundOutcome::Completed && completions == 1,
        "iTaskExit completes exactly once after three nested resumes");
    expect(group, !scheduler.state(task) && !original::iTaskStatus() && !scheduler.allocatedStacks(),
        "Finished slot/reference/stack are released");

    // Deterministic frame order: game slots, watchers, background slice.
    std::vector<int> order;
    for (int slot = 0; slot < 4; ++slot) required(scheduler.exec(slot, [&, slot] {
        for (int frame = 0; frame < 3; ++frame) { order.push_back(slot); original::TaskSleep(1); }
        original::TaskExit();
    }));
    required(original::iTaskExec([&] {
        for (int frame = 0; frame < 3; ++frame) { order.push_back(4); scheduler.backgroundCheckpoint(4096); }
        original::iTaskExit();
    }));
    for (int frame = 0; frame < 4; ++frame) {
        scheduler.runMain();
        if (frame < 3) order.push_back(9); // Explicitly isolated watcher-order probe.
        original::iTaskScheduler(deterministic);
    }
    const std::vector<int> expected{0,1,2,3,9,4,0,1,2,3,9,4,0,1,2,3,9,4};
    expect(group, order == expected, "Original gameplay/watchers/background frame order");

    // Asynchronous requests touch atomics only; all engine state stays here.
    for (int iteration = 0; iteration < 32; ++iteration) {
        std::atomic<bool> entered{false}, requested{false};
        BackgroundOutcome outcome = BackgroundOutcome::Failed;
        TaskHandle handle = required(original::iTaskExec([&] {
            entered.store(true, std::memory_order_release);
            while (!requested.load(std::memory_order_acquire)) std::this_thread::yield();
            scheduler.backgroundCheckpoint();
            expect(group, !scheduler.backgroundActive(), "Cancelled task must never reach this statement");
            original::iTaskExit();
        }, {}, [&](BackgroundOutcome value) { outcome = value; }));
        std::thread callback([&] {
            while (!entered.load(std::memory_order_acquire)) std::this_thread::yield();
            scheduler.requestBackgroundSuspend();
            const bool accepted = scheduler.requestBackgroundCancel(handle);
            // Communicate acceptance through the existing release/acquire edge.
            entered.store(accepted, std::memory_order_relaxed);
            requested.store(true, std::memory_order_release);
        });
        original::iTaskScheduler(deterministic);
        callback.join();
        expect(group, entered.load() && outcome == BackgroundOutcome::Cancelled && !scheduler.backgroundActive(),
            "Cross-thread cancellation safely exits at a checkpoint");
        int fresh = 0;
        const auto replacement = required(original::iTaskExec([&] { ++fresh; original::iTaskExit(); }));
        expect(group, !scheduler.requestBackgroundCancel(handle), "Stale cancellation cannot target replacement slot");
        expect(group, !scheduler.kill(handle), "Stale owner handle cannot kill replacement");
        original::iTaskScheduler(deterministic);
        expect(group, fresh == 1 && !scheduler.state(replacement), "Restart after acknowledged cancellation");
    }
    // Retrace callback on another thread pauses without cancellation.
    std::atomic<bool> entered{false}, paused{false};
    int resumed = 0;
    const auto pauseTask = required(original::iTaskExec([&] {
        entered.store(true, std::memory_order_release);
        while (!paused.load(std::memory_order_acquire)) std::this_thread::yield();
        scheduler.backgroundCheckpoint();
        ++resumed; original::iTaskExit();
    }));
    std::thread retrace([&] {
        while (!entered.load(std::memory_order_acquire)) std::this_thread::yield();
        scheduler.requestBackgroundSuspend(); paused.store(true, std::memory_order_release);
    });
    original::iTaskScheduler(deterministic); retrace.join();
    expect(group, !resumed && (scheduler.state(pauseTask)->status & original::TASK_SUSPEND),
        "Foreign retrace request yields on owner stack");
    original::iTaskScheduler(deterministic);
    expect(group, resumed == 1 && !scheduler.backgroundActive(), "Next frame clears retrace suspension and resumes");

    // Owner kill, module release, buffer lifetime, sleep and failure paths.
    auto buffer = std::make_shared<int>(73);
    std::weak_ptr<int> lifetime = buffer;
    const auto module = scheduler.createModuleLifetime();
    const auto pending = required(original::iTaskExec([buffer] { original::iTaskExit(); }, module));
    buffer.reset();
    expect(group, !lifetime.expired(), "Pending entry retains its input buffer");
    expect(group, scheduler.invalidateModule(module) && lifetime.expired() && !scheduler.state(pending),
        "Module cancellation releases pending background ownership");
    expect(group, !original::iTaskExec([] {}, module), "Expired module cannot restart background code");
    int afterSleep = 0;
    const auto sleeping = required(original::iTaskExec([&] {
        original::TaskSleep(2); ++afterSleep; original::iTaskExit();
    }));
    original::iTaskScheduler(deterministic);
    original::iTaskScheduler(deterministic);
    expect(group, !afterSleep && scheduler.state(sleeping)->sleepVisits == 1, "Background TaskSleep counts frame-tail visits");
    original::iTaskScheduler(deterministic);
    expect(group, afterSleep == 1, "Background TaskSleep resumes after two visits");
    auto owned = std::make_shared<int>(4);
    lifetime = owned;
    required(original::iTaskExec([owned, &scheduler] {
        scheduler.backgroundCheckpoint(4096); original::iTaskExit();
    }));
    owned.reset(); original::iTaskScheduler(deterministic);
    expect(group, !lifetime.expired(), "Paused stack retains callback-owned storage");
    original::iTaskKill();
    expect(group, lifetime.expired() && !scheduler.backgroundActive(), "iTaskKill releases storage only after returning to owner");
    result = BackgroundOutcome::Completed;
    required(original::iTaskExec([] {}, {}, [&](BackgroundOutcome outcome) { result = outcome; }));
    expect(group, rejected([&] { original::iTaskScheduler(deterministic); }), "Unexpected background callback return is an error");
    expect(group, result == BackgroundOutcome::Failed && !scheduler.backgroundActive(), "Failure still publishes status and frees slot");
    expect(group, rejected([&] { scheduler.backgroundCheckpoint(); }), "Root cannot use a task checkpoint");
    expect(group, rejected([&] { scheduler.runBackground({0, 0ns}); }) &&
        rejected([&] { scheduler.runBackground({1, -1ns}); }) &&
        rejected([&] { scheduler.runBackground({1, 2s}); }), "Invalid/overflow-prone slice budgets rejected");
    int timed = 0;
    required(original::iTaskExec([&] {
        ++timed;
        // Deliberately bounded adapter probe ensures the clock limit has elapsed.
        const auto until = std::chrono::steady_clock::now() + 100us;
        while (std::chrono::steady_clock::now() < until) {}
        scheduler.backgroundCheckpoint();
        ++timed; original::iTaskExit();
    }));
    original::iTaskScheduler({1024 * 1024, 1ns});
    expect(group, timed == 1 && scheduler.backgroundActive(), "Clock budget yields before unit budget is exhausted");
    original::iTaskScheduler(deterministic);
    expect(group, timed == 2 && !scheduler.backgroundActive(), "Clock-limited work resumes on the next frame");
    const auto activeModule = scheduler.createModuleLifetime();
    bool invalidatedFromTask = true;
    const auto moduleTask = required(original::iTaskExec([&] {
        invalidatedFromTask = scheduler.invalidateModule(activeModule);
        scheduler.backgroundCheckpoint(4096); original::iTaskExit();
    }, activeModule));
    original::iTaskScheduler(deterministic);
    expect(group, !invalidatedFromTask && scheduler.invalidateModule(activeModule) && !scheduler.state(moduleTask),
        "Active background module is protected, suspended module work can be invalidated");
    result = BackgroundOutcome::Failed;
    required(original::iTaskExec([] { original::iTaskKill(); }, {}, [&](BackgroundOutcome value) { result = value; }));
    original::iTaskScheduler(deterministic);
    expect(group, result == BackgroundOutcome::Cancelled && !scheduler.backgroundActive(), "Background self-kill acknowledges cancellation");
    group.diagnostics.push_back("TRACE background: pending suspended suspended suspended complete frame-order cancel restart retrace module sleep kill failure");
}

void checkYz2(CheckGroup& group) {
    for (int fixture = 0; fixture < 3; ++fixture) {
        const auto* data = fixture == 0 ? fixtures::literals : fixture == 1 ? fixtures::matches : fixtures::adaptive;
        const auto bytes = fixture == 0 ? sizeof(fixtures::literals) : fixture == 1 ? sizeof(fixtures::matches) : sizeof(fixtures::adaptive);
        const auto size = fixture == 0 ? fixtures::literalsOutputBytes : fixture == 1 ? fixtures::matchesOutputBytes : fixtures::adaptiveOutputBytes;
        Yz2Decoder sync(data, bytes);
        expect(group, rejected([&] { sync.output(); }), "Output remains private before decode");
        sync.decode();
        expect(group, sync.state() == DecodeState::Complete && sync.output().size() == size, "Original Yz2 setup/native PPC kernel completes fixture");
        bool exact = true;
        for (std::size_t i = 0; i < size; ++i) exact &= sync.output()[i] == (fixture == 0 ? static_cast<u8>(i) : u8{'A'});
        expect(group, exact, "Reference-verified decompressed bytes match exactly");
        expect(group, rejected([&] { sync.decode(); }), "Decode object rejects accidental reuse");
        if (fixture == 1) {
            const auto& s = sync.statistics();
            expect(group, s.copiedRuns == 5 && s.storedLengths == 1 && s.length16 == 1 && s.length24 == 1 && s.length32 == 1,
                "Stored, overlapping and 16/24/32-bit run-length paths exercised");
        } else if (fixture == 2) {
            const auto& s = sync.statistics();
            expect(group, s.mainRescales > 0 && s.sideRescales > 0 && s.dictionaryWraps > 0,
                "Both adaptive rescale paths and dictionary ring wrap exercised");
        }
        NativeScheduler scheduler;
        auto async = std::make_shared<Yz2Decoder>(data, bytes);
        required(startYz2Decode(scheduler, async));
        scheduler.runMain();
        expect(group, async->state() == DecodeState::Processing, "Background decode waits for frame-tail dispatch");
        original::iTaskScheduler(deterministic);
        // A second instance executes on the owner between background slices.
        // This would corrupt the original global in_ev without our adapter.
        Yz2Decoder betweenSlices(fixtures::matches, sizeof(fixtures::matches));
        betweenSlices.decode();
        expect(group, betweenSlices.output().size() == 26 && async->state() == DecodeState::Processing,
            "Independent decoder state survives interleaving on the engine owner");
        finish(scheduler, {65536, 0ns});
        expect(group, async->state() == DecodeState::Complete && async->output() == sync.output(),
            "Stackful sliced decode is byte-identical to synchronous decode");
        const auto& a = async->statistics();
        const auto& s = sync.statistics();
        expect(group, a.symbols == s.symbols && a.inputBytes == s.inputBytes && a.copiedRuns == s.copiedRuns &&
            a.mainRescales == s.mainRescales && a.sideRescales == s.sideRescales,
            "Slicing preserves exact decoder consumption and model transitions");
    }
    const std::array<u8, 4> malformed{{'1',',','F','F'}};
    expect(group, rejected([&] { Yz2Decoder d(malformed.data(), malformed.size()); }), "Truncated text header rejected");
    expect(group, rejected([&] { Yz2Decoder d(fixtures::literals, sizeof(fixtures::literals), 255); }), "Caller output budget enforced");
    for (std::size_t n : {std::size_t{33}, sizeof(fixtures::literals) - 1}) {
        Yz2Decoder d(fixtures::literals, n);
        expect(group, rejected([&] { d.decode(); }) && d.state() == DecodeState::Failed, "Truncated coded input fails with bounded access");
    }
    std::vector<u8> corrupt(std::begin(fixtures::literals), std::end(fixtures::literals));
    std::fill(corrupt.begin() + 32, corrupt.end(), u8{0});
    Yz2Decoder invalid(corrupt.data(), corrupt.size());
    expect(group, rejected([&] { invalid.decode(); }), "Uninitialized dictionary reference rejected");
    NativeScheduler scheduler;
    auto cancelled = std::make_shared<Yz2Decoder>(fixtures::adaptive, sizeof(fixtures::adaptive));
    const auto old = required(startYz2Decode(scheduler, cancelled));
    original::iTaskScheduler({1500000, 0ns}); // Past setup, inside actual run processing.
    expect(group, cancelled->state() == DecodeState::Processing && scheduler.state(old).has_value(), "Large decoder can pause during processing");
    original::iTaskKill();
    expect(group, cancelled->state() == DecodeState::Cancelled && rejected([&] { cancelled->output(); }),
        "Cancelled partial output cannot be observed as completed data");
    for (int i = 0; i < 8; ++i) {
        auto job = std::make_shared<Yz2Decoder>(fixtures::matches, sizeof(fixtures::matches));
        const auto handle = required(startYz2Decode(scheduler, job));
        expect(group, !scheduler.requestBackgroundCancel(old), "Old decode handle cannot cancel later jobs");
        finish(scheduler, {65536, 0ns});
        expect(group, job->state() == DecodeState::Complete && !scheduler.state(handle) && job->output().size() == 26,
            "Sequential decode requests finish and free their slot");
    }
    auto pending = std::make_shared<Yz2Decoder>(fixtures::literals, sizeof(fixtures::literals));
    auto next = std::make_shared<Yz2Decoder>(fixtures::matches, sizeof(fixtures::matches));
    required(startYz2Decode(scheduler, pending));
    expect(group, !startYz2Decode(scheduler, next) && next->state() == DecodeState::Prepared,
        "Busy slot leaves rejected decoder prepared for retry");
    original::iTaskKill();
    std::weak_ptr<Yz2Decoder> owned = next;
    required(startYz2Decode(scheduler, next)); next.reset();
    original::iTaskScheduler(deterministic);
    expect(group, !owned.expired(), "Background entry and completion retain decode buffers across pause");
    original::iTaskKill();
    expect(group, owned.expired(), "Cancelled decoder storage released after stack disposal");
    auto bad = std::make_shared<Yz2Decoder>(fixtures::literals, 33);
    required(startYz2Decode(scheduler, bad));
    expect(group, rejected([&] { finish(scheduler, {65536, 0ns}); }) && bad->state() == DecodeState::Failed && !scheduler.backgroundActive(),
        "Decoder error publishes Failed and does not leave background busy");
    group.diagnostics.push_back("TRACE yz2: literals256 matches26 adaptive70002 original-setup overlap stored lengths16/24/32 rescale-both ring-wrap sliced-equal cancel restart bounds");
}

void checkBackgroundIo(CheckGroup& group) {
    AsyncFileReader reader;
    NativeScheduler scheduler;
    const auto path = executablePath();
    // Real file I/O, compared against an independent synchronous test read.
    std::ifstream reference(path, std::ios::binary);
    const std::vector<u8> expected{std::istreambuf_iterator<char>(reference), std::istreambuf_iterator<char>()};
    expect(group, !expected.empty(), "Real executable file is readable by the test process");
    auto job = submit(reader, path);
    int polls = 0, gameplay = 0;
    required(scheduler.exec(0, [&] {
        for (;;) { ++gameplay; original::TaskSleep(1); }
    }));
    required(startFileReadPump(scheduler, job));
    const auto pumpDeadline = std::chrono::steady_clock::now() + 5s;
    while (scheduler.backgroundActive() && std::chrono::steady_clock::now() < pumpDeadline) {
        scheduler.runMain(); original::iTaskScheduler(deterministic); std::this_thread::yield();
    }
    expect(group, !scheduler.backgroundActive() && job->state() == ReadState::Complete,
        "Production read pump completes real I/O without waiting on the frame thread");
    job = submit(reader, path);
    // ISOLATED ADAPTER TEST: original DvdReadProc wrapper -> owned native I/O
    // poll. It does not pretend cDvd's SDK, queues, ARAM or retry rules exist.
    verification::Dvd.operation = [&] {
        while (!terminal(job->state())) { ++polls; scheduler.backgroundCheckpoint(4096); }
        original::iTaskExit();
    };
    required(original::iTaskExec(verification::DvdReadProc));
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (scheduler.backgroundActive() && std::chrono::steady_clock::now() < deadline) {
        scheduler.runMain(); original::iTaskScheduler(deterministic); std::this_thread::yield();
    }
    expect(group, !scheduler.backgroundActive() && gameplay > 0, "Gameplay continues while real I/O is polled at frame tail");
    expect(group, job->state() == ReadState::Complete && job->output() == expected && job->bytesRead() == expected.size(),
        "I/O worker publishes exact owned file contents and final progress");
    verification::Dvd.operation = {};
    for (int i = 0; i < 8; ++i) {
        job = submit(reader, path); waitRead(job);
        expect(group, job->state() == ReadState::Complete && job->output() == expected, "Successive real file requests preserve all bytes");
    }
    auto missing = submit(reader, path + ".re4-does-not-exist"); waitRead(missing);
    expect(group, missing->state() == ReadState::Failed && !missing->error().empty(), "I/O failure is owned and reliably published");
    auto limited = submit(reader, path, 0); waitRead(limited);
    expect(group, limited->state() == ReadState::Failed, "Bounded file read rejects output overflow");
    auto cancelled = submit(reader, path);
    cancelled->cancel();
    std::weak_ptr<FileRead> ownership = cancelled;
    // Completion can linearize before cancel for small files; both outcomes
    // are correct. The separate Slot-4 cancellation probes are deterministic.
    waitRead(cancelled);
    expect(group, cancelled->state() == ReadState::Cancelled || cancelled->state() == ReadState::Complete,
        "I/O cancellation acknowledges safely or observes prior completion");
    job = submit(reader, path); waitRead(job);
    cancelled.reset();
    expect(group, ownership.expired(), "Worker releases previous input buffer after cancellation acknowledgement");
    expect(group, job->state() == ReadState::Complete && job->output() == expected, "I/O restart after cancel completes");
    // Last external owner may disappear immediately; worker still owns buffers.
    auto detached = submit(reader, path);
    detached->cancel(); detached.reset();
    job = submit(reader, path); waitRead(job);
    expect(group, job->state() == ReadState::Complete, "Dropping caller ownership cannot free an active worker buffer");
    (void)polls;
    std::atomic<bool> chunkReached{false}, releaseChunk{false}, foreignWorker{false};
    const auto owner = std::this_thread::get_id();
    AsyncFileReader controlled([&](std::uint64_t) {
        foreignWorker.store(std::this_thread::get_id() != owner, std::memory_order_relaxed);
        chunkReached.store(true, std::memory_order_release);
        while (!releaseChunk.load(std::memory_order_acquire)) std::this_thread::yield();
    });
    auto inFlight = submit(controlled, path);
    const auto barrierDeadline = std::chrono::steady_clock::now() + 5s;
    while (!chunkReached.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < barrierDeadline)
        std::this_thread::yield();
    if (!chunkReached.load()) { releaseChunk.store(true); throw std::runtime_error("I/O test barrier not reached"); }
    expect(group, foreignWorker.load() && inFlight->state() == ReadState::Reading, "Real read is in-flight on the single I/O worker");
    expect(group, !controlled.read(path), "Concurrent read request cannot replace an active buffer");
    expect(group, rejected([&] { inFlight->output(); }), "In-flight read buffer is unpublished");
    required(startFileReadPump(scheduler, inFlight));
    original::iTaskScheduler(deterministic);
    expect(group, scheduler.backgroundActive(), "Native read pump yields while actual read is still in-flight");
    original::iTaskKill();
    expect(group, !scheduler.backgroundActive(), "Read-pump cancellation returns without joining the I/O worker");
    expect(group, inFlight->state() == ReadState::Reading, "Cancel request does not falsely acknowledge blocked work");
    std::weak_ptr<FileRead> retained = inFlight;
    inFlight.reset();
    expect(group, !retained.expired(), "Worker retains buffer when caller disappears during cancellation");
    releaseChunk.store(true, std::memory_order_release);
    auto restarted = submit(controlled, path); waitRead(restarted);
    expect(group, retained.expired() && restarted->state() == ReadState::Complete,
        "Cancellation acknowledgement frees buffer and permits subsequent actual read");
    group.diagnostics.push_back("ISOLATED ADAPTER: DvdReadProc wrapper; real native file worker; no cDvd SDK/queue behavior claimed");
    group.diagnostics.push_back("TRACE background_io: native-pump real-file exact-bytes gameplay sequence failure bounds cancel-ack restart owned-buffer");
}
} // namespace re4::port
