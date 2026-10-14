#!/usr/bin/env python3
"""Run unchanged original SEQ/FSE/footSeCheck routines against the host port.
Sound-device, particle and game-status side effects are isolated host stubs.
Usage: python3 tools/check_foot_sound_reference.py --build HOST_BUILD --rooms DECODED_ROOMS
"""
import argparse
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--build', type=pathlib.Path, required=True)
parser.add_argument('--rooms', type=pathlib.Path, required=True)
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]

def routine(path, signature):
    source = (root / path).read_text()
    start = source.index(signature)
    begin = source.index('{', start)
    depth, end = 1, begin + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

pre = r'''
#include "sound_footsteps.h"
#include "archive.h"
#include "asset_reader.h"
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
using f32=float;using u8=std::uint8_t;using s8=std::int8_t;
using u16=std::uint16_t;using s16=std::int16_t;using u32=std::uint32_t;using BOOL=int;
struct Vec {float x{},y{},z{};};using Mtx=float[3][4];
struct Logger {template<class...T>void warn(T...) {}} logger,*pLog=&logger;
constexpr unsigned STA_PL_SE_FOOT=0,STA_MOVIE_ON=0;
constexpr char AREA_TYPE_ERR[]="area";
#define StaFlagOff(...) ((void)0)
#define StaFlagOn(...) ((void)0)
#define StaFlagChk(...) false
#define SQRTF(v) std::sqrt(v)
struct Game {unsigned Rno0=3;} game,*pG=&game;
struct cPlayer {
  struct {struct {u8 Se{},Free{};}Seq_old;float Seq_frame{};}Motion;
  struct Part {Vec world;}parts[26],*pList=&parts[0];
  unsigned id{};Vec pos{};
  Part *getPartsPtr(unsigned part) {return &parts[part];}
  void seqSeCtrl();
} player,*pPL=&player;
std::vector<re4::PlayerSequenceSoundCue> calls;
void SndCall(unsigned block,unsigned number,Vec *pos,unsigned,unsigned,unsigned) {calls.push_back({block,number,unsigned(pos->x)});}
void AddSandPower(Vec,float) {}
void AddWaterPower(Vec,float) {}
const re4::EnvironmentData *waterEnvironment=nullptr;
int GetWaterHeight(Vec *p,float *height) {if(!waterEnvironment)return 0;const auto hit=re4::QueryEnvironmentWaterContact(*waterEnvironment,{p->x,p->y,p->z});if(!hit)return 0;*height=hit->height;return 1;}
unsigned effect{};
void EspFootCall(unsigned,unsigned e,Vec *) {effect=e;}
void seRandomCheck(int,u16 *) {}
int sndExistCheck(int,unsigned) {return 1;}
void sndCallErr(int,unsigned) {}
constexpr unsigned FOOT_SE_NUM=30;
'''
headers = ''
for path in ('upstream/re4/include/area.h', 'upstream/re4/include/flr_at.h'):
    headers += '\n'.join(line for line in (root / path).read_text().splitlines()
                         if not line.startswith('#include')) + '\n'
pre += headers + '\nFLR_AT_SYS floorSystem{},*pFlrSys=&floorSystem;\n'
bodies = '\n'.join((
    routine('upstream/re4/src/game/pl_class.cpp', 'void cPlayer::seqSeCtrl()'),
    routine('upstream/re4/src/game/area.cpp', 'BOOL areaHitCheck_xz4('),
    routine('upstream/re4/src/game/area.cpp', 'BOOL areaHitCheck_Cylinder('),
    routine('upstream/re4/src/game/area.cpp', 'BOOL AreaHitCheck('),
    routine('upstream/re4/src/game/flr_at.cpp', 'FLR_AT_DATA* FlrAtCheck('),
    routine('upstream/re4/src/game/esp_app.cpp', 'int EspPlWaterCall('),
    routine('upstream/re4/src/game/snd.cpp', 'int footSeCheck('),
))
post = r'''
std::uint64_t checks{};
void require(bool ok,const char *message) {++checks;if(!ok)throw std::runtime_error(message);}
void same(unsigned no,Vec point,const re4::FootstepSounds &port) {
  effect=0;u16 ref=u16(no);footSeCheck(&ref,&point);
  const auto result=port.resolve(no,{point.x,point.y,point.z},waterEnvironment,{},floorSystem.cur_group);
  require(result.number==ref,"original/port foot cue mismatch");
  require(result.effect==effect,"original/port foot effect selection mismatch");
}
void floors(const std::filesystem::path &directory) {
  unsigned rooms=0;
  for(const auto &entry:std::filesystem::directory_iterator(directory)) {
    const auto name=entry.path().stem().string();if(name.size()!=4||name[0]!='r'||entry.path().extension()!=".dat")continue;
    ++rooms;const auto room=re4::RoomArchive::Load(entry.path().string());re4::FootstepSounds port(room);
    const auto *block=room.find("FSE");std::vector<FLR_AT_DATA> data;FLR_AT_HEADER header{};
    if(block) {
      const auto bytes=room.copy(std::size_t(block-room.entries().data()));const re4::AssetReader r{bytes};
      if(r.u32(0)==0x46534500 && r.u16(4)==0x103) {
        header.num=r.u16(6);data.resize(header.num);
        for(unsigned i=0;i<data.size();++i) {
          auto &a=data[i];const unsigned p=16+i*132,q=p+20;a.flag=bytes[p];a.type=bytes[p+1];a.group=bytes[p+3];
          a.area.type=bytes[q+1];a.area.xz4.floor=r.f32(q+4);a.area.xz4.height=r.f32(q+8);a.area.xz4.radius=r.f32(q+12);
          for(unsigned k=0;k<4;++k) {a.area.xz4.p[k].x=r.f32(q+16+k*8);a.area.xz4.p[k].z=r.f32(q+20+k*8);}
          a.se.se_type=bytes[p+68];a.se.eff_type=bytes[p+69];a.se.use_kind=bytes[p+71];
        }
      }
    }
    floorSystem={};floorSystem.pHead=&header;floorSystem.pData=data.data();
    waterEnvironment=nullptr;
    for(const auto &a:data) {
      if(a.type || (a.area.type!=1&&a.area.type!=2))continue;
      Vec center{};center.y=a.area.xz4.floor-300;
      if(a.area.type==1)for(const auto &p:a.area.xz4.p) {center.x+=p.x*.25f;center.z+=p.z*.25f;}
      else {center.x=a.area.cylinder.x;center.z=a.area.cylinder.z;}
      std::vector<Vec> points{center};
      for(float y:{a.area.xz4.floor-400,a.area.xz4.floor-400.01f,a.area.xz4.floor+a.area.xz4.height-300,a.area.xz4.floor+a.area.xz4.height-300.01f}) {auto p=center;p.y=y;points.push_back(p);}
      if(a.area.type==1)for(const auto &p:a.area.xz4.p)points.push_back({p.x,center.y,p.z});
      else {auto p=center;p.x+=a.area.cylinder.radius;points.push_back(p);p.x-=.01f;points.push_back(p);}
      for(unsigned group:{0u,unsigned(a.group),0xffu}) {
        floorSystem.cur_group=u8(group);
        for(const auto &p:points)for(unsigned no:{0u,1u,2u,3u,4u,5u,13u,14u,16u,17u,18u,19u,20u})same(no,p,port);
      }
    }
    same(0,{0,0,0},port);
  }
  require(rooms==127,"all127 source FSE rooms");
  re4::EnvironmentData water;water.room=0x10a;re4::WaterGrid grid;grid.generator=0x42;grid.width=120;grid.height=120;grid.size=530;grid.heightRate=1;grid.position={45151.34f,-9200.079f,98120.33f};water.grids.push_back(grid);waterEnvironment=&water;
  floorSystem={};re4::FootstepSounds port;
  for(float y:{grid.position[1]-90,grid.position[1]-90.001f,grid.position[1]-1000,grid.position[1]+1000})
    for(unsigned no=0;no<30;++no)same(no,{grid.position[0],y,grid.position[2]},port);
}
int main(int argc,char **argv) {try {
  if(argc!=2)throw std::runtime_error("decoded room directory required");
  for(unsigned part=0;part<26;++part)player.parts[part].world.x=float(part);
  for(unsigned se=0;se<256;++se)for(unsigned free=0;free<256;++free) {
    calls.clear();player.Motion.Seq_old={u8(se),u8(free)};player.seqSeCtrl();const auto actual=re4::SourceFootSoundCue(se,free);
    require(calls.size()==actual.size(),"original/port SEQ cue count");
    for(unsigned i=0;i<calls.size();++i)require(calls[i].block==actual[i].block&&calls[i].number==actual[i].number&&calls[i].part==actual[i].part,"original/port SEQ cue ordering or part");
    require(player.Motion.Seq_old.Se==0,"original source cue consumed once");
  }
  floors(argv[1]);std::cout<<"Unchanged original SEQ/FSE/footSeCheck oracle PASS: "<<checks<<" comparisons\n";
}catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}}
'''
with tempfile.TemporaryDirectory(prefix='m35-foot-source-') as temporary:
    work = pathlib.Path(temporary)
    source, executable = work / 'reference.cpp', work / 'reference'
    source.write_text(pre + bodies + post)
    subprocess.run(['c++', '-std=c++17', '-O2', '-ffp-contract=off', '-I', str(root/'app/src/main/cpp'),
                    str(source), str(root/'app/src/main/cpp/sound_footsteps.cpp'),
                    str(root/'app/src/main/cpp/environment.cpp'), '-Wl,--start-group',
                    str(args.build/'libre4_platform.a'), str(args.build/'libre4_motion64.a'),
                    '-Wl,--end-group', '-pthread', '-o', str(executable)], check=True)
    subprocess.run([str(executable), str(args.rooms)], check=True)
