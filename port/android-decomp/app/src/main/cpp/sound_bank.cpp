#include "sound_bank.h"
#include "asset_reader.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace re4 {
namespace {
constexpr std::size_t fileBudget = 64u * 1024 * 1024;
constexpr std::size_t sampleBudget = 8u * 1024 * 1024;
// game/snd_sub0.cpp::Snd_dls_vol_tbl: AX attenuation is in tenths of dB.
constexpr int volumeTable[128] = {
    -904,-842,-721,-651,-601,-562,-530,-503,-480,-460,-442,-425,-410,-396,-383,-371,
    -360,-349,-339,-330,-321,-313,-305,-297,-289,-282,-276,-269,-263,-257,-251,-245,
    -239,-234,-229,-224,-219,-214,-210,-205,-201,-196,-192,-188,-184,-180,-176,-173,
    -169,-165,-162,-158,-155,-152,-149,-145,-142,-139,-136,-133,-130,-127,-125,-122,
    -119,-116,-114,-111,-109,-106,-103,-101,-99,-96,-94,-91,-89,-87,-85,-82,
    -80,-78,-76,-74,-72,-70,-68,-66,-64,-62,-60,-58,-56,-54,-52,-50,
    -49,-47,-45,-43,-42,-40,-38,-36,-35,-33,-31,-30,-28,-27,-25,-23,
    -22,-20,-19,-17,-16,-14,-13,-11,-10,-8,-7,-6,-4,-3,-1,0};
int signedByte(std::uint8_t value) { return value < 128 ? value : int(value)-256; }
std::int16_t signedWord(std::uint16_t value) {
  return value < 32768 ? std::int16_t(value) : std::int16_t(int(value)-65536);
}
struct Section { unsigned type{}, argument{}; std::vector<std::uint8_t> bytes; };
struct Pair { Section iss, pcm; bool hasIss{}, hasPcm{}; };
void sections(const std::vector<std::uint8_t> &b, std::size_t base,
              std::size_t extent, unsigned depth, std::map<unsigned, Pair> &out) {
  const AssetReader r{b};
  if (extent < 0x400 || depth > 1)
    throw std::runtime_error("Audio: DVD header is missing");
  r.range(base, extent);
  bool ended = false;
  for (std::size_t entry = 0x20; entry < 0x400; entry += 32) {
    const auto p = base + entry;
    const auto kind = r.u32(p);
    if (kind == 0xffffffffu) { ended = true; break; }
    if (kind == 0xfffffffeu) continue;
    const auto size = r.u32(p + 4), offset = r.u32(p + 12);
    if (kind > 4 || offset < 0x400 || offset > extent || size > extent-offset)
      throw std::runtime_error("Audio: DVD section outside file bounds");
    if (kind == 4) {
      sections(b, base + offset, size, depth+1, out);
    } else if (kind == 1 || kind == 2) {
      const auto block = r.u32(p+16), argument = r.u32(p+20);
      // Enemy slot assignment needs its original loader. Reject an ambiguous
      // duplicate instead of silently replacing a different enemy's samples.
      if (block > 8) throw std::runtime_error("Audio: invalid sound bank");
      auto &pair = out[block];
      bool &present = kind == 1 ? pair.hasIss : pair.hasPcm;
      auto &section = kind == 1 ? pair.iss : pair.pcm;
      if (present) throw std::runtime_error("Audio: Duplicate sound bank");
      present = true;
      section.type = block; section.argument = argument;
      section.bytes.assign(b.begin()+base+offset, b.begin()+base+offset+size);
    }
  }
  if (!ended) throw std::runtime_error("Audio: DVD end is missing");
}
std::size_t tableEntry(const AssetReader &r, std::size_t wt,
                       std::size_t off, std::size_t index, std::size_t stride,
                       std::size_t limit) {
  if (off > limit || index > (limit-off)/stride)
    throw std::runtime_error("Audio: DLS index outside table bounds");
  const auto p = off + index*stride;
  if (p > limit || stride > limit-p)
    throw std::runtime_error("Audio: Truncated DLS entry");
  r.range(wt+p, stride);
  return wt+p;
}
} // namespace

double SoundVolumeGain(int volume) {
  if (volume <= 0) return 0;
  return std::pow(10.0, volumeTable[std::min(volume,127)]/200.0);
}

std::vector<std::int16_t>
DecodeDspAdpcm(const std::vector<std::uint8_t> &data, std::size_t first,
               std::size_t count, DspAdpcmContext context) {
  if (count > sampleBudget || first > sampleBudget || count > sampleBudget-first)
    throw std::runtime_error("Audio: ADPCM sample budget");
  const auto end = first + count;
  const auto required=end ? ((end-1)/14)*8+2+((end-1)%14)/2 : 0;
  if (required > data.size())
    throw std::runtime_error("Audio: Truncated ADPCM data");
  std::vector<std::int16_t> result;
  result.reserve(count);
  std::int64_t h1 = context.history1, h2 = context.history2;
  unsigned ps = context.predictorScale;
  for (std::size_t sample = first; sample < end; ++sample) {
    const auto frame = sample/14, inFrame = sample%14;
    if (sample != first && inFrame == 0) ps = data[frame*8];
    if (ps > 0x7f) throw std::runtime_error("Audio: invalid ADPCM predictor");
    const auto predictor = ps >> 4;
    const auto packed = data[frame*8+1+inFrame/2];
    int nibble = inFrame%2 ? packed&15 : packed>>4;
    if (nibble >= 8) nibble -= 16;
    // Multiplication avoids undefined left shifts of negative signed nibbles.
    const std::int64_t value = std::int64_t(nibble)*(std::int64_t(1)<<(ps&15))*2048 +
      std::int64_t(context.coefficients[predictor*2])*h1 +
      std::int64_t(context.coefficients[predictor*2+1])*h2 + 1024;
    // Portable arithmetic shift, including negative DSP values.
    const auto quantized = value >= 0 ? value/2048 : -((-value+2047)/2048);
    const auto decoded = std::clamp<std::int64_t>(quantized, -32768, 32767);
    result.push_back(std::int16_t(decoded)); h2=h1; h1=decoded;
  }
  return result;
}

std::vector<std::shared_ptr<const SoundBank>> SoundBank::LoadDvd(const std::string &path) {
  std::ifstream file(path, std::ios::binary|std::ios::ate);
  if (!file) throw std::runtime_error("Audio: Disc file is missing");
  const auto length = file.tellg();
  if (length < 0x400 || length > std::streamoff(fileBudget))
    throw std::runtime_error("Audio: Disc file budget");
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(bytes.data()), length))
    throw std::runtime_error("Audio: Truncated disc file");
  return ParseDvd(bytes);
}
std::vector<std::shared_ptr<const SoundBank>>
SoundBank::ParseDvd(const std::vector<std::uint8_t> &bytes) {
  if (bytes.size() > fileBudget) throw std::runtime_error("Audio: File budget");
  std::map<unsigned,Pair> parts;
  sections(bytes, 0, bytes.size(), 0, parts);
  std::vector<std::shared_ptr<const SoundBank>> result;
  for (const auto &entry : parts) {
    const auto &pair=entry.second;
    if (!pair.hasIss || !pair.hasPcm || pair.iss.argument != pair.pcm.argument)
      throw std::runtime_error("Audio: Incomplete ISS/PCM pair");
    result.push_back(Parse(entry.first,pair.iss.argument,pair.iss.bytes,pair.pcm.bytes));
  }
  return result;
}

std::shared_ptr<const SoundBank> SoundBank::Parse(unsigned block, unsigned argument,
    const std::vector<std::uint8_t> &iss, const std::vector<std::uint8_t> &pcm) {
  const AssetReader r{iss};
  // SndBlkInit skips the leading offset except for sequenced BGM blocks 3/4.
  const auto hdr = (block == 3 || block == 4) ? 0 : r.u32(0);
  r.range(hdr,16);
  const auto count=r.u32(hdr);
  const auto wt=std::size_t(hdr)+r.u32(hdr+4),
             sit=std::size_t(hdr)+r.u32(hdr+8);
  // WEP19 deliberately installs a zero-cue bank. Keep its block/argument so
  // the mixer stops resolving grenade requests against the previous weapon.
  if (count > 4096) throw std::runtime_error("Audio: SIT count");
  r.range(sit,std::size_t(count)*24); r.range(wt,24);
  if (wt >= sit) throw std::runtime_error("Audio: DLS bounds");
  const std::size_t limit=sit-wt;
  std::array<std::size_t,6> tables{};
  for (unsigned i=0;i<6;++i) {
    tables[i]=r.u32(wt+i*4);
    if (tables[i]<24 || tables[i]>limit)
      throw std::runtime_error("Audio: DLS table outside bank bounds");
    if (i && tables[i]<tables[i-1])
      throw std::runtime_error("Audio: DLS table order");
  }
  auto bank=std::shared_ptr<SoundBank>(new SoundBank);
  bank->block_=block;bank->argument_=argument;bank->cues_.resize(count);
  std::map<std::tuple<unsigned,unsigned,unsigned>,std::shared_ptr<const SoundSample>> cache;
  std::size_t decodedSamples=0;
  for (unsigned number=0;number<count;++number) {
    auto &cue=bank->cues_[number]; cue.number=number;
    const auto p=sit+number*24;
    cue.flags=r.u16(p+22);cue.priority=iss[p+4];
    if ((cue.flags&0x8000) || (cue.flags&4)) continue; // dummy / MIDI sequence
    const auto note=r.u16(p);
    const auto key=note&255u;
    if (key>=128) throw std::runtime_error("Audio: DLS note index");
    // get_dls_vol_pan indexes WTINST directly. Packed original banks
    // can share a trailing key with the next table: WEP28 cue2 reads key2 at
    // the first zero-valued region word. Bound that two-byte access by the
    // complete DLS extent, then validate its region/sample indices below.
    const auto ip=tableEntry(r,wt,tables[1],std::size_t(note>>8)*128+key,2,limit);
    const auto region=tableEntry(r,wt,tables[2],r.u16(ip),24,tables[3]);
    const auto art=tableEntry(r,wt,tables[3],r.u32(region+16),80,tables[4]);
    const auto sp=tableEntry(r,wt,tables[4],r.u32(region+20),16,tables[5]);
    const auto ap=tableEntry(r,wt,tables[5],r.u16(sp+12),46,limit);
    if (r.u16(sp)!=0) throw std::runtime_error("Audio: Sample format is not DSP ADPCM");
    const auto rate=r.u16(sp+2);
    const auto offset=r.u32(sp+4), length=r.u32(sp+8);
    const auto loopStart=r.u32(region+8), loopLength=r.u32(region+12);
    if (rate<4000 || rate>48000 || !length || length>sampleBudget ||
        loopStart>length || loopLength>length-loopStart || (offset&15))
      throw std::runtime_error("Audio: invalid sample/loop bounds");
    const auto bytes=((std::size_t(length)-1)/14)*8+2+((length-1)%14)/2;
    const auto byteOffset=std::size_t(offset)/2;
    if (byteOffset>pcm.size() || bytes>pcm.size()-byteOffset)
      throw std::runtime_error("Audio: PCM range outside bank bounds");
    const auto identity=std::make_tuple(r.u32(region+20),loopStart,loopLength);
    auto found=cache.find(identity);
    if (found == cache.end()) {
      const auto playable=loopLength ? loopStart+loopLength : length;
      if (playable+loopLength > fileBudget/2-decodedSamples)
        throw std::runtime_error("Audio: Decoded bank budget");
      decodedSamples+=playable+loopLength;
      DspAdpcmContext context;
      for (unsigned c=0;c<16;++c) context.coefficients[c]=signedWord(r.u16(ap+c*2));
      context.predictorScale=r.u16(ap+34);
      context.history1=signedWord(r.u16(ap+36));context.history2=signedWord(r.u16(ap+38));
      std::vector<std::uint8_t> encoded(pcm.begin()+byteOffset,pcm.begin()+byteOffset+bytes);
      auto sample=std::make_shared<SoundSample>(); sample->rate=rate;
      sample->pcm=DecodeDspAdpcm(encoded,0,playable,context);
      if (loopLength) {
        context.predictorScale=r.u16(ap+40);
        context.history1=signedWord(r.u16(ap+42));context.history2=signedWord(r.u16(ap+44));
        sample->loopStart=loopStart;
        sample->loopPcm=DecodeDspAdpcm(encoded,loopStart,loopLength,context);
      }
      found=cache.emplace(identity,std::move(sample)).first;
    }
    cue.sample=found->second;
    cue.pan=signedByte(iss[p+5]);cue.volume=signedByte(iss[p+6]);cue.curve=signedByte(iss[p+9]);
    cue.surroundType=iss[p+16];cue.soundFlags=iss[p+20];
    cue.randomGroup=iss[p+19];
    if (cue.randomGroup>=32) throw std::runtime_error("Audio: SE random group");
    cue.pitchLow=signedWord(r.u16(p+10));cue.pitchHigh=signedWord(r.u16(p+12));
    if (cue.pitchLow>cue.pitchHigh || cue.pitchHigh-cue.pitchLow>32767)
      throw std::runtime_error("Audio: invalid pitch range");
    if (cue.volume<0) {
      const auto attenuation=std::int32_t(r.u32(region+4))/65536;
      cue.volume=0;
      while (cue.volume<127 && attenuation>volumeTable[cue.volume]) ++cue.volume;
    }
    if (cue.pan<-1) cue.pan=std::int32_t(r.u32(art+76));
    if (cue.pan<-1 || cue.pan>127 || cue.volume>127)
      throw std::runtime_error("Audio: invalid volume/pan");
    const auto cents=(int(key)-iss[region])*100+signedWord(r.u16(region+2));
    cue.pitchRatio=std::pow(2.0,cents/1200.0);
    if (!std::isfinite(cue.pitchRatio) || cue.pitchRatio<=0)
      throw std::runtime_error("Audio: invalid pitch");
  }
  if (block!=3 && block!=4) {
    for (const auto &cue:bank->cues_) {
      if (!cue.sample || !cue.randomGroup || !bank->randomGroups_[cue.randomGroup].empty()) continue;
      const auto table=std::size_t(r.u32(4));
      r.range(table,128);
      const auto offset=r.u32(table+cue.randomGroup*4);
      if (!offset) continue;
      const auto group=table+offset;
      const auto length=r.u16(group);
      if (!length || length>4096) throw std::runtime_error("Audio: SE random group budget");
      r.range(group+4,std::size_t(length)*2);
      auto &choices=bank->randomGroups_[cue.randomGroup];
      for (unsigned j=0;j<length;++j) {
        const auto target=r.u16(group+4+j*2);
        if (target>=count || !bank->cues_[target].sample)
          throw std::runtime_error("Audio: SE random group target");
        choices.push_back(target);
      }
    }
  }
  return bank;
}
const SoundCue *SoundBank::cue(unsigned number) const {
  if (number>=cues_.size() || !cues_[number].sample) return nullptr;
  return &cues_[number];
}
const std::vector<unsigned> &SoundBank::randomChoices(unsigned group) const {
  static const std::vector<unsigned> empty;
  return group<32 ? randomGroups_[group] : empty;
}
std::string SoundBank::describe() const {
  std::size_t available=0;
  for (const auto &cue:cues_) if (cue.sample) ++available;
  std::ostringstream out;
  out<<"Original audio: Bank "<<block_<<" · "<<available<<"/"<<cues_.size()
     <<" SIT · DSP-ADPCM";
  return out.str();
}
} // namespace re4
