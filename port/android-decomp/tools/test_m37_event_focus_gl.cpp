#include "event_focus_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace re4;
namespace {
constexpr int W=64,H=48;
unsigned checks;
void require(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);}
struct Egl {
 EGLDisplay display{};EGLConfig config{};EGLSurface surface{};EGLContext context=EGL_NO_CONTEXT;
 Egl(){setenv("EGL_PLATFORM","surfaceless",1);display=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint major{},minor{},count{};
  require(eglInitialize(display,&major,&minor)&&eglBindAPI(EGL_OPENGL_ES_API),"EGL init");
  const EGLint attributes[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_STENCIL_SIZE,8,EGL_NONE};
  require(eglChooseConfig(display,attributes,&config,1,&count)&&count,"EGL config");
  const EGLint size[]{EGL_WIDTH,96,EGL_HEIGHT,72,EGL_NONE};surface=eglCreatePbufferSurface(display,config,size);require(surface!=EGL_NO_SURFACE,"EGL surface");fresh();}
 void fresh(){if(context!=EGL_NO_CONTEXT){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);}
  const EGLint attributes[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};context=eglCreateContext(display,config,EGL_NO_CONTEXT,attributes);
  require(context!=EGL_NO_CONTEXT&&eglMakeCurrent(display,surface,surface,context),"EGL context");}
 ~Egl(){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);}
};
std::vector<std::uint8_t> pixels(int width=W,int height=H){glBindFramebuffer(GL_READ_FRAMEBUFFER,0);std::vector<std::uint8_t> p(width*height*4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,p.data());require(glGetError()==GL_NO_ERROR,"GLES errors");return p;}
float projected(float distance){constexpr float n=100,f=200000;return f/(f-n)-f*n/((f-n)*distance);}
void fixture(int width=W,int height=H){glBindFramebuffer(GL_FRAMEBUFFER,0);glDisable(GL_RASTERIZER_DISCARD);glDisable(GL_DITHER);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glDepthRangef(0,1);glEnable(GL_SCISSOR_TEST);
 for(int x=0;x<width;++x){glScissor(x,0,1,height);glClearDepthf(projected(x<width/3?1000:x<2*width/3?3500:50000));glClear(GL_DEPTH_BUFFER_BIT);}
 for(int y=0;y<height;++y)for(int x=0;x<width;++x){glScissor(x,y,1,1);glClearColor(float((x*73+y*37+11)%256)/255,float((x*29+y*17+47)%256)/255,float((x*13+y*59+53)%256)/255,float((x*3+y*23+71)%256)/255);glClear(GL_COLOR_BUFFER_BIT);}
 glDisable(GL_SCISSOR_TEST);glViewport(0,0,width,height);}
std::vector<GLint> state(){std::vector<GLint> result;const GLenum names[]{GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ACTIVE_TEXTURE,GL_READ_FRAMEBUFFER_BINDING,GL_DRAW_FRAMEBUFFER_BINDING,GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA,GL_BLEND_SRC_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_RGB,GL_BLEND_DST_ALPHA,GL_DEPTH_FUNC,GL_DEPTH_WRITEMASK};
 for(auto name:names){GLint value{};glGetIntegerv(name,&value);result.push_back(value);}GLint view[4];glGetIntegerv(GL_VIEWPORT,view);result.insert(result.end(),view,view+4);
 GLfloat range[2];glGetFloatv(GL_DEPTH_RANGE,range);for(auto value:range){GLint bits;std::memcpy(&bits,&value,4);result.push_back(bits);}GLboolean mask[4];glGetBooleanv(GL_COLOR_WRITEMASK,mask);for(auto value:mask)result.push_back(value);
 const GLenum caps[]{GL_BLEND,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_RASTERIZER_DISCARD,GL_DITHER,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE};for(auto cap:caps)result.push_back(glIsEnabled(cap));
 GLint active{};glGetIntegerv(GL_ACTIVE_TEXTURE,&active);for(unsigned unit:{0u,3u}){glActiveTexture(GL_TEXTURE0+unit);GLint value{};glGetIntegerv(GL_TEXTURE_BINDING_2D,&value);result.push_back(value);glGetIntegerv(GL_SAMPLER_BINDING,&value);result.push_back(value);}glActiveTexture(GLenum(active));return result;}
unsigned changedIn(const std::vector<std::uint8_t>&before,const std::vector<std::uint8_t>&after,int begin,int end){unsigned changed{};
 for(int y=0;y<H;++y)for(int x=begin;x<end;++x){bool diff=false;for(unsigned c=0;c<3;++c)diff|=before[(y*W+x)*4+c]!=after[(y*W+x)*4+c];changed+=diff;require(before[(y*W+x)*4+3]==after[(y*W+x)*4+3],"Filter01 never updates EFB alpha");}
 return changed;}
}
int main(){try{
 Egl egl;fixture();const auto base=pixels();const auto initial=state();
 require(!ApplyEventFocusGl(W,H,{}),"No focus requests skip the pass");require(pixels()==base&&state()==initial,"No focus preserves exact color/state");
 EventFocusState near{true,false,1650,0,6,0},far{false,true,0,10000,0,5};
 fixture();require(ApplyEventFocusGl(W,H,near),"Near request renders");const auto nearImage=pixels();
 require(changedIn(base,nearImage,0,W/3)>500,"Original GEQUAL near depth selects foreground");
 require(changedIn(base,nearImage,W/3,W)==0,"Near depth leaves farther scene pixels exact");
 fixture();require(ApplyEventFocusGl(W,H,far),"Far request renders");const auto farImage=pixels();
 require(changedIn(base,farImage,0,2*W/3)==0,"Original LEQUAL far depth leaves foreground exact");
 require(changedIn(base,farImage,2*W/3,W)>500,"Far depth selects background");
 EventFocusState both=near;both.farEnabled=true;both.farDepth=far.farDepth;both.farLevel=far.farLevel;
 fixture();require(ApplyEventFocusGl(W,H,both),"Both original focus requests render");const auto bothImage=pixels();
 require(changedIn(base,bothImage,W/3,2*W/3)==0,"Focal middle remains pixel-exact");
 // OT head insertion makes far execute first, then near. Validate the actual
 // copies and overlays against separate calls, including overlapping masks.
 EventFocusState overlapNear{true,false,10000,0,6,0},overlapFar{false,true,0,1650,0,5};
 fixture();ApplyEventFocusGl(W,H,overlapFar);ApplyEventFocusGl(W,H,overlapNear);const auto ordered=pixels();
 EventFocusState overlap=overlapNear;overlap.farEnabled=true;overlap.farDepth=overlapFar.farDepth;overlap.farLevel=overlapFar.farLevel;
 fixture();ApplyEventFocusGl(W,H,overlap);require(pixels()==ordered,"Far-then-near original OT order exact");
 fixture();EventFocusState zero{true,false,0,0,0,0};require(!ApplyEventFocusGl(W,H,zero)&&pixels()==base,"CamZ0.01 projected negative quads are clipped, no invented global blur");
 RoomLightCut environment;environment.focusLevel=6;environment.focusDepth=61261;
 fixture();require(ApplyEventFocusGl(W,H,{},100,200000,&environment),"Original ENV near focus renders when event focus is absent");const auto environmentNear=pixels();
 require(changedIn(base,environmentNear,0,W/3)>500,"ENV near screen-depth selects foreground");
 require(changedIn(base,environmentNear,W/3,W)==0,"ENV near focus preserves farther pixels");
 environment.focusMode=1;
 fixture();require(ApplyEventFocusGl(W,H,{},100,200000,&environment),"Original ENV far focus renders");const auto environmentFar=pixels();
 require(changedIn(base,environmentFar,0,W/3)==0,"ENV far focus preserves foreground");
 require(changedIn(base,environmentFar,W/3,W)>500,"ENV far focus selects farther pixels");
 fixture();ApplyEventFocusGl(W,H,near,100,200000,&environment);
 require(pixels()==nearImage,"Explicit event focus overrides ENV focus");
 fixture();GLuint textures[2]{},sampler{},vao{},readFbo{};glGenTextures(2,textures);glGenSamplers(1,&sampler);glGenVertexArrays(1,&vao);glGenFramebuffers(1,&readFbo);
 glBindVertexArray(vao);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[0]);glBindSampler(0,sampler);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,textures[1]);glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo);
 for(auto cap:{GL_BLEND,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_RASTERIZER_DISCARD,GL_DITHER,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE})glEnable(cap);
 glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_SUBTRACT);glBlendFuncSeparate(GL_DST_COLOR,GL_SRC_COLOR,GL_ONE_MINUS_DST_ALPHA,GL_DST_ALPHA);glDepthFunc(GL_GREATER);glDepthMask(GL_FALSE);glDepthRangef(.2f,.8f);glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_FALSE);glViewport(2,3,7,5);
 const auto before=state();require(ApplyEventFocusGl(W,H,both),"Arbitrary GL caller state can render focus");require(state()==before,"All changed GL state, depth, masks, units and FBOs restored");require(pixels()==bothImage,"Arbitrary caller state does not alter focus color");
 glDeleteFramebuffers(1,&readFbo);glDeleteSamplers(1,&sampler);glDeleteTextures(2,textures);glDeleteVertexArrays(1,&vao);
 for(auto cap:{GL_STENCIL_TEST,GL_CULL_FACE,GL_RASTERIZER_DISCARD,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE})glDisable(cap);
 fixture(80,60);require(ApplyEventFocusGl(80,60,both),"New viewport rebuilds half-copy texture");pixels(80,60);
 fixture();ApplyEventFocusGl(W,H,both);require(pixels()==bothImage,"Resize restores original pass pixel-exact");
 ForgetEventFocusGl();egl.fresh();fixture();require(ApplyEventFocusGl(W,H,both),"Context loss lazily rebuilds focus resources");require(pixels()==bothImage,"Restored EGL focus color is pixel-exact");
 DestroyEventFocusGl();fixture();require(ApplyEventFocusGl(W,H,far),"Destroy/recreate resources");require(pixels()==farImage,"Destroyed/rebuilt focus color is exact");DestroyEventFocusGl();
 for(auto invalid:{-1.f,11.f}){bool rejected=false;try{EventFocusPasses(invalid,1000);}catch(const std::exception&){rejected=true;}require(rejected,"Out-of-source focus level rejected");}
 std::cout<<"M37 event Filter01 GLES depth / alpha / OT / state / resize / EGL PASS: "<<checks<<" checks\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
