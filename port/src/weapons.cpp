#include "weapon_original.h"
#include "player_original.h"
#include "re4/port/sha256.h"
#include <cstring>
#include <limits>

namespace re4::port {
namespace {
struct WeaponModuleSnapshot final:ModuleSnapshot {};
struct WeaponModuleState final:NativeModuleState {
    std::shared_ptr<WeaponDomain> d;u32 id;bool first;
    WeaponModuleState(std::shared_ptr<WeaponDomain> x,u32 n,bool f):d(std::move(x)),id(n),first(f){}
    void prolog(ModuleContext &c) override {d->registerProlog(c,id,[&]{if(first)d->prolog01();else d->prolog02();});}
    void epilog(ModuleContext &c) override {ModuleContext::Scope scope(c);if(first)d->epilog01();else d->epilog02();d->clearBindings(id);}
    std::unique_ptr<ModuleSnapshot> save(ModuleContext&) const override {return std::make_unique<WeaponModuleSnapshot>();}
    void restore(ModuleContext&,const ModuleSnapshot &s) override {if(!dynamic_cast<const WeaponModuleSnapshot*>(&s))throw std::invalid_argument("weapon snapshot type");}
    void invoke(ModuleContext&,u16,bool) override {throw std::logic_error("weapon module has no room entry");}
    void resetRoom(ModuleContext&,u16) override {throw std::logic_error("weapon module has no room work");}
};
}
WeaponDomain::WeaponDomain(GameRuntime &r,PlayerDomain &p):runtime(r),modules(r.modules()),objects(r.objects()),pG(&r.state()) {
    if(p.pG!=pG)throw std::logic_error("weapon/player state identity");
    files.resize(OriginalWeaponFileCount);
    for(std::size_t i=0;i<files.size();++i){auto n=std::strlen(OriginalWeaponFileNames[i]);if(n>=sizeof(files[i].name))throw std::length_error("original weapon FileTbl name");std::memcpy(files[i].name,OriginalWeaponFileNames[i],n+1);}
}
void WeaponDomain::installModules(){
    for(const char *name:{"wep01","wep02"}){
        const auto &catalog=originalModuleCatalog().modules;
        auto m=std::find_if(catalog.begin(),catalog.end(),[&](const auto &row){return row.name==name;});
        if(m==catalog.end())throw std::logic_error("weapon module catalog entry absent");
        const bool first=std::strcmp(name,"wep01")==0;auto owner=shared_from_this();const u32 id=m->id;
        modules.registerModule({id,{},[owner,id,first]{return std::make_unique<WeaponModuleState>(owner,id,first);},
            "original weapon prolog/epilog, typed object constructor/init/motion consumers; full gameplay remains gated"});
    }
    std::weak_ptr<WeaponDomain> weak=shared_from_this();
    objects.originalManager().moduleConstruct=[weak](NativeObject *p,u32 id){auto d=weak.lock();if(!d)throw std::logic_error("retired weapon constructor registry");return d->construct(p,id);};
}
void WeaponDomain::registerProlog(ModuleContext &c,u32 id,const TaskEntry &bodyCall){
    if(currentModule && currentModule!=id)throw std::logic_error("two weapon modules would overwrite shared registrations");
    ModuleContext::Scope scope(c);currentModule=id;
    try{bodyCall();}catch(...){clearBindings(id);throw;}
}
void WeaponDomain::clearBindings(u32 id){if(currentModule!=id)return;WeaponInitFunc={};WeaponMoveFunc={};ObjInitFunc={};PlWepMot={};currentModule=0;initialized=false;}
NativeObject *WeaponDomain::construct(NativeObject *p,u32 id){
    if(id>=ObjInitFunc.size() || !ObjInitFunc[id] || !currentModule)throw MissingGameConsumer("ObjInitFunc/unregistered native constructor id="+std::to_string(id));
    NativeObject *result=nullptr;const auto token=modules.nativeCallToken(currentModule);
    modules.callNative(token,[&]{result=ObjInitFunc[id](p);});
    if(!result)throw std::logic_error("original weapon placement returned null");
    auto *weapon=dynamic_cast<NativeWeapon*>(result);if(!weapon)throw std::logic_error("registered weapon native type identity");
    weapon->weaponDomain=shared_from_this();weapon->codeOwner=modules.borrowNativeCode(currentModule);return result;
}
NativeWeapon *WeaponDomain::createBack(u32 id){auto h=objects.create(id,true);if(!h)return nullptr;auto *p=dynamic_cast<NativeWeapon*>(objects.checkedObject(*h));if(!p)throw std::logic_error("non-weapon object in original weapon init");return p;}
void *WeaponDomain::arc(u32 slot) const {if(slot<4 || slot-4>=index.entries.size())throw std::out_of_range("WEP_ARC_PTR slot");return const_cast<u8*>(body.data+index.entries[slot-4].offset);}
ResourceLease WeaponDomain::lease(void *p) const {for(const auto &e:index.entries)if(body.data+e.offset==p)return {body.owner,body.data+e.offset,e.size};throw std::invalid_argument("unowned weapon BIN/TPL/FCV/SEQ reference");}
u8 *WeaponDomain::dataAddress(bool alternate){
    // Fresh native storage preserves old immutable leases when a GC fixed
    // buffer would have been overwritten. Original byte limits remain exact.
    auto &buffer=alternate?alternateBuffer:regularBuffer;buffer=runtime.heaps().allocate(0x70000,13,false);return static_cast<u8*>(buffer.data);
}
void WeaponDomain::setDrsName(char *name){auto n=std::strlen(name);if(n<4 || (std::strcmp(name+n-4,".das") && std::strcmp(name+n-4,".drs")))throw std::invalid_argument("original SET_DRS_NAME source suffix");std::memcpy(name+n-4,".drs",5);}
int WeaponDomain::read(const char *name,void *dst,u32 offset,u32 bytes,u32 aram,u32 mode){
    if(!requests.empty() || offset || bytes || aram || mode!=0x8001)throw std::invalid_argument("original weapon DVD request contract");
    auto target=regularBuffer.data==dst?regularBuffer:alternateBuffer;
    if(!dst || target.data!=dst || nextRequest==std::numeric_limits<int>::max())throw std::invalid_argument("weapon DVD destination/id");
    // Native original ReadCheck/TaskSleep performs the wait. Do not block the
    // frame-owner OS thread in the GameCube synchronous DVD driver branch.
    auto request=runtime.loader().request(name,0x8000);const int id=nextRequest++;
    requests.emplace(id,Request{request,target});trace.push_back("ReadWepData:DVD_READ_N/owned-async");return id;
}
int WeaponDomain::readCheck(int id,WeaponReadInfo *out){
    auto it=requests.find(id);if(it==requests.end() || !out)throw std::invalid_argument("stale weapon DVD request");
    std::shared_ptr<const LoadedFile> result;auto status=runtime.loader().readCheck(it->second.read,result);
    if(!status)return 0;
    auto target=it->second.target;requests.erase(it);if(status!=1 || !result)return -1;
    // cDvdQueue::readMain only writes sizeTbl for TRANS_MRAM/TRANS_ARAM.
    // TRANS_NEST sound banks occupy depth 1 and never root read_size[0][1].
    // Keep the complete checked bank under its file owner for later SndBlkInit;
    // it must not be copied/count as a second weapon MRAM archive.
    *out={};out->read_size[0][0]=result->bodySize;
    if(result->bodySize>target.bytes)return 1; // Original WEP_DATA_MAX rejection.
    auto *data=static_cast<u8*>(target.data);
    std::memcpy(data,result->bytes.data()+result->bodyOffset,result->bodySize);
    loaded=std::move(result);body={target.owner,data,loaded->bodySize};sound={loaded,loaded->bytes.data()+loaded->soundOffset,loaded->soundSize};
    index=indexAssetBody(body.data,body.size);out->start_addr[0][0]=data;return 1;
}
WeaponRelView *WeaponDomain::embedded(void *data){
    if(data!=body.data || !body.owner)throw std::invalid_argument("weapon embedded REL body owner");
    BigEndianView be(body.data,body.size);auto offset=be.u32(4);
    if(!offset || !*offset || *offset>body.size || body.size-*offset<0x4c)throw std::invalid_argument("weapon REL bounds");
    BigEndianView header(body.data+*offset,body.size-*offset);auto id=header.u32(0);if(!id)throw std::invalid_argument("weapon REL identity");
    auto &catalog=originalModuleCatalog().modules;auto m=std::find_if(catalog.begin(),catalog.end(),[&](const auto &row){return row.id==*id;});
    if(m==catalog.end() || m->name.rfind("wep",0)!=0)throw std::invalid_argument("embedded REL is not an original weapon module");
    rel.metadata=*m;rel.offset=*offset;rel.index=indexRelModule(body.data+*offset,body.size-*offset,*m);rel.bssSize=rel.index.bssBytes;return &rel;
}
void WeaponDomain::link(WeaponRelView *m,void*){if(m!=&rel)throw std::invalid_argument("foreign weapon REL view");runtime.awaitModule(ModuleOperation::Activate,m->metadata.id);trace.push_back(m->metadata.name+":original-prolog/native-module");}
void WeaponDomain::unlink(void *p){
    if(p!=&rel)throw std::invalid_argument("foreign native weapon module");
    if(currentModule){auto id=currentModule;if(runtime.loader().scheduler().current())runtime.awaitModule(ModuleOperation::Deactivate,id);else modules.deactivateId(id);}
    clearRead();
}
void WeaponDomain::bindInitCallback(PlayerDomain &p){
    if(!currentModule || !WeaponInitFunc)throw MissingGameConsumer("WeaponInitFunc/native module did not register");
    const auto token=modules.nativeCallToken(currentModule);std::weak_ptr<WeaponDomain> weak=shared_from_this();
    p.WeaponInitFunc=[weak,token](PlayerDomain *player){auto d=weak.lock();if(!d || !player)throw std::logic_error("retired native weapon callback");d->modules.callNative(token,[&]{d->invokeInit(*player);});};
}
void WeaponDomain::invokeInit(PlayerDomain &p){
    if(activePlayer || !p.constructing || !p.modelHandle)throw std::logic_error("weapon init requires an owned player construction");
    activePlayer=&p;
    try{WeaponInitFunc(&p);initialized=true;p.construction.weaponInitialized=true;activePlayer=nullptr;}
    catch(...){activePlayer=nullptr;throw;}
}
void WeaponDomain::clearRead(){
    // Unpublished actor rollback releases exactly the counts acquired by this
    // constructor; shared effect owners from other actors are not erased.
    for(auto id:effectLoads)runtime.effects().releaseData(id,1,0);effectLoads.clear();
    loaded.reset();body={};sound={};index={};regularBuffer={};alternateBuffer={};initialized=false;pG->pWep=nullptr;
}
void WeaponDomain::clear(){
    for(auto &r:requests){runtime.loader().cancel(r.second.read);std::shared_ptr<const LoadedFile> ignored;runtime.loader().readCheck(r.second.read,ignored);}requests.clear();
    if(currentModule)unlink(&rel);else clearRead();
}
WeaponReadSnapshot WeaponDomain::snapshot() const {return {bool(body.owner),bool(WeaponInitFunc),bool(WeaponMoveFunc),initialized,currentModule,mappedWeapon,body.size,sound.size,rel.offset,rel.index.exactBytes,loaded?loaded->identity.path:"",loaded?loaded->identity.sha256:"",body.owner?sha256(body.data,body.size):""};}
void WeaponDomain::PlHandgunMove(PlayerDomain*){throw MissingGameConsumer("PlHandgunMove/full original player routines");}
void WeaponDomain::EspDataLoad(void *p,u32 id,int multiple){
    auto before=runtime.effects().dataSnapshot(id).loads;
    runtime.effects().loadData(lease(p),id,multiple);
    if(runtime.effects().dataSnapshot(id).loads>before)effectLoads.push_back(id);
}
ModelInfoView *WeaponDomain::initializeObjectModel(NativeWeapon &p,void *bin,void *tpl){
    if(!p.manager || p.modelHandle || !p.codeOwner)throw std::logic_error("weapon model registration owner");
    p.manager->index(&p);
    auto owner=std::shared_ptr<ModelCpu>(p.manager->arena.owner,static_cast<ModelCpu*>(&p));
    p.modelHandle=runtime.engineConsumers().initializeModel(lease(bin),lease(tpl),"original weapon",false,1,p.be_flag,std::move(owner));
    trace.push_back("weapon:modelInit/original-owned-BIN-TPL");return p.pModelInfo;
}
void WeaponDomain::bindParent(NativeWeapon &p,ModelCpu *parent){
    if(!activePlayer || parent!=activePlayer->actorModel.get())throw std::logic_error("weapon/player parent identity");
    p.parentOwner=runtime.engineConsumers().borrowModel(*activePlayer->modelHandle);
}
void WeaponDomain::setMotion(NativeWeapon &p,void *m,u8 hokan,u16 frame,u16 flags,void *seq){if(!p.modelHandle)throw std::logic_error("weapon motion model owner");runtime.engineConsumers().initializeMotion(*p.modelHandle,lease(m),seq?lease(seq):ResourceLease{},hokan,flags,frame);}
int WeaponDomain::moveMotion(NativeWeapon &p){if(!p.modelHandle)throw std::logic_error("weapon motion model owner");p.matCalc();return runtime.engineConsumers().advanceMotion(*p.modelHandle,pG->Speed);}
WeaponRuntime::WeaponRuntime(GameRuntime &r,PlayerDomain &p):domain_(std::make_shared<WeaponDomain>(r,p)){domain_->installModules();}
WeaponRuntime::~WeaponRuntime()=default;
WeaponReadSnapshot WeaponRuntime::snapshot() const {return domain_->snapshot();}
void WeaponRuntime::clear(){domain_->clear();}
std::shared_ptr<WeaponDomain> WeaponRuntime::domain() const {return domain_;}
ModelInfoView *NativeWeapon::modelInit(void *bin,void *tpl){if(!weaponDomain)throw std::logic_error("weapon module owner absent");return weaponDomain->initializeObjectModel(*this,bin,tpl);}
void NativeWeapon::motionSet(void *m,u8 h,u16 f,u16 s,void *seq){weaponDomain->setMotion(*this,m,h,f,s,seq);}
int NativeWeapon::motionMove(){return weaponDomain->moveMotion(*this);}
} // namespace re4::port
