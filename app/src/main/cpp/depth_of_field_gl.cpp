#include "depth_of_field_gl.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstdio>
namespace re4 {
namespace {
GLuint focusProgram{},blurProgram{},vao{},frameFbo{},focusFbo{},frame{},depth{},focus{};
int frameWidth{},frameHeight{};
GLenum depthFormat{};
bool worldFocusReady{};
GLuint shader(GLenum type,const char *source) {
 const auto s=glCreateShader(type);glShaderSource(s,1,&source,nullptr);glCompileShader(s);
 GLint ok{};glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
 if(!ok){char log[1024]{};glGetShaderInfoLog(s,sizeof log,nullptr,log);glDeleteShader(s);throw std::runtime_error(log);}return s;
}
GLuint link(const char *fragment) {
 constexpr const char *vertex=R"(#version 300 es
void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=vec4(p*2.0-1.0,0.0,1.0);})";
 auto vs=shader(GL_VERTEX_SHADER,vertex);GLuint fs{};
 try{fs=shader(GL_FRAGMENT_SHADER,fragment);}catch(...){glDeleteShader(vs);throw;}
 auto p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);glDeleteShader(vs);glDeleteShader(fs);
 GLint ok{};glGetProgramiv(p,GL_LINK_STATUS,&ok);
 if(!ok){char log[1024]{};glGetProgramInfoLog(p,sizeof log,nullptr,log);glDeleteProgram(p);throw std::runtime_error(log);}return p;
}
struct State {
 GLint program{},vertexArray{},active{},texture[3]{},sampler[3]{},draw{},read{},viewport[4]{};
 GLboolean mask[4]{},depthMask{};
 static constexpr GLenum caps[]{GL_BLEND,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_RASTERIZER_DISCARD,GL_DITHER,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE};
 GLboolean enabled[9]{};
 State(){
  glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vertexArray);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
  for(int i=0;i<3;++i){glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture[i]);glGetIntegerv(GL_SAMPLER_BINDING,&sampler[i]);}
  glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
  glGetIntegerv(GL_VIEWPORT,viewport);glGetBooleanv(GL_COLOR_WRITEMASK,mask);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
  for(unsigned i=0;i<9;++i)enabled[i]=glIsEnabled(caps[i]);
 }
 ~State(){
  for(unsigned i=0;i<9;++i)if(enabled[i])glEnable(caps[i]);else glDisable(caps[i]);
  glColorMask(mask[0],mask[1],mask[2],mask[3]);glDepthMask(depthMask);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));
  for(int i=0;i<3;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,GLuint(texture[i]));glBindSampler(i,GLuint(sampler[i]));}
  glActiveTexture(GLenum(active));glBindVertexArray(GLuint(vertexArray));glUseProgram(GLuint(program));
 }
};
GLenum sourceDepthFormat(GLuint source) {
 glBindFramebuffer(GL_READ_FRAMEBUFFER,source);
 GLint bits{},stencil{},type{};
 const GLenum attachment=source?GL_DEPTH_ATTACHMENT:GL_DEPTH;
 glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER,attachment,GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,&bits);
 glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER,attachment,GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,&stencil);
 glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER,attachment,GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE,&type);
 if(glGetError()!=GL_NO_ERROR || !bits)throw std::runtime_error("Depth of Field: source depth unavailable");
 if(stencil)return type==GL_FLOAT?GL_DEPTH32F_STENCIL8:GL_DEPTH24_STENCIL8;
 if(type==GL_FLOAT)return GL_DEPTH_COMPONENT32F;
 return bits<=16?GL_DEPTH_COMPONENT16:GL_DEPTH_COMPONENT24;
}
void prepare(int width,int height,GLenum format){
 if(!focusProgram){
  constexpr const char *focusShader=R"(#version 300 es
precision highp float;precision highp int;
uniform sampler2D depths;uniform vec2 planes;out vec4 color;
float distanceAt(vec2 uv){float d=texture(depths,uv).r;return planes.x*planes.y/(planes.y-d*(planes.y-planes.x));}
void main(){
 float z[9];int n=0;
 for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)z[n++]=distanceAt(vec2(.5)+vec2(x,y)*.006);
 for(int i=0;i<8;++i)for(int j=i+1;j<9;++j)if(z[j]<z[i]){float t=z[i];z[i]=z[j];z[j]=t;}
 // One tiny GPU focus pass, no CPU depth readback or frame stall.
 uint d=uint(clamp(round(z[4]),0.0,65535.0));float amount=1.0-smoothstep(3000.0,6000.0,z[4]);
 color=vec4(float(d>>8u)/255.0,float(d&255u)/255.0,amount,1.0);
})";
  constexpr const char *blurShader=R"(#version 300 es
precision highp float;precision highp int;
uniform sampler2D frame;uniform sampler2D depths;uniform sampler2D focus;
uniform vec2 planes;uniform float radius;out vec4 color;
float distanceAt(vec2 uv){float d=texture(depths,uv).r;return planes.x*planes.y/(planes.y-d*(planes.y-planes.x));}
void main(){
 vec2 size=vec2(textureSize(frame,0));vec2 uv=gl_FragCoord.xy/size;
 vec4 original=texelFetch(frame,ivec2(gl_FragCoord.xy),0),f=texelFetch(focus,ivec2(0),0);
 float focal=round(f.r*255.0)*256.0+round(f.g*255.0);
 float start=focal+max(800.0,focal*.25),end=focal+max(2500.0,focal);
 float amount=.6*f.b*smoothstep(start,end,distanceAt(uv));
 if(amount<=0.0){color=original;return;}
 vec3 sum=original.rgb*2.0;float weight=2.0;
 const vec2 taps[8]=vec2[8](vec2(-1,0),vec2(1,0),vec2(0,-1),vec2(0,1),vec2(-.707,-.707),vec2(.707,.707),vec2(-.707,.707),vec2(.707,-.707));
 for(int i=0;i<8;++i){vec2 p=clamp(uv+taps[i]*radius/size,vec2(.5)/size,1.0-vec2(.5)/size);
  // Do not smear the sharply focused foreground into distant edges.
  if(distanceAt(p)>=start){sum+=texture(frame,p).rgb;weight+=1.0;}}
 color=vec4(mix(original.rgb,sum/weight,amount),original.a);
})";
  focusProgram=link(focusShader);
  try{blurProgram=link(blurShader);}catch(...){glDeleteProgram(focusProgram);focusProgram=0;throw;}
  glGenVertexArrays(1,&vao);glGenTextures(1,&frame);glGenTextures(1,&depth);glGenTextures(1,&focus);
  glGenFramebuffers(1,&frameFbo);glGenFramebuffers(1,&focusFbo);
 }
 if(frameWidth==width && frameHeight==height && depthFormat==format)return;
 for(const auto t:{frame,depth,focus}){
  glBindTexture(GL_TEXTURE_2D,t);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,t==frame?GL_LINEAR:GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,t==frame?GL_LINEAR:GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  if(t==depth){
   const bool packed=format==GL_DEPTH24_STENCIL8 || format==GL_DEPTH32F_STENCIL8;
   const GLenum dataType=format==GL_DEPTH_COMPONENT16?GL_UNSIGNED_SHORT:format==GL_DEPTH_COMPONENT32F?GL_FLOAT:format==GL_DEPTH24_STENCIL8?GL_UNSIGNED_INT_24_8:format==GL_DEPTH32F_STENCIL8?GL_FLOAT_32_UNSIGNED_INT_24_8_REV:GL_UNSIGNED_INT;
   glTexImage2D(GL_TEXTURE_2D,0,format,width,height,0,packed?GL_DEPTH_STENCIL:GL_DEPTH_COMPONENT,dataType,nullptr);
  }
  else glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,t==focus?1:width,t==focus?1:height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
 }
 glBindFramebuffer(GL_DRAW_FRAMEBUFFER,frameFbo);glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,frame,0);
 glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);
 if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Depth of Field: frame capture incomplete");
 glBindFramebuffer(GL_DRAW_FRAMEBUFFER,focusFbo);glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,focus,0);
 if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Depth of Field: focus target incomplete");
 frameWidth=width;frameHeight=height;depthFormat=format;
}
}
static bool processDepthOfField(int width,int height,float nearPlane,float farPlane,bool captureOnly){
 if(!userDepthOfField || width<2 || height<2)return false;
 if(!std::isfinite(nearPlane) || !std::isfinite(farPlane) || nearPlane<=0 || farPlane<=nearPlane)throw std::runtime_error("Depth of Field: invalid projection");
 const State previous;
 try {
 for(const auto cap:State::caps)glDisable(cap);
 glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_FALSE);
 prepare(width,height,sourceDepthFormat(GLuint(previous.draw)));
 glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(previous.draw));glBindFramebuffer(GL_DRAW_FRAMEBUFFER,frameFbo);
 glBlitFramebuffer(0,0,width,height,0,0,width,height,(captureOnly?0:GL_COLOR_BUFFER_BIT)|GL_DEPTH_BUFFER_BIT,GL_NEAREST);
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Depth of Field: incompatible framebuffer copy");
 glBindVertexArray(vao);for(int i=0;i<3;++i)glBindSampler(i,0);
 glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,depth);
 if(captureOnly || !worldFocusReady){
 glBindFramebuffer(GL_DRAW_FRAMEBUFFER,focusFbo);glViewport(0,0,1,1);glUseProgram(focusProgram);
 glUniform1i(glGetUniformLocation(focusProgram,"depths"),1);glUniform2f(glGetUniformLocation(focusProgram,"planes"),nearPlane,farPlane);
 glDrawArrays(GL_TRIANGLES,0,3);
 }
 if(captureOnly){
  if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Depth of Field: world focus failed");
  worldFocusReady=true;return true;
 }
 worldFocusReady=false;
 glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(previous.draw));glViewport(0,0,width,height);glUseProgram(blurProgram);
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,frame);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,focus);
 glUniform1i(glGetUniformLocation(blurProgram,"frame"),0);glUniform1i(glGetUniformLocation(blurProgram,"depths"),1);glUniform1i(glGetUniformLocation(blurProgram,"focus"),2);
 glUniform2f(glGetUniformLocation(blurProgram,"planes"),nearPlane,farPlane);glUniform1f(glGetUniformLocation(blurProgram,"radius"),std::clamp(height/480.f*1.5f,1.f,4.f));
 glDrawArrays(GL_TRIANGLES,0,3);
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Depth of Field: postprocess failed");
 return true;
 } catch(const std::exception &error) {
  // Optional graphics must not enter the bridge's permanent simulation pause.
  userDepthOfField=false;worldFocusReady=false;while(glGetError()!=GL_NO_ERROR){}
  std::fprintf(stderr,"RE4Graphics: %s; Depth of Field disabled\n",error.what());
  return false;
 }
}
bool CaptureDepthOfFieldFocusGl(int w,int h,float nearPlane,float farPlane){worldFocusReady=false;return processDepthOfField(w,h,nearPlane,farPlane,true);}
bool ApplyDepthOfFieldGl(int w,int h,float nearPlane,float farPlane){return processDepthOfField(w,h,nearPlane,farPlane,false);}
void ForgetDepthOfFieldGl(){worldFocusReady=false;focusProgram=blurProgram=vao=frameFbo=focusFbo=frame=depth=focus=0;frameWidth=frameHeight=0;depthFormat=0;}
void DestroyDepthOfFieldGl(){glDeleteProgram(focusProgram);glDeleteProgram(blurProgram);glDeleteVertexArrays(1,&vao);glDeleteFramebuffers(1,&frameFbo);glDeleteFramebuffers(1,&focusFbo);glDeleteTextures(1,&frame);glDeleteTextures(1,&depth);glDeleteTextures(1,&focus);ForgetDepthOfFieldGl();}
}
