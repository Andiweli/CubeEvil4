#include "event_playback.h"
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace re4;
namespace {
std::uint64_t checks{},models{},sines{},ghosts{},lights{},screen{},patches{};
void require(bool v,const char *message) {++checks;if(!v)throw std::runtime_error(message);}
unsigned half(const WaterRecord &r,unsigned p) {return unsigned(r.bytes[p])*256+r.bytes[p+1];}
float real(const WaterRecord &r,unsigned p) {
  const std::uint32_t bits=(std::uint32_t(r.bytes[p])<<24)|(std::uint32_t(r.bytes[p+1])<<16)|
      (std::uint32_t(r.bytes[p+2])<<8)|r.bytes[p+3];
  float value;std::memcpy(&value,&bits,4);return value;
}
// Independent cObj04 source state: every actual FC random amplitude is zero;
// none of these records uses collisions, authored motion or attachments.
void modelOracle(const WaterRecord &r,const WaterSprite &s) {
  std::array<float,3> pos{},speed{},angle{},angular{};
  std::array<float,4> color{};
  for(unsigned k=0;k<3;++k) {
    pos[k]=real(r,12+k*4);speed[k]=real(r,36+k*4);
    angle[k]=real(r,88+k*4)*.017453292f;angular[k]=real(r,112+k*4)*.017453292f;
    for(unsigned p:{24u,52u,76u,100u,124u})require(real(r,p+k*4)==0,"Actual Efm04 oracle has no stochastic fields");
  }
  require(real(r,144)==0,"Actual Efm04 scale has no random amplitude");
  for(unsigned k=0;k<4;++k)color[k]=r.bytes[156+k];
  float size=1,plus=real(r,148);unsigned age=half(r,186);
  while(age<s.age) {
    if(half(r,180)<=age)for(unsigned k=0;k<3;++k) {
      pos[k]+=speed[k];speed[k]+=real(r,64+k*4);speed[k]*=real(r,48);
    }
    if(half(r,182)<=age) {size+=plus;plus*=real(r,152);}
    for(unsigned k=0;k<3;++k)angle[k]+=angular[k];
    const unsigned fade=half(r,176),hold=half(r,178);
    if(fade<age) {
      if(fade+hold<=age)for(unsigned k=0;k<4;++k)color[k]=std::min(255.f,color[k]*real(r,160+k*4));
    } else if(fade)color[3]=float(r.bytes[159])*float(age)/float(fade);
    ++age;
  }
  require(s.position==pos && s.speed==speed,"Efm04 source movement/speed matches exactly");
  require(s.angle==angle && s.angleSpeed==angular,"Efm04 source DEG2RAD/angular history matches exactly");
  require(s.size[0]==real(r,136)*.005f && s.size[1]==real(r,140)*.005f,
      "Efm04 source model scale0.005 matches exactly");
  require(s.sizeMultiplier==size && s.sizeStep==plus && s.color==color,
      "Efm04 source scale/colour envelopes match exactly");
  ++models;
}
void check(const std::string &root,const char *name,unsigned expectedModels) {
  OriginalEvent event(root,name);const auto &data=event.effectsData();
  require(data.eventModels.size()==expectedModels,"Complete authored FC model record bank preserved");
  std::map<std::uint64_t,WaterSprite> previous;unsigned previousCut=~0u;
  WorldEffectCamera camera;
  while(!event.finished()) {
    const auto cut=event.sourceCut();if(cut!=previousCut)previous.clear();previousCut=cut;
    const auto &state=event.effectsState();camera.eye=event.camera().eye;
    unsigned activeModels=0;
    for(const auto &s:state.sprites) {
      const auto &r=data.sprites.targets.at(s.target).records.at(s.record);
      if(r.bytes[1]==0xfc) {modelOracle(r,s);++activeModels;}
      if(r.bytes[1]==72) {
        const auto old=previous.find(s.serial);
        if(old!=previous.end()) {
          require(s.sinePhase==old->second.sinePhase+.01f,"Esp48 source phase advances by0.01");
          for(unsigned k=0;k<3;++k) {
            const float offset=real(r,216+k*12)*.1f*motion64::sinf(s.sinePhase*real(r,220+k*12));
            require(s.sineOffset[k]==offset,"Esp48 source sine offset matches exact fdlibm");
            require(s.angle[k]==((old->second.angle[k]-old->second.sineOffset[k])+s.angleSpeed[k])+offset,
                "Esp48 removes only its previous offset before CommonMove");
          }
        }
        previous[s.serial]=s;++sines;
      }
      if(s.motionTrailGhost) {
        require(r.bytes[1]==10 && s.motionTrailLife==110,"Actual Esp0a50-copy source lifetime110");
        require(s.speed==std::array<float,3>{} && s.sizeStep==0 && s.age<=110,
            "Esp0a source copies freeze their motion and scale");++ghosts;
      }
    }
    const auto modelDraws=WorldEffectModels(data,state);
    require(modelDraws.size()==activeModels,"Every live original FC has one model draw owner");
    for(const auto &m:modelDraws) {
      require(m.lightMask==16,"EfmSeqSet source lit mask0x10");
      for(float x:m.matrix)require(std::isfinite(x),"Every source FC model matrix finite");
    }
    const auto requests=WorldEffectLights(data,state);lights+=requests.size();
    for(const auto &l:requests)require(l.kind==0 && l.cut==8 && l.number==0 && l.radius>0,
        "Every authored Esp11 references original COREcut8light0");
    for(const auto &q:WorldEffectQuads(data,state,camera)) {
      require(q.type!=0xfc && q.type!=17,"EFM and LightMgr records never become fake billboard sprites");
      if(q.screen)++screen;
      if(q.type==27)++patches;
      for(const auto &p:q.position)for(float x:p)require(std::isfinite(x),"All actual billboard/spline/screen positions finite");
    }
    event.tick30Hz();
  }
  std::cout<<name<<" authored effect bank PASS\n";
}
}
int main(int argc,char **argv) {
  try {
    if(argc!=2)throw std::runtime_error("Usage: m37_opening_effects_check DISC_FILES");
    check(argv[1],"r120s00",94);check(argv[1],"r120s01",192);check(argv[1],"r100s40",0);
    require(models && sines && ghosts && lights && screen && patches,"Every actual new original effect path executes");
    std::cout<<"M37 original opening effects PASS: "<<checks<<" checks; model poses="<<models
      <<", sine poses="<<sines<<", trail ghosts="<<ghosts<<", light requests="<<lights
      <<", screen quads="<<screen<<", bilinear patch quads="<<patches<<'\n';return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
