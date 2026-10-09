#pragma once
#include "re4/port/scheduler.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace re4::port {
enum class ReadState { Pending, Reading, Complete, Cancelled, Failed };
class FileRead final {
public:
    ReadState state() const noexcept;
    std::uint64_t bytesRead() const noexcept;
    void cancel() noexcept; // Nonblocking; terminal state is the acknowledgement.
    const std::vector<std::uint8_t>& output() const;
    const std::string& error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    explicit FileRead(std::string path, std::uint32_t limit);
    friend class AsyncFileReader;
public:
    ~FileRead();
};
// One persistent I/O worker, never one OS thread per task. It accesses only
// FileRead-owned memory and atomics; it cannot call engine/GameCube APIs.
class AsyncFileReader final {
public:
    // Optional telemetry runs on the I/O worker after each chunk. It must not
    // access engine state. Verification uses a barrier here to test in-flight
    // cancellation/ownership deterministically around a real filesystem read.
    explicit AsyncFileReader(std::function<void(std::uint64_t)> progressObserver = {});
    ~AsyncFileReader(); // Shutdown only: cancel and join in-flight OS read.
    AsyncFileReader(const AsyncFileReader&) = delete;
    AsyncFileReader& operator=(const AsyncFileReader&) = delete;
    // Single outstanding request. nullptr means retry on a later frame after
    // cancellation/completion has been acknowledged; no hidden parallel queue.
    std::shared_ptr<FileRead> read(std::string path, std::uint32_t limit = 64 * 1024 * 1024);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Native bounded replacement for the ISR read-pump control transfer. It polls
// owned I/O state once per slice; cancellation requests an I/O acknowledgement
// without freeing the worker's buffer or blocking the frame. No DVD queue/ARAM
// mapping is supplied by this API.
std::optional<TaskHandle> startFileReadPump(NativeScheduler& scheduler,
    std::shared_ptr<FileRead> job, ModuleToken module = {});
} // namespace re4::port
