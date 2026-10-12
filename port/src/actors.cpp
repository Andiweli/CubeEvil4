#include "actor_original.h"
#include "player_original.h"
#include <algorithm>
#include <new>
namespace re4::port {
void NativeEmActor::move(){throw MissingGameConsumer("cEm::move/non-player actor consumer");}
void NativePlayerActor::move(){
    if(!player || player->actorModel.get()!=static_cast<ActorModelCpu*>(this))throw std::logic_error("retired native player dispatch");
    ++player->construction.virtualMoves;player->moveOriginal();
}
void NativeLeonActor::moveCloth(){player->moveLeonCloth();}
int NativeLeonActor::checkXbutton(){return player->checkXbuttonLeon();}
NativeActorManager::NativeActorManager(PlayerDomain &p,GameplayHeaps &h,EngineConsumers &m,NativeScheduler &s)
    :player(p),heaps(h),models(m),scheduler(s){}
NativeActorManager::~NativeActorManager(){for(auto *p:slots)std::destroy_at(p);}
void NativeActorManager::check() const {if(owner!=std::this_thread::get_id())throw std::logic_error("actor frame owner");}
bool NativeActorManager::owns(NativeEmActor *p) const {return p && std::find(slots.begin(),slots.end(),p)!=slots.end();}
u32 NativeActorManager::index(NativeEmActor *p) const {
    check();auto it=std::find(slots.begin(),slots.end(),p);if(!p || it==slots.end())throw std::logic_error("foreign actor work");return u32(it-slots.begin());
}
NativeEmActor *NativeActorManager::fastAt(u32 n) const {check();if(n>=slots.size())throw std::out_of_range("native actor slot");return slots[n];}
NativeEmActor *NativeActorManager::getActiveWork() const {check();return pAlive;}
NativeEmActor *NativeActorManager::getNext(NativeEmActor *p) const {index(p);auto *q=static_cast<NativeEmActor*>(p->getNext());if(q)index(q);return q;}
void *NativeActorManager::memAlloc(std::size_t bytes){
    if(bytes%stride || bytes/stride>0x10000)throw std::invalid_argument("native actor pool size");
    auto count=bytes/stride;if(!count)return nullptr;
    arena=heaps.allocate(bytes,13,true);slots.assign(count,nullptr);
    try{for(std::size_t n=0;n<count;++n){auto *p=new(static_cast<u8*>(arena.data)+n*stride) NativeEmActor;p->be_flag=0;p->manager=this;slots[n]=p;}}
    catch(...){for(auto *p:slots)if(p)std::destroy_at(p);slots.clear();arena={};throw;}
    return slots.front();
}
void NativeActorManager::memFree(void *p){
    if(p!=pArray || player.actorModel || player.modelHandle)throw std::logic_error("actor pool has a model borrower");
    for(auto *q:slots)std::destroy_at(q);slots.clear();arena={};
}
void NativeActorManager::memClear(NativeEmActor *p,std::size_t bytes){
    if(p==pArray && bytes==stride*nArray){for(auto *q:slots)q->be_flag=0;return;}
    if(bytes!=stride || !p->isEmpty())throw std::logic_error("actor work clear state");destruct(p);
}
void NativeActorManager::destruct(NativeEmActor *p){
    auto n=index(p);if(player.actorModel.get()==static_cast<ActorModelCpu*>(p))throw std::logic_error("actor destroyed before model release");
    std::destroy_at(p);auto *q=new(static_cast<u8*>(arena.data)+n*stride) NativeEmActor;q->manager=this;q->be_flag=0;slots[n]=q;
}
std::shared_ptr<ActorModelCpu> NativeActorManager::modelOwner(NativeEmActor *p) const {index(p);return {arena.owner,static_cast<ActorModelCpu*>(p)};}
NativeEmActor *NativeActorManager::placeLeon(NativeEmActor *p){
    if(player.actorModel || player.modelHandle)throw std::logic_error("two player actors share a PlayerWork");
    auto n=index(p);std::destroy_at(p);slots[n]=nullptr;
    auto *q=new(static_cast<u8*>(arena.data)+n*stride) NativeLeonActor(player);q->manager=this;slots[n]=q;
    // Begin native lifetimes before preserved ctor bodies can throw. No GC
    // pointer reinterpretation; original constructor order and move dispatch
    // are retained. Publication still occurs in the original create body.
    player.actorModel=modelOwner(q);
    player.trace.push_back("EmMgr.construct:cPlayer/cPlLeon native hierarchy");
    player.constructPlayer();player.construction.playerConstructor=true;
    player.constructCharacter();
    return q;
}
NativeEmActor *NativeActorManager::unsupported(NativeEmActor*,u32,const char *className){throw MissingGameConsumer(std::string("cEmMgr::construct/")+className);}
void NativeActorManager::rollbackUnpublished(NativeEmActor *p){
    index(p);for(auto *q=pAlive;q;q=getNext(q))if(q==p)throw std::logic_error("rollback of published actor");
    destruct(p);
}
} // namespace re4::port
