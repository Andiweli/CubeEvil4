#include "archive.h"
#include "collision_query.h"
#include "world_effects.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace re4;
namespace {
std::uint64_t checks{},lines{},lenses{},bounces{},fragments{};
void require(bool v,const char *why){++checks;if(!v)throw std::runtime_error(why);}
std::vector<std::uint8_t> entry(const RoomArchive &a,const char *name) {
  const auto p=a.find(name);require(p,"Actual source archive entry missing");
  return a.copy(p-a.entries().data());
}
unsigned half(const WaterRecord &r,unsigned p){return unsigned(r.bytes[p])*256+r.bytes[p+1];}
unsigned flags(const WaterRecord &r){return unsigned(r.bytes[8])<<24|unsigned(r.bytes[9])<<16|unsigned(r.bytes[10])<<8|r.bytes[11];}
float real(const WaterRecord &r,unsigned p){const std::uint32_t u=unsigned(r.bytes[p])<<24|unsigned(r.bytes[p+1])<<16|unsigned(r.bytes[p+2])<<8|r.bytes[p+3];float f;std::memcpy(&f,&u,4);return f;}
unsigned rnd(std::uint16_t &seed){const auto old=seed;const unsigned next=(unsigned(std::uint8_t((old>>1)+(old>>8)))<<8)|std::uint8_t(old>>1);seed=std::uint16_t(next==old?next+0x101:next);return seed>>8;}
void draw(std::uint32_t &seed,unsigned n){while(n--)seed=seed*0x19660du+0x3c6ef35fu;}
void bank(const WorldEffectsData &data,unsigned sequence,const SatCollision &floor,const CollisionVec &feet) {
  const auto target=unsigned(std::find(data.sequences.begin(),data.sequences.end(),sequence)-data.sequences.begin());
  std::vector<EnvironmentMatrix> parts(24,{1,0,0,float(feet[0]),0,1,0,float(feet[1])+350,0,0,1,float(feet[2])});
  parts[2][3]+=900;parts[23][7]+=500;
  WorldEffectsSimulation simulation(data);
  if(sequence!=9)require(!simulation.trigger(sequence,{},{},100+sequence,&floor),"Source actor parts required, never guessed at owner origin");
  require(simulation.triggerAttached(sequence,{float(feet[0]),float(feet[1]),float(feet[2])},{},100+sequence,parts,&floor),"Actual original EST bank activates");
  require(!simulation.triggerAttached(sequence,{},{},100+sequence,parts,&floor),"Duplicate source event never restarts");
  const auto initial=simulation.state();require(initial.instances.size()==1,"One independent original source instance");
  // Literal source Rnd and LCG draw count oracle for the shared direct EST
  // stream, including cObj05's otherwise separate model backend.
  std::uint16_t random=0x4e22;std::uint32_t seed=rnd(random);seed|=rnd(random)<<8;seed|=rnd(random)<<16;
  const auto initialFragments=simulation.drainFragmentSpawns();unsigned seen=0;
  for(unsigned i=0;i<data.sprites.targets[target].records.size();++i) {
    const auto &r=data.sprites.targets[target].records[i];
    if(half(r,4) || r.bytes[264])continue;
    if(r.bytes[1]==0xfe) {
      require(seen<initialFragments.size(),"Every original direct FE emits its real model request");
      const auto &q=initialFragments[seen++];
      require(q.sourceRecord==i && q.sequence==sequence && q.seed==seed && q.placement==parts[r.bytes[7]],"FE uses exact preceding mixed-EST seed and source posed post");
      draw(seed,13);++fragments;
    } else {
      draw(seed,16+unsigned(bool(flags(r)&8))+unsigned(bool(flags(r)&16))+
          (r.bytes[7]<0xf8 && (flags(r)&0x20)?3u:0u));
    }
  }
  require(seen==initialFragments.size() && seed==initial.instances[0].state.sequenceSeed[target],"Mixed sprite/model direct EST consumes exact source RNG");
  WorldEffectCamera camera;camera.eye={float(feet[0]),float(feet[1])+3000,float(feet[2])+12000};
  std::map<std::uint64_t,WaterSprite> previous;
  for(unsigned tick=0;tick<240;++tick) {
    parts[0][3]+=2;parts[2][3]+=2;parts[23][3]+=2;
    simulation.updateInstanceParts(100+sequence,parts);
    simulation.tick(&camera,&floor);const auto state=simulation.state();
    for(const auto &s:state.sprites) {
      const auto &r=data.sprites.targets[s.target].records[s.record];
      if(r.bytes[1]==9) {
        require(s.history.size()==4,"Source Esp09 Work8=0 creates four history points");
        require(s.history.front()==s.position,"Source Esp09 newest ring point is current world position");
        const auto old=previous.find(s.serial);
        if(old!=previous.end())require(s.history[1]==old->second.history[0],"Source Esp09 ring advances once at original 30Hz");
        previous[s.serial]=s;
      }
      if(r.bytes[1]==7) {
        require(s.bounceFlags==1 || s.bounceFlags==2,"Source Esp07 caches floor or is stopped");
        if(s.bounceFlags==1)require(s.speed==std::array<float,3>{} && s.acceleration==std::array<float,3>{} &&
            s.position[1]==s.bounceFloor+s.size[1]*.5f*s.sizeMultiplier,"Original blood floor snap and zero-reflection stop");
        ++bounces;
      }
    }
    for(const auto &q:WorldEffectLines(data,state)) {
      require(q.position.size()==4 && q.color.size()==4 && q.sourceSignature==data.sprites.signature,"Actual untextured GX line request owns source four vertices");
      const auto s=std::find_if(state.sprites.begin(),state.sprites.end(),[&](const auto &s){return s.serial==q.serial;});
      const auto &r=data.sprites.targets[s->target].records[s->record];
      require(s->size[0]>=real(r,136)-std::abs(real(r,144)) &&
          s->size[0]<=real(r,136)+std::abs(real(r,144)),"Original line base size remains within authored random spread");
      unsigned width=std::uint16_t(s->size[0]*s->sizeMultiplier*(.3f*.1f));
      if(s->age>half(r,186)+1) {
        float square=0;for(unsigned k=0;k<3;++k)square+=(s->position[k]-camera.eye[k])*(s->position[k]-camera.eye[k]);
        const float distance=std::sqrt(square)/5000.f;
        if(distance>1)width=std::uint16_t(float(width)/distance);
      }
      require(q.width==width,"Source GX width preserves truncation and subsequent distance attenuation");
      auto alpha=std::uint8_t(s->color[3]);const auto step=std::uint8_t(s->color[3]/3.f);
      for(const auto &c:q.color){require(c[3]==float(alpha)/255.f,"Source Esp09 alpha subtracts truncated byte step");alpha=std::uint8_t(alpha-step);}++lines;
    }
    for(const auto &q:WorldEffectLenses(data,state)) {
      require(q.centerRatio==1 && q.sizeRatio==0 && q.delDistance==0 && q.hideRadius==10 && q.sourceAge,
          "Actual Esp0e original centre/distance/Z visibility parameters");++lenses;
    }
    for(const auto &q:WorldEffectQuads(data,state,camera))require(q.type!=9 && q.type!=14 && q.type!=17 && q.type!=70,"Lines/lenses/light/filter never become fake billboard fallback");
    fragments+=simulation.drainFragmentSpawns().size();
    if(tick==2 || tick==70) {
      WorldEffectsSimulation resumed(data);require(resumed.restore(simulation.state()),"Actual parented source instance restores atomically");
      resumed.tick(&camera,&floor);simulation.tick(&camera,&floor);
      require(resumed.state().sprites.size()==simulation.state().sprites.size(),"Checkpoint keeps source effect population");
      for(unsigned i=0;i<resumed.state().sprites.size();++i)require(resumed.state().sprites[i].position==simulation.state().sprites[i].position &&
          resumed.state().sprites[i].history==simulation.state().sprites[i].history,"Checkpoint keeps source physics and ring histories");
      previous.clear();
    }
  }
  if(sequence==3 || sequence==5) {
    require(!simulation.state().sprites.empty() && simulation.stopInstance(100+sequence),"Armed source persistent emitters live until original owner deletion");
    require(simulation.state().sprites.empty() && !simulation.stopInstance(100+sequence),"Deleted armed owner cannot emit orphan lights");
  }
}
}
int main(int argc,char **argv) {
  try {
    if(argc!=4)throw std::runtime_error("Usage: m37_trap_effects_check EM2A CORE R100_DAT");
    const auto data=BuildTrapWorldEffects(entry(RoomArchive::LoadDrs(argv[1]),"EFF"),entry(RoomArchive::LoadDrs(argv[2]),"EFF"));
    const Sat sat(entry(RoomArchive::Load(argv[3]),"SAT"));const SatCollision floor(sat);const auto spawn=floor.spawn(200,1800);require(bool(spawn),"Actual r100 SAT source floor available");
    require(data.sprites.targets.size()==7 && data.fragmentRecords.size()==9 && data.sprites.warnings.empty(),"All actual dry EM2A banks route original records without fallback");
    for(unsigned sequence:{0u,1u,2u,3u,4u,5u,9u})bank(data,sequence,floor,spawn->point);
    require(lines && lenses && bounces && fragments,"Every new actual trap source path executes");
    std::cout<<"M37 trap original effects PASS: "<<checks<<" checks; lines="<<lines<<", lenses="<<lenses<<", blood="<<bounces<<", model spawns="<<fragments<<'\n';return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
