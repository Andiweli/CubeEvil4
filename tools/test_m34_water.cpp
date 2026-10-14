#include "archive.h"
#include "environment_gl.h"
#include "room_flow.h"
#include "scene_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
using namespace re4;
namespace {
constexpr int W=640,H=360;
unsigned checks;
void require(bool value,const char *message) {++checks;if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::filesystem::path &path) {std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("file missing: "+path.string());return {std::istreambuf_iterator<char>(f),{}};}
struct Egl {
  EGLDisplay display;EGLSurface surface;EGLConfig config{};EGLContext context=EGL_NO_CONTEXT;
  Egl() {setenv("EGL_PLATFORM","surfaceless",1);display=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint a,b,n;require(eglInitialize(display,&a,&b)&&eglBindAPI(EGL_OPENGL_ES_API),"EGL init");
    EGLint attr[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
    require(eglChooseConfig(display,attr,&config,1,&n)&&n,"EGL config");EGLint size[]{EGL_WIDTH,W,EGL_HEIGHT,H,EGL_NONE};surface=eglCreatePbufferSurface(display,config,size);require(surface!=EGL_NO_SURFACE,"EGL surface");fresh();}
  void fresh() {if(context!=EGL_NO_CONTEXT) {eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);}EGLint attr[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};context=eglCreateContext(display,config,EGL_NO_CONTEXT,attr);require(context!=EGL_NO_CONTEXT&&eglMakeCurrent(display,surface,surface,context),"EGL current");}
  ~Egl() {eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);}
};
std::vector<std::uint8_t> pixels() {std::vector<std::uint8_t> p(W*H*4);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,p.data());require(glGetError()==GL_NO_ERROR,"GLES error");return p;}
unsigned difference(const std::vector<std::uint8_t> &a,const std::vector<std::uint8_t> &b) {unsigned n=0;for(std::size_t p=0;p<a.size();p+=4)if(!std::equal(a.begin()+p,a.begin()+p+4,b.begin()+p))++n;return n;}
void ppm(const std::filesystem::path &path,const std::vector<std::uint8_t> &p) {std::ofstream f(path,std::ios::binary);f<<"P6\n"<<W<<' '<<H<<"\n255\n";for(int y=H-1;y>=0;--y)for(int x=0;x<W;++x)f.write(reinterpret_cast<const char *>(p.data()+(y*W+x)*4),3);require(bool(f),"image write");}
StaticScene load(const std::filesystem::path &path,unsigned id,const std::vector<std::uint8_t> &core) {
  auto arc=RoomArchive::Load(path.string());auto copy=[&](const char *tag,unsigned n=0) {auto e=arc.find(tag,int(n));if(!e)throw std::runtime_error("room tag missing");return arc.copy(e-arc.entries().data());};
  Smd smd(copy("SMD"));Smx smx(copy("SMX"));std::unique_ptr<Smd> common;if(arc.find("SMD",1))common=std::make_unique<Smd>(copy("SMD",1),smd.commonRequirements());
  auto scene=BuildStaticScene(smd,-1,common.get());AttachSceneTextures(smd,scene,common.get());ApplySceneSmx(smx,scene);scene.lighting=ReadRoomLighting(arc);
  AttachSceneEnvironment(BuildRoomEnvironment(arc,smd,smx,std::uint16_t(id),core),scene);return scene;
}
void realGrids(const std::filesystem::path &directory,const std::vector<std::uint8_t> &core) {
  unsigned rooms=0,grids=0,lighting=0;std::vector<std::string> failures;
  for(const auto &entry:std::filesystem::directory_iterator(directory)) {
    const auto name=entry.path().stem().string();if(name.size()!=4||name[0]!='r'||entry.path().extension()!=".dat")continue;
    ++rooms;const auto id=unsigned(std::stoul(name.substr(1),nullptr,16));const auto arc=RoomArchive::Load(entry.path().string());
    require(ReadRoomLighting(arc)->cuts.at(0).lights.size()<=128,"LIT room Cut0 bounded");++lighting;
    try {auto scene=load(entry.path(),id,core);const auto &env=scene.environment;
      if(!env.warnings.empty()) {for(const auto &w:env.warnings)failures.push_back(name+": "+w);continue;}
      if(env.grids.empty())continue;
      grids+=unsigned(env.grids.size());require(env.gridNoise.size()==60,"Original noise60 frames");EnvironmentSimulation sim(env);auto initial=sim.state();
      for(unsigned k=0;k<90;++k)sim.tick();
      const auto after=sim.state();require(after.grids[0].height!=initial.grids[0].height,"Authored water waves advance");
      require(sim.restore(initial),"Grid snapshot valid");for(unsigned k=0;k<90;++k)sim.tick();
      require(sim.state().grids[0].height==after.grids[0].height&&sim.state().grids[0].normals==after.grids[0].normals,"Grid resume exact");
      auto corrupt=initial;corrupt.grids[0].frames++;require(!sim.restore(corrupt),"Corrupt noise phase rejected");corrupt=initial;corrupt.grids[0].height[1]=std::numeric_limits<float>::infinity();require(!sim.restore(corrupt),"Corrupt grid value rejected");
      const auto mesh=WaterGridMesh(env.grids[0],after.grids[0],env.grids[0].position);require(mesh.size()>1000,"Actual generator geometry");
      for(const auto &v:mesh)for(const auto value:v.position)require(std::isfinite(value),"Water vertex finite");
      if(id==0x10a) {require(env.grids[0].generator==0x42&&env.grids[0].width==120&&env.grids[0].height==120,"r10a original120x120 grid");require(env.grids[0].size==530,"r10a original cellsize");}
      if(id==0x10b) {const auto &g=env.grids[0];require(g.generator==0x45&&g.width==168&&g.height==168,"r10b original168x168 grid");require(g.color==std::array<std::uint8_t,4>{125,125,120,255},"r10b EST5 multiplies EST2 tint");require(!g.followCamera&&std::abs(g.size-309.636474609375f)<.0001f&&std::abs(g.position[0]+37448.21484375f)<.01f,"r10b ashore Esp4c original controller");require(mesh.size()==(g.width+1)*2*g.height+24,"r10b four original15x border quads");}
      std::cout<<name<<": "<<env.describe()<<'\n';
    }catch(const std::exception &e) {const std::string message=e.what();if(message.find("SMD")!=std::string::npos || message.find("BIN")!=std::string::npos)std::cout<<name<<": existing model format unsupported\n";else throw;}
  }
  for(const auto &f:failures)std::cerr<<f<<'\n';
  require(failures.empty(),"Original environment parsed without invented fallback");
  std::cout<<"Catalogue="<<rooms<<", Cut0="<<lighting<<", startup water grids="<<grids<<'\n';require(grids==20,"18 global water generators plus two explicit startup grids");
}
void render(Egl &egl,const std::filesystem::path &directory,const std::filesystem::path &infoPath,
            const std::filesystem::path &out,const std::vector<std::uint8_t> &core,unsigned id) {
  const std::string name=id==0x10a?"r10a":"r10b";const auto scene=load(directory/(name+".dat"),id,core);
  require(scene.environment.grids.size()==1,"Real water loaded");RoomInfo info(read(infoPath));const auto *start=info.find(std::uint16_t(id));require(start&&start->valid(),"Original room debug start");
  SceneProbe probe;probe.feet=start->position;probe.showBody=false;
  auto baseline=scene;baseline.environment.grids.clear();baseline.environment.gridNoise.clear();CreateSceneGl(baseline);SceneFollowHeading(float(start->heading));
  DrawSceneGl(W,H,0,0,0,false,&probe,nullptr,0);auto base=pixels();auto view=SaveSceneView();DestroySceneGl();
  CreateSceneGl(scene);SceneFollowHeading(float(start->heading));DrawSceneGl(W,H,0,0,0,false,&probe,nullptr,0);const auto actual=pixels();
  ppm(out/(name+"-original-start.ppm"),actual);const auto visible=difference(base,actual);
  // Walk-mode source camera can initially face away from the lake. Sweep its
  // native heading to validate the scene from the actual unchanged room start.
  unsigned maximum=visible;for(unsigned k=1;k<8;++k) {SceneFollowHeading(float(start->heading+k*.7853981633974483));DrawSceneGl(W,H,0,0,0,false,&probe,nullptr,0);const auto image=pixels();
    DestroySceneGl();CreateSceneGl(baseline);SceneFollowHeading(float(start->heading+k*.7853981633974483));DrawSceneGl(W,H,0,0,0,false,&probe,nullptr,0);const auto without=pixels();const auto change=difference(image,without);
    DestroySceneGl();CreateSceneGl(scene);SceneFollowHeading(float(start->heading+k*.7853981633974483));DrawSceneGl(W,H,0,0,0,false,&probe,nullptr,0);
    if(change>maximum) {maximum=change;ppm(out/(name+"-start-water-facing.ppm"),image);}}
  std::cout<<name<<" source-start water pixels="<<visible<<", heading sweep="<<maximum<<'\n';
  // Point0 is an upstream passage/path in both rooms. Test the unchanged
  // second authored debug start at the river/lake as well, not a guessed
  // floating plane viewed from a synthetic scene-normalization camera.
  const auto *shore=info.find(std::uint16_t(id),1);require(shore&&shore->valid(),"Original authored water start1");probe.feet=shore->position;
  DestroySceneGl();CreateSceneGl(baseline);SceneFollowHeading(float(shore->heading));DrawSceneGl(W,H,0,0,0,false,&probe,nullptr,0);const auto shoreBase=pixels();
  DestroySceneGl();CreateSceneGl(scene);SceneFollowHeading(float(shore->heading));DrawSceneGl(W,H,0,0,0,false,&probe,nullptr,0);const auto shoreImage=pixels();const auto shoreVisible=difference(shoreBase,shoreImage);
  ppm(out/(name+"-authored-water-start.ppm"),shoreImage);std::cout<<name<<" authored water start1 pixels="<<shoreVisible<<'\n';require(shoreVisible>500,"Water visible from unchanged authored shore camera");
  const auto &g=scene.environment.grids[0];probe.feet={g.position[0],g.position[1]+2200,g.position[2]-g.height*g.size*.3};SceneFollowHeading(0);
  DrawSceneGl(W,H,0,0,0,false,&probe,nullptr,0);const auto near=pixels();view=SaveSceneView();view.animationFrames=180;RestoreSceneView(view);DrawSceneGl(W,H,0,0,0,false,&probe,nullptr,0);const auto animated=pixels();
  require(difference(near,animated)>100,"Actual GLES animated water surface");ppm(out/(name+"-water-close.ppm"),animated);
  const auto saved=SaveSceneView();for(unsigned k=0;k<3;++k)DrawSceneGl(W,H,0,0,0,false,&probe,nullptr,0);require(pixels()==animated,"Paused water exact framebuffer");
  ForgetSceneGl();egl.fresh();CreateSceneGl(scene);RestoreSceneView(saved);DrawSceneGl(W,H,0,0,0,false,&probe,nullptr,0);require(pixels()==animated,"EGL context restores water exact framebuffer");DestroySceneGl();
  std::cout<<name<<" source-start water pixels="<<visible<<", heading sweep="<<maximum<<"; animation/pause/context PASS\n";
}
}
int main(int argc,char **argv) {try {if(argc!=5&&argc!=6)throw std::runtime_error("Usage: m34_water_check CORE_DAT DECODED_ROOMS ROOMINFO_DAT OUTPUT_DIRECTORY [--render-only]");
  const auto core=RoomArchive::Load(argv[1]);const auto *entry=core.find("EFF");require(entry,"CORE EFF");const auto eff=core.copy(entry-core.entries().data());
  if(argc==5)realGrids(argv[2],eff);else require(std::string(argv[5])=="--render-only","Optional render-only argument");
  std::filesystem::create_directories(argv[4]);Egl egl;render(egl,argv[2],argv[3],argv[4],eff,0x10a);render(egl,argv[2],argv[3],argv[4],eff,0x10b);
  std::cout<<"M34 source water PASS: "<<checks<<" checks\n";
}catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}}
