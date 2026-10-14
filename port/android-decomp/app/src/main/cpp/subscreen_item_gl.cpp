#include "subscreen_item_gl.h"
#include <mutex>
#include "room_lighting_gl.h"
#include "render_state_gl.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace re4 {
namespace {
std::mutex viewportMutex;
std::array<float,4> itemViewport{100.f/640,110.f/480,440.f/640,260.f/480};
}
void SetSubscreenItemViewport(const std::array<float,4> &bounds) {
  for(auto value:bounds)if(!std::isfinite(value))return;
  const float left=std::clamp(bounds[0],0.f,.99f),top=std::clamp(bounds[1],0.f,.99f);
  const float width=std::clamp(bounds[2],.001f,1-left),height=std::clamp(bounds[3],.001f,1-top);
  const std::lock_guard<std::mutex> lock(viewportMutex);
  itemViewport={left,top,width,height};
}
namespace {
GLuint program{}, vao{}, vbo{};
GLuint frameProgram{},frameVao{},frameVbo{};
struct FrameDraw {GLuint texture{};unsigned blend{};std::array<float,4> color{};};
std::vector<FrameDraw> frameDraws;
std::vector<GLuint> frameTextures;
constexpr float pickupHeight=352; // CORE_EXAMINE_ID borders at +/-176.
GLuint backgroundFbo{},backgroundImage{};
int backgroundWidth{},backgroundHeight{};
std::vector<GLuint> textures;
std::vector<GLuint> specularTextures;
std::vector<GLuint> specularOwners;
std::vector<SubscreenSpecular> specularMaterials;
std::vector<SceneDraw> draws;
std::array<float,3> angles{};
std::shared_ptr<const RoomLighting> lights;
float distance{};
float modelRadius{};
float sourceDistance{},sourceCameraY{};
double spin{};
unsigned itemId{};
unsigned rotationAxis{};
bool examining{};
float orbitYaw{}, orbitPitch{};
GLuint compile(GLenum type, const std::string &source) {
  const auto shader=glCreateShader(type);const auto *code=source.c_str();
  glShaderSource(shader,1,&code,nullptr);glCompileShader(shader);
  GLint ok{};glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
  if(!ok) {
    char log[1024]{};glGetShaderInfoLog(shader,sizeof log,nullptr,log);
    glDeleteShader(shader);throw std::runtime_error(std::string("ItemExam: ")+log);
  }
  return shader;
}
void createPickupFrame(const SubscreenArt &art) {
  if(art.quads.empty())return;
  const auto vs=compile(GL_VERTEX_SHADER,R"GLSL(#version 300 es
layout(location=0) in vec2 p;layout(location=1) in vec2 uv;
uniform vec2 sourceScale;out vec2 texcoord;
void main(){gl_Position=vec4(p*sourceScale,0,1);texcoord=uv;}
)GLSL");
  const auto fs=compile(GL_FRAGMENT_SHADER,R"GLSL(#version 300 es
precision mediump float;in vec2 texcoord;uniform sampler2D image;
uniform vec4 tint;out vec4 color;
void main(){color=texture(image,texcoord)*tint;if(color.a*255.0<=1.0)discard;}
)GLSL");
  frameProgram=glCreateProgram();glAttachShader(frameProgram,vs);glAttachShader(frameProgram,fs);
  glLinkProgram(frameProgram);glDeleteShader(vs);glDeleteShader(fs);
  GLint linked{};glGetProgramiv(frameProgram,GL_LINK_STATUS,&linked);
  if(!linked)throw std::runtime_error("ItemExam: frame shader link");
  std::vector<float> vertices;
  std::vector<std::pair<unsigned,GLuint>> uploaded;
  glActiveTexture(GL_TEXTURE0);
  const auto parent=[&](const SubscreenQuad &q)->const SubscreenQuad* {
    for(const auto &p:art.quads)if(p.unit==q.parent)return &p;
    return nullptr;
  };
  const auto transform=[](const SubscreenQuad &q,std::array<float,3> p) {
    SmdPlacement placement;placement.position=q.position;placement.scale={1,1,1};
    for(unsigned k=0;k<3;++k)placement.rotation[k]=q.rotation[k]*.017453292519943295f;
    return TransformPoint(PlacementMatrix(placement),p);
  };
  // IDSystem queues children first into the front-inserted OT. The common
  // quad pass therefore draws these authored units in reverse table order.
  for(auto i=art.quads.rbegin();i!=art.quads.rend();++i) {
    const auto &q=*i;
    if(q.kind!=0 || q.texture==255 || !(q.flags&8))continue;
    const auto *image=art.texture(q.texture);
    if(!image)throw std::runtime_error("ItemExam: original frame texture missing");
    GLuint texture{};
    for(const auto &entry:uploaded)if(entry.first==q.texture)texture=entry.second;
    // The five referenced original textures are shared by their units.
    // Nothing is decoded or uploaded on a repeated draw.
    if(!texture) {
      glGenTextures(1,&texture);frameTextures.push_back(texture);uploaded.emplace_back(q.texture,texture);
      glBindTexture(GL_TEXTURE_2D,texture);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,GLsizei(image->width),GLsizei(image->height),0,GL_RGBA,GL_UNSIGNED_BYTE,image->rgba.data());
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }
    auto color=q.color;
    std::vector<const SubscreenQuad*> chain;
    for(const auto *p=parent(q);p;p=parent(*p)) {
      if(chain.size()>=art.quads.size())throw std::runtime_error("ItemExam: cyclic frame parents");
      chain.push_back(p);
    }
    std::array<float,4> inherited{255,255,255,255};
    for(auto p=chain.rbegin();p!=chain.rend();++p)
      for(unsigned k=0;k<4;++k)inherited[k]=float(unsigned((*p)->color[k]*inherited[k]/255));
    for(unsigned k=0;k<4;++k)color[k]=float(unsigned(color[k]*inherited[k]/255))/255;
    frameDraws.push_back({texture,q.blend,color});
    constexpr unsigned corners[]{0,1,2,0,2,3};
    constexpr float uv[]{0,0,1,0,1,1,0,1};
    for(unsigned corner:corners) {
      auto p=transform(q,{q.vertices[corner*2],q.vertices[corner*2+1],0});
      for(const auto *group:chain)if(group->kind==1)p=transform(*group,p);else break;
      vertices.insert(vertices.end(),{p[0],p[1],uv[corner*2],uv[corner*2+1]});
    }
  }
  glGenVertexArrays(1,&frameVao);glBindVertexArray(frameVao);glGenBuffers(1,&frameVbo);
  glBindBuffer(GL_ARRAY_BUFFER,frameVbo);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(float)),vertices.data(),GL_STATIC_DRAW);
  for(unsigned k=0;k<2;++k) {
    glEnableVertexAttribArray(k);
    glVertexAttribPointer(k,2,GL_FLOAT,GL_FALSE,4*sizeof(float),reinterpret_cast<void*>(std::uintptr_t(k*2*sizeof(float))));
  }
}
void drawPickupFrame(int width,int height) {
  if(!frameProgram)return;
  const auto scale=std::min(width/640.f,height/pickupHeight);
  glViewport(0,0,width,height);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
  glUseProgram(frameProgram);glBindVertexArray(frameVao);glActiveTexture(GL_TEXTURE0);
  glUniform1i(glGetUniformLocation(frameProgram,"image"),0);
  glUniform2f(glGetUniformLocation(frameProgram,"sourceScale"),2*scale/width,2*scale/height);
  constexpr GLenum src[]{GL_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE,GL_DST_COLOR,GL_DST_COLOR};
  constexpr GLenum dst[]{GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE,GL_ONE,GL_ZERO};
  for(std::size_t i=0;i<frameDraws.size();++i) {
    const auto &draw=frameDraws[i];const auto blend=std::min(draw.blend,4u);
    glBlendFunc(src[blend],dst[blend]);glBindTexture(GL_TEXTURE_2D,draw.texture);
    glUniform4fv(glGetUniformLocation(frameProgram,"tint"),1,draw.color.data());
    glDrawArrays(GL_TRIANGLES,GLint(i*6),6);
  }
  glDisable(GL_BLEND);glDepthMask(GL_TRUE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
}
} // namespace
void ForgetSubscreenItemGl() {
  frameProgram=frameVao=frameVbo=0;frameDraws.clear();frameTextures.clear();
  program=vao=vbo=backgroundFbo=backgroundImage=0;backgroundWidth=backgroundHeight=0;textures.clear();specularTextures.clear();specularOwners.clear();specularMaterials.clear();draws.clear();lights.reset();
}
void DestroySubscreenItemGl() {
  if(frameProgram)glDeleteProgram(frameProgram);
  if(frameVao)glDeleteVertexArrays(1,&frameVao);
  if(frameVbo)glDeleteBuffers(1,&frameVbo);
  if(!frameTextures.empty())glDeleteTextures(GLsizei(frameTextures.size()),frameTextures.data());
  if(backgroundFbo)glDeleteFramebuffers(1,&backgroundFbo);
  if(backgroundImage)glDeleteTextures(1,&backgroundImage);
  if(!textures.empty())glDeleteTextures(GLsizei(textures.size()),textures.data());
  if(!specularOwners.empty())glDeleteTextures(GLsizei(specularOwners.size()),specularOwners.data());
  if(vbo)glDeleteBuffers(1,&vbo);
  if(vao)glDeleteVertexArrays(1,&vao);
  if(program)glDeleteProgram(program);
  ForgetSubscreenItemGl();
}
void CreateSubscreenItemGl(const StaticScene &scene,
    const std::array<float,3> &rotationDegrees,float scale,unsigned id,
    std::shared_ptr<const RoomLighting> lighting,unsigned spinAxis,bool examine,
    const std::array<float,2> &frameY,const std::vector<SubscreenSpecular> &specular,
    const SubscreenArt &pickupFrame) {
  DestroySubscreenItemGl();
  if(scene.vertices.empty() || scene.vertices.size()%3 ||
     scene.normals.size()!=scene.vertices.size() || scene.uv.size()!=scene.vertices.size()/3*2)
    throw std::runtime_error("ItemExam: Mesh layout");
  if(!specular.empty() && specular.size()!=scene.draws.size())throw std::runtime_error("ItemExam: specular draw layout");
  try {
    const auto vs=RoomAtmosphereShader(R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 p;layout(location=1) in vec3 n;layout(location=2) in vec2 uv;layout(location=3) in vec4 rgba;
uniform mat4 projection,model,view;uniform float specularScale;
uniform int vertexColors;
out vec2 texcoord,specularUV;out vec4 lightColor;
void main(){vec3 world=(model*vec4(p,1)).xyz;vec3 normal=mat3(model)*n;
lightColor=litRoomColor(rgba,world,normal,vertexColors);texcoord=uv;
vec3 viewNormal=mat3(view)*normal;specularUV=vec2(0.5)+specularScale*vec2(viewNormal.x,-viewNormal.y);
gl_Position=projection*view*vec4(world,1);}
)GLSL",true);
    const std::string fs=R"GLSL(#version 300 es
precision highp float;
in vec2 texcoord,specularUV;in vec4 lightColor;uniform sampler2D image,specularImage,alphaImage;
uniform vec3 specularColor;uniform int specularType;uniform vec2 roomTevScale;
uniform vec3 materialRGB;uniform int textured,alphaTest,alphaReference;out vec4 color;
void main(){vec4 t=textured!=0?texture(image,texcoord):vec4(1);
// ModelRender's base TEV consumes RGB only. Alpha comes from the separate
// material mask/raster channel, even when the base TPL stores spare alpha.
float a=lightColor.a;
if(alphaTest!=0){a*=texture(alphaImage,texcoord).a;if(floor(a*255.0+0.5)<=float(alphaReference))discard;}
color=vec4(t.rgb*lightColor.rgb*materialRGB,a);
if(specularType>=0){vec4 s=texture(specularImage,specularUV);
if(specularType==0)color.rgb+=s.rgb*specularColor*lightColor.rgb;
else if(specularType==1)color.rgb+=s.a*color.rgb;}
color.rgb=clamp(color.rgb*roomTevScale.x,vec3(0),vec3(1));}
)GLSL";
    const auto vertex=compile(GL_VERTEX_SHADER,vs),fragment=compile(GL_FRAGMENT_SHADER,fs);
    program=glCreateProgram();glAttachShader(program,vertex);glAttachShader(program,fragment);
    glLinkProgram(program);glDeleteShader(vertex);glDeleteShader(fragment);
    GLint linked{};glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if(!linked)throw std::runtime_error("ItemExam: shader link");
    std::array<float,3> center{},minimum{},maximum{};float diagonal{};
    // BuildRoomModelScene retains model geometry, while room bounds are filled
    // by its room caller. An isolated IDM must derive its own actual bounds.
    for(unsigned k=0;k<3;++k)minimum[k]=maximum[k]=scene.vertices[k];
    for(std::size_t i=0;i<scene.vertices.size();++i) {
      if(!std::isfinite(scene.vertices[i]))throw std::runtime_error("ItemExam: nonfinite vertex");
      minimum[i%3]=std::min(minimum[i%3],scene.vertices[i]);
      maximum[i%3]=std::max(maximum[i%3],scene.vertices[i]);
    }
    for(unsigned k=0;k<3;++k) {
      center[k]=(minimum[k]+maximum[k])*.5f;
      const auto d=maximum[k]-minimum[k];diagonal+=d*d;
      angles[k]=rotationDegrees[k]*.017453292519943295f;
    }
    // ItemExamine::move fits the model's HALF-bound diagonal into the angular
    // interval of CORE_EXAMINE_ID units F2/F3, not the full screen's30deg.
    // The Android dialog centres that source-framed view in its own viewport.
    const auto radius=std::sqrt(diagonal)*.5f;
    modelRadius=radius;
    const auto screenDistance=240/std::tan(15.f*.017453292519943295f);
    const auto difference=std::abs(std::atan2(frameY[0],screenDistance)-std::atan2(frameY[1],screenDistance));
    if(!(difference>0 && difference<3.1415927f))throw std::runtime_error("ItemExam: invalid source frame");
    const auto r=radius/std::max(scale,.01f)*std::tan((3.1415927f-difference)*.5f);
    const auto cameraAngle=std::atan2((frameY[0]+frameY[1])*.5f,screenDistance);
    sourceDistance=r*std::cos(cameraAngle);sourceCameraY=-r*std::sin(cameraAngle);
    distance=sourceDistance;
    // The controller presentation can orbit any model. Keep its entire bound
    // within this adapted viewport, including unusually large source scales.
    distance=std::max(distance,radius/std::sin(15.f*.017453292519943295f)*1.04f);
    if(!(distance>0))throw std::runtime_error("ItemExam: empty model bound");
    std::vector<float> vertices;vertices.reserve(scene.vertices.size()/3*12);
    for(std::size_t i=0;i<scene.vertices.size()/3;++i) {
      for(unsigned k=0;k<3;++k)vertices.push_back(scene.vertices[i*3+k]-center[k]);
      vertices.insert(vertices.end(),scene.normals.begin()+i*3,scene.normals.begin()+i*3+3);
      vertices.insert(vertices.end(),scene.uv.begin()+i*2,scene.uv.begin()+i*2+2);
      for(unsigned k=0;k<4;++k)vertices.push_back(scene.colors.size()==scene.vertices.size()/3*4?scene.colors[i*4+k]/255.f:1.f);
    }
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(float)),vertices.data(),GL_STATIC_DRAW);
    for(unsigned attribute=0;attribute<4;++attribute) {
      glEnableVertexAttribArray(attribute);
      glVertexAttribPointer(attribute,attribute==3?4:attribute==2?2:3,GL_FLOAT,GL_FALSE,12*sizeof(float),reinterpret_cast<void*>(std::uintptr_t((attribute==0?0:attribute==1?3:attribute==2?6:8)*sizeof(float))));
    }
    textures.resize(scene.textures.size());
    if(!textures.empty())glGenTextures(GLsizei(textures.size()),textures.data());
    for(std::size_t i=0;i<textures.size();++i) {
      const auto &t=scene.textures[i];glBindTexture(GL_TEXTURE_2D,textures[i]);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,GLsizei(t.width),GLsizei(t.height),0,GL_RGBA,GL_UNSIGNED_BYTE,t.rgba.data());
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,t.wrapS?GL_REPEAT:GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,t.wrapT?GL_REPEAT:GL_CLAMP_TO_EDGE);
    }
    draws=scene.draws;lights=std::move(lighting);spin=0;itemId=id;
    specularMaterials.resize(specular.size());specularTextures.resize(specular.size());
    for(std::size_t i=0;i<specular.size();++i) {
      auto &parameters=specularMaterials[i];const auto &source=specular[i];
      parameters.enabled=source.enabled;parameters.bump=source.bump;parameters.type=source.type;
      parameters.scale=source.scale;parameters.rgb=source.rgb;
      if(!source.enabled)continue;
      const auto &t=specular[i].image;
      if(!t.width || !t.height || t.rgba.size()!=std::size_t(t.width)*t.height*4)throw std::runtime_error("ItemExam: specular image");
      // A BIN may contain hundreds of GX primitives with the same material.
      // They share one immutable original lookup texture, not one GPU copy
      // per primitive. Keep deletion ownership separate from draw references.
      for(std::size_t j=0;j<i;++j)if(specularTextures[j] &&
          specular[j].image.width==t.width && specular[j].image.height==t.height &&
          specular[j].image.rgba==t.rgba) {specularTextures[i]=specularTextures[j];break;}
      if(specularTextures[i])continue;
      glGenTextures(1,&specularTextures[i]);glBindTexture(GL_TEXTURE_2D,specularTextures[i]);
      specularOwners.push_back(specularTextures[i]);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,GLsizei(t.width),GLsizei(t.height),0,GL_RGBA,GL_UNSIGNED_BYTE,t.rgba.data());
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }
    rotationAxis=spinAxis;examining=examine;orbitYaw=orbitPitch=0;
    if(!examining)createPickupFrame(pickupFrame);
    glBindVertexArray(0);
  }catch(...) {DestroySubscreenItemGl();throw;}
}
void DrawSubscreenItemGl(int width,int height,double seconds,float lookX,float lookY) {
  if(!program)return;
  ScopedRenderStateGl state;
  // ItemExamine::move: PI/60 per original30Hz tick (PI/120 for item0x93).
  const auto ticks=float(std::clamp(seconds,0.,.05)*30);
  if(examining) {
    // ItemExamine screen2 camera rates/limits, with the universal stick scaled
    // to the original signed8-bit Key.stick values.
    if(std::isfinite(lookX))orbitYaw+=std::clamp(lookX,-1.f,1.f)*127*-.06666667f*.017453292f*ticks;
    if(std::isfinite(lookY))orbitPitch=std::clamp(orbitPitch+std::clamp(lookY,-1.f,1.f)*127*.05f*.017453292f*ticks,-1.0471976f,.3926991f);
  }else spin+=ticks*3.1415927/(itemId==0x93?120:60);
  std::array<float,4> bounds;
  { const std::lock_guard<std::mutex> lock(viewportMutex);bounds=itemViewport; }
  const int x=int(std::lround(bounds[0]*width)),y=int(std::lround((1-bounds[1]-bounds[3])*height));
  const int w=std::max(1,std::min(width-x,int(std::lround(bounds[2]*width)))),h=std::max(1,std::min(height-y,int(std::lround(bounds[3]*height))));
  const bool pickup=!examining && bounds==std::array<float,4>{0,0,1,1};
  const auto viewDistance=(pickup?sourceDistance:distance)*std::max(1.f,float(h)/w);
  const auto cameraY=pickup?sourceCameraY:0.f;
  // examine.cpp::store freezes a half-resolution EFB. bio4Draw then draws
  // CORE_EXAMINE_ID in OT0x13, the model in0x14 and Message text in0x15.
  if(pickup) {
    GLint read{},draw{};glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    glDisable(GL_SCISSOR_TEST);
    if(!backgroundFbo) {
      glActiveTexture(GL_TEXTURE0);glGenTextures(1,&backgroundImage);glBindTexture(GL_TEXTURE_2D,backgroundImage);
      // getEFB halves SCR_W/SCR_H. Preserve that ratio at the Android
      // framebuffer resolution instead of enlarging a fixed 320x224 image.
      backgroundWidth=std::max(1,width/2);backgroundHeight=std::max(1,height/2);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,backgroundWidth,backgroundHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
      glGenFramebuffers(1,&backgroundFbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,backgroundFbo);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,backgroundImage,0);
      if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("ItemExam: EFB background");
      glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(draw));
      glBlitFramebuffer(0,0,width,height,0,0,backgroundWidth,backgroundHeight,GL_COLOR_BUFFER_BIT,GL_LINEAR);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER,backgroundFbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));
    glBlitFramebuffer(0,0,backgroundWidth,backgroundHeight,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));
    drawPickupFrame(width,height);
  }
  glViewport(x,y,w,h);glEnable(GL_SCISSOR_TEST);glScissor(x,y,w,h);
  glClearColor(.035f,.035f,.025f,1);glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT|(pickup?0:GL_COLOR_BUFFER_BIT));
  glDisable(GL_SCISSOR_TEST);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glFrontFace(GL_CCW);
  glUseProgram(program);glBindVertexArray(vao);
  const auto oldAtmosphere=RoomAtmosphere();RoomAtmosphere()={};RoomAtmosphere().data=lights;
  const auto cy=std::cos(orbitYaw),sy=std::sin(orbitYaw),cp=std::cos(orbitPitch),sp=std::sin(orbitPitch);
  const std::array<float,3> right{cy,0,-sy},up{-sy*sp,cp,-cy*sp},back{sy*cp,sp,cy*cp};
  for(unsigned k=0;k<3;++k)RoomAtmosphere().eye[k]=back[k]*viewDistance+(k==1?cameraY:0);
  RoomAtmosphere().right=right;RoomAtmosphere().up=up;
  for(unsigned k=0;k<3;++k)RoomAtmosphere().look[k]=-back[k];
  SmdPlacement placement;placement.rotation=angles;placement.scale={1,1,1};
  const auto base=PlacementMatrix(placement);
  SmdPlacement turn;turn.rotation={0,float(spin),0};turn.scale={1,1,1};
  const auto rotation=PlacementMatrix(turn);std::array<float,16> model{};model[15]=1;
  // Original axis_id/scr 0 rotates worldY; 1 rotates the current modelY axis.
  for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)
    for(unsigned k=0;k<3;++k)model[c*4+r]+=rotationAxis==1?base[r*4+k]*rotation[k*4+c]:rotation[r*4+k]*base[k*4+c];
  const float view[16]={right[0],up[0],back[0],0,right[1],up[1],back[1],0,right[2],up[2],back[2],0,0,-cameraY,-viewDistance,1};
  const auto f=1/std::tan(15.f*.017453292519943295f);
  const auto sourceScale=pickup?std::min(w/640.f,h/pickupHeight):0.f;
  const auto fx=pickup?f*480*sourceScale/w:f/(float(w)/h);
  const auto fy=pickup?f*480*sourceScale/h:f;
  // Fit depth to the rotated model's bounding sphere. The former near
  // distance0.001 left almost no depth precision on the gem's close facets.
  // All authored geometry fits for every spin angle, without polygon bias.
  const auto near=std::max(viewDistance-modelRadius*1.05f,.1f),far=viewDistance+modelRadius*1.05f;
  const float projection[16]={fx,0,0,0,0,fy,0,0,0,0,-(far+near)/(far-near),-1,0,0,-2*far*near/(far-near),0};
  glUniformMatrix4fv(glGetUniformLocation(program,"projection"),1,GL_FALSE,projection);
  glUniformMatrix4fv(glGetUniformLocation(program,"model"),1,GL_FALSE,model.data());
  glUniformMatrix4fv(glGetUniformLocation(program,"view"),1,GL_FALSE,view);glUniform1i(glGetUniformLocation(program,"image"),0);
  glUniform1i(glGetUniformLocation(program,"specularImage"),1);
  glUniform1i(glGetUniformLocation(program,"alphaImage"),2);
  BindRoomFog(program);BindRoomLights(program,0x20,0xffffffffu,{0,0,0},distance);
  glActiveTexture(GL_TEXTURE0);
  for(std::size_t i=0;i<draws.size();++i) {
    const auto &draw=draws[i];
    const auto *spec=i<specularMaterials.size() && specularMaterials[i].enabled?&specularMaterials[i]:nullptr;
    glUniform1i(glGetUniformLocation(program,"specularType"),spec?int(spec->type):-1);
    glUniform1f(glGetUniformLocation(program,"specularScale"),spec?spec->scale:0);
    if(spec)glUniform3fv(glGetUniformLocation(program,"specularColor"),1,spec->rgb.data());
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,spec?specularTextures[i]:0);glActiveTexture(GL_TEXTURE0);
    glBindSampler(0,0);glBindSampler(1,0);glBindSampler(2,0);
    const bool textured=draw.texture>=0 && std::size_t(draw.texture)<textures.size();
    glUniform1i(glGetUniformLocation(program,"textured"),textured);
    glUniform1i(glGetUniformLocation(program,"alphaReference"),draw.alphaTest?int(draw.alphaReference):0);
    const bool masked=draw.alphaTest && draw.alphaTexture>=0 && std::size_t(draw.alphaTexture)<textures.size();
    glUniform1i(glGetUniformLocation(program,"alphaTest"),masked);
    glUniform1i(glGetUniformLocation(program,"vertexColors"),draw.vertexColors);
    glUniform3f(glGetUniformLocation(program,"materialRGB"),draw.materialRGB[0]/255.f,draw.materialRGB[1]/255.f,draw.materialRGB[2]/255.f);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,masked?textures[draw.alphaTexture]:0);glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,textured?textures[draw.texture]:0);
    if(draw.blendMode<4){constexpr GLenum source[]{GL_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE,GL_DST_COLOR},destination[]{GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE,GL_ONE};
      glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(source[draw.blendMode],destination[draw.blendMode]);}else glDisable(GL_BLEND);
    glDepthFunc(draw.zMode==2?GL_ALWAYS:GL_LEQUAL);
    glDepthMask(draw.zMode==0?GL_TRUE:GL_FALSE);
    if(draw.cullMode==2)glDisable(GL_CULL_FACE);else{glEnable(GL_CULL_FACE);glCullFace(draw.cullMode==0?GL_BACK:GL_FRONT);}
    glDrawArrays(GL_TRIANGLES,GLint(draw.first),GLsizei(draw.count));
  }
  RoomAtmosphere()=oldAtmosphere;RoomLightBindings().valid=false;
  glDisable(GL_BLEND);glDepthMask(GL_TRUE);glActiveTexture(GL_TEXTURE0);glBindVertexArray(0);glViewport(0,0,width,height);
}
} // namespace re4
