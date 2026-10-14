// st1/r101::r101_TowerAttack, sub2::CalcParabolaVector and game/obj01.
#include "village_bottles.h"
#include "archive.h"
#include "ports/rnd.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace re4 {
namespace {
struct RandomOwner {std::uint16_t &value,previous;explicit RandomOwner(std::uint16_t &s):value(s),previous(RndState()){RndInit(s);}~RandomOwner(){value=RndState();RndInit(previous);}};
WorldEffectsData bank(const std::vector<std::uint8_t> &eff,const std::vector<std::uint8_t> &core,const std::vector<std::uint8_t> &room,const std::vector<unsigned> &ids) {
 auto result=BuildEventWorldEffects(eff,core,room,ids);
 for(unsigned i=0;i<result.sequences.size();++i){result.triggerBanks.push_back({result.sequences[i],result.sequences[i],i,0});result.targetPrerun[i]=~0u;}
 result.triggerInstanceLimit=32;return result;
}
Matrix34 matrix(const CollisionVec &p,float angle) {SmdPlacement placement;placement.position={float(p[0]),float(p[1]),float(p[2])};placement.rotation={angle,0,0};placement.scale={1,1,1};return PlacementMatrix(placement);}
}
VillageBottles::VillageBottles(const RoomArchive &room,const std::vector<std::uint8_t> &core) {
 const auto *eff=room.find("EFF");if(!eff)throw std::runtime_error("r101: Bottle effects missing");const auto roomEff=room.copy(eff-room.entries().data());
 model_=std::make_unique<GenericEventActor>("r101-bottle","ROOM_ARC25",room.copy(0x25-4),room.copy(0x26-4));model_->prepareStaticPose();
 core_=bank(core,core,roomEff,{0x11,0x12});trail_=bank(roomEff,core,roomEff,{0x10});
 coreSimulation_=std::make_unique<WorldEffectsSimulation>(core_);trailSimulation_=std::make_unique<WorldEffectsSimulation>(trail_);
}
void VillageBottles::throwAt(const DoorArea &a,const SatCollision &floor) {
 if(bottles_.size()>=32)return;
 RandomOwner random(random_);
 // AreaGetInsidePos's two original fRand0_1 draws and F32 bilinear quad.
 const float s=fRand0_1(),t=fRand0_1();CollisionVec target{};
 for(unsigned k=0;k<2;++k){const float p0=float(a.points[0][k]),p1=float(a.points[1][k]),p2=float(a.points[2][k]),p3=float(a.points[3][k]);const float v0=(p1-p0)*s;const float v3=(p2-p3)*s+p3-p0;target[k?2:0]=(v3-v0)*t+v0+p0;}target[1]=float(a.floor);
 Bottle b;b.pos={15307,2800,-1931};const float apex=std::max(float(b.pos[1]),float(target[1]))+2000.f;
 const float time=std::sqrt((apex-float(b.pos[1]))*.1f)+std::sqrt((apex-float(target[1]))*.1f);
 for(unsigned k=0;k<3;++k)b.speed[k]=(float(target[k])-float(b.pos[k]))/time;
 b.speed[1]=std::sqrt((apex-float(b.pos[1]))*40.f);b.spin=-(fRand0_1()*.049087387f+.19634955f);b.serial=++serial_;++throws_;
 const std::vector<EnvironmentMatrix> parts{matrix(b.pos,b.angle)};
 if(!trailSimulation_->triggerAttached(0x10,{0,0,0},{0,0,0},b.serial,parts,&floor))throw std::runtime_error("r101: Bottle trail could not start");
 bottles_.push_back(b);
}
void VillageBottles::explode(Bottle &b,const SatCollision &floor) {
 if(b.flame)return;
 b.flame=90;explosion_=true;++explosions_;trailSimulation_->stopInstance(b.serial);
 for(unsigned id:{0x12u,0x11u})if(!coreSimulation_->trigger(id,{float(b.pos[0]),float(b.pos[1]),float(b.pos[2])},{0,0,0},++serial_,&floor))throw std::runtime_error("r101: Bottle burst could not start");
}
void VillageBottles::tick(LeonPreview &player,const SatCollision &eat,const WorldEffectCamera &camera) {
 for(auto &b:bottles_) {
  if(b.flame) {
   --b.flame;
   const auto part=TransformPoint(player.placement(),TransformPoint(player.rig().jointMatrices().at(0),{0,0,0}));
   const float dx=part[0]-float(b.pos[0]),dz=part[2]-float(b.pos[2]),dy=part[1]-float(b.pos[1]);
   if(dx*dx+dz*dz<2250000.f && dy>=0 && dy<3000)player.receiveContinuousFlame();
   if(!b.flame)b.timer=0;
   continue;
  }
  if(!b.timer){explode(b,eat);continue;}--b.timer;const auto previous=b.pos;b.speed[1]=float(b.speed[1])-20.f;
  for(unsigned k=0;k<3;++k)b.pos[k]=float(b.pos[k])+float(b.speed[k]);
  const auto hit=eat.resolveScenerySphere(previous,b.pos,25,0x2001);b.pos=hit.position;
  if(hit.contacts) {
   double dot=0;for(unsigned k=0;k<3;++k)dot+=b.speed[k]*hit.normal[k];
   for(unsigned k=0;k<3;++k)b.speed[k]=(b.speed[k]-2*dot*hit.normal[k])*.2;
   b.spin*= -.8f;if(hit.normal[1]>.9)explode(b,eat);
  }
  b.angle=std::remainder(b.angle+b.spin,6.283185307179586f);
  trailSimulation_->updateInstanceParts(b.serial,{matrix(b.pos,b.angle)});
 }
 bottles_.erase(std::remove_if(bottles_.begin(),bottles_.end(),[](const auto &b){return !b.timer && !b.flame;}),bottles_.end());
 coreSimulation_->tick(&camera,&eat);trailSimulation_->tick(&camera,&eat);
}
std::vector<Matrix34> VillageBottles::placements() const {std::vector<Matrix34> result;for(const auto &b:bottles_)if(!b.flame && b.timer)result.push_back(matrix(b.pos,b.angle));return result;}
}
