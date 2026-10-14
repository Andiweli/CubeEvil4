#include "sound_ambient.h"
#include "archive.h"
#include "asset_reader.h"
#include <stdexcept>
namespace re4 {
SoundAmbient::SoundAmbient(const RoomArchive &room) {
  const auto *ese=room.find("ESE");
  if (ese) *this=SoundAmbient(room.copy(std::size_t(ese-room.entries().data())));
}
SoundAmbient::SoundAmbient(const std::vector<std::uint8_t> &ese) {
  if (ese.empty()) return;
  const AssetReader r{ese};r.range(0,16);
  if (r.u32(0)!=0x45534500 || r.u16(4)!=0x100) throw std::runtime_error("Audio: ESE version");
  const auto count=r.u16(6);
  if (count>1024) throw std::runtime_error("Audio: ESE emitter budget");
  r.range(16,std::size_t(count)*44);emitters_.reserve(count);
  for (unsigned i=0;i<count;++i) {
    const auto p=16+i*44;Emitter emitter;
    emitter.enabled=ese[p];emitter.number=ese[p+1];emitter.flags=r.u16(p+2);
    for (unsigned c=0;c<3;++c) emitter.position[c]=r.f32(p+4+c*4);
    emitter.block=r.u16(p+18);emitter.cue=r.u16(p+22);emitter.interval=r.u16(p+24);
    emitter.delay=r.u16(p+26);emitter.counter=r.u16(p+28);
    const auto calls=r.u16(p+30);emitter.calls=calls<32768 ? int(calls) : int(calls)-65536;
    emitter.randomBase=r.u16(p+32);emitter.randomRange=r.u16(p+34);
    emitter.unsupportedRepeat=!emitter.interval && !emitter.randomRange && emitter.calls!=1 && emitter.calls>=0;
    if (emitter.block>13 || emitter.cue>4095)
      throw std::runtime_error("Audio: ESE parameters");
    emitters_.push_back(emitter);
  }
}
unsigned SoundAmbient::randomByte() {
  // Original game/rnd.cpp recurrence, separate audio stream so the ambient
  // scheduler never changes future enemy/combat RNG outcomes.
  const auto old=random_;
  const unsigned next=(unsigned(std::uint8_t((old>>1)+(old>>8)))<<8)|std::uint8_t(old>>1);
  random_=std::uint16_t(next==old ? next+0x101 : next);return random_>>8;
}
void SoundAmbient::tick(const Play &play) {
  if (!play) return;
  for (auto &emitter:emitters_) {
    if (!(emitter.enabled&1) || emitter.calls<0) continue;
    if (emitter.delay) { --emitter.delay;continue; }
    if (emitter.counter) { --emitter.counter;continue; }
    if (!play(emitter.block,emitter.cue,emitter.position,!(emitter.flags&1))) continue;
    if (emitter.calls==1) { emitter.calls=-1;continue; }
    if (!emitter.interval && !emitter.randomRange) {
      // Several original ESE records have an undefined Rnd()%0 repeat period.
      // Play the actual cue once (its own authored DSP loop is preserved),
      // never invent a cadence or stack unbounded voices in the Android mixer.
      emitter.calls=-1;continue;
    }
    if (emitter.calls) --emitter.calls;
    emitter.counter=emitter.interval ? emitter.interval : emitter.randomBase+randomByte()%emitter.randomRange;
  }
}
bool SoundAmbient::setEnabled(unsigned number,bool enabled) {
  for (auto &emitter:emitters_) if (emitter.number==number) {
    if (enabled) emitter.enabled|=1;else emitter.enabled&=~1u;return true;
  }
  return false;
}
std::size_t SoundAmbient::pending() const {
  std::size_t count=0;for (const auto &emitter:emitters_) if ((emitter.enabled&1) && emitter.calls>=0) ++count;return count;
}
std::size_t SoundAmbient::unsupportedRepeats() const {
  std::size_t count=0;
  for (const auto &emitter:emitters_) if (emitter.unsupportedRepeat) ++count;
  return count;
}
} // namespace re4
