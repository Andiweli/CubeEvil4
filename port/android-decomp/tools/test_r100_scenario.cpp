#include "r100_scenario.h"
#include "enemy_encounter.h"
#include "enemy_feedback.h"
#include "archive.h"
#include "weapon_preview.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks;
void require(bool value,const char *message) {++checks;if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const char *path) {
  std::ifstream file(path,std::ios::binary|std::ios::ate);
  if(!file)throw std::runtime_error("Original file missing");
  std::vector<std::uint8_t> b(std::size_t(file.tellg()));file.seekg(0);
  if(!file.read(reinterpret_cast<char*>(b.data()),b.size()))throw std::runtime_error("Original file truncated");
  return b;
}
std::vector<std::uint8_t> sub(const RoomArchive &arc,const char *tag) {
  const auto *entry=arc.find(tag);require(entry,"Original r100 archive entry");
  return arc.copy(entry-arc.entries().data());
}
std::pair<CollisionVec,CollisionVec> bodyRay(const EnemyEncounter &enemy) {
  const auto shapes=NormalGanadoHitShapes();const auto &body=shapes.at(1);
  auto offset=body.offset;offset[1]+=body.height*.5f;
  const auto center=TransformPoint(WeaponAttachmentMatrix(
      enemy.actor().rig().jointMatrices().at(unsigned(body.partsNo-1)),
      enemy.placement()),offset);
  for(unsigned axis:{0u,2u,1u})for(double side:{1.,-1.}) {
    CollisionVec from{center[0],center[1],center[2]},to=from;
    from[axis]-=side*800;to[axis]+=side*800;
    const auto hit=HitAuthoredShapes(shapes,enemy.actor().rig().jointMatrices(),
                                    enemy.placement(),from,to,2560000,0x10);
    if(hit && shapes.at(hit->partIndex).partsNo!=5)return {from,to};
  }
  throw std::runtime_error("No posed non-head knife ray");
}
}
int main(int argc,char **argv) {
 try {
  if(argc!=6)throw std::runtime_error("Usage: test_r100_scenario R100.dat ESL EM12 PL00 WEP01");
  const auto arc=RoomArchive::Load(argv[1]);const EnemySpawns spawns(read(argv[2]));
  const auto aev=sub(arc,"AEV");const R100Scenario trigger(aev);
  require(trigger.firstLookArea().no==0xa && trigger.firstLookArea().triggerType==1,"Authored automatic SCE area0a");
  require(trigger.firstLookHit({-83000,860,-35000},0),"Inside original first event polygon");
  require(!trigger.firstLookHit({-99685,-484,-1343},2.246),"Original room start cannot trigger first fight");
  require(!trigger.firstLookHit({-83000,3000,-35000},0),"Original trigger height clipping");
  auto bad=aev;bad.at(16+trigger.firstLookArea().recordIndex*156+0x35)=1;
  bool rejected=false;try {R100Scenario invalid(bad);}catch(const std::exception&){rejected=true;}
  require(rejected,"Door record cannot become campaign cutscene trigger");
  ScenarioState opening;
  BeginOriginalOpening(opening,false);
  require(!OriginalOpeningCanceled(opening),"Newgame begins the two original r120 engine scenes");
  CancelOriginalOpening(opening);
  require(OriginalOpeningCanceled(opening) && opening.scenario.words()[1]==0x10,
          "Original event cancel is SCF59 / Scenario_flg[1] mask0x10");
  BeginOriginalOpening(opening,false);
  require(!OriginalOpeningCanceled(opening),"A new opening resets previous cancellation");
  BeginOriginalOpening(opening,true);
  require(OriginalOpeningCanceled(opening),"FMV cancellation skips all remaining original entry scenes");
  require(R120RoadJumpPosition()==CollisionVec{-109450,-515,820},"r120 jumps to the authored road point");
  require(R100EntryPlayerPosition()==CollisionVec{-99685,-484,-1343} && R100EntryPlayerHeading==2.246f,
          "r100s40 ends at the original playable gate placement");
  auto &entry=opening.room(0x100);
  require(BeginR100Entry(entry) && entry.room.test(13) && !BeginR100Entry(entry),"Original RSF13 schedules entry only once");
  require(!entry.room.test(3) && !entry.room.test(10),"Entry cannot trigger first fight/truck flags");
  ScenarioState state;auto &save=state.room(0x100);
  require(R100Profile(save).firstPresent && !R100Profile(save).firstActive,"Initial scripted enemy inactive");
  require(!CompleteR100FirstEncounter(state),"Truck sequence cannot complete beforefirst fight");
  require(R100AmbushSpawns(spawns,save).empty(),"Inactive ESL entries cannot spawn before sourceRSF10");
  require(BeginR100FirstEncounter(save) && !BeginR100FirstEncounter(save),"First look setsRSF3 exactlyonce");
  require(R100Profile(save).firstActive,"Native first-battle state afterlook");
  const auto event=R100FirstGanadoSpawn(false),spawn=R100FirstGanadoSpawn();
  require(event.listIndex==0xff && event.setNo==0x13 && event.hp==500 && event.id==0x12 &&
    event.flags==0x21000020 && event.guardRadius==10000,"R100Init realevent record, notESL0/37");
  require(event.position==CollisionVec{-77630,860,-36500},"Original inactive eventplacement");
  require(spawn.position==CollisionVec{-79116,860,-38890} && float(spawn.heading)==-1.39f &&
    spawn.flags==0x21000021,"Exact post-s03 Ganado placement andactivation");
  require(R100FirstPlayerPosition()==CollisionVec{-82910,860,-38480} &&
    R100FirstPlayerHeading==1.75f,"Exact post-s03 Leon placement");
  require(CompleteR100FirstEncounter(state) && !CompleteR100FirstEncounter(state),"Truck completion sourceflags once");
  require(save.room.test(10) && state.scenario.test(17),"RSF10 andSCF17 areMSBfirstsourcebits");
  const auto ambush=R100AmbushSpawns(spawns,save);
  require(ambush.size()==7,"Exactlyseven authoredambush records aftertruck");
  const unsigned indices[]{3,4,5,0x12,0x13,0x26,0x27};
  for(unsigned i=0;i<7;++i) {
    require(ambush[i]->listIndex==indices[i] && !ambush[i]->alive() && ambush[i]->id==0x12,"EmSetFromList2 ignoresinactivebit0 onlywhen explicit");
  }
  require(R100AmbushSpawns(spawns,save,{3,4}).size()==5,"Sourcedeathflags prevent respawn");
  require(GanadoAxeDamageType({0,0,0},0,{0,1300,-200},{0,1300,-1000})==1,"Highback damage type");
  require(GanadoAxeDamageType({0,0,0},0,{0,1299,-200},{0,1299,-1000})==3,"Source strict1300 middlethreshold");
  require(GanadoAxeDamageType({0,0,0},0,{0,799,-200},{0,799,-1000})==5,"Source strict800 lowthreshold");
  require(GanadoAxeDamageType({0,0,0},0,{0,800,-200},{0,800,-1000})==3,"Boundary800 remains middle");
  require(GanadoAxeDamageType({0,0,0},0,{0,1300,200},{0,1300,1000})==0,"Highfront damage type");
  const Sat sat(sub(arc,"SAT")),eat(sub(arc,"EAT"));const SatCollision floor(sat),effects(eat);
  const EnemyEncounter inactive(floor,effects,event,argv[3],true);
  require(inactive.firstHouseEncounter() && inactive.actor().holdsAxe() &&
    std::hypot(inactive.feet()[0]-event.position[0],inactive.feet()[2]-event.position[2])<.01,
    "Originalinactiveeventactor renders at exactSAT-free initialplacement");
  EnemyEncounter enemy(floor,effects,spawn,argv[3],true);
  require(enemy.firstHouseEncounter() && enemy.hp()==500 && enemy.actor().holdsAxe(),"Actual sourcefirst actor accepted");
  require(std::hypot(enemy.feet()[0]-spawn.position[0],enemy.feet()[2]-spawn.position[2])<.01,"Authored spawnXZpreserved onSAT");
  require(enemy.actor().visiblePart(452) && !enemy.actor().visiblePart(449) && enemy.actor().visiblePart(616),"Original leftweaponhand/model visibility");
  require(enemy.actor().renderMesh().fallbackTriangles==0 && enemy.actor().renderMesh().textures.size()==4,"Actualweapon/bodyTPLfully resolved");
  unsigned weaponCorners=0;
  for(const auto &draw:enemy.actor().renderMesh().draws)if(draw.work==616) {
    weaponCorners+=draw.count;
    for(unsigned i=draw.first;i<draw.first+draw.count;++i) {
      const auto &skin=enemy.actor().renderSkin().at(i);
      require(skin.position.count==1 && skin.position.joints[0]==16 && skin.position.rates[0]==1,"Weapon rigidlyshares authored lefthandpart16");
    }
  }
  require(weaponCorners>0,"Originalaxehasrealgeometry");
  LeonPreview player(argv[4],argv[5],argv[3],true);WalkProbe probe(floor);
  require(probe.place(R100FirstPlayerPosition()),"ExactLeon post-s03 placement isSATfree");
  player.setPlacement(probe.position(),R100FirstPlayerHeading);
  for(unsigned tick=0;tick<600 && player.life()==LeonPreview::MaxLife;++tick)
    enemy.stepPlayer(player,probe,0,0,false,false,1./30,{});
  require(enemy.axeAttacks()>0 && enemy.axeHits()>0,"OriginalFCV/SEQ swing damagesposedLeon inrealr100collision");
  require(player.life()<LeonPreview::MaxLife && !player.grabbed(),"Mortalaxereaction replaces unarmedtestgrab");
  const auto hitLife=player.life();
  const auto attacks=enemy.axeAttacks();const auto frame=enemy.actor().rig().status().sequenceFrame;
  enemy.stepPlayer(player,probe,0,0,false,false,0,{});
  require(enemy.axeAttacks()==attacks && enemy.actor().rig().status().sequenceFrame==frame,"Renderonly tick cannot advanceoriginalAI/SEQ");
  // Feed authored collision hits through the real queue, then check the
  // original early death ACTIVE-clear gate independently of clip end.
  player.setUnlimitedHealth(true);
  unsigned accepted=0,deathTicks=0;
  for(unsigned attempt=0;attempt<20 && enemy.alive();++attempt) {
    PlayerMeleeEvent knife;knife.serial=100+attempt;knife.rays.push_back(bodyRay(enemy));
    const auto oldHits=enemy.knifeHits();enemy.queueMelee(knife,player.feet());
    accepted+=enemy.knifeHits()!=oldHits;
    enemy.stepPlayer(player,probe,0,0,false,false,1./30,{});
    if(!enemy.alive()) {deathTicks=1;break;}
    for(unsigned tick=0;tick<20;++tick)enemy.stepPlayer(player,probe,0,0,false,false,1./30,{});
  }
  require(accepted==5 && !enemy.alive() && enemy.hp()==0,"Five original100-power knife body hits finish500HP encounter");
  require(enemy.actor().motion()==EnemyMotion::Death && !enemy.actor().holdsAxe(),"Originalstanding death detachesaxe onentry");
  require(!enemy.scenarioDeathReady() && !enemy.deadSettled(),"Fatalhit cannot prematurely complete truck event");
  while(!enemy.scenarioDeathReady() && deathTicks<150) {
    enemy.stepPlayer(player,probe,0,0,false,false,1./30,{});++deathTicks;
    if(deathTicks<73)require(!enemy.scenarioDeathReady(),"Die_Normal keepsACTIVE before original0x49timer");
  }
  require(deathTicks==73 && !enemy.deadSettled(),"SourceACTIVE clears on73rd death tick before103-frame clip ends");
  const auto deathFrame=enemy.actor().rig().status().sequenceFrame;
  enemy.stepPlayer(player,probe,0,0,false,false,0,{});
  require(enemy.scenarioDeathReady() && enemy.actor().rig().status().sequenceFrame==deathFrame,"Pauseddeath state preservesoriginalevent readiness");
  std::cout<<"M35 actual r100 scenario/axe combat PASS: "<<checks<<" checks; attacks="<<enemy.axeAttacks()<<" hits="<<enemy.axeHits()<<" LeonHP="<<hitLife<<" deathACTIVEtick="<<deathTicks<<'\n';
  return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
