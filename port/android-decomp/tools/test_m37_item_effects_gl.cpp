#include "item_effects_gl.h"
#include "actor_gl.h"
#include "archive.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks;
void require(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);}
struct Egl {
  EGLDisplay display=EGL_NO_DISPLAY;EGLContext context=EGL_NO_CONTEXT;EGLSurface surface=EGL_NO_SURFACE;EGLConfig config{};
  Egl(){setenv("EGL_PLATFORM","surfaceless",1);display=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint major{},minor{};
    require(eglInitialize(display,&major,&minor),"item EGL initialize");require(eglBindAPI(EGL_OPENGL_ES_API),"item EGL API");
    const EGLint attributes[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
      EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};EGLint count{};
    require(eglChooseConfig(display,attributes,&config,1,&count)&&count,"item EGL config");create();}
  void create(){const EGLint c[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE},s[]{EGL_WIDTH,640,EGL_HEIGHT,360,EGL_NONE};
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,c);surface=eglCreatePbufferSurface(display,config,s);
    require(context!=EGL_NO_CONTEXT && surface!=EGL_NO_SURFACE && eglMakeCurrent(display,surface,surface,context),"item EGL current");}
  void drop(){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    if(surface!=EGL_NO_SURFACE)eglDestroySurface(display,surface);
    if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);
    surface=EGL_NO_SURFACE;context=EGL_NO_CONTEXT;}
  ~Egl(){drop();eglTerminate(display);}
};
std::vector<unsigned char> draw(re4::ItemEffects &fx,const re4::WorldEffectCamera &view){
  glViewport(0,0,640,360);glClearColor(0,0,0,1);glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  const float matrix[16]{.0015f,0,0,0,0,.0027f,0,0,0,0,.0001f,0,0,-.1f,0,1};
  re4::DrawItemEffectsGl(matrix,{0,0,0},1,view,fx);
  std::vector<unsigned char> pixels(640*360*4);glReadPixels(0,0,640,360,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
  require(glGetError()==GL_NO_ERROR,"original item GLES draw/read");return pixels;
}
unsigned nonblack(const std::vector<unsigned char> &pixels){unsigned count=0;
  for(unsigned i=0;i<pixels.size();i+=4)count+=pixels[i]||pixels[i+1]||pixels[i+2];
  return count;}
}
int main(int argc,char **argv){try{using namespace re4;
  require(argc==3,"Usage: m37_item_effects_gl_check CORE.DAS OUTPUT.ppm");
  const auto core=RoomArchive::LoadDrs(argv[1]);const auto *entry=core.find("EFF");require(entry,"CORE EFF");
  const auto bytes=core.copy(entry-core.entries().data());ItemEffects fx(bytes);
  ItemEffectSource source;source.key=1;source.id=0x78;source.effectType=2;source.position={0,0,0};
  WorldEffectCamera view;view.eye={0,200,1500};view.forward={0,0,-1};
  fx.sync({source});for(unsigned frame=0;frame<120;++frame)fx.tick(&view);
  require(!fx.effectModels().empty()&&!fx.quads(view).empty(),"original Efm04+nested/spark CPU outputs");
  Egl egl;CreateItemEffectsGl(fx);const auto before=fx.snapshot();const auto first=draw(fx,view);
  require(nonblack(first)>100,"original CORE model/sprites not visible");
  const auto paused=draw(fx,view);require(first==paused,"draw advanced original item clock/RNG");
  require(fx.ticks()==before.ticks && fx.snapshot().random==before.random,"GPU pass advanced CPU source state");
  ForgetItemEffectsGl();ForgetGenericActorGl();ForgetActorGl();egl.drop();egl.create();CreateItemEffectsGl(fx);
  require(draw(fx,view)==first,"context recreation changed exact paused pixels");
  ItemEffects restored(bytes);require(restored.restore(before),"source effect snapshot restore");
  require(draw(restored,view)==first,"separate item GPU owner changed paused pixels");
  fx.sync({});require(nonblack(draw(fx,view))==0,"pickup left original nested/model GPU effects");
  fx.sync({source});for(unsigned frame=0;frame<121;++frame)fx.tick(&view);
  require(draw(fx,view)!=first,"original item animation remained static");
  std::ofstream file(argv[2],std::ios::binary);require(bool(file),"item proof image");file<<"P6\n640 360\n255\n";
  for(int y=359;y>=0;--y)for(unsigned x=0;x<640;++x)file.write(reinterpret_cast<const char*>(first.data()+(y*640+x)*4),3);
  DestroyItemEffectsGl();DestroyGenericActorGl();DestroyActorGl();require(glGetError()==GL_NO_ERROR,"item GPU cleanup");
  std::cout<<"M37 original item effects GLES: "<<checks<<" checks PASS; source model/sprites, exact pause, independent owner and context recovery.\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;}}
