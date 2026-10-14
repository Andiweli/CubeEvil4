// cloth.cpp::clothTrans: OT13, two-sided source grid, original TPL0 and
// scenery lighting, TEV texture*raster scale4, alpha>=4 and source Z/blend.
#include "room_effect_cloth_gl.h"
#include "room_lighting_gl.h"
#include <GLES3/gl3.h>
#include <cstddef>
#include <stdexcept>
namespace re4 {
namespace {
GLuint program{},vao{},vbo{};
std::vector<GLuint> images;
constexpr std::array<GLenum,11> caps{GL_DEPTH_TEST,GL_BLEND,GL_CULL_FACE,
 GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_RASTERIZER_DISCARD,GL_POLYGON_OFFSET_FILL,
 GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE,GL_DITHER,GL_PRIMITIVE_RESTART_FIXED_INDEX};
struct State {
 GLint program{},vao{},buffer{},active{},texture{},sampler{},depthFunc{},cull{},front{},
   srcRgb{},srcAlpha{},dstRgb{},dstAlpha{},eqRgb{},eqAlpha{},unpack{};
 GLboolean enabled[11]{},depthMask{},colorMask[4]{};GLfloat depthRange[2]{};
 State() {
  glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
  glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glGetIntegerv(GL_SAMPLER_BINDING,&sampler);
  glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);glGetIntegerv(GL_CULL_FACE_MODE,&cull);glGetIntegerv(GL_FRONT_FACE,&front);
  glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb);glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);
  glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
  glGetIntegerv(GL_BLEND_EQUATION_RGB,&eqRgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eqAlpha);
  glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
  glGetBooleanv(GL_COLOR_WRITEMASK,colorMask);glGetFloatv(GL_DEPTH_RANGE,depthRange);
  for(unsigned i=0;i<caps.size();++i)enabled[i]=glIsEnabled(caps[i]);
 }
 ~State() {
  glUseProgram(GLuint(program));glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(buffer));
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,GLuint(texture));glBindSampler(0,GLuint(sampler));glActiveTexture(GLenum(active));
  glDepthFunc(GLenum(depthFunc));glDepthMask(depthMask);glDepthRangef(depthRange[0],depthRange[1]);
  glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]);glCullFace(GLenum(cull));glFrontFace(GLenum(front));
  glBlendFuncSeparate(GLenum(srcRgb),GLenum(dstRgb),GLenum(srcAlpha),GLenum(dstAlpha));glBlendEquationSeparate(GLenum(eqRgb),GLenum(eqAlpha));
  glPixelStorei(GL_UNPACK_ALIGNMENT,unpack);
  for(unsigned i=0;i<caps.size();++i)if(enabled[i])glEnable(caps[i]);else glDisable(caps[i]);
 }
};
GLuint compile(GLenum type,const std::string &source) {
 const auto shader=glCreateShader(type);const char*p=source.c_str();glShaderSource(shader,1,&p,nullptr);glCompileShader(shader);
 GLint ok{};glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);if(!ok) {
  char error[2048]{};glGetShaderInfoLog(shader,sizeof error,nullptr,error);glDeleteShader(shader);throw std::runtime_error(error);
 }return shader;
}
void pipeline() {
 const auto vs=compile(GL_VERTEX_SHADER,RoomAtmosphereShader(R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 p;
layout(location=1) in vec3 n;
layout(location=2) in vec2 uv;
uniform mat4 matrix;
uniform vec3 center;
uniform float scale;
uniform vec4 material;
out vec2 texcoord;
out vec3 worldPosition;
out vec4 raster;
void main(){worldPosition=p;texcoord=uv;raster=litRoomColor(vec4(1),p,n,0)*material;gl_Position=matrix*vec4((p-center)*scale,1);}
)GLSL",true));
 const auto fs=compile(GL_FRAGMENT_SHADER,RoomAtmosphereShader(R"GLSL(#version 300 es
precision highp float;
in vec2 texcoord;
in vec3 worldPosition;
in vec4 raster;
uniform sampler2D image;
out vec4 color;
void main(){vec4 c=clamp(texture(image,texcoord)*raster*4.0,0.0,1.0);if(c.a*255.0<4.0)discard;color=fogRoomColor(c,worldPosition);}
)GLSL",false));
 program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);glDeleteShader(vs);glDeleteShader(fs);
 GLint ok{};glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){char error[2048]{};glGetProgramInfoLog(program,sizeof error,nullptr,error);throw std::runtime_error(error);}
 glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
 glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(RoomEffectClothVertex),reinterpret_cast<void*>(offsetof(RoomEffectClothVertex,position)));
 glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(RoomEffectClothVertex),reinterpret_cast<void*>(offsetof(RoomEffectClothVertex,normal)));
 glEnableVertexAttribArray(2);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(RoomEffectClothVertex),reinterpret_cast<void*>(offsetof(RoomEffectClothVertex,uv)));
}
} // namespace
void ForgetRoomEffectClothGl(){program=vao=vbo=0;images.clear();}
void DestroyRoomEffectClothGl() {
 if(program){GLint current{};glGetIntegerv(GL_CURRENT_PROGRAM,&current);if(GLuint(current)==program)glUseProgram(0);glDeleteProgram(program);}
 if(vao)glDeleteVertexArrays(1,&vao);
 if(vbo)glDeleteBuffers(1,&vbo);
 if(!images.empty())glDeleteTextures(GLsizei(images.size()),images.data());
 ForgetRoomEffectClothGl();
}
void CreateRoomEffectClothGl(const WorldEffectsData &data) {
 DestroyRoomEffectClothGl();State previous;
 bool any=false;for(const auto &target:data.sprites.targets)for(const auto &record:target.records)if(record.enabled && record.bytes[1]==78)any=true;
 if(!any)return;
 pipeline();images.resize(data.sprites.animations.size());glActiveTexture(GL_TEXTURE0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
 for(unsigned i=0;i<data.sprites.animations.size();++i) {
  const auto &animation=data.sprites.animations[i];if(animation.images.empty())continue;
  const auto &texture=animation.images.front();
  if(!texture.width || !texture.height || texture.rgba.size()!=std::size_t(texture.width)*texture.height*4)
   throw std::runtime_error("Cloth: original TPL0 is missing");
  glGenTextures(1,&images[i]);glBindTexture(GL_TEXTURE_2D,images[i]);
  // ClothTexSetUp ignores TPL wrap/LOD metadata: GXInitTexObj clamp, no mip.
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,GLsizei(texture.width),GLsizei(texture.height),0,GL_RGBA,GL_UNSIGNED_BYTE,texture.rgba.data());
 }
}
void DrawRoomEffectClothGl(const float *matrix,const std::array<float,3> &center,float scale,
                           const std::vector<RoomEffectClothMesh> &meshes) {
 if(meshes.empty())return;
 if(!program || !matrix)throw std::runtime_error("Cloth: GPU room resources/camera missing");
 State previous;
 for(auto cap:caps)glDisable(cap);
 glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthRangef(0,1);glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);
 glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
 glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glUniform1i(glGetUniformLocation(program,"image"),0);
 glUniformMatrix4fv(glGetUniformLocation(program,"matrix"),1,GL_FALSE,matrix);glUniform3fv(glGetUniformLocation(program,"center"),1,center.data());
 glUniform1f(glGetUniformLocation(program,"scale"),scale);BindRoomFog(program);
 for(const auto &mesh:meshes) {
  if(mesh.vertices.empty())continue;
  if(mesh.animation>=images.size() || !images[mesh.animation])throw std::runtime_error("Cloth: Original texture index");
  glBindTexture(GL_TEXTURE_2D,images[mesh.animation]);
  glDepthMask(mesh.additive?GL_FALSE:GL_TRUE);glBlendFunc(mesh.additive?GL_ONE:GL_SRC_ALPHA,mesh.additive?GL_ONE:GL_ONE_MINUS_SRC_ALPHA);
  glUniform4f(glGetUniformLocation(program,"material"),mesh.color[0]/255.f,mesh.color[1]/255.f,mesh.color[2]/255.f,mesh.color[3]/255.f);
  BindRoomLights(program,16,0xffffffff,mesh.lightCenter,mesh.lightSize);
  glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(mesh.vertices.size()*sizeof(RoomEffectClothVertex)),mesh.vertices.data(),GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES,0,GLsizei(mesh.vertices.size()));
 }
}
} // namespace re4
