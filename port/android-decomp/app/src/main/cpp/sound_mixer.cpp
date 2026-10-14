#include "sound_mixer.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace re4 {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double masterGain = .7;
constexpr unsigned lpfFeedback[24]={
  0x15F6,0x178E,0x1B9C,0x224C,0x29E7,0x3285,0x3C98,0x45A5,
  0x4E3A,0x56DB,0x5DBB,0x63AF,0x693F,0x6D6D,0x70E7,0x740A,
  0x7656,0x7835,0x79B9,0x7B12,0x7C0A,0x7CD2,0x7D82,0x7E01};
double filterFeedback(int index) {
  if (index<0) return 0;
  if (index>23) throw std::runtime_error("Audio: Filter index");
  // Source coefficients process AX's 32 kHz output. Preserve their decay time
  // at the Android mixer's 48 kHz output rather than changing the cutoff.
  return std::pow(lpfFeedback[index]/32768.0,32000.0/re4::SoundMixer::OutputRate);
}
void validateGain(double gain, int pan) {
  if (!std::isfinite(gain) || gain<0 || gain>4 || pan<-1 || pan>127)
    throw std::runtime_error("Audio: invalid blend parameters");
}
} // namespace
SoundMixer::SoundMixer() { voices_.reserve(MaxVoices); }
void SoundMixer::installBanks(
    const std::vector<std::shared_ptr<const SoundBank>> &banks, bool replaceRoom) {
  for (const auto &bank : banks)
    if (!bank) throw std::runtime_error("Audio: Empty sound bank");
  std::lock_guard<std::mutex> lock(mutex_);
  if (replaceRoom) {
    for(auto bank=banks_.begin();bank!=banks_.end();) {
      if(bank->first.first==5 || bank->first.first==6)bank=banks_.erase(bank);
      else ++bank;
    }
    currentArguments_.erase(5);currentArguments_.erase(6);
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
                               [](const Voice &v) { return v.block==5 || v.block==6; }),
                  voices_.end());
  }
  for (const auto &bank : banks) {
    banks_[{bank->block(),bank->argument()}]=bank;
    currentArguments_[bank->block()]=bank->argument();
  }
  for (const auto &bank : banks)
    for (auto last=lastRandom_.begin();last!=lastRandom_.end();)
      if (std::get<0>(last->first)==bank->block() && std::get<1>(last->first)==bank->argument()) last=lastRandom_.erase(last);
      else ++last;
}
std::uint64_t SoundMixer::play(unsigned block, unsigned number, double gain,
                               int panOverride, bool loopAllowed,unsigned argument) {
  validateGain(gain,panOverride);
  std::lock_guard<std::mutex> lock(mutex_);
  return playLocked(block,number,gain,panOverride,loopAllowed,nullptr,{},{},{},{},argument);
}
std::uint64_t SoundMixer::playSpatial(unsigned block,unsigned number,
    const SoundAcoustics &acoustics,const SoundPoint &source,
    const SoundPoint &player,const SoundPoint &eye,const SoundPoint &right,
    bool loopAllowed,unsigned argument) {
  std::lock_guard<std::mutex> lock(mutex_);
  return playLocked(block,number,1,-1,loopAllowed,&acoustics,source,player,eye,right,argument);
}
unsigned SoundMixer::randomByte() {
  // snd_sub0::Snd_rnd reads the high/low bytes in GameCube big-endian order.
  const auto old=random_;
  const auto product=std::uint16_t(old*3);
  const auto high=product>>8;
  const auto low=(unsigned(old&255)+high)&255;
  const auto next=std::uint16_t((high<<8)|low);
  random_=old==next ? std::uint16_t(old+1) : next;
  return low;
}
unsigned SoundMixer::selectionByte() {
  // Same recurrence as game/rnd.cpp, with an owned stream so the consumer
  // cannot race or perturb the gameplay generator used by combat/spawns.
  const auto r=selectionRandom_;
  const unsigned next=(unsigned(std::uint8_t((r>>1)+(r>>8)))<<8)|std::uint8_t(r>>1);
  const auto value=next==r ? next+0x101 : next;
  selectionRandom_=std::uint16_t(value);
  return std::uint8_t(value>>8);
}
std::uint64_t SoundMixer::playLocked(unsigned block,unsigned number,double gain,
    int panOverride,bool loopAllowed,const SoundAcoustics *acoustics,
    const SoundPoint &source,const SoundPoint &player,const SoundPoint &eye,
    const SoundPoint &right,unsigned argument) {
  const auto bank=bankLocked(block,argument);
  if (paused_ || !bank || !bank->cue(number)) return 0;
  const auto group=bank->cue(number)->randomGroup;
  const auto &choices=bank->randomChoices(group);
  if (!choices.empty()) {
    const auto key=std::make_tuple(block,bank->argument(),group);
    const auto last=lastRandom_.find(key);
    for (unsigned retry=0;retry<5;++retry) {
      number=choices[selectionByte()%choices.size()];
      if (last==lastRandom_.end() || number!=last->second) break;
    }
    lastRandom_[key]=number;
  }
  if (++serial_==0) ++serial_;
  const auto handle=serial_;
  const auto *first=bank->cue(number);
  const auto range=first->pitchHigh-first->pitchLow;
  const auto pitch=first->pitchLow+(range==0 ? 0 : range<=16 ?
      int(randomByte()%unsigned(range+1)) : int(randomByte()%17)*(range/16));
  const auto randomPitch=std::pow(2.0,pitch/1200.0);
  // req_iss_main issues consecutive SIT records under the same sound id when
  // 0x2000 is set. WEP01's shot uses this for its left/right authored layers.
  for (unsigned layers=0;layers<64;++layers,++number) {
    const auto *cue=bank->cue(number);
    if (!cue) break;
    if (voices_.size()==MaxVoices) {
      const auto lowest=std::min_element(voices_.begin(),voices_.end(),
          [](const Voice &a,const Voice &b) {
            return a.priority<b.priority || (a.priority==b.priority && a.age<b.age);
          });
      if (lowest->priority>cue->priority ||
          (lowest->priority==cue->priority && !(cue->flags&0x4000))) {
        if (!cue->chained()) break;
        continue;
      }
      voices_.erase(lowest);
    }
    SoundSpatialParameters spatial;
    if (acoustics) spatial=acoustics->parameters(*cue,source,player,eye,right);
    const auto pan=panOverride<0 ? (acoustics ? spatial.pan : cue->pan<0 ? 64 : cue->pan) : panOverride;
    const double angle=std::clamp(pan,0,127)*pi/(2*127);
    Voice voice;
    voice.sample=cue->sample;voice.bank=bank;voice.handle=handle;voice.age=handle;
    voice.block=block;voice.cueNumber=number;voice.priority=cue->priority;
    // iss_ax_set_para clamps its original 32 kHz SRC ratio to 4.
    voice.baseRate=cue->sample->rate*cue->pitchRatio*randomPitch;
    voice.step=std::min(voice.baseRate*spatial.pitchRatio,128000.0)/OutputRate;
    voice.sourceGain=SoundVolumeGain(cue->volume);
    voice.gain=voice.targetGain=gain*voice.sourceGain*spatial.gain;
    voice.left=voice.targetLeft=std::cos(angle);voice.right=voice.targetRight=std::sin(angle);
    voice.filterB=voice.targetFilterB=filterFeedback(spatial.filter);
    voice.loopAllowed=loopAllowed;voice.pauseExempt=(cue->soundFlags&2)!=0;
    voices_.push_back(std::move(voice));
    if (!cue->chained()) break;
  }
  return std::any_of(voices_.begin(),voices_.end(),[handle](const Voice &v){return v.handle==handle;})
             ? handle : 0;
}
void SoundMixer::updateSpatial(std::uint64_t handle,const SoundAcoustics &acoustics,
    const SoundPoint &source,const SoundPoint &player,const SoundPoint &eye,
    const SoundPoint &right) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto &voice:voices_) if (voice.handle==handle) {
    const auto *cue=voice.bank?voice.bank->cue(voice.cueNumber):nullptr;
    if (!cue) continue;
    const auto spatial=acoustics.parameters(*cue,source,player,eye,right);
    const auto angle=spatial.pan*pi/(2*127);
    voice.targetGain=voice.sourceGain*spatial.gain;
    voice.targetLeft=std::cos(angle);voice.targetRight=std::sin(angle);
    voice.step=std::min(voice.baseRate*spatial.pitchRatio,128000.0)/OutputRate;
    voice.targetFilterB=filterFeedback(spatial.filter);
  }
}
void SoundMixer::stop(std::uint64_t handle) {
  std::lock_guard<std::mutex> lock(mutex_);
  voices_.erase(std::remove_if(voices_.begin(),voices_.end(),
                              [handle](const Voice &v){return v.handle==handle;}),voices_.end());
}
void SoundMixer::setVoiceGain(std::uint64_t handle,double gain,int pan) {
  validateGain(gain,pan);
  if (pan<0) pan=64;
  const auto angle=pan*pi/(2*127);
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto &v:voices_) if (v.handle==handle) {
    v.targetGain=gain*v.sourceGain;
    v.targetLeft=std::cos(angle);v.targetRight=std::sin(angle);
  }
}
void SoundMixer::installSequence(std::shared_ptr<const SoundSequence> sequence, bool restart) {
  installSequenceSlot(0,std::move(sequence),restart);
}
void SoundMixer::installSequenceSlot(unsigned slot,std::shared_ptr<const SoundSequence> sequence, bool restart) {
  if (slot>=music_.size()) throw std::runtime_error("Original music: BGM-Slot");
  std::lock_guard<std::mutex> lock(mutex_);
  if (!restart && sequences_[slot] == sequence) return;
  sequences_[slot] = std::move(sequence);
  music_[slot] = sequences_[slot] ? std::make_unique<MusicPlayer>(sequences_[slot]) : nullptr;
  musicFadeFrames_[slot]=0;musicFadeOut_[slot]=false;
}
void SoundMixer::installStream(std::shared_ptr<const SoundStream> stream, bool restart) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto old=std::find_if(streams_.begin(),streams_.end(),[&](const auto &v){return v.stream && v.handle==legacyStreamHandle_;});
  if(!restart && old!=streams_.end() && old->stream==stream)return;
  if(old!=streams_.end())*old={};
  legacyStreamHandle_=legacyStreamFrames_=0;
  if(stream) {
    if(!restart)if(auto *existing=streamLocked(stream->block,stream->number)) {
      if(existing->stream==stream) {existing->playing=true;legacyStreamHandle_=existing->handle;legacyStreamFrames_=existing->frames;return;}
    }
    auto *voice=startStreamLocked(std::move(stream));
    if(!voice)throw std::runtime_error("Original stream: All four work slots occupied");
    legacyStreamHandle_=voice->handle;
  }
}
std::uint64_t SoundMixer::streamRenderedFrames() const {
  std::lock_guard<std::mutex> lock(mutex_);return legacyStreamFrames_;
}
namespace {
void validateStream(const std::shared_ptr<const SoundStream> &stream) {
  if(!stream || stream->block>1 || stream->number>255 || !stream->rate || stream->rate>128000 ||
      stream->volume>127 || stream->pan>127 || stream->left.empty() ||
      (stream->stereo && stream->left.size()!=stream->right.size()) ||
      (stream->stereo && stream->loopLeft.size()!=stream->loopRight.size()) ||
      (stream->looping && stream->loopLeft.empty()))
    throw std::runtime_error("Original stream: invalid resident PCM data");
}
}
SoundMixer::StreamVoice *SoundMixer::streamLocked(unsigned block,unsigned number) {
  const auto found=std::find_if(streams_.begin(),streams_.end(),[&](const auto &v) {
    return v.stream && v.stream->block==block && v.stream->number==number;
  });return found==streams_.end()?nullptr:&*found;
}
const SoundMixer::StreamVoice *SoundMixer::streamLocked(unsigned block,unsigned number) const {
  const auto found=std::find_if(streams_.begin(),streams_.end(),[&](const auto &v) {
    return v.stream && v.stream->block==block && v.stream->number==number;
  });return found==streams_.end()?nullptr:&*found;
}
SoundMixer::StreamVoice *SoundMixer::startStreamLocked(std::shared_ptr<const SoundStream> stream,bool playing) {
  validateStream(stream);
  const auto found=std::find_if(streams_.begin(),streams_.end(),[](const auto &v){return !v.stream;});
  if(found==streams_.end())return nullptr;
  *found={};found->stream=std::move(stream);found->handle=++streamSerial_;
  found->playing=playing;
  found->volume=int(found->stream->volume)<<8;found->step=double(found->stream->rate)/OutputRate;
  found->gain=SoundVolumeGain(found->volume>>8);
  const auto angle=found->stream->pan*pi/(2*127);found->left=std::cos(angle);found->right=std::sin(angle);
  return &*found;
}
void SoundMixer::fadeStreamLocked(StreamVoice &voice,unsigned time,unsigned volume) {
  voice.fadeEnd=int(volume)<<8;
  if(!time) {voice.volume=voice.fadeEnd;voice.gain=SoundVolumeGain(voice.volume>>8);voice.fading=false;if(!volume)voice.stream.reset();return;}
  voice.fadeStep=(voice.fadeEnd-voice.volume)/int(time);
  if(!voice.fadeStep)voice.fadeStep=voice.fadeEnd>voice.volume?1:-1;
  voice.fading=true;
}
void SoundMixer::installStreamAssets(const std::vector<std::shared_ptr<const SoundStream>> &assets,bool replaceRoom) {
  if(assets.size()>512)throw std::runtime_error("Original stream: Resident table budget");
  for(const auto &stream:assets)validateStream(stream);
  std::lock_guard<std::mutex> lock(mutex_);
  auto next=streamAssets_;
  if(replaceRoom)for(auto it=next.begin();it!=next.end();) {
    if(!it->first.first)it=next.erase(it);else ++it;
  }
  for(const auto &stream:assets)next[{stream->block,stream->number}]=stream;
  streamAssets_.swap(next);
  if(replaceRoom)for(auto &voice:streams_)if(voice.stream && !voice.stream->block &&
      !streamAssets_.count({voice.stream->block,voice.stream->number}))voice.stream.reset();
}
std::uint64_t SoundMixer::requestStream(unsigned block,unsigned number,std::uint32_t flags,unsigned time,unsigned volume) {
  if(block>1 || number>255 || time>32767 || volume>127 || (flags&~0x8000001fu))
    throw std::runtime_error("Original stream: invalid SndStrReq parameters");
  std::lock_guard<std::mutex> lock(mutex_);auto *voice=streamLocked(block,number);
  if(flags&1u) {
    if(!block && (flags&0x80000000u) && voice) {
      if(voice->stopRequested) {fadeStreamLocked(*voice,2,voice->stream->volume);voice->stopRequested=false;}
      return voice->handle;
    }
    const auto asset=streamAssets_.find({block,number});if(asset==streamAssets_.end())return 0;
    voice=startStreamLocked(asset->second,false);if(!voice)return 0;
    const auto target=volume?volume:voice->stream->volume;
    voice->volume=int(target)<<8;
    voice->gain=SoundVolumeGain(voice->volume>>8);
  }
  if(!voice)return 0;
  const auto handle=voice->handle;
  if(flags&2u) {
    voice->playing=true;voice->phase=0;voice->frames=0;voice->looping=false;
    voice->fading=false;voice->stopRequested=false;
    voice->volume=int(voice->stream->volume)<<8;
    voice->gain=SoundVolumeGain(voice->volume>>8);
    if(time) {
      voice->volume=0;voice->gain=0;voice->fadeEnd=int(volume)<<8;
      // The combined prepare/play wrapper defaults a zero volume to RIT;
      // a later standalone play request retains its explicit fade target.
      if((flags&1u) && !volume)voice->fadeEnd=int(voice->stream->volume)<<8;
      voice->fadeStep=voice->fadeEnd/int(time);voice->fading=true;
    }
  } else if(flags&0x10u) {
    if(time>127)throw std::runtime_error("Original stream: Direct volume");
    voice->volume=int(time)<<8;voice->fading=false;
    voice->gain=SoundVolumeGain(voice->volume>>8);
  } else if(flags&4u) {
    fadeStreamLocked(*voice,time,volume);voice->stopRequested=!volume;
  } else if(flags&8u) {
    fadeStreamLocked(*voice,50,0);voice->stopRequested=true;
  }
  return handle;
}
bool SoundMixer::streamActive(unsigned block,unsigned number) const {
  std::lock_guard<std::mutex> lock(mutex_);return streamLocked(block,number)!=nullptr;
}
bool SoundMixer::streamReady(unsigned block,unsigned number) const {
  std::lock_guard<std::mutex> lock(mutex_);return streamLocked(block,number)!=nullptr;
}
bool SoundMixer::streamPlaying(unsigned block,unsigned number) const {
  std::lock_guard<std::mutex> lock(mutex_);const auto *voice=streamLocked(block,number);return voice && voice->playing;
}
bool SoundMixer::streamStopping(unsigned block,unsigned number) const {
  std::lock_guard<std::mutex> lock(mutex_);const auto *voice=streamLocked(block,number);return voice && voice->stopRequested;
}
std::uint64_t SoundMixer::streamRenderedFrames(unsigned block,unsigned number) const {
  std::lock_guard<std::mutex> lock(mutex_);const auto *voice=streamLocked(block,number);return voice?voice->frames:0;
}
unsigned SoundMixer::streamVolumeFixed(unsigned block,unsigned number) const {
  std::lock_guard<std::mutex> lock(mutex_);const auto *voice=streamLocked(block,number);return voice?unsigned(voice->volume):0;
}
void SoundMixer::streamAudioTickLocked() {
  for(auto &voice:streams_)if(voice.stream && voice.playing && voice.fading) {
    if(voice.volume==voice.fadeEnd) {
      voice.fading=false;if(!voice.volume)voice.stream.reset();
    } else {
      voice.volume+=voice.fadeStep;
      if(voice.fadeStep>0)voice.volume=std::min(voice.volume,voice.fadeEnd);
      else voice.volume=std::max(voice.volume,voice.fadeEnd);
      voice.gain=SoundVolumeGain(voice.volume>>8);
    }
  }
}
std::uint64_t SoundMixer::musicFrames(unsigned slot) const {
  if (slot>=music_.size()) throw std::runtime_error("Original music: BGM-Slot");
  std::lock_guard<std::mutex> lock(mutex_); return music_[slot] ? music_[slot]->renderedFrames() : 0;
}
bool SoundMixer::hasMusic() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return bool(music_[0]) || bool(music_[1]) ||
      std::any_of(streams_.begin(),streams_.end(),[](const auto &voice){return bool(voice.stream);});
}
void SoundMixer::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  voices_.clear(); banks_.clear();currentArguments_.clear();
  for (unsigned slot=0;slot<music_.size();++slot) { sequences_[slot].reset();music_[slot].reset(); }
  streams_={};streamAssets_.clear();legacyStreamHandle_=legacyStreamFrames_=0;streamAudioClock_=0;
  subscreen_=false; resumeGain_=0; random_=0xD37;
  selectionRandom_=0;lastRandom_.clear();
}
void SoundMixer::setPaused(bool paused) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (paused_!=paused) resumeGain_=0;
  paused_=paused;
}
void SoundMixer::setSubscreen(bool active) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (subscreen_==active) return;
  subscreen_=active;
  for (auto &v:voices_) {
    const auto remaining=v.phase<v.sample->pcm.size() ? v.sample->pcm.size()-v.phase : 0;
    // Original seCtrlPauseOn_sub lets a nonlooping one-shot finish when its
    // remaining nibble count is <=800 (about700 decoded samples).
    v.subscreenPaused=active && !v.pauseExempt &&
        (v.looping || (v.loopAllowed && !v.sample->loopPcm.empty()) || remaining>700);
  }
}
std::shared_ptr<const SoundBank> SoundMixer::bankLocked(unsigned block,unsigned argument) const {
  if(argument) {
    const auto exact=banks_.find({block,argument});
    return exact==banks_.end()?nullptr:exact->second;
  }
  const auto current=currentArguments_.find(block);
  if(current==currentArguments_.end())return {};
  const auto found=banks_.find({block,current->second});
  return found==banks_.end()?nullptr:found->second;
}
bool SoundMixer::hasCue(unsigned block,unsigned number,unsigned argument) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto bank=bankLocked(block,argument);
  return bank && bank->cue(number);
}
bool SoundMixer::voiceActive(std::uint64_t handle) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return handle!=0 && std::any_of(voices_.begin(),voices_.end(),
      [handle](const Voice &voice){return voice.handle==handle;});
}
std::size_t SoundMixer::voiceCount() const {
  std::lock_guard<std::mutex> lock(mutex_);return voices_.size();
}
void SoundMixer::fadeAll() {
  std::lock_guard<std::mutex> lock(mutex_);
  for(auto &v:voices_){v.fadeOut=true;v.fadeFrames=OutputRate*2;}
  for(unsigned slot=0;slot<music_.size();++slot)if(music_[slot]) {
    musicFadeOut_[slot]=true;musicFadeFrames_[slot]=OutputRate*2;
  }
  for(auto &v:streams_)if(v.stream){fadeStreamLocked(v,400,0);v.stopRequested=true;}
}
void SoundMixer::render(std::int16_t *output,std::size_t frames) {
  if (!output || frames>OutputRate)
    throw std::runtime_error("Audio: PCM buffer budget");
  std::lock_guard<std::mutex> lock(mutex_);
  std::fill_n(output,frames*2,std::int16_t(0));
  if (paused_) return;
  const auto seMaster=subscreen_ ? SoundVolumeGain(0x3f) : 1.0;
  for (std::size_t frame=0;frame<frames;++frame) {
    if(streamAudioClock_==OutputRate/200) {streamAudioTickLocked();streamAudioClock_=0;}
    double left=0,right=0;
    for (unsigned slot=0;slot<music_.size();++slot) if (music_[slot]) {
      double l=0,r=0;music_[slot]->sample(l,r,OutputRate);
      const auto gain=musicFadeOut_[slot]?double(musicFadeFrames_[slot])/(OutputRate*2):1.;
      left+=l*gain;right+=r*gain;
      if(musicFadeOut_[slot] && (!musicFadeFrames_[slot] || !--musicFadeFrames_[slot]))music_[slot].reset();
    }
    for(auto &voice:streams_)if(voice.stream && voice.playing) {
      const auto *l=voice.looping ? &voice.stream->loopLeft : &voice.stream->left;
      const auto *r=voice.looping ? &voice.stream->loopRight : &voice.stream->right;
      if (voice.phase>=l->size()) {
        if (voice.stream->looping && !voice.stream->loopLeft.empty()) {
          voice.phase=std::fmod(voice.phase-l->size(),double(voice.stream->loopLeft.size()));
          voice.looping=true;l=&voice.stream->loopLeft;r=&voice.stream->loopRight;
        } else { voice.stream.reset(); }
      }
      if (voice.stream) {
        const auto i=std::size_t(voice.phase);
        if (i<l->size()) {
          const auto fraction=voice.phase-i;
          const auto interpolate=[&](const std::vector<std::int16_t> &pcm,
                                      const std::vector<std::int16_t> &loop) {
            const auto next=i+1<pcm.size() ? pcm[i+1] :
                voice.stream->looping && !loop.empty() ? loop.front() : pcm[i];
            return pcm[i]+(next-pcm[i])*fraction;
          };
          const auto gain=voice.gain;
          const auto lvalue=interpolate(*l,voice.stream->loopLeft);
          if (voice.stream->stereo) { left+=lvalue*gain; right+=interpolate(*r,voice.stream->loopRight)*gain; }
          else {
            left+=lvalue*gain*voice.left;right+=lvalue*gain*voice.right;
          }
          voice.phase+=voice.step;++voice.frames;
          if(voice.handle==legacyStreamHandle_)legacyStreamFrames_=voice.frames;
        }
      }
    }
    for (auto &v:voices_) {
      if (v.subscreenPaused) continue;
      const auto *pcm=v.looping ? &v.sample->loopPcm : &v.sample->pcm;
      if (v.phase>=pcm->size()) {
        if (!v.loopAllowed || v.sample->loopPcm.empty()) continue;
        v.phase=std::fmod(v.phase-pcm->size(),double(v.sample->loopPcm.size()));
        v.looping=true;pcm=&v.sample->loopPcm;
      }
      const auto index=std::size_t(v.phase);
      if (index>=pcm->size()) continue;
      const auto fraction=v.phase-index;
      auto next=(*pcm)[index];
      if (index+1<pcm->size()) next=(*pcm)[index+1];
      else if (v.loopAllowed && !v.sample->loopPcm.empty()) next=v.sample->loopPcm.front();
      double value=(*pcm)[index]+(next-(*pcm)[index])*fraction;
      // Smooth a changing positional level over about 10 ms.
      v.gain+=(v.targetGain-v.gain)/480;
      v.left+=(v.targetLeft-v.left)/480;v.right+=(v.targetRight-v.right)/480;
      v.filterB+=(v.targetFilterB-v.filterB)/480;
      value=(1-v.filterB)*value+v.filterB*v.filterHistory;
      v.filterHistory=value;
      const auto fade=v.fadeOut?double(v.fadeFrames)/(OutputRate*2):1.;
      left+=value*v.gain*v.left*seMaster*fade;right+=value*v.gain*v.right*seMaster*fade;
      if(v.fadeOut && v.fadeFrames)--v.fadeFrames;
      v.phase+=v.step;
    }
    resumeGain_=std::min(1.0,resumeGain_+1.0/480);
    output[frame*2]=std::int16_t(std::clamp(std::lround(left*masterGain*resumeGain_),-32768l,32767l));
    output[frame*2+1]=std::int16_t(std::clamp(std::lround(right*masterGain*resumeGain_),-32768l,32767l));
    ++streamAudioClock_;
  }
  voices_.erase(std::remove_if(voices_.begin(),voices_.end(),[](const Voice &v){
    if(v.fadeOut && !v.fadeFrames)return true;
    const auto &pcm=v.looping ? v.sample->loopPcm : v.sample->pcm;
    return v.phase>=pcm.size() && (!v.loopAllowed || v.sample->loopPcm.empty());
  }),voices_.end());
}
} // namespace re4
