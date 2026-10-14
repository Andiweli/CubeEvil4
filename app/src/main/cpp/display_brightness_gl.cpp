#include "display_brightness_gl.h"
#include "display_brightness.h"
#include <GLES3/gl3.h>
#include <stdexcept>
namespace re4 {
namespace {
GLuint program{},vertexArray{},screen{};
int screenWidth{},screenHeight{};
GLuint shader(GLenum type,const char *source) {
  const auto object=glCreateShader(type);glShaderSource(object,1,&source,nullptr);glCompileShader(object);
  GLint ok{};glGetShaderiv(object,GL_COMPILE_STATUS,&ok);
  if(!ok) {char log[1024]{};glGetShaderInfoLog(object,sizeof log,nullptr,log);glDeleteShader(object);throw std::runtime_error(log);}
  return object;
}
void prepare() {
  if(program)return;
  constexpr const char *vertex=R"(#version 300 es
void main() {
  vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));
  gl_Position=vec4(p*2.0-1.0,0.0,1.0);
})";
  constexpr const char *fragment=R"(#version 300 es
precision highp float;
precision highp int;
uniform sampler2D frame;
uniform uvec3 copyRows;
out vec4 color;
void main() {
  ivec2 size=textureSize(frame,0);
  ivec2 p=ivec2(gl_FragCoord.xy);
  uvec3 sum=uvec3(0);
  for(int row=0;row<3;++row) {
    ivec2 samplePoint=clamp(p+ivec2(0,row-1),ivec2(0),size-ivec2(1));
    uvec3 sampleColor=uvec3(round(texelFetch(frame,samplePoint,0).rgb*255.0));
    sum+=sampleColor*copyRows[row];
  }
  color=vec4(vec3(min(sum>>6,uvec3(255)))/255.0,texelFetch(frame,p,0).a);
})";
  const auto vs=shader(GL_VERTEX_SHADER,vertex);GLuint fs{};
  try {fs=shader(GL_FRAGMENT_SHADER,fragment);}catch(...) {glDeleteShader(vs);throw;}
  const auto candidate=glCreateProgram();glAttachShader(candidate,vs);glAttachShader(candidate,fs);glLinkProgram(candidate);glDeleteShader(vs);glDeleteShader(fs);
  GLint ok{};glGetProgramiv(candidate,GL_LINK_STATUS,&ok);
  if(!ok) {char log[1024]{};glGetProgramInfoLog(candidate,sizeof log,nullptr,log);glDeleteProgram(candidate);throw std::runtime_error(log);}
  program=candidate;glGenVertexArrays(1,&vertexArray);glGenTextures(1,&screen);
}
struct State {
  GLint program{},vao{},activeTexture{},texture{},sampler{},readFramebuffer{},viewport[4]{};
  GLboolean mask[4]{};
  constexpr static GLenum caps[]{GL_BLEND,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_RASTERIZER_DISCARD,GL_DITHER,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE};
  GLboolean enabled[9]{};
  State() {
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glGetIntegerv(GL_SAMPLER_BINDING,&sampler);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFramebuffer);glGetIntegerv(GL_VIEWPORT,viewport);glGetBooleanv(GL_COLOR_WRITEMASK,mask);
    for(unsigned i=0;i<9;++i)enabled[i]=glIsEnabled(caps[i]);
  }
  ~State() {
    for(unsigned i=0;i<9;++i)if(enabled[i])glEnable(caps[i]);else glDisable(caps[i]);
    glColorMask(mask[0],mask[1],mask[2],mask[3]);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(readFramebuffer));glBindSampler(0,GLuint(sampler));glBindTexture(GL_TEXTURE_2D,GLuint(texture));glActiveTexture(GLenum(activeTexture));
    glBindVertexArray(GLuint(vao));glUseProgram(GLuint(program));
  }
};
}
bool ApplyDisplayBrightnessGl(int width,int height,int brightness) {
  const auto filter=DisplayBrightnessFilter(brightness);
  if(brightness==DefaultDisplayBrightness || width<=0 || height<=0)return false;
  const State previous;prepare();
  GLint destination{};glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&destination);glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(destination));
  glBindTexture(GL_TEXTURE_2D,screen);glBindSampler(0,0);
  if(width!=screenWidth || height!=screenHeight) {
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);screenWidth=width;screenHeight=height;
  }
  glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,width,height);
  for(const auto cap:State::caps)glDisable(cap);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glViewport(0,0,width,height);glUseProgram(program);glBindVertexArray(vertexArray);
  // GX's seven copy coefficients cover three EFB rows, not seven rows.
  glUniform3ui(glGetUniformLocation(program,"copyRows"),
      unsigned(filter[0])+filter[1],unsigned(filter[2])+filter[3]+filter[4],unsigned(filter[5])+filter[6]);
  glUniform1i(glGetUniformLocation(program,"frame"),0);glDrawArrays(GL_TRIANGLES,0,3);
  return true;
}
void ForgetDisplayBrightnessGl() {program=vertexArray=screen=0;screenWidth=screenHeight=0;}
void DestroyDisplayBrightnessGl() {if(program)glDeleteProgram(program);if(vertexArray)glDeleteVertexArrays(1,&vertexArray);if(screen)glDeleteTextures(1,&screen);ForgetDisplayBrightnessGl();}
} // namespace re4
