#pragma once
#include "re4/port/objects.h"
#include "re4_object_types.h"
namespace re4::port {
struct EffectDataSnapshot {
    u32 owner{},loads{};std::string sourceHash;
    std::vector<u32> textureIds,textureFrames,modelIds;
    u32 estRecords{},sstRecords{},pathRecords{},textureObjects{};
};
struct EffectSnapshot {bool configured{};u32 sprites{},generators{},liveSprites{},pendingGenerators{};};
// Typed lifecycle views of the named fields consumed by the original delete
// functions. These are never full GC cEsp/cEspgen byte-layout overlays.
struct EffectSprite {
    NativeEffectCore info{};
    u8 m_Be_flg{},m_Id{};
    ModelCpu *m_pMod{};
    u32 m_Guid_pMod{};
    EffectSprite();
    virtual ~EffectSprite();
    virtual void Destruct();
};
struct EffectGenerator {NativeEffectCore Eff_core{};u8 Be_flg{},Id{};};
class EffectRuntime final {
public:
    EffectRuntime(GameplayHeaps &,NativeScheduler &,ObjectRuntime &);
    ~EffectRuntime();
    void configurePools(u32 sprites,u32 generators);
    void clear();
    EffectSnapshot snapshot() const;
    int loadData(ResourceLease,u32 owner,int multipleOK);
    int releaseData(u32 owner,int countCheck,int warn);
    EffectDataSnapshot dataSnapshot(u32 owner) const;
    void deleteSprites(int flag,int kind,void *owner,ModelCpu *model=nullptr);
    void deleteGenerators(int flag,int kind,void *owner);
    void deleteModels(int flag,int kind,void *owner);
    // Only numeric verification fixtures use these. No EstSet/spawning or
    // gameplay success is implied; unsupported concrete destructors reject.
    EffectSprite &spriteFixture(u32);
    EffectGenerator &generatorFixture(u32);
    NativeEffectCore &modelCoreFixture(ObjectHandle);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
