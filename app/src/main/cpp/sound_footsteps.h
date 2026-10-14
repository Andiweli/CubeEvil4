#pragma once
#include "environment.h"
#include "sound_acoustics.h"
#include <vector>
namespace re4 {
class RoomArchive;
struct PlayerSequenceSoundCue {
  unsigned block{},number{},part{};
};
// cPlayer::seqSeCtrl: input is the original SEQ Se/Free byte, not Se-1.
// The optional clothing cue precedes the main foot/landing/other cue.
std::vector<PlayerSequenceSoundCue> SourceFootSoundCue(unsigned sequenceSe,
                                                      unsigned sequenceFree);
struct FootstepSoundSelection {
  unsigned number{},effect{};
  int area=-1;
  bool water{};
};
// Original FSE v0x103 / FLR_AT_DATA 0x84 and snd.cpp::footSeCheck.
// Cue randomization/existence remains the SoundMixer/SoundBank's responsibility.
class FootstepSounds {
public:
  FootstepSounds()=default;
  explicit FootstepSounds(const RoomArchive &room);
  explicit FootstepSounds(const std::vector<std::uint8_t> &fse);
  FootstepSoundSelection resolve(unsigned number,const SoundPoint &point,
      const EnvironmentData *environment=nullptr,
      const std::array<float,3> &cameraTarget={},unsigned group=0) const;
  std::size_t areaCount() const { return areas_.size(); }
private:
  struct Area {
    unsigned enabled{},kind{},group{},shape{},surface{},effect{},useKind{};
    float floor{},height{},radius{};
    std::array<std::array<float,2>,4> points{};
  };
  std::vector<Area> areas_;
  int find(const SoundPoint &point,unsigned useKind,unsigned group) const;
};
} // namespace re4
