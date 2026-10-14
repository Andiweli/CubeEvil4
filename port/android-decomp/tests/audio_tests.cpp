#include "sound_bank.h"
#include "sound_mixer.h"
#include "sound_acoustics.h"
#include "archive.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <thread>

namespace {
using Bytes=std::vector<std::uint8_t>;
std::size_t checks=0;
void checkAt(bool ok,unsigned line) {
  if (!ok) std::cerr<<"Audio check failed: "<<checks+1<<" at line "<<line<<"\n";
  assert(ok);++checks;
}
#define check(value) checkAt((value),__LINE__)
void half(Bytes &b,std::size_t p,unsigned v) { b.at(p)=v>>8;b.at(p+1)=v; }
void word(Bytes &b,std::size_t p,std::uint32_t v) { half(b,p,v>>16);half(b,p+2,v); }
void rejects(const std::function<void()> &call) {
  bool failed=false;
  try { call(); } catch (const std::runtime_error &) { failed=true; }
  check(failed);
}
// One complete, independently authored DVD/ISS/DLS fixture. Offsets use the
// on-disc byte layout rather than native SDK structs or production encoders.
Bytes bank(bool loop=false,unsigned flags=0x4001,unsigned block=1) {
  Bytes b(0x800);
  word(b,0x20,1);word(b,0x24,288);word(b,0x2c,0x400);word(b,0x30,block);
  word(b,0x40,2);word(b,0x44,16);word(b,0x4c,0x600);word(b,0x50,block);
  word(b,0x60,0xffffffff);
  const auto iss=0x400u,hdr=iss+32,wt=hdr+16,sit=wt+192;
  word(b,iss,32);word(b,hdr,2);word(b,hdr+4,16);word(b,hdr+8,sit-hdr);
  for (unsigned i=0;i<6;++i) word(b,wt+i*4,std::array<unsigned,6>{24,24,26,50,130,146}[i]);
  const auto region=wt+26,sample=wt+130,adpcm=wt+146;
  if (loop) { word(b,region+8,7);word(b,region+12,14); }
  half(b,sample+2,32000);word(b,sample+8,28);
  half(b,adpcm+34,8);half(b,adpcm+40,8);half(b,adpcm+42,1234);
  for (unsigned i=0;i<2;++i) {
    const auto p=sit+i*24;b[p+4]=10;b[p+5]=64;b[p+6]=127;b[p+9]=255;
    half(b,p+22,i==0 ? flags : 0x4001);
  }
  for (unsigned frame=0;frame<2;++frame) {
    b[0x600+frame*8]=8;
    std::fill_n(b.begin()+0x601+frame*8,7,std::uint8_t(0x77));
  }
  return b;
}
void real(Bytes &b,std::size_t p,float value) {
  std::uint32_t bits;
  static_assert(sizeof(bits)==sizeof(value));
  std::memcpy(&bits,&value,sizeof(bits));word(b,p,bits);
}
Bytes acoustics() {
  Bytes b(0x300);
  word(b,0x40,0x240);
  b[0x241]=0;b[0x243]=0;b[0x245]=0;
  for (unsigned kind=0;kind<3;++kind) {
    const auto offset=0x248+kind*24;
    word(b,0xc0+kind*0x80,offset);
    word(b,offset,2);real(b,offset+4,1000);
    real(b,offset+8,0);real(b,offset+16,10);
    half(b,offset+14,kind==0 ? 127 : 0);
    half(b,offset+22,kind==0 ? 0 : kind==1 ? 0xfff4 : 4);
  }
  return b;
}
} // namespace

int main() {
  re4::DspAdpcmContext context;
  Bytes encoded{0,0x12,0x34,0x56,0x78,0x9a,0xbc,0xde};
  const auto direct=re4::DecodeDspAdpcm(encoded,0,14,context);
  check(direct==std::vector<std::int16_t>({1,2,3,4,5,6,7,-8,-7,-6,-5,-4,-3,-2}));
  context.coefficients[0]=2048;
  check(re4::DecodeDspAdpcm(encoded,0,5,context)==std::vector<std::int16_t>({1,3,6,10,15}));
  // DSP accelerator rounding differs from FFmpeg's THP decoder: +0x400 before
  // arithmetic >>11 (Dolphin Core/DSP/DSPAccelerator.cpp::ReadSample).
  context.coefficients[0]=1024;context.history1=1;
  check(re4::DecodeDspAdpcm(Bytes{0,0},0,1,context).front()==1);
  context.history1=-1;
  check(re4::DecodeDspAdpcm(Bytes{0,0},0,1,context).front()==0);
  context.coefficients[0]=2048;context.predictorScale=15;context.history1=32760;
  check(re4::DecodeDspAdpcm(Bytes{15,0x70},0,1,context).front()==32767);
  context.history1=-32760;
  check(re4::DecodeDspAdpcm(Bytes{15,0x80},0,1,context).front()==-32768);
  context={};
  check(re4::DecodeDspAdpcm(encoded,5,4,context)==std::vector<std::int16_t>({6,7,-8,-7}));
  check(re4::DecodeDspAdpcm(Bytes{0,0x12,0x30},0,3,context)==std::vector<std::int16_t>({1,2,3}));
  check(re4::DecodeDspAdpcm({},0,0,context).empty());
  rejects([&]{re4::DecodeDspAdpcm({},0,1,context);});
  rejects([&]{re4::DecodeDspAdpcm(encoded,0,15,context);});
  context.predictorScale=0x80;
  rejects([&]{re4::DecodeDspAdpcm(encoded,0,1,context);});
  context={};
  rejects([&]{re4::DecodeDspAdpcm(encoded,0,8*1024*1024+1,context);});
  check(re4::SoundVolumeGain(0)==0);
  check(re4::SoundVolumeGain(127)==1);
  for (int i=1;i<127;++i) check(re4::SoundVolumeGain(i)<re4::SoundVolumeGain(i+1));
  const auto original=bank();
  const auto banks=re4::SoundBank::ParseDvd(original);
  check(banks.size()==1);check(banks.front()->block()==1);
  check(banks.front()->cue(0)->sample->pcm.size()==28);
  check(banks.front()->cue(0)->sample==banks.front()->cue(1)->sample);
  check(banks.front()->cue(999)==nullptr);
  // Original WEP19 has no local cues but still changes the active weapon
  // bank. Original WEP28's last melodic key overlaps the first region word.
  auto empty=original;word(empty,0x420,0);
  const auto emptyBanks=re4::SoundBank::ParseDvd(empty);
  check(emptyBanks.size()==1 && emptyBanks.front()->cueCount()==0);
  check(emptyBanks.front()->block()==1 && emptyBanks.front()->argument()==0);
  auto packedKey=original;half(packedKey,0x430+192,1);
  const auto packedBanks=re4::SoundBank::ParseDvd(packedKey);
  check(packedBanks.front()->cue(0)->sample==packedBanks.front()->cue(1)->sample);
  check(packedBanks.front()->cue(0)->sample->pcm==banks.front()->cue(0)->sample->pcm);
  re4::SoundAcoustics space(acoustics());
  auto positional=*banks.front()->cue(0);positional.pan=-1;positional.curve=0;
  const re4::SoundPoint origin{},right{1,0,0};
  auto near=space.parameters(positional,{0,0,-1},origin,origin,right);
  check(near.pan==64);check(near.filter==4); // strictly-after threshold filter
  check(near.pitchRatio==1); // Original cent result is narrowed to s16.
  const auto far=space.parameters(positional,{10000,0,0},origin,origin,right);
  check(far.pan==127);check(far.gain==0);check(std::abs(far.pitchRatio-.5)<1e-12);
  const auto left=space.parameters(positional,{-10000,0,0},origin,origin,right);
  check(left.pan==0);
  positional.surroundType=1;
  check(space.parameters(positional,{10000,0,0},origin,origin,right).gain==1);
  check(space.parameters(positional,{10000,0,0},origin,origin,right).pan==64);
  auto malformed=acoustics();real(malformed,0x24c,0);
  rejects([&]{re4::SoundAcoustics invalid(malformed);});
  malformed=acoustics();real(malformed,0x258,0);
  rejects([&]{re4::SoundAcoustics invalid(malformed);});
  malformed=acoustics();malformed[0x241]=32;
  rejects([&]{re4::SoundAcoustics invalid(malformed);});
  positional.surroundType=0;
  rejects([&]{space.parameters(positional,{NAN,0,0},origin,origin,right);});
  const auto loop=re4::SoundBank::ParseDvd(bank(true));
  check(loop.front()->cue(0)->sample->pcm.size()==21);
  check(loop.front()->cue(0)->sample->loopPcm.size()==14);
  const auto mutation=[&](std::size_t p,unsigned value,bool isHalf=false){
    auto b=original;if (isHalf) half(b,p,value);else word(b,p,value);
    rejects([&]{re4::SoundBank::ParseDvd(b);});
  };
  mutation(0x24,0xffffff00); // ISS outside outer file
  mutation(0x30,9); // unavailable block
  mutation(0x40,0xffffffff); // missing paired PCM
  mutation(0x400,0xffffffff); // ISS header pointer
  mutation(0x420,0xffffffff); // SIT count
  mutation(0x420+4,0xffffff00); // wavetable pointer
  mutation(0x430+4*4,0); // invalid wavetable table
  mutation(0x430+24,0xffff,true); // key -> region
  mutation(0x430+26+20,0xffffff00); // region -> sample
  mutation(0x430+130,1,true); // unsupported sample format
  mutation(0x430+130+2,0,true); // sample rate
  mutation(0x430+130+4,1); // unaligned nibble offset
  mutation(0x430+130+8,0xffffff00); // sample length
  mutation(0x430+130+12,0xffff,true); // ADPCM index
  mutation(0x430+192,0x80,true); // key out of range
  auto dummy=original;half(dummy,0x430+192+22,0x8000);
  check(!re4::SoundBank::ParseDvd(dummy).front()->cue(0));
  auto sequence=original;half(sequence,0x430+192+22,4);
  check(!re4::SoundBank::ParseDvd(sequence).front()->cue(0));
  auto invalidLoop=bank(true);word(invalidLoop,0x430+26+12,1000);
  rejects([&]{re4::SoundBank::ParseDvd(invalidLoop);});
  re4::SoundMixer mixer;
  mixer.installBanks(banks);
  check(mixer.hasCue(1,0));check(!mixer.hasCue(2,0));
  mixer.installBanks(emptyBanks);
  check(!mixer.hasCue(1,0)); // no stale cue from the previously active weapon
  mixer.installBanks(banks);
  check(mixer.play(1,0)==0); // initial lifecycle pause
  mixer.setPaused(false);
  check(mixer.play(2,0)==0);
  const auto handle=mixer.play(1,0);
  check(handle!=0);check(mixer.voiceCount()==1);
  check(mixer.voiceActive(handle));check(!mixer.voiceActive(0));check(!mixer.voiceActive(handle+1));
  std::vector<std::int16_t> pcm(512*2);
  mixer.setPaused(true);mixer.render(pcm.data(),512);
  check(std::all_of(pcm.begin(),pcm.end(),[](std::int16_t v){return v==0;}));
  check(mixer.voiceCount()==1);
  check(mixer.voiceActive(handle));
  mixer.setPaused(false);mixer.render(pcm.data(),512);check(mixer.voiceCount()==0);
  check(!mixer.voiceActive(handle));
  check(std::any_of(pcm.begin(),pcm.end(),[](std::int16_t v){return v!=0;}));
  rejects([&]{mixer.play(1,0,NAN);});
  rejects([&]{mixer.play(1,0,1,128);});
  rejects([&]{mixer.render(nullptr,0);});
  rejects([&]{mixer.render(pcm.data(),48001);});
  mixer.installBanks(loop);
  for (unsigned i=0;i<100;++i) check(mixer.play(1,0)!=0);
  check(mixer.voiceCount()==re4::SoundMixer::MaxVoices);
  mixer.render(pcm.data(),512);
  check(mixer.voiceCount()==re4::SoundMixer::MaxVoices);
  mixer.clear();check(mixer.voiceCount()==0);check(!mixer.hasCue(1,0));
  const auto chain=re4::SoundBank::ParseDvd(bank(false,0x6001));
  mixer.installBanks(chain);const auto chained=mixer.play(1,0);
  check(chained!=0);check(mixer.voiceCount()==2);
  mixer.stop(chained);check(mixer.voiceCount()==0);
  check(!mixer.voiceActive(chained));
  auto layeredData=bank(false,0x6001);
  // Expand the independent WT key table to include key12. The first SIT note
  // plays that octave; the second remains at key0 and retains the same handle.
  // SndEndCheck must retain the shared handle after the first layer finishes.
  std::copy_backward(layeredData.begin()+0x430+26,layeredData.begin()+0x430+240,
                     layeredData.begin()+0x430+264);
  std::fill(layeredData.begin()+0x430+24,layeredData.begin()+0x430+50,0);
  for(unsigned i=0;i<6;++i)
    word(layeredData,0x430+i*4,std::array<unsigned,6>{24,24,50,74,154,170}[i]);
  word(layeredData,0x24,312);word(layeredData,0x428,0x430+216-0x420);
  half(layeredData,0x430+216,12);
  mixer.installBanks(re4::SoundBank::ParseDvd(layeredData));
  const auto layered=mixer.play(1,0);
  check(layered!=0&&mixer.voiceCount()==2&&mixer.voiceActive(layered));
  mixer.render(pcm.data(),25);check(mixer.voiceCount()==1&&mixer.voiceActive(layered));
  mixer.render(pcm.data(),25);check(mixer.voiceCount()==0&&!mixer.voiceActive(layered));
  const auto room=re4::SoundBank::ParseDvd(bank(true,0x4001,6));
  mixer.installBanks(room);check(mixer.play(6,0)!=0);
  mixer.installBanks({},true);check(!mixer.hasCue(6,0));check(mixer.voiceCount()==0);
  check(mixer.hasCue(1,0));
  auto randomData=bank();
  word(randomData,0x24,512);word(randomData,0x404,0x120);
  randomData[0x430+192+19]=1;
  randomData[0x430+192+5]=127;randomData[0x430+192+24+5]=0;
  word(randomData,0x524,0x80);
  half(randomData,0x5a0,2);half(randomData,0x5a2,0xffff);
  half(randomData,0x5a4,0);half(randomData,0x5a6,1);
  const auto variations=re4::SoundBank::ParseDvd(randomData);
  check(variations.front()->randomChoices(1)==std::vector<unsigned>({0,1}));
  re4::SoundMixer varying;
  varying.installBanks(variations);varying.setPaused(false);
  bool previousLeft=false;
  for (unsigned i=0;i<20;++i) {
    check(varying.play(1,0,1,-1,false)!=0);
    varying.render(pcm.data(),128);
    bool hasLeft=false,hasRight=false;
    for (unsigned j=0;j<128;++j) {
      hasLeft|=pcm[j*2]!=0;hasRight|=pcm[j*2+1]!=0;
    }
    check(hasLeft!=hasRight);
    if (i==2) check(hasLeft!=previousLeft);
    // This owned seed produces five odd bytes on request two. The source
    // accepts the repeat after five tries, rather than looping indefinitely.
    if (i==1) check(hasLeft==previousLeft);
    previousLeft=hasLeft;
  }
  auto badGroup=randomData;half(badGroup,0x5a6,99);
  rejects([&]{re4::SoundBank::ParseDvd(badGroup);});
  re4::SoundMixer once,partitioned;
  once.installBanks(loop);partitioned.installBanks(loop);
  once.setPaused(false);partitioned.setPaused(false);
  check(once.play(1,0)!=0);check(partitioned.play(1,0)!=0);
  std::vector<std::int16_t> whole(2048),parts(2048);
  once.render(whole.data(),1024);
  for (unsigned i=0;i<8;++i) partitioned.render(parts.data()+i*256,128);
  check(whole==parts); // No render-frame-rate dependency or chunk-boundary reset.
  re4::SoundMixer deathFade;deathFade.installBanks(loop);deathFade.setPaused(false);
  const auto oldVoice=deathFade.play(1,0);check(oldVoice!=0);deathFade.fadeAll();
  std::vector<std::int16_t> second(48000*2);deathFade.render(second.data(),48000);
  check(deathFade.voiceActive(oldVoice));
  deathFade.setPaused(true);deathFade.render(second.data(),48000);check(deathFade.voiceActive(oldVoice));
  deathFade.setPaused(false);const auto newVoice=deathFade.play(1,0);check(newVoice!=0);
  deathFade.render(second.data(),48000);check(!deathFade.voiceActive(oldVoice)&&deathFade.voiceActive(newVoice));
  // Concurrent bank swaps, requests and consumer reads exercise the actual
  // mutex/immutable-sample ownership path used by AudioTrack and the GL thread.
  mixer.installBanks(loop);
  std::atomic<bool> failed{false};
  std::thread consumer([&]{
    try { for (unsigned i=0;i<3000;++i) mixer.render(pcm.data(),128); }
    catch (...) { failed=true; }
  });
  for (unsigned i=0;i<3000;++i) {
    mixer.installBanks(loop);const auto h=mixer.play(1,0);
    mixer.setVoiceGain(h,.5,int(i%128));
    if (i%3==0) mixer.stop(h);
    if (i%100==0) { mixer.setPaused(true);mixer.setPaused(false); }
  }
  consumer.join();check(!failed);check(mixer.voiceCount()<=64);
  std::cout<<"Audio CPU: "<<checks<<" checks passed\n";
}
