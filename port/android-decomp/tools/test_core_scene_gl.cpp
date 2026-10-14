// Core mode renderer (core_scene.cpp) on a headless GLES3 context with
// synthetic disc data (the samples of the format comparison): no disc data
// needed, runs in the cloud.
//
//   - a disc folder with em/em99.drs (DVD container: archive with BIN + TPL),
//     a decoded room archive (SMD with a rigid floor + LIT)
//   - the path of a core source is found case-insensitively; a Yz2-packed
//     source is refused with a message
//   - before the first room (step 0) the title path runs (no title data here:
//     black) and no room is wanted
//   - in the room (step 3) the room is built from the prepared file, the
//     camera comes from the frame, and the core's model is drawn with its part
//     matrices: the picture differs from the same frame with the model hidden
//     (be_flag without the draw bits), from one of two opposite cameras, and
//     moves when the part matrices move
#include "core_scene.h"

#include "../../whole/tests/format_compare/samples.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <cstdio>
#include <cstdlib>
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
    const std::string roomPath = root + "/room.arc";
    writeFile(roomPath, makeArchive({{"SMD", makeSmd(makeRigidBin())}, {"LIT", makeLit()}}));

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

    context();
    re4::CoreScene scene(root);
    scene.contextCreated();

    // title (no title data in the synthetic disc: black)
    re4::CoreFrame frame;
    frame.valid = true;
    frame.frame = 10;
    frame.disc = 0;
    frame.room = 0x120;
    scene.draw(frame, kWidth, kHeight, 1 / 60.);
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
    model.nInfos = 1;
    model.matrix[0] = model.matrix[5] = model.matrix[10] = 1;
    model.infos[0].bin = bin;
    model.infos[0].tpl = tpl;
    frame.models = {model};
    frame.parts.assign(2, Re4CorePart{});
    for (auto &p : frame.parts) p.matrix[0] = p.matrix[5] = p.matrix[10] = 1;
    scene.draw(frame, kWidth, kHeight, 1 / 60.);
    require(scene.wantedRoom() == 0x100, "the room is wanted");
    scene.roomPrepared(0x100, roomPath, "");

    unsigned best = 0;
    for (float side : {-1.f, 1.f}) {
      frame.eye = {3000 * side, 900, -600};
      frame.target = {0, 700, -600};
      frame.models[0].beFlag = 1 | 2 | 4;
      scene.draw(frame, kWidth, kHeight, 1 / 60.);
      const auto status = scene.status();
      std::fprintf(stderr, "status: %s\n", status.c_str());
      require(status.find("room loaded") != std::string::npos, "room built");
      require(status.find("drawn 1, missing 0") != std::string::npos, "model drawn");
      const auto with = pixels();
      frame.models[0].beFlag = 1;  // alive, not drawn
      scene.draw(frame, kWidth, kHeight, 1 / 60.);
      require(scene.status().find("drawn 0, missing 0") != std::string::npos, "hidden model skipped");
      const auto without = pixels();
      best = std::max(best, changes(with, without));
    }
    std::fprintf(stderr, "model pixels: %u\n", best);
    require(best > 20, "the model appears in the picture");

    // the core's part matrices pose the model: lifting both parts moves it
    frame.eye = {-3000, 900, -600};
    frame.models[0].beFlag = 1 | 2 | 4;
    scene.draw(frame, kWidth, kHeight, 1 / 60.);
    const auto rest = pixels();
    for (auto &p : frame.parts) p.matrix[7] = 400;
    scene.draw(frame, kWidth, kHeight, 1 / 60.);
    const auto lifted = pixels();
    for (auto &p : frame.parts) p.matrix[7] = 0;
    std::fprintf(stderr, "pose change pixels: %u\n", changes(rest, lifted));
    require(changes(rest, lifted) > 20, "the part matrices move the model");

    // a model without a known source is counted as missing
    frame.models[0].beFlag = 1 | 2 | 4;
    std::memset(&frame.models[0].infos[0].bin, 0, sizeof(Re4CoreSource));
    frame.models[0].infos[0].bin.entry = -1;
    scene.draw(frame, kWidth, kHeight, 1 / 60.);
    require(scene.status().find("drawn 0, missing 1") != std::string::npos, "unknown source missing");

    std::string cleanup = "rm -rf '" + root + "'";
    if (std::system(cleanup.c_str()) != 0) std::fprintf(stderr, "cleanup failed\n");
    std::printf("core_scene_gl_check: %u checks passed\n", checks);
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "core_scene_gl_check FAILED after %u checks: %s\n", checks, e.what());
    return 1;
  }
}
