#include "asset_reader.h"
#include "room_objects.h"
#include <cassert>
#include <iostream>
#include <map>
#include <stdexcept>

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
    const auto layer = re4::BuildRoomObjects(
        room, core.copy(eff - core.entries().data()), roomId);
    std::cout << layer.describe() << '\n';
    for (const auto &warning : layer.warnings)
      std::cout << warning << '\n';
    if (roomId == 0x400) {
      assert(layer.placements.size() == 33);
      assert(layer.rendered == 33 && layer.unsupported == 0);
      assert(layer.doors == 6 && layer.windows == 5 && layer.ladders == 4);
      assert(layer.containers == 18 && layer.lights == 0);
      assert(layer.hiddenSlots.empty() && layer.scriptObjects.empty());
    } else {
      assert(layer.placements.size() == 28);
      assert(layer.rendered == 25 && layer.unsupported == 0);
      assert(layer.doors == 4 && layer.windows == 5 && layer.ladders == 0);
      assert(layer.containers == 16 && layer.lights == 0);
      assert(layer.hiddenSlots == (std::vector<unsigned>{5, 6, 7}));
      assert(layer.scriptObjects.size() == 2);
      const auto &pyre = layer.scriptObjects[0];
      assert(pyre.kind == re4::RoomObjectLayer::ScriptObject::VillagePyre);
      assert(pyre.binOccurrence == 1 && pyre.tplOccurrence == 1);
      assert(pyre.parentSlot == -1);
      assert(pyre.position == (std::array<float, 3>{-2880, 50, 2810}));
      assert(pyre.rotation == (std::array<float, 3>{0, -1.5707964f, 0}));
      const auto &lock = layer.scriptObjects[1];
      assert(lock.kind == re4::RoomObjectLayer::ScriptObject::DoorLock);
      assert(lock.binOccurrence == 0 && lock.tplOccurrence == 0);
      assert(lock.parentSlot == 11);
      std::map<unsigned, std::size_t> rendered;
      for (const auto &draw : layer.mesh.draws) {
        rendered[draw.object] += draw.count / 3;
        assert(draw.roomItemIndex == -1 && draw.roomItemId == -1);
        if (draw.object >= 64)
          assert(draw.roomObjectSlot == -1);
        else
          assert(draw.roomObjectSlot == int(draw.object));
        if (draw.object == 17)
          assert(draw.sortCenter == (std::array<float, 3>{6656, 902, 8925}));
      }
      assert(rendered.size() == 27 && rendered[64] && rendered[65]);
      for (const auto &placement : layer.placements) {
        const bool hidden =
            placement.slot == 5 || placement.slot == 6 || placement.slot == 7;
        assert(bool(rendered.count(placement.slot)) == !hidden);
        if (placement.slot == 17) {
          const auto rawEts =
              room.copy(room.find("ETS") - room.entries().data());
          const re4::AssetReader reader{rawEts};
          assert(placement.position[0] != 6656.f);
          assert(reader.f32(placement.recordOffset + 28) ==
                 placement.position[0]);
        }
      }
      std::cout << "Original r101 initial layout, hidden ladders, script rack, "
                   "pyre and padlock checks PASS\n";
    }
    assert(layer.mesh.vertices.size() == layer.mesh.triangles * 9);
    assert(layer.mesh.normals.size() == layer.mesh.vertices.size());
    assert(layer.mesh.uv.size() == layer.mesh.triangles * 6);
    assert(layer.mesh.colors.size() == layer.mesh.triangles * 12);
    assert(layer.mesh.texturedTriangles == layer.mesh.triangles);
    assert(layer.mesh.fallbackTriangles == 0);
    assert(layer.mesh.unresolvedAlphaTriangles == 0);
    auto scene = layer.mesh;
    const auto draws = scene.draws.size();
    const auto vertices = scene.vertices.size();
    const auto textures = scene.textures.size();
    re4::AppendRoomObjectScene(scene, layer);
    assert(scene.vertices.size() == vertices * 2);
    assert(scene.draws.size() == draws * 2);
    assert(scene.textures.size() == textures * 2);
    for (std::size_t i = 0; i < draws; ++i) {
      const auto &a = scene.draws[i], &b = scene.draws[draws + i];
      assert(b.first == a.first + vertices / 3);
      assert(b.texture == a.texture + static_cast<int>(textures));
      assert(b.work == 0xfe && b.sky == -1 && b.water == -1);
      assert(b.roomObjectSlot == a.roomObjectSlot &&
             b.roomItemIndex == a.roomItemIndex &&
             b.roomItemId == a.roomItemId);
      if (a.alphaTexture >= 0)
        assert(b.alphaTexture == a.alphaTexture + static_cast<int>(textures));
    }
    std::cout << "Original ETS/ETM model+palette checks PASS\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
