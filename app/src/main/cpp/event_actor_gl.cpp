#include "event_actor_gl.h"
#include "actor_gl.h"
#include "world_effects_gl.h"
#include "event_mirror_gl.h"
#include "render_state_gl.h"
#include "r100_secondary.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace re4 {
namespace {
std::map<const GenericEventActor*,std::uint64_t> revisions;
std::set<const StaticScene*> modelOwners;
GLuint captionProgram{},captionVao{},captionVbo{},captionTexture{};
GLuint fadeProgram{},fadeVao{};
const SubscreenFont *captionFont{};
const OriginalEvent *effectsOwner{};
std::uint64_t effectsSignature{};
struct CaptionState {
  GLint program{},vao{},buffer{},active{},texture{},viewport[4]{},
      equationRgb{},equationAlpha{},sourceRgb{},sourceAlpha{},destinationRgb{},destinationAlpha{};
  GLboolean depth{},blend{},cull{},scissor{},stencil{},depthMask{},colorMask[4]{};
  CaptionState() {
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
    glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glActiveTexture(GLenum(active));
    glGetIntegerv(GL_VIEWPORT,viewport);depth=glIsEnabled(GL_DEPTH_TEST);blend=glIsEnabled(GL_BLEND);cull=glIsEnabled(GL_CULL_FACE);
    scissor=glIsEnabled(GL_SCISSOR_TEST);stencil=glIsEnabled(GL_STENCIL_TEST);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);glGetBooleanv(GL_COLOR_WRITEMASK,colorMask);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&equationRgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equationAlpha);
    glGetIntegerv(GL_BLEND_SRC_RGB,&sourceRgb);glGetIntegerv(GL_BLEND_SRC_ALPHA,&sourceAlpha);
    glGetIntegerv(GL_BLEND_DST_RGB,&destinationRgb);glGetIntegerv(GL_BLEND_DST_ALPHA,&destinationAlpha);
  }
  ~CaptionState() {
    glUseProgram(GLuint(program));glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(buffer));
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,GLuint(texture));glActiveTexture(GLenum(active));
    glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    if(depth)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
    if(blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);
    if(cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
    if(scissor)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
    if(stencil)glEnable(GL_STENCIL_TEST);else glDisable(GL_STENCIL_TEST);
    glDepthMask(depthMask);glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]);
    glBlendEquationSeparate(GLenum(equationRgb),GLenum(equationAlpha));
    glBlendFuncSeparate(GLenum(sourceRgb),GLenum(destinationRgb),GLenum(sourceAlpha),GLenum(destinationAlpha));
  }
};
GLuint compile(GLenum type,const char *source) {
  const auto shader=glCreateShader(type);glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);
  GLint okay{};glGetShaderiv(shader,GL_COMPILE_STATUS,&okay);
  if(!okay) {char log[1024]{};glGetShaderInfoLog(shader,sizeof log,nullptr,log);glDeleteShader(shader);throw std::runtime_error(std::string("EVD subtitle: ")+log);}
  return shader;
}
void captionResources(const SubscreenFont &font) {
  if(captionFont==&font && captionProgram)return;
  if(font.sheets.empty() || !font.cellWidth || !font.cellHeight)throw std::runtime_error("EVD subtitle: Original font is missing");
  const auto &sheet=font.sheets.at(0);
  if(!sheet.width || !sheet.height || sheet.rgba.size()!=std::size_t(sheet.width)*sheet.height*4)
    throw std::runtime_error("EVD subtitle: Font texture size");
  if(captionTexture)glDeleteTextures(1,&captionTexture);
  captionTexture=0;
  if(!captionProgram) {
    const auto vs=compile(GL_VERTEX_SHADER,R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec2 p;layout(location=1) in vec2 uv;layout(location=2) in vec4 rgba;
uniform vec2 viewport;uniform float scale;uniform vec2 offset;out vec2 texcoord;out vec4 tint;
void main(){vec2 pixel=p*scale+offset;gl_Position=vec4(pixel.x/viewport.x*2.0-1.0,1.0-pixel.y/viewport.y*2.0,0,1);texcoord=uv;tint=rgba;}
)GLSL"),fs=compile(GL_FRAGMENT_SHADER,R"GLSL(#version 300 es
precision mediump float;
in vec2 texcoord;in vec4 tint;uniform sampler2D fontImage;out vec4 color;
void main(){color=texture(fontImage,texcoord)*tint;}
)GLSL");
    captionProgram=glCreateProgram();glAttachShader(captionProgram,vs);glAttachShader(captionProgram,fs);glLinkProgram(captionProgram);
    glDeleteShader(vs);glDeleteShader(fs);GLint linked{};glGetProgramiv(captionProgram,GL_LINK_STATUS,&linked);
    if(!linked)throw std::runtime_error("EVD subtitle: Shader linking");
    glGenVertexArrays(1,&captionVao);glGenBuffers(1,&captionVbo);
    glBindVertexArray(captionVao);glBindBuffer(GL_ARRAY_BUFFER,captionVbo);
    for(unsigned n=0;n<3;++n) {const unsigned sizes[]={2,2,4},offsets[]={0,2,4};
      glVertexAttribPointer(n,sizes[n],GL_FLOAT,GL_FALSE,8*sizeof(float),reinterpret_cast<void*>(offsets[n]*sizeof(float)));glEnableVertexAttribArray(n);}
  }
  glActiveTexture(GL_TEXTURE0);glGenTextures(1,&captionTexture);glBindTexture(GL_TEXTURE_2D,captionTexture);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,sheet.width,sheet.height,0,GL_RGBA,GL_UNSIGNED_BYTE,sheet.rgba.data());
  if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("EVD subtitle: Font upload");
  captionFont=&font;
}
}
FollowCameraPose EventCameraPose(const EventCamera &camera) {
  if(!camera.valid || !std::isfinite(camera.roll) || !std::isfinite(camera.fovRadians) || camera.fovRadians<=0 || camera.fovRadians>=3.1415927f)
    throw std::runtime_error("EVD: invalid original camera");
  FollowCameraPose pose;double length=0;
  for(unsigned n=0;n<3;++n) {
    if(!std::isfinite(camera.eye[n]) || !std::isfinite(camera.target[n]))throw std::runtime_error("EVD: Non-finite camera");
    pose.eye[n]=camera.eye[n];pose.target[n]=camera.target[n];pose.look[n]=pose.target[n]-pose.eye[n];length+=pose.look[n]*pose.look[n];
  }
  length=std::sqrt(length);if(length<.00001)throw std::runtime_error("EVD: Camera has no view length");
  for(auto &n:pose.look)n/=length;
  auto right=CollisionVec{-pose.look[2],0,pose.look[0]};const auto horizontal=std::hypot(right[0],right[2]);
  if(horizontal>.00001)for(auto &n:right)n/=horizontal;
  else right={1,0,0};
  const auto &look=pose.look;const CollisionVec up{right[1]*look[2]-right[2]*look[1],right[2]*look[0]-right[0]*look[2],right[0]*look[1]-right[1]*look[0]};
  const auto c=std::cos(camera.roll),s=std::sin(camera.roll);
  for(unsigned n=0;n<3;++n) {pose.right[n]=right[n]*c+up[n]*s;pose.up[n]=up[n]*c-right[n]*s;}
  pose.fovDegrees=camera.fovRadians*180/3.14159265358979323846;pose.distance=length;return pose;
}
void SyncEventActorsGl(const OriginalEvent &event) {
  const auto signature=event.effectsData().sprites.signature;
  if(effectsOwner!=&event || effectsSignature!=signature) {
    DestroyEventEffectsGl();
    if(!event.effectsData().sprites.animations.empty())CreateEventEffectsGl(event.effectsData());
    effectsOwner=&event;effectsSignature=signature;
  }
  std::set<const GenericEventActor*> owners;
  for(const auto &entry:event.actors()) {
    const auto *owner=entry.get();owners.insert(owner);
    const auto current=revisions.find(owner);
    if(current==revisions.end()) {CreateGenericActorGl(owner,owner->mesh(),owner->skin());revisions.emplace(owner,owner->meshRevision());}
    else if(current->second!=owner->meshRevision()) {UpdateGenericActorGl(owner,owner->mesh());current->second=owner->meshRevision();}
  }
  for(auto current=revisions.begin();current!=revisions.end();) {
    if(!owners.count(current->first)) {DestroyGenericActorGl(current->first);current=revisions.erase(current);}else ++current;
  }
  std::set<const StaticScene*> models;
  for(const auto &entry:event.effectModels()) {
    const auto *mesh=&entry.second;models.insert(mesh);
    if(modelOwners.count(mesh))continue;
    std::vector<SkinCorner> skin(mesh->vertices.size()/3);
    for(auto &corner:skin)for(auto *weight:{&corner.position,&corner.normal}) {
      weight->count=1;weight->joints[0]=0;weight->rates[0]=1;
    }
    CreateGenericActorGl(mesh,*mesh,skin);
  }
  for(const auto &fragment:event.fragmentData().meshes) {
    const auto *mesh=&fragment.mesh;models.insert(mesh);
    if(modelOwners.count(mesh))continue;
    std::vector<SkinCorner> skin(mesh->vertices.size()/3);
    for(auto &corner:skin)for(auto *weight:{&corner.position,&corner.normal}) {weight->count=1;weight->joints[0]=0;weight->rates[0]=1;}
    CreateGenericActorGl(mesh,*mesh,skin);
  }
  for(const auto *mesh:modelOwners)if(!models.count(mesh))DestroyGenericActorGl(mesh);
  modelOwners=std::move(models);
}
void CreateEventActorsGl(const OriginalEvent &event) {SyncEventActorsGl(event);}
void ForgetEventActorsGl() {
  ForgetEventMirrorGl();
  for(const auto &entry:revisions)ForgetGenericActorGl(entry.first);
  for(const auto *mesh:modelOwners)ForgetGenericActorGl(mesh);
  ForgetEventEffectsGl();effectsOwner=nullptr;effectsSignature=0;
  revisions.clear();captionProgram=captionVao=captionVbo=captionTexture=0;captionFont=nullptr;
  modelOwners.clear();
  fadeProgram=fadeVao=0;
}
void DestroyEventActorsGl() {
  DestroyEventMirrorGl();
  for(const auto &entry:revisions)DestroyGenericActorGl(entry.first);
  for(const auto *mesh:modelOwners)DestroyGenericActorGl(mesh);
  DestroyEventEffectsGl();effectsOwner=nullptr;effectsSignature=0;revisions.clear();
  if(captionTexture)glDeleteTextures(1,&captionTexture);
  if(captionVbo)glDeleteBuffers(1,&captionVbo);
  if(captionVao)glDeleteVertexArrays(1,&captionVao);
  if(captionProgram)glDeleteProgram(captionProgram);
  if(fadeVao)glDeleteVertexArrays(1,&fadeVao);
  if(fadeProgram)glDeleteProgram(fadeProgram);
  captionProgram=captionVao=captionVbo=captionTexture=0;captionFont=nullptr;
  modelOwners.clear();
  fadeProgram=fadeVao=0;
}
void DrawEventActorsGl(const float *camera,const std::array<float,3> &center,float scale,const OriginalEvent &event,bool normalView,
                       unsigned firstPass,unsigned lastPass) {
  SyncEventActorsGl(event);
  std::vector<const GenericEventActor*> ordered;
  std::map<const GenericEventActor*,unsigned> insertion;
  for(unsigned i=0;i<event.actors().size();++i) {
    const auto *owner=event.actors()[i].get();insertion[owner]=i;
    if(owner->visible())ordered.push_back(owner);
  }
  const auto pose=EventCameraPose(event.camera());
  const auto pass=[](const GenericEventActor *a){constexpr unsigned ot[]{13,17,11,11,11,16,20};return ot[a->orderType()];};
  const auto bucket=[&](const GenericEventActor *a) {
    constexpr unsigned direct[]{0,0,4,2,1,0,1};const auto type=a->orderType();
    if(type>1)return direct[type];
    const auto p=type==0?a->lightRootPosition():a->lightPosition();double z=0;
    for(unsigned n=0;n<3;++n)z+=(p[n]-pose.eye[n])*pose.look[n];
    return unsigned(std::clamp(z*(type==0?.01:.05),0.,type==0?127.:1023.));
  };
  // ModelTrans's actual tables: car in scroll OT0xB, ordinary actors in
  // model OT0xD, projected mirror in effect OT0x10, Leon/glass in world OT0x11.
  // Depth buckets apply even to opaque OT1 actors, rather than a glass-only
  // sorting heuristic that paints the mirror over the completed car shot.
  std::stable_sort(ordered.begin(),ordered.end(),[&](const auto *a,const auto *b){
    if(pass(a)!=pass(b))return pass(a)<pass(b);
    if(bucket(a)!=bucket(b))return bucket(a)>bucket(b);
    const auto ai=insertion.at(a->orderOwner()),bi=insertion.at(b->orderOwner());
    if(ai!=bi)return ai>bi; // AddOt inserts the most recent model at the head.
    return insertion.at(a)<insertion.at(b); // addModel keeps its parent's chain.
  });
  bool mirrorDrawn=false;
  for(const auto *owner:ordered) {
    if(pass(owner)<firstPass || pass(owner)>lastPass)continue;
    if(IsEventMirrorActive(event) && (owner->orderOwner()->name()=="pl0010" || owner->name()=="obm3010f"))continue;
    if(!mirrorDrawn && firstPass<=16 && lastPass>=16 && pass(owner)>=16) {
      DrawEventMirrorGl(camera,center,scale,event,normalView);mirrorDrawn=true;
    }
    const auto light=owner->lightRootPosition();
    GenericActorStyle style;style.color=owner->materialColor();style.zMode=owner->depthMode();
    style.lightVolume=owner->lightVolume();
    style.globalIlluminationImage=owner->globalIlluminationTexture();
    style.globalIlluminationScale=owner->globalIlluminationScale();
    if(const auto *rig=owner->rig())DrawGenericActorGl(owner,camera,center,scale,rig->bones(),rig->bindCenters(),owner->worldMatrix(),normalView,
        owner->lightMask(),&light,owner->lightSize(),&style);
    else {
      // The actual s03 handgun wep0200 has an empty FCV and zero rootrest.
      // cModel::setPartsOffset rest translations cancel their inverse binds,
      // so the unchanged local BIN corners use an identity skin palette;
      // its original SetParent/event offset is still in worldMatrix().
      unsigned count=1;
      for(const auto &corner:owner->skin())for(const auto *weight:{&corner.position,&corner.normal})
        for(unsigned n=0;n<weight->count;++n)count=std::max(count,unsigned(weight->joints[n])+1);
      if(count>128)throw std::runtime_error("EVD: Static part budget");
      std::vector<Matrix34> identity(count,Matrix34{1,0,0,0,0,1,0,0,0,0,1,0});
      std::vector<std::array<float,3>> bind(count);
      DrawGenericActorGl(owner,camera,center,scale,identity,bind,owner->worldMatrix(),normalView,
          owner->lightMask(),&light,owner->lightSize(),&style);
    }
  }
  if(!mirrorDrawn && firstPass<=16 && lastPass>=16) {
    DrawEventMirrorGl(camera,center,scale,event,normalView);
  }
}
void DrawEventEffectModelsGl(const float *camera,const std::array<float,3> &center,
                        float scale,const OriginalEvent &event,bool normalView,bool translucent) {
  auto models=WorldEffectModels(event.effectsData(),event.effectsState());
  if(models.empty() && event.fragments().empty() && WorldEffectLines(event.effectsData(),event.effectsState()).empty())return;
  SyncEventActorsGl(event);
  constexpr Matrix34 identity{1,0,0,0,0,1,0,0,0,0,1,0};
  const std::vector<Matrix34> bones{identity};const std::vector<std::array<float,3>> bind(1);
  const auto pose=EventCameraPose(event.camera());
  const auto depth=[&](const WorldEffectModel &m) {
    double z=0;for(unsigned k=0;k<3;++k)z+=(m.matrix[k*4+3]-pose.eye[k])*pose.look[k];return z;
  };
  // cObj04::move switches OT0/OT1 at its original Col_a<250 threshold.
  // Translucent model effects follow opaque actors, sorted back to front.
  std::stable_sort(models.begin(),models.end(),[&](const auto &a,const auto &b){
    return depth(a)>depth(b) || (depth(a)==depth(b) && a.serial>b.serial);
  });
  for(const auto &fragment:event.fragments()) {
    if((fragment.color[3]*255<250)!=translucent)continue;
    const auto &mesh=event.fragmentData().meshes.at(fragment.mesh).mesh;
    GenericActorStyle style;style.color=fragment.color;style.blendMode=fragment.blend;style.zMode=fragment.zMode;style.alphaOmit=128;
    DrawGenericActorGl(&mesh,camera,center,scale,bones,bind,fragment.matrix,normalView,0,nullptr,0,&style);
  }
  if(translucent)DrawTrapEffectLinesGl(camera,center,scale,WorldEffectLines(event.effectsData(),event.effectsState()));
  for(const auto &model:models) {
    if((model.color[3]*255<250)!=translucent)continue;
    const auto found=event.effectModels().find(model.model);
    if(found==event.effectModels().end())throw std::runtime_error("EVD EFM: Group model not prepared");
    const auto &mesh=found->second;
    GenericActorStyle style;style.color=model.color;style.alphaOmit=128;
    style.selectMask=model.selectMask;
    // EfmSetObj04 also sets ModelRender's be_flag20000 for Tool40 clear:
    // GX colour-channel lighting is disabled and uses model material RGB.
    style.unlit=model.selectMask==0;
    const auto &record=event.effectsData().sprites.targets.at(model.target).records.at(model.record);
    style.blendMode=record.bytes[194];
    style.zMode=(std::uint32_t(record.bytes[8])<<24 | std::uint32_t(record.bytes[9])<<16 |
        std::uint32_t(record.bytes[10])<<8 | record.bytes[11])&0x200000?1:0;
    // EfmSeqSet(moveId3) uses the BIN box and LightSetModel anchors
    // distance/directional lights at pList->world, not the box centre.
    // Reuse the fresh bind-pose volume, then apply the effect placement.
    const auto &source=mesh.draws.at(0);
    auto volume=source.lightVolume;
    volume.center=TransformPoint(model.matrix,volume.center);
    for(unsigned axis=0;axis<3;++axis) {
      std::array<float,3> basis{};float squared=0;
      for(unsigned row=0;row<3;++row) {
        for(unsigned k=0;k<3;++k)basis[row]+=model.matrix[row*4+k]*source.lightVolume.axes[axis*3+k];
        squared+=basis[row]*basis[row];
      }
      const float stretch=std::sqrt(squared);
      volume.size[axis]*=stretch;
      for(unsigned row=0;row<3;++row)volume.axes[axis*3+row]=stretch?basis[row]/stretch:0;
    }
    style.lightVolume=volume;
    const auto light=TransformPoint(model.matrix,source.sortCenter);
    DrawGenericActorGl(&mesh,camera,center,scale,bones,bind,model.matrix,normalView,
        model.lightMask,&light,std::max(source.lightVolume.size[0],source.lightVolume.size[1]),&style);
  }
}
void DrawEventFadeGl(int width,int height,unsigned alpha) {
  if(!alpha)return;
  if(width<1 || height<1 || alpha>255)throw std::runtime_error("EVD: Fade-Viewport/Alpha");
  CaptionState previous;
  if(!fadeProgram) {
    const auto vs=compile(GL_VERTEX_SHADER,R"GLSL(#version 300 es
precision highp float;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}
)GLSL"),fs=compile(GL_FRAGMENT_SHADER,R"GLSL(#version 300 es
precision mediump float;
uniform float alpha;out vec4 color;void main(){color=vec4(0,0,0,alpha);}
)GLSL");
    fadeProgram=glCreateProgram();glAttachShader(fadeProgram,vs);glAttachShader(fadeProgram,fs);glLinkProgram(fadeProgram);
    glDeleteShader(vs);glDeleteShader(fs);GLint linked{};glGetProgramiv(fadeProgram,GL_LINK_STATUS,&linked);
    if(!linked)throw std::runtime_error("EVD: Fade shader linking");
    glGenVertexArrays(1,&fadeVao);
  }
  glViewport(0,0,width,height);glUseProgram(fadeProgram);glBindVertexArray(fadeVao);
  glUniform1f(glGetUniformLocation(fadeProgram,"alpha"),float(alpha)/255);
  glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
  glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glDrawArrays(GL_TRIANGLES,0,3);
  if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("EVD: Fade GPU drawing");
}
void DrawEventSubtitlesGl(int width,int height,const OriginalEvent &event) {
  const auto &glyphs=event.subtitle().glyphs;if(glyphs.empty() || event.finished())return;
  if(width<1 || height<1 || glyphs.size()>256)throw std::runtime_error("EVD subtitle: Viewport/character budget");
  CaptionState previous;
  const auto &font=event.font();captionResources(font);const auto &sheet=font.sheets.at(0);
  const auto columns=sheet.width/font.cellWidth;if(!columns)throw std::runtime_error("EVD subtitle: Font columns");
  const bool japanese=font.cellWidth==28;
  const int fontWidth=japanese?25:22,fontHeight=japanese?27:32,
      charGap=japanese?1:-1,lineGap=japanese?27:25,
      space=int((japanese?13:8)*float(fontWidth)/font.cellWidth),top=336-lineGap-fontHeight-1;
  constexpr std::uint32_t colors[]={0xe5d9cff0,0x87cfa5ff,0xcd7d5fff,0x87afffff,0xa55fffff,0x707070ff,0x707070ff,0x52df73ff,0,0};
  std::array<int,16> widths{},pens{};int maximum=0;
  const auto advance=[&](unsigned glyph) {
    if(!glyph)return space; // MessageData::getSpaceWidth * source caption scale.
    if(glyph>=font.bearings.size())throw std::runtime_error("EVD subtitle: Glyph "+std::to_string(glyph)+
        " outside original font "+std::to_string(font.bearings.size())+" ("+event.data().name()+
        ", frame "+std::to_string(event.sourceFrame())+")");
    const auto &b=font.bearings[glyph];return int((int(b[1])-int(b[0]))*float(fontWidth)/font.cellWidth)+charGap;
  };
  for(const auto &g:glyphs) {if(g.line>=16 || g.color>=10)throw std::runtime_error("EVD subtitle: Line/color exceeds original budget");widths[g.line]+=advance(g.glyph);maximum=std::max(maximum,widths[g.line]);}
  for(auto &pen:pens)pen=(512-maximum)>>1; // Message::CalcSize widest-line centering.
  std::vector<float> vertices;vertices.reserve(glyphs.size()*6*8);
  for(const auto &g:glyphs) {
    const auto step=advance(g.glyph);
    if(g.glyph) {
      const auto &b=font.bearings[g.glyph];const int extent=int(b[1])-int(b[0]);
      if(extent<=0 || extent>int(font.cellWidth)*2)throw std::runtime_error("EVD subtitle: Glyph width");
      const float x=pens[g.line]*1.25f,y=(top+g.line*lineGap)*1.25f-60,w=(step-charGap)*1.25f,h=fontHeight*1.25f;
      const float u=float(int((g.glyph%columns)*font.cellWidth)+int(b[0]))/sheet.width,
          v=float(((g.glyph/columns)*font.cellHeight)%sheet.height)/sheet.height,
          du=float(extent)/sheet.width,dv=float(font.cellHeight)/sheet.height;
      const unsigned corners[]={0,1,2,0,2,3};const float p[][2]={{x,y},{x+w,y},{x+w,y+h},{x,y+h}},uv[][2]={{u,v},{u+du,v},{u+du,v+dv},{u,v+dv}};
      for(auto corner:corners) {vertices.push_back(p[corner][0]);vertices.push_back(p[corner][1]);vertices.push_back(uv[corner][0]);vertices.push_back(uv[corner][1]);
        for(unsigned n=0;n<4;++n)vertices.push_back(float((colors[g.color]>>((3-n)*8))&255)/255);}
    }
    pens[g.line]+=step;
  }
  if(vertices.empty())return;
  glViewport(0,0,width,height);glUseProgram(captionProgram);glBindVertexArray(captionVao);glBindBuffer(GL_ARRAY_BUFFER,captionVbo);
  glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STREAM_DRAW);
  const float scale=std::min(width/640.f,height/360.f);
  glUniform2f(glGetUniformLocation(captionProgram,"viewport"),float(width),float(height));glUniform1f(glGetUniformLocation(captionProgram,"scale"),scale);
  glUniform2f(glGetUniformLocation(captionProgram,"offset"),(width-640*scale)*.5f,(height-360*scale)*.5f);
  glUniform1i(glGetUniformLocation(captionProgram,"fontImage"),0);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,captionTexture);
  glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()/8));
  if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("EVD subtitle: GPU drawing");
}
void DrawOriginalRoomMessageGl(int width,int height,const SubscreenFont &font,
                              const R100RoomMessage &message,bool fillScreen) {
  if(message.glyphs.empty())return;
  if(width<1 || height<1 || message.glyphs.size()>256 || !message.fontWidth ||
      !message.fontHeight || !message.lineGap || message.pageStart>=message.words.size())
    throw std::runtime_error("Room text: Original character budget/page");
  CaptionState previous;captionResources(font);const auto &sheet=font.sheets.at(0);
  const unsigned columns=sheet.width/font.cellWidth;
  if(!columns)throw std::runtime_error("Room text: Font columns");
  const auto advance=[&](unsigned glyph) {
    if(!glyph)return int((font.cellWidth==28?13:8)*float(message.fontWidth)/font.cellWidth);
    if(glyph>=font.bearings.size())throw std::runtime_error("Room text: Original glyph is missing");
    const auto &bearing=font.bearings[glyph];
    return int((int(bearing[1])-int(bearing[0]))*float(message.fontWidth)/font.cellWidth)+message.charGap;
  };
  std::array<int,16> widths{},pens{};unsigned line=0,lines=0;
  constexpr unsigned args[]{0,0,1,0,0,1,1,0,0,1,0,1,1,1,0,1,0,1,1};
  // Message::WidthCk measures the entire current page before glyph-speed
  // reveal; centering must remain stable as the live queue grows.
  for(std::size_t cursor=message.pageStart;cursor<message.words.size();++cursor) {
    const auto code=message.words[cursor];
    if(code>=128){widths[line]+=advance(code-128);continue;}
    if(code==1 || code==4){lines=line+(widths[line]>0);break;}
    if(code>=19 || cursor+args[code]>=message.words.size())throw std::runtime_error("Room text: Original controls");
    if(code==3 && ++line>=16)throw std::runtime_error("Room text: Original line budget");
    cursor+=args[code];
  }
  const int maximum=*std::max_element(widths.begin(),widths.end());
  int top=int(message.y);
  if(font.cellWidth==28 && (message.attributes&0x1000) && lines==1)top+=message.lineGap;
  if(message.attributes&0x10000)top=(384-int(lines*message.lineGap))>>1;
  for(unsigned i=0;i<16;++i) {
    if(message.attributes&0x20000)pens[i]=int(message.x)-((message.attributes&0x40000)?widths[i]:0);
    else if(message.attributes&0x80000)pens[i]=(512-widths[i])>>1;
    else if(message.attributes&0x40000)pens[i]=512-widths[i]-(512-maximum)/2;
    else pens[i]=(512-maximum)>>1;
  }
  constexpr std::uint32_t colors[]{0xe5d9cff0,0x87cfa5ff,0xcd7d5fff,0x87afffff,0xa55fffff,0x707070ff,0x707070ff,0x52df73ff,0,0};
  std::vector<float> vertices;vertices.reserve(message.glyphs.size()*6*8);
  for(const auto &glyph:message.glyphs) {
    if(glyph.line>=16 || glyph.color>=10)throw std::runtime_error("Room text: Original color/line");
    const auto step=advance(glyph.glyph);
    if(glyph.glyph) {
      const auto &bearing=font.bearings[glyph.glyph];const int extent=int(bearing[1])-int(bearing[0]);
      const float x=pens[glyph.line]*1.25f,y=(top+int(glyph.line*message.lineGap))*1.25f-60,
          w=(step-message.charGap)*1.25f,h=message.fontHeight*1.25f;
      const float u=float(int((glyph.glyph%columns)*font.cellWidth)+int(bearing[0]))/sheet.width,
          v=float(((glyph.glyph/columns)*font.cellHeight)%sheet.height)/sheet.height,
          du=float(extent)/sheet.width,dv=float(font.cellHeight)/sheet.height;
      const unsigned corners[]{0,1,2,0,2,3};
      const float p[][2]{{x,y},{x+w,y},{x+w,y+h},{x,y+h}},uv[][2]{{u,v},{u+du,v},{u+du,v+dv},{u,v+dv}};
      for(const auto corner:corners){vertices.insert(vertices.end(),{p[corner][0],p[corner][1],uv[corner][0],uv[corner][1]});
        for(unsigned n=0;n<4;++n)vertices.push_back(float((colors[glyph.color]>>((3-n)*8))&255)/255);}
    }
    pens[glyph.line]+=step;
  }
  if(vertices.empty())return;
  glViewport(0,0,width,height);glUseProgram(captionProgram);glBindVertexArray(captionVao);glBindBuffer(GL_ARRAY_BUFFER,captionVbo);
  glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(float)),vertices.data(),GL_STREAM_DRAW);
  const float scale=std::min(width/640.f,height/360.f);
  glUniform2f(glGetUniformLocation(captionProgram,"viewport"),fillScreen?640.f:float(width),fillScreen?360.f:float(height));glUniform1f(glGetUniformLocation(captionProgram,"scale"),fillScreen?1.f:scale);
  glUniform2f(glGetUniformLocation(captionProgram,"offset"),fillScreen?0.f:(width-640*scale)*.5f,fillScreen?0.f:(height-360*scale)*.5f);
  glUniform1i(glGetUniformLocation(captionProgram,"fontImage"),0);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,captionTexture);
  glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()/8));
  if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Room text: GPU drawing");
}
}
