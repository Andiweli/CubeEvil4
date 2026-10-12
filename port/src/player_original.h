#pragma once
#include "re4/port/game_runtime.h"
#include "re4_player_types.h"
#include "re4/port/objects.h"
#include "re4/port/effects.h"
#include "weapon_original.h"
#include "cloth_original.h"
#include "actor_original.h"
#include "damage_original.h"
#include "action_original.h"
#include <map>

namespace re4::port {
using cModelInfo = ModelInfoView;
using cModel = ModelCpu;
using cParts = ModelPart;
using cPlBody = PlayerBodyCpu;
using cObj = NativeObject;
void MotionGetSpeed(ModelCpu*,ModelMotion*,int,Vec*,Vec*);
void motionVecScale(const Vec*,Vec*,f32);
f32 Muku2(f32,f32,f32);
inline constexpr f32 PI=3.1415927f,PI2=6.2831855f;
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
    struct NeckCpu {
        PlayerDomain *pl{};ActorModelCpu *m_pLastTarget{};int m_lockCtr{};u16 m_Flag{};u8 m_Mode{},pad_F{};f32 m_NeckY{};void *m_MotR{},*m_MotL{};
        explicit NeckCpu(PlayerDomain*);
        void init(void*,void*,int);void move();void motSet(void*,int);void setMode(int);
        void clear(){m_MotR=nullptr;}
        NativeEmActor *getTarget();
        bool ownedMotion(void *p) const {if(!p)return false;(void)pl->lease(p);return true;}
    };
    std::unique_ptr<NeckCpu> neckOwner;NeckCpu *Neck{};
    DamagePart m_Yarare[10];
    u32 m_Flag{},m_SeId{};u8 m_CmdTimer{},m_EyeMode{};
    void *m_pEffRoom{},*m_pBoss{};int m_pSatMask{};
    std::array<std::function<void(PlayerDomain*)>,7> Pl_func_tbl;
    void YarareInit(ActorModelCpu*,f32,f32,f32,f32,f32,s16,u16);
    void YarareAdd(ActorModelCpu*,DamagePart*,f32,f32,f32,f32,f32,s16,u16);
    ObjectRuntime &ObjMgr;
    EffectRuntime &effects;
    struct CameraBoundary {
        int areaNo{};
        struct LocationWork {Mtx m_pl_mat{};Vec *m_p_floor_norm{};void setPlayerLocation(Mtx,Vec*);} m_QuasiFPS;
        [[noreturn]] void endScope(){throw MissingGameConsumer("CamCtrl.endScope/live camera");}
        [[noreturn]] void LowerBinocular(){throw MissingGameConsumer("CamCtrl.LowerBinocular/live camera");}
        [[noreturn]] void endPushObject(){throw MissingGameConsumer("CamCtrl.endPushObject/live camera");}
    } CamCtrl;
    struct LightBoundary {[[noreturn]] void update(int,int){throw MissingGameConsumer("LightMgr.update/special player camera");}} LightMgr;
    [[noreturn]] void CameraMove(){throw MissingGameConsumer("CameraMove/full player camera");}
    NativeGameState *pG;
    GameRuntime *runtime{};
    DamageManager DmgMgr;
    ClothDomain cloth;
    GameplayHeaps::Allocation attachCameraStorage;
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
    std::unique_ptr<NativeActorManager> EmMgr;
    ActionManager ActBtn;
    std::shared_ptr<const void> actionLifetime;
    bool scenarioInitialized{}; // Only original SceAtInit may establish a live room field owner.
    u8 PlFanceFlag{},PlReloadDirect{};
    Vec PlFancePos{};
    struct EyeWork {cDelayF direction;int timer{},time{};} eyes;
    int PlKeyReloadType{}; // Original .bss default; shared input owner, not another controller.
    u8 m_BinoRno{},m_ConDmFlag{};u16 m_ConDmTimer{};
    u32 m_Work0{};f32 m_Fwork0{},m_JumpAdjY{};
    Vec m_FallVec{},m_JumpVec{},m_ActCross{},m_ActNorm{};u32 m_ActAttr{};
    u32 playerEffectLoads{};
    LeonConstructionSnapshot construction;
    std::unique_ptr<WeaponRuntime> nativeWeapons;
    std::function<void(PlayerDomain*)> WeaponInitFunc;
    u8 m_Hokan{},m_Frame{}; // Original player.h fields, not native timing substitutes.
    struct ModelInfoFacade {
        PlayerDomain *domain;
        ModelInfoView *create(void *,void *);
    } ModInfoMgr{this};
    struct Log {
        template<class... A> [[noreturn]] void err(int,int,const char *message,A...) { throw std::runtime_error(message); }
        std::vector<std::string> messages;
        void mes(int,int,const char *message) { messages.emplace_back(message); }
    } log;
    Log *pLog=&log;
    PlayerDomain(NativeGameState &g,KEY &key,u8 &pad,GameplayHeaps &h,EngineConsumers &m,NativeScheduler &s,ObjectRuntime &o,EffectRuntime &e)
        :ObjMgr(o),effects(e),pG(&g),DmgMgr(g,h),cloth(g),system{pad},Key(key),heaps(h),models(m),scheduler(s),ActBtn(g,key,s) {}
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
    int endCamera();
    int EspDataRelease(ESP_OWNER id,int count,int warn) {return effects.releaseData(id,count,warn);}
    void EffectEspDelete(int a,ESP_CORE_KIND b,PlayerDomain* c,int model) {if(model)throw std::invalid_argument("native player effect model selector");effects.deleteSprites(a,b,c);}
    void EffectEspgenDelete(int a,ESP_CORE_KIND b,PlayerDomain* c) {effects.deleteGenerators(a,b,c);}
    void EffectEfmDelete(int a,ESP_CORE_KIND b,PlayerDomain* c) {effects.deleteModels(a,b,c);}
    void weaponLoad(int,int);
    void ReadWepData(u32,u32);
    void weaponInit();
    void init1();
    void initCloth();
    void pl_R0_Move(PlayerDomain*);void pl_R1_Footwork(PlayerDomain*);
    void allocateAttachCamera();
    void makeBodyShapeData();
    int EspDataLoad(void *p,ESP_OWNER id,int multiple) {
        int result=effects.loadData(lease(p),id,multiple);
        if(result && id==EFF_PL00)++playerEffectLoads;
        return result;
    }
    void startUp();
    void move();
    void moveOriginal();
    void moveLeonCloth();
    int checkXbuttonLeon();
    int motionMove();
    struct SatBoundary {
        PlayerDomain *player{};
        int check(PlayerDomain*,int);
        int hitCheck(Vec*,Vec*,Vec*,Vec*,int,int);
        f32 getFloor(Vec*,int,f32,f32,int);
    } SatMgr{this};
    ModelPart *getPartsPtr(int n){if(!actorModel || n<0 || n>=actorModel->nParts)throw std::out_of_range("native player part");return actorModel->getPartsPtr(n);}
    void clearStatus(int id){actorModel->clearStatus(id);}
    void setRno(int a,int b,int c,int d){actorModel->setRno(a,b,c,d);}
    void moveMatCalcBefore();void moveCloth();
    void dmgCheck();void moveBinocular();void subCharLiveCheck();void shadowCtrl();
    void moveEye();void moveEyeNormal();void moveEyeMotion();void seqSeCtrl();void visibleCtrl();void debugMove();
    int dmMotCk();int actionSelect();int actionSelectOriginal();void checkCtrl();int keyReload();
    int joyKamae();int joyLKamae();int actCheck();u32 playerStatus();
    int actWallCheck(PlayerDomain*);int fanceCheck(PlayerDomain*);int fallCheck(PlayerDomain*);int jumpCheck(PlayerDomain*);u32 upDownCk(PlayerDomain*);
    int windowCheck(PlayerDomain*,u8*,NativeActionWindow**);int windowStatus(NativeActionWindow*);
    NativeActionFieldInfo *SceAtCheckFieldInfo(Vec*);
    int ChkWindow(ModelCpu*,Vec*,Vec*,int,u16*,Vec*,Vec*,NativeActionWindow**);
    NativeActionField *sceAtSetOtStart(){throw MissingGameConsumer("SceAtInit/native AEV ordering and field owners");}
    NativeActionField *sceAtGetOtAddr(NativeActionField*){throw MissingGameConsumer("SceAtGetOtAddr/native AEV ordering");}
    int sceAtHitCheck(NativeActionField*,ModelCpu*,Vec*,Vec*){throw MissingGameConsumer("sceAtHitCheck/native live AEV area work");}
    int LadderNearCk(Vec*){throw MissingGameConsumer("LadderNearCk/original ladder owners");}
    int EmRackCk(PlayerDomain*,Vec*,f32){throw MissingGameConsumer("EmRackCk/live original cEmRack work");}
    ActionCallback windowAction(NativeActionWindow*);
    void fanceOn();void fallOn();void levelUpOn();void levelDownOn();void level2UpOn();void level2DownOn();void holdOn();void jumpFallOn();
    ActionCallback actionCallback(void (PlayerDomain::*)());
    int checkXbutton();int weaponAimKey();int weaponReloadable();u32 configurationFlags() const;const JOY &inputJoy() const;
    int catchPush(PlayerPushCpu*);void actorCollision(ModelCpu*);void actorCollisionOriginal(ModelCpu*);
    void moveEyeOriginal();
    void motionSet(void*,void*,void*,void*,int,int);
    void initializeMotionWork(ModelMotion*,void*,void*,int,int,int);
    void dispatchRoutine0();void dispatchRoutine1();
    Vec shadowCameraPosition() const {return pG->Camera.param.Campos;}
    [[noreturn]] int requirePartnerControl(){throw MissingGameConsumer("SubCharCheckCtrl/native partner actor");}
    [[noreturn]] u32 requirePartnerStatus(){throw MissingGameConsumer("SubCharGetStatus/native partner actor");}
    [[noreturn]] void requirePartnerCommand(int,int){throw MissingGameConsumer("SubCharCtrl/native partner actor");}
    [[noreturn]] void setDamage(u8,int,int,int,f32){throw MissingGameConsumer("cPlayer::setDamage/LifeDownSet2 and damage routines");}
    [[noreturn]] void bindFootShadowTable() { throw MissingGameConsumer("cPlLeon/pl_fs_tbl foot-shadow consumer"); }
    [[noreturn]] void setPartnerPosition(ModelCpu*,Vec*) { throw MissingGameConsumer("cModel::setPos/model-bound LightInfo.updateMatrix"); }
    void *arc(u32) const;
    ResourceLease lease(void *) const;
    ModelInfoView *modelInit(void *,void *);
    void addModel(ModelInfoView *info) { models.model(*modelHandle).addModel(info); }
    int deleteModelInfo(ModelInfoView *info) { return models.model(*modelHandle).deleteModelInfo(info); }
    void setTevScaleGroup(int g) { models.model(*modelHandle).TevScaleGroup=static_cast<u8>(g); }
    void DLL_Unlink(void *p) {if(!nativeWeapons)throw MissingGameConsumer("native weapon services");nativeWeapons->domain()->unlink(p);}
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
#define StaFlagChk(g,n) ((g)->Status_flg[(n)/32] & (0x80000000u >> ((n)%32)))
#define StaFlagOff(g,n) ((g)->Status_flg[(n)/32] &= ~(0x80000000u >> ((n)%32)))
#define StaFlagOn(g,n) ((g)->Status_flg[(n)/32] |= (0x80000000u >> ((n)%32)))
#define SpfFlagOff(g,n) ((g)->Stop_flg &= ~(0x80000000u >> (n)))
#define ScfFlagChk(g,n) ((g)->Scenario_flg[(n)/32] & (0x80000000u >> ((n)%32)))
#ifndef VALID_PTR
#define VALID_PTR(p) ((p) != nullptr)
#endif
#define HALT() throw std::runtime_error("original Leon model creation HALT")
