// Core mode renderer: the game core's state drawn with the app's GLES3 parts
// (see core_scene.h).
#include "core_scene.h"

#include "actor_gl.h"
#include "archive.h"
#include "environment.h"
#include "event_playback.h"
#include "generic_event_actor.h"
#include "room_blocks.h"
#include "room_lighting.h"
#include "room_objects.h"
#include "room_shadows.h"
#include "scene.h"
#include "scene_gl.h"
#include "smd.h"
#include "smx.h"
#include "startup_data.h"

#include <GLES3/gl3.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

#ifdef __ANDROID__
#include <android/log.h>
#define CORE_LOG(...) __android_log_print(ANDROID_LOG_INFO, "RE4Core", __VA_ARGS__)
#else
#define CORE_LOG(...) (std::fprintf(stderr, "[RE4Core] " __VA_ARGS__), std::fputc('\n', stderr))
#endif

namespace re4 {

void CopyCoreFrame(const Re4CoreSnapshot &s, CoreFrame &out) {
  out.valid = s.apiVersion == RE4_CORE_API_VERSION;
  out.serial = s.serial;
  out.frame = s.frame;
  for (unsigned k = 0; k < 4; ++k) out.step[k] = s.rno[k];
  out.room = s.room;
  out.disc = s.disc;
  out.cameraValid = s.cameraValid != 0;
  for (unsigned k = 0; k < 3; ++k) {
    out.eye[k] = s.cameraEye[k];
    out.target[k] = s.cameraTarget[k];
  }
  out.roll = s.cameraRoll;
  out.fovy = s.cameraFovy;
  const auto models = std::min<unsigned>(s.nModels, RE4_CORE_MAX_MODELS);
  const auto parts = std::min<unsigned>(s.nParts, RE4_CORE_MAX_PARTS);
  out.models.assign(s.models, s.models + models);
  out.parts.assign(s.parts, s.parts + parts);
}

namespace {

bool exists(const std::string &path) {
  struct stat st {};
  return stat(path.c_str(), &st) == 0;
}

bool sameName(const std::string &a, const char *b) {
  std::size_t i = 0;
  for (; i < a.size() && b[i]; ++i)
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  return i == a.size() && b[i] == 0;
}

std::string sourceKey(const Re4CoreSource &s) {
  char text[96];
  std::snprintf(text, sizeof(text), "%.52s@%x#%d", s.file,
                static_cast<unsigned>(s.fileOffset), static_cast<int>(s.entry));
  return text;
}

bool known(const Re4CoreSource &s) { return s.file[0] && s.entry >= 0; }

const char *kTitleVertex = R"(#version 300 es
layout(location=0) in vec2 position;
layout(location=1) in vec2 uv;
uniform vec2 scale;
out vec2 texcoord;
void main() {
  texcoord = uv;
  gl_Position = vec4(position * scale, 0.0, 1.0);
})";
const char *kTitleFragment = R"(#version 300 es
precision mediump float;
in vec2 texcoord;
uniform sampler2D image;
out vec4 color;
void main() { color = vec4(texture(image, texcoord).rgb, 1.0); })";

unsigned compile(GLenum type, const char *source) {
  const auto shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok{};
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    glDeleteShader(shader);
    throw std::runtime_error("Core title: shader");
  }
  return shader;
}

}  // namespace

std::string ResolveCoreDiscPath(const std::string &gameRoot, int disc,
                                const std::string &file) {
  if (disc < 0 || file.empty()) return {};
  std::string path = gameRoot + "/disc" + std::to_string(disc + 1) + "/files";
  std::stringstream parts(file);
  std::string part;
  while (std::getline(parts, part, '/')) {
    if (part.empty() || part == "." || part == "..") return {};
    if (exists(path + "/" + part)) {
      path += "/" + part;
      continue;
    }
    DIR *dir = opendir(path.c_str());
    if (!dir) return {};
    std::string found;
    while (const dirent *e = readdir(dir))
      if (sameName(part, e->d_name)) {
        found = e->d_name;
        break;
      }
    closedir(dir);
    if (found.empty()) return {};
    path += "/" + found;
  }
  return exists(path) ? path : std::string{};
}

std::vector<std::uint8_t> ReadCoreSource(const std::string &gameRoot, int disc,
                                         const Re4CoreSource &source) {
  if (!known(source)) throw std::runtime_error("Core: unknown model source");
  if (source.fileOffset == 0xFFFFFFFFu)
    throw std::runtime_error("Core: model in a Yz2-packed room archive (not yet supported)");
  const auto path = ResolveCoreDiscPath(gameRoot, disc, source.file);
  if (path.empty()) throw std::runtime_error(std::string("Core: file missing: ") + source.file);
  if (source.archiveSize < 0x20 || source.archiveSize > 64u * 1024 * 1024)
    throw std::runtime_error("Core: archive size");
  std::ifstream in(path, std::ios::binary);
  std::vector<std::uint8_t> bytes(source.archiveSize);
  in.seekg(source.fileOffset);
  if (!in.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size())))
    throw std::runtime_error(std::string("Core: archive outside the file: ") + source.file);
  RoomArchive archive(std::move(bytes));
  if (source.entry < 0 || std::size_t(source.entry) >= archive.entries().size())
    throw std::runtime_error("Core: archive entry");
  return archive.copy(std::size_t(source.entry));
}

struct CoreScene::Actor {
  std::unique_ptr<GenericEventActor> model;
  bool gpu{};
  unsigned joints{};  // highest skin joint + 1
  std::string error;
};

struct CoreScene::RoomState {
  std::uint16_t id{};
  std::shared_ptr<const RoomLighting> lighting;
};

CoreScene::CoreScene(std::string gameRoot) : gameRoot_(std::move(gameRoot)) {}

CoreScene::~CoreScene() = default;

void CoreScene::contextCreated() {
  ForgetSceneGl();
  ForgetGenericActorGl();
  for (auto &a : actors_) a.second->gpu = false;
  room_.reset();  // rebuilt from the prepared file on the next frame
  titleTexture_ = titleProgram_ = titleVao_ = titleVbo_ = 0;
  titleTried_ = false;
  std::lock_guard<std::mutex> lock(lock_);
  if (!preparedPath_.empty()) preparedPending_ = true;
}

void CoreScene::roomPrepared(std::uint16_t room, const std::string &decodedPath,
                             const std::string &error) {
  std::lock_guard<std::mutex> lock(lock_);
  preparedRoom_ = room;
  preparedPath_ = decodedPath;
  preparedError_ = error;
  preparedPending_ = true;
}

int CoreScene::wantedRoom() const {
  std::lock_guard<std::mutex> lock(lock_);
  return wantedRoom_;
}

std::string CoreScene::status() const {
  std::lock_guard<std::mutex> lock(lock_);
  return status_;
}

CoreScene::Actor *CoreScene::actorFor(const Re4CoreModel &model, unsigned info,
                                      int disc) {
  const auto &mi = model.infos[info];
  if (!known(mi.bin) || !known(mi.tpl)) return nullptr;
  const auto key = sourceKey(mi.bin) + "|" + sourceKey(mi.tpl);
  auto &slot = actors_[key];
  if (!slot) {
    slot = std::make_unique<Actor>();
    try {
      auto bin = ReadCoreSource(gameRoot_, disc, mi.bin);
      auto tpl = ReadCoreSource(gameRoot_, disc, mi.tpl);
      slot->model = std::make_unique<GenericEventActor>(key, key, std::move(bin), std::move(tpl));
      for (const auto &corner : slot->model->skin())
        for (const auto *w : {&corner.position, &corner.normal})
          for (unsigned j = 0; j < w->count; ++j)
            slot->joints = std::max<unsigned>(slot->joints, w->joints[j] + 1u);
      CORE_LOG("model %s: %u joints", key.c_str(), slot->joints);
    } catch (const std::exception &e) {
      slot->model.reset();
      slot->error = e.what();
      CORE_LOG("model %s: %s", key.c_str(), e.what());
    }
  }
  if (!slot->model) return nullptr;
  if (!slot->gpu) {
    CreateGenericActorGl(slot->model.get(), slot->model->mesh(), slot->model->skin());
    slot->gpu = true;
  }
  return slot.get();
}

void CoreScene::drawActors(const CoreFrame &frame, const float *camera,
                           const std::array<float, 3> &center, float scale,
                           bool normalView) {
  static const Matrix34 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
  std::vector<Matrix34> pose;
  std::vector<std::array<float, 3>> bind;
  unsigned drawn = 0, missing = 0;
  for (const auto &m : frame.models) {
    if ((m.beFlag & 6) != 6 || m.nParts == 0 ||
        std::size_t(m.firstMatrix) + m.nParts > frame.parts.size())
      continue;
    pose.resize(m.nParts);
    bind.resize(m.nParts);
    for (unsigned i = 0; i < m.nParts; ++i) {
      const auto &p = frame.parts[m.firstMatrix + i];
      std::copy(p.matrix, p.matrix + 12, pose[i].begin());
      bind[i] = {p.bind[0], p.bind[1], p.bind[2]};
    }
    const std::array<float, 3> light{pose[0][3], pose[0][7] + 800, pose[0][11]};
    for (unsigned k = 0; k < m.nInfos; ++k) {
      Actor *a = actorFor(m, k, frame.disc);
      if (!a || a->joints > m.nParts || m.nParts > 128) {
        ++missing;
        continue;
      }
      DrawGenericActorGl(a->model.get(), camera, center, scale, pose, bind, identity,
                         normalView, 1, &light, 1000.f);
      ++drawn;
    }
  }
  drawnModels_ = drawn;
  missingModels_ = missing;
}

void CoreScene::buildRoom(std::uint16_t id, const std::string &path) {
  if (!coreLoaded_) {
    coreLoaded_ = true;
    try {
      const auto core = ResolveCoreDiscPath(gameRoot_, 0, "etc/core.das");
      auto archive = RoomArchive::LoadDrs(core);
      if (const auto *eff = archive.find("EFF"))
        coreEff_ = archive.copy(std::size_t(eff - archive.entries().data()));
    } catch (const std::exception &e) {
      CORE_LOG("core.das: %s (no sky/water)", e.what());
    }
  }
  auto arc = RoomArchive::Load(path);
  auto copy = [&](const ArchiveEntry *e) { return arc.copy(std::size_t(e - arc.entries().data())); };
  const auto *smdEntry = arc.find("SMD");
  if (!smdEntry) throw std::runtime_error("Core room: no SMD");
  Smd smd(copy(smdEntry));
  std::unique_ptr<Smd> common;
  if (const auto *e = arc.find("SMD", 1)) common = std::make_unique<Smd>(copy(e), smd.commonRequirements());
  std::unique_ptr<Smx> smx;
  if (const auto *e = arc.find("SMX")) smx = std::make_unique<Smx>(copy(e));
  auto state = std::make_unique<RoomState>();
  state->id = id;
  if (arc.find("LIT")) state->lighting = ReadRoomLighting(arc);
  auto scene = BuildStaticScene(smd, -1, common.get());
  scene.lighting = state->lighting;
  scene.shadows = ReadRoomShadows(arc);
  AttachSceneTextures(smd, scene, common.get());
  if (smx) ApplySceneSmx(*smx, scene);
  if (smx && !coreEff_.empty()) {
    try {
      AttachSceneEnvironment(BuildRoomEnvironment(arc, smd, *smx, id, coreEff_), scene);
    } catch (const std::exception &e) {
      CORE_LOG("room %03x environment: %s", unsigned(id), e.what());
    }
  }
  if (id == 0x100) {
    try {
      AppendStaticSceneLayer(scene, BuildRoomBlocks(arc, smx.get(), path, id).mesh);
    } catch (const std::exception &e) {
      CORE_LOG("room %03x blocks: %s", unsigned(id), e.what());
    }
  }
  DestroySceneGl();
  CreateSceneGl(scene);
  room_ = std::move(state);
  CORE_LOG("room %03x: %zu triangles, %zu textures", unsigned(id), scene.triangles,
           scene.textures.size());
}

void CoreScene::drawTitle(int width, int height) {
  if (!titleTried_) {
    titleTried_ = true;
    try {
      TitleData title(gameRoot_ + "/disc1/files", 1);
      const Texture *best = nullptr;
      for (const auto &t : title.textures())
        if (t.frame == 0 && !t.image.rgba.empty() &&
            (!best || t.image.width * t.image.height > best->width * best->height))
          best = &t.image;
      if (!best) throw std::runtime_error("no title picture");
      glGenTextures(1, &titleTexture_);
      glBindTexture(GL_TEXTURE_2D, titleTexture_);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GLsizei(best->width), GLsizei(best->height), 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, best->rgba.data());
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      titleWidth_ = best->width;
      titleHeight_ = best->height;
      const auto v = compile(GL_VERTEX_SHADER, kTitleVertex);
      const auto f = compile(GL_FRAGMENT_SHADER, kTitleFragment);
      titleProgram_ = glCreateProgram();
      glAttachShader(titleProgram_, v);
      glAttachShader(titleProgram_, f);
      glLinkProgram(titleProgram_);
      glDeleteShader(v);
      glDeleteShader(f);
      const float quad[] = {-1, -1, 0, 1, 1, -1, 1, 1, -1, 1, 0, 0, 1, 1, 1, 0};
      glGenVertexArrays(1, &titleVao_);
      glBindVertexArray(titleVao_);
      glGenBuffers(1, &titleVbo_);
      glBindBuffer(GL_ARRAY_BUFFER, titleVbo_);
      glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                            reinterpret_cast<void *>(2 * sizeof(float)));
      glEnableVertexAttribArray(1);
      CORE_LOG("title picture %ux%u", titleWidth_, titleHeight_);
    } catch (const std::exception &e) {
      titleTexture_ = 0;
      CORE_LOG("title picture: %s", e.what());
    }
  }
  glViewport(0, 0, width, height);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!titleTexture_ || !titleProgram_) return;
  // the picture keeps its own aspect ratio, centred on the screen
  const float screen = float(std::max(width, 1)) / float(std::max(height, 1));
  const float picture = float(titleWidth_) / float(std::max(titleHeight_, 1u));
  const float sx = picture < screen ? picture / screen : 1.f;
  const float sy = picture < screen ? 1.f : screen / picture;
  glUseProgram(titleProgram_);
  glUniform2f(glGetUniformLocation(titleProgram_, "scale"), sx, sy);
  glUniform1i(glGetUniformLocation(titleProgram_, "image"), 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, titleTexture_);
  glBindVertexArray(titleVao_);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
}

void CoreScene::draw(const CoreFrame &frame, int width, int height, double seconds) {
  std::uint16_t preparedRoom{};
  std::string preparedPath, preparedError;
  bool pending = false;
  {
    std::lock_guard<std::mutex> lock(lock_);
    wantedRoom_ = frame.valid && frame.step[0] >= 2 ? int(frame.room) : -1;
    if (preparedPending_) {
      pending = true;
      preparedPending_ = false;
      preparedRoom = preparedRoom_;
      preparedPath = preparedPath_;
      preparedError = preparedError_;
    }
  }
  if (pending) {
    roomError_ = preparedError;
    if (preparedError.empty() && !preparedPath.empty()) {
      try {
        buildRoom(preparedRoom, preparedPath);
        roomError_.clear();
      } catch (const std::exception &e) {
        room_.reset();
        roomError_ = e.what();
        CORE_LOG("room %03x: %s", unsigned(preparedRoom), e.what());
      }
    }
  }
  const bool inRoom = frame.valid && frame.step[0] >= 2 && room_ && room_->id == frame.room;
  drawnModels_ = missingModels_ = 0;
  if (inRoom && frame.cameraValid) {
    EventCamera camera;
    camera.eye = frame.eye;
    camera.target = frame.target;
    camera.roll = frame.roll;
    camera.fovRadians = frame.fovy * 3.14159265358979323846f / 180.f;
    camera.valid = true;
    SceneProbe probe;
    probe.roomCamera = &camera;
    probe.screenAspect = true;
    probe.hidePlayer = true;
    probe.extraActors = [&](const float *matrix, const std::array<float, 3> &center, float scale,
                            bool normalView) { drawActors(frame, matrix, center, scale, normalView); };
    DrawSceneGl(width, height, 0, 0, 0, false, &probe, nullptr, seconds);
  } else if (!frame.valid || frame.step[0] < 2) {
    drawTitle(width, height);
  } else {
    glViewport(0, 0, width, height);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
  char text[256];
  std::snprintf(text, sizeof(text),
                "frame %u  step %u/%u  room r%03x  disc %d\nmodels %zu (drawn %u, missing %u)  room %s%s%s",
                unsigned(frame.frame), unsigned(frame.step[0]), unsigned(frame.step[1]),
                unsigned(frame.room), frame.disc + 1, frame.models.size(), drawnModels_,
                missingModels_, inRoom ? "loaded" : room_ ? "other" : "none",
                roomError_.empty() ? "" : " - ", roomError_.c_str());
  std::lock_guard<std::mutex> lock(lock_);
  status_ = text;
}

}  // namespace re4
