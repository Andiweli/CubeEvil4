#include "r101_scenario.h"
#include "asset_reader.h"
#include <algorithm>
#include <stdexcept>
namespace re4 {
R101Scenario::R101Scenario(const std::vector<std::uint8_t> &bytes) {
  AssetReader r{bytes};r.range(0,16);
  if(r.u32(0)!=0x41455600 || r.u16(4)!=0x104 || r.u16(6)>256)
    throw std::runtime_error("r101: original AEV is missing");
  r.range(16,std::size_t(r.u16(6))*156);
  for(unsigned i=0;i<r.u16(6);++i) {
    const auto p=16+std::size_t(i)*156;const auto no=bytes[p+0x36];
    if(no!=7 && no!=8 && no!=0x13 && no!=0x15 && no!=0x18 && no!=0x1a)continue;
    RoomDoor d;d.sourceRoom=0x101;d.no=no;d.enabled=bytes[p+0x34];
    d.hitType=bytes[p+0x37];d.triggerType=bytes[p+0x38];d.targetType=bytes[p+0x39];
    d.parentPointer=r.u32(p+0x4c);d.hitDirection=static_cast<std::int8_t>(bytes[p+0x48]);
    d.hitOpenAngle=static_cast<std::int8_t>(bytes[p+0x49]);
    d.area.enabled=bytes[p+4];d.area.type=bytes[p+5];d.area.floor=r.f32(p+8);
    d.area.height=r.f32(p+12);d.area.radius=r.f32(p+16);
    for(unsigned k=0;k<8;++k)d.area.points[k/2][k%2]=r.f32(p+20+k*4);
    std::copy_n(bytes.begin()+p,d.raw.size(),d.raw.begin());
    areas_.emplace(no,d);
  }
  for(unsigned no:{7u,8u,0x13u,0x15u,0x18u,0x1au})
    if(!areas_.count(no))throw std::runtime_error("r101: scenario area missing");
}
const std::vector<unsigned> &R101Scenario::actorIndices() {
  static const std::vector<unsigned> indices={0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1e,0x1f,
    0x22,0x23,0x24,0x28,0x29,0x2a,0x2b,0x2c,0x2e,0x2f,0x30,0x31,0x32,
    0x36,0x37,0x38,0x39,0x3a,0x3c,0x3d,0x3e,0x3f,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x48,0x49,0x4a};
  return indices;
}
std::vector<unsigned> R101Scenario::initialSpawns(const EnemySpawns &esl,ScenarioState &s,const std::set<unsigned> &dead) const {
  std::vector<unsigned> out;auto &r=s.room(0x101).room;
  const bool revisit=r.test(7) && s.scenario.test(10);
  const bool firstRevisit=revisit && !r.test(9);
  if(firstRevisit)r.set(9);
  for(auto index:actorIndices()) {
    const auto *e=esl.find(index);
    if(!e || e->room!=0x101 || e->id!=0x15 || !e->hp)
      throw std::runtime_error("r101: original EM15 actor missing");
    const bool post=(index>=0x14 && index<=0x19) || index==0x1e || index==0x1f || index>=0x48;
    if((!r.test(7) && e->alive() && !dead.count(index)) ||
        (revisit && post && (firstRevisit || !dead.count(index))))out.push_back(index);
  }
  return out;
}
void R101Scenario::resume(const ScenarioState &s,unsigned alive) {
  const auto &r=s.room(0x101).room;counting_=r.test(6)&&!r.test(7);
  baseline_=int(alive);countDelay_=counting_?10:0;
}
bool R101Scenario::hit(unsigned no,const CollisionVec &feet,double heading,const std::optional<CollisionVec> &front) const {
  return RoomFlow::hit(areas_.at(no),feet,heading,front);
}
void R101Scenario::spawn(unsigned first,unsigned last,bool count) {
  VillageCue cue{VillageCue::Spawn,{},0};
  for(unsigned i=first;i<=last;++i)cue.indices.push_back(i);
  if(count)baseline_+=int(cue.indices.size());
  cues_.push_back(std::move(cue));
}
void R101Scenario::alarm(ScenarioState &s,unsigned alive) {
  auto &r=s.room(0x101).room;if(r.test(6))return;
  r.set(6);s.scenario.set(36);s.scenario.set(150,false);s.scenario.set(154,false);
  baseline_=int(alive);spawn(0x22,0x24);
  counting_=true;countDelay_=10;cues_.push_back({VillageCue::Alarm,{},0});
}
void R101Scenario::startBell(ScenarioState &s) {
  s.room(0x101).room.set(7);s.scenario.set(36,false);
  s.scenario.set(150);s.scenario.set(154);s.transient.set(3);
  counting_=false;bellPending_=true;bellDelay_=60;
  cues_.push_back({VillageCue::StartBell,{},0});
}
void R101Scenario::tick(ScenarioState &s,const CollisionVec &feet,double heading,unsigned alive,bool found,bool paused,bool canStartEvent) {
  ++entryTicks_;auto &r=s.room(0x101).room;
  if(paused)return;
  if(bellPending_) {if(bellDelay_ && !--bellDelay_)pendingEvent_=VillageEvent::Bell;return;}
  if(r.test(7)) {
    // r101_FindPlayer2: a revisit fight ends when its last Ganado dies.
    // Before the first bell the stream covers the entire scripted fight,
    // including the short pauses before reset waves.
    if(s.scenario.test(10) && found && alive && !postBellCombat_) {
      postBellCombat_=true;cues_.push_back({VillageCue::CombatStart,{},0});
    }
    if(postBellCombat_ && !alive) {
      postBellCombat_=false;cues_.push_back({VillageCue::CombatStop,{},0});
    }
    return;
  }
  if(!r.test(6) && found)alarm(s,alive);
  if(hit(0x18,feet,heading)) {
    ++towerTicks_;if(towerTicks_>599 && towerTicks_%150==0)cues_.push_back({VillageCue::TowerBottle,{},0});
  } else towerTicks_=0;
  if(!counting_)return;
  if(countDelay_) {if(!--countDelay_)baseline_=int(alive);return;}
  if(wavePending_ && waveDelay_)--waveDelay_;
  if(wavePending_ && !waveDelay_ && canStartEvent) {
    const bool right=feet[0]>-3300;s.room(0x101).room.set(right?0:1);
    spawn(right?0x28:0x2e,right?0x2c:0x32,false);
    cues_.push_back({VillageCue::FirstWaveCamera,{},right?5u:4u});
    wavePending_=false;cameraPending_=true;return;
  }
  if(!r.test(3) && kills(alive)>4) {
    r.set(3);r.set(feet[0]>-3300?0:1);wavePending_=true;waveDelay_=90;return;
  }
  if(!r.test(2) && kills(alive)>9) {
    r.set(2);
    if(hit(0x1a,feet,heading))spawn(r.test(0)?0x2e:0x28,r.test(0)?0x32:0x2c);
    else spawn(0x36,0x3a);
    return;
  }
  if(kills(alive)>(r.test(8)?17:14) || battleTicks_>(r.test(8)?13500u:11700u)) {startBell(s);return;}
  ++battleTicks_;
}
std::optional<VillageEvent> R101Scenario::offer(const ScenarioState &s,const CollisionVec &feet,double heading,bool pressed,bool held,const std::optional<CollisionVec> &front) const {
  (void)held; // Original AEV7 trigger8 accepts A on its press edge.
  if(pendingEvent_)return pendingEvent_;
  const auto &r=s.room(0x101).room;
  if(r.test(7))return {};
  if(!r.test(8) && hit(8,feet,heading,front))return VillageEvent::House;
  if(pressed && binocularAvailable(s,feet,heading,front))return VillageEvent::Binoculars;
  return {};
}
bool R101Scenario::binocularAvailable(const ScenarioState &s,const CollisionVec &feet,double heading,const std::optional<CollisionVec> &front) const {
  const auto &r=s.room(0x101).room;
  return !r.test(6) && !r.test(7) && hit(7,feet,heading,front);
}
void R101Scenario::begin(VillageEvent event,ScenarioState &s,unsigned alive) {
  pendingEvent_.reset();
  if(event==VillageEvent::Binoculars)s.room(0x101).room.set(5);
  if(event==VillageEvent::House) {
    s.room(0x101).room.set(8);houseDifference_=kills(alive);
    cues_.push_back({VillageCue::RemoveGanados,{},0});
  }
  if(event==VillageEvent::Bell)cues_.push_back({VillageCue::RemoveGanados,{},0});
}
void R101Scenario::finish(VillageEvent event,ScenarioState &s) {
  if(event==VillageEvent::House) {
    baseline_=houseDifference_;spawn(0x3c,0x46);
    if(!s.room(0x101).room.test(6))alarm(s,11);
  }
  if(event==VillageEvent::Bell) {
    bellPending_=false;s.transient.set(3,false);s.transient.set(2);
    cues_.push_back({VillageCue::BellComplete,{},0});
  }
}
void R101Scenario::finishWaveCamera() {if(cameraPending_){baseline_+=5;cameraPending_=false;}}
std::vector<VillageCue> R101Scenario::takeCues() {auto out=std::move(cues_);cues_.clear();return out;}
} // namespace re4
