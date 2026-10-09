#pragma once
#include "re4/port/scheduler.h"
#include <atomic>
#include <cstdint>
#include <vector>

namespace re4::port {
struct Yz2Statistics {
    std::uint32_t inputBytes = 0, symbols = 0, copiedRuns = 0;
    std::uint32_t storedLengths = 0, length16 = 0, length24 = 0, length32 = 0;
    std::uint32_t dictionaryWraps = 0, mainRescales = 0, sideRescales = 0;
};
enum class DecodeState { Prepared, Processing, Complete, Cancelled, Failed };
// Owns aligned input, original 0x142800-byte work storage and private output.
// Per-instance state replaces the original process-global in_ev. Only the
// engine owner executes decode; observers may read state on any thread.
class Yz2Decoder final {
public:
    explicit Yz2Decoder(const std::uint8_t* input, std::size_t bytes,
        std::uint32_t maximumOutput = 64 * 1024 * 1024);
    ~Yz2Decoder();
    Yz2Decoder(const Yz2Decoder&) = delete;
    Yz2Decoder& operator=(const Yz2Decoder&) = delete;
    void decode(); // Synchronous, bounded data validation; no scheduler needed.
    DecodeState state() const noexcept;
    // Results remain unavailable until Complete (acquire publication).
    const std::vector<std::uint8_t>& output() const;
    const Yz2Statistics& statistics() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    void decodeBackground(NativeScheduler& scheduler);
    void finishBackground(BackgroundOutcome outcome) noexcept;
    friend std::optional<TaskHandle> startYz2Decode(NativeScheduler&,
        std::shared_ptr<Yz2Decoder>, ModuleToken);
};
// Typed background adapter for the decodeData -> Yz2DecodeExec -> iTaskExit
// portion. NOT a replacement for decodeData's unported room/heap integration.
std::optional<TaskHandle> startYz2Decode(NativeScheduler& scheduler,
    std::shared_ptr<Yz2Decoder> decoder, ModuleToken module = {});
} // namespace re4::port
