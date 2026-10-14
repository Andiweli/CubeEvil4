#pragma once
#include "sound_bank.h"
#include <array>
#include <vector>

namespace re4 {
class RoomArchive;
using SoundPoint=std::array<double,3>;
struct SoundSpatialParameters {
  double gain=1, pitchRatio=1;
  int pan=64, filter=-1;
};
// Original room STB curves; decoded once without modifying archive bytes.
// Reverb and obstruction/inner-area overrides remain separate game systems.
class SoundAcoustics {
public:
  SoundAcoustics()=default;
  explicit SoundAcoustics(const RoomArchive &room);
  explicit SoundAcoustics(const std::vector<std::uint8_t> &stb);
  SoundSpatialParameters parameters(const SoundCue &cue,const SoundPoint &source,
      const SoundPoint &playerFeet,const SoundPoint &cameraEye,
      const SoundPoint &cameraRight) const;

private:
  struct Entry { double distance{}, value{}; };
  struct Combine { int volume=-1,pitch=-1,filter=-1; };
  std::array<Combine,32> combines_{};
  std::array<std::vector<Entry>,32> volumes_,pitches_,filters_;
  static double value(const std::vector<Entry> &curve,double distance,bool stepped);
};
} // namespace re4
