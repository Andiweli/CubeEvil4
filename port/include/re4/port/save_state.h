#pragma once
#include "re4_id_save_types.h"
#include <memory>
#include <string>
#include <vector>

namespace re4::port {
class GameplayHeaps;
class NativeScheduler;
struct NativeSaveImage {
    std::shared_ptr<void> owner;
    SAVE_DATA_HEAD *head{};
    u64 generation{};
};
struct SaveStateSnapshot {
    u32 rooms{}, savedRooms{}, roomBytes{}, imageBytes{};
    std::vector<u16> roomIds;
    std::vector<u32> sectionOffsets;
    bool initialized{};
};
// Native in-memory save image, not a GameCube memory-card wire image. Global,
// merchant, inventory and SubScreen aggregate save/load are not yet bound.
// No cGameSave::save/load success is advertised by this limited startup service.
class SaveStateRuntime final {
  public:
    SaveStateRuntime(GameplayHeaps &, NativeScheduler &);
    ~SaveStateRuntime();
    void initialize(); // Original room tables + cGameSave::alloc Title prerequisite.
    void clear();
    SaveStateSnapshot snapshot() const;
    NativeSaveImage image() const;
    NativeSaveImage copyImage(const NativeSaveImage &); // Original checkAddr rebase.
    void offsets(const NativeSaveImage &);
    void addresses(const NativeSaveImage &);
    void saveRooms(const NativeSaveImage &);
    void loadRooms(const NativeSaveImage &);
    void clearRooms(const NativeSaveImage &);
    int passed(u16 room, int part) const;
    void setPassed(u16 room, int part);
    const std::vector<std::string> &trace() const;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
