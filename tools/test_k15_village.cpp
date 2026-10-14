#include "archive.h"
#include "ports/yz2.h"
#include "village_flow.h"
#include "r101_scenario.h"
#include "event_playback.h"
#include "enemy_group.h"
#include "walk_probe.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace re4;
std::vector<std::uint8_t> read(const std::string &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
std::vector<std::uint8_t> entry(const RoomArchive &a,const char *tag){const auto *e=a.find(tag);assert(e);return a.copy(e-a.entries().data());}
int main(int argc,char **argv) {
 try {
  assert(argc==2);const std::string root=argv[1];unsigned rooms=0,doors=0,floors=0,boatArrivals=0;
  for(unsigned id=0x100;id<=0x120;++id)if(IsVillageRoom(id)) {
    char name[16];std::snprintf(name,sizeof name,"r%03x",id);
    const RoomArchive arc(LoadYz2(root+"/St1/"+name+".das").bytes);
    ++rooms;
    if(!arc.find("AEV")){std::cerr<<name<<" has no authored AEV transitions\n";continue;}
    const RoomFlow flow(id,entry(arc,"AEV"));
    for(const auto &d:flow.doors()) {
      assert(IsAuthoredVillageDoor(d));assert(RoomFlow::unavailableReason(d).empty());++doors;
      // r10e's part swaps retain native player placement; AEV's zero vector
      // is recorded verbatim, never used as a fabricated walkable origin.
      if(d.sourceRoom==d.destinationRoom && d.destinationPart)continue;
      char destination[16];std::snprintf(destination,sizeof destination,"r%03x",d.destinationRoom);
      const RoomArchive target(LoadYz2(root+(d.destinationRoom>>8==2?"/St2/":"/St1/")+destination+".das").bytes);
      if(!target.find("SAT")) {std::cerr<<name<<" area "<<unsigned(d.no)<<" -> "<<destination<<" has no static SAT\n";continue;}
      const Sat sat(entry(target,"SAT"));const SatCollision world(sat);WalkProbe player(world);
      if(!player.placeAuthored(d.destination)) {assert(d.sourceRoom==0x11b && d.no==2 && d.destinationRoom==0x10d);++boatArrivals;}
      else ++floors;
    }
  }
  assert(rooms==29 && doors==66 && floors==63 && boatArrivals==1);std::cout<<"29 rooms, 66 authored transitions verified\n";
  const RoomArchive room(LoadYz2(root+"/St1/r101.das").bytes);
  const auto aev=entry(room,"AEV");ScenarioState state;R101Scenario scenario(aev);
  const EnemySpawns esl(read(root+"/etc/emleon00.esl"));const auto initial=scenario.initialSpawns(esl,state);assert(initial.size()==11);
  const Sat sat(entry(room,"SAT")),eat(entry(room,"EAT"));const SatCollision world(sat),effects(eat);
  EnemyGroup group(world,effects,root+"/em/em15.drs");
  for(auto index:R101Scenario::actorIndices()) {const auto *spawn=esl.find(index);assert(spawn);std::cout<<"Actor "<<index<<std::endl;group.spawn(*spawn);}
  assert(group.actors().size()==43);
  assert(VillageEnemyList(0x10b,false,0)==0 && VillageEnemyList(0x111,false,0)==1);
  assert(VillageEnemyList(0x10e,false,1)==1 && VillageEnemyList(0x10e,true,0)==1);
  RoomFlow originalDoors(0x101,aev);auto lockState=state;
  const auto &church=*originalDoors.find(0x19);assert(!VillageDoorReason(church,lockState,0,120).empty());
  assert(!UnlockVillageDoor(church,lockState));lockState.carriedItems[0x3b]=1;assert(UnlockVillageDoor(church,lockState));
  assert(VillageDoorReason(church,lockState,0,120).empty());
  const CollisionVec safe{-52144,91,22524};
  for(bool house:{false,true}) {
    ScenarioState timerState;R101Scenario timer(aev);timer.resume(timerState,11);
    timer.tick(timerState,safe,2.12,11,true);timer.takeCues();
    if(house) {timer.begin(VillageEvent::House,timerState,14);timer.finish(VillageEvent::House,timerState);timer.takeCues();}
    const unsigned alive=house?11:14,deadline=house?13500:11700;
    for(unsigned n=0;n<10;++n)timer.tick(timerState,safe,2.12,alive,false);
    const auto clock=timer.battleTicks();for(unsigned n=0;n<100;++n)timer.tick(timerState,safe,2.12,alive,false,true);
    assert(timer.battleTicks()==clock);
    while(timer.battleTicks()<=deadline)timer.tick(timerState,safe,2.12,alive,false);
    assert(!timerState.room(0x101).room.test(7));timer.tick(timerState,safe,2.12,alive,false);assert(timerState.room(0x101).room.test(7));
  }
  scenario.resume(state,11);scenario.tick(state,safe,2.12,11,true);assert(state.room(0x101).room.test(6));
  unsigned alive=14;scenario.takeCues();for(unsigned i=0;i<10;++i)scenario.tick(state,safe,2.12,alive,false);
  alive-=4;scenario.tick(state,safe,2.12,alive,false);assert(!state.room(0x101).room.test(3));
  --alive;scenario.tick(state,safe,2.12,alive,false);assert(state.room(0x101).room.test(3));
  for(unsigned i=0;i<90;++i)scenario.tick(state,safe,2.12,alive,false);
  const auto wave=scenario.takeCues();assert(wave.size()==2 && wave[0].indices.size()==5);alive+=5;scenario.finishWaveCamera();
  alive-=5;scenario.tick(state,safe,2.12,alive,false);assert(state.room(0x101).room.test(2));alive+=5;scenario.takeCues();
  alive-=4;scenario.tick(state,safe,2.12,alive,false);assert(!state.room(0x101).room.test(7));
  --alive;scenario.tick(state,safe,2.12,alive,false);assert(state.room(0x101).room.test(7));
  assert(state.transient.test(3));assert(!state.scenario.test(36));
  for(unsigned i=0;i<60;++i)scenario.tick(state,safe,2.12,alive,false);
  assert(scenario.offer(state,safe,2.12,false,false)==VillageEvent::Bell);
  scenario.begin(VillageEvent::Bell,state,alive);scenario.finish(VillageEvent::Bell,state);assert(!state.transient.test(3));
  for(const auto *name:{"r101s00","r101s21","r101s30"}) {
    std::cout<<"Event "<<name<<std::endl;OriginalEvent event(root,name,1);
    for(unsigned i=0;i<event.totalFrames()+2 && !event.finished();++i)event.tick30Hz();
    assert(event.finished());assert(event.unsupported().empty());std::cout<<name<<" complete, "<<event.sourceFrame()<<" frames\n";
  }
  std::cout<<"Village source-data, actors, waves and full events passed\n";
 } catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
