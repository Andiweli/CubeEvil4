#include "inventory_case.h"
#include <cassert>
#include <iostream>

int main() {
  re4::InventoryCase c;
  c.resetGameStart();
  assert(c.width() == 10 && c.height() == 6 && c.equippedId() == 0x23);
  assert(c.count(0x23) == 1 && c.count(4) == 20 && c.count(5) == 1);
  assert(c.count(0x7c) == 1 && !c.item(3)->inCase());
  assert(c.item(0)->x == 0 && c.item(0)->y == 0 && c.item(0)->loaded == 10);
  assert(c.item(1)->x == 3 && c.item(1)->y == 0);
  assert(c.item(2)->x == 3 && c.item(2)->y == 1);
  // Native normalized cells match original saved half-cell center positions.
  assert(c.item(0)->x * 2 + 2 == 2 && c.item(0)->y * 2 + 1 == 1);
  assert(c.item(1)->x * 2 + 1 == 7 && c.item(1)->y * 2 == 0);
  assert(c.item(2)->x * 2 == 6 && c.item(2)->y * 2 + 1 == 3);
  assert(re4::OriginalItemFootprint(0x2c).width == 8 && re4::OriginalItemFootprint(0x2c).height == 2);
  assert(re4::OriginalWeaponCapacity(0x23) == 10 && re4::OriginalWeaponCapacity(0x2c) == 6);
  assert(re4::OriginalWeaponAmmo(0x23) == 4 && re4::OriginalWeaponAmmo(0x2c) == 0x18);
  assert(c.canAdd(0x2c, 1) && c.add(0x2c, 1));
  assert(c.item(4)->loaded == 6 && c.equip(4) && c.equippedId() == 0x2c);
  assert(!c.equip(1) && !c.discard(4));
  assert(c.equip(0) && c.discard(4) && !c.item(4));
  assert(c.add(0x2c, 1) && c.item(4)->id == 0x2c); // Stable reusable slot.
  const auto state = c.snapshotJson(123, "Ergebnis \"Test\"\n");
  assert(!c.move(4, 0, 0, 0));
  assert(!c.move(4, -1, 0, 0) && !c.move(4, 0, 0, 4));
  assert(c.snapshotJson(123, "Ergebnis \"Test\"\n") == state);
  assert(state.find("\\\"Test\\\"\\u000a") != std::string::npos);
  assert(c.synchronizeWeaponAmmo(0x23, 8, 20)); // Two shots remove loaded rounds only.
  assert(c.item(0)->loaded == 8 && c.count(4) == 20);
  assert(c.synchronizeWeaponAmmo(0x23, 10, 18)); // Reload removes exactly two reserve rounds.
  assert(c.item(0)->loaded == 10 && c.count(4) == 18);
  assert(!c.synchronizeWeaponAmmo(0x23, 11, 18));
  assert(!c.synchronizeWeaponAmmo(0x29, 6, 0));
  // Original case combine reloads from the selected stack in either order.
  re4::InventoryCase reload;
  reload.resetGameStart();
  assert(reload.synchronizeWeaponAmmo(0x23, 7, 20));
  const auto gunX = reload.item(0)->x, gunY = reload.item(0)->y;
  assert(reload.combine(0, 1));
  assert(reload.item(0)->loaded == 10 && reload.count(4) == 17 &&
         reload.equippedSlot() == 0 && reload.item(0)->x == gunX &&
         reload.item(0)->y == gunY);
  const auto fullGun = reload.snapshotJson(0);
  assert(!reload.combine(1, 0) && reload.snapshotJson(0) == fullGun);
  assert(reload.synchronizeWeaponAmmo(0x23, 9, 1) && reload.combine(1, 0));
  assert(reload.item(0)->loaded == 10 && reload.count(4) == 0 && !reload.item(1));
  assert(reload.add(0x18, 3) && reload.item(1)->id == 0x18);
  const auto wrongAmmo = reload.snapshotJson(0);
  assert(!reload.combine(0, 1) && reload.snapshotJson(0) == wrongAmmo);
  assert(c.add(0x18, 31)); // Two full original 15-round boxes + one.
  unsigned boxes = 0;
  for (const auto &i : c.items()) if (i.active && i.id == 0x18) { ++boxes; assert(i.count <= 15); }
  assert(boxes == 3 && c.count(0x18) == 31);
  assert(c.synchronizeWeaponAmmo(0x2c, 6, 25));
  boxes = 0;
  for (const auto &i : c.items()) if (i.active && i.id == 0x18) ++boxes;
  assert(boxes == 2 && c.count(0x18) == 25); // Smallest stack consumed first.
  assert(c.add(0xaf, 1) && c.add(0x57, 3));
  for (const auto &i : c.items()) if (i.active && (i.id == 0xaf || i.id == 0x57)) assert(!i.inCase());
  assert(c.canAdd(0x78, 30) && c.add(0x78, 30) && c.count(0x78) == 0);
  assert(!c.add(0x100, 1) && !c.add(4, 65536));
  re4::InventoryCase mix;
  assert(mix.add(6, 1) && mix.add(0x19, 1));
  assert(mix.combine(0, 1) && mix.count(6) == 0 && mix.count(0x19) == 0 && mix.count(0x14) == 1);
  assert(!mix.consumeHealing(0, 1200, 1200) && mix.count(0x14) == 1); // ∞ HP never wastes an item.
  assert(mix.consumeHealing(0, 500, 1200) && mix.count(0x14) == 0);
  assert(mix.add(6, 1) && mix.add(6, 1) && mix.combine(0, 1));
  assert(mix.count(0x12) == 1 && re4::OriginalHealingAmount(0x12) == 1300);
  assert(!mix.combine(0, 0));
  re4::InventoryCase full;
  for (unsigned n = 0; n < 30; ++n) assert(full.add(1, 1));
  const auto before = full.snapshotJson(0);
  assert(!full.canAdd(6, 1) && !full.add(6, 1));
  assert(full.snapshotJson(0) == before); // Failed pickup never partially changes the case.
  assert(full.add(0xaf, 1)); // Files do not consume grid space.
  assert(full.discard(0) && full.add(6, 1));
  re4::InventoryCase stacks;
  assert(stacks.add(4, 49));
  for (unsigned n = 0; n < 29; ++n) assert(stacks.add(1, 1));
  const auto stackBefore = stacks.snapshotJson(0);
  assert(!stacks.canAdd(4, 2) && !stacks.add(4, 2)); // Even partial stacking is rolled back.
  assert(stacks.snapshotJson(0) == stackBefore && stacks.count(4) == 49);
  assert(stacks.add(4, 1) && stacks.count(4) == 50);
  re4::InventoryCase xl(3);
  assert(xl.width() == 15 && xl.height() == 8);
  re4::InventoryCase rotate;
  assert(rotate.add(0x2c, 1));
  assert(!rotate.move(0, 0, 0, 1)); // Original small case is only six cells high.
  assert(rotate.move(0, 2, 4, 2));
  bool rejected = false;
  try { re4::InventoryCase invalid(4); } catch (...) { rejected = true; }
  assert(rejected);
  // Deterministic mixed operation stress checks the public packing contract,
  // not the implementation's private occupancy algorithm.
  re4::InventoryCase stress;
  stress.resetGameStart();
  unsigned rng = 0x19990419;
  for (unsigned n = 0; n < 5000; ++n) {
    rng = rng * 1664525u + 1013904223u;
    const auto snapshot = stress.snapshotJson(0);
    bool changed = false;
    const unsigned operation = (rng >> 24) % 6;
    const unsigned slot = (rng >> 8) % 48;
    if (operation == 0) changed = stress.add((rng & 1) ? 4 : 6, (rng & 1) ? 11 : 1);
    else if (operation == 1) changed = stress.move(slot, int((rng >> 16) % 13) - 1, int((rng >> 20) % 9) - 1, rng & 3);
    else if (operation == 2) changed = stress.discard(slot);
    else if (operation == 3) changed = stress.combine(slot, (rng >> 2) % 48);
    else if (operation == 4) changed = stress.consume(4, 3);
    else changed = stress.equip(slot);
    if (!changed) assert(stress.snapshotJson(0) == snapshot);
    bool occupied[6][10]{};
    for (const auto &i : stress.items()) {
      if (!i.active || !i.inCase()) continue;
      auto footprint = re4::OriginalItemFootprint(i.id);
      unsigned w = (i.rotation & 1) ? footprint.height : footprint.width;
      unsigned h = (i.rotation & 1) ? footprint.width : footprint.height;
      assert(i.x >= 0 && i.y >= 0 && unsigned(i.x) + w <= 10 && unsigned(i.y) + h <= 6);
      for (unsigned y = i.y; y < unsigned(i.y) + h; ++y)
        for (unsigned x = i.x; x < unsigned(i.x) + w; ++x) {
          assert(!occupied[y][x]); occupied[y][x] = true;
        }
    }
  }
  std::cout << "Case source start layout, capacities, stacking, transactions, equipment, ammo, herbs, files and packing PASS\n";
}
