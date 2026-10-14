#include "archive.h"
#include "sat.h"
#include "scene_gl.h"
#include "walk_probe.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
// Restore comparisons must hold the clock still: SMX water now scrolls.
void DrawFrozen(int w, int h, float x, float y, float zoom,
                bool normals = false, const re4::SceneProbe *probe = nullptr) {
  re4::DrawSceneGl(w, h, x, y, zoom, normals, probe, nullptr, 0);
}
int main(int argc, char **argv) {
  try {
    if (argc != 3 && argc != 4)
      throw std::runtime_error("Usage: scene_gl_check R300 OUTPUT.ppm [R100]");
    setenv("EGL_PLATFORM", "surfaceless", 1);
    auto d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major, minor;
    if (!eglInitialize(d, &major, &minor))
      throw std::runtime_error("EGL init");
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint attrs[] = {EGL_SURFACE_TYPE,
                      EGL_PBUFFER_BIT,
                      EGL_RENDERABLE_TYPE,
                      EGL_OPENGL_ES3_BIT,
                      EGL_RED_SIZE,
                      8,
                      EGL_GREEN_SIZE,
                      8,
                      EGL_BLUE_SIZE,
                      8,
                      EGL_DEPTH_SIZE,
                      24,
                      EGL_NONE};
    EGLConfig c;
    EGLint n;
    if (!eglChooseConfig(d, attrs, &c, 1, &n) || !n)
      throw std::runtime_error("Config");
    EGLint ca[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    auto context = eglCreateContext(d, c, EGL_NO_CONTEXT, ca);
    EGLint pa[] = {EGL_WIDTH, 960, EGL_HEIGHT, 540, EGL_NONE};
    auto surface = eglCreatePbufferSurface(d, c, pa);
    if (!eglMakeCurrent(d, surface, surface, context))
      throw std::runtime_error("Current");
    auto a = re4::RoomArchive::Load(argv[1]);
    auto e = a.find("SMD");
    if (!e)
      throw std::runtime_error("No SMD in decoded room");
    re4::Smd s(a.copy(e - a.entries().data()));
    std::size_t largest = 0;
    int selected = -1;
    for (std::size_t i = 0; i < s.placements.size(); ++i) {
      auto part = re4::BuildStaticScene(s, int(i));
      if (part.triangles > largest) {
        largest = part.triangles;
        selected = int(i);
      }
    }
    std::cout << "Default model: " << selected << "\n";
    auto scene = re4::BuildStaticScene(s, selected);
    re4::CreateSceneGl(scene);
    DrawFrozen(960, 540, 0, 0, 0);
    std::vector<unsigned char> baseline(960 * 540 * 4);
    glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, baseline.data());
    if (glGetError())
      throw std::runtime_error("Untextured baseline GL error");
    re4::DestroySceneGl();
    re4::AttachSceneTextures(s, scene);
    unsigned mipTextures = 0, mipLevels = 0;
    for (const auto &t : scene.textures) {
      mipTextures += !t.mips.empty();
      mipLevels += t.mips.size();
    }
    std::cout << "Original mipmaps: " << mipTextures << " textures, "
              << mipLevels << " extra levels\n";
    std::cout << scene.describe() << "Textures: " << scene.textures.size()
              << "; textured triangles: " << scene.texturedTriangles
              << "; fallback: " << scene.fallbackTriangles
              << "; alpha triangles: " << scene.alphaTriangles
              << "; unresolved alpha: " << scene.unresolvedAlphaTriangles
              << "\n";
    re4::CreateSceneGl(scene);
    DrawFrozen(960, 540, 0, 0, 0);
    glFinish();
    auto error = glGetError();
    if (error)
      throw std::runtime_error("GL error " + std::to_string(error));
    std::vector<unsigned char> pixels(960 * 540 * 4);
    glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError())
      throw std::runtime_error("Read");
    std::size_t covered = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
      if (pixels[i] > 30 || pixels[i + 1] > 30 || pixels[i + 2] > 30)
        ++covered;
    std::size_t different = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
      if (pixels[i] != baseline[i] || pixels[i + 1] != baseline[i + 1] ||
          pixels[i + 2] != baseline[i + 2])
        ++different;
    if (scene.texturedTriangles && different < 1000)
      throw std::runtime_error("Texture path did not change rendered pixels");
    std::cout << "Base texture render difference PASS: " << different
              << " pixels\n";
    if (covered < 1000)
      throw std::runtime_error("No visible mesh");
    std::ofstream out(argv[2], std::ios::binary);
    out << "P6\n960 540\n255\n";
    for (int y = 539; y >= 0; --y)
      for (int x = 0; x < 960; ++x)
        out.write(reinterpret_cast<char *>(pixels.data() + (y * 960 + x) * 4),
                  3);
    out.close();
    if (!out)
      throw std::runtime_error("Cannot write preview image");
    std::cout << "GLES3 render PASS: " << covered << " mesh pixels; "
              << glGetString(GL_VERSION) << '\n';
    std::cout << "Original normals: " << scene.normals.size() / 3
              << " corners, " << scene.zeroNormals << " zero, "
              << scene.singularNormalPlacements << " singular placements\n";
    auto materialPixels = pixels;
    DrawFrozen(960, 540, 0, 0, 0, true);
    glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError())
      throw std::runtime_error("r300 normal preview GL error");
    unsigned normalDifference = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
      if (pixels[i] != materialPixels[i] ||
          pixels[i + 1] != materialPixels[i + 1] ||
          pixels[i + 2] != materialPixels[i + 2])
        ++normalDifference;
    if (normalDifference < 1000)
      throw std::runtime_error("r300 original normal preview unchanged");
    std::ofstream normalOut(std::string(argv[2]) + ".normals.ppm",
                            std::ios::binary);
    normalOut << "P6\n960 540\n255\n";
    for (int y = 539; y >= 0; --y)
      for (int x = 0; x < 960; ++x)
        normalOut.write(
            reinterpret_cast<char *>(pixels.data() + (y * 960 + x) * 4), 3);
    normalOut.close();
    if (!normalOut)
      throw std::runtime_error("Cannot write normal preview");
    DrawFrozen(960, 540, 0, 0, 0, false);
    glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    if (pixels != materialPixels || glGetError())
      throw std::runtime_error(
          "r300 texture preview changed after normal toggle");
    std::cout << "r300 normal preview PASS: " << normalDifference
              << " changed pixels, exact framebuffer restore\n";
    re4::DestroySceneGl();
    auto second = re4::BuildStaticScene(s, 0);
    re4::AttachSceneTextures(s, second);
    re4::CreateSceneGl(second);
    DrawFrozen(960, 540, 0, 0, 0);
    glFinish();
    if (glGetError())
      throw std::runtime_error("Model switch");
    re4::DestroySceneGl();
    std::cout << "Model switch GPU cleanup PASS\n";
    auto coloredModel = re4::BuildStaticScene(s, 18);
    re4::AttachSceneTextures(s, coloredModel);
    if (!coloredModel.vertexColorTriangles)
      throw std::runtime_error(
          "r300 placement 18 / BIN 12 vertex source not selected");
    re4::CreateSceneGl(coloredModel);
    DrawFrozen(960, 540, 0, 0, 0);
    glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError())
      throw std::runtime_error("r300 colored model GL error");
    re4::DestroySceneGl();
    auto originalColors = pixels;
    for (auto &draw : coloredModel.draws)
      draw.vertexColors = false;
    re4::CreateSceneGl(coloredModel);
    DrawFrozen(960, 540, 0, 0, 0);
    glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError())
      throw std::runtime_error("r300 neutral model GL error");
    re4::DestroySceneGl();
    unsigned colorDifference = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
      if (pixels[i] != originalColors[i] ||
          pixels[i + 1] != originalColors[i + 1] ||
          pixels[i + 2] != originalColors[i + 2])
        ++colorDifference;
    if (colorDifference < 1000)
      throw std::runtime_error("Original r300 vertex colors not visible");
    std::cout << "r300 placement 18 / BIN 12 color PASS: "
              << coloredModel.vertexColorTriangles << " triangles, "
              << colorDifference << " changed pixels\n";
    re4::StaticScene quad;
    quad.vertices = {-1, -1, 0, 1, -1, 0, 1,  1, 0,
                     -1, -1, 0, 1, 1,  0, -1, 1, 0};
    quad.normals = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
    quad.colors = std::vector<std::uint8_t>(24, 255);
    quad.uv = {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1};
    quad.minimum = {-1, -1, 0};
    quad.maximum = {1, 1, 0};
    re4::Texture base;
    base.width = base.height = 1;
    base.rgba = {200, 40, 20, 0};
    re4::Texture mask = base;
    mask.rgba = {0, 0, 0, 255};
    quad.textures = {base, mask};
    re4::SceneDraw part;
    part.count = 6;
    part.texture = 0;
    part.alphaTest = true;
    part.alphaTexture = 1;
    quad.draws = {part};
    auto coverage = [&](unsigned alpha, unsigned reference, bool enable) {
      quad.textures[1].rgba[3] = alpha;
      quad.draws[0].alphaReference = reference;
      quad.draws[0].alphaTest = enable;
      re4::CreateSceneGl(quad);
      DrawFrozen(960, 540, 0, 0, 0);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      if (glGetError())
        throw std::runtime_error("Synthetic alpha GL error");
      std::size_t result = 0;
      for (std::size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i] > 30)
          ++result;
      if (enable && alpha == 128 && reference == 127) {
        unsigned maximumRed = 0;
        for (std::size_t i = 0; i < pixels.size(); i += 4)
          maximumRed = std::max(maximumRed, unsigned(pixels[i]));
        if (maximumRed < 80 || maximumRed > 140)
          throw std::runtime_error("Source alpha blend not applied");
      }
      re4::DestroySceneGl();
      return result;
    };
    if (coverage(0, 0, true) != 0 || coverage(127, 127, true) != 0 ||
        coverage(255, 255, true) != 0 || coverage(128, 127, true) < 1000 ||
        coverage(0, 255, false) < 1000)
      throw std::runtime_error(
          "Strict alpha threshold or base-alpha isolation failed");
    auto all = coverage(255, 0, true);
    quad.draws[0].count = 3;
    auto other = quad.draws[0];
    other.first = 3;
    other.alphaReference = 255;
    quad.draws.push_back(other);
    auto half = coverage(255, 0, true);
    if (half == 0 || half >= all)
      throw std::runtime_error("Distinct alpha batches were merged");
    std::cout << "Alpha framebuffer PASS: zero/equality/255 rejected; "
                 "above-reference visible; base-alpha isolated; distinct batch "
                 "thresholds preserved\n";
    quad.draws.resize(1);
    quad.draws[0].count = 6;
    // Five original SMX modes: a mask is not an instruction to blend. In
    // particular mode 4 keeps a solid RGB surface even with a partial mask.
    quad.draws[0].alphaTest = true;
    quad.draws[0].alphaReference = 0;
    quad.textures[1].rgba[3] = 128;
    unsigned expectedRed[5]{105, 109, 209, 16, 200};
    for (unsigned mode = 0; mode < 5; ++mode) {
      quad.draws[0].blendMode = mode;
      re4::CreateSceneGl(quad);
      DrawFrozen(960, 540, 0, 0, 0);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      unsigned red = pixels[(270 * 960 + 480) * 4];
      if (std::abs(int(red) - int(expectedRed[mode])) > 2 || glGetError())
        throw std::runtime_error("Original five blend modes/mask separation");
      re4::DestroySceneGl();
    }
    quad.draws[0].blendMode = 0;
    std::cout << "SMX blend framebuffer PASS: all 5 original factors; mode 4 "
                 "mask remains solid\n";
    quad.draws[0].alphaTest = false;
    quad.textures.resize(1);
    auto &mipTexture = quad.textures[0];
    mipTexture.width = mipTexture.height = 8;
    mipTexture.maxLOD = 1;
    mipTexture.rgba.clear();
    for (unsigned i = 0; i < 64; ++i)
      mipTexture.rgba.insert(mipTexture.rgba.end(), {255, 0, 0, 255});
    re4::TextureMip stored;
    stored.width = stored.height = 4;
    for (unsigned i = 0; i < 16; ++i)
      stored.rgba.insert(stored.rgba.end(), {0, 255, 0, 255});
    mipTexture.mips = {stored};
    for (auto &v : quad.uv)
      v *= 256;
    auto mipCoverage = [&](bool green) {
      re4::CreateSceneGl(quad);
      DrawFrozen(960, 540, 0, 0, 0);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      if (glGetError())
        throw std::runtime_error("Stored mip framebuffer GL error");
      unsigned hits = 0;
      for (std::size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i + (green ? 1 : 0)] > 240 &&
            pixels[i + (green ? 0 : 1)] < 10)
          ++hits;
      re4::DestroySceneGl();
      return hits;
    };
    if (mipCoverage(true) < 1000)
      throw std::runtime_error("Stored partial mip chain not sampled");
    mipTexture.mips.clear();
    if (mipCoverage(false) < 1000)
      throw std::runtime_error("Base-only sampler regression");
    std::cout << "Stored mip framebuffer PASS: supplied green level sampled; "
                 "base-only red fallback; partial chain complete\n";
    quad.draws[0].vertexColors = true;
    quad.draws[0].alphaTest = false;
    quad.textures[0].width = quad.textures[0].height = 1;
    quad.textures[0].rgba = {200, 120, 80, 0};
    for (std::size_t i = 0; i < quad.colors.size(); i += 4) {
      quad.colors[i] = 128;
      quad.colors[i + 1] = 64;
      quad.colors[i + 2] = 255;
      quad.colors[i + 3] = 255;
    }
    auto maximum = [&](unsigned channel) {
      re4::CreateSceneGl(quad);
      DrawFrozen(960, 540, 0, 0, 0);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      if (glGetError())
        throw std::runtime_error("Vertex color GL error");
      unsigned value = 0;
      for (std::size_t i = channel; i < pixels.size(); i += 4)
        value = std::max(value, unsigned(pixels[i]));
      re4::DestroySceneGl();
      return value;
    };
    if (maximum(0) < 99 || maximum(0) > 102 || maximum(1) < 29 ||
        maximum(1) > 32 || maximum(2) != 80)
      throw std::runtime_error("Stored RGB not modulating sampled texture");
    quad.draws[0].vertexColors = false;
    if (maximum(0) != 200)
      throw std::runtime_error(
          "Register color branch unexpectedly uses vertex colors");
    quad.draws[0].vertexColors = true;
    for (std::size_t i = 3; i < quad.colors.size(); i += 4)
      quad.colors[i] = 128;
    if (maximum(0) < 49 || maximum(0) > 58)
      throw std::runtime_error("Vertex material alpha blend missing");
    re4::Texture alphaMask;
    alphaMask.width = alphaMask.height = 1;
    alphaMask.rgba = {255, 255, 255, 128};
    quad.textures.push_back(alphaMask);
    quad.draws[0].alphaTest = true;
    quad.draws[0].alphaTexture = 1;
    quad.draws[0].alphaReference = 64;
    if (maximum(0) > 10)
      throw std::runtime_error(
          "Combined mask and vertex alpha equality not rejected");
    quad.draws[0].alphaReference = 63;
    if (maximum(0) < 20 || maximum(0) > 35)
      throw std::runtime_error("Combined mask and vertex alpha not blended");
    // Adjacent draws with different raster-color sources must stay separate.
    quad.draws[0].count = 3;
    quad.draws[0].alphaTest = false;
    auto registerDraw = quad.draws[0];
    registerDraw.first = 3;
    registerDraw.vertexColors = false;
    quad.draws.push_back(registerDraw);
    if (maximum(0) != 200)
      throw std::runtime_error("Different vertex-color branches merged");
    std::cout << "Vertex color framebuffer PASS: RGB modulation, neutral "
                 "register branch, material alpha, combined mask threshold, "
                 "distinct batches\n";
    quad.draws.resize(1);
    quad.draws[0].count = 6;
    quad.draws[0].vertexColors = false;
    quad.textures[0].rgba = {200, 120, 80, 0};
    re4::CreateSceneGl(quad);
    DrawFrozen(960, 540, 0, 0, 0, true);
    glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    unsigned normalPixels = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
      if (pixels[i] >= 127 && pixels[i] <= 129 && pixels[i + 1] >= 127 &&
          pixels[i + 1] <= 129 && pixels[i + 2] == 255)
        ++normalPixels;
    if (normalPixels < 1000 || glGetError())
      throw std::runtime_error("Original normal attribute not rendered");
    DrawFrozen(960, 540, 0, 0, 0, false);
    glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    unsigned texturePixels = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
      if (pixels[i] == 200 && pixels[i + 1] == 120 && pixels[i + 2] == 80)
        ++texturePixels;
    re4::DestroySceneGl();
    if (texturePixels < 1000)
      throw std::runtime_error(
          "Return from normal view changed original texture branch");
    quad.draws[0].alphaTest = true;
    quad.draws[0].alphaTexture = 1;
    quad.draws[0].alphaReference = 255;
    re4::CreateSceneGl(quad);
    DrawFrozen(960, 540, 0, 0, 0, true);
    glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    unsigned rejected = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
      if (pixels[i + 2] > 240)
        ++rejected;
    re4::DestroySceneGl();
    if (rejected || glGetError())
      throw std::runtime_error("Normal inspection bypassed alpha threshold");
    quad.draws[0].alphaTest = false;
    std::fill(quad.normals.begin(), quad.normals.end(), 0);
    re4::CreateSceneGl(quad);
    DrawFrozen(960, 540, 0, 0, 0, true);
    glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    unsigned zeroPixels = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
      if (pixels[i] == 255 && pixels[i + 1] == 0 && pixels[i + 2] == 255)
        ++zeroPixels;
    re4::DestroySceneGl();
    if (zeroPixels < 1000 || glGetError())
      throw std::runtime_error("Zero normals not marked in inspection");
    std::cout << "Normal framebuffer PASS: independent normal attribute, "
                 "textured restore, alpha threshold, zero-normal marker\n";
    // Different SMX material registers must not merge adjacent batches.
    quad.normals = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
    quad.uv = {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1};
    quad.colors.assign(24, 255);
    quad.textures.resize(1);
    quad.textures[0].rgba = {255, 255, 255, 0};
    quad.draws[0].count = 3;
    quad.draws[0].vertexColors = false;
    quad.draws[0].alphaTest = false;
    quad.draws[0].materialRGB = {255, 0, 0};
    auto blueMaterial = quad.draws[0];
    blueMaterial.first = 3;
    blueMaterial.materialRGB = {0, 0, 255};
    quad.draws.push_back(blueMaterial);
    re4::CreateSceneGl(quad);
    DrawFrozen(960, 540, 0, 0, 0);
    glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    unsigned redMaterialPixels = 0, blueMaterialPixels = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
      redMaterialPixels +=
          pixels[i] > 240 && pixels[i + 1] < 10 && pixels[i + 2] < 10;
      blueMaterialPixels +=
          pixels[i + 2] > 240 && pixels[i + 1] < 10 && pixels[i] < 10;
    }
    re4::DestroySceneGl();
    if (redMaterialPixels < 1000 || blueMaterialPixels < 1000 || glGetError())
      throw std::runtime_error("SMX material colors merged or base-alpha used");
    std::cout << "SMX material framebuffer PASS: distinct red/blue registers, "
                 "base alpha ignored\n";
    if (argc == 4) {
      auto room = re4::RoomArchive::Load(argv[3]);
      const auto *primary = room.find("SMD");
      const auto *shared = room.find("SMD", 1);
      if (!primary || !shared)
        throw std::runtime_error("r100 SMD pair missing");
      re4::Smd local(room.copy(primary - room.entries().data()));
      re4::Smd common(room.copy(shared - room.entries().data()),
                      local.commonRequirements());
      auto full = re4::BuildStaticScene(local, -1, &common);
      re4::AttachSceneTextures(local, full, &common);
      if (full.included != 13 || full.triangles != 6020 ||
          full.fallbackTriangles || full.commonTextureTriangles != 6020 ||
          full.commonAlphaTriangles != 3543)
        throw std::runtime_error("r100 appended palette counts differ");
      int chosen = -1;
      std::size_t largest = 0;
      for (std::size_t i = 0; i < local.placements.size(); ++i) {
        auto piece = re4::BuildStaticScene(local, int(i), &common);
        if (piece.triangles > largest) {
          largest = piece.triangles;
          chosen = int(i);
        }
      }
      auto piece = re4::BuildStaticScene(local, chosen, &common);
      re4::AttachSceneTextures(local,
                               piece); // M11 unresolved path as baseline.
      re4::CreateSceneGl(piece);
      DrawFrozen(960, 540, 0, 0, 0);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      auto unresolved = pixels;
      re4::DestroySceneGl();
      re4::AttachSceneTextures(local, piece, &common);
      re4::CreateSceneGl(piece);
      DrawFrozen(960, 540, 0, 0, 0);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      auto textured = pixels;
      unsigned difference = 0;
      for (std::size_t i = 0; i < pixels.size(); i += 4)
        difference += !std::equal(pixels.begin() + i, pixels.begin() + i + 3,
                                  unresolved.begin() + i);
      if (difference < 1000 || glGetError())
        throw std::runtime_error("r100 shared textures not visible");
      std::ofstream out(std::string(argv[2]) + ".r100.ppm", std::ios::binary);
      out << "P6\n960 540\n255\n";
      for (int y = 539; y >= 0; --y)
        for (int x = 0; x < 960; ++x)
          out.write(
              reinterpret_cast<const char *>(pixels.data() + (y * 960 + x) * 4),
              3);
      out.close();
      if (!out)
        throw std::runtime_error("r100 framebuffer export");
      DrawFrozen(960, 540, 0, 0, 0, true);
      DrawFrozen(960, 540, 0, 0, 0, false);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      if (pixels != textured || glGetError())
        throw std::runtime_error(
            "r100 texture restore after normal inspection");
      re4::DestroySceneGl();
      const auto *smxEntry = room.find("SMX");
      if (!smxEntry)
        throw std::runtime_error("r100 SMX missing");
      re4::Smx smx(room.copy(smxEntry - room.entries().data()));
      re4::ApplySceneSmx(smx, piece);
      re4::CreateSceneGl(piece);
      DrawFrozen(960, 540, 0, 0, 0);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      if (glGetError())
        throw std::runtime_error("r100 SMX GL error");
      re4::DestroySceneGl();
      auto tintedPiece = re4::BuildStaticScene(local, 0, &common);
      re4::AttachSceneTextures(local, tintedPiece, &common);
      re4::CreateSceneGl(tintedPiece);
      DrawFrozen(960, 540, 0, 0, 0);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      auto beforeTint = pixels;
      re4::DestroySceneGl();
      re4::ApplySceneSmx(smx, tintedPiece);
      if (tintedPiece.materialColorTriangles != 94 ||
          tintedPiece.draws[0].materialRGB !=
              std::array<std::uint8_t, 3>{42, 42, 42})
        throw std::runtime_error("r100 work 0 SMX material mismatch");
      re4::CreateSceneGl(tintedPiece);
      DrawFrozen(960, 540, 0, 0, 0);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      unsigned tintedPixels = 0;
      for (std::size_t i = 0; i < pixels.size(); i += 4)
        tintedPixels += !std::equal(pixels.begin() + i, pixels.begin() + i + 3,
                                    beforeTint.begin() + i);
      if (!tintedPixels || glGetError())
        throw std::runtime_error("r100 real SMX tint invisible");
      DrawFrozen(960, 540, 0, 0, 0, true);
      DrawFrozen(960, 540, 0, 0, 0, false);
      std::vector<unsigned char> restored(pixels.size());
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, restored.data());
      if (restored != pixels)
        throw std::runtime_error("SMX tint lost after normal view");
      re4::DestroySceneGl();
      std::cout << "r100 work 0 material RGB42 PASS: 94 triangles; "
                << tintedPixels
                << " changed pixels; exact normal-view restore\n";
      std::cout << "r100 SMX PASS: " << smx.records.size() << " records; "
                << piece.smxDraws << " matched draws; "
                << piece.materialColorTriangles << " tinted triangles\n";
      std::cout
          << "r100 common framebuffer PASS: 6020 base / 3543 mask triangles, "
          << difference << " changed pixels; model " << chosen
          << "; exact normal-view restore\n";
    }
    for (int roomArg : {1, argc == 4 ? 3 : 1}) {
      auto archive = re4::RoomArchive::Load(argv[roomArg]);
      auto entry = archive.find("SAT", 0);
      if (!entry)
        throw std::runtime_error("Missing SAT test input");
      re4::Sat sat(archive.copy(entry - archive.entries().data()));
      auto collision = sat.scene();
      re4::DestroySceneGl();
      re4::CreateSceneGl(collision);
      DrawFrozen(960, 540, 0, 0, 0);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      std::size_t groups[3]{};
      for (std::size_t i = 0; i < pixels.size(); i += 4) {
        groups[0] +=
            pixels[i] == 60 && pixels[i + 1] == 210 && pixels[i + 2] == 90;
        groups[1] +=
            pixels[i] == 240 && pixels[i + 1] == 190 && pixels[i + 2] == 50;
        groups[2] +=
            pixels[i] == 70 && pixels[i + 1] == 130 && pixels[i + 2] == 240;
      }
      if (groups[0] + groups[1] + groups[2] < 1000 || glGetError())
        throw std::runtime_error("SAT group framebuffer coverage/error");
      auto original = pixels;
      DrawFrozen(960, 540, 0, 0, 0, true);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      if (pixels == original)
        throw std::runtime_error("SAT normals not visible");
      DrawFrozen(960, 540, 0, 0, 0, false);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      if (pixels != original || glGetError())
        throw std::runtime_error("SAT restore/error");
      std::ofstream image(std::string(argv[2]) + ".sat-" +
                              std::to_string(roomArg) + ".ppm",
                          std::ios::binary);
      image << "P6\n960 540\n255\n";
      for (int y = 539; y >= 0; --y)
        for (int x = 0; x < 960; ++x)
          image.write(
              reinterpret_cast<const char *>(pixels.data() + (y * 960 + x) * 4),
              3);
      re4::SatCollision world(sat);
      re4::WalkProbe walker(world);
      if (!walker.reset())
        throw std::runtime_error("No walk start in GL input");
      re4::SceneProbe probe;
      probe.collision = &world;
      probe.feet = walker.position();
      probe.groundTriangle = world.triangle(walker.floorHit()->polygon);
      // The actorless diagnostic capsule has an explicit body heading. The
      // shoulder view's diagnostic yaw is offset from that heading and must
      // never be fed back into the next frame as a new body rotation.
      re4::SceneFollowHeading(0);
      DrawFrozen(960, 540, 0, 0, 0, false, &probe);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      std::size_t markerPixels = 0;
      for (std::size_t i = 0; i < pixels.size(); i += 4)
        markerPixels +=
            pixels[i] > 245 && pixels[i + 1] > 220 && pixels[i + 2] < 70;
      if (markerPixels < 100 || glGetError())
        throw std::runtime_error("Walk marker/follow camera missing");
      auto followPixels = pixels;
      const auto stablePose = re4::SceneFollowPose();
      for (unsigned frame = 0; frame < 4; ++frame)
        DrawFrozen(960, 540, 0, 0, 0, false, &probe);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      const auto repeatedPose = re4::SceneFollowPose();
      if (pixels != followPixels || stablePose.eye != repeatedPose.eye ||
          stablePose.target != repeatedPose.target || glGetError())
        throw std::runtime_error("Paused actorless follow view drifted");
      DrawFrozen(960, 540, 0, 0, 0, true, &probe);
      DrawFrozen(960, 540, 0, 0, 0, false, &probe);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      if (pixels != followPixels || glGetError())
        throw std::runtime_error("Walk normal restore/error");
      std::ofstream walkImage(std::string(argv[2]) + ".walk-" +
                                  std::to_string(roomArg) + ".ppm",
                              std::ios::binary);
      walkImage << "P6\n960 540\n255\n";
      for (int y = 539; y >= 0; --y)
        for (int x = 0; x < 960; ++x)
          walkImage.write(
              reinterpret_cast<const char *>(pixels.data() + (y * 960 + x) * 4),
              3);
      // Same SAT feet/camera against full supported static model geometry.
      auto modelEntry = archive.find("SMD");
      re4::Smd models(archive.copy(modelEntry - archive.entries().data()));
      std::unique_ptr<re4::Smd> shared;
      if (auto common = archive.find("SMD", 1))
        shared = std::make_unique<re4::Smd>(
            archive.copy(common - archive.entries().data()),
            models.commonRequirements());
      auto all = re4::BuildStaticScene(models, -1, shared.get());
      re4::AttachSceneTextures(models, all, shared.get());
      if (auto x = archive.find("SMX")) {
        re4::Smx smx(archive.copy(x - archive.entries().data()));
        re4::ApplySceneSmx(smx, all);
      }
      re4::DestroySceneGl();
      re4::CreateSceneGl(all);
      re4::SceneFollowHeading(0);
      DrawFrozen(960, 540, 0, 0, 0, false, &probe);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      if (glGetError() || all.textures.empty())
        throw std::runtime_error("Textured walk upload/error");
      markerPixels = 0;
      for (std::size_t i = 0; i < pixels.size(); i += 4)
        markerPixels +=
            pixels[i] > 245 && pixels[i + 1] > 220 && pixels[i + 2] < 70;
      if (markerPixels < 100)
        throw std::runtime_error("Textured walk marker missing");
      auto textFollow = pixels;
      DrawFrozen(960, 540, 0, 0, 0, true, &probe);
      DrawFrozen(960, 540, 0, 0, 0, false, &probe);
      glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
      if (pixels != textFollow || glGetError())
        throw std::runtime_error("Textured walk restore/error");
      bool scrolling =
          std::any_of(all.draws.begin(), all.draws.end(), [](const auto &d) {
            return d.uvSpeed != std::array<float, 2>{};
          });
      if (scrolling) {
        for (unsigned frame = 0; frame < 40; ++frame)
          re4::DrawSceneGl(960, 540, 0, 0, 0, false, &probe, nullptr, .05);
        glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        if (pixels == textFollow || glGetError())
          throw std::runtime_error("Original SMX UV scroll inactive");
        auto scrolled = pixels;
        DrawFrozen(960, 540, 0, 0, 0, true, &probe);
        DrawFrozen(960, 540, 0, 0, 0, false, &probe);
        glReadPixels(0, 0, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        if (pixels != scrolled)
          throw std::runtime_error("SMX scrolling material restore");
        std::cout << "SMX UV scroll framebuffer PASS: authored speed advances, "
                     "frozen restore exact\n";
      }
      std::ofstream textImage(std::string(argv[2]) + ".text-walk-" +
                                  std::to_string(roomArg) + ".ppm",
                              std::ios::binary);
      textImage << "P6\n960 540\n255\n";
      for (int y = 539; y >= 0; --y)
        for (int x = 0; x < 960; ++x)
          textImage.write(
              reinterpret_cast<const char *>(pixels.data() + (y * 960 + x) * 4),
              3);
      std::cout << "Walk GLES PASS: " << markerPixels
                << " capsule pixels, follow camera and exact normal restore\n";
      std::cout << "SAT GLES PASS " << argv[roomArg] << ": "
                << sat.polygons.size() << " triangles; group pixels "
                << groups[0] << '/' << groups[1] << '/' << groups[2]
                << "; exact normal-view restore\n";
    }
    re4::DestroySceneGl();
    eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(d, surface);
    eglDestroyContext(d, context);
    eglTerminate(d);
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
