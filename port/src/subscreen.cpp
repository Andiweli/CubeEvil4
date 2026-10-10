#include "subscreen_original.h"
#include <algorithm>
#include <cstdarg>
#include <new>

namespace re4::port {
void InventoryCpu::Log::err(int,int,const char *format,...) {
    char text[256]; va_list args; va_start(args,format);
    std::vsnprintf(text,sizeof(text),format,args); va_end(args);
    // Original diagnostic does not abort a recoverable item-capacity failure.
    std::fprintf(stderr,"%s\n",text);
}
void *InventoryCpu::mem_alloc(std::size_t n,int flags,int heap) {
    if(flags!=1 || heap!=13 || n>UINT32_MAX) throw std::invalid_argument("original inventory allocation");
    auto a=heaps.allocate(n,heap,true); auto *p=a.data;
    if(!p) throw std::bad_alloc();
    allocations.push_back(std::move(a)); return p;
}
void InventoryCpu::Mem_free(void *p) {
    auto i=std::find_if(allocations.begin(),allocations.end(),[p](const auto &a){return a.data==p;});
    if(i==allocations.end()) throw std::invalid_argument("inventory double/foreign free");
    allocations.erase(i);
}
cItem *InventoryCpu::allocateItems(int n) {
    auto *p=static_cast<cItem *>(mem_alloc(std::size_t(n)*sizeof(cItem),1,13));
    for(int i=0;i<n;++i) new(p+i)cItem{};
    return p;
}
ITEM_ORDER *InventoryCpu::allocateOrders(int n) {
    auto *p=static_cast<ITEM_ORDER *>(mem_alloc(std::size_t(n)*sizeof(ITEM_ORDER),1,13));
    for(int i=0;i<n;++i) new(p+i)ITEM_ORDER{};
    return p;
}
void InventoryCpu::reset() {
    allocations.clear(); m_pAvailable=nullptr; m_pItem=m_pNew=m_pWep=nullptr; m_p_order_tbl=nullptr;
    m_flag_num=m_array_num=m_order_tbl_num=0; used_id=m_wep_id=0; m_char=0; m_to_whom=0;
    m_bonus_time=m_bonus_point=0;
}
void SubScreenContext::check(bool task) const {
    if(owner!=std::this_thread::get_id()) throw std::logic_error("SubScreen owner thread");
    if(task && !loader.scheduler().current()) throw std::logic_error("SubScreen initialization requires original stackful task");
}
void SubScreenContext::OSReport(const char *fmt,...) const {
    char text[128]; va_list args; va_start(args,fmt); std::vsnprintf(text,sizeof(text),fmt,args); va_end(args);
    std::fputs(text,stderr); // Native OSReport output; no gameplay state mutation.
}
int SubScreenContext::DvdReadN(const char *path,void *dst,u32 address,int offset,int size,int mode) {
    check(true);
    if(dst || offset || size || mode!=9) throw std::invalid_argument("SubScreen original DVD mode");
    auto *f=files->find(disc,path);
    if(!f || address<0xD00000 || address>=0x1000000 ||
       f->size>0x1000000-address || ((f->size+31u)&~31u)>0x1000000-address)
        throw std::invalid_argument("SubScreen FST/reserved ARAM boundary");
    if(nextRead==INT32_MAX) throw std::overflow_error("SubScreen request identity");
    int id=nextRead++;
    auto req=loader.request(path,9,4,0,0,address); // Actual sync cDvd::ReadReq/blockRead and DMA.
    reads.emplace(id,Read{req,address}); trace.push_back(std::string("DvdReadN:")+path);
    return id;
}
int SubScreenContext::DvdFacade::ReadCheck(int id,int *mram,int *aram,u32 *address) {
    auto &c=*context; c.check(true); auto it=c.reads.find(id);
    if(it==c.reads.end()) throw std::invalid_argument("SubScreen stale DVD request");
    std::shared_ptr<const LoadedFile> file;
    if(c.loader.readCheck(it->second.request,file)!=1 || !file) {
        c.reads.erase(it); throw std::runtime_error("SubScreen original DVD read failed");
    }
    const u32 dma=(file->bodySize+31u)&~31u;
    auto readback=c.loader.readAram(it->second.address,file->bodySize);
    ResourceLease bytes{readback.owner,readback.data,readback.size};
    c.loader.releaseMemory(readback); // Lease pins the checked MRAM copy.
    c.loaded.push_back({file->identity,it->second.address,dma,bytes});
    if(mram) *mram=0;
    if(aram) *aram=static_cast<int>(dma); // trans2aram_cb reports ALIGN32(size).
    if(address) *address=it->second.address; // GC addrTbl[0][0] is a logical ARAM address.
    c.reads.erase(it); return 1;
}
struct SubScreenRuntime::Impl { SubScreenContext c;
    Impl(EngineLoader &l,GameplayHeaps &h,NativeGameState &g,u8 &lang,std::shared_ptr<const ImportedFiles> f,u8 d)
        :c(l,h,g,lang,std::move(f),d) {} };
SubScreenRuntime::SubScreenRuntime(EngineLoader &l,GameplayHeaps &h,NativeGameState &g,u8 &lang,
                                 std::shared_ptr<const ImportedFiles> f,u8 d)
    :impl_(std::make_unique<Impl>(l,h,g,lang,std::move(f),d)) {}
SubScreenRuntime::~SubScreenRuntime()=default;
void SubScreenRuntime::initializeInventory() {
    auto &c=impl_->c; c.check();
    if(c.inventoryReady) throw std::logic_error("inventory already initialized");
    if(!c.ItemMgr.init()) throw std::runtime_error("original cItemMgr::init failed");
    c.inventoryReady=true; c.trace.push_back("cItemMgr::init:original");
}
void SubScreenRuntime::gameInit() {
    auto &c=impl_->c; c.check(true);
    if(!c.inventoryReady) throw std::logic_error("missing original Title_task ItemMgr.init prerequisite");
    c.initialized=false; c.loaded.clear(); c.SubScreenGameInit(); c.initialized=true;
}
void SubScreenRuntime::roomInit() { auto &c=impl_->c; c.check(); if(!c.inventoryReady) throw std::logic_error("inventory not ready"); c.SubScreenRoomInit(); }
void SubScreenRuntime::wait(int frames) { impl_->c.check(); impl_->c.SubScreenWait(frames); }
void SubScreenRuntime::clear() {
    auto &c=impl_->c; c.check();
    if(c.loader.scheduler().allocatedStacks()) throw std::logic_error("SubScreen full reset requires all task stacks released");
    for(auto &r:c.reads) c.loader.cancel(r.second.request);
    c.reads.clear(); c.loaded.clear(); c.ItemMgr.reset(); c.MapMgr.roomInit();
    c.SubScreenWk={}; c.initialized=c.inventoryReady=false; c.trace.clear();
}
SubScreenSnapshot SubScreenRuntime::snapshot() const {
    auto &c=impl_->c; c.check(); auto &w=c.SubScreenWk;
    return {w.filename,w.pFreeOffs,w.pPreplfOffs,w.pCommonOffs,w.pSwitchOffs,w.p_module,
            w.board_size,w.board_next,w.map_mode,w.open_flag,w.attr_flag,w.close_flag,w.wait_cnt,
            c.initialized,!c.MapMgr.pArray && !c.MapMgr.nArray && !c.MapMgr.pAlive};
}
std::vector<SubScreenResource> SubScreenRuntime::resources() const { impl_->c.check(); return impl_->c.loaded; }
ITEM_INFO SubScreenRuntime::itemInfo(u16 id) const { impl_->c.check(); ITEM_INFO r{}; re4::port::itemInfo(id,&r); return r; }
const cItem *SubScreenRuntime::findItem(u16 id) const { impl_->c.check(); return impl_->c.ItemMgr.search(id); }
int SubScreenRuntime::getItem(u16 id,int n) { impl_->c.check(); if(!impl_->c.inventoryReady) throw std::logic_error("inventory not ready"); return impl_->c.ItemMgr.get(id,n); }
void SubScreenRuntime::clearInventory() { impl_->c.check(); impl_->c.ItemMgr.clear(); }
void SubScreenRuntime::inventoryRoomInit() { impl_->c.check(); impl_->c.ItemMgr.roomInit(); }
int SubScreenRuntime::itemSlot(const cItem *p) const { impl_->c.check(); return impl_->c.ItemMgr.searchAt(const_cast<cItem *>(p)); }
int SubScreenRuntime::itemCapacity() const { impl_->c.check(); return impl_->c.ItemMgr.m_array_num; }
void mapAreaFilename(int,char *);
std::string SubScreenRuntime::mapFilename(int area) const {
    impl_->c.check(); if(area<0 || area>17) throw std::out_of_range("original map area");
    char name[64]{}; mapAreaFilename(area,name); return name;
}
const std::vector<std::string> &SubScreenRuntime::trace() const { impl_->c.check(); return impl_->c.trace; }
} // namespace re4::port
