#include "r100_room_hit_effects.h"
#include "scene.h"
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace re4 {
namespace {
unsigned half(const WaterRecord &r,unsigned p) {return unsigned(r.bytes[p])*256+r.bytes[p+1];}
unsigned word(const WaterRecord &r,unsigned p) {
  return unsigned(r.bytes[p])<<24|unsigned(r.bytes[p+1])<<16|unsigned(r.bytes[p+2])<<8|r.bytes[p+3];
}
float real(const WaterRecord &r,unsigned p) {
  const auto bits=word(r,p);float v;std::memcpy(&v,&bits,4);
  if(!std::isfinite(v))throw std::runtime_error("r100 impact: nonfinite original record");
  return v;
}
unsigned rnd(std::uint16_t &seed) {
  const auto old=seed;const unsigned next=unsigned(std::uint8_t((old>>1)+(old>>8)))<<8|std::uint8_t(old>>1);
  seed=std::uint16_t(next==old?next+0x101:next);return seed>>8;
}
float randomFloat(std::uint16_t &seed) {
  const unsigned a=rnd(seed),b=rnd(seed),c=rnd(seed);
  const std::uint32_t bits=a+(b<<8)+((c&0x7f)<<16)+0x3f800000;
  float v;std::memcpy(&v,&bits,4);return v-1.f;
}
bool commonMove(WaterSprite &s,const WaterRecord &r) {
  if(half(r,180)<=s.age)for(unsigned k=0;k<3;++k) {
    s.position[k]+=s.speed[k];s.speed[k]+=s.acceleration[k];s.speed[k]*=real(r,48);
  }
  if(half(r,182)<=s.age) {
    s.sizeMultiplier+=s.sizeStep;s.sizeStep*=real(r,152);
    if(s.sizeMultiplier<=0)return false;
  }
  for(unsigned k=0;k<3;++k)s.angle[k]+=s.angleSpeed[k];
  const unsigned fade=half(r,176),hold=half(r,178);
  if(fade<s.age) {
    if(fade+hold<=s.age) {
      for(unsigned k=0;k<4;++k)s.color[k]=std::min(255.f,s.color[k]*real(r,160+k*4));
      if(s.color[3]<4)return false;
    }
  } else if(fade) {
    const float rate=float(s.age)/float(fade);
    if(r.bytes[194]==3)for(unsigned k=0;k<3;++k)s.color[k]=float(r.bytes[156+k])*rate;
    s.color[3]=s.startAlpha*rate;
  }
  if(half(r,184) && half(r,184)<=s.age)return false;
  s.age=(s.age+1)&65535u;return true;
}
bool animationMove(WaterSprite &s,const WaterRecord &r,const WaterAnimation &a) {
  s.animationCounter=(s.animationCounter+std::uint8_t(int(std::int8_t(r.bytes[189]))+32))&65535u;
  const auto duration=a.duration.at(s.pattern);
  while((s.animationCounter>>5)>duration) {
    s.animationCounter-=duration<<5;++s.pattern;
    if(s.pattern>=a.pattern.size()) {
      if(!a.loop)return false;
      s.pattern=a.loop==1?0u:unsigned(a.pattern.size()-1);
    }
  }
  return true;
}
EnvironmentData cpuData(const EnvironmentData &source) {
  EnvironmentData out;out.room=source.room;out.signature=source.signature;
  out.targets=source.targets;
  for(const auto &a:source.animations) {
    WaterAnimation m;m.id=a.id;m.width=a.width;m.height=a.height;m.loop=a.loop;
    m.centerX=a.centerX;m.centerY=a.centerY;m.pattern=a.pattern;m.duration=a.duration;
    out.animations.push_back(std::move(m));
  }
  return out;
}
unsigned key(const R100SurfaceEffect &e){return (e.owner<<8)|e.sequence;}
} // namespace
std::optional<R100AtEffectInfo> OriginalR100AtEffectInfo(unsigned type) {
  switch(type) {
  case 2:return R100AtEffectInfo{1,{{{1,0x2c},{1,0x2f},{1,0x2e},{1,0x2d},
      {1,0x20},{1,0x20},{1,0x2b},{1,0x2f}}}};
  case 4:return R100AtEffectInfo{0,{{{0xd2,0},{1,0xf},{0,0xb},{0,0xc},
      {1,0xe},{1,0xe},{0,0x36},{0,0x27}}}};
  case 5: {
    R100AtEffectInfo info;info.flags=2;for(auto &e:info.effects)e={0xd2,0};return info;
  }
  default:return {};
  }
}
std::optional<R100SurfaceEffect> OriginalR100WeaponSurfaceEffect(const R100AtEffectInfo &info,unsigned weapon) {
  if((weapon>=1 && weapon<=12) || weapon==0xf || weapon==0x11 || weapon==0x21)return info.effects[4];
  switch(weapon) {
  case 0x13:return info.effects[1];
  case 0x16:return info.effects[2];
  case 0x17:return info.effects[3];
  case 0xd:return info.effects[7];
  default:return {};
  }
}
R100SurfaceImpactPlan OriginalR100SurfaceImpact(std::uint32_t attr,const std::array<float,3> &n,
                                              unsigned weapon,bool puddle) {
  R100SurfaceImpactPlan plan;
  plan.effectType=((attr&0x800000)?1u:0u)+((attr&0x8000)?2u:0u)+((attr&0x80)?4u:0u);
  const auto info=OriginalR100AtEffectInfo(plan.effectType);plan.flags=info?info->flags:0;
  const float horizontal=std::sqrt(n[0]*n[0]+n[2]*n[2]);
  plan.rotation={plan.flags&1 ? std::atan2(horizontal,n[1]) : -std::atan2(n[1],horizontal),
      std::atan2(n[0],n[2]),0};
  switch(plan.effectType) {
  case 0:
    if(puddle && n[1]>.9f) {plan.effects.push_back({0,0x11});plan.rotation={};plan.sound=0xc;}
    else plan.effects.push_back({0,0x1f});
    break;
  case 1:plan.effects={{0,0x1f},{0,0x20}};break;
  case 3:plan.diagnostic="r100 impact: original EAT_ET_PAD has no effect";break;
  default:
    if(!info) {plan.diagnostic="r100 impact: unregistered source AtEffInfo";break;}
    if(const auto effect=OriginalR100WeaponSurfaceEffect(*info,weapon)) {
      if(effect->owner!=0xd2 && effect->sequence!=1)plan.effects.push_back(*effect);
      if(plan.effectType==2 || (plan.flags&1))plan.sound=0xb;
    } else plan.diagnostic="r100 impact: source getWepEff rejects weapon "+std::to_string(weapon);
    break;
  }
  return plan;
}
WorldEffectsData BuildR100RoomHitEffects(const std::vector<std::uint8_t> &room,
                                       const std::vector<std::uint8_t> &core) {
  WorldEffectsData data;auto &d=data.sprites;d.room=0x100;d.signature=1469598103934665603ULL;
  for(const auto *bytes:{&core,&room})for(auto b:*bytes)d.signature=(d.signature^b)*1099511628211ULL;
  d.sourceSequenceSeeds=true;
  for(const auto effect:{R100SurfaceEffect{0,0x1f},{0,0x20},{0,0x11},{1,0x20},{1,0xe}}) {
    WaterTarget target;target.sequence=effect.sequence;
    target.records=ReadOriginalEffectSequence(effect.owner?room:core,effect.sequence);
    unsigned previous=0;
    for(auto &r:target.records) {
      const auto &b=r.bytes;
      if(b[0]!=1 || (b[1]!=0 && b[1]!=5 && b[1]!=64) || b[3] || b[6] || b[7]!=255 ||
          b[194]>5 || b[195] || b[264]>1 || (b[264] && b[265]) ||
          (word(r,8)&~0x6649fu) || half(r,4)<previous)
        throw std::runtime_error("r100 impact: actual ordinary-gun EST profile changed");
      previous=half(r,4);
      // Actual flutter sprites are generated after the EnvironmentSimulation
      // exists, so SetFreeWork consumes its current shared global Rnd.
      if(b[1]==5 && (half(r,4) || !b[264] || real(r,224)!=0))
        throw std::runtime_error("r100 impact: unsupported Esp05 global RNG scheduling");
      for(unsigned p=12;p<=172;p+=4) {
        if(p==156)continue;
        if(std::abs(real(r,p))>100000)throw std::runtime_error("r100 impact: record parameter budget");
      }
      RegisterOriginalEffectAnimation(d,effect.owner?room:core,core,r);
    }
    const auto index=unsigned(d.targets.size());data.triggerBanks.push_back({key(effect),effect.sequence,index,0});
    data.sequences.push_back(effect.sequence);data.targetPrerun.push_back(~0u);d.targets.push_back(std::move(target));
  }
  return data;
}
struct R100RoomHitEffects::Impl {
  struct Instance {
    EnvironmentData data;
    std::unique_ptr<EnvironmentSimulation> simulation;
    unsigned bank{};std::uint64_t serial{};
  };
  WorldEffectsData data;
  std::vector<std::unique_ptr<Instance>> instances;
  std::vector<std::uint64_t> accepted;
  std::vector<R100SurfaceSound> sounds;
  std::vector<R100UnsupportedSurfaceEffect> unsupported;
  std::vector<std::string> warnings;
  std::uint16_t random=0x4e22;
  std::uint64_t frames{},nextSerial{};
  const EnvironmentData *water{};
  WorldEffectCamera camera;
  mutable WorldEffectsState cached;
  explicit Impl(WorldEffectsData source):data(std::move(source)){}
  void warning(const std::string &text) {
    if(std::find(warnings.begin(),warnings.end(),text)==warnings.end())warnings.push_back(text);
  }
  bool initializer(Instance &p,WaterSprite &s,const WaterRecord &r) {
    if(r.bytes[1]==5) {
      auto &global=p.simulation->visualRandom();
      s.sineOffset[0]=real(r,216);s.sineOffset[1]=real(r,220)*.05f;
      s.sineOffset[0]+=s.sineOffset[0]*randomFloat(global)*(real(r,224)*.1f);
      s.sineOffset[1]+=s.sineOffset[1]*randomFloat(global)*(real(r,224)*.1f);
      s.sinePhase=randomFloat(global)*3.1415927f*2.f;
    } else if(r.bytes[1]==64 && water) {
      if(const auto contact=QueryEnvironmentWaterContact(*water,{s.position[0],s.position[1],s.position[2]},camera.eye))
        s.position[1]=contact->height+real(r,16);
    }
    return true;
  }
  std::optional<bool> mover(Instance &p,WaterSprite &s,const WaterRecord &r,EnvironmentState &state) {
    if(r.bytes[1]!=5)return {};
    if(!commonMove(s,r))return true;
    s.position[0]+=s.sineOffset[0]*motion64::sinf(s.sinePhase);
    s.position[2]+=s.sineOffset[0]*motion64::cosf(s.sinePhase*1.7f);
    s.position[1]+=s.sineOffset[0]*.3f*motion64::cosf(s.sinePhase*1.9f);
    s.sinePhase+=s.sineOffset[1]*(randomFloat(state.random)+.2f);
    return !animationMove(s,r,p.data.animations.at(r.animation));
  }
  void syncRandom(Instance &p) {
    auto s=p.simulation->state();s.random=random;
    if(!p.simulation->restore(s))throw std::runtime_error("r100 impact: internal RNG snapshot rejected");
  }
  unsigned spriteCount() const {
    unsigned n=0;for(const auto &p:instances)n+=unsigned(p->simulation->state().sprites.size());return n;
  }
  void spawn(unsigned bank,const std::array<float,3> &position,const std::array<float,3> &rotation) {
    if(instances.size()>=512) {warning("r100 impact: original effect pool full");return;}
    const auto target=data.triggerBanks.at(bank).target;
    unsigned direct=0;
    for(const auto &record:data.sprites.targets.at(target).records)
      if(!half(record,4) && !record.bytes[264])++direct;
    if(direct>512-spriteCount()) {warning("r100 impact: native shared particle budget exhausted");return;}
    auto p=std::make_unique<Instance>();p->bank=bank;p->serial=++nextSerial;p->data=cpuData(data.sprites);
    for(unsigned t=0;t<p->data.targets.size();++t)if(t!=target)p->data.targets[t].records.clear();
    p->data.sourceSequenceSeeds=true;p->data.initialRandom=random;p->data.transformSpawn=true;
    SmdPlacement placement;placement.position=position;placement.rotation=rotation;placement.scale={1,1,1};
    p->data.spawnMatrix=PlacementMatrix(placement);
    auto *owner=p.get();
    p->data.spriteInitializer=[this,owner](WaterSprite &s,const WaterRecord &r,std::uint32_t &){return initializer(*owner,s,r);};
    p->data.spriteMover=[this,owner](WaterSprite &s,const WaterRecord &r,EnvironmentState &st){return mover(*owner,s,r,st);};
    p->simulation=std::make_unique<EnvironmentSimulation>(p->data);random=p->simulation->state().random;
    instances.push_back(std::move(p));
  }
  const WorldEffectsState &snapshot() const {
    cached={};cached.signature=data.sprites.signature;cached.frames=frames;cached.random=random;
    cached.recordCursor.resize(data.sprites.targets.size());cached.sequenceSeed.resize(data.sprites.targets.size());
    for(const auto &p:instances) {
      const auto &st=p->simulation->state();
      for(auto s:st.sprites){s.serial|=p->serial<<16;cached.serial=std::max(cached.serial,s.serial);cached.sprites.push_back(std::move(s));}
      cached.emitters.insert(cached.emitters.end(),st.emitters.begin(),st.emitters.end());
    }
    return cached;
  }
};
R100RoomHitEffects::R100RoomHitEffects(WorldEffectsData data):impl_(std::make_unique<Impl>(std::move(data))){}
R100RoomHitEffects::~R100RoomHitEffects()=default;
bool R100RoomHitEffects::trigger(const SatHit &hit,unsigned weapon,std::uint64_t serial,const EnvironmentData *water,bool puddle) {
  auto &p=*impl_;
  if(!serial || std::find(p.accepted.begin(),p.accepted.end(),serial)!=p.accepted.end())return false;
  for(const auto &v:{hit.point,hit.normal})for(auto f:v)if(!std::isfinite(f) || std::abs(f)>1e7)return false;
  if(hit.normal==CollisionVec{})return false; // GetWepTargetList2 cleared nrm for an actor hit.
  p.accepted.push_back(serial);if(p.accepted.size()>8192)p.accepted.erase(p.accepted.begin(),p.accepted.begin()+4096);
  const std::array<float,3> position{float(hit.point[0]),float(hit.point[1]),float(hit.point[2])},
      normal{float(hit.normal[0]),float(hit.normal[1]),float(hit.normal[2])};
  p.water=water;
  // PlSetWepTarget suppresses a scenery hit at/below an actual water plane.
  if(water)if(const auto contact=QueryEnvironmentWaterContact(*water,hit.point,p.camera.eye))
    if(float(hit.point[1])<=contact->height) {p.water=nullptr;return true;}
  const auto plan=OriginalR100SurfaceImpact(hit.attributes,normal,weapon,puddle);
  if(!plan.diagnostic.empty())p.warning(plan.diagnostic);
  if(plan.sound)p.sounds.push_back({2,*plan.sound,position});
  for(const auto &effect:plan.effects) {
    const auto found=std::find_if(p.data.triggerBanks.begin(),p.data.triggerBanks.end(),[&](const auto &b){return b.objectId==key(effect);});
    if(found==p.data.triggerBanks.end()) {
      p.unsupported.push_back({effect,position,plan.rotation,serial});
      p.warning("r100 impact: unsupported original EST owner"+std::to_string(effect.owner)+"/"+std::to_string(effect.sequence));
    } else p.spawn(unsigned(found-p.data.triggerBanks.begin()),position,plan.rotation);
  }
  p.water=nullptr;return true;
}
void R100RoomHitEffects::tick(const WorldEffectCamera &camera,const EnvironmentData *water,const SatCollision *floor) {
  auto &p=*impl_;p.camera=camera;p.water=water;EnvironmentCamera view;view.eye=camera.eye;view.up=camera.up;
  unsigned count=p.spriteCount();
  for(auto &instance:p.instances) {
    p.syncRandom(*instance);const auto before=unsigned(instance->simulation->state().sprites.size());
    instance->simulation->tickGenerators(&view,512-(count-before),floor);
    p.random=instance->simulation->state().random;
    count=count-before+unsigned(instance->simulation->state().sprites.size());
  }
  for(auto &instance:p.instances) {
    p.syncRandom(*instance);instance->simulation->tickSprites(&view,floor);p.random=instance->simulation->state().random;
  }
  p.instances.erase(std::remove_if(p.instances.begin(),p.instances.end(),[](const auto &i){
    const auto &s=i->simulation->state();
    return s.sprites.empty() && s.emitters.empty() && s.recordCursor.at(i->bank)==i->data.targets.at(i->bank).records.size();
  }),p.instances.end());
  p.water=nullptr;++p.frames;
}
const WorldEffectsData &R100RoomHitEffects::data() const{return impl_->data;}
const WorldEffectsState &R100RoomHitEffects::state() const{return impl_->snapshot();}
std::vector<R100SurfaceSound> R100RoomHitEffects::drainSounds(){auto result=std::move(impl_->sounds);impl_->sounds.clear();return result;}
std::vector<R100UnsupportedSurfaceEffect> R100RoomHitEffects::drainUnsupported(){auto result=std::move(impl_->unsupported);impl_->unsupported.clear();return result;}
const std::vector<std::string> &R100RoomHitEffects::warnings() const{return impl_->warnings;}
} // namespace re4
