#pragma once
#include "sound_acoustics.h"
#include <cstdint>
#include <functional>
#include <vector>
namespace re4 {
class RoomArchive;
// se_at.cpp's original ESE emitter clocks, owned by the game thread. A failed
// SndCall retains the due record, exactly as in SeAtCheck; pause means no tick.
class SoundAmbient {
public:
  using Play=std::function<std::uint64_t(unsigned,unsigned,const SoundPoint &,bool)>;
  SoundAmbient()=default;
  explicit SoundAmbient(const RoomArchive &room);
  explicit SoundAmbient(const std::vector<std::uint8_t> &ese);
  void tick(const Play &play);
  bool setEnabled(unsigned number,bool enabled);
  std::size_t count() const { return emitters_.size(); }
  std::size_t pending() const;
  std::size_t unsupportedRepeats() const;
private:
  struct Emitter {
    unsigned enabled{},number{},flags{},block{},cue{},interval{},delay{},counter{},randomBase{},randomRange{};
    int calls{};
    bool unsupportedRepeat{};
    SoundPoint position{};
  };
  std::vector<Emitter> emitters_;
  std::uint16_t random_{};
  unsigned randomByte();
};
} // namespace re4
