#include "object_actions.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <cstdlib>
#include <algorithm>

namespace {
std::vector<std::uint8_t> copy(const re4::RoomArchive &room, const char *tag) {
  const auto *entry = room.find(tag);
  if (!entry)
    throw std::runtime_error(std::string("Original room: ") + tag + " missing");
  return room.copy(entry - room.entries().data());
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 2 && argc != 3)
      return 2;
    const auto room = re4::RoomArchive::Load(argv[1]);
    const auto placements = re4::ReadRoomObjectPlacements(copy(room, "ETS"));
    const auto roomId = std::uint16_t(argc == 3 ?
        std::strtoul(argv[2], nullptr, 16) : 0x400);
    re4::ObjectActions actions(roomId, room, placements);
    assert(actions.objects().size() == (roomId == 0x400 ? 16u : 13u));
    re4::ObjectDropContext context;
    context.handgunAmmo = 60;
    actions.setDropContext(context);
    std::map<unsigned, unsigned> sources, itemAmounts;
    unsigned checks = 0;
    for (std::size_t index = 0; index < actions.objects().size(); ++index) {
      const auto &object = actions.objects()[index];
      if (object.hasItem) {
        ++sources[object.item.sourceId];
        if (!(object.item.controlFlags & 1))
          assert(object.item.position[1] == double(object.placement.position[1]) + 50);
        assert(object.item.raw[0x46] == 2);
        assert(object.item.raw[0x47] == object.placement.slot);
      }
      assert(actions.states()[index].hp == 1000);
      // Exercise every original oriented hit volume, from both sides. The
      // selected local rays avoid unrelated scene objects without faking a
      // single world-axis rectangle around all village containers.
      const auto &matrix = object.matrix;
      const auto height = object.shapes[0].height * .5f;
      const auto localFrom = re4::TransformPoint(matrix, {0, height, -2000});
      const auto localTo = re4::TransformPoint(matrix, {0, height, 2000});
      const re4::CollisionVec from{localFrom[0], localFrom[1], localFrom[2]};
      const re4::CollisionVec to{localTo[0], localTo[1], localTo[2]};
      const auto hit = re4::HitAuthoredShapes(object.shapes, {}, matrix,
                                              from, to, 1e16f, 1);
      assert(hit);
      assert(actions.applyShot({index, object.placement.slot, *hit}, index + 1));
      assert(!actions.applyShot({index, object.placement.slot, *hit}, index + 1));
      const auto &state = actions.states()[index];
      assert(state.broken && state.itemResolved);
      assert(state.itemActive == bool(state.itemCount));
      if (object.explosive) {
        assert(object.placement.id == 0x2d);
        assert(object.placement.slot >= 39 && object.placement.slot <= 41);
        assert(state.bombWait == 2 && !state.itemCount);
      } else if (state.itemCount)
        itemAmounts[state.itemId] += state.itemCount;
      checks += 8;
    }
    if (roomId == 0x400) {
      assert(sources[0x1005] == 4 && sources[0x1004] == 3 && sources[0x1003] == 1);
      assert(sources[6] == 2 && sources[0x19] == 2 && sources[0x0e] == 1);
      assert(itemAmounts[4] == 80 && itemAmounts[0x18] == 45);
      assert(itemAmounts[6] == 2 && itemAmounts[0x19] == 2 && itemAmounts[0x0e] == 1);
    } else {
      assert(roomId == 0x101 && sources[0x1001] == 7);
      assert(sources[0x18] == 1 && sources[0x1c] == 1 && sources[0x19] == 1);
      assert(sources[2] == 1 && sources[0x0e] == 1 && sources[0x78] == 1);
      assert(itemAmounts[0x18] == 5 && itemAmounts[0x1c] == 1);
      for (std::size_t index = 0; index < actions.objects().size(); ++index) {
        const auto &object = actions.objects()[index];
        const auto &state = actions.states()[index];
        if (object.item.sourceId == 0x1001 && state.itemCount) {
          const auto models = re4::ObjectRandomDropModelIds();
          assert(std::find(models.begin(), models.end(), state.itemId) != models.end());
        }
      }
    }
    const auto cues = actions.drainEvents();
    assert(cues.size() == actions.objects().size());
    for (const auto &cue : cues) {
      assert(cue.soundBlock == (cue.explosive ? 6u : 1u));
      assert(cue.soundNo == (cue.explosive ? 0x56u :
             cue.boxType == 3 || cue.boxType == 5 ? 0x3cu : 0x1au));
      ++checks;
    }
    assert(actions.tick().empty());
    const auto blasts = actions.tick();
    assert(blasts.size() == (roomId == 0x400 ? 3u : 0u));
    assert(actions.tick().empty());
    checks += 3;
    // Original OT ordering can choose a neighboring item first. Repeated
    // action edges at each authored location still collect all 13 once.
    for (const auto &object : actions.objects()) {
      if (!object.hasItem)
        continue;
      const auto &p = object.item.position;
      const re4::CollisionVec feet{p[0], p[1] - 50, p[2] - 550};
      for (unsigned attempt = 0; attempt < 13; ++attempt) {
        const auto picked = actions.pickup(feet, 0, true);
        if (!picked)
          break;
        ++checks;
      }
    }
    for (const auto &state : actions.states()) {
      assert(state.broken && state.itemTaken == bool(state.itemCount) &&
             !state.itemActive && !state.hp);
      ++checks;
    }
    assert(actions.inventory().size() == itemAmounts.size());
    for (const auto &item : actions.inventory()) {
      assert(item.count == itemAmounts[item.id]);
      ++checks;
    }
    const auto saved = actions.snapshot();
    re4::ObjectActions reopened(roomId, room, placements);
    reopened.restore(saved);
    assert(reopened.snapshot().checksum == saved.checksum);
    assert(!reopened.target({0, 1000, -50000}, {0, 1000, 50000}));
    for (const auto &entry : saved.inventory) {
      auto inactive = saved;
      assert(re4::ObjectActions::consumeSnapshotItem(inactive, entry.id, entry.count));
      assert(!re4::ObjectActions::consumeSnapshotItem(inactive, entry.id, 1));
      reopened.restore(inactive);
      ++checks;
    }
    reopened.restore(saved);
    std::cout << reopened.describe() << '\n';
    std::cout << "Original r" << std::hex << roomId << std::dec
              << " sourcecontainers/explosives + ITA contents: " << checks
              << " checks PASS\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
