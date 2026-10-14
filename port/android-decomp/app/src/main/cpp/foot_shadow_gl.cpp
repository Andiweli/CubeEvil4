#include "foot_shadow_gl.h"
#include "room_lighting_gl.h"
#include <GLES3/gl3.h>
#include <stdexcept>
namespace re4 {
namespace {
GLuint program{},vao{},vbo{},image{};
GLuint compile(GLenum type,const std::string &source) {
  const auto result=glCreateShader(type);const auto *text=source.c_str();
  glShaderSource(result,1,&text,nullptr);glCompileShader(result);
  GLint success=0;glGetShaderiv(result,GL_COMPILE_STATUS,&success);
  if(!success){char log[1024]{};glGetShaderInfoLog(result,sizeof(log),nullptr,log);glDeleteShader(result);throw std::runtime_error(log);}
  return result;
}
}
void ForgetFootShadowGl(){program=vao=vbo=image=0;}
void DestroyFootShadowGl(){
  if(image)glDeleteTextures(1,&image);
  if(vbo)glDeleteBuffers(1,&vbo);
  if(vao)glDeleteVertexArrays(1,&vao);
  if(program)glDeleteProgram(program);
  ForgetFootShadowGl();
}
void CreateFootShadowGl(const std::shared_ptr<const Texture> &texture) {
  ForgetFootShadowGl();if(!texture)return;
  if(texture->rgba.size()!=std::size_t(texture->width)*texture->height*4)
    throw std::runtime_error("Foot shadow: Texture size");
  const std::string vertex=R"GLSL(#version 300 es
layout(location=0) in vec3 position;
layout(location=1) in vec2 uv;
layout(location=2) in float alpha;
uniform mat4 camera;
uniform vec3 center;
uniform float worldScale;
out vec2 texcoord;
out float opacity;
out vec3 worldPosition;
void main(){worldPosition=position;gl_Position=camera*vec4((position-center)*worldScale,1);texcoord=uv;opacity=alpha;}
)GLSL";
  const std::string fragment=R"GLSL(#version 300 es
precision highp float;
in vec2 texcoord;
in float opacity;
in vec3 worldPosition;
uniform sampler2D image;
out vec4 color;
void main(){color=fogRoomColor(vec4(0,0,0,texture(image,texcoord).a*opacity),worldPosition);}
)GLSL";
  auto a=compile(GL_VERTEX_SHADER,vertex);GLuint b=0;
  try{b=compile(GL_FRAGMENT_SHADER,RoomAtmosphereShader(fragment,false));}catch(...){glDeleteShader(a);throw;}
  program=glCreateProgram();glAttachShader(program,a);glAttachShader(program,b);glLinkProgram(program);
  glDeleteShader(a);glDeleteShader(b);GLint success=0;glGetProgramiv(program,GL_LINK_STATUS,&success);
  if(!success)throw std::runtime_error("Foot shadow: GLES program");
  glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
  unsigned sizes[]{3,2,1},offsets[]{0,3,5};
  for(unsigned i=0;i<3;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,sizes[i],GL_FLOAT,GL_FALSE,6*sizeof(float),reinterpret_cast<void*>(offsets[i]*sizeof(float)));}
  glGenTextures(1,&image);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,image);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,texture->width,texture->height,0,GL_RGBA,GL_UNSIGNED_BYTE,texture->rgba.data());
}
void DrawFootShadowGl(const float *camera,const std::array<float,3> &center,float scale,
                     const std::vector<FootShadowQuad> &quads) {
  if(!program||quads.empty())return;
  // Bound by the source light-pool maximum (128) and 12 table sprites/light.
  if(quads.size()>1536)throw std::runtime_error("Foot shadow: Sprite budget");
  std::vector<float> vertices;vertices.reserve(quads.size()*36);
  const unsigned corners[]{0,1,2,0,2,3};
  constexpr float u[]{0,1,1,0},v[]{0,0,1,1},x[]{-1,1,1,-1},z[]{-1,-1,1,1};
  for(const auto &q:quads)for(const auto k:corners){
    vertices.insert(vertices.end(),{q.position[0]+x[k]*q.size,q.position[1],q.position[2]+z[k]*q.size,u[k],v[k],q.alpha});
  }
  GLint oldProgram,oldVao,oldArray,oldActive,oldImage,oldDepth,srcRgb,dstRgb,srcAlpha,dstAlpha,equationRgb,equationAlpha;
  glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&oldArray);
  glGetIntegerv(GL_ACTIVE_TEXTURE,&oldActive);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldImage);
  glGetIntegerv(GL_DEPTH_FUNC,&oldDepth);glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb);glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);
  glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
  glGetIntegerv(GL_BLEND_EQUATION_RGB,&equationRgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equationAlpha);
  GLboolean writes;glGetBooleanv(GL_DEPTH_WRITEMASK,&writes);const auto blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE),depth=glIsEnabled(GL_DEPTH_TEST);
  glUseProgram(program);BindRoomFog(program);glUniformMatrix4fv(glGetUniformLocation(program,"camera"),1,GL_FALSE,camera);
  glUniform3fv(glGetUniformLocation(program,"center"),1,center.data());glUniform1f(glGetUniformLocation(program,"worldScale"),scale);
  glUniform1i(glGetUniformLocation(program,"image"),0);glBindTexture(GL_TEXTURE_2D,image);glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_DYNAMIC_DRAW);
  glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()/6));
  glDepthMask(writes);glDepthFunc(oldDepth);if(!depth)glDisable(GL_DEPTH_TEST);
  glBlendEquationSeparate(equationRgb,equationAlpha);glBlendFuncSeparate(srcRgb,dstRgb,srcAlpha,dstAlpha);
  if(!blend)glDisable(GL_BLEND);
  if(cull)glEnable(GL_CULL_FACE);
  glBindTexture(GL_TEXTURE_2D,oldImage);glActiveTexture(oldActive);glBindVertexArray(oldVao);glBindBuffer(GL_ARRAY_BUFFER,oldArray);glUseProgram(oldProgram);
}
} // namespace re4
