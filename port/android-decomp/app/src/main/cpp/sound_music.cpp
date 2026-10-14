#include "sound_music.h"
#include "asset_reader.h"
#include "sound_synth_tables.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace re4 {
namespace {
constexpr std::size_t tableBudget=1024*1024, dataBudget=16*1024*1024;
constexpr std::int32_t noTime=std::numeric_limits<std::int32_t>::min();
std::int16_t s16(std::uint16_t v) { return v<32768 ? std::int16_t(v) : std::int16_t(int(v)-65536); }
std::int32_t s32(std::uint32_t v) { return v<=0x7fffffff ? std::int32_t(v) : std::int32_t(std::int64_t(v)-0x100000000ll); }
std::vector<std::uint8_t> read(const std::string &path,std::size_t budget) {
  std::ifstream in(path,std::ios::binary|std::ios::ate);
  if (!in) throw std::runtime_error("Original music: File missing: "+path);
  const auto size=in.tellg();
  if (size<=0 || size>std::streamoff(budget)) throw std::runtime_error("Original music: File budget");
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));in.seekg(0);
  if (!in.read(reinterpret_cast<char *>(bytes.data()),size)) throw std::runtime_error("Original music: Truncated file");
  return bytes;
}
std::vector<std::uint8_t> rangeRead(const std::string &path,std::size_t offset,std::size_t size) {
  if (!size || size>dataBudget) throw std::runtime_error("Original music: Range budget");
  std::ifstream in(path,std::ios::binary|std::ios::ate);
  if (!in) throw std::runtime_error("Original music: Container missing: "+path);
  const auto end=in.tellg();
  if (end<0 || offset>std::uint64_t(end) || size>std::uint64_t(end)-offset)
    throw std::runtime_error("Original music: Range outside container bounds");
  std::vector<std::uint8_t> result(size);in.seekg(std::streamoff(offset));
  if (!in.read(reinterpret_cast<char *>(result.data()),std::streamsize(size)))
    throw std::runtime_error("Original music: Truncated container");
  return result;
}
std::size_t nibbleSample(std::uint32_t nibble) {
  if (nibble%16<2) throw std::runtime_error("Original music: Loop point in DSP frame header");
  return std::size_t(nibble/16)*14+nibble%16-2;
}
std::uint32_t delta(const std::vector<std::uint8_t> &data,std::size_t &pos) {
  std::uint32_t result=0;
  for (unsigned i=0;i<4;++i) {
    if (pos>=data.size()) throw std::runtime_error("Original music: Truncated delta");
    const auto byte=data[pos++];result=(result<<7)|(byte&127);
    if (!(byte&128)) return result;
  }
  throw std::runtime_error("Original music: Delta exceeds MIDI VLQ bounds");
}
std::int64_t envelopeTime(std::int32_t scale,std::int32_t modulation,unsigned key) {
  if (scale==noTime) return 0;
  // lib/synenv.c uses single precision and its authored __SYNn128 table.
  const auto exponent=modulation==noTime ? float(scale)/(65536*1200.0f) :
      (float(scale)+float(modulation)*SynthNormalizedKey[key])/(65535*1200.0f);
  const auto value=1000.0f*std::pow(2.0f,exponent);
  // float rounds INT32_MAX up to 2^31. Widen the calculated value so the
  // exact source limit is checked without rounding the integer bound.
  if (!std::isfinite(value) ||
      static_cast<double>(value) > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
    throw std::runtime_error("Original music: SYN envelope time");
  return std::int64_t(value);
}
double controllerGain(unsigned value) { return std::pow(10.0,SynthVolumeAttenuation[value]/(65536*200.0)); }
} // namespace

RoomMusicSelection ReadRoomMusicSelection(const std::string &path,
    std::uint16_t room,unsigned scenario) {
  const auto data=read(path,tableBudget);const AssetReader r{data};r.range(0,8);
  const auto offsets=r.u32(0),list=r.u32(4);RoomMusicSelection result;
  // snd.cpp::SndBgmTblInit / SndBgmTblSet: offsets are relative to their table.
  for (unsigned i=0;i<512;++i) {
    const auto id=r.u16(std::size_t(list)+i*2);
    if (id==0xffff) return result;
    if (id!=room) continue;
    const auto record=std::size_t(offsets)+r.u32(std::size_t(offsets)+i*4);
    const auto count=r.u32(record);
    if (!count || count>256) throw std::runtime_error("Original music: Scenario table budget");
    r.range(record+4,std::size_t(count)*52);
    for (unsigned j=0;j<count;++j) {
      const auto p=record+4+j*52;
      if (r.u32(p)!=scenario) continue;
      for (unsigned k=0;k<6;++k) {
        result.bgm[k]=r.u32(p+4+k*4);result.streams[k]=r.u32(p+28+k*4);
      }
      return result;
    }
    throw std::runtime_error("Original music: Scenario table is missing");
  }
  throw std::runtime_error("Original music: Room list has no end");
}

std::shared_ptr<const SoundSequence> SoundSequence::Load(const std::string &header,
    const std::string &file,unsigned bankNumber,unsigned sequenceNumber) {
  const auto hed=read(header,tableBudget);const AssetReader hr{hed};
  const auto begin=hr.u32(std::size_t(bankNumber)*4);
  auto end=hr.u32(std::size_t(bankNumber+1)*4);
  // The final source bank has a zero successor in the padded HED table.
  // Its real extent is the end of bio4midi.dat, not an absent bank.
  if (!end && begin) {
    std::ifstream dataFile(file,std::ios::binary|std::ios::ate);
    if (!dataFile || dataFile.tellg()<0 || std::uint64_t(dataFile.tellg())>0xffffffffu)
      throw std::runtime_error("Original music: MIDI container bounds");
    end=std::uint32_t(dataFile.tellg());
  }
  if (end<=begin || end-begin>dataBudget) throw std::runtime_error("Original music: MIDI bank bounds");
  const auto dvd=rangeRead(file,begin,end-begin);const AssetReader dr{dvd};dr.range(0,0x400);
  std::vector<std::uint8_t> iss,pcm;unsigned originalBlock=0;
  for (std::size_t p=0x20;p<0x400;p+=32) {
    const auto kind=dr.u32(p);
    if (kind==0xffffffffu) break;
    if (kind==0xfffffffeu) continue;
    const auto length=dr.u32(p+4),offset=dr.u32(p+12);dr.range(offset,length);
    if (kind==1) { iss.assign(dvd.begin()+offset,dvd.begin()+offset+length);originalBlock=dr.u32(p+16); }
    if (kind==2) pcm.assign(dvd.begin()+offset,dvd.begin()+offset+length);
  }
  if (iss.empty() || pcm.empty() || (originalBlock!=3 && originalBlock!=4))
    throw std::runtime_error("Original music: MIDI ISS/PCM pair is missing");
  const AssetReader r{iss};r.range(0,16);
  const auto sitCount=r.u32(0);
  const auto wt=std::size_t(r.u32(4)),sit=std::size_t(r.u32(8)),seq=std::size_t(r.u32(12));
  if (sequenceNumber>=sitCount) throw std::runtime_error("Original music: MIDI sequence is missing");
  const auto sitEntry=sit+sequenceNumber*24;r.range(sitEntry,24);
  if (!(r.u16(sitEntry+22)&4)) throw std::runtime_error("Original music: SIT is not a sequence");
  const auto sequenceBank=unsigned(r.u16(sitEntry)>>8);
  if (sequenceBank>=r.u32(seq)) throw std::runtime_error("Original music: Sequence index");
  std::size_t pos=seq+r.u32(seq+4+sequenceBank*4);r.range(pos,1);
  auto result=std::shared_ptr<SoundSequence>(new SoundSequence);
  result->bank_=bankNumber;result->sequence_=sequenceNumber;
  result->volume_=iss[sitEntry+6];
  if (result->volume_>127) throw std::runtime_error("Original music: Sequence volume");
  std::array<unsigned,16> programs{};std::set<std::pair<unsigned,unsigned>> required;
  std::uint64_t millis=0;unsigned tempo=1000;bool loopSet=false,ended=false;
  auto wait=delta(iss,pos);
  for (unsigned events=0;events<65536;++events) {
    // snd_seq1::seq_one_msec counts delta*tempo down by 480 each ms.
    millis+=(std::uint64_t(wait)*tempo+479)/480;
    if (millis>60*60*1000) throw std::runtime_error("Original music: Sequence time budget");
    r.range(pos,1);const unsigned status=iss[pos++],op=status&0xf0,ch=status&15;
    MusicEvent event{millis,status,0,0};
    if (status==0xff) {
      r.range(pos,1);const auto meta=iss[pos++]; const auto length=delta(iss,pos);r.range(pos,length);
      if (meta==0x51 && length==3) {
        tempo=(unsigned(iss[pos])<<16|unsigned(iss[pos+1])<<8|iss[pos+2])/1000;
        if (!tempo) throw std::runtime_error("Original music: Zero tempo");
      } else if (meta==0x2f && !length) { result->endMilliseconds_=millis;ended=true;break; }
      else throw std::runtime_error("Original music: Unsupported MIDI meta event");
      pos+=length;
    } else {
      if (op!=0x80 && op!=0x90 && op!=0xb0 && op!=0xc0 && op!=0xe0)
        throw std::runtime_error("Original music: Unsupported MIDI event");
      r.range(pos,op==0xc0 ? 1 : 2);event.first=iss[pos++];
      if (op!=0xc0) event.second=iss[pos++];
      if (event.first>127 || event.second>127) throw std::runtime_error("Original music: MIDI data range");
      if (op==0xc0) programs[ch]=event.first;
      if (op==0x90 && event.second) {
        if (ch==9) throw std::runtime_error("Original music: SYN drums not ported yet");
        required.emplace(programs[ch],event.first);
      }
      if (op==0xb0 && event.first==0x66) {
        result->loopEvent_=result->events_.size();result->loopMilliseconds_=millis;loopSet=true;
      } else if (op==0xb0 && event.first==0x67) {
        result->looping_=true;result->endMilliseconds_=millis;
        if (!loopSet) { result->loopEvent_=0;result->loopMilliseconds_=0; }
        if (result->endMilliseconds_<=result->loopMilliseconds_ || result->loopEvent_>=result->events_.size())
          throw std::runtime_error("Original music: Empty MIDI loop");
        ended=true;break;
      } else {
        if (op==0xb0 && event.first!=1 && event.first!=6 && event.first!=0x26 && event.first!=7 &&
            event.first!=10 && event.first!=11 && event.first!=0x40 && event.first!=0x5b && event.first!=0x5c)
          throw std::runtime_error("Original music: Unsupported MIDI controller");
        result->events_.push_back(event);
      }
    }
    wait=delta(iss,pos);
  }
  if (!ended || result->events_.empty()) throw std::runtime_error("Original music: Sequence end is missing");
  r.range(wt,24);std::array<std::size_t,6> tables{};
  for (unsigned i=0;i<6;++i) { tables[i]=r.u32(wt+i*4);if (tables[i]<24 || wt+tables[i]>sit || (i && tables[i]<tables[i-1])) throw std::runtime_error("Original music: DLS table"); }
  std::map<std::tuple<unsigned,unsigned,unsigned>,std::shared_ptr<const SoundSample>> samples;
  std::size_t decoded=0;
  const auto entry=[&](unsigned table,std::size_t index,std::size_t stride) {
    const auto begin=tables[table],limit=table==5 ? sit-wt : tables[table+1];
    if (begin>limit || index>=(limit-begin)/stride) throw std::runtime_error("Original music: DLS-Index");
    const auto p=wt+begin+index*stride;r.range(p,stride);return p;
  };
  for (const auto &note:required) {
    const auto region=entry(2,r.u16(entry(1,std::size_t(note.first)*128+note.second,2)),24);
    const auto art=entry(3,r.u32(region+16),80),sp=entry(4,r.u32(region+20),16),ap=entry(5,r.u16(sp+12),46);
    // Do not silently replace unsupported LFO/pitch-envelope/drum synthesis.
    for (unsigned field:{2u,3u,4u,5u,18u}) if (s32(r.u32(art+field*4))) throw std::runtime_error("Original music: SYN LFO/pitch envelope not ported yet");
    if (iss[region+1]) throw std::runtime_error("Original music: SYN key group not ported yet");
    if (r.u16(sp)) throw std::runtime_error("Original music: No DSP instrument");
    const auto rate=r.u16(sp+2);
    const auto offset=r.u32(sp+4),length=r.u32(sp+8),start=r.u32(region+8),loop=r.u32(region+12);
    if (rate<4000 || rate>48000 || !length || length>8*1024*1024 || start>length || loop>length-start || (offset&15)) throw std::runtime_error("Original music: DLS sample bounds");
    const auto identity=std::make_tuple(r.u32(region+20),start,loop);auto sample=samples.find(identity);
    if (sample==samples.end()) {
      const auto playable=loop ? start+loop : length;decoded+=playable+loop;
      if (decoded>16*1024*1024) throw std::runtime_error("Original music: Decoded DLS budget");
      const auto size=((std::size_t(length)-1)/14)*8+2+((length-1)%14)/2,byte=std::size_t(offset)/2;
      if (byte>pcm.size() || size>pcm.size()-byte) throw std::runtime_error("Original music: DLS PCM bounds");
      const std::vector<std::uint8_t> encoded(pcm.begin()+byte,pcm.begin()+byte+size);DspAdpcmContext context;
      for (unsigned c=0;c<16;++c) context.coefficients[c]=s16(r.u16(ap+c*2));
      context.predictorScale=r.u16(ap+34);context.history1=s16(r.u16(ap+36));context.history2=s16(r.u16(ap+38));
      auto output=std::make_shared<SoundSample>();output->rate=rate;output->pcm=DecodeDspAdpcm(encoded,0,playable,context);
      if (loop) {
        context.predictorScale=r.u16(ap+40);context.history1=s16(r.u16(ap+42));context.history2=s16(r.u16(ap+44));
        output->loopStart=start;output->loopPcm=DecodeDspAdpcm(encoded,start,loop,context);
      }
      sample=samples.emplace(identity,std::move(output)).first;
    }
    auto instrument=std::make_shared<MusicInstrument>();instrument->sample=sample->second;
    instrument->unityNote=iss[region];instrument->fineTune=s16(r.u16(region+2));instrument->attenuation=s32(r.u32(region+4));
    for (unsigned i=0;i<6;++i) instrument->envelope[i]=s32(r.u32(art+24+i*4));
    if (instrument->unityNote>127 || instrument->attenuation>0 || instrument->attenuation<-0x3c00000 ||
        instrument->envelope[2]>0x10000 || instrument->envelope[2]<-0x3c00000 || instrument->envelope[3]>0)
      throw std::runtime_error("Original music: SYN level/envelope range");
    // Validate time exponents off the AudioTrack thread. All runtime keys and
    // velocities lie between these bounds; no decoder exception is deferred.
    for (unsigned endpoint:{0u,127u}) {
      envelopeTime(instrument->envelope[0],instrument->envelope[4],endpoint);
      envelopeTime(instrument->envelope[1],instrument->envelope[5],endpoint);
    }
    result->instruments_[note.first][note.second]=std::move(instrument);
  }
  return result;
}
const MusicInstrument *SoundSequence::instrument(unsigned program,unsigned note) const {
  return program<128 && note<128 ? instruments_[program][note].get() : nullptr;
}
std::string SoundSequence::describe() const {
  std::ostringstream out;out<<"Original MIDI "<<bank_<<'/'<<sequence_<<" · "<<events_.size()<<" events · SYN/DSP";return out.str();
}

std::shared_ptr<const SoundStream> SoundStream::Load(const std::string &header,
    const std::string &container,unsigned block,unsigned number) {
  if (block>1) throw std::runtime_error("Original music: Stream block");
  const auto data=read(header,tableBudget);const AssetReader r{data};const auto base=std::size_t(r.u32(block*4));r.range(base,12);
  if (number>=r.u32(base)) throw std::runtime_error("Original music: Stream number");
  const auto rit=base+r.u32(base+8)+number*16;r.range(rit,16);
  const auto sbh=base+r.u32(base+4),shd=sbh+r.u32(sbh+r.u16(rit)*4);r.range(shd,0x7c);
  auto result=std::make_shared<SoundStream>();result->block=block;result->number=number;
  const auto flags=r.u32(shd);result->stereo=flags&1;result->looping=flags&4;
  result->rate=r.u32(shd+12);result->volume=data[rit+4];result->pan=data[rit+5];
  if (flags>15 || result->rate<4000 || result->rate>48000 || result->volume>127 || result->pan>127)
    throw std::runtime_error("Original music: SHD/RIT-Parameter");
  const auto samples=std::size_t(r.u32(shd+4));const auto nibbles=r.u32(shd+8);
  const auto start=nibbleSample(r.u32(shd+16)),loop=nibbleSample(r.u32(shd+20)),end=nibbleSample(r.u32(shd+24))+1;
  if (start || !samples || samples>8*1024*1024 || end>samples || loop>=end || nibbles<(end/14)*16)
    throw std::runtime_error("Original music: SHD sample bounds");
  const auto channelBlock=result->stereo ? 0x4000u : 0x8000u;
  const auto one=std::size_t(channelBlock)*(result->stereo ? 2 : 1);
  const auto channelBytes=(std::size_t(nibbles)+1)/2;
  const auto blocks=(channelBytes+channelBlock-1)/channelBlock;
  const auto encoded=rangeRead(container,r.u32(shd+28),blocks*one);
  std::vector<std::uint8_t> channels[2];
  for (unsigned c=0;c<(result->stereo ? 2u : 1u);++c) {
    channels[c].reserve(blocks*channelBlock);
    for (std::size_t b=0;b<blocks;++b) {
      const auto p=b*one+c*channelBlock;
      channels[c].insert(channels[c].end(),encoded.begin()+p,encoded.begin()+p+channelBlock);
    }
    DspAdpcmContext context;
    // snd_str3 uses the first eight SHD coefficients as A0 and the next
    // eight as A1; this differs from the DLS WTADPCM table's layout.
    for (unsigned i=0;i<8;++i) {
      context.coefficients[i*2]=s16(r.u16(shd+0x20+c*0x20+i*2));
      context.coefficients[i*2+1]=s16(r.u16(shd+0x20+c*0x20+(i+8)*2));
    }
    context.predictorScale=r.u16(shd+0x64+c*2);context.history1=s16(r.u16(shd+0x68+c*2));context.history2=s16(r.u16(shd+0x6c+c*2));
    auto &initial=c ? result->right : result->left;initial=DecodeDspAdpcm(channels[c],0,end,context);
    if (result->looping) {
      context.predictorScale=r.u16(shd+0x70+c*2);context.history1=s16(r.u16(shd+0x74+c*2));context.history2=s16(r.u16(shd+0x78+c*2));
      auto &repeat=c ? result->loopRight : result->loopLeft;repeat=DecodeDspAdpcm(channels[c],loop,end-loop,context);
    }
  }
  return result;
}
std::string SoundStream::describe() const {
  std::ostringstream out;out<<"Original stream "<<block<<'/'<<number<<" · "<<rate<<" Hz · "<<(stereo ? "Stereo" : "Mono");return out.str();
}

MusicPlayer::MusicPlayer(std::shared_ptr<const SoundSequence> sequence):sequence_(std::move(sequence)) {
  if (!sequence_) throw std::runtime_error("Original music: Empty sequence");
  voices_.reserve(64);
}
void MusicPlayer::noteOff(unsigned channel,unsigned note) {
  for (auto &v:voices_) if (v.channel==channel && v.note==note && v.envelopeState!=3) {
    if (channels_[channel].hold) v.held=true;
    else v.envelopeState=3;
  }
}
void MusicPlayer::event(const MusicEvent &e) {
  const auto channel=e.status&15,op=e.status&0xf0;auto &c=channels_[channel];
  if (op==0xc0) c.program=e.first;
  else if (op==0xe0) c.bend=e.first+(e.second<<7);
  else if (op==0xb0) {
    if (e.first==7) c.volume=e.second;
    if (e.first==10) c.pan=e.second;
    if (e.first==11) c.expression=e.second;
    if (e.first==6) c.bendRange=e.second;
    if (e.first==0x40) {
      c.hold=e.second>=64;
      if (!c.hold) for (auto &v:voices_) if (v.channel==channel && v.held) { v.held=false;v.envelopeState=3; }
    }
    // Reverb/chorus are separate AXFX buses, not fabricated replacement notes.
  } else if (op==0x80 || (op==0x90 && !e.second)) noteOff(channel,e.first);
  else if (op==0x90) {
    const auto *instrument=sequence_->instrument(c.program,e.first);if (!instrument) return;
    for (auto &old:voices_) if (old.channel==channel && old.note==e.first) old.envelopeState=3;
    if (voices_.size()==64) voices_.erase(voices_.begin());
    Voice v;v.instrument=instrument;v.channel=channel;v.note=e.first;v.velocity=e.second;
    const auto &envelope=instrument->envelope;v.sustain=envelope[2];v.release=envelope[3];
    if (envelope[0]==noTime) {
      v.envelopeState=1;v.attenuation=0;
      if (envelope[1]==noTime) { v.envelopeState=2;v.attenuation=v.sustain; }
    } else {
      v.envelopeState=0;v.attenuation=-0x3c00000;
      v.attackDelta=0x640000/std::max<std::int64_t>(1,envelopeTime(envelope[0],envelope[4],v.velocity)/5);
    }
    if (v.envelopeState<2) v.decay=-0x3c00000/std::max<std::int64_t>(1,envelopeTime(envelope[1],envelope[5],v.note)/5);
    voices_.push_back(v);
  }
}
void MusicPlayer::envelopeTick(Voice &v) {
  if (v.envelopeState==0) {
    v.attack+=v.attackDelta;
    if (v.attack>=0x630000) { v.attenuation=0;v.envelopeState=1; }
    else {
      const auto index=v.attack>>16;
      v.attenuation=index<=0 ? -0x3c00000 : SynthAttackAttenuation[std::size_t(index)];
    }
  } else if (v.envelopeState==1) {
    v.attenuation+=v.decay;
    if (v.attenuation<=v.sustain) { v.attenuation=v.sustain;v.envelopeState=2; }
    if (v.attenuation<=-0x2d00000) v.finished=true;
  } else if (v.envelopeState==3) {
    if (v.attenuation<=-0x2d00000) v.finished=true;
    else v.attenuation+=v.release;
  }
}
void MusicPlayer::updateVoice(Voice &v,unsigned rate) {
  const auto &c=channels_[v.channel];
  const auto cents=(int(v.note)-int(v.instrument->unityNote))*100+v.instrument->fineTune+
      (int(c.bend)-8192)*double(c.bendRange)*100/8192;
  v.step=std::min(128000.0,v.instrument->sample->rate*std::pow(2.0,cents/1200.0))/rate;
  v.gain=controllerGain(v.velocity)*controllerGain(c.volume)*controllerGain(c.expression)*
      std::pow(10.0,(v.instrument->attenuation+double(v.attenuation))/(65536*200))*SoundVolumeGain(sequence_->volume());
  const auto angle=c.pan*3.14159265358979323846/(2*127);v.left=std::cos(angle);v.right=std::sin(angle);
}
void MusicPlayer::sample(double &left,double &right,unsigned outputRate) {
  if (!outputRate) return;
  const auto &events=sequence_->events();
  const auto atEnd=clockFrames_*1000>=sequence_->endMilliseconds()*outputRate;
  if (atEnd && sequence_->looping()) {
    clockFrames_-=((sequence_->endMilliseconds()-sequence_->loopMilliseconds())*outputRate)/1000;
    nextEvent_=sequence_->loopEvent();
  }
  while (nextEvent_<events.size() && events[nextEvent_].milliseconds*outputRate<=clockFrames_*1000)
    event(events[nextEvent_++]);
  // SYN runs its envelopes every 5 ms. This clock is independent of a game's
  // frame rate, AudioTrack buffer size, pause or EGL surface recreation.
  if (envelopeClock_>=outputRate*5u) {
    envelopeClock_-=outputRate*5u;
    for (auto &voice:voices_) { envelopeTick(voice);updateVoice(voice,outputRate); }
  }
  for (auto &v:voices_) {
    if (v.finished) continue;
    if (!v.step) updateVoice(v,outputRate);
    const auto &sample=*v.instrument->sample;
    const auto *pcm=v.looping ? &sample.loopPcm : &sample.pcm;
    if (v.phase>=pcm->size()) {
      if (sample.loopPcm.empty()) { v.finished=true;continue; }
      v.phase=std::fmod(v.phase-pcm->size(),double(sample.loopPcm.size()));v.looping=true;pcm=&sample.loopPcm;
    }
    const auto index=std::size_t(v.phase);if (index>=pcm->size()) continue;
    auto next=(*pcm)[index];
    if (index+1<pcm->size()) next=(*pcm)[index+1];
    else if (!sample.loopPcm.empty()) next=sample.loopPcm.front();
    const auto value=(*pcm)[index]+(next-(*pcm)[index])*(v.phase-index);
    left+=value*v.gain*v.left;right+=value*v.gain*v.right;v.phase+=v.step;
  }
  voices_.erase(std::remove_if(voices_.begin(),voices_.end(),[](const Voice &v){return v.finished;}),voices_.end());
  ++frames_;++clockFrames_;envelopeClock_+=1000;
}
} // namespace re4
