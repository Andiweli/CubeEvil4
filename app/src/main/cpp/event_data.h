#pragma once
#include "room_lighting.h"
#include <map>

namespace re4 {
struct EventPacket {
  unsigned type{};
  std::uint32_t flags{};
  int cut{}, frame{};
  std::size_t offset{};
  std::vector<std::uint8_t> bytes;
  std::string text(std::size_t offset, std::size_t length) const;
  std::int32_t integer(std::size_t offset) const;
};
struct EventAsset {
  std::string name;
  std::size_t offset{}, extent{};
};
// Immutable checked EVD. Directory fields remain offsets, never PPC pointers.
class EventData {
 public:
  explicit EventData(std::vector<std::uint8_t> bytes);
  static EventData Load(const std::string &path);
  const std::vector<EventPacket> &packets() const { return packets_; }
  const std::vector<EventAsset> &assets() const { return assets_; }
  std::vector<std::uint8_t> asset(const std::string &name) const;
  bool contains(const std::string &name) const;
  const std::vector<unsigned> &cutFrames() const { return cuts_; }
  unsigned totalFrames() const { return total_; }
  const std::string &name() const { return name_; }
  std::uint32_t soundFlags() const { return soundFlags_; }
 private:
  std::vector<std::uint8_t> bytes_;
  std::vector<EventAsset> assets_;
  std::map<std::string,std::size_t> names_;
  std::vector<EventPacket> packets_;
  std::vector<unsigned> cuts_;
  std::string name_;
  unsigned total_{};
  std::uint32_t soundFlags_{};
};
// Camera/shape FCV shares the original three-axis Hermite format. Owns
// endian-converted key bytes consumed by the unchanged motion64 evaluator.
class EventCurve {
 public:
  explicit EventCurve(const std::vector<std::uint8_t> &bytes);
  struct Channel { std::uint16_t kind{}; unsigned index{}; std::vector<std::uint8_t> keys; };
  unsigned maxFrame() const { return maximum_; }
  const std::vector<Channel> &channels() const { return channels_; }
  std::array<float,3> sample(unsigned channel,float frame) const;
 private:
  unsigned maximum_{};
  std::vector<Channel> channels_;
};
}
