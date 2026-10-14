#include "graphics_options_gl.h"
#include "world_effects_gl.h"
#include "environment_gl.h"
#include "source_glow_gl.h"
#include "room_area_effects.h"
#include "room_lighting_gl.h"
#include "cinematic_frame.h"
#include "ports/motion64/runtime.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <map>
#include <set>
#include <stdexcept>
namespace re4 {
namespace {
WorldEffectsData data;
std::unique_ptr<WorldEffectsSimulation> simulation;
GLuint program{}, vao{}, vbo{};
std::vector<std::vector<GLuint>> images;
struct BillboardGpu {
  GLuint program{},vao{},vbo{};
  std::vector<std::vector<GLuint>> images;
};
BillboardGpu muzzleGpu,enemyGpu,eventGpu,trapGpu;
BillboardGpu itemGpu;
BillboardGpu roomAreaGpu;
BillboardGpu roomHitGpu,villageBottleCoreGpu,villageBottleTrailGpu,projectileGpu;
GLuint lineProgram{},lineVao{},lineVbo{};
GLuint lensDepth{},lensDepthFbo{},lensResult{},lensResultFbo{},lensProgram{},lensVao{};
int lensWidth{},lensHeight{};
GLenum lensDepthFormat{};
struct LensHistory {
  std::array<float,3> screen{};
  unsigned age{};
  unsigned delay{};
  float hide{},displayHide{};
  bool initialized{};
};
std::map<std::pair<std::uint64_t,std::uint64_t>,LensHistory> lensHistory;
std::uint64_t trapSignature{};
struct State {
  GLint program{}, vao{}, buffer{}, active{}, texture{}, texture1{}, texture2{}, sampler{}, sampler1{}, sampler2{}, depthFunc{},
      blendEquationRGB{}, blendEquationAlpha{}, blendSrcRGB{}, blendDstRGB{},
      blendSrcAlpha{}, blendDstAlpha{};
  GLboolean depth{}, depthMask{}, blend{}, cull{}, colorMask[4]{};
  State() {
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture1);glGetIntegerv(GL_SAMPLER_BINDING,&sampler1);
    glActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2);
    glGetIntegerv(GL_SAMPLER_BINDING, &sampler2);
    glActiveTexture(GLenum(active));
    depth = glIsEnabled(GL_DEPTH_TEST);
    blend = glIsEnabled(GL_BLEND);
    cull = glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRGB);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
  }
  ~State() {
    glUseProgram(GLuint(program));
    glBindVertexArray(GLuint(vao));
    glBindBuffer(GL_ARRAY_BUFFER, GLuint(buffer));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, GLuint(texture));
    glBindSampler(0,GLuint(sampler));
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,GLuint(texture1));glBindSampler(1,GLuint(sampler1));
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,GLuint(texture2));
    glBindSampler(2,GLuint(sampler2));
    glActiveTexture(GLenum(active));
    if (depth)
      glEnable(GL_DEPTH_TEST);
    else
      glDisable(GL_DEPTH_TEST);
    if (blend)
      glEnable(GL_BLEND);
    else
      glDisable(GL_BLEND);
    if (cull)
      glEnable(GL_CULL_FACE);
    else
      glDisable(GL_CULL_FACE);
    glDepthFunc(GLenum(depthFunc));
    glDepthMask(depthMask);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    glBlendEquationSeparate(GLenum(blendEquationRGB),
                            GLenum(blendEquationAlpha));
    glBlendFuncSeparate(GLenum(blendSrcRGB), GLenum(blendDstRGB),
                        GLenum(blendSrcAlpha), GLenum(blendDstAlpha));
  }
};
GLuint compile(GLenum type, const char *source) {
  GLuint shader = glCreateShader(type);
  if (!shader)
    throw std::runtime_error("Room effect: Could not create shader");
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024]{};
    glGetShaderInfoLog(shader, sizeof log, nullptr, log);
    glDeleteShader(shader);
    throw std::runtime_error(log);
  }
  return shader;
}
void deleteProgram(GLuint name) {
  if(!name)return;
  GLint current{};glGetIntegerv(GL_CURRENT_PROGRAM,&current);
  if(GLuint(current)==name)glUseProgram(0);
  glDeleteProgram(name);
}
void texture(GLuint name, const Texture &image) {
  glBindTexture(GL_TEXTURE_2D, name);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
}
} // namespace
bool TriggerWorldEffectsGl(unsigned objectId,
                           const std::array<float, 3> &position,
                           const std::array<float, 3> &rotation,
                           std::uint64_t eventSerial) {
  return simulation &&
         simulation->trigger(objectId, position, rotation, eventSerial);
}
void ForgetWorldEffectsGl() {
  program = vao = vbo = 0;
  images.clear();
  simulation.reset();
  data = {};
}
void DestroyWorldEffectsGl() {
  deleteProgram(program);
  if (vao)
    glDeleteVertexArrays(1, &vao);
  if (vbo)
    glDeleteBuffers(1, &vbo);
  for (const auto &set : images)
    if (!set.empty())
      glDeleteTextures(GLsizei(set.size()), set.data());
  ForgetWorldEffectsGl();
}
void CreateWorldEffectsGl(const WorldEffectsData &source) {
  ForgetWorldEffectsGl();
  data =
      source; // GPU owner has independent resource lifetimes from worker room.
  simulation = std::make_unique<WorldEffectsSimulation>(data);
  if (data.sprites.targets.empty())
    return;
  State previous;
  try {
    GLint maximum;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    std::size_t budget = 0;
    if (data.sprites.animations.size() > 32)
      throw std::runtime_error("Room effect: GPU animation budget");
    for (const auto &set : data.sprites.animations) {
      if (set.images.empty() || set.images.size() > 128)
        throw std::runtime_error("Room effect: GPU image budget");
      for (const auto &image : set.images) {
        if (!image.width || !image.height || image.width > 1024 ||
            image.height > 1024 || image.width > unsigned(maximum) ||
            image.height > unsigned(maximum) ||
            image.rgba.size() != std::size_t(image.width) * image.height * 4 ||
            image.rgba.size() > 32 * 1024 * 1024 - budget)
          throw std::runtime_error("Room effect: GPU texture budget");
        budget += image.rgba.size();
      }
    }
    GLuint vs =
        compile(GL_VERTEX_SHADER,
                "#version 300 es\nlayout(location=0) in vec3 p;"
                "layout(location=1) in vec2 uv;uniform mat4 mvp;out vec2 st;"
                "void main(){st=uv;gl_Position=mvp*vec4(p,1.0);}");
    GLuint fs = 0;
    try {
      const auto fragment = RoomAtmosphereShader(
          "#version 300 es\nprecision highp float;in vec2 st;out vec4 c;"
          "uniform sampler2D image;uniform vec4 rgba;uniform vec2 scaleColor;"
          "uniform bool effectFog;"
          "uniform vec2 repeatUV,scrollUV;uniform int tiled;uniform bvec2 "
          "flipUV;"
          "void main(){vec2 uv=st;if(tiled!=0){uv=fract(uv*repeatUV-scrollUV);"
          "if(flipUV.x)uv.x=1.0-uv.x;if(flipUV.y)uv.y=1.0-uv.y;}"
          "c=texture(image,uv)*rgba;c=clamp(c*vec4(vec3(scaleColor.x),"
          "scaleColor.y),0.0,1.0);if(c.a<=1.0/255.0)discard;"
          "if(effectFog)c=fogRoomColor(c,vec3(0.0));}", false);
      fs = compile(GL_FRAGMENT_SHADER, fragment.c_str());
    } catch (...) {
      glDeleteShader(vs);
      throw;
    }
    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
      char log[1024]{};
      glGetProgramInfoLog(program, sizeof log, nullptr, log);
      throw std::runtime_error(log);
    }
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    if (!vao || !vbo)
      throw std::runtime_error("Room effect: GPU buffers are missing");
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          reinterpret_cast<const void *>(3 * sizeof(float)));
    glActiveTexture(GL_TEXTURE0);
    images.resize(data.sprites.animations.size());
    for (unsigned i = 0; i < images.size(); ++i) {
      const auto &set = data.sprites.animations[i].images;
      images[i].resize(set.size());
      glGenTextures(GLsizei(set.size()), images[i].data());
      for (unsigned k = 0; k < set.size(); ++k)
        texture(images[i][k], set[k]);
    }
    if (glGetError() != GL_NO_ERROR)
      throw std::runtime_error("Room effect: GPU upload failed");
  } catch (...) {
    DestroyWorldEffectsGl();
    throw;
  }
}
void DrawWorldEffectsGl(const float *matrix, const std::array<float, 3> &center,
                        float scale, const WorldEffectCamera &camera,
                        double frames) {
  if (!simulation)
    return;
  if (!matrix || !std::isfinite(frames) || frames < 0 || frames > 1e9 ||
      !std::isfinite(scale) || scale <= 0)
    throw std::runtime_error("Room effect: invalid tick/scale");
  for (unsigned k = 0; k < 16; ++k)
    if (!std::isfinite(matrix[k]))
      throw std::runtime_error("Room effect: invalid matrix");
  for (auto x : center)
    if (!std::isfinite(x))
      throw std::runtime_error("Room effect: invalid center");
  auto desired = std::uint64_t(std::floor(frames + 1e-9)) + 200;
  if (desired < simulation->state().frames)
    simulation = std::make_unique<WorldEffectsSimulation>(data);
  // Scene clock normally advances by 0/1 ticks; a lost or corrupt timestamp
  // must not stall a render thread for millions of simulation calls.
  if (desired - simulation->state().frames > 36000)
    throw std::runtime_error("Room effect: Tick jump exceeds budget");
  while (simulation->state().frames < desired)
    simulation->tick(&camera);
  if (!program)
    return;
  auto quads = WorldEffectQuads(data, simulation->state(), camera);
  if (quads.size() > 512)
    throw std::runtime_error("Room effect: GPU sprite budget");
  State previous;
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  // Original CommonStateSet writes no Z; these unflagged world layers also
  // keep destination alpha intact (RGB only).
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
  glUseProgram(program);
  BindRoomFog(program);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(glGetUniformLocation(program, "image"), 0);
  glUniformMatrix4fv(glGetUniformLocation(program, "mvp"), 1, GL_FALSE, matrix);
  constexpr GLenum src[6]{GL_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE,
                          GL_DST_COLOR, GL_DST_COLOR, GL_SRC_ALPHA};
  constexpr GLenum dst[6]{
      GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE, GL_ZERO,
      GL_ONE_MINUS_DST_COLOR};
  constexpr unsigned corners[6]{0, 1, 2, 0, 2, 3};
  for (const auto &q : quads) {
    if (q.blend >= 6)
      throw std::runtime_error("Room effect: Blend index");
    float vertices[30]{};
    for (unsigned k = 0; k < 6; ++k) {
      unsigned c = corners[k];
      for (unsigned axis = 0; axis < 3; ++axis)
        vertices[k * 5 + axis] = (q.position[c][axis] - center[axis]) * scale;
      vertices[k * 5 + 3] = q.uv[c][0];
      vertices[k * 5 + 4] = q.uv[c][1];
    }
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STREAM_DRAW);
    glBindTexture(GL_TEXTURE_2D, images.at(q.animation).at(q.image));
    glUniform4fv(glGetUniformLocation(program, "rgba"), 1, q.color.data());
    glUniform2f(glGetUniformLocation(program, "scaleColor"), q.rgbScale,
                q.alphaScale);
    glUniform2fv(glGetUniformLocation(program, "repeatUV"), 1,
                 q.repeats.data());
    glUniform2fv(glGetUniformLocation(program, "scrollUV"), 1, q.scroll.data());
    glUniform1i(glGetUniformLocation(program, "tiled"), q.tiled ? 1 : 0);
    glUniform2i(glGetUniformLocation(program, "flipUV"), q.flipS ? 1 : 0,
                q.flipT ? 1 : 0);
    // EspCommonTrans / Esp08_Trans inherit LightMgr's GX fog. Their
    // framebuffer-copy variants explicitly disable it and restore it later.
    glUniform1i(glGetUniformLocation(program, "effectFog"),
                q.type != 24 && q.type != 15 && !(q.flags & 0x2000));
    glBlendFunc(src[q.blend], dst[q.blend]);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }
  if (glGetError() != GL_NO_ERROR)
    throw std::runtime_error("Room effect: GPU drawing failed");
}
void forgetBillboardGpu(BillboardGpu &gpu) { gpu={}; }
void destroyBillboardGpu(BillboardGpu &gpu) {
  deleteProgram(gpu.program);
  if(gpu.vao)glDeleteVertexArrays(1,&gpu.vao);
  if(gpu.vbo)glDeleteBuffers(1,&gpu.vbo);
  for(const auto &set:gpu.images)if(!set.empty())glDeleteTextures(GLsizei(set.size()),set.data());
  forgetBillboardGpu(gpu);
}
void ForgetMuzzleEffectsGl() {forgetBillboardGpu(muzzleGpu);}
void DestroyMuzzleEffectsGl() {destroyBillboardGpu(muzzleGpu);}
void ForgetEnemyEffectsGl() {forgetBillboardGpu(enemyGpu);}
void DestroyEnemyEffectsGl() {destroyBillboardGpu(enemyGpu);}
void ForgetEventEffectsGl() {forgetBillboardGpu(eventGpu);}
void DestroyEventEffectsGl() {destroyBillboardGpu(eventGpu);}
void ForgetItemBillboardsGl() {forgetBillboardGpu(itemGpu);}
void DestroyItemBillboardsGl() {destroyBillboardGpu(itemGpu);}
void createBillboardGpu(BillboardGpu &gpu,const WorldEffectsData &source,unsigned animationLimit) {
  destroyBillboardGpu(gpu);
  State previous;
  auto &muzzleProgram=gpu.program,&muzzleVao=gpu.vao,&muzzleVbo=gpu.vbo;
  auto &muzzleImages=gpu.images;
  try {
    if(source.sprites.animations.size()>animationLimit) throw std::runtime_error("Muzzle flash: GPU animation budget");
    const auto vs=compile(GL_VERTEX_SHADER,
      "#version 300 es\nlayout(location=0) in vec3 p;layout(location=1) in vec2 uv;uniform mat4 mvp;out vec2 st;void main(){st=uv;gl_Position=mvp*vec4(p,1.0);}");
    GLuint fs{};
    try {const auto fragment=RoomAtmosphereShader(R"GLSL(#version 300 es
precision highp float;precision highp int;
in vec2 st;out vec4 c;
uniform sampler2D image,frameImage,maskImage;uniform vec4 rgba;uniform vec2 scaleColor;
uniform vec2 repeatUV,scrollUV,viewport,frameOffset;uniform bvec2 flipUV;
uniform int tiled,refraction;uniform float alphaReference,frameScale;
uniform bool masked,maskStretched;uniform float maskAlphaScale;
uniform int shimmerType,shimmerCoefficient;
uniform bool effectFog;
void main(){
  vec2 uv=st;
  if(tiled!=0){uv=fract(uv*repeatUV-scrollUV);
    if(flipUV.x)uv.x=1.0-uv.x;if(flipUV.y)uv.y=1.0-uv.y;}
  vec4 mask=texture(image,uv);
  if(shimmerType!=0){
    // GX indirect components are A,B,G. This diagonal source matrix uses
    // A and B, with 8-bit inputs and its signed S0.10 coefficient. Exponent1
    // yields the original S17.7 coordinate shift (integer product >>2).
    ivec2 indirect=ivec2(floor(mask.ab*255.0+0.5));
    if(shimmerType==2)indirect-=ivec2(128);
    ivec2 shift=(indirect*shimmerCoefficient)>>2;
    vec2 base=shimmerType==3?vec2(0.0):gl_FragCoord.xy/viewport;
    vec2 delta=vec2(shift)/128.0/vec2(256.0,224.0);
    c=vec4(texture(frameImage,base+vec2(delta.x,-delta.y)).rgb*rgba.rgb,rgba.a);
  }else if(refraction!=0){
    vec2 frameUV=(gl_FragCoord.xy/viewport-vec2(.5))*frameScale+vec2(.5)+frameOffset;
    c=vec4(texture(frameImage,frameUV).rgb*rgba.rgb,mask.a*rgba.a*scaleColor.y);
  }else c=mask*rgba*vec4(vec3(scaleColor.x),scaleColor.y);
  c=clamp(c,0.0,1.0);
  if(masked)c.a=clamp(c.a*texture(maskImage,maskStretched?st:uv).a*maskAlphaScale,0.0,1.0);
  if(c.a<=alphaReference)discard;
  if(effectFog)c=fogRoomColor(c,vec3(0.0));
}
)GLSL",false);fs=compile(GL_FRAGMENT_SHADER,fragment.c_str());}
    catch(...) {glDeleteShader(vs);throw;}
    muzzleProgram=glCreateProgram();glAttachShader(muzzleProgram,vs);glAttachShader(muzzleProgram,fs);
    glLinkProgram(muzzleProgram);glDeleteShader(vs);glDeleteShader(fs);
    GLint linked{};glGetProgramiv(muzzleProgram,GL_LINK_STATUS,&linked);
    if(!linked) throw std::runtime_error("Muzzle flash: GLSL link error");
    glGenVertexArrays(1,&muzzleVao);glGenBuffers(1,&muzzleVbo);
    if(!muzzleVao || !muzzleVbo) throw std::runtime_error("Muzzle flash: GPU buffer");
    glBindVertexArray(muzzleVao);glBindBuffer(GL_ARRAY_BUFFER,muzzleVbo);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),nullptr);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),reinterpret_cast<void*>(3*sizeof(float)));
    glActiveTexture(GL_TEXTURE0);
    GLint maximum{};glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
    std::size_t budget{};muzzleImages.resize(source.sprites.animations.size());
    for(unsigned a=0;a<muzzleImages.size();++a) {
      const auto &textures=source.sprites.animations[a].images;
      if(textures.empty() || textures.size()>128) throw std::runtime_error("Muzzle flash: GPU pattern budget");
      muzzleImages[a].resize(textures.size());glGenTextures(GLsizei(textures.size()),muzzleImages[a].data());
      for(unsigned i=0;i<textures.size();++i) {
        const auto &t=textures[i];
        if(!t.width || !t.height || t.width>unsigned(maximum) || t.height>unsigned(maximum) ||
            t.width>1024 || t.height>1024 || t.rgba.size()!=std::size_t(t.width)*t.height*4 ||
            t.rgba.size()>16*1024*1024-budget) throw std::runtime_error("Muzzle flash: GPU texture budget");
        budget+=t.rgba.size();texture(muzzleImages[a][i],t);
      }
    }
    if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Muzzle flash: GPU-Upload");
  } catch(...) {destroyBillboardGpu(gpu);throw;}
}
void drawBillboardGpu(const BillboardGpu &gpu,const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectQuad> &quads,unsigned quadLimit,
                         bool event=false,bool beforeWorld=false,
                         const std::vector<RoomAreaEffectQuad> *masks=nullptr,
                         const char *owner="Effect") {
  const auto muzzleProgram=gpu.program,muzzleVao=gpu.vao,muzzleVbo=gpu.vbo;
  const auto &muzzleImages=gpu.images;
  if(!muzzleProgram || quads.empty()) return;
  if(!matrix || !std::isfinite(scale) || scale<=0 || quads.size()>quadLimit)
    throw std::runtime_error("Muzzle flash: GPU input");
  for(unsigned k=0;k<16;++k) if(!std::isfinite(matrix[k])) throw std::runtime_error("Muzzle flash: GPU-Matrix");
  const auto before=glGetError();
  if(before!=GL_NO_ERROR)
    throw std::runtime_error(std::string(owner)+": Earlier GPU error "+std::to_string(before));
  State previous;
  glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
  glUseProgram(muzzleProgram);glBindVertexArray(muzzleVao);glBindBuffer(GL_ARRAY_BUFFER,muzzleVbo);
  BindRoomFog(muzzleProgram);
  glActiveTexture(GL_TEXTURE0);glUniform1i(glGetUniformLocation(muzzleProgram,"image"),0);
  glBindSampler(0,0);glBindSampler(1,0);glBindSampler(2,0);
  glUniform1i(glGetUniformLocation(muzzleProgram,"frameImage"),2);
  glUniform1i(glGetUniformLocation(muzzleProgram,"maskImage"),1);
  glUniformMatrix4fv(glGetUniformLocation(muzzleProgram,"mvp"),1,GL_FALSE,matrix);
  constexpr GLenum src[]{GL_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE,GL_DST_COLOR,GL_DST_COLOR,GL_SRC_ALPHA};
  constexpr GLenum dst[]{GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE,GL_ONE,GL_ZERO,GL_ONE_MINUS_DST_COLOR};
  constexpr unsigned corners[]{0,1,2,0,2,3};
  // C_MTXOrtho(0,448,0,512,0,-100), with GX's [-1,0] depth
  // converted to GLES [-1,1]. Stretch its authored coordinates over the
  // physical viewport, as the opening presentation does for images/video.
  float screenMatrix[]{2.f/512,0,0,0,0,-2.f/448,0,0,
      0,0,.02f,0,-1,1,-1,1};
  if(RoomAtmosphere().cinematicFrame)CropOriginalCinematicFrame(screenMatrix);
  GLint viewport[4]{};glGetIntegerv(GL_VIEWPORT,viewport);
  for(const auto &q:quads) {
    if(event && bool(q.flags&0x400000)!=beforeWorld)continue;
    if(q.blend>5 || q.animation>=muzzleImages.size() || q.image>=muzzleImages[q.animation].size())
      throw std::runtime_error("Muzzle flash: GPU quad profile");
    glUniformMatrix4fv(glGetUniformLocation(muzzleProgram,"mvp"),1,GL_FALSE,q.screen?screenMatrix:matrix);
    glDepthMask(q.type==24?GL_FALSE:bool(q.flags&0x200000));
    glUniform1f(glGetUniformLocation(muzzleProgram,"alphaReference"),
        q.type!=24 && (q.flags&0x100000)?128.f/255:1.f/255);
    glUniform2fv(glGetUniformLocation(muzzleProgram,"repeatUV"),1,q.repeats.data());
    glUniform2fv(glGetUniformLocation(muzzleProgram,"scrollUV"),1,q.scroll.data());
    glUniform1i(glGetUniformLocation(muzzleProgram,"tiled"),q.tiled);
    glUniform2i(glGetUniformLocation(muzzleProgram,"flipUV"),q.flipS,q.flipT);
    glUniform1i(glGetUniformLocation(muzzleProgram,"refraction"),q.type==24);
    const RoomAreaEffectQuad *mask=masks?&masks->at(std::size_t(&q-quads.data())):nullptr;
    const bool masked=mask && mask->maskAnimation.has_value();
    glUniform1i(glGetUniformLocation(muzzleProgram,"masked"),masked);
    if(masked) {
      const auto animation=*mask->maskAnimation;
      if(animation>=muzzleImages.size() || mask->maskImage>=muzzleImages[animation].size())
        throw std::runtime_error("Room EST: original mask texture is missing");
      glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,muzzleImages[animation][mask->maskImage]);glActiveTexture(GL_TEXTURE0);
      glUniform1i(glGetUniformLocation(muzzleProgram,"maskStretched"),mask->maskStretched);
      glUniform1f(glGetUniformLocation(muzzleProgram,"maskAlphaScale"),mask->maskAlphaScale);
    }
    const unsigned shimmer=mask?mask->shimmerType:0;
    if(shimmer>3 || (shimmer && q.type!=8))throw std::runtime_error("Esp08: Original shimmer profile");
    glUniform1i(glGetUniformLocation(muzzleProgram,"shimmerType"),GLint(shimmer));
    glUniform1i(glGetUniformLocation(muzzleProgram,"effectFog"),
        q.type!=24 && q.type!=15 && !shimmer && !(q.flags&0x2000));
    if(shimmer) {
      if(viewport[0] || viewport[1] || viewport[2]<=0 || viewport[3]<=0 || !std::isfinite(q.sourceAlpha))
        throw std::runtime_error("Esp08: Original shimmer copy");
      const auto frame=CaptureEnvironmentScreen(viewport[2],viewport[3],true);
      glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,frame);glActiveTexture(GL_TEXTURE0);
      // Frozen GXSetIndTexMtx truncates each coefficient*1024 to signed11.
      const float coefficient=shimmer==3?.5f:q.sourceAlpha*(1.f/255)*.04f*1000.f/2500.f*(float(mask->shimmerPower)*(1.f/32)+1);
      const int encoded=int(1024*coefficient)&2047;
      glUniform1i(glGetUniformLocation(muzzleProgram,"shimmerCoefficient"),encoded>=1024?encoded-2048:encoded);
      glUniform2f(glGetUniformLocation(muzzleProgram,"viewport"),float(viewport[2]),float(viewport[3]));
    }
    glBindTexture(GL_TEXTURE_2D,muzzleImages[q.animation][q.image]);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,SourceEffectWritesAlphaGl(q.flags));
    // Tool200000 sets GX destination-alpha override to0 while retaining
    // the source alpha for RGB blending and the original alpha compare.
    glBlendFuncSeparate(src[q.blend],dst[q.blend],
        q.flags&0x200000?GL_ZERO:src[q.blend],q.flags&0x200000?GL_ZERO:dst[q.blend]);
    const auto draw=[&](const std::array<float,3> &shift,const std::array<float,4> &color) {
      float vertices[30]{};
      for(unsigned k=0;k<6;++k) {
        const auto c=corners[k];
        for(unsigned axis=0;axis<3;++axis)vertices[k*5+axis]=q.screen?
            q.position[c][axis]+shift[axis]:(q.position[c][axis]+shift[axis]-center[axis])*scale;
        vertices[k*5+3]=q.uv[c][0];vertices[k*5+4]=q.uv[c][1];
      }
      glBufferData(GL_ARRAY_BUFFER,sizeof vertices,vertices,GL_STREAM_DRAW);
      glUniform4fv(glGetUniformLocation(muzzleProgram,"rgba"),1,color.data());
      glDrawArrays(GL_TRIANGLES,0,6);
    };
    if(q.type!=24) {
      glUniform2f(glGetUniformLocation(muzzleProgram,"scaleColor"),q.rgbScale,q.alphaScale);
      draw({},q.color);continue;
    }
    if(!event || viewport[0] || viewport[1] || viewport[2]<=0 || viewport[3]<=0 ||
        !std::isfinite(q.sourceAlpha) || !std::isfinite(q.blurRate))
      throw std::runtime_error("Esp18: invalid original copy pass");
    // Esp18 captures once, then uses that immutable copy for all eight
    // layers. Its TEV stage0 samples the EFB RGB; stage1 supplies only the
    // actual ANM alpha. The configured indirect stage has no indirect TEV
    // command and must not be replaced with a guessed displacement texture.
    CaptureEnvironmentScreen(viewport[2],viewport[3],true);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,muzzleImages[q.animation][q.image]);
    glUniform2f(glGetUniformLocation(muzzleProgram,"viewport"),float(viewport[2]),float(viewport[3]));
    glUniform2f(glGetUniformLocation(muzzleProgram,"scaleColor"),1,(q.flags&0x20000)?2:1);
    std::array<float,3> right{},up{};float rightLength=0,upLength=0;
    for(unsigned axis=0;axis<3;++axis) {
      right[axis]=q.position[1][axis]-q.position[0][axis];
      up[axis]=q.position[0][axis]-q.position[3][axis];
      rightLength+=right[axis]*right[axis];upLength+=up[axis]*up[axis];
    }
    rightLength=std::sqrt(rightLength);upLength=std::sqrt(upLength);
    if(rightLength==0 || upLength==0)continue;
    for(unsigned axis=0;axis<3;++axis){right[axis]/=rightLength;up[axis]/=upLength;}
    float angle=0;
    for(unsigned layer=0;layer<8;++layer) {
      const float rx=5*motion64::sinf(angle)*q.sourceAlpha*.01f;
      const float ry=5*motion64::cosf(angle)*q.sourceAlpha*.01f;
      auto color=q.color;float alpha=q.sourceAlpha>16?255:std::max(0.f,q.sourceAlpha-4)*21.25f;
      color[3]=float(std::clamp(int(alpha*(1.f/float(layer+2))),0,255))/255.f;
      const float mul=.1f*.5f*(1.f/8)*float(layer)*q.blurRate*q.sourceAlpha*(1.f/255)+1;
      glUniform1f(glGetUniformLocation(muzzleProgram,"frameScale"),q.screen?mul:1);
      // The console's world copy excludes its fixed letterbox. The physical
      // GLES viewport has no letterbox; retain the source circular offsets.
      glUniform2f(glGetUniformLocation(muzzleProgram,"frameOffset"),q.screen?0:rx/512,
          q.screen?0:-ry/392);
      std::array<float,3> shift{};
      for(unsigned axis=0;axis<3;++axis)shift[axis]=right[axis]*rx+up[axis]*ry;
      draw(shift,color);angle+=6.2831855f/8;
    }
  }
  const auto error=glGetError();
  if(error!=GL_NO_ERROR) throw std::runtime_error(std::string(owner)+": GPU drawing "+std::to_string(error));
}
void CreateMuzzleEffectsGl(const WorldEffectsData &source) {createBillboardGpu(muzzleGpu,source,16);}
void CreateEnemyEffectsGl(const WorldEffectsData &source) {createBillboardGpu(enemyGpu,source,32);}
void CreateEventEffectsGl(const WorldEffectsData &source) {createBillboardGpu(eventGpu,source,512);}
void CreateRoomAreaEffectsGl(const WorldEffectsData &source) {createBillboardGpu(roomAreaGpu,source,512);}
void CreateRoomHitEffectsGl(const WorldEffectsData &source) {createBillboardGpu(roomHitGpu,source,128);}
void DestroyRoomHitEffectsGl() {destroyBillboardGpu(roomHitGpu);}
void ForgetRoomHitEffectsGl() {forgetBillboardGpu(roomHitGpu);}
void DrawRoomHitEffectsGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectQuad> &quads) {
  drawBillboardGpu(roomHitGpu,matrix,center,scale,quads,8192,false,false,nullptr,"Surface hit");
}
void DestroyRoomAreaEffectsGl() {destroyBillboardGpu(roomAreaGpu);}
void ForgetRoomAreaEffectsGl() {forgetBillboardGpu(roomAreaGpu);}
void DrawRoomAreaEffectsGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<RoomAreaEffectQuad> &wrapped,
                         bool beforeWorld) {
  std::vector<WorldEffectQuad> quads;quads.reserve(wrapped.size());
  for(const auto &q:wrapped)quads.push_back(q.quad);
  drawBillboardGpu(roomAreaGpu,matrix,center,scale,quads,8192,true,beforeWorld,&wrapped,"Room SST");
}
void CreateVillageBottleEffectsGl(const WorldEffectsData &core,const WorldEffectsData &trail) {createBillboardGpu(villageBottleCoreGpu,core,128);createBillboardGpu(villageBottleTrailGpu,trail,128);}
void CreateProjectileEffectsGl(const WorldEffectsData &source){createBillboardGpu(projectileGpu,source,512);}
void DestroyProjectileEffectsGl(){destroyBillboardGpu(projectileGpu);}
void ForgetProjectileEffectsGl(){forgetBillboardGpu(projectileGpu);}
void DrawProjectileEffectsGl(const float *matrix,const std::array<float,3> &center,float scale,const std::vector<WorldEffectQuad> &quads){drawBillboardGpu(projectileGpu,matrix,center,scale,quads,8192,false,false,nullptr,"Player subweapon");}
void DestroyVillageBottleEffectsGl(){destroyBillboardGpu(villageBottleCoreGpu);destroyBillboardGpu(villageBottleTrailGpu);}
void ForgetVillageBottleEffectsGl(){forgetBillboardGpu(villageBottleCoreGpu);forgetBillboardGpu(villageBottleTrailGpu);}
void DrawVillageBottleEffectsGl(const float *matrix,const std::array<float,3> &center,float scale,const std::vector<WorldEffectQuad> &core,const std::vector<WorldEffectQuad> &trail){drawBillboardGpu(villageBottleCoreGpu,matrix,center,scale,core,8192,false,false,nullptr,"Village flame");drawBillboardGpu(villageBottleTrailGpu,matrix,center,scale,trail,8192,false,false,nullptr,"Village bottle trail");}
void CreateItemBillboardsGl(const WorldEffectsData &source) {createBillboardGpu(itemGpu,source,256);}
void DrawItemBillboardsGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectQuad> &quads) {
  drawBillboardGpu(itemGpu,matrix,center,scale,quads,8192,false,false,nullptr,"Item effect");
}
void DrawEventEffectsGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectQuad> &quads,bool beforeWorld) {
  drawBillboardGpu(eventGpu,matrix,center,scale,quads,8192,true,beforeWorld,nullptr,"Event effect");
}
void DrawMuzzleEffectsGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectQuad> &quads) {
  drawBillboardGpu(muzzleGpu,matrix,center,scale,quads,512,false,false,nullptr,"Muzzle flash");
}
void DrawEnemyEffectsGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectQuad> &quads) {
  drawBillboardGpu(enemyGpu,matrix,center,scale,quads,8192,false,false,nullptr,"Enemy effect");
}
struct TrapEffectsPresentationState {
  std::uint64_t signature{};
  std::map<std::pair<std::uint64_t,std::uint64_t>,LensHistory> lenses;
};
std::shared_ptr<const TrapEffectsPresentationState> SaveTrapEffectsPresentationGl() {
  auto state=std::make_shared<TrapEffectsPresentationState>();
  state->signature=trapSignature;state->lenses=lensHistory;return state;
}
bool RestoreTrapEffectsPresentationGl(const std::shared_ptr<const TrapEffectsPresentationState> &state) {
  if(!state || state->signature!=trapSignature)return false;
  lensHistory=state->lenses;return true;
}
void CreateTrapEffectsGl(const WorldEffectsData &source) {
  createBillboardGpu(trapGpu,source,512);
  if(trapSignature!=source.sprites.signature)lensHistory.clear();
  trapSignature=source.sprites.signature;
}
void ForgetTrapEffectsGl() {
  forgetBillboardGpu(trapGpu);lineProgram=lineVao=lineVbo=0;
  lensDepth=lensDepthFbo=lensResult=lensResultFbo=lensProgram=lensVao=0;
  lensWidth=lensHeight=0;lensDepthFormat=0;
  // Esp0e's hide_alpha and previous screen position are source CPU state,
  // so an EGL loss must not restart their visibility fade.
}
void DestroyTrapEffectsGl() {
  destroyBillboardGpu(trapGpu);
  for(const auto p:{lineProgram,lensProgram})deleteProgram(p);
  for(const auto p:{lineVao,lensVao})if(p)glDeleteVertexArrays(1,&p);
  if(lineVbo)glDeleteBuffers(1,&lineVbo);
  for(const auto p:{lensDepth,lensResult})if(p)glDeleteTextures(1,&p);
  for(const auto p:{lensDepthFbo,lensResultFbo})if(p)glDeleteFramebuffers(1,&p);
  ForgetTrapEffectsGl();lensHistory.clear();trapSignature=0;
}
void DrawTrapEffectsGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectQuad> &quads) {
  drawBillboardGpu(trapGpu,matrix,center,scale,quads,8192);
}
void DrawTrapEffectLinesGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const std::vector<WorldEffectLine> &lines) {
  if(lines.empty())return;
  if(!matrix || !std::isfinite(scale) || scale<=0 || lines.size()>512)
    throw std::runtime_error("Esp09: Line budget/matrix");
  State previous;
  if(!lineProgram) {
    const auto vs=compile(GL_VERTEX_SHADER,R"GLSL(#version 300 es
layout(location=0) in vec4 p;layout(location=1) in vec4 rgba;out vec4 tint;
void main(){gl_Position=p;tint=rgba;}
)GLSL"),fs=compile(GL_FRAGMENT_SHADER,R"GLSL(#version 300 es
precision highp float;in vec4 tint;uniform float rgbScale;out vec4 color;
void main(){color=clamp(tint*vec4(vec3(rgbScale),1),0.0,1.0);if(color.a<=1.0/255.0)discard;}
)GLSL");
    lineProgram=glCreateProgram();glAttachShader(lineProgram,vs);glAttachShader(lineProgram,fs);glLinkProgram(lineProgram);
    glDeleteShader(vs);glDeleteShader(fs);GLint okay{};glGetProgramiv(lineProgram,GL_LINK_STATUS,&okay);
    if(!okay)throw std::runtime_error("Esp09: Line shader linking");
    glGenVertexArrays(1,&lineVao);glGenBuffers(1,&lineVbo);
    glBindVertexArray(lineVao);glBindBuffer(GL_ARRAY_BUFFER,lineVbo);
    for(unsigned k=0;k<2;++k){glEnableVertexAttribArray(k);glVertexAttribPointer(k,4,GL_FLOAT,GL_FALSE,8*sizeof(float),reinterpret_cast<void*>(k*4*sizeof(float)));}
  }
  glUseProgram(lineProgram);glBindVertexArray(lineVao);glBindBuffer(GL_ARRAY_BUFFER,lineVbo);
  glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
  constexpr GLenum src[]{GL_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE,GL_DST_COLOR,GL_DST_COLOR,GL_SRC_ALPHA};
  constexpr GLenum dst[]{GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE,GL_ONE,GL_ZERO,GL_ONE_MINUS_DST_COLOR};
  for(const auto &line:lines) {
    if(line.position.size()<2 || line.position.size()>6 || line.color.size()!=line.position.size() || line.blend>5 || line.width>65535)
      throw std::runtime_error("Esp09: Original line profile");
    const auto width=std::uint8_t(line.width); // GXSetLineWidth narrows source u16.
    if(!width)continue;
    std::vector<float> vertices;
    const auto clip=[&](unsigned index) {
      std::array<float,4> p{},world{(line.position[index][0]-center[0])*scale,
          (line.position[index][1]-center[1])*scale,(line.position[index][2]-center[2])*scale,1};
      for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)p[row]+=matrix[col*4+row]*world[col];
      for(float x:p)if(!std::isfinite(x))throw std::runtime_error("Esp09: non-finite projection");
      return p;
    };
    for(unsigned i=1;i<line.position.size();++i) {
      const auto a=clip(i-1),b=clip(i);if(!a[3] || !b[3] || (a[3]<0 && b[3]<0))continue;
      const float dx=(b[0]/b[3]-a[0]/a[3])*256,dy=(b[1]/b[3]-a[1]/a[3])*224;
      const float length=std::hypot(dx,dy);if(!length)continue;
      // GX width is a byte in sixth-pixel units. Expanding in the original
      // 512x448 screen keeps that same width when Android scales the viewport,
      // including GLES drivers whose native glLineWidth is limited to one.
      const float half=float(width)/12,ox=-dy/length*half/256,oy=dx/length*half/224;
      std::array<std::array<float,4>,4> corners{a,a,b,b};
      for(unsigned k=0;k<4;++k){const float sign=(k==0 || k==3)?1:-1;
        corners[k][0]+=ox*sign*corners[k][3];corners[k][1]+=oy*sign*corners[k][3];}
      constexpr unsigned order[]{0,1,2,0,2,3};
      for(const auto k:order){vertices.insert(vertices.end(),corners[k].begin(),corners[k].end());
        const auto &color=line.color[k<2?i-1:i];vertices.insert(vertices.end(),color.begin(),color.end());}
    }
    glUniform1f(glGetUniformLocation(lineProgram,"rgbScale"),line.rgbScale);
    glBlendFunc(src[line.blend],dst[line.blend]);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(float)),vertices.data(),GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()/8));
  }
  if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Esp09: Original line drawing");
}
namespace {
struct LensState : State {
  GLint draw{},read{},viewport[4]{};
  static constexpr GLenum caps[]{GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_RASTERIZER_DISCARD,
      GL_DITHER,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE};
  bool enabled[6]{};
  LensState() {
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
    glGetIntegerv(GL_VIEWPORT,viewport);for(unsigned k=0;k<6;++k)enabled[k]=glIsEnabled(caps[k]);
  }
  ~LensState() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));
    glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    for(unsigned k=0;k<6;++k)if(enabled[k])glEnable(caps[k]);else glDisable(caps[k]);
  }
};
GLenum lensSourceDepthFormat(GLuint framebuffer) {
  GLint bits{},stencil{},component=GL_UNSIGNED_NORMALIZED;
  glGetIntegerv(GL_DEPTH_BITS,&bits);glGetIntegerv(GL_STENCIL_BITS,&stencil);
  if(framebuffer) {
    // A 24-bit depth buffer can be D24 or the depth aspect of D24S8.
    // GLES requires identical source/destination depth formats for a blit.
    // EGL's requested zero stencil bits are a minimum, not an exact format.
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,
        GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,&bits);
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,
        GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,&stencil);
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,
        GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE,&component);
  }
  if(bits==16 && !stencil)return GL_DEPTH_COMPONENT16;
  if(bits==24)return stencil?GL_DEPTH24_STENCIL8:GL_DEPTH_COMPONENT24;
  if(bits==32 && component==GL_FLOAT)return stencil?GL_DEPTH32F_STENCIL8:GL_DEPTH_COMPONENT32F;
  // EGL's ES3 32-bit depth configurations use floating-point depth.
  if(!framebuffer && bits==32)return stencil?GL_DEPTH32F_STENCIL8:GL_DEPTH_COMPONENT32F;
  throw std::runtime_error("Esp0e: Unknown source depth format");
}
void lensGlCheck(const char *step) {
  const auto error=glGetError();
  if(error!=GL_NO_ERROR)throw std::runtime_error(std::string("Esp0e: ")+step+" GL="+std::to_string(error));
}
void prepareLensDepth(int width,int height,GLenum format) {
  if(width<=0 || height<=0)throw std::runtime_error("Esp0e: Depth buffer profile");
  if(!lensProgram) {
    const auto vs=compile(GL_VERTEX_SHADER,R"GLSL(#version 300 es
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}
)GLSL"),fs=compile(GL_FRAGMENT_SHADER,R"GLSL(#version 300 es
precision highp float;precision highp int;
uniform sampler2D depthImage;uniform vec2 position;uniform float depthReference,radius,xScale;
out vec4 color;
const float x[12]=float[12](0.0,.5,.86,1.0,.86,.5,0.0,-.5,-.86,-1.0,-.86,-.5);
const float y[12]=float[12](1.0,.86,.5,0.0,-.5,-.86,-1.0,-.86,-.5,0.0,.5,.86);
void main(){
  int hidden=0;ivec2 size=textureSize(depthImage,0);
  for(int i=0;i<12;++i){
    vec2 p=position+vec2(x[i]*radius*xScale,y[i]*radius);
    if(p.x<0.0 || p.x>=512.0 || p.y<0.0 || p.y>=448.0){++hidden;continue;}
    ivec2 pixel=ivec2(p.x/512.0*float(size.x),(1.0-p.y/448.0)*float(size.y));
    pixel=clamp(pixel,ivec2(0),size-ivec2(1));
    float z=texelFetch(depthImage,pixel,0).r;
    if(depthReference>floor(z*16777215.0))++hidden;
  }
  color=vec4(float(hidden)/255.0,0,0,1);
}
)GLSL");
    lensProgram=glCreateProgram();glAttachShader(lensProgram,vs);glAttachShader(lensProgram,fs);glLinkProgram(lensProgram);
    glDeleteShader(vs);glDeleteShader(fs);GLint okay{};glGetProgramiv(lensProgram,GL_LINK_STATUS,&okay);
    if(!okay)throw std::runtime_error("Esp0e: Z visibility shader linking");
    glGenVertexArrays(1,&lensVao);glGenFramebuffers(1,&lensDepthFbo);glGenFramebuffers(1,&lensResultFbo);
    glGenTextures(1,&lensDepth);glGenTextures(1,&lensResult);
    Texture result;result.width=result.height=1;result.rgba.resize(4);texture(lensResult,result);
    glBindFramebuffer(GL_FRAMEBUFFER,lensResultFbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,lensResult,0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Esp0e: Visibility framebuffer");
  }
  if(lensWidth==width && lensHeight==height && lensDepthFormat==format)return;
  glBindTexture(GL_TEXTURE_2D,lensDepth);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_NONE);
  const bool packed=format==GL_DEPTH24_STENCIL8 || format==GL_DEPTH32F_STENCIL8;
  const GLenum type=format==GL_DEPTH_COMPONENT16?GL_UNSIGNED_SHORT:
      format==GL_DEPTH_COMPONENT32F?GL_FLOAT:format==GL_DEPTH24_STENCIL8?GL_UNSIGNED_INT_24_8:
      format==GL_DEPTH32F_STENCIL8?GL_FLOAT_32_UNSIGNED_INT_24_8_REV:GL_UNSIGNED_INT;
  glTexImage2D(GL_TEXTURE_2D,0,GLint(format),width,height,0,packed?GL_DEPTH_STENCIL:GL_DEPTH_COMPONENT,type,nullptr);
  glBindFramebuffer(GL_FRAMEBUFFER,lensDepthFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_STENCIL_ATTACHMENT,GL_TEXTURE_2D,0,0);
  glFramebufferTexture2D(GL_FRAMEBUFFER,packed?GL_DEPTH_STENCIL_ATTACHMENT:GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,lensDepth,0);
  const GLenum none=GL_NONE;glDrawBuffers(1,&none);glReadBuffer(GL_NONE);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Esp0e: Z copy framebuffer");
  lensGlCheck("Create depth format/copy");
  lensWidth=width;lensHeight=height;lensDepthFormat=format;
}
}
void DrawTrapEffectLensesGl(const float *matrix,const std::array<float,3> &center,
                         float scale,const WorldEffectCamera &camera,
                         const std::vector<WorldEffectLens> &lenses,bool cameraCut) {
  if(lenses.empty()){lensHistory.clear();return;}
  if(!matrix || !std::isfinite(scale) || scale<=0 || lenses.size()>512)
    throw std::runtime_error("Esp0e: Original lens budget");
  std::set<std::pair<std::uint64_t,std::uint64_t>> alive;std::vector<WorldEffectQuad> quads;
  struct Pending {LensHistory *history;const WorldEffectLens *lens;std::array<float,3> previous;};
  std::vector<Pending> pending;
  for(const auto &lens:lenses) {
    const auto key=std::make_pair(lens.sourceSignature,lens.serial);alive.insert(key);auto &history=lensHistory[key];
    std::array<float,4> clip{},world{(lens.position[0]-center[0])*scale,(lens.position[1]-center[1])*scale,
        (lens.position[2]-center[2])*scale,1};
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)clip[row]+=matrix[col*4+row]*world[col];
    float viewZ=0,distance=0;for(unsigned k=0;k<3;++k){const float d=lens.position[k]-camera.eye[k];viewZ-=d*camera.forward[k];distance+=d*d;}
    if(!clip[3])continue;
    const std::array<float,3> screen{(clip[0]/clip[3]*.5f+.5f)*512,
        (-clip[1]/clip[3]*.5f+.5f)*448,viewZ};
    if(!history.initialized || history.age!=lens.sourceAge) {
      if(cameraCut)history.delay=2;
      if(history.delay){--history.delay;history.hide=0;}
      history.displayHide=history.hide;
      if(lens.hideRadius)pending.push_back({&history,&lens,history.screen});
      history.screen=screen;history.age=lens.sourceAge;history.initialized=true;
    }
    float alpha=0;
    if(viewZ<0 && lens.centerRatio>0) {
      const float dx=256-screen[0],dy=224-screen[1];
      const float radius=448*(lens.centerRatio*.7f);alpha=1-(dx*dx+dy*dy)/(radius*radius);
      if(lens.delDistance)alpha*=1-std::clamp(std::sqrt(distance)/lens.delDistance,0.f,1.f);
      if(lens.hideRadius)alpha*=history.displayHide;
    }
    if(alpha<=.01f)continue;
    WorldEffectQuad quad;quad.screen=true;quad.type=0;quad.animation=lens.animation;quad.image=lens.image;
    quad.flags=lens.flags;quad.blend=lens.blend;quad.rgbScale=lens.rgbScale;quad.alphaScale=lens.alphaScale;quad.color=lens.color;
    quad.color[3]=float(std::clamp(int(lens.sourceAlpha*alpha),0,255))/255;
    const float sizeRate=lens.sizeRatio?std::max(0.f,alpha*lens.sizeRatio+1-lens.sizeRatio):1;
    const float sx=lens.size[0]*sizeRate,sy=lens.size[1]*sizeRate;
    const float ox=lens.pivotNormalized[0]?-lens.pivotNormalized[0]:-.5f;
    const float oy=lens.pivotNormalized[1]?lens.pivotNormalized[1]-1:-.5f;
    std::array<std::array<float,3>,4> local{{{ox*sx,(oy+1)*sy,0},{(ox+1)*sx,(oy+1)*sy,0},
        {(ox+1)*sx,oy*sy,0},{ox*sx,oy*sy,0}}};
    motion64::Mtx rotation;motion64::Vec angle{lens.angle[0],lens.angle[1],lens.angle[2]};motion64::RotMatrix(rotation,&angle);
    for(unsigned point=0;point<4;++point) {
      if(lens.flags&2)local[point][0]=-local[point][0];
      if(!(lens.flags&4))local[point][1]=-local[point][1];
      for(unsigned row=0;row<3;++row){quad.position[point][row]=row<2?screen[row]:1;
        for(unsigned col=0;col<3;++col)quad.position[point][row]+=rotation[row][col]*local[point][col];}
    }
    quad.uv={{{0,0},{1,0},{1,1},{0,1}}};quads.push_back(quad);
  }
  drawBillboardGpu(trapGpu,matrix,center,scale,quads,8192);
  if(!pending.empty()) {
    LensState previous;const auto format=lensSourceDepthFormat(GLuint(previous.draw));
    if(previous.viewport[0] || previous.viewport[1])throw std::runtime_error("Esp0e: Z copy requires the complete frame");
    glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);prepareLensDepth(previous.viewport[2],previous.viewport[3],format);
    for(const auto cap:LensState::caps)glDisable(cap);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,ReadableFramebufferGl(GLuint(previous.draw),lensWidth,lensHeight));glBindFramebuffer(GL_DRAW_FRAMEBUFFER,lensDepthFbo);
    glBlitFramebuffer(0,0,lensWidth,lensHeight,0,0,lensWidth,lensHeight,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
    lensGlCheck("Copy original Z buffer");
    glBindFramebuffer(GL_FRAMEBUFFER,lensResultFbo);glViewport(0,0,1,1);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glUseProgram(lensProgram);glBindVertexArray(lensVao);
    glBindTexture(GL_TEXTURE_2D,lensDepth);glUniform1i(glGetUniformLocation(lensProgram,"depthImage"),0);
    for(const auto &request:pending) {
      const float nz=request.previous[2]+150;
      // Literal Esp0e_HideCheck perspective depth, quantized to the original
      // 24-bit EFB before its ring samples. Fixed full viewport omits the
      // source's 56-row console letterbox margin.
      const float inv=1.f/(1000000.f-100),inv2=1.f/-nz;
      const float zv=-1000000.f*100*inv + (-100*inv)*nz;
      const float z=inv2*zv+1;
      const float reference=nz>=0?16777216.f:std::floor(std::clamp(z,0.f,1.f)*16777215.f);
      const float s=5000/nz;
      glUniform2f(glGetUniformLocation(lensProgram,"position"),request.previous[0],request.previous[1]);
      glUniform1f(glGetUniformLocation(lensProgram,"depthReference"),reference);
      glUniform1f(glGetUniformLocation(lensProgram,"radius"),request.lens->hideRadius);
      glUniform1f(glGetUniformLocation(lensProgram,"xScale"),s<1?s*s:1);
      glDrawArrays(GL_TRIANGLES,0,3);std::uint8_t pixel[4]{};glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
      lensGlCheck("Read original Z visibility");
      const auto hidden=unsigned(pixel[0]);if(hidden>12)throw std::runtime_error("Esp0e: invalid Z visibility result");
      if(hidden==12)request.history->hide=0;
      else {const float target=std::clamp(1-float(hidden)*.1f,0.f,1.f);
        request.history->hide+=(target-request.history->hide)*.6f;}
    }
    lensGlCheck("Original Z visibility pass");
  }
  for(auto it=lensHistory.begin();it!=lensHistory.end();)if(!alive.count(it->first))it=lensHistory.erase(it);else ++it;
}
std::shared_ptr<const WorldEffectsState> SaveWorldEffectsState() {
  return simulation ? std::make_shared<WorldEffectsState>(simulation->state())
                    : nullptr;
}
void RestoreWorldEffectsState(
    const std::shared_ptr<const WorldEffectsState> &state) {
  // Camera/view state also survives switching to SAT or a single-object
  // inspector, which deliberately has no room effects. A different source
  // identity starts fresh; only same-source dynamic state must validate.
  if (!simulation || !state || state->signature != data.sprites.signature)
    return;
  if (!simulation->restore(*state))
    throw std::runtime_error("Room effect: invalid GPU context state");
}
} // namespace re4
