#pragma once
#include "re4/port/engine_loader.h"
#include "re4_subscreen_types.h"
#include "re4_runtime_types.h"

namespace re4::port {
class GameplayHeaps;
struct SubScreenSnapshot {
    std::string filename;
    std::uint32_t freeOffset=0, moduleOffset=0, commonOffset=0, puzzleOffset=0, moduleAram=0;
    std::uint8_t boardSize=0, boardNext=0, mapMode=0;
    int open=0, attributes=0, close=0, wait=0;
    bool initialized=false, mapArrayEmpty=true;
};
struct SubScreenResource {
    FileIdentity identity;
    std::uint32_t aramAddress=0, dmaBytes=0;
    ResourceLease bytes; // Actual DMA readback, owned independently of the ARAM bank.
};
// DOL startup path only. Opening Sscrn.rel, rendering widgets and inventory
// gameplay remain unavailable; no native menu replaces the original module.
class SubScreenRuntime final {
  public:
    SubScreenRuntime(EngineLoader &, GameplayHeaps &, NativeGameState &, std::uint8_t &language,
                     std::shared_ptr<const ImportedFiles>, std::uint8_t disc);
    ~SubScreenRuntime();
    void initializeInventory(); // Original Title_task dependency: cItemMgr::init, once per boot.
    void gameInit(); // Actual SubScreenGameInit; stackful owner task required.
    void roomInit();
    void wait(int frames);
    void clear(); // Full native reboot, frame boundary, never an in-game SubScreenExit substitute.
    SubScreenSnapshot snapshot() const;
    std::vector<SubScreenResource> resources() const;
    ITEM_INFO itemInfo(std::uint16_t id) const;
    const cItem *findItem(std::uint16_t id) const;
    int getItem(std::uint16_t id,int count=0); // Actual original get; no generated start inventory.
    void clearInventory();
    void inventoryRoomInit();
    int itemSlot(const cItem *) const;
    int itemCapacity() const;
    std::string mapFilename(int area) const; // Actual original mapAreaFilename.
    const std::vector<std::string> &trace() const;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
