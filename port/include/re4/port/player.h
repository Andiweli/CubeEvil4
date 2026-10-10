#pragma once
#include "re4/port/consumers.h"
#include <memory>

namespace re4::port {
class GameplayHeaps;
struct PlayerInitSnapshot {
    u8 kaiou{}, oldWeaponId{};
    bool weaponSlotEmpty{};
};
struct PlayerMotionBinding { u32 motionIndex{}, archiveSlot{}; ResourceLease bytes; };
// PlayerInit is the original global-state reset, not cPlLeon construction.
// The asset entry runs original Leon model/motion helpers on an owned CPU model;
// it does not publish pPL or bypass weapon/collision/startUp dependencies.
class PlayerRuntime final {
public:
    PlayerRuntime(NativeGameState &, KEY &, u8 &padType, GameplayHeaps &,
                  EngineConsumers &, NativeScheduler &);
    ~PlayerRuntime();
    void gameInit();
    void lifeReset();
    void keyConfig(); // Explicit original actor consumer of the existing central KEY.
    void clear(); // Quiescent lifetime boundary; no reset of gameplay flags.
    PlayerInitSnapshot snapshot() const;
    const std::vector<std::string> &trace() const;
    // Native ownership boundary for future original ReadWepData allocations.
    // No archive loading, REL linking or weapon behavior is claimed here.
    void *allocateWeaponData(u32 bytes, bool debugHeap = false);
    ConsumerHandle initializeLeonAssets(ResourceLease playerArchive);
    ConsumerHandle leonModel() const;
    const std::vector<PlayerMotionBinding> &motionBindings() const;
    std::vector<ModelInfoView *> leonModelInfos() const;
    ModelInfoView *knife() const;
    ModelInfoView *face() const;
    void setHands(std::intptr_t right, std::uintptr_t left);
    void releaseLeonAssets();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
