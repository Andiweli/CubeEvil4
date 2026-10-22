// GX frames of the game core (RE4_GX_DUMP, re4_gx_frame_io.h) drawn by the OUYA's GLES 2 renderer
// and by the main app's GLES 3 one (as gx_chunk_render does), the pictures compared: the check
// with real game pictures on the PC (port/ouya/tools/compare_frames.sh makes the frames).
//
//   gx_gl2_compare OUTPUT_DIR gx-*.bin
//
// Frames with compact vertices (RE4_GX_COMPACT=1, as the core writes them for the OUYA) go to the
// GLES 3 renderer as float vertices (turned back as port/whole/tests/gx_probe.cpp does), to the
// GLES 2 one as they are. Writes OUTPUT_DIR/<name>-gles3.ppm, <name>-gles2.ppm and, where they
// differ, <name>-diff.ppm, and one line per frame. A frame that directly follows the one before
// (next sequence number and frame) runs on the same renderers, any other starts over (it must
// be a RESET chunk), as in gx_chunk_render.
//
// RE4_GX2_TEGRA3=1: the GLES 2 renderer as on the OUYA; RE4_GX2_EXACT=1: with the GLES 3
// renderer's 1/128-texel rounding (GxOptions::texelCoords); RE4_GX2_ALLOWED: the share of a
// picture's pixels that may differ by more than 2 (default 0.02). RE4_GX2_DEVICE=DIR: also the
// pictures a device drew (DIR/<name>-ouya.ppm, gx_gl2_render on the OUYA) against the GLES 3
// ones, with RE4_GX2_DEVICE_ALLOWED (default 0.05; the Tegra 3 computes with fp20 and 16-bit
// depth, and its mipmap chains go on where GX's stop).
#include "gx_gl2.h"
#include "gx_test_common.h"
#include "re4_gx_frame.h"
#include "re4_gx_frame_io.h"

#include <GLES2/gl2.h>

#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using ouyatest::Picture;

namespace {

std::string Stem(const std::string &path) {
  const auto slash = path.find_last_of('/');
  std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  const auto dot = name.rfind('.');
  return dot == std::string::npos ? name : name.substr(0, dot);
}

bool ReadPpm(const std::string &path, Picture &p) {
  FILE *f = std::fopen(path.c_str(), "rb");
  if (!f)
    return false;
  unsigned w = 0, h = 0, max = 0;
  const bool ok = std::fscanf(f, "P6 %u %u %u", &w, &h, &max) == 3 && std::fgetc(f) != EOF;
  if (ok) {
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(w) * h * 3);
    if (std::fread(rgb.data(), 1, rgb.size(), f) == rgb.size()) {
      p.width = w;
      p.height = h;
      p.rgba.resize(static_cast<std::size_t>(w) * h * 4);
      for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; i++) {
        std::memcpy(&p.rgba[i * 4], &rgb[i * 3], 3);
        p.rgba[i * 4 + 3] = 255;
      }
    }
  }
  std::fclose(f);
  return !p.rgba.empty();
}

struct Frame {
  std::string path, stem;
  re4gxio::Loaded loaded;
  bool ok = false;
};

// The chain of frames as gx_chunk_render runs them: `start` true where a renderer starts over.
std::vector<bool> Starts(const std::vector<Frame> &frames) {
  std::vector<bool> starts(frames.size(), true);
  for (std::size_t i = 1; i < frames.size(); i++)
    starts[i] = !(frames[i - 1].ok && frames[i].ok && frames[i].loaded.chunk.sequence == frames[i - 1].loaded.chunk.sequence + 1 &&
                  frames[i].loaded.frame == frames[i - 1].loaded.frame + 1);
  return starts;
}

// The GLES 3 pictures, in a child process.
int RunReference(const std::string &out, std::vector<Frame> &frames) {
  std::string error;
  if (!ouyatest::MakeContext(3, &error)) {
    std::fprintf(stderr, "reference: %s\n", error.c_str());
    return 1;
  }
  const std::vector<bool> starts = Starts(frames);
  std::unique_ptr<ouyatest::Reference> reference;
  for (std::size_t i = 0; i < frames.size(); i++) {
    if (!frames[i].ok)
      continue;
    if (starts[i]) {
      reference = std::make_unique<ouyatest::Reference>();
      if (!reference->Init(&error)) {
        std::fprintf(stderr, "reference: %s\n", error.c_str());
        return 1;
      }
    }
    Re4GxChunk chunk = frames[i].loaded.chunk;
    if (starts[i])
      chunk.flags &= ~RE4_GX_CHUNK_HOLD;
    // without the autofocus markers: the GLES 2 renderer has no depth of field
    ouyatest::FloatChunk storage;
    Re4GxChunk floats;
    ouyatest::ToFloatVertices(chunk, storage, floats, true);
    reference->Run(floats);
    const Picture xfb = reference->Xfb();
    if (!xfb.rgba.empty())
      ouyatest::WritePpm(out + "/" + frames[i].stem + "-gles3.ppm", xfb);
  }
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: gx_gl2_compare OUTPUT_DIR gx-*.bin\n");
    return 2;
  }
  const std::string out = argv[1];
  std::vector<Frame> frames;
  for (int i = 2; i < argc; i++) {
    Frame f;
    f.path = argv[i];
    f.stem = Stem(f.path);
    if (FILE *file = std::fopen(f.path.c_str(), "rb")) {
      f.ok = re4gxio::Read(file, f.loaded);
      std::fclose(file);
    }
    frames.push_back(std::move(f));
  }
  const pid_t child = fork();
  if (child == 0) {
    unsetenv("MESA_GLES_VERSION_OVERRIDE");
    const int code = RunReference(out, frames);
    std::fflush(stderr);
    _exit(code);
  }
  int status = 0;
  waitpid(child, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    std::fprintf(stderr, "FATAL: the GLES 3 reference failed (status %d)\n", status);
    return 1;
  }
  setenv("MESA_GLES_VERSION_OVERRIDE", "2.0", 1);
  std::string error;
  if (!ouyatest::MakeContext(2, &error)) {
    std::fprintf(stderr, "FATAL: %s\n", error.c_str());
    return 1;
  }
  const char *allowedText = std::getenv("RE4_GX2_ALLOWED");
  const double allowed = allowedText ? std::atof(allowedText) : 0.02;
  const char *exact = std::getenv("RE4_GX2_EXACT");
  const std::vector<bool> starts = Starts(frames);
  std::unique_ptr<re4gx2::GxRenderer> renderer;
  int failures = 0;
  unsigned compared = 0, worstPixels = 0;
  for (std::size_t i = 0; i < frames.size(); i++) {
    const Frame &f = frames[i];
    if (!f.ok) {
      std::printf("%s: not readable\n", f.stem.c_str());
      failures++;
      continue;
    }
    if (starts[i]) {
      if (!(f.loaded.chunk.flags & RE4_GX_CHUNK_RESET)) {
        std::printf("%s: follows a frame that is not there (chunk %u), left out\n", f.stem.c_str(),
                    f.loaded.chunk.sequence);
        continue;
      }
      if (renderer)
        renderer->Release();
      renderer = std::make_unique<re4gx2::GxRenderer>();
      re4gx2::GxOptions options;
      options.texelCoords = exact && *exact == '1';
      renderer->SetOptions(options);
      if (!renderer->Init(1, &error)) {
        std::printf("%s: renderer: %s\n", f.stem.c_str(), error.c_str());
        return 1;
      }
    }
    Re4GxChunk chunk = f.loaded.chunk;
    if (starts[i])
      chunk.flags &= ~RE4_GX_CHUNK_HOLD;
    const re4gx2::GxStats before = renderer->Stats();
    renderer->Run(chunk);
    const re4gx2::GxStats &s = renderer->Stats();
    unsigned w = 0, h = 0;
    const GLuint xfbTex = renderer->Xfb(&w, &h);
    Picture mine;
    if (xfbTex) {
      GLuint fbo = 0;
      glGenFramebuffers(1, &fbo);
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, xfbTex, 0);
      mine.width = w;
      mine.height = h;
      mine.rgba.resize(static_cast<std::size_t>(w) * h * 4);
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadPixels(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h), GL_RGBA, GL_UNSIGNED_BYTE, mine.rgba.data());
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers(1, &fbo);
      ouyatest::WritePpm(out + "/" + f.stem + "-gles2.ppm", mine);
    }
    Picture reference;
    std::string verdict = "no picture";
    bool bad = s.shaderErrors > before.shaderErrors;
    if (ReadPpm(out + "/" + f.stem + "-gles3.ppm", reference) && !mine.rgba.empty()) {
      const ouyatest::Difference d = ouyatest::Compare(reference, mine, 2, false);
      char text[96];
      std::snprintf(text, sizeof(text), "%u pixels off (%.2f %%, worst %u)", d.pixels,
                    100.0 * d.pixels / (static_cast<double>(w) * h), d.worst);
      verdict = d.sizeDiffers ? "SIZE DIFFERS" : text;
      compared++;
      worstPixels = std::max(worstPixels, d.pixels);
      if (d.sizeDiffers || d.pixels > allowed * w * h) {
        bad = true;
        ouyatest::WritePpm(out + "/" + f.stem + "-diff.ppm", ouyatest::DifferencePicture(reference, mine));
      }
    }
    // the device's picture of the frame against the same GLES 3 one
    const char *deviceDir = std::getenv("RE4_GX2_DEVICE");
    if (deviceDir && *deviceDir && !reference.rgba.empty()) {
      const char *deviceAllowedText = std::getenv("RE4_GX2_DEVICE_ALLOWED");
      const double deviceAllowed = deviceAllowedText ? std::atof(deviceAllowedText) : 0.05;
      Picture device;
      if (!ReadPpm(std::string(deviceDir) + "/" + f.stem + "-ouya.ppm", device)) {
        verdict += "; device: no picture";
        bad = true;
      } else {
        const ouyatest::Difference d = ouyatest::Compare(reference, device, 2, false);
        char text[96];
        std::snprintf(text, sizeof(text), "; device: %u pixels off (%.2f %%, worst %u)", d.pixels,
                      100.0 * d.pixels / (static_cast<double>(reference.width) * reference.height), d.worst);
        verdict += d.sizeDiffers ? "; device: SIZE DIFFERS" : text;
        if (d.sizeDiffers || d.pixels > deviceAllowed * reference.width * reference.height) {
          bad = true;
          ouyatest::WritePpm(out + "/" + f.stem + "-device-diff.ppm", ouyatest::DifferencePicture(reference, device));
        }
      }
    }
    std::printf("%s %s (frame %u%s): %u draws (%u compact), %u triangles, %u textures (%u levels made), %u copies, "
                "%u shaders%s%s%s%s: %s\n",
                bad ? "FAIL" : "ok  ", f.stem.c_str(), f.loaded.frame, starts[i] ? "" : ", follows",
                s.draws - before.draws, s.compactDraws - before.compactDraws, s.triangles - before.triangles,
                s.textures - before.textures, s.mipmapsMade - before.mipmapsMade, s.textureCopies - before.textureCopies,
                s.programs,
                s.autofocusMarks > before.autofocusMarks ? ", autofocus marker (left out by both)" : "",
                s.missingTextures > before.missingTextures ? ", missing textures" : "",
                s.skippedDraws > before.skippedDraws ? ", draws left out" : "",
                s.shaderErrors > before.shaderErrors ? ", SHADER ERRORS" : "", verdict.c_str());
    if (bad)
      failures++;
  }
  if (renderer) {
    const re4gx2::GxStats &s = renderer->Stats();
    std::printf("in all: %u frames compared, the most pixels off %u; %u programs, %u shader errors, %u mipmap levels made, "
                "%u NPOT clamped, %u depth copies, %u autofocus markers ignored\n",
                compared, worstPixels, s.programs, s.shaderErrors, s.mipmapsMade, s.npotClamped, s.depthCopies,
                s.autofocusMarks);
    renderer->Release();
  }
  std::printf(failures ? "gx_gl2_compare: %d FAILED\n" : "gx_gl2_compare: all passed\n", failures);
  return failures ? 1 : 0;
}
