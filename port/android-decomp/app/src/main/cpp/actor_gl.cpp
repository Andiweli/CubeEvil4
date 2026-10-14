#include "actor_gl.h"
#include "generic_event_actor.h"
#include "room_lighting_gl.h"
#include "model_textures_gl.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
namespace re4 {
namespace {
struct ActorGpu {
  GLuint program{}, vao{}, vbo{}, bonesBuffer{};
  std::vector<GLuint> textures;
  ModelTextureLodGl textureLod;
  std::vector<SceneDraw> draws;
  std::vector<float> dynamicVertices;
  std::array<float, 3> headCenter{};
  float headRadius{};
};
ActorGpu bodyGpu, weaponGpu;
std::unordered_map<const EnemyPreview*,ActorGpu> enemyGpus;
std::unordered_map<const void*,ActorGpu> genericGpus;
std::array<ActorGpu, 2> animalGpu;
std::shared_ptr<const std::vector<Texture>> specularBank;
bool isHeadAttachment(unsigned work) {
  // cPlLeon::setModel: PL_ARC8/6/9 (BIN4/2/5 after the four header words).
  // Body, costume, both hands and the normally hidden knife remain separate.
  return work == 2 || work == 4 || work == 5;
}
void headBound(ActorGpu &gpu, const StaticScene &mesh) {
  std::array<float, 3> low{}, high{};
  low.fill(std::numeric_limits<float>::infinity());
  high.fill(-std::numeric_limits<float>::infinity());
  for (const auto &draw : mesh.draws)
    if (isHeadAttachment(draw.work))
      for (std::size_t vertex = draw.first; vertex < draw.first + draw.count;
           ++vertex)
        for (unsigned axis = 0; axis < 3; ++axis) {
          const auto value = mesh.vertices[vertex * 3 + axis];
          low[axis] = std::min(low[axis], value);
          high[axis] = std::max(high[axis], value);
        }
  if (!std::isfinite(low[0]))
    return;
  for (unsigned axis = 0; axis < 3; ++axis)
    gpu.headCenter[axis] = (low[axis] + high[axis]) * .5f;
  for (const auto &draw : mesh.draws)
    if (isHeadAttachment(draw.work))
      for (std::size_t vertex = draw.first; vertex < draw.first + draw.count;
           ++vertex) {
        float squared{};
        for (unsigned axis = 0; axis < 3; ++axis) {
          const auto d = mesh.vertices[vertex * 3 + axis] - gpu.headCenter[axis];
          squared += d * d;
        }
        gpu.headRadius = std::max(gpu.headRadius, std::sqrt(squared));
      }
}
GLuint shader(GLenum kind, const char *source) {
  auto result = glCreateShader(kind);
  glShaderSource(result, 1, &source, nullptr);
  glCompileShader(result);
  GLint okay = 0;
  glGetShaderiv(result, GL_COMPILE_STATUS, &okay);
  if (!okay) {
    char message[1024]{};
    glGetShaderInfoLog(result, sizeof(message), nullptr, message);
    glDeleteShader(result);
    throw std::runtime_error(message);
  }
  return result;
}
void forget(ActorGpu &gpu) { gpu = {}; }
void destroy(ActorGpu &gpu) {
  if (!gpu.textures.empty())
    glDeleteTextures(GLsizei(gpu.textures.size()), gpu.textures.data());
  if (gpu.bonesBuffer)
    glDeleteBuffers(1, &gpu.bonesBuffer);
  if (gpu.vbo)
    glDeleteBuffers(1, &gpu.vbo);
  if (gpu.vao)
    glDeleteVertexArrays(1, &gpu.vao);
  if (gpu.program) {
    // A deleted current program remains alive until another draw switches
    // it. Detach it now so an event-owner transition cannot capture that
    // deferred-deleted handle and later attempt to restore an invalid name.
    GLint current{};glGetIntegerv(GL_CURRENT_PROGRAM,&current);
    if(GLuint(current)==gpu.program)glUseProgram(0);
    glDeleteProgram(gpu.program);
  }
  forget(gpu);
}
void create(ActorGpu &gpu, const StaticScene &scene,
            const std::vector<SkinCorner> &skin, const std::string &label,
            bool dynamicGeometry = false) {
  auto &program = gpu.program;
  auto &vao = gpu.vao;
  auto &vbo = gpu.vbo;
  auto &bonesBuffer = gpu.bonesBuffer;
  auto &textures = gpu.textures;
  auto &draws = gpu.draws;
  const char *vs = R"(#version 300 es
layout(location=0) in vec3 p;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 rgba;
layout(location=3) in vec3 normal;
layout(location=4) in vec3 positionBones;
layout(location=5) in vec3 positionWeights;
layout(location=6) in vec3 normalBones;
layout(location=7) in vec3 normalWeights;
layout(std140) uniform BoneMatrices {mat4 bones[128];};
uniform mat4 camera;
uniform mat4 placement;
uniform mat3 placementNormal;
uniform vec3 center;
uniform float worldScale;
uniform int vertexColors;
out vec2 texcoord;
out vec4 rasterColor;
out vec3 worldNormal;
out vec3 skinnedPosition;
out vec3 worldPosition;
void main() {
  vec4 point=vec4(0);
  vec3 n=vec3(0);
  for(int i=0;i<3;++i) {
    point+=positionWeights[i]*(bones[int(positionBones[i])]*vec4(p,1));
    n+=normalWeights[i]*(mat3(bones[int(normalBones[i])])*normal);
  }
  vec3 world=(placement*vec4(point.xyz,1)).xyz;
  skinnedPosition=point.xyz;
  worldPosition=world;
  gl_Position=camera*vec4((world-center)*worldScale,1);
  // commonModelTrans uploads the inverse-transposed model/view matrix.
  // Keep the authored normal length for Specular/GI texture coordinates.
  worldNormal=placementNormal*n;
  rasterColor=litRoomColor(rgba,world,worldNormal,vertexColors); texcoord=uv;
})";
  const char *fs = R"(#version 300 es
precision highp float;
in vec2 texcoord;
in vec4 rasterColor;
in vec3 worldNormal, worldPosition;
uniform sampler2D image;
uniform sampler2D alphaImage;
uniform sampler2D projectiveImage;
uniform mat4 projectiveMatrix;
uniform vec3 center;
uniform float worldScale;
uniform int projectiveMirror, shadowMask;
uniform int vertexColors;
uniform int alphaTest;
uniform int unlit;
uniform int tevScaleGroup;
uniform float alphaReference;
uniform int normalView;
uniform float opacity;
uniform vec4 modelColor;
out vec4 color;
void main() {
  // commonModelTrans(flag1) omits textures/alpha tests. GXCopyTex takes
  // the caster's programmed destination alpha as the I8 shadow mask.
  if(shadowMask!=0){color=vec4(modelColor.aaaa);return;}
  if(projectiveMirror!=0) {
    // mirror.cpp::mirrorModelTrans2: project TexRender0 from this event's
    // fixed4:3 source camera, TEV colour scale2, model invisible factor0.4.
    vec4 p=projectiveMatrix*vec4((worldPosition-center)*worldScale,1);
    vec2 uv=p.xy/p.w*.5+.5;
    color=vec4(clamp(texture(projectiveImage,uv).rgb*2.0,0.0,1.0),modelColor.a);
    if(color.a<=0.0)discard;
    color=fogRoomColor(color,worldPosition);
    return;
  }
  // Ordered coverage keeps surviving head fragments opaque. Blending the
  // separate hair/face/eye meshes would expose overlapping skull layers and
  // disabling their depth writes would let nearer attachments paint through
  // one another. The fixed screen pattern has no frame-dependent flicker.
  if(opacity<1.0) {
    ivec2 cell=ivec2(gl_FragCoord.xy)&7;
    int order=0;
    for(int i=0;i<3;++i) {
      order=(order<<2)|(((cell.x^cell.y)&1)<<1)|(cell.y&1);
      cell>>=1;
    }
    if(opacity<(float(order)+0.5)/64.0) discard;
  }
  vec4 c=unlit!=0?vec4(1):(vertexColors!=0 || roomLightingEnabled!=0)?rasterColor:vec4(1);
  if(unlit!=0 || roomLightingEnabled==0)c.rgb*=modelColor.rgb;
  c.a*=modelColor.a;
  float a=c.a;
  if(alphaTest!=0) {
    a*=modelTexture(alphaImage,texcoord).a;
    if(floor(a*255.0+0.5)<=alphaReference) discard;
  }
  float len=length(worldNormal);
  vec3 n=len>0.0?worldNormal/len:vec3(0,1,0);
  // Legacy preview fallback applies only when no LIT room is bound.
  float light=roomLightingEnabled!=0 || unlit!=0?1.0:0.5+0.5*max(dot(n,normalize(vec3(0.3,0.8,0.5))),0.0);
  color=vec4(modelTexture(image,texcoord).rgb*c.rgb*light,a);
  color.rgb=specularRoomColor(color.rgb,c.rgb,worldNormal);
  if(normalView!=0) color.rgb=n*0.5+0.5;
  else {color.rgb=clamp(color.rgb*(tevScaleGroup==1?roomTevScale.y:roomTevScale.x),0.0,1.0);color=fogRoomColor(color,worldPosition);}
})";
  const auto litVertex=RoomAtmosphereShader(vs,true), litFragment=RoomSpecularShader(RoomAtmosphereShader(fs,false));
  auto a = shader(GL_VERTEX_SHADER, litVertex.c_str());
  GLuint b = 0;
  try {
    b = shader(GL_FRAGMENT_SHADER, litFragment.c_str());
  } catch (...) {
    glDeleteShader(a);
    throw;
  }
  program = glCreateProgram();
  glAttachShader(program, a);
  glAttachShader(program, b);
#ifdef RE4_VERIFY_ACTOR_GL
#if defined(RE4_VERIFY_WEAPON_GL) || defined(RE4_VERIFY_ENEMY_GL)
  const char *capture[]{"skinnedPosition", "worldNormal", "worldPosition"};
  glTransformFeedbackVaryings(program, 3, capture, GL_INTERLEAVED_ATTRIBS);
#else
  const char *capture[]{"skinnedPosition", "worldNormal"};
  glTransformFeedbackVaryings(program, 2, capture, GL_INTERLEAVED_ATTRIBS);
#endif
#endif
  glLinkProgram(program);
  glDeleteShader(a);
  glDeleteShader(b);
  GLint okay = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &okay);
  if (!okay)
    throw std::runtime_error(label + ": Skinning shader linking");
  auto block = glGetUniformBlockIndex(program, "BoneMatrices");
  if (block == GL_INVALID_INDEX)
    throw std::runtime_error(label + ": Joint block is missing");
  glUniformBlockBinding(program, block, 0);
  glGenBuffers(1, &bonesBuffer);
  glBindBuffer(GL_UNIFORM_BUFFER, bonesBuffer);
  glBufferData(GL_UNIFORM_BUFFER, 128 * 16 * sizeof(float), nullptr,
               GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, bonesBuffer);
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  if (skin.size() != scene.vertices.size() / 3 ||
      scene.vertices.size() != scene.normals.size() ||
      scene.uv.size() != skin.size() * 2 ||
      scene.colors.size() != skin.size() * 4)
    throw std::runtime_error(label + ": Skinning/mesh size");
  std::vector<float> data;
  data.reserve(skin.size() * 24);
  for (std::size_t i = 0; i < skin.size(); ++i) {
    data.insert(data.end(), scene.vertices.begin() + i * 3,
                scene.vertices.begin() + i * 3 + 3);
    data.insert(data.end(), scene.uv.begin() + i * 2,
                scene.uv.begin() + i * 2 + 2);
    for (unsigned k = 0; k < 4; ++k)
      data.push_back(scene.colors[i * 4 + k] / 255.f);
    data.insert(data.end(), scene.normals.begin() + i * 3,
                scene.normals.begin() + i * 3 + 3);
    for (const auto &w : {skin[i].position, skin[i].normal}) {
      for (auto j : w.joints)
        data.push_back(float(j));
      data.insert(data.end(), w.rates.begin(), w.rates.end());
    }
  }
  glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(),
               dynamicGeometry ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
  if (dynamicGeometry) gpu.dynamicVertices = std::move(data);
  unsigned sizes[8]{3, 2, 4, 3, 3, 3, 3, 3},
      offsets[8]{0, 3, 5, 9, 12, 15, 18, 21};
  for (unsigned i = 0; i < 8; ++i) {
    glVertexAttribPointer(i, sizes[i], GL_FLOAT, GL_FALSE, 24 * sizeof(float),
                          reinterpret_cast<void *>(offsets[i] * sizeof(float)));
    glEnableVertexAttribArray(i);
  }
  // FCV changes the palette, never submission order. Join adjacent GX strips
  // only when their resolved texture/mask and color branch are identical.
  for (const auto &draw : scene.draws) {
    if (!draws.empty() && draws.back().texture == draw.texture &&
        draws.back().alphaTest == draw.alphaTest &&
        draws.back().alphaTexture == draw.alphaTexture &&
        draws.back().alphaReference == draw.alphaReference &&
        draws.back().vertexColors == draw.vertexColors &&
        draws.back().specularEnabled == draw.specularEnabled &&
        draws.back().specularTexture == draw.specularTexture &&
        draws.back().specularType == draw.specularType &&
        draws.back().specularScale == draw.specularScale &&
        draws.back().specularRGB == draw.specularRGB &&
        draws.back().work == draw.work &&
        draws.back().first + draws.back().count == draw.first)
      draws.back().count += draw.count;
    else
      draws.push_back(draw);
  }
  textures.resize(scene.textures.size());
  if (!textures.empty())
    glGenTextures(GLsizei(textures.size()), textures.data());
  glActiveTexture(GL_TEXTURE0);
  auto wrap = [](unsigned m) {
    return m == 1 ? GL_REPEAT : m == 2 ? GL_MIRRORED_REPEAT : GL_CLAMP_TO_EDGE;
  };
  for (std::size_t i = 0; i < textures.size(); ++i) {
    const auto &t = scene.textures[i];
    gpu.textureLod.append(t);
    glBindTexture(GL_TEXTURE_2D, textures[i]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    t.mips.empty() ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(t.mips.size()));
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD,
                    t.mips.empty() ? 0.f : float(t.minLOD));
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, float(t.mips.size()));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap(t.wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap(t.wrapT));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t.width, t.height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, t.rgba.data());
    for (std::size_t level = 0; level < t.mips.size(); ++level) {
      const auto &m = t.mips[level];
      glTexImage2D(GL_TEXTURE_2D, GLint(level + 1), GL_RGBA8, m.width, m.height,
                   0, GL_RGBA, GL_UNSIGNED_BYTE, m.rgba.data());
    }
  }
  if (glGetError() != GL_NO_ERROR)
    throw std::runtime_error(label + ": GPU-Upload");
}
void drawPose(ActorGpu &gpu, const float *camera,
          const std::array<float, 3> &center, float worldScale,
          const std::vector<Matrix34> &bones,
          const std::vector<std::array<float,3>> &bind,
          const Matrix34 &matrix, bool normalView,
          const EnemyPreview *enemy = nullptr,
          const LeonPreview *actor = nullptr,
          const CollisionVec *resolvedEye = nullptr,
          unsigned lightMask = 1,
          const std::array<float,3> *lightPosition = nullptr,
          float lightRadius = 1000,
          const GenericActorStyle *style = nullptr) {
  const auto program = gpu.program;
  const auto vao = gpu.vao;
  const auto bonesBuffer = gpu.bonesBuffer;
  const auto &textures = gpu.textures;
  const auto &draws = gpu.draws;
  if (!program)
    return;
  if (bones.size() > 128 || bones.size() != bind.size())
    throw std::runtime_error("Actor: GPU joint budget");
  float headOpacity = 1;
  if (actor && resolvedEye && gpu.headRadius > 0 && bones.size() > 3) {
    // Platform adaptation for the fixed, common shoulder profile: the source
    // close site can enter Leon's head on a wide screen. Fade only the three
    // original head attachments as the near plane approaches their bound.
    // The bound follows source head part 4 (native joint 3) through animation;
    // torso, arms and weapon retain ordinary opaque, depth-tested rendering.
    auto local = gpu.headCenter;
    for (unsigned axis = 0; axis < 3; ++axis)
      local[axis] -= bind[3][axis];
    const auto center = TransformPoint(matrix, TransformPoint(bones[3], local));
    double squared{};
    for (unsigned axis = 0; axis < 3; ++axis) {
      const auto d = (*resolvedEye)[axis] - center[axis];
      squared += d * d;
    }
    constexpr double nearPlane = 100;
    const auto t = std::clamp((std::sqrt(squared) - gpu.headRadius - nearPlane) /
                                 gpu.headRadius,
                             0., 1.);
    headOpacity = float(t * t * (3 - 2 * t));
  }
  std::array<float, 128 * 16> matrices{};
  for (std::size_t i = 0; i < bones.size(); ++i) {
    auto m = bones[i];
    for (unsigned row = 0; row < 3; ++row)
      m[row * 4 + 3] -= m[row * 4] * bind[i][0] + m[row * 4 + 1] * bind[i][1] +
                        m[row * 4 + 2] * bind[i][2];
    for (unsigned row = 0; row < 3; ++row)
      for (unsigned col = 0; col < 4; ++col)
        matrices[i * 16 + col * 4 + row] = m[row * 4 + col];
    matrices[i * 16 + 15] = 1;
  }
  glBindBuffer(GL_UNIFORM_BUFFER, bonesBuffer);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(matrices), matrices.data());
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, bonesBuffer);
  std::array<float, 16> placement{};
  for (unsigned row = 0; row < 3; ++row)
    for (unsigned col = 0; col < 4; ++col)
      placement[col * 4 + row] = matrix[row * 4 + col];
  placement[15] = 1;
  glUseProgram(program);
  BindRoomFog(program);
  gpu.textureLod.bind(textures);
  if((actor || enemy) && !bones.empty()) {
    // cPlayer / em10_R0_Init: PartsNo1, Offset0, Size{1000,1000,0}.
    const auto lightCenter=TransformPoint(matrix,TransformPoint(bones[0],{0,0,0}));
    RoomLightVolume volume;volume.type=0;volume.center=lightCenter;volume.size={1000,1000,0};
    const auto part=WeaponAttachmentMatrix(bones[0],matrix);
    for(unsigned axis=0;axis<3;++axis)for(unsigned k=0;k<3;++k)volume.axes[axis*3+k]=part[k*4+axis];
    BindRoomLights(program,actor?1:2,0xffffffffu,lightCenter,1000,false,8,256,true,&volume);
  } else {
    BindRoomLights(program,lightMask,style?style->selectMask:0xffffffffu,
        lightPosition?*lightPosition:TransformPoint(matrix,{0,800,0}),lightRadius,false,8,256,
        style && style->lightVolume && style->lightVolume->type!=2,
        style && style->lightVolume?&*style->lightVolume:nullptr);
  }
  glBindVertexArray(vao);
  glUniformMatrix4fv(glGetUniformLocation(program, "camera"), 1, GL_FALSE,
                     camera);
  glUniformMatrix4fv(glGetUniformLocation(program, "placement"), 1, GL_FALSE,
                     placement.data());
  const auto normalMatrix=NormalMatrix(matrix);float normalColumns[9]{};
  for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)normalColumns[col*3+row]=normalMatrix[row*3+col];
  glUniformMatrix3fv(glGetUniformLocation(program,"placementNormal"),1,GL_FALSE,normalColumns);
  glUniform3fv(glGetUniformLocation(program, "center"), 1, center.data());
  glUniform1f(glGetUniformLocation(program, "worldScale"), worldScale);
  glUniform1i(glGetUniformLocation(program, "normalView"), normalView ? 1 : 0);
  glUniform1i(glGetUniformLocation(program, "image"), 0);
  glUniform1i(glGetUniformLocation(program, "alphaImage"), 1);
  glUniform1i(glGetUniformLocation(program,"projectiveImage"),2);
  glUniform1i(glGetUniformLocation(program,"projectiveMirror"),style && style->projectiveMirror);
  glUniform1i(glGetUniformLocation(program,"shadowMask"),style && style->shadowMask);
  glUniform1i(glGetUniformLocation(program,"unlit"),style && style->unlit);
  const bool globalIllumination=style&&style->globalIlluminationImage>=0&&!style->projectiveMirror;
  glUniform1i(glGetUniformLocation(program,"globalIlluminationEnabled"),globalIllumination);
  glUniform1f(glGetUniformLocation(program,"globalIlluminationScale"),style?style->globalIlluminationScale:.5f);
  glUniform1i(glGetUniformLocation(program,"globalIlluminationImage"),4);
  glActiveTexture(GL_TEXTURE4);glBindSampler(4,0);
  glBindTexture(GL_TEXTURE_2D,globalIllumination?textures.at(style->globalIlluminationImage):0);
  glUniform1i(glGetUniformLocation(program,"tevScaleGroup"),style?int(style->tevScaleGroup):actor?1:0);
  if(style && style->projectiveMirror) {
    glUniformMatrix4fv(glGetUniformLocation(program,"projectiveMatrix"),1,GL_FALSE,style->projectiveMatrix.data());
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,style->projectiveImage);glBindSampler(2,0);
  }
  const auto tint=style?style->color:std::array<float,4>{1,1,1,1};
  glUniform4fv(glGetUniformLocation(program,"modelColor"),1,tint.data());
  glUniform3fv(glGetUniformLocation(program,"roomMaterialRGB"),1,tint.data());
  // mirrorModelTrans2 uses GXSetZMode(0,7,1). Disabling the test also
  // suppresses writes; GL_ALWAYS would still occlude later world draws.
  if(style && (style->projectiveMirror || style->shadowMask))glDisable(GL_DEPTH_TEST);else glEnable(GL_DEPTH_TEST);
  glDepthFunc(style && style->zMode==2?GL_ALWAYS:GL_LEQUAL);
  glDepthMask((!style || style->zMode==0) && !(style && style->projectiveMirror)?GL_TRUE:GL_FALSE);
  // model.cpp defaults CullMode to 0; trans.cpp submits that model as a
  // single-sided GX mesh. The decoded PL00/WEP01 corners are CCW when viewed
  // from their authored outward normals in our GLES coordinate system. In
  // particular, drawing both sides exposes the inside of Leon's head when
  // the source camera is compressed to its close-wall site.
  const auto culling = glIsEnabled(GL_CULL_FACE);
  GLint cullFace{}, frontFace{};
  glGetIntegerv(GL_CULL_FACE_MODE, &cullFace);
  glGetIntegerv(GL_FRONT_FACE, &frontFace);
  glEnable(GL_CULL_FACE);
  if(style && (style->shadowMask || style->cullMode==2))glDisable(GL_CULL_FACE);
  glCullFace(style && style->cullMode==1?GL_FRONT:GL_BACK);
  glFrontFace(GL_CCW);
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if(style) {
    if(style->blendMode>4 || style->zMode>2 || style->alphaOmit>255)
      throw std::runtime_error("Actor: Original material state exceeds budget");
    if(style->blendMode==4)glDisable(GL_BLEND);
    else {constexpr GLenum source[]{GL_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE,GL_DST_COLOR},destination[]{GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE,GL_ONE};
      glBlendFunc(source[style->blendMode],destination[style->blendMode]);}
  }
  for (const auto &draw : draws) {
    if (enemy && !enemy->visiblePart(draw.work))
      continue;
    if (actor && !actor->visiblePart(draw.work))
      continue;
    const auto opacity = (actor && isHeadAttachment(draw.work) ? headOpacity : 1) *
        (enemy ? enemy->opacity() : 1);
    if (opacity == 0)
      continue;
    glUniform1f(glGetUniformLocation(program, "opacity"), opacity);
    glActiveTexture(GL_TEXTURE0);
    glBindSampler(0,0);
    glBindTexture(GL_TEXTURE_2D, textures.at(draw.texture));
    bool alpha = draw.alphaTest && draw.alphaTexture >= 0;
    glActiveTexture(GL_TEXTURE1);
    glBindSampler(1,0);
    glBindTexture(GL_TEXTURE_2D, alpha ? textures.at(draw.alphaTexture) : 0);
    glUniform1i(glGetUniformLocation(program, "vertexColors"),
                draw.vertexColors ? 1 : 0);
    glUniform1i(glGetUniformLocation(program, "alphaTest"), alpha ? 1 : 0);
    glUniform1f(glGetUniformLocation(program, "alphaReference"),
                float(style && style->alphaOmit!=255?style->alphaOmit:draw.alphaReference));
    const bool specular=draw.specularEnabled&&draw.specularTexture>=0&&!(style&&style->projectiveMirror);
    glUniform1i(glGetUniformLocation(program,"specularType"),specular?int(draw.specularType):-1);
    glUniform4f(glGetUniformLocation(program,"specularParameters"),draw.specularRGB[0]/255.f,draw.specularRGB[1]/255.f,draw.specularRGB[2]/255.f,draw.specularScale);
    glUniform1i(glGetUniformLocation(program,"specularImage"),3);glActiveTexture(GL_TEXTURE3);glBindSampler(3,0);
    glBindTexture(GL_TEXTURE_2D,specular?textures.at(draw.specularTexture):0);
    glDrawArrays(GL_TRIANGLES, GLint(draw.first), GLsizei(draw.count));
  }
  glActiveTexture(GL_TEXTURE0);
  glDepthMask(GL_TRUE);
  glCullFace(GLenum(cullFace));
  glFrontFace(GLenum(frontFace));
  if (!culling)
    glDisable(GL_CULL_FACE);
}
void draw(ActorGpu &gpu,const float *camera,const std::array<float,3> &center,
          float worldScale,const AnimationRig &rig,const Matrix34 &matrix,
          bool normalView,const EnemyPreview *enemy=nullptr,
          const LeonPreview *actor=nullptr,const CollisionVec *eye=nullptr) {
  drawPose(gpu,camera,center,worldScale,
      actor?actor->renderJointMatrices():rig.bones(),rig.bindCenters(),matrix,
      normalView,enemy,actor,eye);
}
void replace(ActorGpu &destination, const StaticScene &mesh,
             const std::vector<SkinCorner> &skin, const std::string &label,
             bool dynamicGeometry = false) {
  ActorGpu next;
  try {
    const bool needsBank=specularBank&&std::any_of(mesh.draws.begin(),mesh.draws.end(),[](const auto &d){return d.specularEnabled&&!d.specularLocal&&d.specularTexture<0;});
    if(needsBank){auto enriched=mesh;AttachSceneSpecular(enriched,*specularBank);create(next,enriched,skin,label,dynamicGeometry);}
    else create(next, mesh, skin, label, dynamicGeometry);
  } catch (...) {
    destroy(next);
    // The upload error has been reported by the exception; leave a clean GL
    // error state so the caller can recreate the previous room resources.
    for (unsigned i = 0; i < 8 && glGetError() != GL_NO_ERROR; ++i) {
    }
    throw;
  }
  destroy(destination);
  std::swap(destination, next);
}
} // namespace
void SetActorSpecularBank(std::shared_ptr<const std::vector<Texture>> bank){specularBank=std::move(bank);}
void ForgetActorGl() {
  forget(bodyGpu);
  ForgetWeaponGl();
  ForgetEnemyGl();
  ForgetAnimalsGl();
}
void DestroyActorGl() {
  destroy(bodyGpu);
  DestroyWeaponGl();
  DestroyEnemyGl();
  DestroyAnimalsGl();
}
void CreateActorGl(const LeonPreview &actor) {
  replace(bodyGpu, actor.renderMesh(), actor.renderSkin(), "Leon");
  headBound(bodyGpu, actor.renderMesh());
  // A lazy weapon grip extends Leon's mesh while the room remains alive.
  // Each other actor owns its own upload; only scene teardown destroys them.
}
void DrawActorGl(const float *camera, const std::array<float, 3> &center,
                 float worldScale, const LeonPreview &actor, bool normalView,
                 const CollisionVec *resolvedEye) {
  SmdPlacement placement;
  for (unsigned i = 0; i < 3; ++i)
    placement.position[i] = float(actor.renderFeet()[i]);
  placement.rotation[1] = actor.renderHeading();
  placement.scale = {1, 1, 1};
  draw(bodyGpu, camera, center, worldScale, actor.rig(),
       PlacementMatrix(placement), normalView, nullptr, &actor, resolvedEye);
}
void CreateWeaponGl(const WeaponPreview &weapon) {
  replace(weaponGpu, weapon.mesh(), weapon.skin(), "WEP01");
  if(const auto *model=weapon.secondary())CreateGenericActorGl(model,model->mesh(),model->skin());
}
void DestroyWeaponGl() { destroy(weaponGpu); }
void ForgetWeaponGl() { forget(weaponGpu); }
void DrawWeaponGl(const float *camera, const std::array<float, 3> &center,
                  float worldScale, const WeaponPreview &weapon,
                  const Matrix34 &hand, const Matrix34 &actorPlacement,
                  bool normalView) {
  draw(weaponGpu, camera, center, worldScale, weapon.rig(),
       WeaponAttachmentMatrix(hand, actorPlacement), normalView);
}
void DrawWeaponGl(const float *camera,const std::array<float,3> &center,float scale,const LeonPreview &actor,bool normalView) {
  const auto &weapon=*actor.weapon();const auto action=actor.action();const auto frame=actor.rig().sequenceFrame();
  const auto &parts=actor.renderJointMatrices();
  const bool grenade=weapon.family()==WeaponFamily::Grenade;
  const bool released=grenade && action==PlayerAction::Fire && frame>=5 && frame<25;
  if(!grenade || (actor.magazine() && !released))
    drawPose(weaponGpu,camera,center,scale,weapon.posedParts(parts,action,frame),weapon.rig().bindCenters(),actor.renderPlacement(),normalView);
  if(grenade && actor.magazine()>1)
    drawPose(weaponGpu,camera,center,scale,weapon.posedParts(parts,action,frame,true),weapon.rig().bindCenters(),actor.renderPlacement(),normalView);
  if(const auto *model=weapon.secondary()) {
    SmdPlacement offset;offset.scale={1,1,1};Matrix34 parent;
    if(weapon.family()==WeaponFamily::Rocket) {
      if(!actor.magazine())return;
      offset.position={-136,-30.72f,118.85f};offset.rotation={1.5707964f,0,-1.5707964f};
      parent=weapon.posedParts(parts,action,frame).at(0);
    } else {
      // cObjAllow is the arrow held by Krauser's right hand while drawing
      // and reloading; the bow's own part4 carries the nocked arrow.
      if(!((action==PlayerAction::Draw && frame>=4 && frame<11) ||
          (action==PlayerAction::Fire && frame>=16 && frame<35)))return;
      if(!actor.reserve())return;
      offset.position={-850,5,24};offset.rotation={0,-1.5707964f,0};parent=parts.at(10);
    }
    const auto placement=WeaponAttachmentMatrix(WeaponAttachmentMatrix(PlacementMatrix(offset),parent),actor.renderPlacement());
    DrawGenericActorGl(model,camera,center,scale,model->rig()->bones(),model->rig()->bindCenters(),placement,normalView,1);
  }
}
void CreateEnemyGl(const EnemyPreview &enemy) {
  replace(enemyGpus[&enemy],enemy.renderMesh(),enemy.renderSkin(),"EM12");
}
void DestroyEnemyGl() {for(auto &entry:enemyGpus)destroy(entry.second);enemyGpus.clear();}
void ForgetEnemyGl() {for(auto &entry:enemyGpus)forget(entry.second);enemyGpus.clear();}
void DestroyEnemyGl(const EnemyPreview &enemy) {
  const auto found=enemyGpus.find(&enemy);
  if(found!=enemyGpus.end()) {destroy(found->second);enemyGpus.erase(found);}
}
void DrawEnemyGl(const float *camera, const std::array<float, 3> &center,
                 float worldScale, const EnemyPreview &enemy,
                 const Matrix34 &placement, bool normalView) {
  const auto found=enemyGpus.find(&enemy);if(found==enemyGpus.end())return;
  draw(found->second,camera, center, worldScale, enemy.rig(), placement,
       normalView, &enemy);
}
void DrawEnemyShadowMaskGl(const float *camera,const EnemyPreview &enemy,
                          const Matrix34 &placement,float alpha) {
  const auto found=enemyGpus.find(&enemy);if(found==enemyGpus.end())return;
  GenericActorStyle style;style.shadowMask=true;style.blendMode=4;style.zMode=2;style.color[3]=alpha;
  drawPose(found->second,camera,{},1,enemy.rig().bones(),enemy.rig().bindCenters(),placement,
           false,&enemy,nullptr,nullptr,2,nullptr,1000,&style);
}
void CreateGenericActorGl(const void *owner,const StaticScene &mesh,
                           const std::vector<SkinCorner> &skin) {
  if(!owner)throw std::runtime_error("Event: GPU owner is missing");
  replace(genericGpus[owner],mesh,skin,"Event",true);
}
void UpdateGenericActorGl(const void *owner,const StaticScene &mesh) {
  const auto found=genericGpus.find(owner);
  if(found==genericGpus.end())throw std::runtime_error("Event: GPU actor is missing");
  auto &gpu=found->second;auto &data=gpu.dynamicVertices;
  if(data.size()!=mesh.vertices.size()*8 || mesh.vertices.size()!=mesh.normals.size())
    throw std::runtime_error("Event: Shape curve changes original mesh size");
  for(std::size_t vertex=0;vertex<mesh.vertices.size()/3;++vertex)
    for(unsigned axis=0;axis<3;++axis) {
      data[vertex*24+axis]=mesh.vertices[vertex*3+axis];
      data[vertex*24+9+axis]=mesh.normals[vertex*3+axis];
    }
  glBindBuffer(GL_ARRAY_BUFFER,gpu.vbo);
  glBufferSubData(GL_ARRAY_BUFFER,0,data.size()*sizeof(float),data.data());
}
void DrawGenericActorGl(const void *owner,const float *camera,
                         const std::array<float,3> &center,float scale,
                         const std::vector<Matrix34> &pose,
                         const std::vector<std::array<float,3>> &bind,
                         const Matrix34 &placement,bool normalView,
                         unsigned lightMask,
                         const std::array<float,3> *lightPosition,
                         float lightRadius,const GenericActorStyle *style) {
  const auto found=genericGpus.find(owner);if(found==genericGpus.end())return;
  drawPose(found->second,camera,center,scale,pose,bind,placement,normalView,
      nullptr,nullptr,nullptr,lightMask,lightPosition,lightRadius,style);
}
void DestroyGenericActorGl() {for(auto &e:genericGpus)destroy(e.second);genericGpus.clear();}
void ForgetGenericActorGl() {for(auto &e:genericGpus)forget(e.second);genericGpus.clear();}
void DestroyGenericActorGl(const void *owner) {
  const auto found=genericGpus.find(owner);
  if(found!=genericGpus.end()) {destroy(found->second);genericGpus.erase(found);}
}
void ForgetGenericActorGl(const void *owner) {
  const auto found=genericGpus.find(owner);
  if(found!=genericGpus.end()){forget(found->second);genericGpus.erase(found);}
}
void CreateAnimalsGl(const AmbientAnimals &animals) {
  std::array<ActorGpu, 2> next{};
  try {
    for (unsigned variant = 0; variant < 2; ++variant)
      create(next[variant], animals.mesh(variant), animals.skin(), "EM28");
  } catch (...) {
    for (auto &gpu : next) destroy(gpu);
    throw;
  }
  DestroyAnimalsGl();
  animalGpu = std::move(next);
}
void DestroyAnimalsGl() { for (auto &gpu : animalGpu) destroy(gpu); }
void ForgetAnimalsGl() { for (auto &gpu : animalGpu) forget(gpu); }
void DrawAnimalsGl(const float *camera, const std::array<float, 3> &center,
                   float worldScale, const AmbientAnimals &animals, bool normalView) {
  for (const auto &animal : animals.animals())
    draw(animalGpu.at(animal.variant()), camera, center, worldScale,
         animal.rig(), animal.placement(), normalView);
}
} // namespace re4
