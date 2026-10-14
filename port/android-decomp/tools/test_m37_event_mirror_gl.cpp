// Real r120 clone/rear-view surface and production event/actor shaders.
#include "event_mirror_gl.h"
#include "event_actor_gl.h"
#include "actor_gl.h"
#include "room_lighting_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
constexpr unsigned W=640,H=360;
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
std::vector<std::uint8_t> pixels() {
 std::vector<std::uint8_t> result(W*H*4);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,result.data());
 const auto error=glGetError();
 if(error)std::cerr<<"Mirror GLES error 0x"<<std::hex<<error<<std::dec<<'\n';
 require(error==GL_NO_ERROR,"Mirror framebuffer read clean");return result;
}
unsigned changed(const std::vector<std::uint8_t> &a,const std::vector<std::uint8_t> &b) {
 unsigned n=0;for(unsigned p=0;p<a.size();p+=4)if(a[p]!=b[p]||a[p+1]!=b[p+1]||a[p+2]!=b[p+2])++n;return n;
}
void clear() {
 glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,W,H);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
 glDisable(GL_RASTERIZER_DISCARD);glDisable(GL_POLYGON_OFFSET_FILL);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
 glDepthMask(GL_TRUE);glClearColor(.2f,.3f,.4f,1);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}
const GenericEventActor &find(const OriginalEvent &event,const char *name) {
 for(const auto &actor:event.actors())if(actor->name()==name)return *actor;
 throw std::runtime_error("Source mirror actor missing");
}
void cut(OriginalEvent &event,unsigned wanted,unsigned frame) {
 while(!event.finished() && (event.sourceCut()<wanted || event.cutFrame()<frame))event.tick30Hz();
 require(!event.finished() && event.sourceCut()==wanted && event.cutFrame()==frame,"Actual event reaches authored mirror cut/frame");
}
std::vector<GLint> state() {
 std::vector<GLint> out;
 for(auto token:{GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,GL_RENDERBUFFER_BINDING,
      GL_ACTIVE_TEXTURE,GL_READ_FRAMEBUFFER_BINDING,GL_DRAW_FRAMEBUFFER_BINDING,GL_DEPTH_FUNC,
      GL_CULL_FACE_MODE,GL_FRONT_FACE,GL_BLEND_SRC_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_RGB,
      GL_BLEND_DST_ALPHA,GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA}) {
  GLint value;glGetIntegerv(token,&value);out.push_back(value);
 }
 for(auto token:{GL_VIEWPORT,GL_SCISSOR_BOX}) {GLint v[4]{};glGetIntegerv(token,v);out.insert(out.end(),v,v+4);}
 for(auto token:{GL_DEPTH_TEST,GL_BLEND,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_RASTERIZER_DISCARD,
      GL_POLYGON_OFFSET_FILL,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE,GL_DITHER,GL_PRIMITIVE_RESTART_FIXED_INDEX})out.push_back(glIsEnabled(token));
 GLboolean depth,mask[4]{};glGetBooleanv(GL_DEPTH_WRITEMASK,&depth);glGetBooleanv(GL_COLOR_WRITEMASK,mask);out.push_back(depth);
 for(auto v:mask)out.push_back(v);
 GLint active;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
 for(unsigned i=0;i<3;++i) {glActiveTexture(GL_TEXTURE0+i);GLint texture,sampler;glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glGetIntegerv(GL_SAMPLER_BINDING,&sampler);out.push_back(texture);out.push_back(sampler);}
 glActiveTexture(GLenum(active));
 for(auto token:{GL_COLOR_CLEAR_VALUE,GL_DEPTH_CLEAR_VALUE,GL_DEPTH_RANGE}) {
  GLfloat values[4]{};glGetFloatv(token,values);const unsigned n=token==GL_COLOR_CLEAR_VALUE?4:token==GL_DEPTH_RANGE?2:1;
  for(unsigned i=0;i<n;++i) {GLint bits;std::memcpy(&bits,values+i,4);out.push_back(bits);}
 }
 return out;
}
void check(OriginalEvent &event,unsigned wanted,unsigned frame) {
 cut(event,wanted,frame);require(IsEventMirrorActive(event),"Only original r120 callback cut requests RTT0");
 require(glGetError()==GL_NO_ERROR,"Cut advance leaves GPU state clean");
 CreateEventActorsGl(event);require(glGetError()==GL_NO_ERROR,"Mirror actor sync GPU clean");
 auto &atmosphere=RoomAtmosphere();atmosphere.data=event.lighting();atmosphere.eye=event.camera().eye;
 const auto camera=EventMirrorCameraMatrix(event.camera(),{},.0001f);
 const auto &clone=find(event,"pl0010"),&surface=find(event,"obm3010f");
 require(clone.rig() && !clone.mesh().vertices.empty() && !surface.mesh().vertices.empty(),"Original mirror clone and surface loaded");
 require(clone.lightMask()==0x20,"Original mirror-only clone retains special event lighting0x20");
 require(clone.orderType()==4 && surface.orderType()==1,
     "Source clone uses scroll OT4 while the reflective car surface uses world OT1");
 require(clone.globalIlluminationTexture()>=0,
     "Original mirror clone receives the CORE global illumination stage");
 const auto pose=clone.rig()->jointMatrices();const auto placement=clone.worldMatrix();const auto time=event.sourceFrame();
 clear();const auto background=pixels();DrawEventMirrorGl(camera.data(),{},.0001f,event,false);const auto reflected=pixels();
 require(changed(background,reflected)>100,"Actual source mirror surface submits projected RTT image");
 // A clone's weapon/face children use their own GPU owners. They belong
 // to the same RTT family and must never leak into the opaque main pass.
 std::vector<GenericEventActor*> attachments;
 for(const auto &part:event.actors())if(part.get()!=&clone && part->orderOwner()==&clone && part->visible()) {
   attachments.push_back(part.get());part->forceHidden(true);
 }
 clear();DrawEventMirrorGl(camera.data(),{},.0001f,event,false);
 require(changed(pixels(),reflected)>5,"Visible source clone attachments participate in RTT0");
 clear();DrawEventActorsGl(camera.data(),{},.0001f,event,false,11,13);const auto hiddenMain=pixels();
 for(auto *part:attachments)part->forceHidden(false);
 clear();DrawEventActorsGl(camera.data(),{},.0001f,event,false,11,13);
 require(pixels()==hiddenMain,"Whole source clone family is excluded from opaque world rendering");
 clear();DrawEventMirrorGl(camera.data(),{},.0001f,event,false);
 require(pixels()==reflected,"Restoring source attachments restores the original paused RTT pixels");
 require(clone.rig()->jointMatrices()==pose && clone.worldMatrix()==placement && event.sourceFrame()==time,"Mirror render never advances original event/actor clock");
 clear();DrawEventMirrorGl(camera.data(),{},.0001f,event,false);require(pixels()==reflected,"Paused actual mirror framebuffer is deterministic");
 // Mirror-only main pass must exclude the clone's standalone world geometry.
 clear();DrawEventActorsGl(camera.data(),{},.0001f,event,false);const auto together=pixels();
 require(changed(background,together)>changed(background,reflected),"Mirror integrates with other original actors on main framebuffer");
 clear();DrawEventActorsGl(camera.data(),{},.0001f,event,false,0,15);
 DrawEventActorsGl(camera.data(),{},.0001f,event,false,16,20);
 require(pixels()==together,"Source pass16 places mirror between model and world actor ordering");
 GLuint sampler{};glGenSamplers(1,&sampler);glSamplerParameteri(sampler,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);
 glBindSampler(0,sampler);glBindSampler(1,sampler);
 clear();DrawEventActorsGl(camera.data(),{},.0001f,event,false);
 require(pixels()==together,"Foreign mipmap samplers cannot make original event actors black");
 glBindSampler(0,0);glBindSampler(1,0);glDeleteSamplers(1,&sampler);
 glViewport(2,3,W-5,H-7);glScissor(1,2,W-3,H-4);glEnable(GL_SCISSOR_TEST);glEnable(GL_STENCIL_TEST);
 glEnable(GL_POLYGON_OFFSET_FILL);glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);glEnable(GL_SAMPLE_COVERAGE);
 glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);glDisable(GL_DITHER);glDepthFunc(GL_GREATER);glDepthRangef(.15f,.85f);
 glDepthMask(GL_FALSE);glColorMask(GL_TRUE,GL_FALSE,GL_TRUE,GL_FALSE);glCullFace(GL_FRONT_AND_BACK);glFrontFace(GL_CW);
 glBlendFuncSeparate(GL_ONE,GL_ZERO,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA);glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_SUBTRACT);
 glClearColor(.1f,.2f,.3f,.4f);glClearDepthf(.3f);glActiveTexture(GL_TEXTURE5);
 const auto before=state();DrawEventMirrorGl(camera.data(),{},.0001f,event,false);const auto after=state();
 require(before==after,"Mirror restores complete caller GL state including unrelated output modes");
 clear();DestroyEventMirrorGl();DrawEventMirrorGl(camera.data(),{},.0001f,event,false);
 // Re-enable normal source state after the deliberately hostile state test.
 glDepthRangef(0,1);glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);glDisable(GL_SAMPLE_COVERAGE);glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);glEnable(GL_DITHER);
 clear();DrawEventMirrorGl(camera.data(),{},.0001f,event,false);require(pixels()==reflected,"RTT rebuild preserves paused source mirror pixels");
}
}
int main(int argc,char **argv) {
 try {
  if(argc!=2)throw std::runtime_error("Usage: m37_event_mirror_gl_check DISC_FILES");
  Egl egl;OriginalEvent drive(argv[1],"r120s00",1),exit(argv[1],"r120s01",1);
  const auto layout=OriginalEventMirrorLayout();require(layout.width==592 && layout.height==512 && layout.copyX==40 &&
     layout.copyWidth==512 && layout.copyHeight==512 && layout.textureWidth==256 && layout.textureHeight==256,"Exact original RTT resize/copy dimensions");
  check(drive,0,30);check(exit,2,5);check(exit,6,5);
  const auto camera=EventMirrorCameraMatrix(exit.camera(),{},.0001f);
  clear();DrawEventMirrorGl(camera.data(),{},.0001f,exit,false);const auto saved=pixels();
  ForgetEventActorsGl();egl.fresh();CreateEventActorsGl(exit);clear();DrawEventMirrorGl(camera.data(),{},.0001f,exit,false);
  require(pixels()==saved,"EGL loss/recovery preserves paused original mirror image");
  cut(exit,7,1);require(!IsEventMirrorActive(exit),"Original cut7 removes mirror RTT callback");
  clear();const auto baseline=pixels();const auto before=state();DrawEventMirrorGl(camera.data(),{},.0001f,exit,false);
  require(pixels()==baseline && before==state(),"Inactive original mirror performs no draw or GL mutation");
  drive.finish();require(!IsEventMirrorActive(drive),"Skip immediately releases original mirror pass");
  DestroyEventActorsGl();require(glGetError()==GL_NO_ERROR,"Mirror and event GPU teardown clean");
  std::cout<<"M37 actual r120 mirror RTT/GLES PASS: "<<checks<<" checks; cuts=0/2/6\n";return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
