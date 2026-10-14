#include "archive.h"
#include "enemy_encounter.h"
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
  if(argc!=4)throw std::runtime_error("Usage: m37_r100_house_scare_check R100 ESL EM12");
  const auto room=RoomArchive::Load(argv[1]);const EnemySpawns profiles(read(argv[2]));
  const Sat sat(sub(room,"SAT")),eat(sub(room,"EAT"));const SatCollision floor(sat),effects(eat);
  const auto *source=profiles.find(0x25);require(source && source->hp==1000 && source->setNo==0x1b && source->flags==0x24,"Exact original transient house ESL profile");
  EnemyEncounter enemy(floor,effects,*source,argv[3],true);
  require(!enemy.r100HouseScare(),"Diagnostic source spawn retains existing behavior until explicit scenario request");
  enemy.beginR100HouseScare();require(enemy.r100HouseScare() && enemy.visible() && !enemy.scriptSuspended(),"Original temporary actor runs during noSuspend event");
  require(enemy.alive() && enemy.hp()==1000 && !enemy.foundPlayer(),"Original scripted turn/walk remains healthy without entering target AI");
  require(enemy.actor().motion()==EnemyMotion::Idle,"Source phase0 falls through one idle tick on creation");
  bool duplicate=false;try{enemy.beginR100HouseScare();}catch(const std::exception&){duplicate=true;}require(duplicate,"Source transient routine cannot restart on existing actor");
  const auto player=EnemyEncounter::TestPlayerPosition();
  for(unsigned t=0;t<89;++t) {enemy.tick(player);require(enemy.actor().motion()==EnemyMotion::Idle && enemy.hp()==1000,"Source 90-tick idle includes first creation tick");}
  enemy.tick(player);require(enemy.actor().motion()==EnemyMotion::HouseTurn,"Source timer0 immediately falls through turn start+MotionMove");
  const auto turn=enemy.actor().motionSource(EnemyMotion::HouseTurn);
  require(turn.motionEntry==20 && turn.sequenceEntry==21 && turn.blendFrames==10 && turn.flags==1,"Original type0 turn ARC18/19 no mirror, blend10");
  unsigned turning=1;while(enemy.actor().motion()==EnemyMotion::HouseTurn && turning<200) {enemy.tick(player);++turning;}
  require(enemy.actor().motion()==EnemyMotion::HouseWalk && turning>10 && turning<200,"Original nonzero motion state advances turn to walk on next source tick");
  const auto walk=enemy.actor().motionSource(EnemyMotion::HouseWalk);
  require(((walk.motionEntry==4 && walk.sequenceEntry==7)||(walk.motionEntry==10 && walk.sequenceEntry==13)) && walk.blendFrames==7 && walk.flags==5,"Original random walk family and source emset37 sequence2");
  const auto before=enemy.feet();for(unsigned t=0;t<60;++t)enemy.tick(player);
  require(enemy.actor().motion()==EnemyMotion::HouseWalk && enemy.feet()!=before,"Source temporary actor walks with authored root until event destroys it");
  require(!enemy.target({-80000,2000,-38000},{-82000,2000,-38000}),"House event transient actor does not become combat target");
  require(!enemy.deadSettled() && !enemy.scenarioDeathReady() && !enemy.lastDamage(),"Temporary actor does not trigger death/drop persistence");
  enemy.setScriptSuspended(true);const auto paused=enemy.actor().rig().jointMatrices();enemy.tick(player);
  require(enemy.actor().rig().jointMatrices()==paused,"Paused source house event preserves authored actor pose");
  require(enemy.drainSounds().empty(),"Scripted handler does not add unrelated generic combat/breath sounds");
  std::cout<<"M37 actual source R100TurnWalk scare PASS: "<<checks<<" checks; turnticks="<<turning<<'\n';return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
