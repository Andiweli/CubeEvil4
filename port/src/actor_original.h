#pragma once
#include "re4/port/player.h"
#include "re4_actor_types.h"
#include "re4/port/game_runtime.h"
#include "re4/port/scheduler.h"
#include <thread>
namespace re4::port {
struct PlayerDomain;
struct NativeActorManager;
// Native C++ inheritance and dispatch. No GC vtable or EM_WORK_SIZE overlay.
struct NativeEmActor : ActorModelCpu {
    NativeActorManager *manager{};
    virtual ~NativeEmActor()=default;
    virtual void move();
};
struct NativePlayerActor : NativeEmActor {
    PlayerDomain *player{};
    explicit NativePlayerActor(PlayerDomain &p):player(&p){}
    void move() override;
    virtual void moveMatCalcBefore();
    virtual void moveCloth();
    virtual int checkXbutton();
};
struct NativeLeonActor final : NativePlayerActor {
    using NativePlayerActor::NativePlayerActor;
    void move() override;
    void moveCloth() override;
    int checkXbutton() override;
};
struct NativeActorManager {
    PlayerDomain &player;
    GameplayHeaps &heaps;
    EngineConsumers &models;
    NativeScheduler &scheduler;
    const std::thread::id owner=std::this_thread::get_id();
    static constexpr std::size_t stride=(sizeof(NativeLeonActor)+31)&~std::size_t(31);
    GameplayHeaps::Allocation arena;
    std::vector<NativeEmActor*> slots;
    NativeEmActor *pArray{},*pAlive{},*pAlivePush{},*pArrayPush{};
    u32 nArray{},nArrayPush{},maxAlive{},warnDiv=10,Guid{};
    std::size_t size=stride;
    u8 flag=2;
    const char *name="cEmMgr";
    NativeActorManager(PlayerDomain&,GameplayHeaps&,EngineConsumers&,NativeScheduler&);
    ~NativeActorManager();
    void check() const;
    u32 index(NativeEmActor*) const;
    bool owns(NativeEmActor*) const;
    NativeEmActor *fastAt(u32) const;
    NativeEmActor *getActiveWork() const;
    NativeEmActor *getNext(NativeEmActor*) const;
    u32 getArrayNum() const {return nArray;}
    void *memAlloc(std::size_t);
    void memFree(void*);
    void memClear(NativeEmActor*,std::size_t);
    void destruct(NativeEmActor*);
    NativeEmActor *placeLeon(NativeEmActor*);
    NativeEmActor *unsupported(NativeEmActor*,u32,const char*);
    template<class... A> void log(const char*,A...) {}
    void setName(const char*);
    int roomInit();int arrayAlloc(u32);int arrayAllocBase(u32);int arrayFree();
    int construct(NativeEmActor*,u32);
    NativeEmActor *create(int);NativeEmActor *create(int,u32);
    int deleteList(NativeEmActor*);void addListFront(NativeEmActor*);
    u32 countActiveWork();int dieCheck();
    void destroy(NativeEmActor*);void destroyAll();
    std::shared_ptr<ActorModelCpu> modelOwner(NativeEmActor*) const;
    void rollbackUnpublished(NativeEmActor*);
};
} // namespace re4::port
