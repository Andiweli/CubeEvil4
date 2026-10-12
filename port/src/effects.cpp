#include "effect_original.h"
#include "re4/port/sha256.h"
namespace re4::port {
EffectDomain::EffectDomain(GameplayHeaps &h,NativeScheduler &s,ObjectRuntime &o):heaps(h),scheduler(s),ObjMgr(o){}
EffectDomain::~EffectDomain(){for(u32 i=0;i<system.nEsp;++i)std::destroy_at(system.EspArray+i);}
void EffectDomain::check(bool quiescent) const {
    if(frameOwner!=std::this_thread::get_id())throw std::logic_error("effect frame owner");
    if(quiescent && (scheduler.current() || scheduler.allocatedStacks()))throw std::logic_error("effect reset requires quiescent tasks");
    if(!quiescent && !configured)throw MissingGameConsumer("Esp/Espgen native lifecycle pools not allocated");
}
void EffectDomain::configure(u32 esp,u32 gen){
    check(true);if(esp>0x10000 || gen>0x10000)throw std::invalid_argument("effect pool capacity");clear();
    try {
        if(esp){spriteArena=heaps.allocate(std::size_t(esp)*sizeof(EffectSprite),13,true);system.EspArray=static_cast<EffectSprite*>(spriteArena.data);for(u32 i=0;i<esp;++i){new(system.EspArray+i) EffectSprite;system.nEsp=i+1;}}
        if(gen){generatorArena=heaps.allocate(std::size_t(gen)*sizeof(EffectGenerator),13,true);EspgenArray=static_cast<EffectGenerator*>(generatorArena.data);for(u32 i=0;i<gen;++i)new(EspgenArray+i) EffectGenerator{};nEspgen=gen;}
    } catch(...){clear();throw;}
    configured=true;
}
void EffectDomain::clear(){
    check(true);
    if(system.ActiveEspNum)throw std::logic_error("effect pool clear with live sprite resources");
    for(u32 i=0;i<nEspgen;++i)if((EspgenArray[i].Be_flg&1) && !(EspgenArray[i].Be_flg&2))throw std::logic_error("effect pool clear with live generator");
    for(u32 i=0;i<NativeEffectNone;++i)if(system.ownerCnt[i]){EspDataRelease(i,0,0);releaseResourceViews(i);}
    for(u32 i=0;i<system.nEsp;++i)std::destroy_at(system.EspArray+i);
    system={};EspgenArray=nullptr;nEspgen=0;spriteArena={};generatorArena={};configured=false;
}
void EffectDomain::rejectGeneratorDestructors(int flag,int kind,void *parent) const {
    // Whole original table identifies resource-owning ids. Those consumers
    // cannot silently be replaced with NULL destructor entries.
    for(u32 i=0;i<nEspgen;++i){auto &w=EspgenArray[i];if((w.Be_flg&1) && !(w.Be_flg&2) &&
        (!flag || w.Eff_core.Core_flg==flag) && (!kind || w.Eff_core.Core_kind==kind) &&
        (!parent || w.Eff_core.Core_pEm==parent) && w.Id>=0x42 && w.Id<ESPGEN_ID_MAX)
        throw MissingGameConsumer("PushEspgen/concrete generator type registration id="+std::to_string(w.Id));}
}
NativeObj04Free *EffectDomain::effect4(NativeObject *p){auto *q=dynamic_cast<NativeEffect4*>(p);if(!q)throw std::logic_error("Efm04 native type identity");return &q->work;}
NativeObj05Free *EffectDomain::effect5(NativeObject *p){auto *q=dynamic_cast<NativeEffect5*>(p);if(!q)throw std::logic_error("Efm05 native type identity");return &q->work;}
NativeObj09Free *EffectDomain::effect9(NativeObject *p){auto *q=dynamic_cast<NativeEffect9*>(p);if(!q)throw std::logic_error("Efm09 native type identity");return &q->work;}
struct EffectRuntime::Impl {EffectDomain domain;Impl(GameplayHeaps &h,NativeScheduler &s,ObjectRuntime &o):domain(h,s,o){}};
EffectRuntime::EffectRuntime(GameplayHeaps &h,NativeScheduler &s,ObjectRuntime &o):impl_(std::make_unique<Impl>(h,s,o)){}
EffectRuntime::~EffectRuntime()=default;
void EffectRuntime::configurePools(u32 esp,u32 gen){impl_->domain.configure(esp,gen);}
void EffectRuntime::clear(){impl_->domain.clear();}
EffectSnapshot EffectRuntime::snapshot() const {auto &d=impl_->domain;if(d.frameOwner!=std::this_thread::get_id())throw std::logic_error("effect frame owner");u32 pending=0;for(u32 i=0;i<d.nEspgen;++i)pending+=(d.EspgenArray[i].Be_flg&2)!=0;return {d.configured,d.system.nEsp,d.nEspgen,d.system.ActiveEspNum,pending};}
int EffectRuntime::loadData(ResourceLease b,u32 o,int m){return impl_->domain.loadData(b,o,m);}
int EffectRuntime::releaseData(u32 o,int c,int w){return impl_->domain.releaseData(o,c,w);}
EffectDataSnapshot EffectRuntime::dataSnapshot(u32 id) const {
    auto &d=impl_->domain;d.check();if(id>=NativeEffectNone)throw std::out_of_range("effect snapshot owner");
    EffectDataSnapshot out;out.owner=id;out.loads=d.system.ownerCnt[id];auto &r=d.resources[id];
    if(r)out.sourceHash=sha256(r->source.data,r->source.size);
    for(u32 i=0;i<256;++i){auto &t=d.system.Esp_tex_tbl[i];if(t.Owner==id){out.textureIds.push_back(i);out.textureFrames.push_back(t.nTexObj);out.textureObjects+=t.nTexObj;}if(d.system.efmWk[i].owner==id)out.modelIds.push_back(i);}
    if(d.system.estTbl[id].list)out.estRecords=d.system.estTbl[id].list->num;
    if(d.system.sstTbl[id].list)out.sstRecords=d.system.sstTbl[id].list->num;
    if(d.system.pathTbl[id].list)out.pathRecords=d.system.pathTbl[id].list->num;
    return out;
}
void EffectRuntime::deleteSprites(int a,int b,void *c,ModelCpu *m){auto &d=impl_->domain;d.check();d.EffectEspDelete(a,b,c,m);}
void EffectRuntime::deleteGenerators(int a,int b,void *c){auto &d=impl_->domain;d.check();d.rejectGeneratorDestructors(a,b,c);d.EffectEspgenDelete(a,b,c);}
void EffectRuntime::deleteModels(int a,int b,void *c){auto &d=impl_->domain;d.check();d.EffectEfmDelete(a,b,c);}
EffectSprite &EffectRuntime::spriteFixture(u32 i){auto &d=impl_->domain;d.check();if(i>=d.system.nEsp)throw std::out_of_range("sprite fixture slot");auto &p=d.system.EspArray[i];if(p.m_Be_flg&1)throw std::logic_error("live sprite fixture");p.m_Be_flg=1;++d.system.ActiveEspNum;return p;}
EffectGenerator &EffectRuntime::generatorFixture(u32 i){auto &d=impl_->domain;d.check();if(i>=d.nEspgen)throw std::out_of_range("generator fixture slot");auto &p=d.EspgenArray[i];if((p.Be_flg&1) && !(p.Be_flg&2))throw std::logic_error("live generator fixture");p={};p.Be_flg=1;return p;}
NativeEffectCore &EffectRuntime::modelCoreFixture(ObjectHandle h){auto &d=impl_->domain;d.check();auto *p=d.ObjMgr.checkedObject(h);switch(p->id){case 4:return d.effect4(p)->Eff_core;case 5:return d.effect5(p)->Eff_core;case 9:return d.effect9(p)->Eff_core;default:throw std::invalid_argument("non-effect model fixture");}}
} // namespace re4::port
