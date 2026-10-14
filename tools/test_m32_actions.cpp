// Real, separately supplied disc data; independent raw big-endian FCV
// interpolation is the reference for the newly hosted event camera tracks.
#include "animation.h"
#include "asset_reader.h"
#include "archive.h"
#include "destruction_effects.h"
#include "leon_preview.h"
#include "room_interactions.h"
#include "walk_probe.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks;
void check(bool v,const char *s){if(!v)throw std::runtime_error(s);++checks;}
std::vector<std::uint8_t> sub(const re4::RoomArchive&a,const char *tag){const auto*e=a.find(tag);check(e,"Required real disc subfile");return a.copy(e-a.entries().data());}
struct Key {unsigned frame;float value,in,out;};
using Track=std::array<std::vector<Key>,3>;
Track track(const std::vector<std::uint8_t>&bytes,unsigned channel,unsigned part){
 re4::AssetReader r{bytes};const unsigned count=bytes.at(2),table=(3+count*3+3)&~3u;
 static const unsigned widths[16][3]={{4,4,4},{4,2,2},{4,1,1},{},{2,4,4},{2,2,2},{2,1,1},{},{1,4,4},{1,2,2},{1,1,1},{},{},{},{},{4,0,0}};
 for(unsigned i=0;i<count;++i){const unsigned kind=r.u16(3+i*2);if(((kind>>8)&15)!=channel||bytes.at(3+count*2+i)!=part||(kind&255)!=4)continue;
  std::size_t p=r.u32(table+4+i*4);Track result;
  for(auto&axis:result){const unsigned n=r.u16(p);p+=2;std::vector<unsigned> frames;for(unsigned k=0;k<n;++k){frames.push_back(r.u16(p));p+=2;}
   for(unsigned k=0;k<n;++k){float values[3]{};for(unsigned j=0;j<3;++j){const unsigned width=widths[kind>>12][j];if(width==4)values[j]=r.f32(p);else if(width==2)values[j]=std::int16_t(r.u16(p))*.0001f;else if(width==1)values[j]=std::int8_t(bytes.at(p))*.0001f;p+=width;}axis.push_back({frames[k],values[0],values[1],values[2]});}
  }return result;
 }throw std::runtime_error("Original event camera track missing");
}
float sample(const std::vector<Key>&keys,float frame){
 if(frame<=keys.front().frame)return keys.front().value;
 if(frame>=keys.back().frame)return keys.back().value;
 auto b=std::upper_bound(keys.begin(),keys.end(),frame,[](float f,const Key&k){return f<k.frame;});const auto&a=*(b-1);
 const float t=(frame-a.frame)/float(b->frame-a.frame),t2=t*t,t3=t*t2,h01=-(t3+t3)+3*t2,h11=t3-t2,h10=h11-t2+t;
 return a.value*(1-h01)+b->value*h01+a.out*h10+b->in*h11;
}
void camera(const re4::RoomArchive&room,const char *pl){
 re4::RoomObjectArchive etm(sub(room,"ETM"));const auto player=re4::RoomArchive::LoadDrs(pl);re4::SkinnedModel body(player.copy(0));
 for(const char*name:{"pl00537.fcv","pl00538.fcv","pl00536.fcv"}){
  const auto bytes=etm.copy(name);const auto eye=track(bytes,6,0),target=track(bytes,6,1);
  re4::AnimationRig rig(body,std::vector<re4::AnimationBytes>{{bytes,{}}});rig.enableAttachedCamera(true);rig.startActionFlags(0,0,0,0x201);
  bool ended=false;
  for(unsigned tick=0;tick<180&&!ended;++tick){ended=rig.advanceRootAction().ended;const auto c=rig.attachedCamera();check(c.type==1&&c.hasEye&&c.hasTarget,"Source camera type and authored tracks retained");
   for(unsigned axis=0;axis<3;++axis){check(std::abs(c.eye[axis]-sample(eye[axis],rig.curveFrame()))<.02,"Camera eye equals independent raw FCV Hermite reference");check(std::abs(c.target[axis]-sample(target[axis],rig.curveFrame()))<.02,"Camera target equals independent raw FCV Hermite reference");}
   if(tick==10){const auto before=rig.attachedCamera();rig.appendActions({{bytes,{}}});const auto after=rig.attachedCamera();check(before.eye==after.eye&&before.target==after.target,"Appending a clip does not reset active event camera");}
  }check(ended,"All original window camera motions terminate");rig.enableAttachedCamera(false);check(!rig.attachedCamera().type,"Event camera releases on event completion");
 }
}
void item(const re4::RoomArchive&room,const char *pl,const char *wep){
 re4::Sat sat(sub(room,"SAT"));re4::SatCollision world(sat);re4::WalkConfig cfg;cfg.originalScenerySphere=true;cfg.stepUp=600;cfg.drop=999;re4::WalkProbe probe(world,cfg);check(probe.reset(),"Real floor for item-exam fixture");re4::LeonPreview player(pl,wep);player.setPlacement(probe.position(),1);
 const auto bones=player.rig().bones();const auto feet=player.feet();const auto frame=player.rig().sequenceFrame();check(player.beginItemExam(),"Footwork can enter source item-exam freeze");check(!player.beginItemExam(),"An active item-exam cannot be duplicated");
 re4::PlayerActionInput attack;attack.aimHeld=attack.fireHeld=attack.firePressed=attack.knifeHeld=true;
 for(unsigned tick=0;tick<45;++tick){player.step(probe,1,1,true,true,1./30,attack);check(player.itemExamActive()&&player.feet()==feet&&player.rig().bones()==bones&&player.rig().sequenceFrame()==frame&&player.shots()==0,"Item-exam owns input and leaves actor pose/root/attack unchanged");}
 player.endItemExam(false);check(!player.itemExamActive()&&player.feet()==feet,"Cancelling item-exam returns unchanged placement");check(player.beginItemExam(),"A later item-exam can start after cancellation");player.endItemExam(true);check(player.action()==re4::PlayerAction::Footwork,"Accepting restores normal actor state");
}
void glass(const re4::RoomArchive&room,const re4::RoomArchive&core){
 const auto data=re4::BuildDestructionEffects(room,sub(core,"EFF"),0x101);check(!data.meshes.empty(),"Actual EFM geometry decoded");
 unsigned accepted=0;
 for(unsigned object:{0u,7u,0x25u})for(unsigned sequence:{0u,1u,3u,6u,7u}){
  re4::DestructionEffectsSimulation sim(data);const bool available=!(sequence==3&&object!=0);const bool ok=sim.triggerWindow(object,sequence,3,{120,350,700},{0,.5,0},99,12345);check(ok==available,"Only EST actually present on original window owner activates");if(!ok)continue;++accepted;
  check(!sim.triggerWindow(object,sequence,3,{120,350,700},{0,.5,0},99,12345),"Duplicate break event cannot repeat shard burst");check(!sim.instances().empty(),"Break construction emits original model fragments immediately");
  const auto first=sim.state();re4::DestructionEffectsSimulation resumed(data);check(resumed.restore(first),"Real shard state restores including pending timed records");
  if(object==0&&sequence==3){check(first.pending.size()==1&&first.pending.front().remaining==24,"Original falling-vault late glass generator keeps Set_time24");for(unsigned tick=0;tick<23;++tick){sim.tick();resumed.tick();check(sim.state().pending.size()==1&&sim.state().pending.front().remaining==23-tick,"Late generator does not appear before source counter");}sim.tick();resumed.tick();check(sim.state().pending.empty(),"Late original generator starts at source counter24");}
  for(unsigned tick=0;tick<80;++tick){sim.tick();resumed.tick();const auto a=sim.instances(),b=resumed.instances();check(a.size()==b.size(),"Restored shard population evolves identically");for(unsigned i=0;i<a.size();++i)check(a[i].matrix==b[i].matrix&&a[i].color==b[i].color,"Restored authored shard transforms/colors remain deterministic");}
 }
 check(accepted==13,"All thirteen actual event/weapon window sequences checked");
}
}
int main(int argc,char**argv){try{if(argc!=5)throw std::runtime_error("Usage: m32_actions_check R101 PL00 WEP01 CORE");const auto room=re4::RoomArchive::Load(argv[1]),core=re4::RoomArchive::LoadDrs(argv[4]);camera(room,argv[2]);item(room,argv[2],argv[3]);glass(room,core);std::cout<<"M32 action real asset checks PASS: "<<checks<<'\n';return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
