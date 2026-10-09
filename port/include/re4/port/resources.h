#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace re4::port {
// Values are the original DATA_CONDITION codes, never a serialized native layout.
enum class ResourceState : std::uint8_t {
    NoData,
    MramLoading,
    MramReady,
    AramLoading,
    AramReady,
    AramToMram,
    MramToAram,
    AramToAram,
    MramToMram
};
enum class Residency { Mram, Aram };
struct ResourceHandle {
    std::uint64_t runtime = 0, generation = 0;
    std::uint8_t slot = 0;
};
struct ResourceReference {
    ResourceHandle resource;
    std::uint32_t offset = 0, size = 0;
};
struct MemoryReservation {
    // Shared ownership pins a reservation even after its unit is cleared/moved.
    std::shared_ptr<void> owner;
    std::uint8_t *data = nullptr;
    std::uint32_t size = 0;
    std::uint64_t runtime = 0;
};
struct ResourceLease {
    std::shared_ptr<const void> owner;
    const std::uint8_t *data = nullptr;
    std::uint32_t size = 0;
};
struct ResourceInfo {
    ResourceState state = ResourceState::NoData;
    int command = 0, error = 0;
    std::uint32_t size = 0, aramAddress = 0;
    std::string name;
};
struct MemoryLimits {
    std::uint32_t mramBytes = 64 * 1024 * 1024;
    std::uint32_t fileCacheBytes = 16 * 1024 * 1024;
    std::uint32_t decodedCacheBytes = 16 * 1024 * 1024;
    // sndInit fixes ARAM_FREE_BASE=0x800000; datactrl defines ARAM_END=0xD00000.
    std::uint32_t aramEnd = 0xD00000;
    std::uint32_t transferBytesPerFrame = 64 * 1024;
};
struct ResourceMemoryStats {
    std::uint64_t mramBytes = 0, fileCacheBytes = 0, decodedCacheBytes = 0;
    std::uint64_t fileCacheHits = 0, decodedCacheHits = 0, fileReads = 0;
    std::uint32_t aramBackingBytes = 0, aramReservedBytes = 0, transfers = 0;
};
struct BlockResource {
    std::uint8_t number = 0;
    ResourceHandle resource;
    // The original block state is reported separately. Data readiness never
    // claims that BlockCreate/ObjMgr/model activation have run.
    int blockState = 0;
};
} // namespace re4::port
