#pragma once
#include "re4/port/consumers.h"
#include <memory>

namespace re4::port {
class GameplayHeaps;
class ObjectRuntime;class EffectRuntime;
// Constructor prefix only: named native cEm model/damage fields. This is not
// a cPlayer/cPlLeon, an EM_WORK_SIZE overlay, or a published gameplay actor.
struct ActorDamageCpu {
    u8 m_Flag{},m_Timer{},m_Wep{},m_Padding03{};
    Vec m_PosFrom{};
    f32 m_Dist{};
    struct DamagePart *m_pDamageYarare{};
    ActorDamageCpu();
    void clear();
    void set(int,int,u8,Vec*,f32,DamagePart*);
    void set(int,int);
    void move();
};
struct DamagePart {
    Vec offset{},cross{};
    f32 radius{},height{},extent{};
    u16 flag{};
    s16 parts_no{};
    f32 len{},c_dis{};
    DamagePart *pList{};
};
struct ActorModelCpu : ModelCpu {
    ActorDamageCpu dmg;
    s16 hp{},hp_max{};
    ActorModelCpu();
    int initWork();
};
struct PlayerInitSnapshot {
    u8 kaiou{}, oldWeaponId{};
    bool weaponSlotEmpty{};
};
struct PlayerMotionBinding { u32 motionIndex{}, archiveSlot{}; ResourceLease bytes; };
struct LeonConstructionSnapshot {
    bool returnedActor{},playerConstructor{},subobjects{},modelInitialized{},rolledBack{};
    bool schedulerTask{};
    bool weaponReleased{};
    s16 hp{};
    u32 motionSlots{},registeredMotionSlots{},debugCallbacks{};
    u32 parts{},modelInfos{};
    std::string blocker;
};
// PlayerInit is the original global-state reset, not cPlLeon construction.
// The asset entry runs original Leon model/motion helpers on an owned CPU model;
// it does not publish pPL or bypass weapon/collision/startUp dependencies.
class PlayerRuntime final {
public:
    PlayerRuntime(NativeGameState &, KEY &, u8 &padType, GameplayHeaps &,
                  EngineConsumers &, NativeScheduler &,ObjectRuntime &,EffectRuntime &);
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
    void applyWaist(f32 radians); // Original cPlBody::waistSet/move, not a gameplay decision.
    void releaseLeonAssets();
    // Executes the preserved construction sequence. Missing consumers throw;
    // a failed attempt does not publish a player or claim a complete actor.
    ConsumerHandle constructLeon(ResourceLease playerArchive);
    LeonConstructionSnapshot constructionSnapshot() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
