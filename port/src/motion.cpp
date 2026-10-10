#include "motion_original.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace re4::port {
u16 motionBE16(const u8 *p) { return u16((u16(p[0])<<8)|p[1]); }
f32 motionBEFloat(const u8 *p) {
    u32 value=(u32(p[0])<<24)|(u32(p[1])<<16)|(u32(p[2])<<8)|p[3]; f32 f;
    std::memcpy(&f,&value,4); return f;
}
f32 motionBES16(const u8 *p) { return f32(static_cast<s16>(motionBE16(p)))*0.0001f; }
MotionAsset::MotionAsset(ResourceLease bytes,const ModelCpu &model) : source(bytes) {
    auto bound=[&](std::size_t at,std::size_t count) {
        if(!bytes.owner || !bytes.data || at>bytes.size || count>bytes.size-at)
            throw std::invalid_argument("FCV bounds");
    };
    auto word=[&](std::size_t at) { bound(at,4); auto *p=bytes.data+at;
        return (u32(p[0])<<24)|(u32(p[1])<<16)|(u32(p[2])<<8)|p[3]; };
    bound(0,3); data.maxFrame=motionBE16(bytes.data); data.nParts=bytes.data[2];
    const auto n=data.nParts; auto table=(std::size_t(3)+3*n+3)&~std::size_t(3);
    bound(3,std::size_t(n)*3); bound(table,4+std::size_t(n)*4);
    auto size=word(table);
    if(size && size>bytes.size) throw std::invalid_argument("FCV size word");
    const auto end=size?size:bytes.size;
    constexpr int strides[16]={12,8,6,-1,10,6,4,-1,9,5,3,-1,-1,-1,-1,4};
    for(u32 i=0;i<n;++i) {
        auto kind=motionBE16(bytes.data+3+i*2); auto number=bytes.data[3+n*2+i];
        auto fmt=kind>>12; auto k=kind&255; auto channel=(kind>>8)&15;
        if(strides[fmt]<0) throw std::invalid_argument("unsupported original FCC type");
        if(k!=1 && k!=0x40 && channel!=6 && channel!=7 && number>=model.nParts)
            throw std::invalid_argument("FCV references missing model part");
        if(k&0x30) {
            auto extra=(k&0x20)?3u:2u;
            if(u32(number)+extra>=model.nParts || channel>5)
                throw std::invalid_argument("FCV IK chain bounds/axis");
        }
        auto at=std::size_t(word(table+4+i*4));
        if(at<table+4+n*4 || at>=end) throw std::invalid_argument("FCV stream offset");
        auto first=at;
        for(int axis=0;axis<3;++axis) {
            bound(at,2); auto count=motionBE16(bytes.data+at);
            if(!count) throw std::invalid_argument("FCV axis without keys");
            if(fmt==15 && count>1) throw std::invalid_argument("original FCC15 interpolation lacks tangent semantics");
            auto axisSize=std::size_t(2)+count*(std::size_t(2)+std::size_t(strides[fmt]));
            bound(at,axisSize); if(axisSize>end-at) throw std::invalid_argument("FCV axis exceeds size word");
            u16 previous=0;
            for(u32 key=0;key<count;++key) {
                auto frame=motionBE16(bytes.data+at+2+key*2);
                if(key && frame<=previous) throw std::invalid_argument("FCV unordered key frames");
                previous=frame;
                auto p=bytes.data+at+2+count*2+key*strides[fmt];
                if((fmt<3 || fmt==15) && !std::isfinite(motionBEFloat(p))) throw std::invalid_argument("FCV nonfinite value");
                if(fmt==0 || fmt==4 || fmt==8) {
                    auto vsize=fmt==0?4:fmt==4?2:1;
                    if(!std::isfinite(motionBEFloat(p+vsize)) || !std::isfinite(motionBEFloat(p+vsize+4)))
                        throw std::invalid_argument("FCV nonfinite tangent");
                }
            }
            at+=axisSize;
        }
        data.kinds.push_back(kind); data.numbers.push_back(number); data.streams.push_back(bytes.data+first);
    }
}
Vec MotionAsset::sample(u32 joint,f32 frame) const {
    if(joint>=data.nParts || !std::isfinite(frame) || frame<0 || frame>data.maxFrame)
        throw std::invalid_argument("motion sample range");
    HERMITE_SET p{}; p.Frame=frame; p.Frame_max=data.maxFrame; p.Data_fmt=u8(data.kinds[joint]>>12);
    p.pData=const_cast<u8*>(data.streams[joint]);
    Vec result{}; u16 history[3]{}; HermiteInterpolation(&p,&result,history); return result;
}
} // namespace re4::port
