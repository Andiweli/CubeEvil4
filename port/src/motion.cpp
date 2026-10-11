#include "motion_original.h"
#include "re4/port/collision.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace re4::port {
namespace { thread_local f32 frameSpeed=1.f; thread_local const MotionActorContext *actorContext{}; }
f32 motionFrameSpeed() { return frameSpeed; }
int motionCrossedFrame(ModelCpu &m,f32 frame) {
    if(!std::isfinite(frame) || frame<0)throw std::invalid_argument("MotionCheckCrossFrame input");
    return MotionCheckCrossFrame(&m.Motion,frame);
}
const MotionActorContext &motionActorContext(ModelCpu *) {
    if(!actorContext)throw MissingGameConsumer("InverseKinematics original actor status/collision owner");
    return *actorContext;
}
f32 motionFloor(Vec *p,int normal,f32 above,f32 below,int mask) {
    if(normal || !actorContext || !actorContext->sat)throw MissingGameConsumer("InverseKinematics SAT owner");
    return actorContext->sat->getFloor(*p,above,below,mask);
}
u32 motionBlendCount(const u16 *p) { u32 count;std::memcpy(&count,p,4);return count; }
f32 motionLimitAngle(f32 a) {
    if(!std::isfinite(a))throw std::invalid_argument("motion angle");
    while(a>=3.1415927f)a-=6.2831855f;
    while(a< -3.1415927f)a+=6.2831855f;
    return a;
}
void requireNoActiveIk(ModelCpu *m,int) {
    for(auto *p=m->pList;p;p=p->pList)if((p->motParts.flags&4) && !(p->motParts.flags&0x80))
        throw MissingGameConsumer("InverseKinematics/ikCalc/SatMgr::getFloor actor collision state");
}
u32 moveModelMotion(ModelCpu &m,f32 speed,const MotionActorContext *actor) {
    if(!m.Motion.pMot || !m.Motion.Joint_num)throw std::invalid_argument("MotionMove requires nonempty original FCV");
    if(!std::isfinite(speed) || speed<0 || speed>16)throw std::invalid_argument("motion frame speed");
    if(m.pMotionB || m.Motion.Hokan_cnt || m.Motion.pAttachCam)
        throw MissingGameConsumer("MotionMove secondary/Hokan/attached camera");
    bool ik=false;
    for(auto *p=m.pList;p;p=p->pList)if((p->motParts.flags&4) && !(p->motParts.flags&0x80))ik=true;
    if(ik && (!actor || !actor->sat))throw MissingGameConsumer("MotionMove IK original actor status/SAT owner");
    if(m.scale.x==0 || m.scale.y==0 || m.scale.z==0)throw std::invalid_argument("MotionMove zero scale");
    const auto &w=m.Motion;
    if(!w.Seq_frame_num || !std::isfinite(w.Seq_frame) || !std::isfinite(w.Seq_speed))
        throw std::invalid_argument("MotionMove sequence state");
    // Original reverse playback and loops adjust once, not repeatedly. Reject an
    // invalid next index rather than normalizing it into a different animation.
    f32 next=w.Seq_frame;
    if(!(w.Mot_attr&8)) {
        if(w.Mot_attr&2) {
            if(next<=0)next=(w.Mot_attr&4)?next+w.Seq_frame_num:0.f;
            else next-=w.Seq_speed*speed;
        } else {
            next+=w.Seq_speed*speed;
            if(next>=w.Seq_frame_num)next=(w.Mot_attr&4)?next-w.Seq_frame_num:f32(w.Seq_frame_num-1);
        }
    }
    if(!std::isfinite(next) || next<0 || next>=w.Seq_frame_num)
        throw std::invalid_argument("MotionMove next sequence index out of bounds");
    const auto previous=frameSpeed;const auto previousActor=actorContext;frameSpeed=speed;actorContext=actor;
    try { auto result=MotionMove(&m,nullptr);frameSpeed=previous;actorContext=previousActor;return result; }
    catch(...) { frameSpeed=previous;actorContext=previousActor;throw; }
}
MotionSequenceView resolveMotionSequence(ResourceLease bytes) {
    // src/game/motion.cpp MotionSetCore and tools/motion/fcv.py parse_seq.
    if(!bytes.owner || !bytes.data || bytes.size<4 || bytes.data[3])
        throw std::invalid_argument("SEQ owner/header/padding");
    MotionSequenceView view;view.source=bytes;view.count=u16(u16(bytes.data[0])<<8|bytes.data[1]);view.flags=bytes.data[2];
    const auto end=std::size_t(4)+std::size_t(view.count)*4;
    if(!view.count || end>bytes.size || bytes.size-end>=32) throw std::invalid_argument("SEQ count/bounds");
    if(end<bytes.size) {
        auto fill=bytes.data[end];if(fill!=0 && fill!=0xcd)throw std::invalid_argument("SEQ padding byte");
        for(auto p=end;p<bytes.size;++p)if(bytes.data[p]!=fill)throw std::invalid_argument("SEQ mixed padding");
    }
    view.keys.reserve(view.count);
    for(u32 n=0;n<view.count;++n){auto p=4+n*4;view.keys.push_back({u16(u16(bytes.data[p])<<8|bytes.data[p+1]),bytes.data[p+2],bytes.data[p+3]});}
    return view;
}

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
