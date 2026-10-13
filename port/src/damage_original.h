#pragma once
#include "re4/port/game_runtime.h"
#include "re4/port/model_types.h"
#include "re4/port/consumers.h"
#include "re4_damage_types.h"
#include <algorithm>
#include <new>
namespace re4::port {
struct DamageManager;
struct DamageVolume : ModelUnit {
    DamageManager *manager{};NativeGameState *pG{};
    u32 m_Id{};DMG_TYPE m_Type{};int m_Time{};
    virtual ~DamageVolume()=default;
    virtual void beginEvent(u32);
    virtual int hitCheck(Vec*,Vec*);
};
struct DamageCylinder final : DamageVolume {Vec m_Pos{};f32 m_Radius{},m_Height{};int hitCheck(Vec*,Vec*) override;};
struct DamageQuad final : DamageVolume {Vec m_Pos[4]{};f32 m_Height{};int hitCheck(Vec*,Vec*) override;};
struct DamageManager {
    NativeGameState &state;GameplayHeaps &heaps;
    static constexpr std::size_t stride=(std::max(sizeof(DamageCylinder),sizeof(DamageQuad))+31)&~std::size_t(31);
    GameplayHeaps::Allocation arena;std::vector<DamageVolume*> slots;
    DamageVolume *pArray{},*pAlive{},*pAlivePush{},*pArrayPush{};
    u32 nArray{},nArrayPush{},maxAlive{},warnDiv=10;std::size_t size=stride;u8 flag=2;const char *name="cDmgMgr";
    explicit DamageManager(NativeGameState &g,GameplayHeaps &h):state(g),heaps(h){}
    ~DamageManager();
    DamageVolume *fastAt(u32) const;bool owns(DamageVolume*) const;u32 index(DamageVolume*) const;
    void *memAlloc(std::size_t);void memFree(void*);void memClear(DamageVolume*,std::size_t);void destruct(DamageVolume*);
    template<class T> DamageVolume *place(DamageVolume *p){auto n=index(p);std::destroy_at(p);auto *q=new(static_cast<u8*>(arena.data)+n*stride) T;q->manager=this;q->pG=&state;slots[n]=q;return q;}
    template<class... A> void log(const char*,A...){}
    int roomInit();int arrayAlloc(u32);int arrayFree();void setName(const char*);
    int construct(DamageVolume*,int);int construct(DamageVolume*,u32);DamageVolume *create(int);
    int deleteList(DamageVolume*);void addListFront(DamageVolume*);void destroy(DamageVolume*);int dieCheck();u32 countActiveWork();
    int set(int,int,Vec*,f32,f32);int set(int,int,Vec*,f32);int hitCheck(Vec*,Vec*);void move();
    template<class T> T *created(DamageVolume *p){if(!p)return nullptr;index(p);auto *q=dynamic_cast<T*>(p);if(!q)throw std::logic_error("damage constructor dynamic type");return q;}
};
int HitCheckPoint4(Vec*,Vec*);
[[noreturn]] inline void Draw_cylinder(Vec*,f32,f32,u32){throw MissingGameConsumer("DmgMgr debug cylinder draw");}
void C_VECAdd(const Vec*,const Vec*,Vec*);void motionVecScale(const Vec*,Vec*,f32);
} // namespace re4::port
