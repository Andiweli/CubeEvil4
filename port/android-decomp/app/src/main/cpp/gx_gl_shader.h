// Shader generation of the GX renderer (gx_gl.cpp): GLSL ES 3.00 from the GX
// state that shapes the code. Constants (matrices, lights, colours, fog
// parameters, alpha references) live in two uniform blocks the renderer fills.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace re4gx {

// Everything that changes the generated code. Unused fields stay zero so
// equal states compare equal byte for byte.
struct ShaderKey {
  // vertex
  std::uint32_t layout;       // Re4GxLayout bits of the draw
  std::uint32_t numChans;     // XF 0x1009
  std::uint32_t numTexGens;   // XF 0x103F
  std::uint32_t chan[4];      // XF 0x100E-0x1011: colour 0/1, alpha 0/1 control
  std::uint32_t texGen[8];    // XF 0x1040+
  std::uint32_t postGen[8];   // XF 0x1050+ (with dual texture transform)
  std::uint32_t dualTex;      // XF 0x1012 bit 0
  std::uint32_t orthographic; // XF 0x1026
  // fragment
  std::uint32_t numStages;    // BP genmode bits 10-13, + 1
  std::uint32_t numIndStages; // BP genmode bits 16-18
  std::uint32_t colorEnv[16]; // BP 0xC0 + 2s
  std::uint32_t alphaEnv[16]; // BP 0xC1 + 2s
  std::uint32_t order[16];    // per stage: map | coord << 3 | enable << 6 | channel << 7
  std::uint32_t konst[16];    // per stage: kcsel | kasel << 8
  std::uint32_t swap[4];      // swap tables: r | g << 2 | b << 4 | a << 6
  std::uint32_t indCmd[16];   // BP 0x10 + s
  std::uint32_t indRef;       // BP 0x27 (with indirect stages)
  std::uint32_t alphaTest;    // BP 0xF3 bits 16-23: comp0, comp1, logic
  std::uint32_t fog;          // BP 0xF1 bits 20-23: projection, function
  std::uint32_t texMaps;      // maps the stages sample (bit per map)
};

std::string GxVertexShader(const ShaderKey &key);
std::string GxFragmentShader(const ShaderKey &key);

// Uniform blocks (std140, vec4/ivec4 only), shared by all programs.
constexpr int kXfBinding = 0;
constexpr int kPixelBinding = 1;

struct XfBlock {
  float posTex[64][4];  // XF 0x000-0x0FF: position and texture matrices by row
  float normal[32][4];  // XF 0x400-0x45F: normal matrices, 3 floats a row
  float post[64][4];    // XF 0x500-0x5FF: post-transform matrices by row
  float light[8][5][4]; // colour, cos attenuation, distance attenuation, position, direction
  float matAmb[4][4];   // material 0/1, ambient 0/1 (0..1)
  float viewport[2][4]; // sx sy sz ox | oy oz efbWidth efbHeight
  float proj[2][4];     // p0 p1 p2 p3 | p4 p5 type pointSize
};

struct PixelBlock {
  std::int32_t colors[4][4];   // TEV registers prev, c0, c1, c2 (signed 11 bit)
  std::int32_t konst[4][4];    // konstant colours
  std::int32_t alphaRef[4];    // ref0, ref1
  float fog[2][4];             // A, C, b magnitude, b shift | colour r g b (0..255)
  float coordSize[8][4];       // per texture coordinate: rasterizer size s, t (texels, BP 0x30+)
  float mapSize[8][4];         // per texture map: width, height, LOD bias
  std::int32_t indScale[4][4]; // per indirect stage: s, t shift
  std::int32_t indMtx[6][4];   // indirect matrices, two rows each: m0 m1 m2 (s1.10), shift
};

} // namespace re4gx
