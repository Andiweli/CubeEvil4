#include "sound_acoustics.h"
#include "archive.h"
#include "asset_reader.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace re4 {
namespace {
int signedByte(unsigned value) { return value<128 ? int(value) : int(value)-256; }
int signedWord(unsigned value) { return value<32768 ? int(value) : int(value)-65536; }
int selector(unsigned value) {
  const auto index=signedByte(value);
  if (index<-1 || index>=32) throw std::runtime_error("Audio: STB curve index");
  return index;
}
} // namespace
SoundAcoustics::SoundAcoustics(const RoomArchive &room) {
  const auto *stb=room.find("STB");
  if (stb) *this=SoundAcoustics(room.copy(std::size_t(stb-room.entries().data())));
}
SoundAcoustics::SoundAcoustics(const std::vector<std::uint8_t> &stb) {
  if (stb.empty()) return;
  const AssetReader r{stb};r.range(0,0x240);
  for (unsigned i=0;i<32;++i) {
    const auto combine=r.u32(0x40+i*4);
    if (combine) {
      r.range(combine,8);
      // SndCall uses vol_tbl[1], pitch_tbl[1], filter_tbl[1] in stereo mode.
      combines_[i]={selector(stb[combine+1]),selector(stb[combine+3]),selector(stb[combine+5])};
    }
    for (unsigned kind=0;kind<3;++kind) {
      const auto offset=r.u32(0xc0+kind*0x80+i*4);
      if (!offset) continue;
      r.range(offset,8);
      const auto count=r.u32(offset);
      const auto scale=r.f32(offset+4);
      if (!count || count>4096 || scale<=0)
        throw std::runtime_error("Audio: STB curve budget/scale");
      r.range(offset+8,std::size_t(count)*8);
      auto &entries=kind==0 ? volumes_[i] : kind==1 ? pitches_[i] : filters_[i];
      entries.reserve(count);
      for (unsigned j=0;j<count;++j) {
        const auto p=offset+8+j*8;
        const double distance=r.f32(p)*double(scale);
        const auto raw=r.u16(p+6);
        const double level=kind==1 ? signedWord(raw)*100.0 : raw;
        if (!std::isfinite(distance) || distance<0 ||
            (!entries.empty() && distance<=entries.back().distance) ||
            (kind==0 && raw>128) || (kind==2 && raw>23))
          throw std::runtime_error("Audio: invalid STB curve");
        entries.push_back({distance,level});
      }
    }
  }
}
double SoundAcoustics::value(const std::vector<Entry> &curve,double distance,bool stepped) {
  if (curve.empty()) return 0;
  const auto next=std::upper_bound(curve.begin(),curve.end(),distance,
      [](double v,const Entry &e){return v<e.distance;});
  if (next==curve.begin()) return next->value;
  if (next==curve.end()) return curve.back().value;
  // Original filter lookup uses the first threshold strictly after distance.
  if (stepped) return next->value;
  const auto &previous=*(next-1);
  return previous.value+(next->value-previous.value)*
      (distance-previous.distance)/(next->distance-previous.distance);
}
SoundSpatialParameters SoundAcoustics::parameters(const SoundCue &cue,
    const SoundPoint &source,const SoundPoint &player,const SoundPoint &eye,
    const SoundPoint &right) const {
  for (const auto *p:{&source,&player,&eye,&right})
    for (auto v:*p) if (!std::isfinite(v))
      throw std::runtime_error("Audio: invalid listener position");
  SoundSpatialParameters result;
  result.pan=cue.pan<0 ? 64 : cue.pan;
  if (cue.surroundType==1) return result;
  if (cue.pan<0) {
    const auto horizontal=std::hypot(right[0],right[2]);
    if (horizontal>1e-9) {
      // getCam2SndAngle inverts [camera column0, up, cross(column0,up)].
      const auto dx=source[0]-player[0],dz=source[2]-player[2];
      const auto outX=(dx*right[0]+dz*right[2])/(horizontal*horizontal);
      const auto outZ=(-dx*right[2]+dz*right[0])/(horizontal*horizontal);
      const auto angle=std::atan2(outX,-outZ);
      auto folded=std::abs(angle);
      if (folded>=1.5707964) folded=3.1415927-folded;
      result.pan=std::clamp(int(angle>=0 ? folded*40.743664+64 : 64-folded*40.743664),0,127);
    }
  }
  if (cue.curve<0 || cue.curve>=32) return result;
  const auto &combine=combines_[cue.curve];
  const auto distance=std::sqrt((source[0]-eye[0])*(source[0]-eye[0])+
      (source[1]-eye[1])*(source[1]-eye[1])+(source[2]-eye[2])*(source[2]-eye[2]));
  if (combine.volume>=0 && !volumes_[combine.volume].empty()) {
    const auto volume=std::clamp(int(cue.volume*value(volumes_[combine.volume],distance,false)/128),0,127);
    const auto base=SoundVolumeGain(cue.volume);
    result.gain=base>0 ? SoundVolumeGain(volume)/base : 0;
  }
  if (combine.pitch>=0 && !pitches_[combine.pitch].empty()) {
    const auto cents=int(value(pitches_[combine.pitch],distance,false));
    result.pitchRatio=std::pow(2.0,cents/1200.0);
  }
  if (combine.filter>=0 && !filters_[combine.filter].empty())
    result.filter=int(value(filters_[combine.filter],distance,true));
  return result;
}
} // namespace re4
