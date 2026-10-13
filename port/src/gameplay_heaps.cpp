#include "re4/port/game_runtime.h"
#include <algorithm>
#include <cstring>
#include <new>

namespace re4::port {
namespace {
std::size_t aligned(std::size_t bytes) {
    if (bytes > SIZE_MAX - 31)
        throw std::overflow_error("heap allocation size");
    return (bytes + 31) & ~std::size_t(31);
}
void heapIndex(int n) {
    if (n < 0 || n >= 13)
        throw std::out_of_range("original heap index 0..12");
}
} // namespace
struct GameplayHeaps::Impl {
    struct Arena {
        void *data;
        explicit Arena(std::size_t bytes) : data(::operator new(bytes, std::align_val_t(32))) {}
        ~Arena() { ::operator delete(data, std::align_val_t(32)); }
    };
    struct Block {
        std::shared_ptr<Arena> arena;
        std::size_t first, end;
        int heap;
    };
    struct Heap {
        std::size_t first = 0, end = 0;
        bool created = false, suspended = false;
    };
    std::size_t bytes;
    int current = 0;
    std::shared_ptr<Arena> arena;
    std::array<Heap, 13> heaps{};
    std::vector<std::weak_ptr<Block>> blocks;
    const std::thread::id owner = std::this_thread::get_id();
    explicit Impl(std::size_t size) : bytes(aligned(size)) {
        if (!size || size > 1024ULL * 1024 * 1024)
            throw std::invalid_argument("heap arena size");
        reset();
    }
    void check() const {
        if (owner != std::this_thread::get_id())
            throw std::logic_error("gameplay heap owner thread");
    }
    void reset() {
        auto next = std::make_shared<Arena>(bytes);
        heaps = {};
        heaps[0] = {0, bytes, true, false};
        arena = std::move(next);
        blocks.clear();
        current = 0;
    }
    std::vector<std::shared_ptr<Block>> live() {
        std::vector<std::shared_ptr<Block>> result;
        for (auto it = blocks.begin(); it != blocks.end();) {
            if (auto p = it->lock()) {
                result.push_back(std::move(p));
                ++it;
            } else
                it = blocks.erase(it);
        }
        return result;
    }
};
GameplayHeaps::GameplayHeaps(std::size_t bytes) : impl_(std::make_unique<Impl>(bytes)) {}
GameplayHeaps::~GameplayHeaps() = default;
GameplayHeaps::Allocation GameplayHeaps::allocate(std::size_t size, int heap, bool clear) {
    impl_->check();
    if (heap == 13)
        heap = impl_->current;
    heapIndex(heap);
    if (!size)
        return {};
    const auto &h = impl_->heaps[heap];
    if (!h.created || h.suspended)
        throw std::logic_error("allocation from missing/suspended heap");
    auto bytes = aligned(size);
    auto blocks = impl_->live();
    std::sort(blocks.begin(), blocks.end(),
              [](const auto &a, const auto &b) { return a->first < b->first; });
    auto first = h.first;
    for (const auto &b : blocks) {
        if (b->end <= first || b->first >= h.end)
            continue;
        if (b->first >= first && bytes <= b->first - first)
            break;
        first = std::max(first, b->end);
    }
    if (first > h.end || bytes > h.end - first)
        throw std::bad_alloc();
    auto block =
        std::make_shared<Impl::Block>(Impl::Block{impl_->arena, first, first + bytes, heap});
    auto *ptr = static_cast<std::uint8_t *>(impl_->arena->data) + first;
    if (clear)
        std::memset(ptr, 0, size); // Raw allocation, never a constructed native object.
    impl_->blocks.push_back(block);
    return {std::move(block), ptr, size};
}
bool GameplayHeaps::replace(int from, int to) {
    impl_->check();
    heapIndex(from);
    heapIndex(to);
    if (from == to)
        throw std::invalid_argument("heap replacement aliases itself");
    auto &old = impl_->heaps[from];
    auto &next = impl_->heaps[to];
    if (old.suspended || next.suspended)
        return false;
    auto first = next.first, end = next.end;
    const auto blocks = impl_->live();
    if (old.created) {
        first = old.first;
        end = old.end;
        for (const auto &b : blocks)
            if (b->heap == from)
                first = std::max(first, b->end);
    }
    if (first >= end)
        return false;
    // Native safety: an existing target lease must not be overwritten/reused.
    for (const auto &b : blocks)
        if (b->first < end && first < b->end)
            return false;
    old.created = false;
    next = {first, end, true, false};
    return true;
}
bool GameplayHeaps::setCurrent(int heap) {
    impl_->check();
    heapIndex(heap);
    if (!active(heap))
        return false;
    impl_->current = heap;
    return true;
}
int GameplayHeaps::current() const {
    impl_->check();
    return impl_->current;
}
void GameplayHeaps::suspend(int heap) {
    impl_->check();
    heapIndex(heap);
    impl_->heaps[heap].suspended = true;
}
void GameplayHeaps::resume(int heap) {
    impl_->check();
    heapIndex(heap);
    impl_->heaps[heap].suspended = false;
}
bool GameplayHeaps::active(int heap) const {
    impl_->check();
    heapIndex(heap);
    const auto &h = impl_->heaps[heap];
    return h.created && !h.suspended;
}
void GameplayHeaps::reset() {
    impl_->check();
    impl_->reset();
}
std::size_t GameplayHeaps::liveBytes() const {
    impl_->check();
    std::size_t result = 0;
    for (const auto &b : impl_->live())
        result += b->end - b->first;
    return result;
}
} // namespace re4::port
