#pragma once
#include "re4/port/game_runtime.h"
#include "re4_object_types.h"
#include <algorithm>
#include <cstddef>
#include <new>

namespace re4::port {
void RotVector(Vec*,Vec*,Vec*);
void RotMatrix(Mtx,Vec*);
void TransMatrix(Mtx,Vec*);
void C_VECAdd(const Vec*,const Vec*,Vec*);
u32 C_MTXInverse(const Mtx,Mtx);
struct NativeObjectManager;
struct NativeObject : ModelCpu {
    s32 blk{};
    NativeObjectManager *manager{};
    std::optional<ConsumerHandle> modelHandle;
    std::shared_ptr<const void> codeOwner;
    NativeObject();
    virtual ~NativeObject();
    virtual void move();
    void initializeObject();
    void push();
    void releasePartsList(int);
    void releaseModelInfo();
};
struct NativeProp final : NativeObject { NativeFreeObj00 work{};void move() override; };
struct NativeThrown final : NativeObject { NativeFreeWepItem work{};void move() override; };
struct NativeEffect4 final : NativeObject { NativeObj04Free work{};void move() override; };
struct NativeEffect5 final : NativeObject { NativeObj05Free work{};void move() override; };
struct NativeEffect9 final : NativeObject { NativeObj09Free work{};void move() override; };
struct PlayerDomain;
struct ActorModelCpu;
struct WeaponDomain;
struct NativeWeapon : NativeObject {
    ActorModelCpu *m_SightEm{};
    NativeWeapon();
    void initializeWeapon();
    void move() override;
    virtual void init(ModelCpu*);
    virtual void setMotion(PlayerDomain*);
    ModelInfoView *modelInit(void*,void*);
    void resetMotion();
    void motionSet(void*,u8,u16,u16,void*);
    int motionMove();
    void setAbility(f32,f32,f32,f32);
    void setParent(ModelCpu*);
    enum STAT { S_STAY=0,S_READY=1,S_FIRE=2,S_DOWN=3,S_RELOAD=4,S_DROP=5 };
    void setStat(STAT);
    f32 bureX{},bureY{},bureSpeedX{},bureSpeedY{};
    u8 shotFrame[4]{};
    u16 itemId{};
    u8 r_no_0{},r_no_1{};
    std::shared_ptr<WeaponDomain> weaponDomain;
    std::shared_ptr<const void> parentOwner;
    void *motReset[2]{};
    u8 displayFlags{};
    struct Flags {
        u8 &bits;
        void reset(){bits=0;}
        Flags &on(int n){bits|=u8(1u<<n);return *this;}
    } flag{displayFlags};
    static constexpr int F_DISP_0=2,F_DISP_1=3,F_DISP_2=4;
    ModelCpu *m_pParent{};
    u32 m_StopSeId{};
    void interrupt(){throw MissingGameConsumer("cObjWep::interrupt/original weapon action and sound consumer");}
};
struct NativeRuger final:NativeWeapon {void init(ModelCpu*) override;void setMotion(PlayerDomain*) override;};
struct NativeMauser final:NativeWeapon {void init(ModelCpu*) override;void setMotion(PlayerDomain*) override;};
struct NativeFn57 final:NativeWeapon {void init(ModelCpu*) override;void setMotion(PlayerDomain*) override;};
struct NativeObjectManager {
    GameplayHeaps &heaps;
    EngineConsumers &models;
    NativeScheduler &scheduler;
    const std::thread::id owner=std::this_thread::get_id();
    static constexpr std::size_t stride=std::max({sizeof(NativeObject),sizeof(NativeProp),sizeof(NativeThrown),
        sizeof(NativeEffect4),sizeof(NativeEffect5),sizeof(NativeEffect9),sizeof(NativeWeapon)});
    GameplayHeaps::Allocation arena;
    std::vector<NativeObject*> slots;
    std::vector<u64> generations;
    std::vector<std::string> trace;
    NativeObject *pArray{},*pAlive{},*pAlivePush{},*pArrayPush{};
    u32 nArray{},nArrayPush{},maxAlive{},warnDiv=10,Guid{};
    std::size_t size=stride;
    u8 flag=2;
    const char *name="cObjMgr";
    u64 identity{},epoch=1;
    bool configured{};
    std::function<NativeObject*(NativeObject*,u32)> moduleConstruct;
    NativeObject *constructModuleObject(NativeObject*,u32);
    NativeObjectManager(GameplayHeaps &,EngineConsumers &,NativeScheduler &);
    ~NativeObjectManager();
    void check(bool quiescent=false) const;
    bool owns(NativeObject *) const;
    u32 index(NativeObject *) const;
    NativeObject *fastAt(u32 n) const;
    NativeObject *at(u32 n) const;
    void *memAlloc(std::size_t);
    void memFree(void *);
    void memClear(NativeObject *,std::size_t);
    void destruct(NativeObject *);
    void releaseModel(NativeObject *);
    template<class T> NativeObject *place(NativeObject *p) {
        const auto i=index(p);
        std::destroy_at(p);
        slots[i]=nullptr;
        try { auto *q=new(static_cast<u8*>(arena.data)+i*stride) T;slots[i]=q;q->manager=this;return q; }
        catch(...) {auto *q=new(static_cast<u8*>(arena.data)+i*stride) NativeObject;q->be_flag=0;q->manager=this;slots[i]=q;throw;}
    }
    NativeObject *unsupported(NativeObject *,u32,const char *);
    template<class... A> void log(const char *fmt,A...) {trace.emplace_back(fmt);}
    int roomInit();int arrayAlloc(u32);int arrayFree();
    NativeObject *create(int);NativeObject *create();NativeObject *create(int,u32);NativeObject *createBack(int);
    int construct(NativeObject*,u32);int constructTyped(NativeObject*,u32);
    void setName(const char *);
    u32 countActiveWork();int dieCheck();
    int deleteList(NativeObject*);void addListFront(NativeObject*);void addListBack(NativeObject*);
    void destroyBase(NativeObject*);void destroy(NativeObject*);void destroyNow(NativeObject*);void destroyAll();
    NativeObject *getActiveWork() const;NativeObject *getNext(NativeObject*) const;
    void applyFunc(const std::function<void(NativeObject*)>&);
};
} // namespace re4::port
