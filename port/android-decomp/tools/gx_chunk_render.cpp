// GX frames of the game core (RE4_GX_DUMP, re4_gx_frame_io.h) drawn with the
// app's GX renderer (gx_gl.cpp) on a headless GLES3 context: the local check
// with disc data (port/tools/whole_r100_check.sh) shows what the core mode
// of the app shows.
//
//   gx_chunk_render OUTPUT_DIR gx-*.bin
//
// Writes OUTPUT_DIR/<name>.ppm (the display copy, 640 wide) and one line per
// frame: size, draws, triangles, textures, copies, shaders, problems.
#include "gx_gl.h"
#include "re4_gx_frame_io.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

bool Context() {
  auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
  EGLDisplay display = getDisplay ? getDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)
                                  : eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint major{}, minor{};
  if (!eglInitialize(display, &major, &minor))
    return false;
  const EGLint attributes[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE};
  EGLConfig config{};
  EGLint count{};
  eglChooseConfig(display, attributes, &config, 1, &count);
  eglBindAPI(EGL_OPENGL_ES_API);
  const EGLint contextAttributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};
  EGLContext context = eglCreateContext(display, count ? config : nullptr, EGL_NO_CONTEXT, contextAttributes);
  return context != EGL_NO_CONTEXT && eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
}

std::string Stem(const std::string &path) {
  const auto slash = path.find_last_of('/');
  std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  const auto dot = name.rfind('.');
  return dot == std::string::npos ? name : name.substr(0, dot);
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: gx_chunk_render OUTPUT_DIR gx-*.bin\n");
    return 2;
  }
  if (!Context()) {
    std::fprintf(stderr, "no EGL/GLES3 context (Mesa packages: port/tools/whole_setup_linux.sh)\n");
    return 1;
  }
  const std::string out = argv[1];
  int failures = 0;
  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  for (int i = 2; i < argc; i++) {
    const std::string path = argv[i];
    re4gxio::Loaded loaded;
    FILE *f = std::fopen(path.c_str(), "rb");
    const bool ok = f && re4gxio::Read(f, loaded);
    if (f)
      std::fclose(f);
    if (!ok) {
      std::printf("%s: nicht lesbar\n", Stem(path).c_str());
      failures++;
      continue;
    }
    re4gx::GxRenderer renderer;
    std::string error;
    if (!renderer.Init(1, &error)) {
      std::printf("%s: Renderer: %s\n", Stem(path).c_str(), error.c_str());
      failures++;
      continue;
    }
    // a held picture (RE4_GX_CHUNK_HOLD) was not on screen; drawn here anyway
    Re4GxChunk chunk = loaded.chunk;
    const bool held = (chunk.flags & RE4_GX_CHUNK_HOLD) != 0;
    chunk.flags &= ~RE4_GX_CHUNK_HOLD;
    renderer.Run(chunk);
    if (held)
      std::printf("%s: Bild war angehalten (nicht auf dem Bildschirm)\n", Stem(path).c_str());
    unsigned w = 0, h = 0;
    const GLuint xfb = renderer.Xfb(&w, &h);
    const re4gx::GxStats &s = renderer.Stats();
    std::printf("%s (Bild %u): %ux%u, %u Zeichenaufträge, %u Dreiecke, %u Texturen, %u Kopien, %u Shader%s%s\n",
                Stem(path).c_str(), loaded.frame, w, h, s.draws, s.triangles, s.textures, s.textureCopies,
                s.programs, s.missingTextures ? ", fehlende Texturen" : "",
                s.shaderErrors ? ", FEHLER: Shader" : "");
    if (s.shaderErrors)
      failures++;
    if (xfb && w && h) {
      std::vector<unsigned char> rgba(static_cast<std::size_t>(w) * h * 4);
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, xfb, 0);
      glReadPixels(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h), GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
      // XFB row 0 is the picture's top row
      const std::string file = out + "/" + Stem(path) + ".ppm";
      if (FILE *p = std::fopen(file.c_str(), "wb")) {
        std::fprintf(p, "P6\n%u %u\n255\n", w, h);
        for (unsigned y = 0; y < h; y++)
          for (unsigned x = 0; x < w; x++)
            std::fwrite(&rgba[(static_cast<std::size_t>(y) * w + x) * 4], 1, 3, p);
        std::fclose(p);
      }
    }
    renderer.Release();
  }
  return failures ? 1 : 0;
}
