#pragma once
#include "event_playback.h"
namespace re4 {
struct R100RoomMessage;
// GL-owner lifecycle, independent from ordinary room/player actor slots.
// Sync uploads new actors and immutable EST images; only changed facial
// meshes update their VBO. Lifecycle includes the separate event effect slot.
void SyncEventActorsGl(const OriginalEvent &event);
void CreateEventActorsGl(const OriginalEvent &event);
void DestroyEventActorsGl();
void ForgetEventActorsGl();
void DrawEventActorsGl(const float *camera,const std::array<float,3> &center,
                        float scale,const OriginalEvent &event,bool normalView,
                        unsigned firstPass=0,unsigned lastPass=20);
void DrawEventEffectModelsGl(const float *camera,const std::array<float,3> &center,
                        float scale,const OriginalEvent &event,bool normalView,
                        bool translucent = false);
void DrawEventSubtitlesGl(int width,int height,const OriginalEvent &event);
void DrawOriginalRoomMessageGl(int width,int height,const SubscreenFont &font,
                              const R100RoomMessage &message,bool fillScreen=false);
// Source FadeSetW/FadeControl black overlay, after the event's scene/effects.
void DrawEventFadeGl(int width,int height,unsigned alpha);
// Original world camera basis, including authored roll. Invalid/degenerate
// cameras fail explicitly rather than drawing with an invented event view.
FollowCameraPose EventCameraPose(const EventCamera &camera);
}
