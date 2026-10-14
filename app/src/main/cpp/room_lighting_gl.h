#pragma once
#include "room_lighting.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <stdexcept>
namespace re4 {
// GL-thread-owned frame state. CPU room data are shared/immutable, survive a
// lost context, and are rebound by CreateSceneGl. No GLuint is retained here.
struct RoomAtmosphereGl {
  std::shared_ptr<const RoomLighting> data;
  std::array<float, 3> eye{}, right{1,0,0}, up{0,1,0}, look{0,0,-1};
  // Original ParentType1 / parent.no0: current Leon parts in world space.
  // Other owners remain unresolved until their source lifecycle is ported.
  std::vector<std::array<float, 12>> playerParts;
  std::vector<RoomLight> transientLights; // Source Esp11-owned effects, no preview lights.
  RoomLightTransition transition;
  // Rmode's authored EFB viewport; mirror RTT0 temporarily uses592x512.
  std::array<float,2> modelTextureViewport{512,448};
  float modelTextureCameraAspect=4.f/3;
  bool cinematicFrame{};
  bool suspended{};
};
inline RoomAtmosphereGl &RoomAtmosphere() { static RoomAtmosphereGl state; return state; }
inline const RoomLightCut *CurrentRoomLightCut() {
  const auto &s=RoomAtmosphere();
  return s.data?&s.data->cuts.at(s.transition.owner==s.data.get()?s.transition.cut:0):nullptr;
}
inline const RoomFog *CurrentRoomFog() {
  const auto &s=RoomAtmosphere();const auto *c=CurrentRoomLightCut();
  return !c?nullptr:s.transition.owner==s.data.get()?&s.transition.fog:&c->fog;
}
struct RoomLightBindingCache {
  bool valid{}, disabled{};
  GLuint program{};
  unsigned enable{};
  std::uint32_t mask{};
  std::array<float,3> position{};
  float size{};
  bool edgeFade{};
  unsigned limit=8,alphaPower=256;
  const RoomLighting *data{};
  bool scrollAttribute{};
};
inline RoomLightBindingCache &RoomLightBindings() { static RoomLightBindingCache cache; return cache; }
inline std::string RoomAtmosphereShader(std::string source, bool vertex) {
  const char *declarations = vertex ? R"GLSL(
uniform int roomLightingEnabled, roomLightCount;
uniform vec3 roomAmbient;
uniform vec3 roomMaterialRGB;
uniform vec4 roomLightPosition[8], roomLightColor[8], roomLightAttenuation[8], roomLightSpot[8];
vec4 litRoomColor(vec4 c, vec3 position, vec3 normal, int vertexAmbient) {
  if(roomLightingEnabled==0) return c;
  vec3 n=length(normal)>0.0?normalize(normal):vec3(0,1,0);
  ivec3 result=ivec3(round((vertexAmbient!=0?c.rgb:roomAmbient)*255.0));
  for(int i=0;i<8;++i) {
    if(i>=roomLightCount) break;
    vec3 delta=roomLightPosition[i].xyz-position;
    float d=length(delta);
    vec3 l=d>0.0?delta/d:vec3(0);
    float cone=dot(-l,roomLightSpot[i].xyz);
    float a=roomLightAttenuation[i].x+roomLightAttenuation[i].y*cone+roomLightAttenuation[i].z*cone*cone;
    float attenuation=max(a,0.0)/max(roomLightSpot[i].w+roomLightPosition[i].w*d+roomLightAttenuation[i].w*d*d,0.00001);
    result+=ivec3(round(roomLightColor[i].rgb*255.0*max(dot(n,l),0.0)*attenuation));
  }
  // GX XF accumulates byte-valued light contributions, clamps that sum,
  // then multiplies by material with its 255-to-256 integer expansion.
  // Clamping after material falsely brightens tinted, overlit geometry.
  result=clamp(result,ivec3(0),ivec3(255));
  ivec3 material=ivec3(round(clamp(roomMaterialRGB,0.0,1.0)*255.0));
  return vec4(vec3((material*(result+(result>>7)))>>8)/255.0,c.a);
}
)GLSL" : R"GLSL(
uniform int roomLightingEnabled, roomFogType;
uniform vec2 roomTevScale;
uniform vec2 roomFogAC;
uniform uvec2 roomFogB;
uniform int roomFogOrthographic;
uniform vec3 roomFogColor, roomEye, roomLook, roomRight, roomUp;
uniform vec2 roomModelTextureGradient;
uniform float roomModelLodBias;
vec4 modelTexture(sampler2D image,vec2 uv) {
  float bias=exp2(roomModelLodBias);
  return textureGrad(image,uv,dFdx(uv)*roomModelTextureGradient.x*bias,
      dFdy(uv)*roomModelTextureGradient.y*bias);
}
vec4 fogRoomColor(vec4 c, vec3 position) {
  if(roomFogType==0) return c;
  // GX evaluates fog from rasterized 24-bit Z and its quantized registers,
  // after projection. Use the same path for opaque and alpha scenery.
  uint depth=min(uint(clamp(gl_FragCoord.z,0.0,1.0)*16777216.0),0xffffffu);
  float z=roomFogOrthographic!=0?roomFogAC.x*float(depth)/16777216.0:
      roomFogAC.x*16777216.0/float(int(roomFogB.x)-int(depth>>roomFogB.y));
  float x=clamp(z-roomFogAC.y,0.0,1.0);
  float a=x;
  if(roomFogType==4) a=1.0-exp2(-8.0*x);
  else if(roomFogType==5) a=1.0-exp2(-8.0*x*x);
  else if(roomFogType==6) a=exp2(-8.0*(1.0-x));
  else if(roomFogType==7) a=exp2(-8.0*(1.0-x)*(1.0-x));
  int factor=int(round(a*256.0));
  ivec3 before=ivec3(round(clamp(c.rgb,0.0,1.0)*255.0)),fog=ivec3(round(roomFogColor*255.0));
  return vec4(vec3((before*(256-factor)+fog*factor)>>8)/255.0,c.a);
}
)GLSL";
  source.insert(source.find("\n")+1,"precision highp int;\n");
  const auto p=source.find("void main()");
  if(p==std::string::npos) throw std::runtime_error("LIT: Shader entry point is missing");
  source.insert(p,declarations); return source;
}
inline std::string RoomSpecularShader(std::string source) {
  const char *declarations=R"GLSL(
uniform sampler2D specularImage;
uniform sampler2D globalIlluminationImage;
uniform int globalIlluminationEnabled;
uniform float globalIlluminationScale;
uniform int specularType;
uniform vec4 specularParameters;
uniform vec3 roomSpecularNormalScale;
vec3 specularRoomColor(vec3 c,vec3 raster,vec3 normal) {
  // trans.cpp::specularSetup: normal texgen in the current view, authored
  // scale and sign of Y. Preserve the source normal's length.
  vec3 lookupNormal=normal*roomSpecularNormalScale;
  vec2 projected=vec2(dot(lookupNormal,roomRight),-dot(lookupNormal,roomUp));
  if(specularType>=0) {
    vec4 s=texture(specularImage,vec2(.5)+specularParameters.w*projected);
    if(specularType==0)c+=clamp(s.rgb*raster,0.0,1.0)*specularParameters.rgb;
    else c+=s.a*c;
  }
  // GlobalIlluminationSetup: previous TEV RGB * texture RGB, scale2,
  // clamped before the final model colour-scale stage. No added brightness.
  if(globalIlluminationEnabled!=0)c=clamp(c*texture(globalIlluminationImage,vec2(.5)+globalIlluminationScale*projected).rgb*2.0,0.0,1.0);
  return c;
}
)GLSL";
  const auto p=source.find("void main()");
  if(p==std::string::npos)throw std::runtime_error("Specular: Shader entry point is missing");
  source.insert(p,declarations);return source;
}
inline void BindRoomFog(GLuint program) {
  // Every scene/actor draw starts here, also after EGL recreation. Cache only
  // consecutive materials of that draw; camera-relative lights renew next
  // frame and no old program name can suppress a new context's uniforms.
  RoomLightBindings().valid = false;
  const auto &state=RoomAtmosphere();
  const auto *cut=CurrentRoomLightCut();const auto *fog=CurrentRoomFog();
  glUniform1i(glGetUniformLocation(program,"roomFogType"),fog?int(fog->type&7):0);
  glUniform2f(glGetUniformLocation(program,"roomTevScale"),cut?float(1u<<cut->tevScale[0]):1.f,
              cut?float(1u<<cut->tevScale[1]):1.f);
  glUniform3fv(glGetUniformLocation(program,"roomRight"),1,state.right.data());
  glUniform3fv(glGetUniformLocation(program,"roomUp"),1,state.up.data());
  glUniform3f(glGetUniformLocation(program,"roomSpecularNormalScale"),1,1,1);
  GLint viewport[4]{};glGetIntegerv(GL_VIEWPORT,viewport);
  glUniform2f(glGetUniformLocation(program,"roomModelTextureGradient"),
      float(viewport[2])/state.modelTextureViewport[0]*(4.f/3)/state.modelTextureCameraAspect,
      float(viewport[3])/state.modelTextureViewport[1]);
  glUniform1f(glGetUniformLocation(program,"roomModelLodBias"),cut?cut->lodBias:0);
  if(!cut) return;
  const auto registers=RoomFogParameters(*fog);
  glUniform2f(glGetUniformLocation(program,"roomFogAC"),registers.a,registers.c);
  glUniform2ui(glGetUniformLocation(program,"roomFogB"),registers.b,registers.shift);
  glUniform1i(glGetUniformLocation(program,"roomFogOrthographic"),registers.orthographic);
  glUniform3f(glGetUniformLocation(program,"roomFogColor"),fog->color[0]/255.f,fog->color[1]/255.f,fog->color[2]/255.f);
  glUniform3fv(glGetUniformLocation(program,"roomEye"),1,state.eye.data());
  glUniform3fv(glGetUniformLocation(program,"roomLook"),1,state.look.data());
}
inline void BindRoomLights(GLuint program, unsigned enableMask, std::uint32_t selectMask,
                           const std::array<float,3> &position, float size, bool disabled=false,
                           unsigned limit=8,unsigned alphaPower=256,bool edgeFade=false,
                           const RoomLightVolume *volume=nullptr,bool scrollAttribute=false) {
  glUniform3f(glGetUniformLocation(program,"roomMaterialRGB"),1,1,1);
  const auto &state=RoomAtmosphere();
  auto &cache=RoomLightBindings();
  if(!volume && cache.valid && cache.program==program && cache.data==state.data.get() &&
     cache.enable==enableMask && cache.mask==selectMask && cache.position==position &&
     cache.size==size && cache.edgeFade==edgeFade && cache.disabled==disabled && cache.limit==limit && cache.alphaPower==alphaPower && cache.scrollAttribute==scrollAttribute) return;
  cache={!volume,disabled,program,enableMask,selectMask,position,size,edgeFade,limit,alphaPower,state.data.get(),scrollAttribute};
  glUniform1i(glGetUniformLocation(program,"roomLightingEnabled"),state.data?1:0);
  if(!state.data) return;
  const auto &cut=*CurrentRoomLightCut();
  const auto &amb=(enableMask&16)?cut.screenAmbient:(enableMask&8)?cut.effectAmbient:cut.actorAmbient;
  std::array<float,3> ambient{amb[0]/255.f,amb[1]/255.f,amb[2]/255.f};
  std::array<float,32> positions{}, colors{}, attenuations{}, spots{};
  unsigned n=0, selected=0;
  const auto transientBase=std::max(state.data->maximumLights,unsigned(cut.lights.size())+10);
  for(unsigned i=0; !disabled && i<cut.lights.size()+state.transientLights.size() && selected<std::min(limit,8u); ++i) {
    const bool transient=i>=cut.lights.size();
    const auto &l=transient?state.transientLights[i-cut.lights.size()]:cut.lights[i];
    // LightMgr::update reserves max(nMaxLight-nLight,10) alive invisible
    // slots before Esp11::SetFreeWork/create takes the next empty pool slot.
    const auto poolIndex=transient?transientBase+i-unsigned(cut.lights.size()):i;
    // cLightMgr::init/roomInit enables every kind; a nonzero Kind is not an
    // inactive light. Id4 belongs to the separate original shadow pass.
    if((l.flags&3)!=3 || !(l.enableMask&enableMask) || l.id==4 || (scrollAttribute && (l.attribute&4)) ||
        (poolIndex<32 && !(selectMask&(std::uint32_t(1)<<poolIndex)))) continue;
    auto lightPosition=l.position, lightDirection=l.direction;
    if(l.parent) {
      // EmMgr's suspended gameplay Leon lacks MODEL_NOSUSPEND (0x800).
      if(state.suspended)continue;
      if(l.parent!=1 || (l.parentNo&0xffff)!=0 || (l.parentNo>>16)>=state.playerParts.size()) continue;
      const auto &m=state.playerParts[l.parentNo>>16];
      for(unsigned k=0;k<3;++k) {
        lightPosition[k]=m[k*4+3]; lightDirection[k]=0;
        for(unsigned j=0;j<3;++j) {
          lightPosition[k]+=m[k*4+j]*l.position[j];
          lightDirection[k]+=m[k*4+j]*l.direction[j];
        }
      }
    }
    if(!(l.color[0]||l.color[1]||l.color[2])) continue;
    double d2=0;
    for(unsigned k=0;k<3;++k) { const auto v=position[k]-lightPosition[k]; d2+=double(v)*v; }
    const auto d=float(std::sqrt(d2)), range=l.radius+size;
    if(volume?!RoomLightTouches(*volume,lightPosition,l.radius):l.radius && d>=range)continue;
    ++selected; // Type7 ambient still consumes one original GX list slot.
    float fade=1;
    if(edgeFade && l.radius && l.edge>0 && d>=range-l.edge)
      fade=std::clamp((range-d)/l.edge,0.f,1.f);
    if(l.type==7) {
      for(unsigned k=0;k<3;++k) ambient[k]=std::max(ambient[k],l.color[k]/255.f*fade);
      continue;
    }
    auto p=lightPosition;
    if(l.type==5) {
      auto direction=l.direction;
      if(l.directionFlags&1)
        for(unsigned k=0;k<3;++k) direction[k]=state.right[k]*l.direction[0]+state.up[k]*l.direction[1]-state.look[k]*l.direction[2];
      for(unsigned k=0;k<3;++k) p[k]=position[k]+direction[k];
    }
    for(unsigned k=0;k<3;++k) {
      positions[n*4+k]=p[k];
      // lightSetColor truncates after alpha /128 and clamps to byte range.
      colors[n*4+k]=float(std::min(255u,unsigned(l.color[k])*((unsigned(l.color[3])*alphaPower)>>8)/128))/255.f;
    }
    auto &a=attenuations;
    spots[n*4+3]=1;
    a[n*4]=l.intensity*fade;
    if(l.type==1 && l.radius) a[n*4]=std::max(0.f,l.intensity*(l.radius-d)/l.radius);
    if(l.type==2 && l.radius) a[n*4+3]=(l.intensity-.1f)/(.1f*l.radius*l.radius);
    if(l.type==3 || l.type==6) {
      // GX_SP_COS: (cos(theta)-cos(cutoff))/(1-cos(cutoff)).
      // GX_DA_MEDIUM uses a quadratic distance term; refDist=5000.
      if(l.cutoff>0 && l.cutoff<=90) {
        const auto c=std::cos(l.cutoff*.017453292519943295f);
        a[n*4]=-c/(1-c); a[n*4+1]=1/(1-c);
        const auto length=std::sqrt(lightDirection[0]*lightDirection[0]+lightDirection[1]*lightDirection[1]+lightDirection[2]*lightDirection[2]);
        if(length>0) for(unsigned k=0;k<3;++k) spots[n*4+k]=lightDirection[k]/length;
      } else a[n*4]=1;
      const auto br=l.intensity*fade;
      if(l.type==3 && br>0 && br<1) {
        positions[n*4+3]=.5f*(1-br)/(br*5000.f);
        a[n*4+3]=.5f*(1-br)/(br*5000.f*5000.f);
      }
      if(l.type==6 && l.radius && br>0) {
        a[n*4]*=br; a[n*4+1]*=br;
        a[n*4+3]=(l.intensity-.1f)/(.1f*l.radius*l.radius);
      }
    }
    if(l.type==4) {
      for(unsigned k=0;k<3;++k) {
        a[n*4+k]=l.customAttenuation[k];
        spots[n*4+k]=lightDirection[k];
      }
      spots[n*4+3]=l.customAttenuation[3];
      positions[n*4+3]=l.customAttenuation[4];
      a[n*4+3]=l.customAttenuation[5];
    }
    ++n;
  }
  glUniform3fv(glGetUniformLocation(program,"roomAmbient"),1,ambient.data());
  glUniform1i(glGetUniformLocation(program,"roomLightCount"),int(n));
  glUniform4fv(glGetUniformLocation(program,"roomLightPosition"),8,positions.data());
  glUniform4fv(glGetUniformLocation(program,"roomLightColor"),8,colors.data());
  glUniform4fv(glGetUniformLocation(program,"roomLightAttenuation"),8,attenuations.data());
  glUniform4fv(glGetUniformLocation(program,"roomLightSpot"),8,spots.data());
}
} // namespace re4
