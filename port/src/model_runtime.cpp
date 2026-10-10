#include "model_original.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace re4::port {
namespace {
void bounds(ResourceLease b,std::size_t off,std::size_t n) {
    if (!b.owner || !b.data || off>b.size || n>b.size-off) throw std::invalid_argument("model/texture resource bounds");
}
u32 be32(ResourceLease b,std::size_t p) { bounds(b,p,4); return (u32(b.data[p])<<24)|(u32(b.data[p+1])<<16)|(u32(b.data[p+2])<<8)|b.data[p+3]; }
u16 be16(ResourceLease b,std::size_t p) { bounds(b,p,2); return u16((u16(b.data[p])<<8)|b.data[p+1]); }
f32 befloat(ResourceLease b,std::size_t p) { u32 bits=be32(b,p); f32 f; std::memcpy(&f,&bits,4); if(!std::isfinite(f)) throw std::invalid_argument("nonfinite model/texture scalar"); return f; }
ResourceLease span(ResourceLease b,u32 off,u32 n) { bounds(b,off,n); return {b.owner,b.data+off,n}; }
}
struct ModelRuntime::Asset {
    ResourceLease bin;
    ModelDataView data;
    TexturePaletteView texture;
    std::vector<ModelJoint> joints;
    std::vector<s16> vertices;
    std::vector<u16> blend,flip; // Actual u16 objects; no u32/u16 strict-aliasing overlay.
    Asset(ResourceLease b,ResourceLease t) : bin(b) {
        bounds(b,0,0x40);
        auto &d=data;
        d.version=be32(b,0x3c);
        if(d.version!=0x20010801 && d.version!=0x20030818) notBinData();
        d.nParts=b.data[0x19]; d.shift=b.data[0x28]; d.nVtx=be16(b,0x38); d.nNrm=be16(b,0x3a);
        if(!d.nParts) throw MissingGameConsumer("zero-joint BIN original one-part fallback layout");
        d.displist_num=be16(b,0x1a); d.shapeOfs=be32(b,0x2c); d.flags=be32(b,0x20); d.nTex=be32(b,0x24);
        if(d.shift>=31) throw std::invalid_argument("BIN fixed-point shift");
        auto head=be32(b,0), vtx=be32(b,0x30);
        bounds(b,head,std::size_t(d.nParts)*16); bounds(b,vtx,std::size_t(d.nVtx)*8);
        if(head%4 || be32(b,0x1c)%32) throw std::invalid_argument("BIN joint/primitive alignment");
        joints.resize(d.nParts);
        for(u32 i=0;i<d.nParts;++i) {
            auto at=head+i*16; auto &j=joints[i];
            j={b.data[at],b.data[at+1],b.data[at+2],b.data[at+3],{befloat(b,at+4),befloat(b,at+8),befloat(b,at+12)}};
            if(j.parentNo!=255 && j.parentNo>=d.nParts) throw std::invalid_argument("BIN parent index");
            if(j.parentNo!=255 && j.parentNo>=i) throw std::invalid_argument("BIN parent must precede child for original partsWorldCalc");
        }
        for(u32 i=0;i<d.nParts;++i) for(u32 p=i,n=0;p!=255;++n) {
            if(n>=d.nParts) throw std::invalid_argument("cyclic BIN hierarchy");
            p=joints[p].parentNo;
        }
        vertices.resize(std::size_t(d.nVtx)*4);
        for(std::size_t i=0;i<vertices.size();++i) vertices[i]=static_cast<s16>(be16(b,vtx+i*2));
        d.pHead=joints.data(); d.vtxOrig=vertices.data();
        for(auto field : {0xcu,0x10u,0x14u,0x1cu,0x34u}) bounds(b,be32(b,field),0);
        d.pClr=b.data+be32(b,0xc); d.pTex=b.data+be32(b,0x10); d.pWeight=b.data+be32(b,0x14);
        d.pParts=b.data+be32(b,0x1c); d.nrmOrig=b.data+be32(b,0x34);
        if(d.shapeOfs) bounds(b,d.shapeOfs,4);
        if(d.version==0x20030818) {
            bounds(b,0x40,8);
            auto copyTable=[&](u32 off,std::vector<u16> &out,u32 stride) {
                if(!off) return;
                auto count=be32(b,off);
                if(count>b.size/stride) throw std::invalid_argument("BIN motion table count");
                auto n=std::size_t(count)*stride+4; bounds(b,off,n);
                out.resize((n+1)/2);
                if(reinterpret_cast<std::uintptr_t>(out.data())%4) throw std::runtime_error("native motion table alignment");
                std::memcpy(out.data(),&count,4);
                auto *raw=reinterpret_cast<u8*>(out.data());
                for(std::size_t p=4;p<n;p+=2) { auto value=be16(b,off+p); std::memcpy(raw+p,&value,2); }
            };
            auto flipOffset=be32(b,0x44);
            // MotionMoveCore indexes flipTbl[pno], with pno bounded by nParts.
            // Actual checked BIN files carry the joint count in the preceding word.
            if(flipOffset && be32(b,flipOffset)!=d.nParts)
                throw std::invalid_argument("BIN flip count differs from model joints");
            copyTable(be32(b,0x40),blend,8); copyTable(flipOffset,flip,2);
            auto blendOffset=be32(b,0x40);
            for(u32 i=0;blendOffset && i<be32(b,blendOffset);++i)
                for(u32 j=0;j<3;++j)
                    if(be16(b,blendOffset+4+i*8+j*2)>=d.nParts)
                        throw std::invalid_argument("BIN blend joint reference");
            for(u32 i=0;flipOffset && i<d.nParts;++i) {
                auto index=be16(b,flipOffset+4+i*2);
                if(index!=65535 && index>=d.nParts) throw std::invalid_argument("BIN flip joint reference");
            }
            d.blendTbl=blend.empty()?nullptr:blend.data(); d.flipTbl=flip.empty()?nullptr:reinterpret_cast<u8*>(flip.data());
        }
        texture.bytes=t; texture.version=be32(t,0); texture.numDescriptors=be32(t,4);
        if(texture.version!=0x20af30) throw std::invalid_argument("TPL version");
        auto table=be32(t,8);
        if(texture.numDescriptors>t.size/8) throw std::invalid_argument("TPL descriptor count");
        bounds(t,table,std::size_t(texture.numDescriptors)*8);
        for(u32 i=0;i<texture.numDescriptors;++i) {
            auto at=be32(t,table+i*8);
            if(be32(t,table+i*8+4)) throw MissingGameConsumer("TPL CLUT native texture binding");
            bounds(t,at,36); TextureHeaderView h;
            h.height=be16(t,at); h.width=be16(t,at+2); h.format=be32(t,at+4);
            h.wrapS=be32(t,at+12); h.wrapT=be32(t,at+16); h.minFilter=be32(t,at+20); h.magFilter=be32(t,at+24);
            h.LODBias=befloat(t,at+28); h.edgeLODEnable=t.data[at+32]; h.minLOD=t.data[at+33]; h.maxLOD=t.data[at+34];
            if(!h.width || !h.height || t.data[at+35]) throw std::invalid_argument("TPL dimensions/relocation state");
            u32 bytes=0;
            if(h.format==0 || h.format==14) bytes=((u32(h.width)+7)/8)*((u32(h.height)+7)/8)*32;
            else if(h.format==3) bytes=((u32(h.width)+3)/4)*((u32(h.height)+3)/4)*32;
            else throw MissingGameConsumer("TPL format outside checked original gxtex reader");
            h.data=span(t,be32(t,at+8),bytes);
            texture.descriptors.push_back(h);
        }
    }
};
void calcModelAddr(ModelDataView *d) {
    if(!d || (d->nParts && !d->pHead) || (d->nVtx && !d->vtxOrig)) throw std::invalid_argument("unresolved native BIN view");
    d->resolved=true; // Wire offsets were resolved with bounds and BE conversion in Asset.
}
void calcTplAddr(TexturePaletteView *t) {
    if(!t || t->numDescriptors!=t->descriptors.size()) throw std::invalid_argument("unresolved native TPL view");
    for(auto &h:t->descriptors) { if(!h.data.data) throw std::invalid_argument("TPL image reference"); h.unpacked=1; }
    t->resolved=true;
}
ModelRuntime::ModelRuntime(u32 n,u32 p):infos(n),parts(p) {}
ModelRuntime::~ModelRuntime() = default;
ModelRuntime::Object::~Object() {
    if(cpu.pm) cpu.releaseJoint();
    if(cpu.mm) cpu.releaseModelInfo();
}
std::unique_ptr<ModelRuntime::Object> ModelRuntime::initialize(ResourceLease bin,ResourceLease tpl,bool invisible) {
    auto object=std::make_unique<Object>();
    object->asset=std::make_unique<Asset>(bin,tpl);
    object->cpu.mm=&infos; object->cpu.pm=&parts; object->cpu.systemInvisible=invisible;
    if(!object->cpu.modelInit(&object->asset->data,&object->asset->texture)) throw std::runtime_error("original modelInit failed");
    return object;
}
} // namespace re4::port
