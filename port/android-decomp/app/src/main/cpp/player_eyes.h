#pragma once
#include <functional>
namespace re4 {
// Original moveEyeNormal statics belong to the single player; the native
// owner carries them across room reconstruction instead of sharing previews.
struct PlayerEyeState {
  float direction{},target{},delay{};
  int blinkTimer{},wanderTimer{};
};
struct PlayerEyeAngles { float lidPitch{},eyeYaw{}; };
PlayerEyeAngles OriginalPlayerEyeUpdate(PlayerEyeState &state,
                                       const std::function<unsigned()> &random,
                                       float motionLidPitch=0);
} // namespace re4
