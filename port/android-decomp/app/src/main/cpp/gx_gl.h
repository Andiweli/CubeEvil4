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
  unsigned efbRestores = 0;      // MSAA on the tiles: the EFB drawn back after a pass ended (GPU step d2)
  // GL work per draw (what costs driver time): uniform block uploads and their
  // bytes, program switches, texture and sampler binds, render state changes,
  // vertex attribute setups
  unsigned uniformUploads = 0, uniformBytes = 0, programSwitches = 0, textureBinds = 0, stateChanges = 0,
           attributeSetups = 0;
  unsigned streamWaits = 0;  // the uniform stream waited for the GPU before reusing a quarter
  // the uniform stream moved on to its next quarter: in all, and inside a chunk (a chunk larger
  // than the room left; between chunks otherwise, gx_gl.cpp)
  unsigned streamSwitches = 0, streamSplits = 0;
  // GPU time of the chunks (EXT_disjoint_timer_query; 0 when the driver has none)
  unsigned long long gpuMicros = 0;
  unsigned gpuTimed = 0;
  // the pictures put on the screen (Present, once per display refresh) and their GPU time
  unsigned presents = 0;
  unsigned long long presentMicros = 0;
  unsigned presentTimed = 0;
  // how far the GPU is behind, without its timer: a fence after every chunk; after the next
  // chunk the fences it has not reached are chunks it still works on. Summed over the checks,
  // and how many checks found 0, 1, ... 8 behind (8: kFences of gx_gl.cpp, or more)
  unsigned long long behindSum = 0;
  unsigned behindChecks = 0;
  unsigned behindCount[9] = {};
  // in-between pictures (60 FPS, Interpolate): shown, refused as a cut; their perspective draws,
  // of them matched to a draw of the picture before, and draws with blended vertices
  unsigned interpPictures = 0, interpCuts = 0, interpDraws = 0, interpMatched = 0, interpVertexBlends = 0;
};

// Picture options of the app's menu (M11), applied by Init: multisampling of
// the EFB (resolved before every copy out of it) and anisotropic filtering of
// the game's mipmapped, linearly filtered textures. Init keeps what the device
// can do (Samples(), Anisotropy()).
struct GxOptions {
  int samples = 1;          // 1 (off), 2, 4
  float anisotropy = 1.0f;  // 1 (off) .. 16
  // the GPU may forget what a copy's target, the blur and the focus of the autofocus held before
  // they are drawn over completely (glInvalidateFramebuffer, GPU step b, 19.10.2026); poison
  // (tests, RE4_GX_INVALIDATE_POISON=1 in gx_chunk_render): they are painted magenta instead, so
  // a picture that still needed the old content shows it
  bool invalidate = false;  // on the Retroid it saved nothing, at scale 3 alone it cost about 7 %
  bool poison = false;
  // MSAA on the tiles (GPU step d, 19.10.2026; d2 since 19.10.2026): the EFB's own textures take
  // the samples (EXT_multisampled_render_to_texture2); off, or without the extension: separate
  // multisampled buffers, resolved with a blit before every copy. The extension leaves the
  // samples undefined after a pass ends (the Retroid's Adreno 650 kept the colour, lost the depth:
  // Leon's hair, fog, whole objects missing), so d2 draws the EFB back itself at the start of
  // every new pass, from a second set of textures. Off: with d2 too the Retroid lost Leon's hair
  // and parts of textures although its self-test passed (decision of the project owner,
  // 19.10.2026: MSAA in own buffers)
  bool tiledMsaa = false;
  // tests (RE4_GX_LOSE_TILES in gx_chunk_render): 1, every new pass on the tiles starts with
  // magenta and depth 0 before the EFB is drawn back, as a GPU that loses everything would; 2,
  // besides, the resolved depth never reaches the texture (0 there), as the Adreno 650 seems to do:
  // the self-test at Init finds it and takes own buffers
  int loseTiles = 0;
  // the depth of field's blur at half the resolution, mixed into the picture at the full one
  // (GPU step e, 19.10.2026); off: the blur and the copy back at the full resolution
  bool halfDof = true;
  // MSAA: the shaders' texture coordinates and colours at a sample the triangle covers
  // (centroid), not at the pixel's centre. At the edge of a thin cut-out (branches: an alpha
  // test) the centre can lie outside the triangle; its coordinate then points past the cut-out
  // and the alpha test drops or keeps the edge from picture to picture (the Retroid, 20.10.2026:
  // thin branches flickered with 2x and 4x MSAA only). Without MSAA it changes nothing
  bool centroid = true;
  // MSAA in own buffers: every resolve of the whole EFB, not only of the region a copy reads
  // (a test of the device replay)
  bool fullResolve = false;
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
  // the copy targets are forgotten before they are drawn over (GxOptions::invalidate)
  bool Invalidates() const;
  // the samples live in the GPU's tile memory (GxOptions::tiledMsaa and the device can)
  bool TiledMsaa() const;
  // the depth of field's blur at half the resolution (GxOptions::halfDof)
  bool HalfDof() const;
  // What the device can do (a GLES 3.0 context is current): the most EFB
  // samples and the strongest anisotropic filtering (1: none).
  static void Limits(int *maxSamples, float *maxAnisotropy);
  // Runs one chunk (in sequence order).
  void Run(const Re4GxChunk &chunk);
  // 60 FPS (experimental, decision of the project owner 22.10.2026): on, every Run notes how each
  // of its draws was drawn (the position and normal matrices, the projection) for Interpolate.
  void SetRecording(bool on);
  // The picture between `prev`, the chunk run last (the caller still holds it: its vertices), and
  // `next` at t (0 prev .. 1 next), into the XFB: `next` once more, every draw that matches one
  // of `prev` with its matrices (and changed vertices, the figures the game skins on the CPU)
  // blended. The renderer's state, EFB copies and the autofocus stay as after `prev`: Run(next)
  // follows as without it. False when it is no picture to show (a cut of the camera, a chunk that
  // starts over, held or black, nothing noted): run `next` at once.
  bool Interpolate(const Re4GxChunk &prev, const Re4GxChunk &next, float t);
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

// The renderer's and the core mode's log lines: to logcat under `tag` and, once SetLogCopy gave a
// place, also as "tag: text" there (core_mode.cpp: the core's log file; Android 10 and older give
// the app no logcat to record, the error report then has only that file).
void LogTo(const char *tag, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void SetLogCopy(void (*copy)(const char *line));

} // namespace re4gx
