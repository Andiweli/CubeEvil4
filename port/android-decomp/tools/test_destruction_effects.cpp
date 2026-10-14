#include "destruction_effects.h"
#include "archive.h"
#include "room_objects.h"
#include "sat.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
int main(int argc,char**argv){try {
  if(argc!=4)return 2;
  const auto room=re4::RoomArchive::Load(argv[1]);
  const auto core=re4::RoomArchive::LoadDrs(argv[2]);const auto*ce=core.find("EFF");assert(ce);
  const auto id=std::uint16_t(std::stoul(argv[3],nullptr,16));
  const auto data=re4::BuildDestructionEffects(room,core.copy(ce-core.entries().data()),id);
  std::cout<<data.describe()<<'\n';assert(data.models.size()==(id==0x400?8u:7u)&&data.meshes.size()==(id==0x400?34u:23u));
  std::map<unsigned,unsigned> expected{{1,80},{4,193},{9,412},{14,78},{16,52},{25,92},{43,68}};
  if(id==0x400)expected[0x44]=412;
  unsigned checks=0;std::size_t total=0;
  for(const auto&model:data.models){unsigned count=0;for(unsigned k=0;k<model.parts;++k){
    const auto&mesh=data.meshes.at(model.firstMesh+k);assert(mesh.efm==model.efm&&mesh.part==k);
    assert(mesh.mesh.triangles&&mesh.mesh.triangles==mesh.mesh.texturedTriangles);
    assert(!mesh.mesh.fallbackTriangles&&!mesh.mesh.unresolvedAlphaTriangles);
    count+=mesh.mesh.triangles;checks+=4;
  }assert(count==expected.at(model.efm));total+=count;++checks;}
  assert(total==(id==0x400?1387u:975u));++checks;
  const auto*se=room.find("SAT");assert(se);const re4::Sat sat(room.copy(se-room.entries().data()));
  const re4::SatCollision collision(sat,re4::SatQueryMode::Effect),scenario(sat);
  const auto*ets=room.find("ETS");assert(ets);
  const auto placements=re4::ReadRoomObjectPlacements(room.copy(ets-room.entries().data()));
  re4::DestructionEffectsSimulation sim(data,&collision,&scenario);
  unsigned containers=0,explosives=0;
  for(const auto&p:placements){unsigned type;
    if(p.id==1)type=0;else if(p.id==2)type=1;else if(p.id==17)type=3;else if(p.id==30)type=5;else if(p.id==45){type=2;++explosives;}else continue;
    assert(sim.trigger(p.id,type,p.slot,p.position,p.rotation,0,p.slot+100,0x12345+p.slot));
    assert(!sim.trigger(p.id,type,p.slot,p.position,p.rotation,0,p.slot+100,0x12345+p.slot));
    ++containers;checks+=2;
  }
  assert(containers>0);++checks;
  const auto initial=sim.instances();assert(initial.size()>containers);++checks;
  auto copy=sim.state();re4::DestructionEffectsSimulation restored(data,&collision,&scenario);assert(restored.restore(copy));++checks;
  unsigned moving=0,resting=0;
  for(unsigned frame=0;frame<600;++frame){sim.tick();restored.tick();
    const auto a=sim.instances(),b=restored.instances();assert(a.size()==b.size());++checks;
    for(std::size_t k=0;k<a.size();++k){assert(a[k].mesh==b[k].mesh&&a[k].matrix==b[k].matrix&&a[k].color==b[k].color);++checks;}
    for(const auto&m:sim.state().models)for(const auto&p:m.parts){if(p.flag==1)++moving;if(p.flag==2)++resting;}
  }
  assert(moving&&resting);checks+=2;
  // Permanent source fallback remains after the flying parts finish fading.
  assert(sim.instances().size()==containers-explosives);++checks;
  for(const auto&m:sim.state().models){assert(data.records[m.record].sequence==4||data.records[m.record].sequence==8);++checks;}
  re4::StaticScene scene;const auto bounds=scene.minimum;re4::AppendDestructionScene(scene,data);
  assert(scene.triangles==total&&scene.minimum==bounds);++checks;
  for(const auto&draw:scene.draws){assert(draw.destructionMesh>=0&&std::size_t(draw.destructionMesh)<data.meshes.size());++checks;}
  std::cout<<"Original authored Efm05 meshes, seeded trajectories, floor bounces, fade, replay PASS ("<<checks<<" checks, "<<containers<<" containers)\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
