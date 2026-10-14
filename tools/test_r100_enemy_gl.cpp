// Actual EM12 first-house axe / left grip through the production GLES pass.
#include "actor_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
constexpr unsigned W=384,H=384;
unsigned checks;
void require(bool value,const char *message) {++checks;if(!value)throw std::runtime_error(message);}
struct Egl {
 EGLDisplay d=EGL_NO_DISPLAY;EGLContext c=EGL_NO_CONTEXT;EGLSurface s=EGL_NO_SURFACE;EGLConfig config{};
 Egl() {
  setenv("EGL_PLATFORM","surfaceless",1);d=eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint major,minor,n;require(eglInitialize(d,&major,&minor),"EGL initialize");
  require(eglBindAPI(EGL_OPENGL_ES_API),"EGL GLES API");
  const EGLint attributes[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
    EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
  require(eglChooseConfig(d,attributes,&config,1,&n)&&n,"EGL config");
  const EGLint size[]{EGL_WIDTH,W,EGL_HEIGHT,H,EGL_NONE};s=eglCreatePbufferSurface(d,config,size);
  require(s!=EGL_NO_SURFACE,"EGL framebuffer");fresh();
 }
 void fresh() {
  if(c!=EGL_NO_CONTEXT) {eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,c);}
  const EGLint attributes[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};c=eglCreateContext(d,config,EGL_NO_CONTEXT,attributes);
  require(c!=EGL_NO_CONTEXT && eglMakeCurrent(d,s,s,c),"EGL context");
 }
 ~Egl(){eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,c);eglDestroySurface(d,s);eglTerminate(d);}
};
std::vector<std::uint8_t> render(const EnemyPreview &enemy) {
 glViewport(0,0,W,H);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
 glDepthMask(GL_TRUE);glClearColor(.14f,.16f,.18f,1);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 constexpr float camera[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 SmdPlacement placement;placement.scale={1,1,1};
 DrawEnemyGl(camera,{0,1000,0},.00065f,enemy,PlacementMatrix(placement),false);
 std::vector<std::uint8_t> pixels(W*H*4);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
 require(glGetError()==GL_NO_ERROR,"Production enemy shader clean");return pixels;
}
unsigned changed(const std::vector<std::uint8_t> &a,const std::vector<std::uint8_t> &b) {
 unsigned count=0;for(unsigned p=0;p<a.size();p+=4)if(a[p]!=b[p]||a[p+1]!=b[p+1]||a[p+2]!=b[p+2])++count;return count;
}
void ppm(const char *path,const std::vector<std::uint8_t> &image) {
 std::ofstream f(path,std::ios::binary);f<<"P6\n"<<W<<' '<<H<<"\n255\n";
 for(int y=int(H)-1;y>=0;--y)for(unsigned x=0;x<W;++x)f.write(reinterpret_cast<const char*>(&image[(unsigned(y)*W+x)*4]),3);
}
}
int main(int argc,char **argv) {
 try {
  if(argc!=3)throw std::runtime_error("Usage: r100_enemy_gl_check EM12.drs OUTPUT.ppm");
  Egl egl;EnemyPreview enemy(argv[1],0xff,true,true);enemy.start(EnemyMotion::AxeAttack);
  for(unsigned i=0;i<40;++i)enemy.advance();
  CreateEnemyGl(enemy);const auto held=render(enemy);ppm(argv[2],held);
  enemy.dropWeapon();const auto dropped=render(enemy);const auto axePixels=changed(held,dropped);
  require(axePixels>100,"Actual source axe/grip produce visible GPU pixels");
  DestroyEnemyGl();EnemyPreview paused(argv[1],0xff,true,true);paused.start(EnemyMotion::AxeAttack);
  for(unsigned i=0;i<40;++i)paused.advance();
  CreateEnemyGl(paused);
  require(render(paused)==held,"Held axe actor rebuild preserves paused pose/framebuffer");
  ForgetEnemyGl();egl.fresh();CreateEnemyGl(paused);
  require(render(paused)==held,"EGL context rebuild preserves exact source axe framebuffer");
  DestroyEnemyGl();require(glGetError()==GL_NO_ERROR,"Enemy resource teardown clean");
  std::cout<<"M35 real r100 axe GLES PASS: "<<checks<<" checks; axe/grip changedpixels="<<axePixels<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
