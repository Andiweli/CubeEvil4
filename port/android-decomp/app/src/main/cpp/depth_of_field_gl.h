#pragma once
namespace re4 {
inline bool userDepthOfField=false; // Optional gameplay autofocus; GL owner.
// Capture world focus before drawing Leon/weapon; composite keeps their own depth.
bool CaptureDepthOfFieldFocusGl(int width,int height,float nearPlane=100,float farPlane=1000000);
bool ApplyDepthOfFieldGl(int width,int height,float nearPlane=100,float farPlane=1000000);
void ForgetDepthOfFieldGl();
void DestroyDepthOfFieldGl();
}
