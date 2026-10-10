#pragma once
#include "re4/port/model_types.h"
#include <memory>
#include <vector>
namespace re4::port {
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
// Actual MotionMove chain for the supported forward-kinematic subset. Rejects
// active IK, secondary motion and Hokan before mutation.
// The original sequence advances by speed, independently of display cadence.
u32 moveModelMotion(ModelCpu &,f32 speed=1.f);
} // namespace re4::port
