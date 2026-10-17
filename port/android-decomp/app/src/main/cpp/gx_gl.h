// The GX renderer of the core mode: executes the chunks the game core's
// emulated graphics processor decodes (port/whole/runtime/re4_gx_frame.h)
// with GLES 3.0. Plan: port/docs/gx-plan.md, step G3.
//
// It keeps the register state the chunks change (BP, XF, the palette part of
// TMEM), draws into its own embedded frame buffer (EFB, 640x528 like the
// GameCube's, colour and depth), turns EFB copies into textures (GXCopyTex)
// and display copies into the picture to show (GXCopyDisp, the XFB). Shaders
// are generated from the state that shapes them (vertex: XF transform,
// lighting channels, texture coordinate generation; fragment: TEV stages,
// alpha test, fog) and cached.
//
// Orientation: the EFB keeps GameCube rows (row 0 = top) at GL row 0, so
// copies, scissor and viewport need no flips; a display shows the XFB upside
// down in GL terms (Present flips it).
#pragma once

#include <GLES3/gl3.h>

#include <cstdint>
#include <memory>
#include <string>

struct Re4GxChunk;

namespace re4gx {

struct GxStats {
  unsigned chunks = 0, draws = 0, triangles = 0;
  unsigned programs = 0;         // shaders built
  unsigned programsLoaded = 0;   // of them taken from the program cache
  unsigned textures = 0;         // textures decoded
  unsigned textureCopies = 0;    // EFB copies to a texture
  unsigned displayCopies = 0;    // EFB copies to the XFB
  unsigned heldCopies = 0;       // of them not shown: the VI kept the picture before (RE4_GX_CHUNK_HOLD)
  unsigned resets = 0;
  unsigned missingTextures = 0;  // bound keys the renderer did not have
  unsigned shaderErrors = 0;
  unsigned dstAlphaApprox = 0;   // draws whose constant destination alpha is not 0 and not exact
  unsigned autofocus = 0;        // blur passes of the gameplay autofocus (BP 0x5A, the core's marker)
  // GL work per draw (what costs driver time): uniform block uploads and their
  // bytes, program switches, texture and sampler binds, render state changes,
  // vertex attribute setups
  unsigned uniformUploads = 0, uniformBytes = 0, programSwitches = 0, textureBinds = 0, stateChanges = 0,
           attributeSetups = 0;
  unsigned streamWaits = 0;  // the uniform stream waited for the GPU before reusing a quarter
  // GPU time of the chunks (EXT_disjoint_timer_query; 0 when the driver has none)
  unsigned long long gpuMicros = 0;
  unsigned gpuTimed = 0;
};

// Picture options of the app's menu (M11), applied by Init: multisampling of
// the EFB (resolved before every copy out of it) and anisotropic filtering of
// the game's mipmapped, linearly filtered textures. Init keeps what the device
// can do (Samples(), Anisotropy()).
struct GxOptions {
  int samples = 1;          // 1 (off), 2, 4
  float anisotropy = 1.0f;  // 1 (off) .. 16
};

class GxRenderer {
public:
  GxRenderer();
  ~GxRenderer();
  GxRenderer(const GxRenderer &) = delete;
  GxRenderer &operator=(const GxRenderer &) = delete;

  // Creates the GL objects (a GLES 3.0 context is current). `scale`: EFB
  // resolution multiple (1 = the GameCube's 640x528).
  bool Init(int scale, std::string *error);
  // Before Init: the picture options (default: none).
  void SetOptions(const GxOptions &options);
  // After Init: the multisampling and the filtering in use.
  int Samples() const;
  float Anisotropy() const;
  // What the device can do (a GLES 3.0 context is current): the most EFB
  // samples and the strongest anisotropic filtering (1: none).
  static void Limits(int *maxSamples, float *maxAnisotropy);
  // Runs one chunk (in sequence order).
  void Run(const Re4GxChunk &chunk);
  // The latest display copy: an RGBA8 texture, GameCube row 0 at GL row 0;
  // 0 before the first display copy.
  GLuint Xfb(unsigned *width, unsigned *height) const;
  // Draws the latest display copy into the current framebuffer's viewport
  // (x, y, w, h), upright; rows > 0: only XFB rows top .. top + rows - 1
  // (the letterbox cut off). False when there is none yet.
  bool Present(int x, int y, int w, int h, unsigned top = 0, unsigned rows = 0);
  // EFB colour texture (tests): GameCube coordinates times the scale.
  GLuint EfbColor() const;
  int Scale() const;
  const GxStats &Stats() const;
  // True when draws use glDrawElementsBaseVertex (one attribute setup per layout).
  bool BaseVertex() const;
  // Keeps the linked shader programs in `folder` (it exists) and takes them
  // from there next time instead of compiling (the stutter of a new effect);
  // false when the driver has no program binaries (nothing is kept then).
  bool SetProgramCache(const std::string &folder);
  // Frees the GL objects (context current).
  void Release();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace re4gx
