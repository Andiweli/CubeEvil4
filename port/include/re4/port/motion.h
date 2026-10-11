#pragma once
#include "re4/port/model_types.h"
#include <memory>
#include <vector>
namespace re4::port {
class CollisionRuntime;
struct MotionActorContext {
    CollisionRuntime *sat{};
    bool ikOff{};
    u32 atariFlags{};
};
struct MotionDataView {
    u16 maxFrame{};
    u8 nParts{};
    std::vector<u16> kinds;
    std::vector<u8> numbers;
    std::vector<const u8*> streams;
};
struct MotionSequenceView {
    ResourceLease source;
    u16 count{};
    u8 flags{};
    std::vector<SequenceKey> keys;
};
MotionSequenceView resolveMotionSequence(ResourceLease);
// Keeps original FCV bytes alive, checks every axis and model reference before execution.
class MotionAsset final {
public:
    MotionAsset(ResourceLease, const ModelCpu &);
    Vec sample(u32 joint,f32 frame) const;
    MotionDataView data;
    ResourceLease source;
};
void MotionSetCore(ModelCpu *,void *,void *,void *,int,int,int);
void MotionClear(ModelCpu *,int);
void MotionHokan(ModelCpu *,ModelMotion *);
// Original cMot3 algorithm on typed native models. EngineConsumers owns this
// work and all three FCV leases. Never copy/move an attached work object.
class MotionBlend3 final {
    ModelCpu *m_pEm{};
    f32 m_Rate{};
    int m_Mode{};
    void set0(void *,u8,int);
public:
    void *mot0{},*mot1{},*mot2{};
    ModelMotion work{};
    MotionBlend3();
    ~MotionBlend3();
    MotionBlend3(const MotionBlend3&)=delete;
    MotionBlend3& operator=(const MotionBlend3&)=delete;
    void set(ModelCpu *,void *,void *,void *,void *,u8,int,u16,u16);
    void move(f32);
};
// Actual MotionMove chain. Active IK requires the caller's typed original actor
// status/collision view; attached cameras still fail before mutation.
// The original sequence advances by speed, independently of display cadence.
u32 moveModelMotion(ModelCpu &,f32 speed=1.f,const MotionActorContext *actor=nullptr);
int motionCrossedFrame(ModelCpu &,f32 frame);
} // namespace re4::port
