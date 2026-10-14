#pragma once
#include <GLES3/gl3.h>
#include <algorithm>
#include <vector>
#include <atomic>
namespace re4 {
// GL-owner only. Settings survive EGL loss; object names do not.
inline int userAf=8, requestedMsaa=0;
inline bool userMipmapping=true;
inline std::atomic<bool> userRoomFps{true};
inline GLuint singleSceneFbo=0,singleSceneColor=0,singleSceneDepth=0;
inline int singleSceneWidth=0,singleSceneHeight=0;
inline GLuint msaaFbo=0,msaaColor=0,msaaDepth=0;
inline GLuint resolvedFbo=0,resolvedColor=0,resolvedDepth=0;
inline int resolvedWidth=0,resolvedHeight=0;
inline int msaaWidth=0,msaaHeight=0,msaaActual=0;
inline void ForgetGraphicsOptionsGl() {
 singleSceneFbo=singleSceneColor=singleSceneDepth=0;singleSceneWidth=singleSceneHeight=0;
 resolvedFbo=resolvedColor=resolvedDepth=0;resolvedWidth=resolvedHeight=0;
 msaaFbo=msaaColor=msaaDepth=0;msaaWidth=msaaHeight=msaaActual=0;
}
inline void DestroyMsaaGl() {
 glDeleteFramebuffers(1,&singleSceneFbo);glDeleteRenderbuffers(1,&singleSceneColor);glDeleteRenderbuffers(1,&singleSceneDepth);
 glDeleteFramebuffers(1,&resolvedFbo);glDeleteTextures(1,&resolvedColor);glDeleteRenderbuffers(1,&resolvedDepth);
 glDeleteFramebuffers(1,&msaaFbo);glDeleteRenderbuffers(1,&msaaColor);glDeleteRenderbuffers(1,&msaaDepth);
 ForgetGraphicsOptionsGl();
}
inline bool SupportsSamplesGl(GLenum format,int count) {
 GLint n=0;glGetInternalformativ(GL_RENDERBUFFER,format,GL_NUM_SAMPLE_COUNTS,1,&n);
 std::vector<GLint> samples(std::max(0,n));
 if(n)glGetInternalformativ(GL_RENDERBUFFER,format,GL_SAMPLES,n,samples.data());
 return std::find(samples.begin(),samples.end(),count)!=samples.end();
}
inline int PrepareMsaaGl(int width,int height) {
 if(!requestedMsaa && !msaaFbo)return 0;
 if(msaaWidth==width && msaaHeight==height && msaaActual==requestedMsaa)return msaaActual;
 DestroyMsaaGl();
 if(!requestedMsaa || width<1 || height<1)return 0;
 if(!SupportsSamplesGl(GL_RGBA8,requestedMsaa) || !SupportsSamplesGl(GL_DEPTH_COMPONENT24,requestedMsaa))return 0;
 GLint draw,read,rb;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_RENDERBUFFER_BINDING,&rb);
 glGenFramebuffers(1,&msaaFbo);glBindFramebuffer(GL_FRAMEBUFFER,msaaFbo);
 glGenRenderbuffers(1,&msaaColor);glBindRenderbuffer(GL_RENDERBUFFER,msaaColor);
 glRenderbufferStorageMultisample(GL_RENDERBUFFER,requestedMsaa,GL_RGBA8,width,height);
 glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,msaaColor);
 glGenRenderbuffers(1,&msaaDepth);glBindRenderbuffer(GL_RENDERBUFFER,msaaDepth);
 glRenderbufferStorageMultisample(GL_RENDERBUFFER,requestedMsaa,GL_DEPTH_COMPONENT24,width,height);
 glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,msaaDepth);
 const bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE && glGetError()==GL_NO_ERROR;
 if(ok){msaaWidth=width;msaaHeight=height;msaaActual=requestedMsaa;}else DestroyMsaaGl();
 glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glBindRenderbuffer(GL_RENDERBUFFER,GLuint(rb));
 return msaaActual;
}
// GX captures may scale/copy single-sample images. Resolve MSAA first,
// including depth for lens occlusion; never feed a multisampled CopyTexImage.
inline GLuint ReadableFramebufferGl(GLuint source,int width,int height) {
 if(source!=msaaFbo || !msaaActual)return source;
 GLint read,draw,rb,texture;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
 glGetIntegerv(GL_RENDERBUFFER_BINDING,&rb);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
 const bool scissor=glIsEnabled(GL_SCISSOR_TEST);glDisable(GL_SCISSOR_TEST);
 if(!resolvedFbo || resolvedWidth!=width || resolvedHeight!=height) {
  glDeleteFramebuffers(1,&resolvedFbo);glDeleteTextures(1,&resolvedColor);glDeleteRenderbuffers(1,&resolvedDepth);
  glGenFramebuffers(1,&resolvedFbo);glBindFramebuffer(GL_FRAMEBUFFER,resolvedFbo);
  glGenTextures(1,&resolvedColor);glBindTexture(GL_TEXTURE_2D,resolvedColor);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,resolvedColor,0);
  glGenRenderbuffers(1,&resolvedDepth);glBindRenderbuffer(GL_RENDERBUFFER,resolvedDepth);
  glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,resolvedDepth);
  resolvedWidth=width;resolvedHeight=height;
 }
 glBindFramebuffer(GL_READ_FRAMEBUFFER,source);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,resolvedFbo);
 glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT,GL_NEAREST);
 glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));
 glBindRenderbuffer(GL_RENDERBUFFER,GLuint(rb));glBindTexture(GL_TEXTURE_2D,GLuint(texture));if(scissor)glEnable(GL_SCISSOR_TEST);
 return resolvedFbo;
}
// DoF with No MSAA renders into a known D24 target. Android window depth
// cannot be portably blitted on every driver, even with an EGL depth24 config.
inline GLuint PrepareSingleSceneGl(int width,int height) {
 if(singleSceneFbo && singleSceneWidth==width && singleSceneHeight==height)return singleSceneFbo;
 GLint draw,read,rb;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_RENDERBUFFER_BINDING,&rb);
 if(!singleSceneFbo){glGenFramebuffers(1,&singleSceneFbo);glGenRenderbuffers(1,&singleSceneColor);glGenRenderbuffers(1,&singleSceneDepth);}
 glBindFramebuffer(GL_FRAMEBUFFER,singleSceneFbo);
 glBindRenderbuffer(GL_RENDERBUFFER,singleSceneColor);glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,width,height);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,singleSceneColor);
 glBindRenderbuffer(GL_RENDERBUFFER,singleSceneDepth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,singleSceneDepth);
 const bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE && glGetError()==GL_NO_ERROR;
 if(ok){singleSceneWidth=width;singleSceneHeight=height;}
 glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glBindRenderbuffer(GL_RENDERBUFFER,GLuint(rb));
 return ok?singleSceneFbo:0;
}
struct MultisampleFrameGl {
 GLint draw=0,read=0;int width,height;bool active=false;GLuint target=0;
 MultisampleFrameGl(int w,int h,bool depthEffect=false):width(w),height(h) {
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
  active=PrepareMsaaGl(w,h)>0;
  target=active?msaaFbo:depthEffect?PrepareSingleSceneGl(w,h):0;
  active=target!=0;
  if(active)glBindFramebuffer(GL_FRAMEBUFFER,target);
 }
 ~MultisampleFrameGl() {
  if(active){const bool scissor=glIsEnabled(GL_SCISSOR_TEST);glDisable(GL_SCISSOR_TEST);
   glBindFramebuffer(GL_READ_FRAMEBUFFER,target);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));
   glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
   if(scissor)glEnable(GL_SCISSOR_TEST);
  }
  glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));
 }
};
}
