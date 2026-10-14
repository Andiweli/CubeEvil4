// Real EVD/BIN/FCV/TPL/font data through the production GLES event renderer.
// Supplied Disc assets and framebuffer exports are never bundled with the app.
#include "event_actor_gl.h"
#include "actor_gl.h"
#include "room_lighting_gl.h"
#include "world_effects_gl.h"
#include "environment_gl.h"
#include "source_glow_gl.h"
#include "room_area_effects.h"
#include "archive.h"
#include "ports/yz2.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks{},uploads{},meshUpdates{};
void require(bool okay,const char *message) {++checks;if(!okay)throw std::runtime_error(message);}
std::vector<unsigned char> pixels() {
  std::vector<unsigned char> result(960*540*4);glReadPixels(0,0,960,540,GL_RGBA,GL_UNSIGNED_BYTE,result.data());
  require(glGetError()==GL_NO_ERROR,"Event framebuffer read");return result;
}
unsigned changes(const std::vector<unsigned char> &a,const std::vector<unsigned char> &b) {
  unsigned result{};for(unsigned i=0;i<a.size();i+=4)result+=a[i]!=b[i] || a[i+1]!=b[i+1] || a[i+2]!=b[i+2];return result;
}
void save(const std::string &path,const std::vector<unsigned char> &p) {
  std::ofstream out(path,std::ios::binary);out<<"P6\n960 540\n255\n";
  for(int y=539;y>=0;--y)for(int x=0;x<960;++x)out.write(reinterpret_cast<const char*>(p.data()+(y*960+x)*4),3);
  require(bool(out),"Event framebuffer export");
}
std::array<float,16> cameraMatrix(const re4::EventCamera &source) {
  const auto camera=re4::EventCameraPose(source);constexpr float scale=.0001f;
  float view[16]{};view[15]=1;
  for(unsigned k=0;k<3;++k) {view[k]=float(camera.right[k]);view[4+k]=float(camera.up[k]);view[8+k]=float(-camera.look[k]);}
  for(unsigned row=0;row<3;++row)for(unsigned k=0;k<3;++k)view[row*4+3]-=view[row*4+k]*float(camera.eye[k])*scale;
  const float f=1/std::tan(source.fovRadians*.5f),near=scale*100,far=scale*1000000;
  const float projection[16]{f/(960.f/540),0,0,0,0,f,0,0,0,0,-(far+near)/(far-near),-2*far*near/(far-near),0,0,-1,0};
  std::array<float,16> column{};
  for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)for(unsigned k=0;k<4;++k)column[j*4+i]+=projection[i*4+k]*view[k*4+j];
  return column;
}
std::vector<unsigned char> draw(const re4::OriginalEvent &event,bool normals=false,bool captions=false) {
  glViewport(0,0,960,540);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
  glClearColor(.035f,.055f,.075f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  auto &atmosphere=re4::RoomAtmosphere();atmosphere.data=event.lighting();const auto pose=re4::EventCameraPose(event.camera());
  for(unsigned k=0;k<3;++k) {atmosphere.eye[k]=float(pose.eye[k]);atmosphere.right[k]=float(pose.right[k]);atmosphere.up[k]=float(pose.up[k]);atmosphere.look[k]=float(pose.look[k]);}
  atmosphere.playerParts.clear();atmosphere.transientLights.clear();re4::RoomLightBindings().valid=false;
  const auto matrix=cameraMatrix(event.camera());re4::DrawEventActorsGl(matrix.data(),{},.0001f,event,normals);
  if(captions)re4::DrawEventSubtitlesGl(960,540,event);
  require(glGetError()==GL_NO_ERROR,"Original event GLES draw");return pixels();
}
struct Egl {
  EGLDisplay display{};EGLConfig config{};EGLContext context{};EGLSurface surface{};
  Egl() {
    setenv("EGL_PLATFORM","surfaceless",1);display=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint major{},minor{};
    require(eglInitialize(display,&major,&minor),"Event EGL initialize");require(eglBindAPI(EGL_OPENGL_ES_API),"Event EGL API");
    const EGLint attrs[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};EGLint count{};
    require(eglChooseConfig(display,attrs,&config,1,&count) && count,"Event EGL config");create();
  }
  void create() {
    const EGLint ctx[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE},pbuffer[]{EGL_WIDTH,960,EGL_HEIGHT,540,EGL_NONE};
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,ctx);surface=eglCreatePbufferSurface(display,config,pbuffer);
    require(eglMakeCurrent(display,surface,surface,context),"Event EGL current");
  }
  void lose() {eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroySurface(display,surface);eglDestroyContext(display,context);context=EGL_NO_CONTEXT;surface=EGL_NO_SURFACE;}
  ~Egl() {lose();eglTerminate(display);}
};
void captionsState(const re4::OriginalEvent &event) {
  require(!event.subtitle().glyphs.empty(),"Japanese original MDT caption exists");
  GLuint vao{},buffer{},texture{};glGenVertexArrays(1,&vao);glGenBuffers(1,&buffer);glGenTextures(1,&texture);
  glUseProgram(0);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glActiveTexture(GL_TEXTURE3);
  glViewport(3,7,951,527);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_CULL_FACE);
  glColorMask(GL_TRUE,GL_FALSE,GL_TRUE,GL_FALSE);glBlendEquationSeparate(GL_FUNC_SUBTRACT,GL_FUNC_REVERSE_SUBTRACT);
  glBlendFuncSeparate(GL_ONE,GL_ZERO,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA);
  re4::DrawEventSubtitlesGl(960,540,event);
  GLint current{},viewport[4]{};GLboolean mask[4]{};
  glGetIntegerv(GL_CURRENT_PROGRAM,&current);require(current==0,"Caption restores caller program even on first font upload");
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&current);require(GLuint(current)==vao,"Caption restores caller VAO");
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&current);require(GLuint(current)==buffer,"Caption restores caller VBO");
  glGetIntegerv(GL_ACTIVE_TEXTURE,&current);require(current==GL_TEXTURE3,"Caption restores active texture");
  glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&current);require(GLuint(current)==texture,"Caption restores texture0");
  glGetIntegerv(GL_VIEWPORT,viewport);require(viewport[0]==3 && viewport[1]==7 && viewport[2]==951 && viewport[3]==527,"Caption restores caller viewport");
  glGetBooleanv(GL_COLOR_WRITEMASK,mask);require(mask[0] && !mask[1] && mask[2] && !mask[3],"Caption restores color mask");
  require(glIsEnabled(GL_DEPTH_TEST) && !glIsEnabled(GL_BLEND) && glIsEnabled(GL_CULL_FACE),"Caption restores render enable flags");
  glGetIntegerv(GL_BLEND_SRC_ALPHA,&current);require(current==GL_DST_ALPHA,"Caption restores distinct alpha blend");
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&current);require(current==GL_FUNC_REVERSE_SUBTRACT,"Caption restores distinct alpha equation");
  glDeleteTextures(1,&texture);glDeleteBuffers(1,&buffer);glDeleteVertexArrays(1,&vao);glBindVertexArray(0);glDisable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glViewport(0,0,960,540);glActiveTexture(GL_TEXTURE0);
}
void fadeState() {
  // Cover every edge despite a caller's small viewport/scissor and partial
  // channel mask. A source alpha128 halves the actual rendered room color.
  glUseProgram(0);glBindVertexArray(0);
  glViewport(0,0,960,540);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(1,.5f,.25f,1);glClear(GL_COLOR_BUFFER_BIT);
  glViewport(3,7,951,527);glScissor(100,100,2,2);glEnable(GL_SCISSOR_TEST);glEnable(GL_STENCIL_TEST);
  glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glEnable(GL_CULL_FACE);glDisable(GL_BLEND);
  glColorMask(GL_TRUE,GL_FALSE,GL_TRUE,GL_FALSE);
  re4::DrawEventFadeGl(960,540,128);const auto half=pixels();
  for(const auto p:{0u,959u,960u*539u,960u*540u-1u,960u*270u+480u}) {
    require(std::abs(int(half[p*4])-127)<=1 && std::abs(int(half[p*4+1])-64)<=1 && std::abs(int(half[p*4+2])-32)<=1,
        "Source fade alpha128 covers complete framebuffer");
  }
  GLint viewport[4]{};GLboolean mask[4]{};glGetIntegerv(GL_VIEWPORT,viewport);glGetBooleanv(GL_COLOR_WRITEMASK,mask);
  require(viewport[0]==3 && viewport[1]==7 && viewport[2]==951 && viewport[3]==527,"Fade restores caller viewport");
  require(mask[0] && !mask[1] && mask[2] && !mask[3],"Fade restores channel mask");
  require(glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_CULL_FACE) && glIsEnabled(GL_SCISSOR_TEST) && glIsEnabled(GL_STENCIL_TEST) && !glIsEnabled(GL_BLEND),
      "Fade restores depth/cull/scissor/stencil/blend enables");
  re4::DrawEventFadeGl(960,540,0);require(half==pixels(),"Fade alpha0 leaves framebuffer unchanged");
  re4::DrawEventFadeGl(960,540,255);const auto black=pixels();
  for(unsigned p=0;p<black.size();p+=4)require(!black[p]&&!black[p+1]&&!black[p+2],"Source fade alpha255 covers complete screen");
  glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  glViewport(0,0,960,540);
}
double captureSkin(const re4::GenericEventActor &actor,const re4::EventCamera &camera) {
  const auto *rig=actor.rig();require(rig!=nullptr,"Original source body has a motion skeleton");
  const auto matrix=cameraMatrix(camera);const auto placement=actor.worldMatrix();
  const auto light=actor.lightPosition();
  re4::DrawGenericActorGl(&actor,matrix.data(),{},.0001f,rig->bones(),rig->bindCenters(),placement,false,
      actor.lightMask(),&light,actor.lightRadius());
  const auto count=actor.skin().size();GLuint buffer{};glGenBuffers(1,&buffer);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,count*6*sizeof(float),nullptr,GL_STREAM_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
  glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_TRIANGLES);glDrawArrays(GL_TRIANGLES,0,GLsizei(count));glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);
  require(glGetError()==GL_NO_ERROR,"Original event GPU skin capture");
  auto output=static_cast<const float*>(glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,0,count*6*sizeof(float),GL_MAP_READ_BIT));
  require(output!=nullptr,"Original event skin capture mapping");double maximum{};
  const auto &mesh=actor.mesh();const auto &bones=rig->bones();const auto &bind=rig->bindCenters();
  for(unsigned i=0;i<count;++i) {
    std::array<double,3> position{},normal{};const auto &skin=actor.skin()[i];
    for(unsigned weight=0;weight<3;++weight)for(unsigned row=0;row<3;++row) {
      const auto p=skin.position.joints[weight],n=skin.normal.joints[weight];double posed=bones[p][row*4+3];
      for(unsigned col=0;col<3;++col) {posed+=double(bones[p][row*4+col])*(mesh.vertices[i*3+col]-bind[p][col]);normal[row]+=double(skin.normal.rates[weight])*bones[n][row*4+col]*mesh.normals[i*3+col];}
      position[row]+=double(skin.position.rates[weight])*posed;
    }
    for(unsigned row=0;row<3;++row) {
      double transformed{};for(unsigned col=0;col<3;++col)transformed+=placement[row*4+col]*normal[col];
      require(std::isfinite(output[i*6+row]) && std::isfinite(output[i*6+3+row]),"Finite original event GPU skin");
      maximum=std::max(maximum,std::abs(output[i*6+row]-position[row]));maximum=std::max(maximum,std::abs(output[i*6+3+row]-transformed));
    }
  }
  require(glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER)==GL_TRUE,"Original event skin buffer intact");glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glDeleteBuffers(1,&buffer);
  require(maximum<.03,"Real EVD skinned/morphed corners match independent CPU evaluation");return maximum;
}
void eventOwnershipCheck(const re4::OriginalEvent &event) {
  const re4::GenericEventActor *actor{};
  for(const auto &entry:event.actors())if(entry->visible() && entry->rig()){actor=entry.get();break;}
  require(actor!=nullptr,"Actual event has a model for independent persistent-owner fixture");
  // A persistent room actor owns a separate GPU entry, even when it shares
  // immutable source BIN/TPL geometry with a cutscene actor.
  unsigned persistentOwner{};re4::CreateGenericActorGl(&persistentOwner,actor->mesh(),actor->skin());
  const auto camera=cameraMatrix(event.camera());const auto light=actor->lightPosition();const auto *rig=actor->rig();
  const auto drawPersistent=[&] {
    glViewport(0,0,960,540);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
    glClearColor(.035f,.055f,.075f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    re4::DrawGenericActorGl(&persistentOwner,camera.data(),{},.0001f,rig->bones(),rig->bindCenters(),
        actor->worldMatrix(),false,actor->lightMask(),&light,actor->lightRadius());return pixels();
  };
  const auto before=drawPersistent();glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  require(changes(before,pixels())>100,"Independent persistent owner draws actual source geometry");
  re4::DestroyEventActorsGl();
  require(before==drawPersistent(),"Cutscene teardown preserves independent room/radio/forest actor GPU ownership");
  re4::DestroyGenericActorGl(&persistentOwner);
}
void eventCheck(const std::string &files,const std::string &name,const std::string &output,Egl &egl) {
  re4::OriginalEvent event(files,name,0); // Actual Japanese MDT/event font; English suppresses captions in source MesSet.
  for(const auto &actor:event.actors()) {
    if(actor->name()=="pl0000")require(actor->lightMask()==1,"Source event Leon light mask1");
    if(actor->name()=="em1000")require(actor->lightMask()==2,"Source event Ganado light mask2");
    if(actor->name()=="obm2d00")require(actor->lightMask()==4,"Source event truck light mask4");
    require(actor->lightRadius()>0,"Original BIN light bound has positive size");
  }
  uploads=meshUpdates=0;re4::SyncEventActorsGl(event);require(uploads>0,"Real actor textures uploaded");
  uploads=meshUpdates=0;re4::SyncEventActorsGl(event);require(!uploads && !meshUpdates,"Unchanged event actors upload nothing");
  unsigned renderedCuts{},visibleCuts{},morphUploads{},captionFrames{},effectSamples{},visibleEffects{},visibleHandgunCuts{};bool captionChecked=false;double maximumSkinError{};
  while(!event.finished()) {
    uploads=meshUpdates=0;re4::SyncEventActorsGl(event);require(!uploads,"Advancing original EVD does not reload model textures");morphUploads+=meshUpdates>0;
    if(event.sourceFrame()%10==0 && !event.effectsState().sprites.empty()) {
      const auto pose=re4::EventCameraPose(event.camera());re4::WorldEffectCamera camera;
      for(unsigned k=0;k<3;++k) {camera.eye[k]=float(pose.eye[k]);camera.right[k]=float(pose.right[k]);camera.up[k]=float(pose.up[k]);camera.forward[k]=float(pose.look[k]);}
      const auto quads=re4::WorldEffectQuads(event.effectsData(),event.effectsState(),camera);
      if(!quads.empty()) {
        glViewport(0,0,960,540);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glClearColor(.035f,.055f,.075f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);const auto empty=pixels();const auto matrix=cameraMatrix(event.camera());
        re4::DrawEventEffectsGl(matrix.data(),{},.0001f,quads);const auto rendered=pixels();const auto changed=changes(empty,rendered);
        ++effectSamples;if(changed>10) {++visibleEffects;if(visibleEffects==1)save(output+"-"+name+"-est.ppm",rendered);}
      }
    }
    if(event.cutFrame()==6) {
      const auto sourceFrame=event.sourceFrame();auto material=draw(event);const auto normal=draw(event,true);
      if(name=="r100s03") {
        for(const auto &actor:event.actors())if(actor->name()=="wep0200") {
          const bool visible=event.sourceCut()<=4 || event.sourceCut()>=13;
          require(actor->visible()==visible,"Original first-house callback handgun display table");
          if(visible) {
            actor->setDisplayOverride(false);const auto withoutHandgun=draw(event);
            actor->setDisplayOverride(true);visibleHandgunCuts+=changes(material,withoutHandgun)>3;
            require(material==draw(event),"Restoring source handgun display restores exact event pixels");
          }
        }
      }
      glDepthMask(GL_TRUE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);const auto empty=pixels();
      visibleCuts+=changes(material,empty)>100;++renderedCuts;
      require(material==draw(event),"Normal preview restores exact event material framebuffer");
      if(changes(material,empty)>100)require(changes(material,normal)>100,"Original event normal vectors visible");
      event.tick30Hz(false);require(event.sourceFrame()==sourceFrame && material==draw(event),"Pause preserves actual EVD camera/pose pixels");
      for(const auto &actor:event.actors())if(actor->visible() && actor->rig()) {
        maximumSkinError=std::max(maximumSkinError,captureSkin(*actor,event.camera()));break;
      }
      save(output+"-"+name+"-cut"+std::to_string(event.sourceCut())+".ppm",material);
    }
    if(!event.subtitle().glyphs.empty() && !captionChecked) {
      re4::DestroyEventActorsGl();re4::SyncEventActorsGl(event);captionsState(event); // First font upload must restore state too.
      const auto without=draw(event),with=draw(event,false,true);captionFrames=changes(without,with);
      require(captionFrames>100,"Real original MDT/font produce caption pixels");
      save(output+"-"+name+"-caption.ppm",with);captionChecked=true;
      const auto frame=event.sourceFrame();re4::ForgetEventActorsGl();egl.lose();egl.create();
      require(event.sourceFrame()==frame && with==draw(event,false,true),"Lost EGL context preserves source EVD state and exact actors/font pixels");
    }
    event.tick30Hz();
  }
  require(renderedCuts==event.data().cutFrames().size(),"Every actual authored camera cut rendered");
  require(visibleCuts>=(name=="r100s03"?10u:3u),"Actual event actors are visible across source cuts");
  if(name=="r100s03")require(visibleHandgunCuts>0,"Corrected source handgun produces visible GLES pixels across authored cameras");
  require(morphUploads>0,"Real facial FCV updates original GPU mesh");
  require(effectSamples>0 && visibleEffects>0,"Real cut/frame EST effects produce visible GLES pixels");
  if(name=="r100s03")require(captionChecked,"Original s03 Japanese subtitle timeline rendered");
  event.reset();const auto baseline=draw(event);re4::DestroyEventActorsGl();require(baseline==draw(event),"Destroy/recreate preserves original event framebuffer");
  eventOwnershipCheck(event);
  re4::DestroyEventActorsGl();re4::RoomAtmosphere().data.reset();
  std::cout<<name<<" GLES PASS: "<<renderedCuts<<" original cuts, "<<visibleCuts<<" visible cuts, "<<morphUploads<<" morph upload frames, "<<captionFrames<<" caption pixels, "<<visibleEffects<<'/'<<effectSamples<<" EST samples visible, max GPU skin difference "<<maximumSkinError<<" source units\n";
}
void effectPassCheck() {
  // Esp18 source TEV uses EFB RGB and only the ANM's alpha. Changing every
  // ANM RGB byte must preserve all pixels, even on a room without water.
  re4::WorldEffectsData data;data.sprites.animations.resize(1);
  re4::Texture mask;mask.width=mask.height=4;mask.rgba.resize(64);
  for(unsigned p=0;p<64;p+=4){mask.rgba[p]=255;mask.rgba[p+1]=17;mask.rgba[p+2]=9;mask.rgba[p+3]=128;}
  data.sprites.animations[0].images.push_back(mask);
  for(unsigned p=0;p<64;p+=4){mask.rgba[p]=0;mask.rgba[p+1]=240;mask.rgba[p+2]=250;}
  data.sprites.animations[0].images.push_back(mask);
  re4::WorldEffectQuad q;q.screen=true;q.type=24;q.sourceAlpha=255;q.color={.5f,.75f,1,1};
  q.position={{{0,0,1},{512,0,1},{512,448,1},{0,448,1}}};
  q.uv={{{0,0},{1,0},{1,1},{0,1}}};
  constexpr float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  re4::DestroyEventActorsGl();re4::CreateEventEffectsGl(data);
  GLuint textures[2]{},samplers[2]{},vao{},buffer{};
  glGenTextures(2,textures);glGenSamplers(2,samplers);glGenVertexArrays(1,&vao);glGenBuffers(1,&buffer);
  const auto drawLayer=[&](unsigned image) {
    glUseProgram(0);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);
    glViewport(0,0,960,540);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
    glClearColor(.8f,.6f,.4f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[0]);glBindSampler(0,samplers[0]);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,textures[1]);glBindSampler(2,samplers[1]);
    glActiveTexture(GL_TEXTURE3);glEnable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glDisable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_SUBTRACT,GL_FUNC_REVERSE_SUBTRACT);
    glBlendFuncSeparate(GL_ONE,GL_ZERO,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA);
    q.image=image;const auto beforeUploads=uploads;
    re4::DrawEventEffectsGl(identity,{},1,{q});
    // A lazy copy target uploads only once, never per layer or repeat draw.
    if(image)require(uploads==beforeUploads,"Esp18 reuses its cached copy textures");
    GLint state{};glGetIntegerv(GL_CURRENT_PROGRAM,&state);require(state==0,"Esp18 restores caller program");
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&state);require(GLuint(state)==vao,"Esp18 restores caller VAO");
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&state);require(GLuint(state)==buffer,"Esp18 restores caller VBO");
    glGetIntegerv(GL_ACTIVE_TEXTURE,&state);require(state==GL_TEXTURE3,"Esp18 restores active texture");
    for(unsigned k=0;k<2;++k){glActiveTexture(k?GL_TEXTURE2:GL_TEXTURE0);
      glGetIntegerv(GL_TEXTURE_BINDING_2D,&state);require(GLuint(state)==textures[k],"Esp18 restores copy/mask texture bindings");
      glGetIntegerv(GL_SAMPLER_BINDING,&state);require(GLuint(state)==samplers[k],"Esp18 restores copy/mask sampler bindings");}
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&state);require(state==GL_FUNC_REVERSE_SUBTRACT,"Esp18 restores alpha blend equation");
    require(glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_CULL_FACE) && !glIsEnabled(GL_BLEND),"Esp18 restores caller enables");
    return pixels();
  };
  const auto a=drawLayer(0),b=drawLayer(1);require(a==b,"Esp18 uses ANM alpha only, never placeholder ANM RGB");
  const unsigned pixel=(270*960+480)*4;
  double expected[3]{204,153,102},copy[3]{102,153*.75,102};
  for(unsigned layer=0;layer<8;++layer){const auto alpha=std::floor(255.f/float(layer+2))/255*128/255;
    for(unsigned k=0;k<3;++k)expected[k]=copy[k]*alpha+expected[k]*(1-alpha);}
  for(unsigned k=0;k<3;++k)require(std::abs(double(a[pixel+k])-expected[k])<4,"Esp18 eight immutable EFB feedback layers match source alpha envelope");
  // Source OT flag0x400000 routes the foliage cutout before the world; a
  // default ordinary-effects call must leave those authored cards untouched.
  q.type=0;q.sourceAlpha=0;q.color={1,1,1,1};q.flags=0x700000;q.image=0;
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glClearColor(.8f,.6f,.4f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  const auto original=pixels();re4::DrawEventEffectsGl(identity,{},1,{q});require(original==pixels(),"Source before-world quad excluded from after-world pass");
  re4::DrawEventEffectsGl(identity,{},1,{q},true);require(original==pixels(),"Source alpha cutoff128 rejects alpha128 exactly");
  q.flags=0x600000;re4::DrawEventEffectsGl(identity,{},1,{q},true);
  require(changes(original,pixels())>100000,"Source before-world orthographic sprites render at physical screen extent");
  glUseProgram(0);glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);glActiveTexture(GL_TEXTURE0);
  glBindSampler(0,0);glBindSampler(2,0);glDeleteSamplers(2,samplers);glDeleteTextures(2,textures);
  glDeleteVertexArrays(1,&vao);glDeleteBuffers(1,&buffer);re4::DestroyEventEffectsGl();re4::DestroyEnvironmentGl();
  std::cout<<"Esp18 immutable capture/8 layers/ANM-alpha/state and source before-world flags PASS\n";
}
void trapPassCheck() {
  re4::WorldEffectsData data;data.sprites.signature=0x12345678;data.sprites.animations.resize(1);
  re4::Texture white;white.width=white.height=4;white.rgba.assign(64,255);data.sprites.animations[0].images.push_back(white);
  re4::CreateTrapEffectsGl(data);constexpr float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  glViewport(0,0,960,540);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  glDepthMask(GL_TRUE);glClearDepthf(1);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  const auto black=pixels();re4::WorldEffectLine line;line.sourceSignature=data.sprites.signature;line.serial=1;line.width=24;
  line.position={{{-.5f,0,0},{.5f,0,0},{.8f,.2f,0}}};line.color={{{1,0,0,1},{1,0,0,.5f},{1,0,0,0}}};
  re4::DrawTrapEffectLinesGl(identity,{},1,{line});const auto trail=pixels();
  require(changes(black,trail)>500 && changes(black,trail)<10000,"Esp09 original four-pixel fading history remains a narrow line strip");
  require(trail[(270*960+300)*4]>trail[(270*960+600)*4],"Esp09 per-vertex source alpha fades toward history tail");
  const float near=100,far=1000000,f=1/std::tan(.6f);
  const float projection[]{f/(960.f/540),0,0,0,0,f,0,0,0,0,-(far+near)/(far-near),-1,0,0,-2*far*near/(far-near),0};
  re4::WorldEffectCamera camera;re4::WorldEffectLens lens;lens.sourceSignature=data.sprites.signature;lens.serial=2;
  lens.position={0,0,-5000};lens.size={64,64};lens.color={1,1,1,1};lens.sourceAlpha=255;lens.centerRatio=1;lens.hideRadius=10;
  std::vector<unsigned char> visible;
  for(unsigned age=0;age<4;++age) {
    lens.sourceAge=age;glDepthMask(GL_TRUE);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    re4::DrawTrapEffectLensesGl(projection,{},1,camera,{lens});const auto rendered=pixels();
    const unsigned pixel=(270*960+480)*4;
    if(age<2)require(rendered[pixel]==0,"Esp0e first two rendered source states wait for previous-position Z feedback");
    if(age==2)require(std::abs(int(rendered[pixel])-153)<3,"Esp0e source visibility interpolation0.6 produces alpha153");
    if(age==3){visible=rendered;require(std::abs(int(rendered[pixel])-214)<3,"Esp0e next source visibility0.84 produces alpha214");
      glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);re4::DrawTrapEffectLensesGl(projection,{},1,camera,{lens});
      require(visible==pixels(),"Esp0e repeated render of same30Hz source age preserves alpha pixels");
      const auto presentation=re4::SaveTrapEffectsPresentationGl();
      re4::DestroyTrapEffectsGl();re4::CreateTrapEffectsGl(data);re4::RestoreTrapEffectsPresentationGl(presentation);
      glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);re4::DrawTrapEffectLensesGl(projection,{},1,camera,{lens});
      require(visible==pixels(),"Scene source presentation snapshot preserves lens feedback through GPU destroy/recreate");}
  }
  lens.sourceAge=4;glDepthMask(GL_TRUE);glClearDepthf(.1f);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  re4::DrawTrapEffectLensesGl(projection,{},1,camera,{lens});
  lens.sourceAge=5;glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  re4::DrawTrapEffectLensesGl(projection,{},1,camera,{lens});
  require(changes(black,pixels())==0,"Esp0e all12 source Z-ring samples hide an occluded flare");
  re4::DestroyTrapEffectsGl();glClearDepthf(1);
  std::cout<<"Esp09 source line-width/vertex-alpha and Esp0e12-ring Z-feedback/visibility smoothing PASS\n";
}
void glowMaskCheck() {
  glViewport(0,0,960,540);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_BLEND);glUseProgram(0);
  re4::WorldEffectGlowRequest first;first.level=-2;first.priority=255;first.color={255,128,64};first.special=1;
  auto ignored=first;ignored.priority=0;ignored.color={0,255,0};ignored.special=0;
  re4::BeginSourceGlowFrameGl({first,ignored});
  require(re4::SourceEffectWritesAlphaGl(0x800000) && !re4::SourceEffectWritesAlphaGl(0x8000),"Source special glow selects only AlphaDraw2 sprites");
  glClearColor(0,0,0,4.f/255);glClear(GL_COLOR_BUFFER_BIT);re4::DrawSourceGlowGl(960,540);const auto locked=pixels();
  const unsigned pixel=(270*960+480)*4;
  for(unsigned k=0;k<3;++k)require(std::abs(int(locked[pixel+k])-int(std::array<unsigned,3>{32,16,8}[k]))<=2,
      "Filter03 locked priority preserves original half/quarter scale2 and additive source tint");
  re4::BeginSourceGlowFrameGl({});require(re4::SourceEffectWritesAlphaGl(0x800000),"Source AlphaDraw2 status persists until non-special Filter03 render");
  first.special=0;first.level=2;re4::BeginSourceGlowFrameGl({first});
  glClearColor(0,0,0,4.f/255);glClear(GL_COLOR_BUFFER_BIT);re4::DrawSourceGlowGl(960,540);
  require(changes(locked,pixels())==0,"Filter03 feedback preserves a constant alpha field through source loop/offset copies");
  require(re4::SourceEffectWritesAlphaGl(0x8000) && !re4::SourceEffectWritesAlphaGl(0x800000),"Source non-special glow restores ordinary alpha-write group");
  re4::DestroySourceGlowGl();
  re4::WorldEffectsData data;data.sprites.animations.resize(1);
  re4::Texture white;white.width=white.height=4;white.rgba.assign(64,255);
  data.sprites.animations[0].images.push_back(white);
  auto mask=white;for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x){const auto p=(y*4+x)*4;
    mask.rgba[p]=13;mask.rgba[p+1]=79;mask.rgba[p+2]=201;mask.rgba[p+3]=x<2?255:0;}
  data.sprites.animations[0].images.push_back(mask);re4::CreateRoomAreaEffectsGl(data);
  constexpr float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  re4::RoomAreaEffectQuad wrapped;auto &q=wrapped.quad;q.screen=true;q.type=8;q.color={1,1,1,1};
  q.position={{{0,0,1},{512,0,1},{512,448,1},{0,448,1}}};q.uv={{{0,0},{1,0},{1,1},{0,1}}};
  q.tiled=true;q.repeats={2,3};q.scroll={.17f,.33f};q.flipS=q.flipT=true;
  wrapped.maskAnimation=0;wrapped.maskImage=1;wrapped.maskStretched=true;
  glClearColor(0,0,0,77.f/255);glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  re4::DrawRoomAreaEffectsGl(identity,{},1,{wrapped});const auto stretched=pixels();
  const unsigned left=(270*960+120)*4,right=(270*960+840)*4;
  require(stretched[left]>250 && stretched[left+1]>250 && stretched[right]<2,"Room mask stretches once independently of primary repeats/scroll/flips and preserves RGB");
  require(stretched[left+3]==77 && stretched[right+3]==77,"Unflagged source room sprites preserve EFB alpha");
  q.flags=0x208000;glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);re4::DrawRoomAreaEffectsGl(identity,{},1,{wrapped});const auto dstZero=pixels();
  require(dstZero[left+3]==0 && dstZero[right+3]==77,"Source Tool200000 destination-alpha override writes zero only on surviving mask pixels");
  wrapped.maskStretched=false;q.flags=0;glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);re4::DrawRoomAreaEffectsGl(identity,{},1,{wrapped});
  require(changes(stretched,pixels())>100000,"Source non-stretched mask follows primary texture coordinates");
  re4::DestroyRoomAreaEffectsGl();glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  std::cout<<"Filter03 alpha feedback/priority/status and room independent mask/alpha/depth flags PASS\n";
}
void trapDepthFormatsCheck() {
  GLint previousDraw{},previousRead{},previousRenderbuffer{};
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&previousDraw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&previousRead);
  glGetIntegerv(GL_RENDERBUFFER_BINDING,&previousRenderbuffer);
  GLuint framebuffer{},color{},depth{};
  glGenFramebuffers(1,&framebuffer);glGenRenderbuffers(1,&color);glGenRenderbuffers(1,&depth);
  glBindRenderbuffer(GL_RENDERBUFFER,color);glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,960,540);
  for(const GLenum format:{GL_DEPTH_COMPONENT16,GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT32F,GL_DEPTH24_STENCIL8,GL_DEPTH32F_STENCIL8}) {
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,color);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_STENCIL_ATTACHMENT,GL_RENDERBUFFER,0);
    glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,format,960,540);
    const bool packed=format==GL_DEPTH24_STENCIL8 || format==GL_DEPTH32F_STENCIL8;
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,packed?GL_DEPTH_STENCIL_ATTACHMENT:GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Trap source depth profile complete");
    trapPassCheck();
    GLint draw{},read{};glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
    require(draw==GLint(framebuffer) && read==GLint(framebuffer),"Esp0e restores both packed source framebuffer bindings");
    require(glGetError()==GL_NO_ERROR,"Esp0e supports D16/D24/D32F/D24S8/D32FS8 without halting the render loop");
  }
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(previousDraw));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(previousRead));
  glBindRenderbuffer(GL_RENDERBUFFER,GLuint(previousRenderbuffer));glDeleteFramebuffers(1,&framebuffer);
  glDeleteRenderbuffers(1,&color);glDeleteRenderbuffers(1,&depth);
}
void shimmerCheck() {
  re4::WorldEffectsData data;data.sprites.animations.resize(1);
  re4::Texture primary;primary.width=primary.height=4;primary.rgba.resize(64);
  for(unsigned image=0;image<3;++image) {
    for(unsigned p=0;p<64;p+=4){primary.rgba[p]=image?0:255;primary.rgba[p+1]=image?231:19;
      primary.rgba[p+2]=0;primary.rgba[p+3]=image==2?0:255;}
    data.sprites.animations[0].images.push_back(primary);
  }
  re4::CreateRoomAreaEffectsGl(data);constexpr float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  re4::RoomAreaEffectQuad wrapped;auto &q=wrapped.quad;q.screen=true;q.type=8;q.sourceAlpha=255;q.color={1,1,1,1};
  q.position={{{0,0,1},{512,0,1},{512,448,1},{0,448,1}}};q.uv={{{0,0},{1,0},{1,1},{0,1}}};
  wrapped.shimmerType=1;wrapped.shimmerPower=255;
  const auto draw=[&](unsigned image) {
    glViewport(0,0,960,540);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
    glClearDepthf(1);glDisable(GL_STENCIL_TEST);glEnable(GL_SCISSOR_TEST);
    for(unsigned stripe=0;stripe<10;++stripe){glScissor(GLint(stripe*96),0,96,540);glClearColor(float(stripe)/9,.2f,.4f,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
    glDisable(GL_SCISSOR_TEST);q.image=image;re4::DrawRoomAreaEffectsGl(identity,{},1,{wrapped});return pixels();
  };
  const auto red=draw(0),green=draw(1),zero=draw(2);
  require(red==green,"Source Esp08 unsigned indirect warp samples A/B only, independently of primary R/G");
  const unsigned center=(270*960+480)*4;
  // The source downsample/filter footprint crosses the centre stripe edge;
  // its unwarped bilinear value is135, rather than the raw stripe's142.
  require(std::abs(int(red[center])-198)<3 && std::abs(int(zero[center])-135)<3,
      ("Source Esp08 alpha255/power255 signed S0.10 exponent1 offsets immutable EFB sample by72.71 source texels: "+std::to_string(red[center])+"/"+std::to_string(zero[center])).c_str());
  re4::DestroyRoomAreaEffectsGl();re4::DestroyEnvironmentGl();
  std::cout<<"Esp08 immutable EFB/A-B indirect/signed matrix coefficient source warp PASS\n";
}
void openingCheck(const std::string &files,const std::string &name,const std::string &output) {
  const re4::RoomArchive archive(re4::LoadYz2(files+"/St1/r120.das").bytes);
  const auto copy=[&](unsigned sequence){const auto *entry=archive.find("SMD",sequence);require(entry!=nullptr,"Actual intro room SMD bank exists");return archive.copy(entry-archive.entries().data());};
  re4::Smd local(copy(0));std::unique_ptr<re4::Smd> common;
  if(archive.find("SMD",1))common=std::make_unique<re4::Smd>(copy(1),local.commonRequirements());
  re4::OriginalEvent event(files,name);event.prepareRoomEffectModels(local,common.get());
  require(event.effectModels().empty()==event.effectsData().eventModels.empty(),"Every source opening EFM group prepared once");
  unsigned sourceCuts{},modelSamples{},visibleModels{},spriteSamples{},visibleSprites{},refractions{};
  while(!event.finished()) {
    if(event.cutFrame()==6) {
      ++sourceCuts;const auto matrix=cameraMatrix(event.camera());
      const auto baseline=draw(event);
      const auto beforeUpload=uploads;re4::DrawEventEffectModelsGl(matrix.data(),{},.0001f,event,false);
      re4::DrawEventEffectModelsGl(matrix.data(),{},.0001f,event,false,true);
      const auto modelImage=pixels();require(uploads==beforeUpload,"Source EFM instances share immutable SMD mesh/texture cache");
      if(!re4::WorldEffectModels(event.effectsData(),event.effectsState()).empty()) {
        ++modelSamples;visibleModels+=changes(baseline,modelImage)>20;
      }
      re4::WorldEffectCamera camera;const auto pose=re4::EventCameraPose(event.camera());
      for(unsigned k=0;k<3;++k){camera.eye[k]=float(pose.eye[k]);camera.right[k]=float(pose.right[k]);camera.up[k]=float(pose.up[k]);camera.forward[k]=float(pose.look[k]);}
      const auto quads=re4::WorldEffectQuads(event.effectsData(),event.effectsState(),camera);
      if(!quads.empty()) {
        re4::DrawEventEffectsGl(matrix.data(),{},.0001f,quads,true);
        re4::DrawEventEffectsGl(matrix.data(),{},.0001f,quads);
        ++spriteSamples;visibleSprites+=changes(modelImage,pixels())>20;
        for(const auto &q:quads)refractions+=q.type==24;
      }
      if(event.sourceCut()==0 || event.sourceCut()==6)save(output+"-"+name+"-cut"+std::to_string(event.sourceCut())+".ppm",pixels());
    }
    event.tick30Hz();
  }
  require(sourceCuts==event.data().cutFrames().size(),"Every original opening camera cut rendered");
  require(modelSamples && visibleModels && spriteSamples && visibleSprites,"Original opening EFM and sprite banks produce visible GLES pixels");
  if(name=="r120s00")require(refractions>0,"Actual opening Esp18 bank uses the EFB copy pass");
  re4::DestroyEventActorsGl();re4::DestroyEnvironmentGl();re4::RoomAtmosphere().data.reset();
  std::cout<<name<<" opening GLES PASS: "<<sourceCuts<<" source cuts; EFM "<<visibleModels<<'/'<<modelSamples<<", sprites "<<visibleSprites<<'/'<<spriteSamples<<", refractions "<<refractions<<'\n';
}
}
extern "C" void __real_glTexImage2D(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
extern "C" void __wrap_glTexImage2D(GLenum target,GLint level,GLint internal,GLsizei width,GLsizei height,GLint border,GLenum format,GLenum type,const void *data) {
  ++uploads;__real_glTexImage2D(target,level,internal,width,height,border,format,type,data);
}
extern "C" void __real_glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void*);
extern "C" void __wrap_glBufferSubData(GLenum target,GLintptr offset,GLsizeiptr size,const void *data) {
  if(target==GL_ARRAY_BUFFER)++meshUpdates;
  __real_glBufferSubData(target,offset,size,data);
}
int main(int argc,char **argv) {
  try {
    if(argc!=3)throw std::runtime_error("Usage: event_actor_gl_check DISC_FILES OUTPUT_PREFIX");
    Egl egl;fadeState();eventCheck(argv[1],"r100s03",argv[2],egl);eventCheck(argv[1],"r100s20",argv[2],egl);fadeState();
    effectPassCheck();trapPassCheck();trapDepthFormatsCheck();glowMaskCheck();shimmerCheck();openingCheck(argv[1],"r120s00",argv[2]);openingCheck(argv[1],"r120s01",argv[2]);
    re4::DestroyEventActorsGl();
    std::cout<<"Original event GLES PASS: "<<checks<<" checks\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
