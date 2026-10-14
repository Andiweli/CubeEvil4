// Actual room FCVs and all three EM12 families through production GLES.
#include "actor_gl.h"
#include "archive.h"
#include "enemy_spawn.h"
#include "room_lighting_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace re4;
namespace {
constexpr unsigned W=256,H=256;
unsigned checks;
void require(bool v,const char *s) {++checks;if(!v)throw std::runtime_error(s);}
struct Egl {
 EGLDisplay d=EGL_NO_DISPLAY;EGLContext c=EGL_NO_CONTEXT;EGLSurface s=EGL_NO_SURFACE;EGLConfig config{};
 Egl() {
  setenv("EGL_PLATFORM","surfaceless",1);d=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint major,minor,n;
  require(eglInitialize(d,&major,&minor),"EGL initialize");require(eglBindAPI(EGL_OPENGL_ES_API),"EGL GLES API");
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
 ~Egl() {eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,c);eglDestroySurface(d,s);eglTerminate(d);}
};
std::vector<std::uint8_t> read(const char *path) {
 std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("ESL missing");
 std::vector<std::uint8_t> b(std::size_t(f.tellg()));f.seekg(0);
 if(!f.read(reinterpret_cast<char*>(b.data()),b.size()))throw std::runtime_error("ESL truncated");
 return b;
}
std::vector<std::uint8_t> render(const EnemyPreview *enemy) {
 glViewport(0,0,W,H);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
 glDepthMask(GL_TRUE);glClearColor(.14f,.16f,.18f,1);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 if(enemy) {
  constexpr float camera[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};SmdPlacement placement;placement.scale={1,1,1};
  const auto center=TransformPoint(enemy->rig().bones().at(0),{0,0,0});
  DrawEnemyGl(camera,{center[0],center[1]+850,center[2]},.0006f,*enemy,PlacementMatrix(placement),false);
 }
 std::vector<std::uint8_t> pixels(W*H*4);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
 require(glGetError()==GL_NO_ERROR,"Production cliff actor shader clean");return pixels;
}
unsigned changed(const std::vector<std::uint8_t> &a,const std::vector<std::uint8_t> &b) {
 unsigned n=0;for(unsigned p=0;p<a.size();p+=4)if(a[p]!=b[p]||a[p+1]!=b[p+1]||a[p+2]!=b[p+2])++n;return n;
}
}
int main(int argc,char **argv) {
 try {
  if(argc!=4)throw std::runtime_error("Usage: m37_r100_cliff_gl_check EM12 ESL R100");
  Egl egl;const EnemySpawns profiles(read(argv[2]));const auto room=RoomArchive::Load(argv[3]);
  const auto background=render(nullptr);
  std::vector<std::unique_ptr<EnemyPreview>> actors;std::vector<std::vector<std::uint8_t>> saved;
  for(unsigned index=6;index<=8;++index) {
   actors.push_back(std::make_unique<EnemyPreview>(argv[1],*profiles.find(index),true));auto &actor=*actors.back();
   actor.installRoomEventMotion(room.copy(index+36));actor.start(EnemyMotion::RoomEvent);
   for(unsigned t=0;t<25;++t)actor.advance();
   CreateEnemyGl(actor);
   saved.push_back(render(&actor));const auto full=changed(background,saved.back());
   require(full>500,"Source room cliff pose renders original body/head/hands");
   const auto originalPose=actor.rig().jointMatrices();actor.setOpacity(.5f);const auto partial=render(&actor);
   require(changed(background,partial)<full && changed(background,partial)>full/4,"Original visibility factor reaches production actor coverage");
   actor.setOpacity(0);require(render(&actor)==background,"Finished source cliff actor has no framebuffer coverage");
   actor.setOpacity(1);require(render(&actor)==saved.back() && actor.rig().jointMatrices()==originalPose,"Visibility changes preserve authored paused pose and GPU assets");
   for(unsigned i=0;i<actors.size();++i)require(render(actors[i].get())==saved[i],"Independent cliff GPU owners preserve earlier room motion and palette");
  }
  ForgetEnemyGl();egl.fresh();for(auto &actor:actors)CreateEnemyGl(*actor);
  for(unsigned i=0;i<actors.size();++i)require(render(actors[i].get())==saved[i],"Context recovery preserves paused room-authored cliff pose");
  DestroyEnemyGl();require(glGetError()==GL_NO_ERROR,"Cliff actor GPU teardown clean");
  std::cout<<"M37 actual r100 cliff GPU/fade PASS: "<<checks<<" checks; owners=3\n";return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
