#include "skinning_original.h"
#include <algorithm>
#include <limits>

namespace re4::port::skin {
thread_local Context *pG{};
// Lane-for-lane ps_merge00/01/11 translation of src/lib/psmtx.c.
void PSMTXReorder(const Mtx m,float (*out)[3]) {
    for(u32 c=0;c<4;++c)for(u32 r=0;r<3;++r)out[c][r]=m[r][c];
}
}
namespace re4::port {
namespace {
void bounds(ResourceLease b,std::size_t at,std::size_t n) {
    if(!b.owner || !b.data || at>b.size || n>b.size-at)throw std::invalid_argument("skinning BIN boundary");
}
u16 half(ResourceLease b,std::size_t at) { bounds(b,at,2);return u16(u16(b.data[at])<<8|b.data[at+1]); }
u32 word(ResourceLease b,std::size_t at) { return u32(half(b,at))<<16|half(b,at+2); }
int scale(u32 gqr) { int v=int((gqr>>8)&63);return v>=32?v-64:v; }
template<class T> T transform(const std::array<float,12> &m,const T *v,u32 row,int exponent) {
    // ps_madds0, ps_madds1, ps_madds0: three SINGLE fused operations.
    float t=m[9+row];
    for(u32 c=0;c<3;++c)t=std::fma(m[c*3+row],std::ldexp(float(v[c]),-exponent),t);
    t=std::ldexp(t,exponent);
    if(!std::isfinite(t))throw std::invalid_argument("skinning nonfinite PSQ result");
    t=std::max(float(std::numeric_limits<T>::min()),std::min(float(std::numeric_limits<T>::max()),t));
    return static_cast<T>(t); // PSQ: clamp, then truncate towards zero, not round-to-nearest.
}
void validatePalette(const std::vector<std::array<float,12>> &p) {
    if(p.empty() || p.size()>65535)throw std::invalid_argument("skinning palette count");
    for(auto &m:p)for(float f:m)if(!std::isfinite(f))throw std::invalid_argument("skinning nonfinite matrix");
}
struct Scope {
    explicit Scope(skin::Context &c) { if(skin::pG)throw std::logic_error("nested skinning context");skin::pG=&c; }
    ~Scope() { skin::pG=nullptr; }
};
}
std::vector<s16> skinS16(const std::vector<std::array<float,12>> &p,const s16 *v,u32 n,u32 gqr) {
    validatePalette(p);if((n && !v) || (gqr&0xffff)!=((gqr>>16)&0xffff) || (gqr&7)!=7)throw std::invalid_argument("S16 GQR/input");
    std::vector<s16> out(std::size_t(n)*3);int e=scale(gqr);
    for(u32 i=0;i<n;++i){int id=v[i*4+3];if(id<0 || std::size_t(id)>=p.size())throw std::invalid_argument("S16 palette reference");
        for(u32 r=0;r<3;++r)out[i*3+r]=transform(p[id],v+i*4,r,e);}
    return out;
}
std::vector<s8> skinS8(const std::vector<std::array<float,12>> &p,const u8 *v,u32 n,u32 gqr) {
    validatePalette(p);if((n && !v) || (gqr&0xffff)!=((gqr>>16)&0xffff) || (gqr&7)!=6)throw std::invalid_argument("S8 GQR/input");
    std::vector<s8> out(std::size_t(n)*3);int e=scale(gqr);
    for(u32 i=0;i<n;++i){auto id=v[i*4+3];if(id>=p.size())throw std::invalid_argument("S8 palette reference");s8 xyz[3];
        for(u32 r=0;r<3;++r)xyz[r]=static_cast<s8>(v[i*4+r]);
        for(u32 r=0;r<3;++r)out[i*3+r]=transform(p[id],xyz,r,e);}
    return out;
}
SkinningResult skinModelCpu(ModelCpu &m,ModelInfoView &info) {
    if(!info.model_addr || !m.pList)throw std::invalid_argument("skinning model lifetime");
    if(m.be_flag&0x4000)throw MissingGameConsumer("commonScreenMatSub unskinned model override");
    auto &d=*info.model_addr;auto b=d.source;bounds(b,0,64);
    if(info.pShape || std::any_of(std::begin(info.shape),std::end(info.shape),[](auto &s){return s.data!=nullptr;}))throw MissingGameConsumer("skinning CalculateShape_new active morph channels");
    SkinningResult out;out.source=b;out.extended=d.weight_ext_num>255;
    out.staticBypass=d.weight_palette_num<=1 && !out.extended && d.nParts==1 && !(info.be_flag&2);
    bounds(b,word(b,48),std::size_t(d.nVtx)*8);bounds(b,word(b,52),std::size_t(d.nNrm)*((d.flags&0x20000000)?4:8));
    if(out.staticBypass) { // Exact commonScreenMatSub fast path: no quantisation pass.
        out.positions.resize(std::size_t(d.nVtx)*3);
        for(u32 i=0;i<d.nVtx;++i)for(u32 r=0;r<3;++r)out.positions[i*3+r]=d.vtxOrig[i*4+r];
        if(d.flags&0x20000000){out.normals8.resize(std::size_t(d.nNrm)*3);for(u32 i=0;i<d.nNrm;++i)for(u32 r=0;r<3;++r)out.normals8[i*3+r]=s8(d.nrmOrig[i*4+r]);}
        else {out.normals16.resize(std::size_t(d.nNrm)*3);for(u32 i=0;i<d.nNrm;++i)for(u32 r=0;r<3;++r)out.normals16[i*3+r]=s16(half(b,word(b,52)+i*8+r*2));}
        return out;
    }
    skin::Context context;Scope scope(context);Mtx inverse;
    if(!C_MTXInverse(m.pList->mat,inverse))throw std::invalid_argument("skinning singular root matrix");
    for(auto *p=m.pList;p;p=p->pList){if(++out.boneCount>248)throw std::invalid_argument("original skinning bone palette overflow");
        for(u32 r=0;r<3;++r)for(u32 c=0;c<4;++c)if(!std::isfinite(p->mat[r][c]) || !std::isfinite(p->lt_inv_mat[r][c]))throw std::invalid_argument("skinning invalid bone matrix");}
    skin::calcWeightMat(&m);
    u32 n=out.extended?d.weight_ext_num:d.weight_palette_num;auto offset=word(b,20);bounds(b,offset,std::size_t(n)*(out.extended?12:8));
    if(!n)throw std::invalid_argument("skinning missing weights");context.reordered.resize(n);
    std::vector<skin::cWeight> small(n);std::vector<skin::cWeightExt> large(n);
    for(u32 i=0;i<n;++i){u32 at=offset+i*(out.extended?12:8);u32 count=out.extended?half(b,at+6):b.data[at+3];
        if(!count || count>3)throw std::invalid_argument("skinning influence count");
        small[i].nWeight=u8(count);large[i].nWeight=u16(count);
        for(u32 j=0;j<count;++j){auto id=out.extended?half(b,at+j*2):b.data[at+j];if(id>=out.boneCount)throw std::invalid_argument("skinning bone reference");
            small[i].WeightId[j]=u8(id);large[i].WeightId[j]=u16(id);small[i].Weight[j]=large[i].Weight[j]=b.data[at+(out.extended?8:4)+j];}
    }
    out.influenceCount=u32(out.extended?skin::MakeWeightPaletteExt(large.data(),int(n)):skin::MakeWeightPalette(small.data(),int(n)));
    out.palette.resize(n);for(u32 i=0;i<n;++i)std::memcpy(out.palette[i].data(),context.reordered[i].lanes,48);
    std::vector<s16> neutral;
    const s16 *vertices=d.vtxOrig;
    if(info.be_flag&2){neutral.resize(std::size_t(d.nVtx)*4);ResetShape(&info,neutral.data());vertices=neutral.data();}
    out.positions=skinS16(out.palette,vertices,d.nVtx,(u32(d.shift)<<24)|(u32(d.shift)<<8)|0x00070007);
    if(d.flags&0x20000000)out.normals8=skinS8(out.palette,d.nrmOrig,d.nNrm,0x20062006);
    else {std::vector<s16> normals(std::size_t(d.nNrm)*4);for(std::size_t i=0;i<normals.size();++i)normals[i]=s16(half(b,word(b,52)+i*2));out.normals16=skinS16(out.palette,normals.data(),d.nNrm,0x32073207);}
    return out;
}
Vec SkinningResult::position(u32 i) const { if(std::size_t(i)*3+2>=positions.size())throw std::out_of_range("skin vertex");float f=std::ldexp(1.f,-int(source.data[40]));return {positions[i*3]*f,positions[i*3+1]*f,positions[i*3+2]*f}; }
Vec SkinningResult::normal(u32 i) const {
    if(!normals8.empty()){if(std::size_t(i)*3+2>=normals8.size())throw std::out_of_range("skin normal8");return {normals8[i*3]/64.f,normals8[i*3+1]/64.f,normals8[i*3+2]/64.f};}
    if(std::size_t(i)*3+2>=normals16.size())throw std::out_of_range("skin normal16");return {normals16[i*3]/16384.f,normals16[i*3+1]/16384.f,normals16[i*3+2]/16384.f};
}
} // namespace re4::port
