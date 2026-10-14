// Bytewise 64-bit port of original room_jmp.cpp::getRoomInfo and
// sce_at.cpp::sceAtHitCheck / area.cpp. Audited village door conditions are
// handled with native scenario flags; PowerPC callbacks are never executed.
#include "room_flow.h"
#include "village_flow.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace re4 {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double coordinateBudget = 1e7;
void range(const std::vector<std::uint8_t> &b, std::size_t p, std::size_t n) {
  if (p > b.size() || n > b.size() - p)
    throw std::runtime_error("Room logic: Truncated data range");
}
std::uint16_t half(const std::vector<std::uint8_t> &b, std::size_t p) {
  range(b, p, 2);
  return (std::uint16_t(b[p]) << 8) | b[p + 1];
}
std::uint32_t word(const std::vector<std::uint8_t> &b, std::size_t p) {
  range(b, p, 4);
  return (std::uint32_t(b[p]) << 24) | (std::uint32_t(b[p + 1]) << 16) |
         (std::uint32_t(b[p + 2]) << 8) | b[p + 3];
}
double scalar(const std::vector<std::uint8_t> &b, std::size_t p) {
  auto bits = word(b, p);
  float f;
  static_assert(sizeof(f) == sizeof(bits), "IEEE32 float required");
  std::memcpy(&f, &bits, 4);
  if (!std::isfinite(f) || std::abs(double(f)) > coordinateBudget)
    throw std::runtime_error("Room logic: invalid numeric value");
  return f;
}
CollisionVec position(const std::vector<std::uint8_t> &b, std::size_t p) {
  return {scalar(b, p), scalar(b, p + 4), scalar(b, p + 8)};
}
std::int8_t signedByte(std::uint8_t b) {
  return static_cast<std::int8_t>(b < 128 ? int(b) : int(b) - 256);
}
bool finite(const CollisionVec &p) {
  for (auto v : p)
    if (!std::isfinite(v) || std::abs(v) > coordinateBudget)
      return false;
  return true;
}
std::string textAt(const std::vector<std::uint8_t> &b, std::size_t p,
                   std::size_t metadataEnd) {
  if (!p)
    return {};
  if (p < metadataEnd || p >= b.size())
    throw std::runtime_error("Room info: Text offset outside text bounds");
  auto begin = b.begin() + p;
  auto end =
      std::find(begin, begin + std::min(std::size_t(512), b.size() - p), 0);
  if (end == begin + std::min(std::size_t(512), b.size() - p))
    throw std::runtime_error("Room info: Text end is missing");
  return std::string(begin, end);
}
bool areaHit(const DoorArea &a, const CollisionVec &p) {
  // These boundaries deliberately match area.cpp, including floor-100 and
  // the strict upper height/radius limits.
  if (p[1] + 100 < a.floor || p[1] >= a.floor + a.height)
    return false;
  if (a.type == 2)
    return std::hypot(p[0] - a.points[0][0], p[2] - a.points[0][1]) < a.radius;
  if (a.type != 1)
    return false;
  auto cross = [&](unsigned from, unsigned to) {
    const auto &x = a.points[from], &y = a.points[to];
    return (y[0] - x[0]) * (p[2] - x[1]) - (y[1] - x[1]) * (p[0] - x[0]);
  };
  double twiceArea = 0;
  constexpr unsigned order[] = {0, 1, 2, 3, 0};
  for (int i = 0; i < 4; ++i) {
    auto &x = a.points[order[i]], &y = a.points[order[i + 1]];
    twiceArea += x[0] * y[1] - y[0] * x[1];
  }
  if (std::abs(twiceArea) < 1e-6)
    return false;
  return !(cross(0, 3) > 0 || cross(0, 1) < 0 || cross(2, 3) < 0 ||
           cross(2, 1) > 0);
}
} // namespace

RoomInfo::RoomInfo(const std::vector<std::uint8_t> &b) {
  if (b.size() < 8 || b.size() > 1024 * 1024)
    throw std::runtime_error("Room info: File budget exceeded");
  stages_ = word(b, 0);
  if (!stages_ || stages_ > 16)
    throw std::runtime_error("Room info: invalid stage count");
  std::size_t headerEnd = 4 + stages_ * 4, metadataEnd = headerEnd;
  range(b, 0, headerEnd);
  struct Stage {
    std::size_t id, offset, count, end;
  };
  std::vector<Stage> stages;
  std::vector<std::pair<std::size_t, std::size_t>> spans;
  std::size_t total = 0;
  for (std::size_t st = 0; st < stages_; ++st) {
    auto offset = word(b, 4 + st * 4);
    if (!offset)
      continue;
    if (offset < headerEnd || offset % 4)
      throw std::runtime_error("Room info: invalid stage offset");
    auto count = word(b, offset);
    if (count > 256 || total + count > 4096)
      throw std::runtime_error(
          "Room info: Start point count limit exceeded");
    range(b, offset + 4, std::size_t(count) * 32);
    std::size_t end = offset + 4 + std::size_t(count) * 32;
    stages.push_back({st, offset, count, end});
    spans.emplace_back(offset, end);
    metadataEnd = std::max(metadataEnd, end);
    total += count;
  }
  std::sort(spans.begin(), spans.end());
  for (std::size_t i = 1; i < spans.size(); ++i)
    if (spans[i].first < spans[i - 1].second)
      throw std::runtime_error("Room info: overlapping stage data");
  starts_.reserve(total);
  std::map<std::uint16_t, std::size_t> nextPoint;
  for (auto st : stages) {
    for (std::size_t i = 0; i < st.count; ++i) {
      std::size_t p = st.offset + 4 + i * 32;
      RoomStart s;
      s.flags = half(b, p);
      s.room = half(b, p + 2);
      if ((s.room >> 8) != st.id)
        throw std::runtime_error("Room info: Room/stage mismatch");
      s.point = nextPoint[s.room]++;
      s.recordOffset = p;
      s.position = position(b, p + 4);
      s.heading = scalar(b, p + 16);
      s.name = textAt(b, word(b, p + 20), metadataEnd);
      s.person = textAt(b, word(b, p + 24), metadataEnd);
      s.person2 = textAt(b, word(b, p + 28), metadataEnd);
      std::copy_n(b.begin() + p, s.raw.size(), s.raw.begin());
      starts_.push_back(std::move(s));
    }
  }
}
const RoomStart *RoomInfo::find(std::uint16_t room, std::size_t point) const {
  for (auto &s : starts_)
    if (s.room == room && s.point == point)
      return s.valid() ? &s : nullptr;
  return nullptr;
}

RoomFlow::RoomFlow(std::uint16_t sourceRoom, const std::vector<std::uint8_t> &b)
    : sourceRoom_(sourceRoom) {
  if (b.size() < 16 || b.size() > 1024 * 1024)
    throw std::runtime_error("AEV: File budget exceeded");
  if (b[0] != 'A' || b[1] != 'E' || b[2] != 'V' || b[3] != 0 ||
      half(b, 4) != 0x104)
    throw std::runtime_error("AEV: only original GC version0x104 is supported");
  count_ = half(b, 6);
  if (count_ > 256)
    throw std::runtime_error("AEV: Range count limit exceeded");
  range(b, 16, count_ * 156);
  std::set<std::uint8_t> numbers;
  for (std::size_t i = 0; i < count_; ++i) {
    std::size_t p = 16 + i * 156;
    if (!numbers.insert(b[p + 0x36]).second)
      throw std::runtime_error("AEV: Duplicate area number");
    if (b[p + 0x35] != 1 && b[p + 0x35] != 0x10 && b[p + 0x35] != 8)
      continue;
    RoomDoor d;
    d.sourceRoom = sourceRoom;
    d.recordIndex = i;
    d.no = b[p + 0x36];
    d.enabled = b[p + 0x34];
    d.hitType = b[p + 0x37];
    d.triggerType = b[p + 0x38];
    d.targetType = b[p + 0x39];
    d.taskLevel = b[p + 0x3a];
    d.parameterPointer = word(b, p + 0x3c);
    d.functionPointer = word(b, p + 0x40);
    d.priority = b[p + 0x44];
    d.kind = b[p + 0x45];
    d.waitingType = b[p + 0x46];
    d.waitingNo = b[p + 0x47];
    d.hitDirection = signedByte(b[p + 0x48]);
    d.hitOpenAngle = signedByte(b[p + 0x49]);
    d.parentPointer = word(b, p + 0x4c);
    d.country = b[p + 0x52];
    d.area.enabled = b[p + 4];
    d.area.type = b[p + 5];
    d.area.floor = scalar(b, p + 8);
    d.area.height = scalar(b, p + 12);
    d.area.radius = scalar(b, p + 16);
    // The payload is eleven floats for the quad, or five plus six padding
    // floats for cylinder/eye. Check all disk floats even when not used.
    for (std::size_t j = 0; j < 8; ++j)
      d.area.points[j / 2][j % 2] = scalar(b, p + 20 + j * 4);
    if (d.area.height < 0 || d.area.radius < 0 || d.hitOpenAngle < 0 ||
        d.priority > 15)
      throw std::runtime_error("AEV: invalid door range");
    if (b[p + 0x35] == 8) {
      std::copy_n(b.begin() + p + 4, d.area.raw.size(), d.area.raw.begin());
      std::copy_n(b.begin() + p, d.raw.size(), d.raw.begin());
      // sce_at.cpp::SceAtRoomSet, save areas use ACT_SAVE (0x2f), priority 5.
      if (!(d.triggerType & 8)) {
        d.triggerType = (d.triggerType & 0x80) | 8;
        d.priority = 5;
      }
      typewriters_.push_back(std::move(d));
      continue;
    }
    d.destination = position(b, p + 0x5c);
    d.destinationHeading = scalar(b, p + 0x68);
    if (b[p + 0x35] == 0x10) {
      std::copy_n(b.begin() + p + 4, d.area.raw.size(), d.area.raw.begin());
      std::copy_n(b.begin() + p, d.raw.size(), d.raw.begin());
      // SceAtRoomSet normalizes a fixed ladder to ACT_GO_UP/DOWN, priority5.
      if (!(d.triggerType & 8)) {
        d.triggerType = (d.triggerType & 0x80) | 8;
        d.priority = 5;
      }
      RoomLadder ladder;
      ladder.trigger = d;
      ladder.position = d.destination;
      ladder.heading = d.destinationHeading;
      ladder.level = signedByte(b[p + 0x6c]);
      ladder.type = b[p + 0x6d]; ladder.posSet = b[p + 0x6e];
      for (unsigned k = 0; k < 3; ++k) ladder.cameraCuts[k] = b[p + 0x6f + k];
      ladders_.push_back(std::move(ladder));
      continue;
    }
    d.destinationRoom = half(b, p + 0x6c);
    d.keyId = b[p + 0x6e];
    d.keyFlag = b[p + 0x6f];
    d.exitFunctionPointer = word(b, p + 0x70);
    d.destinationPart = b[p + 0x74];
    d.fadeEffect = b[p + 0x77];
    d.exitParameterPointer = word(b, p + 0x78);
    std::copy_n(b.begin() + p + 4, d.area.raw.size(), d.area.raw.begin());
    std::copy_n(b.begin() + p, d.raw.size(), d.raw.begin());
    doors_.push_back(std::move(d));
  }
  // SceAtInit inserts records in reverse order, then visits higher OT priority
  // first. Equal-priority entries therefore preserve their disk record order.
  std::stable_sort(
      doors_.begin(), doors_.end(),
      [](const auto &a, const auto &b) { return a.priority > b.priority; });
}
const RoomDoor *RoomFlow::find(std::uint8_t no) const {
  for (auto &d : doors_)
    if (d.no == no)
      return &d;
  return nullptr;
}
bool RoomFlow::hit(const RoomDoor &d, const CollisionVec &feet, double heading,
                   const std::optional<CollisionVec> &clippedFront) {
  if (!finite(feet) || !std::isfinite(heading) ||
      (clippedFront && !finite(*clippedFront)) || !(d.enabled & 1) ||
      !(d.targetType & 1) || d.parentPointer || (d.hitType & ~3))
    return false;
  CollisionVec pos = feet;
  pos[1] += 250;
  if (d.hitType & 1) {
    pos = clippedFront.value_or(
        CollisionVec{feet[0] + std::sin(heading) * 550, feet[1] + 250,
                     feet[2] + std::cos(heading) * 550});
  }
  if (!areaHit(d.area, pos))
    return false;
  if (d.hitType & 2) {
    double direction = double(d.hitDirection) * 2 * pi / 180;
    double delta = std::remainder(direction - heading, 2 * pi);
    double limit = double(d.hitOpenAngle) * 2 * pi / 180;
    if (delta < -limit || delta > limit)
      return false;
  }
  return true;
}
std::string RoomFlow::unavailableReason(const RoomDoor &d) {
  if (!(d.enabled & 1))
    return "Door area is disabled";
  const bool village=IsAuthoredVillageDoor(d);
  if (d.keyId && !village)
    return "Door requires a key or scenario flag";
  if (d.parentPointer || d.waitingType || d.waitingNo)
    return "Door area requires a dynamic model";
  if (d.functionPointer || d.parameterPointer || d.exitFunctionPointer ||
      d.exitParameterPointer || (!village && (d.taskLevel || d.kind)))
    return "Door requires a scenario callback";
  if (d.country || d.targetType != 1 || d.enabled != 3 || d.area.enabled != 1 ||
      (d.area.type != 1 && d.area.type != 2) || d.area.height <= 0 ||
      (d.area.type == 2 && d.area.radius <= 0) || (d.hitType & ~3) ||
      (d.triggerType != 8 && d.triggerType != 2 && !(village && d.triggerType==1)) || (!village && d.destinationPart) ||
      d.fadeEffect > 1)
    return "Door conditions are outside the native test";
  if(village)return {};
  const bool testLoop = d.triggerType == 8 &&
      ((d.sourceRoom == 0x300 && d.destinationRoom == 0x301) ||
       (d.sourceRoom == 0x301 && d.destinationRoom == 0x300));
  // Disc1 r100 AEV area0: an ordinary key-free held-action room door.
  // Original r100.cpp never replaces/disables it or requires the seven
  // Ganados to die. Its house door2 has a separate ETS runtime and RSF gates.
  // Village reverse transitions are admitted by the audited graph above;
  // their live battle/key conditions are evaluated by VillageDoorReason.
  const bool forestExit = d.triggerType == 2 && d.sourceRoom == 0x100 &&
                          d.destinationRoom == 0x101;
  if (d.no != 0 || (!testLoop && !forestExit))
    return "Scenario logic for this room transition is not ported yet";
  return {};
}
DoorSelection RoomFlow::select(const CollisionVec &feet, double heading,
                               bool pressed, bool held,
                               const std::optional<CollisionVec> &front,const ScenarioState *state,
                               unsigned part,unsigned entryTicks) const {
  DoorSelection result;
  for (auto &d : doors_) {
    if (!hit(d, feet, heading, front))
      continue;
    result.door = &d;
    result.reason = unavailableReason(d);
    if(result.reason.empty() && state && IsAuthoredVillageDoor(d))
      result.reason=VillageDoorReason(d,*state,part,entryTicks);
    result.available = result.reason.empty();
    result.activated = (d.triggerType & 8)   ? pressed
                       : (d.triggerType & 2) ? (pressed || held)
                                             : (d.triggerType & 1) != 0;
    return result;
  }
  return result;
}
} // namespace re4
