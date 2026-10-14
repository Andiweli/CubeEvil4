#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace re4 {
// GameCube DSP ADPCM: each eight-byte frame carries fourteen signed nibbles.
// Contexts and coefficients are from the disc's original WTADPCM table.
struct DspAdpcmContext {
  std::array<std::int16_t, 16> coefficients{};
  std::uint16_t predictorScale{};
  std::int16_t history1{}, history2{};
};
std::vector<std::int16_t>
DecodeDspAdpcm(const std::vector<std::uint8_t> &data, std::size_t firstSample,
               std::size_t count, DspAdpcmContext context);

struct SoundSample {
  unsigned rate{};
  std::vector<std::int16_t> pcm;
  // Independently decoded with WTADPCM's loop predictor/history. Retaining this
  // context matters when a loop starts in the middle of a DSP frame.
  std::vector<std::int16_t> loopPcm;
  std::size_t loopStart{};
};
struct SoundCue {
  std::shared_ptr<const SoundSample> sample;
  unsigned number{}, flags{}, priority{};
  unsigned surroundType{}, soundFlags{}; // SIT se_flag: bit1 ignores ordinary SE pause
  int pan = 64, volume = 127, curve = -1;
  int pitchLow{}, pitchHigh{};
  unsigned randomGroup{};
  double pitchRatio = 1;
  bool chained() const { return (flags & 0x2000) != 0; }
};

class SoundBank {
public:
  // DvdHeader tables, including their single nested level, carry the paired ISS
  // and PCM sections. No proprietary bytes are compiled into the application.
  static std::vector<std::shared_ptr<const SoundBank>>
  LoadDvd(const std::string &path);
  static std::vector<std::shared_ptr<const SoundBank>>
  ParseDvd(const std::vector<std::uint8_t> &bytes);
  unsigned block() const { return block_; }
  unsigned argument() const { return argument_; }
  std::size_t cueCount() const { return cues_.size(); }
  const SoundCue *cue(unsigned number) const;
  const std::vector<unsigned> &randomChoices(unsigned group) const;
  std::string describe() const;

private:
  unsigned block_{}, argument_{};
  std::vector<SoundCue> cues_;
  std::array<std::vector<unsigned>,32> randomGroups_;
  static std::shared_ptr<const SoundBank>
  Parse(unsigned block, unsigned argument,
        const std::vector<std::uint8_t> &iss,
        const std::vector<std::uint8_t> &pcm);
};
double SoundVolumeGain(int volume);
} // namespace re4
