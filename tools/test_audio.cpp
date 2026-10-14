#include "sound_bank.h"
#include "sound_mixer.h"
#include "sound_acoustics.h"
#include "archive.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main(int argc,char **argv) {
  if (argc<2) { std::cerr<<"Usage: audio_check original-packed-file... [decoded-room.dat...]\n";return 2; }
  try {
    std::size_t checks=0,cues=0;
    re4::SoundMixer mixer;
    mixer.setPaused(false);
    std::vector<std::int16_t> output(4096*2);
    std::vector<std::shared_ptr<const re4::SoundBank>> lastBanks;
    for (int path=1;path<argc;++path) {
      const std::string filename=argv[path];
      if (filename.size()>=4 && filename.substr(filename.size()-4)==".dat") {
        const re4::SoundAcoustics acoustic(re4::RoomArchive::Load(filename));
        for (const auto &bank:lastBanks) {
          for (unsigned number=0;number<bank->cueCount();++number) {
            const auto *cue=bank->cue(number);
            if (!cue) continue;
            for (double distance:{0.,1000.,5000.,10000.,50000.,100000.}) {
              const auto p=acoustic.parameters(*cue,{distance,100,0},{},{},{1,0,0});
              assert(std::isfinite(p.gain) && p.gain>=0 && p.gain<=2);++checks;
              assert(p.pan>=0 && p.pan<=127);++checks;
              assert(std::isfinite(p.pitchRatio) && p.pitchRatio>0);++checks;
              assert(p.filter>=-1 && p.filter<=23);++checks;
            }
          }
        }
        std::cout<<filename<<": original STB curves checked\n";
        continue;
      }
      const auto banks=re4::SoundBank::LoadDvd(argv[path]);
      lastBanks=banks;
      // Some original rooms intentionally contain no local audio sections.
      // Their well-formed DVD table is still a valid parser result.
      ++checks;
      if (banks.empty()) std::cout<<filename<<": no local sound bank\n";
      mixer.installBanks(banks,true);
      for (const auto &bank:banks) {
        std::cout<<argv[path]<<": "<<bank->describe()<<"\n";
        for (unsigned number=0;number<bank->cueCount();++number) {
          const auto *cue=bank->cue(number);
          if (!cue) continue;
          ++cues;
          assert(cue->sample->rate>=4000 && cue->sample->rate<=48000);++checks;
          assert(!cue->sample->pcm.empty());++checks;
          assert(std::isfinite(cue->pitchRatio) && cue->pitchRatio>0);++checks;
          const auto handle=mixer.play(bank->block(),number,1,-1,false);
          assert(handle);++checks;
          mixer.render(output.data(),4096);++checks;
          mixer.stop(handle);
          assert(!mixer.voiceCount());++checks;
        }
      }
    }
    std::cout<<"Original audio: "<<cues<<" cues, "<<checks<<" checks passed\n";
  } catch (const std::exception &failure) {
    std::cerr<<failure.what()<<"\n";return 1;
  }
}
