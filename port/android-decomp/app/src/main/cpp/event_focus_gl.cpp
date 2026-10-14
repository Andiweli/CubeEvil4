// Filter01 type1 (event.cpp::FocusMove), translated from the frozen original.
#include "event_focus_gl.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace re4 {
namespace {
constexpr float sourceNear = 100, sourceFar = 1000000;
GLuint copyProgram{}, drawProgram{}, vao{}, fullTexture{}, halfTexture{}, halfFbo{}, fullFbo{};
int frameWidth{}, frameHeight{};

float sourceDepth(float distance) {
  // Keep the original float operation order. Its u32 cast is stored into an
  // int in Filter01SetParam; explicitly retain those signed low32 bits for
  // the valid negative result when camz0 becomes0.01, without C++ UB.
  if (distance == 0) distance = .01f;
  const float camz = -distance;
  const float inv = 1.f / (sourceFar - sourceNear);
  const float zv = -(sourceFar * sourceNear) * inv + (-sourceNear * inv) * camz;
  const float screen = ((1.f / -camz) * zv + 1.f) * 65535.f;
  if (!std::isfinite(screen) || screen < float(std::numeric_limits<std::int32_t>::min()) ||
      screen >= 2147483648.f)
    throw std::runtime_error("Event focus: projected depth outside original signed 32-bit range");
  return float(static_cast<std::int32_t>(screen));
}
float renderDepth(float z, float near, float far) {
  // The source overlay uses an ortho far plane65536, while the source depth
  // request is scaled by65535. Convert its actual threshold through camera
  // distance to the current GLES scene projection instead of using the same
  // numeric Z with a different scene far plane.
  if (z <= 0) return 0;
  if (z >= 65536) return 1;
  const double screen = double(z) / 65536.;
  const double distance = double(sourceNear) * sourceFar /
      (sourceFar - screen * (sourceFar - sourceNear));
  return float(std::clamp(double(far) / (far - near) -
      double(far) * near / ((far - near) * distance), 0., 1.));
}
GLuint shader(GLenum type, const char *source) {
  const auto object = glCreateShader(type);
  glShaderSource(object, 1, &source, nullptr); glCompileShader(object);
  GLint ok{}; glGetShaderiv(object, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024]{}; glGetShaderInfoLog(object, sizeof log, nullptr, log);
    glDeleteShader(object); throw std::runtime_error(log);
  }
  return object;
}
GLuint link(const char *vertex, const char *fragment) {
  const auto vs = shader(GL_VERTEX_SHADER, vertex); GLuint fs{};
  try { fs = shader(GL_FRAGMENT_SHADER, fragment); }
  catch (...) { glDeleteShader(vs); throw; }
  const auto program = glCreateProgram();
  glAttachShader(program, vs); glAttachShader(program, fs); glLinkProgram(program);
  glDeleteShader(vs); glDeleteShader(fs);
  GLint ok{}; glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[1024]{}; glGetProgramInfoLog(program, sizeof log, nullptr, log);
    glDeleteProgram(program); throw std::runtime_error(log);
  }
  return program;
}
struct State {
  GLint program{}, vertexArray{}, activeTexture{}, texture{}, sampler{};
  GLint drawFramebuffer{}, readFramebuffer{}, viewport[4]{}, depthFunction{};
  GLint blendEquationRgb{}, blendEquationAlpha{}, blendSourceRgb{}, blendSourceAlpha{};
  GLint blendDestinationRgb{}, blendDestinationAlpha{};
  GLfloat depthRange[2]{}; GLboolean mask[4]{}, depthMask{};
  static constexpr GLenum caps[]{GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST,
      GL_CULL_FACE, GL_SCISSOR_TEST, GL_RASTERIZER_DISCARD, GL_DITHER,
      GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_COVERAGE};
  GLboolean enabled[9]{};
  State() {
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture); glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport); glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask); glGetFloatv(GL_DEPTH_RANGE, depthRange);
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunction);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSourceRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSourceAlpha);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDestinationRgb);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestinationAlpha);
    for (unsigned i = 0; i < 9; ++i) enabled[i] = glIsEnabled(caps[i]);
  }
  ~State() {
    for (unsigned i = 0; i < 9; ++i)
      if (enabled[i]) glEnable(caps[i]); else glDisable(caps[i]);
    glColorMask(mask[0], mask[1], mask[2], mask[3]); glDepthMask(depthMask);
    glDepthRangef(depthRange[0], depthRange[1]); glDepthFunc(GLenum(depthFunction));
    glBlendEquationSeparate(GLenum(blendEquationRgb), GLenum(blendEquationAlpha));
    glBlendFuncSeparate(GLenum(blendSourceRgb), GLenum(blendDestinationRgb),
        GLenum(blendSourceAlpha), GLenum(blendDestinationAlpha));
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(drawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(readFramebuffer));
    glBindSampler(0, GLuint(sampler)); glBindTexture(GL_TEXTURE_2D, GLuint(texture));
    glActiveTexture(GLenum(activeTexture)); glBindVertexArray(GLuint(vertexArray));
    glUseProgram(GLuint(program));
  }
};
void prepare(int width, int height) {
  if (!copyProgram) {
    constexpr const char *copyVertex = R"(#version 300 es
void main() {
  vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));
  gl_Position=vec4(p*2.0-1.0,0.0,1.0);
})";
    constexpr const char *copyFragment = R"(#version 300 es
precision highp float;
precision highp int;
uniform sampler2D frame;
out vec4 color;
uvec3 filtered(ivec2 p) {
  ivec2 size=textureSize(frame,0);
  // GX's seven coefficients{32,0,0,0,0,0,32} span three EFB rows.
  uvec3 top=uvec3(round(texelFetch(frame,clamp(p-ivec2(0,1),ivec2(0),size-1),0).rgb*255.0));
  uvec3 bottom=uvec3(round(texelFetch(frame,clamp(p+ivec2(0,1),ivec2(0),size-1),0).rgb*255.0));
  return (top+bottom)>>1;
}
void main() {
  ivec2 p=ivec2(gl_FragCoord.xy)*2;
  uvec3 sum=filtered(p)+filtered(p+ivec2(1,0))+filtered(p+ivec2(0,1))+filtered(p+ivec2(1,1));
  color=vec4(vec3(sum>>2)/255.0,1.0);
})";
    constexpr const char *drawVertex = R"(#version 300 es
precision highp float;
uniform vec2 shift;
uniform float depth;
out vec2 uv;
void main() {
  uv=vec2(float(gl_VertexID&1),float((gl_VertexID>>1)&1));
  gl_Position=vec4(uv*2.0-1.0+shift,depth*2.0-1.0,1.0);
})";
    constexpr const char *drawFragment = R"(#version 300 es
precision highp float;
uniform sampler2D frame;
uniform float opacity;
in vec2 uv;
out vec4 color;
void main() {color=vec4(texture(frame,uv).rgb,opacity);}
)";
    copyProgram = link(copyVertex, copyFragment);
    try { drawProgram = link(drawVertex, drawFragment); }
    catch (...) { glDeleteProgram(copyProgram); copyProgram = 0; throw; }
    glGenVertexArrays(1, &vao); glGenTextures(1, &fullTexture);
    glGenTextures(1, &halfTexture); glGenFramebuffers(1, &halfFbo); glGenFramebuffers(1,&fullFbo);
  }
  if (frameWidth == width && frameHeight == height) return;
  for (const auto texture : {fullTexture, halfTexture}) {
    glBindTexture(GL_TEXTURE_2D, texture);
    const bool half = texture == halfTexture;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, half ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, half ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, half ? std::max(1, width / 2) : width,
        half ? std::max(1, height / 2) : height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  }
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fullFbo);
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,fullTexture,0);
  if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Event focus: resolve framebuffer incomplete");
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, halfFbo);
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D, halfTexture, 0);
  if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    throw std::runtime_error("Event focus: half-size copy framebuffer incomplete");
  frameWidth = width; frameHeight = height;
}
void copyFrame(int width, int height, GLuint destination) {
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination);
  GLint drawBuffer{}; glGetIntegerv(GL_DRAW_BUFFER0, &drawBuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, destination);
  GLint oldReadBuffer{}; glGetIntegerv(GL_READ_BUFFER, &oldReadBuffer);
  glReadBuffer(GLenum(drawBuffer));
  glBindTexture(GL_TEXTURE_2D, fullTexture);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fullFbo);
  glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
  glReadBuffer(GLenum(oldReadBuffer));
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, halfFbo);
  glViewport(0, 0, std::max(1, width / 2), std::max(1, height / 2));
  glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glUseProgram(copyProgram); glUniform1i(glGetUniformLocation(copyProgram, "frame"), 0);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination); glViewport(0, 0, width, height);
  glEnable(GL_DEPTH_TEST); glEnable(GL_BLEND);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
  glBindTexture(GL_TEXTURE_2D, halfTexture); glUseProgram(drawProgram);
  glUniform1i(glGetUniformLocation(drawProgram, "frame"), 0);
}
bool render(int width, int height, GLuint destination, const std::vector<EventFocusPass> &passes,
            bool near) {
  if (std::none_of(passes.begin(), passes.end(),
      [](const auto &p) { return p.sourceDepth >= 0 && p.sourceDepth <= 65536; }))
    return false; // Source ortho clips these quads as well (e.g. camz0).
  copyFrame(width, height, destination);
  glDepthFunc(near ? GL_GEQUAL : GL_LEQUAL);
  for (const auto &pass : passes) {
    if (pass.recopy) { copyFrame(width, height, destination); glDepthFunc(near ? GL_GEQUAL : GL_LEQUAL); }
    if (pass.sourceDepth < 0 || pass.sourceDepth > 65536) continue;
    glUniform2f(glGetUniformLocation(drawProgram, "shift"),
        pass.x * (2.f / 512.f), -pass.y * (2.f / 448.f));
    glUniform1f(glGetUniformLocation(drawProgram, "depth"), pass.renderDepth);
    glUniform1f(glGetUniformLocation(drawProgram, "opacity"), float(pass.alpha) / 255.f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  return true;
}
} // namespace
std::vector<EventFocusPass> EventFocusPasses(float level, float cameraDepth,
                                          float sceneNear, float sceneFar) {
  if (!std::isfinite(level) || level < 0 || level > 10 || !std::isfinite(cameraDepth) ||
      !std::isfinite(sceneNear) || !std::isfinite(sceneFar) ||
      sceneNear <= 0 || sceneFar <= sceneNear)
    throw std::runtime_error("Event focus: invalid source level/depth/projection");
  const float offset = level * .33f;
  float z = sourceDepth(cameraDepth);
  std::vector<EventFocusPass> result;
  for (const auto &sign : std::array<std::array<float, 2>, 4>{
           std::array<float, 2>{-1, -1}, {1, 1}, {1, -1}, {-1, 1}}) {
    z += 100.f; if (z > 65536.f) z = 65536.f;
    result.push_back({sign[0] < 0 ? 0.f - offset : 0.f + offset,
        sign[1] < 0 ? 0.f - offset : 0.f + offset, z,
        renderDepth(z, sceneNear, sceneFar), 128, false});
  }
  if (level > 1) {
    float alpha = (level - 1.f) * 25.f;
    if (alpha > 255.f) alpha = 255.f;
    if (alpha > 0) result.push_back({-offset + .25f, offset + .25f, z,
        renderDepth(z, sceneNear, sceneFar), static_cast<std::uint8_t>(alpha), true});
  }
  return result;
}
std::vector<EventFocusPass> RoomFocusPasses(const RoomLightCut &cut,float sceneNear,float sceneFar) {
  if(!cut.focusLevel)return {};
  if(cut.focusLevel>10 || cut.focusMode>1)throw std::runtime_error("LIT: original focus outside Filter01 table bounds");
  constexpr float offsets[]{0,.6f,1.5f,2.6f,1.5f,1.6f,1.6f,2.5f,2.6f,4.5f,6.6f};
  const unsigned count=cut.focusLevel<=3?1:cut.focusLevel==4?2:cut.focusLevel==5?3:4;
  const auto offset=offsets[cut.focusLevel];float z=float(cut.focusDepth);
  const int signs[4][2]{{-1,-1},{1,1},{1,-1},{-1,1}};
  std::vector<EventFocusPass> result;
  for(unsigned i=0;i<count;++i) {z=std::min(z+100.f,65536.f);
    result.push_back({signs[i][0]*offset,signs[i][1]*offset,z,renderDepth(z,sceneNear,sceneFar),128,false});}
  return result;
}
bool ApplyEventFocusGl(int width, int height, const EventFocusState &focus,
                      float sceneNear, float sceneFar,const RoomLightCut *fallback) {
  if (width <= 1 || height <= 1) return false;
  const bool room=!focus.nearEnabled&&!focus.farEnabled&&fallback&&fallback->focusLevel;
  if(!room && !focus.nearEnabled && !focus.farEnabled)return false;
  const auto roomPasses=room?RoomFocusPasses(*fallback,sceneNear,sceneFar):std::vector<EventFocusPass>{};
  const auto near = room && fallback->focusMode==0?roomPasses:focus.nearEnabled ? EventFocusPasses(focus.nearLevel,
      focus.nearDepth, sceneNear, sceneFar) : std::vector<EventFocusPass>{};
  const auto far = room && fallback->focusMode==1?roomPasses:focus.farEnabled ? EventFocusPasses(focus.farLevel,
      focus.farDepth, sceneNear, sceneFar) : std::vector<EventFocusPass>{};
  const State previous; prepare(width, height);
  for (const auto cap : State::caps) glDisable(cap);
  glDepthMask(GL_FALSE); glDepthRangef(0, 1);
  glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
  glBindSampler(0, 0); glBindVertexArray(vao);
  // AddOtDirect inserts at the head of bucket7: Filter01Trans queues near,
  // then far; ExecOt therefore executes far first, then near.
  const bool drewFar = render(width, height, GLuint(previous.drawFramebuffer), far, false);
  const bool drewNear = render(width, height, GLuint(previous.drawFramebuffer), near, true);
  return drewFar || drewNear;
}
void ForgetEventFocusGl() {
  copyProgram = drawProgram = vao = fullTexture = halfTexture = halfFbo = fullFbo = 0;
  frameWidth = frameHeight = 0;
}
void DestroyEventFocusGl() {
  if (copyProgram) glDeleteProgram(copyProgram);
  if (drawProgram) glDeleteProgram(drawProgram);
  if (vao) glDeleteVertexArrays(1, &vao);
  if (fullTexture) glDeleteTextures(1, &fullTexture);
  if (halfTexture) glDeleteTextures(1, &halfTexture);
  if (halfFbo) glDeleteFramebuffers(1, &halfFbo);
  if(fullFbo)glDeleteFramebuffers(1,&fullFbo);
  ForgetEventFocusGl();
}
} // namespace re4
