#include "damage_original.h"
namespace re4::port {
int DamageVolume::hitCheck(Vec*,Vec*){throw std::logic_error("empty damage slot virtual hit query");}
DamageManager::~DamageManager(){for(auto *p:slots)std::destroy_at(p);}
bool DamageManager::owns(DamageVolume *p) const{return p && std::find(slots.begin(),slots.end(),p)!=slots.end();}
u32 DamageManager::index(DamageVolume *p) const{auto i=std::find(slots.begin(),slots.end(),p);if(!p || i==slots.end())throw std::logic_error("foreign damage slot");return u32(i-slots.begin());}
DamageVolume *DamageManager::fastAt(u32 n) const{if(n>=slots.size())throw std::out_of_range("damage slot");return slots[n];}
void *DamageManager::memAlloc(std::size_t bytes){
    if(bytes%stride || bytes/stride>0x10000)throw std::invalid_argument("damage arena size");
    auto count=bytes/stride;if(!count)return nullptr;arena=heaps.allocate(bytes,13,true);slots.assign(count,nullptr);
    try{for(std::size_t n=0;n<count;++n){auto *p=new(static_cast<u8*>(arena.data)+n*stride) DamageVolume;p->be_flag=0;p->manager=this;p->pG=&state;slots[n]=p;}}
    catch(...){for(auto *p:slots)if(p)std::destroy_at(p);slots.clear();arena={};throw;}return slots.front();
}
void DamageManager::memFree(void *p){if(p!=pArray)throw std::logic_error("damage arena owner");for(auto *q:slots)std::destroy_at(q);slots.clear();arena={};}
void DamageManager::destruct(DamageVolume *p){auto n=index(p);std::destroy_at(p);auto *q=new(static_cast<u8*>(arena.data)+n*stride) DamageVolume;q->be_flag=0;q->manager=this;q->pG=&state;slots[n]=q;}
void DamageManager::memClear(DamageVolume *p,std::size_t bytes){if(p==pArray && bytes==stride*nArray){for(auto *q:slots)q->be_flag=0;return;}if(bytes!=stride || !p->isEmpty())throw std::logic_error("damage slot clear state");destruct(p);}
} // namespace re4::port
