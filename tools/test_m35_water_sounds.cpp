#include "archive.h"
#include "asset_reader.h"
#include "collision_query.h"
#include "environment.h"
#include "sound_bank.h"
#include "sound_footsteps.h"
#include "sound_mixer.h"
#include "walk_probe.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks;
void require(bool ok,const char *message) {++checks;if(!ok)throw std::runtime_error(message);}
void u16(std::vector<std::uint8_t>&b,unsigned p,unsigned v) {b.at(p)=std::uint8_t(v>>8);b.at(p+1)=std::uint8_t(v);}
void f32(std::vector<std::uint8_t>&b,unsigned p,float f) {std::uint32_t v;std::memcpy(&v,&f,4);b.at(p)=std::uint8_t(v>>24);b.at(p+1)=std::uint8_t(v>>16);b.at(p+2)=std::uint8_t(v>>8);b.at(p+3)=std::uint8_t(v);}
std::vector<std::uint8_t> floorFixture(unsigned shape=1) {
  std::vector<std::uint8_t>b(16+132);b[0]='F';b[1]='S';b[2]='E';u16(b,4,0x103);u16(b,6,1);
  const unsigned p=16,a=p+20;b[p]=1;b[a+1]=std::uint8_t(shape);f32(b,a+4,0);f32(b,a+8,1000);f32(b,a+12,100);
  const std::array<std::array<float,2>,4>points{{{-100,-100},{100,-100},{100,100},{-100,100}}};
  for(unsigned k=0;k<4;++k) {f32(b,a+16+k*8,points[k][0]);f32(b,a+20+k*8,points[k][1]);}
  if(shape==2) {f32(b,a+16,0);f32(b,a+20,0);}
  b[p+68]=2;b[p+69]=3;b[p+71]=3;return b;
}
EnvironmentData water(unsigned generator=0x42) {
  EnvironmentData e;e.room=0x10a;WaterGrid g;g.generator=generator;g.width=4;g.height=4;g.size=100;g.originalSize=100;g.heightRate=1;g.position={0,1000,0};g.bounded=true;g.followCamera=false;e.grids.push_back(g);return e;
}
void contracts() {
  const auto left=SourceFootSoundCue(1,0);require(left.size()==2&&left[0].block==1&&left[0].number==0x2a&&left[0].part==0&&left[1].block==5&&left[1].number==0&&left[1].part==0x15,"walk SEQ clothing then actual left foot");
  const auto right=SourceFootSoundCue(4,0);require(right.size()==2&&right[0].number==0x2f&&right[1].part==0x19,"run SEQ actual right foot");
  const auto landing=SourceFootSoundCue(5,0);require(landing.size()==1&&landing[0].block==5&&landing[0].number==4&&landing[0].part==0,"landing authored root SEQ");
  for(unsigned sub:{1,2,3,4,5,6,7}) {const auto cue=SourceFootSoundCue(1,sub);require(cue.size()==1&&cue[0].part==0x15&&cue[0].block==(sub==1?1u:sub==2?6u:sub==3?2u:5u),"SEQ Free overrides keep actual part");}
  require(SourceFootSoundCue(0,0).empty(),"no SEQ transition means no fabricated cadence");
  auto e=water();const auto edge=QueryEnvironmentWaterContact(e,{200,909,200});require(edge&&edge->footSound(),"inclusive source grid XZ edge");
  require(!QueryEnvironmentWaterContact(e,{200.001,909,200}),"outside bounded water has no contact");
  const auto boundary=QueryEnvironmentWaterContact(e,{0,910,0});require(boundary&&!boundary->footSound(),"strict original90 immersion boundary");
  require(QueryEnvironmentWaterContact(e,{0,909.99,0})->footSound(),"over90 immersion enables wet cue");
  require(!QueryEnvironmentWaterContact(e,{0,std::numeric_limits<double>::infinity(),0}),"nonfinite contact rejected");
  for(unsigned room:{0x102,0x112,0x222,0x303,0x31a,0x31b}) {auto disabled=e;disabled.room=std::uint16_t(room);require(!QueryEnvironmentWaterContact(disabled,{0,0,0}),"authored room NoWater disables physical contact");}
  auto mix=e;auto low=water(0x45).grids[0];low.position[1]=500;mix.grids.push_back(low);require(QueryEnvironmentWaterContact(mix,{0,0,0})->height==1000,"bounded45 retains maximum42 height");
  mix.grids[1].bounded=false;require(QueryEnvironmentWaterContact(mix,{0,0,0})->height==500,"unbounded45 overrides42 as source");
  auto follow=water(0x45);follow.grids[0].followCamera=true;require(!QueryEnvironmentWaterContact(follow,{1000,0,2000}),"follow grid needs original camera target");require(QueryEnvironmentWaterContact(follow,{1000,0,2000},{1000,0,2000})->height==1000,"source follow-camera water collision");
  auto b=floorFixture();FootstepSounds floor(b);const SoundPoint inside{0,0,0};auto choice=floor.resolve(0,inside);require(choice.number==60&&choice.area==0&&choice.effect==3&&!choice.water,"FSE source sound/effect fields");
  require(floor.resolve(0,{-100,-400,-100}).area==0,"FSE inclusive lower y and quad edge");require(floor.resolve(0,{0,700,0}).area==-1,"FSE exclusive upper y includes source300 shift");
  require(floor.resolve(0,{101,0,0}).number==0,"dry default is source zero variant");
  auto wet=water();wet.grids[0].position[1]=1000;choice=floor.resolve(0,inside,&wet);require(choice.number==30&&choice.water&&choice.area==-1,"water precedes ordinary FSE");
  choice=floor.resolve(16,inside,&wet);require(choice.number==76&&!choice.water&&choice.area==0,"source16..19 query use2 before water");
  b[16+71]=1;require(FootstepSounds(b).resolve(16,inside,&wet).number==16,"NPC branch has no water/default fallback");
  b[16+3]=2;require(FootstepSounds(b).resolve(0,inside).area==-1&&FootstepSounds(b).resolve(0,inside,nullptr,{},2).area==0&&FootstepSounds(b).resolve(0,inside,nullptr,{},0xff).area==0,"FSE source groups and any-group");
  b[16]=0;require(FootstepSounds(b).resolve(0,inside,nullptr,{},2).area==-1,"disabled FSE record ignored");
  FootstepSounds circle(floorFixture(2));require(circle.resolve(0,{99.99,0,0}).area==0&&circle.resolve(0,{100,0,0}).area==-1,"source strict cylinder radius");
}
std::vector<std::uint8_t> copy(const RoomArchive &room,const char *tag) {const auto *entry=room.find(tag);if(!entry)throw std::runtime_error(std::string("missing ")+tag);return room.copy(std::size_t(entry-room.entries().data()));}
void real(const std::filesystem::path &directory,const std::string &corePath,const std::string &audioPath) {
  unsigned rooms=0,areas=0;
  for(const auto &entry:std::filesystem::directory_iterator(directory)) {
    const auto name=entry.path().stem().string();if(name.size()!=4||name[0]!='r'||entry.path().extension()!=".dat")continue;
    const auto room=RoomArchive::Load(entry.path().string());FootstepSounds sounds;
    try {sounds=FootstepSounds(room);}catch(const std::exception &e) {throw std::runtime_error(name+": "+e.what());}
    areas+=unsigned(sounds.areaCount());++rooms;
    require(sounds.resolve(0,{0,0,0}).number<=7650,"all room FSE fields bounded");
  }
  require(rooms==127,"all127 original room FSE blocks checked");
  const auto room=RoomArchive::Load((directory/"r10a.dat").string()),core=RoomArchive::Load(corePath);
  const Smd smd(copy(room,"SMD"));const Smx smx(copy(room,"SMX"));const auto env=BuildRoomEnvironment(room,smd,smx,0x10a,copy(core,"EFF"));
  require(env.grids.size()==1&&env.grids[0].generator==0x42,"actual r10a original physical water42");
  const Sat sat(copy(room,"SAT"));const SatCollision collision(sat);WalkConfig config;config.originalScenerySphere=true;WalkProbe probe(collision,config);FootstepSounds sounds(room);
  unsigned wetFloors=0,walkable=0;SoundPoint feet{};EnvironmentWaterContact contact{};unsigned polygon=0;
  for(unsigned i=0;i<sat.floor+sat.slope;++i) {
    const auto triangle=collision.triangle(i);SoundPoint centroid{};for(const auto &v:triangle)for(unsigned k=0;k<3;++k)centroid[k]+=v[k]/3;
    const auto floor=collision.ground(centroid,250,250);if(!floor)continue;
    const auto found=QueryEnvironmentWaterContact(env,floor->point);if(!found||!found->footSound())continue;
    ++wetFloors;if(!probe.place(floor->point,250))continue;++walkable;
    if(walkable==1) {feet=probe.position();contact=*QueryEnvironmentWaterContact(env,feet);polygon=unsigned(probe.floorHit()->polygon);}
  }
  require(wetFloors&&walkable,"actual r10a SAT has physically walkable immersed river floor");
  for(unsigned no=0;no<=5;++no) {const auto selected=sounds.resolve(no,feet,&env);require(selected.water&&selected.number==no+30,"walk run knee/body landing use exact original wet variant");}
  const SoundPoint aerial{feet[0],contact.height+1000,feet[2]};require(!sounds.resolve(4,aerial,&env).water,"jump above water cannot play wet landing early");
  const SoundPoint threshold{feet[0],contact.height-90,feet[2]};require(!sounds.resolve(4,threshold,&env).water,"real water strict90 exact boundary");
  auto dry=feet;dry[0]=env.grids[0].position[0]-env.grids[0].width*env.grids[0].size/2-1000;require(!sounds.resolve(0,dry,&env).water,"outside original r10a grid stays dry");
  const auto banks=SoundBank::LoadDvd(audioPath);auto bank=std::find_if(banks.begin(),banks.end(),[](const auto &b){return b->block()==5;});require(bank!=banks.end(),"original r10a owns foot soundbank5");
  SoundMixer mixer;mixer.setPaused(false);mixer.installBanks(banks,true);std::vector<std::int16_t>pcm(8192*2);
  for(unsigned no=30;no<=35;++no) {
    const auto *cue=(*bank)->cue(no);require(cue,"wet walk/run/landing cue exists in original r10a DAS");
    const auto handle=mixer.play(5,no,1,-1,false);require(handle!=0,"original wet cue accepted by mixer");mixer.render(pcm.data(),8192);
    require(std::any_of(pcm.begin(),pcm.end(),[](auto v){return v!=0;}),"original wet cue produces audible PCM");mixer.stop(handle);
  }
  std::cout<<"Catalogue="<<rooms<<", original FSE areas="<<areas<<"; r10a wet floor centroids="<<wetFloors<<", walkable="<<walkable<<"\n";
  std::cout<<"Actual r10a wet contact polygon="<<polygon<<", feet=("<<feet[0]<<","<<feet[1]<<","<<feet[2]<<"), water="<<contact.height<<", depth="<<contact.depth<<"; wet cues30..35 audible\n";
}
}
int main(int argc,char **argv) {try {if(argc!=4)throw std::runtime_error("Usage: m35_water_sounds_check DECODED_ROOMS CORE_DAT R10A_DAS");contracts();real(argv[1],argv[2],argv[3]);std::cout<<"M35 original water/foot sounds PASS: "<<checks<<" checks\n";}catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}}
