#include "graphics_options_gl.h"
// Source game/filter00.cpp: previous-frame blur, copy, then contrast.
#include "room_filter_gl.h"
#include "room_lighting_gl.h"
#include "render_state_gl.h"
#include <algorithm>
#include <stdexcept>
namespace re4 {
namespace {
constexpr int historyWidth=256,historyHeight=224; // SCR_W/2, SCR_H/2.
constexpr std::size_t historyBytes=historyWidth*historyHeight*4;
GLuint program{},vao{},base{},output{},framebuffer{};
RoomFilterState clock;
GLuint shader(GLenum type,const char *source) {
  const auto object=glCreateShader(type);glShaderSource(object,1,&source,nullptr);glCompileShader(object);
  GLint ok{};glGetShaderiv(object,GL_COMPILE_STATUS,&ok);
  if(!ok){char log[2048]{};glGetShaderInfoLog(object,sizeof log,nullptr,log);glDeleteShader(object);throw std::runtime_error(log);}
  return object;
}
void prepare() {
  if(program)return;
  constexpr const char *vertex=R"(#version 300 es
out vec2 uv;
void main(){uv=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=vec4(uv*2.0-1.0,0,1);}
)";
  constexpr const char *fragment=R"(#version 300 es
precision highp float;
precision highp int;
in vec2 uv;
uniform sampler2D history;
uniform vec4 parameters; // inset, byte bias, TEV scale, alpha.
out vec4 color;
void main(){
  vec3 rgb=texture(history,mix(vec2(parameters.x),vec2(1.0-parameters.x),uv)).rgb;
  // The subtraction's TEV clamp follows its scale (levels1/2/3).
  rgb=clamp((rgb-vec3(parameters.y))*parameters.z,0.0,1.0);
  color=vec4(rgb,parameters.w);
}
)";
  const auto vs=shader(GL_VERTEX_SHADER,vertex);GLuint fs{};
  try{fs=shader(GL_FRAGMENT_SHADER,fragment);}catch(...){glDeleteShader(vs);throw;}
  program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);
  glDeleteShader(vs);glDeleteShader(fs);GLint ok{};glGetProgramiv(program,GL_LINK_STATUS,&ok);
  if(!ok){char log[2048]{};glGetProgramInfoLog(program,sizeof log,nullptr,log);throw std::runtime_error(log);}
  glGenVertexArrays(1,&vao);glGenTextures(1,&base);glGenTextures(1,&output);glGenFramebuffers(1,&framebuffer);
  for(const auto image:{base,output}) {
    glBindTexture(GL_TEXTURE_2D,image);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,historyWidth,historyHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
  }
}
void attach(GLenum target,GLuint texture) {
  glBindFramebuffer(target,framebuffer);glFramebufferTexture2D(target,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
  if(glCheckFramebufferStatus(target)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Filter00: incomplete framebuffer");
}
void draw(GLuint image,float inset,float bias,float scale,float alpha) {
  glBindTexture(GL_TEXTURE_2D,image);glUniform4f(glGetUniformLocation(program,"parameters"),inset,bias,scale,alpha);
  glDrawArrays(GL_TRIANGLES,0,3);
}
}
bool ApplyRoomFilterGl(int width,int height,const RoomLightCut &cut,std::uint64_t frame,
                       std::shared_ptr<const RoomLighting> room) {
  if(width<=0 || height<=0)return false;
  if(!cut.blurRate && !cut.contrast[0]){clock={};return false;}
  if(clock.room!=room)clock={};
  clock.room=std::move(room);
  const ScopedRenderStateGl previous;glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);prepare();
  for(const auto cap:caps)glDisable(cap);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);glDepthMask(GL_FALSE);
  glViewport(0,0,width,height);glUseProgram(program);glBindVertexArray(vao);glUniform1i(glGetUniformLocation(program,"history"),0);
  // A second display refresh of the same source tick reuses the same input
  // history. It cannot accumulate blur while paused or change a restored frame.
  if(clock.initialized && clock.frame!=frame){std::swap(base,output);clock.baseValid=clock.outputValid;}
  clock.initialized=true;clock.frame=frame;
  if(clock.baseValid && cut.blurRate) {
    glEnable(GL_BLEND);glBlendEquation(cut.blurType==3?GL_FUNC_REVERSE_SUBTRACT:GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA,cut.blurType==2?GL_ONE:GL_ONE_MINUS_SRC_ALPHA);
    const float inset=cut.blurType==1?cut.blurPower/1024.f:0;
    const float bias=cut.blurType==2?float(std::uint8_t(cut.blurPower))/255.f:0;
    draw(base,inset,bias,1,cut.blurRate/255.f);
  }
  glDisable(GL_BLEND);
  // GXCopyTex captures the blurred scene before the additive contrast pass.
  GLint destination{};glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&destination);
  const auto source=ReadableFramebufferGl(GLuint(destination),width,height);
  attach(GL_DRAW_FRAMEBUFFER,output);glBindFramebuffer(GL_READ_FRAMEBUFFER,source);
  glBlitFramebuffer(0,0,width,height,0,0,historyWidth,historyHeight,GL_COLOR_BUFFER_BIT,GL_LINEAR);
  clock.outputValid=true;glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(destination));
  if(cut.contrast[0]>=1 && cut.contrast[0]<=3) {
    glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    draw(output,0,cut.contrast[2]/255.f,float(1u<<(cut.contrast[0]-1)),cut.contrast[1]/255.f);
  }
  return true;
}
std::shared_ptr<const RoomFilterState> SaveRoomFilterGl() {
  // createScene is also entered after EGL has discarded the old context.
  // A previous-context FBO cannot be read back in that fresh context.
  if(!program || !clock.initialized || !glIsProgram(program))return {};
  const ScopedRenderStateGl previous;auto state=std::make_shared<RoomFilterState>(clock);
  glPixelStorei(GL_PACK_ALIGNMENT,1);
  for(unsigned i=0;i<2;++i)if(i?clock.outputValid:clock.baseValid) {
    auto &pixels=i?state->output:state->base;pixels.resize(historyBytes);attach(GL_READ_FRAMEBUFFER,i?output:base);
    glReadPixels(0,0,historyWidth,historyHeight,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
  }
  return state;
}
void RestoreRoomFilterGl(const std::shared_ptr<const RoomFilterState> &state) {
  clock={};if(!state)return;
  // Filter00RoomInit clears history on a room change. Event-cut LITs may
  // differ within the same room and deliberately retain its history.
  if(state->room && state->room!=RoomAtmosphere().data)return;
  if((state->baseValid && state->base.size()!=historyBytes) || (state->outputValid && state->output.size()!=historyBytes))
    throw std::runtime_error("Filter00: invalid restore state");
  const ScopedRenderStateGl previous;glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);prepare();glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  for(unsigned i=0;i<2;++i)if(i?state->outputValid:state->baseValid) {
    glBindTexture(GL_TEXTURE_2D,i?output:base);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,historyWidth,historyHeight,GL_RGBA,GL_UNSIGNED_BYTE,(i?state->output:state->base).data());
  }
  clock=*state;clock.base.clear();clock.output.clear();
}
void ForgetRoomFilterGl(){program=vao=base=output=framebuffer=0;clock={};}
void DestroyRoomFilterGl(){if(program)glDeleteProgram(program);if(vao)glDeleteVertexArrays(1,&vao);if(base)glDeleteTextures(1,&base);if(output)glDeleteTextures(1,&output);if(framebuffer)glDeleteFramebuffers(1,&framebuffer);ForgetRoomFilterGl();}
} // namespace re4
