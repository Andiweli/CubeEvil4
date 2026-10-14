#include "room_effect_cloth_gl.h"
#include "room_area_effects.h"
#include "room_lighting_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
namespace {
using namespace re4;
constexpr unsigned W=256,H=256;unsigned checks;
void check(bool v,const char*s){++checks;if(!v)throw std::runtime_error(s);}
struct Egl {
 EGLDisplay d;EGLConfig config;EGLSurface surface;EGLContext context=EGL_NO_CONTEXT;
 Egl(){setenv("EGL_PLATFORM","surfaceless",1);d=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint major,minor,n;
  check(eglInitialize(d,&major,&minor),"EGL init");check(eglBindAPI(EGL_OPENGL_ES_API),"GLES API");
  const EGLint attrs[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
  check(eglChooseConfig(d,attrs,&config,1,&n)&&n,"EGL config");const EGLint size[]{EGL_WIDTH,W,EGL_HEIGHT,H,EGL_NONE};
  surface=eglCreatePbufferSurface(d,config,size);check(surface!=EGL_NO_SURFACE,"EGL surface");fresh();}
 void fresh(){if(context!=EGL_NO_CONTEXT){eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,context);}
  const EGLint attr[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};context=eglCreateContext(d,config,EGL_NO_CONTEXT,attr);
  check(context!=EGL_NO_CONTEXT && eglMakeCurrent(d,surface,surface,context),"EGL context");}
 ~Egl(){eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,context);eglDestroySurface(d,surface);eglTerminate(d);}
};
void clear(){glViewport(0,0,W,H);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_RASTERIZER_DISCARD);glDisable(GL_POLYGON_OFFSET_FILL);
 glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);glDisable(GL_SAMPLE_COVERAGE);glDepthRangef(0,1);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
 glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
std::vector<std::uint8_t> pixels(){std::vector<std::uint8_t> p(W*H*4);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,p.data());
 check(glGetError()==GL_NO_ERROR,"cloth GPU state clean");return p;}
unsigned changed(const std::vector<std::uint8_t>&a,const std::vector<std::uint8_t>&b){unsigned n=0;for(unsigned i=0;i<a.size();i+=4)if(a[i]!=b[i]||a[i+1]!=b[i+1]||a[i+2]!=b[i+2])++n;return n;}
std::vector<std::uint8_t> bytes(const std::string&p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("asset missing");
 auto n=f.tellg();std::vector<std::uint8_t> b(n);f.seekg(0);f.read((char*)b.data(),n);return b;}
std::vector<GLint> state(){std::vector<GLint> out;for(auto token:{GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,GL_ACTIVE_TEXTURE,
 GL_DEPTH_FUNC,GL_CULL_FACE_MODE,GL_FRONT_FACE,GL_BLEND_SRC_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_RGB,GL_BLEND_DST_ALPHA,
 GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA,GL_UNPACK_ALIGNMENT}){GLint n;glGetIntegerv(token,&n);out.push_back(n);}
 for(auto token:{GL_DEPTH_TEST,GL_BLEND,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_RASTERIZER_DISCARD,GL_POLYGON_OFFSET_FILL,
 GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE,GL_DITHER,GL_PRIMITIVE_RESTART_FIXED_INDEX})out.push_back(glIsEnabled(token));
 GLboolean z,c[4];glGetBooleanv(GL_DEPTH_WRITEMASK,&z);glGetBooleanv(GL_COLOR_WRITEMASK,c);out.push_back(z);for(auto v:c)out.push_back(v);
 GLfloat range[2];glGetFloatv(GL_DEPTH_RANGE,range);for(auto f:range){GLint n;std::memcpy(&n,&f,4);out.push_back(n);}
 GLint active;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);for(unsigned u=0;u<3;++u){glActiveTexture(GL_TEXTURE0+u);GLint t,s;
 glGetIntegerv(GL_TEXTURE_BINDING_2D,&t);glGetIntegerv(GL_SAMPLER_BINDING,&s);out.push_back(t);out.push_back(s);}glActiveTexture(active);return out;}
std::array<float,16> camera(const RoomEffectClothMesh&m,const std::array<float,3>&center){
 std::array<float,3> right{},normal{};float len=0;
 for(unsigned i=0;i<3;++i){right[i]=m.vertices[2].position[i]-m.vertices[0].position[i];len+=right[i]*right[i];}
 for(auto&x:right)x/=std::sqrt(len);
 normal={-right[2],0,right[0]};
 std::array<float,16> a{};for(unsigned i=0;i<3;++i){a[i*4]=right[i]/600.f;a[i*4+1]=(i==1?1.f:0.f)/750.f;a[i*4+2]=normal[i]/10000.f;}
 a[12]=0;a[13]=.65f;a[15]=1;(void)center;return a;
}
void actual(Egl&egl,const std::string&roomFile,const std::string&coreFile){
 const auto room=RoomArchive::Load(roomFile);const auto data=BuildRoomAreaEffects(room,0x100,bytes(coreFile));RoomAreaEffects effects(data);
 auto sheets=effects.clothMeshes();check(sheets.size()==3,"actual SST1 three cloth sheets alive");
 check(sheets[0].vertices.size()==432,"actual grid expands 9 original strips");
 RoomAtmosphere().data=ReadRoomLighting(room);RoomAtmosphere().eye=sheets[0].lightCenter;RoomAtmosphere().look={0,0,-1};
 CreateRoomEffectClothGl(data.effects);const auto matrix=camera(sheets[0],sheets[0].lightCenter);const auto center=sheets[0].lightCenter;
 clear();const auto blank=pixels();DrawRoomEffectClothGl(matrix.data(),center,1,sheets);const auto initial=pixels();
 check(changed(blank,initial)>1000,"actual TPL0 cloth visible with source lighting/material");
 clear();DrawRoomEffectClothGl(matrix.data(),center,1,sheets);check(pixels()==initial,"paused cloth rendering deterministic");
 WorldEffectCamera view;for(unsigned i=0;i<20;++i)effects.tick({},view,0xffffffff);sheets=effects.clothMeshes();
 clear();DrawRoomEffectClothGl(matrix.data(),center,1,sheets);const auto animated=pixels();check(changed(initial,animated)>20,"actual source wave mesh changes rendered image");
 glEnable(GL_SCISSOR_TEST);glEnable(GL_STENCIL_TEST);glEnable(GL_RASTERIZER_DISCARD);glEnable(GL_CULL_FACE);glCullFace(GL_FRONT_AND_BACK);glFrontFace(GL_CW);
 glEnable(GL_POLYGON_OFFSET_FILL);glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);glEnable(GL_SAMPLE_COVERAGE);glDisable(GL_DITHER);glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
 glDepthMask(GL_FALSE);glDepthFunc(GL_GREATER);glDepthRangef(.2f,.8f);glColorMask(GL_TRUE,GL_FALSE,GL_TRUE,GL_FALSE);glActiveTexture(GL_TEXTURE5);glPixelStorei(GL_UNPACK_ALIGNMENT,8);
 glBlendFuncSeparate(GL_ONE,GL_ZERO,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA);glBlendEquationSeparate(GL_FUNC_SUBTRACT,GL_FUNC_REVERSE_SUBTRACT);
 auto before=state();DrawRoomEffectClothGl(matrix.data(),center,1,sheets);check(before==state(),"cloth restores complete caller GL state");
 clear();DrawRoomEffectClothGl(matrix.data(),center,1,sheets);check(pixels()==animated,"hostile preceding GL state does not change cloth pixels");
 DestroyRoomEffectClothGl();CreateRoomEffectClothGl(data.effects);clear();DrawRoomEffectClothGl(matrix.data(),center,1,sheets);check(pixels()==animated,"GPU rebuild preserves paused cloth image");
 ForgetRoomEffectClothGl();egl.fresh();CreateRoomEffectClothGl(data.effects);clear();DrawRoomEffectClothGl(matrix.data(),center,1,sheets);check(pixels()==animated,"EGL recovery preserves paused cloth image");
 DestroyRoomEffectClothGl();RoomAtmosphere()={};
}
RoomEffectClothMesh quad(){RoomEffectClothMesh m;m.color={105,100,90,255};
 const RoomEffectClothVertex a{{-.8f,-.8f,0},{0,0,1},{0,1}},b{{.8f,-.8f,0},{0,0,1},{1,1}},c{{-.8f,.8f,0},{0,0,1},{0,0}},d{{.8f,.8f,0},{0,0,1},{1,0}};
 m.vertices={a,b,c,c,b,d};return m;}
void contract(){WorldEffectsData data;data.sprites.targets.resize(1);data.sprites.targets[0].records.resize(1);data.sprites.targets[0].records[0].bytes[1]=78;
 data.sprites.animations.resize(1);Texture texture;texture.width=texture.height=2;texture.rgba.resize(16);
 for(unsigned i=0;i<16;i+=4){texture.rgba[i]=64;texture.rgba[i+1]=128;texture.rgba[i+2]=32;texture.rgba[i+3]=255;}
 data.sprites.animations[0].images.push_back(texture);CreateRoomEffectClothGl(data);
 constexpr float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};auto mesh=quad();clear();DrawRoomEffectClothGl(identity,{},1,{mesh});auto p=pixels();const unsigned at=(H/2*W+W/2)*4;
 check(std::abs(int(p[at])-105)<=1 && std::abs(int(p[at+1])-201)<=1 && std::abs(int(p[at+2])-45)<=1,"source TEV4 color/material contract");
 for(unsigned i=0;i<6;i+=3)std::swap(mesh.vertices[i],mesh.vertices[i+2]);
 clear();DrawRoomEffectClothGl(identity,{},1,{mesh});check(pixels()==p,"original cloth is two-sided");
 mesh.color[3]=0;clear();const auto blank=pixels();DrawRoomEffectClothGl(identity,{},1,{mesh});check(pixels()==blank,"source GEQUAL4 alpha removes transparent cloth");
 mesh.color[3]=255;auto far=mesh;far.color={0,255,0,255};for(auto&v:far.vertices)v.position[2]=.5f;
 clear();DrawRoomEffectClothGl(identity,{},1,{mesh,far});check(pixels()==p,"source standard cloth writes depth");
 mesh.additive=true;clear();DrawRoomEffectClothGl(identity,{},1,{mesh,far});auto add=pixels();check(add[at]==0 && add[at+1]==255 && add[at+2]==0,"source additive cloth suppresses depth writes");
 clear();const auto before=state();DrawRoomEffectClothGl(nullptr,{},1,{});check(before==state(),"empty cloth draw leaves caller state untouched");
 DestroyRoomEffectClothGl();
}
}
int main(int argc,char**argv){try{if(argc!=3)throw std::runtime_error("Usage: room_cloth_gl_check R100 CORE_EFF");Egl egl;actual(egl,argv[1],argv[2]);contract();
 std::cout<<"M37 actual r100 cloth GPU: "<<checks<<" checks PASS\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
