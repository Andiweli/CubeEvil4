#include "archive.h"
#include "room_flow.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
namespace {
std::vector<std::uint8_t> read(const std::string &path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in)
    throw std::runtime_error("File missing");
  auto size = in.tellg();
  if (size < 8 || size > 1024 * 1024)
    throw std::runtime_error("File budget");
  std::vector<std::uint8_t> b(static_cast<std::size_t>(size));
  in.seekg(0);
  if (!in.read(reinterpret_cast<char *>(b.data()), size))
    throw std::runtime_error("Short read");
  return b;
}
template <std::size_t N> void raw(const std::array<std::uint8_t, N> &bytes) {
  std::cout << std::hex << std::setfill('0');
  for (auto b : bytes)
    std::cout << std::setw(2) << unsigned(b);
  std::cout << std::dec;
}
void starts(const re4::RoomInfo &info, const re4::SatCollision *world = nullptr,
            int room = -1) {
  for (const auto &s : info.starts()) {
    if (room >= 0 && s.room != room)
      continue;
    std::cout << "S " << s.room << ' ' << s.flags << ' ' << s.point << ' '
              << s.recordOffset;
    for (auto p : s.position)
      std::cout << ' ' << p;
    std::cout << ' ' << s.heading << ' ';
    raw(s.raw);
    std::cout << '\n';
    if (world && s.valid()) {
      auto hit = world->ground(s.position, 250.01, 250.01);
      std::cout << "G " << s.room << ' ' << s.point << ' ' << bool(hit);
      if (hit)
        std::cout << ' ' << hit->point[1] - s.position[1] << ' '
                  << world->clearCapsule(hit->point, 300, 1600) << ' '
                  << hit->polygon;
      std::cout << '\n';
    }
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    std::cout << std::setprecision(17);
    if (argc == 3 && std::string(argv[1]) == "--info") {
      re4::RoomInfo info(read(argv[2]));
      std::cout << "I " << info.stageCount() << ' ' << info.starts().size()
                << '\n';
      starts(info);
      return 0;
    }
    if (argc != 3 && argc != 4) {
      std::cerr << "Usage: room_flow_tool ROOM.dat ROOM_HEX [ROOMINFO.dat]\n"
                   "       room_flow_tool --info ROOMINFO.dat\n";
      return 2;
    }
    std::size_t used = 0;
    auto room = std::stoul(argv[2], &used, 16);
    if (used != std::string(argv[2]).size() || room > 0xffff)
      throw std::runtime_error("Invalid room id");
    auto archive = re4::RoomArchive::Load(argv[1]);
    auto entry = archive.find("AEV");
    if (!entry) {
      std::cout << "F " << room << " 0 0\n";
      return 0;
    }
    re4::RoomFlow flow(room, archive.copy(entry - archive.entries().data()));
    std::cout << "F " << room << ' ' << flow.recordCount() << ' '
              << flow.doors().size() << '\n';
    for (const auto &d : flow.doors()) {
      std::cout << "D " << d.sourceRoom << ' ' << unsigned(d.no) << ' '
                << d.recordIndex << ' ' << d.destinationRoom << ' '
                << unsigned(d.destinationPart) << ' ' << unsigned(d.keyId)
                << ' ' << unsigned(d.keyFlag) << ' ' << unsigned(d.enabled)
                << ' ' << unsigned(d.hitType) << ' ' << unsigned(d.triggerType)
                << ' ' << unsigned(d.targetType) << ' ' << unsigned(d.priority)
                << ' ' << unsigned(d.area.enabled) << ' '
                << unsigned(d.area.type) << ' ' << d.area.floor << ' '
                << d.area.height << ' ' << d.area.radius;
      for (auto point : d.area.points)
        std::cout << ' ' << point[0] << ' ' << point[1];
      for (auto p : d.destination)
        std::cout << ' ' << p;
      std::cout << ' ' << d.destinationHeading << ' ' << int(d.hitDirection)
                << ' ' << int(d.hitOpenAngle) << ' '
                << re4::RoomFlow::unavailableReason(d).empty() << ' ';
      raw(d.raw);
      std::cout << '\n';
      re4::CollisionVec center{0, d.area.floor, 0};
      if (d.area.type == 1) {
        for (auto p : d.area.points) {
          center[0] += p[0] * .25;
          center[2] += p[1] * .25;
        }
      } else {
        center[0] = d.area.points[0][0];
        center[2] = d.area.points[0][1];
      }
      for (double x : {0., 2000., -2000.}) {
        for (double heading : {0., .7, 3.141592653589793, -.4, 2., -3.}) {
          auto feet = center;
          feet[0] += x;
          std::cout << "H " << unsigned(d.no) << ' ' << feet[0] << ' '
                    << feet[1] << ' ' << feet[2] << ' ' << heading << ' '
                    << re4::RoomFlow::hit(d, feet, heading) << '\n';
        }
      }
    }
    if (argc == 4) {
      re4::RoomInfo info(read(argv[3]));
      auto satEntry = archive.find("SAT");
      if (satEntry) {
        re4::Sat sat(archive.copy(satEntry - archive.entries().data()));
        re4::SatCollision world(sat);
        starts(info, &world, room);
      } else {
        starts(info, nullptr, room);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
