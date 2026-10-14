#include "archive.h"
#include "laser_sight.h"
#include "room_ladders.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks;
void require(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
  ++checks;
}
std::vector<std::uint8_t> entry(const re4::RoomArchive &archive, const char *tag) {
  const auto *found = archive.find(tag);
  require(found != nullptr, "Original archive entry missing");
  return archive.copy(found - archive.entries().data());
}
}
int main(int argc, char **argv) {
  try {
    if (argc != 5) throw std::runtime_error("Usage: m28_assets_check ROOM.dat PL00.drs WEP01.drs CORE.das");
    const auto room = re4::RoomArchive::Load(argv[1]);
    const re4::Sat sat(entry(room, "SAT"));
    const re4::SatCollision world(sat);
    re4::WalkProbe probe(world);
    const std::string path(argv[1]);
    const unsigned id = path.find("r400") != std::string::npos ? 0x400 : 0x101;
    re4::RoomFlow flow(id, entry(room, "AEV"));
    re4::RoomLadders ladders(flow);
    re4::LeonPreview leon(argv[2], argv[3]);
    require(ladders.count() == 1 && flow.ladders().size() == 1,
            "Expected checked original fixed tower ladder");
    const auto &source = flow.ladders().front();
    constexpr double pi = 3.14159265358979323846;
    const float heading = float(source.heading + pi);
    auto approach = source.position;
    approach[0] += std::sin(source.heading) * 600;
    approach[2] += std::cos(source.heading) * 600;
    require(probe.place(approach), "Original ladder approach has real capsule clearance");
    leon.setPlacement(probe.position(), heading);
    const auto start = probe.position();
    const auto offer = ladders.offer(leon, probe);
    require(offer && !offer->down, "Original AEV offers ascent");
    require(ladders.interact(leon, probe), "Ascent starts on original ladder");
    unsigned upSounds = 0, ticks = 0;
    while (leon.ladderActive() && ticks++ < 400) {
      leon.step(probe, 0, 0, false, false, 1. / 30);
      upSounds += leon.drainLadderSounds().size();
      if (ticks == 50) {
        const auto before = leon.feet();
        const auto motion = leon.ladderState().ticks;
        leon.suspendClock();
        leon.step(probe, 0, 0, false, false, 0);
        require(leon.feet() == before && leon.ladderState().ticks == motion,
                "Pause retains authored ladder position and time");
      }
    }
    require(ticks == 198 && !leon.ladderActive() && !leon.ladderLandingFailed(),
            "Original ascent completes in 198 ticks with a valid landing");
    require(upSounds == 40, "Original ladder sound frame pairs fire exactly once");
    require(probe.position()[1] - start[1] > 8500, "Original vertical root motion reaches tower floor");
    leon.setPlacement(probe.position(), float(heading + pi));
    require(ladders.offer(leon, probe) && ladders.offer(leon, probe)->down,
            "Checked reciprocal return is offered on real tower floor");
    require(ladders.interact(leon, probe), "Return starts");
    ticks = 0;
    while (leon.ladderActive() && ticks++ < 400) {
      leon.step(probe, 0, 0, false, false, 1. / 30);
      leon.drainLadderSounds();
    }
    require(ticks == 204 && !leon.ladderActive() && !leon.ladderLandingFailed(),
            "Original descent completes in 204 ticks with a valid landing");
    require(std::abs(probe.position()[1] - start[1]) < 100,
            "Return reaches genuine ground rather than a synthetic floor");
    const auto core = re4::RoomArchive::LoadDrs(argv[4]);
    const auto laser = re4::BuildLaserSightData(entry(core, "EFF"));
    require(laser.maxLineLength == 19000 && laser.pointTexture.width == 96 &&
            laser.pointTexture.height == 96 && laser.blend == 1,
            "Original CORE laser line and additive target image decoded");
    std::cout << "M28 original-asset checks passed: " << checks << " (room "
              << std::hex << id << ")\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n'; return 1;
  }
}
