#pragma once
#include "sound_bank.h"
#include "sound_acoustics.h"
#include "sound_music.h"
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>

namespace re4 {
// Disc banks are immutable. Control requests and the AudioTrack worker share
// one short mutex; neither thread accesses gameplay state from the other.
class SoundMixer {
public:
  static constexpr unsigned OutputRate = 48000;
  static constexpr unsigned MaxVoices = 64;
  static constexpr unsigned MaxStreams = 4; // Original BGM_STAT/driver work slots.
  SoundMixer();
  void installBanks(const std::vector<std::shared_ptr<const SoundBank>> &banks,
                    bool replaceRoom = false);
  std::uint64_t play(unsigned block, unsigned number, double gain = 1,
                     int panOverride = -1, bool loopAllowed = true,
                     unsigned argument = 0);
  std::uint64_t playSpatial(unsigned block,unsigned number,
      const SoundAcoustics &acoustics,const SoundPoint &source,
      const SoundPoint &playerFeet,const SoundPoint &cameraEye,
      const SoundPoint &cameraRight,bool loopAllowed=true,
      unsigned argument=0);
  void updateSpatial(std::uint64_t handle,const SoundAcoustics &acoustics,
      const SoundPoint &source,const SoundPoint &playerFeet,
      const SoundPoint &cameraEye,const SoundPoint &cameraRight);
  void stop(std::uint64_t handle);
  void setVoiceGain(std::uint64_t handle, double gain, int pan = 64);
  void installSequence(std::shared_ptr<const SoundSequence> sequence, bool restart = false);
  void installStream(std::shared_ptr<const SoundStream> stream, bool restart = false);
  std::uint64_t streamRenderedFrames() const; // Output PCM frames; frozen by pause, reset by stream install.
  // Register immutable original (block,number) streams without starting them.
  // replaceRoom replaces only block0's resident asset set; shared event assets
  // and active tracks present in the destination keep their own PCM owner.
  void installStreamAssets(const std::vector<std::shared_ptr<const SoundStream>> &streams,
                           bool replaceRoom = false);
  // SndStrReq flags1=prepare,2=play,4=volume,8=stop,0x10=immediate volume.
  // time is an AX audio-callback count (5ms), not a gameplay frame count.
  std::uint64_t requestStream(unsigned block,unsigned number,std::uint32_t flags,
                              unsigned time=0,unsigned volume=0);
  bool streamActive(unsigned block,unsigned number) const;
  bool streamReady(unsigned block,unsigned number) const;
  bool streamPlaying(unsigned block,unsigned number) const;
  bool streamStopping(unsigned block,unsigned number) const;
  std::uint64_t streamRenderedFrames(unsigned block,unsigned number) const;
  unsigned streamVolumeFixed(unsigned block,unsigned number) const;
  void installSequenceSlot(unsigned slot, std::shared_ptr<const SoundSequence> sequence, bool restart = false);
  std::uint64_t musicFrames(unsigned slot = 0) const;
  bool hasMusic() const;
  void clear();
  // snd.cpp::SndAllFadeOut: existing SE/sequence voices over2s,
  // existing streams over400 AX callbacks; newly requested cues are unaffected.
  void fadeAll();
  void setPaused(bool paused);
  void setSubscreen(bool active);
  bool hasCue(unsigned block, unsigned number,unsigned argument=0) const;
  bool voiceActive(std::uint64_t handle) const;
  std::size_t voiceCount() const;
  void render(std::int16_t *stereoPcm, std::size_t frames);

private:
  struct Voice {
    std::shared_ptr<const SoundSample> sample;
    std::shared_ptr<const SoundBank> bank;
    std::uint64_t handle{}, age{};
    unsigned block{}, cueNumber{}, priority{};
    double phase{}, step{}, baseRate{}, sourceGain{}, gain{}, targetGain{}, left{}, right{};
    double targetLeft{},targetRight{},filterB{},targetFilterB{},filterHistory{};
    bool looping{}, loopAllowed{}, pauseExempt{}, subscreenPaused{};
    unsigned fadeFrames{};
    bool fadeOut{};
  };
  struct StreamVoice {
    std::shared_ptr<const SoundStream> stream;
    std::uint64_t handle{},frames{};
    double phase{},step{},left{},right{},gain{};
    int volume{},fadeEnd{},fadeStep{}; // Original signed8.8 volume.
    bool looping{},fading{},stopRequested{},playing{};
  };
  mutable std::mutex mutex_;
  std::map<std::pair<unsigned,unsigned>, std::shared_ptr<const SoundBank>> banks_;
  // Legacy block-only requests follow the current installed bank, as the
  // original weapon-bank switch did. Enemy requests pass their explicit id.
  std::map<unsigned,unsigned> currentArguments_;
  std::vector<Voice> voices_;
  std::uint64_t serial_{};
  std::uint16_t random_=0xD37;
  std::uint16_t selectionRandom_{};
  std::map<std::tuple<unsigned,unsigned,unsigned>,unsigned> lastRandom_;
  std::array<std::shared_ptr<const SoundSequence>,2> sequences_;
  std::array<std::unique_ptr<MusicPlayer>,2> music_;
  std::array<unsigned,2> musicFadeFrames_{};
  std::array<bool,2> musicFadeOut_{};
  std::map<std::pair<unsigned,unsigned>,std::shared_ptr<const SoundStream>> streamAssets_;
  std::array<StreamVoice,MaxStreams> streams_;
  std::uint64_t streamSerial_{},legacyStreamHandle_{},legacyStreamFrames_{};
  unsigned streamAudioClock_{};
  bool subscreen_{};
  bool paused_ = true;
  double resumeGain_{};
  std::uint64_t playLocked(unsigned block,unsigned number,double gain,
      int panOverride,bool loopAllowed,const SoundAcoustics *acoustics,
      const SoundPoint &source,const SoundPoint &playerFeet,
      const SoundPoint &cameraEye,const SoundPoint &cameraRight,
      unsigned argument);
  std::shared_ptr<const SoundBank> bankLocked(unsigned block,unsigned argument) const;
  unsigned randomByte();
  unsigned selectionByte();
  StreamVoice *streamLocked(unsigned block,unsigned number);
  const StreamVoice *streamLocked(unsigned block,unsigned number) const;
  StreamVoice *startStreamLocked(std::shared_ptr<const SoundStream> stream,bool playing=true);
  void fadeStreamLocked(StreamVoice &voice,unsigned time,unsigned volume);
  void streamAudioTickLocked();
};
} // namespace re4
