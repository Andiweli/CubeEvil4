#pragma once
#include <GLES3/gl3.h>
#include <array>
namespace re4 {
constexpr std::array<GLenum,11> caps{GL_DEPTH_TEST,GL_BLEND,GL_CULL_FACE,
 GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_RASTERIZER_DISCARD,GL_POLYGON_OFFSET_FILL,
 GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE,GL_DITHER,GL_PRIMITIVE_RESTART_FIXED_INDEX};
struct ScopedRenderStateGl {
 GLint pack{},unpack{},ubo{},ubo0{};
 GLint program{},vao{},buffer{},renderbuffer{},active{},textures[5]{},samplers[5]{},
   read{},draw{},viewport[4]{},scissor[4]{},depthFunc{},cullMode{},frontFace{},
   srcRgb{},srcAlpha{},dstRgb{},dstAlpha{},equationRgb{},equationAlpha{};
 GLboolean enabled[11]{},depthMask{},colorMask[4]{};
 GLfloat clear[4]{},clearDepth{},depthRange[2]{};
 ScopedRenderStateGl() {
  glGetIntegerv(GL_PACK_ALIGNMENT,&pack);glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);
  glGetIntegerv(GL_UNIFORM_BUFFER_BINDING,&ubo);glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING,0,&ubo0);
  glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);glGetIntegerv(GL_RENDERBUFFER_BINDING,&renderbuffer);
  glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
  for(unsigned i=0;i<5;++i) {glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&textures[i]);glGetIntegerv(GL_SAMPLER_BINDING,&samplers[i]);}
  glActiveTexture(GLenum(active));glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
  glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_SCISSOR_BOX,scissor);
  glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);glGetIntegerv(GL_CULL_FACE_MODE,&cullMode);glGetIntegerv(GL_FRONT_FACE,&frontFace);
  glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb);glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);
  glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
  glGetIntegerv(GL_BLEND_EQUATION_RGB,&equationRgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equationAlpha);
  glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);glGetBooleanv(GL_COLOR_WRITEMASK,colorMask);
  glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);glGetFloatv(GL_DEPTH_CLEAR_VALUE,&clearDepth);glGetFloatv(GL_DEPTH_RANGE,depthRange);
  for(unsigned i=0;i<caps.size();++i)enabled[i]=glIsEnabled(caps[i]);
 }
 ~ScopedRenderStateGl() {
  glPixelStorei(GL_PACK_ALIGNMENT,pack);glPixelStorei(GL_UNPACK_ALIGNMENT,unpack);
  glBindBufferBase(GL_UNIFORM_BUFFER,0,GLuint(ubo0));glBindBuffer(GL_UNIFORM_BUFFER,GLuint(ubo));
  glUseProgram(GLuint(program));glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(buffer));
  glBindRenderbuffer(GL_RENDERBUFFER,GLuint(renderbuffer));
  for(unsigned i=0;i<5;++i) {glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,GLuint(textures[i]));glBindSampler(i,GLuint(samplers[i]));}
  glActiveTexture(GLenum(active));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));
  glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glScissor(scissor[0],scissor[1],scissor[2],scissor[3]);
  glDepthFunc(GLenum(depthFunc));glDepthMask(depthMask);glDepthRangef(depthRange[0],depthRange[1]);
  glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]);glCullFace(GLenum(cullMode));glFrontFace(GLenum(frontFace));
  glBlendFuncSeparate(GLenum(srcRgb),GLenum(dstRgb),GLenum(srcAlpha),GLenum(dstAlpha));
  glBlendEquationSeparate(GLenum(equationRgb),GLenum(equationAlpha));
  glClearColor(clear[0],clear[1],clear[2],clear[3]);glClearDepthf(clearDepth);
  for(unsigned i=0;i<caps.size();++i)if(enabled[i])glEnable(caps[i]);else glDisable(caps[i]);
 }
};
}
