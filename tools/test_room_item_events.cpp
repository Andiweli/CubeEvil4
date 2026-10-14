#include "room_item_events.h"
#include "save_native.h"
#include "ports/yz2.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace re4;
std::vector<std::uint8_t> entry(const RoomArchive &a,const char *tag){const auto *e=a.find(tag);assert(e);return a.copy(e-a.entries().data());}
int main(int argc,char **argv){try {
 assert(argc==2);const std::string root=argv[1];unsigned tests=0;
 for(unsigned id:{0x103,0x113,0x106}) {
  char name[16];std::snprintf(name,sizeof name,"r%03x",id);RoomArchive room(LoadYz2(root+"/St1/"+name+".das").bytes);
  Smd smd(entry(room,"SMD"));ScenarioState state;RoomPickups pickups(id,room,state);
  RoomItemEvents events(id,room,smd,state,pickups);assert(events.shelves().size()==(id==0x106?2:3));
  assert(!events.active() && !events.camera());
  for(unsigned n=0;n<events.shelves().size();++n) {
   const auto &s=events.shelves()[n];std::size_t item=~std::size_t(0);
   for(std::size_t k=0;k<pickups.items().size();++k)if(pickups.items()[k].trigger.no==s.itemArea)item=k;
   assert(item<pickups.items().size());assert(!pickups.visible(int(item)));assert(!pickups.take(item));
   const auto effects=pickups.effectSources();assert(std::none_of(effects.begin(),effects.end(),[&](auto &e){return e.key==0x20000000ULL+item+1;}));
   CollisionVec feet{0,s.area.area.floor+100,0};
   if(s.area.area.type==1){for(auto p:s.area.area.points){feet[0]+=p[0]*.25;feet[2]+=p[1]*.25;}}
   else {feet[0]=s.area.area.points[0][0];feet[2]=s.area.area.points[0][1];}
   bool reachable=false;for(unsigned h=0;h<360;++h) {
    const float angle=float(h)*.017453292f;auto front=feet;front[0]+=std::sin(angle)*550;front[2]+=std::cos(angle)*550;
    if(auto offer=events.offer(feet,angle,front);offer && *offer==n){reachable=true;break;}
   }
   const auto carriedBefore=state.carriedItems.size();const auto moneyBefore=state.pesetas;
   assert(reachable);assert(events.begin(n));assert(!events.begin(n));assert(events.active() && events.camera()->valid);
   assert(events.takeOpenSound() && !events.takeOpenSound());assert(state.room(id).room.test(s.flag));assert(pickups.visible(int(item)));
   // An authored opening reveals its pickup; it never grants it directly.
   assert(state.carriedItems.size()==carriedBefore && state.pesetas==moneyBefore);
   const auto pivot=TransformPoint(*events.delta(s.left),s.leftPose.position);
   for(unsigned k=0;k<3;++k)assert(std::abs(pivot[k]-s.leftPose.position[k])<.01f);
   volatile float originalAngle=0;for(unsigned k=0;k<30;++k)originalAngle+=-.063995406f;
   unsigned ticks=0;while(events.active()){events.tick30Hz();assert(++ticks<300);}
   assert(s.steps==30 && s.leftAngle==originalAngle && s.rightAngle==-originalAngle);
   assert(ticks>=s.camera->maximumFrame()+15);assert(!events.camera());const auto nextOffer=events.offer(feet,0);assert(!nextOffer || *nextOffer!=n);
   const auto result=pickups.take(item);assert(result && !pickups.take(item));assert(!pickups.visible(int(item)));
   ++tests;
  }
  InventoryCase bag(0);bag.add(0x7c,1);NativeSaveSnapshot save;save.inventory=bag.items();save.scenario=state;save.room=id;auto restored=DecodeNativeSave(EncodeNativeSave(save));
  RoomPickups restoredItems(id,room,restored.scenario);RoomItemEvents returned(id,room,smd,restored.scenario,restoredItems);
  for(const auto &s:returned.shelves()){assert(s.steps==30 && s.leftAngle==-1.9198622f && s.rightAngle==1.9198622f);
   auto at=std::find_if(restoredItems.items().begin(),restoredItems.items().end(),[&](auto &i){return i.trigger.no==s.itemArea;});
   assert(at!=restoredItems.items().end() && at->eventEnabled && !restoredItems.visible(int(at-restoredItems.items().begin())));}
  std::cout<<name<<" shelves, exact camera lifecycle, pickup prerequisites and native save roundtrip verified\n";
 }
 std::cout<<tests<<" authored shelf events PASS\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
