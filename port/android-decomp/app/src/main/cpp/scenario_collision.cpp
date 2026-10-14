#include "scenario_collision.h"
#include "asset_reader.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace re4 {
ScenarioCollision::ScenarioCollision(std::uint16_t room,const std::vector<std::uint8_t> &bytes)
    :room_(room) {
  const AssetReader r{bytes};r.range(0,16);
  if(r.u32(0)!=0x41455600 || r.u16(4)!=0x104 || r.u16(6)>4096)
    throw std::runtime_error("Scenario collision: Unsupported AEV header");
  const auto count=r.u16(6);r.range(16,std::size_t(count)*156);
  std::set<unsigned> numbers;
  for(unsigned i=0;i<count;++i) {
    const auto p=16+std::size_t(i)*156;
    if(bytes[p+0x35]!=0xb)continue;
    if(!numbers.insert(bytes[p+0x36]).second)
      throw std::runtime_error("Scenario collision: Duplicate area number");
    if(r.u32(p+0x4c)) {++parented_;continue;}
    if(bytes[p+5]!=1)continue; // sceAtSetScrAt only creates quad areas.
    ScenarioCollisionPiece piece;
    piece.number=bytes[p+0x36];piece.enabled=bytes[p+0x34];
    piece.sightAttributes=r.u32(p+0x68);piece.effectAttributes=r.u32(p+0x6c);
    piece.control=r.u32(p+0x70);piece.flags=r.u32(p+0x74);
    piece.floor=r.f32(p+8);piece.height=r.f32(p+12);
    // createFloorSat ignores height; the source masks the three control
    // bits separately and does not reject additional editor bits.
    if(std::abs(piece.height)>1e7f || (!(piece.flags&0x200) && piece.height<=0))
      throw std::runtime_error("Scenario collision: Invalid piece dimensions/flags");
    for(unsigned k=0;k<8;++k)piece.points[k/2][k%2]=r.f32(p+20+k*4);
    pieces_.push_back(piece);
  }
}
bool ScenarioCollision::enabled(const ScenarioCollisionPiece &piece,const RoomSaveState &state) const {
  if(const auto at=overrides_.find(piece.number);at!=overrides_.end())return at->second;
  if(room_==0x100) {
    // R100Init / r100_Sce_zombi_dead enable the ravine wall after s20.
    // r100_Car_pos_move removes the truck and both police-car collision sets.
    const bool after=state.room.test(10);
    if(piece.number==0x1c)return after;
    if(piece.number==0x1e)return !after;
    for(unsigned number:{4u,5u,0x25u,0x26u,0x27u})
      if(piece.number==number)return !after;
  }
  return (piece.enabled&1)!=0;
}
void ScenarioCollision::setEnabled(unsigned number,bool value) {
  if(std::none_of(pieces_.begin(),pieces_.end(),[&](const auto &p){return p.number==number;}))
    throw std::runtime_error("Scenario collision: script area is missing");
  const auto at=overrides_.find(number);
  if(at!=overrides_.end() && at->second==value)return;
  overrides_[number]=value;installed_=false;
}
Sat ScenarioCollision::geometry(const RoomSaveState &state,SatQueryMode mode) const {
  if(mode!=SatQueryMode::Sight && mode!=SatQueryMode::Effect)
    throw std::runtime_error("Scenario collision: Invalid query mode");
  std::vector<ScenarioTriangle> triangles;
  // Literal vertex indices from atari.cpp createSat/createBoxSat/createFloorSat.
  constexpr unsigned walls[8][3]={{5,2,1},{2,5,6},{3,4,0},{3,7,4},{0,4,1},{1,4,5},{2,7,3},{2,6,7}};
  constexpr unsigned caps[4][3]={{4,6,5},{6,4,7},{0,1,2},{0,2,3}};
  constexpr unsigned floor[2][3]={{0,2,1},{2,0,3}};
  for(const auto &piece:pieces_) {
    if(!enabled(piece,state))continue;
    const bool sight=mode==SatQueryMode::Sight;
    if(piece.control&(sight?2u:1u))continue;
    auto attributes=sight?piece.sightAttributes:piece.effectAttributes;
    if(sight && !(piece.control&4))attributes|=0x40;
    std::array<std::array<float,3>,8> vertices{};
    // Match unparented source subtraction and addition in float, retaining
    // its first-corner origin rather than deriving a model bounding box.
    for(unsigned k=0;k<4;++k) {
      vertices[k]={piece.points[0][0]+(piece.points[k][0]-piece.points[0][0]),
                   piece.floor,piece.points[0][1]+(piece.points[k][1]-piece.points[0][1])};
      vertices[k+4]=vertices[k];vertices[k+4][1]+=piece.height;
    }
    const auto append=[&](const unsigned *ids,unsigned group) {
      triangles.push_back({{vertices[ids[0]],vertices[ids[1]],vertices[ids[2]]},attributes,group});
    };
    if(piece.flags&0x200)for(const auto &ids:floor)append(ids,0);
    else {
      if(piece.flags&0x100)for(const auto &ids:caps)append(ids,1);
      for(const auto &ids:walls)append(ids,2);
    }
  }
  return Sat::ScenarioGeometry(triangles);
}
bool ScenarioCollision::sync(const RoomSaveState &state,SatCollision *sight,SatCollision *effect) {
  const bool after=room_==0x100 && state.room.test(10);
  if(installed_ && after==afterTruck_)return false;
  // Validate both meshes before publishing either one.
  auto sat=geometry(state,SatQueryMode::Sight),eat=geometry(state,SatQueryMode::Effect);
  if(sight)sight->setScenarioGeometry(std::move(sat));
  if(effect)effect->setScenarioGeometry(std::move(eat));
  afterTruck_=after;installed_=true;return true;
}
} // namespace re4
