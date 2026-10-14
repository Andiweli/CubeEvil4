// Portable translation of Capcom's original Yz2 decoder.
// See docs/YZ2.md for original instruction mapping and differential validation.
#include "yz2.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace re4 {
namespace {
[[noreturn]] void fail(const char *reason) { throw std::runtime_error(reason); }
std::uint32_t big32(const std::vector<std::uint8_t> &s, std::size_t p) {
  if (p > s.size() || s.size() - p < 4)
    fail("Yz2: truncated container");
  return (std::uint32_t(s[p]) << 24) | (std::uint32_t(s[p + 1]) << 16) |
         (std::uint32_t(s[p + 2]) << 8) | s[p + 3];
}
int hex(std::uint8_t v) {
  if (v >= '0' && v <= '9')
    return v - '0';
  if (v >= 'a' && v <= 'f')
    return v - 'a' + 10;
  if (v >= 'A' && v <= 'F')
    return v - 'A' + 10;
  return -1;
}
std::uint32_t field(const std::vector<std::uint8_t> &s, std::size_t &p,
                    std::size_t end) {
  std::uint32_t n = 0;
  unsigned count = 0;
  while (p < end && hex(s[p]) >= 0) {
    if (++count > 8)
      fail("Yz2: Size field exceeds 32 bits");
    n = (n << 4) | std::uint32_t(hex(s[p++]));
  }
  if (!count)
    fail("Yz2: invalid hex header");
  return n;
}
struct Model {
  std::vector<std::uint16_t> freq, weight, base, lookup;
  std::uint32_t total, threshold;
  unsigned bits;
  bool dirty = true;
  explicit Model(unsigned n)
      : freq(n, 1), weight(n), base(n), lookup(32768), total(n), bits(0) {
    for (unsigned i = 0, sum = 0; i < n; i++) {
      weight[i] = std::uint16_t(32768 / n + (i < 32768 % n));
      base[i] = std::uint16_t(sum);
      sum += weight[i];
    }
    while (total >= (1u << bits))
      ++bits;
    threshold = 1u << bits;
  }
  void rebuild() {
    std::fill(lookup.begin(), lookup.end(),
              std::numeric_limits<std::uint16_t>::max());
    for (unsigned i = 0; i < freq.size(); i++) {
      unsigned end = unsigned(base[i]) + weight[i];
      if (end > lookup.size())
        fail("Yz2: invalid frequency model");
      std::fill(lookup.begin() + base[i], lookup.begin() + end,
                std::uint16_t(i));
    }
    dirty = false;
  }
  void update(unsigned symbol) {
    ++freq[symbol];
    ++total;
    if (bits <= 14 && total == threshold) {
      unsigned scale = 1u << (15 - bits), sum = 0;
      for (unsigned i = 0; i < freq.size(); i++) {
        base[i] = std::uint16_t(sum);
        weight[i] = std::uint16_t(unsigned(freq[i]) * scale);
        sum += weight[i];
      }
      dirty = true;
      threshold = 1u << ++bits;
    } else if (bits > 14 && total > 32767) {
      unsigned sum = 0;
      total = 0;
      for (unsigned i = 0; i < freq.size(); i++) {
        // Original: cumulative weights use PRE-halving counts. Do not reorder.
        base[i] = std::uint16_t(sum);
        weight[i] = freq[i];
        sum += freq[i];
        if (freq[i] > 1)
          freq[i] >>= 1;
        total += freq[i];
      }
      dirty = true;
    }
  }
};
struct Reader {
  const std::vector<std::uint8_t> &source;
  std::size_t at, end;
  std::uint32_t range = 0x80, code;
  Reader(const std::vector<std::uint8_t> &s, std::size_t start,
         std::size_t limit)
      : source(s), at(start), end(limit), code(byte()) {}
  std::uint8_t byte() {
    if (at >= end)
      fail("Yz2: compressed stream is truncated");
    return source[at++];
  }
  unsigned decode(Model &m) {
    // Both models share ONE arithmetic range/code state (original Yz2Dec).
    if (range <= 0x800000) {
      unsigned count = range > 0x8000 ? 1 : range > 0x80 ? 2 : 3;
      for (unsigned i = 0; i < count; i++) {
        range <<= 8;
        code = (code << 8) | byte();
      }
    }
    const std::uint32_t unit = range >> 14;
    if (!unit)
      fail("Yz2: Null range in decoder");
    const std::uint32_t value = code / unit;
    if (m.dirty)
      m.rebuild();
    if (value >= m.lookup.size())
      fail("Yz2: Symbol outside model bounds");
    unsigned symbol = m.lookup[value];
    if (symbol >= m.freq.size())
      fail("Yz2: Unassigned symbol");
    // Keep unsigned 32-bit multiply/wrap semantics of mullw/subf/srwi.
    code -= unit * std::uint32_t(m.base[symbol]);
    range = (unit * std::uint32_t(m.weight[symbol])) >> 1;
    m.update(symbol);
    return symbol;
  }
};
struct Dictionary {
  unsigned next = 0;
  std::array<std::size_t, 512> offsets{};
  std::array<std::uint32_t, 512> lengths{};
  std::array<bool, 512> valid{};
};
} // namespace
Yz2Result DecodeYz2(const std::vector<std::uint8_t> &s, std::size_t maxOutput) {
  if (s.empty() || s.size() > 128u * 1024 * 1024)
    fail("Yz2: Unsupported input size");
  std::size_t start = 0, end = s.size();
  if (big32(s, 0) == 0xcab6be20) {
    if (s.size() < 1024)
      fail("Yz2: Truncated DVD header");
    for (unsigned i = 0; i < 8; i++)
      if (big32(s, i * 4) != 0xcab6be20)
        fail("Yz2: Corrupted DVD identifier");
    bool found = false, terminated = false;
    for (std::size_t p = 32; p < 1024; p += 32) {
      auto type = big32(s, p);
      if (type == 0xffffffff) {
        terminated = true;
        break;
      }
      if (type > 4 && type != 0xfffffffe)
        fail("Yz2: Unknown DVD part type");
      if (type == 0) {
        if (found)
          fail("Yz2: multiple MRAM sections are not supported yet");
        auto size = big32(s, p + 4), offset = big32(s, p + 12);
        if (offset < 1024 || (offset & 31) || offset > s.size() ||
            size > s.size() - offset)
          fail("Yz2: DVD section outside file bounds");
        start = offset;
        end = start + size;
        found = true;
      }
    }
    if (!found || !terminated)
      fail("Yz2: DVD MRAM section is missing");
  }
  std::size_t p = start, headerEnd = std::min(end, start + 32);
  auto packed = field(s, p, headerEnd);
  if (p >= headerEnd || (s[p] != '\t' && s[p] != ' '))
    fail("Yz2: Header separator is missing");
  ++p;
  auto size = field(s, p, headerEnd);
  if (p >= headerEnd || (s[p] != '\n' && s[p] != '\r' && s[p] != 0))
    fail("Yz2: Header terminator is missing");
  auto stream = (p + 32) & ~std::size_t(31);
  // Original packed field excludes the four-byte arithmetic coder flush.
  if (stream > end || packed > end - stream || end - stream - packed < 4)
    fail("Yz2: declared stream is truncated");
  end = stream + packed + 4;
  if (!size || size > maxOutput)
    fail("Yz2: Output budget exceeded");
  Yz2Result result;
  result.payloadOffset = start;
  result.streamOffset = stream;
  result.packedSize = packed;
  result.bytes.resize(size);
  Reader reader(s, stream, end);
  Model main(0x500), extra(0x100);
  std::vector<Dictionary> dictionary(256);
  std::size_t pos = 0, anchor = 0;
  while (pos < size) {
    unsigned symbol = reader.decode(main);
    std::uint32_t length = 1;
    if (symbol > 0x3ff)
      result.bytes[pos++] = std::uint8_t(symbol);
    else {
      if (anchor >= pos)
        fail("Yz2: Reference before first literal");
      auto &d = dictionary[result.bytes[anchor]];
      unsigned slot = (d.next + symbol) & 511;
      if (!d.valid[slot])
        fail("Yz2: uninitialized dictionary reference");
      if (symbol <= 0x1ff) {
        auto v = reader.decode(extra);
        if (v <= 2) {
          unsigned count = 4 - v;
          v = 0;
          for (unsigned j = 0; j < count; j++)
            v = (v << 8) | reader.decode(extra);
        }
        if (v <= 1)
          fail("Yz2: invalid reference length");
        length = v - 1;
      } else
        length = d.lengths[slot];
      auto source = d.offsets[slot];
      if (!length || source >= pos || length > size - pos)
        fail("Yz2: Reference outside output bounds");
      // Byte-wise forward copy intentionally preserves overlapping LZ runs.
      for (std::uint32_t j = 0; j < length; j++)
        result.bytes[pos + j] = result.bytes[source + j];
      pos += length;
    }
    if (anchor < pos - 1) {
      auto &d = dictionary[result.bytes[anchor]];
      d.offsets[d.next] = anchor + 1;
      d.lengths[d.next] = length;
      d.valid[d.next] = true;
      d.next = (d.next + 1) & 511;
      anchor = pos - 1;
    }
  }
  result.consumed = reader.at - stream;
  return result;
}
Yz2Result LoadYz2(const std::string &path, std::size_t maxOutput) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f)
    fail("Yz2: Could not open room file");
  auto size = f.tellg();
  if (size <= 0 || size > 128 * 1024 * 1024)
    fail("Yz2: File is too large or empty");
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  f.seekg(0);
  if (!f.read(reinterpret_cast<char *>(bytes.data()), size))
    fail("Yz2: Could not read complete file");
  return DecodeYz2(bytes, maxOutput);
}
} // namespace re4
