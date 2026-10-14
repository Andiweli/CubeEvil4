#include "room_jumps.h"
#include <algorithm>
#include <cmath>
namespace re4 {
namespace {
constexpr double pi=3.14159265358979323846;
CollisionVec rotate(const CollisionVec &v, double h) {
  const auto c=std::cos(h), s=std::sin(h);
  return {c*v[0]+s*v[2],v[1],-s*v[0]+c*v[2]};
}
float turn(float current, float goal, float maximum) {
  return float(std::remainder(current+std::clamp(std::remainder(double(goal)-current,2*pi),-double(maximum),double(maximum)),2*pi));
}
}
JumpPlayback StartJumpPlacement(const CollisionVec &feet, float heading,
                                const JumpDownOffer &offer) {
  JumpPlayback state; state.feet=feet; state.heading=heading;
  state.base=offer.alignment; state.targetHeading=offer.heading; return state;
}
void AdvanceJumpPlacement(JumpPlayback &s, const RootMotionStep &root, float frame) {
  const auto displacement=rotate(root.translation,s.heading);
  if(s.adjustment) {
    for(unsigned k=0;k<3;++k) { s.base[k]+=displacement[k]; s.feet[k]+=(s.base[k]-s.feet[k])/s.adjustment; }
    s.targetHeading=float(std::remainder(double(s.targetHeading)+root.yaw,2*pi));
    if(frame<=20) s.heading=turn(s.heading,s.targetHeading,.31415927f);
    s.heading=turn(s.heading,s.targetHeading,float(pi/s.adjustment));
    --s.adjustment;
  } else {
    for(unsigned k=0;k<3;++k) s.feet[k]+=displacement[k];
    s.heading=float(std::remainder(double(s.heading)+root.yaw,2*pi));
    if(frame<=20 && s.phase==0) s.heading=turn(s.heading,s.targetHeading,.31415927f);
  }
  ++s.ticks;
}
std::optional<JumpDownOffer> FindJumpDown(const SatCollision &world,
    const WalkProbe &probe, float heading, const std::vector<JumpSample> &path,
    const CollisionVec &landingTravel) {
  if(!probe.ready() || !std::isfinite(heading) || path.empty()) return {};
  const auto feet=probe.position();
  auto from=feet; from[1]+=400;
  auto to=from; to[0]+=std::sin(heading)*1000; to[2]+=std::cos(heading)*1000;
  const auto first=world.line(from,to,SatSubset::All,0); if(!first) return {};
  for(unsigned k=0;k<3;++k) to[k]=from[k]-first->normal[k]*1000;
  const auto wall=world.line(from,to,SatSubset::All,0); if(!wall) return {};
  auto attributes=wall->attributes;
  const auto h=float(std::atan2(-wall->normal[0],-wall->normal[2]));
  for(double x : {-350.,350.}) {
    const auto side=rotate({x,300,0},h);
    for(unsigned k=0;k<3;++k) { from[k]=feet[k]+side[k]; to[k]=from[k]-wall->normal[k]*1000; }
    const auto hit=world.line(from,to,SatSubset::All,0);
    if(!hit) return {};
    attributes&=hit->attributes;
  }
  if(!(attributes&0x100010)) return {};
  JumpDownOffer offer; offer.wall=*wall; offer.heading=h; offer.alignment=wall->point;
  for(unsigned k=0;k<3;++k) offer.alignment[k]+=wall->normal[k]*400;
  offer.alignment[1]=feet[1];
  auto state=StartJumpPlacement(feet,heading,offer);
  float previousPelvis=path.front().pelvisY;
  for(const auto &sample : path) {
    auto oldPelvis=state.feet; oldPelvis[1]+=previousPelvis;
    AdvanceJumpPlacement(state,sample.root,sample.frame);
    previousPelvis=sample.pelvisY;
    if(sample.frame<=20) continue;
    oldPelvis[0]=state.feet[0]; oldPelvis[2]=state.feet[2];
    const auto floor=world.line(oldPelvis,state.feet,SatSubset::Ground,0);
    if(!floor) continue;
    // Adapter excludes same-height step actions and unbounded empty drops.
    if(floor->point[1]>feet[1]-600 || feet[1]-floor->point[1]>50000 || floor->normal[1]<.5) return {};
    auto landing=state.feet; landing[1]=floor->point[1];
    auto checked=probe;
    if(!checked.place(landing,5)) return {};
    const auto tail=rotate(landingTravel,state.heading);
    for(unsigned k=0;k<3;++k) landing[k]+=tail[k];
    // getFloor checks the original 600-above/100000-below interval; the
    // normal grounded resolver takes over only after the landing motion.
    const auto end=world.ground(landing,600,100000);
    if(!end) return {};
    landing[1]=end->point[1];
    if(!checked.place(landing,5)) return {};
    offer.landing=checked.position(); return offer;
  }
  return {};
}
std::optional<JumpFallOffer> FindJumpFall(const SatCollision &world,
    const WalkProbe &probe, float heading, const std::vector<JumpSample> &path,
    const CollisionVec &landingTravel) {
  if (!probe.ready() || !std::isfinite(heading) || path.empty()) return {};
  const auto start=probe.position();
  std::optional<SatHit> wall;
  std::uint32_t attributes=~0u;
  for (double side : {300.,-300.}) {
    const auto a=rotate({side,400,0},heading), b=rotate({side,400,600},heading);
    CollisionVec from{},to{};
    for(unsigned k=0;k<3;++k) { from[k]=start[k]+a[k]; to[k]=start[k]+b[k]; }
    wall=world.line(from,to,SatSubset::All,0);
    if(!wall) return {};
    attributes&=wall->attributes;
  }
  if(!(attributes&0x80000)) return {};
  JumpFallOffer offer; offer.wall=*wall;
  offer.direction={-wall->normal[0],wall->normal[1],-wall->normal[2]};
  offer.heading=float(std::atan2(offer.direction[0],offer.direction[2]));
  auto beyond=wall->point;
  for(unsigned k=0;k<3;++k) beyond[k]+=offer.direction[k]*3800;
  beyond[1]+=1500;
  const auto floor=world.ground(beyond,600,100000);
  if(!floor) return {};
  offer.heightAdjustment=float(floor->point[1]-start[1]);
  if(std::abs(offer.heightAdjustment)>1500) return {}; // r226 exception not installed.
  auto feet=start;
  auto playbackHeading=offer.heading;
  unsigned work=0;
  for(const auto &sample:path) {
    const auto oldPelvisY=feet[1]+sample.pelvisY;
    ++work;
    if(work>=5 && work<=14) feet[1]+=offer.heightAdjustment*.1f;
    // fallCheck runs before motionMove and uses last tick's part4 world Y.
    if(sample.frame>=7) {
      if(const auto contact=world.line({feet[0],oldPelvisY,feet[2]},feet,SatSubset::All,0)) {
        feet[1]=contact->point[1];
        const auto tail=rotate(landingTravel,playbackHeading);
        for(unsigned k=0;k<3;++k) feet[k]+=tail[k];
        const auto landing=probe.traversalLanding(feet);
        if(!landing) return {};
        offer.landing=*landing;
        return offer;
      }
    }
    const auto delta=rotate(sample.root.translation,playbackHeading);
    for(unsigned k=0;k<3;++k) feet[k]+=delta[k];
    playbackHeading=float(std::remainder(double(playbackHeading)+sample.root.yaw,2*pi));
  }
  return {};
}
FencePlayback StartFencePlacement(const CollisionVec &feet, float heading,
                                  const FenceOffer &offer) {
  FencePlayback state;
  state.feet = feet; state.heading = heading;
  state.base = offer.alignment; state.targetHeading = offer.heading;
  state.adjustSides = offer.adjustSides;
  return state;
}
void AdvanceFencePlacement(const SatCollision &world, FencePlayback &state,
                           const RootMotionStep &root, float frame) {
  // pl_R1_Fance calls fanceAdjust before MotionMove, through frame24.
  if (state.adjustSides && frame <= 24) {
    for (const auto &[x, shift] : std::array<std::pair<double,double>,2>{{{300,-50},{-700,50}}}) {
      const auto side = rotate({x,400,0}, state.heading);
      const auto ahead = rotate({x,400,1000}, state.heading);
      CollisionVec a{}, b{};
      for (unsigned k=0;k<3;++k) { a[k]=state.feet[k]+side[k]; b[k]=state.feet[k]+ahead[k]; }
      const auto hit = world.line(a,b,SatSubset::All,0);
      if (hit && !(hit->attributes & 0x20)) {
        const auto delta = rotate({shift,0,0},state.heading);
        for (unsigned k=0;k<3;++k) state.feet[k]+=delta[k];
        break;
      }
    }
  }
  const auto displacement = rotate(root.translation,state.heading);
  if (state.adjustment) {
    for (unsigned k=0;k<3;++k) {
      state.base[k]+=displacement[k];
      state.feet[k]+=(state.base[k]-state.feet[k])/state.adjustment;
    }
    state.targetHeading=float(std::remainder(double(state.targetHeading)+root.yaw,2*pi));
    state.heading=turn(state.heading,state.targetHeading,float(pi/state.adjustment));
    --state.adjustment;
  } else {
    for (unsigned k=0;k<3;++k) state.feet[k]+=displacement[k];
    state.heading=float(std::remainder(double(state.heading)+root.yaw,2*pi));
  }
  ++state.ticks;
}
std::optional<FenceOffer> FindFence(const SatCollision &world,
    const WalkProbe &probe, float heading, const CollisionVec &parts0World,
    const std::array<std::vector<FenceSample>, 2> &paths) {
  if (!probe.ready() || !std::isfinite(heading)) return {};
  const auto feet=probe.position();
  auto from=feet; from[1]+=400;
  auto to=from; to[0]+=std::sin(heading)*1000; to[2]+=std::cos(heading)*1000;
  const auto first=world.line(from,to,SatSubset::All,0); if (!first) return {};
  for (unsigned k=0;k<3;++k) to[k]=from[k]-first->normal[k]*1000;
  const auto wall=world.line(from,to,SatSubset::All,0); if (!wall) return {};
  auto attributes=wall->attributes;
  const auto h=float(std::atan2(-wall->normal[0],-wall->normal[2]));
  for (double x : {-350.,350.}) {
    const auto side=rotate({x,300,0},h);
    for (unsigned k=0;k<3;++k) { from[k]=feet[k]+side[k]; to[k]=from[k]-wall->normal[k]*1000; }
    const auto hit=world.line(from,to,SatSubset::All,0); if (!hit) return {};
    attributes&=hit->attributes;
  }
  if (!(attributes&0x20)) return {};
  CollisionVec a{}, b{};
  const auto right=rotate({400,300,0},h), left=rotate({-400,300,0},h);
  for (unsigned k=0;k<3;++k) {
    a[k]=feet[k]+right[k]-wall->normal[k]*1000;
    b[k]=feet[k]+left[k]-wall->normal[k]*1000;
  }
  if (world.line(a,b,SatSubset::All,0) || world.line(b,a,SatSubset::All,0)) return {};
  auto beyond=wall->point;
  for (unsigned k=0;k<3;++k) beyond[k]-=wall->normal[k]*1500;
  const auto floor=world.ground(beyond,0,10000);
  if (!floor || floor->normal[1]<.5 || std::abs(floor->point[1]-feet[1])>300) return {};
  const auto wa=rotate({-1200,500,-500},h), wb=rotate({-1200,500,500},h);
  for (unsigned k=0;k<3;++k) { a[k]=feet[k]+wa[k]; b[k]=feet[k]+wb[k]; }
  const auto width=world.line(a,b,SatSubset::All,0);
  const bool wide=(!width || !(width->attributes&~0x20u)) &&
                   !world.line(parts0World,a,SatSubset::All,0);
  const auto &path=paths[wide?0:1]; if (path.empty()) return {};
  FenceOffer offer; offer.wall=*wall; offer.heading=h; offer.wide=wide;
  offer.alignment=wall->point;
  for (unsigned k=0;k<3;++k) offer.alignment[k]+=wall->normal[k]*400;
  // getHeighAdjust uses the actual floor 1000 behind the action wall.
  auto heightProbe=wall->point;
  for (unsigned k=0;k<3;++k) heightProbe[k]-=wall->normal[k]*1000;
  const auto heightFloor=world.ground(heightProbe,600,100000); if (!heightFloor) return {};
  offer.alignment[1]=heightFloor->point[1];
  auto playback=StartFencePlacement(feet,heading,offer);
  for (const auto &sample:path) AdvanceFencePlacement(world,playback,sample.root,sample.frame);
  const auto endFloor=world.ground(playback.feet,600,100000);
  if (!endFloor || endFloor->normal[1]<.5 || std::abs(endFloor->point[1]-playback.feet[1])>600) return {};
  playback.feet[1]=endFloor->point[1];
  auto checked=probe;
  if (!checked.place(playback.feet,5)) return {};
  offer.landing=checked.position();
  return offer;
}
} // namespace re4
