#include "archive.h"
#include "enemy_feedback.h"
#include "enemy_encounter.h"
#include "ports/rnd.h"
#include "sound_bank.h"
#include "world_effects.h"
#include "weapon_preview.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks;
void require(bool value, const char *message) {
  ++checks;
  if (!value) throw std::runtime_error(message);
}
std::vector<std::uint8_t> sub(const RoomArchive &archive, const char *tag) {
  const auto *entry = archive.find(tag);
  require(entry, "Original source asset exists");
  return archive.copy(entry - archive.entries().data());
}
Sat flat() {
  std::vector<std::uint8_t> b(20+48+12+12+40);
  auto half=[&](unsigned p,unsigned value){ b.at(p)=std::uint8_t(value>>8);b.at(p+1)=std::uint8_t(value);};
  auto word=[&](unsigned p,unsigned value){half(p,value>>16);half(p+2,value);};
  b[0]=32;half(2,4);half(4,1);half(6,1);half(10,2);half(12,2);
  unsigned p=20;
  for(float f:{-100000.f,0.f,-100000.f,100000.f,0.f,-100000.f,
               100000.f,0.f,100000.f,-100000.f,0.f,100000.f,0.f,1.f,0.f}) {
    unsigned bits;std::memcpy(&bits,&f,4);word(p,bits);p+=4;
  }
  p+=12;half(p+2,2);half(p+4,1);half(p+22,3);half(p+24,2);
  return Sat(b);
}
PlayerShot aimedRay(const EnemyEncounter &enemy, bool head, unsigned serial) {
  const auto shapes=NormalGanadoHitShapes();
  const auto &shape=shapes.at(head?0:1);
  auto offset=shape.offset;offset[1]+=shape.height*.5f;
  const auto center=TransformPoint(WeaponAttachmentMatrix(
      enemy.actor().rig().jointMatrices().at(unsigned(shape.partsNo-1)),
      enemy.placement()),offset);
  for(unsigned axis:{0u,2u,1u}) for(double side:{1.,-1.}) {
    PlayerShot ray;ray.serial=serial;
    ray.from=ray.to={center[0],center[1],center[2]};
    ray.from[axis]-=side*800;ray.to[axis]+=side*800;
    const auto hit=HitAuthoredShapes(shapes,enemy.actor().rig().jointMatrices(),
                                   enemy.placement(),ray.from,ray.to,2560000);
    if(hit && (shapes.at(hit->partIndex).partsNo==5)==head) return ray;
  }
  throw std::runtime_error("No posed authored capsule ray");
}
void encounterFeedback(const SatCollision &floor,const char *path) {
  EnemySpawn spawn;spawn.room=0x100;spawn.id=0x12;spawn.type=0;
  spawn.listIndex=EnemyEncounter::TestSpawn;spawn.flags=0x24;spawn.hp=1000;
  EnemyEncounter enemy(floor,floor,spawn,path);
  const auto random=RndState();
  require(enemy.drainSounds().empty() && enemy.drainEffects().empty(),
          "Encounter load never invents an impact");
  auto shot=aimedRay(enemy,false,1);
  enemy.tick(enemy.feet(),&shot);
  require(enemy.drainSounds().empty() && enemy.drainEffects().empty(),
          "Feedback obeys original enemy-before-player deferred damage");
  enemy.tick(enemy.feet());
  const auto impacts=enemy.drainSounds();const auto blood=enemy.drainEffects();
  require(impacts.size()==1 && impacts[0].block==8 && impacts[0].number==0xc &&
          impacts[0].channel==EnemySoundChannel::Impact,"Original flesh impact8:0c");
  require(blood.size()==1 && blood[0].sequence==1 && blood[0].eventSerial==1,
          "Original damage activates exactly one EST1 at first enemy tick");
  require(enemy.drainSounds().empty() && enemy.drainEffects().empty(),
          "Draining feedback consumes it once");
  enemy.tick(enemy.feet(),&shot);
  const auto voice=enemy.drainSounds();
  require(voice.size()==2 && voice[0].number==0x47 &&
          voice[0].channel==EnemySoundChannel::Voice && voice[0].replace &&
          voice[1].channel==EnemySoundChannel::Breath && voice[1].stop,
          "Source body Timer3 voice is delayed one tick and stops breath");
  require(enemy.drainEffects().empty(),"Repeated shot serial never repeats blood");
  require(RndState()==random,"Encounter feedback preserves caller combat RNG");
  EnemyEncounter knife(floor,floor,spawn,path,true);
  const auto ray=aimedRay(knife,false,2);
  PlayerMeleeEvent swipe;swipe.kind=PlayerMeleeKind::Knife;swipe.serial=2;
  swipe.rays.push_back({ray.from,ray.to});
  knife.queueMelee(swipe,knife.feet());knife.tick(knife.feet());
  const auto cut=knife.drainEffects();
  require(cut.size()==1 && cut[0].sequence==0x29,"Knife applies original EST29");
  EnemyEncounter shotgun(floor,floor,spawn,path);
  shotgun.setFeedbackCamera({0,1600,3000});
  auto shell=aimedRay(shotgun,false,3);shell.weaponItemId=0x2c;
  shotgun.tick(shotgun.feet(),&shell);shotgun.tick(shotgun.feet());
  const auto heavyImpact=shotgun.drainSounds();const auto spray=shotgun.drainEffects();
  require(heavyImpact.size()==1 && heavyImpact[0].number==0x7a,
          "Near shotgun resolves source impact8:7a");
  require(spray.size()==1 && spray[0].sequence==0x36,
          "Near shotgun and camera resolve source large blood36");
}
}
int main(int argc,char **argv) {
  try {
    if(argc!=3) throw std::runtime_error("Usage: enemy_feedback_check EM12.drs CORE.das");
    const auto enemy=RoomArchive::LoadDrs(argv[1]);
    const auto core=RoomArchive::LoadDrs(argv[2]);
    const auto data=BuildEnemyWorldEffects(sub(enemy,"EFF"),sub(core,"EFF"));
    require(data.originalEnemyBlood && data.triggerBanks.size()==8,"All selected original blood banks");
    const auto banks=SoundBank::LoadDvd(argv[1]);
    const SoundBank *sound=nullptr;
    for(const auto &bank:banks) if(bank->block()==8) sound=bank.get();
    require(sound,"EM12 original block8 is present");
    for(unsigned slot:{0u,2u,5u,7u,8u,11u,13u})
      require(sound->cue(GanadoVoiceCue(slot)),"Source original type0 voice cue resolves");
    for(unsigned cue:{0xcu,0x7au,0x85u})
      require(sound->cue(cue),"Source impact/choke cue resolves");
    const auto sat=flat();const SatCollision floor(sat);
    encounterFeedback(floor,argv[1]);
    WorldEffectCamera camera; camera.eye={0,1500,5000};
    const auto random=RndState();
    unsigned totalDecals=0,totalQuads=0;
    for(const auto &bank:data.triggerBanks) {
      WorldEffectsSimulation effect(data);
      require(effect.state().sprites.empty(),"Blood never activates on room load");
      require(effect.trigger(bank.sequence,{0,1600,0},{0,1.1f,0},1,&floor),"Source EST trigger accepted");
      require(!effect.trigger(bank.sequence,{0,1600,0},{},1,&floor),"Shot event cannot duplicate blood");
      for(unsigned frame=0;frame<180;++frame) {
        effect.tick(&camera,&floor);
        const auto &state=effect.state();
        require(state.sprites.size()<=512,"Source shared particle pool bounded");
        for(const auto &sprite:state.sprites) {
          const auto &record=data.sprites.targets.at(sprite.target).records.at(sprite.record);
          for(float value:sprite.position) require(std::isfinite(value),"Blood position finite");
          if(record.bytes[1]==0x10) {
            ++totalDecals;
            require(std::abs(sprite.position[1]-65)<.0001f,"Esp10 original floor+65 rule");
          }
        }
        const auto quads=WorldEffectQuads(data,state,camera);totalQuads+=unsigned(quads.size());
        for(const auto &quad:quads) for(const auto &point:quad.position)
          for(float value:point) require(std::isfinite(value),"Original blood geometry finite");
        if(frame==30) {
          const auto checkpoint=state;
          require(effect.restore(checkpoint),"Complete sprite/decal checkpoint restoration");
        }
      }
    }
    require(totalDecals>0 && totalQuads>0,"Original spray and floor blood render");
    require(RndState()==random,"Blood appearance never alters combat RNG");
    std::cout<<data.describe()<<'\n'<<"Original EM12 audio/blood checks: "<<checks
             <<"; floor decal samples: "<<totalDecals<<"; quads: "<<totalQuads<<'\n';
    return 0;
  } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
