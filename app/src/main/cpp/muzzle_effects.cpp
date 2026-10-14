#include "muzzle_effects.h"
#include "asset_reader.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace re4 {
struct MuzzleEffects::Instance {
  unsigned serial{},age{};
  WorldEffectsData data;
  std::unique_ptr<EnvironmentSimulation> simulation;
  Matrix34 parent{1,0,0,0,0,1,0,0,0,0,1,0};
  Instance(const WorldEffectsData &source,unsigned event):serial(event),data(source){}
};
MuzzleEffects::MuzzleEffects(const WorldEffectsData &data):data_(data) {
  if(data.sprites.targets.size()!=1 || data.targetPrerun!=std::vector<unsigned>{0})
    throw std::runtime_error("Muzzle flash: Bank profile");
}
MuzzleEffects::~MuzzleEffects()=default;
bool MuzzleEffects::trigger(unsigned serial) {
  if(!serial || serial<=lastSerial_) return false;
  if(instances_.size()>=16) instances_.erase(instances_.begin());
  instances_.push_back(std::make_unique<Instance>(data_,serial));
  lastSerial_=serial;return true;
}
void MuzzleEffects::clear() { instances_.clear(); }
void MuzzleEffects::tick(const Matrix34 &parent) {
  for(auto x:parent) if(!std::isfinite(x)||std::abs(x)>10000000)
    throw std::runtime_error("Muzzle flash: Weapon matrix");
  for(auto &instance:instances_) {
    instance->data.sprites.spawnMatrix=parent;
    instance->parent=parent;
    // EnvironmentSimulation emits records with Time=0 in its constructor.
    // Construct only after the real posed parent is available; constructing
    // at trigger() baked every immediate flash into the world origin.
    if (!instance->simulation)
      instance->simulation=std::make_unique<EnvironmentSimulation>(instance->data.sprites);
    instance->simulation->tick(nullptr,256);
    ++instance->age;
  }
  instances_.erase(std::remove_if(instances_.begin(),instances_.end(),[](const auto &i) {
    const auto &s=i->simulation->state();
    return i->age>=180 || (i->age>20 && s.sprites.empty() && s.emitters.empty());
  }),instances_.end());
}
std::vector<WorldEffectQuad> MuzzleEffects::quads(const WorldEffectCamera &camera) const {
  std::vector<WorldEffectQuad> result;
  for(const auto &instance:instances_) {
    if (!instance->simulation) continue;
    WorldEffectsState state;
    static_cast<EnvironmentState&>(state)=instance->simulation->state();
    auto quads=WorldEffectQuads(instance->data,state,camera);
    for(auto &q:quads) {q.serial+=(std::uint64_t(instance->serial)<<32);result.push_back(q);}
  }
  std::stable_sort(result.begin(),result.end(),[](const auto &a,const auto &b) {
    return a.depth>b.depth || (a.depth==b.depth && a.serial>b.serial);
  });return result;
}
MuzzleEffectsState MuzzleEffects::state() const {
  MuzzleEffectsState out{data_.sprites.signature,lastSerial_,{}};
  for(const auto &i:instances_) out.instances.push_back({i->serial,i->age,
      i->simulation ? i->simulation->state() : EnvironmentState{},i->parent,!i->simulation});
  return out;
}
bool MuzzleEffects::restore(const MuzzleEffectsState &state) {
  if(state.signature!=data_.sprites.signature || state.instances.size()>16) return false;
  std::vector<std::unique_ptr<Instance>> restored;
  unsigned previous=0;
  for(const auto &i:state.instances) {
    if(!i.serial || i.serial<=previous || i.serial>state.lastSerial || i.age>180 || i.age!=i.state.frames) return false;
    for(auto x:i.parent) if(!std::isfinite(x)||std::abs(x)>10000000) return false;
    auto instance=std::make_unique<Instance>(data_,i.serial);
    if(i.pending) {
      if(i.age || i.state.signature || i.state.frames || i.state.serial ||
          i.state.random!=EnvironmentState{}.random || !i.state.skyAngles.empty() ||
          !i.state.recordCursor.empty() || !i.state.sequenceSeed.empty() ||
          !i.state.emitters.empty() || !i.state.sprites.empty() || !i.state.grids.empty()) return false;
    } else {
      instance->data.sprites.spawnMatrix=i.parent;
      instance->simulation=std::make_unique<EnvironmentSimulation>(instance->data.sprites);
      if(!instance->simulation->restore(i.state)) return false;
    }
    instance->age=i.age;instance->parent=i.parent;previous=i.serial;restored.push_back(std::move(instance));
  }
  lastSerial_=state.lastSerial;instances_=std::move(restored);return true;
}
std::vector<RoomLight> MuzzleEffects::lights(const RoomLighting &core) const {
  std::vector<RoomLight> out;
  if(!data_.weaponLight || core.cuts.size()<=8 || !core.present[8] || core.cuts[8].lights.empty()) return out;
  const auto &record=*data_.weaponLight;
  const std::vector<unsigned char> bytes(record.bytes.begin(),record.bytes.end());
  const AssetReader r{bytes};
  const unsigned life=r.u16(184);
  // Frozen esp11::SetFreeWork/SetParam: CORE cut8 light0, exact authored
  // radius and source record tint. Esp11 itself draws no billboard.
  for(const auto &instance:instances_) if(instance->age && instance->age<=life) {
    auto light=core.cuts[8].lights[0];
    light.parent=0;light.parentNo=0;light.flags=3|0x40;
    light.position=TransformPoint(instance->parent,{r.f32(12),r.f32(16),r.f32(20)});
    light.radius=r.f32(136)*10;
    for(unsigned k=0;k<3;++k) light.color[k]=std::uint8_t(unsigned(light.color[k])*bytes[156+k]/255);
    light.color[3]=std::uint8_t(float(light.color[3])*float(bytes[159])*(1.f/255.f)*.5f);
    out.push_back(light);
  }
  return out;
}
}
