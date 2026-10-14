#include "destruction_effects.h"
#include "ports/rnd.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
using namespace re4;
namespace {
void word(DestructionRecord&r,unsigned p,unsigned n){for(unsigned i=0;i<4;++i)r.bytes[p+i]=std::uint8_t(n>>(24-i*8));}
void half(DestructionRecord&r,unsigned p,unsigned n){r.bytes[p]=n>>8;r.bytes[p+1]=n;}
void real(DestructionRecord&r,unsigned p,float v){unsigned n;std::memcpy(&n,&v,4);word(r,p,n);}
DestructionEffectsData fixture(){
  DestructionEffectsData d;d.signature=0x123456789;d.models.push_back({1,0,1});
  DestructionMesh mesh;mesh.efm=1;mesh.restCenter={100,100,0};d.meshes.push_back(mesh);
  DestructionRecord r;r.objectId=1;r.model=0;r.bytes[0]=1;r.bytes[1]=254;r.bytes[2]=1;r.bytes[7]=255;
  real(r,136,200);real(r,140,200);real(r,152,1);r.bytes[156]=r.bytes[157]=r.bytes[158]=r.bytes[159]=255;
  for(unsigned p=160;p<=172;p+=4)real(r,p,1);
  real(r,172,.5f);r.bytes[200]=8;r.bytes[201]=255;word(r,204,10);r.bytes[203]=0;
  d.records.push_back(r);r.sequence=4;r.bytes[200]=0;real(r,172,1);word(r,204,0);d.records.push_back(r);
  return d;
}
bool close(float a,float b){return std::abs(a-b)<.0001f;}
}
int main(){
  unsigned checks=0;auto d=fixture();DestructionEffectsSimulation sim(d);
  RndInit(0x913f);const auto gameRnd=RndState();
  assert(sim.trigger(1,0,7,{1000,2000,3000},{},0,42,0x1234));++checks;
  assert(RndState()==gameRnd);++checks;
  assert(sim.state().models.size()==2&&sim.instances().size()==2);++checks;
  const auto&m=sim.state().models.front();const auto&p=m.parts.front();
  // Original first source move: age becomes1, range2*511+1, radial impulse.
  const float impulse=8.f*4096.f/1023.f/std::sqrt(2.f);
  assert(m.age==1&&p.flag==1);++checks;
  assert(close(p.position[0],100+impulse)&&close(p.position[1],100+impulse));++checks;
  assert(close(p.speed[0],impulse)&&close(p.speed[1],impulse-1));++checks;
  assert(close(p.world[0],1100+impulse)&&close(p.world[1],2100+impulse)&&p.world[2]==3000);++checks;
  assert(m.color[3]==255);++checks;
  assert(!sim.trigger(1,0,7,{1000,2000,3000},{},0,42,0x1234));++checks;
  const auto before=sim.state();assert(!sim.trigger(1,0,7,{}, {},3,99,4));++checks;
  assert(sim.state().serial==before.serial&&sim.state().models.size()==before.models.size());++checks;
  sim.tick();const auto&next=sim.state().models.front();
  assert(next.age==2&&next.color[3]==127.5f);++checks;
  assert(close(next.parts[0].position[0],100+2*impulse));++checks;
  assert(close(next.parts[0].position[1],100+2*impulse-1));++checks;
  DestructionEffectsSimulation replay(d);assert(replay.restore(sim.state()));++checks;
  for(unsigned frame=0;frame<20;++frame){sim.tick();replay.tick();
    const auto a=sim.instances(),b=replay.instances();assert(a.size()==b.size());++checks;
    for(unsigned i=0;i<a.size();++i){assert(a[i].matrix==b[i].matrix&&a[i].color==b[i].color);++checks;}
  }
  assert(sim.instances().size()==1);++checks;
  // Neutral paused rendering has no simulation side effects.
  const auto paused=sim.state();for(unsigned i=0;i<120;++i)(void)sim.instances();
  assert(sim.state().frames==paused.frames&&sim.state().serial==paused.serial);++checks;
  auto bad=sim.state();bad.signature++;assert(!sim.restore(bad));++checks;
  bad=sim.state();bad.models.front().parts.front().flag=3;assert(!sim.restore(bad));++checks;
  bad=sim.state();bad.models.front().position[0]=std::numeric_limits<float>::quiet_NaN();assert(!sim.restore(bad));++checks;
  bad=sim.state();bad.models.front().record=999;assert(!sim.restore(bad));++checks;
  bad=sim.state();bad.models.front().parts.clear();assert(!sim.restore(bad));++checks;
  assert(sim.state().frames==paused.frames);++checks;
  assert(!sim.restoreBroken(1,0,7,{},{})&&sim.restoreBroken(1,0,8,{},{}));++checks;
  assert(sim.instances().size()==2);++checks;
  // Persistent original debris retains the u16 Life_time wrap after 36min.
  auto rollover=sim.state();
  for(auto&model:rollover.models)model.age=65535;
  assert(sim.restore(rollover));++checks;
  sim.tick();
  for(const auto&model:sim.state().models){assert(model.age==0);++checks;}
  auto overage=sim.state();overage.models.front().age=65536;
  assert(!sim.restore(overage));++checks;
  // Nonuniform scale is reapplied after local part rotation by partsWorldCalc.
  auto scaled=fixture();real(scaled.records[0],136,100);real(scaled.records[0],140,200);
  DestructionEffectsSimulation anisotropic(scaled);assert(anisotropic.trigger(1,0,1,{}, {},0,1,1));++checks;
  auto state=anisotropic.state();state.models.front().parts.front().rotation[2]=1.57079635f;
  assert(anisotropic.restore(state));++checks;const auto transform=anisotropic.instances().front().matrix;
  assert(close(transform[1],-1)&&close(transform[4],.5f));++checks;
  // Original emitter emits count+1 copies; two source ticks, then stops.
  auto generated=fixture();auto&r=generated.records[0];r.bytes[264]=1;r.bytes[269]=2;half(r,272,2);
  DestructionEffectsSimulation emitter(generated);assert(emitter.trigger(1,0,9,{}, {},0,6,7));++checks;
  assert(emitter.state().models.size()==4&&emitter.state().emitters.size()==1);++checks;
  emitter.tick();assert(emitter.state().models.size()==7&&emitter.state().emitters.empty());++checks;
  emitter.tick();assert(emitter.state().models.size()==7);++checks;
  // Obj05 Tool_flg2 uses the original SAT segment reflection, which takes
  // precedence over the floor branch when both flags are authored.
  std::vector<std::uint8_t> satBytes(20);satBytes[0]=0x20;Sat wall(satBytes);
  wall.vertices={{-1000,-1000,100},{-1000,1000,100},{1000,1000,100},{1000,-1000,100}};
  wall.normals={{0,0,-1}};wall.polygons={{{0,1,2},{},0,0,0},{{0,2,3},{},0,0,0}};wall.wall=2;
  const SatCollision scenario(wall);
  auto reflecting=fixture();word(reflecting.records[1],8,3);real(reflecting.records[1],228,5);
  DestructionEffectsSimulation bounce(reflecting,nullptr,&scenario);
  assert(bounce.restoreBroken(1,0,55,{},{}));++checks;
  auto flying=bounce.state();auto&part=flying.models[0].parts[0];
  part.position=part.world={0,100,0};part.speed={0,0,200};part.angularSpeed={1,2,3};part.flag=1;
  assert(bounce.restore(flying));++checks;bounce.tick();const auto&bounced=bounce.state().models[0].parts[0];
  assert(bounced.world[2]==99&&bounced.speed[2]==-100);++checks;
  assert(close(bounced.angularSpeed[0],-.8f)&&close(bounced.angularSpeed[1],-1.6f));++checks;
  assert(RndState()==gameRnd);++checks;
  std::cout<<"Efm05 analytic impulse, gravity, fade, emitter, replay, bounds, scale and RNG isolation PASS ("<<checks<<" checks)\n";
}
