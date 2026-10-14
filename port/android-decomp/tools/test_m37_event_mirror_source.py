#!/usr/bin/env python3
"""Execute frozen TexRender resize/copy and SDK light projection.
Usage: test_m37_event_mirror_source.py CXX CPP_DIR UPSTREAM_DIR GLES_INCLUDE GLES_LIBRARY
"""
from pathlib import Path
import subprocess
import sys
import tempfile

if len(sys.argv) != 6:
    raise SystemExit(__doc__)
cxx, cpp, upstream, include, library = sys.argv[1:]
cpp, upstream = Path(cpp).resolve(), Path(upstream).resolve()


def function(path, name):
    text = path.read_text()
    start = text.index('void '+name+'(')
    brace = text.index('{', start)
    count, end = 1, brace+1
    while count:
        count += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


source = function(upstream/'src/game/TexRender.cpp', 'RenderTexRenderMgr')
source += function(upstream/'src/game/TexRender.cpp', 'CopyTexRenderMgr')
source += function(upstream/'src/lib/mtx.c', 'C_MTXLightPerspective')
prefix = r'''
#include "event_mirror_gl.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
using u8=std::uint8_t;using u32=std::uint32_t;using f32=float;
using Mtx=float[3][4];
#define ASSERTMSGLINE(...)
#define MTXDegToRad(a) ((a)*0.017453292519943295f)
struct GXTexObj{};
struct TexRenderMng{unsigned GetWSize(){return 256;}unsigned GetHSize(){return 256;}
 unsigned GetRepeatType(){return 1;}void*GetBufAddr(){return this;}GXTexObj*GetTexObj(){return &tex;}GXTexObj tex;};
TexRenderMng g_RndMgr[1];unsigned g_RndMgrNum=1;int g_draw;
struct Log{template<class...A>void err(A...){throw std::runtime_error("original error");}}log_;Log*pLog=&log_;
struct GXRenderModeObj{u8 aa{1};u8 sample_pattern[12][2]{};u8 vfilter[7]{};}Rmode;
int pG;constexpr unsigned STA_TEX_RENDER=1;
std::array<unsigned,2> size{},restore{};std::array<unsigned,4> scissor{},copy{};
std::array<unsigned,3> destination{};unsigned wrapS{},wrapT{},aa{},filter{},minFilter{},magFilter{};
bool StaFlagChk(int,unsigned){return true;}void StaFlagOff(int,unsigned){}
void EFBReSize(unsigned w,unsigned h){size={w,h};}
void GXSetScissor(unsigned x,unsigned y,unsigned w,unsigned h){scissor={x,y,w,h};}
void GXSetTexCopySrc(unsigned x,unsigned y,unsigned w,unsigned h){copy={x,y,w,h};}
void GXSetTexCopyDst(unsigned w,unsigned h,unsigned f,unsigned mip){destination={w,h,f};if(mip!=1)throw std::runtime_error("original copy mip flag");}
void GXSetCopyFilter(unsigned a,u8(*)[2],unsigned f,u8*){if(!f){aa=a;filter=f;}}
void GXInitTexObj(GXTexObj*,void*,unsigned,unsigned,unsigned,unsigned s,unsigned t,unsigned){wrapS=s;wrapT=t;}
void GXInitTexObjLOD(GXTexObj*,unsigned min,unsigned mag,float,float,float,unsigned,unsigned,unsigned){minFilter=min;magFilter=mag;}
void ScreenReSize(unsigned w,unsigned h){restore={w,h};}
template<class...A>void ignore(A...){}
#define GXSetAlphaUpdate ignore
#define GXCopyTex ignore
#define GXPixModeSync ignore
#define GXInvalidateTexAll ignore
#define SetScissorState ignore
'''
suffix = r'''
int main(){try{
 unsigned checks=0;RenderTexRenderMgr(&g_RndMgr[0]);CopyTexRenderMgr(&g_RndMgr[0]);
 const auto layout=re4::OriginalEventMirrorLayout();
 if(size!=std::array<unsigned,2>{layout.width,layout.height} ||
    scissor!=std::array<unsigned,4>{layout.copyX,layout.copyY,layout.copyWidth,layout.copyHeight} ||copy!=scissor ||
    destination!=std::array<unsigned,3>{layout.textureWidth,layout.textureHeight,6} ||wrapS!=1||wrapT!=1||
    aa!=0||filter!=0||minFilter!=1||magFilter!=1||restore!=std::array<unsigned,2>{512,448})
  throw std::runtime_error("original/native RTT resize/crop/downsample/wrap/filter mismatch");checks+=18;
 for(float fov:{.3f,.5f,.7f,1.f,1.4f,2.f}) {
  re4::EventCamera c;c.eye={0,0,0};c.target={0,0,-1000};c.fovRadians=fov;c.valid=true;
  const auto native=re4::EventMirrorCameraMatrix(c,{},.001f);Mtx original;
  C_MTXLightPerspective(original,fov*180.f/3.14159265358979323846f,1.3333334f,.5f,-.5f,.5f,.5f);
  for(float x:{-300.f,0.f,300.f})for(float y:{-400.f,0.f,400.f})for(float z:{-500.f,-1000.f,-5000.f}) {
   const float q=-z;
   const float sourceS=(original[0][0]*x+original[0][2]*z)/q;
   const float sourceT=(original[1][1]*y+original[1][2]*z)/q;
   const float glS=(native[0]*x/native[11]/z)*.5f+.5f;
   const float glT=(native[5]*y/native[11]/z)*.5f+.5f;
   // GX texture copies are top-down; native FBO storage is bottom-up.
   if(std::abs(sourceS-glS)>.000001f || std::abs(sourceT-(1-glT))>.000001f)
    throw std::runtime_error("original/native fixed4:3 projected texture coordinates");checks+=2;
  }
 }
 std::cout<<"M37 original mirror source RTT/projection PASS: "<<checks<<" checks\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
'''
with tempfile.TemporaryDirectory(prefix='re4-m37-mirror-source-') as temp:
    temp = Path(temp)
    harness = temp/'mirror.cpp'
    harness.write_text(prefix+source+suffix)
    binary = temp/'mirror'
    subprocess.run([cxx, '-std=c++17', '-O2', '-fno-fast-math', '-ffp-contract=off',
                    '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
                    '-I', str(cpp), '-I', include, str(harness),
                    str(cpp/'event_mirror_gl.cpp'), library, '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
