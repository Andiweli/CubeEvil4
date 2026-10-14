#pragma once
#include "event_playback.h"
#include <cstdint>
#include <vector>

namespace re4 {
// Original event FocusMove always requests Filter01 type1. Preserve its
// projected integer depth, four diagonal taps and optional recopy/fade tap.
struct EventFocusPass {
  float x{}, y{}, sourceDepth{}, renderDepth{};
  std::uint8_t alpha{};
  bool recopy{};
};
std::vector<EventFocusPass> EventFocusPasses(float level, float cameraDepth,
    float sceneNear = 100, float sceneFar = 200000);
std::vector<EventFocusPass> RoomFocusPasses(const RoomLightCut &cut,
    float sceneNear = 100, float sceneFar = 1000000);
// Called after the event's final world draw, before captions/fade/brightness.
// The existing scene depth is tested but never changed. Source logical EFB
// shifts are scaled to the fullscreen output; Filter01's copy stays half-size.
bool ApplyEventFocusGl(int width, int height, const EventFocusState &focus,
    float sceneNear = 100, float sceneFar = 200000,const RoomLightCut *fallback=nullptr);
void ForgetEventFocusGl();
void DestroyEventFocusGl();
} // namespace re4
