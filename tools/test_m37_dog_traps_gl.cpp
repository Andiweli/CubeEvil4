// Actual EM21/EM2A, including no-FCV stretched wires, through production GLES.
#include "dog_traps.h"
#include "actor_gl.h"
#include "archive.h"
#include "room_lighting_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
constexpr unsigned W=320,H=320;
unsigned checks;
void require(bool value,const char *why) {++checks;if(!value)throw std::runtime_error(why);}
struct Egl {
  EGLDisplay display=EGL_NO_DISPLAY;EGLContext context=EGL_NO_CONTEXT;
  EGLSurface surface=EGL_NO_SURFACE;EGLConfig config{};
  Egl() {
    setenv("EGL_PLATFORM","surfaceless",1);display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major,minor,n;
    require(eglInitialize(display,&major,&minor) && eglBindAPI(EGL_OPENGL_ES_API),"EGL init");
    const EGLint attributes[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
      EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
    require(eglChooseConfig(display,attributes,&config,1,&n) && n,"EGL config");
    const EGLint size[]{EGL_WIDTH,W,EGL_HEIGHT,H,EGL_NONE};
    surface=eglCreatePbufferSurface(display,config,size);require(surface!=EGL_NO_SURFACE,"EGL surface");fresh();
  }
  void fresh() {
    if(context!=EGL_NO_CONTEXT) {
      eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);
    }
    const EGLint attributes[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,attributes);
    require(context!=EGL_NO_CONTEXT && eglMakeCurrent(display,surface,surface,context),"EGL context");
  }
  ~Egl() {
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);
  }
};
std::vector<std::uint8_t> read(const char *path) {
  std::ifstream file(path,std::ios::binary);require(bool(file),"Actual ESL exists");
  return {std::istreambuf_iterator<char>(file),{}};
}
Matrix34 viewingPlacement() {
  SmdPlacement placement;placement.scale={1,1,1};placement.rotation={-.55f,.6f,0};return PlacementMatrix(placement);
}
std::array<double,3> position(const GenericEventActor &actor,unsigned corner) {
  std::array<double,3> out{};const auto &skin=actor.skin()[corner].position;
  const auto &pose=actor.rig()->bones();const auto &bind=actor.rig()->bindCenters();
  for(unsigned weight=0;weight<3;++weight)for(unsigned row=0;row<3;++row) {
    const auto joint=skin.joints[weight];double value=pose[joint][row*4+3];
    for(unsigned col=0;col<3;++col)
      value+=double(pose[joint][row*4+col])*(actor.mesh().vertices[corner*3+col]-bind[joint][col]);
    out[row]+=double(skin.rates[weight])*value;
  }
  return out;
}
std::vector<std::uint8_t> render(const GenericEventActor *actor) {
  glViewport(0,0,W,H);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  glDepthMask(GL_TRUE);glClearDepthf(1);glClearColor(.15f,.17f,.2f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  if(actor) {
    const auto placement=viewingPlacement();std::array<float,3> low{1e9f,1e9f,1e9f},high{-1e9f,-1e9f,-1e9f};
    for(unsigned corner=0;corner<actor->skin().size();++corner) {
      const auto local=position(*actor,corner);std::array<float,3> value{};
      for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)value[row]+=placement[row*4+col]*float(local[col]);
      for(unsigned k=0;k<3;++k) {low[k]=std::min(low[k],value[k]);high[k]=std::max(high[k],value[k]);}
    }
    float span=0;std::array<float,3> center{};
    for(unsigned k=0;k<3;++k) {span=std::max(span,high[k]-low[k]);center[k]=(low[k]+high[k])*.5f;}
    require(span>0,"Actual source posed model has extent");
    constexpr float camera[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    DrawGenericActorGl(actor,camera,center,1.6f/span,actor->rig()->bones(),actor->rig()->bindCenters(),
        placement,false,2,&center,8000);
    require(RoomLightBindings().enable==2,"Original source light selector2");
  }
  std::vector<std::uint8_t> pixels(W*H*4);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
  require(glGetError()==GL_NO_ERROR,"Production dog/trap shader clean");return pixels;
}
unsigned changed(const std::vector<std::uint8_t> &a,const std::vector<std::uint8_t> &b) {
  unsigned n=0;for(unsigned i=0;i<a.size();i+=4)n+=a[i]!=b[i] || a[i+1]!=b[i+1] || a[i+2]!=b[i+2];return n;
}
double capture(const GenericEventActor &actor) {
  render(&actor);const auto count=actor.skin().size();const auto placement=viewingPlacement();GLuint buffer;
  glGenBuffers(1,&buffer);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,count*6*sizeof(float),nullptr,GL_STREAM_READ);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);glEnable(GL_RASTERIZER_DISCARD);
  glBeginTransformFeedback(GL_TRIANGLES);glDrawArrays(GL_TRIANGLES,0,GLsizei(count));
  glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);require(glGetError()==GL_NO_ERROR,"Skin capture");
  const auto gpu=static_cast<const float*>(glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,0,count*6*sizeof(float),GL_MAP_READ_BIT));
  require(gpu,"Skin capture map");double error=0;
  for(unsigned corner=0;corner<count;++corner) {
    const auto expected=position(actor,corner);std::array<double,3> normal{};
    const auto &weights=actor.skin()[corner].normal;const auto &pose=actor.rig()->bones();
    for(unsigned weight=0;weight<3;++weight)for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)
      normal[row]+=double(weights.rates[weight])*pose[weights.joints[weight]][row*4+col]*actor.mesh().normals[corner*3+col];
    for(unsigned row=0;row<3;++row) {
      double posedNormal=0;for(unsigned col=0;col<3;++col)posedNormal+=placement[row*4+col]*normal[col];
      require(std::isfinite(gpu[corner*6+row]) && std::isfinite(gpu[corner*6+3+row]),"Finite actual source GPU palette");
      error=std::max(error,std::abs(expected[row]-gpu[corner*6+row]));
      error=std::max(error,std::abs(posedNormal-gpu[corner*6+3+row]));
    }
  }
  require(glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER),"Skin capture integrity");glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glDeleteBuffers(1,&buffer);
  require(error<.03,"Original dog/trap GPU differs from independent source palette");return error;
}
}
int main(int argc,char **argv) {
  try {
    require(argc==7,"Usage: m37_dog_traps_gl_check R100 ESL EM21 EM2A PL00 WEP01");Egl egl;
    const auto room=RoomArchive::Load(argv[1]);const auto *entry=room.find("SAT");require(entry,"Actual SAT exists");
    const Sat raw(room.copy(entry-room.entries().data()));const SatCollision sat(raw);const EnemySpawns spawns(read(argv[2]));
    ScenarioState scenario;DogTraps world(sat,spawns,argv[3],argv[4],scenario);world.releaseAfterTruck();
    const auto upload=[&]() {for(const auto &actor:world.actors())CreateGenericActorGl(actor.get(),actor->mesh(),actor->skin());};upload();
    const auto background=render(nullptr);double error=0;
    for(const auto &actor:world.actors()) {
      require(changed(background,render(actor.get()))>20,"Actual dog/trap source model visibly renders");
      error=std::max(error,capture(*actor));
    }
    LeonPreview player(argv[5],argv[6]);world.installPlayerMotions(player);
    WalkProbe probe(sat,WalkConfig{400,1600,2600,250,600,true});require(probe.reset(),"Original SAT has a validated start floor");
    probe.setScriptPosition(world.sources()[0].position);
    player.setPlacement(probe.position(),float(world.sources()[0].heading));require(world.beginRescue(player,probe),"Actual rescue begins");
    const auto before=render(world.actors()[0].get());
    for(unsigned i=0;i<90;++i) {world.tick(player,probe);player.step(probe,0,0,false,false,1./30,{});}
    const auto rescued=render(world.actors()[0].get());require(changed(before,rescued)>100,"Original rescue FCV changes source GPU pose");
    for(const auto &actor:world.actors())error=std::max(error,capture(*actor));
    ScenarioState otherScenario;DogTraps second(sat,spawns,argv[3],argv[4],otherScenario);
    for(const auto &actor:second.actors())CreateGenericActorGl(actor.get(),actor->mesh(),actor->skin());
    require(render(world.actors()[0].get())==rescued,"Independent static and animated owner uploads preserve paused dog");
    for(const auto &actor:second.actors())DestroyGenericActorGl(actor.get());
    require(render(world.actors()[0].get())==rescued,"Other owner teardown preserves paused dog");
    ForgetGenericActorGl();egl.fresh();upload();require(render(world.actors()[0].get())==rescued,"EGL reconstruction preserves original dog pose and pixels");
    for(const auto &actor:world.actors())error=std::max(error,capture(*actor));
    DestroyGenericActorGl();require(glGetError()==GL_NO_ERROR,"GPU teardown clean");
    std::cout<<"M37 dog/trap GLES: "<<checks<<" checks PASS, all8 source owners; skin max error "<<error<<".\n";
    return 0;
  } catch(const std::exception &error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
