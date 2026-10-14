// Seven authored EM12 owners through the production GLES pass.
#include "actor_gl.h"
#include "enemy_spawn.h"
#include "room_lighting_gl.h"
#include <memory>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
constexpr unsigned W=512,H=512;
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
 const auto &light=RoomLightBindings();
 const auto sourceCenter=TransformPoint(PlacementMatrix(placement),
     TransformPoint(enemy.rig().bones().at(0),{0,0,0}));
 require(light.enable==2,"Original em10 LightInfo mask2, independently of Leon mask1");
 require(light.position==sourceCenter,"Light center follows actual posed source Parts0 instead of fixed body offset");
 require(light.size==2000,"Documented enclosing radius from original Size1000/1000");
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
std::vector<std::uint8_t> read(const char *path) {
 std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("Original ESL missing");
 std::vector<std::uint8_t> b(std::size_t(file.tellg()));file.seekg(0);
 if(!file.read(reinterpret_cast<char*>(b.data()),b.size()))throw std::runtime_error("Original ESL truncated");
 return b;
}
int main(int argc,char **argv) {
 try {
  if(argc!=4)throw std::runtime_error("Usage: r100_group_gl_check EM12 ESL OUTPUT.ppm");
  Egl egl;const EnemySpawns profiles(read(argv[2]));
  std::vector<std::unique_ptr<EnemyPreview>> actors;std::vector<std::vector<std::uint8_t>> saved;
  const unsigned indices[]{3,4,5,0x12,0x13,0x26,0x27};
  for(unsigned i=0;i<7;++i) {
   actors.push_back(std::make_unique<EnemyPreview>(argv[1],*profiles.find(indices[i]),true));
   auto &actor=*actors.back();actor.start(actor.weaponType()==1?EnemyMotion::HoeAttackHigh:EnemyMotion::AxeAttack);
   for(unsigned tick=0;tick<20+i*5;++tick)actor.advance();
   CreateEnemyGl(actor);saved.push_back(render(actor));
   for(unsigned owner=0;owner<=i;++owner)require(render(*actors[owner])==saved[owner],"Another owner GPU creation preserves every prior body/tool palette");
  }
  for(unsigned i=1;i<7;++i)require(changed(saved[0],saved[i])>1000,"Distinct original type/tool poses render independently");
  // All seven draw in one framebuffer, with independent viewport placement.
  glClearColor(.14f,.16f,.18f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  constexpr float camera[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  SmdPlacement placement;placement.scale={1,1,1};const auto world=PlacementMatrix(placement);
  for(unsigned i=0;i<7;++i) {
   glViewport((i%4)*128,(i/4)*256,128,256);
   DrawEnemyGl(camera,{0,1000,0},.00065f,*actors[i],world,false);
  }
  std::vector<std::uint8_t> together(W*H*4);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,together.data());ppm(argv[3],together);
  require(glGetError()==GL_NO_ERROR,"Seven actor owners submit in one GLES world pass");
  DestroyEnemyGl(*actors[3]);
  for(unsigned i=0;i<7;++i)if(i!=3)require(render(*actors[i])==saved[i],"One owner destruction preserves six independent GPU actors");
  CreateEnemyGl(*actors[3]);require(render(*actors[3])==saved[3],"One GPU owner rebuild preserves source frame/pose");
  ForgetEnemyGl();egl.fresh();for(auto &actor:actors)CreateEnemyGl(*actor);
  for(unsigned i=0;i<7;++i)require(render(*actors[i])==saved[i],"Context recovery preserves every paused family/tool framebuffer");
  DestroyEnemyGl();require(glGetError()==GL_NO_ERROR,"Group actor resource teardown clean");
  std::cout<<"M36 seven real r100 GPU actors PASS: "<<checks<<" checks; owners=7 families=4 tools=3\n";return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
