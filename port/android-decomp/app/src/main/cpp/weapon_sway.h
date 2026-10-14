#pragma once
#include <cstdint>
namespace re4 {
// CameraScope::move owns a separate 30-Hz breathing clock. These angular
// offsets belong to the camera, never to PlWepLockRand's muzzle centres.
struct ScopeSwayState {
  float phaseX{},phaseY{},speedX{},speedY{},gain{},targetGain{};
  float pitch{},yaw{};
  std::uint8_t countdown{};
};
void OriginalScopeSway(ScopeSwayState &state,float randomX,float randomY,float randomGain);
struct WeaponSwayState { float pitchCenter{},yawCenter{},amplitude{}; };
struct WeaponSwayAbility { float pitchRange{},yawRange{},pitchStep{},yawStep{}; };
// cObjWep::setAbility defaults used by actual FN57/Ruger/shotgun profiles.
WeaponSwayAbility OriginalDefaultWeaponSwayAbility();
// Literal PlWepLockRand arithmetic. Caller supplies the two source fRand1_1
// samples for idle/fresh paths; moved bit1 consumes no random samples.
void OriginalWeaponSway(WeaponSwayState &state,const WeaponSwayAbility &ability,
    unsigned flags,float randomPitch,float randomYaw,float &pitchRate,float &yaw);
} // namespace re4
