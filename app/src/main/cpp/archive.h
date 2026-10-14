#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace re4 {
struct ArchiveEntry {
  std::array<std::uint8_t, 4> tag{};
  std::size_t offset{}, extent{}, occurrence{};
};
// Owns immutable BE file bytes. Extents include padding: original directory has
// NO lengths.
class RoomArchive {
public:
  explicit RoomArchive(std::vector<std::uint8_t> bytes);
  static RoomArchive Load(const std::string &path);
  static RoomArchive LoadDrs(const std::string &path);
  const std::vector<ArchiveEntry> &entries() const { return entries_; }
  std::size_t size() const { return bytes_.size(); }
  const ArchiveEntry *find(const std::string &tag, int occurrence = 0) const;
  std::vector<std::uint8_t> copy(std::size_t entry) const;
  std::string describe() const;

private:
  std::vector<std::uint8_t> bytes_;
  std::vector<ArchiveEntry> entries_;
};
} // namespace re4
