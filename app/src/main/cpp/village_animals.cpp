#include "village_animals.h"
#include "archive.h"
#include "enemy_feedback.h"
#include "ports/rnd.h"
#include "turn_rules.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace re4 {
namespace {
constexpr float pi=3.1415927f;
struct RandomOwner {
  std::uint16_t &owner;std::uint16_t previous;
  explicit RandomOwner(std::uint16_t &state):owner(state),previous(RndState()){RndInit(owner);}
  ~RandomOwner(){owner=RndState();RndInit(previous);}
};
std::vector<std::uint8_t> asset(const RoomArchive &arc,unsigned index,const char *tag) {
  if(index<4 || index-4>=arc.entries().size() || !std::equal(tag,tag+3,arc.entries()[index-4].tag.begin()))
    throw std::runtime_error("r101 animals: missing authored ARC asset");
  return arc.copy(index-4);
}
AnimationBytes clip(const RoomArchive &arc,unsigned motion,unsigned sequence=0) {
  return {asset(arc,motion,"FCV"),sequence?asset(arc,sequence,"SEQ"):std::vector<std::uint8_t>{},false};
}
float turn(float from,float to,float amount) {
  const float delta=GameCubeLimitAngle(to-from);
  return GameCubeLimitAngle(from+std::clamp(delta,-amount,amount));
}
std::vector<AuthoredHitShape> shapes(unsigned id) {
  if(id==0x26)return {{{0,-150,-150},500,1200,0,HitOn|HitZAxis,2},
      {{0,-50,-100},300,350,0,HitOn|HitZAxis,5},{{0,0,-200},100,200,0,HitOn|HitZAxis,24}};
  return {{{0,0,-130},200,100,0,HitOn|HitZAxis,3}};
}
CollisionVec part(const VillageAnimal &a,unsigned index) {
  const auto &m=a.model().rig()->jointMatrices().at(index);
  const auto p=TransformPoint(a.model().worldMatrix(),{m[3],m[7],m[11]});return {p[0],p[1],p[2]};
}
float distanceXZ(const CollisionVec &a,const CollisionVec &b) {
  const float x=float(a[0])-float(b[0]),z=float(a[2])-float(b[2]);return x*x+z*z;
}
}
VillageAnimals::VillageAnimals(const SatCollision &sat,const EnemySpawns &spawns,const std::string &dir, std::uint16_t room):sat_(sat) {
  if(room!=0x101 && room!=0x103)throw std::runtime_error("EM26/28: room profile is not audited");
  const auto cow=RoomArchive::LoadDrs(dir+"/em26.drs"),hen=RoomArchive::LoadDrs(dir+"/em28.drs");
  RandomOwner random(randomState_);
  for(const auto &source:spawns.records()) {
    if(source.room!=room || (source.id!=0x26 && source.id!=0x28) || !source.alive() || source.alreadySet())continue;
    if(source.setNo || source.type>1 || !source.hp)throw std::runtime_error("r101 animals: unsupported ESL state");
    auto a=std::make_unique<VillageAnimal>();a->source_=source;a->hp_=source.hp;
    for(unsigned k=0;k<3;++k)a->position_[k]=float(source.position[k]);
    const auto raw=unsigned(source.raw[20])<<8|source.raw[21];
    a->heading_=float(raw<0x8000?int(raw):int(raw)-0x10000)*(pi/16384.f);
    const auto &arc=source.id==0x26?cow:hen;
    a->model_=std::make_unique<GenericEventActor>("r101 animal "+std::to_string(source.listIndex),
        "body.bin",asset(arc,4,"BIN"),asset(arc,source.type?6:5,"TPL"));
    a->model_->prepareStaticPose();auto &rig=*a->model_->rig();
    if(source.id==0x26) {
      std::vector<AnimationBytes> clips;
      for(unsigned i=8;i<=0x10;++i)clips.push_back(clip(cow,i,i==0xd?0x11+unsigned(source.listIndex)%5:i==0xe?0x16:i==0xf?0x17:0));
      rig.appendActions(clips);
      std::vector<std::uint16_t> flip{0,1,2,3,4,5,10,11,12,13,6,7,8,9,14,19,20,21,22,15,16,17,18,23,24,25,26,27,28,29,30,0};
      if(rig.jointMatrices().size()>flip.size())throw std::runtime_error("EM26: unsupported skeleton");
      flip.resize(rig.jointMatrices().size());rig.setFlipTable(flip);
      (void)Rnd();a->breathTimer_=Rnd()%60+30;(void)Rnd();start(*a,0,0,5);
    } else {
      std::vector<AnimationBytes> clips;
      for(unsigned i=0xc;i<=0x1a;++i)clips.push_back(clip(hen,i,i==0x10?0x1b:i>=0x12&&i<=0x14?0x1c+i-0x12:0));
      rig.appendActions(clips);
      std::vector<std::uint16_t> flip{0,1,2,3,4,8,9,10,5,6,7,11,15,16,17,12,13,14,18,19,20,21,22,15,16,17,18,23,24,25,26,27,28,29,30,0};
      if(rig.jointMatrices().size()>flip.size())throw std::runtime_error("EM28: unsupported skeleton");
      flip.resize(rig.jointMatrices().size());rig.setFlipTable(flip);
      start(*a,0,0,5);
    }
    rig.advanceAction(0);place(*a);a->model_->setDisplayOverride(true);animals_.push_back(std::move(a));
  }
  const unsigned cows=unsigned(std::count_if(animals_.begin(),animals_.end(),[](const auto &a){return a->source_.id==0x26;}));
  if(animals_.size()!=(room==0x101?3:12) || cows!=(room==0x101?1:6))
    throw std::runtime_error("r101 animals: original one cow/two chicken records missing");
}
void VillageAnimals::place(VillageAnimal &a) {
  SmdPlacement p;p.position=a.position_;p.rotation={0,a.heading_,0};p.scale={1,1,1};a.model_->setPlacement(p);
}
void VillageAnimals::start(VillageAnimal &a,unsigned c,unsigned blend,unsigned flags) {
  a.model_->rig()->startActionFlags(c,blend,0,std::uint16_t(flags));
}
bool VillageAnimals::motion(VillageAnimal &a,bool ground) {
  const auto previous=a.position_;const auto root=a.model_->rig()->advanceRootAction();
  const float c=std::cos(a.heading_),s=std::sin(a.heading_);
  a.position_[0]+=c*float(root.translation[0])+s*float(root.translation[2]);
  a.position_[1]+=float(root.translation[1]);a.position_[2]+=-s*float(root.translation[0])+c*float(root.translation[2]);
  a.heading_=GameCubeLimitAngle(a.heading_+float(root.yaw));
  if(ground) {
    const float radius=a.source_.id==0x26?750:200;
    const auto resolved=sat_.resolveScenerySphere({previous[0],previous[1]+radius,previous[2]},
        {a.position_[0],a.position_[1]+radius,a.position_[2]},radius,0x1001);
    a.position_[0]=float(resolved.position[0]);a.position_[2]=float(resolved.position[2]);
    if(const auto floor=sat_.ground({a.position_[0],previous[1],a.position_[2]},600,100000)) {
      if(a.source_.id==0x26 || floor->point[1]>=previous[1]-250)a.position_[1]=float(floor->point[1]);
      else if(a.hp_ && a.routine_!=VillageAnimalRoutine::Jump){a.routine_=VillageAnimalRoutine::Jump;a.phase_=0;}
    }
    const float wanted=std::hypot(float(root.translation[0]),float(root.translation[2]));
    const float moved=std::hypot(a.position_[0]-previous[0],a.position_[2]-previous[2]);
    a.stuck_=wanted>.01f && moved<wanted*.5f?a.stuck_+1:0;
  }
  place(a);return a.model_->rig()->status().state!=0;
}
bool VillageAnimals::air(VillageAnimal &a) {
  const auto old=a.feet();const float c=std::cos(a.heading_),s=std::sin(a.heading_);
  a.speed_[1]-=15;a.position_[0]+=c*a.speed_[0]+s*a.speed_[2];
  a.position_[1]+=a.speed_[1];a.position_[2]+=-s*a.speed_[0]+c*a.speed_[2];
  if(a.speed_[1]<0)if(const auto floor=sat_.ground({a.position_[0],old[1],a.position_[2]},600,100000))
    if(a.position_[1]<floor->point[1]) {a.position_[1]=float(floor->point[1]);a.speed_[1]=0;a.airborne_=false;place(a);return true;}
  const auto resolved=sat_.resolveScenerySphere({old[0],old[1]+200,old[2]},
      {a.position_[0],a.position_[1]+200,a.position_[2]},200,0x1001);
  a.position_[0]=float(resolved.position[0]);a.position_[2]=float(resolved.position[2]);
  if(a.position_[1]<-100000) {a.position_[1]=-100000;a.airborne_=false;place(a);return true;}
  place(a);return false;
}
void VillageAnimals::sound(const VillageAnimal &a,unsigned cue) {
  if(sounds_.size()==128)sounds_.erase(sounds_.begin());
  sounds_.push_back({unsigned(a.source_.listIndex),cue,a.source_.id,a.feet()});
}
void VillageAnimals::drop(VillageAnimal &a) {
  if(a.dropEmitted_)return;
  a.dropEmitted_=true;std::optional<EnemyDropChoice> item;
  auto &state=dropState_?*dropState_:localDropState_;
  if(a.authoredDrop_) {
    if(scenario_ && ClaimAuthoredEnemyDrop(*a.authoredDrop_,*scenario_))item=ResolveAuthoredEnemyDrop(*a.authoredDrop_,dropContext_,state);
  } else item=OriginalEnemyRandomDrop(a.source_.id,dropContext_,state);
  if(item)drops_.push_back({unsigned(a.source_.listIndex),item->id,item->count,
      a.authoredDrop_&&a.authoredDrop_->fixedPosition?a.authoredDrop_->position:a.feet(),
      a.authoredDrop_?int(a.authoredDrop_->effect):-1});
}
void VillageAnimals::die(VillageAnimal &a) {
  a.hp_=0;a.phase_=0;
  if(a.source_.id==0x26) {a.routine_=VillageAnimalRoutine::Die;start(a,5);sound(a,8);drop(a);}
  else if(a.airborne_) {a.routine_=VillageAnimalRoutine::DieAir;start(a,9,3);}
  else {
    a.routine_=VillageAnimalRoutine::Die;const unsigned variant=Rnd()%3;
    if(variant==1 && a.pendingDamage_)a.heading_=std::atan2(float(a.pendingDamage_->from[0])-a.position_[0],float(a.pendingDamage_->from[2])-a.position_[2]);
    start(a,variant==0?5:variant==1?11:14,3);a.timer_=14;
  }
}
void VillageAnimals::damage(VillageAnimal &a) {
  if(a.pendingBlast_) {
    const auto weapon=*a.pendingBlast_;a.pendingBlast_.reset();
    if(weapon==23) {
      if(a.source_.id==0x28 && !a.airborne_){a.routine_=(Rnd()&1)?VillageAnimalRoutine::Jump:VillageAnimalRoutine::Dash;a.phase_=0;}
    }else {a.burnFade_=true;die(a);a.pendingDamage_.reset();return;}
  }
  if(!a.pendingDamage_)return;
  const auto hit=*a.pendingDamage_;a.pendingDamage_.reset();
  if(a.source_.id==0x28) {
    if(hit.weapon==22 || hit.weapon==23 || hit.weapon==42 || hit.weapon==14) {
      if(hit.weapon==14)a.hp_=1;
      if(!a.airborne_){a.routine_=(Rnd()&1)?VillageAnimalRoutine::Jump:VillageAnimalRoutine::Dash;a.phase_=0;}
    } else {sound(a,0);a.pendingDamage_=hit;die(a);a.pendingDamage_.reset();}
    return;
  }
  if(hit.weapon==20 || hit.weapon==22 || hit.weapon==23 || hit.weapon==42 || hit.weapon==14)return;
  const unsigned amount=hit.weapon<=0x2d?GanadoFirearmBaseDamage(hit.weapon,hit.power,hit.hit.lenSq<36000000.f):100;
  (void)Rnd();(void)Rnd();a.hp_-=std::min(a.hp_,amount);a.damageTotal_+=amount;
  a.damageWait_=hit.weapon==16?17:1;sound(a,0xa);
  if(!a.hp_){die(a);return;}
  const unsigned zone=hit.hit.partIndex;
  if(zone==1 || zone==2 || Rnd()%5==0) {
    a.routine_=VillageAnimalRoutine::Hurt;a.phase_=0;sound(a,8);
    // Source kind==0 selects ARC A; head kind==1 selects B/10.
    start(a,zone==2?4:zone==1?((Rnd()&1)?3:8):2);
  }
}
void VillageAnimals::tickCow(VillageAnimal &a,const CollisionVec &player,LeonPreview *leon) {
  using R=VillageAnimalRoutine;
  if(a.routine_==R::Die) {if(a.phase_==0 && motion(a))a.phase_=1;return;}
  if(a.routine_==R::Hurt) {if(motion(a)){a.routine_=R::Wait;a.phase_=0;}return;}
  if(a.routine_==R::Attack) {
    if(!a.phase_) {
      const float relative=GameCubeLimitAngle(std::atan2(float(player[0])-a.position_[0],float(player[2])-a.position_[2])-a.heading_);
      start(a,relative<0?6:7);a.attackHit_=false;a.phase_=1;
    }
    if(leon && !a.attackHit_ && (a.model_->rig()->sequenceFree()&1) && !leon->damageWait()) {
      const auto head=part(a,4);HitVector from{float(head[0]),float(head[1]),float(head[2])};
      HitVector bottom{float(player[0]),float(player[1])+300,float(player[2])},top=bottom;top[1]+=1000;
      if(HitLineCapsule(from,from,top,bottom,600))
        a.attackHit_=leon->receiveGanadoAxeHit(a.feet(),300,GanadoAxeDamageType(player,leon->heading(),head,a.feet()));
    }
    if(motion(a)){a.damageTotal_=0;a.routine_=R::Wait;a.phase_=0;}return;
  }
  if(a.routine_==R::Moo) {if(motion(a)){a.routine_=R::Wait;a.phase_=0;}return;}
  if(!a.phase_){start(a,0,0,5);a.phase_=1;}
  if(motion(a) && (Rnd()&3)==0) {a.routine_=R::Moo;start(a,1);sound(a,4);}
  else if(a.breathTimer_)--a.breathTimer_;
  else {a.breathTimer_=Rnd()%60+30;sound(a,6);}
  if(a.damageTotal_>500 && distanceXZ(part(a,4),player)<1000000.f && std::abs(a.position_[1]-float(player[1]))<500) {
    a.routine_=R::Attack;a.phase_=0;
  }
}
void VillageAnimals::tickChicken(VillageAnimal &a,const CollisionVec &player,bool gunFired,bool alarm,bool burst) {
  using R=VillageAnimalRoutine;
  if(alarm)a.source_.flags&=~0x80000000u;
  if(a.routine_==R::Die) {
    if(a.timer_ && !--a.timer_)sound(a,1);
    if(!a.phase_ && motion(a)){drop(a);a.phase_=1;}return;
  }
  if(a.routine_==R::DieAir) {
    if(!a.phase_) {
      const bool landed=air(a);motion(a,false);
      if(landed){start(a,10,3);sound(a,1);a.phase_=1;}
    }else if(a.phase_==1 && motion(a)){drop(a);a.phase_=2;}
    return;
  }
  if(a.escapeWait_)--a.escapeWait_;
  const float distance=distanceXZ(a.feet(),player);
  if(!a.airborne_ && !a.escapeWait_ && (distance<4000000.f || (gunFired && distance<100000000.f) || (burst && distance<225000000.f))) {
    a.routine_=(Rnd()&3)?R::Dash:R::Jump;a.phase_=0;a.escapeWait_=Rnd()%15+15;
  }
  if(a.routine_==R::Jump) {
    if(!a.phase_) {
      start(a,6,3);a.targetHeading_=GameCubeLimitAngle(std::atan2(a.position_[0]-float(player[0]),a.position_[2]-float(player[2]))+fRand1_1()*pi/2);
      (void)Rnd();a.mirror_=(Rnd()&1)!=0;sound(a,2);a.phase_=1;
    }
    a.heading_=turn(a.heading_,a.targetHeading_,pi/16);
    if(a.phase_==1) {if(motion(a)){a.phase_=2;start(a,7,3,4);a.speed_={0,fRand0_1()*100+150,fRand0_1()*50+150};a.airborne_=true;}}
    else if(a.phase_==2) {const bool landed=air(a);motion(a,false);if(landed){a.phase_=3;start(a,8,3);}}
    else if(motion(a)) {a.routine_=R::Dash;a.phase_=0;}
    return;
  }
  if(a.routine_==R::Dash || a.routine_==R::Walk) {
    const bool dash=a.routine_==R::Dash;
    if(!a.phase_) {
      start(a,dash?4:((Rnd()&1)?2:3),dash?3:10,5);
      a.targetHeading_=dash?GameCubeLimitAngle(std::atan2(a.position_[0]-float(player[0]),a.position_[2]-float(player[2]))+fRand1_1()*pi/2):fRand1_1()*pi;
      a.loops_=Rnd()%(dash?3:5)+3;a.turnTimer_=dash?Rnd()%30+30:0;
      if(dash){sound(a,2);a.mirror_=(Rnd()&1)!=0;}a.phase_=1;
    }
    if(dash) {
      if(a.turnTimer_)--a.turnTimer_;else {a.turnTimer_=Rnd()%15+15;a.targetHeading_=GameCubeLimitAngle(a.targetHeading_+fRand1_1()*pi/4);}
      if(a.stuck_>3)a.targetHeading_=GameCubeLimitAngle(a.targetHeading_+(a.mirror_?1:-1)*pi/5);
    }else if(a.stuck_>30)a.loops_=0;
    a.heading_=turn(a.heading_,a.targetHeading_,dash?pi/16:pi/128);
    if(motion(a)) {if(a.loops_)--a.loops_;else{a.routine_=R::Wait;a.phase_=0;}}
    return;
  }
  if(a.routine_==R::Peck) {
    if(a.timer_ && !--a.timer_ && !a.eggLaid_) {
      const unsigned id=(Rnd()&7)?(Rnd()%3?8:9):10;a.eggLaid_=true;
      drops_.push_back({unsigned(a.source_.listIndex),id,1,a.feet(),-1});
    }
    if(motion(a)){a.routine_=(a.eggLaid_||!(Rnd()&1))?R::Walk:R::Wait;a.phase_=0;}return;
  }
  if(!a.phase_){start(a,0,30,5);a.phase_=1;}
  if(motion(a) && !(a.source_.flags&0x80000000u) && (Rnd()&1)) {
    if(Rnd()&3){a.routine_=R::Peck;start(a,1,10);}
    else if((Rnd()&3)==0 && !a.eggLaid_){a.routine_=R::Peck;start(a,13,10);a.timer_=15;}
    else {a.routine_=R::Peck;start(a,12,10);}
  }
}
void VillageAnimals::tick(const CollisionVec &player,bool fired,bool alarm,bool bell,LeonPreview *leon) {
  for(double v:player)if(!std::isfinite(v)||std::abs(v)>1e7)throw std::runtime_error("r101 animals: invalid player position");
  RandomOwner random(randomState_);const bool burst=bell&&!previousBell_;previousBell_=bell;
  for(auto &ptr:animals_) {
    auto &a=*ptr;if(a.savedDead_)continue;
    if(a.damageWait_)--a.damageWait_;
    if(a.hp_)damage(a);
    if(a.source_.id==0x26) {
      // cEm26::move restores these X scales before the next pose.
      for(unsigned p:{1u,2u}){auto scale=a.model_->rig()->partScale(p);scale[0]=1;a.model_->rig()->setPartScale(p,scale);}
      tickCow(a,player,leon);
    }else tickChicken(a,player,fired,alarm,burst);
    if(a.burnFade_ && !a.hp_) {
      // Original death fades RGB to 0x20; retain the solid corpse alpha.
      a.deathShade_=std::max(32.f/255,a.deathShade_-32.f/255);
    }
    place(a);
  }
}
std::optional<VillageAnimalHit> VillageAnimals::target(const CollisionVec &from,const CollisionVec &to,unsigned weapon,float maximum) const {
  std::optional<VillageAnimalHit> result;
  for(std::size_t i=0;i<animals_.size();++i) {
    const auto &a=*animals_[i];if(!a.hp_ || a.savedDead_ || a.damageWait_ || a.pendingDamage_)continue;
    if(const auto hit=HitAuthoredShapes(shapes(a.source_.id),a.model_->rig()->jointMatrices(),a.model_->worldMatrix(),from,to,maximum,weapon))
      if(!result || hit->lenSq<result->hit.lenSq)result=VillageAnimalHit{i,*hit};
  }
  return result;
}
void VillageAnimals::applyShot(const VillageAnimalHit &hit,unsigned serial,unsigned weapon,unsigned power,const CollisionVec &from) {
  if(hit.actor>=animals_.size() || power>6)throw std::runtime_error("r101 animals: invalid hit");
  auto &a=*animals_[hit.actor];auto &previous=weapon==16?a.lastMelee_:a.lastShot_;
  if(!serial || previous==serial || !a.hp_ || a.savedDead_ || a.damageWait_ || a.pendingDamage_)return;
  previous=serial;a.pendingDamage_=VillageAnimal::Damage{serial,weapon,power,hit.hit,from};
}
void VillageAnimals::queueExplosion(const CollisionVec &center,float radius,unsigned weapon) {
  if(!std::isfinite(radius)||radius<0)throw std::runtime_error("r101 animals: invalid explosion radius");
  for(auto &a:animals_)if(a->hp_) {
    double d=0;for(unsigned k=0;k<3;++k){const double v=a->position_[k]-center[k];d+=v*v;}
    if(d<radius*radius)a->pendingBlast_=weapon;
  }
}
void VillageAnimals::setAuthoredDrops(const RoomArchive &room,ScenarioState &scenario) {
  scenario_=&scenario;for(auto &a:animals_)a->authoredDrop_=FindAuthoredEnemyDrop(room,unsigned(a->source_.listIndex),scenario,a->source_.room);
}
std::vector<VillageAnimalSound> VillageAnimals::takeSounds(){auto result=std::move(sounds_);sounds_.clear();return result;}
std::vector<VillageAnimalDrop> VillageAnimals::takeDrops(){auto result=std::move(drops_);drops_.clear();return result;}
std::vector<unsigned> VillageAnimals::deadIndices() const {std::vector<unsigned> r;for(const auto &a:animals_)if(!a->hp_)r.push_back(unsigned(a->source_.listIndex));return r;}
void VillageAnimals::restoreDead(const std::vector<unsigned> &indices) {
  for(auto &a:animals_)if(std::find(indices.begin(),indices.end(),unsigned(a->source_.listIndex))!=indices.end()) {
    a->hp_=0;a->savedDead_=a->dropEmitted_=true;a->pendingDamage_.reset();a->pendingBlast_.reset();a->model_->forceHidden(true);
  }
}
}
