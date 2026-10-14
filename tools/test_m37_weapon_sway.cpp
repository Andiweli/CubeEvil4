#include "weapon_sway.h"
#include "leon_preview.h"
#include "walk_probe.h"
#include "ports/rnd.h"
#include "oracles/player_eyes_original.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks;
void require(bool value,const char *why) {++checks;if(!value)throw std::runtime_error(why);}
using f32=float;
constexpr float PI=3.1415927f;
float lockRandCtr;
std::uint16_t sourceSeed;
unsigned sourceDraws;
unsigned byte() {
  const unsigned n=(unsigned(std::uint8_t((sourceSeed>>1)+(sourceSeed>>8)))<<8)|std::uint8_t(sourceSeed>>1);
  sourceSeed=std::uint16_t(n==sourceSeed?n+0x101:n);return std::uint8_t(sourceSeed>>8);
}
float randomSample(std::uint16_t &seed) {
  const auto previous=sourceSeed;sourceSeed=seed;
  const unsigned a=byte(),b=byte(),c=byte();seed=sourceSeed;sourceSeed=previous;
  const std::uint32_t bits=a+(b<<8)+((c&127)<<16)+0x3f800000u;
  float value;std::memcpy(&value,&bits,4);return value*2.f-3.f;
}
float fRand1_1() {++sourceDraws;auto seed=sourceSeed;const auto value=randomSample(seed);sourceSeed=seed;return value;}
struct cObjWep {
  re4::WeaponSwayAbility ability;
  void getAbility(float *a,float *b,float *c,float *d) {*a=ability.pitchRange;*b=ability.yawRange;*c=ability.pitchStep;*d=ability.yawStep;}
};
struct cPlWep {cObjWep *m_pWep;float pitch{},m_CenterY{};};
struct cModel {};
struct cPlayer:cModel {cPlWep *Wep;};
#include "oracles/weapon_sway_original.inc"
std::vector<std::uint8_t> sub(const re4::RoomArchive &archive,const char *tag) {
  const auto *entry=archive.find(tag);require(entry,"Actual room subfile exists");return archive.copy(entry-archive.entries().data());
}
}
int main(int argc,char **argv) {
  try {
    const auto defaultAbility=re4::OriginalDefaultWeaponSwayAbility();
    require(defaultAbility.pitchRange==5.73f*.017453292f && defaultAbility.yawRange==2.86f*.017453292f &&
        defaultAbility.pitchStep==.2864f*.017453292f && defaultAbility.yawStep==.2864f*.017453292f,
        "Actual default cObjWep ability uses the original degree conversion");
    for(unsigned seed=0;seed<128;++seed)for(float factor:{0.f,.2f}) {
      cObjWep weapon{defaultAbility};cPlWep aim{&weapon};cPlayer player;player.Wep=&aim;
      re4::WeaponSwayState native;native.amplitude=factor;lockRandCtr=factor;
      sourceSeed=std::uint16_t(seed*509);auto nativeSeed=sourceSeed;
      float pitch=0,yaw=0,nativePitch=0,nativeYaw=0;sourceDraws=0;
      for(unsigned tick=0;tick<512;++tick) {
        const unsigned flags=tick%31==0?1u:tick%17==0?2u:tick%29==0?3u:0u;
        if(flags&1) {pitch=float(int(tick%19)-9)*.1f;yaw=float(int(tick%23)-11)*.1f;nativePitch=pitch;nativeYaw=yaw;}
        const auto previousDraws=sourceDraws;
        PlWepLockRand(&player,int(flags),&pitch,&yaw);
        const float rp=(flags&1)?0:randomSample(nativeSeed),ry=(flags&1)?0:randomSample(nativeSeed);
        re4::OriginalWeaponSway(native,defaultAbility,flags,rp,ry,nativePitch,nativeYaw);
        require(nativePitch==pitch && nativeYaw==yaw,"Literal source sway/recoil float outputs match exactly");
        require(native.pitchCenter==aim.pitch && native.yawCenter==aim.m_CenterY,"Original centers update only in the moved branch");
        require(nativeSeed==sourceSeed && sourceDraws-previousDraws==((flags&1)?0u:2u),"Original random sequence consumes exactly zero or two samples");
      }
    }
    if(argc==1) {std::cout<<"M37 original weapon sway: "<<checks<<" checks PASS.\n";return 0;}
    require(argc==4,"Usage: m37_weapon_sway_check R100 PL00 WEP01");
    const auto room=re4::RoomArchive::Load(argv[1]);const re4::Sat sat(sub(room,"SAT"));const re4::SatCollision collision(sat);
    RndInit(0x5271);re4::LeonPreview leon(argv[2],argv[3]);re4::WalkProbe probe(collision);
    require(probe.reset(),"Actual r100 floor starts native Leon");leon.setPlacement(probe.position(),0);
    const auto playerArchive=re4::RoomArchive::LoadDrs(argv[2]);const re4::SkinnedModel sourceGrip(playerArchive.copy(18));
    unsigned sourceCorners=0,radioCorners=0;
    for(const auto &primitive:sourceGrip.mesh.primitives)sourceCorners+=unsigned(primitive.triangles.size()*3);
    for(const auto &draw:leon.renderMesh().draws)if(draw.work==618)radioCorners+=draw.count;
    require(sourceCorners && radioCorners==sourceCorners,"Radio actual source grip BIN18 is preloaded exactly once without shotgun");
    require(leon.leftHandType()==4 && leon.visiblePart(20) && !leon.visiblePart(618),"Original starting left handgun grip remains selected");
    leon.setRoomScriptLeftHand(2);
    require(leon.leftHandType()==2 && !leon.visiblePart(20) && leon.visiblePart(618),"Radio switches to actual source grip2 independent of weapon profile");
    leon.setRoomScriptLeftHand(0x63);
    require(leon.leftHandType()==4 && leon.visiblePart(20) && !leon.visiblePart(618),"Original oldLhand restore63 returns the actual previous grip");
    leon.setRoomScriptLeftHand(0x63);
    require(leon.leftHandType()==2,"Repeated source63 uses the newly updated oldLhandNo");
    leon.setRoomScriptLeftHand(0x63);
    re4::PlayerActionInput held;held.aimHeld=true;
    for(unsigned tick=0;tick<90 && leon.action()!=re4::PlayerAction::Aim;++tick)
      leon.step(probe,0,0,false,false,1./30,held);
    require(leon.action()==re4::PlayerAction::Aim,"Actual handgun draw reaches source idle aim");
    cObjWep weapon{defaultAbility};cPlWep aim{&weapon,leon.weaponSwayState().pitchCenter,leon.weaponSwayState().yawCenter};
    cPlayer source;source.Wep=&aim;lockRandCtr=0;sourceSeed=leon.weaponRandomState();
    player_eye_oracle::OriginalEyeOracle eyes;eyes.restore(leon.eyeState());
    float pitch=leon.aimRate(),yaw=leon.weaponAimYaw(),waist=leon.waistYaw();
    const auto centerPitch=pitch;bool moved=false;
    for(unsigned tick=0;tick<300;++tick) {
      PlWepLockRand(&source,0,&pitch,&yaw);waist=waist*.6f+yaw*.4f;
      eyes.seed=sourceSeed;eyes.moveEyeNormal();sourceSeed=eyes.seed;
      const auto global=RndState();leon.step(probe,0,0,false,false,1./30,held);
      require(leon.aimRate()==pitch && leon.weaponAimYaw()==yaw && leon.waistYaw()==waist,
          "Real native handgun idle executes the literal source walk and waist smoothing");
      require(leon.weaponRandomState()==sourceSeed && RndState()==global,"Native player owns source RNG without consuming another owner's sequence");
      moved|=pitch!=centerPitch;
    }
    require(moved,"The real idle muzzle now has original sway");
    // fire00 traces two original spread samples, recenters with two fresh
    // samples, then handgun control walks twice before moveEye consumes its
    // own bytes. The authored pose was solved at the preceding aim rate.
    re4::PlayerActionInput fire=held;fire.firePressed=fire.fireHeld=true;
    const auto beforeShots=leon.shots();
    auto spreadSeed=sourceSeed;(void)randomSample(spreadSeed);(void)randomSample(spreadSeed);sourceSeed=spreadSeed;
    PlWepLockRand(&source,2,&pitch,&yaw);
    PlWepLockRand(&source,0,&pitch,&yaw);
    waist=waist*.6f+leon.weaponAimYaw()*.4f;
    waist=waist*.6f+yaw*.4f;
    eyes.seed=sourceSeed;eyes.moveEyeNormal();sourceSeed=eyes.seed;
    leon.step(probe,0,0,false,false,1./30,fire);
    require(leon.shots()==beforeShots+1 && leon.action()==re4::PlayerAction::Fire,
        "Actual handgun fire executes one source bullet without needing an afterTick observer");
    require(leon.aimRate()==pitch && leon.weaponAimYaw()==yaw && leon.waistYaw()==waist && leon.weaponRandomState()==sourceSeed,
        "Actual shot preserves source spread/fresh/idle/eye random order and its two waist solves");
    const auto beforePause=leon.rig().bones();const auto seedPause=leon.weaponRandomState();
    leon.suspendClock();require(leon.rig().bones()==beforePause && leon.weaponRandomState()==seedPause,"Pause does not advance sway or pose");
    re4::LeonPreview restored(argv[2],argv[3]);restored.inheritInventory(leon);
    require(restored.weaponRandomState()==leon.weaponRandomState(),"Room reconstruction retains the authoritative player random state");
    std::cout<<"M37 original weapon sway/radio grip: "<<checks<<" checks PASS; actual native aim and exact source float/RNG oracle.\n";
    return 0;
  } catch(const std::exception &error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
