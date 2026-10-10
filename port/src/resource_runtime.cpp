#include "loader_original.h"
#include <algorithm>
#include <cstdlib>
#include <type_traits>

namespace re4::port::loading {
std::uintptr_t nativeAddress(const void *p) { return reinterpret_cast<std::uintptr_t>(p); }
u32 alignedSize(u32 bytes) {
    if (bytes > 0xffffffe0u)
        throw std::invalid_argument("resource size alignment overflow");
    return (bytes + 31u) & ~31u;
}
u32 aramOffset(const void *p) {
    auto n = nativeAddress(p);
    if (!n)
        return 0;
    if (n < ARAM_FREE_BASE || n >= context().limits.aramEnd)
        throw std::invalid_argument("MRAM pointer used as logical ARAM offset");
    return static_cast<u32>(n);
}
void *aramPointer(u32 offset) {
    if (!offset)
        return nullptr;
    validateAram(offset, 0);
    return reinterpret_cast<void *>(static_cast<std::uintptr_t>(offset));
}
void validateAram(std::uintptr_t address, u32 bytes) {
    // sscrn.cpp reserves a distinct 3 MiB bank. General DC free-space code
    // remains bounded by limits.aramEnd. Never cross the arena/bank boundary.
    const auto end = address >= 0xD00000u ? 0x1000000u : context().limits.aramEnd;
    if (address < ARAM_FREE_BASE || address >= end || (address & 31u) || alignedSize(bytes) > end-address)
        throw std::invalid_argument("ARAM range/alignment exceeds data arena");
}
void validateMram(const void *p, u32 bytes) {
    auto &c = context();
    auto address = nativeAddress(p);
    for (const auto &a : c.allocations) {
        auto base = nativeAddress(a.first);
        if (address >= base && address - base <= a.second.bytes &&
            bytes <= a.second.bytes - (address - base))
            return;
    }
    throw std::invalid_argument("MRAM span is not a bounded native reservation");
}
void checkedMove(void *dst, const void *src, u32 bytes) {
    validateMram(dst, bytes);
    validateMram(src, bytes);
    // Relocating inside the unit's own allocation and then releasing its base
    // would invalidate the destination. Require a distinct reservation.
    for (const auto &a : context().allocations) {
        auto base = nativeAddress(a.first);
        auto within = [&](const void *p) {
            auto n = nativeAddress(p);
            return n >= base && n - base < a.second.bytes;
        };
        if (within(src) && within(dst) && src != dst)
            throw std::invalid_argument("MRAM relocation needs a distinct reservation");
    }
    std::memmove(dst, src, bytes);
}
void *allocate(u32 bytes) {
    auto &c = context();
    const auto padded = alignedSize(bytes);
    if (padded > c.limits.mramBytes || *c.allocatedBytes > c.limits.mramBytes - padded)
        return nullptr;
    auto *base = std::calloc(static_cast<std::size_t>(padded) + 32, 1);
    if (!base)
        return nullptr;
    auto *p = reinterpret_cast<void *>((nativeAddress(base) + 31) & ~std::uintptr_t{31});
    *c.allocatedBytes += padded;
    auto count = c.allocatedBytes;
    std::shared_ptr<void> storage(p, [base, count, padded](void *) {
        std::free(base);
        *count -= padded;
    });
    c.allocations.emplace(p, Context::Allocation{bytes, std::move(storage)});
    return p;
}
void release(void *p) {
    if (!p)
        return;
    if (!context().allocations.erase(p))
        throw std::logic_error("foreign/double-free MRAM pointer");
}
static std::size_t slot(cDataUnit *u) {
    for (std::size_t i = 0; i < 32; ++i)
        if (DC.nativeUnit(i) == u)
            return i;
    throw std::logic_error("foreign data unit");
}
void resourceRegistered(cDataUnit *u) {
    auto i = slot(u);
    context().resourceGenerations[i] = ++context().generation;
    context().unitFiles[i].reset();
    context().resourceErrors[i] = 0;
}
void unitFileReady(cDataUnit *u) { context().unitFiles[slot(u)] = context().lastFile; }
void unitFileCleared(cDataUnit *u) { context().unitFiles[slot(u)].reset(); }
void nativeUnitFault(cDataUnit *u) {
    // A failed command remains diagnosable and does not repeatedly allocate.
    if (!u->getErr())
        u->setErr(3);
    context().resourceErrors[slot(u)] = u->getErr();
    u->setCommand(CMND_NONE, nullptr, 0);
}
void checkAramRequest(int type, std::uintptr_t src, std::uintptr_t dst, u32 bytes) {
    if (type != 0 && type != 1)
        throw std::invalid_argument("invalid ARQ transfer direction");
    validateAram(type == 0 ? dst : src, bytes);
    validateMram(reinterpret_cast<const void *>(type == 0 ? src : dst), bytes);
}
void cAram::DmaTrans(cAramQueue *q, int mode) {
    checkAramRequest(static_cast<int>(q->type), q->src, q->dst, q->len);
    const auto address = q->type == 0 ? q->dst : q->src;
    const auto needed = address + alignedSize(q->len) - ARAM_FREE_BASE;
    if (context().aramStorage.size() < needed)
        context().aramStorage.resize(std::max<std::size_t>(needed,context().limits.aramEnd-ARAM_FREE_BASE));
    if (mode & 1) {
        while (!q->CkFlag(0x04000000))
            advance(context().limits.transferBytesPerFrame);
        q->PushQueue();
    }
}
void cAram::watcher() {
    if (!pCur_queue && pQueue_list) {
        pCur_queue = pQueue_list;
        pQueue_list = pCur_queue->next;
        pCur_queue->next = nullptr;
        DmaTrans(pCur_queue, 0);
    }
}
void cAram::advance(std::size_t budget) {
    auto *q = pCur_queue;
    if (!q)
        return;
    const auto n = std::min<std::size_t>(budget, q->len - q->cursor);
    auto address = q->type == 0 ? q->dst : q->src;
    auto *aram = context().aramStorage.data() + address - ARAM_FREE_BASE + q->cursor;
    auto *mram = reinterpret_cast<u8 *>(q->type == 0 ? q->src : q->dst) + q->cursor;
    if (n) {
        if (q->type == 0)
            std::memcpy(aram, mram, n);
        else
            std::memcpy(mram, aram, n);
    }
    q->cursor += n;
    if (q->cursor == q->len) {
        // CPU backing copies logical bytes, never reads unowned DMA padding.
        if (q->type == 0 && alignedSize(q->len) > q->len)
            std::memset(context().aramStorage.data() + q->dst - ARAM_FREE_BASE + q->len, 0,
                        alignedSize(q->len) - q->len);
        q->SetFlag(0x04000000);
        pCur_queue = nullptr;
    }
}
void waitAram(int id) {
    // Called only by explicit original blocking waits. Ordinary frames never spin.
    while (Aram.TransCheck(id) == 0) {
        Aram.watcher();
        Aram.advance(context().limits.transferBytesPerFrame);
    }
}
int cAram::DmaCancel(int id) {
    if (id < 0 || id >= 16)
        return -1;
    auto *q = &AramQueue[id];
    if (q == pCur_queue) {
        // Owner-only CPU transfer has no hardware operation still writing. Detach
        // before setClear frees MRAM; partially written ARAM is never published.
        pCur_queue = nullptr;
        q->PushQueue();
        return 1;
    }
    auto **link = &pQueue_list;
    while (*link && *link != q)
        link = &(*link)->next;
    if (*link) {
        *link = q->next;
        q->PushQueue();
        return 1;
    }
    q->PushQueue();
    return 0;
}
void cAram::DmaCancelAll() {
    pCur_queue = pQueue_list = nullptr;
    for (auto &q : AramQueue)
        q.PushQueue();
}
std::string fileCacheKey(const cDvdQueue &q) {
    return std::to_string(context().disc) + ":" + q.m_Name + ":" +
           std::to_string(q.request.mode & 0x8000) + ":" + std::to_string(q.request.offset) + ":" +
           std::to_string(q.request.size);
}
template <class T> void trim(std::map<std::string, T> &cache, std::uint64_t limit) {
    auto size = [](const T &e) {
        if constexpr (std::is_same_v<T, Context::FileCacheEntry>)
            return e.file->bytes.size();
        else
            return e.body->size();
    };
    std::uint64_t total = 0;
    for (const auto &e : cache)
        total += size(e.second);
    while (total > limit && !cache.empty()) {
        auto victim =
            std::min_element(cache.begin(), cache.end(), [](const auto &a, const auto &b) {
                return a.second.used < b.second.used;
            });
        total -= size(victim->second);
        cache.erase(victim);
    }
}
void cacheFile(cDvdQueue &q) {
    auto &c = context();
    if (q.result->bytes.size() > c.limits.fileCacheBytes)
        return;
    c.fileCache[fileCacheKey(q)] = {q.result, ++c.cacheClock};
    trim(c.fileCache, c.limits.fileCacheBytes);
}
void cacheDecoded() {
    auto &c = context();
    if (c.decoded->size() > c.limits.decodedCacheBytes)
        return;
    c.decodedCache[c.lastFile->identity.sha256] = {c.decoded, ++c.cacheClock};
    trim(c.decodedCache, c.limits.decodedCacheBytes);
}
// Data-only boundary: BLOCK_CREATE is reserved for actual BlockCreate/ObjMgr.
// No models exist in Phase 3c; active data remains BLOCK_MRAM_LOAD + MRAM_OK.
void resourceBlockCommands() {
    auto &b = Block;
    for (auto &u : b.units)
        if (u.flags & 1) {
            if (u.command == BLOCK_CMD_MRAM_LOAD)
                u.setBlockLoadToMram();
            else if (u.command == BLOCK_CMD_ARAM_LOAD) {
                if (u.state == BLOCK_NO_DATA ||
                    (u.state == BLOCK_MRAM_LOAD && u.pData->isUseOk())) {
                    u.state = BLOCK_ARAM_LOAD_SET;
                    u.command = BLOCK_CMD_NONE;
                } else if (u.state == BLOCK_ARAM_OK)
                    u.command = BLOCK_CMD_NONE;
            } else if (u.command == BLOCK_CMD_DELETE && u.pData->isLoadOk()) {
                u.state = BLOCK_DELETE;
                u.command = BLOCK_CMD_NONE;
            }
        }
}
void *cBlock::getBlockMemFree(u32) {
    throw std::logic_error("Block model pool requires Phase 4 binding");
}
void resourceBlockConditions() {
    for (auto &u : Block.units)
        if (u.flags & 1) {
            switch (u.state) {
            case BLOCK_MRAM_LOAD_SET:
                u.checkBlockLoadToMramSet();
                break;
            case BLOCK_ARAM_LOAD_SET:
                u.checkBlockLoadToAramSet();
                break;
            case BLOCK_ARAM_LOAD:
                u.checkBlockLoadToAram();
                break;
            case BLOCK_DELETE:
                u.checkBlockDelete();
                break;
            default:
                break;
            }
        }
}
} // namespace re4::port::loading
