#pragma once
#include "re4/port/asset_index.h"
#include "re4/port/resources.h"
#include "re4/port/scheduler.h"
#include <memory>
#include <string>
#include <vector>
namespace re4::port {
struct FileIdentity {
    std::uint8_t disc;
    std::uint32_t fstIndex, size;
    std::string path, sha256;
};
class ImportedFiles final {
  public:
    // A private, versioned catalog generated from Phase 3a; never a guessed path map.
    ImportedFiles(std::string root, std::string catalog);
    const FileIdentity *find(std::uint8_t disc, const std::string &path) const;
    std::string blobPath(const FileIdentity &identity) const;
    const std::vector<FileIdentity> &entries() const { return entries_; }

  private:
    std::string root_;
    std::vector<FileIdentity> entries_;
};
struct LoadRequest {
    std::uint64_t runtime = 0, generation = 0;
    std::uint8_t slot = 0, id = 0;
};
enum class LoadStatus { Pending, Reading, Complete, Cancelled, Failed, Released };
enum class RoomLoadStage { Idle, Reading, Decoding, Blocks, Ready, Cancelled, Failed };
struct LoadedFile {
    FileIdentity identity;
    std::vector<std::uint8_t> bytes; // The complete verified original file, including sound.
    std::uint32_t bodyOffset = 0, bodySize = 0, soundOffset = 0, soundSize = 0;
};
struct RoomArchive {
    std::uint16_t id = 0;
    std::shared_ptr<const LoadedFile> source;
    std::shared_ptr<const std::vector<std::uint8_t>> body;
    AssetIndex index;
    std::vector<std::shared_ptr<const LoadedFile>> blocks;
    std::vector<AssetIndex> blockIndices;
    const AssetSpan *find(const char *tag, std::uint32_t occurrence = 0) const {
        return index.find(tag, occurrence);
    }
};
class EngineLoader final {
  public:
    explicit EngineLoader(std::shared_ptr<const ImportedFiles> files, std::uint8_t disc = 0,
                          std::function<void(std::uint64_t)> ioProgressObserver = {},
                          MemoryLimits limits = {});
    ~EngineLoader();
    std::uint8_t disc() const;
    // Imported immutable discs replace the hardware lid prompt. Queued reads
    // must finish before switching; existing leases keep their disc identity.
    void selectDisc(std::uint8_t);
    EngineLoader(const EngineLoader &) = delete;
    EngineLoader &operator=(const EngineLoader &) = delete;
    // DvdReadN -> original ReadReq/LinkQueue, one current read; lower priority first/FIFO ties.
    LoadRequest request(const std::string &path, std::uint16_t mode = 0x100,
                        std::uint16_t priority = 4, std::uint32_t offset = 0,
                        std::uint32_t length = 0, std::uint32_t aramAddress = 0);
    LoadRequest requestFile(std::uint16_t fileTableIndex, std::uint16_t mode = 0x100,
                            std::uint32_t offset = 0, std::uint32_t length = 0);
    LoadStatus status(LoadRequest request) const;
    int readCheck(LoadRequest request, std::shared_ptr<const LoadedFile> &result);
    bool cancel(LoadRequest request);
    void cancelAll();
    bool suspend(LoadRequest request);
    bool resume(LoadRequest request);
    // One original gameplay visit, Watcher, bounded slot 4, DC check (main.cpp order).
    void frame(BackgroundBudget budget = {});
    // Original ReadAreaData control flow runs as a stackful task. Optional BLK file
    // discovery/registration uses cBlock::getBlockWork and cDataCtrl::setData.
    bool loadRoom(std::uint16_t roomId, bool loadBlocks = true);
    bool roomBusy() const;
    std::shared_ptr<const RoomArchive> room() const;
    std::string roomError() const;
    RoomLoadStage roomStage() const;
    void cancelRoom();
    NativeScheduler &scheduler();
    std::size_t liveAllocations() const;
    std::vector<std::string> observedFunctions() const;
    // Original DC registration and command/condition machinery, with typed
    // boundaries instead of casting an ARM64 pointer to a GameCube u32.
    ResourceHandle registerResource(const std::string &path);
    ResourceInfo resourceInfo(ResourceHandle resource) const;
    void loadResource(ResourceHandle resource, Residency destination, bool wait = false,
                      const MemoryReservation &mram = {}, std::uint32_t aramAddress = 0);
    void clearResource(ResourceHandle resource);
    void deleteResource(ResourceHandle resource);
    MemoryReservation reserveMemory(std::uint32_t bytes);
    // Explicit synchronous original cAram DMA readback. Includes the reserved
    // SubScreen bank; DC allocations/compaction still end at aramEnd (0xD00000).
    MemoryReservation readAram(std::uint32_t address, std::uint32_t bytes);
    void releaseMemory(MemoryReservation &memory); // Reject while referenced by a live unit.
    ResourceReference reference(ResourceHandle resource, std::uint32_t offset,
                                std::uint32_t bytes) const;
    ResourceLease resolve(ResourceReference reference) const; // MRAM_READY only
    std::vector<BlockResource> blockResources() const;
    void setBlockResidency(std::uint8_t block, Residency destination);
    void clearBlock(std::uint8_t block);
    void setResourceControl(bool enabled, bool holdCommands = false);
    void setAramCompaction(bool enabled);
    ResourceMemoryStats memoryStats() const;
    void trimCaches(); // Drops cache ownership; live archives/leases remain valid.

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Android monotonic Choreographer timestamps, independent of display frequency.
// Count rational 60 Hz VI ticks, then original GetSystemVcnt (normally 2) per
// gameplay frame. No rendering or extra gameplay visit per display refresh.
class FrameViAdapter final {
  public:
    explicit FrameViAdapter(EngineLoader &loader, std::uint8_t systemVcnt = 2);
    explicit FrameViAdapter(std::function<void()> gameplayTick, std::uint8_t systemVcnt = 2);
    std::uint32_t onDisplayFrame(std::int64_t monotonicNanoseconds);
    void reset(); // Lifecycle pause discards wall time; no catch-up after resume.
    std::uint64_t gameplayFrames() const { return frames_; }
    std::uint64_t retraces() const { return ticks_; }

  private:
    std::function<void()> tick_;
    std::uint8_t vcnt_;
    std::int64_t previous_ = -1;
    std::uint64_t fraction_ = 0, ticks_ = 0, frames_ = 0, pending_ = 0;
};
// Lazy English MDT access: malformed original tables fail only on access. Returns
// a byte span; UTF-16 BE/control-code interpretation remains with the message system.
AssetSpan englishMessage(const std::uint8_t *mdt, std::size_t bytes, std::uint32_t message);
std::uint32_t englishMessageCount(const std::uint8_t *mdt, std::size_t bytes);
} // namespace re4::port
