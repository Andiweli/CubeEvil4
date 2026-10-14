#include "player_eyes.h"
#include "leon_preview.h"
#include "walk_probe.h"
#include "ports/rnd.h"
#include "oracles/player_eyes_original.h"
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks;
void require(bool ok,const char *why) {++checks;if(!ok)throw std::runtime_error(why);}
bool sameState(const re4::PlayerEyeState &a,const re4::PlayerEyeState &b) {
  return a.direction==b.direction && a.target==b.target && a.delay==b.delay &&
      a.blinkTimer==b.blinkTimer && a.wanderTimer==b.wanderTimer;
}
std::vector<std::uint8_t> sub(const re4::RoomArchive &archive,const char *tag) {
  const auto *entry=archive.find(tag);require(entry,"Actual room subfile exists");return archive.copy(entry-archive.entries().data());
}
}
int main(int argc,char **argv) {try {
  bool doubleBlink=false;
  for(unsigned initial=0;initial<64;++initial) {
    player_eye_oracle::OriginalEyeOracle source,random;source.seed=random.seed=std::uint16_t(initial*509);
    re4::PlayerEyeState native;
    for(unsigned tick=0;tick<8192;++tick) {
      const float motionLid=float(int(tick%15)-7)*.01f;
      source.parts[0x1c].ang.x=motionLid;source.moveEyeNormal();
      const auto angles=re4::OriginalPlayerEyeUpdate(native,[&]{return random.Rnd();},motionLid);
      require(source.parts[0x1c].ang.x==angles.lidPitch && source.parts[0x20].ang.y==angles.eyeYaw &&
          source.parts[0x21].ang.y==angles.eyeYaw,"Original eyelid/eye angles match bit exactly including unchanged motion cases");
      require(sameState(source.state(),native),"Original cDelay, blink and wander counters match exactly");
      require(source.seed==random.seed && source.draws==random.draws,"Original eye random bytes and their consumption order match");
      doubleBlink|=native.blinkTimer==0x5d;
    }
  }
  require(doubleBlink,"Literal original random branch exercises the complete secondary blink");
  if(argc==1) {std::cout<<"M37 original player eyes: "<<checks<<" checks PASS.\n";return 0;}
  require(argc==4,"Usage: m37_player_eyes_check R100 PL00 WEP01");
  const auto room=re4::RoomArchive::Load(argv[1]);const re4::Sat sat(sub(room,"SAT"));const re4::SatCollision collision(sat);
  RndInit(0x5271);re4::LeonPreview leon(argv[2],argv[3]);re4::WalkProbe probe(collision);
  require(probe.reset(),"Actual original r100 floor starts Leon");leon.setPlacement(probe.position(),0);
  player_eye_oracle::OriginalEyeOracle source;source.seed=leon.weaponRandomState();
  for(unsigned tick=0;tick<1000;++tick) {
    const auto before=source.timer;source.moveEyeNormal();const auto global=RndState();
    leon.step(probe,0,0,false,false,1./30,{});
    require(leon.rig().partRotation(0x20)[1]==source.parts[0x20].ang.y &&
        leon.rig().partRotation(0x21)[1]==source.parts[0x21].ang.y,"Actual live body applies both original eye directions");
    if(before!=0x1e && before!=0x58)require(leon.rig().partRotation(0x1c)[0]==source.parts[0x1c].ang.x,
        "Actual live body applies the original blink to the authored eyelid joint");
    require(sameState(leon.eyeState(),source.state()) && leon.weaponRandomState()==source.seed && RndState()==global,
        "Actual player eye clocks and owned RNG match the literal source without touching another owner");
  }
  const auto player=re4::RoomArchive::LoadDrs(argv[2]);
  const unsigned clip=leon.installRoomScriptMotion({player.copy(117),{}});
  leon.beginRoomScriptMotion(probe,probe.position(),0,clip,5,0x201);
  leon.setRoomScriptEyeMode(1);const auto pausedEyes=leon.eyeState();const auto seed=leon.weaponRandomState();
  bool asymmetricSource=false;
  re4::SkinnedModel body(player.copy(0));
  re4::AnimationRig raw(body,std::vector<re4::AnimationBytes>{{player.copy(117),{}}});raw.startActionFlags(0,5,0,0x201);
  for(unsigned tick=0;tick<100;++tick) {
    raw.advanceRootAction();leon.step(probe,0,0,false,false,1./30,{});
    asymmetricSource|=raw.partRotation(0x20)!=raw.partRotation(0x21);
    require(leon.rig().partRotation(0x21)==leon.rig().partRotation(0x20),
        "Radio motion eye mode copies all three authored left-eye angles into the right eye");
    require(sameState(leon.eyeState(),pausedEyes) && leon.weaponRandomState()==seed,
        "Motion eye mode suspends the complete normal eye clocks and random consumption");
  }
  require(asymmetricSource,"Actual radio FCV needs the source parts20 to21 adapter");
  leon.setRoomScriptEyeMode(0);leon.endRoomScriptMotion();source.seed=seed;source.restore(pausedEyes);source.moveEyeNormal();
  leon.step(probe,0,0,false,false,1./30,{});
  require(sameState(leon.eyeState(),source.state()) && leon.weaponRandomState()==source.seed,
      "Leaving radio resumes the original blink/wander clocks at their retained state");
  re4::LeonPreview next(argv[2],argv[3]);next.inheritInventory(leon);
  require(sameState(next.eyeState(),leon.eyeState()) && next.weaponRandomState()==leon.weaponRandomState(),
      "Room reconstruction preserves single-player original eye statics and source RNG");
  std::cout<<"M37 original player eyes: "<<checks<<" checks PASS; actual radio and literal blink/wander source.\n";
  return 0;
}catch(const std::exception &error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
