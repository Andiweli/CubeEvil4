#include "room_interactions.h"
#include "room_jumps.h"
#include <cassert>
#include <cmath>
#include <iostream>
namespace {
using Triangle = std::array<std::array<float,3>,3>;
re4::Sat fixture(unsigned attributes=0x20, bool blocked=false, float farY=0) {
  std::vector<Triangle> walls{{{{-3000,0,500},{3000,900,500},{3000,0,500}}},
                              {{{-3000,0,500},{-3000,900,500},{3000,900,500}}}};
  if (blocked) {
    walls.push_back({{{0,0,600},{0,900,600},{0,900,1400}}});
    walls.push_back({{{0,0,600},{0,900,1400},{0,0,1400}}});
  }
  auto sat=re4::Sat::DynamicWalls(walls);
  for (unsigned i=0;i<2;++i) sat.polygons[i].attributes=attributes;
  const std::vector<Triangle> floors{{{{-5000,0,-5000},{5000,0,500},{5000,0,-5000}}},
                                    {{{-5000,0,-5000},{-5000,0,500},{5000,0,500}}},
                                    {{{-5000,farY,500},{5000,farY,5000},{5000,farY,500}}},
                                    {{{-5000,farY,500},{-5000,farY,5000},{5000,farY,5000}}}};
  std::vector<re4::SatPolygon> polygons;
  for (const auto &triangle:floors) {
    re4::SatPolygon polygon;
    polygon.normal=std::uint16_t(sat.normals.size()); sat.normals.push_back({0,1,0});
    for (unsigned k=0;k<3;++k) {
      polygon.vertices[k]=std::uint16_t(sat.vertices.size()); sat.vertices.push_back(triangle[k]);
    }
    polygons.push_back(polygon);
  }
  sat.polygons.insert(sat.polygons.begin(),polygons.begin(),polygons.end());
  sat.floor=floors.size();
  return sat;
}
std::array<std::vector<re4::FenceSample>,2> paths() {
  std::array<std::vector<re4::FenceSample>,2> result;
  for (auto &path:result)
    for (unsigned tick=1;tick<=20;++tick)
      path.push_back({{{0,0,50},0,tick==20},float(tick)});
  return result;
}
}
int main() {
  // Source pl_wep.cpp explicitly maps knife0x10 to size group2. Group2
  // bypasses all four WindowData hp policies; group0 keeps the ET07 fence.
  for (unsigned type=0;type<4;++type) {
    assert(re4::WindowWeaponHitPoints(1000,type,0x10)==0);
    assert(re4::WindowWeaponHitPoints(1000,type,7)==0);
  }
  assert(re4::WindowWeaponHitPoints(1000,0,1)==0);
  assert(re4::WindowWeaponHitPoints(1000,1,1)==500);
  assert(re4::WindowWeaponHitPoints(400,1,2)==0);
  assert(re4::WindowWeaponHitPoints(1000,2,1)==750);
  assert(re4::WindowWeaponHitPoints(1000,3,1)==1000);
  auto sat=fixture(); re4::SatCollision world(sat); re4::WalkProbe probe(world);
  assert(probe.place({0,0,-200}));
  const auto path=paths();
  const auto offer=re4::FindFence(world,probe,0,{0,1000,-200},path);
  assert(offer && offer->wide && std::abs(offer->alignment[2]-100)<.01);
  assert(std::abs(offer->landing[2]-1100)<.01 && offer->landing[1]==0);
  auto playback=re4::StartFencePlacement(probe.position(),0,*offer);
  for (unsigned tick=0;tick<10;++tick)
    re4::AdvanceFencePlacement(world,playback,path[0][tick].root,path[0][tick].frame);
  assert(playback.adjustment==0 && std::abs(playback.feet[2]-600)<.01);
  for (unsigned tick=10;tick<20;++tick)
    re4::AdvanceFencePlacement(world,playback,path[0][tick].root,path[0][tick].frame);
  assert(playback.feet==offer->landing);
  // Geometry alone must not turn arbitrary scenery into a climbable fence.
  auto ordinary=fixture(0); re4::SatCollision ordinaryWorld(ordinary);
  re4::WalkProbe ordinaryProbe(ordinaryWorld); assert(ordinaryProbe.place({0,0,-200}));
  assert(!re4::FindFence(ordinaryWorld,ordinaryProbe,0,{0,1000,-200},path));
  re4::FencePlayback before24; before24.feet={0,0,-200}; before24.adjustment=0;
  auto after24=before24;
  re4::AdvanceFencePlacement(ordinaryWorld,before24,{},24);
  re4::AdvanceFencePlacement(ordinaryWorld,after24,{},25);
  assert(before24.feet[0]==-50 && after24.feet[0]==0);
  auto obstruction=fixture(0x20,true); re4::SatCollision obstructedWorld(obstruction);
  re4::WalkProbe obstructedProbe(obstructedWorld); assert(obstructedProbe.place({0,0,-200}));
  assert(!re4::FindFence(obstructedWorld,obstructedProbe,0,{0,1000,-200},path));
  auto raised=fixture(0x20,false,301); re4::SatCollision raisedWorld(raised);
  re4::WalkProbe raisedProbe(raisedWorld); assert(raisedProbe.place({0,0,-200}));
  assert(!re4::FindFence(raisedWorld,raisedProbe,0,{0,1000,-200},path));
  auto missing=path; missing[0].clear();
  assert(!re4::FindFence(world,probe,0,{0,1000,-200},missing));
  assert(!re4::FindFence(world,probe,NAN,{0,1000,-200},path));
  std::cout << "Source knife group2, fence SAT permission, shoulder obstruction, floor reach and MotBase placement PASS\n";
}
