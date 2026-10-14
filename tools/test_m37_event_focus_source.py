#!/usr/bin/env python3
"""Execute the frozen original Filter01 type0/type1 draw against the native pass plan.

Usage: test_m37_event_focus_source.py CXX CPP_DIR UPSTREAM_DIR GLES_INCLUDE GLES_LIBRARY
The source is compiled into a temporary instrumented GX harness. No game assets
are needed and no source equations are copied into a second expected model.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

if len(sys.argv) != 6:
    raise SystemExit(__doc__)
cxx, cpp, upstream, gl_include, gl_library = sys.argv[1:]
cpp = Path(cpp).resolve()
source = (Path(upstream) / 'src/game/filter01.cpp').read_text()
lines = [line for line in source.splitlines()
         if not line.startswith('#include') and not line.startswith('asm(')]
source = '\n'.join(lines).replace('__attribute__((aligned(32)))', '')
# Host float->u32 is undefined for negative screen-Z. Use the original
# tealeaf.c __cvt_fp2unsigned signed-truncation/low32 behavior at this cast.
source = source.replace('(u32) (((1.0f / -camz)', 'OriginalPpcU32(((1.0f / -camz)')
# The full source's dead-strip and API structure remain intact. Replace just
# its duplicate declarations/names so both original and port coexist.
for name in ['Filter01Init', 'Filter01RoomInit', 'Filter01Trans',
             'Filter01CopyEFB', 'Filter01Render', 'Filter01SetParam_ScrZ',
             'Filter01SetParam_CamZ', 'Filter01SetParam']:
    import re
    source = re.sub(r'\b' + name + r'\b', 'Original_' + name, source)
prefix = r'''
#include "event_focus_gl.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using u8=std::uint8_t;using u32=std::uint32_t;using f32=float;
using Mtx44=float[4][4];using Mtx=float[3][4];
struct GXColor{u8 r,g,b,a;};struct GXTexObj{};
struct GXRenderModeObj{u8 aa{};u8 sample_pattern[12][2]{};u8 vfilter[7]{};}Rmode;
constexpr unsigned SCR_W=512,SCR_H=448;constexpr float ZNEAR=100,ZFAR=1000000;
struct Env{int FocusLevel{},FocusZ{},FocusMode{};}env;
using cLightEnv=Env;
struct Light{Env* getEnvPtr(){return &env;}void setFog(){}}LightMgr;
struct Log{template<class...A>void warn(A...){throw std::runtime_error("original warn");}
 template<class...A>void err(A...){throw std::runtime_error("original err");}}logger;
Log*pLog=&logger;
struct Captured{float x,y,z;u8 alpha{};bool recopy{};};
std::vector<Captured> captured;unsigned vertex{},copies{};bool capture{},copyPending{};
template<class...A>void ignored(A...){ }
#define GXSetCopyFilter ignored
#define GXSetTexCopySrc ignored
#define GXSetTexCopyDst ignored
#define GXPixModeSync ignored
#define GXInvalidateTexAll ignored
#define DCInvalidateRange ignored
#define GXSetFog ignored
#define GXSetAlphaCompare ignored
#define GXInitTexObj ignored
#define GXInitTexObjLOD ignored
#define GXSetColorUpdate ignored
#define GXSetAlphaUpdate ignored
#define GXSetCullMode ignored
#define C_MTXOrtho ignored
#define GXSetProjection ignored
#define PSMTXIdentity ignored
#define GXLoadPosMtxImm ignored
#define GXSetCurrentMtx ignored
#define GXSetBlendMode ignored
#define GXSetNumTevStages ignored
#define GXSetNumChans ignored
#define GXSetNumTexGens ignored
#define GXSetTexCoordGen ignored
#define GXSetChanCtrl ignored
#define GXSetChanAmbColor ignored
#define GXLoadTexObj ignored
#define GXSetTevOrder ignored
#define GXSetTevColorIn ignored
#define GXSetTevColorOp ignored
#define GXSetTevAlphaIn ignored
#define GXSetTevAlphaOp ignored
#define GXClearVtxDesc ignored
#define GXSetVtxDesc ignored
#define GXSetVtxAttrFmt ignored
#define GXSetZMode ignored
#define GXTexCoord2f32 ignored
#define AddOtDirect ignored
u32 OriginalPpcU32(float value){return u32(std::int32_t(value));}
bool Render_checkBlurPermission(){return true;}
void* GetDrawTmpBufAddr(unsigned){static char memory;return &memory;}
void GXCopyTex(void*,int){if(capture){++copies;copyPending=copies>1;}}
void GXBegin(unsigned,unsigned,unsigned){vertex=0;}
void GXPosition3f32(float x,float y,float z){if(capture&&vertex++==0){captured.push_back({x,y,z,0,copyPending});copyPending=false;}}
void GXColor4u8(u8,u8,u8,u8 alpha){if(capture)captured.back().alpha=alpha;}
'''
suffix = r'''
int main(){try{
 unsigned checks=0;
 for(float level:{0.f,.25f,1.f,1.01f,2.f,3.f,5.f,6.f,10.f})
  for(float depth:{0.f,.01f,50.f,100.f,101.f,1650.f,1750.f,3583.f,3584.f,10000.f,50000.f,1000000.f})
   for(int mode:{0,1}){
    captured.clear();copies=0;copyPending=false;capture=true;
    Original_Filter01SetParam_CamZ(mode,level,depth,1);
    Original_Filter01Render(mode?&g_LeFar:&g_LeNear);
    capture=false;
    auto port=re4::EventFocusPasses(level,depth);
    if(captured.size()!=port.size())throw std::runtime_error("original/native focus pass count");++checks;
    for(std::size_t i=0;i<port.size();++i){
     const auto&a=captured[i];const auto&b=port[i];
     if(std::memcmp(&a.x,&b.x,4)||std::memcmp(&a.y,&b.y,4)||std::memcmp(&a.z,&b.sourceDepth,4)||a.alpha!=b.alpha||a.recopy!=b.recopy)
      {std::cerr<<"level="<<level<<" depth="<<depth<<" mode="<<mode<<" pass="<<i<<" original="<<a.x<<","<<a.y<<","<<a.z<<","<<unsigned(a.alpha)<<","<<a.recopy<<" port="<<b.x<<","<<b.y<<","<<b.sourceDepth<<","<<unsigned(b.alpha)<<","<<b.recopy<<"\n";throw std::runtime_error("original/native focus coordinates, depth, alpha or recopy mismatch");}
     checks+=5;
    }
   }
 for(unsigned level=0;level<=10;++level)for(int depth:{0,61261,65499,65536})for(int mode:{0,1}) {
  captured.clear();copies=0;copyPending=false;capture=true;
  LensEffectWork w{};w.type=0;w.level=float(level);w.z=depth;w.Mode=mode;
  Original_Filter01Render(&w);capture=false;
  re4::RoomLightCut cut;cut.focusLevel=level;cut.focusDepth=depth;cut.focusMode=mode;
  const auto port=re4::RoomFocusPasses(cut);
  if(captured.size()!=port.size())throw std::runtime_error("original/native ENV focus pass count");++checks;
  for(std::size_t i=0;i<port.size();++i) {
   const auto&a=captured[i];const auto&b=port[i];
   if(std::memcmp(&a.x,&b.x,4)||std::memcmp(&a.y,&b.y,4)||std::memcmp(&a.z,&b.sourceDepth,4)||a.alpha!=b.alpha||a.recopy!=b.recopy)
    throw std::runtime_error("original/native ENV focus offset, depth, alpha or copy mismatch");
   checks+=5;
  }
 }
 std::cout<<"M37 original Filter01 type0/type1 source oracle PASS: "<<checks<<" exact checks; 216 event requests and 88 ENV requests\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
'''
with tempfile.TemporaryDirectory(prefix='re4-m37-focus-source-') as temp:
    temp = Path(temp)
    harness = temp / 'focus.cpp'
    harness.write_text(prefix + source + suffix)
    binary = temp / 'focus'
    subprocess.run([cxx, '-std=c++17', '-O2', '-fno-fast-math', '-ffp-contract=off',
                    '-I', str(cpp), '-I', gl_include, str(harness),
                    str(cpp / 'event_focus_gl.cpp'), gl_library, '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
