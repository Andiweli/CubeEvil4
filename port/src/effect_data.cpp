#include "effect_original.h"
#include "re4/port/sha256.h"
#include <algorithm>
#include <cstring>
namespace re4::port {
namespace {
void bounds(ResourceLease b,u32 at,std::size_t n){if(!b.owner || !b.data || at>b.size || n>b.size-at)throw std::invalid_argument("original EFF resource bounds");}
u32 word(ResourceLease b,u32 at){bounds(b,at,4);return u32(b.data[at])<<24|u32(b.data[at+1])<<16|u32(b.data[at+2])<<8|b.data[at+3];}
u16 half(ResourceLease b,u32 at){bounds(b,at,2);return u16(u16(b.data[at])<<8|b.data[at+1]);}
ResourceLease slice(ResourceLease b,u32 at,u32 n){bounds(b,at,n);return {b.owner,b.data+at,n};}
EffectIds ids(ResourceLease b){EffectIds out;out.source=b;out.num=word(b,0);bounds(b,4,std::size_t(out.num)*8);for(u32 i=0;i<out.num;++i)out.ent.push_back({half(b,4+i*8),half(b,6+i*8),word(b,8+i*8)});return out;}
EffectOffsets offsets(ResourceLease b){
    EffectOffsets out;out.source=b;out.num=word(b,0);bounds(b,4,std::size_t(out.num)*4);
    std::vector<u32> starts;for(u32 i=0;i<out.num;++i){auto at=word(b,4+i*4);if(at<4+u64(out.num)*4 || at>=b.size || at%4)throw std::invalid_argument("original EFF table-relative offset");starts.push_back(at);}
    for(auto at:starts){u32 end=b.size;for(auto next:starts)if(next>at)end=std::min(end,next);out.entries.push_back(slice(b,at,end-at));}return out;
}
EffectAnimation animation(ResourceLease b){
    bounds(b,0,16);EffectAnimation a;a.source=b;a.Width=half(b,0);a.Height=half(b,2);
    auto x=half(b,4),y=half(b,6);std::memcpy(&a.Cx,&x,2);std::memcpy(&a.Cy,&y,2);
    a.Frames=half(b,8);a.Xn=b.data[10];a.Loop=b.data[11];a.Data_num=b.data[12];
    bounds(b,16,a.Frames);return a;
}
EffectModelData model(ResourceLease b){
    bounds(b,0,20);std::array<u32,4> at{word(b,4),word(b,8),word(b,12),word(b,16)};
    if(!at[0] || !at[1])throw std::invalid_argument("original EFM lacks model/TPL");
    std::array<ResourceLease,4> parts;
    for(u32 i=0;i<4;++i)if(at[i]){if(at[i]<20 || at[i]>=b.size || at[i]%4)throw std::invalid_argument("original EFM entry-relative offset");u32 end=b.size;for(auto next:at)if(next>at[i])end=std::min(end,next);parts[i]=slice(b,at[i],end-at[i]);}
    return {parts[0],parts[1],parts[2],parts[3]};
}
}
std::shared_ptr<EffectDataFile> resolveEffectData(ResourceLease b){
    if(!b.data)return {};bounds(b,0,48);auto out=std::make_shared<EffectDataFile>();out->Num=word(b,0);out->source=b;
    if(out->Num!=11)return out; // Original version rejection still owns its load-count transition.
    std::array<u32,11> at;for(u32 i=0;i<11;++i){at[i]=word(b,4+i*4);if((i<5 && !at[i]) || (at[i] && (at[i]<48 || at[i]>=b.size || at[i]%4)))throw std::invalid_argument("original EFF header table bounds");}
    for(u32 i=0;i<11;++i)if(at[i]){u32 end=b.size;for(auto next:at)if(next>at[i])end=std::min(end,next);auto part=slice(b,at[i],end-at[i]);if(i<5)out->ids[i]=ids(part);else out->tables[i-5]=offsets(part);}
    if(out->ids[0].num!=out->tables[0].num || out->ids[0].num!=out->tables[1].num || out->ids[4].num!=out->tables[5].num)throw std::invalid_argument("original EFF texture/animation/model table counts");
    for(u32 i=0;i<out->ids[0].num;++i){if(out->ids[0].ent[i].id>255)throw std::invalid_argument("original effect texture id truncation");out->textures.push_back(resolveMessageTpl(out->tables[0].entries[i],NativeEffectTexMax));out->animations.push_back(animation(out->tables[1].entries[i]));}
    for(u32 i=0;i<out->ids[4].num;++i){if(out->ids[4].ent[i].id>255)throw std::invalid_argument("original effect model id truncation");out->models.push_back(model(out->tables[5].entries[i]));}
    for(u32 i=1;i<4;++i)if(out->ids[i].num!=out->tables[i+1].num)throw std::invalid_argument("original EFF EST/SST/path count mismatch");
    return out;
}
int EffectDomain::loadData(ResourceLease bytes,u32 id,int multiple){
    check();if(id>=NativeEffectNone)throw std::out_of_range("effect data owner id");
    // Decode into separate native views. No in-place relocation of immutable
    // GC bytes or u32 pointer casts. Malformed offsets fail before publication.
    auto candidate=resolveEffectData(bytes);const auto before=system.ownerCnt[id];
    try{auto result=EspDataLoad(candidate.get(),id,multiple);if(!before && system.ownerCnt[id])resources[id]=std::move(candidate);return result;}
    catch(...){if(!before && system.ownerCnt[id]){EspDataRelease(id,0,0);releaseResourceViews(id);}throw;}
}
void EffectDomain::releaseResourceViews(u32 id){
    for(auto &w:system.Esp_tex_tbl)if(w.Owner==NativeEffectNone)w={};
    for(auto &w:system.efmWk)if(w.owner==NativeEffectNone)w={};
    for(auto *table:{system.estTbl,system.sstTbl,system.pathTbl})for(u32 i=0;i<NativeEffectMax;++i)if(table[i].owner==NativeEffectNone)table[i]={};
    for(u32 i=0;i<NativeEffectTexMax;++i)if(!system.GetTexObjFlag(i))system.texObj[i]={};
    resources[id].reset();loadedTlut={};
}
int EffectDomain::releaseData(u32 id,int count,int warn){check();if(id>=NativeEffectNone)throw std::out_of_range("effect release owner id");auto result=EspDataRelease(id,count,warn);if(!system.ownerCnt[id])releaseResourceViews(id);return result;}
} // namespace re4::port
