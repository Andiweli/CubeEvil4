#include "forest_animals.h"
#include "archive.h"
#include "ports/rnd.h"
#include "ports/motion64/runtime.h"
#include "turn_rules.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
namespace re4 {
namespace {
constexpr float pi=3.1415927f;
// Module ARC indices include the four REL prefix slots; DRS data starts
// at ARC4. FCV/SEQ are loaded by exact authored ARC, never by tag guessing.
struct CrowClip {unsigned motion,sequence;};
constexpr std::array<CrowClip,18> clips{{
 {9,0},{10,0},{11,28},{12,29},
 {14,36},{14,37},{14,38},{14,39},{14,40},
 {13,35},{13,34},{13,33},{13,32},{13,31},{13,30},
 {22,42},{21,0},{20,0}
}};
struct RandomOwner {
 std::uint16_t &owner;std::uint16_t previous;
 explicit RandomOwner(std::uint16_t &state):owner(state),previous(RndState()){RndInit(owner);}
 ~RandomOwner(){owner=RndState();RndInit(previous);}
};
std::vector<std::uint8_t> asset(const RoomArchive &a,unsigned sourceArc,const char *tag) {
 if(sourceArc<4 || sourceArc-4>=a.entries().size() ||
    !std::equal(tag,tag+3,a.entries()[sourceArc-4].tag.begin()))
  throw std::runtime_error("EM23: Original asset is missing or has the wrong type");
 return a.copy(sourceArc-4);
}
float turn(float from,float to,float add) {
 float d=to-from;if(d<0)d+=6.2831855f;
 if(d<pi) {if(add>d)add=d;}
 else {d-=6.2831855f;if(add>-d)add=d;else add=-add;}
 return GameCubeLimitAngle(from+add);
}
float muku(float from,const std::array<float,3> &position,const CollisionVec &to,float limit) {
 const float dx=float(to[0])-position[0],dz=float(to[2])-position[2];
 const float desired=float(std::atan2(double(dx),double(dz)));
 const float d=GameCubeLimitAngle(desired-from);
 float step=limit;
 if(d>0){if(d<step)step=d;}else if(d>-step)step=-d;
 if(!(d>=0))step=-step;
 return GameCubeLimitAngle(from+step);
}
void append(const SkinnedModel &body,StaticScene &scene,std::vector<SkinCorner> &skin) {
 const auto &m=body.mesh;
 for(const auto &primitive:m.primitives) {
  if(primitive.triangles.empty())continue;
  const auto &h=m.materialHeaders.at(primitive.displayList);SceneDraw d;
  d.first=scene.vertices.size()/3;d.count=primitive.triangles.size()*3;
  d.image=h[12];d.repeat=m.colored;d.vertexColors=m.colored&&m.vertexAmbient;
  d.alphaTest=(h[11]&4)!=0;d.alphaImage=h[14];d.alphaReference=h[20];scene.draws.push_back(d);
  for(const auto &t:primitive.triangles)for(auto c:t) {
   const auto &r=primitive.vertices.at(c);const auto &p=m.positions.at(r.position),&n=m.normals.at(r.normal);
   const auto &uv=m.texcoords.at(r.texcoord);scene.vertices.insert(scene.vertices.end(),p.begin(),p.end());
   scene.normals.insert(scene.normals.end(),n.begin(),n.end());scene.uv.insert(scene.uv.end(),uv.begin(),uv.end());
   const auto color=d.vertexColors?m.colors.at(r.color):std::array<std::uint8_t,4>{255,255,255,255};
   scene.colors.insert(scene.colors.end(),color.begin(),color.end());
   skin.push_back({body.weights.at(body.positionWeights.at(r.position)),body.weights.at(body.normalWeights.at(r.normal))});
  }
  scene.triangles+=primitive.triangles.size();
 }
 scene.included++;
}
std::vector<AuthoredHitShape> crowShape() {
 // em23_R0_Init::YarareInit(em,0,-100,0,250,200,1,YAT_FLAG_ON).
 return {{{0,-100,0},250,200,0,HitOn,0}};
}
unsigned weaponNumber(unsigned item) {return OriginalWeaponNumber(item);}
}
std::vector<EnemySpawn> ForestCrowSpawns(const EnemySpawns &spawns,std::uint16_t room) {
 if(room!=0x100)throw std::runtime_error("EM23: unsupported room corpse/scenario policy");
 std::vector<EnemySpawn> result;
 for(const auto &s:spawns.records()) {
  if(s.room!=room || s.id!=0x23 || !s.alive() || s.alreadySet())continue;
  if(s.setNo!=0 || s.type!=0 || !s.hp || s.flags || result.size()>=5)
   throw std::runtime_error("r100: unsupported authored crow state");
  result.push_back(s);
 }
 if(result.size()!=5)throw std::runtime_error("r100: five original EM23 records missing");
 return result;
}
ForestCrow::ForestCrow(const SatCollision &sat,const EnemySpawn &source,
 const SkinnedModel &body,const std::vector<AnimationBytes> &animations)
 :source_(source),sat_(&sat),rig_(std::make_unique<AnimationRig>(body,animations)),hp_(source.hp) {
 for(unsigned axis=0;axis<3;++axis)position_[axis]=float(source.position[axis]);
 // EmSetWork uses a float rotation unit; preserve that operation, rather
 // than rounding the ESL adapter's precomputed double heading.
 const auto raw=unsigned(source.raw[20])<<8|source.raw[21];
 const auto signedYaw=raw<0x8000?int(raw):int(raw)-0x10000;
 heading_=float(signedYaw)*(pi/16384.f);
 scale_=fRand1_1()*.1f+1.f;
 stateTimer_=Rnd()%300+150;
 flyHeight_=float(unsigned(source.listIndex)%5)*1000.f+8000.f;
 rig_->startActionFlags(0,0,0,5);rig_->advanceRootAction();
}
ForestCrow::ForestCrow(ForestCrow &&) noexcept=default;
ForestCrow &ForestCrow::operator=(ForestCrow &&) noexcept=default;
ForestCrow::~ForestCrow()=default;
CollisionVec ForestCrow::feet() const {return {position_[0],position_[1],position_[2]};}
Matrix34 ForestCrow::placement() const {
 SmdPlacement p;p.position=position_;p.rotation={0,heading_,0};p.scale={scale_,scale_,scale_};return PlacementMatrix(p);
}
const void *ForestCrow::gpuOwner(unsigned variant) const {
 if(variant>1)throw std::runtime_error("EM23: invalid wing GPU owner");
 return &gpuOwners_[variant];
}
void ForestCrow::start(unsigned clip,unsigned flags,unsigned hokan) {
 if(clip>=clips.size())throw std::runtime_error("EM23: invalid authored motion");
 clip_=clip;rig_->startActionFlags(clip,hokan,0,std::uint16_t(flags));
}
bool ForestCrow::moveMotion(bool root) {
 if(!root)return rig_->advanceAction().state!=0;
 const auto delta=rig_->advanceRootAction();
 motion64::Mtx matrix;motion64::Vec local{float(delta.translation[0]),float(delta.translation[1]),float(delta.translation[2])},world{};
 motion64::PSMTXRotRad(matrix,'y',heading_);motion64::PSMTXMultVecSR(matrix,&local,&world);
 position_[0]+=world.x;position_[1]+=world.y;position_[2]+=world.z;
 heading_=GameCubeLimitAngle(heading_+float(delta.yaw));
 return rig_->status().state!=0;
}
bool ForestCrow::addSpeedAir(float heading) {
 const float oldY=position_[1];motion64::Mtx matrix;
 motion64::Vec local{speed_[0],speed_[1],speed_[2]},world{};
 motion64::PSMTXRotRad(matrix,'y',heading);motion64::PSMTXMultVecSR(matrix,&local,&world);
 position_[0]+=world.x;position_[1]+=world.y;position_[2]+=world.z;
 if(world.y>=0)return false;
 CollisionVec a=feet(),b=feet();a[1]=oldY+500.f;
 if(const auto hit=sat_->line(a,b,SatSubset::All,0,0))if(hit->point[1]>position_[1]) {
  position_[1]=float(hit->point[1]);speed_[1]*=.5f;return true;
 }
 if(position_[1]<-100000.f) {position_[1]=-100000.f;return true;}
 return false;
}
void ForestCrow::damage(unsigned weapon,float distanceSq) {
 unsigned amount=9999;
 switch(weapon) {
 case 0:case 2:case 0xb:case 0x10:amount=Rnd()%200+900;break;
 case 7:case 8:case 0x21:if(distanceSq>=36000000.f)amount=Rnd()%200+900;break;
 case 0xe:amount=0;break;
 default:break;
 }
 hp_=amount>=hp_?0:hp_-amount;routine_=CrowRoutine::DamageAir;routineStep_=0;damageSound_=true;
}
void ForestCrow::tick(const CollisionVec &player,bool gunFired,
 std::vector<ForestAnimalSound> &sounds,std::vector<ForestAnimalDrop> &drops,
 const EnemyDropContext &context,EnemyDropState &dropState) {
 if(savedDead_)return;
 const auto sound=[&](unsigned cue,bool cooldown=false){sounds.push_back({unsigned(source_.listIndex),8,cue,0x23,feet(),cooldown});};
 if(pendingBlast_) {
  pendingBlast_=false;if(hp_) {hp_=0;pendingDamage_.reset();routine_=CrowRoutine::DamageAir;routineStep_=0;}
 }
 if(pendingDamage_) {const auto hit=*pendingDamage_;pendingDamage_.reset();damage(hit.first,hit.second);}
 if(damageSound_){damageSound_=false;sound(0xf);}
 if(stateTimer_)--stateTimer_;
 const float dx=float(player[0])-position_[0],dz=float(player[2])-position_[2];
 const float distanceSq=dx*dx+dz*dz;
 switch(routine_) {
 case CrowRoutine::Wait:
  if(!routineStep_) {start(Rnd()&3);wing_=1;routineStep_=1;}
  {
   // cModel::partsFixMemory(0x14), then partsFixAdjust after the pose:
   // keep the authored planted foot's XZ, retaining all original FCV data.
   const auto &before=rig_->jointMatrices().at(20);
   const auto oldFoot=TransformPoint(placement(),{before[3],before[7],before[11]});
   if(moveMotion(true))routineStep_=0;
   const auto &after=rig_->jointMatrices().at(20);
   const auto newFoot=TransformPoint(placement(),{after[3],after[7],after[11]});
   position_[0]-=newFoot[0]-oldFoot[0];position_[2]-=newFoot[2]-oldFoot[2];
  }
  if(distanceSq<25000000.f || (gunFired && distanceSq<100000000.f)) {
   routine_=CrowRoutine::Takeoff;routineStep_=0;
  }
  break;
 case CrowRoutine::Takeoff:
  if(!routineStep_) {
   start(4+unsigned(source_.listIndex)%5,1);speed_={0,0,0};
   stateTimer_=Rnd()%300+150;
   targetHeading_=GameCubeLimitAngle(heading_+fRand1_1()*(pi/2.f));routineStep_=1;
  }
  heading_=turn(heading_,targetHeading_,pi/16.f);
  if(rig_->sequenceFree()&1){wing_=0;sound(0xa,true);}
  if(moveMotion(true)) {
   routine_=CrowRoutine::Turn;routineStep_=0;speed_={0,20,30};
  }
  break;
 case CrowRoutine::Turn:
  if(!routineStep_) {
   start(9+std::min(blendVariant_,5u));loops_=5;turnTimer_=Rnd()%120+120;turnDir_=Rnd()&1;routineStep_=1;
  }
  if(turnTimer_ && !--turnTimer_) {turnDir_^=1;turnTimer_=Rnd()%120+120;}
  heading_=muku(heading_,position_,player,turnDir_?pi/256.f:.017453292f);
  if(moveMotion(true)) {
   if(loops_)--loops_;
   else {blendVariant_=std::min(blendVariant_+1,5u);routineStep_=0;}
  }
  speed_[2]+=10.f;if(speed_[2]>150.f)speed_[2]=150.f;
  if(position_[1]<float(player[1])+flyHeight_) {
   speed_[1]+=10.f;if(speed_[1]>50.f)speed_[1]=50.f;
  }else {speed_[1]-=10.f;if(speed_[1]<0.f)speed_[1]=0.f;}
  addSpeedAir(heading_);
  break;
 case CrowRoutine::DamageAir:
  if(!routineStep_) {
   start(15);fallSpin_=(Rnd()&1)?pi/20.f:-pi/20.f;
   fallHeading_=heading_;lastHeight_=position_[1];routineStep_=1;
  }
  if(routineStep_==1) {
   heading_=GameCubeLimitAngle(heading_+fallSpin_);speed_[1]-=10.f;speed_[2]*=.95f;
   if(addSpeedAir(fallHeading_) && position_[1]>=lastHeight_-10.f) {
    sound(9);
    if(hp_)routineStep_=2;
    else {routine_=CrowRoutine::DieNormal;routineStep_=0;}
   }else {lastHeight_=position_[1];moveMotion(true);}
  }else if(routineStep_==2) {start(16);speed_[1]=0;routineStep_=3;}
  else {
   fallSpin_*=.9f;heading_=GameCubeLimitAngle(heading_+fallSpin_);speed_[2]*=.85f;
   addSpeedAir(fallHeading_);
   if(moveMotion(true)){routine_=CrowRoutine::Takeoff;routineStep_=0;}
  }
  wing_=0;
  break;
 case CrowRoutine::DieNormal:
  if(!routineStep_) {
   start(17,1);sound(8);routineStep_=1;
   if(!dropEmitted_) {
    dropEmitted_=true;
    std::optional<EnemyDropChoice> item;
    if(authoredDrop_) {
     if(scenario_ && ClaimAuthoredEnemyDrop(*authoredDrop_,*scenario_))
      item=ResolveAuthoredEnemyDrop(*authoredDrop_,context,dropState);
    }else item=OriginalEnemyRandomDrop(0x23,context,dropState);
    if(item)drops.push_back({unsigned(source_.listIndex),item->id,item->count,
       authoredDrop_ && authoredDrop_->fixedPosition?authoredDrop_->position:feet(),
       authoredDrop_?int(authoredDrop_->effect):-1});
   }
  }
  if(routineStep_==1 && moveMotion(true))routineStep_=2;
  wing_=0;
  break;
 }
 // Original cEm23::move uses Free0x80 after the motion update for flap SE.
 if((rig_->sequenceFree()&0x80) && position_[1]-float(player[1])<3000.f)sound(0xb);
 if(auto sequence=rig_->consumeSequenceSound()) {
  const auto &root=rig_->jointMatrices().at(0);
  const auto point=TransformPoint(placement(),{root[3],root[7],root[11]});
  sounds.push_back({unsigned(source_.listIndex),8,unsigned(sequence->se)-1,0x23,{point[0],point[1],point[2]}});
 }
}
ForestAnimals::ForestAnimals(const SatCollision &sat,const std::vector<std::uint8_t> &esl,
 const std::string &drs,std::uint16_t room) {
 const auto sources=ForestCrowSpawns(EnemySpawns(esl),room);const auto archive=RoomArchive::LoadDrs(drs);
 SkinnedModel body(asset(archive,5,"BIN"));body.validateSkeleton(24);
 if(body.mesh.joints.size()!=24)throw std::runtime_error("EM23: original 24-part skeleton missing");
 for(unsigned wing=0;wing<2;++wing) {
  SkinnedModel wings(asset(archive,6+wing,"BIN"));wings.validateSkeleton(24);
  append(body,meshes_[wing],skin_[wing]);append(wings,meshes_[wing],skin_[wing]);
  meshes_[wing].minimum=body.mesh.minimum;meshes_[wing].maximum=body.mesh.maximum;
  std::vector<std::uint8_t> header(16);header[0]=64;Smd palettes(header);
  palettes.palettes.push_back(asset(archive,8,"TPL"));AttachSceneTextures(palettes,meshes_[wing]);
  if(meshes_[wing].vertices.empty() || meshes_[wing].fallbackTriangles || meshes_[wing].unresolvedAlphaTriangles)
   throw std::runtime_error("EM23: original textured body/wings missing");
 }
 std::vector<AnimationBytes> animations;
 for(const auto &clip:clips)animations.push_back({asset(archive,clip.motion,"FCV"),clip.sequence?asset(archive,clip.sequence,"SEQ"):std::vector<std::uint8_t>{}});
 RandomOwner random(randomState_);animals_.reserve(sources.size());
 for(const auto &source:sources)animals_.push_back(ForestCrow(sat,source,body,animations));
}
const StaticScene &ForestAnimals::mesh(unsigned wing) const {
 if(wing>1)throw std::runtime_error("EM23: invalid wing variant");
 return meshes_[wing];
}
const std::vector<SkinCorner> &ForestAnimals::skin(unsigned wing) const {
 if(wing>1)throw std::runtime_error("EM23: invalid wing variant");
 return skin_[wing];
}
void ForestAnimals::setAuthoredDrops(const RoomArchive &room,ScenarioState &scenario) {
 for(auto &animal:animals_) {
  animal.authoredDrop_=FindAuthoredEnemyDrop(room,unsigned(animal.source().listIndex),scenario,animal.source().room,dropContext_.playerType);
  animal.scenario_=&scenario;
 }
}
void ForestAnimals::tick(const CollisionVec &player,bool gunFired) {
 for(double value:player)if(!std::isfinite(value) || std::abs(value)>10000000.)
  throw std::runtime_error("EM23: invalid player position");
 RandomOwner random(randomState_);
 if(takeoffSoundWait_)--takeoffSoundWait_;
 for(auto &animal:animals_) {
  const auto first=sounds_.size();animal.tick(player,gunFired,sounds_,drops_,dropContext_,sharedDropState_?*sharedDropState_:dropState_);
  // Ctrl11SetSe(...,10,10,11): one shared10-frame crow takeoff budget.
  for(auto i=sounds_.begin()+std::ptrdiff_t(first);i!=sounds_.end();) {
   if(i->sharedCooldown && takeoffSoundWait_)i=sounds_.erase(i);
   else {if(i->sharedCooldown)takeoffSoundWait_=10;++i;}
  }
 }
 ++ticks_;
}
void ForestAnimals::step(double seconds,const CollisionVec &player,bool gunFired) {
 if(!std::isfinite(seconds) || seconds<0 || seconds>.25)throw std::runtime_error("EM23: invalid step");
 accumulator_+=seconds;
 while(accumulator_+1e-12>=1./30) {accumulator_-=1./30;if(accumulator_<0)accumulator_=0;tick(player,gunFired);}
}
std::optional<std::pair<std::size_t,AuthoredHitResult>> ForestAnimals::nearest(
 const CollisionVec &from,const CollisionVec &to,unsigned weapon) const {
 std::optional<std::pair<std::size_t,AuthoredHitResult>> result;
 for(std::size_t i=0;i<animals_.size();++i) {
  const auto &a=animals_[i];if(!a.alive() || !a.visible() || a.routine()==CrowRoutine::DamageAir)continue;
  const auto hit=HitAuthoredShapes(crowShape(),a.rig().jointMatrices(),a.placement(),from,to,1e16f,weapon);
  if(hit && (!result || hit->lenSq<result->second.lenSq))result=std::make_pair(i,*hit);
 }
 return result;
}
std::optional<AuthoredHitResult> ForestAnimals::target(const CollisionVec &from,const CollisionVec &to,unsigned weapon) const {
 if(auto hit=nearest(from,to,weapon))return hit->second;
 return {};
}
void ForestAnimals::queueShot(const PlayerShot &shot) {
 if(!shot.serial || shot.serial==lastShotSerial_)return;
 lastShotSerial_=shot.serial;
 const auto ray=[&](const CollisionVec &from,const CollisionVec &to) {
  if(auto hit=nearest(from,to,weaponNumber(shot.weaponItemId))) {
   auto &a=animals_[hit->first];if(!a.pendingDamage_)a.pendingDamage_=std::make_pair(weaponNumber(shot.weaponItemId),hit->second.lenSq);
  }
 };
 if(shot.pellets.empty())ray(shot.from,shot.to);
 else for(const auto &p:shot.pellets)ray(p[0],p[1]);
}
void ForestAnimals::queueMelee(const PlayerMeleeEvent &melee) {
 if(melee.kind!=PlayerMeleeKind::Knife || !melee.serial || melee.serial==lastMeleeSerial_)return;
 for(const auto &ray:melee.rays)if(auto hit=nearest(ray.first,ray.second,0x10)) {
  auto &a=animals_[hit->first];if(!a.pendingDamage_)a.pendingDamage_=std::make_pair(0x10u,hit->second.lenSq);
  lastMeleeSerial_=melee.serial;break;
 }
}
void ForestAnimals::queueExplosion(const CollisionVec &center,float radius) {
 if(!std::isfinite(radius)||radius<0)throw std::runtime_error("EM23: invalid explosion radius");
 for(auto &a:animals_)if(a.alive()) {
  float length=0;for(unsigned axis=0;axis<3;++axis){const float delta=a.position_[axis]-float(center[axis]);length+=delta*delta;}
  if(length<radius*radius)a.pendingBlast_=true;
 }
}
std::vector<ForestAnimalSound> ForestAnimals::takeSounds() {auto out=std::move(sounds_);sounds_.clear();return out;}
std::vector<ForestAnimalDrop> ForestAnimals::takeDrops() {auto out=std::move(drops_);drops_.clear();return out;}
std::vector<unsigned> ForestAnimals::deadIndices() const {
 std::vector<unsigned> out;for(const auto &a:animals_)if(!a.alive())out.push_back(unsigned(a.source().listIndex));return out;
}
void ForestAnimals::restoreDead(const std::vector<unsigned> &indices) {
 for(auto &a:animals_)if(std::find(indices.begin(),indices.end(),unsigned(a.source().listIndex))!=indices.end()) {
  a.hp_=0;a.savedDead_=true;a.dropEmitted_=true;a.pendingDamage_.reset();a.pendingBlast_=false;
 }
}
std::string ForestAnimals::describe() const {
 std::ostringstream out;out<<animals_.size()<<" original EM23 crows · 24 parts · "<<meshes_[0].triangles<<"/"<<meshes_[1].triangles<<" triangles · "<<ticks_<<" Ticks";return out.str();
}
} // namespace re4
