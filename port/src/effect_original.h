#pragma once
#include "object_original.h"
#include "re4/port/effects.h"
namespace re4::port {
struct EffectDomain {
    GameplayHeaps &heaps;NativeScheduler &scheduler;ObjectRuntime &ObjMgr;
    const std::thread::id owner=std::this_thread::get_id();
    struct System {EffectSprite *EspArray{};u32 nEsp{},ActiveEspNum{};} system;
    System *g_pEspSys=&system;
    EffectGenerator *EspgenArray{};u32 nEspgen{};
    GameplayHeaps::Allocation spriteArena,generatorArena;
    bool configured{};
    u16 g_Core_flg{};u8 g_Core_kind{};void *g_Core_pEm{};
    using EspgenDestructFunc=void(*)(EffectGenerator*);
    std::array<EspgenDestructFunc,0x40> EspgenDestructTbl{};
    std::array<EspgenDestructFunc,6> EspgenDestructTblApp{};
    static constexpr u32 ESPGEN_APP_ID=0x40,ESPGEN_ID_MAX=0x46;
    struct Log {template<class... A> void warn(int,int,const char*,A...) {}} log;
    Log *pLog=&log;
    EffectDomain(GameplayHeaps &,NativeScheduler &,ObjectRuntime &);
    ~EffectDomain();
    void check(bool quiescent=false) const;
    void configure(u32,u32);void clear();
    u32 GetEspgenIdMax();
    void PushEsp(EffectSprite*);void EspDelete(int,int,void*,ModelCpu*);
    void PushEspgen(EffectGenerator*);void EspgenDelete(int,int,void*);
    void EfmDelete(int,int,void*);void EfmDeleteSub(NativeObject*);
    void EffectEspDelete(int,int,void*,ModelCpu*);
    void EffectEspgenDelete(int,int,void*);void EffectEfmDelete(int,int,void*);
    void rejectGeneratorDestructors(int,int,void*) const;
    NativeObj04Free *effect4(NativeObject*);
    NativeObj05Free *effect5(NativeObject*);
    NativeObj09Free *effect9(NativeObject*);
};
} // namespace re4::port
