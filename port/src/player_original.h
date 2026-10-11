#pragma once
#include "re4/port/game_runtime.h"
#include "re4_player_types.h"
#include <map>

namespace re4::port {
using cModelInfo = ModelInfoView;
using cModel = ModelCpu;
using cParts = ModelPart;
using cPlBody = PlayerBodyCpu;
using cObj = NativePlayerObject; // Opaque until the generic object owner is integrated.
void ShapeEnd(void *);
void SetOriginalShape(ModelInfoView *);
[[noreturn]] inline int ShapeSet(void *, int, void *, int) {
    throw MissingGameConsumer("ShapeSet/checked native morph data and shape playback");
}
inline void memclr_asm(ShapeKey *key, std::size_t size) {
    if(size!=sizeof(ShapeKey)) throw std::logic_error("native shape clear size");
    *key=ShapeKey{};
}
struct PlayerDomain {
    // A live generic object pool is not yet available. This boundary refuses
    // access; returning an invented empty list would bypass weaponRelease.
    struct ObjectBoundary {
        [[noreturn]] NativePlayerObject *getActiveWork() { throw MissingGameConsumer("cPlayer::weaponRelease/ObjMgr.getActiveWork (live generic object pool)"); }
        [[noreturn]] NativePlayerObject *getNext(NativePlayerObject*) { throw MissingGameConsumer("ObjMgr.getNext/live object lifetime"); }
        [[noreturn]] void destroy(NativePlayerObject*) { throw MissingGameConsumer("ObjMgr.destroy/weapon object lifetime"); }
        [[noreturn]] void destroyNow(NativePlayerObject*) { throw MissingGameConsumer("ObjMgr.destroyNow/weapon object lifetime"); }
        [[noreturn]] int id(NativePlayerObject*) { throw MissingGameConsumer("ObjMgr.id/live object lifetime"); }
    } ObjMgr;
    NativeGameState *pG;
    struct System { u8 &pad_type; } system;
    System *pSys=&system;
    KEY &Key;
    GameplayHeaps &heaps;
    EngineConsumers &models;
    NativeScheduler &scheduler;
    u8 PlKaiou{},oldWepId{},m_BbtnCnt{};
    NativePlayerModule WepReadModule{};
    using MODULE_DAT=NativePlayerModule;
    std::map<void *,GameplayHeaps::Allocation> allocations;
    std::vector<std::string> trace;
    ResourceLease archive;
    AssetIndex archiveIndex;
    std::optional<ConsumerHandle> modelHandle;
    std::shared_ptr<ActorModelCpu> actorModel;
    PlayerBodyCpu body{nullptr};
    PlayerBodyCpu *Body=&body;
    std::array<void *,0x6D> motionTable{}; // Original 0x1B4 / sizeof(GC pointer).
    void **m_MotTbl=motionTable.data();
    std::vector<PlayerMotionBinding> bindings;
    PlayerFlag<u32,PlayerActorFlag> stat;
    static constexpr auto F_NO_WEP_EFF=re4::port::F_NO_WEP_EFF;
    ActorModelCpu *pPL{},*pSUB{};
    void *subArc{};
    void **m_MotTbl2{};
    struct NativeMotionTable {
        GameplayHeaps::Allocation storage;
        void **entries{};
        u32 count{};
        bool empty() const { return !count; }
        u32 size() const { return count; }
        void **data() const { return entries; }
        void clear() { entries=nullptr;count=0;storage={}; }
    } actorMotions,registeredMotions;
    std::unique_ptr<PlayerWeaponCpu> weaponOwner;
    std::unique_ptr<PlayerBodyCpu> bodyOwner;
    std::unique_ptr<PlayerPushCpu> pushOwner;
    std::unique_ptr<PlayerWaistCpu> waistOwner;
    std::unique_ptr<PlayerMotionBaseCpu> motionBaseOwner;
    std::unique_ptr<PlayerMaho> mahoOwner;
    PlayerWeaponCpu *Wep{};
    PlayerPushCpu *Push{};
    PlayerWaistCpu *Waist{};
    PlayerMotionBaseCpu *MotBase{};
    PlayerMaho *pMaho{};
    bool constructing{};
    LeonConstructionSnapshot construction;
    struct ModelInfoFacade {
        PlayerDomain *domain;
        ModelInfoView *create(void *,void *);
    } ModInfoMgr{this};
    struct Log {
        [[noreturn]] void err(int,int,const char *message) { throw std::runtime_error(message); }
        std::vector<std::string> messages;
        void mes(int,int,const char *message) { messages.emplace_back(message); }
    } log;
    Log *pLog=&log;
    PlayerDomain(NativeGameState &g,KEY &key,u8 &pad,GameplayHeaps &h,EngineConsumers &m,NativeScheduler &s)
        :pG(&g),system{pad},Key(key),heaps(h),models(m),scheduler(s) {}
    void PlayerInit();
    void PlayerLifeReset();
    void ReleaseWepData();
    void InitModule(MODULE_DAT *);
    void keyConfig();
    void keyConfigTypeA();
    void setModel();
    void setMotion();
    void setWound();
    void setRightHand(std::intptr_t);
    void setLeftHand(std::uintptr_t);
    void setFace(int);
    void constructPlayer();
    void constructCharacter();
    void init0();
    void debugInit();
    void weaponRelease();
    void mahoSkelOn();void mahoSkelOff();void mahoThroughOn();void mahoThroughOff();
    void mahoKaiou4();void mahoKaiou3();void mahoKaiou2();void mahoKaiouOff();
    void mahoMuteki();void mahoInfBul();void mahoCallSc();
    void requireConstructionOwner() const {
        if(!constructing || !actorModel || pPL!=actorModel.get()) throw std::logic_error("retired native player construction callback");
    }
    void *allocateMotionTable(u32 count,int line);
    void clearMotionTable(void **,u32);
    void *arcEntry(void *base,u32 slot) const {
        if(base!=archive.data) throw std::invalid_argument("player archive base owner");return arc(slot);
    }
    [[noreturn]] int endCamera() { throw MissingGameConsumer("cPlayer::endCamera/live CameraControl"); }
    [[noreturn]] void EspDataRelease(ESP_OWNER,int,int) { throw MissingGameConsumer("EspDataRelease/live effect owners"); }
    [[noreturn]] void EffectEspDelete(int,ESP_CORE_KIND,PlayerDomain*,int) { throw MissingGameConsumer("EffectEspDelete/live effect list"); }
    [[noreturn]] void EffectEspgenDelete(int,ESP_CORE_KIND,PlayerDomain*) { throw MissingGameConsumer("EffectEspgenDelete/live effect list"); }
    [[noreturn]] void EffectEfmDelete(int,ESP_CORE_KIND,PlayerDomain*) { throw MissingGameConsumer("EffectEfmDelete/live effect list"); }
    [[noreturn]] void weaponLoad(int,int) { throw MissingGameConsumer("cPlayer::weaponLoad/ReadWepData/native weapon module"); }
    [[noreturn]] void weaponInit() { throw MissingGameConsumer("cPlayer::weaponInit/WeaponInitFunc"); }
    [[noreturn]] void init1() { throw MissingGameConsumer("cPlayer::init1/YarareInit/cloth/light/collision setup"); }
    [[noreturn]] void EspDataLoad(void*,ESP_OWNER,int) { throw MissingGameConsumer("EspDataLoad/native effect data owners"); }
    [[noreturn]] void startUp() { throw MissingGameConsumer("cPlayer::startUp/full virtual player move dependencies"); }
    [[noreturn]] void bindFootShadowTable() { throw MissingGameConsumer("cPlLeon/pl_fs_tbl foot-shadow consumer"); }
    [[noreturn]] void setPartnerPosition(ModelCpu*,Vec*) { throw MissingGameConsumer("cModel::setPos/model-bound LightInfo.updateMatrix"); }
    void *arc(u32) const;
    ResourceLease lease(void *) const;
    ModelInfoView *modelInit(void *,void *);
    void addModel(ModelInfoView *info) { models.model(*modelHandle).addModel(info); }
    int deleteModelInfo(ModelInfoView *info) { return models.model(*modelHandle).deleteModelInfo(info); }
    void setTevScaleGroup(int g) { models.model(*modelHandle).TevScaleGroup=static_cast<u8>(g); }
    [[noreturn]] void DLL_Unlink(void *) { throw MissingGameConsumer("InitModule/native weapon REL binding"); }
    void Mem_free(void *);
    void Debug_free(void *p) { Mem_free(p); } // Same owned native heap storage, never a GC address.
    void memclr_asm(MODULE_DAT *m,std::size_t bytes) {
        if(m!=&WepReadModule || bytes!=sizeof(MODULE_DAT)) throw std::logic_error("native weapon slot clear");
        *m=MODULE_DAT{}; // Typed reset; pointer null representation and native padding are not wire bytes.
    }
};
} // namespace re4::port
#define DbgFlagOn(g,n) ((g)->Debug_flg[(n)/32] |= (0x80000000u >> ((n)%32)))
#define DpfFlagOn(g,n) ((g)->Disp_flg |= (0x80000000u >> (n)))
#define DpfFlagOff(g,n) ((g)->Disp_flg &= ~(0x80000000u >> (n)))
#define DbgFlagChk(g,n) ((g)->Debug_flg[(n)/32] & (0x80000000u >> ((n)%32)))
#define DbgFlagOff(g,n) ((g)->Debug_flg[(n)/32] &= ~(0x80000000u >> ((n)%32)))
#define SysFlagChk(g,n) ((g)->System_flg & (0x80000000u >> (n)))
#define ScfFlagChk(g,n) ((g)->Scenario_flg[(n)/32] & (0x80000000u >> ((n)%32)))
#define VALID_PTR(p) ((p)!=nullptr)
#define HALT() throw std::runtime_error("original Leon model creation HALT")
