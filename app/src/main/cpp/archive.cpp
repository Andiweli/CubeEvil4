// Original lookup semantics: upstream src/game/read.cpp::GetDataExt.
// Disk layouts are parsed bytewise, independently of native pointer size and
// endianness.
#include "archive.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
namespace re4 {
namespace {
std::uint32_t word(const std::vector<std::uint8_t> &b, std::size_t p) {
  if (p > b.size() || b.size() - p < 4)
    throw std::runtime_error("Room archive: truncated header");
  return (std::uint32_t(b[p]) << 24) | (std::uint32_t(b[p + 1]) << 16) |
         (std::uint32_t(b[p + 2]) << 8) | b[p + 3];
}
std::string key(const std::array<std::uint8_t, 4> &t) {
  return std::string(reinterpret_cast<const char *>(t.data()), 3);
}
std::string display(const std::array<std::uint8_t, 4> &t) {
  std::ostringstream s;
  for (unsigned i = 0; i < 3; i++) {
    if (t[i] >= 32 && t[i] <= 126)
      s << char(t[i]);
    else
      s << "\\x" << std::hex << std::setw(2) << std::setfill('0')
        << unsigned(t[i]);
  }
  return s.str();
}
} // namespace
RoomArchive::RoomArchive(std::vector<std::uint8_t> bytes)
    : bytes_(std::move(bytes)) {
  if (bytes_.size() < 16 || bytes_.size() > 64u * 1024 * 1024)
    throw std::runtime_error("Room archive: Unsupported size");
  auto count = word(bytes_, 0);
  if (count > 4096 || count > (bytes_.size() - 16) / 8)
    throw std::runtime_error("Room archive: File table outside data bounds");
  const std::size_t end = 16 + std::size_t(count) * 8,
                    tags = 16 + std::size_t(count) * 4;
  entries_.reserve(count);
  std::vector<std::size_t> boundaries;
  std::map<std::string, std::size_t> occurrences;
  for (unsigned i = 0; i < count; i++) {
    ArchiveEntry e;
    e.offset = word(bytes_, 16 + i * 4);
    if (e.offset < end || e.offset > bytes_.size())
      throw std::runtime_error("Room archive: Subfile outside data bounds");
    std::copy_n(bytes_.begin() + tags + i * 4, 4, e.tag.begin());
    e.occurrence = occurrences[key(e.tag)]++;
    boundaries.push_back(e.offset);
    entries_.push_back(e);
  }
  boundaries.push_back(bytes_.size());
  std::sort(boundaries.begin(), boundaries.end());
  boundaries.erase(std::unique(boundaries.begin(), boundaries.end()),
                   boundaries.end());
  for (auto &e : entries_) {
    auto next =
        std::upper_bound(boundaries.begin(), boundaries.end(), e.offset);
    e.extent = next == boundaries.end() ? 0 : *next - e.offset;
  }
}
RoomArchive RoomArchive::Load(const std::string &path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in)
    throw std::runtime_error("Room archive: File missing: " + path);
  auto length = in.tellg();
  if (length < 16 || length > 64 * 1024 * 1024)
    throw std::runtime_error("Room archive: File budget exceeded");
  std::vector<std::uint8_t> b(static_cast<std::size_t>(length));
  in.seekg(0);
  if (!in.read(reinterpret_cast<char *>(b.data()), length))
    throw std::runtime_error("Room archive: Truncated file");
  return RoomArchive(std::move(b));
}
RoomArchive RoomArchive::LoadDrs(const std::string &path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in)
    throw std::runtime_error("DRS: File is missing");
  auto size = in.tellg();
  if (size < 0x400 || size > 64 * 1024 * 1024)
    throw std::runtime_error("DRS: File budget");
  std::vector<std::uint8_t> header(0x400);
  in.seekg(0);
  if (!in.read(reinterpret_cast<char *>(header.data()), header.size()))
    throw std::runtime_error("DRS: Truncated header");
  auto offset = word(header, 0x2c), length = word(header, 0x24);
  if (word(header, 0x20) != 0 || offset != 0x400 || length < 16 ||
      length > static_cast<std::size_t>(size) - offset)
    throw std::runtime_error("DRS: invalid data record");
  std::vector<std::uint8_t> body(length);
  in.seekg(offset);
  if (!in.read(reinterpret_cast<char *>(body.data()), body.size()))
    throw std::runtime_error("DRS: Truncated data record");
  return RoomArchive(std::move(body));
}
const ArchiveEntry *RoomArchive::find(const std::string &tag,
                                      int occurrence) const {
  if (tag.size() != 3 || occurrence < 0)
    return nullptr;
  for (const auto &e : entries_)
    if (key(e.tag) == tag && e.occurrence == std::size_t(occurrence))
      return &e;
  return nullptr;
}
std::vector<std::uint8_t> RoomArchive::copy(std::size_t index) const {
  if (index >= entries_.size())
    throw std::out_of_range("Room archive: invalid file index");
  const auto &e = entries_[index];
  return {bytes_.begin() + e.offset, bytes_.begin() + e.offset + e.extent};
}
std::string RoomArchive::describe() const {
  std::ostringstream s;
  s << "Archive: " << entries_.size() << " subfiles · Bounds verified\n";
  for (const auto &e : entries_) {
    s << display(e.tag) << "[" << e.occurrence << "] · Offset 0x" << std::hex
      << e.offset << std::dec << " · Range " << e.extent << " bytes\n";
  }
  s << "Ranges may contain alignment padding; no exact "
       "payload lengths.\n";
  return s.str();
}
} // namespace re4
