#include "object_actions.h"
#include "room_pickups.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>

using Bytes = std::vector<std::uint8_t>;
namespace {
void half(Bytes &bytes, std::size_t offset, unsigned value) {
  bytes.at(offset) = value >> 8;
  bytes.at(offset + 1) = value;
}
void word(Bytes &bytes, std::size_t offset, std::uint32_t value) {
  half(bytes, offset, value >> 16);
  half(bytes, offset + 2, value);
}
void real(Bytes &bytes, std::size_t offset, float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  word(bytes, offset, bits);
}
void rejects(const std::function<void()> &action) {
  bool rejected = false;
  try { action(); } catch (const std::runtime_error &) { rejected = true; }
  assert(rejected);
}
Bytes contents() {
  Bytes bytes(16 + 2 * 156);
  word(bytes, 0, 0x49544100);
  half(bytes, 4, 0x105);
  half(bytes, 6, 2);
  for (unsigned index = 0; index < 2; ++index) {
    const auto p = 16 + index * 156;
    bytes[p + 0x34] = 3;
    bytes[p + 0x35] = 3;
    bytes[p + 0x36] = index;
    bytes[p + 0x37] = 1;
    bytes[p + 0x38] = 8;
    bytes[p + 0x39] = 1;
    bytes[p + 0x44] = 8;
    bytes[p + 0x46] = 2;
    bytes[p + 0x47] = index + 20;
    bytes[p + 0x4a] = 0x28;
    half(bytes, p + 0x78, index ? 6 : 0x1005);
    half(bytes, p + 0x7e, index + 1);
    bytes[p + 0x80] = 2;
  }
  return bytes;
}
re4::RoomArchive room(const Bytes &ita) {
  Bytes bytes(32 + ita.size());
  word(bytes, 0, 1);
  word(bytes, 16, 32);
  word(bytes, 20, 0x49544100);
  std::copy(ita.begin(), ita.end(), bytes.begin() + 32);
  return re4::RoomArchive(bytes);
}
std::vector<re4::RoomObjectPlacement> placements() {
  Bytes bytes(16 + 2 * 40);
  half(bytes, 0, 2);
  for (unsigned index = 0; index < 2; ++index) {
    const auto p = 16 + index * 40;
    half(bytes, p, 1);
    half(bytes, p + 2, index + 20);
    real(bytes, p + 36, index * 2000.f);
  }
  return re4::ReadRoomObjectPlacements(bytes);
}
re4::PlayerShot shot(unsigned serial) {
  return {serial, {0, 175, -1000}, {0, 175, 3000}};
}
re4::Sat wall(float height = 5000) {
  Bytes empty(20);
  empty[0] = 0x20;
  re4::Sat sat(empty);
  sat.vertices = {{-5000, -5000, -500}, {-5000, height, -500},
                  {5000, height, -500}, {5000, -5000, -500}};
  sat.normals = {{0, 0, -1}};
  sat.polygons = {{{0, 1, 2}, {}, 0, 0, 0},
                   {{0, 2, 3}, {}, 0, 0, 0}};
  sat.wall = 2;
  return sat;
}
} // namespace
int main() {
  const auto ita = contents();
  const auto authored = placements();
  re4::ObjectActions actions(0x400, room(ita), authored);
  assert(actions.objects().size() == 2 && actions.states().size() == 2);
  assert(actions.objects()[0].item.id == 4);
  assert(actions.objects()[0].item.count == 20);
  assert(actions.objects()[0].item.sourceId == 0x1005);
  assert(actions.objects()[1].item.id == 6);
  assert(actions.objects()[1].item.count == 1);
  assert(actions.objects()[0].item.position == (re4::CollisionVec{0, 50, 0}));
  assert(actions.objects()[0].shapes[0].radius == 150);
  assert(actions.objects()[0].shapes[0].height == 350);
  assert(actions.objects()[0].shapes[0].extent == 325);
  assert(!actions.pickupTarget({0, 0, -550}, 0));
  const auto ray = shot(1);
  auto hit = actions.target(ray.from, ray.to);
  assert(hit && hit->slot == 20 && hit->hit.cross[2] == -325);
  assert(hit->hit.lenSq == 675 * 675);
  assert(!actions.target(ray.from, ray.to, 1, hit->hit.lenSq - 1));
  // Original knife override ignores a smaller caller/EAT distance bound.
  assert(actions.target(ray.from, ray.to, 0x10, 0));
  assert(actions.shoot(ray));
  assert(actions.states()[0].hp == 0 && actions.states()[0].broken);
  assert(actions.states()[0].itemActive && !actions.states()[0].itemTaken);
  assert(!actions.shoot(ray)); // Same serial cannot break the next crate.
  assert(actions.states()[1].hp == 1000);
  assert(!actions.pickup({0, 0, -550}, 0, false));
  // Cylinder radius and vertical bounds retain source strictness.
  assert(actions.pickupTarget({1499, 0, -550}, 0));
  assert(!actions.pickupTarget({1500, 0, -550}, 0));
  assert(actions.pickupTarget({0, -2300, -550}, 0));
  assert(!actions.pickupTarget({0, -2301, -550}, 0));
  assert(actions.pickupTarget({0, 799, -550}, 0));
  assert(!actions.pickupTarget({0, 800, -550}, 0));
  assert(!actions.pickupTarget({0, 0, -550}, 0,
                               re4::CollisionVec{0, 250, -1500}));
  const auto beforePickup = actions.snapshot();
  const auto pickup = actions.pickup({0, 0, -550}, 0, true);
  assert(pickup && pickup->id == 4 && pickup->count == 20);
  assert(actions.states()[0].itemTaken && !actions.states()[0].itemActive);
  assert(!actions.pickup({0, 0, -550}, 0, true));
  assert(actions.inventory().size() == 1 && actions.inventory()[0].count == 20);
  auto ammoSnapshot = actions.snapshot();
  const auto ammoChecksum = ammoSnapshot.checksum;
  assert(re4::ObjectActions::consumeSnapshotItem(ammoSnapshot, 4, 7));
  assert(ammoSnapshot.inventory[0].count == 13);
  assert(ammoSnapshot.checksum != ammoChecksum);
  re4::ObjectActions inactiveAmmo(0x400, room(ita), authored);
  inactiveAmmo.restore(ammoSnapshot);
  assert(inactiveAmmo.inventory()[0].count == 13);
  assert(!actions.consumeItem(4, 21) && !actions.consumeItem(4, 0));
  assert(actions.consumeItem(4, 17) && actions.inventory()[0].count == 3);
  assert(actions.consumeItem(4, 3) && actions.inventory().empty());
  actions.restore(beforePickup);
  assert(actions.states()[0].itemActive && actions.inventory().empty());
  re4::PlayerMeleeEvent knife;
  knife.kind = re4::PlayerMeleeKind::Knife;
  knife.serial = 1;
  knife.rays = {{ray.from, ray.to}, {ray.from, ray.to}};
  const auto knifeHits = actions.knife(knife);
  assert(knifeHits.size() == 1 && knifeHits[0].slot == 21);
  assert(actions.knife(knife).empty());
  assert(actions.states()[1].itemActive);
  const auto herb = actions.pickup({0, 0, 1450}, 0, true);
  assert(herb && herb->id == 6 && herb->count == 1);
  const auto state = actions.snapshot();
  re4::ObjectActions reopened(0x400, room(ita), authored);
  reopened.restore(state);
  assert(reopened.snapshot().checksum == state.checksum);
  assert(reopened.states()[1].itemTaken && reopened.inventory()[0].id == 6);
  auto inactive = state;
  assert(!re4::ObjectActions::consumeSnapshotItem(inactive, 6, 2));
  assert(!re4::ObjectActions::consumeSnapshotItem(inactive, 6, 0));
  assert(!re4::ObjectActions::consumeSnapshotItem(inactive, 0x18, 1));
  assert(inactive.checksum == state.checksum);
  assert(inactive.inventory[0].count == 1);
  assert(re4::ObjectActions::consumeSnapshotItem(inactive, 6, 1));
  assert(inactive.inventory.empty() && inactive.checksum != state.checksum);
  reopened.restore(inactive);
  assert(reopened.inventory().empty() && reopened.states()[1].itemTaken);
  assert(!re4::ObjectActions::consumeSnapshotItem(inactive, 6, 1));
  reopened.restore(state);
  for (unsigned corruption = 0; corruption < 6; ++corruption) {
    auto malformed = state;
    switch (corruption) {
    case 0: ++malformed.room; break;
    case 1: ++malformed.profile; break;
    case 2: malformed.states.pop_back(); break;
    case 3: malformed.states[0].hp = 1; break;
    case 4: ++malformed.inventory[0].count; break;
    case 5: ++malformed.checksum; break;
    }
    rejects([&] { reopened.restore(malformed); });
    assert(reopened.snapshot().checksum == state.checksum);
    const auto originalChecksum = malformed.checksum;
    const auto originalInventory = malformed.inventory;
    assert(!re4::ObjectActions::consumeSnapshotItem(malformed, 6, 1));
    assert(malformed.checksum == originalChecksum);
    assert(malformed.inventory.size() == originalInventory.size());
    for (std::size_t i = 0; i < originalInventory.size(); ++i) {
      assert(malformed.inventory[i].id == originalInventory[i].id);
      assert(malformed.inventory[i].count == originalInventory[i].count);
    }
  }
  auto differentPlacements = authored;
  differentPlacements[0].position[0] += 1;
  re4::ObjectActions moved(0x400, room(ita), differentPlacements);
  rejects([&] { moved.restore(state); });
  for (std::size_t length = 1; length < ita.size(); ++length)
    rejects([&] {
      re4::ObjectActions incomplete(
          0x400, room(Bytes(ita.begin(), ita.begin() + length)), authored);
    });
  for (unsigned corruption = 0; corruption < 11; ++corruption) {
    auto malformed = ita;
    const auto p = 16u;
    switch (corruption) {
    case 0: half(malformed, 4, 0x104); break;
    case 1: half(malformed, 6, 257); break;
    case 2: malformed[16 + 156 + 0x36] = 0; break;
    case 3: malformed[16 + 156 + 0x47] = 20; break;
    case 4: half(malformed, p + 0x78, 0x1006); break;
    case 5: word(malformed, p + 0x4c, 0x1234); break;
    case 6: malformed[p + 0x39] = 2; break;
    case 7: malformed[p + 0x82] = 2; break;
    case 8: real(malformed, p + 0x88, -1); break;
    case 9: real(malformed, p + 0x8c,
                 std::numeric_limits<float>::quiet_NaN()); break;
    case 10: malformed[p + 0x44] = 16; break;
    }
    if (corruption == 4 || corruption == 5 || corruption == 6 ||
        corruption == 7 || corruption == 10) {
      re4::ObjectActions partial(0x20b,room(malformed),authored);
      assert(partial.objects().size()==1 && partial.states().size()==1);
      assert(partial.objects()[0].placement.slot==21);
      assert(partial.shoot(shot(401)) && partial.states()[0].broken);
    } else
      rejects([&] { re4::ObjectActions invalid(0x400, room(malformed), authored); });
  }
  // Original emBoxInit itemNo=-1 permits an unbound empty container. Its
  // damage/debris must remain usable when another record is script-owned.
  re4::ObjectActions emptyBoxes(0x20b,room({}),authored);
  assert(emptyBoxes.objects().size()==2 && emptyBoxes.shoot(shot(402)));
  assert(emptyBoxes.states()[0].broken && !emptyBoxes.states()[0].itemActive);
  assert(emptyBoxes.drainEvents().size()==1);
  auto unrelated=ita;unrelated[16+0x47]=63;
  re4::ObjectActions unbound(0x20b,room(unrelated),authored);
  assert(unbound.objects().size()==2 && !unbound.objects()[0].hasItem);
  assert(unbound.shoot(shot(403)) && !unbound.states()[0].itemActive);
  // Item counts follow cItemMgr::get across the full original itemInfo
  // table. Money returns before stack clamping; attachments are type9.
  for (auto tuple : {std::array<unsigned,3>{3,7,1},
                     std::array<unsigned,3>{0x3f,7,1},
                     std::array<unsigned,3>{4,250,50},
                     std::array<unsigned,3>{0x6a,0,50},
                     std::array<unsigned,3>{0x79,0,30},
                     std::array<unsigned,3>{0x79,2500,2500}}) {
    auto items=ita;half(items,16+0x78,tuple[0]);half(items,16+0x7c,tuple[1]);
    re4::ObjectActions source(0x20b,room(items),authored);
    assert(source.objects()[0].item.id==tuple[0]);
    assert(source.objects()[0].item.count==tuple[2]);
    items[16+0x46]=0;items[16+156+0x46]=0;
    re4::ScenarioState state;
    re4::RoomPickups loose(0x20b,room(items),state);
    assert(loose.items().size()==2 && loose.items()[0].count==tuple[2]);
    const auto taken=loose.take(0);
    assert(taken && taken->count==tuple[2]);
    assert(state.room(0x20b).items.test(1) && !state.room(0x20b).itemsFound.test(1));
    if (tuple[0]==0x79) assert(taken->pesetas==tuple[2]*100 && state.carriedItems.empty());
    else assert(state.carriedItems.at(tuple[0])==tuple[2]);
  }
  // Mercenaries time/score items require the mode manager. They must not
  // become ordinary inventory pickups in campaign exploration.
  for (auto id : {0x73u,0x75u}) {
    auto modeItem=ita;half(modeItem,16+0x78,id);
    re4::ObjectActions partial(0x20b,room(modeItem),authored);
    assert(partial.objects().size()==1 && partial.objects()[0].placement.slot==21);
    modeItem[16+0x46]=0;modeItem[16+156+0x46]=0;
    re4::ScenarioState state;re4::RoomPickups loose(0x20b,room(modeItem),state);
    assert(loose.items().size()==1 && loose.items()[0].id==6);
  }
  // Global itemFlag takes precedence over autoFlag and remains authoritative
  // when a different room has an already-active copy or restores its drop.
  auto shared=ita;half(shared,16+0x78,0x5e);half(shared,16+0x7a,7);
  re4::ScenarioState scenario;
  re4::ObjectActions roomA(0x103,room(shared),authored,nullptr,&scenario);
  re4::ObjectActions roomB(0x113,room(shared),authored,nullptr,&scenario);
  assert(roomA.shoot(shot(610)) && roomB.shoot(shot(610)));
  assert(roomA.itemVisible(0) && roomB.itemVisible(0));
  const auto stale=roomB.snapshot();
  assert(!roomA.pickup({0,0,-550},0,false));
  assert(!scenario.globalItems.test(7) && !scenario.globalItemsFound.test(7));
  assert(roomA.takePickup(0));
  assert(scenario.globalItems.test(7) && !scenario.globalItemsFound.test(7));
  assert(!scenario.room(0x103).items.test(1) && !scenario.room(0x103).itemsFound.test(1));
  assert(!roomB.itemVisible(0) && !roomB.pickupTarget({0,0,-550},0) && !roomB.takePickup(0));
  roomB.restore(stale);
  assert(!roomB.itemVisible(0) && !roomB.takePickup(0) && roomB.inventory().empty());
  re4::ObjectActions reloaded(0x113,room(shared),authored,nullptr,&scenario);
  assert(reloaded.shoot(shot(611)) && !reloaded.itemVisible(0) && !reloaded.takePickup(0));
  assert(reloaded.states()[0].broken && reloaded.drainEvents().size()==1);
  // Room-local auto flags with the same number are independent per room.
  re4::ObjectActions localA(0x10a,room(ita),authored,nullptr,&scenario);
  re4::ObjectActions localB(0x10b,room(ita),authored,nullptr,&scenario);
  assert(localA.shoot(shot(612)) && localB.shoot(shot(612)));
  assert(localA.takePickup(0));
  assert(scenario.room(0x10a).items.test(1) && !scenario.room(0x10a).itemsFound.test(1));
  assert(!scenario.room(0x10b).items.test(1) && localB.itemVisible(0));
  scenario.room(0x10b).itemsFound.set(1);
  assert(localB.takePickup(0) && scenario.room(0x10b).itemsFound.test(1));
  auto freeShared=shared;freeShared[16+0x46]=0;freeShared[16+156+0x46]=0;
  re4::ScenarioState freeFlags;
  re4::RoomPickups freeA(0x103,room(freeShared),freeFlags),freeB(0x113,room(freeShared),freeFlags);
  assert(freeA.take(0));
  assert(freeFlags.globalItems.test(7) && !freeFlags.globalItemsFound.test(7));
  assert(!freeFlags.room(0x103).items.test(1) && !freeB.take(0));
  freeFlags.room(0x103).itemsFound.set(2);
  assert(freeA.take(1) && freeFlags.room(0x103).items.test(2) && freeFlags.room(0x103).itemsFound.test(2));
  re4::ObjectActions emptySaved(0x10a,room({}),authored,nullptr,&scenario);
  assert(emptySaved.shoot(shot(613)) && !emptySaved.itemVisible(0));
  assert(!emptySaved.takePickup(0) && emptySaved.drainEvents().size()==1);
  auto malformedPlacements = authored;
  malformedPlacements[1].slot = 20;
  rejects([&] {
    re4::ObjectActions invalid(0x400, room(ita), malformedPlacements);
  });
  assert(re4::ObjectActions(0x400, room(ita), {}).objects().empty());
  rejects([&] {
    actions.target({std::numeric_limits<double>::quiet_NaN(), 0, 0}, ray.to);
  });
  rejects([&] { actions.target(ray.from, ray.to, 0xff); });
  assert(!actions.pickupTarget({0, 0, 0},
                               std::numeric_limits<double>::quiet_NaN()));
  // Actual EAT geometry blocks a bullet before the crate and all three item
  // visibility traces. Knife keeps the original cEmBox EAT exception.
  const auto blockingSat = wall();
  const re4::SatCollision blocking(blockingSat, re4::SatQueryMode::Effect);
  re4::ObjectActions occluded(0x400, room(ita), authored, &blocking);
  assert(!occluded.shoot(shot(1)));
  assert(occluded.states()[0].hp == 1000);
  assert(occluded.target(ray.from, ray.to, 0x10));
  assert(occluded.knife(knife).size() == 2);
  assert(!occluded.pickupTarget({0, 0, -550}, 0));
  const auto shortSat = wall(500);
  const re4::SatCollision shortWall(shortSat, re4::SatQueryMode::Effect);
  re4::ObjectActions visibleOverWall(0x400, room(ita), authored, &shortWall);
  const auto knifeHit = visibleOverWall.target(ray.from, ray.to, 0x10);
  assert(knifeHit && visibleOverWall.applyKnife(*knifeHit, 1));
  assert(visibleOverWall.pickupTarget({0, 0, -550}, 0));
  // The item's source IDs map through the actual Leon ammo cases, never to
  // invented handgun ammunition for a weapon that uses another caliber.
  for (auto pair : {std::pair<unsigned, unsigned>{0x1003, 15},
                    {0x1004, 10}, {0x1005, 20}}) {
    auto mapped = ita;
    half(mapped, 16 + 0x78, pair.first);
    re4::ObjectActions source(0x400, room(mapped), authored);
    assert(source.objects()[0].item.count == pair.second);
    assert(source.objects()[0].item.id == (pair.first == 0x1005 ? 4u : 0x18u));
  }

  // M26: the authored non-rolling red barrels use a single 330/1250/330
  // cube; break starts once, damage follows two source ticks, and every
  // chained barrel retains a fresh two ticks independent of slot order.
  auto bombs = authored;
  bombs.resize(3);
  for (unsigned i = 0; i < bombs.size(); ++i) {
    bombs[i].id = 0x2d;
    bombs[i].slot = i + 39;
    bombs[i].position = {0, 0, float(i == 2 ? 11000 : i * 5000)};
  }
  re4::ObjectActions explosions(0x400, room({}), bombs);
  assert(explosions.objects().size() == 3);
  assert(explosions.objects()[0].explosive && !explosions.objects()[0].hasItem);
  assert(explosions.objects()[0].shapes.size() == 1);
  assert(explosions.objects()[0].shapes[0].radius == 330);
  assert(explosions.shoot({77, {0, 625, -1000}, {0, 625, 12000}}));
  assert(explosions.states()[0].broken && !explosions.states()[0].itemActive);
  auto starts = explosions.drainEvents();
  assert(starts.size() == 1 && starts[0].explosive);
  assert(starts[0].soundBlock == 6 && starts[0].soundNo == 0x56);
  assert(explosions.drainEvents().empty());
  assert(explosions.tick().empty() && explosions.states()[0].bombWait == 1);
  const auto armed = explosions.snapshot();
  re4::ObjectActions resumed(0x400, room({}), bombs);
  resumed.restore(armed);
  assert(resumed.drainEvents().empty());
  const auto first = resumed.tick();
  assert(first.size() == 1 && first[0].slot == 39);
  assert(first[0].position == (re4::CollisionVec{0, 500, 0}));
  assert(first[0].radius == 6000 && first[0].weaponNo == 0x13);
  assert(resumed.states()[1].broken && resumed.states()[1].bombWait == 2);
  assert(!resumed.states()[2].broken);
  starts = resumed.drainEvents();
  assert(starts.size() == 1 && starts[0].slot == 40 && starts[0].breakKind == 2);
  assert(resumed.tick().empty());
  assert(resumed.tick().size() == 1);
  assert(resumed.states()[2].broken && resumed.states()[2].bombWait == 2);
  assert(resumed.tick().empty());
  assert(resumed.tick().size() == 1);
  assert(resumed.tick().empty());
  // Original scenario policy: outer object hits are occluded, while the
  // inner30% sphere deliberately bypasses the scenario ray.
  auto behindWall=bombs;behindWall.resize(2);behindWall[1].position[2]=-3000;
  re4::ObjectActions blockedChain(0x400,room({}),behindWall,&blocking);
  assert(blockedChain.shoot({1,{0,625,500},{0,625,-1000}}));
  assert(blockedChain.tick().empty());assert(blockedChain.tick().size()==1);
  assert(!blockedChain.states()[1].broken);
  behindWall[1].position[2]=-1000;
  re4::ObjectActions nearChain(0x400,room({}),behindWall,&blocking);
  assert(nearChain.shoot({1,{0,625,500},{0,625,-1000}}));
  nearChain.tick();nearChain.tick();assert(nearChain.states()[1].bombWait==2);
  // Source emSphereAtCk clamps blast vertical r2 even though a6000-unit
  // sphere overlaps an upstairs crate. The sixface cube boundary is closed.
  auto layered=authored;layered[0].position={0,3000,1000};
  layered[1].position={0,0,6325};layered.push_back(bombs[0]);
  re4::ObjectActions layeredBlast(0x400,room(ita),layered);
  assert(layeredBlast.shoot({1,{0,625,-1000},{0,625,1000}}));
  layeredBlast.tick();layeredBlast.tick();
  assert(!layeredBlast.states()[0].broken&&layeredBlast.states()[1].broken);
  layered[1].position[2]=6325.1f;
  re4::ObjectActions outsideCube(0x400,room(ita),layered);
  assert(outsideCube.shoot({1,{0,625,-1000},{0,625,1000}}));
  outsideCube.tick();outsideCube.tick();assert(!outsideCube.states()[1].broken);
  // Original player blast inner life-damage vs outer damage9 stagger.
  assert(resumed.playerBlastHit(first[0], {2500, 500, 0})->inner);
  assert(resumed.playerBlastHit(first[0], {2500.1, 500, 0})->baseDamage == 0);
  assert(resumed.playerBlastHit(first[0], {6000, 500, 0}));
  assert(!resumed.playerBlastHit(first[0], {6000.1, 500, 0}));
  re4::ObjectActions walledBomb(0x400, room({}), bombs, &blocking);
  assert(!walledBomb.playerBlastHit(first[0], {0, 500, -1000}));
  assert(walledBomb.playerBlastHit(first[0], {0, 500, -3000}));
  // The cEmBox knife-through-EAT exception does not apply to cEmBarrel.
  assert(!walledBomb.target({0, 625, -1000}, {0, 625, 1000}, 0x10));
  // Original ctrlbit0 leaves the authored ITA position intact.
  auto positioned = ita;
  positioned[16 + 0x84] = 1;
  real(positioned, 16 + 0x5c, 123);
  real(positioned, 16 + 0x60, 456);
  real(positioned, 16 + 0x64, 789);
  re4::ObjectActions authoredItem(0x101, room(positioned), authored);
  assert(authoredItem.objects()[0].item.position ==
         (re4::CollisionVec{123, 456, 789}));
  // System random contents are resolved at break, not room load. Restoring
  // before/after a break keeps the source RNG and never rerolls a supply.
  auto randomIta = ita;
  half(randomIta, 16 + 0x78, 0x1001);
  re4::ObjectActions randomDrop(0x101, room(randomIta), authored);
  assert(!randomDrop.states()[0].itemResolved);
  assert(randomDrop.objects()[0].item.id == 0);
  re4::ObjectDropContext context;
  context.handgunAmmo = 40;
  context.health = 500;
  randomDrop.setDropContext(context);
  const auto beforeRandom = randomDrop.snapshot();
  assert(randomDrop.shoot(shot(31)));
  assert(randomDrop.states()[0].itemResolved);
  const auto afterRandom = randomDrop.snapshot();
  randomDrop.restore(beforeRandom);
  assert(randomDrop.shoot(shot(31)));
  assert(randomDrop.snapshot().checksum == afterRandom.checksum);
  randomDrop.restore(afterRandom);
  assert(randomDrop.snapshot().checksum == afterRandom.checksum);
  auto corruptRandom = afterRandom;
  ++corruptRandom.randomState;
  rejects([&] { randomDrop.restore(corruptRandom); });
  auto forcedIta = randomIta;
  half(forcedIta, 16 + 0x78, 0x1002);
  re4::ObjectActions forcedDrop(0x101, room(forcedIta), authored);
  context.handgunAmmo = 500;
  context.recoveryPoints = 4;
  forcedDrop.setDropContext(context);
  assert(forcedDrop.shoot(shot(1)));
  assert(forcedDrop.states()[0].itemId == 0x78);
  assert(forcedDrop.states()[0].itemCount >= 50 &&
         forcedDrop.states()[0].itemCount <= 990);
  // Shotgun registration is per target. Source EmMgr applies box damage
  // only after all13 pellet intersections have been registered.
  {
    auto burst=shot(901);burst.weaponItemId=0x2c;
    burst.pellets.assign(13,{burst.from,burst.to});
    re4::ObjectActions front(0x400,room(ita),authored);
    assert(front.shoot(burst));
    assert(front.states()[0].broken&&!front.states()[1].broken);
    const auto events=front.drainEvents();
    assert(events.size()==1&&events[0].breakKind==1);
    assert(!front.shoot(burst)&&!front.states()[1].broken);
    re4::ObjectActions distant(0x400,room(ita),authored);
    auto far=shot(902);far.weaponItemId=0x2c;far.from[2]=-6500;
    assert(distant.shoot(far));
    const auto farEvents=distant.drainEvents();
    assert(farEvents.size()==1&&farEvents[0].breakKind==0);
    re4::ObjectActions multiple(0x400,room(ita),authored);
    auto wide=burst;wide.serial=903;
    wide.pellets[1]={re4::CollisionVec{1000,175,2000},re4::CollisionVec{-1000,175,2000}};
    assert(multiple.shoot(wide));
    assert(multiple.states()[0].broken&&multiple.states()[1].broken);
    const auto multiEvents=multiple.drainEvents();
    assert(multiEvents.size()==2&&multiEvents[0].breakKind==1&&multiEvents[1].breakKind==1);
    const auto multiSaved=multiple.snapshot();
    re4::ObjectActions reopenedBurst(0x400,room(ita),authored);reopenedBurst.restore(multiSaved);
    assert(reopenedBurst.snapshot().checksum==multiSaved.checksum&&!reopenedBurst.shoot(wide));
    auto corrupt=multiSaved;corrupt.states[0].shotSerial++;
    rejects([&]{reopenedBurst.restore(corrupt);});
    re4::ObjectActions pistol(0x400,room(ita),authored);
    auto punisher=shot(904);punisher.weaponItemId=0x23;
    assert(pistol.shoot(punisher)&&pistol.drainEvents()[0].breakKind==0);
  }
  std::cout << "Original container shapes, serials, ITA drops, pickup bounds, "
               "inventory and atomic snapshots PASS\n";
}
