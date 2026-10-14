// Original game/sce_at.cpp::sceAtItemEffSet/sceAtItemDisappearEffSet,
// esp0c.cpp, esp_efm.cpp::EfmSetObj04 and obj04.cpp. CORE records and
// paths remain byte-for-byte original; no sparkle geometry is synthesized.
#include "item_effects.h"
#include "asset_reader.h"
#include "item_rules.h"
#include "ports/rnd.h"
#include "ports/motion64/runtime.h"
#include "room_objects.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <stdexcept>

namespace re4 {
namespace {
unsigned word(const WaterRecord &r,unsigned p) {
  return (unsigned(r.bytes[p])<<24)|(unsigned(r.bytes[p+1])<<16)|
         (unsigned(r.bytes[p+2])<<8)|r.bytes[p+3];
}
unsigned half(const WaterRecord &r,unsigned p) {
  return unsigned(r.bytes[p])*256+r.bytes[p+1];
}
float real(const WaterRecord &r,unsigned p) {
  auto bits=word(r,p);float value;std::memcpy(&value,&bits,4);
  if(!std::isfinite(value))throw std::runtime_error("Item effect: NaN/Inf");
  return value;
}
std::uint64_t hash(const std::vector<std::uint8_t> &bytes) {
  std::uint64_t result=1469598103934665603ULL;
  for(auto x:bytes)result=(result^x)*1099511628211ULL;
  return result;
}
bool finite(const std::array<float,3> &v,float limit=100000) {
  for(auto x:v)if(!std::isfinite(x)||std::abs(x)>limit)return false;
  return true;
}
void checkSource(const ItemEffectSource &s) {
  // Original r106/r11b item positions exceed 100,000 world units. Keep the
  // wider world bound separate from local effect offsets and velocities.
  if(!s.key || s.key>0xffffffffULL || s.id>255 || s.effectType<0 || s.effectType>9 ||
      !finite(s.position,1000000) || !finite(s.offset,10000) || s.parentParts.size()>256)
    throw std::runtime_error("Item effect: invalid original source (item " +
        std::to_string(s.id) + ", effect " + std::to_string(s.effectType) +
        ", position " + std::to_string(s.position[0]) + ", " +
        std::to_string(s.position[1]) + ", " + std::to_string(s.position[2]) +
        ", offset " + std::to_string(s.offset[0]) + ", " +
        std::to_string(s.offset[1]) + ", " + std::to_string(s.offset[2]) + ")");
  for(const auto &m:s.parentParts)for(float x:m)
    if(!std::isfinite(x)||std::abs(x)>1000000)
      throw std::runtime_error("Item effect: invalid model matrix");
  if(s.onModel && s.effectType==1 && s.parentParts.empty())
    throw std::runtime_error("Item effect: Original model part 0 is missing");
}
bool sameSource(const ItemEffectSource &a,const ItemEffectSource &b) {
  return a.key==b.key && a.id==b.id && a.effectType==b.effectType &&
      a.position==b.position && a.offset==b.offset &&
      a.disappearing==b.disappearing && a.onModel==b.onModel &&
      a.parentParts==b.parentParts;
}
bool validEffectSprite(const WaterSprite &s) {
  if(!finite(s.position,1000000)||!finite(s.speed)||!finite(s.acceleration)||
      !finite(s.angle)||!finite(s.angleSpeed)||!finite(s.cameraOffset)||
      !std::isfinite(s.sizeMultiplier)||!std::isfinite(s.sizeStep)||
      !std::isfinite(s.startAlpha)||s.age>65535||s.animationCounter>65535||
      s.parented||!s.history.empty())return false;
  for(float v:s.size)if(!std::isfinite(v)||v<=0||v>100000)return false;
  for(float v:s.scroll)if(!std::isfinite(v))return false;
  for(float v:s.color)if(!std::isfinite(v)||v<0||v>255)return false;
  return true;
}
EnvironmentMatrix placement(const std::array<float,3> &p) {
  return {1,0,0,p[0],0,1,0,p[1],0,0,1,p[2]};
}
void randomVec(std::array<float,3> &out,const WaterRecord &r,unsigned base,unsigned jitter,
               std::uint32_t &seed) {
  for(unsigned k=0;k<3;++k)out[k]=real(r,base+k*4)+real(r,jitter+k*4)*fRandSeed1_1(&seed);
}
WaterSprite sourceSpawn(const WaterRecord &r,std::uint32_t seed,
                        const EnvironmentMatrix &matrix) {
  WaterSprite s;
  // The admitted controllers and Efm04 models have no Tool8/Tool10
  // random flips. Their common source setup consumes exactly16 seed draws.
  randomVec(s.position,r,12,24,seed);randomVec(s.speed,r,36,52,seed);
  randomVec(s.acceleration,r,64,76,seed);randomVec(s.angle,r,88,100,seed);
  randomVec(s.angleSpeed,r,112,124,seed);
  const float randomSize=real(r,144)*fRandSeed1_1(&seed);
  s.flags=word(r,8);s.size={real(r,136)+randomSize,real(r,140)+randomSize};
  s.sizeStep=real(r,148);s.age=half(r,186);s.startAlpha=r.bytes[159];
  for(unsigned k=0;k<4;++k)s.color[k]=r.bytes[156+k];
  for(unsigned k=0;k<3;++k) {
    s.angle[k]*=3.14f/180.f;s.angleSpeed[k]*=3.14f/180.f;
  }
  ApplyEnvironmentSpriteMatrix(s,matrix);
  return s;
}
bool moveModel(WaterSprite &s,const WaterRecord &r) {
  // The real CORE0x25 glow has no bounce/motion flags. This is the
  // corresponding literal cObj04 envelope and operation order.
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
  }else if(fade)s.color[3]=s.startAlpha*float(s.age)/float(fade);
  if(half(r,184)&&half(r,184)<=s.age)return false;
  ++s.age;return true;
}
std::array<float,3> pathPosition(const std::vector<std::array<float,4>> &path,float distance) {
  if(distance<0 || distance>=path.back()[3])return {};
  unsigned index=1;while(index+1<path.size() && path[index][3]<distance)++index;
  const auto &a=path[index-1],&b=path[index];
  const float rate=(distance-a[3])/(b[3]-a[3]);
  std::array<float,3> out{};
  for(unsigned k=0;k<3;++k)out[k]=(b[k]-a[k])*rate+a[k];
  return out;
}
WaterSprite sourcePathSpawn(const WaterRecord &r,std::uint32_t seed,
    const EnvironmentMatrix &parent,const std::vector<std::array<float,4>> &path) {
  if(path.size()<2)throw std::runtime_error("Item effect: Original path is missing");
  float t=(float(r.bytes[262])+fRandSeed0_1(&seed)*float(r.bytes[263]))*.01f;
  while(t>1)t-=1;
  while(t<0)t+=1;
  float distance=path.back()[3]*t;
  if(distance<0)distance=0;
  if(distance>path.back()[3]-1.01f)distance=path.back()[3]-1.01f;
  const auto point=pathPosition(path,distance),next=pathPosition(path,distance+1);
  motion64::Vec pos{point[0],point[1],point[2]},dir{next[0]-point[0],next[1]-point[1],next[2]-point[2]},
      up{0,1,0},right{},rotation{float(r.bytes[297])*3.1415927f*2.f*.00390625f,
      float(r.bytes[298])*3.1415927f*2.f*.00390625f,0};
  motion64::VECNormalize(&dir,&dir);motion64::PSVECCrossProduct(&dir,&up,&right);
  motion64::PSVECCrossProduct(&right,&dir,&up);
  motion64::Mtx orient{{right.x,right.y,right.z,0},{up.x,up.y,up.z,0},{dir.x,dir.y,dir.z,0}},
      base,combined,rot,scaled;
  std::memcpy(base,parent.data(),sizeof base);
  for(unsigned k=0;k<3;++k)base[k][3]-=real(r,12+k*4)+parent[k*4+3];
  motion64::PSMTXScale(scaled,1+real(r,280)*.1f,1+real(r,284)*.1f,1+real(r,288)*.1f);
  motion64::PSMTXMultVec(scaled,&pos,&pos);motion64::RotMatrix(rot,&rotation);
  motion64::PSMTXMultVec(rot,&pos,&pos);
  motion64::PSMTXConcat(orient,base,combined);motion64::PSMTXConcat(rot,combined,combined);
  const float coordinates[]{pos.x,pos.y,pos.z};
  for(unsigned k=0;k<3;++k)combined[k][3]+=real(r,12+k*4)+parent[k*4+3]+coordinates[k];
  EnvironmentMatrix matrix;std::memcpy(matrix.data(),combined,sizeof combined);
  return sourceSpawn(r,seed,matrix);
}
bool movePathSprite(WaterSprite &s,const WaterRecord &r,const WaterAnimation &a) {
  if(!moveModel(s,r))return false;
  s.animationCounter=(s.animationCounter+unsigned(std::uint8_t(
      int(static_cast<std::int8_t>(r.bytes[189]))+32)))&65535;
  const unsigned duration=a.duration.at(s.pattern);
  while((s.animationCounter>>5)>duration) {
    ++s.pattern;s.animationCounter-=duration<<5;
    if(s.pattern>=a.pattern.size()) {
      if(!a.loop)return false;
      s.pattern=a.loop==1?0:unsigned(a.pattern.size()-1);
    }
  }return true;
}
struct ModelReader {
  const std::vector<std::uint8_t> &bytes;AssetReader r;
  explicit ModelReader(const std::vector<std::uint8_t> &b):bytes(b),r{b} {
    r.range(0,48);if(b.size()>16u*1024*1024 || r.u32(0)!=11)
      throw std::runtime_error("Item-EFM: EFF version/size");
  }
  std::size_t table(unsigned field,unsigned stride) const {
    const auto p=r.u32(field);
    if(p<48 || r.u32(p)>4096)throw std::runtime_error("Item EFM: Table");
    r.range(p+4,std::size_t(r.u32(p))*stride);return p;
  }
  std::size_t entry(std::size_t t,unsigned i) const {
    const auto n=r.u32(t);if(i>=n)throw std::runtime_error("Item-EFM: Index");
    const auto d=r.u32(t+4+i*4);
    if(d<4+n*4 || d>bytes.size()-t)throw std::runtime_error("Item-EFM: Offset");
    r.range(t+d,20);return t+d;
  }
  std::size_t end(std::size_t p) const {
    auto result=bytes.size();
    for(unsigned field=4;field<=44;field+=4) {
      if(!r.u32(field))continue;
      const auto t=table(field,field<24?8:4);
      if(t>p)result=std::min(result,t);
      if(field<24)continue;
      for(unsigned i=0;i<r.u32(t);++i) {
        const auto q=t+r.u32(t+4+i*4);
        if(q<t || q>bytes.size())throw std::runtime_error("Item EFM: Range");
        if(q>p)result=std::min(result,q);
      }
    }return result;
  }
  std::vector<std::uint8_t> sub(std::size_t e,unsigned field) const {
    const auto d=r.u32(e+field);
    if(d<20 || d>bytes.size()-e)throw std::runtime_error("Item EFM: Model file");
    const auto start=e+d;auto finish=end(e);
    for(unsigned f=4;f<=16;f+=4) {
      const auto next=r.u32(e+f);if(!next)continue;
      if(next<20 || next>bytes.size()-e)throw std::runtime_error("Item-EFM: Subfile");
      if(e+next>start)finish=std::min(finish,e+next);
    }
    if(finish<=start)throw std::runtime_error("Item EFM: Model range");
    return {bytes.begin()+start,bytes.begin()+finish};
  }
};
} // namespace
unsigned OriginalItemEffectType(unsigned id) {
  if(id==0x8c)return 8;
  switch(OriginalItemInfo(id).type) {
  case 1:case 2:case 3:case 4:return 5;
  case 6:return 4;
  case 0:case 5:case 7:return 2;
  default:return 3;
  }
}
unsigned ResolveOriginalItemEffectType(unsigned id,int effect,bool runtime) {
  if(effect==6 || (runtime&&(effect==-1 || effect==0)))return OriginalItemEffectType(id);
  if(effect<0 || effect>9)throw std::runtime_error("Item effect: Original type");
  return unsigned(effect);
}
std::vector<ItemEffectRequest> OriginalItemEffectRequests(const ItemEffectSource &s,bool shootingMode) {
  checkSource(s);if(shootingMode || s.effectType==0)return {};
  auto p=s.position;for(unsigned k=0;k<3;++k)p[k]+=s.offset[k];
  if(s.disappearing && s.onModel) {
    // Preserve the actual frozen code's pModel.pos.x for all three axes;
    // r100's runtime disappear branch passes a null model.
    p={s.position[0]+s.offset[0],s.position[0]+s.offset[1],s.position[0]+s.offset[2]};
  }
  std::vector<ItemEffectRequest> out;
  auto add=[&](unsigned id,bool attached=false){out.push_back({id,p,attached});};
  if(s.disappearing) {
    switch(s.effectType) {
    case 3:add(0x2e);break;case 5:add(0x30);break;case 4:add(0x32);break;
    case 2:add(0x34);break;case 7:add(0x47);break;
    case 8:p[1]-=800;add(0x34);break;default:break;
    }
  }else switch(s.effectType) {
    case 1:if(s.onModel){p=s.offset;add(0x2d,true);}else add(0x21);break;
    case 3:add(0x2c);break;case 5:add(0x2f);break;case 4:add(0x31);break;
    case 2:add(0x33);add(0x21);break;case 7:add(0x46);add(0x21);break;
    case 8:p[1]-=800;add(0x33);p[1]+=800;add(0x21);break;
    case 9:add(0x4d);break;case 6:break;
  }
  return out;
}
WorldEffectsData BuildCoreItemWorldEffects(const std::vector<std::uint8_t> &core) {
  WorldEffectsData data;data.sprites.signature=hash(core)^0x4954454d454646ULL;
  data.sprites.sourceSequenceSeeds=true;
  for(unsigned sequence:{0x21u,0x24u,0x25u,0x2cu,0x2du,0x2eu,0x2fu,0x30u,
      0x31u,0x32u,0x33u,0x34u,0x46u,0x47u,0x4du}) {
    WaterTarget target;target.sequence=sequence;
    target.records=ReadOriginalEffectSequence(core,sequence);
    std::vector<std::vector<std::array<float,4>>> paths(target.records.size());
    unsigned previous=0;
    for(unsigned index=0;index<target.records.size();++index) {
      auto &r=target.records[index];const auto &b=r.bytes;
      if(half(r,4)<previous)throw std::runtime_error("Item effect: EST order");
      previous=half(r,4);
      if(b[0]!=1 || (b[1]!=0 && b[1]!=11 && b[1]!=12 && b[1]!=16 && b[1]!=255) ||
          b[3] || b[6] || (b[7]!=0 && b[7]!=255) || b[194]>5 || b[195] ||
          (word(r,8)&~0x280041u) || b[264]>1 ||
          (b[264]&&(b[265]!=0&&b[265]!=2)) || b[266] || (b[267]&~3u) ||
          b[269] || half(r,272)>600 || b[296]>32)
        throw std::runtime_error("Item effect: unsupported original profile");
      for(unsigned p=12;p<=172;p+=4) {
        if(p==156)continue;
        if(std::abs(real(r,p))>100000)throw std::runtime_error("Item effect: Parameter budget");
      }
      if(real(r,136)<=0 || real(r,140)<=0 || real(r,136)>20000 || real(r,140)>20000 ||
          real(r,48)<0 || real(r,48)>1 || real(r,152)<0 || real(r,152)>1)
        throw std::runtime_error("Item effect: Scale budget");
      if(b[1]==255) {
        if(b[2]!=0x25 || b[7]!=255 || word(r,8)!=0x200040 || b[264])
          throw std::runtime_error("Item effect: non-original Efm04 model");
        r.externalSeedDraws=16;continue;
      }
      if(b[1]==12) {
        if(sequence!=0x21 || b[7]!=255 || word(r,8) || b[200] ||
            (b[201]!=0x24 && b[201]!=0x25) || b[202] || b[203] ||
            real(r,144)!=0 || real(r,136)!=200 || real(r,140)!=200)
          throw std::runtime_error("Item effect: non-original Esp0c controller");
        r.externalSeedDraws=16;continue;
      }
      if(b[265]==2) {
        if(b[296] || b[299]!=1 || b[7]!=255 || b[267])
          throw std::runtime_error("Item effect: Espgen02 profile");
        paths[index]=ReadOriginalEffectPath(core,b[261]);
        // Espgen02 mode1 orients the source spark along its path before
        // CommonMove. Preserve its extra path RNG draw and actual matrix.
        r.externalSeedDraws=17;
      }
      RegisterOriginalEffectAnimation(data.sprites,core,core,r);
    }
    data.triggerBanks.push_back({sequence,sequence,unsigned(data.sprites.targets.size()),0});
    data.sprites.recordParents.emplace_back(target.records.size());
    data.sprites.recordEmitMatrices.emplace_back(target.records.size());
    data.sprites.recordPaths.push_back(std::move(paths));
    data.sprites.targets.push_back(std::move(target));
    data.sequences.push_back(sequence);data.targetPrerun.push_back(~0u);
  }
  return data;
}
std::map<unsigned,StaticScene> BuildCoreItemEffectModels(const std::vector<std::uint8_t> &core) {
  ModelReader e(core);const auto ids=e.table(20,8),offsets=e.table(44,4);
  if(e.r.u32(ids)!=e.r.u32(offsets))throw std::runtime_error("Item EFM: Model count");
  for(unsigned i=0;i<e.r.u32(ids);++i)if(e.r.u16(ids+4+i*8)==0x25) {
    const auto p=e.entry(offsets,i);auto mesh=BuildRoomModelScene(e.sub(p,4),e.sub(p,8));
    if(mesh.vertices.empty() || mesh.fallbackTriangles || mesh.unresolvedAlphaTriangles)
      throw std::runtime_error("Item-EFM: Incomplete original model");
    std::map<unsigned,StaticScene> out;out.emplace(0x25,std::move(mesh));return out;
  }
  throw std::runtime_error("Item-EFM: CORE model 0x25 is missing");
}
struct ItemEffects::Impl {
  WorldEffectsData data;
  std::map<unsigned,StaticScene> meshes;
  struct Instance {
    std::uint64_t owner{},serial{};unsigned sequence{};
    std::array<float,3> position{};
    std::vector<EnvironmentMatrix> parentParts;
    EnvironmentData data;
    std::unique_ptr<EnvironmentSimulation> simulation;
    std::vector<ItemEffectsState::Instance::Model> models;
    std::vector<WaterSprite> pathSprites;
    std::vector<EnvironmentExternalSpawn> pending;
  };
  std::vector<std::unique_ptr<Instance>> instances;
  std::vector<ItemEffectSource> sources;
  std::uint64_t ticks{},serial{};
  std::uint16_t random{0x4e22};
  explicit Impl(const std::vector<std::uint8_t> &core)
      :data(BuildCoreItemWorldEffects(core)),meshes(BuildCoreItemEffectModels(core)) {}
  std::unique_ptr<Instance> create(std::uint64_t owner,const ItemEffectRequest &request,
      const std::vector<EnvironmentMatrix> &parts,const SatCollision *floor,
      std::optional<std::uint64_t> savedSerial={}) {
    if(instances.size()>=512 || (!savedSerial && serial>=0xffffffffULL))
      throw std::runtime_error("Item effect: Instance budget");
    auto result=std::make_unique<Instance>();result->owner=owner;
    result->serial=savedSerial?*savedSerial:++serial;result->sequence=request.sequence;
    result->position=request.position;result->parentParts=parts;result->data=data.sprites;
    auto &d=result->data;
    // Rendering textures remain in immutable data, rather than being copied
    // into every independently clocked original EST instance.
    for(auto &a:d.animations)a.images.clear();
    const auto bank=std::find_if(data.triggerBanks.begin(),data.triggerBanks.end(),
        [&](const auto &x){return x.objectId==request.sequence;});
    if(bank==data.triggerBanks.end())throw std::runtime_error("Item effect: Original EST is missing");
    for(unsigned t=0;t<d.targets.size();++t)if(t!=bank->target)d.targets[t].records.clear();
    d.initialRandom=random;d.transformSpawn=true;d.spawnMatrix=placement(request.position);
    for(unsigned j=0;j<d.targets[bank->target].records.size();++j) {
      const auto &r=d.targets[bank->target].records[j];
      if(r.bytes[7]<0xf8) {
        if(!request.attached || r.bytes[7]>=parts.size())
          throw std::runtime_error("Item effect: Original model part is missing");
        auto matrix=parts[r.bytes[7]];
        const auto p=TransformPoint(matrix,request.position);
        matrix[3]=p[0];matrix[7]=p[1];matrix[11]=p[2];
        d.recordParents[bank->target][j]=matrix;
      }
    }
    result->simulation=std::make_unique<EnvironmentSimulation>(d,floor);
    random=result->simulation->state().random;
    drain(*result);return result;
  }
  void drain(Instance &instance) {
    for(const auto &spawn:instance.simulation->drainExternalSpawns()) {
      const auto &r=instance.data.targets[spawn.target].records[spawn.record];
      if(r.bytes[1]==12)instance.pending.push_back(spawn);
      else if(r.bytes[1]==255) {
        auto s=sourceSpawn(r,spawn.seed,instance.data.spawnMatrix);
        s.size[0]*=.005f;s.size[1]*=.005f;
        s.target=spawn.target;s.record=spawn.record;
        s.serial=(instance.serial<<32)|spawn.serial;
        instance.models.push_back({spawn.target,spawn.record,s.serial,s});
      }else if(r.bytes[1]==0 && r.bytes[265]==2) {
        auto s=sourcePathSpawn(r,spawn.seed,instance.data.spawnMatrix,
            instance.data.recordPaths[spawn.target][spawn.record]);
        s.target=spawn.target;s.record=spawn.record;
        s.serial=(instance.serial<<32)|spawn.serial;
        s.pattern=r.bytes[188];s.animationCounter=half(r,190);
        instance.pathSprites.push_back(std::move(s));
      }else throw std::runtime_error("Item effect: Foreign external generator");
    }
  }
};
ItemEffects::ItemEffects(const std::vector<std::uint8_t> &core):impl_(std::make_unique<Impl>(core)){}
ItemEffects::~ItemEffects()=default;
const WorldEffectsData &ItemEffects::data() const {return impl_->data;}
const std::map<unsigned,StaticScene> &ItemEffects::modelMeshes() const {return impl_->meshes;}
void ItemEffects::sync(const std::vector<ItemEffectSource> &input,const SatCollision *floor,bool shootingMode) {
  if(input.size()>128)throw std::runtime_error("Item effect: Source budget");
  std::set<std::uint64_t> keys;
  for(const auto &s:input) {checkSource(s);if(!keys.insert(s.key).second)
    throw std::runtime_error("Item effect: Duplicate owner");}
  auto sources=shootingMode?std::vector<ItemEffectSource>{}:input;
  for(auto it=impl_->sources.begin();it!=impl_->sources.end();++it) {
    const auto fresh=std::find_if(sources.begin(),sources.end(),[&](const auto &s){return s.key==it->key;});
    if(fresh==sources.end() || !sameSource(*it,*fresh))
      impl_->instances.erase(std::remove_if(impl_->instances.begin(),impl_->instances.end(),
          [&](const auto &x){return x->owner==it->key;}),impl_->instances.end());
  }
  for(const auto &source:sources) {
    const auto old=std::find_if(impl_->sources.begin(),impl_->sources.end(),
        [&](const auto &x){return sameSource(x,source);});
    if(old!=impl_->sources.end())continue;
    for(const auto &request:OriginalItemEffectRequests(source))
      impl_->instances.push_back(impl_->create(source.key,request,source.parentParts,floor));
  }
  impl_->sources=std::move(sources);
}
void ItemEffects::tick(const WorldEffectCamera *camera,const SatCollision *floor) {
  EnvironmentCamera view;if(camera){view.eye=camera->eye;view.up=camera->up;}
  std::vector<std::unique_ptr<Impl::Instance>> children;
  for(auto &instance:impl_->instances) {
    // Esp0c draws nothing. Its first move calls the child's original EstSet.
    for(const auto &spawn:instance->pending) {
      const auto &r=instance->data.targets[spawn.target].records[spawn.record];
      const auto s=sourceSpawn(r,spawn.seed,instance->data.spawnMatrix);
      if(s.size[0]!=200 || s.size[1]!=200 || s.color!=std::array<float,4>{255,255,255,255} ||
          s.speed!=std::array<float,3>{})
        throw std::runtime_error("Item effect: unsupported original Esp0c control parameter");
      children.push_back(impl_->create(instance->owner,{r.bytes[201],s.position,false},{},floor));
    }
    instance->pending.clear();
    auto state=instance->simulation->state();state.random=impl_->random;
    if(!instance->simulation->restore(state))throw std::runtime_error("Item effect: RNG state");
    instance->simulation->tick(camera?&view:nullptr,512,floor);
    impl_->random=instance->simulation->state().random;impl_->drain(*instance);
    instance->models.erase(std::remove_if(instance->models.begin(),instance->models.end(),[&](auto &m){
      return !moveModel(m.state,instance->data.targets[m.target].records[m.record]);
    }),instance->models.end());
    instance->pathSprites.erase(std::remove_if(instance->pathSprites.begin(),instance->pathSprites.end(),[&](auto &s){
      const auto &r=instance->data.targets[s.target].records[s.record];
      return !movePathSprite(s,r,instance->data.animations[r.animation]);
    }),instance->pathSprites.end());
  }
  for(auto &child:children)impl_->instances.push_back(std::move(child));
  impl_->instances.erase(std::remove_if(impl_->instances.begin(),impl_->instances.end(),[&](const auto &x){
    const auto &s=x->simulation->state();
    if(!s.sprites.empty() || !s.emitters.empty() || !x->models.empty() || !x->pathSprites.empty() || !x->pending.empty())return false;
    for(unsigned t=0;t<x->data.targets.size();++t)
      if(s.recordCursor[t]!=x->data.targets[t].records.size())return false;
    return true;
  }),impl_->instances.end());
  ++impl_->ticks;
}
std::vector<WorldEffectQuad> ItemEffects::quads(const WorldEffectCamera &camera) const {
  std::vector<WorldEffectQuad> out;
  for(const auto &instance:impl_->instances) {
    WorldEffectsState state;static_cast<EnvironmentState&>(state)=instance->simulation->state();
    for(auto &s:state.sprites) {
      if(s.parented) {
        ApplyEnvironmentSpriteMatrix(s,*instance->data.recordParents[s.target][s.record]);s.parented=false;
      }
      s.serial=(instance->serial<<32)|s.serial;
    }
    state.sprites.insert(state.sprites.end(),instance->pathSprites.begin(),instance->pathSprites.end());
    auto next=WorldEffectQuads(impl_->data,state,camera);
    out.insert(out.end(),next.begin(),next.end());
  }
  std::stable_sort(out.begin(),out.end(),[](const auto &a,const auto &b){return a.depth>b.depth;});
  return out;
}
std::vector<ItemEffectModel> ItemEffects::effectModels() const {
  std::vector<ItemEffectModel> out;
  for(const auto &instance:impl_->instances)for(const auto &m:instance->models) {
    const auto &r=instance->data.targets[m.target].records[m.record];const auto &s=m.state;
    ItemEffectModel outModel;auto &model=outModel.model;
    model.model=r.bytes[2];model.target=m.target;model.record=m.record;model.serial=m.serial;
    model.lightMask=16;outModel.blend=r.bytes[194];outModel.zMode=(word(r,8)&0x200000)?1:0;
    motion64::Mtx matrix;motion64::Vec angle{s.angle[0],s.angle[1],s.angle[2]},
        pos{s.position[0],s.position[1],s.position[2]},
        scale{s.size[0]*s.sizeMultiplier,s.size[1]*s.sizeMultiplier,s.size[0]*s.sizeMultiplier};
    motion64::RotMatrix(matrix,&angle);motion64::TransMatrix(matrix,&pos);motion64::ScaleMatrix(matrix,&scale);
    std::memcpy(model.matrix.data(),matrix,sizeof matrix);
    for(unsigned k=0;k<3;++k)model.color[k]=float(std::clamp(int(s.color[k]),0,255))/255.f;
    model.color[3]=s.age==0&&half(r,176)?0:std::clamp(s.color[3],0.f,255.f)/255.f;
    out.push_back(outModel);
  }
  return out;
}
ItemEffectsState ItemEffects::snapshot() const {
  ItemEffectsState state;state.signature=impl_->data.sprites.signature;state.ticks=impl_->ticks;
  state.serial=impl_->serial;state.random=impl_->random;state.sources=impl_->sources;
  for(const auto &x:impl_->instances)
    state.instances.push_back({x->owner,x->serial,x->sequence,x->position,x->parentParts,
        x->simulation->state(),x->models,x->pathSprites,x->pending});
  return state;
}
bool ItemEffects::restore(const ItemEffectsState &state,const SatCollision *floor) {
  if(state.signature!=impl_->data.sprites.signature || state.sources.size()>128 ||
      state.instances.size()>512 || state.serial>0xffffffffULL)return false;
  // Creating the replacement instances consumes the private visual RNG.
  // Failed validation must leave the current owner, clock and RNG intact.
  struct RestoreRandom {
    std::uint16_t &random;
    std::uint16_t previous;
    bool committed{};
    ~RestoreRandom(){if(!committed)random=previous;}
  } rollback{impl_->random,impl_->random};
  try {
    std::set<std::uint64_t> keys,serials;
    for(const auto &s:state.sources) {checkSource(s);if(!keys.insert(s.key).second)return false;}
    std::vector<std::unique_ptr<Impl::Instance>> restored;
    for(const auto &x:state.instances) {
      if(!keys.count(x.owner) || !x.serial || x.serial>state.serial ||
          !serials.insert(x.serial).second || !finite(x.position,1000000) || x.models.size()>512 || x.pathSprites.size()>512 || x.pending.size()>512)
        return false;
      auto instance=impl_->create(x.owner,{x.sequence,x.position,!x.parentParts.empty()},x.parentParts,floor,x.serial);
      if(!instance->simulation->restore(x.state))return false;
      for(const auto &m:x.models) {
        if(m.target>=instance->data.targets.size() ||
            m.record>=instance->data.targets[m.target].records.size() ||
            instance->data.targets[m.target].records[m.record].bytes[1]!=255 ||
            m.state.target!=m.target || m.state.record!=m.record ||
            m.serial!=m.state.serial || (m.serial>>32)!=x.serial || !validEffectSprite(m.state))return false;
      }
      for(const auto &p:x.pending)
        if(p.target>=instance->data.targets.size() ||
            p.record>=instance->data.targets[p.target].records.size() ||
            instance->data.targets[p.target].records[p.record].bytes[1]!=12 ||
            !p.serial || p.serial>x.state.externalSerial)return false;
      for(const auto &s:x.pathSprites) {
        if(s.target>=instance->data.targets.size() ||
            s.record>=instance->data.targets[s.target].records.size() ||
            instance->data.targets[s.target].records[s.record].externalSeedDraws!=17 ||
            (s.serial>>32)!=x.serial || !validEffectSprite(s))return false;
        const auto &record=instance->data.targets[s.target].records[s.record];
        if(record.animation>=instance->data.animations.size() ||
            s.pattern>=instance->data.animations[record.animation].pattern.size())return false;
      }
      instance->models=x.models;instance->pathSprites=x.pathSprites;instance->pending=x.pending;restored.push_back(std::move(instance));
    }
    auto sources=state.sources;
    impl_->instances=std::move(restored);impl_->sources=std::move(sources);
    impl_->serial=state.serial;impl_->ticks=state.ticks;impl_->random=state.random;
    rollback.committed=true;return true;
  }catch(const std::exception &) {return false;}
}
std::uint64_t ItemEffects::ticks() const {return impl_->ticks;}
std::size_t ItemEffects::sourceCount() const {return impl_->sources.size();}
std::size_t ItemEffects::instanceCount() const {return impl_->instances.size();}
} // namespace re4
