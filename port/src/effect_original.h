#pragma once
#include "object_original.h"
#include "re4/port/effects.h"
#include "effect_data_original.h"
namespace re4::port {
struct EffectDomain {
    GameplayHeaps &heaps;NativeScheduler &scheduler;ObjectRuntime &ObjMgr;
    const std::thread::id frameOwner=std::this_thread::get_id();
    using System=EffectSystemCpu;
    EffectSystemCpu system;
    EffectSystemCpu *g_pEspSys=&system;
    std::array<std::shared_ptr<EffectDataFile>,NativeEffectMax> resources;
    MessagePalette loadedTlut;
    std::vector<std::string> resourceTrace;
    int EspDataLoad(EffectDataFile*,u32,int);int EspDataRelease(u32,int,int);
    MessageTexture *EspPullTexObj(u32);
    int espTexRegist(MessageTpl*,EffectAnimation*,u8,u32);
    int estRegist(EffectOffsets*,EffectIds*,u32);int sstRegist(EffectOffsets*,EffectIds*,u32);int pathRegist(EffectOffsets*,EffectIds*,u32);
    int efmRegist(ResourceLease,ResourceLease,ResourceLease,ResourceLease,u8,u32);
    int espTexRelease(u32);int estRelease(u32);int sstRelease(u32);int pathRelease(u32);int efmRelease(u32);
    void GXLoadTlut(MessagePalette *p,u32 slot){if(!p || !p->initialized || slot)throw std::invalid_argument("effect native TLUT state");loadedTlut=*p;}
    void recordResourceError(u32 owner){resourceTrace.push_back("original effect registration error owner="+std::to_string(owner));}
    int loadData(ResourceLease,u32,int);int releaseData(u32,int,int);void releaseResourceViews(u32);
    EffectGenerator *EspgenArray{};u32 nEspgen{};
    GameplayHeaps::Allocation spriteArena,generatorArena;
    bool configured{};
    u16 g_Core_flg{};u8 g_Core_kind{};void *g_Core_pEm{};
    using EspgenDestructFunc=void(*)(EffectGenerator*);
    std::array<EspgenDestructFunc,0x40> EspgenDestructTbl{};
    std::array<EspgenDestructFunc,6> EspgenDestructTblApp{};
    static constexpr u32 ESPGEN_APP_ID=0x40,ESPGEN_ID_MAX=0x46;
    struct Log {template<class... A> void warn(int,int,const char*,A...) {}
        template<class... A> [[noreturn]] void err(int,int,const char *m,A...){throw std::invalid_argument(m);}} log;
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
