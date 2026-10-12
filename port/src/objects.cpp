#include "object_original.h"
#include <atomic>
#include <limits>

namespace re4::port {
namespace {std::atomic<u64> objectIdentities{1};}
NativeObject::NativeObject(){initializeObject();}
void NativeObject::move(){throw MissingGameConsumer("cObj/native virtual movement registration");}
void NativeProp::move(){throw MissingGameConsumer("cObj00::move/parent,rope,motion consumers");}
void NativeThrown::move(){throw MissingGameConsumer("cWepItem::move/grenade,effect,hit consumers");}
void NativeEffect4::move(){throw MissingGameConsumer("cObj04::move/effect motion consumers");}
void NativeEffect5::move(){throw MissingGameConsumer("cObj05::move/kaboom consumers");}
void NativeEffect9::move(){throw MissingGameConsumer("cObj09::move/rigid body consumers");}
NativeWeapon::NativeWeapon(){initializeWeapon();}
void NativeWeapon::move(){throw MissingGameConsumer("cObjWep::move/native weapon virtual consumers");}
void NativeWeapon::init(ModelCpu*){throw MissingGameConsumer("cObjWep::init/native concrete weapon");}
void NativeWeapon::setMotion(PlayerDomain*){throw MissingGameConsumer("cObjWep::setMotion/native concrete weapon");}
NativeObject *NativeObjectManager::constructModuleObject(NativeObject *p,u32 id){
    if(!moduleConstruct)throw MissingGameConsumer("ObjInitFunc/native weapon module registration");
    return moduleConstruct(p,id);
}
void NativeObject::releasePartsList(int){if(!manager)throw std::logic_error("object model owner absent");manager->releaseModel(this);}
void NativeObject::releaseModelInfo(){if(modelHandle || pModelInfo)throw std::logic_error("object model release incomplete");}
NativeObjectManager::NativeObjectManager(GameplayHeaps &h,EngineConsumers &m,NativeScheduler &s)
    :heaps(h),models(m),scheduler(s),identity(objectIdentities.fetch_add(1)) {setName("cObjMgr");}
NativeObjectManager::~NativeObjectManager(){for(auto *p:slots)if(p)std::destroy_at(p);}
void NativeObjectManager::check(bool quiescent) const {
    if(owner!=std::this_thread::get_id())throw std::logic_error("object frame owner");
    if(quiescent && (scheduler.current() || scheduler.allocatedStacks()))throw std::logic_error("object pool reset requires quiescent tasks");
}
bool NativeObjectManager::owns(NativeObject *p) const {return p && std::find(slots.begin(),slots.end(),p)!=slots.end();}
u32 NativeObjectManager::index(NativeObject *p) const {
    auto it=std::find(slots.begin(),slots.end(),p);if(!p || it==slots.end())throw std::invalid_argument("foreign or retired object pointer");
    return u32(it-slots.begin());
}
NativeObject *NativeObjectManager::fastAt(u32 i) const {if(i>=slots.size())throw std::out_of_range("object slot");return slots[i];}
NativeObject *NativeObjectManager::at(u32 i) const {return i<nArray?fastAt(i):nullptr;}
void *NativeObjectManager::memAlloc(std::size_t bytes) {
    if(bytes%stride || bytes/stride>0x10000)throw std::invalid_argument("native object pool bounds");
    const auto n=bytes/stride;
    if(!n){slots.clear();generations.clear();return nullptr;}
    static_assert(stride%alignof(NativeObject)==0);
    arena=heaps.allocate(bytes,13,true);slots.assign(n,nullptr);generations.assign(n,0);
    try {for(std::size_t i=0;i<n;++i){auto *p=new(static_cast<u8*>(arena.data)+i*stride) NativeObject;p->be_flag=0;p->manager=this;slots[i]=p;}}
    catch(...){for(auto *p:slots)if(p)std::destroy_at(p);slots.clear();generations.clear();arena={};throw;}
    return slots.front();
}
void NativeObjectManager::memFree(void *p) {
    if(p!=pArray)throw std::invalid_argument("object arena free owner");
    for(auto *q:slots){if(q->modelHandle)throw std::logic_error("object arena free with model owner");}
    for(auto *q:slots)std::destroy_at(q);slots.clear();generations.clear();arena={};
}
void NativeObjectManager::memClear(NativeObject *p,std::size_t bytes) {
    if(bytes==stride*nArray && p==pArray){for(auto *q:slots)q->be_flag=0;return;}
    const auto i=index(p);if(bytes!=stride || !p->isEmpty() || p->modelHandle)throw std::logic_error("object clear lifetime");
    std::destroy_at(p);auto *q=new(static_cast<u8*>(arena.data)+i*stride) NativeObject;q->be_flag=0;q->manager=this;slots[i]=q;
}
void NativeObjectManager::destruct(NativeObject *p) {
    const auto i=index(p);if(p->modelHandle)throw std::logic_error("deferred object destructor before model push");
    trace.push_back("destruct:"+std::to_string(p->id)+":"+std::to_string(p->guid));
    std::destroy_at(p);auto *q=new(static_cast<u8*>(arena.data)+i*stride) NativeObject;q->be_flag=0;q->manager=this;slots[i]=q;
    if(++generations[i]==0)throw std::overflow_error("object slot generation");
}
void NativeObjectManager::releaseModel(NativeObject *p) {
    index(p);
    if(p->modelHandle){models.releaseObjectModel(*p->modelHandle,*p);p->modelHandle.reset();}
    else if(p->pList || p->pModelInfo)throw std::logic_error("unowned object model references");
}
NativeObject *NativeObjectManager::unsupported(NativeObject*,u32 id,const char *cls){throw MissingGameConsumer(std::string("cObjMgr::construct/")+cls+" id="+std::to_string(id));}
NativeObject *NativeObjectManager::getActiveWork() const {check();if(!configured)throw MissingGameConsumer("ObjMgr.roomInit/arrayAlloc");return pAlive;}
NativeObject *NativeObjectManager::getNext(NativeObject *p) const {check();index(p);auto *next=static_cast<NativeObject*>(p->getNext());if(next)index(next);return next;}
void NativeObjectManager::applyFunc(const std::function<void(NativeObject*)> &f){for(u32 i=0;i<nArray;++i)f(fastAt(i));}
struct ObjectRuntime::Impl {
    NativeObjectManager manager;
    Impl(GameplayHeaps &h,EngineConsumers &m,NativeScheduler &s):manager(h,m,s){}
    NativeObject *resolve(ObjectHandle h) const {
        const auto &m=manager;m.check();
        if(h.runtime!=m.identity || h.epoch!=m.epoch || h.slot>=m.nArray || h.generation!=m.generations[h.slot])throw std::invalid_argument("retired object handle");
        auto *p=m.fastAt(h.slot);if(p->isEmpty())throw std::invalid_argument("empty object handle");return p;
    }
    ObjectHandle handle(NativeObject *p) const {auto i=manager.index(p);return {manager.identity,manager.epoch,manager.generations[i],i};}
};
ObjectRuntime::ObjectRuntime(GameplayHeaps &h,EngineConsumers &m,NativeScheduler &s):impl_(std::make_unique<Impl>(h,m,s)){}
ObjectRuntime::~ObjectRuntime()=default;
bool ObjectRuntime::configured() const {impl_->manager.check();return impl_->manager.configured;}
void ObjectRuntime::configure(u32 n){auto &m=impl_->manager;m.check(true);if(n>0x10000)throw std::invalid_argument("native object pool capacity");clear();m.roomInit();m.arrayAlloc(n);m.configured=true;m.trace.push_back("ObjMgr.roomInit/arrayAlloc:original/native-stride");}
void ObjectRuntime::clear(){auto &m=impl_->manager;m.check(true);m.destroyAll();m.dieCheck();m.dieCheck();m.arrayFree();m.roomInit();m.configured=false;if(++m.epoch==0)throw std::overflow_error("object epoch");}
std::optional<ObjectHandle> ObjectRuntime::create(u32 id,bool back){auto &m=impl_->manager;m.check();m.getActiveWork();if(id>0x3f)throw std::out_of_range("GC object ID");auto *p=back?m.createBack(int(id)):m.create(int(id));if(!p)return {};auto i=m.index(p);++m.generations[i];return impl_->handle(p);}
std::optional<ObjectHandle> ObjectRuntime::createAt(u32 id,u32 slot){auto &m=impl_->manager;m.check();m.getActiveWork();if(id>0x3f)throw std::out_of_range("GC object ID");auto *p=m.create(int(id),slot);if(!p)return {};auto i=m.index(p);++m.generations[i];return impl_->handle(p);}
void ObjectRuntime::destroy(ObjectHandle h,bool now){auto *p=impl_->resolve(h);if(now)impl_->manager.destroyNow(p);else impl_->manager.destroy(p);}
void ObjectRuntime::move(ObjectHandle h){auto *p=impl_->resolve(h);if(!p->isAlive())throw std::logic_error("pending object movement");p->move();}
NativeObject *ObjectRuntime::checkedObject(ObjectHandle h){return impl_->resolve(h);}
NativeObjectManager &ObjectRuntime::originalManager(){impl_->manager.check();return impl_->manager;}
void ObjectRuntime::dieCheck(){impl_->manager.check();impl_->manager.dieCheck();}
void ObjectRuntime::destroyAll(){impl_->manager.check();impl_->manager.destroyAll();}
ObjectSnapshot ObjectRuntime::snapshot() const {auto &m=impl_->manager;m.check();u32 active=0,occupied=0,pending=0;for(auto *p=m.pAlive;p;p=m.getNext(p))++active;for(auto *p:m.slots){occupied+=!p->isEmpty();pending+=(p->be_flag&0x600)!=0;}return {m.nArray,active,occupied,m.maxAlive,m.Guid,pending,m.epoch};}
ObjectWorkSnapshot ObjectRuntime::snapshot(ObjectHandle h) const {auto *p=impl_->resolve(h);return {p->be_flag,p->guid,p->id,p->kindid,p->blk,bool(p->modelHandle)};}
std::vector<ObjectHandle> ObjectRuntime::active() const {auto &m=impl_->manager;m.check();std::vector<ObjectHandle> out;for(auto *p=m.getActiveWork();p;p=m.getNext(p))out.push_back(impl_->handle(p));return out;}
void ObjectRuntime::initializeModel(ObjectHandle h,ResourceLease bin,ResourceLease tpl){auto *p=impl_->resolve(h);auto &m=impl_->manager;if(!p->isAlive() || p->modelHandle)throw std::logic_error("object model activation lifetime");auto owner=std::shared_ptr<ModelCpu>(m.arena.owner,p);p->modelHandle=m.models.initializeModel(bin,tpl,"ObjMgr",false,1,p->be_flag,std::move(owner));}
NativeObject *ObjectRuntime::getActiveWork(){return impl_->manager.getActiveWork();}
NativeObject *ObjectRuntime::getNext(NativeObject *p){return impl_->manager.getNext(p);}
void ObjectRuntime::destroy(NativeObject *p){auto &m=impl_->manager;m.check();m.index(p);m.destroy(p);}
void ObjectRuntime::destroyNow(NativeObject *p){auto &m=impl_->manager;m.check();m.index(p);m.destroyNow(p);}
int ObjectRuntime::id(NativeObject *p) const {impl_->manager.check();impl_->manager.index(p);return p->id;}
void ObjectRuntime::applyFunc(const std::function<void(NativeObject*)> &f){impl_->manager.getActiveWork();impl_->manager.applyFunc(f);}
const std::vector<std::string> &ObjectRuntime::trace() const {impl_->manager.check();return impl_->manager.trace;}
} // namespace re4::port
