#include "event_playback.h"
#include "asset_reader.h"
#include "r100_scenario.h"
#include "weapon_preview.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks{};
void require(bool value,const char *message) {++checks;if(!value)throw std::runtime_error(message);}
const GenericEventActor &find(const OriginalEvent &event,const char *name) {
 for(const auto &actor:event.actors())if(actor->name()==name)return *actor;
 throw std::runtime_error(std::string("Actual source actor missing: ")+name);
}
// Raw source key records, evaluated independently in double precision.
float focusOracle(const std::vector<std::uint8_t> &bytes,unsigned base,unsigned frame) {
 const AssetReader r{bytes};const auto count=r.u32(base);
 for(unsigned i=0;i+1<count;++i) {
  const auto p=base+4+i*16,q=p+16;
  const double first=r.f32(p),last=r.f32(q),t=frame;
  if(t<first || t>last)continue;
  const double u=(t-first)/(last-first),v=1-u;
  return float((1+2*u)*v*v*r.f32(p+4)+u*u*(3-2*u)*r.f32(q+4)+
      (last-first)*(u*v*v*r.f32(p+8)-u*u*v*r.f32(q+12)));
 }
 return 0;
}
bool focusMatches(float actual,float oracle) {
 // The oracle evaluates the mathematical polynomial in double; the GC
 // source evaluates each basis/product in float. Several actual far-depth
 // keys are thousands of units, so fixed .0005 is smaller than one ULP.
 const float ulp=std::nextafter(oracle,INFINITY)-oracle;
 return std::abs(actual-oracle)<=std::max(.0005f,4.f*ulp);
}
unsigned cutStart(const EventData &data,unsigned cut) {
 unsigned frame=0;for(unsigned i=0;i<cut;++i)frame+=data.cutFrames().at(i);return frame;
}
void check(const std::string &root,const char *name,unsigned assets,unsigned packets,unsigned frames,unsigned cuts) {
 OriginalEvent event(root,name,0);std::cout<<event.describe()<<'\n';
 require(event.data().assets().size()==assets,"Original asset directory count");
 require(event.data().packets().size()==packets,"Original executable packet count");
 require(event.totalFrames()==frames && event.data().cutFrames().size()==cuts,"Original camera durations/cut count");
 if(std::string(name)=="r100s40")require(event.hiddenRoomObjects()==std::vector<unsigned>{0x44},"R100StartEvent hides the exact original scroll object0x44");
 else require(!event.hiddenRoomObjects().empty(),"r120 hides replaced source room scroll models");
 unsigned observedCuts=0,voice=0,subtitles=0,focusFrames=0,visibleFrames=0;
 std::vector<unsigned> playedCuts;
 const bool drive=std::string(name)=="r120s00",exit=std::string(name)=="r120s01";
 while(!event.finished()) {
  const auto cut=event.sourceCut(),frame=event.cutFrame(),total=event.sourceFrame();
  const auto camera=event.camera();require(camera.valid,"Every original opening cut has a camera");
  for(const auto *v:{&camera.eye,&camera.target})for(float value:*v)require(std::isfinite(value),"Authored opening camera is finite");
  require(std::isfinite(camera.roll) && camera.fovRadians>0 && camera.fovRadians<3.1415927f,"Original roll/FOV valid");
  const auto fade=event.fadeAlpha();const auto focus=event.focus();
  event.tick30Hz(false);
  require(event.sourceFrame()==total && event.sourceCut()==cut && event.camera().eye==camera.eye && event.fadeAlpha()==fade,
          "Pause keeps event camera/source clock/fade unchanged");
  require(event.focus().nearEnabled==focus.nearEnabled && event.focus().farDepth==focus.farDepth,"Pause preserves focus curve requests");
  if(frame==0) {++observedCuts;playedCuts.push_back(cut);}
  if(drive) {
   require(event.drawsRoomBackground()==(cut!=0),"DPF_SCR suppresses the room only in original s00 cut0 (objTrans gate)");
   unsigned expected=0;
   if(cut==0)expected=frame<120?255:frame<180?255u*(180-frame)/60u:0;
   else if(cut==8)expected=frame<50?0:255u*(frame-50)/60u;
   require(fade==expected,"Original driving callback FadeControl integer alpha");
   if(cut==1 || cut==8) {
    const auto filename=std::string("event/r120/s00/etc/s00_")+(cut==1?"001":"008")+".lit";
    const auto raw=event.data().asset(filename);const AssetReader r{raw};
    const auto firstCut=r.u32(4),count=r.u32(firstCut+4);
    bool foundLight=false;
    for(unsigned index=0;index<count;++index) {
     const auto offset=firstCut+260+index*300;if(raw.at(offset+29)!=1)continue;
     const auto part=r.u32(offset+32)>>16;
     const auto &owner=find(event,"obm3000c");
     const auto expected=TransformPoint(WeaponAttachmentMatrix(owner.rig()->jointMatrices().at(part),owner.worldMatrix()),
                                      {r.f32(offset+4),r.f32(offset+8),r.f32(offset+12)});
     const auto light=event.lighting()->cuts.at(0).lights.at(index);
     require(light.parent==0,"Original event object light is submitted in resolved world coordinates");
     for(unsigned axis=0;axis<3;++axis)require(std::abs(light.position[axis]-expected[axis])<.005f,"Kind1 light follows actual obm3000c parent part");
     foundLight=true;break;
    }
    require(foundLight,"Actual drive cuts contain source kind1 attached light");
   }
   if(frame==0) {
    require(find(event,"pl0010").lightMask()==0x20,"Source rear mirror Leon light mask0x20");
    require(find(event,"pl0010").visible()==(cut==0),"Original mirror actor display callback");
   }
  } else if(exit) {
   require(event.drawsRoomBackground(),"r120s01 keeps room geometry except explicit r120_setTrans hidden groups");
   unsigned expected=total<150?255u*(150-total)/150u:0;
   if(cut==14 && frame>=370)expected=255u*(frame-370)/215u;
   require(fade==expected,"Original car stop/exit callback fades");
   if(cut==3 || cut==13) {
    ++focusFrames;require(focus.nearEnabled && focus.farEnabled,"Both actual source FocusMove requests execute");
    const auto asset=std::string("event/r120/s01/etc/s01_")+(cut==3?"003":"013")+".fcs";
    const auto bytes=event.data().asset(asset);const AssetReader r{bytes};
    require(focusMatches(focus.nearDepth,focusOracle(bytes,0,frame)),"Near focus follows raw Hermite key oracle");
    require(focusMatches(focus.farDepth,focusOracle(bytes,0x404,frame)),"Far focus follows raw Hermite key oracle");
    require(focus.nearLevel==r.f32(0x808) && focus.farLevel==r.f32(0x80c),"Original near/far blur levels");
   } else require(!focus.nearEnabled && !focus.farEnabled,"Camera cuts release explicit Filter01 requests");
   if(frame==0)require(find(event,"evm0000").lightMask()==0x20,"Source villagers select the special0x20 lights");
  } else require(fade==(total<30?255u*(30-total)/30u:0),"Original r100 entry fade-in30");
  for(const auto *window:{"obm3000a","obm3000b","obm3000c","obm3000d","obm3000e","obm3000f"})
  {
   require(find(event,window).lightMask()==2,"EventCarInit source window masks");
   require(find(event,window).depthMode()==1,"EventCarInit source transparent windows keep depth testing without depth writes");
  }
  if(drive || exit) {
   const auto &mirror=find(event,"obm3010f");
   require(mirror.depthMode()==1,"Original mirrored glass callback depth mode1");
   const auto alpha=exit && cut==6?192.f/255.f:1.f;
   require(mirror.materialColor()[3]==alpha,"Original s01 glass alpha192 cut6 and restored255 cut7");
  }
  const auto &car=find(event,"obm1a00");
  if(drive || exit)for(const auto *police:{"pl0700","pl0700a"}) {
    require(find(event,police).lightMask()==0x40,"Original SetObj18 PLXX policemen receive NPC mask0x40 in every camera cut");
    require(find(event,police).globalIlluminationTexture()>=0,"Original EventCarInit enables CORE global illumination on both policemen");
  }
  require(car.rig()!=nullptr,"Actual police car has the original posed skeleton");
  for(unsigned part:{0x12u,0x13u,0x14u,0x15u,0x16u,0x17u,0xfu})
   require(car.rig()->partScale(part)==std::array<float,3>{0,0,0},"EventCarInit hides the actual car model subparts by original scale");
  for(const auto &actor:event.actors())if(actor->visible()) {
   ++visibleFrames;require(actor->mesh().fallbackTriangles==0 && actor->mesh().unresolvedAlphaTriangles==0,"All displayed event TPL/materials resolve");
   for(float v:actor->worldMatrix())require(std::isfinite(v),"Displayed event owner placement finite");
   if(actor->rig())for(const auto &bone:actor->rig()->bones())for(float value:bone)require(std::isfinite(value),"Actual opening original motion pose finite");
  }
  for(const auto &cue:event.takeAudioCues()) {
   require(cue.stream && cue.block==1 && cue.synchronized,"Opening uses the authored event speech stream");
   require(cue.number==(drive?30u:exit?31u:15u) && cue.sourceFrame==0,"Exact source opening DSP stream identity");
   const auto stream=event.audioStream(cue);require(stream && !stream->left.empty(),"Actual source opening DSP has decoded samples");++voice;
  }
  if(event.subtitle().remainingFrames && event.subtitle().message>=0) {require(!event.subtitle().words.empty(),"Actual opening subtitle MDT decoded");++subtitles;}
  event.tick30Hz();
 }
 require(observedCuts==cuts && playedCuts.front()==0 && playedCuts.back()==cuts-1,"Every authored opening camera cut executes");
 require(voice==1 && subtitles>0 && visibleFrames>0,"Opening speech, MDT and original models all execute");
 require(focusFrames==(exit?130u:0u),"Exact original focus cut durations");
 require(event.unsupported().empty(),"No actual opening EVD packets silently skipped");
 event.reset();require(event.sourceFrame()==0 && event.camera().valid && !event.finished(),"Opening reset restores original first camera");
 event.finish();require(event.finished() && event.takeAudioCues().empty(),"Skip cancels opening speech once");
 event.reset();for(unsigned i=0;i<cutStart(event.data(),cuts/2)+3;++i)event.tick30Hz();
 event.finish();require(event.finished(),"Mid-cut source skip completes bounded remaining packets");
 OriginalEvent english(root,name,1);
 while(!english.finished()) {
  require(english.subtitle().words.empty() && english.subtitle().glyphs.empty(),"Original language1 suppresses message lookup as well as caption draw");
  english.tick30Hz();
 }
 std::cout<<name<<" full original opening PASS; frames="<<frames<<", cuts="<<cuts<<", visible actor frames="<<visibleFrames<<", focus="<<focusFrames<<'\n';
}
}
int main(int argc,char **argv) {
 try {
  if(argc!=2)throw std::runtime_error("Usage: m37_opening_event_check DISC_FILES");
  check(argv[1],"r120s00",94,124,1970,9);
  check(argv[1],"r120s01",120,142,2360,15);
  check(argv[1],"r100s40",89,102,1175,8);
  std::cout<<"M37 actual opening event PASS: "<<checks<<" checks\n";return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
