#include "village_props.h"
#include "save_native.h"
#include "ports/yz2.h"
#include <algorithm>
#include <cassert>
#include <iostream>
using namespace re4;
namespace {
std::vector<std::uint8_t> entry(const RoomArchive &a,const char *tag){const auto *e=a.find(tag);assert(e);return a.copy(e-a.entries().data());}
struct Fixture {
 unsigned id;RoomArchive archive;Sat sat,eat;Smd smd;SatCollision sight,effect;ScenarioCollision collision;
 RoomPickups pickups;VillageProps props;
 Fixture(unsigned room,const std::string &root,ScenarioState &state)
  :id(room),archive(LoadYz2(root+"/St1/r"+([](unsigned n){char b[8];snprintf(b,sizeof b,"%03x",n);return std::string(b);})(room)+".das").bytes),
   sat(entry(archive,"SAT")),eat(entry(archive,"EAT")),smd(entry(archive,"SMD")),sight(sat),effect(eat,SatQueryMode::Effect),
   collision(room,entry(archive,"AEV")),pickups(room,archive,state,&effect),props(room,archive,smd,state,pickups,collision,sight,effect) {
   auto core=RoomArchive::LoadDrs(root+"/etc/core.das");props.initializeEffects(archive,entry(core,"EFF"));
 }
 void step(){pickups.tick();props.tick30Hz();props.tickEffects({});}
 void land(){unsigned count=0;while(pickups.items().at(props.pendant()).controlFlags&16){step();assert(++count<200);}step();}
 VillageProps::Hit coverHit() {
  auto pose=*std::find_if(smd.placements.begin(),smd.placements.end(),[](auto &p){return p.work==0x52;});pose.rotation[0]=-.5235988f;
  const auto a=TransformPoint(PlacementMatrix(pose),{0,600,1000}),b=TransformPoint(PlacementMatrix(pose),{0,600,-1000});
  const auto hit=props.target({a[0],a[1],a[2]},{b[0],b[1],b[2]},1);assert(hit && hit->kind==VillageProps::Hit::Cover);return *hit;
 }
 void shootPendant() {
  const auto &item=pickups.items().at(props.pendant());auto a=item.position,b=a;a[2]+=1000;b[2]-=1000;
  assert(!pickups.shootDownTarget(a,b,16));const auto hit=pickups.shootDownTarget(a,b,1);
  assert(hit && hit->index==props.pendant());assert(pickups.shootDown(*hit,100,1));assert(!pickups.shootDown(*hit,100,1));
  assert(!pickups.take(props.pendant()));
 }
};
ScenarioState saved(ScenarioState state,unsigned room) {
 InventoryCase bag(0);assert(bag.add(0x7c,1));NativeSaveSnapshot save;save.inventory=bag.items();save.scenario=state;save.room=room;
 return DecodeNativeSave(EncodeNativeSave(save)).scenario;
}
}
int main(int argc,char **argv){try {
 assert(argc==2);const std::string root=argv[1];
 for(unsigned id:{0x103,0x113}) {
  // Both original action orders, with native EAT collision, not a fabricated landing height.
  for(bool closeFirst:{true,false}) {
   ScenarioState state;Fixture f(id,root,state);assert(f.props.medals().size()==7);
   assert(f.pickups.visible(int(f.props.pendant())) && !f.pickups.take(f.props.pendant()));
   for(std::size_t n=0;n<f.pickups.items().size();++n)if(f.pickups.items()[n].id==0x89)assert(!f.pickups.visible(int(n)));
   const auto hit=f.coverHit();
   if(closeFirst){assert(f.props.applyHit(hit,101,1));assert(!f.props.applyHit(hit,101,1));for(unsigned n=0;n<30;++n)f.step();}
   f.shootPendant();f.land();
   assert(f.pickups.found(f.props.pendant()) && state.globalItemsFound.test(6));
   assert(state.scenario.test(27)==!closeFirst);
   const auto &item=f.pickups.items()[f.props.pendant()];assert(item.id==(closeFirst?0x58u:0x89u));
   const auto layer=BuildRoomItems(f.archive,f.pickups.renderPlacements(),true);
   assert(std::any_of(layer.mesh.draws.begin(),layer.mesh.draws.end(),[&](auto &d){return d.roomItemIndex==int(f.props.pendant()) && d.roomItemId==0x58;}));
   assert(std::any_of(layer.mesh.draws.begin(),layer.mesh.draws.end(),[&](auto &d){return d.roomItemIndex==int(f.props.pendant()) && d.roomItemId==0x89;}));
   assert(f.pickups.visibleModel(int(f.props.pendant()),int(item.id)));
   assert(!f.pickups.visibleModel(int(f.props.pendant()),closeFirst?0x89:0x58));
   if(closeFirst) {
    assert(!f.props.openLid());const auto pickup=f.pickups.take(f.props.pendant());assert(pickup && pickup->id==0x58);
    assert(f.props.openLid());
   }else {
    assert(f.props.applyHit(hit,102,1));for(unsigned n=0;n<30;++n)f.step();
    assert(!f.pickups.visible(int(f.props.pendant())) && !f.pickups.take(f.props.pendant()));
    const auto stateOnClosed=saved(state,id);auto closedReload=stateOnClosed;Fixture closed(id,root,closedReload);
    assert(closed.props.lidAngle()==1.12f && !closed.pickups.visible(int(closed.props.pendant())));
    assert(f.props.openLid());f.land();const auto pickup=f.pickups.take(f.props.pendant());assert(pickup && pickup->id==0x89);
   }
   for(unsigned n=0;n<60;++n)f.step();
   assert(f.props.lidAngle()==-1.83f);
   auto reloaded=saved(state,id);Fixture returned(id,root,reloaded);
   assert(returned.props.lidAngle()==-1.83f && !returned.props.scrollVisible(0x52));
   assert(!returned.pickups.visible(int(returned.props.pendant())) && !returned.pickups.take(returned.props.pendant()));
   // Day/night rooms share the original global pendant and well flags.
   Fixture counterpart(id==0x103?0x113:0x103,root,reloaded);
   assert(!counterpart.pickups.visible(int(counterpart.props.pendant())));
   std::cout<<std::hex<<id<<(closeFirst?" clean":" dirty")<<" pendant, collision, variants, save/re-entry and counterpart PASS\n";
  }
 }
 ScenarioState state;
 for(unsigned id:{0x103,0x108}) {
  Fixture f(id,root,state);assert(f.props.medals().size()==(id==0x103?7:8));
  for(std::size_t n=0;n<f.props.medals().size();++n) {
   const auto &m=f.props.medals()[n];auto matrix=PlacementMatrix(m.pose);
   // YarareInitCube uses the real part0 matrix and its authored -1000 offset.
   const auto local=TransformPoint(m.parts[0],{0,-900,0});const auto center=TransformPoint(matrix,local);
   CollisionVec a{center[0],center[1],center[2]+2000},b{center[0],center[1],center[2]-2000};
   auto hit=f.props.target(a,b,1);assert(hit && hit->kind==VillageProps::Hit::Medallion && hit->index==n);
   assert(f.props.applyHit(*hit,200+unsigned(n),1));assert(!f.props.applyHit(*hit,200+unsigned(n),1));
   assert(!f.props.roomObjectVisible(m.slot));
   if(VillageProps::syncMedallionFlags(state)==10)assert(state.scenario.test(13));
   bool sprites=false,lines=false;
   for(unsigned frame=0;frame<8;++frame){f.step();sprites|=!f.props.medalQuads({}).empty();lines|=!f.props.medalLines().empty();}
   assert(sprites && lines);
   assert(state.room(id==0x103?0x113:0x118).object(m.slot,1));
  }
 }
 assert(VillageProps::syncMedallionFlags(state)==15 && state.scenario.test(13) && state.scenario.test(48));
 state=saved(state,0x108);Fixture night(0x118,root,state);
 for(const auto &m:night.props.medals())assert(!night.props.roomObjectVisible(m.slot));
 std::cout<<"15 original medallions, paired room flags, mission flags and save roundtrip PASS\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
