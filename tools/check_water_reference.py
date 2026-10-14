#!/usr/bin/env python3
"""Compare Espgen42/45 CPU waves with the unchanged original move routines.
Only PowerPC/GX address/cache/register operations are replaced with host stubs.
Usage: python3 tools/check_water_reference.py --build HOST_BUILD
"""
import argparse
import pathlib
import subprocess
import tempfile
import re
parser=argparse.ArgumentParser();parser.add_argument('--build',type=pathlib.Path,required=True);args=parser.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]
def routine(path,name):
    source=(root/path).read_text();start=source.index('void '+name+'(cEspgen* pGen)');begin=source.index('{',start);depth=1;end=begin+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    text=source[start:end]
    text=text.replace('(u8*) GXGetTexObjData(tex) + 0x80000000','(u8*) GXGetTexObjData(tex)')
    text=text.replace('asm("r0")','')
    text=re.sub(r'asm\([^;]*\);',';',text)
    return text
pre=r'''
#include "environment.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
using f32=float;using u8=std::uint8_t;using u16=std::uint16_t;using u32=std::uint32_t;
struct Vec {float x,y,z;};using Mtx=float[3][4];
struct ESPGEN45_WK {
  u16 Width{},Height{};unsigned Type{},wave_ratio_base{};
  float Prm_a{},Prm_dmp{},Size{},Base_y{};Vec Pos{};
  Mtx Wld_mat{},Inv_mat{};
  float *pPosBuf{},*pSpdBuf{};Vec *pHeightBuf{},*pNorBuf{};u8 *pTexBuf{};
};
struct ESP4C_WK {Vec ang{};unsigned wave_ratio_base{},Type{};float Prm_a{},Prm_dmp{};};
struct cEspgen {struct {ESPGEN45_WK storage;ESPGEN45_WK *buff=&storage;} Free;};
struct Game {unsigned Frame_cnt{};} game,*pG=&game;
struct JoyData {unsigned on{};} Joy[1];
struct GXTexObj {void *data{};} noiseTexture;
GXTexObj *EspGetTexObj(unsigned,unsigned) {return &noiseTexture;}
void *GXGetTexObjData(GXTexObj *t) {return t->data;}
constexpr unsigned DBG_IN_ESP_TOOL=0,STA_WATER_ALIVE=0;
int g_bTargetCamera=0,g_bTargetHeight=0,g_bSizeOverWrite=0,g_bSetParam=0;
float g_Target_x=0,g_Target_y=0,g_Target_z=0,g_Size=0,inv_mul=1;
ESP4C_WK g_Free;
#define DbgFlagChk(...) false
#define StaFlagOn(...) ((void)0)
#define PPCMtpmc1(...) ((void)0)
#define PPCMtpmc2(...) ((void)0)
#define PPCMtpmc3(...) ((void)0)
#define PPCMtpmc4(...) ((void)0)
#define PPCMtmmcr1(...) ((void)0)
#define PPCMtmmcr0(...) ((void)0)
#define PSQ_L_U8_TO(dst,p) ((dst)=float(*(p)))
#define DCStoreRange(...) ((void)0)
void PSVECNormalize(const Vec *a,Vec *b) {const float length=std::sqrt(a->x*a->x+a->y*a->y+a->z*a->z);*b={a->x/length,a->y/length,a->z/length};}
void PSVECScale(const Vec *a,Vec *b,float f) {*b={a->x*f,a->y*f,a->z*f};}
void PSMTXScale(Mtx,float,float,float) {}
void RotMatrix(Mtx,Vec *) {}
void PSMTXConcat(Mtx,Mtx,Mtx) {}
void PSMTXTransApply(Mtx,Mtx,float,float,float) {}
void PSMTXInverse(Mtx,Mtx) {}
'''
# Camera target references are unreachable with g_bTargetCamera=0, but C++ still
# needs their structure for compiling the original branch.
pre=pre.replace('struct Game {unsigned Frame_cnt{};}','struct Game {unsigned Frame_cnt{};struct {struct {Vec Target{};} param;} Camera;}')
post=r'''
unsigned checks=0;float maxError=0;
void equal(float a,float b) {++checks;maxError=std::max(maxError,std::abs(a-b));if(a!=b)throw std::runtime_error("original/port grid float mismatch");}
int main() {try {
  for(unsigned generator:{0x42u,0x45u})for(unsigned type=0;type<3;++type) {
    re4::EnvironmentData data;data.signature=type+generator;re4::WaterGrid g;
    g.generator=generator;g.width=type==2?40:16;g.height=16;g.type=type;g.size=530;g.heightRate=.0001f;
    g.damping=type==2?.025f:.05f;g.spread=type==2?.92f:.95f;data.grids={g};data.gridNoise.resize(60);
    for(unsigned f=0;f<60;++f) {data.gridNoise[f].resize(4096);for(unsigned k=0;k<4096;++k)data.gridNoise[f][k]=u8((k*31+f*17)&255);}
    re4::EnvironmentSimulation sim(data);const auto start=sim.state().grids.at(0);
    auto a=start.position,b=start.speed;std::vector<Vec> height(start.height.size()),normal(height.size());
    std::vector<u8> bump((g.width+1)*(g.height+1)*12);
    for(unsigned k=0;k<height.size();++k) {height[k].y=start.height[k];const auto n=start.normals[k];normal[k]={n[0],n[1],n[2]};}
    cEspgen ref;auto &p=*ref.Free.buff;p.Width=g.width;p.Height=g.height;p.Type=type;p.Prm_a=g.damping;p.Prm_dmp=g.spread;
    p.pPosBuf=a.data();p.pSpdBuf=b.data();p.pHeightBuf=height.data();p.pNorBuf=normal.data();p.pTexBuf=bump.data();
    // EspGenLoopMove calls200 moves without advancing Game.Frame_cnt.
    game.Frame_cnt=0;noiseTexture.data=data.gridNoise[0].data();
    for(unsigned f=0;f<200;++f) {if(generator==0x42)Espgen42_Move00(&ref);else Espgen45_Move00(&ref);}
    data.grids[0].prerun=200;re4::EnvironmentSimulation warmed(data);
    auto compare=[&](const re4::WaterGridState &actual) {
      for(unsigned k=0;k<height.size();++k) {equal(actual.position[k],a[k]);equal(actual.speed[k],b[k]);equal(actual.height[k],height[k].y);equal(actual.normals[k][0],normal[k].x);equal(actual.normals[k][1],normal[k].y);equal(actual.normals[k][2],normal[k].z);}
    };
    compare(warmed.state().grids.at(0));
    for(unsigned f=0;f<240;++f) {
      const auto frames=warmed.state().frames+1;game.Frame_cnt=unsigned(frames);noiseTexture.data=data.gridNoise[frames%60].data();
      if(generator==0x42)Espgen42_Move00(&ref);else Espgen45_Move00(&ref);warmed.tick();compare(warmed.state().grids.at(0));
    }
  }
  std::cout<<"Original Espgen42/45 wave oracle PASS: "<<checks<<" exact floats, max error="<<maxError<<'\n';
} catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}}
'''
with tempfile.TemporaryDirectory(prefix='re4-water-oracle-') as directory:
    path=pathlib.Path(directory);source=path/'oracle.cpp';source.write_text(pre+'\n'+routine('upstream/re4/src/game/Espgen42.cpp','Espgen42_Move00')+'\n'+routine('upstream/re4/src/game/espgen45.cpp','Espgen45_Move00')+'\n'+post)
    subprocess.run(['c++','-std=c++17','-O0','-ffp-contract=off','-fno-fast-math','-Wno-register','-I',str(root/'app/src/main/cpp'),str(source),'-Wl,--start-group',str(args.build/'libre4_platform.a'),str(args.build/'libre4_motion64.a'),'-Wl,--end-group','-o',str(path/'oracle')],check=True)
    subprocess.run([str(path/'oracle')],check=True)
