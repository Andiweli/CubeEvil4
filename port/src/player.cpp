#include "player_original.h"
#include <algorithm>

namespace re4::port {
struct PlayerRuntime::Impl {
    PlayerDomain original;
    const std::thread::id owner=std::this_thread::get_id();
    Impl(NativeGameState &g,KEY &k,u8 &p,GameplayHeaps &h,EngineConsumers &m,NativeScheduler &s)
        : original(g,k,p,h,m,s) {}
    void check(bool boundary=false) const {
        if(owner!=std::this_thread::get_id()) throw std::logic_error("player frame owner");
        if(boundary && (original.scheduler.current() || original.scheduler.allocatedStacks()))
            throw std::logic_error("player assets require quiescent task borrowers");
    }
};
void PlayerDomain::Mem_free(void *p) {
    auto it=allocations.find(p);
    if(it==allocations.end()) throw std::invalid_argument("InitModule unowned native allocation");
    allocations.erase(it);
}
void *PlayerDomain::arc(u32 slot) const {
    if(slot<4 || slot-4>=archiveIndex.entries.size()) throw std::out_of_range("original PL_ARC_PTR slot");
    const auto &e=archiveIndex.entries[slot-4];
    return const_cast<u8 *>(archive.data+e.offset);
}
ResourceLease PlayerDomain::lease(void *p) const {
    if(!archive.owner) throw std::logic_error("player archive owner absent");
    for(const auto &e:archiveIndex.entries) if(archive.data+e.offset==p)
        return {archive.owner,archive.data+e.offset,e.size};
    throw std::invalid_argument("player BIN/TPL/motion is not an owned archive entry");
}
ModelInfoView *PlayerDomain::modelInit(void *bin,void *tpl) {
    if(modelHandle) throw std::logic_error("Leon model initialized twice without release");
    modelHandle=models.initializeModel(lease(bin),lease(tpl),"pl00",SysFlagChk(pG,SYS_INVISIBLE)!=0);
    auto &cpu=models.model(*modelHandle);
    body=PlayerBodyCpu(&cpu);
    return cpu.pModelInfo;
}
ModelInfoView *PlayerDomain::ModelInfoFacade::create(void *bin,void *tpl) {
    if(!domain->modelHandle) throw std::logic_error("Leon attachment without original root model");
    return domain->models.createModelInfoFor(domain->models.model(*domain->modelHandle),domain->lease(bin),domain->lease(tpl));
}
PlayerRuntime::PlayerRuntime(NativeGameState &g,KEY &k,u8 &p,GameplayHeaps &h,EngineConsumers &m,NativeScheduler &s)
    :impl_(std::make_unique<Impl>(g,k,p,h,m,s)) {}
PlayerRuntime::~PlayerRuntime()=default;
void PlayerRuntime::gameInit() { impl_->check();impl_->original.PlayerInit(); }
void PlayerRuntime::lifeReset() { impl_->check();impl_->original.PlayerLifeReset(); }
void PlayerRuntime::keyConfig() { impl_->check();impl_->original.keyConfig(); }
PlayerInitSnapshot PlayerRuntime::snapshot() const {
    impl_->check();auto &d=impl_->original;auto &s=d.WepReadModule;
    bool zero=std::all_of(std::begin(s.pBss),std::end(s.pBss),[](u32 v){return !v;});
    return {d.PlKaiou,d.oldWepId,zero && !s.id && !s.ctrl_flag.get() && !s.pData && !s.pDll && !s.DataSize && !s.DllSize && !s.EmInitFunc};
}
const std::vector<std::string> &PlayerRuntime::trace() const { impl_->check();return impl_->original.trace; }
void *PlayerRuntime::allocateWeaponData(u32 bytes,bool debug) {
    impl_->check(true);auto &d=impl_->original;
    if(!bytes || d.WepReadModule.pData) throw std::logic_error("weapon data ownership boundary");
    auto block=d.heaps.allocate(bytes,13,true);auto *data=block.data;
    d.allocations.emplace(data,std::move(block));auto &s=d.WepReadModule;
    s.pData=data;s.DataSize=bytes;s.ctrl_flag.set(MODULE_CTRL_DATA_MALLOC);
    if(debug)s.ctrl_flag.set(MODULE_CTRL_DATA_DMALLOC);
    return data;
}
ConsumerHandle PlayerRuntime::initializeLeonAssets(ResourceLease bytes) {
    impl_->check(true);auto &d=impl_->original;
    if(d.modelHandle || d.archive.owner) throw std::logic_error("Leon assets already bound");
    if(d.pG->pl_type!=0) throw std::invalid_argument("Leon helper requires original pl_type zero");
    if(!bytes.owner || !bytes.data) throw std::invalid_argument("player archive ownership");
    auto index=indexAssetBody(bytes.data,bytes.size);
    d.archive=bytes;d.archiveIndex=std::move(index);d.body=PlayerBodyCpu(nullptr);
    try {
        d.setModel();d.trace.push_back("cPlLeon::setModel:original");
        if(!d.modelHandle) throw std::runtime_error("original Leon modelInit absent");
        d.setMotion();d.trace.push_back("cPlLeon::setMotion:original");
        for(u32 n=0;n<d.motionTable.size();++n) if(d.motionTable[n]) {
            auto b=d.lease(d.motionTable[n]);u32 slot=0;
            for(const auto &e:d.archiveIndex.entries) if(b.data==d.archive.data+e.offset) {slot=e.index+4;break;}
            d.bindings.push_back({n,slot,b});
        }
    } catch(...) { releaseLeonAssets();throw; }
    return *d.modelHandle;
}
ConsumerHandle PlayerRuntime::leonModel() const {
    impl_->check();if(!impl_->original.modelHandle) throw std::logic_error("Leon CPU assets absent");
    return *impl_->original.modelHandle;
}
const std::vector<PlayerMotionBinding> &PlayerRuntime::motionBindings() const { impl_->check();return impl_->original.bindings; }
std::vector<ModelInfoView *> PlayerRuntime::leonModelInfos() const {
    impl_->check();std::vector<ModelInfoView *> result;
    for(auto *p=impl_->original.models.model(leonModel()).pModelInfo;p;p=p->pList) result.push_back(p);
    return result;
}
ModelInfoView *PlayerRuntime::knife() const { impl_->check();return impl_->original.body.m_pKnife; }
ModelInfoView *PlayerRuntime::face() const { impl_->check();return impl_->original.body.m_pFace; }
void PlayerRuntime::setHands(std::intptr_t right,std::uintptr_t left) {
    impl_->check(true);(void)leonModel();auto &d=impl_->original;
    // Non-selector pointers are accepted only when lease() resolves a complete owned BIN.
    d.setRightHand(right);d.setLeftHand(left);
}
void PlayerRuntime::releaseLeonAssets() {
    impl_->check();auto &d=impl_->original;
    if(d.modelHandle || d.archive.owner) impl_->check(true);
    if(d.modelHandle) d.models.releaseModel(*d.modelHandle);
    d.modelHandle.reset();d.archive={};d.archiveIndex={};d.motionTable.fill(nullptr);d.bindings.clear();d.body=PlayerBodyCpu(nullptr);
}
void PlayerRuntime::clear() {
    impl_->check(true);releaseLeonAssets();auto &d=impl_->original;
    d.InitModule(&d.WepReadModule);d.allocations.clear();d.PlKaiou=d.oldWepId=d.m_BbtnCnt=0;d.trace.clear();
}
} // namespace re4::port
