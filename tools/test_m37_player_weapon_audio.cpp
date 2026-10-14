#include "leon_preview.h"
#include "walk_probe.h"
#include "sound_mixer.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks;
void require(bool ok,const char *why) {++checks;if(!ok)throw std::runtime_error(why);}
re4::CollisionVec point(const re4::LeonPreview &player,unsigned part) {
  const auto &m=player.rig().jointMatrices().at(part);
  const auto p=re4::TransformPoint(player.placement(),{m[3],m[7],m[11]});return {p[0],p[1],p[2]};
}
std::vector<std::uint8_t> sub(const re4::RoomArchive &archive,const char *tag) {
  const auto *entry=archive.find(tag);require(entry,"Actual room subfile exists");return archive.copy(entry-archive.entries().data());
}
void checkCue(const re4::PlayerWeaponSound &cue,unsigned block,unsigned number,unsigned part,
              float frame,const re4::CollisionVec &position) {
  require(cue.block==block && cue.number==number && cue.part==part && cue.frame==frame,
      "Actual source SndCall block/cue/part and call frame match the original weapon routine");
  require(cue.position==position,"Source sound retains the actual part position at its original pre/post Motion call point");
}
}
int main(int argc,char **argv) {try {
  require(argc==7,"Usage: m37_player_weapon_audio_check R100 PL00 WEP01 WEP02 WEP07 EM12");
  const auto room=re4::RoomArchive::Load(argv[1]);const re4::Sat sat(sub(room,"SAT"));const re4::SatCollision collision(sat);
  // Original ready10 calls at Seq_frame2 BEFORE MotionMove; shotgun's pump
  // cue is at10 AFTER MotionMove. Validate live part positions at both points
  // and fixed tick ownership under different Android rendering frequencies.
  for(unsigned profile:{0u,1u,2u})for(unsigned frequency:{30u,60u,120u}) {
    re4::LeonPreview player(argv[2],argv[3]);
    if(profile==1)player.installHandgun(argv[4]);
    if(profile==2)player.installShotgun(argv[5]);
    if(profile)require(player.equipWeapon(profile==1?0x23:0x2c),"Actual early weapon equips");
    re4::WalkProbe probe(collision);require(probe.reset(),"Actual r100 floor starts player");player.setPlacement(probe.position(),.57f);
    re4::PlayerActionInput input;input.aimHeld=true;unsigned draws=0,pumps=0;
    for(unsigned call=0;call<frequency*3 && player.action()!=re4::PlayerAction::Aim;++call) {
      const auto before=point(player,0);player.step(probe,0,0,false,false,1./frequency,input);
      for(const auto &cue:player.drainWeaponSounds()) {
        if(cue.block==1) {checkCue(cue,1,0x28,0,2,before);require(cue.tick==3,"Original draw cue belongs to native tick3 at every render frequency");++draws;}
        else {checkCue(cue,2,9,0xa,10,point(player,0xa));require(cue.tick==10,"Original shotgun ready-end cue is native tick10");++pumps;}
      }
    }
    require(player.action()==re4::PlayerAction::Aim && draws==1 && pumps==(profile==2?1u:0u),
        "Each real weapon draw emits exactly its original calls without bridge-dependent duplication");
    require(player.drainWeaponSounds().empty(),"Cue drain cannot replay the same source call");
    // A physical trigger with no magazine/reserve clicks once. Held trigger
    // is silent until a new trigger; an automatic reload produces no click.
    player.setWeaponAmmo(profile==2?0x2c:profile==1?0x23:0x21,0,0);
    input.firePressed=input.fireHeld=true;player.step(probe,0,0,false,false,1./30,input);
    const auto empty=player.drainWeaponSounds();require(empty.size()==1,"Original empty fire trigger emits one click");
    checkCue(empty[0],2,profile==2?3:0x17,4,player.rig().sequenceFrame(),point(player,4));
    input.firePressed=false;
    for(unsigned tick=0;tick<80;++tick) {player.step(probe,0,0,false,false,1./30,input);require(player.drainWeaponSounds().empty(),"Held empty fire never repeats the trigger-only click");}
    input.firePressed=true;player.step(probe,0,0,false,false,1./30,input);require(player.drainWeaponSounds().size()==1,"A second physical empty trigger clicks once");
    player.setWeaponAmmo(profile==2?0x2c:profile==1?0x23:0x21,0,profile==2?6:10);
    player.step(probe,0,0,false,false,1./30,input);
    require(player.action()==re4::PlayerAction::Reload && player.drainWeaponSounds().empty(),"Reloadable empty gun follows original automatic reload without empty sound");
  }
  // Source fire10 emits the held last-shell click only after finishing its
  // complete pump, while aim and fire remain held. Test both release gates.
  for(unsigned release:{0u,1u,2u}) {
    re4::LeonPreview player(argv[2],argv[3]);player.installShotgun(argv[5]);player.equipWeapon(0x2c);
    re4::WalkProbe probe(collision);require(probe.reset(),"Actual shotgun pump floor starts");player.setPlacement(probe.position(),.27f);
    re4::PlayerActionInput input;input.aimHeld=true;
    for(unsigned tick=0;tick<60 && player.action()!=re4::PlayerAction::Aim;++tick)player.step(probe,0,0,false,false,1./30,input);
    player.drainWeaponSounds();player.setWeaponAmmo(0x2c,1,0);input.firePressed=input.fireHeld=true;
    player.step(probe,0,0,false,false,1./30,input);input.firePressed=false;
    if(release==1)input.fireHeld=false;
    if(release==2)input.aimHeld=false;
    unsigned clicks=0;
    for(unsigned tick=0;tick<55;++tick) {
      player.step(probe,0,0,false,false,1./30,input);
      for(const auto &cue:player.drainWeaponSounds()) {checkCue(cue,2,3,4,40,point(player,4));++clicks;}
    }
    require(clicks==(release==0?1u:0u),"Last-shell pump click preserves both original held-input gates");
  }
  re4::LeonPreview knife(argv[2],argv[3],argv[6],true);re4::WalkProbe probe(collision);require(probe.reset(),"Actual knife floor starts");knife.setPlacement(probe.position(),0);
  re4::PlayerActionInput input;input.knifeHeld=true;
  for(unsigned tick=0;tick<90 && knife.action()!=re4::PlayerAction::KnifeReady;++tick)knife.step(probe,0,0,false,false,1./30,input);
  require(knife.action()==re4::PlayerAction::KnifeReady && knife.drainWeaponSounds().empty(),"Dry original knife draw does not invent a gun/sheath call");
  input.firePressed=input.fireHeld=true;knife.step(probe,0,0,false,false,1./30,input);input.firePressed=input.fireHeld=false;
  unsigned swishes=0;
  for(unsigned tick=0;tick<90 && knife.action()!=re4::PlayerAction::KnifeReady;++tick) {
    const auto before=point(knife,4);knife.step(probe,0,0,false,false,1./30,input);
    // Original MotionCheckCrossFrame reads Mot_frame, not Seq_frame. This
    // actual slash's curve crossing3 carries Seq_frame4 before MotionMove.
    for(const auto &cue:knife.drainWeaponSounds()) {checkCue(cue,1,3,4,4,before);++swishes;}
  }
  require(swishes==1,"Actual original knife FCV slash emits one pre-Motion cross-frame swish");
  input.knifeHeld=false;
  for(unsigned tick=0;tick<90 && knife.action()!=re4::PlayerAction::Footwork;++tick) {knife.step(probe,0,0,false,false,1./30,input);require(knife.drainWeaponSounds().empty(),"Dry original knife down has no explicit splash/sheath SndCall");}
  // Confirm each chosen call decodes an actual resident Disc cue, retaining
  // the selected WEP bank rather than borrowing a generic enemy sample.
  for(unsigned profile:{0u,1u,2u}) {
    re4::SoundMixer mixer;mixer.installBanks(re4::SoundBank::LoadDvd(argv[2]));
    mixer.installBanks(re4::SoundBank::LoadDvd(argv[profile==0?3:profile==1?4:5]));mixer.setPaused(false);
    const std::array<std::pair<unsigned,unsigned>,4> cues{{{1,0x28},{1,3},{2,profile==2?3u:0x17u},{2,profile==2?9u:0u}}};
    for(const auto &cue:cues) {
      require(mixer.hasCue(cue.first,cue.second),"Original selected PL/WEP sound bank contains the actual routine cue");
      require(mixer.play(cue.first,cue.second)!=0,"Original routine cue creates a decoded PCM voice");
      std::array<std::int16_t,4096> pcm{};mixer.render(pcm.data(),2048);
      require(std::any_of(pcm.begin(),pcm.end(),[](auto sample){return sample!=0;}),"Actual source routine sound produces audible PCM");
    }
  }
  std::cout<<"M37 original player weapon sound: "<<checks<<" checks PASS; actual draw/empty/pump/knife call points and decoded PL/WEP PCM.\n";return 0;
}catch(const std::exception &error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
