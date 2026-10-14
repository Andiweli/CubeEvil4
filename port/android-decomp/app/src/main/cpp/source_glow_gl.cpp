#include "source_glow_gl.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <string>
namespace re4 {
namespace {
GLuint program{},vao{},capture{},captureFbo{};
std::array<GLuint,4> images{},fbos{};
int captureWidth{},captureHeight{};
std::optional<WorldEffectGlowRequest> pending;
bool alphaDraw2{};
struct State {
  GLint program{},vao{},active{},image{},sampler{},read{},draw{},viewport[4]{},
      equationRgb{},equationAlpha{},srcRgb{},dstRgb{},srcAlpha{},dstAlpha{};
  GLboolean depth{},blend{},cull{},scissor{},stencil{},depthMask{},mask[4]{};
  State() {
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&image);glGetIntegerv(GL_SAMPLER_BINDING,&sampler);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    glGetIntegerv(GL_VIEWPORT,viewport);depth=glIsEnabled(GL_DEPTH_TEST);blend=glIsEnabled(GL_BLEND);
    cull=glIsEnabled(GL_CULL_FACE);scissor=glIsEnabled(GL_SCISSOR_TEST);stencil=glIsEnabled(GL_STENCIL_TEST);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);glGetBooleanv(GL_COLOR_WRITEMASK,mask);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&equationRgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equationAlpha);
    glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb);glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
  }
  ~State() {
    glUseProgram(GLuint(program));glBindVertexArray(GLuint(vao));
    glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));
    glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,GLuint(image));glBindSampler(0,GLuint(sampler));glActiveTexture(GLenum(active));
    for(const auto &cap:std::array<std::pair<GLenum,GLboolean>,5>{{{GL_DEPTH_TEST,depth},{GL_BLEND,blend},{GL_CULL_FACE,cull},{GL_SCISSOR_TEST,scissor},{GL_STENCIL_TEST,stencil}}})
      if(cap.second)glEnable(cap.first);else glDisable(cap.first);
    glDepthMask(depthMask);glColorMask(mask[0],mask[1],mask[2],mask[3]);
    glBlendEquationSeparate(GLenum(equationRgb),GLenum(equationAlpha));
    glBlendFuncSeparate(GLenum(srcRgb),GLenum(dstRgb),GLenum(srcAlpha),GLenum(dstAlpha));
  }
};
GLuint compile(GLenum kind,const char *source) {
  const auto name=glCreateShader(kind);glShaderSource(name,1,&source,nullptr);glCompileShader(name);
  GLint okay{};glGetShaderiv(name,GL_COMPILE_STATUS,&okay);
  if(!okay){char log[1024]{};glGetShaderInfoLog(name,sizeof log,nullptr,log);glDeleteShader(name);throw std::runtime_error(std::string("Filter03: ")+log);}return name;
}
void texture(GLuint image,int width,int height,GLenum format) {
  glBindTexture(GL_TEXTURE_2D,image);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,0,GLint(format),width,height,0,format==GL_R8?GL_RED:GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
}
void resources() {
  if(program)return;
  const auto vs=compile(GL_VERTEX_SHADER,R"GLSL(#version 300 es
precision highp float;out vec2 uv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.0-1.0,0.0,1.0);}
)GLSL"),fs=compile(GL_FRAGMENT_SHADER,R"GLSL(#version 300 es
precision highp float;in vec2 uv;out vec4 color;
uniform sampler2D image;uniform float factor;uniform vec2 offset;uniform vec3 tint;uniform int mode;
void main(){float value=mode==0?texture(image,uv).a:texture(image,uv+offset).r;
  // A8 copy quantization precedes each TEV multiplication.
  if(mode!=3)value=floor(value*255.0+0.5)/255.0;
  if(mode==3)color=vec4(clamp(tint*value*2.0,0.0,1.0),1.0);
  else color=vec4(clamp(value*factor,0.0,1.0),0.0,0.0,1.0);}
)GLSL");
  program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);
  glDeleteShader(vs);glDeleteShader(fs);GLint okay{};glGetProgramiv(program,GL_LINK_STATUS,&okay);
  if(!okay)throw std::runtime_error("Filter03: Shader linking");
  glGenVertexArrays(1,&vao);glGenTextures(1,&capture);glGenFramebuffers(1,&captureFbo);glGenTextures(4,images.data());glGenFramebuffers(4,fbos.data());
  // Original A8 copies are 256x224,128x112,64x56. The feedback draw
  // covers the quarter-size EFB region, then copies it down again.
  constexpr int width[]{256,128,64,128},height[]{224,112,56,112};
  for(unsigned i=0;i<4;++i){texture(images[i],width[i],height[i],GL_R8);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbos[i]);glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,images[i],0);
    if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Filter03: Alpha target buffer");}
}
}
void BeginSourceGlowFrameGl(const std::vector<WorldEffectGlowRequest> &requests) {
  pending.reset();
  for(const auto &request:requests) {
    if(pending && (pending->priority==255 || (request.priority!=255 && pending->priority<request.priority)))continue;
    pending=request;if(request.special)alphaDraw2=true;
  }
}
bool SourceEffectWritesAlphaGl(unsigned flags) {
  return alphaDraw2?bool(flags&0x800000):bool(flags&0x8000);
}
void RestoreSourceGlowAlphaDraw2Gl(bool enabled) {alphaDraw2=enabled;}
void DrawSourceGlowGl(int width,int height) {
  if(!pending)return;
  if(width<1 || height<1)throw std::runtime_error("Filter03: Viewport");
  State previous;resources();
  glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  glBindVertexArray(vao);glUseProgram(program);glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);
  glUniform1i(glGetUniformLocation(program,"image"),0);glUniform3f(glGetUniformLocation(program,"tint"),1,1,1);
  glBindTexture(GL_TEXTURE_2D,capture);
  if(width!=captureWidth || height!=captureHeight){texture(capture,width,height,GL_RGBA8);captureWidth=width;captureHeight=height;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,captureFbo);glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,capture,0);
    if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Filter03: Frame alpha copy");}
  glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(previous.draw));glBindFramebuffer(GL_DRAW_FRAMEBUFFER,captureFbo);
  // NEAREST also resolves a multisampled Android EFB into this immutable copy.
  glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
  const auto draw=[&](GLuint target,int w,int h,GLuint source,int mode,float factor,float x=0,float y=0) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,target);glViewport(0,0,w,h);glBindTexture(GL_TEXTURE_2D,source);
    glUniform1i(glGetUniformLocation(program,"mode"),mode);glUniform1f(glGetUniformLocation(program,"factor"),factor);
    glUniform2f(glGetUniformLocation(program,"offset"),x,y);glDrawArrays(GL_TRIANGLES,0,3);
  };
  draw(fbos[0],256,224,capture,0,2);draw(fbos[1],128,112,images[0],1,2);
  // SetParam updates loop only; roomInit's level remains1 (two offsets).
  const unsigned loops=unsigned(std::max(-2,pending->level)+2);
  for(unsigned i=0;i<loops;++i)for(unsigned j=0;j<2;++j) {
    draw(fbos[2],64,56,images[1],1,1);
    draw(fbos[3],128,112,images[2],1,256.f/255,j?.008f:-.008f);
    std::swap(images[1],images[3]);std::swap(fbos[1],fbos[3]);
  }
  draw(fbos[2],64,56,images[1],1,1);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(previous.draw));glViewport(0,0,width,height);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_ONE,GL_ONE);
  glUniform3f(glGetUniformLocation(program,"tint"),pending->color[0]/255.f,pending->color[1]/255.f,pending->color[2]/255.f);
  draw(GLuint(previous.draw),width,height,images[2],3,1,loops?.008f:0);
  if(!pending->special)alphaDraw2=false;
  pending.reset();
  if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Filter03: GPU drawing");
}
void ForgetSourceGlowGl() {
  program=vao=capture=captureFbo=0;images={};fbos={};captureWidth=captureHeight=0;pending.reset();alphaDraw2=false;
}
void DestroySourceGlowGl() {
  GLint current{};glGetIntegerv(GL_CURRENT_PROGRAM,&current);if(program && GLuint(current)==program)glUseProgram(0);
  if(program)glDeleteProgram(program);
  if(vao)glDeleteVertexArrays(1,&vao);
  if(capture)glDeleteTextures(1,&capture);
  if(captureFbo)glDeleteFramebuffers(1,&captureFbo);
  glDeleteTextures(4,images.data());glDeleteFramebuffers(4,fbos.data());ForgetSourceGlowGl();
}
}
