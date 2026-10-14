#include "event_playback.h"
#include "asset_reader.h"
#include <cmath>
#include <functional>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks{};
void require(bool v,const char *m) {++checks;if(!v)throw std::runtime_error(m);}
void reject(const std::function<void()> &fn) {bool caught=false;try{fn();}catch(const std::exception&){caught=true;}require(caught,"Malformed EVD must fail");}
// Independent raw BE first-key oracle. CameraMotion::move maps tracks0/1
// to position/target, roll2.y and fov3.y. First authored keys are frame0.
std::array<float,3> first(const std::vector<std::uint8_t> &b,unsigned channel) {
 const AssetReader r{b};const auto n=b[2];const auto fmt=r.u16(3+channel*2)>>12;
 constexpr unsigned widths[16][3]={{4,4,4},{4,2,2},{4,1,1},{},{2,4,4},{2,2,2},{2,1,1},{},{1,4,4},{1,2,2},{1,1,1},{},{},{},{},{4,0,0}};
 auto p=std::size_t(r.u32(((3+n*3+3)&~3u)+4+channel*4));std::array<float,3> out{};
 for(unsigned axis=0;axis<3;++axis) {
  const auto count=r.u16(p);require(r.u16(p+2)==0,"Camera first key frame0");const auto start=p+2+count*2;
  if(widths[fmt][0]==4) out[axis]=r.f32(start);
  else if(widths[fmt][0]==2)out[axis]=float(std::int16_t(r.u16(start)))*.0001f;
  else out[axis]=float(std::int8_t(b.at(start)))*.0001f;
  p=start+count*(widths[fmt][0]+widths[fmt][1]+widths[fmt][2]);
 }
 return out;
}
void check(const std::string &root,const std::string &name,unsigned packets,unsigned assets,unsigned frames,unsigned cuts) {
 OriginalEvent event(root,name);std::cout<<event.describe()<<'\n';
 require(event.data().packets().size()==packets,"Actual packet count");require(event.data().assets().size()==assets,"Actual asset count");
 require(event.totalFrames()==frames,"Source total duration");require(event.data().cutFrames().size()==cuts,"Source camera cuts");
 unsigned observedCuts=0,voices=0,music=0,subtitles=0,morphs=0,effectFrames=0,ribbonFrames=0,pathFrames=0;std::vector<std::uint64_t> revisions(event.actors().size());
 while(!event.finished()) {
  const auto frame=event.sourceFrame(),cut=event.sourceCut();const auto camera=event.camera();
  require(camera.valid,"Real event camera valid");const auto effectClock=event.effectsState().frames;event.tick30Hz(false);
  require(event.sourceFrame()==frame && event.sourceCut()==cut && event.camera().eye==camera.eye,"Pause preserves source frame/camera");
  require(event.effectsState().frames==effectClock,"Pause preserves original effect clock");
  WorldEffectCamera view;view.eye=camera.eye;
  float length=0;for(unsigned axis=0;axis<3;++axis) {view.forward[axis]=camera.target[axis]-camera.eye[axis];length+=view.forward[axis]*view.forward[axis];}
  length=std::sqrt(length);for(auto &v:view.forward)v/=length;
  const auto quads=WorldEffectQuads(event.effectsData(),event.effectsState(),view);
  if(!quads.empty())++effectFrames;
  for(const auto &quad:quads) {
    require(!quad.tiled && quad.blend<=1,"Actual event source blend/geometry supported");
    require(quad.animation<event.effectsData().sprites.animations.size() && quad.image<event.effectsData().sprites.animations[quad.animation].images.size(),"Actual CORE animation valid");
    require(quad.sequence==(cut/10)*16+cut%10,"Camera cut deletes prior EST sprites");
    for(const auto &point:quad.position)for(float value:point)require(std::isfinite(value),"Source event particle geometry finite");
  }
  for(const auto &sprite:event.effectsState().sprites) {
    const auto &rec=event.effectsData().sprites.targets.at(sprite.target).records.at(sprite.record);
    if(rec.bytes[1]==18) {require(sprite.history.size()==5,"Esp12 original five-point ribbon");++ribbonFrames;}
    if(rec.bytes[265]==2)++pathFrames;
  }
  if(event.cutFrame()==0) {
   ++observedCuts;const EventPacket *cam=nullptr;
   for(const auto &p:event.data().packets())if(p.type==6 && p.cut==int(cut))cam=&p;
   require(cam!=nullptr,"All camera cuts sourced from packet");const auto b=event.data().asset(cam->text(16,48));
   for(unsigned i=0;i<b[2];++i) {
    const auto oracle=first(b,i);const auto index=b[3+b[2]*2+i];
    if(index==0 || index==1)for(unsigned axis=0;axis<3;++axis) require(std::abs(oracle[axis]-(index==0?camera.eye:camera.target)[axis])<.005f,"Original raw first-key camera oracle");
    if(index==2)require(std::abs(oracle[1]-camera.roll)<.00001f,"Original roll oracle");
    if(index==3)require(std::abs(oracle[1]-camera.fovRadians)<.00001f,"Original FOV oracle");
   }
  }
  for(const auto &cue:event.takeAudioCues())if(cue.stream) {
   const auto stream=event.audioStream(cue);require(stream!=nullptr && !stream->left.empty(),"Real source DSP event audio");
   if(cue.block)++voices;else ++music;
  }
  if(event.subtitle().remainingFrames) {
   require(event.subtitle().words.empty(),"Source MesSet omits English MDT lookup entirely");
   require(event.subtitle().glyphs.empty(),"Source MesSet suppresses English captions");++subtitles;
  }
  for(unsigned i=0;i<event.actors().size();++i) {
   const auto &a=*event.actors()[i];require(a.mesh().vertices.size()/3==a.skin().size(),"Actual model corners/skin agree");
   require(a.lightRadius()>0 && std::isfinite(a.lightRadius()),"Original BIN light bound radius");
   for(const auto value:a.lightPosition())require(std::isfinite(value),"Original rootparts light bound follows pose");
   if(a.name()=="obm2d00")require(a.lightMask()==4,"Original truck selects OBM lights");
   if(a.name()=="pl0000" || a.name()=="wep0200")require(a.lightMask()==1,"Original Leon/handgun light mask");
   if(name=="r100s03" && a.name()=="wep0200") {
    require(a.visible()==(cut<=4 || cut>=13),"Evt_R100S03_Func be_flag2 means handgun drawn on cuts0-4/13-20");
    const auto matrix=a.worldMatrix();
    for(unsigned col=0;col<3;++col) {
      const double length=double(matrix[col])*matrix[col]+double(matrix[4+col])*matrix[4+col]+double(matrix[8+col])*matrix[8+col];
      require(std::abs(length-1)<.00001,"Source obj18SetOya strips parent-parts scale from handgun basis");
    }
   }
   if(a.rig())for(const auto &m:a.rig()->bones())for(auto v:m)require(std::isfinite(v),"All authored actor poses finite");
   if(a.meshRevision()!=revisions[i]) {++morphs;revisions[i]=a.meshRevision();}
  }
  event.tick30Hz();
 }
 require(observedCuts==cuts,"All authored camera cuts played");require(voices==1,"Original event voice cue played once");require(subtitles>0 && morphs>0,"Original subtitle and facial animation timelines execute");
 require(music==(name=="r100s03"?1u:0u),"Source music cue count");
 require(event.unsupported().empty(),"All actual original event packet/effect types supported");
 require(effectFrames>0,"Original EST quads appear on source clock");
 require(name=="r100s03"?ribbonFrames>0:pathFrames>0,"Actual source ribbon/path emitter executes");
 const auto finalPose=event.endPlayerPlacement();
 require((name=="r100s03")==bool(finalPose),"Source s20 PlPosNoSet preserves gameplay player");
 event.reset();require(event.sourceFrame()==0 && !event.finished(),"Reset rebuilds same source clock");
 event.finish();require(event.finished() && event.effectsState().sprites.empty(),"Cancel releases original event effects");
 const auto skippedPose=event.endPlayerPlacement();
 if(finalPose)for(unsigned axis=0;axis<3;++axis) {
   require(std::abs(finalPose->position[axis]-skippedPose->position[axis])<.001f,"Source cancel reaches exact final player root");
   require(std::abs(finalPose->rotation[axis]-skippedPose->rotation[axis])<.00001f,"Source cancel reaches exact final player heading");
 }
 event.reset();event.tick30Hz();event.tick30Hz();event.finish();
 require(event.finished(),"Cancel during existing cut finishes bounded source packets");
 std::cout<<name<<" full playback PASS; cuts="<<observedCuts<<", subtitle frames="<<subtitles<<", morph revisions="<<morphs<<", effect frames="<<effectFrames<<'\n';
}
}
int main(int argc,char **argv) {
 try {
  if(argc!=2)throw std::runtime_error("Usage: m36_event_playback_check DISC_FILES");
  check(argv[1],"r100s03",128,125,1465,21);check(argv[1],"r100s20",34,29,570,5);
  reject([]{EventData data(std::vector<std::uint8_t>(80));});
  std::ifstream file(std::string(argv[1])+"/Evd/r100s03.evd",std::ios::binary);
  const std::vector<std::uint8_t> original{std::istreambuf_iterator<char>(file),{}};
  const AssetReader reader{original};const auto packet=reader.u32(64),directory=reader.u32(76);
  reject([&]{auto bytes=original;for(unsigned i=0;i<4;++i)bytes[76+i]=bytes[64+i];EventData data(std::move(bytes));});
  reject([&]{auto bytes=original;bytes[packet+12]=bytes[packet+13]=255;EventData data(std::move(bytes));});
  reject([&]{auto bytes=original;for(unsigned i=0;i<4;++i)bytes[directory+48+i]=0;EventData data(std::move(bytes));});
  reject([&]{OriginalEvent unsupportedCaptionResources(argv[1],"r100s03",2);});
  std::cout<<"M36 original EVD playback PASS: "<<checks<<" checks\n";return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
