#pragma once
#include "event_playback.h"
namespace re4 {
// r120.cpp::EvtTexRenderCamTrans and TexRenderInit(0x100,1).
struct EventMirrorLayout {
 unsigned width=592,height=512,copyX=40,copyY=0,copyWidth=512,copyHeight=512,
     textureWidth=256,textureHeight=256;
};
EventMirrorLayout OriginalEventMirrorLayout();
bool IsEventMirrorActive(const OriginalEvent &event);
// Current EVD camera, fixed source4:3 projection, independent of output aspect.
std::array<float,16> EventMirrorCameraMatrix(const EventCamera &camera,
    const std::array<float,3> &worldCenter,float worldScale);
void DrawEventMirrorGl(const float *mainCamera,
    const std::array<float,3> &worldCenter,float worldScale,
    const OriginalEvent &event,bool normalView);
void ForgetEventMirrorGl();
void DestroyEventMirrorGl();
}
