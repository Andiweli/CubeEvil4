#include "room_shadows_gl.h"
#include "actor_gl.h"
#include "enemy_encounter.h"
#include "event_mirror_gl.h"
#include "room_lighting_gl.h"
#include "render_state_gl.h"
#include <cmath>
#include <stdexcept>
namespace re4 {
namespace {
GLuint program{},vao{},vbo{},images[3]{},buffers[3]{};
GLsizei corners{};
GLuint shader(GLenum type,const std::string &source) {
  const auto object=glCreateShader(type);const auto *text=source.c_str();glShaderSource(object,1,&text,nullptr);glCompileShader(object);
  GLint ok{};glGetShaderiv(object,GL_COMPILE_STATUS,&ok);
  if(!ok){char log[2048]{};glGetShaderInfoLog(object,sizeof log,nullptr,log);glDeleteShader(object);throw std::runtime_error(log);}
  return object;
}
void resources() {
  if(images[0])return;
  glGenTextures(3,images);glGenFramebuffers(3,buffers);
  for(unsigned i=0;i<3;++i) {
    glBindTexture(GL_TEXTURE_2D,images[i]);const int size=256>>i;
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER,buffers[i]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,images[i],0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("SHD: incomplete shadow buffer");
  }
}
void resize(unsigned from,unsigned to) {
  glBindFramebuffer(GL_READ_FRAMEBUFFER,buffers[from]);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,buffers[to]);
  glBlitFramebuffer(0,0,256>>from,256>>from,0,0,256>>to,256>>to,GL_COLOR_BUFFER_BIT,GL_LINEAR);
}
}
void CreateRoomShadowsGl(const std::shared_ptr<const RoomShadowData> &data) {
  ForgetRoomShadowsGl();if(!data || data->vertices.empty())return;
  const ScopedRenderStateGl previous;
  constexpr const char *vertex=R"(#version 300 es
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
uniform mat4 camera,lightCamera;
uniform vec3 center,eye,look,lightPosition,lightDirection;
uniform float worldScale,coneCosine;
out vec4 projected;
out vec3 worldPosition;
out float rasterAlpha;
void main(){
  // shadowModelTrans pushes the receiver by the source one-unit camera look.
  vec3 world=position-look;
  worldPosition=world;gl_Position=camera*vec4((world-center)*worldScale,1);
  projected=lightCamera*vec4(world,1);
  vec3 l=normalize(lightPosition-position);
  float cone=max((dot(-l,lightDirection)-coneCosine)/(1.0-coneCosine),0.0);
  rasterAlpha=clamp(max(dot(normalize(normal),l),0.0)*cone,0.0,1.0);
}
)";
  constexpr const char *fragment=R"(#version 300 es
precision highp float;
in vec4 projected;
in vec3 worldPosition;
in float rasterAlpha;
uniform sampler2D shadow;
out vec4 color;
void main(){
  if(projected.w<=0.0)discard;
  vec2 uv=projected.xy/projected.w*.5+.5;
  float alpha=texture(shadow,uv).a*rasterAlpha;
  if(alpha<=0.0 || alpha>=1.0)discard; // GX_GREATER0 AND GX_LESS255.
  color=fogRoomColor(vec4(0,0,0,alpha),worldPosition);
}
)";
  const auto vs=shader(GL_VERTEX_SHADER,vertex);GLuint fs{};
  try{fs=shader(GL_FRAGMENT_SHADER,RoomAtmosphereShader(fragment,false));}catch(...){glDeleteShader(vs);throw;}
  program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);glDeleteShader(vs);glDeleteShader(fs);
  GLint ok{};glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){char log[2048]{};glGetProgramInfoLog(program,sizeof log,nullptr,log);throw std::runtime_error(log);}
  glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
  glBufferData(GL_ARRAY_BUFFER,data->vertices.size()*sizeof(float),data->vertices.data(),GL_STATIC_DRAW);
  for(unsigned i=0;i<2;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,3,GL_FLOAT,GL_FALSE,6*sizeof(float),reinterpret_cast<void*>(i*3*sizeof(float)));}
  corners=GLsizei(data->vertices.size()/6);
}
void DrawRoomEnemyShadowGl(const float *camera,const std::array<float,3> &center,
                          float scale,const EnemyEncounter &enemy) {
  const auto *cut=CurrentRoomLightCut();if(!program || !cut || !enemy.visible())return;
  const auto target=TransformPoint(enemy.placement(),TransformPoint(enemy.actor().rig().bones().at(0),{0,0,0}));
  for(const auto &light:cut->lights) {
    // Source Fit_ParallelShadowModelSet. This path implements fit lights;
    // foot, fixed, parallel and self shadows have separate source owners.
    if((light.flags&3)!=3 || light.id!=4 || light.type!=0 || !(light.enableMask&2) || light.shadow.kind!=0 || light.parent)continue;
    auto eye=(light.shadow.flags&2)?light.shadow.sourcePosition:light.position;
    float distance=0;std::array<float,3> direction{};
    for(unsigned k=0;k<3;++k){direction[k]=target[k]-eye[k];distance+=direction[k]*direction[k];}
    distance=std::sqrt(distance);if(distance<=0 || (light.radius && distance>light.radius))continue;
    for(auto &v:direction)v/=distance;
    const float radius=1300; // Source Type0 LightInfo: Size.y*1.1+200.
    float fov=std::atan2(radius,distance)*114.59156f-light.shadow.fovySub;
    fov=std::max(1.f,fov);if(fov>89.95f){fov=89.95f;for(unsigned k=0;k<3;++k)eye[k]=target[k]-direction[k]*radius;}
    unsigned alpha=light.color[0];
    if(light.radius && distance/light.radius>.7f)alpha=unsigned(alpha*(1.f-distance/light.radius)*(10.f/3.f));
    if(!alpha)continue;
    EventCamera view;view.valid=true;view.fovRadians=fov*.017453292f;
    for(unsigned k=0;k<3;++k){view.eye[k]=eye[k];view.target[k]=target[k];}
    // The RTT/shadow lens helper uses4:3, so adjust FOV's X component to
    // the source shadow square's aspect1 after its world-space view.
    auto lightCamera=EventMirrorCameraMatrix(view,{},1);
    for(unsigned column=0;column<4;++column)lightCamera[column*4]*=1.3333334f;
    const ScopedRenderStateGl previous;glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);resources();
    for(const auto cap:caps)glDisable(cap);
    glBindFramebuffer(GL_FRAMEBUFFER,buffers[0]);glViewport(0,0,256,256);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glEnable(GL_SCISSOR_TEST);glScissor(2,2,252,252);
    DrawEnemyShadowMaskGl(lightCamera.data(),enemy.actor(),enemy.placement(),alpha/255.f);
    glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);resize(0,1);
    if(light.shadow.softLevel) {
      // MakeSoftShadow level1:256->128->64->256->128. The authored house
      // lights use hard and level1 masks; no generated blur kernel.
      resize(1,2);resize(2,0);resize(0,1);
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(previous.draw));glViewport(previous.viewport[0],previous.viewport[1],previous.viewport[2],previous.viewport[3]);
    glUseProgram(program);BindRoomFog(program);glBindVertexArray(vao);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,images[1]);glBindSampler(0,0);
    glUniformMatrix4fv(glGetUniformLocation(program,"camera"),1,GL_FALSE,camera);
    glUniformMatrix4fv(glGetUniformLocation(program,"lightCamera"),1,GL_FALSE,lightCamera.data());
    glUniform3fv(glGetUniformLocation(program,"center"),1,center.data());glUniform1f(glGetUniformLocation(program,"worldScale"),scale);
    glUniform3fv(glGetUniformLocation(program,"look"),1,RoomAtmosphere().look.data());glUniform3fv(glGetUniformLocation(program,"lightPosition"),1,eye.data());
    glUniform3fv(glGetUniformLocation(program,"lightDirection"),1,direction.data());glUniform1f(glGetUniformLocation(program,"coneCosine"),std::cos(fov*.9f*.017453292f));
    glUniform1i(glGetUniformLocation(program,"shadow"),0);glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
    glDrawArrays(GL_TRIANGLES,0,corners);
  }
}
void ForgetRoomShadowsGl(){program=vao=vbo=0;for(unsigned i=0;i<3;++i)images[i]=buffers[i]=0;corners=0;}
void DestroyRoomShadowsGl(){if(program)glDeleteProgram(program);if(vao)glDeleteVertexArrays(1,&vao);if(vbo)glDeleteBuffers(1,&vbo);glDeleteTextures(3,images);glDeleteFramebuffers(3,buffers);ForgetRoomShadowsGl();}
} // namespace re4
