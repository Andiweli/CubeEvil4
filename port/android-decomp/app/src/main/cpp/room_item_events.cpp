#include "room_item_events.h"
#include "asset_reader.h"
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace re4 {
namespace {
struct Binding {unsigned area,item,flag,cut,left,right;};
const std::vector<Binding> &bindings(unsigned room) {
  static const std::vector<Binding> day{{7,0x92,0,0xa,0x57,0x58},{8,0x81,1,0xb,0x59,0x5a},{9,0x83,2,9,0x5b,0x5c}};
  static const std::vector<Binding> night{{8,0x8e,0,0xa,0x57,0x58},{9,0x8f,1,0xb,0x59,0x5a},{0xa,0x8b,2,9,0x5b,0x5c}};
  static const std::vector<Binding> next{{6,0x85,0,6,0x29,0x2a},{7,0x86,1,7,0x2b,0x2c}};
  switch(room){case 0x103:return day;case 0x113:return night;case 0x106:return next;}
  throw std::runtime_error("Item event: unsupported room");
}
std::vector<std::uint8_t> entry(const RoomArchive &a,const char *tag) {
  const auto *e=a.find(tag);if(!e)throw std::runtime_error(std::string("Item event: missing ")+tag);
  return a.copy(e-a.entries().data());
}
RoomDoor area(unsigned room,const std::vector<std::uint8_t> &a,unsigned no) {
  const AssetReader r{a};r.range(0,16);
  if(r.u32(0)!=0x41455600 || r.u16(4)!=0x104)throw std::runtime_error("Item event: original AEV format");
  const auto count=r.u16(6);r.range(16,std::size_t(count)*156);
  for(unsigned i=0;i<count;++i) {
    const auto p=16+std::size_t(i)*156;if(a[p+0x36]!=no)continue;
    RoomDoor d;d.sourceRoom=room;d.recordIndex=i;d.no=no;d.enabled=a[p+0x34];
    d.hitType=a[p+0x37];d.triggerType=8;d.targetType=1;d.priority=5;
    d.hitDirection=std::int8_t(a[p+0x48]);d.hitOpenAngle=std::int8_t(a[p+0x49]);
    d.area.enabled=a[p+4];d.area.type=a[p+5];d.area.floor=r.f32(p+8);
    d.area.height=r.f32(p+12);d.area.radius=r.f32(p+16);
    for(unsigned n=0;n<8;++n)d.area.points[n/2][n%2]=r.f32(p+20+n*4);
    std::copy_n(a.begin()+p,156,d.raw.begin());std::copy_n(a.begin()+p+4,48,d.area.raw.begin());
    if(d.hitType>3 || d.area.height<=0 || (d.area.type!=1 && d.area.type!=2))
      throw std::runtime_error("Item event: invalid authored area");
    return d;
  }
  throw std::runtime_error("Item event: original action area missing");
}
SmdPlacement pose(const Smd &s,unsigned work) {
  const auto i=std::find_if(s.placements.begin(),s.placements.end(),[&](const auto &p){return p.work==work;});
  if(i==s.placements.end() || i->motion!=0xff)throw std::runtime_error("Item event: original static hinge missing");
  return *i;
}
Matrix34 hinge(const SmdPlacement &p,float angle) {
  const auto base=PlacementMatrix(p);SmdPlacement local;local.scale={1,1,1};local.rotation[1]=angle;
  const auto rot=PlacementMatrix(local);motion64::Mtx a{},b{},inverse{},moving{},delta{};
  std::memcpy(a,base.data(),sizeof a);std::memcpy(b,rot.data(),sizeof b);
  if(!motion64::PSMTXInverse(a,inverse))throw std::runtime_error("Item event: singular original hinge");
  motion64::PSMTXConcat(a,b,moving);motion64::PSMTXConcat(moving,inverse,delta);
  Matrix34 result;std::memcpy(result.data(),delta,sizeof delta);return result;
}
}
RoomItemEvents::RoomItemEvents(std::uint16_t room,const RoomArchive &archive,const Smd &smd,
                               ScenarioState &state,RoomPickups &pickups)
 :room_(room),state_(state),pickups_(pickups) {
  const auto aev=entry(archive,"AEV"),cam=entry(archive,"CAM");
  for(const auto &b:bindings(room)) {
    Shelf s;s.area=area(room,aev,b.area);s.itemArea=b.item;s.flag=b.flag;s.cut=b.cut;s.left=b.left;s.right=b.right;
    s.leftPose=pose(smd,b.left);s.rightPose=pose(smd,b.right);s.camera=std::make_unique<R100RoomCamera>(cam,b.cut);
    const bool opened=state_.room(room_).room.test(b.flag);
    if(opened){s.leftAngle=-1.9198622f;s.rightAngle=1.9198622f;s.steps=30;}
    pickups_.setAuthoredAreaEnabled(b.item,opened);shelves_.push_back(std::move(s));
  }
}
std::optional<std::size_t> RoomItemEvents::offer(const CollisionVec &feet,double heading,
                          const std::optional<CollisionVec> &front) const {
  if(active())return {};
  std::optional<std::size_t> best;
  for(std::size_t n=0;n<shelves_.size();++n) {
    const auto &s=shelves_[n];if(state_.room(room_).room.test(s.flag) || !RoomFlow::hit(s.area,feet,heading,front))continue;
    if(!best || s.area.recordIndex<shelves_[*best].area.recordIndex)best=n;
  }
  return best;
}
void RoomItemEvents::step(Shelf &s) {
  if(s.steps==30)return;
  s.leftAngle+=-.063995406f;s.rightAngle+=.063995406f;++s.steps;
}
bool RoomItemEvents::begin(std::size_t index) {
  if(active() || index>=shelves_.size() || state_.room(room_).room.test(shelves_[index].flag))return false;
  auto &s=shelves_[index];pickups_.setAuthoredAreaEnabled(s.itemArea,true);
  state_.room(room_).room.set(s.flag);s.camera->reset();active_=index;wait_=0;waiting_=false;sound_=true;
  step(s);return true; // first original loop iteration precedes SceSleep(1)
}
void RoomItemEvents::tick30Hz() {
  if(!active())return;
  ++ticks_;auto &s=shelves_[*active_];step(s);s.camera->tick30Hz();
  if(s.steps==30 && s.camera->finished()) {
    if(!waiting_){waiting_=true;wait_=15;}
    else if(wait_ && !--wait_)active_.reset();
  }
}
const EventCamera *RoomItemEvents::camera() const {return active()?&shelves_[*active_].camera->camera():nullptr;}
std::optional<unsigned> RoomItemEvents::lightArea() const {return active()?std::optional<unsigned>(shelves_[*active_].camera->lightArea()):std::nullopt;}
std::optional<Matrix34> RoomItemEvents::delta(unsigned work) const {
  for(const auto &s:shelves_){if(s.left==work)return hinge(s.leftPose,s.leftAngle);if(s.right==work)return hinge(s.rightPose,s.rightAngle);}
  return {};
}
bool RoomItemEvents::takeOpenSound(){const bool sound=sound_;sound_=false;return sound;}
}
