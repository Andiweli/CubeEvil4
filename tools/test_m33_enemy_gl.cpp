// Actual original EM12/core blood assets through the production GLES3 pass.
// No source game data is included in this test.
#include "archive.h"
#include "collision_query.h"
#include "world_effects_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
using namespace re4;
namespace {
constexpr unsigned W=256,H=256;
unsigned checks;
void require(bool value,const char *message) {
  ++checks;if(!value)throw std::runtime_error(message);
}
struct Egl {
  EGLDisplay display=EGL_NO_DISPLAY;EGLConfig config{};
  EGLContext context=EGL_NO_CONTEXT;EGLSurface surface=EGL_NO_SURFACE;
  Egl() {
    setenv("EGL_PLATFORM","surfaceless",1);display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major{},minor{},count{};
    require(eglInitialize(display,&major,&minor),"EGL initialize");
    require(eglBindAPI(EGL_OPENGL_ES_API),"EGL GLES API");
    const EGLint attributes[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,
      EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
    require(eglChooseConfig(display,attributes,&config,1,&count)&&count,"EGL config");
    const EGLint size[]{EGL_WIDTH,W,EGL_HEIGHT,H,EGL_NONE};
    surface=eglCreatePbufferSurface(display,config,size);
    require(surface!=EGL_NO_SURFACE,"EGL pbuffer");fresh();
  }
  void fresh() {
    if(context!=EGL_NO_CONTEXT) {
      require(eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT),"EGL detach");
      require(eglDestroyContext(display,context),"EGL old context destroy");
    }
    const EGLint attributes[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,attributes);
    require(context!=EGL_NO_CONTEXT&&eglMakeCurrent(display,surface,surface,context),"Fresh EGL context");
  }
  ~Egl(){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);}
};
struct Sentinel {
  GLuint vao{},buffer{},texture{};
  Sentinel() {
    glGenVertexArrays(1,&vao);glGenBuffers(1,&buffer);glGenTextures(1,&texture);
    glUseProgram(0);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
    glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,texture);
    glDisable(GL_DEPTH_TEST);glDepthFunc(GL_GREATER);glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);glEnable(GL_CULL_FACE);
    glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_SUBTRACT);
    glBlendFuncSeparate(GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,GL_ONE,GL_ZERO);
    glColorMask(GL_TRUE,GL_FALSE,GL_TRUE,GL_TRUE);glViewport(8,8,W-16,H-16);
  }
  ~Sentinel(){glDeleteVertexArrays(1,&vao);glDeleteBuffers(1,&buffer);glDeleteTextures(1,&texture);}
  void check() const {
    GLint value{},viewport[4]{};GLboolean depth{},mask[4]{};
    auto is=[&](GLenum key,GLint expected){glGetIntegerv(key,&value);require(value==expected,"GLES state restoration");};
    is(GL_CURRENT_PROGRAM,0);is(GL_VERTEX_ARRAY_BINDING,GLint(vao));
    is(GL_ARRAY_BUFFER_BINDING,GLint(buffer));is(GL_ACTIVE_TEXTURE,GL_TEXTURE3);
    is(GL_TEXTURE_BINDING_2D,GLint(texture));glActiveTexture(GL_TEXTURE0);
    is(GL_TEXTURE_BINDING_2D,GLint(texture));glActiveTexture(GL_TEXTURE3);
    require(!glIsEnabled(GL_DEPTH_TEST),"Depth enable restored");is(GL_DEPTH_FUNC,GL_GREATER);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&depth);require(depth,"Depth write restored");
    require(glIsEnabled(GL_BLEND)&&glIsEnabled(GL_CULL_FACE),"Blend/cull restored");
    is(GL_BLEND_EQUATION_RGB,GL_FUNC_REVERSE_SUBTRACT);is(GL_BLEND_EQUATION_ALPHA,GL_FUNC_SUBTRACT);
    is(GL_BLEND_SRC_RGB,GL_DST_ALPHA);is(GL_BLEND_DST_RGB,GL_ONE_MINUS_DST_ALPHA);
    is(GL_BLEND_SRC_ALPHA,GL_ONE);is(GL_BLEND_DST_ALPHA,GL_ZERO);
    glGetBooleanv(GL_COLOR_WRITEMASK,mask);require(mask[0]&&!mask[1]&&mask[2]&&mask[3],"Color mask restored");
    glGetIntegerv(GL_VIEWPORT,viewport);require(viewport[0]==8&&viewport[1]==8&&
      viewport[2]==int(W-16)&&viewport[3]==int(H-16),"Viewport preserved");
    require(glGetError()==GL_NO_ERROR,"Clean GLES state");
  }
};
void clear(float depth=1) {
  glViewport(0,0,W,H);glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
  glClearColor(.85f,.85f,.85f,.5f);glClearDepthf(depth);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}
std::vector<std::uint8_t> pixels() {
  std::vector<std::uint8_t> result(W*H*4);
  glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,result.data());
  require(glGetError()==GL_NO_ERROR,"Read actual GPU framebuffer");return result;
}
unsigned redPixels(const std::vector<std::uint8_t> &frame) {
  unsigned count{};for(unsigned i=0;i<frame.size();i+=4)
    if(frame[i]>frame[i+1]+8&&frame[i]>frame[i+2]+8)++count;
  return count;
}
std::vector<std::uint8_t> effect(const RoomArchive &archive) {
  const auto *entry=archive.find("EFF");require(entry,"Actual original EFF exists");
  return archive.copy(entry-archive.entries().data());
}
Sat flat() {
  std::vector<std::uint8_t> b(20+48+12+12+40);
  auto half=[&](unsigned p,unsigned n){b.at(p)=std::uint8_t(n>>8);b.at(p+1)=std::uint8_t(n);};
  auto word=[&](unsigned p,unsigned n){half(p,n>>16);half(p+2,n);};
  b[0]=32;half(2,4);half(4,1);half(6,1);half(10,2);half(12,2);unsigned p=20;
  for(float f:{-100000.f,0.f,-100000.f,100000.f,0.f,-100000.f,
      100000.f,0.f,100000.f,-100000.f,0.f,100000.f,0.f,1.f,0.f}) {
    unsigned bits;std::memcpy(&bits,&f,4);word(p,bits);p+=4;
  }
  p+=12;half(p+2,2);half(p+4,1);half(p+22,3);half(p+24,2);return Sat(b);
}
constexpr float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
struct FrameSite {std::array<float,3> center{};float scale{};};
FrameSite framing(const std::vector<WorldEffectQuad> &quads) {
  std::array<float,3> low{},high{};low.fill(std::numeric_limits<float>::max());
  high.fill(std::numeric_limits<float>::lowest());
  for(const auto&q:quads)for(const auto&p:q.position)for(unsigned k=0;k<3;++k) {
    low[k]=std::min(low[k],p[k]);high[k]=std::max(high[k],p[k]);
  }
  FrameSite site;float extent=1;
  for(unsigned k=0;k<3;++k){site.center[k]=(low[k]+high[k])*.5f;extent=std::max(extent,high[k]-low[k]);}
  site.scale=1.6f/extent;return site;
}
void preview(unsigned sequence,const std::vector<std::uint8_t> &frame) {
  const auto *prefix=std::getenv("RE4_EFFECT_PREVIEW_PREFIX");if(!prefix)return;
  std::ofstream out(std::string(prefix)+"m33-blood-"+std::to_string(sequence)+".ppm",std::ios::binary);
  out<<"P6\n"<<W<<' '<<H<<"\n255\n";
  for(int y=int(H)-1;y>=0;--y)for(unsigned x=0;x<W;++x)
    out.write(reinterpret_cast<const char *>(frame.data()+(unsigned(y)*W+x)*4),3);
  require(bool(out),"Actual blood framebuffer saved");
}
}
int main(int argc,char **argv) {
  try {
    if(argc!=3)throw std::runtime_error("Usage: m33_enemy_gl_check EM12.drs CORE.das");
    Egl egl;const auto enemy=RoomArchive::LoadDrs(argv[1]),core=RoomArchive::LoadDrs(argv[2]);
    const auto data=BuildEnemyWorldEffects(effect(enemy),effect(core));
    require(data.originalEnemyBlood&&data.triggerBanks.size()==8,"Eight original enemy blood EST profiles");
    require(data.sprites.animations.size()>=22,"Complete actual source22-animation GPU upload");
    const auto sat=flat();const SatCollision floor(sat);
    WorldEffectCamera camera;camera.eye={0,1600,5000};camera.forward={0,0,-1};
    std::set<unsigned> drawnAnimations;
    for(const auto &bank:data.triggerBanks) {
      {Sentinel sentinel;CreateEnemyEffectsGl(data);sentinel.check();}
      WorldEffectsSimulation simulation(data);
      require(simulation.trigger(bank.sequence,{0,1600,0},{0,1.1f,0},1,&floor),"Original EST trigger");
      std::vector<std::uint8_t> best;std::vector<WorldEffectQuad> bestQuads;FrameSite site;
      WorldEffectsState pausedState;
      unsigned visible{};
      for(unsigned tick=0;tick<24;++tick) {
        simulation.tick(&camera,&floor);const auto quads=WorldEffectQuads(data,simulation.state(),camera);
        if(quads.empty())continue;
        const auto view=framing(quads);clear();const auto base=pixels();
        {Sentinel sentinel;DrawEnemyEffectsGl(identity,view.center,view.scale,quads);sentinel.check();}
        for(const auto &quad:quads)drawnAnimations.insert(quad.animation);
        const auto frame=pixels();
        for(unsigned i=3;i<frame.size();i+=4)require(frame[i]==base[i],"Blood preserves destination alpha");
        const unsigned count=redPixels(frame);
        if(count>visible){visible=count;best=frame;bestQuads=quads;site=view;pausedState=simulation.state();}
      }
      require(visible>5,"Actual original blood textures visibly render red");
      preview(bank.sequence,best);
      clear();{Sentinel sentinel;DrawEnemyEffectsGl(identity,site.center,site.scale,bestQuads);sentinel.check();}
      require(pixels()==best,"Paused blood draw is pixel-identical");
      clear(0);const auto blocked=pixels();
      {Sentinel sentinel;DrawEnemyEffectsGl(identity,site.center,site.scale,bestQuads);sentinel.check();}
      require(pixels()==blocked,"Blood is occluded by nearer opaque depth");
      egl.fresh();ForgetEnemyEffectsGl();
      {Sentinel sentinel;CreateEnemyEffectsGl(data);sentinel.check();}
      require(simulation.restore(pausedState),"Original dynamic blood snapshot restored");
      clear();{Sentinel sentinel;DrawEnemyEffectsGl(identity,site.center,site.scale,
        WorldEffectQuads(data,simulation.state(),camera));sentinel.check();}
      require(pixels()==best,"Fresh EGL upload preserves exact paused blood frame");
      DestroyEnemyEffectsGl();require(glGetError()==GL_NO_ERROR,"Clean enemy GPU teardown");
      std::cout<<"EST0x"<<std::hex<<bank.sequence<<std::dec<<" red pixels="<<visible<<'\n';
    }
    require(drawnAnimations.size()==data.sprites.animations.size(),"All22 actual source texture animations drawn");
    std::cout<<"M33 actual EM12 blood GLES3 PASS: "<<checks<<'\n';return 0;
  }catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
