#pragma once
#include "../src/skinning_original.h"
#include <limits>
namespace re4::port {
// Isolated format/arithmetic checks; real original assets use test_skinning.py.
inline u32 verifyCpuSkinningAdapters() {
    u32 checks=0;
    auto check=[&](bool b){++checks;if(!b)throw std::runtime_error("CPU skinning assertion "+std::to_string(checks));};
    auto fail=[&](auto f){bool b=false;try{f();}catch(const std::exception&){b=true;}check(b);};
    std::vector<std::array<float,12>> p(1);p[0]={1,0,0,0,1,0,0,0,1,5,-2,3};
    s16 xyz[]={-3,10,20,0};auto v=skinS16(p,xyz,1,0x00070007);check(v==std::vector<s16>{2,8,23});
    v=skinS16(p,xyz,1,0x02070207);check(v==std::vector<s16>{17,2,32});
    s16 normals[]={16384,-16384,0,0};v=skinS16(p,normals,1,0x32073207);check(v==std::vector<s16>{16384,-16384,0});
    u8 n8[]={64,192,0,0};auto n=skinS8(p,n8,1,0x20062006);check(n==std::vector<s8>{64,-64,0});
    p[0][9]=100000;p[0][10]=-100000;v=skinS16(p,xyz,1,0x00070007);check(v[0]==32767 && v[1]==-32768);
    n=skinS8(p,n8,1,0x00060006);check(n[0]==127 && n[1]==-128);
    p[0][9]=.9f;p[0][10]=-.9f;s16 zeros[]={0,0,0,0};v=skinS16(p,zeros,1,0x00070007);check(v[0]==0 && v[1]==0);
    check(skinS16(p,nullptr,0,0x00070007).empty());check(skinS8(p,nullptr,0,0x20062006).empty());
    xyz[3]=-1;fail([&]{skinS16(p,xyz,1,0x00070007);});xyz[3]=1;fail([&]{skinS16(p,xyz,1,0x00070007);});xyz[3]=0;
    n8[3]=255;fail([&]{skinS8(p,n8,1,0x20062006);});n8[3]=0;
    fail([&]{skinS16(p,nullptr,1,0x00070007);});fail([&]{skinS16(p,xyz,1,0x00060006);});
    fail([&]{skinS16(p,xyz,1,0x01070007);});
    p[0][0]=std::numeric_limits<float>::quiet_NaN();fail([&]{skinS16(p,xyz,1,0x00070007);});
    auto raw=std::make_shared<std::vector<u8>>(128,0);auto put16=[&](u32 at,u16 x){raw->at(at)=u8(x>>8);raw->at(at+1)=u8(x);};auto put32=[&](u32 at,u32 x){put16(at,u16(x>>16));put16(at+2,u16(x));};
    put32(20,80);put32(48,64);put32(52,72);(*raw)[40]=0;
    ModelPart a,b;C_MTXIdentity(a.mat);C_MTXIdentity(b.mat);b.mat[0][3]=10;C_MTXIdentity(a.lt_inv_mat);C_MTXIdentity(b.lt_inv_mat);a.pList=&b;
    ModelCpu m;m.pList=&a;ModelDataView d;d.source={raw,raw->data(),u32(raw->size())};d.nParts=2;d.nVtx=1;d.nNrm=1;d.weight_palette_num=1;d.vtxOrig=xyz;d.nrmOrig=raw->data()+72;d.flags=0x20000000;
    ModelInfoView info;info.model_addr=&d;
    (*raw)[80]=0;(*raw)[81]=1;(*raw)[83]=2;(*raw)[84]=50;(*raw)[85]=99;
    auto s=skinModelCpu(m,info);check(s.positions[0]==2);check(s.boneCount==2 && s.influenceCount==2 && !s.extended);check(s.palette[0][9]==5);
    (*raw)[83]=1;(*raw)[80]=1;(*raw)[84]=0;s=skinModelCpu(m,info);check(s.palette[0][9]==10); // last influence is 100%, regardless of stored percentage
    (*raw)[83]=0;fail([&]{skinModelCpu(m,info);});(*raw)[83]=4;fail([&]{skinModelCpu(m,info);});(*raw)[83]=1;
    (*raw)[80]=2;fail([&]{skinModelCpu(m,info);});(*raw)[80]=0;
    d.source.size=80;fail([&]{skinModelCpu(m,info);});d.source.size=u32(raw->size());
    info.shape[0].data=reinterpret_cast<ShapeData*>(std::uintptr_t(1));fail([&]{skinModelCpu(m,info);});info.shape[0].data=nullptr;
    C_MTXScale(a.mat,0,0,0);fail([&]{skinModelCpu(m,info);});C_MTXIdentity(a.mat);
    // Real extended selection rule is >255 entries, independent of bone count.
    raw->resize(80+256*12);d.source={raw,raw->data(),u32(raw->size())};d.nrmOrig=raw->data()+72;d.weight_ext_num=256;
    for(u32 i=0;i<256;++i){put16(80+i*12,1);put16(80+i*12+6,1);}
    s=skinModelCpu(m,info);check(s.extended && s.palette.size()==256 && s.influenceCount==256 && s.positions[0]==7);
    // All prefixes containing less than one complete extended table fail.
    for(u32 size=80;size<raw->size();size+=37){d.source.size=size;fail([&]{skinModelCpu(m,info);});}
    d.source.size=u32(raw->size());s=skinModelCpu(m,info);check(s.positions[0]==7);
    return checks;
}
}
