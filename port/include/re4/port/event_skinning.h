#pragma once
#include "re4/port/skinning.h"
#include "re4/port/consumers.h"
namespace re4::port {
struct EventSkinModel {
    std::string name,parent,bin,tpl,motion,poseStatus;
    ResourceLease texture;
    SkinningResult skin;
    Mtx world{},rootMatrix{},infoMatrix{};
    Vec position{},angle{},scale{1,1,1},rootWorld{},boundOffset{},boundSize{};
    std::vector<std::array<float,12>> bones,bind;
    std::vector<Vec> localPosition,localAngle,localScale;
    u32 motionFrame{};
    u8 cullMode{},otType{};
};
struct EventSkinSnapshot {
    std::vector<EventSkinModel> models;
    std::vector<std::string> dependencies;
    std::string eventHash;
    u32 cut{},frame{};
    std::string report() const;
};
EventSkinModel captureSkinnedModel(ModelCpu &,ModelInfoView &);
// Isolated original Event::SetMod/SetPartsSub, MotionSetCore/MotionMove and
// skinning execution. Does not execute SetObj18 cloth or the event player.
// Requires no existing model/event borrowers and retires the diagnostic domain.
EventSkinSnapshot evaluateEventSkinning(EngineLoader &,EngineConsumers &,
                                       const std::string &eventPath,u32 cut,u32 frame);
} // namespace re4::port
