#include "player_eyes.h"
namespace re4 {
PlayerEyeAngles OriginalPlayerEyeUpdate(PlayerEyeState &s,
                                       const std::function<unsigned()> &random,
                                       float motionLidPitch) {
  const auto assign=[&](float value) {s.target=value;if(s.delay==0.f)s.direction=s.target;};
  const auto add=[&](float value) {s.target+=value;if(s.delay==0.f)s.direction=s.target;};
  // The switch deliberately preserves cases which do not overwrite the
  // current motion's eyelid, just as the original cParts angle does.
  PlayerEyeAngles result{motionLidPitch,0};
  switch(s.blinkTimer++) {
  default: result.lidPitch=0.f;break;
  case 0:
    assign((float(random()%200)*.01f-1.f)*3.1415927f*.1f);
    result.lidPitch=.0872664600610733f;break;
  case 1:result.lidPitch=.1745329201221466f;break;
  case 2:result.lidPitch=.3490658402442932f;break;
  case 3:result.lidPitch=.3141592741012573f;break;
  case 4:result.lidPitch=.24434609711170197f;break;
  case 5:result.lidPitch=.1745329201221466f;break;
  case 6:result.lidPitch=.0872664600610733f;break;
  case 0x1e:assign(0.f);break;
  case 0x58:s.blinkTimer=(random()&3)?0:0x5a;break;
  case 0x5a:result.lidPitch=.0872664600610733f;break;
  case 0x5b:result.lidPitch=.1745329201221466f;break;
  case 0x5c:result.lidPitch=.3490658402442932f;break;
  case 0x5d:result.lidPitch=.296705961227417f;break;
  case 0x5e:result.lidPitch=.33161255717277527f;break;
  case 0x5f:result.lidPitch=.3490658402442932f;break;
  case 0x60:result.lidPitch=.2617993950843811f;break;
  case 0x61:result.lidPitch=.1745329201221466f;break;
  case 0x62:result.lidPitch=.0872664600610733f;s.blinkTimer=10;break;
  }
  if(--s.wanderTimer<0) {
    add((float(random()%200)*.01f-1.f)*.03141592815518379f);
    s.wanderTimer=int(random()%3)+2;
  }
  if(s.target<-.3141592741012573f)s.target=-.3141592741012573f;
  else if(s.target>.3141592741012573f)s.target=.3141592741012573f;
  if(s.delay==0.f)s.direction=s.target;
  result.eyeYaw=s.direction;
  s.direction=s.direction*s.delay+s.target*(1.f-s.delay);
  return result;
}
} // namespace re4
