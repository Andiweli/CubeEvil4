#!/usr/bin/env python3
"""Run frozen Cloth/esp4e/Rnd bodies against native real-r100 state dump.

Usage: test_m37_room_cloth_source.py CXX CPP UPSTREAM MOTION_LIB DUMP
Only SDK vector/matrix primitives are host adapters. The SINF paired-single
instructions are translated lane by lane from the frozen inline assembly.
This verifies source arithmetic/lifecycle, not PPC reciprocal-estimate bits.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def function(source, signature):
    start = source.index(signature)
    begin = source.index('{', start)
    depth, end = 1, begin + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    cxx, cpp, upstream, motion, dump = sys.argv[1:]
    cpp, upstream = Path(cpp).resolve(), Path(upstream).resolve()
    cloth = (upstream / 'src/game/cloth.cpp').read_text()
    esp = (upstream / 'src/game/esp4e.cpp').read_text()
    rnd = (upstream / 'src/game/rnd.cpp').read_text()
    math = (upstream / 'src/game/math_sub.cpp').read_text()
    coeff = re.search(r'f32 Coeff\[10\] = \{.*?\};', math, re.S)[0]
    # Decode only actual PS instructions; never call the implementation's sin.
    instructions = re.findall(r'"(ps[^"\\]*)(?:\\n\\t)?"', function(math, 'f32 SINF(f32 x)'))
    sin = coeff + '\nf32 SINF(f32 x){x=LIMIT_ANGLE(x);f32 r=0;float v[6][2]{};\n'
    for line in instructions:
        fields = line.replace(',', ' ').split()
        op, a = fields[:2]
        if op == 'psq_l':
            offset, base = re.fullmatch(r'(\d+)\((\d+)\)', fields[2]).groups()
            offset, base = int(offset), int(base)
            values = f'Coeff[{offset//4}],Coeff[{offset//4+1}]' if base == 9 else '1,1' if base == 10 else '0,0'
            sin += f'v[{a}][0]=({values.split(",")[0]});v[{a}][1]=({values.split(",")[1]});\n'
        elif op == 'ps_merge00':
            sin += 'v[2][0]=v[3][0];v[2][1]=x;\n'
        elif op == 'ps_muls0':
            sin += 'v[2][0]=v[2][0]*x;v[2][1]=v[2][1]*x;\n'
        elif op == 'ps_muls1':
            # Source/destination register aliases require preserving lane1.
            sin += '{const float old=v[3][1];v[3][0]=v[2][0]*old;v[3][1]=v[2][1]*old;}\n'
        elif op == 'ps_madd':
            sin += 'v[4][0]=std::fma(v[3][0],v[5][0],v[4][0]);v[4][1]=std::fma(v[3][1],v[5][1],v[4][1]);\n'
        elif op == 'ps_sum0':
            sin += 'r=v[4][0]+v[4][1];\n'
        else:
            raise RuntimeError('Unadapted source SINF instruction: ' + line)
    sin += 'return r;}\n'
    prefix = r'''
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
using f32=float;using u8=std::uint8_t;using s8=std::int8_t;using u16=std::uint16_t;using u32=std::uint32_t;
namespace motion64=re4::motion64;
using Vec=motion64::Vec;using Mtx=motion64::Mtx;
using motion64::RotMatrix;using motion64::TransMatrix;using motion64::PSMTXMultVec;
using motion64::PSVECSubtract;using motion64::PSVECMag;using motion64::PSVECScale;using motion64::PSVECAdd;
using motion64::PSVECCrossProduct;using motion64::VECNormalize;using motion64::PSMTXInverse;using motion64::PSMTXConcat;
using motion64::PSMTXScale;using motion64::PSMTXTranspose;using motion64::PSMTXMultVecSR;
float LIMIT_ANGLE(float x){while(x>=3.1415927f)x-=6.2831855f;while(x<-3.1415927f)x+=6.2831855f;return x;}
#define MEM_ALLOC(n,...) calloc(1,n)
void OSReport(const char*){throw std::runtime_error("source allocation");}
struct GXTexObj{};struct GXTlutObj{};
class Cloth {public:float Wgap,Hgap,Scale,radius;u8 divH,divV,be_flag,colR,colG,colB,colA;int isWave,blendMode;
 Mtx mat;Vec*pVer,*pNor,*pSpd,center;void*m_pMem;GXTexObj*tex,*pTobjA;GXTlutObj*pTlobjC;
 void Set(Vec,Vec,u8,u8,float,float,float,GXTexObj*,GXTexObj*,GXTlutObj*,int);
 void SetPosAng(Vec,Vec);void calcSpeed(float);void move();void calcNormal();void disturbance(u32,u32,float);
};
float K_PARAM=7.5f,G_PARAM=.5f,T_PARAM=.06f;
Cloth pool[3];unsigned poolCursor;
int PullCloth(Cloth**p){*p=&pool[poolCursor++];return 1;}
int ClothTexSetUp(void*,GXTexObj*,GXTexObj*,GXTlutObj*){return 0;}
int EspGetTplAddr(unsigned,void**p){*p=pool;return 1;}
struct Log{template<class...A>void err(A...){throw std::runtime_error("source error");}}log_;Log*pLog=&log_;
struct Parent{Mtx mat;};Parent parent_;Parent*pEffParentWorld=&parent_;
void low_RotMatrix(Mtx m,Vec*v){RotMatrix(m,v);}
void Matrix2AxisAngle(const Mtx,Vec*){throw std::runtime_error("unexpected r100 parent");}
struct cEspSeqTbl{Vec Vec0;u32 Tool_flg,Blend_type;u8 Work8[4],WorkSp8[3];struct{struct{u32 xCC,xD0;}w;}prm;u32 xD4;};
struct ESP4E_WK{Cloth*pCl{};GXTexObj tex;GXTlutObj tlut;float time{},time2{};s8 time_plus,time_plus2,pow,pow2;
 float range,range2,offset,wind_time{},wind_time_plus,wind_range_pow,rand_ratio;};
class cEsp4e{public:Vec m_Pos,m_Ang,m_Speed{};float m_Col_r,m_Col_g,m_Col_b,m_Col_a,m_Size_base_x,m_Size_base_y;unsigned m_Tex_id;
 Parent*parent=pEffParentWorld;ESP4E_WK m_Free{};bool CommonMove(){return true;}
 void move();int SetFreeWork(cEspSeqTbl*,u32*);
};
static u16 Random;
'''
    source = prefix + sin
    for signature in ['void Cloth::Set(', 'void Cloth::SetPosAng(', 'void Cloth::calcSpeed(',
                      'void Cloth::move()', 'void Cloth::calcNormal()', 'void Cloth::disturbance(']:
        source += function(cloth, signature) + '\n'
    for signature in ['void RndInit(', 'u8 Rnd()', 'f32 fRand0_1()']:
        source += function(rnd, signature) + '\n'
    source += function(esp, 'void cEsp4e::move()') + '\n' + function(esp, 'int cEsp4e::SetFreeWork(')
    source += r'''
unsigned checks;std::ifstream file;
template<class T>T read(){T value{};file.read((char*)&value,sizeof value);if(!file)throw std::runtime_error("dump truncated");return value;}
void compare(float source){const auto actual=read<float>();++checks;if(source!=actual){
 std::cerr<<"Cloth check"<<checks<<" source="<<source<<" native="<<actual<<" diff="<<source-actual<<'\n';
 throw std::runtime_error("frozen/native cloth arithmetic mismatch");}}
void state(cEsp4e&e){compare(e.m_Free.time);compare(e.m_Free.time2);compare(e.m_Free.wind_time);Cloth*c=e.m_Free.pCl;
 for(auto p:{c->pVer,c->pNor,c->pSpd})for(unsigned k=0;k<c->divH*c->divV;++k){compare(p[k].x);compare(p[k].y);compare(p[k].z);}
 for(auto&row:c->mat)for(auto x:row)compare(x);compare(c->center.x);compare(c->center.y);compare(c->center.z);
 // Original clothTrans builds mat*Scale and inverse-transpose normals, then
 // emits one row strip. Expand that source strip into native triangles.
 Mtx scaled,tmp,norm;PSMTXScale(scaled,c->Scale,c->Scale,c->Scale);PSMTXConcat(c->mat,scaled,scaled);
 PSMTXInverse(scaled,tmp);PSMTXTranspose(tmp,norm);
 if(read<u32>()!=u32(c->divV-1)*(c->divH-1)*6)throw std::runtime_error("source strip vertex count");
 auto vertex=[&](unsigned x,unsigned y){const auto k=x+y*c->divH;Vec p,n;PSMTXMultVec(scaled,&c->pVer[k],&p);PSMTXMultVecSR(norm,&c->pNor[k],&n);
  compare(p.x);compare(p.y);compare(p.z);compare(n.x);compare(n.y);compare(n.z);compare(float(x)/float(c->divH-1));compare(float(y)/float(c->divV-1));};
 for(unsigned y=0;y<unsigned(c->divV-1);++y)for(unsigned x=0;x<unsigned(c->divH-1);++x){vertex(x,y);vertex(x,y+1);vertex(x+1,y);vertex(x+1,y);vertex(x,y+1);vertex(x+1,y+1);}}
unsigned big(const u8*b,unsigned p){return u32(b[p])<<24|u32(b[p+1])<<16|u32(b[p+2])<<8|b[p+3];}
float real(const u8*b,unsigned p){auto bits=big(b,p);float n;memcpy(&n,&bits,4);return n;}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("source dump required");file.open(argv[1],std::ios::binary);
 auto n=read<u32>(),frames=read<u32>();if(n!=3||frames!=40)throw std::runtime_error("actual r100 three-sheets fixture");
 cEsp4e effects[3];cEspSeqTbl seq[3]{};
 for(unsigned k=0;k<n;++k){u8 raw[300];file.read((char*)raw,300);auto&e=effects[k];auto&s=seq[k];
  if(raw[1]!=78||raw[7]!=254)throw std::runtime_error("source world cloth profile");
  // The actual3 records have no CommonMove displacement/acceleration/fade,
  // scale/angle steps or lifetime; the inert CommonMove adapter is bounded.
  for(unsigned p:{24u,28u,32u,36u,40u,44u,96u,100u,104u,108u,112u,116u,144u,148u})
   if(real(raw,p)!=0)throw std::runtime_error("non-inert actual CommonMove profile");
  for(unsigned p:{160u,164u,168u,172u})if(real(raw,p)!=1)throw std::runtime_error("authored no fade");
  if(raw[176]||raw[177]||raw[184]||raw[185])throw std::runtime_error("bounded immortal CommonMove profile");
  e.m_Pos={real(raw,12),real(raw,16),real(raw,20)};e.m_Ang={real(raw,88)*.017453292f,real(raw,92)*.017453292f,real(raw,96)*.017453292f};
  e.m_Col_r=raw[156];e.m_Col_g=raw[157];e.m_Col_b=raw[158];e.m_Col_a=raw[159];
  e.m_Size_base_x=real(raw,136);e.m_Size_base_y=real(raw,140);e.m_Tex_id=raw[2];
  s.Vec0={real(raw,216),real(raw,220),real(raw,224)};s.Tool_flg=big(raw,8);s.Blend_type=raw[194];
  std::copy_n(raw+200,4,s.Work8);std::copy_n(raw+252,3,s.WorkSp8);s.prm.w={big(raw,204),big(raw,208)};s.xD4=big(raw,212);
  u32 seed=0;if(!e.SetFreeWork(&s,&seed))throw std::runtime_error("actual source cloth allocation");state(e);
 }
 RndInit(0x4e22);
 for(unsigned f=0;f<frames;++f){for(auto&e:effects){e.move();state(e);}++checks;
  if(read<u16>()!=Random)throw std::runtime_error("source shared visual RNG order/count mismatch");}
 if(file.peek()!=EOF)throw std::runtime_error("source dump trailing bytes");
 std::cout<<"M37 frozen Cloth/esp4e/Rnd + decoded paired-single SINF: "<<checks<<" checks PASS\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
'''
    with tempfile.TemporaryDirectory(prefix='re4-m37-cloth-source-') as folder:
        folder = Path(folder)
        harness, binary = folder / 'cloth.cpp', folder / 'cloth'
        harness.write_text(source)
        subprocess.run([cxx, '-std=c++17', '-O2', '-ffp-contract=off', '-fno-strict-aliasing',
                        '-I', str(cpp), str(harness), '-Wl,--start-group', motion,
                        str(Path(motion).parent/'libre4_platform.a'), '-Wl,--end-group',
                        '-o', str(binary)], check=True)
        subprocess.run([str(binary), dump], check=True)


if __name__ == '__main__':
    main()
