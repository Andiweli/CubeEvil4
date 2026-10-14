// Real Disc1 first-room completion: no synthetic doorway or spawn offsets.
#include "archive.h"
#include "room_flow.h"
#include "walk_probe.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks;
void require(bool value, const char *message) {
  ++checks;
  if (!value) throw std::runtime_error(message);
}
std::vector<std::uint8_t> entry(const re4::RoomArchive &archive,
                              const char *name) {
  const auto *data = archive.find(name);
  require(data != nullptr, "Original room entry missing");
  return archive.copy(data - archive.entries().data());
}
}
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw std::runtime_error("Usage: m37_campaign_path_check r100.dat r101.dat");
    const auto forest = re4::RoomArchive::Load(argv[1]);
    const auto village = re4::RoomArchive::Load(argv[2]);
    const re4::RoomFlow outgoing(0x100, entry(forest, "AEV"));
    const re4::RoomFlow incoming(0x101, entry(village, "AEV"));
    require(outgoing.doors().size() == 1,
            "Original r100 has exactly one room exit, not a direct r102 exit");
    const auto *door = outgoing.find(0);
    require(door && door->destinationRoom == 0x101 &&
                door->destinationPart == 0 && door->fadeEffect == 0,
            "Actual forward room/part/fade preserved");
    require(door->destination == re4::CollisionVec{
                -52144.40234375, 90.57080078125, 22524.2734375} &&
                door->destinationHeading == 2.1220216751098633,
            "Exact authored village placement, without RoomInfo substitution");
    require(door->triggerType == 2 && door->keyId == 0 &&
                door->keyFlag == 0 && door->parentPointer == 0 &&
                door->functionPointer == 0 && door->parameterPointer == 0 &&
                door->exitFunctionPointer == 0 &&
                door->exitParameterPointer == 0,
            "Plain held-action door has no key, model parent or callback");
    require(re4::RoomFlow::unavailableReason(*door).empty(),
            "Native campaign can leave r100 at its actual exit");
    const re4::Sat forestSat(entry(forest, "SAT"));
    const re4::Sat villageSat(entry(village, "SAT"));
    const re4::SatCollision forestCollision(forestSat);
    const re4::SatCollision villageCollision(villageSat);
    // The polygon straddles the closed gate: its center itself has no SAT
    // floor. Search its authored bounds for a real body placement from which
    // the original 550-unit (SAT-clipped) front probe can reach the area.
    re4::CollisionVec center{0, door->area.floor + door->area.height - 251, 0};
    double minX = door->area.points[0][0], maxX = minX;
    double minZ = door->area.points[0][1], maxZ = minZ;
    for (const auto &point : door->area.points) {
      center[0] += point[0] * .25;
      center[2] += point[1] * .25;
      minX = std::min(minX, point[0]); maxX = std::max(maxX, point[0]);
      minZ = std::min(minZ, point[1]); maxZ = std::max(maxZ, point[1]);
    }
    re4::WalkProbe before(forestCollision), after(villageCollision);
    std::optional<re4::CollisionVec> clippedFront;
    double heading = 0;
    bool standableExit = false;
    for (unsigned ix = 0; ix <= 12 && !standableExit; ++ix) {
      for (unsigned iz = 0; iz <= 12 && !standableExit; ++iz) {
        re4::CollisionVec point{
            minX + (maxX - minX) * ix / 12,
            center[1], minZ + (maxZ - minZ) * iz / 12};
        const auto floor = forestCollision.ground(point, 0, door->area.height);
        if (!floor || !before.place(floor->point)) continue;
        const auto feet = before.position();
        const double angle = std::atan2(center[0] - feet[0], center[2] - feet[2]);
        auto from = feet; from[1] += 250;
        auto front = from;
        front[0] += std::sin(angle) * 550;
        front[2] += std::cos(angle) * 550;
        if (auto contact = forestCollision.line(from, front, re4::SatSubset::All, 0))
          front = contact->point;
        if (!re4::RoomFlow::hit(*door, feet, angle, front)) continue;
        standableExit = true; heading = angle; clippedFront = front;
      }
    }
    require(standableExit,
            "Actual gate is reachable from clear SAT with original clipped front probe");
    const auto feet = before.position();
    auto offer = outgoing.select(feet, heading, false, false, clippedFront);

    require(offer.door == door && offer.available && !offer.activated,
            "Exit remains offered while A is released");
    offer = outgoing.select(feet, heading, false, true, clippedFront);
    require(offer.door == door && offer.available && offer.activated,
            "Holding A activates source trigger2 without a new press edge");
    offer = outgoing.select(feet, heading, true, false, clippedFront);
    require(offer.available && offer.activated,
            "Pressing A activates the same original exit");
    require(!outgoing.select({-82910, 860, -38480}, 1.75, true, true).door,
            "House battle placement cannot accidentally leave the room");
    require(after.place(door->destination),
            "Authored village destination satisfies existing Leon floor/body resolver");
    require(std::hypot(after.position()[0] - door->destination[0],
                       after.position()[2] - door->destination[2]) < .001,
            "Transition preserves authored destination XZ exactly");
    require(std::abs(after.position()[1] - door->destination[1]) <= 250,
            "Only existing original floor snap applies on room load");
    re4::WalkConfig sourceBody;
    sourceBody.radius = 400;
    sourceBody.height = 800;
    sourceBody.originalScenerySphere = true;
    re4::WalkProbe afterOriginalBody(villageCollision, sourceBody);
    require(afterOriginalBody.place(door->destination),
            "Actual destination also accepts original 400-unit scenery sphere");
    const auto *returnDoor = incoming.find(0);
    require(returnDoor && returnDoor->destinationRoom == 0x100 &&
                !re4::RoomFlow::unavailableReason(*returnDoor).empty(),
            "Village return callback is not silently bypassed");
    for (const auto &other : incoming.doors())
      require(!re4::RoomFlow::unavailableReason(other).empty(),
              "Unaudited village exits stay unavailable");
    std::cout << "M37 actual first-room exit / village destination PASS: "
              << checks << " checks; r100 area0 -> r101, held-action=2\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
