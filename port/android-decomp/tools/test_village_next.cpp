#include "archive.h"
#include "enemy_spawn.h"
#include "enemy_group.h"
#include "village_animals.h"
#include "dog_traps.h"
#include "room_interactions.h"
#include "room_flow.h"
#include "room_area_effects.h"
#include "ports/yz2.h"
#include <fstream>
#include <iostream>
#include <cassert>
#include <cmath>
using namespace re4;
std::vector<uint8_t> read(const std::string&p){std::ifstream f(p,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
std::vector<uint8_t> entry(const RoomArchive&a,const char*t){auto e=a.find(t);assert(e);return a.copy(e-a.entries().data());}
int main(int argc,char**argv){try{assert(argc==2);std::string root=argv[1];EnemySpawns day(read(root+"/etc/emleon00.esl")),night(read(root+"/etc/emleon01.esl"));
 for(unsigned id:{0x103,0x106,0x111,0x113,0x11a}){
 char name[12];snprintf(name,sizeof name,"r%03x",id);RoomArchive arc(LoadYz2(root+"/St1/"+name+".das").bytes);
 Sat sat(entry(arc,"SAT")),eat(entry(arc,"EAT"));SatCollision world(sat),effect(eat,SatQueryMode::Effect);RoomFlow flow(id,entry(arc,"AEV"));
 const auto core=RoomArchive::LoadDrs(root+"/etc/core.das");auto areaData=BuildRoomAreaEffects(arc,id,entry(core,"EFF"));WorldEffectCamera camera;camera.eye={-25000,3000,-9000};RoomAreaEffects areas(areaData,0xffffffffu,&camera);for(unsigned n=0;n<100;++n)areas.tick(camera.eye,camera,0xffffffffu);std::cout<<name<<" SST "<<areaData.banks.size()<<" banks warnings "<<areaData.warnings.size()<<std::endl;const auto &esl=(id==0x103 || id==0x106)?day:night;unsigned em=(id==0x111||id==0x113)?0x17:0x12;std::string path=root+"/em/"+(em==0x17?"em17.drs":"em12.drs");EnemyGroup group(world,effect,path);
 unsigned count=0;for(const auto&s:esl.records())if(s.room==id && s.id==em && s.alive() && !s.alreadySet()){std::cout<<name<<" enemy "<<s.listIndex<<" type "<<unsigned(s.type)<<std::endl;auto&a=group.spawn(s);assert(std::abs(a.feet()[0]-s.position[0])<.01 && std::abs(a.feet()[2]-s.position[2])<.01);for(auto v:a.feet())assert(std::isfinite(v));++count;}
 assert(count==(id==0x103?5:id==0x106?7:id==0x11a?11:8));
 if(id==0x103){VillageAnimals animals(world,esl,root+"/em",id);assert(animals.animals().size()==12);for(unsigned n=0;n<300;++n)animals.tick({-25000,0,-9000},false,false,false);}
 if(id==0x106 || id==0x111 || id==0x113){ScenarioState state;std::set<unsigned> consumed;DogTraps traps(world,esl,root+"/em/em21.drs",root+"/em/em2a.drs",state,&consumed,id);assert(traps.dogState()==DogTraps::DogState::Gone);assert(traps.actors().size()==(id==0x111?5:id==0x106?9:8));traps.releaseAfterTruck();assert(traps.released());for(const auto&a:traps.actors())assert(a->visible());
 auto objects=BuildRoomObjects(arc,{},id);RoomInteractions interior(id,arc,objects.placements,root+"/em/pl00.drs",{},&effect);
 if(id==0x111){unsigned broken=0;for(const auto&w:interior.windows())if(w.placement.slot==0 || w.placement.slot==0x13){assert(w.hp==0);++broken;}assert(broken==2);assert(interior.racks().size()==3);}}
 std::cout<<name<<" verified "<<count<<" source Ganados, "<<flow.typewriters().size()<<" typewriters\n";
 }
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
