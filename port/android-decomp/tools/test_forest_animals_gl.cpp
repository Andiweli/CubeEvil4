#include "forest_animals.h"
#include "actor_gl.h"
#include "room_lighting_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
using namespace re4;
namespace {
constexpr unsigned W=640,H=480;
unsigned checks;
void require(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
struct Egl {
 EGLDisplay d{};EGLContext c{};EGLSurface s{};EGLConfig conf{};
 Egl(){setenv("EGL_PLATFORM","surfaceless",1);d=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint a,b,n;
  require(eglInitialize(d,&a,&b)&&eglBindAPI(EGL_OPENGL_ES_API),"EGL init");
  const EGLint attrs[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
  require(eglChooseConfig(d,attrs,&conf,1,&n)&&n,"EGL config");
  const EGLint size[]{EGL_WIDTH,W,EGL_HEIGHT,H,EGL_NONE};s=eglCreatePbufferSurface(d,conf,size);fresh();
 }
 void fresh(){if(c){eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,c);}
  const EGLint attrs[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};c=eglCreateContext(d,conf,EGL_NO_CONTEXT,attrs);require(c&&eglMakeCurrent(d,s,s,c),"EGL context");}
 ~Egl(){eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,c);eglDestroySurface(d,s);eglTerminate(d);}
};
Matrix34 viewPlacement(const ForestCrow &crow){SmdPlacement p;p.scale={crow.scale(),crow.scale(),crow.scale()};p.rotation={-.5f,.4f,0};return PlacementMatrix(p);}
std::vector<unsigned char> render(const ForestAnimals &forest,unsigned slot,unsigned wing) {
 const auto &crow=forest.animals()[slot];glViewport(0,0,W,H);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
 glDepthMask(GL_TRUE);glClearColor(.15f,.17f,.2f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 constexpr float camera[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};const auto placement=viewPlacement(crow);
 const auto light=TransformPoint(placement,TransformPoint(crow.rig().jointMatrices()[0],{0,0,0}));
 DrawGenericActorGl(crow.gpuOwner(wing),camera,{0,100,0},.001f,crow.rig().bones(),crow.rig().bindCenters(),placement,false,2,&light,2000);
 require(RoomLightBindings().enable==2&&RoomLightBindings().position==light,"source crow mask/partcenter");
 std::vector<unsigned char> p(W*H*4);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,p.data());require(glGetError()==GL_NO_ERROR,"crow GLES render");return p;
}
unsigned changed(const std::vector<unsigned char> &a,const std::vector<unsigned char> &b){unsigned n=0;for(unsigned p=0;p<a.size();p+=4)n+=a[p]!=b[p]||a[p+1]!=b[p+1]||a[p+2]!=b[p+2];return n;}
double capture(const ForestAnimals &f,unsigned slot,unsigned wing) {
 render(f,slot,wing);const auto &crow=f.animals()[slot];const auto &m=f.mesh(wing);const auto &skin=f.skin(wing);
 const auto &pose=crow.rig().bones();const auto &bind=crow.rig().bindCenters();const auto matrix=viewPlacement(crow);const auto count=skin.size();
 GLuint buffer;glGenBuffers(1,&buffer);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,count*6*sizeof(float),nullptr,GL_STREAM_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
 glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_TRIANGLES);glDrawArrays(GL_TRIANGLES,0,GLsizei(count));glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);
 require(glGetError()==GL_NO_ERROR,"crow skin capture");auto gpu=static_cast<const float*>(glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,0,count*6*sizeof(float),GL_MAP_READ_BIT));require(gpu,"crow capture map");double error=0;
 for(unsigned i=0;i<count;++i) {
  std::array<double,3> p{},n{};
  for(unsigned w=0;w<3;++w)for(unsigned row=0;row<3;++row) {
   const auto pi=skin[i].position.joints[w],ni=skin[i].normal.joints[w];double value=pose[pi][row*4+3];
   for(unsigned col=0;col<3;++col){value+=double(pose[pi][row*4+col])*(m.vertices[i*3+col]-bind[pi][col]);n[row]+=double(skin[i].normal.rates[w])*pose[ni][row*4+col]*m.normals[i*3+col];}
   p[row]+=double(skin[i].position.rates[w])*value;
  }
  for(unsigned row=0;row<3;++row) {
   double normal=0;for(unsigned col=0;col<3;++col)normal+=matrix[row*4+col]*n[col];
   require(std::isfinite(gpu[i*6+row])&&std::isfinite(gpu[i*6+3+row]),"nonfinite crow skin");
   error=std::max(error,std::abs(p[row]-gpu[i*6+row]));error=std::max(error,std::abs(normal-gpu[i*6+3+row]));
  }
 }
 require(glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER),"crow capture integrity");glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glDeleteBuffers(1,&buffer);require(error<.02,"crow body/wing GPU differs from source palette");return error;
}
void ppm(const char *path,const std::vector<unsigned char> &p){std::ofstream o(path,std::ios::binary);o<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int y=H-1;y>=0;--y)for(unsigned x=0;x<W;++x)o.write(reinterpret_cast<const char*>(&p[(unsigned(y)*W+x)*4]),3);require(bool(o),"crow framebufferexport");}
}
int main(int argc,char **argv){try{
 require(argc==5,"Usage: forest_animals_gl_check R100.DAT ESL EM23.DRS OUTPUT.PPM");Egl egl;
 const auto room=RoomArchive::Load(argv[1]);const auto *entry=room.find("SAT");require(entry,"source SATmissing");const Sat raw(room.copy(entry-room.entries().data()));const SatCollision sat(raw);
 std::ifstream in(argv[2],std::ios::binary);std::vector<std::uint8_t> esl((std::istreambuf_iterator<char>(in)),{});ForestAnimals forest(sat,esl,argv[3]);
 auto upload=[&](){for(const auto &a:forest.animals())for(unsigned wing=0;wing<2;++wing)CreateGenericActorGl(a.gpuOwner(wing),forest.mesh(wing),forest.skin(wing));};upload();
 const auto closed=render(forest,0,1),open=render(forest,0,0);require(changed(closed,open)>1000,"source interchangeable wingmodels indistinguishable");
 double error=0;for(unsigned slot=0;slot<5;++slot)for(unsigned wing=0;wing<2;++wing)error=std::max(error,capture(forest,slot,wing));
 auto near=forest.animals()[0].feet();near[0]+=1000;for(unsigned i=0;i<70;++i)forest.tick(near);
 require(forest.animals()[0].routine()==CrowRoutine::Turn&&forest.animals()[0].wing()==0,"source takeoff changes visiblewingmodel");const auto flight=render(forest,0,0);require(changed(closed,flight)>1000,"crow motion remainsstatic");
 for(unsigned slot=0;slot<5;++slot)for(unsigned wing=0;wing<2;++wing)error=std::max(error,capture(forest,slot,wing));
 ForestAnimals second(sat,esl,argv[3]);for(const auto &a:second.animals())for(unsigned w=0;w<2;++w)CreateGenericActorGl(a.gpuOwner(w),second.mesh(w),second.skin(w));require(render(forest,0,0)==flight,"second crowcollection overwrote original GPUowners");
 for(const auto &a:second.animals())for(unsigned w=0;w<2;++w)DestroyGenericActorGl(a.gpuOwner(w));
 require(render(forest,0,0)==flight,"otherowner teardown destroyed pausedcrow");
 ForgetGenericActorGl();egl.fresh();upload();require(render(forest,0,0)==flight,"EGL rebuild changed paused originalcrow pose/pixels");ppm(argv[4],flight);DestroyGenericActorGl();require(glGetError()==GL_NO_ERROR,"crow GPUteardown");
 std::cout<<"M37 forest GLES: "<<checks<<" checks PASS, five2-variant owners; source24part skin max error "<<error<<".\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
