#include "subscreen_portrait_gl.h"
#include "actor_gl.h"
#include "room_lighting_gl.h"
#include "render_state_gl.h"
#include "generic_event_actor.h"
#include "graphics_options_gl.h"
#include <map>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace re4 {
namespace {
struct PortraitGpu {
  GLuint framebuffer{},image{},depth{};
  GLuint antialiasFbo{},antialiasColor{},antialiasDepth{};
  int width{},height{};
  GLuint readback[3]{};GLsync ready[3]{};unsigned write=0;Texture latest;
  std::vector<const void*> owners;
};
std::map<const OriginalMenuModel*,PortraitGpu> portraits;
void destroy(PortraitGpu &gpu) {
  for(const auto *owner:gpu.owners)DestroyGenericActorGl(owner);
  glDeleteFramebuffers(1,&gpu.antialiasFbo);glDeleteRenderbuffers(1,&gpu.antialiasColor);glDeleteRenderbuffers(1,&gpu.antialiasDepth);
  glDeleteFramebuffers(1,&gpu.framebuffer);glDeleteTextures(1,&gpu.image);glDeleteRenderbuffers(1,&gpu.depth);
  for(auto &fence:gpu.ready)if(fence)glDeleteSync(fence);
  glDeleteBuffers(3,gpu.readback);
  gpu={};
}
Texture animatedPortrait(const OriginalMenuModel &model,int width,int height) {
  ScopedRenderStateGl state;
  struct PackRestore {GLint old{};PackRestore(){glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&old);}~PackRestore(){glBindBuffer(GL_PIXEL_PACK_BUFFER,GLuint(old));}} packRestore;
  struct AtmosphereRestore {
    RoomAtmosphereGl old=RoomAtmosphere();
    ~AtmosphereRestore(){RoomAtmosphere()=std::move(old);RoomLightBindings().valid=false;}
  } atmosphere;
  auto &gpu=portraits[&model];
  if(gpu.width!=width || gpu.height!=height)destroy(gpu);
  glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  if(!gpu.framebuffer) {
    try {
      glGenTextures(1,&gpu.image);glBindTexture(GL_TEXTURE_2D,gpu.image);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      glGenRenderbuffers(1,&gpu.depth);glBindRenderbuffer(GL_RENDERBUFFER,gpu.depth);
      glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
      glGenFramebuffers(1,&gpu.framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,gpu.framebuffer);
      glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,gpu.image,0);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,gpu.depth);
      if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("SS-Leon: animated framebuffer");
      for(const auto &actor:model.actors){gpu.owners.push_back(actor.get());CreateGenericActorGl(actor.get(),actor->mesh(),actor->skin());}
      const int samples=SupportsSamplesGl(GL_RGBA8,4)&&SupportsSamplesGl(GL_DEPTH_COMPONENT24,4)?4:
          SupportsSamplesGl(GL_RGBA8,2)&&SupportsSamplesGl(GL_DEPTH_COMPONENT24,2)?2:0;
      if(samples) {
        glGenFramebuffers(1,&gpu.antialiasFbo);glBindFramebuffer(GL_FRAMEBUFFER,gpu.antialiasFbo);
        glGenRenderbuffers(1,&gpu.antialiasColor);glBindRenderbuffer(GL_RENDERBUFFER,gpu.antialiasColor);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER,samples,GL_RGBA8,width,height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,gpu.antialiasColor);
        glGenRenderbuffers(1,&gpu.antialiasDepth);glBindRenderbuffer(GL_RENDERBUFFER,gpu.antialiasDepth);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER,samples,GL_DEPTH_COMPONENT24,width,height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,gpu.antialiasDepth);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("SS-Leon: antialias framebuffer");
      }
      glGenBuffers(3,gpu.readback);
      for(auto buffer:gpu.readback){glBindBuffer(GL_PIXEL_PACK_BUFFER,buffer);glBufferData(GL_PIXEL_PACK_BUFFER,GLsizeiptr(width)*height*4,nullptr,GL_STREAM_READ);}
      glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
      gpu.width=width;gpu.height=height;
    }catch(...){destroy(gpu);throw;}
  }
  glBindFramebuffer(GL_FRAMEBUFFER,gpu.antialiasFbo?gpu.antialiasFbo:gpu.framebuffer);
  for(const auto capability:caps)glDisable(capability);
  glViewport(0,0,width,height);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
  glDepthRangef(0,1);glClearDepthf(1);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  RoomAtmosphere()={};RoomAtmosphere().data=model.lighting;RoomAtmosphere().eye={0,0,5000};
  RoomAtmosphere().look={0,0,-1};RoomLightBindings().valid=false;
  const float near=100,far=1000000,f=1/std::tan(10.f*.017453292519943295f),z=-(far+near)/(far-near);
  const float camera[]{f/(4.f/3),0,0,0,0,f,0,0,0,0,z,-1,0,0,-5000*z-2*far*near/(far-near),5000};
  GenericActorStyle style;style.tevScaleGroup=1;
  const std::array<float,3> position{850,-1370,0};
  for(unsigned i=0;i<model.actors.size();++i) {
    const auto &actor=model.actors[i];style.color=actor->materialColor();
    DrawGenericActorGl(actor.get(),camera,{},1,model.renderBones(i),actor->rig()->bindCenters(),model.renderPlacement(i),false,1,&position,0,&style);
  }
  if(gpu.antialiasFbo) {
    glDisable(GL_SCISSOR_TEST);glBindFramebuffer(GL_READ_FRAMEBUFFER,gpu.antialiasFbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,gpu.framebuffer);
    glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER,gpu.framebuffer);
  }
  GLint oldPack=0;glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&oldPack);
  const auto bytes=std::size_t(width)*height*4,stride=std::size_t(width)*4;
  auto flip=[&](){for(int y=0;y<height/2;++y)std::swap_ranges(gpu.latest.rgba.begin()+y*stride,gpu.latest.rgba.begin()+(y+1)*stride,gpu.latest.rgba.begin()+(height-1-y)*stride);};
  if(gpu.latest.rgba.empty()) {
    gpu.latest.width=unsigned(width);gpu.latest.height=unsigned(height);gpu.latest.rgba.resize(bytes);
    glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,gpu.latest.rgba.data());flip();
  }
  // Retire oldest to newest, without waiting for unfinished GPU work.
  for(unsigned k=0;k<3;++k){const auto i=(gpu.write+k)%3;if(!gpu.ready[i])continue;
    const auto status=glClientWaitSync(gpu.ready[i],0,0);
    if(status!=GL_ALREADY_SIGNALED && status!=GL_CONDITION_SATISFIED)continue;
    glBindBuffer(GL_PIXEL_PACK_BUFFER,gpu.readback[i]);
    if(const auto *pixels=static_cast<const unsigned char*>(glMapBufferRange(GL_PIXEL_PACK_BUFFER,0,bytes,GL_MAP_READ_BIT))) {
      std::copy(pixels,pixels+bytes,gpu.latest.rgba.begin());glUnmapBuffer(GL_PIXEL_PACK_BUFFER);flip();
    }
    glDeleteSync(gpu.ready[i]);gpu.ready[i]=nullptr;
  }
  if(!gpu.ready[gpu.write]) {
    glBindBuffer(GL_PIXEL_PACK_BUFFER,gpu.readback[gpu.write]);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    gpu.ready[gpu.write]=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);gpu.write=(gpu.write+1)%3;glFlush();
  }
  glBindBuffer(GL_PIXEL_PACK_BUFFER,GLuint(oldPack));
  if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("SS-Leon: buffered draw/read");
  return gpu.latest;
}
}
void DestroySubscreenPortraitGl(const OriginalMenuModel *model) {
  if(model){const auto it=portraits.find(model);if(it!=portraits.end()){destroy(it->second);portraits.erase(it);}return;}
  for(auto &entry:portraits)destroy(entry.second);
  portraits.clear();
}
void ForgetSubscreenPortraitGl() {
  for(auto &entry:portraits)for(const auto *owner:entry.second.owners)ForgetGenericActorGl(owner);
  portraits.clear();
}
Texture RenderSubscreenPortraitGl(const OriginalMenuModel &model,int width,int height) {
  GLint maximum{};glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
  if(width<1 || height<1 || width>maximum || height>maximum || !model.lighting)
    throw std::runtime_error("SS-Leon: invalid image size/light data");
  if(!model.actors.empty())return animatedPortrait(model,width,height);
  ScopedRenderStateGl state;
  const auto atmosphere=RoomAtmosphere();
  struct Resources {
    GLuint framebuffer{},image{},depth{};int owner{};
    RoomAtmosphereGl atmosphere;
    ~Resources() {
      DestroyGenericActorGl(&owner);
      glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(1,&image);glDeleteRenderbuffers(1,&depth);
      RoomAtmosphere()=std::move(atmosphere);RoomLightBindings().valid=false;
    }
  } resources;resources.atmosphere=atmosphere;
  glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glGenTextures(1,&resources.image);glBindTexture(GL_TEXTURE_2D,resources.image);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glGenRenderbuffers(1,&resources.depth);glBindRenderbuffer(GL_RENDERBUFFER,resources.depth);
  glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
  glGenFramebuffers(1,&resources.framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,resources.framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,resources.image,0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,resources.depth);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("SS-Leon: Framebuffer");
  for(const auto capability:caps)glDisable(capability);
  glViewport(0,0,width,height);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
  glDepthRangef(0,1);glClearDepthf(1);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  std::vector<SkinCorner> skin(model.scene.vertices.size()/3);
  for(auto &corner:skin){corner.position.count=corner.normal.count=1;corner.position.rates={1,0,0};corner.normal.rates={1,0,0};}
  CreateGenericActorGl(&resources.owner,model.scene,skin);
  RoomAtmosphere()={};RoomAtmosphere().data=model.lighting;RoomAtmosphere().eye={0,0,5000};
  RoomAtmosphere().look={0,0,-1};RoomLightBindings().valid=false;
  // sscrnCameraInit: 20-degree vertical FOV, source 4:3, eye z5000.
  // Android scales that authored layout to its full physical viewport.
  const float near=100,far=1000000,f=1/std::tan(10.f*.017453292519943295f);
  const float z=-(far+near)/(far-near);
  const float camera[]{f/(4.f/3),0,0,0,0,f,0,0,0,0,z,-1,0,0,-5000*z-2*far*near/(far-near),5000};
  const std::vector<Matrix34> identity{{1,0,0,0,0,1,0,0,0,0,1,0}};
  const std::vector<std::array<float,3>> bind{{0,0,0}};
  GenericActorStyle style;style.tevScaleGroup=1;
  std::array<float,3> position{};
  for(unsigned k=0;k<3;++k)position[k]=model.placement[k*4+3];
  DrawGenericActorGl(&resources.owner,camera,{},1,identity,bind,model.placement,false,1,&position,0,&style);
  Texture result;result.width=unsigned(width);result.height=unsigned(height);result.rgba.resize(std::size_t(width)*height*4);
  glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,result.rgba.data());
  if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("SS-Leon: Draw/read original model");
  const auto stride=std::size_t(width)*4;
  for(int y=0;y<height/2;++y)std::swap_ranges(result.rgba.begin()+y*stride,result.rgba.begin()+(y+1)*stride,
      result.rgba.begin()+(height-1-y)*stride);
  return result;
}
}
