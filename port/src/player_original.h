#pragma once
#include "re4/port/game_runtime.h"
#include "re4_player_types.h"
#include <map>

namespace re4::port {
using cModelInfo = ModelInfoView;
using cModel = ModelCpu;
using cPlBody = PlayerBodyCpu;
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
    struct ModelInfoFacade {
        PlayerDomain *domain;
        ModelInfoView *create(void *,void *);
    } ModInfoMgr{this};
    struct Log {
        [[noreturn]] void err(int,int,const char *message) { throw std::runtime_error(message); }
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
#define DbgFlagChk(g,n) ((g)->Debug_flg[(n)/32] & (0x80000000u >> ((n)%32)))
#define DbgFlagOff(g,n) ((g)->Debug_flg[(n)/32] &= ~(0x80000000u >> ((n)%32)))
#define SysFlagChk(g,n) ((g)->System_flg & (0x80000000u >> (n)))
#define ScfFlagChk(g,n) ((g)->Scenario_flg[(n)/32] & (0x80000000u >> ((n)%32)))
#define VALID_PTR(p) ((p)!=nullptr)
#define HALT() throw std::runtime_error("original Leon model creation HALT")
