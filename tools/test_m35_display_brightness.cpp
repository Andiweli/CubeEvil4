#include "display_brightness.h"
#include "display_brightness_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>
using namespace re4;
namespace {
constexpr int W=19,H=11;
unsigned checks;
void require(bool ok,const char *message) {++checks;if(!ok)throw std::runtime_error(message);}
struct Egl {
  EGLDisplay display{};EGLConfig config{};EGLSurface surface{};EGLContext context=EGL_NO_CONTEXT;
  Egl() {setenv("EGL_PLATFORM","surfaceless",1);display=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint major{},minor{},count{};require(eglInitialize(display,&major,&minor)&&eglBindAPI(EGL_OPENGL_ES_API),"EGL initialization");
    const EGLint attributes[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_STENCIL_SIZE,8,EGL_NONE};
    require(eglChooseConfig(display,attributes,&config,1,&count)&&count,"EGL configuration");const EGLint size[]{EGL_WIDTH,W,EGL_HEIGHT,H,EGL_NONE};surface=eglCreatePbufferSurface(display,config,size);require(surface!=EGL_NO_SURFACE,"EGL surface");fresh();}
  void fresh() {if(context!=EGL_NO_CONTEXT) {eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);}const EGLint attributes[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};context=eglCreateContext(display,config,EGL_NO_CONTEXT,attributes);require(context!=EGL_NO_CONTEXT&&eglMakeCurrent(display,surface,surface,context),"EGL current context");}
  ~Egl() {eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);}
};
std::vector<std::uint8_t> pixels() {std::vector<std::uint8_t> p(W*H*4);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,p.data());require(glGetError()==GL_NO_ERROR,"GLES error");return p;}
void fixture() {
  glBindFramebuffer(GL_FRAMEBUFFER,0);glDisable(GL_RASTERIZER_DISCARD);glDisable(GL_DITHER);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glEnable(GL_SCISSOR_TEST);
  for(int y=0;y<H;++y) {glScissor(0,y,W,1);glClearColor(float((y*37+11)%256)/255,float((y*17+47)%256)/255,float((y*59+53)%256)/255,float((y*23+71)%256)/255);glClear(GL_COLOR_BUFFER_BIT);}
  glDisable(GL_SCISSOR_TEST);glViewport(0,0,W,H);
}
void expected(const std::vector<std::uint8_t>&base,const std::vector<std::uint8_t>&actual,int brightness) {
  const auto filter=DisplayBrightnessFilter(brightness);
  const std::array<unsigned,3> rows{unsigned(filter[0])+filter[1],unsigned(filter[2])+filter[3]+filter[4],unsigned(filter[5])+filter[6]};
  for(int y=0;y<H;++y)for(int x=0;x<W;++x)for(unsigned channel=0;channel<4;++channel) {
    unsigned value=base[(y*W+x)*4+channel];
    if(channel<3) {unsigned sum=0;for(int row=0;row<3;++row)sum+=base[(std::clamp(y+row-1,0,H-1)*W+x)*4+channel]*rows[unsigned(row)];value=std::min(255u,sum>>6);}
    require(actual[(y*W+x)*4+channel]==value,"brightness framebuffer differs from original coefficients and three GX rows");
  }
}
std::vector<GLint> state() {
  std::vector<GLint> result;const GLenum values[]{GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ACTIVE_TEXTURE,GL_READ_FRAMEBUFFER_BINDING,GL_DRAW_FRAMEBUFFER_BINDING,GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA,GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB};
  for(const auto name:values) {GLint v{};glGetIntegerv(name,&v);result.push_back(v);}
  GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);result.insert(result.end(),viewport,viewport+4);GLboolean mask[4];glGetBooleanv(GL_COLOR_WRITEMASK,mask);for(auto b:mask)result.push_back(b);
  const GLenum caps[]{GL_BLEND,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_RASTERIZER_DISCARD,GL_DITHER,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE};for(auto cap:caps)result.push_back(glIsEnabled(cap));
  GLint active{};glGetIntegerv(GL_ACTIVE_TEXTURE,&active);for(unsigned unit:{0u,3u}) {glActiveTexture(GL_TEXTURE0+unit);GLint v{};glGetIntegerv(GL_TEXTURE_BINDING_2D,&v);result.push_back(v);glGetIntegerv(GL_SAMPLER_BINDING,&v);result.push_back(v);}glActiveTexture(GLenum(active));return result;
}
}
int main() {try {
  for(int value=34;value<=114;++value) {const auto filter=DisplayBrightnessFilter(value);require(std::accumulate(filter.begin(),filter.end(),0)==value,"source weights sum to exact brightness");for(const auto tap:filter)require(tap<=22,"VI coefficient bound");}
  require(DisplayBrightnessFilter(64)==std::array<std::uint8_t,7>{8,8,10,12,10,8,8},"source normal filter");
  for(int invalid:{33,115,-1,10000}) {bool rejected=false;try {DisplayBrightnessFilter(invalid);}catch(const std::exception &) {rejected=true;}require(rejected,"brightness outside source UI bounds rejected");}
  Egl egl;fixture();const auto baseline=pixels();const auto defaultState=state();require(!ApplyDisplayBrightnessGl(W,H,64),"default brightness skips post pass");require(pixels()==baseline&&state()==defaultState,"default64 preserves exact framebuffer and GL state");
  for(int value=34;value<=114;++value)if(value!=64) {fixture();require(ApplyDisplayBrightnessGl(W,H,value),"nondefault brightness draws pass");expected(baseline,pixels(),value);}
  fixture();GLuint vao{},textures[2]{},sampler{},readFramebuffer{};glGenVertexArrays(1,&vao);glGenTextures(2,textures);glGenSamplers(1,&sampler);glGenFramebuffers(1,&readFramebuffer);
  glBindVertexArray(vao);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[0]);glBindSampler(0,sampler);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,textures[1]);glBindFramebuffer(GL_READ_FRAMEBUFFER,readFramebuffer);
  glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_SUBTRACT);glBlendFunc(GL_DST_COLOR,GL_SRC_COLOR);glEnable(GL_DEPTH_TEST);glEnable(GL_STENCIL_TEST);glEnable(GL_CULL_FACE);glEnable(GL_SCISSOR_TEST);glEnable(GL_RASTERIZER_DISCARD);glEnable(GL_DITHER);glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);glEnable(GL_SAMPLE_COVERAGE);glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_FALSE);glViewport(2,3,7,5);
  const auto before=state();require(ApplyDisplayBrightnessGl(W,H,114),"brightness pass with arbitrary source state");require(state()==before,"all mutated GLES state restored");glBindFramebuffer(GL_READ_FRAMEBUFFER,0);const auto bright=pixels();expected(baseline,bright,114);
  glDeleteFramebuffers(1,&readFramebuffer);glDeleteSamplers(1,&sampler);glDeleteTextures(2,textures);glDeleteVertexArrays(1,&vao);
  ForgetDisplayBrightnessGl();egl.fresh();fixture();require(ApplyDisplayBrightnessGl(W,H,114),"lazy brightness context recreation");require(pixels()==bright,"fresh EGL context restores brightness pixel-exact");
  DestroyDisplayBrightnessGl();fixture();require(ApplyDisplayBrightnessGl(W,H,34),"destroy/recreate brightness resources");expected(baseline,pixels(),34);DestroyDisplayBrightnessGl();
  std::cout<<"M35 display brightness PASS: "<<checks<<" checks\n";
}catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}}
