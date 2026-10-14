#include "object_actions.h"
#include "room_items.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>

namespace {
void rejects(const std::function<void()> &action) {
  bool rejected = false;
  try {
    action();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 3 && argc != 4)
      return 2;
    const auto roomId = argc == 4 ? std::stoul(argv[3], nullptr, 16) : 0x400u;
    assert(roomId == 0x400 || roomId == 0x101);
    const auto room = re4::RoomArchive::Load(argv[1]);
    const auto core = re4::RoomArchive::LoadDrs(argv[2]);
    const auto *eff = core.find("EFF");
    if (!eff)
      throw std::runtime_error("CORE EFF fehlt");
    const auto props = re4::BuildRoomObjects(
        room, core.copy(eff - core.entries().data()), roomId);
    const auto *entry = room.find("ITM");
    assert(entry);
    const re4::RoomItemArchive archive(
        room.copy(entry - room.entries().data()));
    assert(archive.assets().size() == (roomId == 0x400 ? 15 : 21));
    const re4::ObjectActions actions(roomId, room, props.placements);
    const auto before = actions.snapshot();
    const auto items = re4::BuildRoomItems(room, actions);
    assert(items.owners == 13);
    assert(items.rendered == (roomId == 0x400 ? 13 : 62));
    assert(items.uniqueModels == (roomId == 0x400 ? 5 : 10));
    assert(actions.snapshot().checksum ==
           before.checksum); // Loading never rolls drops.
    assert(items.mesh.vertices.size() == items.mesh.triangles * 9);
    assert(items.mesh.normals.size() == items.mesh.vertices.size());
    assert(items.mesh.uv.size() == items.mesh.triangles * 6);
    assert(items.mesh.colors.size() == items.mesh.triangles * 12);
    assert(items.mesh.texturedTriangles == items.mesh.triangles);
    assert(items.mesh.fallbackTriangles == 0 &&
           items.mesh.unresolvedAlphaTriangles == 0);
    for(const auto &draw:items.mesh.draws)assert(draw.orderType==0);
    assert(re4::BuildRoomItems(room, std::vector<re4::RoomItemPlacement>{})
               .rendered == 0);
    // The direct placement API retains its strict one-pickup-per-owner
    // contract. Only the checked ObjectActions path expands random variants.
    auto fixed = items.placements;
    fixed.resize(2);
    fixed[1].index = fixed[0].index + 1;
    auto malformed = fixed;
    malformed[1].index = malformed[0].index;
    rejects([&] { re4::BuildRoomItems(room, malformed); });
    malformed = fixed;
    malformed[0].index = 64;
    rejects([&] { re4::BuildRoomItems(room, malformed); });
    malformed = fixed;
    malformed[0].id = 0xa5; // No real room model; never synthesize a cube.
    const auto mixed=re4::BuildRoomItems(room,malformed);
    assert(mixed.owners==2 && mixed.rendered==1 && mixed.uniqueModels==1);
    assert(mixed.placements.size()==2 && mixed.placements[0].index==malformed[0].index &&
           mixed.placements[0].id==0xa5 && mixed.placements[1].index==malformed[1].index);
    assert(!mixed.mesh.draws.empty());
    for(const auto &draw:mixed.mesh.draws)
      assert(draw.roomItemIndex==int(malformed[1].index) && draw.roomItemId==int(malformed[1].id));
    auto missingOnly=malformed;missingOnly[1].id=0xa5;
    const auto noModels=re4::BuildRoomItems(room,missingOnly);
    assert(noModels.owners==2 && !noModels.rendered && !noModels.uniqueModels &&
           noModels.placements.size()==2 && noModels.mesh.draws.empty());
    assert(actions.snapshot().checksum==before.checksum);
    malformed = fixed;
    malformed[0].position[1] = std::numeric_limits<float>::quiet_NaN();
    rejects([&] { re4::BuildRoomItems(room, malformed); });
    malformed = fixed;
    malformed[0].rotation[1] = std::numeric_limits<float>::infinity();
    rejects([&] { re4::BuildRoomItems(room, malformed); });
    malformed.resize(65);
    rejects([&] { re4::BuildRoomItems(room, malformed); });
    std::map<std::pair<unsigned, unsigned>, std::size_t> triangles;
    std::map<unsigned, std::vector<unsigned>> variants;
    for (const auto &placement : items.placements)
      variants[placement.index].push_back(placement.id);
    for (const auto &draw : items.mesh.draws) {
      assert(draw.roomObjectSlot == -1 && draw.roomItemIndex >= 0);
      const auto index = unsigned(draw.roomItemIndex);
      assert(index < actions.objects().size());
      assert(draw.roomItemId >= 0);
      const auto owner = actions.objects()[index].item.position;
      for (unsigned k = 0; k < 3; ++k)
        assert(draw.sortCenter[k] == float(owner[k]));
      triangles[{index, unsigned(draw.roomItemId)}] += draw.count / 3;
    }
    assert(triangles.size() == items.placements.size());
    for (std::size_t i = 0; i < items.placements.size(); ++i) {
      const auto &placement = items.placements[i];
      const auto &object = actions.objects()[placement.index];
      assert(object.hasItem);
      if (object.item.sourceId == 0x1001 || object.item.sourceId == 0x1002) {
        assert(variants[placement.index] ==
               re4::ObjectRandomDropModelIds(roomId == 0x400));
        assert(object.item.id == 0 && object.item.count == 0);
        assert(!actions.states()[placement.index].itemResolved);
      } else {
        assert(placement.id == object.item.id);
        assert(variants[placement.index].size() == 1);
      }
      for (unsigned k = 0; k < 3; ++k)
        assert(placement.position[k] == float(object.item.position[k]));
      assert(triangles[std::make_pair(placement.index, placement.id)]);
    }
    for (const auto &draw : props.mesh.draws)
      assert(draw.roomObjectSlot ==
                 (draw.object < 64 ? int(draw.object) : -1) &&
             draw.roomItemIndex == -1 && draw.roomItemId == -1);
    auto combined = props.mesh;
    const auto count = combined.draws.size(),
               vertices = combined.vertices.size();
    const auto textures = combined.textures.size();
    re4::AppendRoomItemScene(combined, items);
    assert(combined.draws.size() == count + items.mesh.draws.size());
    for (std::size_t i = 0; i < items.mesh.draws.size(); ++i) {
      const auto &added = combined.draws[count + i];
      const auto &original = items.mesh.draws[i];
      assert(added.first == original.first + vertices / 3);
      assert(added.texture == original.texture + int(textures));
      assert(added.roomItemIndex == original.roomItemIndex &&
             added.roomItemId == original.roomItemId &&
             added.roomObjectSlot == -1);
    }
    std::cout << items.describe() << " · Texturen "
              << items.mesh.textures.size() << '\n';
    std::cout
        << "Original ITM BIN/TPL drop ownership, model-less pickups and deferred variants PASS\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
