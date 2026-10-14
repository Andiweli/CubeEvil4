#include "sound_mixer.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks;
void require(bool ok,const char *message) {
  ++checks;if(!ok)throw std::runtime_error(message);
}
}
int main(int argc,char **argv) {
  try {
    if(argc!=2)throw std::runtime_error("Usage: m37_startup_audio_check original-ss/Cmn/title.snd");
    const auto banks=re4::SoundBank::LoadDvd(argv[1]);
    const auto found=std::find_if(banks.begin(),banks.end(),[](const auto &bank){return bank->block()==6;});
    require(found!=banks.end(),"Original title bank6 absent");
    const auto &bank=**found;
    double sourceOutputFrames=0;
    unsigned layers=0;
    for(unsigned number=2;;++number) {
      const auto *cue=bank.cue(number);
      require(cue!=nullptr,"Original START GAME layer absent");
      require(cue->pitchLow==0&&cue->pitchHigh==0&&cue->sample->loopPcm.empty(),
          "START GAME fixture has an unexpected source pitch/loop");
      sourceOutputFrames=std::max(sourceOutputFrames,
          cue->sample->pcm.size()*double(re4::SoundMixer::OutputRate)/
          std::min(cue->sample->rate*cue->pitchRatio,128000.));
      ++layers;if(!cue->chained())break;
    }
    re4::SoundMixer mixer;mixer.installBanks(banks);mixer.setPaused(false);
    const auto voice=mixer.play(6,2);
    require(voice!=0&&mixer.voiceActive(voice),"Original SndCall(6,2) did not become active");
    require(mixer.voiceCount()==layers,"Original chained title layers lost shared voice ownership");
    std::vector<std::int16_t> pcm(1024);std::size_t rendered=0;
    while(rendered<3*re4::SoundMixer::OutputRate) {
      const auto frames=std::min<std::size_t>(512,3*re4::SoundMixer::OutputRate-rendered);
      mixer.render(pcm.data(),frames);rendered+=frames;
    }
    require(sourceOutputFrames>rendered&&mixer.voiceActive(voice),
        "A fixed 90-frame fade incorrectly equals the complete source spoken cue");
    const auto count=mixer.voiceCount();mixer.setPaused(true);mixer.render(pcm.data(),512);
    require(mixer.voiceActive(voice)&&mixer.voiceCount()==count&&
        std::all_of(pcm.begin(),pcm.end(),[](auto sample){return sample==0;}),
        "Lifecycle pause consumed or emitted protected spoken cue samples");
    mixer.setPaused(false);
    while(mixer.voiceActive(voice)) {
      require(rendered<=std::size_t(std::ceil(sourceOutputFrames))+1,
          "Title voice completion missed the source final PCM frame");
      mixer.render(pcm.data(),1);++rendered;
    }
    require(std::abs(double(rendered)-std::ceil(sourceOutputFrames))<=1,
        "Title voice ended before all original chained layer samples");
    require(!mixer.voiceActive(voice)&&mixer.voiceCount()==0,
        "Finished original voice retained a stale active handle");
    std::cout<<"M37 original START GAME voice: "<<checks<<" checks PASS; "<<layers
             <<" layers, "<<rendered<<" output frames ("<<rendered/48000.
             <<" s), original fade 3 s\n";
  } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
