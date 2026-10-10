#include "light_original.h"
#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <set>
#include <limits>

namespace re4::port {
namespace {
thread_local LightCall *currentLightCall{};
struct CallScope {
    LightCall *previous;
    explicit CallScope(LightCall &c):previous(currentLightCall) { currentLightCall=&c; }
    ~CallScope() { currentLightCall=previous; }
};
void range(ResourceLease b,std::size_t at,std::size_t n) {
    if(!b.owner || !b.data || at>b.size || n>b.size-at) throw std::invalid_argument("LIT/path resource boundary");
}
u32 word(ResourceLease b,std::size_t at) { range(b,at,4);return u32(b.data[at])<<24|u32(b.data[at+1])<<16|u32(b.data[at+2])<<8|b.data[at+3]; }
u16 half(ResourceLease b,std::size_t at) { range(b,at,2);return u16(u16(b.data[at])<<8|b.data[at+1]); }
std::size_t nativeWordCount(u32 bytes) { auto n=(u64(bytes)+3)/4;if(n>std::numeric_limits<std::size_t>::max()/4)throw std::invalid_argument("light native allocation overflow");return std::size_t(n); }
void scalar32(std::vector<u32> &to,ResourceLease b,std::size_t at) { auto v=word(b,at);std::memcpy(reinterpret_cast<u8*>(to.data())+at,&v,4); }
void scalar16(std::vector<u32> &to,ResourceLease b,std::size_t at) { auto v=half(b,at);std::memcpy(reinterpret_cast<u8*>(to.data())+at,&v,2); }
void diagnostics(const char *fmt,va_list args) { std::vfprintf(stderr,fmt,args);std::fputc('\n',stderr); }
u64 identity() {
    static std::atomic<u64> next{1};auto n=next.load();
    for(;;) { if(n==std::numeric_limits<u64>::max()) throw std::overflow_error("light runtime identity");if(next.compare_exchange_weak(n,n+1)) return n; }
}
} // namespace
cLit *LightBlockView::header() { return reinterpret_cast<cLit*>(nativeWords.data()); }
const cLit *LightBlockView::header() const { return reinterpret_cast<const cLit*>(nativeWords.data()); }
LightPathHeader *LightPathView::header() { return reinterpret_cast<LightPathHeader*>(nativeWords.data()); }
std::shared_ptr<LightBlockView> resolveLightBlock(ResourceLease b) {
    range(b,0,4);auto n=half(b,0);
    if(b.data[2]>0x2C) throw std::invalid_argument("LIT version newer than original versionUp");
    range(b,4,std::size_t(n)*4);
    auto out=std::make_shared<LightBlockView>();out->source=b;out->sourceVersion=b.data[2];
    out->nativeWords.resize(nativeWordCount(b.size));std::memcpy(out->nativeWords.data(),b.data,b.size);
    cLit h{};h.CutNum=n;h.Version=b.data[2];h.nMaxLight=b.data[3];new(out->nativeWords.data())cLit(h);
    std::set<std::pair<u32,u32>> spans;
    for(u32 i=0;i<n;++i) {
        auto at=word(b,4+i*4);out->cutOffsets.push_back(at);scalar32(out->nativeWords,b,4+i*4);
        if(!at) continue;
        if(at%4 || at<4u+4u*n) throw std::invalid_argument("LIT cut alignment/header overlap");
        range(b,at,sizeof(cLightEnv));auto count=word(b,at+4);
        if(count>(b.size-at-sizeof(cLightEnv))/sizeof(cLightWork)) throw std::invalid_argument("LIT work count boundary");
        auto bytes=u32(sizeof(cLightEnv)+std::size_t(count)*sizeof(cLightWork));
        if(!spans.emplace(at,bytes).second) continue; // Original shared cut offset.
        auto *native=reinterpret_cast<u8*>(out->nativeWords.data());
        // Native objects live in aligned storage; the pointer-free records have
        // identical fixed layouts on both supported ABIs. Retain unknown bytes.
        auto env=cLightEnv{};std::memcpy(&env,b.data+at,sizeof(env));new(native+at)cLightEnv(env);
        for(auto off:{4u,8u,12u,16u,24u,28u,32u,40u,68u,248u}) scalar32(out->nativeWords,b,at+off);
        for(u32 j=0;j<count;++j) {
            auto wAt=at+u32(sizeof(cLightEnv))+j*u32(sizeof(cLightWork));
            auto w=cLightWork{};std::memcpy(&w,b.data+wAt,sizeof(w));new(native+wAt)cLightWork(w);
            for(auto off:{4u,8u,12u,16u,24u,32u,40u}) scalar32(out->nativeWords,b,wAt+off);
            scalar16(out->nativeWords,b,wAt+36);scalar16(out->nativeWords,b,wAt+38);
            for(u32 off=44;off<80;off+=4) scalar32(out->nativeWords,b,wAt+off);
            auto work=wAt+108;auto id=b.data[wAt+2];
            if(id>16) throw std::invalid_argument("LIT handler ID outside original LightFuncTbl");
            u32 floatCount=id==2?4:id==3?3:id==6?3:id==7?6:0;
            for(u32 k=0;k<floatCount;++k) scalar32(out->nativeWords,b,work+k*4);
            if(id==4) {
                scalar16(out->nativeWords,b,work);scalar16(out->nativeWords,b,work+4);scalar16(out->nativeWords,b,work+6);
                for(u32 k=12;k<24;k+=4) scalar32(out->nativeWords,b,work+k);
            }
            // Id 5 pointers are runtime state. The wire Id/Flag at 0xC/0xD
            // remain raw until LightCpu assignment expands them into native work.
        }
    }
    u32 end=4u+4u*n;
    for(auto [at,bytes]:spans) { if(at<end) throw std::invalid_argument("LIT overlapping cut ranges");end=at+bytes; }
    return out;
}
std::shared_ptr<LightPathView> resolveLightPaths(ResourceLease b) {
    range(b,0,4);auto n=b.data[0];range(b,4,std::size_t(n)*4);
    auto out=std::make_shared<LightPathView>();out->source=b;
    out->nativeWords.resize(nativeWordCount(b.size));std::memcpy(out->nativeWords.data(),b.data,b.size);
    LightPathHeader h{};std::memcpy(&h,b.data,sizeof(h));new(out->nativeWords.data())LightPathHeader(h);
    for(u32 i=0;i<n;++i) {
        auto at=word(b,4+i*4);out->offsets.push_back(at);scalar32(out->nativeWords,b,4+i*4);
        if(!at) { out->lengths.push_back(0);continue; }
        if(at<4u+4u*n || at>=b.size) throw std::invalid_argument("light path offset boundary");
        auto end=at;while(end<b.size && b.data[end]!=0xFF) {
            if(b.data[end]>200) throw std::invalid_argument("original light brightness outside 0..200");++end;
        }
        if(end==b.size || end==at) throw std::invalid_argument("light path empty/missing terminator");
        out->lengths.push_back(end-at+1);
    }
    return out;
}
LightCall &LightCall::active() { if(!currentLightCall) throw std::logic_error("original light call domain");return *currentLightCall; }
bool LightCall::ownsData(const void *p) const {
    auto a=reinterpret_cast<std::uintptr_t>(p);
    auto owns=[&](const auto &words){auto base=reinterpret_cast<std::uintptr_t>(words.data());return p && a>=base && a-base<words.size()*4;};
    if(paths && owns(paths->nativeWords)) return true;
    return std::any_of(blocks.begin(),blocks.end(),[&](const auto &b){return b && owns(b->nativeWords);});
}
ModelCpu *LightCall::resolve(u8 type,u16 no) {
    if(!resolver) throw MissingGameConsumer("light parent manager lookup type "+std::to_string(type));
    auto h=resolver(type,no);lastResolved=h;return h ? &consumers.model(*h) : nullptr;
}
ModelCpu *LightCall::enemy(u16 n) { return resolve(1,n); }
int resolveEtc(u16 n,ModelCpu **p) { *p=LightCall::active().resolve(3,n);return *p?1:0; }
u32 readNativePathOffset(LightPathHeader *p,u8 n) { u32 ofs{};std::memcpy(&ofs,reinterpret_cast<u8*>(p)+4+u32(n)*4,4);return ofs; }
void LightLog::err(int,int,const char *fmt,...) { va_list a;va_start(a,fmt);diagnostics(fmt,a);va_end(a); }
void LightLog::warn(int,int,const char *fmt,...) { va_list a;va_start(a,fmt);diagnostics(fmt,a);va_end(a); }
template<class T> void LightPool<T>::log(const char *fmt,...) { va_list a;va_start(a,fmt);diagnostics(fmt,a);va_end(a); }
template void LightPool<LightCpu>::log(const char *,...);
void LightCpu::prepareNativeWork(const cLightWork &w) {
    if(Id==5) {
        const auto *wire=reinterpret_cast<const u8*>(&w.sub);
        auto id=wire[12],flags=wire[13];
        new(work)LIT05_MOVE_FREE{};
        auto *pathWork=reinterpret_cast<LIT05_MOVE_FREE*>(work);pathWork->Id=id;pathWork->Flag=flags;
    }
}
int LightCpu::setParent(ModelCpu *) { throw MissingGameConsumer("cLight::setParent(cModel*) manager identity search"); }
struct LightRuntime::Impl {
    GameplayHeaps &heaps;NativeScheduler &domain;EngineConsumers &consumers;
    LightManagerCpu mgr;LightCall call;
    const std::thread::id owner=std::this_thread::get_id();
    u64 runtime=identity(),epoch=1;bool initialized{};
    std::vector<u64> generations;std::vector<std::optional<ConsumerHandle>> parents;std::vector<std::string> trace;
    Impl(GameplayHeaps &h,NativeScheduler &s,EngineConsumers &c):heaps(h),domain(s),consumers(c),mgr(h),call(mgr,c) {}
    void check(bool quiescent=false) const {
        if(owner!=std::this_thread::get_id()) throw std::logic_error("light frame owner");
        if(quiescent && domain.allocatedStacks()) throw std::logic_error("light release requires quiescent stacks");
    }
    void invalidate() { if(epoch==std::numeric_limits<u64>::max()) throw std::overflow_error("light epoch");++epoch; }
    LightCpu *unit(LightHandle h) {
        check();if(h.runtime!=runtime || h.epoch!=epoch || h.slot>=mgr.nArray || h.generation!=generations[h.slot] || !mgr.pArray[h.slot].isAlive()) throw std::invalid_argument("stale/foreign light handle");
        if(parents[h.slot] && &consumers.model(*parents[h.slot])!=mgr.pArray[h.slot].pParent) throw std::logic_error("changed light model owner");
        return mgr.pArray+h.slot;
    }
};
LightRuntime::LightRuntime(GameplayHeaps &h,NativeScheduler &s,EngineConsumers &c,NativeGameState &):impl_(std::make_unique<Impl>(h,s,c)) {}
LightRuntime::~LightRuntime()=default;
void LightRuntime::clear() {
    auto &i=*impl_;i.check(true);CallScope scope(i.call);i.mgr.destroyAll();i.mgr.arrayFree();i.mgr.LightPool<LightCpu>::roomInit();
    i.call.blocks.clear();i.call.paths.reset();i.mgr.pLitPath=nullptr;i.mgr.pLitHeader=i.mgr.m_pLitCore=i.mgr.m_pLitRoom=i.mgr.m_pLitRoom2=nullptr;
    i.call.resolver={};i.generations.clear();i.parents.clear();i.initialized=false;i.invalidate();i.trace.clear();
}
void LightRuntime::initialize() {
    auto &i=*impl_;i.check();if(i.mgr.pArray) throw std::logic_error("original light init would abandon live room pool");
    CallScope scope(i.call);i.mgr.init(LightFuncTbl);i.call.blocks.clear();i.call.paths.reset();i.initialized=true;i.invalidate();i.trace.push_back("cLightMgr::init:original");
}
void LightRuntime::initializePaths(ResourceLease b) {
    auto &i=*impl_;i.check();if(!i.initialized) throw std::logic_error("light manager uninitialized");
    if(i.mgr.countActiveWork()) throw std::logic_error("cannot replace path data while lights retain cursors");
    auto p=resolveLightPaths(b);i.call.paths=p;CallScope scope(i.call);
    if(i.mgr.initPath(p->header())!=1) throw std::runtime_error("original light initPath failed");i.trace.push_back("cLightMgr::initPath:original");
}
void LightRuntime::bindRoom(ResourceLease core,ResourceLease room,ResourceLease third,u32 capacity) {
    auto &i=*impl_;i.check(true);if(!i.initialized || !capacity || capacity>4096) throw std::invalid_argument("light room pool contract");
    std::vector<std::shared_ptr<LightBlockView>> blocks{resolveLightBlock(core),resolveLightBlock(room),resolveLightBlock(third)};
    CallScope scope(i.call);i.mgr.destroyAll();i.mgr.arrayFree();i.call.blocks=std::move(blocks);i.invalidate();
    if(!i.mgr.roomInit(i.call.blocks[0]->header(),i.call.blocks[1]->header(),i.call.blocks[2]->header())) throw std::runtime_error("original light roomInit failed");
    i.mgr.arrayAlloc(capacity);i.generations.assign(capacity,0);i.parents.assign(capacity,std::nullopt);i.trace.push_back("cLightMgr::roomInit:original");
    i.trace.push_back("cLit::versionUp:core-room-original");
}
void LightRuntime::bindArchives(ResourceLease core,ResourceLease room) {
    impl_->check(true);if(!core.owner || !room.owner)throw std::invalid_argument("light archive ownership");auto ci=indexAssetBody(core.data,core.size),ri=indexAssetBody(room.data,room.size);
    auto span=[](ResourceLease b,const AssetSpan *e){if(!e)throw std::invalid_argument("missing original light/CNS archive entry");return ResourceLease{b.owner,b.data+e->offset,e->size};};
    if(!core.owner || !room.owner || ci.entries.size()<=ORIGINAL_CORE_LIGHT_SLOT-4) throw std::invalid_argument("light archive ownership/core index");
    const auto &ce=ci.entries[ORIGINAL_CORE_LIGHT_SLOT-4];
    if(std::memcmp(ce.tag.data(),"LIT",3)) throw std::invalid_argument("CORE_LIGHT tag");
    LightConstants cons;std::vector<u32> constants;
    if(auto *e=ri.find("CNS",0)) {
        auto bytes=span(room,e);auto n=word(bytes,0);auto bitWords=(std::size_t(n)>>5)+1;
        auto limit=(bytes.size-4)/4;if(n>limit || bitWords>limit-n)throw std::invalid_argument("CNS count/bitmap boundary");
        range(bytes,4,4*(bitWords+std::size_t(n)));
        constants.resize(1+bitWords+n);for(std::size_t k=0;k<constants.size();++k) constants[k]=word(bytes,k*4);
        cons.ConsInitRoom(reinterpret_cast<ConsRoom*>(constants.data()));
    } else cons.ConsInitRoom(nullptr);
    auto capacity=cons.ConsGetRoomValue(CONS_R_NLIGHT);
    bindRoom(span(core,&ce),span(room,ri.find("LIT",0)),span(room,ri.find("LIT",1)),capacity);
    impl_->trace.push_back("ConsInitRoom/ConsGetRoomValue:original-CONS_R_NLIGHT");
}
LightSnapshot LightRuntime::snapshot() const {
    auto &i=*impl_;i.check();LightSnapshot s;s.initialized=i.initialized;s.pathsBound=bool(i.call.paths);s.capacity=i.mgr.nArray;
    for(u32 n=0;n<i.mgr.nArray;++n) if(i.mgr.pArray[n].isAlive()) ++s.alive;
    s.electricPower=i.mgr.ElecPower;s.environment=i.mgr.LightEnv;
    std::copy_n(i.mgr.kindFlags,8,s.kindFlags.begin());std::copy_n(i.mgr.m_Tune,3,s.tune.begin());return s;
}
const std::vector<std::string> &LightRuntime::trace() const { impl_->check();return impl_->trace; }
std::shared_ptr<const LightPathView> LightRuntime::paths() const { impl_->check();return impl_->call.paths; }
std::vector<int> LightRuntime::pathSteps(u8 id,u8 flags,u32 steps) {
    auto &i=*impl_;i.check();if(!i.call.paths || id>=i.call.paths->offsets.size() || !i.call.paths->offsets[id] || steps>4096 || flags>3) throw std::invalid_argument("light path cursor contract");
    CallScope scope(i.call);cLightPath path{};if(!path.setPath(i.mgr.getPathPtr(id),flags)) throw std::runtime_error("original path setup failed");
    std::vector<int> out;for(u32 n=0;n<steps;++n)out.push_back(path.movePath());return out;
}
std::shared_ptr<const LightBlockView> LightRuntime::block(int n) const { impl_->check();if(n<0 || std::size_t(n)>=impl_->call.blocks.size()) throw std::out_of_range("light data block");return impl_->call.blocks[n]; }
cLightEnv LightRuntime::cut(int b,int n) const { auto p=block(b);if(n<0 || n>=p->header()->CutNum) throw std::out_of_range("LIT cut");CallScope scope(impl_->call);auto *c=const_cast<cLit*>(p->header())->getCut(u16(n));if(!c) throw std::invalid_argument("absent LIT cut");return *c; }
std::vector<cLightWork> LightRuntime::records(int b,int n) const {
    (void)cut(b,n);auto p=block(b);CallScope scope(impl_->call);auto *c=const_cast<cLit*>(p->header())->getCut(u16(n));
    std::vector<cLightWork> out;for(u32 j=0;j<c->nLight;++j) out.push_back(*c->getLightWork(int(j)));return out;
}
std::optional<LightHandle> LightRuntime::create(int b,int n,int w,bool back) {
    auto &i=*impl_;i.check();auto all=records(b,n);if(w<0 || std::size_t(w)>=all.size()) throw std::out_of_range("light record");
    auto &record=all[std::size_t(w)];if(record.Id==5 && (!i.call.paths || reinterpret_cast<const u8*>(&record.sub)[12]>=i.call.paths->offsets.size() || !i.call.paths->offsets[reinterpret_cast<const u8*>(&record.sub)[12]])) throw std::invalid_argument("light path record reference");
    CallScope scope(i.call);LightCpu *p=nullptr;
    // Resolve required model owners before allocation; no partially published work
    // may survive a missing manager dependency.
    std::optional<ConsumerHandle> parent;
    if(record.ParentType>=1 && record.ParentType<=4) { (void)i.call.resolve(record.ParentType,record.ParentType==1?u8(record.ParentNo):u16(record.ParentNo));parent=i.call.lastResolved; }
    if(record.Id==8 && reinterpret_cast<const u8*>(&record.sub)[0]==0) (void)i.call.enemy(reinterpret_cast<const u8*>(&record.sub)[1]);
    LightCpu *candidate=nullptr;
    for(u32 j=0;j<i.mgr.nArray;++j) { auto k=back?i.mgr.nArray-1-j:j;if(i.mgr.pArray[k].isEmpty()) { candidate=i.mgr.pArray+k;break; } }
    if(candidate && i.generations[u32(candidate-i.mgr.pArray)]==std::numeric_limits<u64>::max()) throw std::overflow_error("light slot generation");
    try { p=back?i.mgr.createBack(&record):i.mgr.create(&record); }
    catch(...) { if(candidate && candidate->isAlive()) i.mgr.destroy(candidate);throw; }
    if(!p || !p->isAlive()) return {};
    auto slot=u32(p-i.mgr.pArray);if(i.generations[slot]==std::numeric_limits<u64>::max()) throw std::overflow_error("light slot generation");++i.generations[slot];
    i.parents[slot]=parent;
    return LightHandle{i.runtime,i.epoch,slot,i.generations[slot]};
}
LightStateView LightRuntime::state(LightHandle h) const {
    auto *p=impl_->unit(h);return {p->be_flag,p->ParentNo,p->Type,p->Id,p->Kind,p->EnableMask,p->ParentType,p->Rno0,p->Pos,p->World,p->normal,p->Radius,p->Intensity,p->Col,p->DispCol,p->LitIndex};
}
void LightRuntime::moveType(LightHandle h) { auto &i=*impl_;auto *p=i.unit(h);CallScope scope(i.call);p->move(); }
void LightRuntime::destroy(LightHandle h) { auto &i=*impl_;auto *p=i.unit(h);CallScope scope(i.call);i.mgr.destroy(p); }
void LightRuntime::event(bool begin) { auto &i=*impl_;i.check();CallScope scope(i.call);if(begin)i.mgr.beginEvent();else i.mgr.endEvent(); }
void LightRuntime::kind(u8 n,bool on) { impl_->check();CallScope scope(impl_->call);if(on)impl_->mgr.onKind(n);else impl_->mgr.offKind(n); }
bool LightRuntime::kind(u8 n) const { impl_->check();CallScope scope(impl_->call);return impl_->mgr.checkKind(n)!=0; }
f32 LightRuntime::power(f32 d) { impl_->check();if(!std::isfinite(d)) throw std::invalid_argument("light electric power delta");CallScope scope(impl_->call);return impl_->mgr.setElecPower(d); }
void LightRuntime::selectBlock(int n) { auto &i=*impl_;i.check();if(n < -1 || n>2 || i.call.blocks.size()!=3) throw std::out_of_range("active light block");CallScope scope(i.call);i.mgr.roomLitSet(n==-1?nullptr:i.call.blocks[n]->header()); }
bool LightRuntime::roomSelected() const { impl_->check();CallScope scope(impl_->call);return impl_->mgr.roomLitCheck()!=0; }
void LightRuntime::tune(int b,int n) { auto c=cut(b,n);CallScope scope(impl_->call);impl_->mgr.setTune(&c); }
void LightRuntime::fogDistances(f32 start,f32 end) { impl_->check();if(!std::isfinite(start)||!std::isfinite(end)) throw std::invalid_argument("fog distance");CallScope scope(impl_->call);impl_->mgr.setFogStart(start);impl_->mgr.setFogEnd(end); }
void LightRuntime::setModelResolver(ModelResolver r) { impl_->check(true);if(snapshot().alive) throw std::logic_error("cannot replace live light model resolver");impl_->call.resolver=std::move(r); }
} // namespace re4::port
