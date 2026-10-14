#include "village_props.h"
#include "asset_reader.h"
#include "model_skin.h"
#include "animation.h"
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace re4 {
namespace {
Matrix34 changed(const SmdPlacement &original,const SmdPlacement &live) {
  const auto a=PlacementMatrix(original),b=PlacementMatrix(live);
  motion64::Mtx ma{},mb{},inv{},delta{};
  std::memcpy(ma,a.data(),sizeof ma);std::memcpy(mb,b.data(),sizeof mb);
  if(!motion64::PSMTXInverse(ma,inv))throw std::runtime_error("Village props: singular authored matrix");
  motion64::PSMTXConcat(mb,inv,delta);Matrix34 result;std::memcpy(result.data(),delta,sizeof delta);return result;
}
SmdPlacement scroll(const Smd &s,unsigned work) {
  const auto at=std::find_if(s.placements.begin(),s.placements.end(),[&](const auto &p){return p.work==work;});
  if(at==s.placements.end())throw std::runtime_error("Village props: authored scroll object missing");
  return *at;
}
std::vector<std::uint8_t> entry(const RoomArchive &a,const char *tag) {
  const auto *e=a.find(tag);if(!e)throw std::runtime_error("Village props: original resource missing");
  return a.copy(e-a.entries().data());
}
RoomDoor actionArea(unsigned room,const std::vector<std::uint8_t> &bytes,unsigned no) {
  AssetReader r{bytes};r.range(0,16);r.range(16,std::size_t(r.u16(6))*156);
  for(unsigned i=0;i<r.u16(6);++i) {
    const auto p=16+std::size_t(i)*156;if(bytes[p+0x36]!=no)continue;
    RoomDoor d;d.sourceRoom=room;d.recordIndex=i;d.no=no;d.enabled=bytes[p+0x34];
    d.hitType=bytes[p+0x37];d.triggerType=8;d.targetType=1;d.priority=bytes[p+0x44];
    d.hitDirection=std::int8_t(bytes[p+0x48]);d.hitOpenAngle=std::int8_t(bytes[p+0x49]);
    d.area.enabled=bytes[p+4];d.area.type=bytes[p+5];d.area.floor=r.f32(p+8);
    d.area.height=r.f32(p+12);d.area.radius=r.f32(p+16);
    for(unsigned k=0;k<8;++k)d.area.points[k/2][k%2]=r.f32(p+20+k*4);
    return d;
  }
  throw std::runtime_error("Village props: original lid action area missing");
}
struct Target {unsigned first,second,slot;};
constexpr Target mission[]={
 {0x103,0x113,15},{0x103,0x113,16},{0x103,0x113,17},{0x103,0x113,18},
 {0x103,0x113,20},{0x103,0x113,19},{0x103,0x113,21},
 {0x108,0x118,4},{0x108,0x118,5},{0x108,0x118,6},{0x108,0x118,7},
 {0x108,0x118,9},{0x108,0x118,8},{0x108,0x118,10},{0x108,0x118,11}};
}
unsigned VillageProps::syncMedallionFlags(ScenarioState &state) {
  unsigned count=0;
  for(const auto &t:mission)if(state.room(t.first).object(t.slot,1) || state.room(t.second).object(t.slot,1)) {
    state.room(t.first).setObject(t.slot,1);state.room(t.second).setObject(t.slot,1);++count;
  }
  // Source stock reward is set at exactly ten; its stock/UI adapter is separate.
  if(count==10)state.scenario.set(13);
  if(count==15)state.scenario.set(48);
  return count;
}
VillageProps::VillageProps(unsigned room,const RoomArchive &archive,const Smd &smd,
    ScenarioState &state,RoomPickups &pickups,ScenarioCollision &collision,
    SatCollision &sight,SatCollision &effect)
 :room_(room),state_(state),pickups_(pickups),collision_(collision),sight_(sight),effect_(effect) {
  syncMedallionFlags(state_);
  const auto ets=ReadRoomObjectPlacements(entry(archive,"ETS"));
  const RoomObjectArchive etm(entry(archive,"ETM"));
  for(const auto &p:ets)if(p.id==0x1a) {
    Medal m;m.slot=p.slot;m.original.position=p.position;m.original.rotation=p.rotation;m.original.scale={1,1,1};m.pose=m.original;
    const auto model=SkinnedModel(etm.copy("et1a00.bin"));
    const auto centers=RestJointCenters(model.mesh);
    if(centers.empty())throw std::runtime_error("Village props: medallion parts missing");
    if((room==0x103 || room==0x113) && p.slot==0x13) {
      const auto parent=scroll(smd,8);const auto &joints=smd.models.at(parent.bin).joints;
      auto matrix=PlacementMatrix(parent);
      if(!joints.empty()) {
        const auto center=TransformPoint(matrix,joints[0].center);
        matrix[3]=center[0];matrix[7]=center[1];matrix[11]=center[2];
      }
      m.pose.position=TransformPoint(matrix,{-120,p.position[1]-parent.position[1],0});
      // The authored farm parent and target have only Y rotation.
      m.pose.rotation[1]+=parent.rotation[1];
    }
    for(const auto &center:centers) {
      Matrix34 part{1,0,0,center[0],0,1,0,center[1],0,0,1,center[2]};m.parts.push_back(part);
    }
    medals_.push_back(std::move(m));
  }
  hasWell_=room==0x103 || room==0x113;
  if(!hasWell_)return;
  cover_=scroll(smd,0x52);lid_=scroll(smd,0x53);
  openArea_=actionArea(room,entry(archive,"AEV"),room==0x103?0xa:7);
  auto find=[&](unsigned area) {
    for(std::size_t n=0;n<pickups_.items_.size();++n)if(pickups_.items_[n].trigger.no==area)return n;
    throw std::runtime_error("Village props: original pendant ITA missing");
  };
  pendant_=find(room==0x103?0x8a:0x81);donor_=find(room==0x103?0x9e:0x9f);
  pickups_.items_[donor_].eventEnabled=false;
  pickups_.variants_[pendant_]={0x58,0x89};
  lidAngle_=state_.scenario.test(37)?-1.83f:state_.scenario.test(26)?1.12f:0;
  collision_.setEnabled(6,state_.scenario.test(26) && !state_.scenario.test(37));
  collision_.sync(state_.room(room_),&sight_,&effect_);
  auto &item=pickups_.items_[pendant_];
  if(pickups_.found(pendant_)) {
    const auto floor=effect_.ground(item.renderPosition,0,100000);
    if(!floor)throw std::runtime_error("Village props: pendant EAT landing missing");
    item.position[1]=floor->point[1];item.modelYOffset=0;pickups_.autoArea(item);
  }
  if(room==0x103)state_.scenario.set(19);
  syncWell();
}
void VillageProps::syncWell() {
  if(!hasWell_)return;
  auto &item=pickups_.items_[pendant_];
  const bool closed=state_.scenario.test(26),open=state_.scenario.test(37);
  if(pickups_.found(pendant_) && (!closed || open) && !state_.scenario.test(27))state_.scenario.set(27);
  if(state_.scenario.test(27)) {
    item.id=0x89;item.eventEnabled=!closed || open;
  }
  if(closed && !open)item.foundSound=5;
  if(open)item.foundSound=4;
}
bool VillageProps::openOffer(const CollisionVec &feet,double heading,const std::optional<CollisionVec> &front) const {
  if(!hasWell_ || !state_.scenario.test(26) || state_.scenario.test(37))return false;
  if(!state_.scenario.test(27) && pickups_.found(pendant_) && !pickups_.taken(pickups_.items_[pendant_]))return false;
  return RoomFlow::hit(openArea_,feet,heading,front);
}
void VillageProps::sound(unsigned number,const std::array<float,3> &position) {
  sounds_.push_back({number,{position[0],position[1],position[2]}});
}
bool VillageProps::openLid() {
  if(!hasWell_ || !state_.scenario.test(26) || state_.scenario.test(37))return false;
  if(!state_.scenario.test(27) && pickups_.found(pendant_) && !pickups_.taken(pickups_.items_[pendant_]))return false;
  state_.scenario.set(37);sound(9,lid_.position);opening_=true;
  lidAngle_-=.06981317f;
  collision_.setEnabled(6,false);collision_.sync(state_.room(room_),&sight_,&effect_);
  auto &item=pickups_.items_[pendant_];
  if(state_.scenario.test(27) && !pickups_.taken(item)) {
    item.eventEnabled=true;item.controlFlags|=0x10;item.falling=true;item.fallVelocity=-10;
    item.position[1]+=item.modelYOffset+10;item.modelYOffset=0;
  }
  syncWell();return true;
}
void VillageProps::tick30Hz() {
  syncMedallionFlags(state_);if(!hasWell_)return;
  if(opening_) {lidAngle_-=.06981317f;if(lidAngle_<-1.83f){lidAngle_=-1.83f;opening_=false;}}
  if(closing_) {
    lidAngle_+=speed_;
    if(lidAngle_>1.12f) {
      lidAngle_=1.12f;closing_=false;bounce_=1;sound(8,lid_.position);
      effects_.push_back({0x11,{0,0,0}}); // EstSet(NULL): room EST is already in world coordinates.
      lidAngle_-=.06981317f;
    }else speed_+=.017453292f;
  }else if(bounce_) {
    constexpr float steps[]={0,-.02617994f,.02617994f,.06981317f};
    lidAngle_+=steps[bounce_];if(++bounce_==4)bounce_=0;
  }
  syncWell();
}
std::optional<VillageProps::Hit> VillageProps::target(const CollisionVec &from,const CollisionVec &to,
    unsigned weapon,float maxLenSq) const {
  std::optional<Hit> best;
  if(hasWell_ && !state_.scenario.test(26)) {
    auto p=cover_;p.rotation[0]=-.5235988f;
    if(const auto hit=HitAuthoredShapes({{{0,0,50},100,1200,100,HitOn|HitCube,0}}, {},PlacementMatrix(p),from,to,maxLenSq,weapon)) {
      best=Hit{Hit::Cover,0,*hit};maxLenSq=hit->lenSq;
    }
  }
  if(weapon==0x10 || weapon==0x14 || weapon==0x16 || weapon==0x17 || weapon==0x2a || weapon==0xe)return best;
  for(std::size_t n=0;n<medals_.size();++n) {
    const auto &m=medals_[n];if(state_.room(room_).object(m.slot,1))continue;
    if(const auto hit=HitAuthoredShapes({{{0,-1000,0},100,200,55,HitOn|HitCube,1}},m.parts,
        PlacementMatrix(m.pose),from,to,maxLenSq,weapon)) {best=Hit{Hit::Medallion,n,*hit};maxLenSq=hit->lenSq;}
  }
  return best;
}
bool VillageProps::applyHit(const Hit &hit,unsigned serial,unsigned weapon) {
  if(hit.kind==Hit::Medallion) {
    if(hit.index>=medals_.size() || weapon==0x10 || weapon==0x14 || weapon==0x16 || weapon==0x17 || weapon==0x2a || weapon==0xe)return false;
    const auto &m=medals_[hit.index];if(state_.room(room_).object(m.slot,1))return false;
    state_.room(room_).setObject(m.slot,1);sound(0x2e,m.pose.position);syncMedallionFlags(state_);
    if(medalEffects_) {
      std::vector<EnvironmentMatrix> parts;
      const auto root=PlacementMatrix(m.pose);
      for(const auto &part:m.parts) {
        motion64::Mtx a{},b{},out{};std::memcpy(a,root.data(),sizeof a);std::memcpy(b,part.data(),sizeof b);
        motion64::PSMTXConcat(a,b,out);EnvironmentMatrix p;std::memcpy(p.data(),out,sizeof out);parts.push_back(p);
      }
      if(!medalEffects_->triggerAttached(0,m.pose.position,m.pose.rotation,++effectSerial_,parts,&effect_))
        throw std::runtime_error("Village props: authored medallion EST rejected");
    }
    return true;
  }
  if(!hasWell_ || state_.scenario.test(26) || lastCoverShot_==serial)return false;
  lastCoverShot_=serial;state_.scenario.set(26);closing_=true;speed_=.017453292f;
  sound(7,cover_.position);effects_.push_back({0x10,{0,0,0}});
  collision_.setEnabled(6,true);collision_.sync(state_.room(room_),&sight_,&effect_);syncWell();return true;
}
bool VillageProps::roomObjectVisible(int slot) const {
  for(const auto &m:medals_)if(int(m.slot)==slot)return !state_.room(room_).object(m.slot,1);
  return true;
}
std::optional<Matrix34> VillageProps::roomObjectDelta(int slot) const {
  for(const auto &m:medals_)if(int(m.slot)==slot)return changed(m.original,m.pose);
  return {};
}
bool VillageProps::scrollVisible(unsigned work) const {return !hasWell_ || work!=0x52 || !state_.scenario.test(26);}
std::optional<Matrix34> VillageProps::scrollDelta(unsigned work) const {
  if(!hasWell_)return {};
  if(work==0x52) {auto p=cover_;p.rotation[0]=-.5235988f;return changed(cover_,p);}
  if(work==0x53) {SmdPlacement local;local.scale={1,1,1};local.rotation[0]=lidAngle_;
    // cObj's parent placement stays fixed; pList->ang rotates locally.
    const auto base=PlacementMatrix(lid_),rot=PlacementMatrix(local);motion64::Mtx a{},b{},inv{},moving{},delta{};
    std::memcpy(a,base.data(),sizeof a);std::memcpy(b,rot.data(),sizeof b);
    if(!motion64::PSMTXInverse(a,inv))throw std::runtime_error("Village props: lid hinge inverse");
    motion64::PSMTXConcat(a,b,moving);motion64::PSMTXConcat(moving,inv,delta);Matrix34 result;std::memcpy(result.data(),delta,sizeof delta);return result;
  }
  return {};
}
std::vector<VillageProps::Cue> VillageProps::takeSounds(){auto out=std::move(sounds_);sounds_.clear();return out;}
std::vector<VillageProps::Effect> VillageProps::takeEffects(){auto out=std::move(effects_);effects_.clear();return out;}
void VillageProps::initializeEffects(const RoomArchive &archive,const std::vector<std::uint8_t> &core) {
  auto prepare=[](WorldEffectsData &data) {
    data.triggerInstanceLimit=32;
    for(unsigned n=0;n<data.sprites.targets.size();++n) {
      const auto sequence=data.sprites.targets[n].sequence;
      data.triggerBanks.push_back({sequence,sequence,n,0});
      if(data.targetPrerun.at(n))throw std::runtime_error("Village props: unexpected authored effect pre-run");
      data.targetPrerun[n]=~0u; // Triggered EstSet, not a room-entry SST stream.
    }
  };
  if(hasWell_) {
    wellEffectsData_=BuildEventWorldEffects(entry(archive,"EFF"),core,{}, {0x10,0x11});prepare(wellEffectsData_);
    wellEffects_=std::make_unique<WorldEffectsSimulation>(wellEffectsData_);
  }
  if(!medals_.empty()) {
    RoomObjectArchive etm(entry(archive,"ETM"));
    medalEffectsData_=BuildEventWorldEffects(etm.copy("et1a.eff"),core,{}, {0});prepare(medalEffectsData_);
    medalEffects_=std::make_unique<WorldEffectsSimulation>(medalEffectsData_);
  }
}
void VillageProps::tickEffects(const WorldEffectCamera &camera) {
  for(const auto &effect:takeEffects())if(wellEffects_ &&
      !wellEffects_->trigger(effect.sequence,{float(effect.position[0]),float(effect.position[1]),float(effect.position[2])},
          {},++effectSerial_,&effect_))throw std::runtime_error("Village props: authored well EST rejected");
  if(wellEffects_)wellEffects_->tick(&camera,&effect_);
  if(medalEffects_)medalEffects_->tick(&camera,&effect_);
}
std::vector<WorldEffectQuad> VillageProps::wellQuads(const WorldEffectCamera &camera) const {
  return wellEffects_?WorldEffectQuads(wellEffectsData_,wellEffects_->state(),camera):std::vector<WorldEffectQuad>{};
}
std::vector<WorldEffectQuad> VillageProps::medalQuads(const WorldEffectCamera &camera) const {
  return medalEffects_?WorldEffectQuads(medalEffectsData_,medalEffects_->state(),camera):std::vector<WorldEffectQuad>{};
}
std::vector<WorldEffectLine> VillageProps::medalLines() const {
  return medalEffects_?WorldEffectLines(medalEffectsData_,medalEffects_->state()):std::vector<WorldEffectLine>{};
}
} // namespace re4
