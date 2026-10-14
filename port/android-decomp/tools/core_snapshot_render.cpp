// Draws snapshots of the game core (RE4_BRIDGE_SNAPSHOT, port/whole/runtime/
// core_api.cpp) exactly as the app's core mode does (core_scene.cpp), on a
// headless GLES3 context, with the disc data: the local r100 check
// (port/tools/whole_r100_check.sh) shows these pictures before any device test.
//
//   core_snapshot_render GAME_ROOT OUTPUT_DIR SNAPSHOT.bin...
//
// GAME_ROOT holds the discs as the app imports them (disc1/files, disc2/files).
// The room archive is unpacked with the app's Yz2 decoder and the r100 block
// files are copied next to it, as RoomRepository prepares them on the device.
// Writes OUTPUT_DIR/<snapshot name>.ppm and one status line per snapshot.
#include "core_scene.h"

#include "archive.h"
#include "ports/yz2.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const int kWidth = 960, kHeight = 540;

void context() {
  auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
      eglGetProcAddress("eglGetPlatformDisplayEXT"));
  EGLDisplay display = getDisplay ? getDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)
                                  : eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint major{}, minor{};
  if (!eglInitialize(display, &major, &minor)) throw std::runtime_error("EGL initialise");
  const EGLint attributes[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE};
  EGLConfig config{};
  EGLint count{};
  eglChooseConfig(display, attributes, &config, 1, &count);
  eglBindAPI(EGL_OPENGL_ES_API);
  const EGLint contextAttributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};
  EGLContext context = eglCreateContext(display, count ? config : nullptr, EGL_NO_CONTEXT, contextAttributes);
  if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context))
    throw std::runtime_error("EGL context");
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
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    throw std::runtime_error("framebuffer");
}

std::vector<std::uint8_t> readFile(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read " + path);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void writeFile(const std::string &path, const std::vector<std::uint8_t> &bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  if (!out) throw std::runtime_error("cannot write " + path);
}

// The decoded room archive and its block files, as RoomRepository prepares them.
std::string prepareRoom(const std::string &root, const std::string &cache, unsigned room, int disc) {
  char name[32];
  std::snprintf(name, sizeof(name), "st%x/r%03x.das", room >> 8, room);
  std::string source = re4::ResolveCoreDiscPath(root, disc, name);
  if (source.empty()) source = re4::ResolveCoreDiscPath(root, disc == 0 ? 1 : 0, name);
  if (source.empty()) throw std::runtime_error(std::string("room archive missing: ") + name);
  std::snprintf(name, sizeof(name), "/room-r%03x.dat", room);
  const std::string target = cache + name;
  writeFile(target, re4::LoadYz2(source).bytes);
  for (unsigned i = 0; i < 16; ++i) {
    char block[32];
    std::snprintf(block, sizeof(block), "st%x/r%03x_%02x.dat", room >> 8, room, i);
    const auto path = re4::ResolveCoreDiscPath(root, disc, block);
    if (path.empty()) continue;
    std::snprintf(block, sizeof(block), "/room-r%03x_%02x.dat", room, i);
    writeFile(cache + block, readFile(path));
  }
  return target;
}

void writePpm(const std::string &path) {
  std::vector<unsigned char> rgba(kWidth * kHeight * 4);
  glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
  std::ofstream out(path, std::ios::binary);
  out << "P6\n" << kWidth << " " << kHeight << "\n255\n";
  for (int y = kHeight - 1; y >= 0; --y)
    for (int x = 0; x < kWidth; ++x) out.write(reinterpret_cast<const char *>(&rgba[(y * kWidth + x) * 4]), 3);
  if (!out) throw std::runtime_error("cannot write " + path);
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: core_snapshot_render GAME_ROOT OUTPUT_DIR SNAPSHOT.bin...\n");
    return 2;
  }
  try {
    const std::string root = argv[1], out = argv[2];
    context();
    re4::CoreSceneLogBounds = true;
    re4::CoreScene scene(root);
    scene.contextCreated();
    int failed = 0;
    unsigned preparedRoom = 0xFFFF;
    for (int i = 3; i < argc; ++i) {
      const std::string path = argv[i];
      const auto bytes = readFile(path);
      if (bytes.size() != sizeof(Re4CoreSnapshot)) {
        std::printf("%s: wrong size %zu (expected %zu)\n", path.c_str(), bytes.size(), sizeof(Re4CoreSnapshot));
        ++failed;
        continue;
      }
      auto snapshot = std::make_unique<Re4CoreSnapshot>();
      std::memcpy(snapshot.get(), bytes.data(), bytes.size());
      re4::CoreFrame frame;
      re4::CopyCoreFrame(*snapshot, frame);
      if (frame.room != preparedRoom) {
        try {
          scene.roomPrepared(frame.room, prepareRoom(root, out, frame.room, frame.disc < 0 ? 0 : frame.disc), "");
        } catch (const std::exception &e) {
          scene.roomPrepared(frame.room, "", e.what());
        }
        preparedRoom = frame.room;
      }
      // twice: the first frame builds the room and loads the models
      scene.draw(frame, kWidth, kHeight, 1 / 30.);
      scene.draw(frame, kWidth, kHeight, 1 / 30.);
      auto name = path.substr(path.find_last_of('/') + 1);
      name = name.substr(0, name.find_last_of('.'));
      writePpm(out + "/" + name + ".ppm");
      auto status = scene.status();
      for (auto &c : status)
        if (c == '\n') c = ';';
      std::printf("%s: %s\n", name.c_str(), status.c_str());
    }
    return failed ? 1 : 0;
  } catch (const std::exception &e) {
    std::printf("core_snapshot_render: %s\n", e.what());
    return 1;
  }
}
