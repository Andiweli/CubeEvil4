#include "sound_footsteps.h"
#include "archive.h"
#include "asset_reader.h"
#include <cmath>
#include <stdexcept>
namespace re4 {
std::vector<PlayerSequenceSoundCue> SourceFootSoundCue(unsigned se,unsigned free) {
  if(!se)return {};
  if(se>255 || free>255)throw std::runtime_error("Audio: SEQ sound byte");
  const unsigned no=se-1,sub=free&7;
  unsigned part=0,block=1;
  switch(no) {
  case 0:case 2:case 0x0d:part=0x15;block=5;break;
  case 1:case 3:case 0x0e:case 0x14:part=0x19;block=5;break;
  case 4:case 5:block=5;break;
  default:break;
  }
  std::vector<PlayerSequenceSoundCue> cues;
  if(!sub) {
    switch(no) {
    case 0:case 1:cues.push_back({1,0x2a,0});break;
    case 2:case 3:case 0x0d:case 0x0e:case 0x14:cues.push_back({1,0x2f,0});break;
    default:break;
    }
  }
  switch(sub) {
  case 1:block=1;break;
  case 2:block=6;break;
  case 3:block=2;break;
  default:break;
  }
  cues.push_back({block,no,part});return cues;
}
FootstepSounds::FootstepSounds(const RoomArchive &room) {
  if(const auto *entry=room.find("FSE"))
    *this=FootstepSounds(room.copy(std::size_t(entry-room.entries().data())));
}
FootstepSounds::FootstepSounds(const std::vector<std::uint8_t> &bytes) {
  if(bytes.empty())return;
  const AssetReader r{bytes};r.range(0,16);
  // FlrAtInit leaves pHead null for absent/placeholder/old FSE blocks.
  // Fourteen original room archives contain a zero-filled placeholder.
  if(r.u32(0)!=0x46534500 || r.u16(4)!=0x103)return;
  const auto count=r.u16(6);if(count>4096)throw std::runtime_error("Audio: FSE budget");
  r.range(16,std::size_t(count)*132);areas_.reserve(count);
  for(unsigned i=0;i<count;++i) {
    const auto p=16+i*132,a=p+20;Area area;
    area.enabled=bytes[p];area.kind=bytes[p+1];area.group=bytes[p+3];area.shape=bytes[a+1];
    area.floor=r.f32(a+4);area.height=r.f32(a+8);area.radius=r.f32(a+12);
    // Non-foot payloads and AREA_TYPE_EYE are not queried by footSeCheck.
    if(area.kind==0) {
      area.surface=bytes[p+68];area.effect=bytes[p+69];area.useKind=bytes[p+71];
      if(area.shape==1 || area.shape==2)for(unsigned k=0;k<(area.shape==1?4u:1u);++k)
        area.points[k]={r.f32(a+16+k*8),r.f32(a+20+k*8)};
    }
    areas_.push_back(area);
  }
}
int FootstepSounds::find(const SoundPoint &point,unsigned useKind,unsigned group) const {
  const float x=float(point[0]),y=float(point[1])+300,z=float(point[2]);
  for(unsigned i=0;i<areas_.size();++i) {
    const auto &a=areas_[i];
    if(!(a.enabled&1) || a.kind || (group!=0xff && a.group!=group) || !(a.useKind&useKind))continue;
    if(y+100<a.floor || y>=a.floor+a.height)continue;
    if(a.shape==2) {
      const float dx=x-a.points[0][0],dz=z-a.points[0][1];
      if(std::sqrt(dx*dx+dz*dz)<a.radius)return int(i);
    } else if(a.shape==1) {
      const auto &p=a.points;
      // Keep the source cross-product order and inclusive horizontal edges.
      if((p[3][0]-p[0][0])*(z-p[0][1])>(p[3][1]-p[0][1])*(x-p[0][0]) ||
         (p[1][0]-p[0][0])*(z-p[0][1])<(p[1][1]-p[0][1])*(x-p[0][0]) ||
         (p[3][0]-p[2][0])*(z-p[2][1])<(p[3][1]-p[2][1])*(x-p[2][0]) ||
         (p[1][0]-p[2][0])*(z-p[2][1])>(p[1][1]-p[2][1])*(x-p[2][0]))continue;
      return int(i);
    }
  }
  return -1;
}
FootstepSoundSelection FootstepSounds::resolve(unsigned no,const SoundPoint &point,
    const EnvironmentData *environment,const std::array<float,3> &target,unsigned group) const {
  if(no>65535 || (group>63 && group!=0xff))throw std::runtime_error("Audio: FootSe parameters");
  for(const auto value:point)if(!std::isfinite(value) || std::abs(value)>1e7)
    throw std::runtime_error("Audio: FootSe-Position");
  FootstepSoundSelection result;result.number=no;
  if(no>=0x10 && no<=0x13)result.area=find(point,2,group);
  else {
    if(environment)if(const auto water=QueryEnvironmentWaterContact(*environment,point,target))
      if(water->footSound()) {result.water=true;result.number+=30;return result;}
    result.area=find(point,1,group);
  }
  if(result.area>=0) {
    const auto &area=areas_[unsigned(result.area)];result.number+=area.surface*30;
    if(no<=3)result.effect=area.effect;
  }
  // FlrAtInit zeroes default SE variants. The six room overrides in st1
  // change only the default visual effect, keeping SE variant zero.
  return result;
}
} // namespace re4
