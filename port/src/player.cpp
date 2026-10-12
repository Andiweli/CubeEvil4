#include "player_original.h"
#include "actor_original.h"
#include "re4/port/sha256.h"
#include <algorithm>

namespace re4::port {
struct PlayerRuntime::Impl {
    PlayerDomain original;
    const std::thread::id owner=std::this_thread::get_id();
    Impl(NativeGameState &g,KEY &k,u8 &p,GameplayHeaps &h,EngineConsumers &m,NativeScheduler &s,ObjectRuntime &o,EffectRuntime &e)
        : original(g,k,p,h,m,s,o,e) {original.EmMgr=std::make_unique<NativeActorManager>(original,h,m,s);}
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
    if(nativeWeapons)return nativeWeapons->domain()->lease(p);
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
void PlayerRuntime::bindWeaponModules(GameRuntime &r){impl_->check(true);auto &d=impl_->original;if(d.nativeWeapons)throw std::logic_error("weapon services already bound");d.runtime=&r;d.nativeWeapons=std::make_unique<WeaponRuntime>(r,d);}
WeaponRuntime &PlayerRuntime::weapons(){impl_->check();if(!impl_->original.nativeWeapons)throw MissingGameConsumer("native weapon services");return *impl_->original.nativeWeapons;}
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
    if(d.Wep){
        if(d.Wep->m_pWep){d.ObjMgr.destroyNow(d.Wep->m_pWep);d.Wep->m_pWep=nullptr;}
        if(d.Wep->m_pWepHand){d.ObjMgr.destroyNow(d.Wep->m_pWepHand);d.Wep->m_pWepHand=nullptr;}
    }
    while(d.playerEffectLoads){d.effects.releaseData(EFF_PL00,1,0);--d.playerEffectLoads;}
    if(d.modelHandle) {
        if(d.attachCameraStorage && d.actorModel && d.actorModel->Motion.pAttachCam){
            if(d.actorModel->Motion.pAttachCam!=d.attachCameraStorage.data)throw std::logic_error("player attach-camera ownership mismatch");
            if(d.actorModel->attachCameraRegistry)d.actorModel->attachCameraRegistry->deleteAttachCamera(d.actorModel->Motion.pAttachCam,d.actorModel.get());
            std::destroy_at(d.actorModel->Motion.pAttachCam);d.actorModel->Motion.pAttachCam=nullptr;
        }
        if(d.constructing)d.models.releaseUnpublishedModel(*d.modelHandle,d.actorModel);
        else d.models.releaseModel(*d.modelHandle);
    }
    auto *nativeActor=d.actorModel?dynamic_cast<NativeEmActor*>(d.actorModel.get()):nullptr;
    d.modelHandle.reset();d.actorModel.reset();
    if(nativeActor){
        if(d.EmMgr->pAlive==nativeActor){d.EmMgr->destroy(nativeActor);d.EmMgr->dieCheck();d.EmMgr->dieCheck();}
        else d.EmMgr->rollbackUnpublished(nativeActor);
    }
    d.archive={};d.archiveIndex={};d.motionTable.fill(nullptr);d.bindings.clear();d.body=PlayerBodyCpu(nullptr);
    d.attachCameraStorage={};d.cloth.clear();
    d.CamCtrl.m_QuasiFPS={};
    d.Body=&d.body;d.bodyOwner.reset();d.weaponOwner.reset();d.pushOwner.reset();d.waistOwner.reset();d.motionBaseOwner.reset();d.mahoOwner.reset();d.Neck=nullptr;d.neckOwner.reset();d.Pl_func_tbl={};
    d.actorMotions.clear();d.registeredMotions.clear();d.m_MotTbl=d.motionTable.data();d.m_MotTbl2=nullptr;
    d.Wep=nullptr;d.Push=nullptr;d.Waist=nullptr;d.MotBase=nullptr;d.pMaho=nullptr;d.pPL=nullptr;d.pSUB=nullptr;d.subArc=nullptr;d.constructing=false;
    if(d.nativeWeapons && (d.nativeWeapons->snapshot().loaded || !d.nativeWeapons->domain()->requests.empty()))d.ReleaseWepData();
    if(d.WepReadModule.pData || d.WepReadModule.pDll || d.WepReadModule.ctrl_flag.get())d.InitModule(&d.WepReadModule);
    if(d.nativeWeapons)d.nativeWeapons->clear();
    d.WeaponInitFunc={};
}
void PlayerDomain::allocateAttachCamera(){
    requireConstructionOwner();if(attachCameraStorage || actorModel->Motion.pAttachCam)throw std::logic_error("duplicate original attach-camera allocation");
    attachCameraStorage=heaps.allocate(sizeof(MotionCamera),13,false);
    actorModel->Motion.pAttachCam=new(attachCameraStorage.data) MotionCamera{};
    construction.cameraAllocated=true;
}
void PlayerDomain::initCloth(){
    requireConstructionOwner();cloth.initialize(actorModel.get());
    const ClothInfo *chains[]={&cloth.leonHair,&cloth.leonJacket,&cloth.leonHolster};
    for(u32 n=0;n<3;++n){
        const auto &c=*chains[n];auto &out=construction.cloth[n];
        out.flags=c.Flag;out.volumes=u32(c.At_num);out.gravity=c.Gravity;out.rate=c.Rate;out.stretchy=c.Stretchy;out.moveRate=c.Move_rate;
        for(int i=0;i<c.Num;++i){
            if(c.pCloth[i]==255)continue;
            auto *part=actorModel->getPartsPtr(c.pCloth[i]);const auto &p=part->Pen;ClothLinkSnapshot link;
            link.refs={c.pCloth[i],c.pParent[i],c.pChild[i],c.pLeft?c.pLeft[i]:u8(255),c.pRight?c.pRight[i]:u8(255),c.pUpLeft?c.pUpLeft[i]:u8(255),c.pUpRight?c.pUpRight[i]:u8(255)};
            link.jointWorld=part->world;if(c.pChild[i]!=255)link.childWorld=actorModel->getPartsPtr(c.pChild[i])->world;
            link.offset=p.Offset;link.normal=p.Normal;link.position=p.Pos;link.speed=p.Speed;link.length=p.Length;
            link.neighborLengths={p.Length_l,p.Length_r,p.Length_lu,p.Length_ru};link.motionFlags=part->motParts.flags;
            out.links.push_back(link);
        }
    }
    construction.clothInitialized=true;
}
void PlayerDomain::makeBodyShapeData(){
    requireConstructionOwner();Body->makeSpaeData();
    for(u32 i=0;i<2;++i){auto &from=Body->pl_shape_data[i];auto &out=construction.body[i];
        out.maxFrame=from.head.max_frame;out.tableCount=from.head.tbl_num;
        for(u32 t=0;t<2;++t){out.offsets[t]=from.tbl[t].offset;out.shapeIds[t]=from.tbl[t].shape_id;out.keyCounts[t]=from.tbl[t].key_num;}
        for(u32 t=0;t<4;++t){out.frames[t]=from.mot[t].frame;out.values[t]=from.mot[t].value;out.right[t]=from.mot[t].r_value;out.left[t]=from.mot[t].l_value;}
    }
    construction.bodyInitialized=true;
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
    if(!d.EmMgr || !d.EmMgr->nArray)throw MissingGameConsumer("EmMgr.roomInit/CNS actor capacity");
    auto index=indexAssetBody(bytes.data,bytes.size);
    d.construction={};d.construction.schedulerTask=bool(d.scheduler.current());d.archive=bytes;d.archiveIndex=std::move(index);
    d.m_BinoRno=d.m_ConDmFlag=0;d.m_ConDmTimer=0;d.m_Work0=0;d.m_Fwork0=d.m_JumpAdjY=0;
    d.m_FallVec=d.m_JumpVec=d.m_ActCross=d.m_ActNorm={};d.m_ActAttr=0;
    d.constructing=true;
    try {
        auto *actor=d.EmMgr->create(0,0);
        if(!actor || !d.modelHandle)throw std::runtime_error("original EmMgr player creation failed");
        d.constructing=false;d.construction.returnedActor=true;
        return *d.modelHandle;
    } catch(const std::exception &error) {
        d.construction.playerEffect=d.effects.dataSnapshot(EFF_PL00);
        if(d.actorModel){
        d.construction.primaryMotion=d.actorModel->Motion.pMot!=nullptr;
        d.construction.neckMotion=d.actorModel->pMotionB==&d.actorModel->m_SubMot && d.actorModel->m_SubMot.pMot!=nullptr;
        d.construction.sequenceFrame=d.actorModel->Motion.Seq_frame;
        d.construction.sequenceFrames=d.actorModel->Motion.Seq_frame_num;
        d.construction.position=d.actorModel->pos;d.construction.rotation=d.actorModel->ang;
        d.construction.routines={d.actorModel->r_no_0,d.actorModel->r_no_1,d.actorModel->r_no_2,d.actorModel->r_no_3};
        if(d.actorModel->pList && !d.construction.startUpEntered)d.construction.lightCenter=d.actorModel->getPartsPtr(0)->world;
        for(u32 r=0;r<3;++r)for(u32 c=0;c<4;++c)d.construction.lightInverse[r*4+c]=d.actorModel->LightInfo.imat[r][c];
        if(d.nativeWeapons)d.construction.weapon=d.nativeWeapons->snapshot();
        if(d.nativeWeapons && !d.nativeWeapons->domain()->effectLoads.empty())d.construction.weaponEffect=d.effects.dataSnapshot(d.nativeWeapons->domain()->effectLoads.front());
        d.construction.init1Reached=bool(d.Neck);d.construction.lightFlags=d.actorModel->State.m_LightFlag;
        for(auto *box=&d.actorModel->hitInfo;box && box->flag;box=box->pList)++d.construction.hitboxes;
        }
        if(d.Wep && d.Wep->m_pWep){auto &w=*d.Wep->m_pWep;d.construction.weaponObjectId=w.id;d.construction.weaponItemId=w.itemId;d.construction.weaponParts=w.nParts;
            if(w.pModelInfo){auto &b=w.pModelInfo->model_addr->source;auto &t=static_cast<TexturePaletteView*>(w.pModelInfo->tpl_addr)->bytes;d.construction.weaponBinHash=sha256(b.data,b.size);d.construction.weaponTplHash=sha256(t.data,t.size);}
            if(w.pList && w.pList->pParent){for(u32 n=0;n<d.actorModel->nParts;++n)if(w.pList->pParent==d.actorModel->getPartsPtr(int(n)))d.construction.weaponParentPart=n;}
            for(u32 n=0;n<0x5f;++n)if(d.m_MotTbl[n]){++d.construction.weaponMotions;auto motionBytes=d.lease(d.m_MotTbl[n]);u32 slot=0;std::string path;
                for(auto &entry:d.archiveIndex.entries)if(d.archive.data+entry.offset==motionBytes.data){slot=entry.index+4;path="em/pl00.drs";}
                auto owner=d.nativeWeapons->domain();for(auto &entry:owner->index.entries)if(owner->body.data+entry.offset==motionBytes.data){slot=entry.index+4;path=owner->loaded->identity.path;}
                if(!slot || path.empty())throw std::logic_error("weapon motion source identity");d.construction.weaponBindings.push_back({n,slot,path,sha256(motionBytes.data,motionBytes.size)});
            }}
        d.construction.hp=d.actorModel?d.actorModel->hp:0;
        d.construction.motionSlots=u32(d.actorMotions.size());d.construction.registeredMotionSlots=u32(d.registeredMotions.size());
        d.construction.debugCallbacks=d.pMaho?d.pMaho->count():0;
        d.construction.subobjects=d.Wep && d.Body && d.Push && d.Waist && d.MotBase;
        d.construction.modelInitialized=d.modelHandle.has_value();d.construction.blocker=error.what();
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
    d.EmMgr->arrayFree();d.EmMgr->roomInit();
    d.DmgMgr.arrayFree();d.DmgMgr.roomInit();
}
void PlayerRuntime::configureActors(u32 n){impl_->check(true);auto &d=impl_->original;if(d.actorModel || d.modelHandle)throw std::logic_error("actor pool resize with live player");d.EmMgr->arrayFree();d.EmMgr->roomInit();d.EmMgr->arrayAlloc(n);d.DmgMgr.arrayFree();d.DmgMgr.roomInit();d.DmgMgr.arrayAlloc(20);}
void PlayerDomain::move(){
    if(!actorModel)throw std::logic_error("player virtual move without actor");
    auto *actor=dynamic_cast<NativePlayerActor*>(actorModel.get());
    if(!actor || actor->player!=this)throw std::logic_error("native player dynamic type identity");
    actor->move();
}
void PlayerDomain::moveLeonCloth(){throw MissingGameConsumer("PlClothMoveLeon/PenClothMove native simulation owner");}
} // namespace re4::port
