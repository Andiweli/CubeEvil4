// GLSL ES 1.00 from the GX state (gx_gl2_shader.h). The arithmetic is the main app's
// (gx_gl_shader.cpp, after Dolphin's shader generators) in floats: every TEV value is a whole
// number held in a float, `x & 255` becomes x - 256 floor(x / 256), shifts become products and
// floors, the lerp splits into a + (b - a) c' / 256 so the operands stay small. With highp the
// results equal the GLES 3 renderer's; with the Tegra 3's mediump (13 bits) the lerp's rounding
// and the fog mix are approximate.
#include "gx_gl2_shader.h"

#include <cstdarg>
#include <cstdio>

namespace re4gx2 {
namespace {

// Re4GxLayout (re4_gx_frame.h)
constexpr std::uint32_t kNormal = 1u << 0, kNbt = 1u << 1, kColor0 = 1u << 2, kColor1 = 1u << 3, kTex0 = 1u << 4,
                        kCompact = 1u << 13;

void Add(std::string &out, const char *format, ...) __attribute__((format(printf, 2, 3)));
void Add(std::string &out, const char *format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  out += buffer;
}

bool Has(const ShaderKey &k, std::uint32_t bit) { return (k.layout & bit) != 0; }

const char kXyzw[] = "xyzw";

unsigned Popcount(std::uint32_t v) {
  unsigned n = 0;
  for (; v; v &= v - 1)
    n++;
  return n;
}

// texgen i: type 0 regular (and 4..7 as regular), 1 emboss, 2/3 colour
unsigned GenType(std::uint32_t g) { return (g >> 4) & 7; }
bool Regular(std::uint32_t g) { return GenType(g) != 1 && GenType(g) != 2 && GenType(g) != 3; }
unsigned GenRow(std::uint32_t g) { return (g >> 7) & 31; }

// lighting channel c (0/1) colour or alpha: lit
bool Lit(const ShaderKey &k, unsigned c, bool alpha) {
  return c < k.numChans && c < 2 && (k.chan[c + (alpha ? 2 : 0)] & 2);
}

std::uint32_t LightMask(std::uint32_t c) { return ((c >> 2) & 15) | (((c >> 11) & 15) << 4); }

// The expression of matrix slot `slot` (0 position, 1 + n texture n): from the vertex or the
// draw's defaults.
std::string Slot(const ShaderKey &k, unsigned slot) {
  char buffer[32];
  if (k.mtxSlots & (1u << slot)) {
    const unsigned j = Popcount(k.mtxSlots & ((1u << slot) - 1));
    std::snprintf(buffer, sizeof(buffer), "aMtx%u.%c", j / 4, kXyzw[j % 4]);
  } else {
    std::snprintf(buffer, sizeof(buffer), "uMtxDef[%u].%c", slot / 4, kXyzw[slot % 4]);
  }
  return buffer;
}

// the dequantisation factor of a compact attribute (uDequant: pos, nrm, tex0, tex1 | tex2-5 | tex6, 7)
std::string Dequant(int source) {
  int i = source == kAttrPos ? 0 : (source == kAttrNrm || source == kAttrBin || source == kAttrTan) ? 1
                                                                                                  : 2 + (source - kAttrTex0);
  char buffer[24];
  std::snprintf(buffer, sizeof(buffer), "uDequant[%d].%c", i / 4, kXyzw[i % 4]);
  return buffer;
}

} // namespace

const char *GxAttributeName(int source) {
  static const char *names[kAttrSources] = {"aPos",  "aNrm",  "aBin",  "aTan",  "aClr0", "aClr1", "aTex0",
                                            "aTex1", "aTex2", "aTex3", "aTex4", "aTex5", "aTex6", "aTex7",
                                            "aMtx0", "aMtx1", "aMtx2"};
  return source >= 0 && source < kAttrSources ? names[source] : "?";
}

ShaderInfo GxShaderInfo(const ShaderKey &k) {
  ShaderInfo s{};
  s.compact = Has(k, kCompact);
  const bool hasNrm = Has(k, kNormal), hasNbt = hasNrm && Has(k, kNbt);
  bool lit = false;
  for (unsigned c = 0; c < 2; c++)
    lit = lit || Lit(k, c, false) || Lit(k, c, true);
  bool needNrm = hasNrm && lit, needNbt = false, needClr[2] = {false, false}, needTex[8] = {};
  bool normalMatrix = hasNrm && lit;
  std::uint32_t slots = 1; // the position matrix
  for (unsigned i = 0; i < k.numTexGens; i++) {
    const std::uint32_t g = k.texGen[i];
    if (GenType(g) == 1) {
      const unsigned source = (g >> 12) & 7, light = (g >> 15) & 7;
      if (source < i && hasNbt) {
        needNbt = true;
        normalMatrix = true;
        s.lights |= 1u << light;
      }
      continue;
    }
    if (!Regular(g))
      continue;
    slots |= 2u << i;
    const unsigned row = GenRow(g);
    if (row == 1 && hasNrm)
      needNrm = true;
    else if ((row == 3 || row == 4) && hasNbt)
      needNbt = true;
    else if (row >= 5 && row <= 12 && Has(k, kTex0 << (row - 5)))
      needTex[row - 5] = true;
  }
  // colours: what the lighting channels read of the vertex (gx_gl_shader.cpp Light), and the
  // channels taken as they are
  for (unsigned c = 0; c < 2; c++) {
    if (!Has(k, c == 0 ? kColor0 : kColor1))
      continue;
    if (c >= k.numChans) {
      needClr[c] = true;
      continue;
    }
    for (int alpha = 0; alpha < 2; alpha++) {
      const std::uint32_t ctrl = k.chan[c + 2 * alpha];
      if ((ctrl & 1) || ((ctrl & 2) && (ctrl & 64)))
        needClr[c] = true;
    }
  }
  for (unsigned c = 0; c < 2; c++)
    for (int alpha = 0; alpha < 2; alpha++)
      if (Lit(k, c, alpha))
        s.lights |= LightMask(k.chan[c + 2 * alpha]);
  // attributes, in location order
  s.attribs[s.attribCount++] = kAttrPos;
  if (needNrm || normalMatrix)
    s.attribs[s.attribCount++] = kAttrNrm;
  if (needNbt) {
    s.attribs[s.attribCount++] = kAttrBin;
    s.attribs[s.attribCount++] = kAttrTan;
  }
  for (unsigned c = 0; c < 2; c++)
    if (needClr[c])
      s.attribs[s.attribCount++] = kAttrClr0 + static_cast<int>(c);
  for (unsigned t = 0; t < 8; t++)
    if (needTex[t])
      s.attribs[s.attribCount++] = kAttrTex0 + static_cast<int>(t);
  unsigned mtxWords = 0;
  for (unsigned slot = 0; slot < 9; slot++) {
    if (!(slots & (1u << slot)))
      continue;
    if (k.mtxSlots & (1u << slot)) {
      const unsigned j = Popcount(k.mtxSlots & ((1u << slot) - 1));
      mtxWords |= 1u << (j / 4);
    } else {
      s.mtxDefaults = true;
    }
  }
  for (unsigned w = 0; w < 3; w++)
    if (mtxWords & (1u << w))
      s.attribs[s.attribCount++] = kAttrMtx0 + static_cast<int>(w);
  s.normalMatrix = normalMatrix;
  // post-transform rows, packed
  if (k.dualTex)
    for (unsigned i = 0; i < k.numTexGens; i++) {
      if (!Regular(k.texGen[i]))
        continue;
      const int pr = static_cast<int>(k.postGen[i] & 63);
      for (int r = pr; r < pr + 3; r++) {
        bool known = false;
        for (int j = 0; j < s.postCount; j++)
          known = known || s.postRows[j] == r;
        if (!known && s.postCount < 24)
          s.postRows[s.postCount++] = r;
      }
    }
  // texture coordinates: which maps read each, projected or not
  s.fixedPoint = k.numIndStages > 0;
  for (int i = 0; i < 8; i++)
    s.coordMap[i] = -2;
  std::uint32_t readers[8] = {};
  for (unsigned n = 0; n < k.numStages; n++) {
    const std::uint32_t order = k.order[n];
    const unsigned map = order & 7, coord = (order >> 3) & 7, enable = (order >> 6) & 1;
    if (coord < k.numTexGens && enable && (k.texMaps & (1u << map)))
      readers[coord] |= 1u << map;
    // indirect stages read their coordinate for the wrap and the matrix
    if (s.fixedPoint && coord < k.numTexGens)
      readers[coord] |= 0x100;
  }
  if (s.fixedPoint)
    for (unsigned st = 0; st < k.numIndStages && st < 4; st++) {
      const unsigned coord = (k.indRef >> (6 * st + 3)) & 7;
      if (coord < k.numTexGens)
        readers[coord] |= 0x100;
    }
  for (unsigned i = 0; i < k.numTexGens; i++) {
    const std::uint32_t r = readers[i] & 0xFF;
    if (!readers[i])
      continue;
    if (!s.fixedPoint && !k.texelCoords && r && (r & (r - 1)) == 0) {
      int m = 0;
      while (!(r & (1u << m)))
        m++;
      s.coordMap[i] = m;
    } else if (r || s.fixedPoint) {
      s.coordMap[i] = -1;
    }
  }
  for (unsigned i = 0; i < k.numTexGens; i++) {
    const std::uint32_t g = k.texGen[i];
    if (Regular(g))
      s.projective[i] = ((g >> 1) & 1) || k.dualTex;
    else if (GenType(g) == 1) {
      const unsigned source = (g >> 12) & 7;
      s.projective[i] = source < i && s.projective[source];
    }
  }
  // sizes: vertex uniforms (vec4) and varying floats
  s.vertexUniforms = 64 + (s.normalMatrix ? 32 : 0) + s.postCount + 5 * static_cast<int>(Popcount(s.lights)) + 4 + 4 +
                     (s.mtxDefaults ? 3 : 0) + (s.compact ? 3 : 0) + 4;
  s.varyingComponents = 8;
  for (unsigned i = 0; i < k.numTexGens; i++)
    if (s.coordMap[i] != -2)
      s.varyingComponents += s.projective[i] ? 3 : 2;
  return s;
}

namespace {

// The packed place of light `light` in uLight (five vec4 each).
int LightSlot(const ShaderInfo &s, unsigned light) { return static_cast<int>(Popcount(s.lights & ((1u << light) - 1))); }

int PostSlot(const ShaderInfo &s, int row) {
  for (int j = 0; j < s.postCount; j++)
    if (s.postRows[j] == row)
      return j;
  return 0;
}

// One lighting channel (gx_gl_shader.cpp Light).
void Light(std::string &out, const ShaderKey &k, const ShaderInfo &s, int chan, bool alpha) {
  const std::uint32_t c = k.chan[chan + (alpha ? 2 : 0)];
  const char *sw = alpha ? "a" : "rgb";
  const char *type = alpha ? "float" : "vec3";
  bool hasColor = false;
  for (int a = 0; a < s.attribCount; a++)
    hasColor = hasColor || s.attribs[a] == kAttrClr0 + chan;
  Add(out, "  {\n");
  if ((c & 1) && hasColor)
    Add(out, "    %s mat = aClr%d.%s;\n", type, chan, sw);
  else if (c & 1)
    Add(out, "    %s mat = %s(1.0);\n", type, type);
  else
    Add(out, "    %s mat = uMatAmb[%d].%s;\n", type, chan, sw);
  if (c & 2) {
    if ((c & 64) && hasColor)
      Add(out, "    %s lacc = aClr%d.%s;\n", type, chan, sw);
    else if (c & 64)
      Add(out, "    %s lacc = %s(1.0);\n", type, type);
    else
      Add(out, "    %s lacc = uMatAmb[%d].%s;\n", type, 2 + chan, sw);
    const unsigned mask = LightMask(c);
    const unsigned diffuse = (c >> 7) & 3, attn = (c >> 9) & 3;
    for (unsigned i = 0; i < 8; i++) {
      if (!(mask & (1u << i)))
        continue;
      const int b = LightSlot(s, i) * 5; // colour, cos att, dist att, position, direction
      Add(out, "    {\n");
      if (attn == 1) { // specular
        Add(out, "      vec3 ldir = normalize(uLight[%d].xyz - pos);\n", b + 3);
        Add(out, "      float at = dot(nrm, ldir) >= 0.0 ? max(0.0, dot(nrm, uLight[%d].xyz)) : 0.0;\n", b + 4);
        if (diffuse == 0)
          Add(out, "      vec3 dist = uLight[%d].xyz;\n", b + 2);
        else
          Add(out, "      vec3 dist = normalize(uLight[%d].xyz);\n", b + 2);
        Add(out, "      at = max(0.0, dot(uLight[%d].xyz, vec3(1.0, at, at * at))) / dot(dist, vec3(1.0, at, at * at));\n",
            b + 1);
      } else if (attn == 3) { // spot
        Add(out, "      vec3 ldir = uLight[%d].xyz - pos;\n", b + 3);
        Add(out, "      float dist2 = dot(ldir, ldir);\n      float dist = sqrt(dist2);\n");
        Add(out, "      ldir = ldir / dist;\n");
        Add(out, "      float at = max(0.0, dot(ldir, uLight[%d].xyz));\n", b + 4);
        Add(out,
            "      at = max(0.0, uLight[%d].x + uLight[%d].y * at + uLight[%d].z * at * at) / "
            "dot(uLight[%d].xyz, vec3(1.0, dist, dist2));\n",
            b + 1, b + 1, b + 1, b + 2);
      } else { // none / directional
        Add(out, "      vec3 ldir = normalize(uLight[%d].xyz - pos);\n", b + 3);
        Add(out, "      float at = 1.0;\n");
      }
      if (diffuse == 0)
        Add(out, "      lacc += at * uLight[%d].%s;\n", b, sw);
      else if (diffuse == 1)
        Add(out, "      lacc += at * dot(ldir, nrm) * uLight[%d].%s;\n", b, sw);
      else
        Add(out, "      lacc += at * max(0.0, dot(ldir, nrm)) * uLight[%d].%s;\n", b, sw);
      Add(out, "    }\n");
    }
    Add(out, "    col%d.%s = mat * clamp(lacc, 0.0, 1.0);\n", chan, sw);
  } else {
    Add(out, "    col%d.%s = mat;\n", chan, sw);
  }
  Add(out, "  }\n");
}

bool Reads(const ShaderInfo &s, int source) {
  for (int a = 0; a < s.attribCount; a++)
    if (s.attribs[a] == source)
      return true;
  return false;
}

} // namespace

std::string GxVertexShader(const ShaderKey &k, const ShaderInfo &s) {
  std::string out = "precision highp float;\n";
  for (int a = 0; a < s.attribCount; a++) {
    const int src = s.attribs[a];
    const char *type = src == kAttrPos || src == kAttrNrm || src == kAttrBin || src == kAttrTan ? "vec3"
                       : src >= kAttrTex0 && src < kAttrMtx0                                    ? "vec2"
                                                                                                 : "vec4";
    Add(out, "attribute %s %s;\n", type, GxAttributeName(src));
  }
  out += "uniform vec4 uPosTex[64];\n";
  if (s.normalMatrix)
    out += "uniform vec4 uNrm[32];\n";
  if (s.postCount)
    Add(out, "uniform vec4 uPost[%d];\n", s.postCount);
  if (s.lights)
    Add(out, "uniform vec4 uLight[%u];\n", 5 * Popcount(s.lights));
  out += "uniform vec4 uMatAmb[4];\nuniform vec4 uVp[2];\nuniform vec4 uProj[2];\n";
  if (s.mtxDefaults)
    out += "uniform vec4 uMtxDef[3];\n";
  if (s.compact)
    out += "uniform vec4 uDequant[3];\n";
  out += "uniform vec4 uTcScale[4];\n";
  out += "varying vec4 vColor0;\nvarying vec4 vColor1;\n";
  for (unsigned i = 0; i < k.numTexGens; i++)
    if (s.coordMap[i] != -2)
      Add(out, "varying %s vTex%u;\n", s.projective[i] ? "vec3" : "vec2", i);
  out += "void main() {\n";
  // the attributes as numbers (the compact vertex scaled by its fraction bits)
  for (int a = 0; a < s.attribCount; a++) {
    const int src = s.attribs[a];
    if (src >= kAttrClr0 && src <= kAttrClr1)
      continue;
    if (src >= kAttrMtx0)
      continue;
    const char *type = src >= kAttrTex0 ? "vec2" : "vec3";
    static const char *local[kAttrMtx0] = {"lPos",  "lNrm",  "lBin",  "lTan",  "",      "",      "lTex0",
                                           "lTex1", "lTex2", "lTex3", "lTex4", "lTex5", "lTex6", "lTex7"};
    if (s.compact)
      Add(out, "  %s %s = %s * %s;\n", type, local[src], GxAttributeName(src), Dequant(src).c_str());
    else
      Add(out, "  %s %s = %s;\n", type, local[src], GxAttributeName(src));
  }
  Add(out, "  float slot0 = %s;\n", Slot(k, 0).c_str());
  out += "  int pi = int(slot0);\n";
  out += "  vec4 p4 = vec4(lPos, 1.0);\n";
  out += "  vec3 pos = vec3(dot(uPosTex[pi], p4), dot(uPosTex[pi + 1], p4), dot(uPosTex[pi + 2], p4));\n";
  out += "  vec3 nrm = vec3(0.0, 0.0, 1.0);\n  vec3 vbin = vec3(0.0);\n  vec3 vtan = vec3(0.0);\n";
  if (s.normalMatrix) {
    out += "  int ni = int(mod(slot0, 32.0));\n";
    out += "  nrm = vec3(dot(uNrm[ni].xyz, lNrm), dot(uNrm[ni + 1].xyz, lNrm), dot(uNrm[ni + 2].xyz, lNrm));\n";
    out += "  if (dot(nrm, nrm) > 0.0) nrm = normalize(nrm);\n";
    if (Reads(s, kAttrBin)) {
      out += "  vbin = vec3(dot(uNrm[ni].xyz, lBin), dot(uNrm[ni + 1].xyz, lBin), dot(uNrm[ni + 2].xyz, lBin));\n";
      out += "  vtan = vec3(dot(uNrm[ni].xyz, lTan), dot(uNrm[ni + 1].xyz, lTan), dot(uNrm[ni + 2].xyz, lTan));\n";
    }
  }
  if (k.orthographic)
    out += "  vec4 clip = vec4(uProj[0].x * pos.x + uProj[0].y, uProj[0].z * pos.y + uProj[0].w, uProj[1].x * pos.z "
           "+ uProj[1].y, 1.0);\n";
  else
    out += "  vec4 clip = vec4(uProj[0].x * pos.x + uProj[0].y * pos.z, uProj[0].z * pos.y + uProj[0].w * pos.z, "
           "uProj[1].x * pos.z + uProj[1].y, -pos.z);\n";
  // GX viewport (EFB pixels + 342) to the GL viewport over the whole EFB; no flip
  out += "  float W = uVp[1].z, H = uVp[1].w;\n";
  out += "  gl_Position = vec4(((uVp[0].w - 342.0) * 2.0 / W - 1.0) * clip.w + uVp[0].x * 2.0 / W * clip.x,\n"
         "                     ((uVp[1].x - 342.0) * 2.0 / H - 1.0) * clip.w + uVp[0].y * 2.0 / H * clip.y,\n"
         "                     2.0 * (uVp[0].z * clip.z + uVp[1].y * clip.w) / 16777215.0 - clip.w, clip.w);\n"
         "  gl_PointSize = max(1.0, uProj[1].w);\n";
  // lighting channels
  out += "  vec4 col0 = vec4(1.0);\n  vec4 col1 = vec4(1.0);\n";
  for (unsigned c = 0; c < 2 && c < k.numChans; c++) {
    Light(out, k, s, static_cast<int>(c), false);
    Light(out, k, s, static_cast<int>(c), true);
  }
  if (k.numChans == 0)
    out += Reads(s, kAttrClr0) ? "  col0 = aClr0;\n" : "  col0 = vec4(1.0);\n";
  if (k.numChans < 2)
    out += Reads(s, kAttrClr1) ? "  col1 = aClr1;\n" : "  col1 = col0;\n";
  out += "  vColor0 = col0;\n  vColor1 = col1;\n";
  // texture coordinate generation: t0..t7 as GX makes them, then scaled for the fragment shader
  for (unsigned i = 0; i < k.numTexGens; i++) {
    const std::uint32_t g = k.texGen[i];
    const unsigned proj = (g >> 1) & 1, form = (g >> 2) & 1, type = GenType(g), row = GenRow(g);
    Add(out, "  vec3 t%u = vec3(0.0, 0.0, 1.0);\n", i);
    if (type == 1) { // emboss: the source coordinate moved along the light in tangent space
      const unsigned source = (g >> 12) & 7, light = (g >> 15) & 7;
      if (source < i && Reads(s, kAttrBin)) {
        Add(out, "  {\n    vec3 ldir = normalize(uLight[%d].xyz - pos);\n", LightSlot(s, light) * 5 + 3);
        Add(out, "    t%u = t%u + vec3(dot(ldir, vtan), dot(ldir, vbin), 0.0);\n  }\n", i, source);
      } else if (source < i) {
        Add(out, "  t%u = t%u;\n", i, source);
      }
      continue;
    }
    if (type == 2 || type == 3) {
      Add(out, "  t%u = vec3(col%u.xy, 1.0);\n", i, type - 2);
      continue;
    }
    out += "  {\n    vec4 src = vec4(0.0, 0.0, 1.0, 1.0);\n";
    if (row == 0)
      out += "    src = vec4(lPos, 1.0);\n";
    else if (row == 1 && Reads(s, kAttrNrm))
      out += "    src = vec4(lNrm, 1.0);\n";
    else if (row == 2)
      out += "    src = col0;\n";
    else if (row == 3 && Reads(s, kAttrTan))
      out += "    src = vec4(lTan, 1.0);\n";
    else if (row == 4 && Reads(s, kAttrBin))
      out += "    src = vec4(lBin, 1.0);\n";
    else if (row >= 5 && row <= 12 && Reads(s, kAttrTex0 + static_cast<int>(row - 5)))
      Add(out, "    src = vec4(lTex%u, 1.0, 1.0);\n", row - 5);
    if (form == 0)
      out += "    src.z = 1.0;\n";
    Add(out, "    int ti = int(%s);\n", Slot(k, i + 1).c_str());
    if (proj)
      out += "    vec3 t = vec3(dot(src, uPosTex[ti]), dot(src, uPosTex[ti + 1]), dot(src, uPosTex[ti + 2]));\n";
    else
      out += "    vec3 t = vec3(dot(src, uPosTex[ti]), dot(src, uPosTex[ti + 1]), 1.0);\n";
    if (k.dualTex) {
      const std::uint32_t post = k.postGen[i];
      if (post & 0x100)
        out += "    if (dot(t, t) > 0.0) t = normalize(t);\n";
      const int pr = static_cast<int>(post & 63);
      const int r0 = PostSlot(s, pr), r1 = PostSlot(s, pr + 1), r2 = PostSlot(s, pr + 2);
      Add(out,
          "    t = vec3(dot(uPost[%d].xyz, t) + uPost[%d].w, dot(uPost[%d].xyz, t) + uPost[%d].w, "
          "dot(uPost[%d].xyz, t) + uPost[%d].w);\n",
          r0, r0, r1, r1, r2, r2);
    }
    // q = 0: the GameCube's special case; z = 1 then, so the fragment shader's division keeps it
    out += "    if (t.z == 0.0) { t.xy = clamp(t.xy / 2.0, vec2(-1.0), vec2(1.0)); t.z = 1.0; }\n";
    Add(out, "    t%u = t;\n  }\n", i);
  }
  for (unsigned i = 0; i < k.numTexGens; i++) {
    if (s.coordMap[i] == -2)
      continue;
    const char *scale = (i & 1) ? "zw" : "xy";
    if (s.projective[i])
      Add(out, "  vTex%u = vec3(t%u.xy * uTcScale[%u].%s, t%u.z);\n", i, i, i / 2, scale, i);
    else
      Add(out, "  vTex%u = t%u.xy * uTcScale[%u].%s;\n", i, i, i / 2, scale);
  }
  out += "}\n";
  return out;
}

namespace {

const char *kColorIn[16] = {"prev.rgb",     "prev.aaa",     "c0.rgb",        "c0.aaa",       "c1.rgb",
                            "c1.aaa",       "c2.rgb",       "c2.aaa",        "textemp.rgb",  "textemp.aaa",
                            "rastemp.rgb",  "rastemp.aaa",  "vec3(255.0)",   "vec3(128.0)",  "konsttemp.rgb",
                            "vec3(0.0)"};
const char *kAlphaIn[8] = {"prev.a", "c0.a", "c1.a", "c2.a", "textemp.a", "rastemp.a", "konsttemp.a", "0.0"};
const char *kDest[4] = {"prev", "c0", "c1", "c2"};
const char *kKonstConst[8] = {"255.0", "223.0", "191.0", "159.0", "128.0", "96.0", "64.0", "32.0"};
const char kChannel[4] = {'r', 'g', 'b', 'a'};

std::string KonstColor(unsigned sel) {
  if (sel < 8)
    return std::string("vec3(") + kKonstConst[sel] + ")";
  if (sel >= 0x0C && sel <= 0x0F)
    return "uKonst[" + std::to_string(sel - 0x0C) + "].rgb";
  if (sel >= 0x10)
    return "uKonst[" + std::to_string(sel & 3) + "]." + std::string(3, kChannel[(sel - 0x10) >> 2]);
  return "vec3(0.0)";
}

std::string KonstAlpha(unsigned sel) {
  if (sel < 8)
    return kKonstConst[sel];
  if (sel >= 0x10)
    return "uKonst[" + std::to_string(sel & 3) + "]." + std::string(1, kChannel[(sel - 0x10) >> 2]);
  return "0.0";
}

std::string Swizzle(std::uint32_t table) {
  std::string s;
  for (int i = 0; i < 4; i++)
    s += kChannel[(table >> (2 * i)) & 3];
  return s;
}

// (d + bias) op lerp(a, b, c), scaled, with the hardware's rounding (gx_gl_shader.cpp Regular):
// c widened to 0..256, a scale above one inside the lerp, a rounding bias before the division by
// 256 (not with the halving scale). lerp / 256 = a + (b - a) c' / 256: whole numbers below 2^17.
std::string RegularOp(const char *comp, unsigned bias, unsigned op, unsigned scale) {
  static const char *biasStr[4] = {"", " + 128.0", " - 128.0", ""};
  static const char *mul[4] = {"", " * 2.0", " * 4.0", ""};
  const std::string c = comp;
  const std::string d = "((tevin_d." + c + biasStr[bias] + ")" + mul[scale] + ")";
  const std::string lerp = "((tevin_a." + c + " + (tevin_b." + c + " - tevin_a." + c + ") * ((tevin_c." + c +
                           " + floor(tevin_c." + c + " / 128.0)) / 256.0))" + mul[scale] + ")";
  const std::string rounded = "floor(" + lerp + (scale != 3 ? (op ? " + 0.49609375" : " + 0.5") : "") + ")";
  const std::string v = "(" + d + (op ? " - " : " + ") + rounded + ")";
  return scale == 3 ? "floor(" + v + " * 0.5)" : v;
}

const char *kColorCompare[8] = {"(tevin_a.r > tevin_b.r ? tevin_c.rgb : vec3(0.0))",
                                "(tevin_a.r == tevin_b.r ? tevin_c.rgb : vec3(0.0))",
                                "(gt16(tevin_a.rgb, tevin_b.rgb) ? tevin_c.rgb : vec3(0.0))",
                                "(eq16(tevin_a.rgb, tevin_b.rgb) ? tevin_c.rgb : vec3(0.0))",
                                "(gt24(tevin_a.rgb, tevin_b.rgb) ? tevin_c.rgb : vec3(0.0))",
                                "(eq24(tevin_a.rgb, tevin_b.rgb) ? tevin_c.rgb : vec3(0.0))",
                                "(vec3(greaterThan(tevin_a.rgb, tevin_b.rgb)) * tevin_c.rgb)",
                                "(vec3(equal(tevin_a.rgb, tevin_b.rgb)) * tevin_c.rgb)"};
const char *kAlphaCompare[8] = {"(tevin_a.r > tevin_b.r ? tevin_c.a : 0.0)",
                                "(tevin_a.r == tevin_b.r ? tevin_c.a : 0.0)",
                                "(gt16(tevin_a.rgb, tevin_b.rgb) ? tevin_c.a : 0.0)",
                                "(eq16(tevin_a.rgb, tevin_b.rgb) ? tevin_c.a : 0.0)",
                                "(gt24(tevin_a.rgb, tevin_b.rgb) ? tevin_c.a : 0.0)",
                                "(eq24(tevin_a.rgb, tevin_b.rgb) ? tevin_c.a : 0.0)",
                                "(tevin_a.a > tevin_b.a ? tevin_c.a : 0.0)",
                                "(tevin_a.a == tevin_b.a ? tevin_c.a : 0.0)"};

std::string AlphaFunc(unsigned f, const char *ref) {
  static const char *op[8] = {"false", "<", "==", "<=", ">", "!=", ">=", "true"};
  if (f == 0 || f == 7)
    return op[f];
  return std::string("(prev.a ") + op[f] + " " + ref + ")";
}

// uLodBias / uMapScale entries of map m
std::string Bias(unsigned m) { return "uLodBias[" + std::to_string(m / 4) + "]." + kXyzw[m % 4]; }

} // namespace

std::string GxFragmentShader(const ShaderKey &k, const ShaderInfo &s) {
  // samplers too: texture reads have the sampler's precision, lowp by default (the Tegra 3's lowp
  // is fixed point in 1/256, which turns some of the 255 steps of a texel into the next)
  std::string out = "#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\nprecision highp sampler2D;\n#else\n"
                    "precision mediump float;\nprecision mediump sampler2D;\n#endif\n";
  for (int m = 0; m < 8; m++)
    if (k.texMaps & (1u << m))
      Add(out, "uniform sampler2D s%d;\n", m);
  out += "uniform vec4 uColors[4];\nuniform vec4 uKonst[4];\nuniform vec4 uAlphaRef;\nuniform vec4 uFog[2];\n"
         "uniform vec4 uLodBias[2];\n";
  bool texel = false;
  for (unsigned i = 0; i < k.numTexGens; i++)
    texel = texel || s.coordMap[i] == -1;
  if (texel || s.fixedPoint)
    out += "uniform vec4 uMapScale[8];\n";
  if (s.fixedPoint)
    out += "uniform vec4 uIndScale[4];\nuniform vec4 uIndMtx[6];\n";
  out += "varying vec4 vColor0;\nvarying vec4 vColor1;\n";
  for (unsigned i = 0; i < k.numTexGens; i++)
    if (s.coordMap[i] != -2)
      Add(out, "varying %s vTex%u;\n", s.projective[i] ? "vec3" : "vec2", i);
  out += "vec4 wrap8(vec4 v) { return v - 256.0 * floor(v / 256.0); }\n"
         "vec2 trunc2(vec2 v) { return sign(v) * floor(abs(v)); }\n"
         "bool gt16(vec3 a, vec3 b) { return a.g > b.g || (a.g == b.g && a.r > b.r); }\n"
         "bool eq16(vec3 a, vec3 b) { return a.r == b.r && a.g == b.g; }\n"
         "bool gt24(vec3 a, vec3 b) { return a.b > b.b || (a.b == b.b && gt16(a, b)); }\n"
         "bool eq24(vec3 a, vec3 b) { return eq16(a, b) && a.b == b.b; }\n"
         "vec2 shiftTrans(vec2 t, float w) { return w >= 0.0 ? floor(t * exp2(-w)) : t * exp2(-w); }\n";
  // 0..1 to 0..255; with texelCoords as the GLES 3 renderer's round() on Mesa (ties to even: a
  // linear read at a texel corner gives x.5), else half up (cheaper)
  if (k.texelCoords)
    out += "vec4 to8(vec4 v) {\n  vec4 x = v * 255.0;\n  vec4 r = floor(x + 0.5);\n"
           "  return r - vec4(equal(r - x, vec4(0.5))) * mod(r, 2.0);\n}\n";
  else
    out += "vec4 to8(vec4 v) { return floor(v * 255.0 + 0.5); }\n";
  // a map read at a coordinate in 1/128 texels
  if (texel || s.fixedPoint)
    for (int m = 0; m < 8; m++)
      if (k.texMaps & (1u << m))
        Add(out,
            "vec4 texmap%d(vec2 c) {\n"
            "  return to8(texture2D(s%d, c * uMapScale[%d].xy, %s));\n"
            "}\n",
            m, m, m, Bias(static_cast<unsigned>(m)).c_str());
  out += "void main() {\n";
  out += "  vec4 prev = uColors[0];\n  vec4 c0 = uColors[1];\n  vec4 c1 = uColors[2];\n  vec4 c2 = uColors[3];\n";
  out += "  vec4 rastemp = vec4(0.0);\n  vec4 textemp = vec4(0.0);\n  vec4 konsttemp = vec4(0.0);\n";
  out += "  vec4 tevin_a, tevin_b, tevin_c, tevin_d;\n";
  out += "  vec2 tevcoord = vec2(0.0);\n  float alphabump = 0.0;\n";
  out += "  vec4 ras0 = to8(clamp(vColor0, 0.0, 1.0));\n";
  out += "  vec4 ras1 = to8(clamp(vColor1, 0.0, 1.0));\n";
  // coordinates in 1/128 texels: truncated like the GLES 3 renderer's integers
  for (unsigned i = 0; i < k.numTexGens; i++)
    if (s.coordMap[i] == -1) {
      if (s.projective[i])
        Add(out, "  vec2 fixuv%u = trunc2(vTex%u.xy / vTex%u.z);\n", i, i, i);
      else
        Add(out, "  vec2 fixuv%u = trunc2(vTex%u);\n", i, i);
    }
  // indirect texture lookups
  for (unsigned st = 0; st < k.numIndStages && st < 4; st++) {
    const unsigned map = (k.indRef >> (6 * st)) & 7, coord = (k.indRef >> (6 * st + 3)) & 7;
    if (coord < k.numTexGens && (k.texMaps & (1u << map)))
      Add(out, "  vec3 iindtex%u = texmap%u(floor(fixuv%u * uIndScale[%u].xy)).abg;\n", st, map, coord, st);
    else
      Add(out, "  vec3 iindtex%u = vec3(0.0);\n", st);
  }
  for (unsigned n = 0; n < k.numStages; n++) {
    const std::uint32_t cc = k.colorEnv[n], ac = k.alphaEnv[n], order = k.order[n], ind = k.indCmd[n];
    const unsigned map = order & 7, coord = (order >> 3) & 7, enable = (order >> 6) & 1, chan = (order >> 7) & 7;
    const bool hasCoord = coord < k.numTexGens;
    Add(out, "  // stage %u\n  {\n", n);
    const unsigned bt = ind & 3, fmt = (ind >> 2) & 3, bias = (ind >> 4) & 7, bs = (ind >> 7) & 3,
                   mid = (ind >> 9) & 15, sw = (ind >> 13) & 7, tw = (ind >> 16) & 7, addprev = (ind >> 20) & 1;
    const bool indirect = ind != 0 && bt < k.numIndStages;
    if (indirect) {
      static const char *fmtMod[4] = {"", "32.0", "16.0", "8.0"};
      static const char *alphaMod[4] = {"8.0", "32.0", "16.0", "8.0"};
      static const char *biasField[8] = {"", "x", "y", "xy", "z", "xz", "yz", "xyz"};
      static const char *biasAdd[4] = {"-128.0", "1.0", "1.0", "1.0"};
      if (bs) {
        const char comp = "xxyz"[bs];
        Add(out, "    alphabump = iindtex%u.%c - mod(iindtex%u.%c, %s);\n", bt, comp, bt, comp, alphaMod[fmt]);
      }
      if (fmt)
        Add(out, "    vec3 crd = mod(iindtex%u, %s);\n", bt, fmtMod[fmt]);
      else
        Add(out, "    vec3 crd = iindtex%u;\n", bt);
      if (bias)
        Add(out, "    crd.%s += %s;\n", biasField[bias], biasAdd[fmt]);
      out += "    vec2 trans = vec2(0.0);\n";
      if (mid >= 1 && mid <= 3) {
        const unsigned r = 2 * (mid - 1);
        Add(out, "    trans = floor(vec2(dot(uIndMtx[%u].xyz, crd), dot(uIndMtx[%u].xyz, crd)) / 8.0);\n", r, r + 1);
        Add(out, "    trans = shiftTrans(trans, uIndMtx[%u].w);\n", r);
      } else if (mid >= 5 && mid <= 7 && hasCoord) {
        const unsigned r = 2 * (mid - 5);
        Add(out, "    trans = shiftTrans(floor(fixuv%u * crd.xx / 256.0), uIndMtx[%u].w);\n", coord, r);
      } else if (mid >= 9 && mid <= 11 && hasCoord) {
        const unsigned r = 2 * (mid - 9);
        Add(out, "    trans = shiftTrans(floor(fixuv%u * crd.yy / 256.0), uIndMtx[%u].w);\n", coord, r);
      }
      // wrap 1-5: 256 .. 16 texels (in 1/128), 6: zero, 7: none (the GLES 3 renderer's & -1)
      static const char *wrap[8] = {"", "32768.0", "16384.0", "8192.0", "4096.0", "2048.0", "", ""};
      if (hasCoord) {
        Add(out, "    vec2 wrapped = fixuv%u;\n", coord);
        if (sw == 6)
          out += "    wrapped.x = 0.0;\n";
        else if (sw && sw < 6)
          Add(out, "    wrapped.x = mod(wrapped.x, %s);\n", wrap[sw]);
        if (tw == 6)
          out += "    wrapped.y = 0.0;\n";
        else if (tw && tw < 6)
          Add(out, "    wrapped.y = mod(wrapped.y, %s);\n", wrap[tw]);
      } else {
        out += "    vec2 wrapped = vec2(0.0);\n";
      }
      out += addprev ? "    tevcoord += wrapped + trans;\n" : "    tevcoord = wrapped + trans;\n";
    } else if (s.fixedPoint) {
      if (hasCoord && s.coordMap[coord] == -1)
        Add(out, "    tevcoord = fixuv%u;\n", coord);
      else
        out += "    tevcoord = vec2(0.0);\n";
    }
    const std::string rswap = Swizzle(k.swap[ac & 3]);
    const std::string tswap = Swizzle(k.swap[(ac >> 2) & 3]);
    if (enable && (k.texMaps & (1u << map))) {
      if (indirect || s.fixedPoint) {
        Add(out, "    textemp = texmap%u(tevcoord).%s;\n", map, tswap.c_str());
      } else if (hasCoord && s.coordMap[coord] == static_cast<int>(map)) {
        // read as the vertex shader scaled it
        if (s.projective[coord])
          Add(out, "    textemp = to8(texture2DProj(s%u, vTex%u, %s)).%s;\n", map, coord,
              Bias(map).c_str(), tswap.c_str());
        else
          Add(out, "    textemp = to8(texture2D(s%u, vTex%u, %s)).%s;\n", map, coord, Bias(map).c_str(),
              tswap.c_str());
      } else if (hasCoord && s.coordMap[coord] == -1) {
        Add(out, "    textemp = texmap%u(fixuv%u).%s;\n", map, coord, tswap.c_str());
      } else {
        // no coordinate: texel 0 as the GLES 3 renderer reads it
        Add(out, "    textemp = to8(texture2D(s%u, vec2(0.0), %s)).%s;\n", map, Bias(map).c_str(),
            tswap.c_str());
      }
    } else {
      out += "    textemp = vec4(255.0);\n";
    }
    switch (chan) {
    case 0:
      Add(out, "    rastemp = ras0.%s;\n", rswap.c_str());
      break;
    case 1:
      Add(out, "    rastemp = ras1.%s;\n", rswap.c_str());
      break;
    case 5:
      out += "    rastemp = vec4(alphabump);\n";
      break;
    case 6:
      out += "    rastemp = vec4(alphabump + floor(alphabump / 32.0));\n";
      break;
    default:
      out += "    rastemp = vec4(0.0);\n";
      break;
    }
    const unsigned kc = k.konst[n] & 0xFF, ka = (k.konst[n] >> 8) & 0xFF;
    Add(out, "    konsttemp = vec4(%s, %s);\n", KonstColor(kc).c_str(), KonstAlpha(ka).c_str());
    Add(out, "    tevin_a = wrap8(vec4(%s, %s));\n", kColorIn[(cc >> 12) & 15], kAlphaIn[(ac >> 13) & 7]);
    Add(out, "    tevin_b = wrap8(vec4(%s, %s));\n", kColorIn[(cc >> 8) & 15], kAlphaIn[(ac >> 10) & 7]);
    Add(out, "    tevin_c = wrap8(vec4(%s, %s));\n", kColorIn[(cc >> 4) & 15], kAlphaIn[(ac >> 7) & 7]);
    Add(out, "    tevin_d = vec4(%s, %s);\n", kColorIn[cc & 15], kAlphaIn[(ac >> 4) & 7]);
    {
      const unsigned b = (cc >> 16) & 3, op = (cc >> 18) & 1, clampOut = (cc >> 19) & 1, scale = (cc >> 20) & 3,
                     dest = (cc >> 22) & 3;
      const std::string v = b == 3 ? std::string("(tevin_d.rgb + ") + kColorCompare[op | scale << 1] + ")"
                                   : RegularOp("rgb", b, op, scale);
      Add(out, "    %s.rgb = clamp(%s, %s);\n", kDest[dest], v.c_str(),
          clampOut ? "vec3(0.0), vec3(255.0)" : "vec3(-1024.0), vec3(1023.0)");
    }
    {
      const unsigned b = (ac >> 16) & 3, op = (ac >> 18) & 1, clampOut = (ac >> 19) & 1, scale = (ac >> 20) & 3,
                     dest = (ac >> 22) & 3;
      const std::string v = b == 3 ? std::string("(tevin_d.a + ") + kAlphaCompare[op | scale << 1] + ")"
                                   : RegularOp("a", b, op, scale);
      Add(out, "    %s.a = clamp(%s, %s);\n", kDest[dest], v.c_str(), clampOut ? "0.0, 255.0" : "-1024.0, 1023.0");
    }
    out += "  }\n";
  }
  if (k.numStages) {
    const unsigned cd = (k.colorEnv[k.numStages - 1] >> 22) & 3, ad = (k.alphaEnv[k.numStages - 1] >> 22) & 3;
    if (cd)
      Add(out, "  prev.rgb = %s.rgb;\n", kDest[cd]);
    if (ad)
      Add(out, "  prev.a = %s.a;\n", kDest[ad]);
  }
  out += "  prev = wrap8(prev);\n";
  // alpha test
  {
    const unsigned f0 = k.alphaTest & 7, f1 = (k.alphaTest >> 3) & 7, logic = (k.alphaTest >> 6) & 3;
    static const char *logicOp[4] = {"&&", "||", "!=", "=="};
    const bool always = (f0 == 7 && f1 == 7) || (logic == 1 && (f0 == 7 || f1 == 7));
    if (!always)
      Add(out, "  if (!(%s %s %s)) discard;\n", AlphaFunc(f0, "uAlphaRef.x").c_str(), logicOp[logic],
          AlphaFunc(f1, "uAlphaRef.y").c_str());
  }
  // fog: uFog[0] = A, C, B magnitude / 2^24, 2^-shift (z * (2^24 - 1) >> shift as a fraction of 2^24)
  {
    const unsigned proj = k.fog & 1, fsel = (k.fog >> 1) & 7;
    if (fsel) {
      if (proj == 0)
        out += "  float ze = uFog[0].x / (uFog[0].z - gl_FragCoord.z * uFog[0].w);\n";
      else
        out += "  float ze = uFog[0].x * gl_FragCoord.z * 0.99999994;\n";
      out += "  float fog = clamp(ze - uFog[0].y, 0.0, 1.0);\n";
      if (fsel == 4)
        out += "  fog = 1.0 - exp2(-8.0 * fog);\n";
      else if (fsel == 5)
        out += "  fog = 1.0 - exp2(-8.0 * fog * fog);\n";
      else if (fsel == 6)
        out += "  fog = exp2(-8.0 * (1.0 - fog));\n";
      else if (fsel == 7)
        out += "  fog = 1.0 - fog;\n  fog = exp2(-8.0 * fog * fog);\n";
      out += "  float ifog = floor(fog * 256.0 + 0.5);\n";
      out += "  prev.rgb = prev.rgb + floor((uFog[1].rgb - prev.rgb) * (ifog / 256.0));\n";
    }
  }
  out += "  gl_FragColor = prev / 255.0;\n";
  // destination alpha (GXSetDstAlpha) when the renderer lets the shader write it
  out += "  if (uAlphaRef.z >= 0.0) gl_FragColor.a = uAlphaRef.z / 255.0;\n}\n";
  return out;
}

} // namespace re4gx2
