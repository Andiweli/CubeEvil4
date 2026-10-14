#include "radio_term_gl.h"
#include "actor_gl.h"
#include "event_actor_gl.h"
#include "event_mirror_gl.h"
#include "environment_gl.h"
#include "room_lighting_gl.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
namespace re4 {
namespace {
GLuint program{},vao{},vbo{};const RadioTermRuntime *owner{};
std::map<std::pair<unsigned,unsigned>,GLuint> images;
std::map<const GenericEventActor*,std::uint64_t> actors;
struct State {
 GLint prog{},array{},buffer{},active{},tex[3]{},sampler[3]{},viewport[4]{},func{},src[2]{},dst[2]{},eq[2]{};
 GLboolean depth{},blend{},cull{},scissor{},stencil{},depthMask{},colorMask[4]{};
 GLfloat clearDepth{};
 State(){glGetIntegerv(GL_CURRENT_PROGRAM,&prog);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&array);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
  for(unsigned i=0;i<3;++i){glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&tex[i]);glGetIntegerv(GL_SAMPLER_BINDING,&sampler[i]);}glActiveTexture(GLenum(active));
  glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_DEPTH_FUNC,&func);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);glGetBooleanv(GL_COLOR_WRITEMASK,colorMask);glGetFloatv(GL_DEPTH_CLEAR_VALUE,&clearDepth);
  depth=glIsEnabled(GL_DEPTH_TEST);blend=glIsEnabled(GL_BLEND);cull=glIsEnabled(GL_CULL_FACE);scissor=glIsEnabled(GL_SCISSOR_TEST);stencil=glIsEnabled(GL_STENCIL_TEST);
  glGetIntegerv(GL_BLEND_SRC_RGB,&src[0]);glGetIntegerv(GL_BLEND_SRC_ALPHA,&src[1]);glGetIntegerv(GL_BLEND_DST_RGB,&dst[0]);glGetIntegerv(GL_BLEND_DST_ALPHA,&dst[1]);glGetIntegerv(GL_BLEND_EQUATION_RGB,&eq[0]);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eq[1]);}
 ~State(){glUseProgram(GLuint(prog));glBindVertexArray(GLuint(array));glBindBuffer(GL_ARRAY_BUFFER,GLuint(buffer));
  for(unsigned i=0;i<3;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,GLuint(tex[i]));glBindSampler(i,GLuint(sampler[i]));}glActiveTexture(GLenum(active));glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
  glDepthFunc(GLenum(func));glDepthMask(depthMask);glClearDepthf(clearDepth);glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]);glBlendFuncSeparate(GLenum(src[0]),GLenum(dst[0]),GLenum(src[1]),GLenum(dst[1]));glBlendEquationSeparate(GLenum(eq[0]),GLenum(eq[1]));
  for(const auto &v:{std::pair<GLenum,GLboolean>{GL_DEPTH_TEST,depth},{GL_BLEND,blend},{GL_CULL_FACE,cull},{GL_SCISSOR_TEST,scissor},{GL_STENCIL_TEST,stencil}})if(v.second)glEnable(v.first);else glDisable(v.first);
 }
};
GLuint compile(GLenum type,const char *text){const auto shader=glCreateShader(type);glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);GLint ok{};glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
 if(!ok){char log[1024]{};glGetShaderInfoLog(shader,sizeof log,nullptr,log);glDeleteShader(shader);throw std::runtime_error(std::string("Radio GPU: ")+log);}return shader;}
GLuint image(unsigned id,unsigned frame){const auto i=images.find({id,frame});if(i==images.end())throw std::runtime_error("Radio GPU: actual ID texture is missing "+std::to_string(id)+":"+std::to_string(frame));return i->second;}
void ui(int width,int height,const RadioTermRuntime &runtime,unsigned group){
 glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);
 glUniform1i(glGetUniformLocation(program,"image"),0);glUniform1i(glGetUniformLocation(program,"maskImage"),1);glUniform1i(glGetUniformLocation(program,"frameImage"),2);glUniform2f(glGetUniformLocation(program,"viewport"),float(width),float(height));
 for(unsigned i=0;i<3;++i)glBindSampler(i,0);
 constexpr GLenum src[]{GL_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE,GL_DST_COLOR,GL_DST_COLOR},dst[]{GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE,GL_ONE,GL_ZERO};
 // IDSystem::unitTrans inserts children first; the original OT executes the
 // equal-priority entries in reverse insertion order. Runtime.quads already
 // provides this exact reversed source-tree postorder for each class.
 for(const auto &q:runtime.quads())if(q.group==group){if(q.blend>4)throw std::runtime_error("Radio GPU: ID blend budget");
  const auto effect=runtime.textureEffect(q.group,q.unit);glUniform1i(glGetUniformLocation(program,"effect"),int(effect[0]));
  glUniform1i(glGetUniformLocation(program,"negativePower"),int(std::min(effect[1],2u)));
  if(effect[0]){
   const auto frame=CaptureEnvironmentScreen(width,height,true);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,frame);
   // The recovered IdShimmerTrans has an uninitialised local depth; its
   // defined lower bound is1500. Keep this source reconstruction limitation
   // explicit instead of substituting a random viewport-dependent strength.
   const float coefficient=(q.color[3]/255)*.04f*1000.f/1500.f*(float(effect[1])/32+1);
   const int encoded=int(coefficient*1024)&2047;glUniform1i(glGetUniformLocation(program,"indCoefficient"),encoded>=1024?encoded-2048:encoded);
  }
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,image(q.texture,runtime.textureFrame(q.group,q.unit)));
  glUniform1i(glGetUniformLocation(program,"masked"),q.mask!=255);
  if(q.mask!=255){glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,image(q.mask,runtime.textureFrame(q.group,q.unit,true)));glActiveTexture(GL_TEXTURE0);}
  glBlendFunc(src[q.blend],dst[q.blend]);std::array<float,4> color{};for(unsigned k=0;k<4;++k)color[k]=std::uint8_t(q.color[k])/255.f;glUniform4fv(glGetUniformLocation(program,"rgba"),1,color.data());
  constexpr unsigned corners[]{0,1,2,0,2,3};constexpr float uv[][2]{{0,0},{1,0},{1,1},{0,1}};float vertices[24]{};
  for(unsigned k=0;k<6;++k){const auto c=corners[k];vertices[k*4]=q.vertices[c*2]/320;vertices[k*4+1]=q.vertices[c*2+1]/180;vertices[k*4+2]=uv[c][0];vertices[k*4+3]=uv[c][1];}
  glBufferData(GL_ARRAY_BUFFER,sizeof vertices,vertices,GL_STREAM_DRAW);glDrawArrays(GL_TRIANGLES,0,6);
 }
}
}
void ForgetRadioTermGl(){for(const auto &a:actors)ForgetGenericActorGl(a.first);actors.clear();images.clear();program=vao=vbo=0;owner=nullptr;}
void DestroyRadioTermGl(){for(const auto &a:actors)DestroyGenericActorGl(a.first);for(const auto &i:images)glDeleteTextures(1,&i.second);if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);if(program){GLint active{};glGetIntegerv(GL_CURRENT_PROGRAM,&active);if(GLuint(active)==program)glUseProgram(0);glDeleteProgram(program);}ForgetRadioTermGl();}
void CreateRadioTermGl(const RadioTermRuntime &runtime){DestroyRadioTermGl();State previous;owner=&runtime;
 try{
 const char *vs=R"GLSL(#version 300 es
precision highp float;layout(location=0)in vec2 p;layout(location=1)in vec2 uv;out vec2 st;
void main(){st=uv;gl_Position=vec4(p,0,1);}
)GLSL";
 const char *fs=R"GLSL(#version 300 es
precision highp float;precision highp int;in vec2 st;uniform sampler2D image,maskImage,frameImage;uniform vec4 rgba;uniform vec2 viewport;
uniform int effect,negativePower,indCoefficient;uniform bool masked;out vec4 c;
void main(){vec4 t=texture(image,st);
 if(effect==1){vec3 f=clamp(texture(frameImage,gl_FragCoord.xy/viewport).rgb*exp2(float(negativePower)),vec3(0),vec3(1));c=vec4((vec3(1)-f)*rgba.rgb,t.a*rgba.a);}
 else if(effect==2 || effect==3){ivec2 v=ivec2(floor(t.ab*255.0+0.5));if(effect==3)v-=ivec2(128);ivec2 shift=(v*indCoefficient)>>2;
 vec2 delta=vec2(shift)/128.0/(viewport*0.5);c=vec4(texture(frameImage,gl_FragCoord.xy/viewport+vec2(delta.x,-delta.y)).rgb*rgba.rgb,rgba.a);}
 else c=t*rgba;
 if(masked)c.a*=texture(maskImage,st).a;if(c.a<=1.0/255.0)discard;c=clamp(c,vec4(0),vec4(1));}
)GLSL";
 const auto vert=compile(GL_VERTEX_SHADER,vs),frag=compile(GL_FRAGMENT_SHADER,fs);program=glCreateProgram();glAttachShader(program,vert);glAttachShader(program,frag);glLinkProgram(program);glDeleteShader(vert);glDeleteShader(frag);GLint linked{};glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked)throw std::runtime_error("Radio GPU: Shader linking");
 glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),nullptr);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),reinterpret_cast<void*>(2*sizeof(float)));glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
 glActiveTexture(GL_TEXTURE0);for(const auto &t:runtime.textures()){GLuint texture{};glGenTextures(1,&texture);images[{t.id,t.frame}]=texture;glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,t.image.width,t.image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,t.image.rgba.data());}
 for(const auto &a:runtime.actors()){CreateGenericActorGl(a.get(),a->mesh(),a->skin());actors[a.get()]=a->meshRevision();}
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Radio GPU: Original data upload");
 }catch(...){DestroyRadioTermGl();throw;}
}
void DrawRadioTermGl(int width,int height,const RadioTermRuntime &runtime){if(width<=0 || height<=0)throw std::runtime_error("Radio GPU: Screen budget");if(owner!=&runtime)CreateRadioTermGl(runtime);State previous;
 auto atmosphere=RoomAtmosphere();RoomAtmosphere().data=runtime.lighting();RoomAtmosphere().eye={0,0,2000};RoomAtmosphere().right={1,0,0};RoomAtmosphere().up={0,1,0};RoomAtmosphere().look={0,0,-1};RoomAtmosphere().playerParts.clear();RoomAtmosphere().transientLights.clear();
 try{
 glViewport(0,0,width,height);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
 ui(width,height,runtime,8);glDepthMask(GL_TRUE);glClearDepthf(1);glClear(GL_DEPTH_BUFFER_BIT);
 EventCamera view;view.valid=true;view.eye={0,0,2000};view.target={0,0,0};view.fovRadians=50*.017453292f;
 const auto camera=EventMirrorCameraMatrix(view,{},1);
 for(const auto &a:runtime.actors()){auto &revision=actors.at(a.get());if(revision!=a->meshRevision()){UpdateGenericActorGl(a.get(),a->mesh());revision=a->meshRevision();}const auto *rig=a->rig();if(!rig)throw std::runtime_error("Radio GPU: Original motion is missing");
  const auto light=TransformPoint(a->worldMatrix(),{0,0,0});DrawGenericActorGl(a.get(),camera.data(),{},1,rig->bones(),rig->bindCenters(),a->worldMatrix(),false,1,&light,1414.213562f);
 }ui(width,height,runtime,9);DrawOriginalRoomMessageGl(width,height,runtime.data().font(),runtime.message(),true);
 DrawEventFadeGl(width,height,runtime.fadeAlpha());
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Radio GPU: Drawing");
 RoomAtmosphere()=std::move(atmosphere);RoomLightBindings().valid=false;
 }catch(...){RoomAtmosphere()=std::move(atmosphere);RoomLightBindings().valid=false;throw;}
}
}
