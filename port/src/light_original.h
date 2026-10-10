#pragma once
#include "re4/port/game_runtime.h"
#include "re4/port/lights.h"
#include "model_original.h"
#include "rnd.h"
#include <cstring>
#include <cstdio>

namespace re4::port {
struct LightCpu : ModelUnit,LightFields {
    LightCpu();
    void retire() { be_flag &= ~0x601u; }
    void move();
    LightCpu &operator=(cLightWork &);
    int checkScr();
    void setPartsNo(int);
    int setParent(u8,u32);
    int setParent(ModelCpu *);
    ModelCpu *calcParent();
    void setTrans(int);
    void setSpotNormal(Vec *);
    void setSpotTarget(Vec *);
    void prepareNativeWork(const cLightWork &);
};
template<class T> struct LightPool {
    GameplayHeaps &heaps;
    explicit LightPool(GameplayHeaps &h):heaps(h) {}
    T *pArray{},*pAlive{},*pAlivePush{},*pArrayPush{};
    u32 nArray{},nArrayPush{},maxAlive{},warnDiv=10;
    std::size_t size=sizeof(T);
    u8 flag{};
    const char *name="cLightMgr";
    void (**funcTbl)(T*){};
    GameplayHeaps::Allocation allocation;
    int roomInit();void init(void (**)(T*));
    int arrayFree();int arrayAlloc(u32);
    u32 countActiveWork();T *create(int);T *createBack(int);
    void destroy(T *);void destroyAll();int dieCheck();
    int deleteList(T *);void addListFront(T *);void addListBack(T *);
    void *memAlloc(std::size_t n) {
        allocation=heaps.allocate(n,13,true);
        auto *p=reinterpret_cast<T*>(allocation.data);
        for(std::size_t i=0;i<n/sizeof(T);++i) { new(p+i)T{};p[i].be_flag=0; }
        return p;
    }
    void memFree(void *) { allocation={};pAlive=nullptr; }
    void memClear(T *p,std::size_t n) { for(std::size_t i=0;i<n/sizeof(T);++i) { p[i]=T{};p[i].be_flag=0; } }
    bool owns(const T *p) const { auto a=reinterpret_cast<std::uintptr_t>(p),b=reinterpret_cast<std::uintptr_t>(pArray);return p && a>=b && a-b<std::size_t(nArray)*size && (a-b)%size==0; }
    T *fastAt(u32 n) { return n<nArray ? pArray+n : nullptr; }
    T *getActiveWork() { return pAlive; } T *getNext(T *p) { return static_cast<T*>(p->getNext()); }
    int construct(T *p,u32 id) { if(id>16) throw std::invalid_argument("original light handler ID");new(p)T{};return 1; }
    void log(const char *,...);
};
struct LightManagerCpu : LightPool<LightCpu> {
    using LightPool::LightPool;
    cLit *pLitHeader{},*m_pLitCore{},*m_pLitRoom{},*m_pLitRoom2{},*dbMem{};
    cLightEnv LightEnv{};
    u32 kindFlags[8]{};
    LightPathHeader *pLitPath{};
    f32 ElecPower{},m_ColBrendRate{};
    GXColor m_Tune[3]{};
    u32 x204{},dbFlag{};
    void init(void (**)(LightCpu*));int roomInit(cLit*,cLit*,cLit*);
    LightCpu *create(cLightWork *);LightCpu *createBack(cLightWork *);
    f32 setElecPower(f32);int onKind(u8);int offKind(u8);int checkKind(u8);
    LightCpu *getKindLight(u8);int roomLitSet(cLit *);int roomLitCheck();
    cLightEnv *getEnvPtr();int initPath(LightPathHeader *);u8 *getPathPtr(u8);LightPathHeader *getPathHeader();
    void beginEvent();void endEvent();void setTune(cLightEnv *);
    void setFogStart(f32);void setFogEnd(f32);f32 getFogStart();f32 getFogEnd();
    void deleteScr();void offScr(u8);int countScr();cLightEnv *getCutAddr(int,int);
    static void memclr_asm(void *p,std::size_t n) { std::memset(p,0,n); }
    static void memset_asm(void *p,int c,std::size_t n) { std::memset(p,c,n); }
};
struct LightLog { void err(int,int,const char *,...);void warn(int,int,const char *,...); };
struct LightConstants {
    ConsRoom *pConsRoom{};
    int ConsInitRoom(ConsRoom *);
    u32 ConsGetRoomValue(u32);
};
struct LightCall {
    LightManagerCpu &mgr;
    LightLog log;
    std::vector<std::shared_ptr<LightBlockView>> blocks;
    std::shared_ptr<LightPathView> paths;
    EngineConsumers &consumers;
    LightRuntime::ModelResolver resolver;
    std::optional<ConsumerHandle> lastResolved;
    LightCall(LightManagerCpu &m,EngineConsumers &c):mgr(m),consumers(c) {}
    bool ownsData(const void *) const;
    ModelCpu *enemy(u16);
    ModelCpu *resolve(u8,u16);
    static LightCall &active();
};
extern void (*LightFuncTbl[17])(LightCpu*);
u32 readNativePathOffset(LightPathHeader *,u8);
int resolveEtc(u16,ModelCpu **);
void C_VECSubtract(const Vec *,const Vec *,Vec *);
void C_VECAdd(const Vec *,const Vec *,Vec *);
void C_VECNormalize(const Vec *,Vec *);
f32 C_VECMag(const Vec *);
f32 LIMIT_ANGLE(f32);
} // namespace re4::port
#define PSVECMag C_VECMag
#define PSVECSubtract C_VECSubtract
#define PSVECAdd C_VECAdd
#define VECNormalize C_VECNormalize
