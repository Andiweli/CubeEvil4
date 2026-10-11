#include "player_original.h"
#include <algorithm>

namespace re4::port {
struct PlayerRuntime::Impl {
    PlayerDomain original;
    const std::thread::id owner=std::this_thread::get_id();
    Impl(NativeGameState &g,KEY &k,u8 &p,GameplayHeaps &h,EngineConsumers &m,NativeScheduler &s,ObjectRuntime &o,EffectRuntime &e)
        : original(g,k,p,h,m,s,o,e) {}
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
    if(!actorModel)actorModel=std::make_shared<ActorModelCpu>();
    modelHandle=models.initializeModel(lease(bin),lease(tpl),"pl00",SysFlagChk(pG,SYS_INVISIBLE)!=0,0,0,actorModel);
    trace.push_back("cDmgInfo::cDmgInfo/cEm::cEm/initWork:original");
    auto &cpu=models.model(*modelHandle);
    if(!constructing)body=PlayerBodyCpu(&cpu);
    return cpu.pModelInfo;
}
ModelInfoView *PlayerDomain::ModelInfoFacade::create(void *bin,void *tpl) {
    if(!domain->modelHandle) throw std::logic_error("Leon attachment without original root model");
    return domain->models.createModelInfoFor(domain->models.model(*domain->modelHandle),domain->lease(bin),domain->lease(tpl));
}
PlayerRuntime::PlayerRuntime(NativeGameState &g,KEY &k,u8 &p,GameplayHeaps &h,EngineConsumers &m,NativeScheduler &s,ObjectRuntime &o,EffectRuntime &e)
    :impl_(std::make_unique<Impl>(g,k,p,h,m,s,o,e)) {}
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
void PlayerRuntime::applyWaist(f32 radians) {
    impl_->check();auto &d=impl_->original;(void)leonModel();
    if(!std::isfinite(radians) || !d.Body || d.Body->modelOwner()!=d.actorModel.get() || d.actorModel->nParts<4)
        throw std::invalid_argument("original waist model/parts ownership");
    d.Body->waistSet(radians);d.Body->move();
}
void PlayerRuntime::releaseLeonAssets() {
    impl_->check();auto &d=impl_->original;
    // A construction attempt has no published actor/callback borrowers. It
    // must be able to roll back on its original GameTask caller's stack.
    if((d.modelHandle || d.archive.owner) && !d.constructing) impl_->check(true);
    if(d.modelHandle) {
        if(d.constructing)d.models.releaseUnpublishedModel(*d.modelHandle,d.actorModel);
        else d.models.releaseModel(*d.modelHandle);
    }
    d.modelHandle.reset();d.actorModel.reset();d.archive={};d.archiveIndex={};d.motionTable.fill(nullptr);d.bindings.clear();d.body=PlayerBodyCpu(nullptr);
    d.Body=&d.body;d.bodyOwner.reset();d.weaponOwner.reset();d.pushOwner.reset();d.waistOwner.reset();d.motionBaseOwner.reset();d.mahoOwner.reset();
    d.actorMotions.clear();d.registeredMotions.clear();d.m_MotTbl=d.motionTable.data();d.m_MotTbl2=nullptr;
    d.Wep=nullptr;d.Push=nullptr;d.Waist=nullptr;d.MotBase=nullptr;d.pMaho=nullptr;d.pPL=nullptr;d.pSUB=nullptr;d.subArc=nullptr;d.constructing=false;
}
void *PlayerDomain::allocateMotionTable(u32 count,int line) {
    requireConstructionOwner();
    auto &table=line==373?actorMotions:registeredMotions;
    if((line!=373 && line!=376) || (line==373?count!=0x6D:count!=12) || !table.empty())
        throw std::invalid_argument("original player motion-table allocation identity");
    // Original heap 13 / current heap, native pointer-width storage. The
    // standard non-allocating placement-array new has zero array overhead;
    // it starts typed pointer lifetimes instead of aliasing raw disc words.
    auto block=heaps.allocate(std::size_t(count)*sizeof(void*),13,false);
    auto **entries=new(block.data) void*[count]{};
    table.storage=std::move(block);table.entries=entries;table.count=count;
    return table.data();
}
void PlayerDomain::clearMotionTable(void **table,u32 count) {
    requireConstructionOwner();
    const bool first=table==actorMotions.data() && count==actorMotions.size();
    const bool second=table==registeredMotions.data() && count==registeredMotions.size();
    if(!first && !second)throw std::invalid_argument("player motion-table clear bounds");
    std::fill_n(table,count,nullptr);
}
ConsumerHandle PlayerRuntime::constructLeon(ResourceLease bytes) {
    impl_->check();auto &d=impl_->original;
    if(d.modelHandle || d.archive.owner || d.constructing)throw std::logic_error("Leon construction with live assets");
    if(d.pG->pl_type!=0 || !bytes.owner || !bytes.data)throw std::invalid_argument("Leon construction archive/type");
    auto index=indexAssetBody(bytes.data,bytes.size);
    d.construction={};d.construction.schedulerTask=bool(d.scheduler.current());d.archive=bytes;d.archiveIndex=std::move(index);
    d.actorModel=std::make_shared<ActorModelCpu>();d.constructing=true;
    try {
        d.trace.push_back("cPlayer::cPlayer:original-body/native-owned-fields");d.constructPlayer();
        d.construction.playerConstructor=true;
        d.trace.push_back("cPlLeon::cPlLeon:original-body/native-owned-fields");d.constructCharacter();
        // Success publication is intentionally impossible until the complete
        // EmMgr type/vtable and every downstream consumer are integrated.
        throw MissingGameConsumer("EmMgr.construct/full cPlLeon virtual actor publication");
    } catch(const std::exception &e) {
        d.construction.hp=d.actorModel->hp;
        d.construction.motionSlots=u32(d.actorMotions.size());d.construction.registeredMotionSlots=u32(d.registeredMotions.size());
        d.construction.debugCallbacks=d.pMaho?d.pMaho->count():0;
        d.construction.subobjects=d.Wep && d.Body && d.Push && d.Waist && d.MotBase;
        d.construction.modelInitialized=d.modelHandle.has_value();d.construction.blocker=e.what();
        if(d.modelHandle) {
            auto &cpu=d.models.model(*d.modelHandle);d.construction.parts=cpu.nParts;
            for(auto *info=cpu.pModelInfo;info;info=info->pList)++d.construction.modelInfos;
        }
        d.trace.push_back("Leon construction rollback:native-owner");releaseLeonAssets();d.construction.rolledBack=true;throw;
    } catch(...) {releaseLeonAssets();d.construction.rolledBack=true;throw;}
}
LeonConstructionSnapshot PlayerRuntime::constructionSnapshot() const {impl_->check();return impl_->original.construction;}
void PlayerRuntime::clear() {
    impl_->check(true);releaseLeonAssets();auto &d=impl_->original;
    d.InitModule(&d.WepReadModule);d.allocations.clear();d.PlKaiou=d.oldWepId=d.m_BbtnCnt=0;d.trace.clear();
}
} // namespace re4::port
