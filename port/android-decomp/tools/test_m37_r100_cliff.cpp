#include "archive.h"
#include "enemy_group.h"
#include "object_actions.h"
#include "r100_scenario.h"
#include "weapon_preview.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks;
void require(bool v,const char *s) {++checks;if(!v)throw std::runtime_error(s);}
std::vector<std::uint8_t> read(const char *path) {
 std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("ESL missing");
 std::vector<std::uint8_t> b(std::size_t(f.tellg()));f.seekg(0);
 if(!f.read(reinterpret_cast<char*>(b.data()),b.size()))throw std::runtime_error("ESL truncated");
 return b;
}
std::vector<std::uint8_t> sub(const RoomArchive &a,const char *tag) {
 const auto *e=a.find(tag);if(!e)throw std::runtime_error("Room source entry missing");return a.copy(e-a.entries().data());
}
}
int main(int argc,char **argv) {
 try {
  if(argc!=6)throw std::runtime_error("Usage: m37_r100_cliff_check R100 ESL EM12 PL00 WEP01");
  const auto room=RoomArchive::Load(argv[1]);const EnemySpawns profiles(read(argv[2]));
  const Sat sat(sub(room,"SAT")),eat(sub(room,"EAT"));const SatCollision floor(sat),effects(eat);
  EnemyGroup group(floor,effects,argv[3]);
  for(unsigned index=6;index<=8;++index) {
   const auto *source=profiles.find(index);require(source,"Original cliff ESL identity");
   auto &enemy=group.spawn(*source);enemy.installR100CliffMotion(room.copy(index+36));
   require(enemy.r100CliffEncounter() && enemy.hp()==0 && enemy.maxHp()==1000,"Original phase0 HP0 is scripted, not a generic kill");
   require(!enemy.cliffFalling() && !enemy.deadSettled() && !enemy.scenarioDeathReady(),"Initial HP0 does not prematurely persist source EmSetDie");
   require(enemy.feet()==source->position,"Cliff source placement bypasses collision floor snapping");
   require(!enemy.visible() && enemy.scriptSuspended() && enemy.actor().opacity()==1,"Prepared source cliffs await room release");
   require(enemy.actor().rig().clipCount()==31 && enemy.actor().roomEventAction()==30,"Room motion appends without replacing existing EM12 actions");
   require(!enemy.actor().holdsWeapon() && !enemy.target({0,0,0},{100000,100000,100000}),"Scripted HP0 cliffs never act as combat targets");
  }
  LeonPreview player(argv[4],argv[5],argv[3],true);WalkProbe probe(floor);
  require(probe.place(R100FirstPlayerPosition()),"Source player placement");player.setPlacement(probe.position(),R100FirstPlayerHeading);
  const auto pose=group.find(6)->actor().rig().jointMatrices();
  for(unsigned t=0;t<5;++t)group.tickScriptActors(player,probe);
  require(group.find(6)->actor().rig().jointMatrices()==pose,"Suspension leaves phase0 pose untouched");
  group.releaseR100CliffActors();
  for(unsigned index=6;index<=8;++index) {
   auto &enemy=*group.find(index);require(enemy.visible() && !enemy.scriptSuspended(),"Only explicit source room release enables cliff actors");
   auto far=enemy.feet();far[0]+=28000;enemy.tick(far);
   require(!enemy.cliffFalling() && enemy.actor().motion()==EnemyMotion::RoomEvent,"Original proximity threshold is strictly below 28000");
   auto close=enemy.feet();close[0]+=27999;enemy.tick(close);
   require(enemy.cliffFalling() && !enemy.deadSettled(),"Source proximity starts cliff phase2 before death-inactive");
   const auto sounds=enemy.drainSounds();
   require(sounds.size()==(index==6?1u:0u),"Only source set2 cliff leader plays SndCall6,4");
   if(index==6)require(sounds[0].block==6 && sounds[0].number==4,"Exact original cliff sound cue");
   const auto start=enemy.feet();const auto frames=unsigned(enemy.actor().rig().maxFrame(30));
   for(unsigned t=0;t<frames-10;++t) {enemy.tick(close);require(enemy.actor().opacity()==1 && enemy.visible(),"Original Timer=maxFrame-10 precedes fade");}
   require(enemy.feet()!=start,"Authored cliff FCV root displacement continues beyond SAT floor");
   float opacity=1;
   for(unsigned t=0;t<10;++t) {
    enemy.tick(close);opacity=std::max(0.f,opacity-.1f);
    require(enemy.actor().opacity()==opacity,"Source float subtraction .1 each 30Hz fade tick");
    require(enemy.visible()==(t<9),"Source ACTIVE/BE2 clears exactly when invisible factor reaches zero");
   }
   require(enemy.deadSettled() && enemy.scenarioDeathReady() && !enemy.visible(),"Original cliff death-inactive only after authored motion and fade");
   const auto stopped=enemy.feet();enemy.tick(close);require(enemy.feet()==stopped,"Inactive original cliff actor stops evaluating");
  }
  // Actual object runtime plus no Ganado intersection reproduced floatmax
  // flowing into its bounded target API and resetting the old debug engine.
  ObjectActions objects(0x100,room,ReadRoomObjectPlacements(sub(room,"ETS")),&effects);
  EnemyGroup misses(floor,effects,argv[3]);misses.setObjectActions(&objects);
  PlayerShot miss;miss.serial=1;miss.weaponItemId=0x23;
  miss.from={-80000,20000,-38000};miss.to={-70000,20000,-38000};misses.queueShot(miss);
  require(objects.drainEvents().empty(),"Actual object-backed gun miss uses finite squared ray extent without crash");
  require(misses.accuracyHits()==0,"Source accuracy does not count a gun miss");
  unsigned externalCalls=0;
  for(unsigned item:{0x23u,0x21u,0x2cu}) {
   EnemyGroup targets(floor,effects,argv[3]);auto a=*profiles.find(3),b=*profiles.find(0x13);
   a.position={-79116,860,-38890};b.position={-79116,860,-37890};a.heading=b.heading=0;
   auto &front=targets.spawn(a);auto &back=targets.spawn(b);
   const auto shape=NormalGanadoHitShapes().at(1);auto offset=shape.offset;offset[1]+=shape.height*.5f;
   const auto center=TransformPoint(WeaponAttachmentMatrix(front.actor().rig().jointMatrices().at(1),front.placement()),offset);
   PlayerShot shot;shot.serial=1;shot.weaponItemId=item;
   shot.from={center[0],center[1],center[2]-800};shot.to={center[0],center[1],center[2]+1800};
   targets.setExternalTarget([&](const CollisionVec&,const CollisionVec&,unsigned){++externalCalls;return std::optional<AuthoredHitResult>({0,10000,{}});});
   require(!targets.target(shot.from,shot.to,item==0x2c?7:item==0x23?2:1),"Nearest external crow/dog/trap competes with Ganado reticle target");
   if(item==0x2c)for(unsigned t=0;t<13;++t)shot.pellets.push_back({shot.from,shot.to});
   targets.queueShot(shot);targets.stepPlayer(player,probe,0,0,false,false,1./30,{});
   require(targets.accuracyHits()==1,"Source primary hit counts once, even with13 shotgun rays");
   require(front.hits()==(item==0x21?1u:0u) && back.hits()==0,"External consumes one target slot; only source Punisher may reach second actor");
  }
  require(externalCalls==18,"External callback receives primary and all secondary shotgun rays");
  EnemyGroup secondaryOnly(floor,effects,argv[3]);
  secondaryOnly.setExternalTarget([](const CollisionVec&from,const CollisionVec&,unsigned){
    return from[0]==20?std::optional<AuthoredHitResult>({0,10000,{}}):std::nullopt;
  });
  PlayerShot secondary;secondary.serial=1;secondary.weaponItemId=0x2c;
  secondary.pellets.push_back({CollisionVec{10,20000,0},CollisionVec{10,20000,200}});
  secondary.pellets.push_back({CollisionVec{20,20000,0},CollisionVec{20,20000,200}});
  secondaryOnly.queueShot(secondary);
  require(secondaryOnly.accuracyHits()==0,"Source secondary-only shotgun hit does not alter accuracy");
  std::cout<<"M37 source r100 cliff and gun-miss/external dispatch PASS: "<<checks<<" checks\n";return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
