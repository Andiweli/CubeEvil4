#include "room_effect_cloth.h"
#include "archive.h"
#include "asset_reader.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks;
void check(bool v,const char *s){++checks;if(!v)throw std::runtime_error(s);}
std::vector<re4::WaterRecord> records(const std::string &path) {
  const auto room=re4::RoomArchive::Load(path);const auto *entry=room.find("EFF");
  if(!entry)throw std::runtime_error("EFF missing");
  const auto bytes=room.copy(entry-room.entries().data());const re4::AssetReader r{bytes};
  const auto ids=r.u32(12),table=r.u32(36);std::vector<re4::WaterRecord> out;
  for(unsigned i=0;i<r.u32(ids);++i)if(r.u16(ids+4+i*8)==1) {
    const auto head=table+r.u32(table+4+i*4);
    for(unsigned j=0;j<r.u16(head);++j) {
      re4::WaterRecord record;r.range(head+48+j*300,300);std::copy_n(bytes.data()+head+48+j*300,300,record.bytes.data());
      if(record.bytes[1]==0x4e)out.push_back(record);
    }
  }
  return out;
}
re4::WaterSprite sprite(const re4::WaterRecord &r,unsigned index) {
  std::vector<std::uint8_t> bytes(r.bytes.begin(),r.bytes.end());re4::AssetReader a{bytes};re4::WaterSprite s;
  s.target=1;s.record=index;s.serial=index+1;
  for(unsigned i=0;i<3;++i){s.position[i]=a.f32(12+i*4);s.angle[i]=a.f32(88+i*4)*.017453292f;}
  s.size={a.f32(136),a.f32(140)};
  for(unsigned i=0;i<4;++i)s.color[i]=r.bytes[156+i];
  s.startAlpha=s.color[3];return s;
}
template<class T>void write(std::ofstream &f,const T &v){f.write(reinterpret_cast<const char*>(&v),sizeof(v));}
void dumpState(std::ofstream &f,const re4::RoomEffectCloth &cloth) {
  const auto &s=cloth.state();
  write(f,s.time);write(f,s.time2);write(f,s.windTime);
  for(const auto *a:{&s.positions,&s.normals,&s.speeds})for(const auto &v:*a)for(auto x:v)write(f,x);
  for(auto x:s.placement)write(f,x);
  for(auto x:s.center)write(f,x);
  const auto mesh=cloth.mesh();write(f,std::uint32_t(mesh.vertices.size()));
  for(const auto &v:mesh.vertices) {
    for(auto x:v.position)write(f,x);
    for(auto x:v.normal)write(f,x);
    for(auto x:v.uv)write(f,x);
  }
}
}
int main(int argc,char **argv) {
  try {
    if(argc<2 || argc>3)throw std::runtime_error("Usage: room_cloth_check R100 [SOURCE_DUMP]");
    auto source=records(argv[1]);check(source.size()==3,"actual three SST1 sheets");
    std::vector<re4::WaterSprite> sprites;std::vector<re4::RoomEffectCloth> sheets;
    for(unsigned i=0;i<source.size();++i) {
      sprites.push_back(sprite(source[i],i));sheets.emplace_back(sprites.back(),source[i]);
      const auto &state=sheets.back().state();check(state.columns==9 && state.rows==10,"source grid9x10");
      check(state.pinFourRows && state.scale==60.857143f && !state.additive,"source pin/scale/blend");
      check(state.positions.size()==90 && state.normals[89]==std::array<float,3>{0,0,1},"source allocation/prewarm normal state");
      const auto mesh=sheets.back().mesh();check(mesh.vertices.size()==432,"original row-strip triangles");
      check(mesh.vertices.front().uv==std::array<float,2>{0,0} && mesh.vertices.back().uv==std::array<float,2>{1,1},"source full-sheet UVs");
      check(mesh.vertices.front().position==sprites.back().position,"source hanging point world placement");
      check(mesh.color==std::array<std::uint8_t,4>{255,255,255,255},"source Set initialwhite beforemove");
    }
    std::ofstream dump;
    constexpr unsigned Frames=40;
    if(argc==3) {
      dump.open(argv[2],std::ios::binary);write(dump,std::uint32_t(source.size()));write(dump,std::uint32_t(Frames));
      for(unsigned i=0;i<source.size();++i){dump.write(reinterpret_cast<const char*>(source[i].bytes.data()),300);dumpState(dump,sheets[i]);}
    }
    const auto initial=sheets[0].state();std::uint16_t seed=0x4e22;
    for(unsigned frame=0;frame<Frames;++frame) {
      for(unsigned i=0;i<source.size();++i) {
        check(!sheets[i].move(sprites[i],source[i],seed),"source infinite sheet stays alive");
        if(dump.is_open())dumpState(dump,sheets[i]);
      }
      if(dump.is_open())write(dump,seed);
    }
    const auto &state=sheets[0].state();
    check(state.positions!=initial.positions && state.speeds!=initial.speeds,"source waves animate actualsheet");
    for(unsigned i=0;i<36;++i)check(state.positions[i]==initial.positions[i],"source four pinned rows stay fixed");
    check(sheets[0].mesh().color==std::array<std::uint8_t,4>{105,100,90,255},"source authored clothmaterial copied");
    check(sprites[0].position==sprite(source[0],0).position && sprites[0].age==40,"source CommonMove positionreset/timeadvance");
    const auto mesh=sheets[0].mesh();const auto paused=sheets[0].state();
    check(mesh.vertices==sheets[0].mesh().vertices && paused.positions==sheets[0].state().positions,"paused mesh read doesnot tick");
    auto deadSource=source[0];deadSource.bytes[185]=1;auto dying=sprite(deadSource,0);re4::RoomEffectCloth dead(dying,deadSource);
    check(!dead.move(dying,deadSource,seed) && dead.move(dying,deadSource,seed) && dead.mesh().vertices.empty(),"source lifetime removes owned sheet");
    check(std::abs(re4::OriginalClothSin(1.f)-std::sin(1.f))<.00001f && re4::OriginalClothSin(1.f)!=std::sin(1.f),"originalpaired Taylor usesactualcoefficients");
    if(dump.is_open() && !dump)throw std::runtime_error("dump failed");
    std::cout<<"M37 actual r100 cloth: "<<checks<<" checks PASS; sourcegrid9x10x3\n";
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
