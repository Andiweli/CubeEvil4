#include "archive.h"
#include "scenario_collision.h"
#include "walk_probe.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
unsigned checks;
void require(bool pass,const char *message){++checks;if(!pass)throw std::runtime_error(message);}
std::vector<std::uint8_t> entry(const re4::RoomArchive &arc,const char *tag) {
  const auto *e=arc.find(tag);if(!e)throw std::runtime_error("Original room entry missing");
  return arc.copy(e-arc.entries().data());
}
void dump(const re4::Sat &sat) {
  for(const auto &polygon:sat.polygons) {
    std::cout<<polygon.attributes<<' ';
    for(auto vertex:polygon.vertices)for(auto value:sat.vertices[vertex])std::cout<<value<<' ';
    std::cout<<'\n';
  }
}
}
int main(int argc,char **argv){try {
  using namespace re4;
  if(argc!=2 && argc!=5)throw std::runtime_error("Usage: scenario_collision_check ROOM [--dump BEFORE|AFTER SAT|EAT]");
  const auto arc=RoomArchive::Load(argv[1]);const auto bytes=entry(arc,"AEV");
  const ScenarioCollision pieces(0x100,bytes);RoomSaveState before,after;after.room.set(10);
  if(argc==5) {
    require(std::string(argv[2])=="--dump","Invalid dump command");
    std::cout.precision(9);dump(pieces.geometry(std::string(argv[3])=="AFTER"?after:before,
        std::string(argv[4])=="EAT"?SatQueryMode::Effect:SatQueryMode::Sight));return 0;
  }
  require(pieces.pieces().size()==7 && pieces.parentedCount()==0,"Seven original unparented r100 barriers");
  auto satBefore=pieces.geometry(before,SatQueryMode::Sight),satAfter=pieces.geometry(after,SatQueryMode::Sight);
  auto eatBefore=pieces.geometry(before,SatQueryMode::Effect),eatAfter=pieces.geometry(after,SatQueryMode::Effect);
  require(satBefore.polygons.size()==20 && satBefore.slope==4 && satBefore.wall==16,"Truck/car use original SAT walls and box caps");
  require(satAfter.polygons.size()==8 && satAfter.wall==8,"Posttruck ravine replaces vehicle walls");
  require(eatBefore.polygons.size()==60 && eatAfter.polygons.empty(),"Original separate EAT control bits and car/truck removal");
  const Sat roomSat(entry(arc,"SAT")),roomEat(entry(arc,"EAT"));
  SatCollision sight(roomSat),effect(roomEat,SatQueryMode::Effect);
  ScenarioCollision live(0x100,bytes);
  require(live.sync(before,&sight,&effect) && !live.sync(before,&sight,&effect),"Room installs once without rebuilding stable geometry");
  unsigned grounded=0;
  for(const auto &piece:pieces.pieces()) {
    require(pieces.enabled(piece,before)==(piece.number!=0x1c),"R100Init barrier enable state");
    require(pieces.enabled(piece,after)==(piece.number==0x1c),"r100_Car_pos_move/RSF10 barrier enable state");
    if(piece.number!=4 && piece.number!=5)continue;
    SatCollision authored(satBefore);
    for(unsigned edge=0;edge<4;++edge) {
      const auto &a=piece.points[edge],&b=piece.points[(edge+1)%4];
      double nx=b[1]-a[1],nz=-(b[0]-a[0]),len=std::hypot(nx,nz);nx/=len;nz/=len;
      CollisionVec from{(double(a[0])+b[0])/2+nx*1200,piece.floor,
                        (double(a[1])+b[1])/2+nz*1200};
      // Source wall indices put the outward normal on the right of each XZ edge.
      const CollisionVec motion{-nx*2400,0,-nz*2400};
      auto hit=authored.sweepCapsule(from,motion,300,600,SatSubset::Walls,0x1000,SatSidedness::AuthoredFront);
      require(bool(hit) && hit->fraction<.5,"Every vehicle face blocks a fast original-side body sweep");
      require(!authored.sweepCapsule(from,motion,300,600,SatSubset::Walls,0x4000),"Source sight exclusion bit0x40 stays effective");
      auto floor=sight.ground(from,600,100000);
      if(!floor || std::abs(floor->point[1]-from[1])>=1000)continue;
      from[1]=floor->point[1];WalkConfig config;config.originalScenerySphere=true;config.stepUp=600;config.drop=999;
      WalkProbe player(sight,config);if(!player.place(from,5))continue;
      for(unsigned step=0;step<90;++step)player.moveDelta({motion[0]/60,0,motion[2]/60});
      const auto position=player.position();
      const double distance=(position[0]-(double(a[0])+b[0])/2)*nx+(position[2]-(double(a[1])+b[1])/2)*nz;
      require(distance>200,"Leon cannot walk into a car/truck from actual surrounding room floor");++grounded;
      CollisionVec rayFrom=from,rayTo=from;rayFrom[1]+=700;
      rayTo={from[0]+motion[0],rayFrom[1],from[2]+motion[2]};
      auto dynamic=sight.line(rayFrom,rayTo,SatSubset::Walls);
      require(bool(dynamic),"Scene queries include scenario wall");
      require(sight.triangle(dynamic->polygon).size()==3,"Combined polygon identity maps to owned geometry");
    }
  }
  require(grounded>=4,"Actual room movement tested on at least four vehicle approaches");
  // Reproduce the provided truck/photo corridor without inventing a model box.
  const CollisionVec truckRayFrom{-65000,400,-17220},truckRayTo{-57000,400,-17220};
  auto barrier=sight.line(truckRayFrom,truckRayTo,SatSubset::Walls);
  require(barrier && barrier->polygon>=roomSat.polygons.size(),"Photo truck corridor hits its authored AEV4 barrier");
  require(effect.line(truckRayFrom,truckRayTo).has_value(),"Original EAT also blocks truck shots");
  require(live.sync(after,&sight,&effect),"Live truck event updates owned pieces before gameplay resumes");
  require(!sight.line(truckRayFrom,truckRayTo,SatSubset::Walls),"Truck corridor releases after s20");
  require(!effect.line(truckRayFrom,truckRayTo),"Truck EAT releases with the visual event");
  require(live.sync(before,&sight,&effect),"Fresh/save-restored pretruck room restores barriers");
  require(sight.line(truckRayFrom,truckRayTo,SatSubset::Walls).has_value(),"Repeated room state restores vehicle barrier");
  auto floorBytes=bytes;
  for(std::size_t p=16;p+156<=floorBytes.size();p+=156)if(floorBytes[p+0x35]==0xb && floorBytes[p+0x36]==4) {
    std::fill_n(floorBytes.begin()+p+12,4,0);
    floorBytes[p+0x70]=0x80; // Unused high control bit must not change source masks.
    floorBytes[p+0x74]=0;floorBytes[p+0x75]=0;floorBytes[p+0x76]=2;floorBytes[p+0x77]=0;
  }
  const auto floorOnly=ScenarioCollision(0x100,floorBytes).geometry(before,SatQueryMode::Sight);
  require(floorOnly.floor==2 && floorOnly.polygons.size()==14,"Source floor piece accepts unused zero height/high control bits");
  bool rejected=false;try{ScenarioCollision invalid(0x100,std::vector<std::uint8_t>(15));}catch(const std::runtime_error&){rejected=true;}
  require(rejected,"Truncated AEV rejected before publication");
  std::cout<<"Original scenario collision: "<<checks<<" checks PASS; grounded vehicle approaches="<<grounded<<'\n';
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
