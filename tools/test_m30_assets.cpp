// Regression paths from the supplied log, replayed on unmodified disc data.
#include "room_interactions.h"
#include "room_lighting.h"
#include "leon_preview.h"
#include "walk_probe.h"
#include "room_blocks.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;}
std::vector<unsigned char> data(const re4::RoomArchive &a,const char *tag){auto *e=a.find(tag);check(e,"Disc subfile exists");return a.copy(e-a.entries().data());}
re4::WalkConfig config(){re4::WalkConfig c;c.originalScenerySphere=true;c.stepUp=600;c.drop=999;return c;}
re4::CollisionVec point(const re4::RoomObjectPlacement &p,float x,float y,float z){re4::SmdPlacement pose;pose.position=p.position;pose.rotation=p.rotation;pose.scale={1,1,1};auto v=re4::TransformPoint(re4::PlacementMatrix(pose),{x,y,z});return {v[0],v[1],v[2]};}
}
int main(int argc,char **argv){try{
 if(argc!=6)throw std::runtime_error("Usage: m30_assets_check R101 PL00 WEP01 CORE R100");
 auto room=re4::RoomArchive::Load(argv[1]);auto core=re4::RoomArchive::LoadDrs(argv[4]);
 auto objects=re4::BuildRoomObjects(room,data(core,"EFF"),0x101);
 re4::Sat sat(data(room,"SAT")),eat(data(room,"EAT"));re4::SatCollision world(sat),effect(eat,re4::SatQueryMode::Effect);
 re4::WalkProbe stair(world,config());
 const re4::CollisionVec blocked{3408.8,2342.8,-14045.9};
 check(stair.place(blocked),"Logged staircase start has genuine floor");

 for(unsigned i=0;i<35;++i) stair.moveDelta({0,0,-50});
 std::cout<<"stairs top "<<stair.position()[0]<<","<<stair.position()[1]<<","<<stair.position()[2]<<'\n';
 check(stair.position()[1]>3100 && stair.position()[2]<-15600,"Original foot sphere passes the logged stair underside and reaches landing");
 for(unsigned i=0;i<35;++i) stair.moveDelta({0,0,50});
 check(stair.position()[1]<2450,"The same real stair can be descended");
 for(unsigned slot:{1u,2u,3u})for(bool front:{true,false}){
  re4::RoomInteractions interior(0x101,room,objects.placements,argv[2],{},&effect);
  re4::WalkProbe probe(world,config());probe.setObjectCollision(interior.collision());
  auto &door=interior.doors().at(slot-1);auto start=point(door.placement,-650,0,front?-650:650);
  check(probe.place(start),"Door approach remains solid and grounded");re4::LeonPreview player(argv[2],argv[3]);
  const auto heading=door.placement.rotation[1]+(front?0:3.1415927f);player.setPlacement(probe.position(),heading);
  check(interior.interact(player,probe),"Hand opening available from both sides");
  for(unsigned i=0;i<12;++i)player.step(probe,0,0,false,false,1./30,{}, {},[&]{interior.tick(player,probe);});
  check(interior.interact(player,probe),"Repeated A starts original kick");
  for(unsigned i=0;i<55;++i)player.step(probe,0,0,false,false,1./30,{}, {},[&]{interior.tick(player,probe);});
  check(!player.doorActive()&&door.phase==re4::InteriorDoorPhase::Open&&door.openTimer,"Kick action finishes and original 150-tick open timer remains active");
  // Re-enable collision with the exact position that became stuck in the log.
  if(slot==1&&!front){probe.setScriptPosition({-10956.7,0,-6374.3});player.setPlacement(probe.position(),heading);interior.tick(player,probe);std::cout<<"door recovery "<<probe.position()[0]<<","<<probe.position()[2]<<'\n';check(interior.collision()->clearCapsule(probe.position(),300,1600),"Logged door overlap is separated by source circle/rectangle contact");}
  auto before=probe.position();
  for(unsigned i=0;i<20;++i)player.step(probe,0,1,false,false,1./30,{}, {},[&]{interior.tick(player,probe);});
  check(std::hypot(player.feet()[0]-before[0],player.feet()[2]-before[2])>500,"Open door permits movement instead of trapping the body");
 }
 unsigned pushed=0;
 for(unsigned slot:{13u,15u,17u})for(unsigned dir:{0u,1u,2u,3u}){
  re4::RoomInteractions interior(0x101,room,objects.placements,argv[2],{},&effect);
  re4::WalkProbe probe(world,config());probe.setObjectCollision(interior.collision());
  const re4::InteriorRack *rack=nullptr;for(auto &r:interior.racks())if(r.placement.slot==slot)rack=&r;
  check(rack,"Original rack exists");const auto &p=rack->placement;
  const float x[]={0,-950,0,950},z[]={650,0,-650,0};const float turn[]={3.1415927f,1.5707964f,0,4.712389f};
  auto start=point(p,x[dir],0,z[dir]);if(!probe.place(start))continue;
  re4::LeonPreview player(argv[2],argv[3]);player.setPlacement(probe.position(),p.rotation[1]+turn[dir]);
  auto offer=interior.pushOffer(player,probe);if(!offer||offer->slot!=slot)continue;
  const auto original=p.position;
  // Catch is aborted directly into the original idle blend when forward
  // is released before the furniture push cycle begins.
  player.beginPush(offer->heading);
  player.step(probe,0,0,false,false,1./30);
  check(!player.pushActive(),"Releasing LS during catch returns directly to source idle");
  for(unsigned i=0;i<65;++i)player.step(probe,0,1,false,false,1./30,{}, {},[&]{interior.tick(player,probe,1);});
  const auto distance=std::hypot(p.position[0]-original[0],p.position[2]-original[2]);
  std::cout<<"rack "<<slot<<" dir "<<dir<<" moved "<<distance<<" action "<<re4::PlayerActionLabel(player.action())<<'\n';
  check(player.pushActive()&&!player.weaponVisible(),"Original push owns player and hides handgun");
  if(distance>200)++pushed;
  const auto delta=interior.delta(slot);check(delta.has_value(),"Rack model follows source-owned placement");
  for(unsigned k=0;k<3;++k)check(std::abs((*delta)[k*4+3]-(p.position[k]-original[k]))<.01,"Rack render delta matches collider placement");
  for(unsigned i=0;i<30;++i)player.step(probe,0,0,false,false,1./30,{}, {},[&]{interior.tick(player,probe,0);});
  check(!player.pushActive(),"Releasing LS finishes original release motion");
 }
 check(pushed>=2,"At least two genuine furniture paths execute original rack and player root motion");
 auto r100=re4::RoomArchive::Load(argv[5]);auto lighting=re4::ReadRoomLighting(r100);
 re4::Smx smx(data(r100,"SMX"));auto blocks=re4::BuildRoomBlocks(r100,&smx,argv[5],0x100);
 check(blocks.count==5&&blocks.mesh.triangles>20000&&blocks.mesh.included>50,"All original r100 block files produce actual terrain and house placements");
 check(blocks.mesh.texturedTriangles==blocks.mesh.triangles,"Every original block triangle has its own resolved local/common material");
 check(blocks.areas.size()==4 && blocks.visibleSets==std::vector<std::uint32_t>({7,14,28,24}),
     "Original BLK connectivity displays only main-memory neighbours, never all cached blocks");
 for(const auto &area:blocks.areas) {
  re4::CollisionVec center{};center[1]=area.volume.floor+500;
  for(const auto &point:area.volume.points){center[0]+=point[0]/4;center[2]+=point[1]/4;}
  check(blocks.visibleMask(center)==blocks.visibleSets.at(unsigned(area.number)),
      "Original player trigger selects its authored visible block set");
  check(blocks.visibleMask({1e8,1e8,1e8})==blocks.visibleSets.at(unsigned(area.number)),
      "Outside all BLK areas the original previous area stays selected");
 }
 for(const auto &draw:blocks.mesh.draws)check(draw.roomBlock>=0 && draw.roomBlock<5,
     "Every original block draw retains independent cObj::blk ownership");
 re4::Smd main(data(r100,"SMD"));auto visible=re4::BuildStaticScene(main);
 re4::AppendStaticSceneLayer(visible,blocks.mesh);
 for(bool after:{false,true}){
  auto stateScene=visible;re4::RoomSaveState state;state.room.set(10,after);
  re4::ApplyRoomScriptVisibility(0x100,state,stateScene);
  for(unsigned work:{0x23u,0x44u,0x37u,0x38u}) {
   const bool present=std::any_of(stateScene.draws.begin(),stateScene.draws.end(),[work](const auto &d){return d.work==work;});
   check(present==(after?(work==0x37||work==0x38):(work==0x23||work==0x44)),"r100 bridge variants follow literal R100Init and car-event visibility");
  }
 }
 std::cout<<"r100 "<<blocks.describe()<<'\n';
 std::cout<<"r100 "<<lighting->describe()<<'\n';
 std::cout<<"M30 real asset regressions passed: "<<checks<<'\n';
 return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
