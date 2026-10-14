#include "archive.h"
#include "sound_ambient.h"
#include "sound_music.h"
#include "sound_mixer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks;
void check(bool pass,const std::string &message) {
  ++checks;if (!pass) throw std::runtime_error(message);
}
std::uint64_t fingerprint(const std::vector<std::int16_t> &pcm) {
  std::uint64_t hash=1469598103934665603ull;
  for (const auto value:pcm) { const auto bits=std::uint16_t(value);hash^=bits&255;hash*=1099511628211ull;hash^=bits>>8;hash*=1099511628211ull; }
  return hash;
}
void put16(std::vector<std::uint8_t> &bytes,std::size_t p,unsigned v) {
  bytes[p]=std::uint8_t(v>>8);bytes[p+1]=std::uint8_t(v);
}
void emitterChecks() {
  std::vector<std::uint8_t> data(16+44);data[0]='E';data[1]='S';data[2]='E';put16(data,4,0x100);put16(data,6,1);
  data[16]=1;data[17]=5;put16(data,18,1);put16(data,34,6);put16(data,38,9);
  put16(data,40,3);put16(data,42,2);put16(data,46,2);
  re4::SoundAmbient ambient(data);unsigned calls=0;
  const auto play=[&](unsigned block,unsigned cue,const re4::SoundPoint &,bool positional) {
    check(block==6 && cue==9 && !positional,"ESE original block/cue/nonpositional");++calls;return std::uint64_t(calls);
  };
  ambient.tick(play);ambient.tick(play);check(calls==0,"ESE original two-frame initial delay");
  ambient.tick([](unsigned,unsigned,const re4::SoundPoint &,bool){return std::uint64_t(0);});
  check(ambient.pending()==1,"ESE failed sound retains pending record");
  ambient.tick(play);check(calls==1,"ESE first successful play");
  for (unsigned i=0;i<3;++i) ambient.tick(play);
  check(calls==1,"ESE original interval waits three full frames");ambient.tick(play);
  check(calls==2 && ambient.pending()==0,"ESE call count reaches original -1 done state");
  check(ambient.setEnabled(5,false) && !ambient.setEnabled(99,true),"ESE source numbered enable lookup");
  data.resize(59);bool refused=false;
  try { re4::SoundAmbient bad(data); } catch (const std::exception &) { refused=true; }
  check(refused,"ESE truncated record refused");
}
} // namespace
int main(int argc,char **argv) {
  try {
    if (argc<3) throw std::runtime_error("usage: m32_audio_check DISC1_FILES R101_DAT [--all-streams]");
    emitterChecks();const std::string base=argv[1],bgm=base+"/bgm/";
    const auto selection=re4::ReadRoomMusicSelection(bgm+"bgmtbl.dat",0x101);
    check(selection.bgm[0]==0xc000,"r101 original default MIDI0 automatic bit");
    check(selection.streams[0]==0x8002 && !(selection.streams[0]&0x4000),"r101 battle stream2 is not auto-start ambient");
    const auto r100=re4::ReadRoomMusicSelection(bgm+"bgmtbl.dat",0x100);
    check(r100.bgm[0]==0x8000 && r100.streams[0]==0x8002,"r100 source scenario0 waits script");
    const auto r100Reset=re4::ReadRoomMusicSelection(bgm+"bgmtbl.dat",0x100,1);
    check(r100Reset.bgm[0]==0xc000,"r100 source scenario1");
    check(re4::ReadRoomMusicSelection(bgm+"bgmtbl.dat",0xfffe).bgm[0]==0,"missing room has no invented music");
    auto sequence=re4::SoundSequence::Load(bgm+"bio4midi.hed",bgm+"bio4midi.dat",0,0);
    // Independent source MIDI decode of the owned G4BE08 bank gives 1391
    // raw events: one tempo, CC102/103 plus EOT/control flow leave 1387.
    check(sequence->events().size()==1387,"original MIDI0 dispatched events");
    check(sequence->loopEvent()==321 && sequence->loopMilliseconds()==24140 && sequence->endMilliseconds()==85708,"original integer 480/tempo event timeline and loop");
    check(sequence->volume()==90,"source SIT music volume");
    const auto &events=sequence->events();
    check(events[0].status==0xc0 && events[0].first==0 && events[0].milliseconds==14,"first authored MIDI program/time");
    const auto *instrument=sequence->instrument(3,60);
    check(instrument && instrument->unityNote==60 && instrument->sample->rate==32000 && instrument->sample->loopStart==1998 && instrument->sample->loopPcm.size()==45957,"source DLS village layer and loop");
    check(sequence->instrument(127,127)==nullptr,"no substitute system soundfont");
    // All 17 owned MIDI banks contain 23 authored SIT sequences. Validate
    // every one, not only the village's ambient program.
    if (argc>3 && std::string(argv[3])=="--all-streams") {
      unsigned sequences=0;
      for (unsigned bank=0;bank<17;++bank) {
        const auto count=bank==7 ? 3u : (bank==8 || bank==9 || bank==11 || bank==13) ? 2u : 1u;
        for (unsigned no=0;no<count;++no) {
          auto original=re4::SoundSequence::Load(bgm+"bio4midi.hed",bgm+"bio4midi.dat",bank,no);
          check(!original->events().empty() && original->endMilliseconds()>0,"source MIDI bank/sequence validated");
          re4::MusicPlayer playback(original);double left=0,right=0;
          for(unsigned frame=0;frame<48000;++frame) playback.sample(left,right,48000);
          check(std::isfinite(left) && std::isfinite(right) && playback.renderedFrames()==48000,"source MIDI instrument render finite");++sequences;
        }
      }
      check(sequences==23,"all 23 original MIDI sequences");
    }
    re4::SoundAmbient ambient(re4::RoomArchive::Load(argv[2]));check(ambient.count()==2 && ambient.pending()==2,"r101 source ESE two emitters");
    std::vector<unsigned> sourceCues;
    ambient.tick([&](unsigned block,unsigned cue,const re4::SoundPoint &point,bool positional){
      check(block==6 && positional && std::isfinite(point[0]),"r101 ESE source position");sourceCues.push_back(cue);return std::uint64_t(1);
    });
    check(sourceCues==std::vector<unsigned>{0x57,0x56} && ambient.pending()==0,"r101 original ESE cues played once, sample owns loop");
    re4::SoundMixer mixer;mixer.installSequence(sequence);check(mixer.hasMusic(),"village sequence installed");
    std::vector<std::int16_t> output(9600),reference(9600);
    mixer.render(output.data(),4800);check(mixer.musicFrames()==0 && std::all_of(output.begin(),output.end(),[](auto x){return x==0;}),"paused music produces silence without advancing");
    mixer.setPaused(false);for(unsigned i=0;i<10;++i)mixer.render(output.data(),4800);
    check(mixer.musicFrames()==48000 && std::any_of(output.begin(),output.end(),[](auto x){return x!=0;}),"original MIDI/DSP renders sound");
    mixer.installSequence(sequence);check(mixer.musicFrames()==48000,"same original asset preserves playback across room/EGL");
    mixer.setPaused(true);mixer.render(output.data(),4800);check(mixer.musicFrames()==48000,"pause freezes sequence notes and clock");
    mixer.setPaused(false);mixer.render(output.data(),4800);check(mixer.musicFrames()==52800,"resume progresses only rendered active frames");
    re4::SoundMixer a,b;a.installSequence(sequence);b.installSequence(sequence);a.setPaused(false);b.setPaused(false);
    std::vector<std::int16_t> whole(96000),split(96000);a.render(whole.data(),48000);
    for(unsigned i=0;i<10;++i)b.render(split.data()+i*9600,4800);
    check(whole==split,"MIDI envelopes and events independent of AudioTrack buffer size");
    for(unsigned i=0;i<860;++i)a.render(output.data(),4800);
    check(a.musicFrames()==4176000 && std::any_of(output.begin(),output.end(),[](auto x){return x!=0;}),"original MIDI loop survives intro/cycle transition");
    check(fingerprint(whole)==fingerprint(split),"deterministic PCM fingerprint");
    for (unsigned block=0;block<2;++block) {
      const auto numbers=argc>3 && std::string(argv[3])=="--all-streams" ? (block ? 240u : 67u) : 1u;
      for (unsigned i=0;i<numbers;++i) {
        const unsigned no=numbers==1 ? (block ? 0x3bu : 2u) : i;
        auto stream=re4::SoundStream::Load(bgm+"bio4str.hed",bgm+(block ? "bio4evt.sbb" : "bio4bgm.sbb"),block,no);
        check(stream->rate>=4000 && stream->rate<=48000 && !stream->left.empty(),"original SHD sample rate/data");
        check(!stream->stereo || stream->left.size()==stream->right.size(),"source 16 KiB stereo block deinterleave");
        check(stream->looping==!stream->loopLeft.empty(),"source SHD bit4 = loop, header context retained");
        check(!stream->stereo || stream->loopLeft.size()==stream->loopRight.size(),"source independent right loop context");
        re4::SoundMixer streamMixer;streamMixer.installStream(stream);streamMixer.setPaused(false);streamMixer.render(reference.data(),4800);
        bool audible=std::any_of(reference.begin(),reference.end(),[](auto x){return x!=0;});
        for (unsigned attempt=0;attempt<30 && !audible;++attempt) {
          streamMixer.render(reference.data(),4800);
          audible=std::any_of(reference.begin(),reference.end(),[](auto x){return x!=0;});
        }
        check(stream->volume==0 ? !audible : audible,"requested original stream renders source RIT gain "+std::to_string(block)+"/"+std::to_string(no));
        if (no==2 && block==0) check(stream->left.size()==1654642 && stream->loopLeft.size()==1282824,"battle stream2 exact source loop/sample extents");
      }
    }
    const auto core=re4::SoundBank::LoadDvd(base+"/etc/core.das");
    const auto roomSounds=re4::SoundBank::LoadDvd(base+"/St1/r101.das");
    re4::SoundMixer menu;menu.installBanks(core);menu.setPaused(false);menu.setSubscreen(true);
    for (unsigned cue:{4u,5u,6u,7u,8u,9u,10u,11u}) {
      check(menu.hasCue(0,cue),"original core menu cue present");
      const auto handle=menu.play(0,cue);check(handle!=0,"source SIT bit2 permits menu cue during subscreen");
      menu.render(output.data(),4800);check(std::any_of(output.begin(),output.end(),[](auto x){return x!=0;}),"source menu effect audible during game pause");menu.stop(handle);
    }
    re4::SoundMixer pausedWorld,referenceWorld;
    for(auto *m:{&pausedWorld,&referenceWorld}) {m->installBanks(roomSounds);m->setPaused(false);check(m->play(6,0x58)!=0,"original pyre spatial cue loaded");m->render(output.data(),4800);}
    pausedWorld.setSubscreen(true);pausedWorld.render(output.data(),4800);
    check(std::all_of(output.begin(),output.end(),[](auto x){return x==0;}),"subscreen freezes eligible original world SE");
    pausedWorld.setSubscreen(false);pausedWorld.render(output.data(),4800);referenceWorld.render(reference.data(),4800);
    check(output==reference,"subscreen resumes world loop from frozen DSP position");
    menu.installSequence(sequence);const auto musicBefore=menu.musicFrames();menu.render(output.data(),4800);
    check(menu.musicFrames()==musicBefore+4800,"source subscreen keeps original MIDI running");
    menu.installSequenceSlot(1,sequence);menu.render(output.data(),4800);
    check(menu.musicFrames(0)>0 && menu.musicFrames(1)==4800,"source second BGM slot has independent sample clock");
    menu.installSequenceSlot(0,nullptr);check(menu.hasMusic() && menu.musicFrames(0)==0,"clearing first source slot retains second");
    mixer.clear();check(!mixer.hasMusic() && mixer.musicFrames()==0,"session clear releases PCM/player");
    std::cout<<"M32 original audio: "<<checks<<" checks passed\n";
    return 0;
  } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
