#pragma once
#include "sound_bank.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace re4 {
// Six source scenario entries; bit 15 means resident, bit 14 auto-start.
// r101 uses MIDI bank 0/sequence 0 automatically. Its stream 2 only starts
// when the original encounter script requests it.
struct RoomMusicSelection {
  std::array<std::uint32_t,6> bgm{}, streams{};
};
RoomMusicSelection ReadRoomMusicSelection(const std::string &table,
    std::uint16_t room,unsigned scenario=0);

struct MusicInstrument {
  std::shared_ptr<const SoundSample> sample;
  unsigned unityNote{}, keyGroup{};
  int fineTune{};
  std::int32_t attenuation{};
  std::array<std::int32_t,6> envelope{}; // attack, decay, sustain, release, vel, key
};
struct MusicEvent {
  std::uint64_t milliseconds{};
  unsigned status{}, first{}, second{};
};
// Original sampled MIDI, not Android's system soundfont. Only supported SYN
// articulation features are accepted; unsupported banks fail explicitly.
class SoundSequence {
public:
  static std::shared_ptr<const SoundSequence> Load(const std::string &header,
      const std::string &data,unsigned bank,unsigned sequence=0);
  unsigned volume() const { return volume_; }
  unsigned bank() const { return bank_; }
  unsigned sequence() const { return sequence_; }
  const std::vector<MusicEvent> &events() const { return events_; }
  std::size_t loopEvent() const { return loopEvent_; }
  std::uint64_t loopMilliseconds() const { return loopMilliseconds_; }
  std::uint64_t endMilliseconds() const { return endMilliseconds_; }
  bool looping() const { return looping_; }
  const MusicInstrument *instrument(unsigned program,unsigned note) const;
  std::string describe() const;
private:
  unsigned volume_{},bank_{},sequence_{};
  std::vector<MusicEvent> events_;
  std::array<std::array<std::shared_ptr<const MusicInstrument>,128>,128> instruments_{};
  std::size_t loopEvent_{};
  std::uint64_t loopMilliseconds_{},endMilliseconds_{};
  bool looping_{};
};

// Stereo DSP streams use source SHD tables and alternating 16 KiB channel
// blocks. Only the requested stream is read from the SBB container.
struct SoundStream {
  unsigned block{}, number{}, rate{}, volume{}, pan{};
  bool stereo{}, looping{};
  std::vector<std::int16_t> left, right, loopLeft, loopRight;
  static std::shared_ptr<const SoundStream> Load(const std::string &header,
      const std::string &container,unsigned block,unsigned number);
  std::string describe() const;
};

// Owned only by SoundMixer under its mutex; no disc I/O or game-state access
// during render. PCM and event descriptions are immutable.
class MusicPlayer {
public:
  explicit MusicPlayer(std::shared_ptr<const SoundSequence> sequence);
  void sample(double &left,double &right,unsigned outputRate);
  std::size_t voiceCount() const { return voices_.size(); }
  std::uint64_t renderedFrames() const { return frames_; }
private:
  struct Channel { unsigned program{},volume=100,expression=127,pan=64,bend=8192,
      bendRange=2; bool hold{}; };
  struct Voice {
    const MusicInstrument *instrument{};
    unsigned channel{},note{},velocity{},envelopeState{};
    std::int64_t attack{},attackDelta{},attenuation{},decay{},sustain{},release{};
    double phase{},gain{},step{},left{},right{};
    bool looping{},held{},finished{};
  };
  std::shared_ptr<const SoundSequence> sequence_;
  std::array<Channel,16> channels_{};
  std::vector<Voice> voices_;
  std::uint64_t frames_{},clockFrames_{},envelopeClock_{};
  std::size_t nextEvent_{};
  void event(const MusicEvent &event);
  void noteOff(unsigned channel,unsigned note);
  void updateVoice(Voice &voice,unsigned outputRate);
  void envelopeTick(Voice &voice);
};
} // namespace re4
