#include "graphics_options_gl.h"
#include "environment_gl.h"
#include "scene.h"
#include "room_lighting_gl.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
namespace re4 {
namespace {
EnvironmentData data;
std::unique_ptr<EnvironmentSimulation> simulation;
GLuint program{}, vao{}, vbo{}, screen{};
GLuint captureSource{},captureFbo{},captureProgram{},captureVao{};
GLuint gridProgram{},gridVao{},gridVbo{};
std::vector<GLuint> gridSpecular,gridMask,gridBump;
int screenWidth{}, screenHeight{};
std::vector<GLuint> targets, fbo, raster, rasterFbo;
std::vector<std::vector<GLuint>> images;
GLuint compile(GLenum type, const char *source) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &source, nullptr);
  glCompileShader(s);
  GLint ok;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024]{};
    glGetShaderInfoLog(s, sizeof log, nullptr, log);
    glDeleteShader(s);
    throw std::runtime_error(log);
  }
  return s;
}
void texture(GLuint name, unsigned w, unsigned h, const void *pixels,
             bool repeat) {
  glBindTexture(GL_TEXTURE_2D, name);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                  repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);
}
} // namespace
void ForgetEnvironmentGl() {
  program = vao = vbo = screen = 0;
  captureSource=captureFbo=captureProgram=captureVao=0;
  gridProgram=gridVao=gridVbo=0;gridSpecular.clear();gridMask.clear();gridBump.clear();
  screenWidth = screenHeight = 0;
  raster.clear();
  rasterFbo.clear();
  targets.clear();
  fbo.clear();
  images.clear();
  simulation.reset();
  data = {};
}
void DestroyEnvironmentGl() {
  if(gridProgram)glDeleteProgram(gridProgram);
  if(gridVao)glDeleteVertexArrays(1,&gridVao);
  if(gridVbo)glDeleteBuffers(1,&gridVbo);
  if(!gridSpecular.empty())glDeleteTextures(GLsizei(gridSpecular.size()),gridSpecular.data());
  if(!gridMask.empty())glDeleteTextures(GLsizei(gridMask.size()),gridMask.data());
  if(!gridBump.empty())glDeleteTextures(GLsizei(gridBump.size()),gridBump.data());
  if (program)
    glDeleteProgram(program);
  if (vao)
    glDeleteVertexArrays(1, &vao);
  if (vbo)
    glDeleteBuffers(1, &vbo);
  if (screen)
    glDeleteTextures(1, &screen);
  if(captureSource)glDeleteTextures(1,&captureSource);
  if(captureFbo)glDeleteFramebuffers(1,&captureFbo);
  if(captureProgram)glDeleteProgram(captureProgram);
  if(captureVao)glDeleteVertexArrays(1,&captureVao);
  for (auto &i : images)
    if (!i.empty())
      glDeleteTextures(GLsizei(i.size()), i.data());
  if (!targets.empty())
    glDeleteTextures(GLsizei(targets.size()), targets.data());
  if (!fbo.empty())
    glDeleteFramebuffers(GLsizei(fbo.size()), fbo.data());
  if (!raster.empty())
    glDeleteTextures(GLsizei(raster.size()), raster.data());
  if (!rasterFbo.empty())
    glDeleteFramebuffers(GLsizei(rasterFbo.size()), rasterFbo.data());
  ForgetEnvironmentGl();
}
void CreateEnvironmentGl(const EnvironmentData &source) {
  ForgetEnvironmentGl();
  data = source;
  simulation = std::make_unique<EnvironmentSimulation>(data);
  if (data.targets.empty() && data.grids.empty())
    return;
  GLint oldFbo, oldRead, oldTexture, oldVao, oldBuffer, oldProgram;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldFbo);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldRead);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldBuffer);
  glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
  auto restore = [&] {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(oldFbo));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(oldRead));
    glBindTexture(GL_TEXTURE_2D, GLuint(oldTexture));
    glBindVertexArray(GLuint(oldVao));
    glBindBuffer(GL_ARRAY_BUFFER, GLuint(oldBuffer));
    glUseProgram(GLuint(oldProgram));
  };
  try {
    if(!data.grids.empty()) {
      const auto vertex=RoomAtmosphereShader(R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
uniform mat4 camera;
uniform vec3 roomCenter,waterAmbient;
uniform float roomScale;
uniform mat3 normalMatrix;
out vec3 world,waterNormal;
out vec2 texcoord;
out vec3 lit;
void main() {
  world=position;waterNormal=normal;texcoord=uv;
  vec3 n=normalMatrix*normal;
  lit=waterAmbient+litRoomColor(vec4(0,0,0,1),world,n,1).rgb;
  gl_Position=camera*vec4((position-roomCenter)*roomScale,1);
}
)GLSL",true);
      const auto fragment=RoomAtmosphereShader(R"GLSL(#version 300 es
precision highp float;
in vec3 world,waterNormal;
in vec2 texcoord;
in vec3 lit;
uniform sampler2D sceneImage,specularImage,maskImage,bumpImage;
uniform vec2 viewport,shimmer;
uniform vec3 waterColor,viewRight,viewUp;
uniform float waterAlpha;
uniform int reflection,masked;
out vec4 color;
void main() {
  float bump=texture(bumpImage,texcoord).r-128.0/255.0;
  vec2 offset=shimmer*bump*2.0;
  vec2 screen=clamp(gl_FragCoord.xy/viewport+offset,vec2(0),vec2(1));
  vec3 c=texture(sceneImage,screen).rgb*waterColor*exp2(float(reflection));
  vec2 st=vec2(.5+dot(waterNormal,viewRight),.5-.5*dot(waterNormal,viewUp));
  c+=texture(specularImage,st).rgb*lit;
  float alpha=waterAlpha*(masked!=0?texture(maskImage,texcoord).a:1.0);
  color=fogRoomColor(vec4(clamp(c,0.0,1.0),alpha),world);
}
)GLSL",false);
      const auto vs=compile(GL_VERTEX_SHADER,vertex.c_str());GLuint fs=0;
      try {fs=compile(GL_FRAGMENT_SHADER,fragment.c_str());}catch(...) {glDeleteShader(vs);throw;}
      gridProgram=glCreateProgram();glAttachShader(gridProgram,vs);glAttachShader(gridProgram,fs);glLinkProgram(gridProgram);
      glDeleteShader(vs);glDeleteShader(fs);GLint linked;glGetProgramiv(gridProgram,GL_LINK_STATUS,&linked);
      if(!linked)throw std::runtime_error("Water raster shader linking");
      glGenVertexArrays(1,&gridVao);glGenBuffers(1,&gridVbo);glBindVertexArray(gridVao);glBindBuffer(GL_ARRAY_BUFFER,gridVbo);
      glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(WaterGridVertex),nullptr);glEnableVertexAttribArray(0);
      glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(WaterGridVertex),reinterpret_cast<void *>(3*sizeof(float)));glEnableVertexAttribArray(1);
      glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(WaterGridVertex),reinterpret_cast<void *>(6*sizeof(float)));glEnableVertexAttribArray(2);
      gridSpecular.resize(data.grids.size());gridMask.resize(data.grids.size());gridBump.resize(data.grids.size());
      glGenTextures(GLsizei(gridSpecular.size()),gridSpecular.data());glGenTextures(GLsizei(gridMask.size()),gridMask.data());
      glGenTextures(GLsizei(gridBump.size()),gridBump.data());
      for(unsigned i=0;i<data.grids.size();++i) {
        const auto &g=data.grids[i];texture(gridSpecular[i],g.specular.width,g.specular.height,g.specular.rgba.data(),false);
        if(g.masked)texture(gridMask[i],g.mask.width,g.mask.height,g.mask.rgba.data(),false);
        else {constexpr unsigned white=0xffffffff;texture(gridMask[i],1,1,&white,false);}
        glBindTexture(GL_TEXTURE_2D,gridBump[i]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_R8,g.width,g.height,0,GL_RED,GL_UNSIGNED_BYTE,nullptr);
      }
    }
    if(data.targets.empty()) {restore();return;}
    auto vs = compile(GL_VERTEX_SHADER,
                      "#version 300 es\nlayout(location=0) in vec2 "
                      "p;layout(location=1) in vec2 uv;out vec2 st;void "
                      "main(){st=uv;gl_Position=vec4((p.x/"
                      "256.0-1.0)*1.15625,1.0-p.y/224.0,0,1);}");
    GLuint fs = 0;
    try {
      fs = compile(GL_FRAGMENT_SHADER,
                   "#version 300 es\nprecision highp float;in vec2 st;out vec4 "
                   "c;uniform sampler2D image;uniform vec4 rgba;uniform vec2 "
                   "scaleColor;uniform vec2 repeatUV,scrollUV;uniform int "
                   "tiled;uniform bvec2 flipUV;void main(){vec2 "
                   "uv=st;if(tiled!=0){uv=fract(uv*repeatUV-scrollUV);if("
                   "flipUV.x)uv.x=1.0-uv.x;if(flipUV.y)uv.y=1.0-uv.y;}c="
                   "texture(image,uv)*rgba;c=clamp(c*vec4(vec3(scaleColor.x),"
                   "scaleColor.y),0.0,1.0);if(c.a<=1.0/255.0)discard;}");
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
    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
      throw std::runtime_error("Water shader linking failed");
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    images.resize(data.animations.size());
    for (unsigned a = 0; a < images.size(); ++a) {
      images[a].resize(data.animations[a].images.size());
      glGenTextures(GLsizei(images[a].size()), images[a].data());
      for (unsigned i = 0; i < images[a].size(); ++i) {
        const auto &t = data.animations[a].images[i];
        texture(images[a][i], t.width, t.height, t.rgba.data(), false);
      }
    }
    targets.resize(data.targets.size());
    fbo.resize(targets.size());
    raster.resize(targets.size());
    rasterFbo.resize(targets.size());
    glGenTextures(GLsizei(targets.size()), targets.data());
    glGenFramebuffers(GLsizei(fbo.size()), fbo.data());
    glGenTextures(GLsizei(raster.size()), raster.data());
    glGenFramebuffers(GLsizei(rasterFbo.size()), rasterFbo.data());
    for (unsigned i = 0; i < targets.size(); ++i) {
      texture(targets[i], data.targets[i].width, data.targets[i].height,
              nullptr, true);
      glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, targets[i], 0);
      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Incomplete water framebuffer");
      texture(raster[i], 2 * data.targets[i].width, 2 * data.targets[i].height,
              nullptr, false);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rasterFbo[i]);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, raster[i], 0);
      if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) !=
          GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Incomplete water raster framebuffer");
    }
    if (glGetError() != GL_NO_ERROR)
      throw std::runtime_error("Water GPU upload failed");
    restore();
  } catch (...) {
    restore();
    DestroyEnvironmentGl();
    throw;
  }
}
void DrawEnvironmentTargets(double frames) {
  if (!simulation)
    return;
  auto desired =
      static_cast<std::uint64_t>(std::max(0., std::floor(frames + 1e-9)));
  // View restore normally supplies a snapshot. A deliberate backwards clock
  // without a snapshot is handled by deterministic reset.
  if (desired < simulation->state().frames)
    simulation = std::make_unique<EnvironmentSimulation>(data);
  while (simulation->state().frames < desired)
    simulation->tick();
  if (!program)
    return;
  GLint oldFbo, oldRead, oldViewport[4];
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldFbo);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldRead);
  glGetIntegerv(GL_VIEWPORT, oldViewport);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_CULL_FACE);
  glUseProgram(program);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(glGetUniformLocation(program, "image"), 0);
  auto quads = WaterQuads(data, simulation->state());
  for (unsigned t = 0; t < targets.size(); ++t) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rasterFbo[t]);
    glViewport(0, 0, 2 * data.targets[t].width, 2 * data.targets[t].height);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    for (const auto &q : quads) {
      if (q.target != t)
        continue;
      float vertices[24]{};
      unsigned corners[6]{0, 1, 2, 0, 2, 3};
      for (unsigned k = 0; k < 6; ++k) {
        unsigned c = corners[k];
        vertices[k * 4] = q.position[c][0];
        vertices[k * 4 + 1] = q.position[c][1];
        vertices[k * 4 + 2] = q.uv[c][0];
        vertices[k * 4 + 3] = q.uv[c][1];
      }
      glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STREAM_DRAW);
      glBindTexture(GL_TEXTURE_2D, images.at(q.animation).at(q.image));
      glUniform4fv(glGetUniformLocation(program, "rgba"), 1, q.color.data());
      glUniform2f(glGetUniformLocation(program, "scaleColor"), q.rgbScale,
                  q.alphaScale);
      glUniform2fv(glGetUniformLocation(program, "repeatUV"), 1,
                   q.repeats.data());
      glUniform2fv(glGetUniformLocation(program, "scrollUV"), 1,
                   q.scroll.data());
      glUniform1i(glGetUniformLocation(program, "tiled"), q.tiled ? 1 : 0);
      glUniform2i(glGetUniformLocation(program, "flipUV"), q.flipS ? 1 : 0,
                  q.flipT ? 1 : 0);
      // GX's blend table has six modes; render-target alpha is written only
      // for Tool_flg 0x8000, independently of blend/source alpha.
      constexpr GLenum src[6]{GL_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE,
                              GL_DST_COLOR, GL_DST_COLOR, GL_SRC_ALPHA};
      constexpr GLenum dst[6]{
          GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE, GL_ZERO,
          GL_ONE_MINUS_DST_COLOR};
      glEnable(GL_BLEND);
      glBlendEquation(GL_FUNC_ADD);
      glBlendFunc(src[q.blend], dst[q.blend]);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE,
                  q.updateAlpha ? GL_TRUE : GL_FALSE);
      glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    // Original GX captures at twice the output size. Linear GLES resolve is
    // bounded to this target; it does not read/write the main room image.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, rasterFbo[t]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[t]);
    glBlitFramebuffer(0, 0, 2 * data.targets[t].width,
                      2 * data.targets[t].height, 0, 0, data.targets[t].width,
                      data.targets[t].height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  }
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(oldFbo));
  glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(oldRead));
  glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
}
unsigned EnvironmentTexture(unsigned target) {
  return target < targets.size() ? targets[target] : 0;
}
EnvironmentMatrix EnvironmentSkyMatrix(unsigned sky) {
  if (!simulation || sky >= data.sky.size())
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
  return SkyDeltaMatrix(data.sky[sky], simulation->state().skyAngles[sky]);
}
std::shared_ptr<const EnvironmentState> SaveEnvironmentState() {
  return simulation ? std::make_shared<EnvironmentState>(simulation->state())
                    : nullptr;
}
void RestoreEnvironmentState(
    const std::shared_ptr<const EnvironmentState> &state) {
  if (simulation && state)
    simulation->restore(*state);
}
unsigned CaptureEnvironmentScreen(int w, int h, bool force) {
  if (!force && std::none_of(
          data.surfaces.begin(), data.surfaces.end(),
          [](const auto &s) { return s.target >= 0 && s.shader == 2; }) && data.grids.empty())
    return 0;
  if(
      w <= 0 || h <= 0)
    return 0;
  GLint max;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
  if (w > max || h > max)
    throw std::runtime_error("Water: Screen texture budget");
  GLint oldProgram,oldVao,oldActive,oldTexture,oldSampler,oldDraw,oldRead,oldViewport[4];
  GLboolean oldMask[4];
  glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);
  glGetIntegerv(GL_ACTIVE_TEXTURE,&oldActive);glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldTexture);glGetIntegerv(GL_SAMPLER_BINDING,&oldSampler);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&oldDraw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&oldRead);
  glGetIntegerv(GL_VIEWPORT,oldViewport);glGetBooleanv(GL_COLOR_WRITEMASK,oldMask);
  constexpr GLenum capabilities[]{GL_BLEND,GL_CULL_FACE,GL_DEPTH_TEST,GL_SCISSOR_TEST,
      GL_STENCIL_TEST,GL_RASTERIZER_DISCARD,GL_DITHER,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE};
  bool enabled[sizeof(capabilities)/sizeof(capabilities[0])];
  for(unsigned i=0;i<std::size(capabilities);++i)enabled[i]=glIsEnabled(capabilities[i]);
  const auto restore=[&] {
    glUseProgram(GLuint(oldProgram));glBindVertexArray(GLuint(oldVao));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(oldDraw));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(oldRead));
    glViewport(oldViewport[0],oldViewport[1],oldViewport[2],oldViewport[3]);
    glColorMask(oldMask[0],oldMask[1],oldMask[2],oldMask[3]);
    for(unsigned i=0;i<std::size(capabilities);++i)if(enabled[i])glEnable(capabilities[i]);else glDisable(capabilities[i]);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,GLuint(oldTexture));glBindSampler(0,GLuint(oldSampler));
    glActiveTexture(GLenum(oldActive));
  };
  try {
    if(!captureProgram) {
      constexpr const char *vertex=R"GLSL(#version 300 es
void main() {
  vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
  gl_Position=vec4(p*2.0-1.0,0,1);
}
)GLSL";
      // Render_before installs Bg_brightness_set(64): {8,8,10,12,10,8,8}.
      // GX's seven coefficients address three EFB rows (0/1, 2/3/4, 5/6),
      // giving 16/32/16. GXSetTexCopyDst(...,mipmap=1) then halves both axes.
      // Keep the fullscreen GLES viewport mapping; the source's 56-row crop
      // removes its fixed console letterbox, which this viewport does not have.
      constexpr const char *fragment=R"GLSL(#version 300 es
precision highp float;
precision highp int;
uniform sampler2D image;
uniform vec2 outputSize;
out vec4 color;
uvec4 sampleRow(vec2 st) {
  return uvec4(floor(texture(image,st)*255.0+0.0001));
}
void main() {
  vec2 st=gl_FragCoord.xy/outputSize;
  vec2 row=vec2(0,1.0/float(textureSize(image,0).y));
  uvec4 above=sampleRow(st-row),middle=sampleRow(st),below=sampleRow(st+row);
  uvec3 rgb=(above.rgb*16u+middle.rgb*32u+below.rgb*16u)>>6;
  color=vec4(vec3(rgb),float(middle.a))/255.0;
}
)GLSL";
      const GLuint vs=compile(GL_VERTEX_SHADER,vertex);GLuint fs{};
      try {fs=compile(GL_FRAGMENT_SHADER,fragment);}catch(...) {glDeleteShader(vs);throw;}
      captureProgram=glCreateProgram();glAttachShader(captureProgram,vs);glAttachShader(captureProgram,fs);glLinkProgram(captureProgram);
      glDeleteShader(vs);glDeleteShader(fs);GLint linked;glGetProgramiv(captureProgram,GL_LINK_STATUS,&linked);
      if(!linked)throw std::runtime_error("Water copy filter shader linking");
      glGenVertexArrays(1,&captureVao);glGenFramebuffers(1,&captureFbo);
      glGenTextures(1,&captureSource);glGenTextures(1,&screen);
    }
    const int halfWidth=std::max(1,w/2),halfHeight=std::max(1,h/2);
    if(w!=screenWidth || h!=screenHeight) {
      texture(captureSource,unsigned(w),unsigned(h),nullptr,false);
      texture(screen,unsigned(halfWidth),unsigned(halfHeight),nullptr,false);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER,captureFbo);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,screen,0);
      if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Water copy filter framebuffer");
      screenWidth=w;screenHeight=h;
    }
    glBindTexture(GL_TEXTURE_2D,captureSource);glBindSampler(0,0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,ReadableFramebufferGl(GLuint(oldDraw),w,h));
    glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,w,h);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,captureFbo);glViewport(0,0,halfWidth,halfHeight);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    for(const auto capability:capabilities)glDisable(capability);
    glUseProgram(captureProgram);glBindVertexArray(captureVao);
    glUniform1i(glGetUniformLocation(captureProgram,"image"),0);
    glUniform2f(glGetUniformLocation(captureProgram,"outputSize"),float(halfWidth),float(halfHeight));
    glDrawArrays(GL_TRIANGLES,0,3);
    restore();
  }catch(...) {restore();throw;}
  // Existing material refraction expects the completed copy on texture unit2.
  glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,screen);glActiveTexture(GL_TEXTURE0);
  return screen;
}
void DrawEnvironmentGridWater(const float *camera,const std::array<float,3> &center,
                             float scale,const std::array<float,3> &target,
                             int w,int h) {
  if(!gridProgram || !simulation || !screen)return;
  GLint oldProgram,oldVao,oldBuffer,oldActive,oldTextures[4],src,dst,srcAlpha,dstAlpha,equation,equationAlpha;
  GLboolean depthMask;glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&oldBuffer);glGetIntegerv(GL_ACTIVE_TEXTURE,&oldActive);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
  const bool blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE);
  glGetIntegerv(GL_BLEND_SRC_RGB,&src);glGetIntegerv(GL_BLEND_DST_RGB,&dst);glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
  glGetIntegerv(GL_BLEND_EQUATION_RGB,&equation);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equationAlpha);
  for(unsigned i=0;i<4;++i) {glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldTextures[i]);}
  glUseProgram(gridProgram);glBindVertexArray(gridVao);glBindBuffer(GL_ARRAY_BUFFER,gridVbo);
  glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_TRUE);
  BindRoomFog(gridProgram);
  glUniformMatrix4fv(glGetUniformLocation(gridProgram,"camera"),1,GL_FALSE,camera);
  glUniform3fv(glGetUniformLocation(gridProgram,"roomCenter"),1,center.data());glUniform1f(glGetUniformLocation(gridProgram,"roomScale"),scale);
  glUniform3fv(glGetUniformLocation(gridProgram,"viewRight"),1,RoomAtmosphere().right.data());glUniform3fv(glGetUniformLocation(gridProgram,"viewUp"),1,RoomAtmosphere().up.data());
  glUniform2f(glGetUniformLocation(gridProgram,"viewport"),float(w),float(h));
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,screen);glUniform1i(glGetUniformLocation(gridProgram,"sceneImage"),0);
  glUniform1i(glGetUniformLocation(gridProgram,"specularImage"),1);glUniform1i(glGetUniformLocation(gridProgram,"maskImage"),2);
  glUniform1i(glGetUniformLocation(gridProgram,"bumpImage"),3);
  for(unsigned i=0;i<data.grids.size();++i) {
    const auto &g=data.grids[i];const auto mesh=WaterGridMesh(g,simulation->state().grids.at(i),target);
    auto lightPosition=g.position;if(g.generator==0x45 && g.followCamera) {lightPosition[0]=target[0];lightPosition[2]=target[2];}
    BindRoomLights(gridProgram,16,0xffffffff,lightPosition,10000,false,5,g.ambient[3]);
    glUniform3f(glGetUniformLocation(gridProgram,"waterAmbient"),g.ambient[0]/255.f,g.ambient[1]/255.f,g.ambient[2]/255.f);
    SmdPlacement normalPlacement;normalPlacement.scale={g.size,g.size*.05f+100,g.size};normalPlacement.rotation=g.angle;
    auto normalWorld=PlacementMatrix(normalPlacement);normalWorld[5]=(g.originalSize?g.originalSize:g.size)*.05f+100;
    const auto matrix=NormalMatrix(normalWorld);float columnNormal[9];
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)columnNormal[c*3+r]=matrix[r*3+c];
    glUniformMatrix3fv(glGetUniformLocation(gridProgram,"normalMatrix"),1,GL_FALSE,columnNormal);
    glUniform3f(glGetUniformLocation(gridProgram,"waterColor"),g.color[0]/255.f,g.color[1]/255.f,g.color[2]/255.f);
    glUniform1f(glGetUniformLocation(gridProgram,"waterAlpha"),g.color[3]/255.f);
    glUniform2f(glGetUniformLocation(gridProgram,"shimmer"),g.shimmerS,g.shimmerT);
    glUniform1i(glGetUniformLocation(gridProgram,"reflection"),int(g.reflection));glUniform1i(glGetUniformLocation(gridProgram,"masked"),g.masked);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,gridSpecular[i]);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,gridMask[i]);
    std::vector<std::uint8_t> bump(g.width*g.height,128);const auto &state=simulation->state().grids.at(i);
    for(unsigned y=1;y<g.height;++y) {unsigned k=y*(g.width+1)+(g.type==1?0:1);for(unsigned x=1;x<g.width;++x,++k) {
      const float raw=state.normals[k][0]-(float(x)-float(g.width/2))*(1.f/float(g.width));
      const auto value=static_cast<int>(raw*255*2+128);bump[y*g.width+x]=std::uint8_t(value);
    }}
    glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,gridBump[i]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,g.width,g.height,GL_RED,GL_UNSIGNED_BYTE,bump.data());
    glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(mesh.size()*sizeof(WaterGridVertex)),mesh.data(),GL_STREAM_DRAW);
    const auto strip=(g.width+1)*2*g.height;
    glDrawArrays(GL_TRIANGLE_STRIP,0,GLsizei(strip));
    if(mesh.size()>strip)glDrawArrays(GL_TRIANGLES,GLint(strip),GLsizei(mesh.size()-strip));
  }
  for(unsigned i=0;i<4;++i) {glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,GLuint(oldTextures[i]));}
  glActiveTexture(GLenum(oldActive));glUseProgram(GLuint(oldProgram));glBindVertexArray(GLuint(oldVao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(oldBuffer));
  glDepthMask(depthMask);glBlendFuncSeparate(GLenum(src),GLenum(dst),GLenum(srcAlpha),GLenum(dstAlpha));
  glBlendEquationSeparate(GLenum(equation),GLenum(equationAlpha));
  if(!blend)glDisable(GL_BLEND);
  if(cull)glEnable(GL_CULL_FACE);
}
} // namespace re4
