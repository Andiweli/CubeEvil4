#include "re4_scheduler_original.h"
#include "yz2_internal.h"
#include <limits>

namespace re4::port {
namespace {
std::uint8_t *aligned32(std::vector<std::uint8_t> &buffer) {
    auto address = reinterpret_cast<std::uintptr_t>(buffer.data());
    return reinterpret_cast<std::uint8_t *>((address + 31u) & ~std::uintptr_t{31});
}
bool space(std::uint8_t c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
int hex(std::uint8_t c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
// Validate the two strtoul fields inside the supplied byte span before calling
// the original parser. Their first field is NOT used as a guessed stream bound.
std::uint32_t field(const std::uint8_t *data, std::size_t bytes, std::size_t &p) {
    while (p < bytes && space(data[p]))
        ++p;
    if (p < bytes && data[p] == '+')
        ++p;
    if (bytes - p >= 2 && data[p] == '0' && (data[p + 1] == 'x' || data[p + 1] == 'X'))
        p += 2;
    const auto begin = p;
    std::uint64_t value = 0;
    while (p < bytes && hex(data[p]) >= 0) {
        value = value * 16 + static_cast<unsigned>(hex(data[p++]));
        if (value > std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("Yz2 header exceeds original u32 width");
    }
    if (p == begin || p == bytes)
        throw std::runtime_error("Truncated/invalid Yz2 header");
    return static_cast<std::uint32_t>(value);
}
void checkpoint(void *scheduler, u32 units) {
    static_cast<NativeScheduler *>(scheduler)->backgroundCheckpoint(units);
}
} // namespace
struct Yz2Decoder::Impl {
    std::vector<u8> inputStorage, workStorage;
    std::shared_ptr<std::vector<u8>> result = std::make_shared<std::vector<u8>>();
    detail::Yz2InEv ev{};
    Yz2Statistics stats;
    std::atomic<DecodeState> state{DecodeState::Prepared};
    Impl(const u8 *input, std::size_t bytes, u32 maximum) {
        if (!input || !bytes || bytes > std::numeric_limits<u32>::max() ||
            bytes > std::numeric_limits<std::size_t>::max() - 32)
            throw std::invalid_argument("Invalid Yz2 input span");
        std::size_t p = 0;
        field(input, bytes, p);
        ++p; // Yz2DecodeSet consumes exactly one separator byte.
        const auto unpacked = field(input, bytes, p);
        if (unpacked > maximum)
            throw std::runtime_error("Yz2 output exceeds configured limit");
        if (p > std::numeric_limits<std::size_t>::max() - 32 ||
            ((p + 32) & ~std::size_t{31}) >= bytes)
            throw std::runtime_error("Yz2 aligned stream is outside the supplied span");
        inputStorage.resize(bytes + 32);
        auto *source = aligned32(inputStorage);
        std::memcpy(source, input, bytes);
        // A trailing zero makes the already validated strtoul reads safe.
        source[bytes] = 0;
        workStorage.resize(detail::yz2WorkBytes + 31);
        auto *work = aligned32(workStorage);
        ev.srcEnd = source + bytes;
        ev.workEnd = work + detail::yz2WorkBytes;
        ev.statistics = &stats;
        const auto parsed = detail::Yz2DecodeSet(reinterpret_cast<char *>(source), work, ev);
        if (parsed != unpacked)
            throw std::runtime_error("Yz2 header validation disagrees with original parser");
        result->resize(unpacked);
    }
    void run(detail::Yz2Checkpoint hook) {
        ev.checkpoint = hook;
        detail::Yz2DecodeExec(result->data(), ev);
        stats.inputBytes = static_cast<u32>(ev.src - aligned32(inputStorage));
    }
};
Yz2Decoder::Yz2Decoder(const u8 *data, std::size_t bytes, u32 maximum)
    : impl_(std::make_unique<Impl>(data, bytes, maximum)) {}
Yz2Decoder::~Yz2Decoder() = default;
DecodeState Yz2Decoder::state() const noexcept {
    return impl_->state.load(std::memory_order_acquire);
}
const std::vector<u8> &Yz2Decoder::output() const {
    if (state() != DecodeState::Complete)
        throw std::logic_error("Yz2 result is not published");
    return *impl_->result;
}
const Yz2Statistics &Yz2Decoder::statistics() const {
    if (state() != DecodeState::Complete)
        throw std::logic_error("Yz2 statistics are not published");
    return impl_->stats;
}
std::shared_ptr<const std::vector<u8>> Yz2Decoder::sharedOutput() const {
    if (state() != DecodeState::Complete)
        throw std::logic_error("Yz2 result is not published");
    return impl_->result;
}
void Yz2Decoder::decode() {
    auto expected = DecodeState::Prepared;
    if (!impl_->state.compare_exchange_strong(expected, DecodeState::Processing))
        throw std::logic_error("Yz2 decoder is single-use");
    try {
        impl_->run({nullptr, nullptr});
    } catch (...) {
        impl_->state.store(DecodeState::Failed, std::memory_order_release);
        throw;
    }
    impl_->state.store(DecodeState::Complete, std::memory_order_release);
}
void Yz2Decoder::decodeBackground(NativeScheduler &scheduler) {
    impl_->run({checkpoint, &scheduler});
}
void Yz2Decoder::finishBackground(BackgroundOutcome outcome) noexcept {
    const auto result = outcome == BackgroundOutcome::Completed   ? DecodeState::Complete
                        : outcome == BackgroundOutcome::Cancelled ? DecodeState::Cancelled
                                                                  : DecodeState::Failed;
    impl_->state.store(result, std::memory_order_release);
}
std::optional<TaskHandle> startYz2Decode(NativeScheduler &scheduler,
                                         std::shared_ptr<Yz2Decoder> decoder, ModuleToken module) {
    if (!decoder)
        throw std::invalid_argument("Null Yz2 job");
    auto expected = DecodeState::Prepared;
    if (!decoder->impl_->state.compare_exchange_strong(expected, DecodeState::Processing))
        throw std::logic_error("Yz2 decoder is single-use");
    try {
        auto task = scheduler.execBackground(
            [decoder, &scheduler] {
                decoder->decodeBackground(scheduler);
                original::iTaskExit();
            },
            module, [decoder](BackgroundOutcome outcome) { decoder->finishBackground(outcome); });
        if (!task)
            decoder->impl_->state.store(DecodeState::Prepared, std::memory_order_release);
        return task;
    } catch (...) {
        decoder->impl_->state.store(DecodeState::Prepared, std::memory_order_release);
        throw;
    }
}
} // namespace re4::port
