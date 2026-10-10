#include "re4/port/tex_render.h"
#include "re4/port/game_runtime.h"
#include <limits>
#include <algorithm>
namespace re4::port {
TexRenderRuntime::TexRenderRuntime(std::function<MemoryReservation(u32)> allocator):allocate_(std::move(allocator)) {
    if(!allocate_) throw std::invalid_argument("TexRender allocator required");
    for(auto &t:g_RndMgr) t.owner=this;
    TexRenderMgrInit();
}
TexRenderRuntime::~TexRenderRuntime() { clear(); }
void TexRenderRuntime::check() const { if(owner_!=std::this_thread::get_id()) throw std::logic_error("TexRender frame owner"); }
void TexRenderRuntime::clear() { check(); buffers_.clear(); TexRenderMgrRoomInit(); }
TexRenderTarget *TexRenderRuntime::initialize(int size,int repeat) {
    check(); if(size<0 || repeat<0 || repeat>2) throw std::invalid_argument("TexRender dimensions/repeat");
    auto previous=g_RndMgrNum; TexRenderTarget *p=nullptr;
    try { TexRenderInit(&p,size,repeat); return p; }
    catch(...) { if(p) { release(p->GetBufAddr()); p->Init(); } g_RndMgrNum=previous; throw; }
}
const TexRenderTarget &TexRenderRuntime::target(u32 n) const { check(); if(n>=g_RndMgrNum) throw std::out_of_range("TexRender slot"); return g_RndMgr[n]; }
u32 TexRenderTarget::GetTexBufSize() const {
    auto bytes=u64(m_W_size)*m_H_size*4;
    if(!bytes || bytes>UINT32_MAX) throw std::overflow_error("TexRender buffer size");
    return static_cast<u32>(bytes);
}
void *TexRenderRuntime::allocate(TexRenderTarget *target,u32 bytes) {
    if(!target || target->owner!=this || std::none_of(g_RndMgr.begin(),g_RndMgr.end(),[&](const auto &t){return &t==target;}))
        throw std::logic_error("foreign TexRender target");
    check(); auto buffer=allocate_(bytes);
    if(!buffer.owner || !buffer.data || buffer.size<bytes || reinterpret_cast<std::uintptr_t>(buffer.data)%32)
        throw std::runtime_error("TexRender requires owned 32-aligned buffer");
    auto ptr=buffer.data;
    if(!buffers_.emplace(ptr,std::move(buffer)).second) throw std::logic_error("duplicate TexRender allocation");
    return ptr;
}
void TexRenderRuntime::release(void *p) { check(); if(p && !buffers_.erase(p)) throw std::logic_error("foreign TexRender buffer"); }
std::size_t TexRenderRuntime::bufferBytes() const { check(); std::size_t n=0; for(auto &b:buffers_) n+=b.second.size; return n; }
void TexRenderRuntime::copyToTexture() { check(); throw MissingGameConsumer("CopyTexRenderMgr/GX EFB copy and GPU texture object (Phase 5)"); }
} // namespace re4::port
