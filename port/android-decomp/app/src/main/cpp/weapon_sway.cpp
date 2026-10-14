#include "weapon_sway.h"
#include <cmath>
#include <initializer_list>
#include <stdexcept>
namespace re4 {
void OriginalScopeSway(ScopeSwayState &state,float randomX,float randomY,float randomGain) {
  // Frozen game/cam_extra.cpp::CameraScope::move, sct-- uses the old byte.
  constexpr float speed=.062831856f,range=.0005f,pi=3.1415927f;
  const auto old=state.countdown--;
  if(!old) {
    state.speedX=speed*(randomX*.5f+1.f);
    state.speedY=speed*(randomY*.5f+1.f);
    state.targetGain=randomGain*range;
    state.countdown=0x96;
  }
  state.gain=state.gain*.95f+state.targetGain*.05f;
  state.pitch+=std::cos(state.phaseX)*state.gain;
  state.yaw+=std::cos(state.phaseY)*(range-state.gain);
  state.phaseX=std::remainder(state.phaseX+state.speedX,2*pi);
  state.phaseY=std::remainder(state.phaseY+state.speedY,2*pi);
}
WeaponSwayAbility OriginalDefaultWeaponSwayAbility() {
  constexpr float degrees=.017453292f;
  return {5.73f*degrees,2.86f*degrees,.2864f*degrees,.2864f*degrees};
}
void OriginalWeaponSway(WeaponSwayState &state,const WeaponSwayAbility &ability,
    unsigned flags,float randomPitch,float randomYaw,float &pitchRate,float &yaw) {
  for(float value:{state.pitchCenter,state.yawCenter,state.amplitude,ability.pitchRange,
      ability.yawRange,ability.pitchStep,ability.yawStep,randomPitch,randomYaw,pitchRate,yaw})
    if(!std::isfinite(value))throw std::runtime_error("Leon: invalid original weapon sway");
  constexpr float pi=3.1415927f;
  pitchRate*=pi/2.f;
  if(flags&1) {state.pitchCenter=pitchRate;state.yawCenter=yaw;}
  else if(flags&2) {
    pitchRate=randomPitch*ability.pitchRange*state.amplitude+state.pitchCenter;
    yaw=randomYaw*ability.yawRange*state.amplitude+state.yawCenter;
  } else {
    pitchRate=ability.pitchStep*randomPitch+pitchRate;
    yaw=ability.yawStep*randomYaw+yaw;
    if(pitchRate>state.pitchCenter+ability.pitchRange)pitchRate=state.pitchCenter+ability.pitchRange;
    else if(pitchRate<state.pitchCenter-ability.pitchRange)pitchRate=state.pitchCenter-ability.pitchRange;
    // The source also uses rP for the idle yaw clamp; preserve that detail.
    if(yaw>state.yawCenter+ability.pitchRange)yaw=state.yawCenter+ability.pitchRange;
    else if(yaw<state.yawCenter-ability.pitchRange)yaw=state.yawCenter-ability.pitchRange;
  }
  pitchRate*=2.f/pi;
}
} // namespace re4
