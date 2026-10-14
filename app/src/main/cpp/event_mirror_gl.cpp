// Original r120 rear-view image. No reflected camera or invented scene pass:
// the source draws pl0010 alone with the current event camera into RTT0.
#include "event_mirror_gl.h"
#include "room_lighting_gl.h"
#include "actor_gl.h"
#include "render_state_gl.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace re4 {
namespace {
GLuint sourceFbo{},sourceImage{},sourceDepth{},copyFbo{},copyImage{};
void image(GLuint &texture,unsigned width,unsigned height) {
 glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
}
void resources() {
 if(sourceFbo)return;
 const auto layout=OriginalEventMirrorLayout();glActiveTexture(GL_TEXTURE0);
 image(sourceImage,layout.width,layout.height);image(copyImage,layout.textureWidth,layout.textureHeight);
 glGenRenderbuffers(1,&sourceDepth);glBindRenderbuffer(GL_RENDERBUFFER,sourceDepth);
 glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,layout.width,layout.height);
 glGenFramebuffers(1,&sourceFbo);glBindFramebuffer(GL_FRAMEBUFFER,sourceFbo);
 glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,sourceImage,0);
 glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,sourceDepth);
 if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("EVD mirror: Source framebuffer");
 glGenFramebuffers(1,&copyFbo);glBindFramebuffer(GL_FRAMEBUFFER,copyFbo);
 glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,copyImage,0);
 if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("EVD mirror: Copy framebuffer");
}
const GenericEventActor &find(const OriginalEvent &event,const char *name) {
 for(const auto &actor:event.actors())if(actor->name()==name)return *actor;
 throw std::runtime_error("EVD mirror: original model is missing");
}
void draw(const GenericEventActor &actor,const float *camera,const std::array<float,3> &center,float scale,
          bool normals,const GenericActorStyle &style) {
 const auto light=actor.lightRootPosition();
 auto lightingStyle=style;lightingStyle.lightVolume=actor.lightVolume();
 if(const auto *rig=actor.rig())DrawGenericActorGl(&actor,camera,center,scale,rig->bones(),rig->bindCenters(),actor.worldMatrix(),normals,
     actor.lightMask(),&light,actor.lightSize(),&lightingStyle);
 else {
  // mirror.cpp accepts the actual one-part unskinned rear-view surface.
  const std::vector<Matrix34> identity{{1,0,0,0,0,1,0,0,0,0,1,0}};
  const std::vector<std::array<float,3>> bind{{0,0,0}};
  DrawGenericActorGl(&actor,camera,center,scale,identity,bind,actor.worldMatrix(),normals,
      actor.lightMask(),&light,actor.lightSize(),&lightingStyle);
 }
}
}
EventMirrorLayout OriginalEventMirrorLayout() {return {};}
bool IsEventMirrorActive(const OriginalEvent &event) {
 if(event.finished())return false;
 const auto &name=event.data().name();const auto cut=event.sourceCut();
 return (name=="r120s00" && cut==0) || (name=="r120s01" && (cut==2 || cut==6));
}
std::array<float,16> EventMirrorCameraMatrix(const EventCamera &camera,const std::array<float,3> &center,float scale) {
 if(!camera.valid || !std::isfinite(scale) || scale<=0 || !std::isfinite(camera.roll) ||
     !std::isfinite(camera.fovRadians) || camera.fovRadians<=0 || camera.fovRadians>=3.1415927f)
  throw std::runtime_error("EVD mirror: invalid original camera");
 std::array<double,3> look{},right{},up{};double length=0;
 for(unsigned i=0;i<3;++i) {
  if(!std::isfinite(camera.eye[i]) || !std::isfinite(camera.target[i]) || !std::isfinite(center[i]))
   throw std::runtime_error("EVD mirror: Non-finite camera");
  look[i]=double(camera.target[i])-camera.eye[i];length+=look[i]*look[i];
 }
 length=std::sqrt(length);if(length<.00001)throw std::runtime_error("EVD mirror: Camera has no view length");
 for(auto &v:look)v/=length;
 right={-look[2],0,look[0]};const auto horizontal=std::hypot(right[0],right[2]);
 if(horizontal>.00001)for(auto &v:right)v/=horizontal;else right={1,0,0};
 up={right[1]*look[2]-right[2]*look[1],right[2]*look[0]-right[0]*look[2],right[0]*look[1]-right[1]*look[0]};
 const auto cosine=std::cos(camera.roll),sine=std::sin(camera.roll);
 float view[16]{},eye[3]{};
 for(unsigned i=0;i<3;++i) {
  view[i]=float(right[i]*cosine+up[i]*sine);view[4+i]=float(up[i]*cosine-right[i]*sine);view[8+i]=float(-look[i]);
  eye[i]=(camera.eye[i]-center[i])*scale;
 }
 for(unsigned row=0;row<3;++row)view[row*4+3]=-(view[row*4]*eye[0]+view[row*4+1]*eye[1]+view[row*4+2]*eye[2]);
 view[15]=1;
 const float f=float(1./std::tan(double(camera.fovRadians)*.5)),near=100*scale,far=1000000*scale;
 const float projection[16]{f/1.3333334f,0,0,0,0,f,0,0,0,0,-(far+near)/(far-near),-2*far*near/(far-near),0,0,-1,0};
 std::array<float,16> column{};
 for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)for(unsigned k=0;k<4;++k)
  column[col*4+row]+=projection[row*4+k]*view[k*4+col];
 return column;
}
void DrawEventMirrorGl(const float *mainCamera,const std::array<float,3> &center,float scale,
                       const OriginalEvent &event,bool normals) {
 if(!IsEventMirrorActive(event))return;
 if(!mainCamera)throw std::runtime_error("EVD mirror: Main camera is missing");
 const auto camera=EventMirrorCameraMatrix(event.camera(),center,scale);
 const auto &clone=find(event,"pl0010"),&mirror=find(event,"obm3010f");
 ScopedRenderStateGl previous;resources();const auto layout=OriginalEventMirrorLayout();
 glBindFramebuffer(GL_FRAMEBUFFER,sourceFbo);glViewport(0,0,layout.width,layout.height);
 for(auto cap:caps)glDisable(cap);
 glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glDepthRangef(0,1);
 glClearColor(0,0,0,0);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 glEnable(GL_SCISSOR_TEST);glScissor(layout.copyX,layout.copyY,layout.copyWidth,layout.copyHeight);
 const auto nativeViewport=RoomAtmosphere().modelTextureViewport;
 const auto nativeAspect=RoomAtmosphere().modelTextureCameraAspect;
 struct TextureViewportScope {
   std::array<float,2> previous;float aspect;
   ~TextureViewportScope(){RoomAtmosphere().modelTextureViewport=previous;RoomAtmosphere().modelTextureCameraAspect=aspect;}
 } textureViewport{nativeViewport,nativeAspect};
 RoomAtmosphere().modelTextureViewport={float(layout.width),float(layout.height)};
 RoomAtmosphere().modelTextureCameraAspect=4.f/3;
 for(const auto &entry:event.actors()) {
  const auto &part=*entry;
  if(part.orderOwner()!=&clone || !part.visible())continue;
  GenericActorStyle cloneStyle;cloneStyle.color=part.materialColor();cloneStyle.zMode=clone.depthMode();
  cloneStyle.lightVolume=part.lightVolume();
  cloneStyle.globalIlluminationImage=part.globalIlluminationTexture();
  cloneStyle.globalIlluminationScale=part.globalIlluminationScale();
  draw(part,camera.data(),center,scale,normals,cloneStyle);
 }
 RoomAtmosphere().modelTextureViewport=nativeViewport;
 RoomAtmosphere().modelTextureCameraAspect=nativeAspect;
 glDisable(GL_SCISSOR_TEST);glBindFramebuffer(GL_READ_FRAMEBUFFER,sourceFbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,copyFbo);
 glBlitFramebuffer(layout.copyX,layout.copyY,layout.copyX+layout.copyWidth,layout.copyY+layout.copyHeight,
     0,0,layout.textureWidth,layout.textureHeight,GL_COLOR_BUFFER_BIT,GL_LINEAR);
 glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(previous.read));glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(previous.draw));
 glViewport(previous.viewport[0],previous.viewport[1],previous.viewport[2],previous.viewport[3]);
 glScissor(previous.scissor[0],previous.scissor[1],previous.scissor[2],previous.scissor[3]);
 if(previous.enabled[3])glEnable(GL_SCISSOR_TEST);
 glColorMask(previous.colorMask[0],previous.colorMask[1],previous.colorMask[2],previous.colorMask[3]);
 GenericActorStyle style;style.color=mirror.materialColor();
 // Original col.a assignment truncates u8; cut6 alpha192 becomes76.
 style.color[3]=std::floor(style.color[3]*255*.4f)/255;
 style.projectiveImage=copyImage;style.projectiveMatrix=camera;style.projectiveMirror=true;
 draw(mirror,mainCamera,center,scale,normals,style);
}
void ForgetEventMirrorGl() {sourceFbo=sourceImage=sourceDepth=copyFbo=copyImage=0;}
void DestroyEventMirrorGl() {
 if(sourceFbo)glDeleteFramebuffers(1,&sourceFbo);
 if(copyFbo)glDeleteFramebuffers(1,&copyFbo);
 if(sourceImage)glDeleteTextures(1,&sourceImage);
 if(copyImage)glDeleteTextures(1,&copyImage);
 if(sourceDepth)glDeleteRenderbuffers(1,&sourceDepth);
 ForgetEventMirrorGl();
}
}
