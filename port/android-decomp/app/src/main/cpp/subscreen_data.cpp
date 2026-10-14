#include "subscreen_data.h"
#include "weapon_carry_source.h"
#include "weapon_preview.h"
#include "asset_reader.h"
#include "room_objects.h"
#include "inventory_case.h"
#include "sat.h"
#include "room_items.h"
#include "animation.h"
#include "generic_event_actor.h"
#include <limits>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace re4 {
namespace {
std::vector<std::uint8_t> CoreBody(const std::string &discFiles);
constexpr std::size_t FileBudget = 32u * 1024 * 1024;
std::vector<std::uint8_t> ReadFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) throw std::runtime_error("Original menu: File missing: " + path);
  const auto end = file.tellg();
  if (end <= 0 || static_cast<std::uint64_t>(end) > FileBudget)
    throw std::runtime_error("Original menu: File budget");
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(bytes.data()), bytes.size()))
    throw std::runtime_error("Original menu: Truncated file");
  return bytes;
}
std::vector<std::uint8_t> Slice(const std::vector<std::uint8_t> &bytes,
                               std::size_t offset, std::size_t end) {
  AssetReader{bytes}.range(offset, end >= offset ? end - offset : bytes.size());
  if (end <= offset) throw std::runtime_error("Original menu: empty entry");
  return {bytes.begin() + offset, bytes.begin() + end};
}
std::string Language(const std::string &name) {
  for (const auto *valid : {"jpn", "eng", "ger", "fra", "esp", "ita"})
    if (name == valid) return name;
  throw std::runtime_error("Original menu: Invalid language");
}
std::string DiscLanguage(const std::string &name) {
  auto result = Language(name); result[0] -= 'a' - 'A'; return result;
}
SmdModel Model(const std::vector<std::uint8_t> &bin) {
  // Same bounded standalone-BIN wrapper used by model_skin.cpp; unlike a
  // skinned actor, a static map/item BIN need not contain a weight palette.
  AssetReader{bin}.range(0, 64);
  std::vector<std::uint8_t> resource(32);
  resource[0] = 0x40; resource[7] = 16; resource[19] = 16;
  resource.insert(resource.end(), bin.begin(), bin.end());
  Smd parsed(resource, {1, 0, 0});
  auto result = std::move(parsed.models.at(0));
  result.offset = 0;
  return result;
}
std::size_t TableExtent(const std::vector<std::uint8_t> &bytes,
                        unsigned index, std::size_t &offset) {
  AssetReader r{bytes};
  if (bytes.size() > FileBudget || index < 4 || index > 2047)
    throw std::runtime_error("Original menu: Invalid archive index");
  r.range(std::size_t(index) * 4, 4);
  // Word zero is the sub-file count, words 1..3 reserved; offset slots start
  // at word 4. ss_pzzl's count 0x1a8 includes its final slot 0x1ab.
  const auto count = r.u32(0);
  if (!count || count > 2044 || index - 4 >= count)
    throw std::runtime_error("Original menu: Offset directory is missing");
  const std::size_t boundary = std::size_t(count + 4) * 4;
  r.range(0, boundary);
  offset = r.u32(std::size_t(index) * 4);
  if (offset < boundary || offset >= bytes.size() || (offset & 3))
    throw std::runtime_error("Original menu: Invalid file offset");
  std::size_t end = bytes.size();
  for (std::size_t p = 16; p + 4 <= boundary; p += 4) {
    const auto entry = r.u32(p);
    if (entry > offset && entry < end) end = entry;
  }
  return end;
}
float LastCurve(const AssetReader &r, std::size_t offset, float fallback) {
  if (!offset) return fallback;
  const auto count = r.u32(offset);
  if (count == 0 || count == 0xffffffffu) return fallback;
  if (count > 64) throw std::runtime_error("Original menu: Curve budget");
  r.range(offset + 4, std::size_t(count) * 16);
  float previous = -1;
  for (unsigned i = 0; i < count; ++i) {
    const auto time = r.f32(offset + 4 + i * 16);
    if (time < previous) throw std::runtime_error("Original menu: Curve time");
    previous = time;
  }
  return r.f32(offset + 8 + (count - 1) * 16);
}
unsigned MessageLanguage(const std::string &language) {
  const auto lang = Language(language);
  if (lang == "jpn") return 0;
  if (lang == "eng") return 1;
  if (lang == "ger") return 2;
  if (lang == "fra") return 3;
  if (lang == "esp") return 4;
  return 5;
}
} // namespace

std::vector<std::uint8_t>
CopySubscreenEntry(const std::vector<std::uint8_t> &archive, unsigned index) {
  std::size_t offset = 0;
  const auto end = TableExtent(archive, index, offset);
  return Slice(archive, offset, end);
}

std::vector<SubscreenTexture>
DecodeSubscreenIdTextures(const std::vector<std::uint8_t> &pack) {
  AssetReader r{pack}; r.range(0, 32);
  if (r.u32(0) != 0xB) throw std::runtime_error("Original menu: IdTex version");
  const std::size_t ids = r.u32(4), tpls = r.u32(24), anims = r.u32(28);
  const auto count = r.u32(ids);
  if (!count || count > 512 || r.u32(tpls) < count || r.u32(anims) < count)
    throw std::runtime_error("Original menu: IdTex table budget");
  r.range(ids + 4, std::size_t(count) * 8);
  r.range(tpls + 4, std::size_t(count) * 4);
  r.range(anims + 4, std::size_t(count) * 4);
  std::vector<SubscreenTexture> result;
  std::size_t totalPixels = 0;
  for (unsigned i = 0; i < count; ++i) {
    const unsigned id = r.u16(ids + 4 + i * 8) & 255;
    const std::size_t p = tpls + r.u32(tpls + 4 + i * 4);
    const std::size_t a = anims + r.u32(anims + 4 + i * 4);
    r.range(p, 12); r.range(a, 10);
    const unsigned frames = r.u32(p + 4);
    if (r.u32(p) != 0x20af30 || !frames || frames > 128 ||
        !r.u16(a + 8) || r.u16(a + 8) > frames)
      throw std::runtime_error("Original menu: IdTex frame budget");
    // All TPL-internal offsets are relative to this TPL's base, not the pack.
    const auto tpl = Slice(pack, p, pack.size());
    for (unsigned frame = 0; frame < frames; ++frame) {
      auto image = DecodeTpl(tpl, frame);
      totalPixels += std::size_t(image.width) * image.height;
      if (totalPixels > 16u * 1024 * 1024)
        throw std::runtime_error("Original menu: Texture pixel budget");
      result.push_back({id, frame, std::move(image),r.u16(a+8)});
    }
  }
  return result;
}

std::vector<SubscreenQuad>
DecodeSubscreenLayout(const std::vector<std::uint8_t> &table, unsigned group) {
  AssetReader r{table}; r.range(0, 8);
  const bool v2 = table[0] == '2' && table[1] == '.';
  if (!v2 && !(table[0] == '1' && table[1] == '.'))
    throw std::runtime_error("Original menu: ID layout version");
  const unsigned stride = v2 ? 0x8c : 0x88, count = table[5];
  r.range(8, std::size_t(count) * stride);
  std::vector<SubscreenQuad> result;
  result.reserve(count);
  for (unsigned i = 0; i < count; ++i) {
    const std::size_t p = 8 + i * stride, curve = p + (v2 ? 0x74 : 0x70);
    SubscreenQuad q;
    q.group = group; q.flags = table[p + 3]; q.mark = table[p + 4];
    q.unit = table[p + 5]; q.parent = table[p + 7]; q.kind = table[p + 9];
    q.texture = table[p + 11]; q.anchor = table[p + 12] & 15;
    // idDrawCommon enables the second texture stage only when tex_flag bit
    // zero is set. Unused maskId fields are often zero, not TEX_ID_NULL.
    q.mask = (table[p + (v2 ? 0x6f : 0x6b)] & 1)
                 ? table[p + (v2 ? 0x6e : 0x6a)] : 255;
    q.blend = table[p + (v2 ? 0x6c : 0x68)];
    for (unsigned j = 0; j < 3; ++j) {
      q.position[j] = r.f32(p + 0x14 + j * 4);
      q.rotation[j] = r.f32(p + (v2 ? 0x60 : 0x5c) + j * 4);
    }
    // idSysMove00 subtracts FuncPath(t=0) from the interpolated point and
    // adds pos0. At the authored final n-1 parameter, the B-spline passes
    // through its final control point exactly. Keep unended paths at pos0.
    const auto path = r.u32(curve), positionCurve = r.u32(curve + 8);
    if (path) {
      r.range(path, 8); const unsigned points = table[path + 7];
      if (points > 64) throw std::runtime_error("Original menu: Path budget");
      r.range(path + 8, std::size_t(points) * 12);
      if (points > 1 && positionCurve &&
          std::abs(LastCurve(r, positionCurve, 0) - float(points - 1)) < .00001f)
        for (unsigned axis = 0; axis < 3; ++axis)
          q.position[axis] += r.f32(path + 8 + (points - 1) * 12 + axis * 4) -
                              r.f32(path + 8 + axis * 4);
    }
    const auto rotationCurve = r.u32(curve + 20);
    const unsigned axis = table[p + 15];
    if (rotationCurve && axis < 3)
      q.rotation[axis] = LastCurve(r, rotationCurve, q.rotation[axis]);
    float width = r.f32(p + 0x50), height = r.f32(p + 0x54);
    const float scale = LastCurve(r, r.u32(curve + 12), 1);
    const auto scaleType = table[p + 14];
    if (!(scaleType & 0x20)) width *= scale;
    if (!(scaleType & 0x10)) height *= scale;
    if (std::abs(width) > 100000 || std::abs(height) > 100000)
      throw std::runtime_error("Original menu: ID quad budget");
    float left = -width * .5f, top = height * .5f;
    if (q.anchor == 1) { left = 0; top = 0; }
    else if (q.anchor == 2) { left = -width; top = 0; }
    else if (q.anchor == 3) { left = -width; top = height; }
    else if (q.anchor == 4) { left = 0; top = height; }
    if (q.anchor <= 4)
      q.vertices = {left, top, left + width, top, left + width, top - height,
                    left, top - height};
    else
      for (unsigned j = 0; j < 4; ++j) {
        q.vertices[j * 2] = r.f32(p + 0x20 + j * 12);
        q.vertices[j * 2 + 1] = r.f32(p + 0x24 + j * 12);
      }
    const auto colorCurve = r.u32(curve + 16);
    const bool animateColor = colorCurve && r.u32(colorCurve) && r.u32(colorCurve)!=0xffffffffu;
    const bool color1 = animateColor && v2 && r.u32(p + 0x5c) != 0;
    const float alpha = LastCurve(r, colorCurve, table[p + 0x5b]);
    for (unsigned j = 0; j < 4; ++j) {
      float color = table[p + 0x58 + j];
      if (color1) color += alpha * (float(table[p + 0x5c + j]) - color);
      else if (j == 3) color = alpha;
      q.color[j] = std::clamp(color, 0.f, 255.f);
    }
    q.hasPath = r.u32(curve) != 0;
    for (unsigned j = 2; j < 6; ++j)
      q.hasAnimation = q.hasAnimation || r.u32(curve + j * 4) != 0;
    result.push_back(q);
  }
  return result;
}

const Texture *SubscreenArt::texture(unsigned id, unsigned frame) const {
  for (const auto &t : textures) if (t.id == id && t.frame == frame) return &t.image;
  return nullptr;
}
std::string SubscreenArt::layoutJson() const {
  std::ostringstream out;
  out << std::setprecision(8) << "{\"width\":640,\"height\":480,\"quads\":[";
  bool comma = false;
  for (const auto &q : quads) {
    if (comma) out << ',';
    comma = true;
    out << "{\"mark\":" << q.mark << ",\"unit\":" << q.unit
        << ",\"parent\":" << q.parent << ",\"group\":" << q.group
        << ",\"kind\":" << q.kind << ",\"texture\":" << q.texture
        << ",\"mask\":" << q.mask << ",\"blend\":" << q.blend
        << ",\"x\":" << q.position[0] << ",\"y\":" << q.position[1]
        << ",\"rotation\":" << q.rotation[2] << ",\"vertices\":[";
    for (unsigned i = 0; i < 8; ++i) { if (i) out << ','; out << q.vertices[i]; }
    out << "],\"color\":[";
    for (unsigned i = 0; i < 4; ++i) { if (i) out << ','; out << q.color[i]; }
    out << "],\"path\":" << (q.hasPath ? "true" : "false") << '}';
  }
  out << "],\"textures\":[";
  comma = false;
  for (const auto &t : textures) {
    if (comma) out << ',';
    comma = true;
    out << "{\"id\":" << t.id << ",\"frame\":" << t.frame
        << ",\"width\":" << t.image.width << ",\"height\":" << t.image.height << '}';
  }
  return out.str() + "]}";
}

SubscreenArt LoadSubscreenArt(const std::string &discFiles,
                              SubscreenArtKind kind, const std::string &language) {
  std::string name;
  unsigned textureEntry = 0;
  std::vector<unsigned> layouts;
  switch (kind) {
  case SubscreenArtKind::Common: name = "ss_cmmn.dat"; textureEntry = 6;
    layouts = {7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0x10, 0x11}; break;
  case SubscreenArtKind::Case: name = "ss_pzzl.dat"; textureEntry = 0x1aa; layouts = {0x1ab}; break;
  case SubscreenArtKind::Keys: name = "ss_item.dat"; textureEntry = 5; layouts = {6, 7}; break;
  case SubscreenArtKind::Files: name = "ss_file.dat"; textureEntry = 6; layouts = {7, 8, 9}; break;
  case SubscreenArtKind::Map: name = "ss_map.dat"; textureEntry = 4; layouts = {5, 6, 7, 8, 9, 10, 11}; break;
  default: throw std::runtime_error("Original menu: Unknown screen");
  }
  const auto archive = ReadFile(discFiles + "/ss/" + DiscLanguage(language) + "/" + name);
  SubscreenArt result;
  result.textures = DecodeSubscreenIdTextures(CopySubscreenEntry(archive, textureEntry));
  if(kind==SubscreenArtKind::Map) {
    const auto tpl=CopySubscreenEntry(archive,0xe);
    const auto count=AssetReader{tpl}.u32(4);
    if(count>256)throw std::runtime_error("Original menu: Map TPL budget");
    for(unsigned i=0;i<count;++i)result.textures.push_back({512+i,0,DecodeTpl(tpl,i)});
  }
  // Texture registration survives across SS groups in the original. Shared
  // glyphs/digits/solid plates live in CORE_COCKPIT_TEXTURE (0x1d), whereas
  // the tab captions and metal frame override IDs in ss_cmmn's own pack.
  auto coreTextures = DecodeSubscreenIdTextures(CopySubscreenEntry(CoreBody(discFiles), 0x1d));
  for (auto &texture : coreTextures)
    if (!result.texture(texture.id, texture.frame)) result.textures.push_back(std::move(texture));
  for (const auto index : layouts) {
    auto quads = DecodeSubscreenLayout(CopySubscreenEntry(archive, index), index);
    result.quads.insert(result.quads.end(), quads.begin(), quads.end());
  }
  return result;
}

std::vector<std::uint16_t>
ReadSubscreenMessage(const std::vector<std::uint8_t> &table,
                     unsigned message, unsigned language) {
  AssetReader r{table};
  if (language > 5 || table.size() > FileBudget)
    throw std::runtime_error("Original menu: Text budget/language");
  const std::size_t block = r.u32((language + 1) * 4);
  const auto count = r.u32(block + 4);
  if (!count || count > 8192 || message >= count)
    throw std::runtime_error("Original menu: Invalid message number");
  r.range(block + 8, std::size_t(count) * 4);
  const std::size_t relative = r.u32(block + 8 + message * 4);
  if (relative < 8 + std::size_t(count) * 4 || (relative & 1))
    throw std::runtime_error("Original menu: Invalid message offset");
  const std::size_t start = block + relative;
  std::size_t end = table.size();
  // The last message of a language block ends before the next language's
  // block, even when no later message offset exists in the selected block.
  for (unsigned i = 0; i < 6; ++i) {
    const std::size_t p = r.u32((i + 1) * 4);
    if (p > start) end = std::min(end, p);
  }
  for (unsigned i = 0; i < count; ++i) {
    const std::size_t p = block + r.u32(block + 8 + i * 4);
    if (p > start) end = std::min(end, p);
  }
  r.range(start, end >= start ? end - start : table.size());
  if (end - start > 16384) throw std::runtime_error("Original menu: Message is too long");
  std::vector<std::uint16_t> result;
  for (std::size_t p = start; p + 2 <= end; p += 2) result.push_back(r.u16(p));
  return result;
}
std::vector<SubscreenGlyph>
DecodeSubscreenGlyphs(const std::vector<std::uint16_t> &words,bool choiceMarkers) {
  // mes.cpp CommandArg: exact argument counts for codes 00..12. Calls and
  // jumps require a second message table; reject rather than display their
  // argument as an invented glyph.
  static constexpr unsigned args[19] = {0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1};
  std::vector<SubscreenGlyph> result;
  unsigned line = 0, color = 0;
  for (std::size_t i = 0; i < words.size(); ++i) {
    const auto code = words[i];
    if (code >= 0x80) { result.push_back({unsigned(code - 0x80), line, color}); continue; }
    if(code==7 && choiceMarkers){result.push_back({0,line,99});continue;}
    if (code >= 19) throw std::runtime_error("Original menu: Unknown text control");
    if (code == 1) return result; // end / return at top-level
    if (i + args[code] >= words.size())
      throw std::runtime_error("Original menu: Truncated text control");
    if (code == 2 || code == 15 || code == 17 || code == 18 || code == 10)
      throw std::runtime_error("Original menu: dynamic text control");
    if (code == 3) ++line;
    if (code == 6) { color = words[i + 1]; if (color > 9) throw std::runtime_error("Original menu: Text color"); }
    i += args[code];
  }
  throw std::runtime_error("Original menu: Text end is missing");
}
SubscreenFont DecodeSubscreenFont(const std::vector<std::uint8_t> &bytes,
                                 unsigned cellWidth, unsigned cellHeight) {
  AssetReader r{bytes}; r.range(0, 8);
  if (!cellWidth || !cellHeight || cellWidth > 64 || cellHeight > 64)
    throw std::runtime_error("Original menu: Font cell");
  const std::size_t tpl = r.u32(0), widths = r.u32(4);
  if (tpl < 8 || widths < 8 || tpl >= bytes.size() || widths >= bytes.size())
    throw std::runtime_error("Original menu: Font offset");
  const auto source = Slice(bytes, tpl, widths > tpl ? widths : bytes.size());
  AssetReader t{source};
  const auto count = t.u32(4);
  if (!count || count > 32) throw std::runtime_error("Original menu: Font sheet budget");
  SubscreenFont result;
  result.cellWidth = cellWidth; result.cellHeight = cellHeight;
  std::size_t glyphs = 0;
  for (unsigned i = 0; i < count; ++i) {
    auto sheet = DecodeTpl(source, i);
    glyphs += std::size_t(sheet.width / cellWidth) * (sheet.height / cellHeight);
    result.sheets.push_back(std::move(sheet));
  }
  const std::size_t end = tpl > widths ? tpl : bytes.size();
  const auto stored = (end - widths) / 2;
  glyphs = std::min(glyphs, stored);
  if (glyphs < 2) throw std::runtime_error("Original menu: Font bearings are missing");
  for (std::size_t i = 0; i < glyphs; ++i)
    result.bearings.push_back({static_cast<std::int8_t>(bytes[widths + i * 2]),
                              static_cast<std::int8_t>(bytes[widths + i * 2 + 1])});
  return result;
}

unsigned OriginalFileNumber(unsigned id) {
  if (id >= 0xac && id <= 0xb7) return id - 0xab;
  if (id >= 0x48 && id <= 0x50) return id - 0x3b;
  if (id >= 0xf4 && id <= 0xfd) return id - 0xde;
  return 0;
}
unsigned OriginalFileItem(unsigned no) {
  if (!no || no > 31) return 0;
  if (no <= 12) return no + 0xab;
  if (no <= 21) return no + 0x3b;
  return no + 0xde;
}
unsigned OriginalKeyItemTextureFrame(unsigned itemId) {
  // Sscrn/ss_item.cpp itemTexNo: IdNum units use texId 0x80 and this frame.
  static constexpr unsigned ids[105] = {
    0xff,0xa2,0x3b,0x3c,0x3d,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,
    0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x6e,0x6f,0x70,0x77,0x82,0x83,0x84,0x85,
    0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8f,0x90,0x91,0xd2,0x93,0xd3,0xd4,0x96,
    0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,0x98,0xa1,0x74,0xa3,0xa4,0xa5,0xa6,0x0f,0xa7,0xb8,
    0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,0xc0,0xc1,0xc2,0xc3,0xc4,0x92,0x1d,0x1e,0x1f,
    0x39,0x80,0x3a,0x69,0x7a,0x7b,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
    0xd0,0xd1,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb
  };
  const auto found = std::find(std::begin(ids), std::end(ids), itemId);
  return found == std::end(ids) ? 0 : unsigned(found - std::begin(ids));
}
OriginalFilePage OriginalFilePageInfo(unsigned number, unsigned page,
                                      const std::string &language) {
  // ss_file.cpp has 31 id slots but complete reader metadata for only the
  // first 29 source files. The final two are not silently mapped elsewhere.
  if (!number || number > 29) throw std::runtime_error("Original menu: File metadata is missing");
  const bool japanese = MessageLanguage(language) == 0;
  static constexpr unsigned message[29] = {0,5,12,16,21,23,28,34,38,43,50,55,59,63,70,80,88,93,99,103,108,116,120,127,129,133,140,144,150};
  static constexpr unsigned count[29][2] = {{5,5},{6,7},{4,4},{5,5},{2,2},{5,5},{6,6},{4,4},{5,5},{6,7},{5,5},{4,4},{4,4},{4,7},{6,10},{7,8},{5,5},{5,6},{4,4},{4,5},{6,8},{4,4},{6,7},{2,2},{5,4},{5,7},{4,4},{4,6},{5,6}};
  const auto pages = count[number - 1][japanese ? 0 : 1];
  if (page >= pages) throw std::runtime_error("Original menu: Invalid document page");
  unsigned picture = 1;
  if (number == 1 || number == 3 || number == 9) picture = std::min(page + 1, number == 3 ? 4u : 5u);
  else if ((number == 2 || number == 7) && page >= 4) picture = 2;
  else if (number == 13 && page >= 2) picture = 2;
  else if (number == 16 && page >= (japanese ? 4u : 5u)) picture = 2;
  else if (number == 21 && page >= 3) picture = 2;
  std::ostringstream name;
  name << "ss/" << DiscLanguage(language) << "/f" << std::setfill('0')
       << std::setw(2) << number << char('a' + picture - 1) << ".tpl";
  return {number, page, pages, message[number - 1] + page, picture, name.str()};
}
std::vector<SubscreenGlyph> LoadOriginalFileGlyphs(const std::string &discFiles,
                                                  unsigned number, unsigned page,
                                                  const std::string &language) {
  const auto info = OriginalFilePageInfo(number, page, language);
  const auto bytes = ReadFile(discFiles + "/ss/" + DiscLanguage(language) + "/ss_file.dat");
  return DecodeSubscreenGlyphs(ReadSubscreenMessage(CopySubscreenEntry(bytes, 5),
                                                   info.message, MessageLanguage(language)));
}
Texture LoadOriginalFilePicture(const std::string &discFiles, unsigned number,
                                unsigned page, const std::string &language) {
  return DecodeTpl(ReadFile(discFiles + "/" + OriginalFilePageInfo(number, page, language).picturePath));
}
SubscreenFont LoadOriginalMenuFont(const std::string &discFiles) {
  return DecodeSubscreenFont(ReadFile(discFiles + "/font/common_p.fnt"));
}
namespace {
std::vector<std::uint8_t> CoreBody(const std::string &discFiles) {
  const auto das = ReadFile(discFiles + "/etc/core.das");
  AssetReader r{das};
  const auto offset = r.u32(0x2c), length = r.u32(0x24);
  if (r.u32(0x20) || offset != 0x400 || length < 16)
    throw std::runtime_error("Original menu: CORE-DAS-Record");
  r.range(offset, length);
  return Slice(das, offset, offset + length);
}
}
std::shared_ptr<const RoomLighting>
LoadOriginalExamLighting(const std::string &discFiles, unsigned light) {
  if (light > 4) throw std::runtime_error("Original menu: Examine light number");
  return std::make_shared<const RoomLighting>(CopySubscreenEntry(CoreBody(discFiles), 0x16 + light));
}
std::vector<SubscreenQuad> LoadOriginalExamLayout(const std::string &discFiles) {
  return DecodeSubscreenLayout(CopySubscreenEntry(CoreBody(discFiles), 0x1e), 0x1e);
}
SubscreenArt LoadOriginalExamArt(const std::string &discFiles) {
  const auto core=CoreBody(discFiles);
  SubscreenArt art;
  art.quads=DecodeSubscreenLayout(CopySubscreenEntry(core,0x1e),0x1e);
  auto textures=DecodeSubscreenIdTextures(CopySubscreenEntry(core,0x1d));
  for(auto &texture:textures)
    if(std::any_of(art.quads.begin(),art.quads.end(),[&](const auto &q) {
      return q.kind==0 && (q.texture==texture.id || q.mask==texture.id);
    }))art.textures.push_back(std::move(texture));
  return art;
}

OriginalExamProfile OriginalItemExamProfile(unsigned itemId, bool inventory) {
  // Exact game/examine.cpp exam_info / exam_info_ext values.
  struct Info { unsigned id; std::array<float, 3> rotation; float scale; unsigned light, pickup, axis; };
  static constexpr Info info[] = {
    {0x00, {60.0f, 0.0f, 0.0f}, 1.1f, 4, 0, 0},
    {0x01, {0.0f, 0.0f, 20.0f}, 1.1f, 4, 0, 0},
    {0x02, {0.0f, 0.0f, 20.0f}, 1.1f, 4, 0, 0},
    {0x03, {0.0f, 0.0f, 4.0f}, 2.2f, 2, 0, 0},
    {0x04, {60.0f, 0.0f, 0.0f}, 1.0f, 4, 0, 0},
    {0x05, {0.0f, 0.0f, 40.0f}, 1.1f, 4, 0, 0},
    {0x06, {30.0f, 0.0f, 0.0f}, 1.7f, 4, 1, 0},
    {0x07, {60.0f, 0.0f, 0.0f}, 1.2f, 4, 0, 0},
    {0x08, {0.0f, 0.0f, 30.0f}, 0.8f, 0, 0, 0},
    {0x09, {0.0f, 0.0f, 30.0f}, 0.8f, 0, 0, 0},
    {0x0A, {0.0f, 0.0f, 30.0f}, 0.8f, 0, 0, 0},
    {0x0B, {10.0f, 0.0f, 0.0f}, 1.0f, 4, 1, 1},
    {0x0C, {-30.0f, 0.0f, 0.0f}, 1.2f, 4, 0, 0},
    {0x0D, {-20.0f, 0.0f, 20.0f}, 1.7f, 4, 0, 0},
    {0x0E, {0.0f, 0.0f, 20.0f}, 1.1f, 4, 0, 0},
    {0x0F, {60.0f, 0.0f, 0.0f}, 1.5f, 4, 0, 0},
    {0x10, {0.0f, 0.0f, 0.0f}, 1.0f, 1, 0, 0},
    {0x11, {90.0f, -160.0f, 0.0f}, 1.4f, 4, 0, 0},
    {0x12, {0.0f, 0.0f, 0.0f}, 1.2f, 1, 0, 0},
    {0x13, {0.0f, 0.0f, 0.0f}, 1.2f, 1, 0, 0},
    {0x14, {0.0f, 0.0f, 0.0f}, 1.2f, 1, 0, 0},
    {0x15, {0.0f, 0.0f, 0.0f}, 1.2f, 1, 0, 0},
    {0x16, {0.0f, 0.0f, 0.0f}, 1.2f, 1, 0, 0},
    {0x17, {0.0f, 0.0f, 5.0f}, 2.5f, 2, 0, 0},
    {0x18, {60.0f, 0.0f, 0.0f}, 1.1f, 4, 0, 0},
    {0x19, {30.0f, 0.0f, 0.0f}, 1.7f, 4, 1, 0},
    {0x1A, {50.0f, 0.0f, 0.0f}, 1.3f, 4, 0, 0},
    {0x1C, {30.0f, 0.0f, 0.0f}, 1.7f, 4, 1, 0},
    {0x1D, {50.0f, 0.0f, 0.0f}, 1.3f, 4, 0, 0},
    {0x1E, {-20.0f, 0.0f, 0.0f}, 1.4f, 4, 0, 0},
    {0x1F, {-20.0f, 0.0f, 0.0f}, 1.4f, 4, 0, 0},
    {0x20, {60.0f, 0.0f, 0.0f}, 1.2f, 4, 0, 0},
    {0x21, {-30.0f, 0.0f, -20.0f}, 1.4f, 0, 0, 0},
    {0x22, {-30.0f, 0.0f, -14.0f}, 1.8f, 0, 0, 0},
    {0x23, {-30.0f, 0.0f, -20.0f}, 1.5f, 0, 0, 0},
    {0x24, {-30.0f, 0.0f, -14.0f}, 1.8f, 0, 0, 0},
    {0x25, {-30.0f, 0.0f, -14.0f}, 1.8f, 0, 0, 0},
    {0x26, {0.0f, 0.0f, 0.0f}, 2.3f, 2, 0, 0},
    {0x27, {-30.0f, 0.0f, -14.0f}, 1.5f, 0, 0, 0},
    {0x28, {0.0f, 0.0f, -8.0f}, 1.8f, 0, 0, 0},
    {0x29, {70.0f, 0.0f, 20.0f}, 2.1f, 4, 0, 0},
    {0x2A, {70.0f, 0.0f, 20.0f}, 1.5f, 4, 0, 0},
    {0x2B, {70.0f, 0.0f, 14.0f}, 1.8f, 4, 0, 0},
    {0x2C, {-30.0f, 0.0f, 4.0f}, 2.4f, 2, 0, 0},
    {0x2D, {0.0f, 0.0f, 5.0f}, 2.1f, 2, 0, 0},
    {0x2E, {0.0f, 0.0f, -5.0f}, 2.4f, 2, 0, 0},
    {0x2F, {30.0f, 0.0f, -5.0f}, 2.4f, 2, 0, 0},
    {0x30, {-30.0f, 0.0f, 12.0f}, 1.6f, 4, 0, 0},
    {0x31, {-30.0f, 0.0f, 12.0f}, 2.0f, 4, 0, 0},
    {0x32, {-25.0f, 0.0f, 12.0f}, 2.0f, 4, 0, 0},
    {0x33, {-30.0f, 0.0f, 12.0f}, 1.8f, 4, 0, 0},
    {0x34, {-30.0f, 0.0f, -10.0f}, 2.4f, 2, 0, 0},
    {0x35, {0.0f, 0.0f, 5.0f}, 2.5f, 2, 0, 0},
    {0x36, {0.0f, 0.0f, 0.0f}, 2.2f, 2, 0, 0},
    {0x37, {-30.0f, 0.0f, -14.0f}, 2.0f, 0, 0, 0},
    {0x38, {-20.0f, 0.0f, 20.0f}, 1.7f, 4, 0, 0},
    {0x39, {-20.0f, 0.0f, 0.0f}, 1.4f, 4, 0, 0},
    {0x3A, {70.0f, -30.0f, 0.0f}, 1.25f, 4, 0, 0},
    {0x3B, {60.0f, 0.0f, 20.0f}, 1.3f, 4, 0, 0},
    {0x3C, {-10.0f, 0.0f, 0.0f}, 1.4f, 4, 0, 0},
    {0x3D, {0.0f, 0.0f, 0.0f}, 0.8f, 4, 0, 0},
    {0x3E, {15.0f, 0.0f, 7.0f}, 2.3f, 4, 0, 0},
    {0x3F, {0.0f, 0.0f, 0.0f}, 1.4f, 2, 0, 0},
    {0x40, {0.0f, 0.0f, 0.0f}, 1.4f, 4, 0, 0},
    {0x41, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0x42, {0.0f, 0.0f, 0.0f}, 1.4f, 4, 0, 0},
    {0x43, {0.0f, 0.0f, 0.0f}, 1.4f, 2, 0, 0},
    {0x44, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 0, 0},
    {0x45, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 0, 0},
    {0x46, {35.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0x47, {30.0f, 0.0f, 0.0f}, 1.3f, 4, 1, 1},
    {0x51, {30.0f, 0.0f, -5.0f}, 2.4f, 2, 0, 0},
    {0x52, {65.0f, 0.0f, 0.0f}, 1.8f, 2, 0, 0},
    {0x56, {10.0f, 0.0f, 0.0f}, 1.3f, 3, 0, 0},
    {0x57, {80.0f, 0.0f, 0.0f}, 0.8f, 3, 0, 0},
    {0x58, {40.0f, 0.0f, 0.0f}, 1.6f, 4, 0, 0},
    {0x59, {35.0f, 0.0f, 0.0f}, 1.9f, 4, 0, 0},
    {0x5A, {30.0f, 0.0f, 0.0f}, 1.6f, 4, 0, 0},
    {0x5B, {90.0f, 0.0f, 0.0f}, 1.7f, 4, 0, 0},
    {0x5C, {-30.0f, 0.0f, 0.0f}, 1.3f, 4, 0, 0},
    {0x5D, {-20.0f, 0.0f, 0.0f}, 1.3f, 4, 0, 0},
    {0x5E, {5.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0x5F, {90.0f, 0.0f, 60.0f}, 0.8f, 0, 0, 0},
    {0x60, {90.0f, 0.0f, 60.0f}, 0.8f, 0, 0, 0},
    {0x61, {90.0f, 0.0f, 60.0f}, 0.8f, 0, 0, 0},
    {0x62, {5.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0x63, {5.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0x64, {5.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0x65, {5.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0x66, {5.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0x67, {5.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0x68, {5.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0x69, {70.0f, -30.0f, 0.0f}, 1.3f, 4, 0, 0},
    {0x6A, {50.0f, 0.0f, 0.0f}, 1.4f, 4, 0, 0},
    {0x6B, {0.0f, 0.0f, -5.0f}, 2.4f, 2, 0, 0},
    {0x6C, {30.0f, 0.0f, -5.0f}, 2.4f, 2, 0, 0},
    {0x6D, {0.0f, 0.0f, 30.0f}, 1.1f, 4, 0, 0},
    {0x6E, {15.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0x6F, {15.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0x70, {0.0f, 50.0f, -65.0f}, 2.0f, 4, 0, 0},
    {0x71, {120.0f, -40.0f, -120.0f}, 1.25f, 4, 0, 0},
    {0x72, {0.0f, 0.0f, -20.0f}, 1.8f, 4, 0, 0},
    {0x73, {-30.0f, 0.0f, 0.0f}, 1.3f, 4, 0, 0},
    {0x74, {-30.0f, -30.0f, 0.0f}, 1.2f, 4, 0, 0},
    {0x75, {-30.0f, 0.0f, 0.0f}, 1.3f, 4, 0, 0},
    {0x77, {80.0f, 0.0f, 0.0f}, 0.9f, 4, 0, 0},
    {0x78, {20.0f, 0.0f, 0.0f}, 0.8f, 4, 0, 0},
    {0x79, {30.0f, 0.0f, 0.0f}, 1.1f, 4, 0, 0},
    {0x7A, {70.0f, -30.0f, 0.0f}, 1.45f, 4, 0, 0},
    {0x7B, {90.0f, -30.0f, -60.0f}, 1.3f, 4, 0, 0},
    {0x80, {-180.0f, 30.0f, 110.0f}, 2.3f, 4, 0, 0},
    {0x81, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0x82, {60.0f, -30.0f, 0.0f}, 1.6f, 4, 0, 0},
    {0x83, {-30.0f, -30.0f, 0.0f}, 1.2f, 4, 0, 0},
    {0x84, {-30.0f, -30.0f, 0.0f}, 1.2f, 4, 0, 0},
    {0x85, {-20.0f, 0.0f, 0.0f}, 1.4f, 4, 0, 0},
    {0x86, {-20.0f, 0.0f, 0.0f}, 1.3f, 4, 0, 0},
    {0x87, {-20.0f, 0.0f, 0.0f}, 1.5f, 4, 0, 0},
    {0x88, {0.0f, 0.0f, 0.0f}, 1.6f, 4, 0, 0},
    {0x89, {40.0f, 0.0f, 0.0f}, 1.6f, 4, 0, 0},
    {0x8A, {35.0f, 0.0f, 0.0f}, 1.9f, 4, 0, 0},
    {0x8B, {60.0f, 0.0f, 20.0f}, 1.3f, 0, 0, 0},
    {0x8C, {60.0f, 0.0f, 20.0f}, 1.3f, 4, 0, 0},
    {0x8D, {-30.0f, 0.0f, 10.0f}, 1.1f, 1, 0, 0},
    {0x8E, {30.0f, 0.0f, 10.0f}, 1.0f, 4, 0, 0},
    {0x8F, {40.0f, 0.0f, 0.0f}, 1.3f, 4, 0, 0},
    {0x90, {10.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0x91, {70.0f, 0.0f, 0.0f}, 1.5f, 4, 0, 0},
    {0x92, {-30.0f, -30.0f, 0.0f}, 1.2f, 4, 0, 0},
    {0x93, {15.0f, 0.0f, 0.0f}, 2.6f, 4, 1, 1},
    {0x94, {-30.0f, 0.0f, 4.0f}, 2.4f, 2, 0, 0},
    {0x95, {0.0f, 0.0f, 0.0f}, 1.3f, 0, 0, 0},
    {0x96, {30.0f, 0.0f, 0.0f}, 1.3f, 4, 0, 0},
    {0x97, {0.0f, 0.0f, 0.0f}, 2.2f, 2, 0, 0},
    {0x98, {70.0f, 0.0f, 0.0f}, 1.4f, 4, 0, 0},
    {0x99, {0.0f, 0.0f, -5.0f}, 2.2f, 2, 0, 0},
    {0x9A, {-12.0f, 0.0f, 0.0f}, 2.2f, 4, 0, 0},
    {0x9B, {70.0f, 0.0f, 0.0f}, 1.1f, 4, 0, 0},
    {0x9C, {80.0f, 0.0f, 0.0f}, 1.2f, 4, 0, 0},
    {0x9D, {-12.0f, 0.0f, 0.0f}, 2.15f, 4, 0, 0},
    {0x9E, {-12.0f, 0.0f, 0.0f}, 2.2f, 4, 0, 0},
    {0x9F, {-12.0f, 0.0f, 0.0f}, 2.2f, 4, 0, 0},
    {0xA0, {40.0f, 0.0f, 0.0f}, 1.2f, 4, 0, 0},
    {0xA1, {80.0f, 0.0f, 0.0f}, 0.9f, 0, 0, 0},
    {0xA2, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xA3, {75.0f, 0.0f, 20.0f}, 1.4f, 4, 0, 0},
    {0xA4, {60.0f, 0.0f, 0.0f}, 1.3f, 4, 0, 0},
    {0xA5, {60.0f, 0.0f, 0.0f}, 1.3f, 4, 0, 0},
    {0xA6, {60.0f, 0.0f, 0.0f}, 1.6f, 4, 0, 0},
    {0xA7, {90.0f, 0.0f, -70.0f}, 1.3f, 4, 0, 0},
    {0xA8, {0.0f, 0.0f, 0.0f}, 1.2f, 1, 0, 0},
    {0xA9, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xAA, {-20.0f, 0.0f, 0.0f}, 1.0f, 4, 0, 0},
    {0xAB, {0.0f, 0.0f, 0.0f}, 2.2f, 2, 0, 0},
    {0xAC, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xAD, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xAE, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xAF, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xB0, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xB1, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xB2, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xB3, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xB4, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xB5, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xB6, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xB7, {0.0f, 0.0f, 0.0f}, 1.0f, 0, 0, 0},
    {0xB8, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0xB9, {100.0f, -10.0f, -130.0f}, 0.8f, 0, 0, 0},
    {0xBA, {100.0f, -10.0f, -130.0f}, 0.8f, 0, 0, 0},
    {0xBB, {100.0f, -10.0f, -130.0f}, 0.8f, 0, 0, 0},
    {0xBC, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0xBD, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0xBE, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0xBF, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0xC0, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0xC1, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0xC2, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 1, 1},
    {0xC3, {0.0f, -40.0f, -60.0f}, 1.3f, 4, 0, 0},
    {0xC4, {-180.0f, 30.0f, 110.0f}, 2.3f, 4, 0, 0},
    {0xC6, {60.0f, 0.0f, 0.0f}, 1.9f, 4, 0, 0},
    {0xC7, {10.0f, 0.0f, 30.0f}, 0.8f, 4, 0, 0},
    {0xC8, {10.0f, 0.0f, 30.0f}, 0.8f, 4, 0, 0},
    {0xC9, {10.0f, 0.0f, 30.0f}, 0.8f, 4, 0, 0},
    {0xCA, {60.0f, 0.0f, 0.0f}, 1.9f, 4, 0, 0},
    {0xCB, {60.0f, 0.0f, 0.0f}, 1.9f, 4, 0, 0},
    {0xCC, {60.0f, 0.0f, 0.0f}, 1.9f, 4, 0, 0},
    {0xCD, {60.0f, 0.0f, 0.0f}, 1.9f, 4, 0, 0},
    {0xCE, {60.0f, 0.0f, 0.0f}, 1.9f, 4, 0, 0},
    {0xCF, {60.0f, 0.0f, 0.0f}, 1.9f, 4, 0, 0},
    {0xD0, {60.0f, 0.0f, 0.0f}, 1.9f, 4, 0, 0},
    {0xD1, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 0, 0},
    {0xD2, {90.0f, 10.0f, 90.0f}, 0.8f, 4, 0, 0},
    {0xD3, {80.0f, 0.0f, 10.0f}, 0.8f, 4, 0, 0},
    {0xD4, {-80.0f, 0.0f, 170.0f}, 0.8f, 4, 0, 0},
    {0xD5, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 0, 0},
    {0xD6, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 0, 0},
    {0xD7, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 0, 0},
    {0xD8, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 0, 0},
    {0xD9, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 0, 0},
    {0xDA, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 0, 0},
    {0xDB, {0.0f, 0.0f, 0.0f}, 1.5f, 4, 0, 0},
    {0xDC, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xDD, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xDE, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xDF, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xE0, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xE1, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xE2, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xE3, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xE4, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xE5, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xE6, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xE7, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xE8, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xE9, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xEA, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xEB, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xEC, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xED, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xEE, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xEF, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xF0, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xF1, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xF2, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
    {0xF3, {0.0f, 0.0f, 0.0f}, 20.0f, 0, 0, 0},
  };
  static constexpr Info extra[] = {
    {0x59, {-5.0f, 0.0f, 0.0f}, 2.1f, 4, 0, 0},
    {0x8A, {-5.0f, 0.0f, 0.0f}, 2.1f, 4, 0, 0},
  };
  const Info *found = nullptr;
  if (inventory) for (const auto &entry : extra) if (entry.id == itemId) { found = &entry; break; }
  if (!found) for (const auto &entry : info) if (entry.id == itemId) { found = &entry; break; }
  OriginalExamProfile result;
  if (found) {
    result.rotationDegrees = found->rotation; result.scale = found->scale;
    result.light = found->light; result.inventoryAxis = found->axis; result.pickupAxis = found->pickup; result.authored = true;
  }
  return result;
}

namespace {
std::string ItemModelPath(const std::string &files,unsigned id,bool examine) {
  std::ostringstream path;
  // sce_at.cpp::itemZoom loads SS/cmn/itm%02x. Inventory examination
  // uses ss_main.cpp's IDM/CAP models. Money has no IDM at all.
  if(!examine)path<<files<<"/ss/Cmn/itm"<<std::hex<<std::setfill('0')<<std::setw(2)<<id;
  else if(id>=0xdb && id<0xf3)path<<files<<"/ss/Item/cap"<<std::setfill('0')<<std::setw(2)<<(id-0xdb);
  else path<<files<<"/ss/Item/idm"<<std::hex<<std::setfill('0')<<std::setw(3)<<id;
  return path.str();
}
std::pair<std::vector<std::uint8_t>,std::vector<std::uint8_t>> ItemModelBytes(
    const std::string &files,unsigned id,bool examine,const RoomArchive *room) {
  // An existing item's ITM model is also the original zoom model. Only
  // treasure-map pickups deliberately replace it with SS/cmn/itm95/97.
  if(!examine && room && id!=0x95 && id!=0x97)if(const auto *entry=room->find("ITM")) {
    RoomItemArchive items(room->copy(entry-room->entries().data()));
    if(items.find(id))return {items.bin(id),items.tpl(id)};
  }
  const auto path=ItemModelPath(files,id,examine);
  return {ReadFile(path+".bin"),ReadFile(path+".tpl")};
}
}
OriginalItemModel LoadOriginalItemModel(const std::string &discFiles,
                                        unsigned itemId, bool examine, const RoomArchive *room) {
  if (itemId > 255) throw std::runtime_error("Original menu: Item ID");
  const auto resources=ItemModelBytes(discFiles,itemId,examine,room);
  OriginalItemModel result;
  const auto &bin = resources.first;
  result.mesh = Model(bin);
  result.sourceFlags = AssetReader{bin}.u32(0x20);
  const auto &bytes = resources.second;
  const auto count = AssetReader{bytes}.u32(4);
  if (!count || count > 128) throw std::runtime_error("Original menu: Item texture budget");
  for (unsigned i = 0; i < count; ++i) result.textures.push_back(DecodeTpl(bytes, i));
  const auto profile = OriginalItemExamProfile(itemId, examine);
  result.rotationDegrees = profile.rotationDegrees;
  result.viewRatio = profile.scale;
  result.light = profile.light;
  return result;
}
StaticScene LoadOriginalItemScene(const std::string &discFiles, unsigned itemId, bool examine, const RoomArchive *room) {
  if (itemId > 255) throw std::runtime_error("Original menu: Item ID");
  const auto resources=ItemModelBytes(discFiles,itemId,examine,room);
  auto scene=BuildRoomModelScene(resources.first,resources.second);
  // cItemModel inherits cModel's CullMode0. The generic room decoder uses
  // two-sided diagnostic geometry; that is not the item model's state.
  for(auto &draw:scene.draws)draw.cullMode=0;
  return scene;
}
std::vector<SubscreenSpecular>
LoadOriginalItemSpecular(const std::string &discFiles, unsigned itemId, bool examine, const RoomArchive *room) {
  const auto model = LoadOriginalItemModel(discFiles, itemId, examine, room);
  std::vector<SubscreenSpecular> result;
  std::vector<std::uint8_t> palette;
  std::vector<std::pair<unsigned, Texture>> environments;
  for (const auto &primitive : model.mesh.primitives) {
    if (primitive.triangles.empty()) continue;
    const auto &header = model.mesh.materialHeaders.at(primitive.displayList);
    SubscreenSpecular spec;
    spec.bump = (header[11] & 0x80) != 0;
    spec.enabled = (header[11] & 0x13) != 0 && !spec.bump;
    spec.type = header[19];
    spec.scale = header[21] ? header[21] * .01f : .5f;
    if (model.sourceFlags & 0x20000000u) spec.scale *= .5f;
    for (unsigned k = 0; k < 3; ++k) spec.rgb[k] = header[16 + k] / 255.f;
    if (spec.enabled) {
      if (spec.type > 1) throw std::runtime_error("Original menu: Specular type");
      if (header[11] & 0x10) spec.image = model.textures.at(header[23]);
      else {
        if (palette.empty()) palette = CopySubscreenEntry(CoreBody(discFiles), 4);
        const unsigned texture = header[15] == 255 ? 0 : header[15];
        const auto found = std::find_if(environments.begin(), environments.end(),
            [texture](const auto &entry) { return entry.first == texture; });
        if (found == environments.end()) {
          environments.push_back({texture, DecodeTpl(palette, texture)});
          spec.image = environments.back().second;
        } else spec.image = found->second;
      }
    }
    result.push_back(std::move(spec));
  }
  return result;
}
namespace {
StaticScene CaseModel(const std::string &files, unsigned id) {
  if (!OriginalItemFootprint(id).width)
    throw std::runtime_error("Original menu: not a case item");
  // Exact pieceTblInit aliases in Sscrn/ss_pzzl.cpp. Case pieces use their
  // dedicated flat 3-D models, not the item-examine BIN orientation.
  unsigned model = id, texture = id;
  if (id == 0x40) model = texture = 0x21;
  else if (id == 0x17 || id == 0x35 || id == 0x6d) model = 0x35;
  else if ((id >= 0x12 && id <= 0x16) || id == 0xa8) model = 0x12;
  else if (id == 1 || id == 2 || id == 0xe) model = 1;
  else if (id >= 8 && id <= 0xa) model = 8;
  else if (id == 6 || id == 0x19 || id == 0x1c) model = 6;
  else if (id == 0 || id == 4 || id == 7 || id == 0x18 || id == 0x1a ||
           id == 0x20 || id == 0x6a || id == 0xa0) model = 0;
  if (id == 0x95 || id == 0x97) texture = 0x95;
  if (id == 0x6d) texture = 0x35;
  const auto archive = ReadFile(files + "/ss/Eng/ss_pzzl.dat");
  return BuildRoomModelScene(CopySubscreenEntry(archive, model * 2 + 4),
                             CopySubscreenEntry(archive, texture * 2 + 5));
}
std::array<std::uint8_t, 4> Pixel(const Texture &image, float u, float v, bool repeat) {
  if (repeat) { u -= std::floor(u); v -= std::floor(v); }
  else { u = std::clamp(u, 0.f, 1.f); v = std::clamp(v, 0.f, 1.f); }
  const auto x = std::min(image.width - 1, unsigned(u * image.width));
  const auto y = std::min(image.height - 1, unsigned(v * image.height));
  const auto p = (std::size_t(y) * image.width + x) * 4;
  return {image.rgba[p], image.rgba[p+1], image.rgba[p+2], image.rgba[p+3]};
}
std::array<std::uint8_t,4> LinearPixel(const Texture &image,float x,float y) {
  const int ix=int(std::floor(x)),iy=int(std::floor(y));const float fx=x-ix,fy=y-iy;
  std::array<std::uint8_t,4> result{};
  for(unsigned k=0;k<4;++k) {
    float value=0;
    for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx) {
      const auto px=std::clamp(ix+dx,0,int(image.width)-1),py=std::clamp(iy+dy,0,int(image.height)-1);
      value+=image.rgba[(std::size_t(py)*image.width+px)*4+k]*(dx?fx:1-fx)*(dy?fy:1-fy);
    }
    result[k]=std::uint8_t(std::clamp(std::lround(value),0l,255l));
  }
  return result;
}
void BlendPixel(Texture &image, unsigned x, unsigned y,
                 const std::array<std::uint8_t, 4> &color) {
  if (x >= image.width || y >= image.height || !color[3]) return;
  const auto p = (std::size_t(y) * image.width + x) * 4;
  const unsigned a = color[3], inverse = 255 - a;
  const unsigned oldA = image.rgba[p+3];
  const unsigned resultA = a + (oldA * inverse + 127) / 255;
  for (unsigned k = 0; k < 3; ++k)
    image.rgba[p+k] = static_cast<std::uint8_t>((unsigned(color[k]) * a +
      (unsigned(image.rgba[p+k]) * oldA * inverse + 127) / 255 + resultA/2) / resultA);
  image.rgba[p+3] = static_cast<std::uint8_t>(resultA);
}
float Edge(float ax, float ay, float bx, float by, float x, float y) {
  return (x-ax)*(by-ay) - (y-ay)*(bx-ax);
}
}
Texture LoadOriginalItemIcon(const std::string &discFiles, unsigned itemId) {
  const auto scene = CaseModel(discFiles, itemId);
  const auto footprint = OriginalItemFootprint(itemId);
  Texture result;
  // Constant 48 pixels per case cell preserves the original piece aspect.
  result.width = footprint.width * 48; result.height = footprint.height * 48;
  result.rgba.resize(std::size_t(result.width) * result.height * 4);
  std::vector<float> depth(std::size_t(result.width) * result.height,
                            -std::numeric_limits<float>::infinity());
  std::array<float, 2> low{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()}, high{-low[0],-low[1]};
  for (std::size_t p=0; p<scene.vertices.size(); p+=3)
    for (unsigned k=0;k<2;++k) { low[k]=std::min(low[k],scene.vertices[p+k]); high[k]=std::max(high[k],scene.vertices[p+k]); }
  const auto dx=high[0]-low[0],dy=high[1]-low[1];
  if (dx<=0 || dy<=0) throw std::runtime_error("Original menu: flat part without a view");
  const auto scale=std::min((result.width-4.f)/dx,(result.height-4.f)/dy);
  const auto cx=(low[0]+high[0])*.5f,cy=(low[1]+high[1])*.5f;
  // Source case camera faces positive Z with Y up; piece orientation zero is
  // the canonical flat-front model pose. This CPU thumbnail uses original
  // diffuse textures/vertex colours, without recreating GX specular passes.
  for (const auto &draw : scene.draws) {
    const auto &texture=scene.textures.at(draw.texture);
    for(std::size_t first=draw.first;first<draw.first+draw.count;first+=3) {
      std::array<float,3> x{},y{},z{},u{},v{};
      for(unsigned k=0;k<3;++k) {
        const auto p=first+k;
        x[k]=result.width*.5f+(scene.vertices[p*3]-cx)*scale;
        y[k]=result.height*.5f-(scene.vertices[p*3+1]-cy)*scale;
        z[k]=scene.vertices[p*3+2];u[k]=scene.uv[p*2];v[k]=scene.uv[p*2+1];
      }
      const auto area=Edge(x[0],y[0],x[1],y[1],x[2],y[2]);
      if(std::abs(area)<1e-6f)continue;
      const int left=std::max(0,int(std::floor(*std::min_element(x.begin(),x.end()))));
      const int right=std::min(int(result.width)-1,int(std::ceil(*std::max_element(x.begin(),x.end()))));
      const int top=std::max(0,int(std::floor(*std::min_element(y.begin(),y.end()))));
      const int bottom=std::min(int(result.height)-1,int(std::ceil(*std::max_element(y.begin(),y.end()))));
      for(int py=top;py<=bottom;++py)for(int px=left;px<=right;++px) {
        const float a=Edge(x[1],y[1],x[2],y[2],px+.5f,py+.5f)/area;
        const float b=Edge(x[2],y[2],x[0],y[0],px+.5f,py+.5f)/area;
        const float c=1-a-b;
        if(a<-.00001f||b<-.00001f||c<-.00001f)continue;
        const auto p=std::size_t(py)*result.width+px;
        const auto zp=a*z[0]+b*z[1]+c*z[2];
        if(zp<depth[p])continue;
        const auto tu=a*u[0]+b*u[1]+c*u[2],tv=a*v[0]+b*v[1]+c*v[2];
        auto color=Pixel(texture,tu,tv,draw.repeat);
        // Same original TEV alpha contract as the live item/portrait pass:
        // base RGB carries no coverage; the authored alphaImage supplies it.
        color[3]=draw.alphaTest && draw.alphaTexture>=0?Pixel(scene.textures.at(draw.alphaTexture),tu,tv,draw.repeat)[3]:255;
        if(draw.alphaTest && color[3]<=draw.alphaReference)continue;
        if(draw.vertexColors)for(unsigned k=0;k<4;++k)
          color[k]=static_cast<std::uint8_t>(color[k]*(a*scene.colors[first*4+k]+b*scene.colors[(first+1)*4+k]+c*scene.colors[(first+2)*4+k])/255.f);
        for(unsigned k=0;k<3;++k)color[k]=static_cast<std::uint8_t>(unsigned(color[k])*draw.materialRGB[k]/255);
        if(!color[3])continue;
        depth[p]=zp;BlendPixel(result,unsigned(px),unsigned(py),color);
      }
    }
  }
  return result;
}

Texture LoadOriginalFilePage(const std::string &discFiles, unsigned fileItemId,
                             unsigned page) {
  const auto number=OriginalFileNumber(fileItemId);
  if(!number)throw std::runtime_error("Original menu: not a document");
  const auto picture=LoadOriginalFilePicture(discFiles,number,page);
  const auto font=LoadOriginalMenuFont(discFiles);
  const auto glyphs=LoadOriginalFileGlyphs(discFiles,number,page);
  const auto &sheet=font.sheets.at(0);
  Texture result;result.width=640;result.height=480;
  result.rgba.resize(640*480*4);
  for(unsigned y=0;y<336;++y)for(unsigned x=0;x<640;++x)
    BlendPixel(result,x,y+56,Pixel(picture,x/640.f,y/336.f,false));
  const auto archive=ReadFile(discFiles+"/ss/Eng/ss_file.dat");
  const auto layout=DecodeSubscreenLayout(CopySubscreenEntry(archive,8));
  float origin=48, originY=40;
  for(const auto &q:layout)if(q.mark==0xfe){origin=(q.position[0]+320.f)*.8f;originY=(240.f-q.position[1])*.8f;}
  const bool centred=number==1||number==3||number==9;
  std::array<int,32> lineWidths{};
  unsigned lines=1;
  auto advance=[&](unsigned glyph) {
    if(!glyph)return 5; // getSpaceWidth(8) * source layout scale_w(20/32).
    const auto b=font.bearings.at(glyph);
    return int((b[1]-b[0])*(20.f/32.f))-1;
  };
  for(const auto &g:glyphs){if(g.line>=32)throw std::runtime_error("Original menu: Document row budget");lineWidths[g.line]+=advance(g.glyph);lines=std::max(lines,g.line+1);}
  std::array<int,32> pens{};
  for(unsigned i=0;i<lines;++i)pens[i]=centred?(512-lineWidths[i])/2:int(origin);
  const float startY=centred?originY:float((384-int(lines)*21)/2);
  static constexpr std::uint32_t colors[10]={0xe5d9cff0,0x87cfa5ff,0xcd7d5fff,0x87afffff,0xa55fffff,0x707070ff,0x707070ff,0x52df73ff,0,0};
  const auto columns=sheet.width/font.cellWidth;
  for(const auto &g:glyphs) {
    const auto width=advance(g.glyph);
    if(g.glyph) {
      const auto bearing=font.bearings.at(g.glyph);
      const int sx=int((g.glyph%columns)*font.cellWidth)+int(bearing[0]);
      const unsigned sy=((g.glyph/columns)*font.cellHeight)%sheet.height;
      const int sourceWidth=bearing[1]-bearing[0];
      const int targetWidth=std::max(0,int((sourceWidth*(20.f/32.f))*1.25f));
      const int left=int(pens[g.line]*1.25f),top=int((startY+g.line*21)*1.25f);
      const auto tint=colors[g.color];
      for(int y=0;y<31;++y)for(int x=0;x<targetWidth;++x) {
        const int tx=int(sx)+x*sourceWidth/std::max(1,targetWidth),ty=int(sy)+y*int(font.cellHeight)/31;
        if(tx<0||ty<0||tx>=int(sheet.width)||ty>=int(sheet.height)||left+x<0||top+y<0)continue;
        const auto p=(std::size_t(ty)*sheet.width+tx)*4;
        std::array<std::uint8_t,4> color{};
        for(unsigned k=0;k<4;++k)color[k]=static_cast<std::uint8_t>(unsigned(sheet.rgba[p+k])*((tint>>((3-k)*8))&255)/255);
        BlendPixel(result,unsigned(left+x),unsigned(top+y),color);
      }
    }
    pens[g.line]+=width;
  }
  return result;
}
Texture LoadOriginalItemName(const std::string &discFiles, unsigned itemId) {
  if (itemId > 255) throw std::runtime_error("Original menu: Item ID");
  const auto glyphs = DecodeSubscreenGlyphs(ReadSubscreenMessage(
      CopySubscreenEntry(CoreBody(discFiles), 0x15), itemId));
  if (glyphs.empty()) throw std::runtime_error("Original menu: Item name is missing");
  const auto font = LoadOriginalMenuFont(discFiles);
  const auto &sheet = font.sheets.at(0);
  std::array<unsigned, 4> lineWidths{}, pens{};
  unsigned lines = 1;
  auto advance = [&](unsigned glyph) -> unsigned {
    if (!glyph) return 8; // MessageData::getSpaceWidth, English.
    const auto &b = font.bearings.at(glyph);
    return unsigned(std::max(0, int(b[1]) - int(b[0]) - 1));
  };
  for (const auto &g : glyphs) {
    if (g.line >= lineWidths.size()) throw std::runtime_error("Original menu: Name row budget");
    lineWidths[g.line] += advance(g.glyph);
    if (lineWidths[g.line] > 2048) throw std::runtime_error("Original menu: Name width budget");
    lines = std::max(lines, g.line + 1);
  }
  Texture result;
  result.width = std::max(1u, *std::max_element(lineWidths.begin(), lineWidths.end()) + 1);
  result.height = lines * 32;
  result.rgba.resize(std::size_t(result.width) * result.height * 4);
  const auto columns = sheet.width / font.cellWidth;
  static constexpr std::uint32_t colors[10] = {
      0xe5d9cff0,0x87cfa5ff,0xcd7d5fff,0x87afffff,0xa55fffff,
      0x707070ff,0x707070ff,0x52df73ff,0,0};
  for (const auto &g : glyphs) {
    if (g.glyph) {
      const auto &bearing = font.bearings.at(g.glyph);
      const int left = int((g.glyph % columns) * font.cellWidth) + bearing[0];
      const auto top = ((g.glyph / columns) * font.cellHeight) % sheet.height;
      const auto width = std::max(0, int(bearing[1]) - int(bearing[0]));
      for (unsigned y = 0; y < 32; ++y) for (int x = 0; x < width; ++x) {
        const auto sx = left + x;
        if (sx < 0 || sx >= int(sheet.width) || top + y >= sheet.height) continue;
        const auto p = (std::size_t(top + y) * sheet.width + unsigned(sx)) * 4;
        std::array<std::uint8_t,4> pixel{};
        for (unsigned k = 0; k < 4; ++k)
          pixel[k] = std::uint8_t(unsigned(sheet.rgba[p + k]) *
              ((colors[g.color] >> ((3 - k) * 8)) & 255) / 255);
        BlendPixel(result, pens[g.line] + unsigned(x), g.line * 32 + y, pixel);
      }
    }
    pens[g.line] += advance(g.glyph);
  }
  return result;
}
Texture LoadOriginalPickupMessage(const std::string &files,unsigned id,unsigned message,unsigned count,unsigned selection,bool caseMessage,std::vector<std::array<int,4>> *choiceBounds) {
  if(id>255 || ((!caseMessage && (message<0x11 || message>0x15)) || (caseMessage && message!=0)) || count>99999999 || selection>2)
    throw std::runtime_error("ItemGet: Message parameters");
  const auto core=CoreBody(files),table=caseMessage?CopySubscreenEntry(ReadFile(files+"/ss/Eng/ss_cmmn.dat"),5):CopySubscreenEntry(core,0x0a),names=CopySubscreenEntry(core,0x15);
  const auto font=LoadOriginalMenuFont(files);
  struct Glyph {unsigned code,line,color;bool choice;unsigned choiceNo;};
  std::vector<Glyph> glyphs;unsigned line=0,color=0,choice=0;
  const auto name=ReadSubscreenMessage(names,id);
  const auto words=ReadSubscreenMessage(table,message);
  auto expand=[&](const std::vector<std::uint16_t> &text,bool nested) {
    for(std::size_t i=0;i<text.size();++i) {
      const auto code=text[i];
      if(code>=0x80) {glyphs.push_back({unsigned(code-0x80),line,color,false,0});continue;}
      if(code==1 || code==0x0e)return;
      if(code==0)continue;
      if(code==3){++line;continue;}
      if(code==6){color=text.at(++i);continue;}
      if(code==7){glyphs.push_back({0,line,color,true,choice++});continue;}
      if(code==8)continue;
      if(code==0x10 && !nested) {
        const auto inserted=DecodeSubscreenGlyphs(name);
        for(const auto &g:inserted)glyphs.push_back({g.glyph,line+g.line,color,false,0});
        continue;
      }
      if(code==0x0a) {
        for(const auto digit:std::to_string(count))glyphs.push_back({unsigned(3+digit-'0'),line,color,false,0});
        continue;
      }
      throw std::runtime_error("ItemGet: Unsupported original opcode");
    }
    throw std::runtime_error("ItemGet: Message end is missing");
  };
  expand(words,false);
  // mes.cpp::setLayout(LAYOUT_SUBSCRN), English: 18x24, gap -1, line 19.
  const int glyphWidth=caseMessage?18:22,glyphHeight=caseMessage?24:32;
  const int originY=caseMessage?160:240,lineGap=caseMessage?19:25;
  const auto advance=[&](const Glyph &g) {
    // gameInit2 installs layout0 (22x32, gap-1), independently of MesSet's
    // font_no4. getSize(0) uses bearing1, and putSelCursol changes it to1.
    const auto &bearing=font.bearings.at(g.code?g.code:1);
    return int((int(bearing[1])-int(bearing[0]))*float(glyphWidth)/32)-1;
  };
  std::array<int,16> widths{},pens{};
  for(const auto &g:glyphs){if(g.line>=16 || g.color>=10)throw std::runtime_error("ItemGet: Glyph budget");widths[g.line]+=advance(g);}
  const auto maximum=*std::max_element(widths.begin(),widths.end());
  if(maximum>512)throw std::runtime_error("ItemGet: Original line too wide");
  pens.fill((512-maximum)/2);
  Texture result;result.width=512;result.height=384;result.rgba.resize(512*384*4);
  constexpr std::uint32_t colors[10]={0xe5d9cff0,0x87cfa5ff,0xcd7d5fff,0x87afffff,0xa55fffff,0x707070ff,0x707070ff,0x52df73ff,0,0};
  const auto &sheet=font.sheets.at(0);const auto columns=sheet.width/font.cellWidth;
  if(choiceBounds)choiceBounds->assign(choice,{0,0,0,0});
  int activeChoice=-1;unsigned choiceLine=~0u;
  for(const auto &g:glyphs) {
    const auto x=pens[g.line],y=originY+int(g.line)*lineGap;
    if(g.choice){activeChoice=int(g.choiceNo);choiceLine=g.line;if(choiceBounds)(*choiceBounds)[g.choiceNo]={x,y,x,y+glyphHeight};}
    if(choiceBounds && activeChoice>=0 && g.line==choiceLine)(*choiceBounds)[unsigned(activeChoice)][2]=x+advance(g);
    // Unselected choice slots draw the blank glyph0; selected slots draw1.
    if(g.code || (g.choice && g.choiceNo==selection)) {
      const auto code=g.choice?1:g.code;
      const auto &bearing=font.bearings.at(code);
      const int sourceX=int(code%columns*font.cellWidth)+bearing[0],sourceY=int(code/columns*font.cellHeight);
      const int width=std::max(0,int(bearing[1])-int(bearing[0]));
      const int outWidth=std::max(1,int(width*float(glyphWidth)/32));
      for(int py=0;py<glyphHeight;++py)for(int px=0;px<outWidth;++px) {
        // MessageFont::create/setAttribute uses GX_LINEAR. Sample at the
        // fragment centre after the source 32->22 glyph transform; nearest
        // integer sampling thickened some question glyphs and their outline.
        const float sx=sourceX+(px+.5f)*float(width)/outWidth-.5f,sy=float(sourceY)+(py+.5f)*32/glyphHeight-.5f;
        auto pixel=LinearPixel(sheet,sx,sy);
        for(unsigned k=0;k<4;++k)pixel[k]=std::uint8_t(unsigned(pixel[k])*((colors[g.color]>>((3-k)*8))&255)/255);
        if(x+px>=0 && x+px<512 && y+py<384)BlendPixel(result,unsigned(x+px),unsigned(y+py),pixel);
      }
    }
    pens[g.line]+=advance(g);
  }
  return result;
}
std::vector<SmdModel>
ReadOriginalVillageMapRoom(const std::vector<std::uint8_t> &archive, unsigned room) {
  static constexpr unsigned rooms[22] = {0x100,0x101,0x102,0x103,0x104,0x105,0x106,0x107,0x108,0x109,0x10a,0x10b,0x10c,0x10d,0x10e,0x10f,0x117,0x11c,0x11d,0x11e,0x11f,0x200};
  const auto it = std::find(std::begin(rooms), std::end(rooms), room);
  if (it == std::end(rooms)) throw std::runtime_error("Original menu: Village map room is missing");
  const auto bytes = CopySubscreenEntry(archive, 0xb + unsigned(it - std::begin(rooms)));
  AssetReader r{bytes};
  const auto count = r.u32(0);
  if (count < 3 || count > 64) throw std::runtime_error("Original menu: Map model budget");
  const auto models = count - 2;
  std::vector<SmdModel> result;
  for (unsigned i = 0; i < models; ++i) result.push_back(Model(CopySubscreenEntry(bytes, 4 + i)));
  return result;
}
namespace {
struct MapRoomSource { unsigned room, slot; const char *file; };
// Exact mapDataInit_St* lists; boundary rooms intentionally occur twice.
constexpr MapRoomSource MapRooms[] = {
    {0x100, 0x0B, "map_obj1.dat"},
    {0x101, 0x0C, "map_obj1.dat"},
    {0x102, 0x0D, "map_obj1.dat"},
    {0x103, 0x0E, "map_obj1.dat"},
    {0x104, 0x0F, "map_obj1.dat"},
    {0x105, 0x10, "map_obj1.dat"},
    {0x106, 0x11, "map_obj1.dat"},
    {0x107, 0x12, "map_obj1.dat"},
    {0x108, 0x13, "map_obj1.dat"},
    {0x109, 0x14, "map_obj1.dat"},
    {0x10A, 0x15, "map_obj1.dat"},
    {0x10B, 0x16, "map_obj1.dat"},
    {0x10C, 0x17, "map_obj1.dat"},
    {0x10D, 0x18, "map_obj1.dat"},
    {0x10E, 0x19, "map_obj1.dat"},
    {0x10F, 0x1A, "map_obj1.dat"},
    {0x117, 0x1B, "map_obj1.dat"},
    {0x11C, 0x1C, "map_obj1.dat"},
    {0x11D, 0x1D, "map_obj1.dat"},
    {0x11E, 0x1E, "map_obj1.dat"},
    {0x11F, 0x1F, "map_obj1.dat"},
    {0x200, 0x20, "map_obj1.dat"},
    {0x200, 0x0B, "map_obj2a.dat"},
    {0x201, 0x0C, "map_obj2a.dat"},
    {0x202, 0x0D, "map_obj2a.dat"},
    {0x203, 0x0E, "map_obj2a.dat"},
    {0x204, 0x0F, "map_obj2a.dat"},
    {0x205, 0x10, "map_obj2a.dat"},
    {0x206, 0x11, "map_obj2a.dat"},
    {0x207, 0x12, "map_obj2a.dat"},
    {0x208, 0x13, "map_obj2a.dat"},
    {0x209, 0x14, "map_obj2a.dat"},
    {0x20A, 0x15, "map_obj2a.dat"},
    {0x20B, 0x16, "map_obj2a.dat"},
    {0x20C, 0x17, "map_obj2a.dat"},
    {0x20D, 0x18, "map_obj2a.dat"},
    {0x20E, 0x19, "map_obj2a.dat"},
    {0x20F, 0x1A, "map_obj2a.dat"},
    {0x210, 0x1B, "map_obj2a.dat"},
    {0x211, 0x1C, "map_obj2a.dat"},
    {0x212, 0x1D, "map_obj2a.dat"},
    {0x213, 0x1E, "map_obj2a.dat"},
    {0x214, 0x1F, "map_obj2a.dat"},
    {0x215, 0x20, "map_obj2a.dat"},
    {0x216, 0x21, "map_obj2a.dat"},
    {0x217, 0x22, "map_obj2a.dat"},
    {0x218, 0x23, "map_obj2a.dat"},
    {0x219, 0x24, "map_obj2a.dat"},
    {0x222, 0x25, "map_obj2a.dat"},
    {0x21D, 0x0B, "map_obj2b.dat"},
    {0x220, 0x0C, "map_obj2b.dat"},
    {0x221, 0x0D, "map_obj2b.dat"},
    {0x223, 0x0E, "map_obj2b.dat"},
    {0x224, 0x0F, "map_obj2b.dat"},
    {0x225, 0x10, "map_obj2b.dat"},
    {0x226, 0x11, "map_obj2b.dat"},
    {0x227, 0x12, "map_obj2b.dat"},
    {0x228, 0x13, "map_obj2b.dat"},
    {0x229, 0x14, "map_obj2b.dat"},
    {0x22A, 0x15, "map_obj2b.dat"},
    {0x21A, 0x0B, "map_obj2c.dat"},
    {0x21B, 0x0C, "map_obj2c.dat"},
    {0x300, 0x0B, "map_obj3a.dat"},
    {0x301, 0x0B, "map_obj3b.dat"},
    {0x303, 0x0C, "map_obj3b.dat"},
    {0x304, 0x0D, "map_obj3b.dat"},
    {0x305, 0x0E, "map_obj3b.dat"},
    {0x306, 0x0F, "map_obj3b.dat"},
    {0x306, 0x0B, "map_obj3c.dat"},
    {0x307, 0x0C, "map_obj3c.dat"},
    {0x308, 0x0D, "map_obj3c.dat"},
    {0x309, 0x0E, "map_obj3c.dat"},
    {0x30A, 0x0F, "map_obj3c.dat"},
    {0x30B, 0x10, "map_obj3c.dat"},
    {0x30C, 0x11, "map_obj3c.dat"},
    {0x30E, 0x12, "map_obj3c.dat"},
    {0x310, 0x0B, "map_obj3d.dat"},
    {0x311, 0x0C, "map_obj3d.dat"},
    {0x30D, 0x0E, "map_obj3d.dat"},
    {0x30F, 0x0F, "map_obj3d.dat"},
    {0x312, 0x0D, "map_obj3d.dat"},
    {0x315, 0x0B, "map_obj3e.dat"},
    {0x316, 0x0C, "map_obj3e.dat"},
    {0x317, 0x0D, "map_obj3e.dat"},
    {0x318, 0x0E, "map_obj3e.dat"},
    {0x31A, 0x0F, "map_obj3e.dat"},
    {0x31B, 0x10, "map_obj3e.dat"},
    {0x31D, 0x11, "map_obj3e.dat"},
    {0x31C, 0x12, "map_obj3e.dat"},
    {0x320, 0x13, "map_obj3e.dat"},
    {0x321, 0x14, "map_obj3e.dat"},
    {0x325, 0x15, "map_obj3e.dat"},
    {0x326, 0x16, "map_obj3e.dat"},
    {0x327, 0x17, "map_obj3e.dat"},
    {0x329, 0x0B, "map_obj3f.dat"},
    {0x330, 0x0C, "map_obj3f.dat"},
    {0x331, 0x0D, "map_obj3f.dat"},
    {0x332, 0x0E, "map_obj3f.dat"},
    {0x333, 0x0F, "map_obj3f.dat"},
    {0x400, 0x0B, "map_obj4a.dat"},
    {0x402, 0x0B, "map_obj4b.dat"},
    {0x403, 0x0B, "map_obj4c.dat"},
    {0x404, 0x0B, "map_obj4d.dat"},
    {0x405, 0x0B, "map_obj4e.dat"},
    {0x406, 0x0B, "map_obj4f.dat"},
    {0x40A, 0x0B, "map_obj4g.dat"},
    {0x40B, 0x0C, "map_obj4g.dat"},
    {0x40C, 0x0D, "map_obj4g.dat"},
    {0x40D, 0x0E, "map_obj4g.dat"},
    {0x40E, 0x0F, "map_obj4g.dat"},
    {0x40F, 0x10, "map_obj4g.dat"},
    {0x410, 0x11, "map_obj4g.dat"},
    {0x411, 0x12, "map_obj4g.dat"},
};
constexpr const char *MapAreaFiles[] = {
    "map_obj1.dat", "map_obj1.dat", "map_obj2a.dat", "map_obj2b.dat",
    "map_obj2c.dat", "map_obj3a.dat", "map_obj3b.dat", "map_obj3c.dat",
    "map_obj3d.dat", "map_obj3e.dat", "map_obj3f.dat", "map_obj4a.dat",
    "map_obj4b.dat", "map_obj4c.dat", "map_obj4d.dat", "map_obj4e.dat",
    "map_obj4f.dat", "map_obj4g.dat"};
unsigned MapArea(unsigned room, unsigned stage) {
  // getAreaNo uses campaign stage, not just the room number. In particular,
  // r200 is present at the edge of the village map while stage 1 is active.
  if (stage <= 1) return stage;
  if (stage == 2) {
    if ((room >= 0x200 && room <= 0x219) || room == 0x222) return 2;
    if (room == 0x21d || (room >= 0x220 && room <= 0x221) ||
        (room >= 0x223 && room <= 0x22a)) return 3;
    if (room >= 0x21a && room <= 0x21b) return 4;
  }
  if (stage == 3) {
    if (room == 0x300) return 5;
    if (room == 0x301 || (room >= 0x303 && room <= 0x306)) return 6;
    if ((room >= 0x307 && room <= 0x30c) || room == 0x30e) return 7;
    if (room == 0x30d || (room >= 0x30f && room <= 0x312)) return 8;
    if ((room >= 0x315 && room <= 0x318) ||
        (room >= 0x31a && room <= 0x31d) ||
        (room >= 0x320 && room <= 0x321) ||
        (room >= 0x325 && room <= 0x327)) return 9;
    if (room == 0x329 || (room >= 0x330 && room <= 0x333)) return 10;
  }
  if (stage == 4) {
    if (room == 0x400) return 11;
    if (room >= 0x402 && room <= 0x406) return 12 + room - 0x402;
    return 17;
  }
  return 0;
}
const MapRoomSource *MapSource(unsigned room, unsigned area) {
  for (const auto &source : MapRooms)
    if (source.room == room && source.file == std::string(MapAreaFiles[area]))
      return &source;
  return nullptr;
}
struct MapGeometry {
  const MapRoomSource *source;
  unsigned models;
  struct Triangle {
    std::array<std::array<float,3>,3> points{};
    std::array<std::array<float,2>,3> uv{};
    unsigned texture{},color{};
    std::array<float,4> rgba{};
  };
  std::vector<Triangle> triangles;
  std::size_t firstPath{}, pathCount{};
  std::array<double, 4> bounds{
      std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::infinity(),
      -std::numeric_limits<double>::infinity(),
      -std::numeric_limits<double>::infinity()};
};
bool Coordinates(const std::array<double,3> &point,
                   const std::array<float,3> &a,
                   const std::array<float,3> &b,
                   const std::array<float,3> &c, double &s, double &t) {
  const double ux=b[0]-a[0],uz=b[2]-a[2],vx=c[0]-a[0],vz=c[2]-a[2];
  const auto determinant=ux*vz-uz*vx;
  if(std::abs(determinant)<1e-12)return false;
  const double x=point[0]-a[0],z=point[2]-a[2];
  s=(x*vz-z*vx)/determinant;t=(ux*z-uz*x)/determinant;
  return std::isfinite(s)&&std::isfinite(t);
}
}
std::string OriginalMapSnapshotJson(const std::string &files, unsigned room,
                                    const std::array<double,3> &player, float heading, int stage,
                                    const ScenarioState *scenario,
                                    const std::map<std::uint16_t,std::uint8_t> *passed,
                                    const std::string &language) {
  for(const auto value:player)if(!std::isfinite(value))throw std::runtime_error("Original menu: Map position");
  if(!std::isfinite(heading))throw std::runtime_error("Original menu: Map view direction");
  if (stage < -1 || stage > 4)
    throw std::runtime_error("Original menu: Map stage is missing");
  const bool progressKnown=scenario && passed;
  if(stage<0 && scenario)stage=(room>>8)==4?4:scenario->scenario.test(47)?3:
      scenario->scenario.test(40)?2:scenario->scenario.test(29)?1:0;
  const auto area = MapArea(room, stage < 0 ? room >> 8 : unsigned(stage));
  const auto *source=MapSource(room, area);
  if(!source)throw std::runtime_error("Original menu: Map room is missing");
  const auto archive=ReadFile(files+"/ss/Cmn/"+source->file);
  const auto bytes=CopySubscreenEntry(archive,source->slot);
  const auto count=AssetReader{bytes}.u32(0);
  if(count<3||count>64)throw std::runtime_error("Original menu: Map room budget");
  const Sat mini(CopySubscreenEntry(bytes,count+2));
  const Sat world(CopySubscreenEntry(bytes,count+3));
  // mapPositionCheck applies the room-world SAT triangle's barycentrics to
  // the paired mini-map SAT vertices. The source deliberately chooses the
  // lowest intersected polygon; multilevel models constrain +/-1500 units.
  const bool multi=count>3;
  double best=std::numeric_limits<double>::infinity(),s=0,t=0;
  const SatPolygon *selected=nullptr;
  for(std::size_t i=0;i<world.floor+world.slope;++i) {
    const auto &p=world.polygons.at(i);
    double a=0,b=0;
    if(!Coordinates(player,world.vertices.at(p.vertices[0]),world.vertices.at(p.vertices[1]),world.vertices.at(p.vertices[2]),a,b)||a<-.00001||b<-.00001||a+b>1.00001)continue;
    const auto y=world.vertices[p.vertices[0]][1]+a*(world.vertices[p.vertices[1]][1]-world.vertices[p.vertices[0]][1])+b*(world.vertices[p.vertices[2]][1]-world.vertices[p.vertices[0]][1]);
    const auto limit=multi?1500.:100000.;
    if(std::abs(y-player[1])>limit||y>best)continue;
    best=y;s=a;t=b;selected=&p;
  }
  std::array<double,3> marker{};
  double markerHeight=0;
  bool valid=false;
  if(selected) {
    const auto &p=*selected;
    for(const auto index:p.vertices)if(index>=mini.vertices.size())throw std::runtime_error("Original menu: Map pair mismatch");
    const auto &a=mini.vertices.at(p.vertices[0]),&b=mini.vertices.at(p.vertices[1]),&c=mini.vertices.at(p.vertices[2]);
    marker={a[0]+s*(b[0]-a[0])+t*(c[0]-a[0]),a[2]+s*(b[2]-a[2])+t*(c[2]-a[2]),0};
    markerHeight=a[1]+s*(b[1]-a[1])+t*(c[1]-a[1]);
    auto forward=player;forward[0]+=std::sin(heading)*1000;forward[2]+=std::cos(heading)*1000;
    double fs=0,ft=0;
    if(Coordinates(forward,world.vertices.at(p.vertices[0]),world.vertices.at(p.vertices[1]),world.vertices.at(p.vertices[2]),fs,ft)) {
      const auto x=(fs-s)*(b[0]-a[0])+(ft-t)*(c[0]-a[0]);
      const auto z=(fs-s)*(b[2]-a[2])+(ft-t)*(c[2]-a[2]);
      marker[2]=std::atan2(x,z);valid=true;
    }
  }
  // The R10e map model is scaled by 10 in mapModelInit; its mini-map marker
  // receives the same parts matrix. Other map model root matrices are identity.
  const double scale=room==0x10e?10.:1.;
  marker[0]*=scale;marker[1]*=scale;
  markerHeight*=scale;
  const auto mapData=ReadFile(files+"/ss/"+DiscLanguage(language)+"/ss_map.dat");
  const auto tpl=CopySubscreenEntry(mapData,0xe);
  const auto colorTable=CopySubscreenEntry(mapData,5);
  const auto colorUnits=DecodeSubscreenLayout(colorTable);
  std::array<std::array<float,4>,11> colors{};
  for(unsigned i=0;i<colors.size();++i) {
    const auto unit=std::find_if(colorUnits.begin(),colorUnits.end(),[&](const auto &q){return q.mark==i;});
    if(unit==colorUnits.end())throw std::runtime_error("Original menu: mapColor ID is missing");
    const unsigned stride=colorTable[0]=='2'?0x8c:0x88;
    const auto position=8+std::size_t(unit-colorUnits.begin())*stride;
    for(unsigned component=0;component<4;++component)colors[i][component]=colorTable.at(position+0x58+component);
  }
  const auto colorIndex=[&](unsigned id) {
    if(id==room)return 0u;
    if(!progressKnown)return 2u;
    // mapColor's stage1 table; SCF_NULL is an ordinary source flag bit.
    const unsigned hide=area<=1?29:area<=4?40:area<=10?47:29;
    unsigned clear=0;
    if(area<=1)clear=(id==0x100 || (id>=0x104 && id<=0x107))?28:
        (id>=0x109 && id<=0x10e)?11:0;
    const auto visited=passed->find(std::uint16_t(id));
    const bool seen=visited!=passed->end() && (visited->second&0x80);
    if(scenario->scenario.test(hide))return 4u;
    if(!seen && !scenario->scenario.test(0))return 4u;
    if(scenario->scenario.test(clear))return 3u;
    return seen?1u:2u;
  };
  std::vector<MapGeometry> geometry;
  std::size_t pathCount = 0;
  std::array<double, 4> bounds{std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::infinity(),
      -std::numeric_limits<double>::infinity(),
      -std::numeric_limits<double>::infinity()};
  for (const auto &entry : MapRooms) {
    if (std::string(entry.file) != source->file) continue;
    const auto data = CopySubscreenEntry(archive, entry.slot);
    const auto entries = AssetReader{data}.u32(0);
    if (entries < 3 || entries > 64)
      throw std::runtime_error("Original menu: Map room budget");
    MapGeometry map;map.source=&entry;map.models=entries-2;
    map.firstPath = pathCount;
    const double modelScale = entry.room == 0x10e ? 10. : 1.;
    const unsigned baseColor=colorIndex(entry.room);
    for(unsigned model=0;model<map.models;++model) {
      const auto bin=CopySubscreenEntry(data,4+model);
      const auto mesh=BuildRoomModelScene(bin,tpl);
      if(mesh.fallbackTriangles || mesh.unresolvedAlphaTriangles)
        throw std::runtime_error("Original menu: Map model texture is missing");
      const auto parsed=Model(bin);
      unsigned color=baseColor;
      if(color==0 && map.models>1 && !parsed.joints.empty()) {
        const int floor=int(parsed.joints[0].center[1]/100+.5f);
        const int playerFloor=selected?int((mini.vertices[selected->vertices[0]][1]+
            s*(mini.vertices[selected->vertices[1]][1]-mini.vertices[selected->vertices[0]][1])+
            t*(mini.vertices[selected->vertices[2]][1]-mini.vertices[selected->vertices[0]][1]))/100+.5):floor;
        if(floor!=playerFloor)color=unsigned(floor+5);
        if(color>=colors.size())throw std::runtime_error("Original menu: Map floor outside original palette bounds");
      }
      for(const auto &draw:mesh.draws)for(std::size_t first=draw.first;first<draw.first+draw.count;first+=3) {
        MapGeometry::Triangle tri;tri.texture=512+draw.image;tri.color=color;tri.rgba=colors[color];
        for(unsigned corner=0;corner<3;++corner) {
          const auto vertex=first+corner;
          for(unsigned axis=0;axis<3;++axis)tri.points[corner][axis]=float(mesh.vertices.at(vertex*3+axis)*modelScale);
          tri.uv[corner]={mesh.uv.at(vertex*2),mesh.uv.at(vertex*2+1)};
        }
        map.triangles.push_back(tri);
      }
    }
    for (const auto &tri : map.triangles) {
      for (const auto &v : tri.points) {
        const double x = v[0], z = v[2];
        if (!std::isfinite(x) || !std::isfinite(z))
          throw std::runtime_error("Original menu: Invalid map coordinates");
        map.bounds[0] = std::min(map.bounds[0], x);
        map.bounds[1] = std::min(map.bounds[1], z);
        map.bounds[2] = std::max(map.bounds[2], x);
        map.bounds[3] = std::max(map.bounds[3], z);
      }
      ++map.pathCount;
    }
    if (!map.pathCount || map.pathCount > 100000 ||
        pathCount > 100000 - map.pathCount)
      throw std::runtime_error("Original menu: Map polygon budget");
    pathCount += map.pathCount;
    for (unsigned n = 0; n < 2; ++n) {
      bounds[n] = std::min(bounds[n], map.bounds[n]);
      bounds[n + 2] = std::max(bounds[n + 2], map.bounds[n + 2]);
    }
    geometry.push_back(std::move(map));
  }
  const unsigned cameraStage=stage<0?std::min(room>>8,3u):std::min(unsigned(stage),3u);
  constexpr double cameras[][3]={{0,0,10000},{21000,12650,82850},
      {-425,-3210,104000},{-425,-3210,104000}};
  // ss_map::markGoalPosition: highest set scenario flag selects the part.
  static const std::vector<unsigned> goalFlags[18]={
      {0},{0,0x11,0x12,0x13,0x14,0x15,0x2c,0x17,0x0b,0x2e},
      {0x28,0x0f,0x2d,0x44,0x23,0x45},{0x46,0x47,0x48},{0},
      {0,0x49},{0,0x4a,0x4b},{0,0x4c,0x4d,0x4e},{0},
      {0,0x4f,0x50,0x51,0x52,0x53},{0,0x54,0x55},
      {0},{0},{0},{0},{0},{0},{0}};
  const bool areaGoal=area!=0 && area!=11 && area!=12 && area!=13 && area!=14;
  const auto goalModel=Model(CopySubscreenEntry(areaGoal?archive:mapData,areaGoal?6:0x0f));
  unsigned goalPart=0;
  if(scenario)for(unsigned i=1;i<goalFlags[area].size();++i)
    if(scenario->scenario.test(goalFlags[area][i]))goalPart=i;
  if(goalPart>=goalModel.joints.size())throw std::runtime_error("Original menu: Destination map part is missing");
  const auto goal=goalModel.joints[goalPart].center;
  const double tangent=std::tan(55.*3.1415927/360.);
  const auto focusHeight=std::min(cameras[cameraStage][2],
      std::max(4000./tangent,std::max(std::abs(marker[1]-goal[2]),std::abs(marker[0]-goal[0])*.75)/tangent));
  const auto frames=DecodeSubscreenLayout(CopySubscreenEntry(mapData,7));
  const auto frame=std::find_if(frames.begin(),frames.end(),[](const auto &q){return q.mark==0xfe;});
  if(frame==frames.end())throw std::runtime_error("Original menu: Map viewport is missing");
  const double sourceWidth=std::abs(frame->vertices[2]-frame->vertices[0]);
  const double sourceHeight=std::abs(frame->vertices[5]-frame->vertices[3]);
  const double frameWidth=std::max(sourceWidth,sourceHeight/.75),frameHeight=std::max(sourceHeight,sourceWidth*.75);
  std::ostringstream out; out << std::setprecision(12);
  out << "{\"source\":\"original\",\"scope\":\"area\",\"area\":" << area
      << ",\"stageKnown\":" << (stage >= 0 ? "true" : "false")
      << ",\"progressKnown\":" << (progressKnown?"true":"false")
      << ",\"geometry\":\"BIN\",\"camera\":[" << cameras[cameraStage][0] << ','
      << cameras[cameraStage][1] << ',' << cameras[cameraStage][2] << ",55]"
      << ",\"goal\":[" << goal[0] << ',' << goal[2] << ',' << goal[1] << ']'
      << ",\"focus\":[" << (marker[0]+goal[0])*.5 << ',' << (marker[1]+goal[2])*.5
      << ',' << (markerHeight+goal[1])*.5+focusHeight << ']'
      << ",\"zoomLimits\":[" << 4000./tangent << ',' << cameras[cameraStage][2] << ']'
      << ",\"stage\":" << int(cameraStage)
      << ",\"viewport\":[" << frame->position[0]+320 << ',' << 240-frame->position[1] << ',' << frameWidth << ',' << frameHeight << ']'
      << ",\"roomCount\":" << geometry.size()
      << ",\"pathCount\":" << pathCount << ",\"room\":" << room
      << ",\"markerValid\":" << (valid ? "true" : "false")
      << ",\"player\":[" << marker[0] << ',' << marker[1] << ',' << marker[2]
      << ',' << markerHeight << "],\"bounds\":[" << bounds[0] << ',' << bounds[1] << ',' << bounds[2]
      << ',' << bounds[3] << "],\"paths\":[";
  bool comma = false;
  for (const auto &map : geometry) {
    for (const auto &tri : map.triangles) {
      if (comma) out << ',';
      comma = true;
      out << "{\"texture\":" << tri.texture << ",\"colorIndex\":" << tri.color << ",\"color\":[";
      for(unsigned i=0;i<4;++i){if(i)out<<',';out<<tri.rgba[i];}
      out << "],\"points\":[";
      for (unsigned i = 0; i < 3; ++i) {
        if (i) out << ',';
        const auto &v = tri.points[i];
        out << '[' << v[0] << ',' << v[2] << ',' << v[1] << ']';
      }
      out << "],\"uv\":[";
      for(unsigned i=0;i<3;++i){if(i)out<<',';out<<'['<<tri.uv[i][0]<<','<<tri.uv[i][1]<<']';}
      out << "]}";
    }
  }
  out << "],\"rooms\":[";
  comma = false;
  for (const auto &map : geometry) {
    if (comma) out << ',';
    comma = true;
    out << "{\"id\":" << map.source->room << ",\"current\":"
        << (map.source->room == room ? "true" : "false")
        << ",\"modelCount\":" << map.models << ",\"firstPath\":" << map.firstPath
        << ",\"pathCount\":" << map.pathCount << ",\"bounds\":["
        << map.bounds[0] << ',' << map.bounds[1] << ',' << map.bounds[2] << ','
        << map.bounds[3] << "]}";
  }
  out << "]}";
  return out.str();
}
namespace {
// Bounded software projection for immutable SS models. The UI reads cached
// pixels; it never performs Disc I/O, Motion evaluation or rasterization.
struct MenuVertex { float x{}, y{}, reciprocalDepth{}; std::array<float,2> uv{}; std::array<float,3> light{}; };
Texture MenuModelImage(const StaticScene &scene, const Matrix34 &placement,
                       const RoomLighting &lighting, unsigned enable,
                       bool cullBackFaces) {
  if(scene.triangles>100000 || scene.vertices.size()!=scene.normals.size())
    throw std::runtime_error("Original menu: Model budget");
  Texture image; image.width=640; image.height=480; image.rgba.resize(640*480*4);
  std::vector<float> depth(640*480,std::numeric_limits<float>::infinity());
  const auto normals=NormalMatrix(placement);
  const auto &cut=lighting.cuts.at(0);
  const float focal=240/std::tan(10.f*.017453292519943295f);
  std::vector<MenuVertex> projected(scene.vertices.size()/3);
  for(std::size_t v=0;v<projected.size();++v) {
    const auto p=TransformPoint(placement,{scene.vertices[v*3],scene.vertices[v*3+1],scene.vertices[v*3+2]});
    auto n=TransformNormal(normals,{scene.normals[v*3],scene.normals[v*3+1],scene.normals[v*3+2]});
    const auto length=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
    if(length>0)for(auto &axis:n)axis/=length;
    auto &out=projected[v]; const auto d=5000-p[2];
    if(!(d>1))throw std::runtime_error("Original menu: Model is behind the camera");
    out.x=320+p[0]*focal/d;out.y=240-p[1]*focal/d;out.reciprocalDepth=1/d;
    out.uv={scene.uv.at(v*2),scene.uv.at(v*2+1)};
    const auto &ambient=enable==16?cut.screenAmbient:cut.actorAmbient;
    for(unsigned k=0;k<3;++k)out.light[k]=ambient[k]/255.f;
    unsigned selected=0;
    // sscrnLightCreate creates precisely source records0..2 individually;
    // LightMgr.create(cLit,cut,no,flag=0) activates the copied record with
    // be_flag|=3. This promotion is specific to SS, not a room-light rule.
    for(unsigned source=0;source<std::min(3u,unsigned(cut.lights.size()));++source) {
      const auto &l=cut.lights[source];
      const auto flags=l.flags|3;
      if((flags&3)!=3 || !(l.enableMask&enable) || l.id==4 || l.parent || selected>=8)continue;
      ++selected;
      if(l.type==7){for(unsigned k=0;k<3;++k)out.light[k]=std::max(out.light[k],l.color[k]/255.f);continue;}
      auto lp=l.position;if(l.type==5)for(unsigned k=0;k<3;++k)lp[k]=placement[k*4+3]+l.direction[k];
      std::array<float,3> delta{};float square=0;
      for(unsigned k=0;k<3;++k){delta[k]=lp[k]-p[k];square+=delta[k]*delta[k];}
      const auto distance=std::sqrt(square);if(!(distance>0))continue;
      float diffuse=0;for(unsigned k=0;k<3;++k)diffuse+=n[k]*delta[k]/distance;
      diffuse=std::max(0.f,diffuse);float attenuation=l.intensity;
      if(l.type==1 && l.radius)attenuation=std::max(0.f,l.intensity*(l.radius-distance)/l.radius);
      if(l.type==2 && l.radius)attenuation/=std::max(.00001f,1+(l.intensity-.1f)*square/(.1f*l.radius*l.radius));
      if(l.type==3 || l.type==6) {
        float cone=0,dirLength=0;for(auto axis:l.direction)dirLength+=axis*axis;
        dirLength=std::sqrt(dirLength);
        if(dirLength>0)for(unsigned k=0;k<3;++k)cone-=delta[k]/distance*l.direction[k]/dirLength;
        const auto cutoff=std::cos(l.cutoff*.017453292519943295f);
        attenuation=l.cutoff>0 && l.cutoff<=90?std::max(0.f,(cone-cutoff)/(1-cutoff)):1;
        if(l.type==3 && l.intensity>0 && l.intensity<1)
          attenuation/=1+.5f*(1-l.intensity)/l.intensity*(distance/5000+square/25000000);
      }
      for(unsigned k=0;k<3;++k)out.light[k]+=float(std::min(255u,unsigned(l.color[k])*l.color[3]/128))/255.f*diffuse*attenuation;
    }
    for(auto &k:out.light)k=std::clamp(k,0.f,1.f);
  }
  for(const auto &draw:scene.draws) {
    if(draw.texture<0)continue;
    const auto &tex=scene.textures.at(unsigned(draw.texture));
    for(std::size_t first=draw.first;first<draw.first+draw.count;first+=3) {
      const auto &a=projected.at(first),&b=projected.at(first+1),&c=projected.at(first+2);
      const auto area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);if(std::abs(area)<.00001f)continue;
      // Source cModel CullMode0 is single-sided; the case deliberately uses
      // CullMode2 (both sides). Y-down image coordinates invert CCW winding.
      if(cullBackFaces && area>=0)continue;
      const int minX=std::max(0,int(std::floor(std::min({a.x,b.x,c.x})))),maxX=std::min(639,int(std::ceil(std::max({a.x,b.x,c.x}))));
      const int minY=std::max(0,int(std::floor(std::min({a.y,b.y,c.y})))),maxY=std::min(479,int(std::ceil(std::max({a.y,b.y,c.y}))));
      for(int y=minY;y<=maxY;++y)for(int x=minX;x<=maxX;++x) {
        const auto px=x+.5f,py=y+.5f;
        const float wb=((px-a.x)*(c.y-a.y)-(py-a.y)*(c.x-a.x))/area;
        const float wc=((b.x-a.x)*(py-a.y)-(b.y-a.y)*(px-a.x))/area,wa=1-wb-wc;
        if(wa<0||wb<0||wc<0)continue;
        const auto inv=wa*a.reciprocalDepth+wb*b.reciprocalDepth+wc*c.reciprocalDepth;
        const auto d=1/inv;const auto pixel=std::size_t(y)*640+std::size_t(x);if(d>=depth[pixel])continue;
        std::array<float,3> w{wa*a.reciprocalDepth/inv,wb*b.reciprocalDepth/inv,wc*c.reciprocalDepth/inv};
        const auto u=w[0]*a.uv[0]+w[1]*b.uv[0]+w[2]*c.uv[0],v=w[0]*a.uv[1]+w[1]*b.uv[1]+w[2]*c.uv[1];
        auto address=[](float t,unsigned count,unsigned wrap){if(wrap)t-=std::floor(t);return unsigned(std::clamp(int(t*float(count)),0,int(count)-1));};
        const auto tx=address(u,tex.width,tex.wrapS),ty=address(v,tex.height,tex.wrapT);
        const auto t=(std::size_t(ty)*tex.width+tx)*4;
        // Ordinary model materials use the raster/material alpha. Texture
        // alpha participates only in the source alphaSetup stage (flag4).
        unsigned alpha=255;
        if(draw.alphaTest && draw.alphaTexture>=0) {
          const auto &mask=scene.textures.at(unsigned(draw.alphaTexture));
          const auto m=(std::size_t(address(v,mask.height,mask.wrapT))*mask.width+address(u,mask.width,mask.wrapS))*4;
          alpha=mask.rgba.at(m+3);if(alpha<=draw.alphaReference)continue;
        }
        if(!alpha)continue;
        depth[pixel]=d;
        for(unsigned k=0;k<3;++k){const auto light=w[0]*a.light[k]+w[1]*b.light[k]+w[2]*c.light[k];image.rgba[pixel*4+k]=std::uint8_t(std::clamp(float(tex.rgba[t+k])*light,0.f,255.f));}
        image.rgba[pixel*4+3]=std::uint8_t(alpha);
      }
    }
  }
  return image;
}
StaticScene PosedMenuPart(const std::vector<std::uint8_t> &bin,
                         const std::vector<std::uint8_t> &tpl,
                         const AnimationRig &rig) {
  const SkinnedModel model(bin);model.validateSkeleton(rig.bones().size());
  StaticScene scene;
  const auto palette=model.palette(rig.bones(),rig.bindCenters());
  for(const auto &prim:model.mesh.primitives) {
    if(prim.triangles.empty())continue;
    const auto &header=model.mesh.materialHeaders.at(prim.displayList);
    SceneDraw draw;draw.first=scene.vertices.size()/3;draw.count=prim.triangles.size()*3;
    draw.image=header[12];SetSceneSpecular(draw,header,model.mesh.sourceFlags);draw.alphaTest=(header[11]&4)!=0;draw.alphaImage=header[14];draw.alphaReference=header[20];
    scene.draws.push_back(draw);
    for(const auto &triangle:prim.triangles)for(auto corner:triangle) {
      const auto &ref=prim.vertices.at(corner);
      const auto &p=model.mesh.positions.at(ref.position),&n=model.mesh.normals.at(ref.normal);
      const auto point=TransformPoint(palette.at(model.positionWeights.at(ref.position)),p);
      const auto normal=TransformNormal(NormalMatrix(palette.at(model.normalWeights.at(ref.normal))),n);
      scene.vertices.insert(scene.vertices.end(),point.begin(),point.end());scene.normals.insert(scene.normals.end(),normal.begin(),normal.end());
      const auto &uv=model.mesh.texcoords.at(ref.texcoord);scene.uv.insert(scene.uv.end(),uv.begin(),uv.end());
      scene.colors.insert(scene.colors.end(),{255,255,255,255});
    }
    scene.triangles+=prim.triangles.size();
  }
  std::vector<std::uint8_t> empty(16);empty[0]=64;Smd palettes(empty);palettes.palettes.push_back(tpl);
  AttachSceneTextures(palettes,scene);
  if(scene.fallbackTriangles || scene.unresolvedAlphaTriangles)throw std::runtime_error("Original menu: Portrait texture is missing");
  return scene;
}
}
Texture LoadOriginalCaseBoardImage(const std::string &discFiles,unsigned columns,unsigned rows) {
  // puzzle.cpp::pzlPlayer::init, not the dimensions from later remakes.
  const unsigned dimensions[4][2]={{10,6},{11,7},{12,8},{15,8}};unsigned board=4;
  for(unsigned i=0;i<4;++i)if(dimensions[i][0]==columns && dimensions[i][1]==rows)board=i;
  if(board==4)throw std::runtime_error("Original menu: Original case size");
  const auto arc=ReadFile(discFiles+"/ss/Eng/ss_pzzl.dat");
  auto scene=BuildRoomModelScene(CopySubscreenEntry(arc,0x1a6+board),CopySubscreenEntry(arc,0x1a5));
  const auto common=ReadFile(discFiles+"/ss/Eng/ss_cmmn.dat");
  RoomLighting light(CopySubscreenEntry(common,0x12));
  const RoomLighting coreLight(CopySubscreenEntry(CoreBody(discFiles),0x0b));
  const auto &environment=coreLight.cuts.at(9);
  // SubScreenExec sets CORE_LIGHT cut9's environment before adding SS lamps.
  const auto lamps=light.cuts.at(0).lights;
  light.cuts.at(0)=environment;light.cuts.at(0).lights=lamps;
  // caseModelMove: settled FE unit=(-95,110), y += pzzlDbgNo=-12,
  // pzzlDbgPos=-300, case_rot=(-0.57,0,0).
  const float screen=5000*std::tan(10.f*.017453292519943295f)/240;
  SmdPlacement pose;pose.position={-95*screen,98*screen,-300};pose.rotation={-.57f,0,0};pose.scale={1,1,1};
  return MenuModelImage(scene,PlacementMatrix(pose),light,16,false);
}
void OriginalMenuModel::advance(unsigned ticks) {
  if(actors.empty())return;
  for(unsigned i=0;i<ticks;++i) {
    previousBones.clear();previousPlacements.clear();
    for(const auto &actor:actors) {
      previousBones.push_back(actor->rig()->bones());previousPlacements.push_back(actor->worldMatrix());
    }
    actors.front()->rig()->advanceAction();
  }
}
std::vector<Matrix34> OriginalMenuModel::renderBones(unsigned index) const {
  auto result=actors.at(index)->rig()->bones();
  if(index<previousBones.size() && previousBones[index].size()==result.size())
    for(unsigned i=0;i<result.size();++i)result[i]=InterpolateMotionMatrix(previousBones[index][i],result[i],renderFraction);
  return result;
}
Matrix34 OriginalMenuModel::renderPlacement(unsigned index) const {
  const auto current=actors.at(index)->worldMatrix();
  return index<previousPlacements.size()?InterpolateMotionMatrix(previousPlacements[index],current,renderFraction):current;
}
OriginalMenuModel LoadOriginalMenuPortrait(const std::string &discFiles,unsigned weaponItemId,bool animated) {
  const auto *carry=OriginalWeaponCarry(weaponItemId);
  const unsigned weapon=carry?carry->number:0,variant=carry?carry->variant:0;
  const auto player=RoomArchive::LoadDrs(discFiles+"/em/pl00.drs");
  const bool grenade=weapon==19 || weapon==22 || weapon==23,bow=weapon==28;
  std::ostringstream path;path<<discFiles<<"/ss/Cmn/ss_wep"<<std::setfill('0')<<std::setw(2)<<(grenade?19:weapon)<<".dat";
  // Sscrn/ss_model.cpp::leonModelInit / wepXXInit, original menu archives.
  // Bow is a test-only Leon combination: use its actual carry pose, because
  // the original menu's bow profile belongs to Krauser's player archive.
  const auto ss=bow?std::vector<std::uint8_t>{}:ReadFile(path.str());
  unsigned model=5,palette=4,right=7,left=24,motion=8;bool leftWeapon=false,playerModel=false;
  switch(weapon) {
    case 0: right=18;left=20;break;
    case 1:case 2:case 4: model+=variant;break;
    case 3: if(variant==2){model=6;motion=9;}break;
    case 5:case 15: right=6;left=7;leftWeapon=true;break;
    case 6: model+=variant;right=7;left=8;leftWeapon=true;motion=9;break;
    case 7:case 8:case 33: model=4;palette=5;right=6;left=weapon==8?25:22;motion=7;break;
    case 9:case 10: model+=variant;right=8;left=22;motion=9;break;
    case 11: model+=variant;right=9;left=21;motion=variant<2?11:12;break;
    case 12: right=6;left=22;motion=7;break;
    case 13: model=0x76;palette=0x75;playerModel=true;right=4;motion=5;break;
    case 14: model+=variant;motion=8+variant;break;
    case 17: right=6;motion=7;break;
    case 19:case 22:case 23: model=0x6a;palette=weapon==19?0x6b:weapon==22?0x6d:0x6f;
      playerModel=true;right=4;left=20;motion=5;break;
  }
  const SkinnedModel body(player.copy(0));
  std::unique_ptr<AnimationRig> owned;
  std::optional<RoomArchive> bowArchive;
  if(bow) {
    bowArchive.emplace(RoomArchive::LoadDrs(discFiles+"/em/wep28.drs"));
    owned=std::make_unique<AnimationRig>(body,std::vector<AnimationBytes>{{bowArchive->copy(carry->locomotion[0]-4),{}}});
  } else owned=std::make_unique<AnimationRig>(body,std::vector<AnimationBytes>{{CopySubscreenEntry(ss,motion),{}}});
  auto &rig=*owned;rig.sample(0,0);StaticScene scene;
  std::vector<std::shared_ptr<GenericEventActor>> actors;
  auto addPart=[&](const std::vector<std::uint8_t> &bin,const std::vector<std::uint8_t> &tpl) {
    if(!animated){AppendStaticSceneLayer(scene,PosedMenuPart(bin,tpl,rig));return;}
    auto actor=std::make_shared<GenericEventActor>("pl00-ss","SS",bin,tpl,actors.empty()?nullptr:actors.front().get());
    actor->setDisplayOverride(true);actor->setLightMaskOverride(1);
    if(actors.empty()) {
      actor->prepareMotions({{"ss-idle",bow?bowArchive->copy(carry->locomotion[0]-4):CopySubscreenEntry(ss,motion)}});
      // ss_model.cpp: MotionSetCore(ssPlModel,...,0,0,4), MotionMove every tick.
      actor->rig()->startActionFlags(0,0,0,4);
      actor->rig()->advanceAction(0);
    }
    actors.push_back(std::move(actor));
  };
  for(const auto &part:std::vector<std::array<unsigned,2>>{{0,1},{6,1},{4,3},{2,3},{5,3}})
    addPart(player.copy(part[0]),player.copy(part[1]));
  const auto rightBytes=bow?player.copy(0x12-4):weapon==0?player.copy(right-4):CopySubscreenEntry(ss,right);
  const auto leftBytes=bow?bowArchive->copy(7-4):leftWeapon?CopySubscreenEntry(ss,left):player.copy(left-4);
  addPart(rightBytes,player.copy(13));addPart(leftBytes,player.copy(13));
  auto attach=[&](StaticScene gun,const Matrix34 &matrix) {
    const auto normalMatrix=NormalMatrix(matrix);
    for(std::size_t v=0;v<gun.vertices.size()/3;++v){const auto p=TransformPoint(matrix,{gun.vertices[v*3],gun.vertices[v*3+1],gun.vertices[v*3+2]});const auto n=TransformNormal(normalMatrix,{gun.normals[v*3],gun.normals[v*3+1],gun.normals[v*3+2]});for(unsigned k=0;k<3;++k){gun.vertices[v*3+k]=p[k];gun.normals[v*3+k]=n[k];}}
    AppendStaticSceneLayer(scene,gun);
  };
  if(weapon) {
    auto addGun=[&](const std::vector<std::uint8_t> &bin,const std::vector<std::uint8_t> &tpl,const SmdPlacement &offset) {
      auto actor=std::make_shared<GenericEventActor>("wep-ss","SS",bin,tpl);
      actor->prepareStaticPose();actor->attach(actors.front().get(),bow?16:10);
      actor->setPlacement(offset);actor->setDisplayOverride(true);actor->setLightMaskOverride(1);
      if(weaponItemId==0x17)actor->setMaterialColor({160/255.f,208/255.f,224/255.f,1});
      actors.push_back(std::move(actor));
    };
    if(animated) {
      SmdPlacement offset;offset.scale={1,1,1};
      if(grenade){offset.position={-95,-30,-5};offset.rotation={1.5707964f,0,0};}
      addGun(bow?bowArchive->copy(1):playerModel?player.copy(model-4):CopySubscreenEntry(ss,model),
             bow?bowArchive->copy(2):playerModel?player.copy(palette-4):CopySubscreenEntry(ss,palette),offset);
      if(weapon==13){offset.position={-136,-30.72f,118.85f};offset.rotation={1.5707964f,0,-1.5707964f};addGun(player.copy(0x70-4),player.copy(0x71-4),offset);}
    } else {
    auto gun=bow?BuildRoomModelScene(bowArchive->copy(5-4),bowArchive->copy(6-4)):
      playerModel?BuildRoomModelScene(player.copy(model-4),player.copy(palette-4)):
      BuildRoomModelScene(CopySubscreenEntry(ss,model),CopySubscreenEntry(ss,palette));
    if(weaponItemId==0x17)for(auto &draw:gun.draws)draw.materialRGB={0xa0,0xd0,0xe0};
    auto matrix=rig.jointMatrices().at(bow?16:10);
    if(grenade) {SmdPlacement offset;offset.position={-95,-30,-5};offset.rotation={1.5707964f,0,0};offset.scale={1,1,1};matrix=WeaponAttachmentMatrix(PlacementMatrix(offset),matrix);}
    attach(std::move(gun),matrix);
    if(weapon==13) {
      auto rocket=BuildRoomModelScene(player.copy(0x70-4),player.copy(0x71-4));
      SmdPlacement offset;offset.position={-136,-30.72f,118.85f};offset.rotation={1.5707964f,0,-1.5707964f};offset.scale={1,1,1};
      attach(std::move(rocket),WeaponAttachmentMatrix(PlacementMatrix(offset),matrix));
    }
    }
  }
  const auto common=ReadFile(discFiles+"/ss/Eng/ss_cmmn.dat");RoomLighting light(CopySubscreenEntry(common,0x12));
  const RoomLighting coreLight(CopySubscreenEntry(CoreBody(discFiles),0x0b));
  const auto &environment=coreLight.cuts.at(9);
  auto lamps=light.cuts.at(0).lights;lamps.resize(std::min(std::size_t(3),lamps.size()));
  for(auto &lamp:lamps)lamp.flags|=3;
  light.cuts.at(0)=environment;light.cuts.at(0).lights=std::move(lamps);
  SmdPlacement pose;pose.position={850,-1370,0};pose.rotation={0,-.3f,0};pose.scale={1,1,1};
  for(std::size_t v=0;v<scene.vertices.size()/3;++v)for(unsigned k=0;k<3;++k) {
    if(!v)scene.minimum[k]=scene.maximum[k]=scene.vertices[k];
    scene.minimum[k]=std::min(scene.minimum[k],scene.vertices[v*3+k]);
    scene.maximum[k]=std::max(scene.maximum[k],scene.vertices[v*3+k]);
  }
  const auto specular=LoadCoreSpecular(discFiles+"/etc/core.das");
  AttachSceneSpecular(scene,specular);
  for(auto &actor:actors)actor->prepareSpecular(specular);
  if(!actors.empty())actors.front()->setPlacement(pose);
  return {std::move(scene),PlacementMatrix(pose),std::make_shared<const RoomLighting>(std::move(light)),std::move(actors),{},{},1};
}
Texture LoadOriginalMenuPortraitImage(const std::string &discFiles,unsigned weaponItemId) {
  const auto source=LoadOriginalMenuPortrait(discFiles,weaponItemId);
  return MenuModelImage(source.scene,source.placement,*source.lighting,1,true);
}

} // namespace re4
