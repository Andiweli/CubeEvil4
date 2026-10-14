#include "enemy_encounter.h"
#include "archive.h"
#include "r100_scenario.h"
#include "weapon_preview.h"
#include "sound_bank.h"
#include "sound_footsteps.h"
#include "ports/rnd.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks;
void require(bool v,const char *s){++checks;if(!v)throw std::runtime_error(s);}
std::vector<std::uint8_t> sub(const re4::RoomArchive &a,const char *tag){const auto *e=a.find(tag);require(e,"original room entry");return a.copy(e-a.entries().data());}
std::vector<std::uint8_t> read(const char *p){std::ifstream f(p,std::ios::binary|std::ios::ate);require(bool(f),"ESL exists");std::vector<std::uint8_t>b(std::size_t(f.tellg()));f.seekg(0);require(bool(f.read(reinterpret_cast<char*>(b.data()),b.size())),"ESL read");return b;}
re4::PlayerShot bodyShot(const re4::EnemyEncounter &enemy,unsigned serial){
 const auto shapes=re4::NormalGanadoHitShapes();const auto &shape=shapes[1];
 auto offset=shape.offset;offset[1]+=shape.height*.5f;
 const auto center=re4::TransformPoint(re4::WeaponAttachmentMatrix(enemy.actor().rig().jointMatrices().at(shape.partsNo-1),enemy.placement()),offset);
 for(unsigned axis:{0u,2u,1u})for(double side:{1.,-1.}){
  re4::PlayerShot shot;shot.serial=serial;shot.weaponItemId=0x21;
  shot.from=shot.to={center[0],center[1],center[2]};shot.from[axis]-=side*800;shot.to[axis]+=side*800;
  const auto hit=enemy.target(shot.from,shot.to);if(hit && shapes[hit->partIndex].partsNo!=5)return shot;
 }throw std::runtime_error("original body death target unreachable");
}
}
int main(int argc,char **argv){try{using namespace re4;
 require(argc==5,"Usage: m37_enemy_sequence_audio_check R100.DAT ESL EM12.DRS R100.DAS");
 const auto room=RoomArchive::Load(argv[1]);const Sat sat(sub(room,"SAT")),eat(sub(room,"EAT"));
 const SatCollision floor(sat),walls(eat);const EnemySpawns esl(read(argv[2]));
 const FootstepSounds material(room);const auto banks=SoundBank::LoadDvd(argv[4]);
 const SoundBank *foot=nullptr;for(const auto &b:banks)if(b->block()==5)foot=b.get();require(foot,"original r100 material block5");
 unsigned total=0;RndInit(0x1119);
 for(unsigned index:{3u,4u,5u,0x12u,0x13u,0x26u,0x27u,0xffu}){
  const auto spawn=index==0xff?R100FirstGanadoSpawn(true):*esl.find(index);
  EnemyEncounter encounter(floor,walls,spawn,argv[3],true);EnemyPreview oracle(argv[3],spawn,true);
  oracle.setOriginalWalking(encounter.actor().motionSource(EnemyMotion::Walk).motionEntry==10,
      encounter.foundPlayer());
  oracle.start(EnemyMotion::Walk);unsigned events=0;
  for(unsigned tick=0;tick<300;++tick){
   auto target=encounter.feet();target[2]+=10000;encounter.tick(target);
   oracle.advance();const auto source=oracle.rig().consumeSequenceSound();
   std::vector<EnemySoundEvent> footsteps;for(const auto &e:encounter.drainSounds())if(e.block==5)footsteps.push_back(e);
   const bool expected=source && source->se<=4;
   require(footsteps.size()==unsigned(expected),"original SEQ event duplicated/omitted");
   if(expected){++events;++total;const auto &e=footsteps[0];const auto se=source->se-1;
    require(e.number==0x10+unsigned(se) && e.channel==EnemySoundChannel::Impact && !e.replace && !e.stop,"original em10FootSe cue/channel");
    const unsigned part=(se&1)?0x19:0x15;const auto &matrix=encounter.actor().rig().jointMatrices().at(part);
    const auto point=TransformPoint(encounter.placement(),{matrix[3],matrix[7],matrix[11]});
    for(unsigned axis=0;axis<3;++axis)require(e.position[axis]==point[axis],"original posed foot position");
    const auto selected=material.resolve(e.number,e.position);require(foot->cue(selected.number),"original material foot cue resolves in disc bank");
   }
   require(encounter.drainSounds().empty(),"foot event drained twice");require(RndState()==0x1119,"footstep changed caller game RNG");
  }require(events,"original actor walk has no footsteps");
  if(index==0xff) {
   EnemyEncounter spread(floor,walls,spawn,argv[3],true);auto shot=bodyShot(spread,81);
   shot.weaponItemId=0x2c;for(unsigned ray=0;ray<13;++ray)shot.pellets.push_back({shot.from,shot.to});
   spread.tick(spread.feet(),&shot);const auto scenery=spread.takeShotScenery(shot.serial);
   require(scenery.size()==13 && !scenery[0] && spread.hits()==1,"standalone shotgun source13 rays/first actor suppression");
   const auto wall=walls.line(shot.from,shot.to,SatSubset::All,0,0x00400000);
   for(unsigned ray=1;ray<13;++ray)require(bool(scenery[ray])==bool(wall) &&
       (!wall || (scenery[ray]->point==wall->point && scenery[ray]->normal==wall->normal)),
       "standalone pending dmg.isDamage skips actor only on subsequent source pellets");
   spread.tick(spread.feet());require(!spread.alive() && spread.hits()==1,"standalone source shotgun damage queued once");
  }
  EnemyEncounter death(floor,walls,spawn,argv[3],true);
  for(unsigned serial=1;serial<20 && death.alive();++serial){
   const auto shot=bodyShot(death,serial);death.tick(death.feet(),&shot);
   require(death.takeShotScenery(serial+1).empty(),"standalone unrelated serial consumed scenery");
   const auto scenery=death.takeShotScenery(serial);
   require(scenery.size()==1 && !scenery[0],"standalone actor hit emitted scenery after pending damage");
   require(death.takeShotScenery(serial).empty(),"standalone scenery consumed twice");
   death.tick(death.feet());
   if(death.alive())for(unsigned frame=0;frame<60;++frame)death.tick(death.feet());
  }
  require(!death.alive() && death.actor().motion()==EnemyMotion::Death,"ordinary source standing collapse fixture");
  require(!death.scenarioDeathReady() && !death.deadSettled(),"standing death drops on HP0 rather than source timer");
  require(!death.sourceKillCount(),"source EmSetDieCnt occurred before Die_Cramp");
  for(unsigned frame=1;frame<73;++frame){
   require(!death.scenarioDeathReady(),"original standing drop occurred before Timer49");death.tick(death.feet());
  }
  require(death.scenarioDeathReady() && !death.deadSettled(),"original standing Timer49 ACTIVE/ITEMSET transition missing");
  require(!death.sourceKillCount(),"original drop Timer49 incorrectly counts kill");
  for(unsigned frame=0;frame<120&&!death.deadSettled();++frame)death.tick(death.feet());
  require(death.deadSettled()&&death.scenarioDeathReady(),"collapse completion lost source drop readiness");
  require(!death.sourceKillCount(),"Die_Normal completion prematurely executed Die_Cramp");
  death.tick(death.feet());require(death.sourceKillCount()==1,"source Die_Cramp kill not counted once");
  for(unsigned frame=0;frame<120;++frame)death.tick(death.feet());
  require(death.sourceKillCount()==1,"corpse ticks duplicated source kill count");
 }
 std::cout<<"M37 original Ganado sequence footsteps/death: "<<checks<<" checks PASS; "<<total<<" source SEQ sounds from eight authored profiles, posed feet/material bank and Timer49 drop transitions.\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
