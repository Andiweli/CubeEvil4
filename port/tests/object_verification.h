#pragma once
#include "re4/port/game_runtime.h"
#include <array>

namespace re4::port::tests {
// Numeric lifecycle fixtures run the preserved manager/delete bodies. They
// are not spawned effects, weapons, room resources or a constructed player.
template<class Checks> void objectTests(Checks &c,GameRuntime &game) {
    auto &o=game.objects();auto &e=game.effects();
    c.rejects([&]{o.getActiveWork();},"unconfigured ObjMgr rejects original iteration");
    c.rejects([&]{e.deleteSprites(0,0,nullptr);},"unconfigured effect pool rejects deletion");
    o.configure(6);e.configurePools(3,3);
    constexpr std::array<u32,6> ids{0,4,5,9,10,11};
    std::array<ObjectHandle,6> h{};
    for(u32 i=0;i<ids.size();++i) {
        h[i]=*o.create(ids[i]);auto w=o.snapshot(h[i]);
        c.test(w.flags==0x21 && w.kind==1 && w.id==ids[i] && w.guid==i,"original object constructor/id/guid");
        c.rejects([&]{o.move(h[i]);},"unbound concrete movement is not a success stub");
    }
    auto list=o.active();
    for(u32 i=0;i<ids.size();++i)c.test(list[i].slot==h[5-i].slot,"original front insertion order");
    c.test(!o.create(0) && o.snapshot().occupied==6 && o.snapshot().peak==6,"original full object pool");
    auto old=h[0];o.destroy(old);
    c.test(o.snapshot().active==5 && o.snapshot().occupied==6 && (o.snapshot(old).flags&0x200),"original deferred removal reserves slot");
    o.dieCheck();c.test((o.snapshot(old).flags&0x600)==0x600,"original first dieCheck delays destruction");
    o.dieCheck();c.rejects([&]{o.snapshot(old);},"original second dieCheck retires handle");
    h[0]=*o.create(0,true);
    c.test(h[0].slot==old.slot && h[0].generation!=old.generation && o.active().back().slot==old.slot,"native generation and original back insertion");
    auto weapon=h[5];o.destroy(weapon,true);
    c.rejects([&]{o.snapshot(weapon);},"destroyNow original immediate deletion");
    h[5]=*o.createAt(11,weapon.slot);
    c.test(!o.createAt(0,weapon.slot) && !o.createAt(0,99),"original indexed allocation boundaries");
    c.rejects([&]{o.configure(0x10001);},"invalid capacity retains existing native pool");
    c.test(o.snapshot().active==6,"capacity rejection is non-destructive");
    int ownerA=0,ownerB=0;
    auto &a=e.spriteFixture(0);a.info.Core_kind=2;a.info.Core_pEm=&ownerA;
    auto &b=e.spriteFixture(1);b.info.Core_kind=2;b.info.Core_pEm=&ownerB;
    auto &ga=e.generatorFixture(0);ga.Id=0;ga.Eff_core.Core_kind=2;ga.Eff_core.Core_pEm=&ownerA;
    auto &gb=e.generatorFixture(1);gb.Id=1;gb.Eff_core.Core_kind=2;gb.Eff_core.Core_pEm=&ownerB;
    auto &unsupported=e.generatorFixture(2);unsupported.Id=0x42;unsupported.Eff_core.Core_kind=2;unsupported.Eff_core.Core_pEm=&ownerB;
    for(u32 i=1;i<=3;++i){auto &core=e.modelCoreFixture(h[i]);core.Core_kind=2;core.Core_pEm=i==2?static_cast<void*>(&ownerB):static_cast<void*>(&ownerA);}
    e.deleteSprites(0,2,&ownerA);
    c.test(!(a.m_Be_flg&1) && (b.m_Be_flg&1) && e.snapshot().liveSprites==1,"original EspDelete owner filter and counter");
    e.deleteGenerators(0,2,&ownerA);
    c.test((ga.Be_flg&2) && !(gb.Be_flg&2),"original EspgenDelete deferred flag and filter");
    c.rejects([&]{e.deleteGenerators(0,2,&ownerB);},"resource-owning generator destructor remains explicit boundary");
    c.test(!(gb.Be_flg&2) && !(unsupported.Be_flg&2),"failed deletion does not partially release matching generators");
    unsupported.Id=0x40; // Numeric fixture: original NULL application destructor.
    e.deleteModels(0,2,&ownerA);
    c.test(o.snapshot().pending==2 && o.snapshot(h[2]).flags==0x21,"original EfmDelete typed work filter");
    e.deleteSprites(0,0,nullptr);e.deleteGenerators(0,0,nullptr);
    c.test(e.snapshot().liveSprites==0 && e.snapshot().pendingGenerators==3,"original wildcard effect release");
    ModelCpu parent;parent.guid=17;
    auto &bound=e.spriteFixture(2);bound.m_pMod=&parent;bound.m_Guid_pMod=16;
    e.deleteSprites(0,0,nullptr,&parent);
    c.test(bound.m_Be_flg&1,"original model-guid mismatch retains sprite");
    bound.m_Guid_pMod=17;e.deleteSprites(0,0,nullptr,&parent);
    c.test(!(bound.m_Be_flg&1) && e.snapshot().liveSprites==0,"original model-guid match deletes sprite");
    o.destroyAll();o.dieCheck();o.dieCheck();
    c.test(o.snapshot().active==0 && o.snapshot().occupied==0,"original all-work deferred destruction");
    c.rejects([&]{o.create(63);},"unbound concrete object constructor rejects");
    c.test(o.snapshot().occupied==0,"failed constructor does not publish slot");
    e.clear();o.clear();c.rejects([&]{o.snapshot(h[2]);},"room epoch retires object references");
    const auto live=game.heaps().liveBytes();
    for(int round=0;round<20;++round){o.configure(2);auto item=*o.create(11);o.destroy(item,true);o.clear();}
    c.test(game.heaps().liveBytes()==live && game.engineConsumers().modelCount()==0,"repeated object arena allocation and release");
}
} // namespace re4::port::tests
