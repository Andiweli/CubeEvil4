// Frozen objSubWep.cpp::setThrowSpeed/cSubWep, objRocket.cpp and emmine.cpp.
#include "player_projectiles.h"
#include "weapon_preview.h"
#include "ports/rnd.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace re4 {
namespace {
constexpr float pi=3.14159265358979323846f;
struct RandomOwner {std::uint16_t &state,previous;explicit RandomOwner(std::uint16_t &s):state(s),previous(RndState()){RndInit(s);}~RandomOwner(){state=RndState();RndInit(previous);}};
CollisionVec subtract(const CollisionVec &a,const CollisionVec &b){CollisionVec out{};for(unsigned k=0;k<3;++k)out[k]=a[k]-b[k];return out;}
double square(const CollisionVec &a){double out=0;for(auto x:a)out+=x*x;return out;}
Matrix34 placed(const CollisionVec &p,const std::array<float,3> &r){SmdPlacement s;s.position={float(p[0]),float(p[1]),float(p[2])};s.rotation=r;s.scale={1,1,1};return PlacementMatrix(s);}
WorldEffectsData burstBank(const std::vector<std::uint8_t> &core) {
  // Keep the original world sprite records and textures. Scorch decals,
  // screen filters and light actors have their separate renderer owners.
  WorldEffectsData data;data.sprites.signature=1469598103934665603ULL;
  for(auto b:core)data.sprites.signature=(data.sprites.signature^b)*1099511628211ULL;
  data.sprites.sourceSequenceSeeds=true;
  for(unsigned id:{0xbu,0xcu,0xdu,0x26u,0x27u,0x36u}) {
    WaterTarget target;target.sequence=id;
    std::vector<std::vector<std::array<float,4>>> paths;
    for(auto record:ReadOriginalEffectSequence(core,id)) {
      const auto type=record.bytes[1];
      if(type!=0 && type!=8 && type!=11 && type!=16)continue;
      if(record.bytes[7]<0xf8 || record.bytes[264]>1)continue;
      record.bytes[7]=0xff;
      RegisterOriginalEffectAnimation(data.sprites,core,core,record);
      paths.push_back(record.bytes[264]==1 && record.bytes[265]==2
          ?ReadOriginalEffectPath(core,record.bytes[261]):std::vector<std::array<float,4>>{});
      target.records.push_back(record);
    }
    if(target.records.empty())throw std::runtime_error("Projectile: Original burst sprites missing for EST"+std::to_string(id));
    const unsigned index=unsigned(data.sprites.targets.size());
    data.sprites.recordParents.emplace_back(target.records.size());
    data.sprites.recordPaths.push_back(std::move(paths));
    data.sprites.targets.push_back(std::move(target));data.sequences.push_back(id);
    data.targetPrerun.push_back(~0u);data.triggerBanks.push_back({id,id,index,0});
  }
  data.triggerInstanceLimit=32;return data;
}
}
struct PlayerProjectiles::Flight {
  PlayerShot shot;
  CollisionVec position{},speed{};
  std::array<float,3> rotation{},spin{};
  unsigned number{},life{},stuck{},flame{};
  bool finished{};
  std::unique_ptr<AnimationRig> rocket;
};
PlayerProjectiles::PlayerProjectiles(const std::string &path,const std::vector<std::uint8_t> &core)
    :player_(RoomArchive::LoadDrs(path)),effects_(burstBank(core)),simulation_(std::make_unique<WorldEffectsSimulation>(effects_)) {}
PlayerProjectiles::~PlayerProjectiles()=default;
GenericEventActor &PlayerProjectiles::model(unsigned number) {
  auto at=models_.find(number);if(at!=models_.end())return *at->second;
  unsigned bin=number==14?0x72:number==13 || number==28?0x70:0x6a;
  unsigned tpl=number==14?0x73:number==13 || number==28?0x71:number==22?0x6d:number==23?0x6f:0x6b;
  auto result=std::make_unique<GenericEventActor>("Player subweapon","PL_ARC",player_.copy(bin-4),player_.copy(tpl-4));
  result->prepareStaticPose();auto *owner=result.get();models_.emplace(number,std::move(result));return *owner;
}
void PlayerProjectiles::launch(const PlayerShot &shot,const LeonPreview &player,const SatCollision &eat) {
  if(!shot.projectile() || !shot.serial)throw std::runtime_error("Projectile: Invalid launch");
  if(flights_.size()>=32)throw std::runtime_error("Projectile: Original actor pool is full");
  model(shot.weaponNumber());RandomOwner random(random_);
  auto f=std::make_unique<Flight>();f->shot=shot;f->number=shot.weaponNumber();f->position=shot.from;
  if(shot.weaponItemId==0xab) {
    // objMine::setBullet's scoped dart starts beside/below the camera;
    // its direction still converges on the original camera target.
    const auto matrix=player.placement();const std::array<float,3> offset{-100,-100,-300};
    for(unsigned k=0;k<3;++k)for(unsigned j=0;j<3;++j)f->position[k]+=matrix[k*4+j]*offset[j];
  }
  const auto vector=subtract(shot.to,f->position);const double length=std::sqrt(square(vector));
  if(length<=.00001)throw std::runtime_error("Projectile: Zero launch direction");
  for(unsigned k=0;k<3;++k)f->speed[k]=vector[k]*(1500./length);
  f->rotation={float(-std::atan2(vector[1],std::hypot(vector[0],vector[2]))),float(std::atan2(vector[0],vector[2])),0};
  f->life=f->number==13?300:210;
  if(f->number==19 || f->number==22 || f->number==23) {
    f->life=f->number==19?45:300;
    float y=30.000002f,z=283.5f;const float pitch=player.aimRate();
    if(pitch>.1f){y*=pitch+1;z*=pitch+1;}
    else if(pitch<-.2f){y*=pitch*.4f+1;z*=pitch*.4f+1;}
    else {const float c=std::cos(-.2617994f),s=std::sin(-.2617994f);const float old=y;y=c*y-s*z;z=s*old+c*z;}
    const float x=fRand1_1()*15;const float c=std::cos(pitch*-.7853982f),s=std::sin(pitch*-.7853982f);
    const float localY=c*y-s*z,localZ=s*y+c*z;const auto matrix=player.placement();
    for(unsigned k=0;k<3;++k)f->speed[k]=matrix[k*4]*x+matrix[k*4+1]*localY+matrix[k*4+2]*localZ;
    f->rotation={0,player.heading(),0};f->spin={fRand0_1()*.19634955f+.09817477f,0,fRand0_1()*.09817477f+.09817477f};
    if(Rnd()&1){f->spin[0]=-f->spin[0];f->spin[2]=-f->spin[2];}
  } else if(f->number==13) {
    if(!rocketBody_) {rocketBody_=std::make_unique<SkinnedModel>(player_.copy(0x70-4));rocketMotion_={player_.copy(0x74-4),{}};}
    f->rocket=std::make_unique<AnimationRig>(*rocketBody_,std::vector<AnimationBytes>{rocketMotion_});
    f->rocket->startActionFlags(0,0,0,1);
  }
  if(const auto wall=eat.line(player.parts0World(),f->position,SatSubset::All,0,0))
    for(unsigned k=0;k<3;++k)f->position[k]=wall->point[k]+wall->normal[k]*(f->number==28?2000.:500.);
  flights_.push_back(std::move(f));
}
void PlayerProjectiles::burst(Flight &f,const SatCollision &eat) {
  if(f.finished || f.flame)return;
  const unsigned effect=f.number==13?0x27:f.number==14?0x36:f.number==22?0xb:f.number==23?0xc:0xd;
  simulation_->trigger(effect,{float(f.position[0]),float(f.position[1]),float(f.position[2])},{0,0,0},++effectSerial_,&eat);
  cues_.push_back({1,f.number==22?0x22u:f.number==23?0x13u:0x14u,f.position});
  const unsigned weapon=f.number==13?18:f.number==23?23:f.number==22?20:19;
  const float radius=f.number==13?8000.f:f.number==23?15000.f:f.number==22?2500.f:
      f.number==14?(f.shot.powerLevel==0?2000.f:f.shot.powerLevel==1?4000.f:6000.f):6000.f;
  blasts_.push_back({0,0,++blastSerial_,weapon,f.position,radius,1});
  if(f.number==22)f.flame=75;else f.finished=true;
}
void PlayerProjectiles::tick(const SatCollision &eat,const WorldEffectCamera &camera,const Target &target,
                            const std::function<void(const PlayerShot&)> &impact) {
  for(auto &owner:flights_) {
    auto &f=*owner;
    if(f.flame) {
      if(!--f.flame){f.finished=true;continue;}
      blasts_.push_back({0,0,++blastSerial_,20,f.position,2500,1});continue;
    }
    if(f.stuck) {
      if(f.number==14 && (f.stuck==150 || f.stuck%17==0))cues_.push_back({1,5,f.position});
      if(!--f.stuck){if(f.number==14)burst(f,eat);else f.finished=true;}
      continue;
    }
    if(!f.life || !--f.life){if(f.number==28)f.finished=true;else burst(f,eat);continue;}
    const auto previous=f.position;
    if(f.rocket) {
      const auto root=f.rocket->advanceRootAction(1);
      const auto matrix=placed(f.position,f.rotation);
      for(unsigned k=0;k<3;++k)for(unsigned j=0;j<3;++j)f.position[k]+=matrix[k*4+j]*root.translation[j];
    } else {
      if(f.number==19 || f.number==22 || f.number==23)f.speed[1]=float(f.speed[1])-20.f;
      for(unsigned k=0;k<3;++k)f.position[k]=float(f.position[k])+float(f.speed[k]);
    }
    if(f.number==19 || f.number==22 || f.number==23) {
      const auto hit=eat.resolveScenerySphere(previous,f.position,25,0x2001);f.position=hit.position;
      if(hit.contacts) {
        double dot=0;for(unsigned k=0;k<3;++k)dot+=f.speed[k]*hit.normal[k];
        for(unsigned k=0;k<3;++k)f.speed[k]=(f.speed[k]-2*dot*hit.normal[k])*.5;
        f.spin[0]*=-.8f;f.spin[2]*=-.8f;
        if(hit.normal[1]>=0 && hit.normal[1]<.7)f.speed[1]=std::max(50.,f.speed[1]);
        if(hit.normal[1]>.7 && f.number!=19)burst(f,eat);
      }
      for(unsigned k=0;k<3;++k)f.rotation[k]=std::remainder(f.rotation[k]+f.spin[k],2*pi);
      continue;
    }
    auto wall=eat.line(previous,f.position,SatSubset::All,0,f.number==13?0x400000:0x404000);
    const auto actor=target?target(previous,f.position,f.number):std::nullopt;
    const bool actorFirst=actor && (!wall || actor->lenSq<square(subtract(wall->point,previous)));
    if(!actorFirst && !wall)continue;
    const auto contact=actorFirst?CollisionVec{actor->cross[0],actor->cross[1],actor->cross[2]}:wall->point;
    PlayerShot ray=f.shot;ray.from=previous;ray.to=f.position;ray.flightImpact=true;
    f.position=contact;
    if(f.number==13){burst(f,eat);continue;}
    if(actorFirst && impact)impact(ray);
    f.stuck=150;
  }
  flights_.erase(std::remove_if(flights_.begin(),flights_.end(),[](const auto &f){return f->finished;}),flights_.end());
  simulation_->tick(&camera,&eat);
}
void PlayerProjectiles::clear(){flights_.clear();blasts_.clear();cues_.clear();simulation_=std::make_unique<WorldEffectsSimulation>(effects_);}
std::vector<ObjectBlast> PlayerProjectiles::drainBlasts(){std::vector<ObjectBlast> out;out.swap(blasts_);return out;}
std::vector<PlayerProjectiles::Cue> PlayerProjectiles::drainCues(){std::vector<Cue> out;out.swap(cues_);return out;}
std::vector<PlayerProjectiles::Draw> PlayerProjectiles::draws() const {
  std::vector<Draw> out;
  for(const auto &f:flights_)if(!f->flame) {
    const auto *model=models_.at(f->number).get();
    out.push_back({model,placed(f->position,f->rotation),f->rocket?f->rocket.get():model->rig()});
  }
  return out;
}
} // namespace re4
