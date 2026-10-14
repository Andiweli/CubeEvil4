#pragma once
#include "event_playback.h"
#include "input.h"
#include <algorithm>
#include <cmath>
namespace re4 {
// st1/r101::Event00 / CameraBinocular, English release target and 30-Hz inputs.
class VillageBinoculars {
 public:
  VillageBinoculars() {update();}
  void tick(float x,float y,float zoomY,unsigned held,bool reverse=false) {
    zoom_=std::clamp(zoom_+zoomY*127.f*.001f,0.f,1.f);
    const float gain=1.f-.9f*zoom_,unit=.05f*.017453292f;
    float yaw=gain*(-x*127.f*unit+((held&MenuLeft)? .062831856f:0)-((held&MenuRight)? .062831856f:0));
    float pitch=gain*(y*127.f*unit+((held&MenuUp)? .062831856f:0)-((held&MenuDown)? .062831856f:0));
    if(reverse)pitch=-pitch;
    yaw_=std::clamp(yaw_+yaw,-.2532f,.14943f);
    pitch_=std::clamp(pitch_+pitch,-.05992f,.2645f);
    ++ticks_;update();
  }
  const EventCamera &camera() const {return camera_;}
  float fov() const {return 45.f-42.f*zoom_;}
  float heading() const {return heading_;}
  unsigned ticks() const {return ticks_;}
 private:
  void update() {
    camera_.eye={-38027.f,1891.f,11114.f};
    const float dx=-3573.f-camera_.eye[0],dy=2400.f-camera_.eye[1],dz=2113.f-camera_.eye[2];
    const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
    heading_=std::atan2(dx,dz)+yaw_;
    const float elevation=std::atan2(dy,std::sqrt(dx*dx+dz*dz))-pitch_;
    camera_.target={camera_.eye[0]+distance*std::cos(elevation)*std::sin(heading_),
      camera_.eye[1]+distance*std::sin(elevation),camera_.eye[2]+distance*std::cos(elevation)*std::cos(heading_)};
    camera_.fovRadians=fov()*.017453292f;camera_.valid=true;
  }
  EventCamera camera_;
  float zoom_{},yaw_{},pitch_{},heading_{};
  unsigned ticks_{};
};
}
