#include "re4/port/async_file.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <limits>
#include <stdexcept>
#include <thread>

namespace re4::port {
struct FileRead::Impl {
    const std::string path;
    const std::uint32_t limit;
    std::atomic<ReadState> state{ReadState::Pending};
    std::atomic<bool> cancelled{false};
    std::atomic<std::uint64_t> progress{0};
    std::vector<std::uint8_t> result;
    std::string failure;
    Impl(std::string name, std::uint32_t size) : path(std::move(name)), limit(size) {}
    void run(const std::function<void(std::uint64_t)>& observer) noexcept {
        try {
            if (cancelled.load(std::memory_order_acquire)) { finish(ReadState::Cancelled); return; }
            state.store(ReadState::Reading, std::memory_order_release);
            std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
            if (!file) throw std::runtime_error("Cannot open input file");
            std::array<char, 64 * 1024> chunk;
            while (!cancelled.load(std::memory_order_acquire)) {
                file.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
                const auto n = static_cast<std::size_t>(file.gcount());
                if (file.bad() || (file.fail() && !file.eof())) throw std::runtime_error("File read failed");
                if (n > limit - result.size()) throw std::runtime_error("File exceeds configured input limit");
                result.insert(result.end(), chunk.data(), chunk.data() + n);
                progress.store(result.size(), std::memory_order_release);
                if (observer) observer(result.size());
                if (file.eof()) {
                    finish(cancelled.load(std::memory_order_acquire) ? ReadState::Cancelled : ReadState::Complete);
                    return;
                }
            }
            finish(ReadState::Cancelled);
        } catch (const std::exception& e) {
            // No exception crosses the worker boundary; publish owned failure.
            try { failure = e.what(); } catch (...) {}
            finish(ReadState::Failed);
        } catch (...) { finish(ReadState::Failed); }
    }
    void finish(ReadState finalState) noexcept { state.store(finalState, std::memory_order_release); }
};
FileRead::FileRead(std::string path, std::uint32_t limit) : impl_(std::make_unique<Impl>(std::move(path), limit)) {}
FileRead::~FileRead() = default;
ReadState FileRead::state() const noexcept { return impl_->state.load(std::memory_order_acquire); }
std::uint64_t FileRead::bytesRead() const noexcept { return impl_->progress.load(std::memory_order_acquire); }
void FileRead::cancel() noexcept { impl_->cancelled.store(true, std::memory_order_release); }
const std::vector<std::uint8_t>& FileRead::output() const {
    if (state() != ReadState::Complete) throw std::logic_error("File data is not published");
    return impl_->result;
}
const std::string& FileRead::error() const {
    if (state() != ReadState::Failed) throw std::logic_error("File failure is not published");
    return impl_->failure;
}
struct AsyncFileReader::Impl {
    const std::thread::id owner = std::this_thread::get_id();
    std::mutex mutex;
    std::condition_variable ready;
    std::shared_ptr<FileRead> job;
    bool stopping = false;
    const std::function<void(std::uint64_t)> observer;
    std::thread worker;
    explicit Impl(std::function<void(std::uint64_t)> callback)
        : observer(std::move(callback)), worker([this] { loop(); }) {}
    ~Impl() {
        { std::lock_guard<std::mutex> lock(mutex); stopping = true; if (job) job->cancel(); }
        ready.notify_one();
        worker.join();
    }
    void loop() {
        for (;;) {
            std::shared_ptr<FileRead> current;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock, [this] { return stopping || job; });
                if (stopping && !job) return;
                current = job;
            }
            current->impl_->run(observer); // No engine memory and no mutex held during I/O.
            {
                std::lock_guard<std::mutex> lock(mutex);
                job.reset();
                if (stopping) return;
            }
        }
    }
};
AsyncFileReader::AsyncFileReader(std::function<void(std::uint64_t)> observer)
    : impl_(std::make_unique<Impl>(std::move(observer))) {}
AsyncFileReader::~AsyncFileReader() = default;
std::shared_ptr<FileRead> AsyncFileReader::read(std::string path, std::uint32_t limit) {
    if (std::this_thread::get_id() != impl_->owner) throw std::logic_error("I/O submission requires engine owner");
    if (path.empty()) throw std::invalid_argument("Empty file path");
    // try_lock never waits for worker acknowledgement on a gameplay frame.
    std::unique_lock<std::mutex> lock(impl_->mutex, std::try_to_lock);
    if (!lock || impl_->job || impl_->stopping) return {};
    auto job = std::shared_ptr<FileRead>(new FileRead(std::move(path), limit));
    impl_->job = job;
    impl_->ready.notify_one();
    return job;
}
std::optional<TaskHandle> startFileReadPump(NativeScheduler& scheduler,
    std::shared_ptr<FileRead> job, ModuleToken module) {
    if (!job) throw std::invalid_argument("Null file-read pump input");
    return scheduler.execBackground([job, &scheduler] {
        for (;;) {
            switch (job->state()) {
            case ReadState::Complete: scheduler.exitBackground();
            case ReadState::Failed: throw std::runtime_error(job->error());
            case ReadState::Cancelled: scheduler.killBackground(); std::terminate();
            default: scheduler.backgroundCheckpoint(std::numeric_limits<std::uint32_t>::max()); break;
            }
        }
    }, module, [job](BackgroundOutcome outcome) {
        if (outcome != BackgroundOutcome::Completed) job->cancel();
    });
}
} // namespace re4::port
