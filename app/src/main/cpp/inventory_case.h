#pragma once
#include <string>
#include <vector>

namespace re4 {
unsigned OriginalCaseCommandType(unsigned id,bool attached);
struct ItemFootprint { unsigned width{}, height{}; };
ItemFootprint OriginalItemFootprint(unsigned id);
unsigned OriginalWeaponCapacity(unsigned id);
unsigned OriginalWeaponAmmo(unsigned id);
unsigned OriginalHealingAmount(unsigned id);
const char *OriginalItemLabel(unsigned id);

struct CaseItem {
  unsigned slot{}, id{}, count{}, loaded{}, rotation{};
  int x{-1}, y{-1};
  bool active{};
  // Original type9: lv == 1, bullet == owning weapon's slot. The part stays
  // a separate packed item, so attaching it never loses ammo or save identity.
  unsigned attachedTo{~0u};
  bool space{}, held{}; // transient puzzle boards, not savegame flags
  bool inCase() const { return x >= 0 && y >= 0 && !space && !held; }
  bool onBoard() const { return x >= 0 && y >= 0 && !held; }
};
// Native owner of session items. Source dimensions/stack limits/recipes come
// from game/puzzle.cpp and game/item.cpp; no 32-bit GC pointers survive here.
// Rectangle origins are normalized top-left cells instead of GC half-cell
// centers. The original 58 non-empty piece shapes are all solid rectangles.
class InventoryCase {
public:
  explicit InventoryCase(unsigned size = 0);
  void clear();
  void resetGameStart(unsigned handgunId = 0x23);
  void resetTestLoadout(unsigned loaded = 10, unsigned reserve = 60,
                        unsigned handgunId = 0x21);
  unsigned width() const;
  unsigned height() const;
  const std::vector<CaseItem> &items() const { return items_; }
  const CaseItem *item(unsigned slot) const;
  unsigned count(unsigned id) const;
  unsigned equippedId() const;
  unsigned weaponId(unsigned slot) const;
  unsigned equippedSlot() const { return equipped_; }
  unsigned size() const { return size_; }
  // Original save loading retains source slot numbers, gaps and placement.
  // Validation is transactional; invalid data leaves this case unchanged.
  void restoreSnapshot(unsigned size,const std::vector<CaseItem> &items,unsigned equippedSlot);
  bool canAdd(unsigned id, unsigned count) const;
  bool add(unsigned id, unsigned count);
  bool equip(unsigned slot);
  bool move(unsigned slot, int x, int y, unsigned rotation);
  bool discard(unsigned slot);
  bool puzzleTake(unsigned slot);
  // 0 blocked; 1 placed; 2 combined; 3 exchanged (new item in hand).
  unsigned puzzlePut(int x,int y,unsigned rotation,bool space);
  bool puzzleReturn();
  bool puzzleDiscardSpace();
  unsigned heldSlot() const { return held_; }
  bool puzzleUnsettled() const;

  bool consume(unsigned id, unsigned count);
  bool combine(unsigned first, unsigned second);
  bool detach(unsigned slot);
  bool consumeHealing(unsigned slot, unsigned life, unsigned maxLife);
  // After an original weapon action/reload, mirror loaded rounds and consume
  // the changed reserve from case stacks. Unexpected increases must fit the
  // case; a failed transaction leaves every slot unchanged.
  bool synchronizeWeaponAmmo(unsigned id, unsigned loaded, unsigned reserve);
  std::string snapshotJson(unsigned pesetas, const std::string &message = {}) const;
private:
  unsigned size_{}, equipped_{~0u};
  std::vector<CaseItem> items_;
  unsigned held_{~0u};
  CaseItem handOrigin_;
  bool fitsBoard(unsigned ignore,unsigned id,int x,int y,unsigned rotation,bool space) const;
  bool addImpl(unsigned id, unsigned count);
  bool fits(unsigned ignore, unsigned id, int x, int y, unsigned rotation) const;
  bool place(CaseItem &item);
};
} // namespace re4
