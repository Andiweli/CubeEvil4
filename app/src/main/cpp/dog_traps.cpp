#include "dog_traps.h"
#include "archive.h"
#include "weapon_preview.h"
#include "ports/motion64/runtime.h"
#include "ports/rnd.h"
#include "turn_rules.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
namespace re4 {
namespace {
constexpr float pi=3.1415927f;
constexpr std::array<unsigned,8> indices{9,10,11,12,13,14,0x1d,0x7a};
std::vector<std::uint8_t> arc(const RoomArchive &a,unsigned id,const char *tag) {
  if(id<4 || id-4>=a.entries().size() ||
      !std::equal(tag,tag+3,a.entries()[id-4].tag.begin()))
    throw std::runtime_error("EM21/EM2A: Original asset is missing or has the wrong type");
  return a.copy(id-4);
}
AnimationBytes clip(const RoomArchive &a,unsigned mot,unsigned seq=0) {
  return {arc(a,mot,"FCV"),seq?arc(a,seq,"SEQ"):std::vector<std::uint8_t>{}};
}
SmdPlacement placement(const EnemySpawn &s) {
  SmdPlacement p;p.scale={1,1,1};
  for(unsigned k=0;k<3;++k) {p.position[k]=float(s.position[k]);p.rotation[k]=float(s.rotation[k]);}
  return p;
}
CollisionVec localPoint(const Matrix34 &matrix,const CollisionVec &point) {
  motion64::Mtx m{},inv{};for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)m[r][c]=matrix[r*4+c];
  if(!motion64::PSMTXInverse(m,inv))throw std::runtime_error("EM2A: singular trap matrix");
  motion64::Vec p{float(point[0]),float(point[1]),float(point[2])},v{};
  motion64::PSMTXMultVec(inv,&p,&v);return {v.x,v.y,v.z};
}
std::array<float,3> partWorld(const GenericEventActor &a,unsigned part) {
  const auto matrix=WeaponAttachmentMatrix(a.rig()->jointMatrices().at(part),a.worldMatrix());
  return {matrix[3],matrix[7],matrix[11]};
}
float squared(const CollisionVec &a,const CollisionVec &b) {
  const float x=float(a[0])-float(b[0]),y=float(a[1])-float(b[1]),z=float(a[2])-float(b[2]);
  return x*x+y*y+z*z;
}
bool ignoredBear(unsigned w) {return w==0x14 || w==0x16 || w==0x17 || w==0x2a || w==0xe || w==0x13 || w==0x29 || w==0x2d;}
bool ignoredWire(unsigned w) {return w==0x14 || w==0x16 || w==0x17 || w==0x2a || w==0xe;}
float turn(float from,float to,float limit) {
  float d=to-from;if(d<0)d+=6.2831855f;
  if(d<pi) {if(limit>d)limit=d;} else {d-=6.2831855f;limit=limit>-d?d:-limit;}
  return GameCubeLimitAngle(from+limit);
}
std::vector<AuthoredHitShape> dogShapes() {
  return {{{0,-100,-150},250,650,0,HitOn|HitZAxis,2},
          {{0,0,0},150,100,0,HitOn|HitZAxis,6},
          {{0,-400,0},100,400,0,HitOn,11},
          {{0,-400,0},100,400,0,HitOn,15},
          {{0,-400,0},150,200,0,HitOn,20},
          {{0,-400,0},150,200,0,HitOn,24}};
}
}
bool OriginalBearTrapContact(const CollisionVec &trap,const CollisionVec &player,bool alive,bool damaged) {
  const float x=float(trap[0])-float(player[0]),z=float(trap[2])-float(player[2]);
  return x*x+z*z<=90000.f && alive && !damaged && std::fabs(float(trap[1])-float(player[1]))<=500.f;
}
bool OriginalTripwireContact(const CollisionVec &v,float length) {
  return !(float(v[2])<0) && !(float(v[2])>length+100.f) &&
      !(float(v[0])>400.f) && !(float(v[0])< -400.f) &&
      !(float(v[1])>250.f) && !(float(v[1])< -2000.f);
}
struct DogTraps::Trap {
  enum State { Armed,Bite,Broken,Reset,DogHeld,DogReleased,Wire,Pending,Gone };
  unsigned actor{},state{},timer{},age{},motion{};
  bool pendingHit{},hasShot{},hasKnife{};
  unsigned weapon{},shotSerial{},knifeSerial{};
  float length{};
  CollisionVec catchPlayer{};
  std::optional<EnemyDamagePose> damagePose;
  SmdPlacement placement;
  std::vector<AuthoredHitShape> shapes;
};
DogTraps::DogTraps(const SatCollision &sat,const EnemySpawns &spawns,
    const std::string &em21Drs,const std::string &em2aDrs,ScenarioState &scenario,
    std::set<unsigned> *consumed, std::uint16_t room):scenario_(scenario),consumed_(consumed),hasDog_(room==0x100) {
  const auto dog=RoomArchive::LoadDrs(em21Drs),trap=RoomArchive::LoadDrs(em2aDrs);
  dogClips_={clip(dog,0x18),clip(dog,0x19,0x1c),clip(dog,0x1a,0x1d),
             clip(dog,9,0x12),clip(dog,0x1e)};
  bearClips_={clip(trap,0xb,0xd),clip(trap,0xc,0xe),clip(trap,0xc,0xf),
              clip(trap,0x13,0x14),clip(trap,0x15),clip(trap,0xa)};
  playerClips_={clip(dog,0x1b),clip(trap,0x10,0x11)};
  std::vector<unsigned> selected;
  if(hasDog_)selected.assign(indices.begin(),indices.end());
  else for(const auto &s:spawns.records())if(s.room==room && s.id==0x2a && s.alive() && !s.alreadySet())selected.push_back(unsigned(s.listIndex));
  for(unsigned list:selected) {
    const auto *s=spawns.find(list);
    if(!s || s->room!=room || s->id!=(hasDog_ && list==9?0x21:0x2a) || s->type>2)
      throw std::runtime_error("r100: original dog/trap ESL mismatch");
    sources_.push_back(*s);const unsigned id=s->id==0x21?4:4+unsigned(s->type)*2;
    const auto &data=s->id==0x21?dog:trap;
    auto a=std::make_unique<GenericEventActor>("em"+(s->id==0x21?std::string("21_"):std::string("2a_"))+std::to_string(list),
        "source.bin",arc(data,id,"BIN"),arc(data,s->id==0x21?(s->type==1?6:5):id+1,"TPL"));
    a->prepareStaticPose();a->setLightMaskOverride(2);a->setDisplayOverride(false);
    a->setPlacement(placement(*s));
    if(hasDog_ && list==9) {
      a->rig()->appendActions(dogClips_);dogPlacement_=placement(*s);
      dogProbe_=std::make_unique<WalkProbe>(sat,WalkConfig{450,1000,2600,250,600});
      dogProbe_->place(s->position,1000);
      a->rig()->startActionFlags(0,0,0,5);a->rig()->advanceAction(0);
      dogTimer_=random()%30+75;
    } else {
      auto t=std::make_unique<Trap>();t->actor=actors_.size();t->placement=placement(*s);
      if(s->type==0) {
        a->rig()->appendActions(bearClips_);
        t->state=hasDog_ && s->setNo==1?Trap::DogHeld:Trap::Armed;
        t->shapes={{{0,0,0},500,200,500,HitOn|HitCube,0}};
        if(t->state==Trap::DogHeld) {
          // em21TrapSearch snaps the unused set1 trap to the hind leg.
          t->placement=dogPlacement_;
          t->placement.position=TransformPoint(PlacementMatrix(dogPlacement_),{256.48f,0,-304.75f});
          a->setPlacement(t->placement);a->rig()->startActionFlags(3,0,0,1);a->rig()->advanceAction(0);
        } else {
          a->rig()->startActionFlags(5,0,0,5);a->rig()->advanceAction(0);
        }
      } else {
        t->state=Trap::Wire;
        const float scale=float(std::max<unsigned>(s->hp,1))*.001f*.5f;
        for(unsigned part:{1u,2u}) {auto p=a->rig()->partPosition(part);p[2]*=scale;a->rig()->setPartPosition(part,p);}
        const auto p0=partWorld(*a,0),p2=partWorld(*a,2);
        const float dx=p0[0]-p2[0],dy=p0[1]-p2[1],dz=p0[2]-p2[2];t->length=std::sqrt(dx*dx+dy*dy+dz*dz);
        const float h=std::fabs(a->rig()->partPosition(1)[2])*.4f;
        t->shapes={{{0,-150,-100},130,300,0,HitOn,1},{{0,-150,100},130,300,0,HitOn,3},
                   {{0,-30,0},130,h,0,HitOn|HitZAxis,1},{{0,-50,h},130,h,0,HitOn|HitZAxis,1},
                   {{0,-70,h+h},130,h,0,HitOn|HitZAxis,1},{{0,-50,h*3},130,h,0,HitOn|HitZAxis,1},
                   {{0,-30,h*4},130,h,0,HitOn|HitZAxis,1}};
      }
      if(consumed_ && consumed_->count(list)) {
        if(s->type) t->state=Trap::Gone;
        else {t->state=Trap::Broken;a->rig()->startActionFlags(2,0,unsigned(a->rig()->maxFrame(2))-1,1);a->rig()->advanceAction(0);}
      }
      traps_.push_back(std::move(t));
    }
    actors_.push_back(std::move(a));
  }
  if(!hasDog_ || scenario_.scenario.test(12) || (consumed_ && consumed_->count(9))) dogState_=DogState::Gone;
}
DogTraps::~DogTraps()=default;
unsigned DogTraps::random() {
  const auto old=RndState();RndInit(randomState_);const auto result=Rnd();randomState_=RndState();RndInit(old);return result;
}
void DogTraps::consume(unsigned id) {if(consumed_)consumed_->insert(id);}
void DogTraps::installPlayerMotions(LeonPreview &p) const {p.installDogTrapMotions(playerClips_);}
void DogTraps::releaseAfterTruck() {
  if(released_)return;
  released_=true;
  if(hasDog_)actors_[0]->setDisplayOverride(dogState_!=DogState::Gone);
  for(const auto &t:traps_) {
    actors_[t->actor]->setDisplayOverride(t->state!=Trap::Gone);
    if(t->state==Trap::Wire)effect(t->actor,sources_[t->actor].type==1?3:5);
  }
}
bool DogTraps::rescueAvailable(const LeonPreview &p) const {
  if(!released_ || dogState_!=DogState::Trapped || !p.alive() || p.damageWait() ||
      p.traversalActive() || p.grabbed() || p.blastActive() || p.hurtActive() || p.dogTrapActive() || p.itemExamActive())return false;
  const CollisionVec dog{dogPlacement_.position[0],dogPlacement_.position[1],dogPlacement_.position[2]};
  return !(squared(p.feet(),dog)>9000000.f);
}
bool DogTraps::beginRescue(LeonPreview &p,WalkProbe &probe) {
  if(!rescueAvailable(p))return false;
  const auto v=TransformPoint(PlacementMatrix(dogPlacement_),{431.86002f,0,-989.88f});
  p.beginDogRescue(probe,{v[0],v[1],v[2]},dogPlacement_.rotation[1]);
  dogState_=DogState::Releasing;startDog(1,3,5);rescueStream_=true;
  scenario_.scenario.set(12);
  for(auto &t:traps_)if(t->state==Trap::DogHeld) {
    t->state=Trap::DogReleased;t->motion=3;t->age=0;
    actors_[t->actor]->rig()->startActionFlags(3,0,0,1);
  }
  return true;
}
void DogTraps::sound(unsigned actor,unsigned block,unsigned cue,bool replace) {
  DogTrapSound s;s.block=block;s.number=cue;s.enemyId=sources_.at(actor).id;s.replace=replace;
  const auto &m=actors_.at(actor)->worldMatrix();s.position={m[3],m[7],m[11]};sounds_.push_back(s);
}
void DogTraps::effect(unsigned actor,unsigned sequence) {
  DogTrapEffect e;e.actor=actor;e.sequence=sequence;e.eventSerial=++serial_;
  e.player=sequence==1;e.detached=sequence==9;
  if(e.detached && actor && !traps_[actor-1]->damagePose)return;
  const auto &m=actors_.at(actor)->worldMatrix();e.position={m[3],m[7],m[11]};
  for(unsigned k=0;k<3;++k)e.rotation[k]=float(sources_[actor].rotation[k]);
  if(e.detached && actor) {
    const auto &pose=*traps_[actor-1]->damagePose;e.position=pose.position;e.rotation=pose.rotation;
  }
  effects_.push_back(e);
}
void DogTraps::startDog(unsigned c,unsigned blend,unsigned flags) {
  dogMotion_=c;dogMotionAge_=0;actors_[0]->rig()->startActionFlags(c,blend,0,flags);
}
void DogTraps::escapeDog(bool hurt) {
  if(dogState_==DogState::Gone)return;
  // The release FCV moves the original cEm directly while collision is off.
  // Transfer that position to the collision owner before enabling its run;
  // otherwise the first colliding step would snap back to the trap spawn.
  if(dogProbe_->ready())dogProbe_->setScriptPosition({dogPlacement_.position[0],
      dogPlacement_.position[1],dogPlacement_.position[2]});
  hurtEscape_=hurt;dogState_=DogState::Running;dogPhase_=0;consume(9);
  startDog(hurt?3:2,3,5);if(hurt)sound(0,8,0xb,true);
}
void DogTraps::moveDog(const RootMotionStep &step,bool collision) {
  const auto matrix=PlacementMatrix(dogPlacement_);
  std::array<float,3> d{};
  for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)d[r]+=matrix[r*4+c]*float(step.translation[c]);
  if(collision && dogProbe_->ready()) {
    dogProbe_->moveDelta({d[0],0,d[2]});const auto p=dogProbe_->position();
    dogPlacement_.position={float(p[0]),float(p[1]),float(p[2])};
  } else for(unsigned k=0;k<3;++k)dogPlacement_.position[k]+=d[k];
  dogPlacement_.rotation[1]=GameCubeLimitAngle(dogPlacement_.rotation[1]+float(step.yaw));
  actors_[0]->setPlacement(dogPlacement_);
}
void DogTraps::dogTick() {
  if(dogState_==DogState::Gone)return;
  auto &rig=*actors_[0]->rig();
  if(dogState_==DogState::Trapped) {
    if(dogTimer_)--dogTimer_;else {dogTimer_=random()%30+75;sound(0,8,0xc,true);}
    rig.advanceAction();++dogMotionAge_;
  } else if(dogState_==DogState::Releasing) {
    // em21_R1_Trap's flags5 enables MotionAddSpeed, including the step out
    // of the jaws near the end of EM21_MOT_TRAP_RELEASE. The hosted rig
    // leaves root ownership to this controller, as in the escape motions.
    moveDog(rig.advanceRootAction(),false);
    // Source MotionMove also returns nonzero on a looping SEQ boundary.
    if(rig.status().state)escapeDog(false);
    ++dogMotionAge_;
  } else if(dogState_==DogState::Running) {
    const float x=-24950.f-dogPlacement_.position[0],z=-26740.f-dogPlacement_.position[2];
    const float desired=std::atan2(x,z),old=dogPlacement_.rotation[1];
    dogPlacement_.rotation[1]=turn(old,desired,pi/40.f);
    const float d=GameCubeLimitAngle(desired-dogPlacement_.rotation[1]);
    const float target=std::fabs(d)<.049087387f?0.f:d>0?-.5235988f:.5235988f;
    dogTilt_=dogTilt_*.8f+target*.2f;dogPlacement_.rotation[2]=dogTilt_;
    moveDog(rig.advanceRootAction(),true);++dogMotionAge_;
    const float dx=dogPlacement_.position[0]+24950.f,dz=dogPlacement_.position[2]+26740.f;
    if(dx*dx+dz*dz<36000000.f) {dogState_=DogState::Jumping;dogTilt_*=.8f;startDog(4,3,1);}
  } else if(dogState_==DogState::Jumping) {
    if(rig.sequenceFrame()>2.7f && rig.sequenceFrame()<3.3f)sound(0,8,7);
    dogTilt_*=.8f;dogPlacement_.rotation[2]=dogTilt_;
    const auto step=rig.advanceRootAction();moveDog(step,false);++dogMotionAge_;
    if(step.ended){dogState_=DogState::Leaving;startDog(2,3,5);}
  } else if(dogState_==DogState::Leaving) {
    const auto step=rig.advanceRootAction();dogTilt_*=.8f;dogPlacement_.rotation[2]=dogTilt_;
    moveDog(step,false);++dogMotionAge_;if(rig.status().state)dogState_=DogState::Fading;
  } else {
    moveDog(rig.advanceRootAction(),false);dogAlpha_-=.1f;
    actors_[0]->setAlphaOverride(unsigned(std::max(0.f,dogAlpha_)*255));
    if(dogAlpha_<=0){dogState_=DogState::Gone;actors_[0]->setDisplayOverride(false);}
  }
  if(const auto s=rig.consumeSequenceSound()) if(s->se)sound(0,8,s->se);
}
void DogTraps::tick(LeonPreview &p,WalkProbe &probe,const std::vector<CollisionVec> &ganados) {
  if(!released_)return;
  ++ticks_;
  for(auto &t:traps_) {
    auto &a=*actors_[t->actor];auto &rig=*a.rig();const auto &source=sources_[t->actor];
    if(t->pendingHit) {t->pendingHit=false;hitActor(t->actor,t->weapon);t->damagePose.reset();}
    if(t->state==Trap::Wire) {
      bool crossed=OriginalTripwireContact(localPoint(a.worldMatrix(),p.feet()),t->length);
      if(!crossed)for(const auto &e:ganados)if(OriginalTripwireContact(localPoint(a.worldMatrix(),e),t->length)){crossed=true;break;}
      if(crossed){t->state=Trap::Pending;t->timer=3;consume(source.listIndex);}
    }
    if(t->state==Trap::Pending) {
      if(t->timer)--t->timer;
      else {
        t->state=Trap::Gone;a.setDisplayOverride(false);sound(t->actor,1,0x14);effect(t->actor,source.type==1?0:4);
        const auto p1=partWorld(a,1);
        for(unsigned part:{0u,2u}) {
          const auto v=partWorld(a,part);motion64::Vec d{p1[0]-v[0],p1[1]-v[1],p1[2]-v[2]},unit{};
          motion64::PSVECNormalize(&d,&unit);
          ObjectBlast b;b.index=t->actor;b.slot=source.listIndex;b.serial=unsigned(++serial_);b.radius=3000;
          b.position={v[0]+unit.x*2000.f,v[1]+unit.y*2000.f,v[2]+unit.z*2000.f};blasts_.push_back(b);
        }
      }
    } else if(t->state==Trap::Armed) {
      const CollisionVec pos{t->placement.position[0],t->placement.position[1],t->placement.position[2]};
      if(OriginalBearTrapContact(pos,p.feet(),p.alive(),p.damageWait()!=0 || p.dogTrapActive() || p.traversalActive() || p.grabbed())) {
        if(p.beginBearTrapBite(probe,pos)) {
          t->state=Trap::Bite;t->timer=10;t->age=0;t->motion=0;consume(source.listIndex);
          t->catchPlayer=p.feet();
          const float toward=std::atan2(float(pos[0]-p.feet()[0]),float(pos[2]-p.feet()[2]));
          auto catchPlacement=t->placement;catchPlacement.position={float(p.feet()[0]),float(p.feet()[1]),float(p.feet()[2])};
          catchPlacement.rotation[1]=toward;
          t->placement.position=TransformPoint(PlacementMatrix(catchPlacement),{34.69f,0,250.42f});
          t->placement.rotation[1]=std::atan2(float(p.feet()[0]-pos[0]),float(p.feet()[2]-pos[2]));
          a.setPlacement(t->placement);
          p.setBearTrapCameraOrigin({t->placement.position[0],t->placement.position[1],t->placement.position[2]});
          rig.startActionFlags(0,5,0,1);sound(t->actor,8,0);sound(t->actor,1,0x39);
          sounds_.back().position=p.feet();sounds_.back().enemyId=0;effect(t->actor,1);
        }
      }
      if(t->state==Trap::Armed)rig.advanceAction();
    } else if(t->state==Trap::DogHeld) {
      // R1 case1 deliberately reinstalls the same closed leg pose every tick.
      rig.startActionFlags(3,0,0,1);rig.advanceAction();
    } else if(t->state==Trap::DogReleased) {
      if(rig.advanceAction().ended) {t->state=Trap::Broken;consume(source.listIndex);}
    } else if(t->state==Trap::Reset) {
      if(rig.advanceAction().ended)t->state=Trap::Armed;
    } else if(t->state==Trap::Bite || t->state==Trap::Broken) {
      if(t->state==Trap::Bite && t->timer) {
        --t->timer;
        // EmCatchPLSet type0: trap closes its full offset onto Leon, while
        // Leon's Catch_pos_adj remains zero. Same worldoffset10-tick owner.
        const auto feet=p.feet();
        t->placement.position[0]+=float(feet[0]-t->catchPlayer[0]);
        t->placement.position[2]+=float(feet[2]-t->catchPlayer[2]);
        t->catchPlayer=feet;a.setPlacement(t->placement);
      }
      if(t->state==Trap::Bite)p.setBearTrapCameraOrigin(
          {t->placement.position[0],t->placement.position[1],t->placement.position[2]});
      if(rig.advanceAction().ended && t->state==Trap::Bite)t->state=Trap::Broken;
    }
    if(source.type==0)if(const auto s=rig.consumeSequenceSound())if(s->se)sound(t->actor,8,s->se);
  }
  dogTick();
}
std::optional<DogTrapHit> DogTraps::target(const CollisionVec &from,const CollisionVec &to,unsigned weapon,float maxLenSq) const {
  if(!released_)return{};
  std::optional<DogTrapHit> nearest;
  for(unsigned i=0;i<actors_.size();++i) {
    if(!actors_[i]->visible())continue;
    std::vector<AuthoredHitShape> shapes;
    if(hasDog_ && i==0) {if(dogState_!=DogState::Trapped)continue;shapes=dogShapes();}
    else {const auto &t=*traps_[i-unsigned(hasDog_)];if(t.state==Trap::Gone || t.state==Trap::Broken || t.state==Trap::Pending)continue;shapes=t.shapes;}
    const auto &a=*actors_[i];auto hit=HitAuthoredShapes(shapes,a.rig()->jointMatrices(),a.worldMatrix(),from,to,maxLenSq,weapon);
    if(hit && (!nearest || hit->lenSq<nearest->hit.lenSq)) {
      nearest=DogTrapHit{i,*hit,from,(shapes.at(hit->partIndex).flags&HitNoMark)?3u:2u};
      maxLenSq=hit->lenSq;
    }
  }
  return nearest;
}
bool DogTraps::hitActor(std::size_t actor,unsigned weapon) {
  if(hasDog_ && !actor) {
    if(dogState_!=DogState::Trapped)return false;
    for(auto &t:traps_)if(t->state==Trap::DogHeld){t->state=Trap::Reset;actors_[t->actor]->rig()->startActionFlags(4,0,0,1);sound(t->actor,8,0);}
    escapeDog(true);return true;
  }
  auto &t=*traps_.at(actor-unsigned(hasDog_));auto &a=*actors_[actor];
  if(t.state==Trap::Gone || t.state==Trap::Pending || t.state==Trap::Broken)return false;
  if(sources_[actor].type) {
    if(ignoredWire(weapon))return false;
    t.state=Trap::Pending;t.timer=3;consume(sources_[actor].listIndex);effect(actor,9);return true;
  }
  if(ignoredBear(weapon))return false;
  sound(actor,8,2);effect(actor,9);
  if(t.state==Trap::DogHeld) {t.state=Trap::Reset;a.rig()->startActionFlags(4,0,0,1);escapeDog(true);}
  else {t.state=Trap::Broken;t.age=0;t.motion=1;a.rig()->startActionFlags(1,0,0,1);sound(actor,8,0);effect(actor,2);consume(sources_[actor].listIndex);}
  return true;
}
bool DogTraps::applyShot(const DogTrapHit &hit,unsigned serial,unsigned weapon) {
  if(hit.actor>=actors_.size() || !actors_[hit.actor]->visible())return false;
  if(hasDog_ && !hit.actor)return hitActor(0,weapon);
  auto &t=*traps_[hit.actor-unsigned(hasDog_)];if(t.hasShot && t.shotSerial==serial)return false;
  if(hit.hit.partIndex<t.shapes.size()) {
    const auto &shape=t.shapes[hit.hit.partIndex];const auto &a=*actors_[hit.actor];
    const auto matrix=shape.partsNo>0?WeaponAttachmentMatrix(a.rig()->jointMatrices().at(shape.partsNo-1),a.worldMatrix()):a.worldMatrix();
    t.damagePose=GanadoDamagePose(shape,hit.hit.cross,matrix,hit.source);
  }
  t.hasShot=true;t.shotSerial=serial;t.pendingHit=true;t.weapon=weapon;return true;
}
bool DogTraps::shoot(const PlayerShot &s,float maxLenSq) {
  const unsigned w=s.weaponNumber();
  bool hit=false;const auto attempt=[&](const CollisionVec &from,const CollisionVec &to){if(auto h=target(from,to,w,maxLenSq))hit=applyShot(*h,s.serial,w)||hit;};
  attempt(s.from,s.to);for(const auto &ray:s.pellets)attempt(ray[0],ray[1]);return hit;
}
bool DogTraps::knife(const PlayerMeleeEvent &e) {
  if(e.kind!=PlayerMeleeKind::Knife)return false;
  bool hit=false;
  for(const auto &ray:e.rays)if(auto h=target(ray.first,ray.second,0x10)) {
    if(hasDog_ && !h->actor){hit=hitActor(0,0x10)||hit;continue;}
    auto &t=*traps_[h->actor-unsigned(hasDog_)];if(t.hasKnife && t.knifeSerial==e.serial)continue;
    if(h->hit.partIndex<t.shapes.size()) {
      const auto &shape=t.shapes[h->hit.partIndex];const auto &a=*actors_[h->actor];
      const auto matrix=shape.partsNo>0?WeaponAttachmentMatrix(a.rig()->jointMatrices().at(shape.partsNo-1),a.worldMatrix()):a.worldMatrix();
      t.damagePose=GanadoDamagePose(shape,h->hit.cross,matrix,h->source);
    }
    t.hasKnife=true;t.knifeSerial=e.serial;t.pendingHit=true;t.weapon=0x10;hit=true;
  }
  return hit;
}
void DogTraps::applyBlast(const ObjectBlast &blast) {
  if(!released_)return;
  for(unsigned i=0;i<actors_.size();++i)if(actors_[i]->visible()) {
    const auto &m=actors_[i]->worldMatrix();
    if(squared(blast.position,{m[3],m[7],m[11]})>(blast.radius+450)*(blast.radius+450))continue;
    if(hasDog_ && !i)hitActor(0,0x13);else if(sources_[i].type)hitActor(i,0x13);
  }
}
std::vector<DogTrapSound> DogTraps::drainSounds(){std::vector<DogTrapSound> v;v.swap(sounds_);return v;}
std::vector<DogTrapEffect> DogTraps::drainEffects(){std::vector<DogTrapEffect> v;v.swap(effects_);return v;}
std::vector<Matrix34> DogTraps::effectParents(const DogTrapEffect &event,const LeonPreview &player) const {
  std::vector<Matrix34> parts;
  if(event.detached)return parts;
  const auto &joints=event.player?player.rig().jointMatrices():actors_.at(event.actor)->rig()->jointMatrices();
  const auto placement=event.player?player.placement():actors_.at(event.actor)->worldMatrix();
  for(const auto &joint:joints)parts.push_back(WeaponAttachmentMatrix(joint,placement));
  return parts;
}
std::vector<ObjectBlast> DogTraps::drainBlasts(){std::vector<ObjectBlast> v;v.swap(blasts_);return v;}
bool DogTraps::takeRescueStreamRequest(){const bool r=rescueStream_;rescueStream_=false;return r;}
std::string DogTraps::describe() const {
  std::ostringstream s;s<<(hasDog_?"EM21 dog + ":"")<<traps_.size()<<" original EM2A traps / released="<<released_<<" / Tick="<<ticks_;return s.str();
}
} // namespace re4
