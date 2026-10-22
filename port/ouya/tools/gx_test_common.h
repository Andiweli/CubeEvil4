// Shared parts of the OUYA renderer's checks on the PC (gx_gl2_check.cpp, gx_gl2_compare.cpp):
// EGL contexts on Mesa without a window, pictures, the main app's GLES 3 renderer as the
// reference. No GL headers here: the GLES 3 renderer (gx_test_common.cpp) and the GLES 2 one
// (the checks) live in separate translation units.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct Re4GxChunk;

namespace ouyatest {

struct Picture {
  unsigned width = 0, height = 0;
  std::vector<std::uint8_t> rgba; // GL rows: row 0 first (GameCube row 0 for the EFB and XFB)
};

// A GLES `major` (2 or 3) context on Mesa's surfaceless platform, current. For GLES 2 set
// MESA_GLES_VERSION_OVERRIDE=2.0 before: Mesa then offers what a GLES 2.0 device does (the
// override holds for the whole process, so GLES 3 runs in another one).
bool MakeContext(int major, std::string *error);

void WritePpm(const std::string &path, const Picture &picture);

struct Difference {
  unsigned pixels = 0; // pixels with a channel off by more than the tolerance
  unsigned worst = 0;  // the largest difference of a channel
  bool sizeDiffers = false;
};
Difference Compare(const Picture &a, const Picture &b, int tolerance, bool alpha = true);
// a picture of the differences: black where equal, the difference scaled up elsewhere
Picture DifferencePicture(const Picture &a, const Picture &b);

// The main app's GX renderer (port/android-decomp/app/src/main/cpp/gx_gl.cpp) on the current
// GLES 3 context.
class Reference {
public:
  Reference();
  ~Reference();
  bool Init(std::string *error);
  void Run(const Re4GxChunk &chunk);
  Picture Efb() const;
  Picture Xfb() const; // empty before the first display copy
  unsigned ShaderErrors() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// The compact vertex as a renderer reads it, back to the float vertex (port/whole/tests/
// gx_probe.cpp asFloatVertices): a chunk whose compact draws become float draws, without the
// RE4_GX_OP_COMPACT_FORMAT ops; the GLES 3 renderer draws such a chunk. `storage` holds the new
// ops and vertices; the chunk points into it and into `chunk`'s indices and blob.
struct FloatChunk {
  std::vector<std::uint32_t> ops;
  std::vector<std::uint8_t> vertices;
};
void ToFloatVertices(const Re4GxChunk &chunk, FloatChunk &storage, Re4GxChunk &out);

} // namespace ouyatest
