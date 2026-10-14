#include "archive.h"
#include "environment_gl.h"
#include "scene_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace re4;
namespace {
int Width=640,Height=360;
unsigned checks{};
void require(bool condition,const char *message) {++checks;if(!condition)throw std::runtime_error(message);}
struct Egl {
  EGLDisplay display{};EGLSurface surface{};EGLConfig config{};EGLContext context=EGL_NO_CONTEXT;
  Egl() {
    setenv("EGL_PLATFORM","surfaceless",1);display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major{},minor{},count{};require(eglInitialize(display,&major,&minor)&&eglBindAPI(EGL_OPENGL_ES_API),"EGL initialization");
    const EGLint attributes[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_STENCIL_SIZE,8,EGL_NONE};
    require(eglChooseConfig(display,attributes,&config,1,&count)&&count,"EGL configuration");
    const EGLint size[]{EGL_WIDTH,Width,EGL_HEIGHT,Height,EGL_NONE};surface=eglCreatePbufferSurface(display,config,size);require(surface!=EGL_NO_SURFACE,"EGL surface");fresh();
  }
  void fresh() {
    if(context!=EGL_NO_CONTEXT) {eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);}
    const EGLint attributes[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};context=eglCreateContext(display,config,EGL_NO_CONTEXT,attributes);
    require(context!=EGL_NO_CONTEXT&&eglMakeCurrent(display,surface,surface,context),"EGL current context");
  }
  ~Egl() {eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);}
};
std::vector<std::uint8_t> pixels(int width=Width,int height=Height) {
  std::vector<std::uint8_t> result(std::size_t(width)*height*4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,result.data());
  require(glGetError()==GL_NO_ERROR,"GLES error");return result;
}
void ppm(const std::filesystem::path &path,const std::vector<std::uint8_t> &image) {
  std::ofstream f(path,std::ios::binary);f<<"P6\n"<<Width<<' '<<Height<<"\n255\n";
  for(int y=Height-1;y>=0;--y)for(int x=0;x<Width;++x)f.write(reinterpret_cast<const char*>(image.data()+(y*Width+x)*4),3);
  require(bool(f),"image output");
}
StaticScene load(const std::filesystem::path &roomPath,const std::filesystem::path &corePath) {
  const auto room=RoomArchive::Load(roomPath.string()),core=RoomArchive::Load(corePath.string());
  const auto copy=[&](const char *tag) {const auto *entry=room.find(tag);require(entry,"room resource exists");return room.copy(entry-room.entries().data());};
  const auto *eff=core.find("EFF");require(eff,"CORE EFF exists");
  Smd smd(copy("SMD"));Smx smx(copy("SMX"));auto scene=BuildStaticScene(smd);AttachSceneTextures(smd,scene);ApplySceneSmx(smx,scene);
  scene.lighting=ReadRoomLighting(room);AttachSceneEnvironment(BuildRoomEnvironment(room,smd,smx,0x10a,core.copy(eff-core.entries().data())),scene);
  require(scene.environment.grids.size()==1&&scene.environment.grids[0].generator==0x42,"real r10a Esp42 water");return scene;
}
std::vector<GLint> state() {
  std::vector<GLint> result;
  const GLenum names[]{GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_READ_FRAMEBUFFER_BINDING,GL_DRAW_FRAMEBUFFER_BINDING,GL_DEPTH_FUNC,GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA,GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB};
  for(const auto name:names) {GLint value{};glGetIntegerv(name,&value);result.push_back(value);}
  GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);result.insert(result.end(),viewport,viewport+4);
  GLboolean mask[4],depth{};glGetBooleanv(GL_COLOR_WRITEMASK,mask);for(const auto value:mask)result.push_back(value);
  glGetBooleanv(GL_DEPTH_WRITEMASK,&depth);result.push_back(depth);
  const GLenum capabilities[]{GL_BLEND,GL_CULL_FACE,GL_DEPTH_TEST,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_RASTERIZER_DISCARD,GL_DITHER,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE};
  for(const auto cap:capabilities)result.push_back(glIsEnabled(cap));
  GLint active{};glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
  for(unsigned unit=0;unit<4;++unit) {
    glActiveTexture(GL_TEXTURE0+unit);GLint value{};
    // The result is intentionally bound on unit2; other textures and all
    // samplers belong to the caller and must survive the capture pass.
    if(unit!=2) {glGetIntegerv(GL_TEXTURE_BINDING_2D,&value);result.push_back(value);}
    glGetIntegerv(GL_SAMPLER_BINDING,&value);result.push_back(value);
  }
  glActiveTexture(GLenum(active));return result;
}
void copyOracle(const EnvironmentData &environment) {
  constexpr int w=16,h=12;std::vector<std::uint8_t> source(w*h*4);
  for(int y=0;y<h;++y)for(int x=0;x<w;++x)for(unsigned c=0;c<4;++c)source[(y*w+x)*4+c]=std::uint8_t((x*31+y*47+int(c)*61)%256);
  // Include an isolated bright sample: copying the source unchanged used to
  // expose single-pixel terrain cracks directly through the water.
  for(unsigned c=0;c<3;++c)source[(5*w+5)*4+c]=255;
  GLuint input{},inputFbo{},readFbo{},outputFbo{},textures[3]{},sampler{},vao{};
  glGenTextures(1,&input);glBindTexture(GL_TEXTURE_2D,input);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,source.data());
  glGenFramebuffers(1,&inputFbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,inputFbo);glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,input,0);
  require(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"source framebuffer complete");
  glGenFramebuffers(1,&readFbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo); // Deliberately unrelated caller read state.
  glGenTextures(3,textures);glGenSamplers(1,&sampler);glGenVertexArrays(1,&vao);glBindVertexArray(vao);
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[0]);glBindSampler(0,sampler);
  glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,textures[1]);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,textures[2]);
  glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_SUBTRACT);glBlendFunc(GL_DST_COLOR,GL_SRC_COLOR);
  glEnable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_GREATER);glDepthMask(GL_FALSE);
  glEnable(GL_SCISSOR_TEST);glEnable(GL_STENCIL_TEST);glEnable(GL_RASTERIZER_DISCARD);glEnable(GL_DITHER);glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);glEnable(GL_SAMPLE_COVERAGE);
  glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_FALSE);glViewport(2,3,7,5);
  CreateEnvironmentGl(environment);const auto before=state();const auto captured=CaptureEnvironmentScreen(w,h);require(captured,"source copy exists");
  require(state()==before,"capture restores framebuffers/program/VAO/viewport/enables/color/depth/samplers/textures");
  GLint active{};glGetIntegerv(GL_ACTIVE_TEXTURE,&active);require(active==GL_TEXTURE0,"capture returns to material unit0");
  glActiveTexture(GL_TEXTURE2);GLint bound{};glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);require(GLuint(bound)==captured,"completed copy bound on material unit2");
  glGenFramebuffers(1,&outputFbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,outputFbo);glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,captured,0);
  const auto actual=pixels(w/2,h/2);
  for(int y=0;y<h/2;++y)for(int x=0;x<w/2;++x)for(unsigned c=0;c<4;++c) {
    const auto average=[&](int row) {
      unsigned sum{};for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)sum+=source[(std::clamp(y*2+dy+row,0,h-1)*w+x*2+dx)*4+c];return sum/4;
    };
    const auto middle=average(0);const auto expected=c==3?middle:(average(-1)*16+middle*32+average(1)*16)>>6;
    // GLES linear sampling may round its half-texel interpolation by one
    // byte before the integer GX filter. The filter, range and alpha are
    // still checked against the independently computed source box samples.
    require(std::abs(int(actual[(y*(w/2)+x)*4+c])-int(expected))<=1,"half-size GX three-row copy RGB/alpha oracle");
  }
  glBindFramebuffer(GL_FRAMEBUFFER,0);for(const auto cap:{GL_BLEND,GL_CULL_FACE,GL_DEPTH_TEST,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_RASTERIZER_DISCARD,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE})glDisable(cap);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);
  glDeleteTextures(3,textures);glDeleteTextures(1,&input);glDeleteSamplers(1,&sampler);glDeleteVertexArrays(1,&vao);
  for(const auto fbo:{inputFbo,readFbo,outputFbo})glDeleteFramebuffers(1,&fbo);
  DestroyEnvironmentGl();
}
std::array<double,2> project(const std::array<double,3> &point,const FollowCameraPose &pose) {
  std::array<double,3> relative{};for(unsigned k=0;k<3;++k)relative[k]=point[k]-pose.eye[k];
  const auto dot=[&](const auto &axis) {double value{};for(unsigned k=0;k<3;++k)value+=relative[k]*axis[k];return value;};
  const double f=1/std::tan(pose.fovDegrees*3.14159265358979323846/360),z=dot(pose.look);
  return {Width/2.+dot(pose.right)*f*Height/2./z,Height/2.-dot(pose.up)*f*Height/2./z};
}
void realSeam(Egl &egl,const StaticScene &scene,const std::filesystem::path &out) {
  require(std::filesystem::exists(out)||std::filesystem::create_directories(out),"render output directory");
  const std::array<float,3> coarse{18750,-10000,96250},fine{20310,-10000,94685};
  const auto exists=[&](const auto &point) {for(std::size_t i=0;i<scene.vertices.size();i+=3)if(std::equal(point.begin(),point.end(),scene.vertices.begin()+i))return true;return false;};
  require(exists(coarse)&&exists(fine)&&coarse[0]+coarse[2]-fine[0]-fine[2]==5,"original quantized coarse/fine SMD boundary retained");
  SceneProbe probe;probe.showBody=false;probe.feet={20575,-10000,98320};
  auto diagnostic=scene;diagnostic.environment.grids.clear();diagnostic.environment.gridNoise.clear();
  auto lighting=std::make_shared<RoomLighting>(*scene.lighting);
  // Keep the source background active, but place its fog interval far beyond
  // all original room geometry so only uncovered pixels show magenta.
  lighting->cuts[0].fog.type=2;lighting->cuts[0].fog.start=1e8f;lighting->cuts[0].fog.end=2e8f;
  lighting->cuts[0].fog.color={255,0,255,255};diagnostic.lighting=lighting;
  CreateSceneGl(diagnostic);SceneFollowHeading(float(3.14159265358979323846));SceneSeedFollowView(0,-.65);
  DrawSceneGl(Width,Height,0,0,0,false,&probe,nullptr,0);const auto gaps=pixels();const auto pose=SceneFollowPose();DestroySceneGl();
  const auto a=project({25000,-10000,90000},pose),b=project({18750,-10000,96250},pose);const double slope=(b[1]-a[1])/(b[0]-a[0]),intercept=a[1]-slope*a[0];
  std::vector<std::array<int,2>> samples;
  for(int topY=140*Height/360;topY<270*Height/360;++topY)for(int x=0;x<Width;++x) {
    if(std::abs(topY+.5-slope*(x+.5)-intercept)>.75)continue;
    const int y=Height-1-topY;const auto *p=gaps.data()+(y*Width+x)*4;
    if(p[0]==255&&p[1]==0&&p[2]==255)samples.push_back({x,y});
  }
  require(samples.size()>=20,"actual original terrain raster gaps reproduced without water");ppm(out/"r10a-original-terrain-gaps.ppm",gaps);
  // bio4_AddBgColor ignores a stored fog RGB when Type==0. The same
  // uncovered original terrain samples must then expose black, even if
  // the inactive LIT record retains the deliberately bright color above.
  auto noFog=diagnostic;auto disabledFog=std::make_shared<RoomLighting>(*diagnostic.lighting);
  disabledFog->cuts[0].fog.type=0;noFog.lighting=disabledFog;
  CreateSceneGl(noFog);SceneFollowHeading(float(3.14159265358979323846));SceneSeedFollowView(0,-.65);
  DrawSceneGl(Width,Height,0,0,0,false,&probe,nullptr,0);const auto black=pixels();DestroySceneGl();
  require(std::all_of(samples.begin(),samples.end(),[&](const auto &p) {
    const auto *c=black.data()+(p[1]*Width+p[0])*4;return c[0]==0&&c[1]==0&&c[2]==0&&c[3]==255;
  }),"original Fog.Type0 clears black despite stored nonblack fog color");
  CreateSceneGl(scene);SceneFollowHeading(float(3.14159265358979323846));SceneSeedFollowView(0,-.65);
  DrawSceneGl(Width,Height,0,0,0,false,&probe,nullptr,0);const auto image=pixels();ppm(out/"r10a-water-filtered.ppm",image);
  double maximum{},total{};for(const auto &sample:samples) {
    const auto luma=[&](int dy) {const auto *p=image.data()+((sample[1]+dy)*Width+sample[0])*4;return (p[0]+p[1]+p[2])/3.;};
    const auto contrast=luma(0)-(luma(-2)+luma(2))/2;maximum=std::max(maximum,contrast);total+=contrast;
  }
  require(maximum<8&&total/samples.size()<5,"real r10a water suppresses bright dashed terrain seam through source copy filter");
  const auto saved=SaveSceneView();for(unsigned i=0;i<3;++i)DrawSceneGl(Width,Height,0,0,0,false,&probe,nullptr,0);require(pixels()==image,"paused filtered water exact framebuffer");
  ForgetSceneGl();egl.fresh();CreateSceneGl(scene);RestoreSceneView(saved);DrawSceneGl(Width,Height,0,0,0,false,&probe,nullptr,0);require(pixels()==image,"filtered water fresh-context pixel equality");
  DestroySceneGl();
  std::cout<<"r10a original dry terrain gaps="<<samples.size()<<", filtered water mean contrast="<<total/samples.size()<<", maximum="<<maximum<<"\n";
}
}
int main(int argc,char **argv) {try {
  if(argc!=4&&argc!=5)throw std::runtime_error("Usage: m36_water_seams_check R10A_DAT CORE_DAT OUTPUT_DIRECTORY [--1080p]");
  if(argc==5) {require(std::string(argv[4])=="--1080p","optional upload resolution");Width=1920;Height=1080;}
  const auto scene=load(argv[1],argv[2]);Egl egl;copyOracle(scene.environment);realSeam(egl,scene,argv[3]);
  std::cout<<"M36 water copy/seam PASS: "<<checks<<" checks\n";return 0;
}catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}}
