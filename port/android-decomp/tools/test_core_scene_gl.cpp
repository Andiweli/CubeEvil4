// Core mode renderer (core_scene.cpp) on a headless GLES3 context with
// synthetic disc data (the samples of the format comparison): no disc data
// needed, runs in the cloud.
//
//   - a disc folder with em/em99.drs (DVD container: archive with BIN + TPL),
//     a decoded room archive (SMD with a rigid floor + LIT)
//   - the path of a core source is found case-insensitively; a Yz2-packed
//     source is refused with a message
//   - event-file sources (entry -2: data at an offset of the file), and the
//     last known source of a model when the core loses it for a while
//   - the infos as the game draws them (trans.cpp): hidden without be_flag 8,
//     a rigid model hung on its part, the info's own matrix on top
//   - nested sources (innerOffset: a BIN inside another entry) and sources in
//     the Yz2-packed room archive (read from the decoded room file) are drawn;
//     the room's scenery objects (ObjMgr id 2) are left to the room scene
//   - scenery (ObjMgr id 2): an SMD work the room scene holds is left to it,
//     and the room scene follows the core's state of it (hidden: gone, moved:
//     moves along); scenery a room script made (no SMD work, r100's police
//     car) is drawn like the other models
//   - a running shape morph (talking, blinking): the core's positions replace
//     the BIN's, and the BIN's come back when it ends
//   - before the first room (step 0) the title path runs (no title data here:
//     black) and no room is wanted
//   - in the room (step 3) the room is built from the prepared file, the
//     camera comes from the frame, and the core's model is drawn with its part
//     matrices: the picture differs from the same frame with the model hidden
//     (be_flag without the draw bits), from one of two opposite cameras, and
//     moves when the part matrices move
#include "core_scene.h"

#include "archive.h"
#include "scene.h"
#include "smd.h"

#include "../../whole/tests/format_compare/samples.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {

unsigned checks{};
void require(bool okay, const char *message) {
  ++checks;
  if (!okay) throw std::runtime_error(message);
}

void writeFile(const std::string &path, const std::vector<std::uint8_t> &bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  require(bool(out), "write sample file");
}

const int kWidth = 320, kHeight = 180;
// no time passes between the test's frames: the room's own animations (sky,
// water, texture scroll at 30 Hz) would change every other picture
const double kStill = 0;

std::vector<unsigned char> pixels() {
  std::vector<unsigned char> result(kWidth * kHeight * 4);
  glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
  require(glGetError() == GL_NO_ERROR, "read framebuffer");
  return result;
}

unsigned changes(const std::vector<unsigned char> &a, const std::vector<unsigned char> &b) {
  unsigned n{};
  for (std::size_t i = 0; i < a.size(); i += 4)
    n += a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2];
  return n;
}

void context() {
  auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
      eglGetProcAddress("eglGetPlatformDisplayEXT"));
  EGLDisplay display = getDisplay ? getDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)
                                  : eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint major{}, minor{};
  require(eglInitialize(display, &major, &minor), "EGL initialise");
  const EGLint attributes[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE};
  EGLConfig config{};
  EGLint count{};
  eglChooseConfig(display, attributes, &config, 1, &count);
  eglBindAPI(EGL_OPENGL_ES_API);
  const EGLint contextAttributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};
  EGLContext context = eglCreateContext(display, count ? config : nullptr, EGL_NO_CONTEXT, contextAttributes);
  require(context != EGL_NO_CONTEXT, "EGL context");
  require(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context), "EGL current");
  GLuint fbo{}, color{}, depth{};
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glGenRenderbuffers(1, &color);
  glBindRenderbuffer(GL_RENDERBUFFER, color);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kWidth, kHeight);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
  glGenRenderbuffers(1, &depth);
  glBindRenderbuffer(GL_RENDERBUFFER, depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kWidth, kHeight);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth);
  require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "framebuffer");
}

void setSource(Re4CoreSource &s, const char *file, unsigned offset, unsigned size, int entry) {
  std::memset(&s, 0, sizeof(s));
  std::snprintf(s.file, sizeof(s.file), "%s", file);
  s.fileOffset = offset;
  s.archiveSize = size;
  s.entry = entry;
}

}  // namespace

int main() {
  try {
    using namespace re4samples;
    char base[] = "/tmp/re4-core-scene-XXXXXX";
    require(mkdtemp(base) != nullptr, "temporary folder");
    const std::string root = base;
    for (const char *dir : {"/disc1", "/disc1/files", "/disc1/files/em"})
      mkdir((root + dir).c_str(), 0755);
    const auto archive = makeArchive({{"BIN", makeBin(true, false, true)}, {"TPL", makeTpl()}});
    writeFile(root + "/disc1/files/em/em99.drs", makeDrs(archive));
    // a BIN 0x40 bytes into an entry (as inside an SMD)
    Bytes nested(0x40, 0);
    {
      const auto b = makeBin(true, false, true);
      nested.insert(nested.end(), b.begin(), b.end());
    }
    const auto archive2 = makeArchive({{"SMD", nested}, {"TPL", makeTpl()}});
    const auto archive3 = makeArchive({{"BIN", makeRigidBin()}, {"TPL", makeTpl()}});
    writeFile(root + "/disc1/files/em/em97.drs", makeDrs(archive3));
    writeFile(root + "/disc1/files/em/em98.drs", makeDrs(archive2));
    // an event file: a container part at 0x400 holding the BIN at +0x100 and the TPL after it
    Bytes eventPart(0x100, 0);
    const auto eventBin = makeBin(true, false, true);
    const auto eventTpl = makeTpl();
    eventPart.insert(eventPart.end(), eventBin.begin(), eventBin.end());
    const auto tplOffset = unsigned(eventPart.size());
    eventPart.insert(eventPart.end(), eventTpl.begin(), eventTpl.end());
    {
      Bytes file(0x400, 0);
      file.insert(file.end(), eventPart.begin(), eventPart.end());
      mkdir((root + "/disc1/files/Evd").c_str(), 0755);
      writeFile(root + "/disc1/files/Evd/r100s00.evd", file);
    }
    // the decoded room archive also holds a model (entries 2 and 3)
    const std::string roomPath = root + "/room.arc";
    writeFile(roomPath, makeArchive({{"SMD", makeSmd(makeRigidBin())}, {"LIT", makeLit()},
                                     {"BIN", makeBin(true, false, true)}, {"TPL", makeTpl()}}));

    // sources
    require(re4::ResolveCoreDiscPath(root, 0, "EM/EM99.DRS") == root + "/disc1/files/em/em99.drs",
            "case-insensitive disc path");
    require(re4::ResolveCoreDiscPath(root, 0, "em/missing.drs").empty(), "missing disc path");
    require(re4::ResolveCoreDiscPath(root, 1, "em/em99.drs").empty(), "second disc absent");
    Re4CoreSource bin{}, tpl{}, packed{};
    setSource(bin, "em/em99.drs", 0x400, unsigned(archive.size()), 0);
    setSource(tpl, "em/em99.drs", 0x400, unsigned(archive.size()), 1);
    setSource(packed, "st1/r100.das", 0xFFFFFFFFu, 0x1000, 3);
    require(re4::ReadCoreSource(root, 0, bin) == makeBin(true, false, true), "BIN bytes from the disc");
    bool refused = false;
    try {
      re4::ReadCoreSource(root, 0, packed);
    } catch (const std::exception &) {
      refused = true;
    }
    require(refused, "Yz2-packed source refused");
    Re4CoreSource inner{};
    setSource(inner, "em/em98.drs", 0x400, unsigned(archive2.size()), 0);
    inner.innerOffset = 0x40;
    require(re4::ReadCoreSource(root, 0, inner) == makeBin(true, false, true), "nested BIN from its entry");
    {
      const auto decoded = re4::RoomArchive::Load(roomPath);
      Re4CoreSource inRoom{};
      setSource(inRoom, "st1/r100.das", 0xFFFFFFFFu, unsigned(decoded.size()), 2);
      require(re4::ReadCoreSource(root, 0, inRoom, &decoded) == makeBin(true, false, true),
              "packed source from the decoded room archive");
    }

    // the placement of the room SMD's floor (work 0)
    std::array<float, 12> floorPlacement{};
    {
      const auto decoded = re4::RoomArchive::Load(roomPath);
      const re4::Smd smd(decoded.copy(0));
      floorPlacement = re4::PlacementMatrix(smd.placements.at(0));
    }

    context();
    re4::CoreSceneLogBounds = true;  // the diagnostics run along (stderr)
    re4::CoreScene scene(root);
    scene.contextCreated();

    // title (no title data in the synthetic disc: black)
    re4::CoreFrame frame;
    frame.valid = true;
    frame.frame = 10;
    frame.disc = 0;
    frame.room = 0x120;
    scene.draw(frame, kWidth, kHeight, kStill);
    require(scene.wantedRoom() == -1, "no room wanted on the title");
    const auto title = pixels();
    require(title[0] == 0 && title[1] == 0 && title[2] == 0, "title without data is black");

    // the room with one model of the core
    frame.step = {3, 0, 0, 0};
    frame.room = 0x100;
    frame.cameraValid = true;
    frame.roll = 0;
    frame.fovy = 50;
    Re4CoreModel model{};
    model.address = 0x82001000u;
    model.beFlag = 1 | 2 | 4;
    model.nParts = 2;
    model.firstMatrix = 0;
    model.firstInfo = 0;
    model.nInfos = 1;
    model.matrix[0] = model.matrix[5] = model.matrix[10] = 1;
    model.opacity = 1;  // invisible_factor * invisible_factor2
    Re4CoreModelInfo info{};
    info.bin = bin;
    info.tpl = tpl;
    info.flags = 8;  // drawn (cModelInfo::be_flag)
    info.rigidPart = -1;
    info.opacity = 1;
    std::memset(info.color, 0xFF, 4);
    info.matrix[0] = info.matrix[5] = info.matrix[10] = 1;
    frame.models = {model};
    frame.infos = {info};
    frame.parts.assign(2, Re4CorePart{});
    for (auto &p : frame.parts) p.matrix[0] = p.matrix[5] = p.matrix[10] = 1;
    scene.draw(frame, kWidth, kHeight, kStill);
    require(scene.wantedRoom() == 0x100, "the room is wanted");
    scene.roomPrepared(0x100, roomPath, "");

    // how the game draws one info (trans.cpp): blend row, depth mode, side, fade
    {
      Re4CoreModel m{};
      Re4CoreModelInfo i{};
      m.opacity = 0.5f;
      m.zMode = 1;
      m.cullMode = 2;
      i.opacity = 1;
      i.blendMode = 2;
      std::memset(i.color, 0xFF, 4);
      i.color[3] = 0x80;
      const auto style = re4::CoreModelStyle(m, i);
      require(style.blendMode == 2 && style.zMode == 1 && style.cullMode == 2, "draw state taken over");
      require(std::fabs(style.color[3] - 0x80 / 255.f * 0.5f) < 1e-4f, "fade scales the material alpha");
      m.opacity = 1;
      i.blendMode = 9;  // out of the table: plain
      const auto plain = re4::CoreModelStyle(m, i);
      require(plain.color[3] == 1.f && plain.blendMode == 0, "no fade, plain blending");
    }

    unsigned best = 0;
    for (float side : {-1.f, 1.f}) {
      frame.eye = {3000 * side, 900, -600};
      frame.target = {0, 700, -600};
      frame.models[0].beFlag = 1 | 2 | 4;
      scene.draw(frame, kWidth, kHeight, kStill);
      const auto status = scene.status();
      std::fprintf(stderr, "status: %s\n", status.c_str());
      require(status.find("room loaded") != std::string::npos, "room built");
      require(status.find("drawn 1, missing 0") != std::string::npos, "model drawn");
      const auto with = pixels();
      frame.models[0].beFlag = 1;  // alive, not drawn
      scene.draw(frame, kWidth, kHeight, kStill);
      require(scene.status().find("drawn 0, missing 0") != std::string::npos, "hidden model skipped");
      const auto without = pixels();
      // the game leaves it out (an event holds the world, a display switch)
      frame.models[0].beFlag = 1 | 2 | 4;
      frame.models[0].flags = RE4_CORE_MODEL_HIDDEN;
      scene.draw(frame, kWidth, kHeight, kStill);
      require(scene.status().find("drawn 0, missing 0") != std::string::npos, "model the game hides skipped");
      require(changes(without, pixels()) == 0, "nothing of the hidden model in the picture");
      frame.models[0].flags = 0;
      // the game's blending: added on (a glow) it is still drawn, but lighter
      // than the plain model; half faded it changes the picture less
      frame.infos[0].blendMode = 2;
      scene.draw(frame, kWidth, kHeight, kStill);
      require(scene.status().find("drawn 1, missing 0") != std::string::npos, "added-on model drawn");
      const auto added = pixels();
      std::fprintf(stderr, "added on: %u pixels from the plain model, %u from none\n", changes(with, added),
                   changes(without, added));
      // (from one of the two sides the model shows its back: nothing to see)
      require(changes(with, without) < 20 || (changes(with, added) > 0 && changes(without, added) > 0),
              "added on: neither plain nor absent");
      frame.infos[0].blendMode = 0;
      frame.models[0].opacity = 0.5f;
      scene.draw(frame, kWidth, kHeight, kStill);
      require(changes(with, without) < 20 || changes(with, pixels()) > 0, "a faded model differs from the plain one");
      frame.models[0].opacity = 1;
      best = std::max(best, changes(with, without));
    }
    std::fprintf(stderr, "model pixels: %u\n", best);
    require(best > 20, "the model appears in the picture");

    // the core's part matrices pose the model: lifting both parts moves it
    frame.eye = {-3000, 900, -600};
    frame.models[0].beFlag = 1 | 2 | 4;
    scene.draw(frame, kWidth, kHeight, kStill);
    const auto rest = pixels();
    for (auto &p : frame.parts) p.matrix[7] = 400;
    scene.draw(frame, kWidth, kHeight, kStill);
    const auto lifted = pixels();
    for (auto &p : frame.parts) p.matrix[7] = 0;
    std::fprintf(stderr, "pose change pixels: %u\n", changes(rest, lifted));
    require(changes(rest, lifted) > 20, "the part matrices move the model");
    // a face's shape morph: the core's positions replace the BIN's while it
    // runs (the sample BIN's four positions, halved), the BIN's come back after
    {
      const std::int16_t original[4][3] = {{160, -32, 4000}, {-2, 32767, -32768}, {0, 0, 1}, {1200, -1200, 77}};
      frame.morph.clear();
      for (const auto &p : original)
        frame.morph.push_back({std::int16_t(p[0] / 2), std::int16_t(p[1] / 2), std::int16_t(p[2] / 2), 0});
      frame.infos[0].firstMorph = 0;
      frame.infos[0].nMorph = 4;
      scene.draw(frame, kWidth, kHeight, kStill);
      const auto morphed = pixels();
      frame.infos[0].nMorph = 0;
      frame.morph.clear();
      scene.draw(frame, kWidth, kHeight, kStill);
      const auto back = pixels();
      std::fprintf(stderr, "morph: %u pixels changed, %u after it ends\n", changes(rest, morphed), changes(rest, back));
      require(changes(rest, morphed) > 20 && changes(rest, back) == 0, "the core's morphed positions drawn");
    }

    // a model without a known source is counted as missing (other data than
    // the source remembered for it)
    frame.models[0].beFlag = 1 | 2 | 4;
    std::memset(&frame.infos[0].bin, 0, sizeof(Re4CoreSource));
    frame.infos[0].bin.entry = -1;
    frame.infos[0].binAddress = 0x82FF0000u;
    scene.draw(frame, kWidth, kHeight, kStill);
    require(scene.status().find("drawn 0, missing 1") != std::string::npos, "unknown source missing");

    // nested and packed sources are drawn; scenery is left to the room scene
    frame.eye = {-3000, 900, -600};
    frame.infos[0].bin = inner;
    setSource(frame.infos[0].tpl, "em/em98.drs", 0x400, unsigned(archive2.size()), 1);
    scene.draw(frame, kWidth, kHeight, kStill);
    std::fprintf(stderr, "status: %s\n", scene.status().c_str());
    require(scene.status().find("drawn 1, missing 0") != std::string::npos, "nested source drawn");
    setSource(frame.infos[0].bin, "st1/r100.das", 0xFFFFFFFFu, 0, 2);
    setSource(frame.infos[0].tpl, "st1/r100.das", 0xFFFFFFFFu, 0, 3);
    scene.draw(frame, kWidth, kHeight, kStill);
    require(scene.status().find("drawn 1, missing 0") != std::string::npos, "packed room source drawn");
    frame.models[0].manager = 1;
    frame.models[0].id = 2;
    frame.models[0].scrollBlock = -1;
    frame.models[0].scrollIndex = 0;  // the room SMD's floor, where the room scene has it
    const auto savedMatrix = frame.models[0];
    std::copy(floorPlacement.begin(), floorPlacement.end(), frame.models[0].matrix);
    scene.draw(frame, kWidth, kHeight, kStill);
    require(scene.status().find("drawn 0, missing 0, scenery 1") != std::string::npos, "scenery left to the room");
    std::copy(savedMatrix.matrix, savedMatrix.matrix + 12, frame.models[0].matrix);
    frame.models[0].scrollIndex = RE4_CORE_NO_SCROLL;  // a room script's scenery (SetObjSmd)
    scene.draw(frame, kWidth, kHeight, kStill);
    std::fprintf(stderr, "status: %s\n", scene.status().c_str());
    require(scene.status().find("drawn 1, missing 0, scenery 0") != std::string::npos, "script scenery drawn");
    frame.models[0].manager = 0;
    frame.screen = RE4_CORE_SCREEN_SUB;
    scene.draw(frame, kWidth, kHeight, kStill);
    require(scene.status().find("MENU OPEN") != std::string::npos, "open sub screen reported");

    // an event model: data at an offset of the event file
    frame.screen = 0;
    setSource(frame.infos[0].bin, "evd/r100s00.evd", 0x400, unsigned(eventPart.size()), -2);
    frame.infos[0].bin.innerOffset = 0x100;
    setSource(frame.infos[0].tpl, "evd/r100s00.evd", 0x400, unsigned(eventPart.size()), -2);
    frame.infos[0].tpl.innerOffset = tplOffset;
    require(re4::ReadCoreSource(root, 0, frame.infos[0].bin).size() == eventPart.size() - 0x100,
            "event file data from its offset");
    frame.models[0].address = 0x82002000u;
    frame.infos[0].binAddress = 0x82100100u;
    frame.infos[0].tplAddress = 0x82100100u + tplOffset - 0x100;
    scene.draw(frame, kWidth, kHeight, kStill);
    require(scene.status().find("drawn 1, missing 0") != std::string::npos, "event file model drawn");
    // the core loses the source (memory swapped out): the same data is still drawn
    std::memset(&frame.infos[0].bin, 0, sizeof(Re4CoreSource));
    std::memset(&frame.infos[0].tpl, 0, sizeof(Re4CoreSource));
    frame.infos[0].bin.entry = frame.infos[0].tpl.entry = -1;
    scene.draw(frame, kWidth, kHeight, kStill);
    require(scene.status().find("drawn 1, missing 0") != std::string::npos, "last known source kept");
    frame.infos[0].binAddress += 0x20;  // other data: not the remembered source
    scene.draw(frame, kWidth, kHeight, kStill);
    require(scene.status().find("drawn 0, missing 1") != std::string::npos, "other data not mistaken");

    // the infos as the game draws them
    frame.eye = {-3000, 900, -600};
    frame.models[0].address = 0x82003000u;
    frame.infos[0].binAddress = 0x82200000u;
    frame.infos[0].bin = bin;
    frame.infos[0].tpl = tpl;
    frame.infos[0].flags = 0;  // hidden by the game
    scene.draw(frame, kWidth, kHeight, kStill);
    require(scene.status().find("drawn 0, missing 0") != std::string::npos, "hidden info not drawn");
    frame.infos[0].flags = 8;
    // a rigid model (one part, no weights) hung on part 1: it follows part 1, not part 0
    setSource(frame.infos[0].bin, "em/em97.drs", 0x400, unsigned(archive3.size()), 0);
    setSource(frame.infos[0].tpl, "em/em97.drs", 0x400, unsigned(archive3.size()), 1);
    frame.infos[0].binAddress = 0x82300000u;
    frame.infos[0].rigidPart = 1;
    frame.eye = {0, 6000, -3000};
    frame.target = {0, 0, 0};
    scene.draw(frame, kWidth, kHeight, kStill);
    require(scene.status().find("drawn 1, missing 0") != std::string::npos, "rigid info drawn");
    const auto rigidRest = pixels();
    frame.parts[0].matrix[3] = 1500;  // part 0 moves: the rigid model stays
    scene.draw(frame, kWidth, kHeight, kStill);
    const auto rootMoved = pixels();
    frame.parts[0].matrix[3] = 0;
    // part 1 moves: the rigid model (a plane like the floor) goes with it, up
    // towards the camera
    frame.parts[1].matrix[7] = 1000;
    scene.draw(frame, kWidth, kHeight, kStill);
    const auto partMoved = pixels();
    frame.parts[1].matrix[7] = 0;
    std::fprintf(stderr, "rigid: root moved %u, its part moved %u pixels\n", changes(rigidRest, rootMoved),
                 changes(rigidRest, partMoved));
    require(changes(rigidRest, rootMoved) == 0 && changes(rigidRest, partMoved) > 20, "rigid info on its part");
    // the info's own matrix moves it as well
    frame.infos[0].matrix[7] = 1000;
    scene.draw(frame, kWidth, kHeight, kStill);
    require(changes(rigidRest, pixels()) > 20, "info matrix applied");
    frame.infos[0].matrix[7] = 0;
    // every info of every model (Leon has nine: the hair is the sixth), the
    // models' infos side by side in one list
    frame.infos.assign(9, frame.infos[0]);
    frame.models[0].nInfos = 9;
    frame.models.push_back(frame.models[0]);
    frame.models[1].address = 0x82004000u;
    frame.models[1].firstInfo = 8;
    frame.models[1].nInfos = 1;
    scene.draw(frame, kWidth, kHeight, kStill);
    std::fprintf(stderr, "status: %s\n", scene.status().c_str());
    require(scene.status().find("drawn 10, missing 0") != std::string::npos, "all infos of all models drawn");
    frame.models.resize(1);
    frame.infos.resize(1);
    frame.models[0].nInfos = 1;

    // the room scene follows the core's scroll objects: the floor (SMD work 0)
    {
      const auto saved = frame;
      const auto &placement = floorPlacement;
      Re4CoreModel floor{};
      floor.address = 0x82005000u;
      floor.beFlag = 1 | 2 | 4;
      floor.opacity = 1;
      floor.manager = 1;
      floor.id = 2;
      floor.nParts = 1;
      floor.scrollBlock = -1;
      floor.scrollIndex = 0;
      std::copy(placement.begin(), placement.end(), floor.matrix);
      frame.models = {floor};
      frame.infos.clear();
      frame.parts.assign(1, Re4CorePart{});
      frame.eye = {0, 6000, -3000};
      frame.target = {0, 0, 0};
      scene.draw(frame, kWidth, kHeight, kStill);
      std::fprintf(stderr, "status: %s\n", scene.status().c_str());
      require(scene.status().find("scenery 1, hidden 0, moved 0") != std::string::npos, "floor where it was built");
      const auto shown = pixels();
      frame.models[0].beFlag = 1 | 4;  // the game hides it
      scene.draw(frame, kWidth, kHeight, kStill);
      const auto hidden = pixels();
      require(scene.status().find("hidden 1") != std::string::npos, "hidden floor counted");
      frame.models[0].beFlag = 1 | 2 | 4;
      frame.models[0].matrix[7] += 1000;  // a script moves it (up: it covers more of the marker at the feet)
      scene.draw(frame, kWidth, kHeight, kStill);
      const auto moved = pixels();
      require(scene.status().find("moved 1") != std::string::npos, "moved floor counted");
      // the core names no SMD work the scene holds (another, unused work): the
      // scene stays as it was built (a work the core lacks while it names
      // others, like a scroll block not loaded, is hidden; the sample SMD has
      // only one work)
      frame.models[0].matrix[7] -= 1000;
      frame.models[0].scrollIndex = 1;
      scene.draw(frame, kWidth, kHeight, kStill);
      const auto absent = pixels();
      std::fprintf(stderr, "scenery: hidden %u, moved %u, absent %u pixels changed\n", changes(shown, hidden),
                   changes(shown, moved), changes(shown, absent));
      require(changes(shown, hidden) > 20 && changes(shown, moved) > 20 && changes(shown, absent) == 0,
              "room scene follows the core's scroll objects");
      frame = saved;
    }

    std::string cleanup = "rm -rf '" + root + "'";
    if (std::system(cleanup.c_str()) != 0) std::fprintf(stderr, "cleanup failed\n");
    std::printf("core_scene_gl_check: %u checks passed\n", checks);
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "core_scene_gl_check FAILED after %u checks: %s\n", checks, e.what());
    return 1;
  }
}
