#include "r100_scenario.h"
#include "asset_reader.h"
#include <algorithm>
#include <stdexcept>

namespace re4 {
CollisionVec R100FirstPlayerPosition() { return {-82910, 860, -38480}; }
CollisionVec R120RoadJumpPosition() { return {-109450, -515, 820}; }
CollisionVec R100EntryPlayerPosition() { return {-99685, -484, -1343}; }
void BeginOriginalOpening(ScenarioState &state,bool movieCanceled) {
  state.scenario.set(OriginalOpeningCancelFlag,movieCanceled);
}
void CancelOriginalOpening(ScenarioState &state) {
  state.scenario.set(OriginalOpeningCancelFlag);
}
bool OriginalOpeningCanceled(const ScenarioState &state) {
  return state.scenario.test(OriginalOpeningCancelFlag);
}
bool BeginR100Entry(RoomSaveState &state) {
  if(state.room.test(13))return false;
  state.room.set(13);return true;
}
EnemySpawn R100FirstGanadoSpawn(bool afterLook) {
  EnemySpawn spawn;
  spawn.listIndex = spawn.emsetNo = 0xff;
  spawn.room = 0x100; spawn.beFlags = 7;
  spawn.id = 0x12; spawn.type = 0; spawn.setNo = 0x13;
  spawn.flags = 0x21000020u | (afterLook ? 1u : 0u);
  spawn.hp = 500; spawn.guardRadius = 10000; spawn.character = 0;
  spawn.position = afterLook ? CollisionVec{-79116, 860, -38890}
                             : CollisionVec{-77630, 860, -36500};
  spawn.heading = afterLook ? -1.39f : 8192.f * (3.1415927f / 16384.f);
  spawn.rotation[1] = spawn.heading;
  return spawn;
}
R100RoomProfile R100Profile(const RoomSaveState &state) {
  const bool after = state.room.test(10);
  return {!after, !after && state.room.test(3), after,
          state.room.test(4), state.room.test(15)};
}
bool BeginR100FirstEncounter(RoomSaveState &state) {
  if (state.room.test(3) || state.room.test(10)) return false;
  state.room.set(3); return true;
}
bool CompleteR100FirstEncounter(ScenarioState &state) {
  auto &room = state.room(0x100);
  if (!room.room.test(3) || room.room.test(10)) return false;
  room.room.set(10);
  state.scenario.set(17); // global.h SCF_R100_KILL_GANADE_1ST
  return true;
}
bool ReleaseR100HouseAmbush(RoomSaveState &state,bool playerInsideHouse) {
  if(!state.room.test(10) || state.room.test(4) || playerInsideHouse)return false;
  state.room.set(4);return true;
}
std::vector<const EnemySpawn *> R100AmbushSpawns(
    const EnemySpawns &spawns, const RoomSaveState &state,
    const std::set<unsigned> &dead) {
  std::vector<const EnemySpawn *> result;
  if (!state.room.test(10)) return result;
  for (unsigned index : {3u, 4u, 5u, 0x12u, 0x13u, 0x26u, 0x27u}) {
    const auto *spawn = spawns.find(index);
    // EmSetFromList2(no,1) deliberately ignores be_flag bit0, but checks
    // room/id/already-created/death. A native caller owns those live flags.
    if (spawn && spawn->room == 0x100 && spawn->id &&
        !spawn->alreadySet() && !dead.count(index)) result.push_back(spawn);
  }
  return result;
}
R100Scenario::R100Scenario(const std::vector<std::uint8_t> &bytes) {
  AssetReader r{bytes}; r.range(0,16);
  if (r.u32(0) != 0x41455600 || r.u16(4) != 0x104 || r.u16(6) > 256)
    throw std::runtime_error("r100: original AEV 0x104 is missing");
  r.range(16, std::size_t(r.u16(6))*156);
  bool found = false,foundHouse=false;
  for (unsigned i=0;i<r.u16(6);++i) {
    const auto p=16+std::size_t(i)*156;
    const bool house=bytes[p+0x36]==R100HouseFlagArea;
    if (!house && bytes[p+0x36] != R100FirstLookArea) continue;
    if (house) {
      if(foundHouse || bytes[p+0x35]!=4 || bytes[p+0x5c]!=0 || bytes[p+0x5d]!=0 || r.u16(p+0x5e)!=64)
        throw std::runtime_error("r100: invalid original house flag range");
      foundHouse=true;
    } else {
      if(found || bytes[p+0x35]!=2)throw std::runtime_error("r100: invalid first event range");
      found=true;
    }
    auto &d=house?house_:firstLook_;d.sourceRoom=0x100;d.recordIndex=i;
    d.no=bytes[p+0x36];d.enabled=bytes[p+0x34];d.hitType=bytes[p+0x37];
    d.triggerType=bytes[p+0x38];d.targetType=bytes[p+0x39];
    d.parentPointer=r.u32(p+0x4c);
    d.hitDirection=static_cast<std::int8_t>(bytes[p+0x48]);
    d.hitOpenAngle=static_cast<std::int8_t>(bytes[p+0x49]);
    d.area.enabled=bytes[p+4];d.area.type=bytes[p+5];
    d.area.floor=r.f32(p+8);d.area.height=r.f32(p+12);d.area.radius=r.f32(p+16);
    for(unsigned j=0;j<8;++j)d.area.points[j/2][j%2]=r.f32(p+20+j*4);
    if(d.parentPointer || d.area.height<=0 || d.area.radius<0 ||
       (d.hitType&~3) || d.hitOpenAngle<0 || d.area.type!=1)
      throw std::runtime_error("r100: Event range exceeds original profile bounds");
    std::copy_n(bytes.begin()+p,d.raw.size(),d.raw.begin());
    std::copy_n(bytes.begin()+p+4,d.area.raw.size(),d.area.raw.begin());
  }
  if(!found || !foundHouse)throw std::runtime_error("r100: original event/house range is missing");
  for(unsigned i=0;i<r.u16(6);++i) {
    const auto p=16+std::size_t(i)*156;RoomDoor d;
    d.sourceRoom=0x100;d.recordIndex=i;d.no=bytes[p+0x36];
    d.enabled=bytes[p+0x34];d.hitType=bytes[p+0x37];d.triggerType=bytes[p+0x38];
    d.targetType=bytes[p+0x39];d.priority=bytes[p+0x44];d.parentPointer=r.u32(p+0x4c);
    d.hitDirection=static_cast<std::int8_t>(bytes[p+0x48]);d.hitOpenAngle=static_cast<std::int8_t>(bytes[p+0x49]);
    d.area.enabled=bytes[p+4];d.area.type=bytes[p+5];d.area.floor=r.f32(p+8);
    d.area.height=r.f32(p+12);d.area.radius=r.f32(p+16);
    for(unsigned k=0;k<8;++k)d.area.points[k/2][k%2]=r.f32(p+20+k*4);
    std::copy_n(bytes.begin()+p,d.raw.size(),d.raw.begin());
    std::copy_n(bytes.begin()+p+4,d.area.raw.size(),d.area.raw.begin());
    // SceAtRoomSet converts message/save editor triggers into action
    // contexts. Keep raw untouched for the disk oracle.
    if(d.raw[0x35]==5 && !(d.triggerType&8)) {d.triggerType=(d.triggerType&0x80)|8;d.priority=2;}
    if(d.raw[0x35]==8 && !(d.triggerType&8)) {d.triggerType=(d.triggerType&0x80)|8;d.priority=5;}
    if(!areas_.emplace(d.no,d).second)throw std::runtime_error("r100: Duplicate original AEV area number");
  }
  for(unsigned area:{1u,0x15u,0x18u,0x19u,0x1au,0x1bu})
    if(!areas_.count(area) || areas_.at(area).raw[0x35]!=2)
      throw std::runtime_error("r100: secondary original execution range is missing");
  const auto &closure=authoredArea(3);
  if(closure.raw[0x35]!=4 || closure.targetType!=3 || closure.raw[0x5c]!=0 ||
      closure.raw[0x5d]!=0 || closure.raw[0x5e]!=0 || closure.raw[0x5f]!=0 ||
      authoredArea(0xb).raw[0x35]!=2 || authoredArea(0xb).triggerType!=8)
    throw std::runtime_error("r100: original door flag/knock ranges are missing");
}
bool R100Scenario::firstLookHit(const CollisionVec &feet,double heading,
                               const std::optional<CollisionVec> &front) const {
  return RoomFlow::hit(firstLook_,feet,heading,front);
}
bool R100Scenario::houseInsideHit(const CollisionVec &feet,double heading,
                                  const std::optional<CollisionVec> &front) const {
  return RoomFlow::hit(house_,feet,heading,front);
}
bool R100Scenario::doorClosureFlagHit(const CollisionVec &feet,double heading) const {
  return RoomFlow::hit(authoredArea(3),feet,heading);
}
const RoomDoor &R100Scenario::authoredArea(unsigned number) const {
  const auto found=areas_.find(number);
  if(found==areas_.end())throw std::runtime_error("r100: Original range is missing");
  return found->second;
}
std::optional<R100SecondaryOffer> R100Scenario::secondaryOffer(const RoomSaveState &state,
    const CollisionVec &feet,double heading,bool pressed,bool held,const std::optional<CollisionVec> &front,
    bool doorMessageEnabled) const {
  const bool after=state.room.test(10);std::optional<R100SecondaryOffer> result;
  for(unsigned no:{1u,0x15u,0x18u,0x19u,0x1au,0x1bu,0xbu,0xcu,0x10u,0x11u,0x12u,0x13u,0x16u,0x17u,0x1du,0x1fu,0x20u,0x21u}) {
    const auto &area=areas_.at(no);
    if(no==0xb && !doorMessageEnabled)continue;
    if(!RoomFlow::hit(area,feet,heading,front))continue;
    if((area.triggerType&8) && !pressed)continue; // original action button callback is a press
    if(!(area.triggerType&8) && !(area.triggerType&1) && !((area.triggerType&2) && pressed))continue;
    (void)held; // Action2 offers are confirmed once, not repeated while held.
    R100SecondaryOffer offer;offer.area=no;
    if(no==0xb) {
      offer.kind=R100SecondaryOffer::RoomMessage;offer.message=0xb;
    } else if(no==0xc) {
      if(after)continue;
      offer.kind=R100SecondaryOffer::RoomMessage;offer.message=0xc;
    } else if(area.raw[0x35]==5) {
      offer.kind=R100SecondaryOffer::RoomMessage;
      offer.message=(unsigned(area.raw[0x5e])<<8)|area.raw[0x5f];
      const auto cam=area.raw[0x60];offer.cameraCut=cam?cam-1:~0u;
    } else if(no==0x15) {
      if(state.room.test(15))continue;
      offer.kind=R100SecondaryOffer::HouseScare;offer.cameraCut=0xa;
      offer.playerPosition=CollisionVec{-80540,-8,-12073};
    } else if(no==0x19 || no==0x1a || no==0x1b) {
      if(after)continue;
      offer.phase=no==0x19?6:no==0x1a?7:8;
      if(no==0x1b)offer.playerPosition=CollisionVec{-112251,-193,-4420};
    } else {
      if(!after)continue; // R100Init disables area1 and does not bind area18 before RSF10.
      offer.preservePlayerPosition=true;
      if(!state.room.test(14))offer.phase=9;
      else {
        offer.kind=R100SecondaryOffer::RoomCamera;
        offer.cameraCut=no==1?9:8;offer.message=no==1?0x28:0xe;
      }
    }
    if(!result || area.priority>areas_.at(result->area).priority ||
        (area.priority==areas_.at(result->area).priority && area.recordIndex<areas_.at(result->area).recordIndex))result=offer;
  }
  return result;
}
bool R100Scenario::beginSecondary(RoomSaveState &state,const R100SecondaryOffer &offer) const {
  if(offer.kind==R100SecondaryOffer::CorpseMessage)return true;
  if(!areas_.count(offer.area))throw std::runtime_error("r100: secondary range is missing");
  if(offer.kind==R100SecondaryOffer::HouseScare) {
    if(state.room.test(15))return false;
    state.room.set(15);
  } else if(offer.kind==R100SecondaryOffer::EngineEvent && offer.phase==9) {
    if(state.room.test(14))return false;
    state.room.set(14);
  }
  return true;
}
} // namespace re4
