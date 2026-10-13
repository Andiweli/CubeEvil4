#include "id_save_original.h"
#include "re4/port/game_runtime.h"
#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <limits>
#include <new>

namespace re4::port {
namespace {
std::atomic<u64> nextIdRuntime{1};
u64 runtimeIdentity() {
    auto id=nextIdRuntime.load(std::memory_order_relaxed);
    for(;;) {
        if(id==std::numeric_limits<u64>::max()) throw std::overflow_error("ID runtime identity");
        if(nextIdRuntime.compare_exchange_weak(id,id+1,std::memory_order_relaxed)) return id;
    }
}
}
void NativeInitLog::err(int,int,const char *fmt,...) {
    va_list args; va_start(args,fmt); std::vfprintf(stderr,fmt,args); va_end(args);
    std::fputc('\n',stderr); // Original errors can return; never fake a successful allocation.
}
void *IdSystemCpu::allocatePool(std::size_t n,int flag,int heap) {
    if(allocation || flag!=1 || heap!=13 || n%sizeof(ID_UNIT)) throw std::invalid_argument("ID pool allocation");
    if(!n) return nullptr;
    allocation=heaps.allocate(n,heap);
    auto *p=static_cast<ID_UNIT *>(allocation.data);
    for(std::size_t i=0;i<n/sizeof(ID_UNIT);++i) new(p+i) ID_UNIT{};
    return p;
}
void IdSystemCpu::Mem_free(void *p) {
    if(p!=allocation.data) throw std::invalid_argument("ID pool foreign free");
    allocation={};
}
void *RoomSaveCpu::mem_calloc(std::size_t n,int flag,int heap) {
    if(allocation || flag!=1 || heap!=13 || n<sizeof(RoomSaveHdr) || (n-sizeof(RoomSaveHdr))%sizeof(ROOM_SAVE_DATA))
        throw std::invalid_argument("room state allocation");
    allocation=heaps.allocate(n,heap,true);
    auto *h=new(allocation.data) RoomSaveHdr{};
    auto *records=reinterpret_cast<ROOM_SAVE_DATA*>(reinterpret_cast<u8*>(h)+sizeof(*h));
    for(std::size_t i=0;i<(n-sizeof(*h))/sizeof(*records);++i) new(records+i) ROOM_SAVE_DATA{};
    return h;
}
void *SaveImageCpu::mem_calloc(std::size_t n,int flag,int heap) {
    if(flag!=1 || heap!=13 || n<0x3740 || n>UINT32_MAX) throw std::invalid_argument("save image allocation");
    auto memory=heaps.allocate(n,heap,true);
    auto *head=new(memory.data) SAVE_DATA_HEAD{};
    allocations.push_back(std::move(memory)); return head;
}

struct IdRuntime::Impl {
    IdSystemCpu cpu;
    NativeScheduler &scheduler;
    const std::thread::id owner=std::this_thread::get_id();
    const u64 runtime=runtimeIdentity();
    u64 serial=0;
    std::vector<u64> versions;
    bool initialized=false;
    Impl(GameplayHeaps &h,NativeScheduler &s):cpu(h),scheduler(s) {}
    void check(bool ready=true) const {
        if(owner!=std::this_thread::get_id()) throw std::logic_error("ID owner thread");
        if(ready && !initialized) throw std::logic_error("ID pool not initialized");
    }
    u64 next() {
        if(serial==std::numeric_limits<u64>::max()) throw std::overflow_error("ID generation");
        return ++serial;
    }
    ID_UNIT *resolve(IdHandle h) const {
        check();
        if(h.runtime!=runtime || h.slot>=versions.size() || h.generation!=versions[h.slot] || cpu.m_IdUnit[h.slot].be_flag==0xFF)
            throw std::invalid_argument("stale ID unit handle");
        return cpu.m_IdUnit+h.slot;
    }
    void invalidate() { for(auto &v:versions) v=next(); }
};
IdRuntime::IdRuntime(GameplayHeaps &h,NativeScheduler &s):impl_(std::make_unique<Impl>(h,s)) {}
IdRuntime::~IdRuntime()=default;
void IdRuntime::initialize() {
    auto &i=*impl_; i.check(false);
    if(i.initialized || i.scheduler.allocatedStacks()) throw std::logic_error("ID boot boundary");
    i.cpu.trace.clear(); i.cpu.gameInit(ORIGINAL_MAIN_ID_CAPACITY);
    if(!i.cpu.m_IdUnit || i.cpu.m_maxId!=ORIGINAL_MAIN_ID_CAPACITY) throw std::bad_alloc();
    i.initialized=true; i.versions.resize(static_cast<std::size_t>(i.cpu.m_maxId)); i.invalidate();
}
void IdRuntime::cockpitGameInit() { impl_->check(); impl_->cpu.cockpitGameInit(); impl_->invalidate(); }
void IdRuntime::roomInit() { impl_->check(); impl_->cpu.roomInit(); impl_->invalidate(); }
void IdRuntime::clear() {
    auto &i=*impl_; i.check(false);
    if(i.scheduler.allocatedStacks()) throw std::logic_error("ID free requires released task stacks");
    if(i.initialized) i.cpu.free();
    i.initialized=false; i.versions.clear();
}
IdSystemSnapshot IdRuntime::snapshot() const {
    auto &i=*impl_; i.check(false); auto &c=i.cpu;
    return {c.m_maxId,c.m_nId,c.m_levelMax,{c.m_set_flag,c.m_set_flag+8},{c.m_disp_off,c.m_disp_off+8},i.initialized};
}
std::optional<IdHandle> IdRuntime::pull() {
    auto &i=*impl_; i.check(); auto *u=i.cpu.unitPull(); if(!u) return {};
    auto n=static_cast<u32>(u-i.cpu.m_IdUnit); i.versions[n]=i.next(); return IdHandle{n,i.versions[n],i.runtime};
}
const ID_UNIT &IdRuntime::unit(IdHandle h) const { return *impl_->resolve(h); }
ID_UNIT &IdRuntime::unit(IdHandle h) { return *impl_->resolve(h); }
void IdRuntime::parent(IdHandle p,IdHandle c) {
    auto &i=*impl_; auto *parent=i.resolve(p),*child=i.resolve(c);
    // Reject cycles before the original recursive depth walk. Existing original
    // callers form trees; malformed native references must not recurse forever.
    std::size_t depth=0;
    for(auto *u=parent;u;u=u->pParent) {
        const auto at=reinterpret_cast<std::uintptr_t>(u),base=reinterpret_cast<std::uintptr_t>(i.cpu.m_IdUnit);
        if(at<base || at-base>=i.versions.size()*sizeof(ID_UNIT) || (at-base)%sizeof(ID_UNIT))
            throw std::invalid_argument("ID parent outside native pool");
        if(u==child || ++depth>i.versions.size()) throw std::invalid_argument("ID parent cycle");
    }
    i.cpu.unitParent(parent,child);
}
void IdRuntime::push(IdHandle h) { auto &i=*impl_; i.cpu.unitPush(i.resolve(h)); }
void IdRuntime::kill(u8 mark,ID_CLASS c) { impl_->check(); impl_->cpu.kill(mark,c); }
void IdRuntime::display(ID_CLASS c,int sw) { impl_->check(); if(unsigned(c)>255) throw std::out_of_range("ID class"); impl_->cpu.dispSw(c,sw); }
int IdRuntime::isSet(ID_CLASS c) const { impl_->check(); if(unsigned(c)>255) throw std::out_of_range("ID class"); return impl_->cpu.setCk(c); }
void IdRuntime::time(IdHandle h,s16 t) { impl_->cpu.setTime(impl_->resolve(h),t); }
void IdRuntime::moving(IdHandle h,bool sw) { impl_->cpu.beMove(impl_->resolve(h),sw?1:0); }
const std::vector<std::string> &IdRuntime::trace() const { impl_->check(false); return impl_->cpu.trace; }

struct SaveStateRuntime::Impl {
    GameplayHeaps &heaps;
    NativeScheduler &scheduler;
    RoomSaveCpu rooms;
    SaveImageCpu save;
    NativeSaveImage current;
    u64 generation=0;
    std::vector<u32> sections;
    const std::thread::id owner=std::this_thread::get_id();
    Impl(GameplayHeaps &h,NativeScheduler &s):heaps(h),scheduler(s),rooms(h),save(h,rooms) {}
    void check(bool ready=true) const {
        if(owner!=std::this_thread::get_id()) throw std::logic_error("save owner thread");
        if(ready && !current.head) throw std::logic_error("save startup storage not initialized");
    }
    std::vector<std::uintptr_t> pointers(const SAVE_DATA_HEAD &h) const {
        return {reinterpret_cast<std::uintptr_t>(h.pGlobal),reinterpret_cast<std::uintptr_t>(h.pRm),
                reinterpret_cast<std::uintptr_t>(h.pSs),reinterpret_cast<std::uintptr_t>(h.pMr),reinterpret_cast<std::uintptr_t>(h.pItm)};
    }
    void validate(const NativeSaveImage &image,bool addressRequired=false) const {
        check();
        auto allocation=std::find_if(save.allocations.begin(),save.allocations.end(),[&](const auto &a){return a.data==image.head && a.owner==image.owner;});
        if(image.generation!=generation || allocation==save.allocations.end() || image.head->size!=allocation->bytes)
            throw std::invalid_argument("stale/foreign save image");
        auto &h=*image.head; auto base=reinterpret_cast<std::uintptr_t>(h.base);
        if(addressRequired && h.base!=&h) throw std::invalid_argument("save image is not rebased");
        auto values=pointers(h);
        for(std::size_t n=0;n<values.size();++n)
            if(values[n]<base || values[n]-base!=sections[n]) throw std::invalid_argument("save section offset/reference");
    }
    void roomImage(const NativeSaveImage &image) const {
        validate(image,true);
        auto *h=static_cast<const RoomSaveHdr *>(image.head->pRm);
        const auto limit=sections[2]-sections[1];
        if(h->size<sizeof(RoomSaveHdr) || h->size>limit || h->num>(h->size-sizeof(RoomSaveHdr))/sizeof(ROOM_SAVE_DATA))
            throw std::invalid_argument("save room record boundary");
    }
};
SaveStateRuntime::SaveStateRuntime(GameplayHeaps &h,NativeScheduler &s):impl_(std::make_unique<Impl>(h,s)) {}
SaveStateRuntime::~SaveStateRuntime()=default;
void SaveStateRuntime::initialize() {
    auto &i=*impl_; i.check(false);
    if(i.current.head || i.scheduler.allocatedStacks()) throw std::logic_error("save boot boundary");
    if(i.generation==std::numeric_limits<u64>::max()) throw std::overflow_error("save generation");
    ++i.generation; i.rooms.trace.clear(); i.save.trace.clear(); i.rooms.init();
    auto *head=i.save.alloc(); auto &a=i.save.allocations.back();
    i.current={a.owner,head,i.generation}; auto base=reinterpret_cast<std::uintptr_t>(head);
    i.sections.clear(); for(auto p:i.pointers(*head)) {
        if(p<base || p-base>UINT32_MAX || p-base>=head->size) throw std::logic_error("original save section boundary");
        i.sections.push_back(static_cast<u32>(p-base));
    }
    new(head->pGlobal) GameSaveBlock{};
    new(head->pRm) RoomSaveHdr{};
    auto *records=reinterpret_cast<ROOM_SAVE_DATA*>(static_cast<u8*>(head->pRm)+sizeof(RoomSaveHdr));
    for(u32 n=0;n<i.rooms.m_SaveNum;++n) new(records+n) ROOM_SAVE_DATA{};
    new(head->pSs) SSCRN_SAVE_DATA{}; new(head->pMr) MERCHANT_DATA{}; new(head->pItm) _ITEM_SAVE_DATA{};
    // cGameSave::alloc only clears storage. It does not save a gameplay state.
}
void SaveStateRuntime::clear() {
    auto &i=*impl_; i.check(false);
    if(i.scheduler.allocatedStacks()) throw std::logic_error("save free requires released task stacks");
    i.current={}; i.save.allocations.clear(); i.rooms.allocation={};
    i.rooms.m_pRoomSaveHead=nullptr; i.rooms.m_pRoomSaveData=nullptr; i.rooms.m_RoomNum=i.rooms.m_SaveNum=0; i.sections.clear();
}
SaveStateSnapshot SaveStateRuntime::snapshot() const {
    auto &i=*impl_; i.check(false); SaveStateSnapshot s;
    s.rooms=i.rooms.m_RoomNum; s.savedRooms=i.rooms.m_SaveNum; s.sectionOffsets=i.sections; s.initialized=bool(i.current.head);
    if(s.initialized) { s.roomBytes=i.rooms.m_pRoomSaveHead->size; s.imageBytes=i.current.head->size;
        auto *p=reinterpret_cast<const ROOM_SAVE_DATA*>(i.rooms.m_pRoomSaveData);
        for(u32 n=0;n<s.savedRooms;++n) s.roomIds.push_back(p[n].RoomNo);
    } return s;
}
NativeSaveImage SaveStateRuntime::image() const { impl_->check(); return impl_->current; }
NativeSaveImage SaveStateRuntime::copyImage(const NativeSaveImage &src) {
    auto &i=*impl_; i.validate(src,true); auto memory=i.heaps.allocate(src.head->size,13);
    auto *head=new(memory.data) SAVE_DATA_HEAD(*src.head);
    std::memcpy(reinterpret_cast<u8*>(head)+sizeof(*head),reinterpret_cast<const u8*>(src.head)+sizeof(*head),src.head->size-sizeof(*head));
    // Begin the lifetimes of typed copied sections before their first access.
    for(std::size_t n=0;n<i.sections.size();++n) {
        auto *p=reinterpret_cast<u8*>(head)+i.sections[n];
        if(n==0) new(p) GameSaveBlock;
        if(n==1) { new(p) RoomSaveHdr; for(u32 j=0;j<i.rooms.m_SaveNum;++j) new(p+sizeof(RoomSaveHdr)+j*sizeof(ROOM_SAVE_DATA)) ROOM_SAVE_DATA; }
        if(n==2) new(p) SSCRN_SAVE_DATA;
        if(n==3) new(p) MERCHANT_DATA;
        if(n==4) new(p) _ITEM_SAVE_DATA;
    }
    i.save.allocations.push_back(memory); NativeSaveImage result{memory.owner,head,i.generation};
    i.validate(result); i.save.checkAddr(head); i.validate(result,true); return result;
}
void SaveStateRuntime::offsets(const NativeSaveImage &h) { impl_->validate(h); impl_->save.calcOffset(h.head,0); impl_->validate(h); }
void SaveStateRuntime::addresses(const NativeSaveImage &h) { impl_->validate(h); impl_->save.calcAddr(h.head); impl_->validate(h,true); }
void SaveStateRuntime::saveRooms(const NativeSaveImage &h) { impl_->validate(h,true); impl_->rooms.save(h.head->pRm); }
void SaveStateRuntime::loadRooms(const NativeSaveImage &h) { impl_->roomImage(h); impl_->rooms.load(h.head->pRm); }
void SaveStateRuntime::clearRooms(const NativeSaveImage &h) { impl_->roomImage(h); impl_->rooms.clear(h.head->pRm); }
bool SaveStateRuntime::save(const NativeSaveImage &h,int mode,NativeGameState &g,SubScreenRuntime &ss,MerchantRuntime &mr) {
    auto &i=*impl_;i.validate(h);
    if(!h.head->base)return i.save.save(h.head,mode); // Original offset-form image returns false without mutation.
    i.validate(h,true);
    if(!ss.snapshot().initialized || !mr.snapshot().initialized)throw std::logic_error("aggregate save managers not initialized");
    if(!ss.usesState(g) || !mr.usesState(g))throw std::invalid_argument("aggregate save state owner");
    auto &s=i.save;
    if(s.pG)throw std::logic_error("recursive aggregate save");
    struct Unbind { SaveImageCpu &s;~Unbind(){s.pG=nullptr;s.ItemMgr.writer={};s.subscreenWriter={};s.merchantWriter={};} } unbind{s};
    s.pG=&g;s.ItemMgr.writer=[&ss](void *p){ss.saveInventory(*static_cast<_ITEM_SAVE_DATA *>(p));};
    s.subscreenWriter=[&ss](SSCRN_SAVE_DATA *p){ss.saveState(*p);};
    s.merchantWriter=[&mr](void *p){mr.save(*static_cast<MERCHANT_DATA *>(p));};
    return s.save(h.head,mode);
}
bool SaveStateRuntime::load(const NativeSaveImage &h,NativeGameState &g,SubScreenRuntime &ss,MerchantRuntime &mr) {
    auto &i=*impl_;i.validate(h);
    if(!h.head->base)return i.save.load(h.head);
    i.roomImage(h);
    if(h.head->pGlobal->SaveKind==3)throw MissingGameConsumer("cGameSave::load/new round/SndBgmTblInit");
    if(!ss.snapshot().initialized || !mr.snapshot().initialized)throw std::logic_error("aggregate load managers not initialized");
    if(!ss.usesState(g) || !mr.usesState(g))throw std::invalid_argument("aggregate load state owner");
    ss.validateInventory(*static_cast<_ITEM_SAVE_DATA *>(h.head->pItm));
    auto &s=i.save;if(s.pG)throw std::logic_error("recursive aggregate load");
    struct Unbind { SaveImageCpu &s;~Unbind(){s.pG=nullptr;s.ItemMgr.reader={};s.subscreenReader={};s.merchantReader={};s.costume={};} } unbind{s};
    s.pG=&g;s.ItemMgr.reader=[&ss](void *p){ss.loadInventory(*static_cast<_ITEM_SAVE_DATA *>(p));};
    s.subscreenReader=[&ss](SSCRN_SAVE_DATA *p){ss.loadState(*p);};
    s.merchantReader=[&mr](void *p){mr.load(*static_cast<MERCHANT_DATA *>(p));};
    s.costume=[&ss]{return ss.setCostume();};
    return s.load(h.head);
}
int SaveStateRuntime::passed(u16 room,int part) const { impl_->check(); if(part<0 || part>7) throw std::out_of_range("passed bit"); return impl_->rooms.checkPassed(room,part); }
void SaveStateRuntime::setPassed(u16 room,int part) { impl_->check(); if(part<0 || part>7) throw std::out_of_range("passed bit"); impl_->rooms.setPassed(room,part); }
const std::vector<std::string> &SaveStateRuntime::trace() const { impl_->check(false); return impl_->save.trace; }
} // namespace re4::port
