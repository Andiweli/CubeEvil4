#pragma once
namespace re4 {
// Apply once after the final room/menu/UI draw. Default64 is a no-op so the
// existing framebuffer remains exact. Returns whether a pass was drawn.
bool ApplyDisplayBrightnessGl(int width,int height,int brightness);
// Forget after EGL loss; destroy only while its owning context is current.
void ForgetDisplayBrightnessGl();
void DestroyDisplayBrightnessGl();
} // namespace re4
