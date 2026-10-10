#include "message_original.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace re4::port {
namespace {
void range(ResourceLease b,std::size_t at,std::size_t n) {
    if(!b.owner || !b.data || at>b.size || n>b.size-at) throw std::invalid_argument("message resource boundary");
}
u32 word(ResourceLease b,std::size_t at) {
    range(b,at,4); return u32(b.data[at])<<24|u32(b.data[at+1])<<16|u32(b.data[at+2])<<8|b.data[at+3];
}
u16 half(ResourceLease b,std::size_t at) { range(b,at,2); return u16(u16(b.data[at])<<8|b.data[at+1]); }
ResourceLease slice(ResourceLease b,u32 at,u32 n) { range(b,at,n); return {b.owner,b.data+at,n}; }
u32 textureBytes(u32 fmt,u16 w,u16 h) {
    u32 x=4,y=4,bytes=32;
    switch(fmt) {
    case 0: case 8: case 14: x=y=8; break;
    case 1: case 2: case 9: x=8; y=4; break;
    case 3: case 4: case 5: case 10: break;
    case 6: bytes=64; break;
    default: throw std::invalid_argument("unsupported original TPL format");
    }
    if(!w || !h || w>1024 || h>1024) throw std::invalid_argument("GX texture dimensions");
    return ((u32(w)+x-1)/x)*((u32(h)+y-1)/y)*bytes;
}
void slot(int n,int count) { if(n<0 || n>=count) throw std::out_of_range("message slot/index"); }
} // namespace
std::shared_ptr<MessageTpl> resolveMessageTpl(ResourceLease b,u32 capacity) {
    range(b,0,12);
    auto p=std::make_shared<MessageTpl>(); p->source=b;
    p->version=word(b,0); p->numDescriptors=word(b,4);
    auto count=p->numDescriptors,table=word(b,8);
    if(p->version!=0x20af30 || !count || count>capacity || table%4 || table<12)
        throw std::invalid_argument("TPL signature/descriptor capacity");
    range(b,table,std::size_t(count)*8);
    p->textures.resize(count); p->cluts.resize(count); p->descriptors.resize(count);
    for(u32 i=0;i<count;++i) {
        auto tex=word(b,table+i*8),cl=word(b,table+i*8+4);
        if(tex%4) throw std::invalid_argument("TPL texture header alignment");
        range(b,tex,36);
        auto &h=p->textures[i];
        h.height=half(b,tex); h.width=half(b,tex+2); h.format=word(b,tex+4);
        auto pixels=word(b,tex+8);
        h.wrapS=word(b,tex+12); h.wrapT=word(b,tex+16);
        h.minFilter=word(b,tex+20); h.magFilter=word(b,tex+24);
        u32 bias=word(b,tex+28); std::memcpy(&h.LODBias,&bias,4);
        h.edgeLODEnable=b.data[tex+32]; h.minLOD=b.data[tex+33]; h.maxLOD=b.data[tex+34];
        if(pixels%32 || h.wrapS>2 || h.wrapT>2 || !std::isfinite(h.LODBias) || h.minLOD || h.maxLOD || b.data[tex+35])
            throw std::invalid_argument("TPL offsets/LOD: mip chains remain an explicit dependency");
        h.data=slice(b,pixels,textureBytes(h.format,h.width,h.height)); h.unpacked=1;
        auto &c=p->cluts[i];
        if(cl) {
            if(cl%4) throw std::invalid_argument("TPL CLUT header alignment");
            range(b,cl,12); c.numEntries=half(b,cl); c.format=word(b,cl+4);
            auto offset=word(b,cl+8);
            if(b.data[cl+2] || c.format>2 || !c.numEntries || c.numEntries>0x4000 || offset%32)
                throw std::invalid_argument("TPL CLUT format/offset/entries");
            c.data=slice(b,offset,u32(c.numEntries)*2); c.unpacked=1;
        }
        if(h.format>=8 && h.format<=10) {
            auto limit=h.format==8?16u:h.format==9?256u:16384u;
            if(!cl || c.numEntries>limit) throw std::invalid_argument("indexed TPL has no valid palette");
        }
        p->descriptors[i]={&h,&c}; // Absent CLUT has an explicit unloaded native header.
    }
    p->descriptorArray=p->descriptors.data(); return p;
}
void requireMessageTpl(MessageTpl *p) {
    if(!p || !p->source.owner || !p->numDescriptors || p->descriptorArray!=p->descriptors.data() ||
       p->numDescriptors!=p->descriptors.size()) throw std::invalid_argument("unresolved native TPL view");
}
void GXInitTexObj(MessageTexture *o,ResourceLease pixels,u16 w,u16 h,u32 fmt,u32 s,u32 t,int mip) {
    if(!o || s>2 || t>2 || mip) throw std::invalid_argument("GXInitTexObj CPU initialization scope");
    range(pixels,0,textureBytes(fmt,w,h));
    *o={}; o->pixels=pixels; o->width=w; o->height=h; o->format=fmt; o->wrapS=s; o->wrapT=t;
    o->initialized=true; // Actual stored GX defaults, no graphics submission.
}
void GXInitTexObjCI(MessageTexture *o,ResourceLease pixels,u16 w,u16 h,u32 fmt,u32 s,u32 t,int mip,u32 tlut) {
    if(fmt<8 || fmt>10) throw std::invalid_argument("GXInitTexObjCI format");
    GXInitTexObj(o,pixels,w,h,fmt,s,t,mip); o->indexed=true; o->tlutName=tlut;
}
void GXInitTlutObj(MessagePalette *o,ResourceLease entries,u32 fmt,u16 n) {
    if(!o || fmt>2 || !n || n>0x4000) throw std::invalid_argument("GXInitTlutObj format/count");
    range(entries,0,u32(n)*2); *o={entries,fmt,n,true};
}
void MessageFontCpu::requireResolvedPalette(MessageTpl *p) {
    requireMessageTpl(p);
    if(p!=storage.get() || p->numDescriptors>2 || !widths.owner) throw std::logic_error("font view ownership/capacity");
}
void MessageFontCpu::releasePalette() {
    storage.reset(); widths={}; m_tpl=nullptr; pWidth=nullptr;
    for(auto &t:m_mTex) t={};
}
void MessageControlCpu::check(bool task) const {
    if(owner!=std::this_thread::get_id()) throw std::logic_error("message runtime owner thread");
    if(task && !const_cast<EngineLoader &>(loader).scheduler().current()) throw std::logic_error("original message read needs stackful task");
}
void MessageControlCpu::reset() {
    check();
    Clear(); // Original slot deletion before releasing font and table ownership.
    for(auto &r:reads) { loader.cancel(r.second.request); abandoned.push_back(r.second.request); }
    reads.clear();
    collectCancelledReads();
    for(auto &f:MesFont) { if(f.be_flag) f.destroy(); f={}; }
    allocations.clear(); loadedSizes.clear(); core.reset(); coreIndex={}; corePalettes.clear(); tables.clear();
    MesData={}; system.language=1; m_state=m_stop=0; initialized=false;
    for(auto &m:m_Msg) m={};
    for(auto &p:m_font_addr) p=nullptr;
    for(auto &t:Specular) t={};
    for(auto &t:IndTex) t={};
    for(auto &t:GlobalIlmTex) t={};
    ThermoTlut={}; trace.clear(); pG->IsMessageInit=0;
}
void MessageControlCpu::collectCancelledReads() {
    check();
    for(auto it=reads.begin();it!=reads.end();) {
        if(loader.scheduler().state(it->second.caller)) { ++it; continue; }
        loader.cancel(it->second.request); abandoned.push_back(it->second.request); it=reads.erase(it);
    }
    for(auto it=abandoned.begin();it!=abandoned.end();) {
        std::shared_ptr<const LoadedFile> discarded;
        if(loader.readCheck(*it,discarded)==0) ++it; else it=abandoned.erase(it);
    }
}
void *MessageControlCpu::mem_alloc(u32 n,const char *,int,int flag,int heap) {
    if(flag!=1 || heap!=13) throw std::invalid_argument("message original mem_alloc flags");
    auto a=heaps.allocate(n,heap,false); auto ptr=a.data;
    if(!ptr) throw std::bad_alloc(); allocations.push_back(std::move(a)); return ptr;
}
int MessageControlCpu::DvdFacade::FileExistCheck(const char *path,u32 *size) {
    context->check(); auto *f=context->files->find(context->disc,path);
    if(!f) return -1;
    if(size) *size=f->size;
    return static_cast<int>(f->fstIndex);
}
int MessageControlCpu::DvdReadN(const char *path,void *dst,int a,int b,int c,int mode,const char *,int) {
    check(true);
    if(a || b || c || mode!=0x11) throw std::invalid_argument("font original DvdReadN flags");
    auto it=std::find_if(allocations.begin(),allocations.end(),[&](const auto &v){return v.data==dst;});
    if(it==allocations.end()) throw std::invalid_argument("font destination allocation");
    auto *f=files->find(disc,path);
    if(!f || f->size>it->bytes) throw std::invalid_argument("font file exceeds original allocation");
    if(nextRead==INT32_MAX) throw std::overflow_error("message read identity");
    int id=nextRead++; reads.emplace(id,Read{loader.request(path,static_cast<u16>(mode)),dst,it->bytes,false,*loader.scheduler().current()});
    trace.push_back(std::string("DvdReadN:")+path);
    return id;
}
void MessageControlCpu::coreReadBegin() { check(true); if(core) throw std::logic_error("core already owned"); }
int MessageControlCpu::readCore(int index,u16 mode) {
    check(true);
    if(index!=3 || mode!=0x8001) throw std::invalid_argument("original core FileTbl request");
    if(nextRead==INT32_MAX) throw std::overflow_error("message read identity");
    int id=nextRead++; reads.emplace(id,Read{loader.requestFile(3,mode),nullptr,0,true,*loader.scheduler().current()}); return id;
}
int MessageControlCpu::finishRead(int id,ReadInfo *info) {
    check(true); auto it=reads.find(id);
    if(it==reads.end()) throw std::invalid_argument("stale message Dvd request");
    while(loader.status(it->second.request)==LoadStatus::Pending || loader.status(it->second.request)==LoadStatus::Reading)
        TaskSleep(1); // Original blocking Dvd mode, stack survives the read.
    std::shared_ptr<const LoadedFile> f;
    if(loader.readCheck(it->second.request,f)!=1 || !f) { reads.erase(it); throw std::runtime_error("original message/core disc read failed"); }
    if(info) info->read_size[0][0]=f->bodySize;
    if(it->second.core) {
        if(f->bodySize>0x234000) throw std::invalid_argument("original CORE_DATA_MAX exceeded");
        core=f; coreIndex=indexAssetBody(f->bytes.data()+f->bodyOffset,f->bodySize);
        trace.push_back("CoreDataRead:original-verified-body");
    } else {
        if(f->bytes.size()>it->second.capacity) throw std::invalid_argument("font read destination boundary");
        std::memcpy(it->second.destination,f->bytes.data(),f->bytes.size());
        loadedSizes[it->second.destination]=static_cast<u32>(f->bytes.size());
    }
    reads.erase(it); return 1;
}
int MessageControlCpu::DvdFacade::ReadCheck(int id,int a,int b,int c) {
    if(a || b || c) throw std::invalid_argument("message Dvd.ReadCheck signature");
    return context->finishRead(id,nullptr);
}
int MessageControlCpu::DvdFacade::ReadCheck(int id,ReadInfo *info) { return context->finishRead(id,info); }
void *MessageControlCpu::coreArc(u32 n) {
    if(!core || n<4 || n-4>=coreIndex.entries.size()) throw std::out_of_range("original CORE_ARC index");
    auto &e=coreIndex.entries[n-4];
    auto b=slice({core,core->bytes.data()+core->bodyOffset,core->bodySize},e.offset,e.size);
    if(!std::memcmp(e.tag.data(),"TPL",3)) {
        auto found=corePalettes.find(n);
        if(found==corePalettes.end()) {
            u32 cap=n==4?9:n==16?5:1;
            found=corePalettes.emplace(n,resolveMessageTpl(b,cap)).first;
        }
        return found->second.get();
    }
    if(std::memcmp(e.tag.data(),"MDT",3)) throw std::invalid_argument("unported core resource consumer");
    auto found=tables.find(n);
    if(found==tables.end()) {
        auto count=englishMessageCount(b.data,b.size),off=word(b,8);
        std::size_t end=b.size;
        for(u32 i=4;i<=24;i+=4) { auto next=word(b,i); if(next>off && next<end) end=next; }
        if(off%4 || (end-off)%2) throw std::invalid_argument("message block alignment");
        for(u32 i=0;i<count;++i) (void)englishMessage(b.data,b.size,i);
        MessageTable t; t.raw=b; t.native.resize((std::size_t(b.size)+3)/4);
        auto *data=reinterpret_cast<u8 *>(t.native.data()); std::memcpy(data,b.data,b.size);
        auto store=[&](u32 at,u32 v){std::memcpy(data+at,&v,4);};
        store(8,off); store(off,word(b,off)); store(off+4,count);
        for(u32 i=0;i<count;++i) store(off+8+i*4,word(b,off+8+i*4));
        for(std::size_t at=std::size_t(off)+8+std::size_t(count)*4;at<end;at+=2) {
            u16 v=half(b,at); std::memcpy(data+at,&v,2);
        }
        found=tables.emplace(n,std::move(t)).first;
    }
    return found->second.native.data();
}
void MessageControlCpu::setupFontView(int w,int h,u8 *address,int n) {
    slot(n,4);
    if(w<=0 || w>255 || h<=0 || h>255) throw std::invalid_argument("font cell dimensions");
    auto a=std::find_if(allocations.begin(),allocations.end(),[&](const auto &x){return x.data==address;});
    auto loaded=loadedSizes.find(address);
    if(a==allocations.end() || loaded==loadedSizes.end()) throw std::invalid_argument("font buffer not loaded/owned");
    ResourceLease b{a->owner,address,loaded->second};
    auto tpl=word(b,0),width=word(b,4);
    if(tpl<8 || tpl%32 || width<=tpl || width>b.size || (b.size-width)%2 || b.size-width<4)
        throw std::invalid_argument("FNT TPL/width offsets");
    auto p=resolveMessageTpl(slice(b,tpl,width-tpl),2);
    auto &f=MesFont[n]; f.storage=p; f.widths=slice(b,width,b.size-width);
    f.create(w,h,p.get(),const_cast<u8 *>(f.widths.data));
    trace.push_back("MessageFont::create:original-slot-"+std::to_string(n));
}
struct MessageRuntime::Impl { MessageControlCpu c; Impl(EngineLoader &l,GameplayHeaps &h,NativeGameState &g,u8 &language,std::shared_ptr<const ImportedFiles> f,u8 d):c(l,h,g,language,std::move(f),d) {} };
MessageRuntime::MessageRuntime(EngineLoader &l,GameplayHeaps &h,NativeGameState &g,u8 &language,std::shared_ptr<const ImportedFiles> f,u8 d):impl_(std::make_unique<Impl>(l,h,g,language,std::move(f),d)) {}
MessageRuntime::~MessageRuntime()=default;
void MessageRuntime::initialize() {
    auto &c=impl_->c; c.check(true);
    if(c.initialized || c.core) throw std::logic_error("message boot must be released before reinitializing");
    c.system.language=1; // User's reference-language policy, shared with original pSys.
    c.CoreDataRead(); c.init(); c.initialized=true; c.trace.push_back("MessageControl::init:original-complete");
}
void MessageRuntime::gameInit() {
    auto &c=impl_->c; c.check(true);
    if(!c.initialized) throw MissingGameConsumer("cMes.gameInit()");
    c.gameInit(); c.trace.push_back("MessageControl::gameInit:original-complete");
}
void MessageRuntime::clear() {
    impl_->c.check();
    if(impl_->c.loader.scheduler().current()) throw std::logic_error("message release requires frame boundary");
    impl_->c.reset();
}
bool MessageRuntime::ready() const { impl_->c.check(); return impl_->c.initialized; }
void MessageRuntime::setLayout(int n,int type) { impl_->c.check(); slot(n,16); slot(type,9); impl_->c.setLayout(n,type); }
void MessageRuntime::releaseFont(int n) { impl_->c.check(); slot(n,4); impl_->c.releaseFont(n); }
void MessageRuntime::reloadCommonFont() { impl_->c.check(true); impl_->c.loadCommonFont(); }
MessageFontSnapshot MessageRuntime::font(int n) const {
    auto &c=impl_->c; c.check(); slot(n,4); auto &f=c.MesFont[n]; MessageFontSnapshot s;
    s.loaded=(f.be_flag&1)!=0; s.textureWidth=f.m_tex_w; s.textureHeight=f.m_tex_h;
    s.charWidth=f.m_char_w; s.charHeight=f.m_char_h; s.widths=f.widths;
    if(s.loaded) { s.tplSource=f.storage->source; for(u32 i=0;i<f.m_tpl->numDescriptors;++i) { s.textures.push_back(f.m_mTex[i].tex); s.palettes.push_back(f.m_mTex[i].tlut); } }
    return s;
}
MessageLayoutSnapshot MessageRuntime::layout(int n) const { impl_->c.check(); slot(n,16); auto &s=impl_->c.m_Msg[n]; return {s.m_font_w,s.m_font_h,s.m_char_gap,s.m_line_gap,s.m_queue!=nullptr}; }
s16 MessageRuntime::glyphWidth(int n,int glyph,s8 *l,s8 *r) {
    impl_->c.check(); slot(n,4); auto &f=impl_->c.MesFont[n];
    if(!(f.be_flag&1) || !l || !r || glyph<0 || glyph>32767 || std::size_t(glyph?glyph:1)*2+1>=f.widths.size)
        throw std::out_of_range("font glyph width boundary");
    return f.getSize(static_cast<s16>(glyph),l,r);
}
int MessageRuntime::messageCount(int type) { impl_->c.check(); slot(type,4); if(!impl_->c.MesData.m_Data[type]) throw std::logic_error("message data not registered"); return impl_->c.MesData.getMesNum(type); }
ResourceLease MessageRuntime::message(int type,int n) {
    slot(type,4); auto count=messageCount(type); if(n<0 || n>=count) throw std::out_of_range("message number");
    auto &t=impl_->c.tables.at(type==3?21u:10u); auto span=englishMessage(t.raw.data,t.raw.size,static_cast<u32>(n));
    auto *p=impl_->c.MesData.getAddr(n,type);
    if(reinterpret_cast<u8 *>(p)!=reinterpret_cast<u8 *>(t.native.data())+span.offset) throw std::logic_error("original message reference mismatch");
    return slice(t.raw,span.offset,span.size);
}
u16 MessageRuntime::firstNativeWord(int type,int n) { (void)message(type,n); u16 wordValue; std::memcpy(&wordValue,impl_->c.MesData.getAddr(n,type),2); return wordValue; }
std::vector<u16> MessageRuntime::nativeMessageWords(int type,int n) {
    auto raw=message(type,n);
    if(raw.size%2) throw std::invalid_argument("message word span alignment");
    std::vector<u16> out(raw.size/2); std::memcpy(out.data(),impl_->c.MesData.getAddr(n,type),raw.size); return out;
}
void MessageRuntime::collectCancelledReads() { impl_->c.collectCancelledReads(); }
ResourceLease MessageRuntime::coreBody() const { auto &c=impl_->c; c.check(); if(!c.core) return {}; return {c.core,c.core->bytes.data()+c.core->bodyOffset,c.core->bodySize}; }
std::vector<MessageTexture> MessageRuntime::coreTextures() const {
    impl_->c.check(); std::vector<MessageTexture> out;
    for(auto &t:impl_->c.Specular) if(t.initialized) out.push_back(t);
    for(auto &t:impl_->c.IndTex) if(t.initialized) out.push_back(t);
    for(auto &t:impl_->c.GlobalIlmTex) if(t.initialized) out.push_back(t);
    return out;
}
MessagePalette MessageRuntime::thermalPalette() const { impl_->c.check(); return impl_->c.ThermoTlut; }
const std::vector<std::string> &MessageRuntime::trace() const { impl_->c.check(); return impl_->c.trace; }
} // namespace re4::port
