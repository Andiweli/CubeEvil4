#include "sound_mixer.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks;
void require(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
}
int main(int argc,char **argv){try{
 require(argc==5,"Usage: enemy_audio_banks_check EM12.DRS EM21.DRS EM23.DRS EM2A.DRS");
 std::array<std::vector<std::shared_ptr<const re4::SoundBank>>,4> banks;
 const std::array<unsigned,4> args{0x10,0x21,0x23,0x2a};
 for(unsigned i=0;i<4;++i) {
  banks[i]=re4::SoundBank::LoadDvd(argv[i+1]);
  require(banks[i].size()==1&&banks[i][0]->block()==8&&banks[i][0]->argument()==args[i],"original bank id/arg mismatch");
 }
 const unsigned cue=6;
 re4::SoundMixer combined;combined.setPaused(false);
 for(const auto &bank:banks)combined.installBanks(bank);
 for(unsigned i=0;i<4;++i) {
  require(combined.hasCue(8,cue,args[i]),"coexisting original same-block bank missing");
  const auto handle=combined.play(8,cue,1,-1,false,args[i]);require(handle!=0,"specific bank voice failed");
  combined.stop(handle);
 }
 require(!combined.hasCue(8,cue,0x29)&&!combined.play(8,cue,1,-1,false,0x29),"missing explicit bank fellback");
 for(unsigned i=0;i<4;++i) {
  // Selection must render precisely the same decoded sample as that bank
  // alone, even when other animals share block8 and the same cue number.
  re4::SoundMixer solo,multi;solo.setPaused(false);multi.setPaused(false);
  solo.installBanks(banks[i]);for(const auto &bank:banks)multi.installBanks(bank);
  require(solo.play(8,cue,1,-1,false)!=0&&multi.play(8,cue,1,-1,false,args[i])!=0,"isolated/selected play failed");
  std::array<std::int16_t,4096> expected{},actual{};solo.render(expected.data(),2048);multi.render(actual.data(),2048);
  require(expected==actual,"bank collision changed decoded output PCM");
 }
 // Existing block-only weapon/current-bank behavior remains compatible.
 combined.clear();combined.setPaused(false);combined.installBanks(banks[0]);
 require(combined.hasCue(8,cue)&&!combined.hasCue(8,cue,args[1]),"legacy singlebank lookup");
 combined.installBanks(banks[2]);require(combined.hasCue(8,cue,args[0]),"secondbank discarded earlierone");
 re4::SoundMixer legacy,reference;legacy.setPaused(false);reference.setPaused(false);
 legacy.installBanks(banks[0]);legacy.installBanks(banks[2]);reference.installBanks(banks[2]);
 legacy.play(8,cue);reference.play(8,cue);
 std::array<std::int16_t,2048> expected{},actual{};reference.render(expected.data(),1024);legacy.render(actual.data(),1024);
 require(expected==actual,"legacy currentbank changed");
 combined.installBanks({},true);for(unsigned arg:args)require(combined.hasCue(8,cue,arg)==(arg==args[0]||arg==args[2]),"room bank replacement erased enemybank");
 combined.clear();for(unsigned arg:args)require(!combined.hasCue(8,cue,arg),"clear left argumentbank");
 require(!combined.hasCue(8,cue),"clear left legacy defaultbank");
 std::cout<<"M37 enemy sound banks: "<<checks<<" checks PASS; EM12/21/23/2A coexist in block8, explicit PCM matches isolated bank.\n";return 0;
 }catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
