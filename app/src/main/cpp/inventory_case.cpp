// Frozen USA source tables: game/puzzle.cpp::piece_info (58 pieces + sentinel),
// game/item.cpp::wep_info (43 rows) and combination_info (77 rows).
#include "inventory_case.h"
#include "item_rules.h"
#include "weapon_carry_source.h"
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace re4 {
namespace {
struct Shape { unsigned id, w, h; };
constexpr Shape shapes[] = {
  {0x0003, 5, 2},
  {0x0021, 3, 2},
  {0x0040, 3, 2},
  {0x0023, 3, 2},
  {0x0025, 4, 2},
  {0x0027, 3, 2},
  {0x0029, 4, 2},
  {0x002A, 4, 2},
  {0x002C, 8, 2},
  {0x002D, 5, 2},
  {0x002E, 9, 1},
  {0x002F, 7, 2},
  {0x0030, 3, 2},
  {0x0034, 7, 3},
  {0x0035, 8, 2},
  {0x0036, 5, 2},
  {0x0037, 4, 2},
  {0x0038, 1, 3},
  {0x0094, 8, 2},
  {0x0017, 8, 2},
  {0x006D, 8, 2},
  {0x003E, 4, 2},
  {0x0052, 7, 3},
  {0x0004, 2, 1},
  {0x0020, 2, 1},
  {0x0018, 2, 1},
  {0x0007, 2, 1},
  {0x006A, 2, 1},
  {0x001A, 2, 1},
  {0x0000, 2, 1},
  {0x0046, 2, 1},
  {0x0072, 4, 1},
  {0x003F, 2, 1},
  {0x0042, 3, 1},
  {0x0043, 2, 2},
  {0x0044, 3, 1},
  {0x0045, 3, 1},
  {0x00AA, 2, 2},
  {0x00C5, 3, 1},
  {0x0001, 1, 2},
  {0x0002, 1, 2},
  {0x000E, 1, 2},
  {0x0005, 1, 2},
  {0x0008, 1, 1},
  {0x0009, 1, 1},
  {0x000A, 1, 1},
  {0x0095, 1, 3},
  {0x0097, 2, 6},
  {0x0006, 1, 2},
  {0x0019, 1, 2},
  {0x001C, 1, 2},
  {0x0014, 1, 2},
  {0x0016, 1, 2},
  {0x00A8, 1, 2},
  {0x0012, 1, 2},
  {0x0013, 1, 2},
  {0x0015, 1, 2},
  {0x000C, 1, 2},
};
struct Weapon { unsigned id, ammo, capacity; };
constexpr Weapon weapons[] = {
  {0x23, 0x04, 10},
  {0x24, 0x04, 10},
  {0x25, 0x04, 8},
  {0x26, 0x04, 8},
  {0x03, 0x04, 15},
  {0x21, 0x04, 10},
  {0x22, 0x04, 10},
  {0x40, 0x04, 10},
  {0x27, 0x04, 15},
  {0x28, 0x04, 15},
  {0x29, 0x00, 6},
  {0x2C, 0x18, 6},
  {0x2D, 0x18, 12},
  {0x2E, 0x07, 5},
  {0x6B, 0x07, 5},
  {0x99, 0x07, 5},
  {0x2F, 0x07, 10},
  {0x6C, 0x07, 10},
  {0x51, 0x07, 10},
  {0x30, 0x20, 30},
  {0x31, 0x20, 30},
  {0x32, 0x20, 30},
  {0x33, 0x20, 30},
  {0x36, 0x46, 5},
  {0xAB, 0x46, 5},
  {0x2A, 0x00, 7},
  {0x2B, 0x00, 7},
  {0x34, 0x6A, 20},
  {0x35, 0xFF, 1},
  {0x17, 0xFF, 1},
  {0x6D, 0xFF, 1},
  {0x37, 0x1A, 3},
  {0x38, 0xFF, 1},
  {0x01, 0xFF, 1},
  {0x02, 0xFF, 1},
  {0x0E, 0xFF, 1},
  {0x800, 0xFF, 1},
  {0x08, 0xFF, 1},
  {0x09, 0xFF, 1},
  {0x0A, 0xFF, 1},
  {0x94, 0x18, 7},
  {0x52, 0x72, 0},
  {0x3E, 0x20, 30},
};
struct Recipe { unsigned a, b, result; };
constexpr Recipe recipes[] = {
  {0x21, 0x3F, 0x22},
  {0x23, 0x3F, 0x24},
  {0x25, 0x42, 0x26},
  {0x27, 0x3F, 0x28},
  {0x30, 0x43, 0x32},
  {0x31, 0x43, 0x33},
  {0x2E, 0x44, 0x6B},
  {0x99, 0x44, 0x6B},
  {0x2E, 0xC5, 0x99},
  {0x6B, 0xC5, 0x99},
  {0x2F, 0x45, 0x6C},
  {0x51, 0x45, 0x6C},
  {0x2F, 0xC5, 0x51},
  {0x6C, 0xC5, 0x51},
  {0x36, 0xAA, 0xAB},
  {0x06, 0x06, 0x12},
  {0x12, 0x06, 0x13},
  {0x06, 0x19, 0x14},
  {0x06, 0x1C, 0x16},
  {0x19, 0x1C, 0xA8},
  {0x14, 0x1C, 0x15},
  {0x16, 0x19, 0x15},
  {0xA8, 0x06, 0x15},
  {0x5E, 0x5F, 0x62},
  {0x5E, 0x60, 0x63},
  {0x5E, 0x61, 0x64},
  {0x63, 0x5F, 0x65},
  {0x64, 0x5F, 0x66},
  {0x62, 0x60, 0x65},
  {0x64, 0x60, 0x67},
  {0x62, 0x61, 0x66},
  {0x63, 0x61, 0x67},
  {0x67, 0x5F, 0x68},
  {0x66, 0x60, 0x68},
  {0x65, 0x61, 0x68},
  {0x9A, 0x9B, 0x9D},
  {0x9A, 0x9C, 0x9E},
  {0x9D, 0x9C, 0x9F},
  {0x9E, 0x9B, 0x9F},
  {0xB8, 0xB9, 0xBC},
  {0xB8, 0xBA, 0xBD},
  {0xB8, 0xBB, 0xBE},
  {0xBD, 0xB9, 0xBF},
  {0xBE, 0xB9, 0xC0},
  {0xBC, 0xBA, 0xBF},
  {0xBE, 0xBA, 0xC1},
  {0xBC, 0xBB, 0xC0},
  {0xBD, 0xBB, 0xC1},
  {0xC1, 0xB9, 0xC2},
  {0xC0, 0xBA, 0xC2},
  {0xBF, 0xBB, 0xC2},
  {0xC6, 0xC7, 0xCA},
  {0xC6, 0xC8, 0xCB},
  {0xC6, 0xC9, 0xCC},
  {0xCB, 0xC7, 0xCD},
  {0xCC, 0xC7, 0xCE},
  {0xCA, 0xC8, 0xCD},
  {0xCC, 0xC8, 0xCF},
  {0xCA, 0xC9, 0xCE},
  {0xCB, 0xC9, 0xCF},
  {0xCF, 0xC7, 0xD0},
  {0xCE, 0xC8, 0xD0},
  {0xCD, 0xC9, 0xD0},
  {0xD1, 0xD2, 0xD5},
  {0xD1, 0xD3, 0xD6},
  {0xD1, 0xD4, 0xD7},
  {0xD6, 0xD2, 0xD8},
  {0xD7, 0xD2, 0xD9},
  {0xD5, 0xD3, 0xD8},
  {0xD7, 0xD3, 0xDA},
  {0xD5, 0xD4, 0xD9},
  {0xD6, 0xD4, 0xDA},
  {0xDA, 0xD2, 0xDB},
  {0xD9, 0xD3, 0xDB},
  {0xD8, 0xD4, 0xDB},
  {0xA4, 0xA5, 0xA6},
  {0x3A, 0x69, 0x7A},
};
std::string json(const std::string &s) {
  std::ostringstream out; out << '"';
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') out << '\\' << char(c);
    else if (c < 32) out << "\\u" << std::hex << std::setfill('0') << std::setw(4) << unsigned(c) << std::dec;
    else out << char(c);
  }
  out << '"'; return out.str();
}
unsigned maxCount(unsigned id) {
  const auto info = OriginalItemInfo(id);
  return info.type == 1 || info.type == 9 ? 1u : info.maxNum;
}
bool money(unsigned id) { return id == 0x78 || id == 0x79 || id == 0x71; }
} // namespace
ItemFootprint OriginalItemFootprint(unsigned id) {
  for (const auto &s : shapes) if (s.id == id) return {s.w, s.h};
  return {};
}
unsigned OriginalWeaponCapacity(unsigned id) {
  for (const auto &w : weapons) if (w.id == id) return w.capacity;
  return 0;
}
unsigned OriginalWeaponAmmo(unsigned id) {
  for (const auto &w : weapons) if (w.id == id) return w.ammo;
  return 0xff;
}
unsigned OriginalHealingAmount(unsigned id) {
  switch (id) {
  case 0x08: return 400;
  case 0x09: return 800;
  case 0x95: return 900;
  case 0x06: case 0x16: return 600;
  case 0x12: return 1300;
  case 0x05: case 0x0a: case 0x13: case 0x14: case 0x15: case 0x97: return 2400;
  default: return 0;
  }
}
const char *OriginalItemLabel(unsigned id) {
  switch (id) {
  case 0x38: return "Knife";
  case 0x40: return "Wesker's handgun";
  case 0x21: return "Punisher";
  case 0x23: return "Handgun";
  case 0x2c: return "Shotgun";
  case 0x25: return "Red9";
  case 0x27: return "Blacktail";
  case 0x03: return "Matilda";
  case 0x29: return "Broken Butterfly";
  case 0x2a: return "Killer7";
  case 0x37: return "Handcannon";
  case 0x94: return "Riot Gun";
  case 0x2d: return "Striker";
  case 0x2e: return "Rifle";
  case 0x2f: return "Rifle (semi-auto)";
  case 0x30: return "TMP";
  case 0x3e: return "TMP (HUNK)";
  case 0x34: return "Chicago Typewriter";
  case 0x36: return "Mine Thrower";
  case 0x35: return "Rocket Launcher";
  case 0x17: return "Rocket Launcher (special)";
  case 0x6d: return "Infinite Launcher";
  case 0x52: return "Krauser Bow";
  case 0x3f: return "Silencer (Handgun)";
  case 0x42: return "Stock (Red9)";
  case 0x43: return "Stock (TMP)";
  case 0x44: return "Scope (Rifle)";
  case 0x45: return "Scope (semi-auto rifle)";
  case 0xaa: return "Scope (Mine Thrower)";
  case 0xc5: return "Infrared Scope";

  case 0x04: return "Handgun Ammo";
  case 0x18: return "Shotgun Shells";
  // item.cpp::wep_info: WEP06/0x2A uses 0x00; WEP09/0x2E uses 0x07.
  case 0x00: return "Magnum Ammo";
  case 0x07: return "Rifle Ammo";
  case 0x20: return "TMP Ammo";
  case 0x05: return "First Aid Spray";
  case 0x01: return "Hand Grenade";
  case 0x02: return "Flash Grenade";
  case 0x0e: return "Incendiary Grenade";
  case 0x06: return "Green Herb";
  case 0x19: return "Red Herb";
  case 0x1c: return "Yellow Herb";
  case 0x12: return "Mixed Herbs (G+G)";
  case 0x13: return "Mixed Herbs (G+G+G)";
  case 0x14: return "Mixed Herbs (G+R)";
  case 0x16: return "Mixed Herbs (G+Y)";
  case 0xa8: return "Mixed Herbs (R+Y)";
  case 0x15: return "Mixed Herbs (G+R+Y)";
  case 0x08: return "Egg";
  case 0x09: return "Brown Egg";
  case 0x0a: return "Golden Egg";
  case 0x57: return "Treasure 0x57";
  case 0x78: case 0x79: case 0x71: return "Pesetas";
  case 0xaf: return "Village note";
  case 0xae: return "Document";
  default: return "Item";
  }
}
InventoryCase::InventoryCase(unsigned size) : size_(size) {
  if (size > 3) throw std::invalid_argument("Case size outside original table bounds");
}
unsigned InventoryCase::width() const {
  constexpr unsigned widths[]{10, 11, 12, 15}; return widths[size_];
}
unsigned InventoryCase::height() const {
  constexpr unsigned heights[]{6, 7, 8, 8}; return heights[size_];
}
void InventoryCase::clear() { items_.clear(); equipped_ = held_ = ~0u;handOrigin_={}; }
void InventoryCase::restoreSnapshot(unsigned size,const std::vector<CaseItem> &items,unsigned equippedSlot) {
  InventoryCase next(size);
  for(const auto &i:items) {
    if(!i.active)continue;
    const auto info=OriginalItemInfo(i.id);
    if(i.space || i.held || i.slot>=0x180 || i.id>255 || !info.type || !i.count || i.count>maxCount(i.id) ||
       i.rotation>3 || i.loaded>0x1fff || ((i.x<0)!=(i.y<0)))
      throw std::runtime_error("Original save: invalid case slot");
    if(next.items_.size()<=i.slot) {
      const auto before=next.items_.size();next.items_.resize(i.slot+1);
      for(unsigned n=unsigned(before);n<next.items_.size();++n)next.items_[n].slot=n;
    }
    if(next.items_[i.slot].active || (i.inCase() && !next.fits(i.slot,i.id,i.x,i.y,i.rotation)))
      throw std::runtime_error("Original save: Case slots overlap");
    if(!OriginalItemFootprint(i.id).width && i.inCase())
      throw std::runtime_error("Original save: Item has no case shape");
    next.items_[i.slot]=i;
  }
  if(equippedSlot!=~0u && !next.equip(equippedSlot))
    throw std::runtime_error("Original save: equipped case slot is missing");
  for(const auto &i:next.items_)if(i.active && i.attachedTo!=~0u) {
    const auto *owner=next.item(i.attachedTo);
    if(OriginalItemInfo(i.id).type!=9 || !owner || OriginalItemInfo(owner->id).type!=1)
      throw std::runtime_error("Original save: accessory owner is missing");
    bool valid=false;
    for(const auto &r:recipes)if(r.a==owner->id && r.b==i.id)valid=true;
    if(!valid)throw std::runtime_error("Original save: incompatible accessory");
  }
  *this=std::move(next);
}
void InventoryCase::resetGameStart(unsigned handgunId) {
  if (handgunId != 0x21 && handgunId != 0x23)
    throw std::invalid_argument("Unsupported starting handgun");
  size_ = 0; clear();
  if (!add(handgunId, 1) || !add(0x04, 20) || !add(0x05, 1))
    throw std::logic_error("Original starting case does not fit");
  // Source saved half-cell centers (2,1), (7,0), (6,3) become normalized
  // rectangle origins (0,0), (3,0), (3,1).
  if (!move(2, 3, 1, 0)) throw std::logic_error("Original spray position is blocked");
  if (!add(0x7c, 1)) throw std::logic_error("Original starting item does not fit");
  equip(0);
}
void InventoryCase::resetTestLoadout(unsigned loaded, unsigned reserve, unsigned handgunId) {
  if ((handgunId != 0x21 && handgunId != 0x23) || loaded > OriginalWeaponCapacity(handgunId) || reserve > 65535)
    throw std::invalid_argument("Invalid test ammo");
  size_ = 0; clear();
  if (!add(handgunId, 1) || (reserve && !add(0x04, reserve)))
    throw std::logic_error("Test case does not fit");
  items_[0].loaded = loaded; equip(0);
}
const CaseItem *InventoryCase::item(unsigned slot) const {
  return slot < items_.size() && items_[slot].active ? &items_[slot] : nullptr;
}
unsigned InventoryCase::count(unsigned id) const {
  unsigned result = 0;
  for (const auto &i : items_) if (i.active && i.id == id) result += i.count;
  return result;
}
unsigned InventoryCase::equippedId() const {
  return weaponId(equipped_);
}
unsigned InventoryCase::weaponId(unsigned slot) const {
  const auto *p=item(slot);if(!p)return 0xffff;
  unsigned id=p->id;
  for(const auto &part:items_)if(part.active && part.attachedTo==slot)
    for(const auto &r:recipes)if(r.a==id && r.b==part.id){id=r.result;break;}
  return id;
}
bool InventoryCase::fits(unsigned ignore,unsigned id,int x,int y,unsigned rotation) const {
  return fitsBoard(ignore,id,x,y,rotation,false);
}
bool InventoryCase::fitsBoard(unsigned ignore, unsigned id, int x, int y, unsigned rotation,bool space) const {
  if (rotation > 3) return false;
  auto d = OriginalItemFootprint(id);
  if (rotation & 1) std::swap(d.width, d.height);
  if (!d.width || x < 0 || y < 0 || unsigned(x) + d.width > (space?6:width()) || unsigned(y) + d.height > (space?12:height())) return false;
  for (const auto &i : items_) {
    if (!i.active || i.slot == ignore || !i.onBoard() || i.space!=space) continue;
    auto other = OriginalItemFootprint(i.id);
    if (i.rotation & 1) std::swap(other.width, other.height);
    if (x < i.x + int(other.width) && x + int(d.width) > i.x &&
        y < i.y + int(other.height) && y + int(d.height) > i.y) return false;
  }
  return true;
}
bool InventoryCase::place(CaseItem &i) {
  for (unsigned rotation = 0; rotation < 2; ++rotation)
    for (unsigned y = 0; y < height(); ++y)
      for (unsigned x = 0; x < width(); ++x)
        if (fits(i.slot, i.id, int(x), int(y), rotation)) {
          i.x = int(x); i.y = int(y); i.rotation = rotation; return true;
        }
  return false;
}
bool InventoryCase::addImpl(unsigned id, unsigned amount) {
  if (id > 255 || amount > 65535) return false;
  if (money(id)) return true; // ScenarioState owns money; no packing piece.
  const auto info = OriginalItemInfo(id);
  const unsigned max = maxCount(id);
  if (!max) return false;
  if (!amount) amount = info.type == 1 || info.type == 9 ? 1u : info.defNum;
  if (!amount) return false;
  const auto footprint = OriginalItemFootprint(id);
  if (!footprint.width && (info.type == 1 || info.type == 2 || info.type == 3 || info.type == 6 || info.type == 9)) return false;
  // Original ordering() fills the largest existing stacks first.
  std::vector<unsigned> stacks;
  if (info.type == 2 || info.type == 5 || info.type == 12 || info.type == 13)
    for (const auto &i : items_) if (i.active && i.id == id && i.count < max) stacks.push_back(i.slot);
  std::stable_sort(stacks.begin(), stacks.end(), [&](unsigned a, unsigned b) { return items_[a].count > items_[b].count; });
  for (unsigned slot : stacks) {
    auto &i = items_[slot]; const auto put = std::min(amount, max - i.count);
    i.count += put; amount -= put; if (!amount) return true;
  }
  while (amount) {
    unsigned slot = 0;
    while (slot < items_.size() && items_[slot].active) ++slot;
    if (slot == 0x180) return false;
    CaseItem value; value.slot = slot; value.id = id; value.active = true;
    value.count = std::min(amount, max);
    value.loaded = info.type == 1 ? OriginalWeaponCapacity(id) : 0;
    if (slot == items_.size()) items_.push_back(value); else items_[slot] = value;
    if (footprint.width && !place(items_[slot])) return false;
    amount -= value.count;
  }
  return true;
}
bool InventoryCase::canAdd(unsigned id, unsigned amount) const {
  auto copy = *this; return copy.addImpl(id, amount);
}
bool InventoryCase::add(unsigned id, unsigned amount) {
  auto copy = *this; if (!copy.addImpl(id, amount)) return false;
  *this = std::move(copy); return true;
}
bool InventoryCase::equip(unsigned slot) {
  const auto *p = item(slot);
  if (!p || !OriginalWeaponCarry(p->id)) return false;
  equipped_ = slot; return true;
}
bool InventoryCase::move(unsigned slot, int x, int y, unsigned rotation) {
  const auto *p = item(slot);
  if (!p || !p->inCase() || !fits(slot, p->id, x, y, rotation)) return false;
  items_[slot].x = x; items_[slot].y = y; items_[slot].rotation = rotation;
  return true;
}
bool InventoryCase::discard(unsigned slot) {
  const auto *p = item(slot);
  if (!p || !p->onBoard()) return false;
  if(slot==equipped_)equipped_=~0u;
  items_[slot].active = false;
  for(auto &part:items_)if(part.attachedTo==slot)part.attachedTo=~0u;
  return true;
}
bool InventoryCase::puzzleTake(unsigned slot) {
  const auto *p=item(slot);if(held_!=~0u || !p || !p->onBoard() || !OriginalItemFootprint(p->id).width)return false;
  handOrigin_=*p;held_=slot;items_[slot].held=true;return true;
}
bool InventoryCase::puzzleUnsettled() const {
  if(held_!=~0u)return true;
  for(const auto &p:items_)if(p.active && p.space)return true;
  return false;
}
bool InventoryCase::puzzleReturn() {
  if(!item(held_))return false;
  auto &p=items_[held_];p.x=handOrigin_.x;p.y=handOrigin_.y;p.rotation=handOrigin_.rotation;p.space=handOrigin_.space;
  if(!fitsBoard(p.slot,p.id,p.x,p.y,p.rotation,p.space))return false;
  p.held=false;held_=~0u;return true;
}
unsigned InventoryCase::puzzlePut(int x,int y,unsigned rotation,bool space) {
  if(!item(held_) || rotation>3 || x<0 || y<0)return 0;
  auto &hand=items_[held_];hand.x=x;hand.y=y;hand.rotation=rotation;hand.space=space;
  auto shape=OriginalItemFootprint(hand.id);if(rotation&1)std::swap(shape.width,shape.height);
  if(!shape.width || x+shape.width>(space?6:width()) || y+shape.height>(space?12:height()))return 0;
  if(fitsBoard(held_,hand.id,x,y,rotation,space)){hand.held=false;held_=~0u;return 1;}
  unsigned overlap=~0u;
  for(const auto &p:items_)if(p.active && p.onBoard() && p.space==space){
    auto d=OriginalItemFootprint(p.id);if(p.rotation&1)std::swap(d.width,d.height);
    if(x<p.x+int(d.width) && x+int(shape.width)>p.x && y<p.y+int(d.height) && y+int(shape.height)>p.y){
      if(overlap!=~0u)return 0;
      overlap=p.slot;
    }
  }
  if(overlap==~0u)return 0;
  // Original PzzlThinking: putPiece -> cmbPiece -> chgPiece.
  auto candidate=*this;const auto oldTarget=candidate.items_[overlap];const auto heldBefore=held_;
  if(candidate.combine(overlap,heldBefore)) {
    unsigned result=overlap;
    if(!candidate.item(overlap) || candidate.items_[overlap].id!=oldTarget.id) {
      result=~0u;
      for(const auto &p:candidate.items_)if(p.active && (!item(p.slot) || item(p.slot)->id!=p.id)){result=p.slot;break;}
    }
    if(result!=~0u){auto &p=candidate.items_[result];p.x=oldTarget.x;p.y=oldTarget.y;p.rotation=oldTarget.rotation;p.space=oldTarget.space;p.held=false;}
    if(!candidate.item(heldBefore) || !candidate.items_[heldBefore].held)candidate.held_=~0u;
    else if(OriginalItemInfo(oldTarget.id).type==9 || OriginalItemInfo(items_[heldBefore].id).type==9)candidate.puzzleReturn();
    bool valid=true;for(const auto &p:candidate.items_)if(p.active && p.onBoard() && OriginalItemFootprint(p.id).width)
      valid=valid && candidate.fitsBoard(p.slot,p.id,p.x,p.y,p.rotation,p.space);
    if(valid){*this=std::move(candidate);return 2;}
  }
  candidate=*this;auto &other=candidate.items_[overlap];const auto origin=other;other.held=true;
  if(!candidate.fitsBoard(held_,hand.id,x,y,rotation,space))return 0;
  candidate.items_[held_].held=false;candidate.held_=overlap;candidate.handOrigin_=origin;
  *this=std::move(candidate);return 3;
}
bool InventoryCase::puzzleDiscardSpace() {
  if(held_!=~0u)return false;
  for(auto &p:items_)if(p.active && p.space){
    const auto slot=p.slot;p.active=false;if(equipped_==slot)equipped_=~0u;
    for(auto &part:items_)if(part.active && part.attachedTo==slot)part.attachedTo=~0u;
  }
  return true;
}
bool InventoryCase::detach(unsigned slot) {
  const auto *p=item(slot);
  if(!p || OriginalItemInfo(p->id).type!=9 || p->attachedTo==~0u)return false;
  items_[slot].attachedTo=~0u;return true;
}
bool InventoryCase::consume(unsigned id, unsigned amount) {
  if (!amount || count(id) < amount || OriginalItemInfo(id).type == 1) return false;
  auto copy = *this;
  // cItemMgr::minimumSearch consumes the smallest stack first.
  while (amount) {
    unsigned selected = ~0u;
    for (const auto &i : copy.items_)
      if (i.active && i.id == id && (selected == ~0u || i.count < copy.items_[selected].count)) selected = i.slot;
    auto &i = copy.items_[selected]; const auto remove = std::min(amount, i.count);
    i.count -= remove; amount -= remove; if (!i.count) i.active = false;
  }
  *this = std::move(copy); return true;
}
bool InventoryCase::combine(unsigned first, unsigned second) {
  const auto *a = item(first), *b = item(second);
  if (!a || !b || first == second) return false;
  const auto typeA = OriginalItemInfo(a->id).type, typeB = OriginalItemInfo(b->id).type;
  // item.cpp::combine(flag=1)/reload_main supports both selection orders.
  // Reload only the selected reserve stack and keep the original weapon
  // slot, packing location and equipped state; no firing motion is started.
  if ((typeA == 1 && typeB == 2) || (typeA == 2 && typeB == 1)) {
    const unsigned weaponSlot = typeA == 1 ? first : second;
    const unsigned ammoSlot = typeA == 2 ? first : second;
    const auto &weapon = items_[weaponSlot], &ammo = items_[ammoSlot];
    const unsigned capacity = OriginalWeaponCapacity(weapon.id);
    if (ammo.id != OriginalWeaponAmmo(weapon.id) ||
        weapon.loaded >= capacity || !ammo.count) return false;
    const unsigned take = std::min(capacity - weapon.loaded, ammo.count);
    items_[weaponSlot].loaded += take;
    items_[ammoSlot].count -= take;
    if (!items_[ammoSlot].count) items_[ammoSlot].active = false;
    return true;
  }
  if((typeA==1 && typeB==9) || (typeA==9 && typeB==1)) {
    const auto weaponSlot=typeA==1?first:second,partSlot=typeA==9?first:second;
    bool valid=false;
    for(const auto &r:recipes)if(r.a==items_[weaponSlot].id && r.b==items_[partSlot].id)valid=true;
    if(!valid)return false;
    // cItemMgr::partsCombine: move a part from its previous weapon, replace
    // the existing scope/stock/silencer and update weaponId via the link.
    for(auto &part:items_)if(part.active && part.attachedTo==weaponSlot)part.attachedTo=~0u;
    items_[partSlot].attachedTo=weaponSlot;return true;
  }
  if (typeA == 1 || typeB == 1 || typeA == 9 || typeB == 9) return false;
  auto copy = *this;
  auto &aa = copy.items_[first]; auto &bb = copy.items_[second];
  if (aa.id == bb.id && (typeA == 2 || typeA == 3)) {
    const auto room = maxCount(aa.id) - aa.count;
    const auto take = std::min(room, bb.count); if (!take) return false;
    aa.count += take; bb.count -= take; if (!bb.count) bb.active = false;
  } else {
    unsigned result = 0xffff;
    for (const auto &r : recipes)
      if ((r.a == aa.id && r.b == bb.id) || (r.b == aa.id && r.a == bb.id)) { result = r.result; break; }
    if (result == 0xffff) return false;
    // item.cpp combines into the target slot; the puzzle rehash keeps its
    // board/position. A full case must not block a recipe on the space board.
    const auto target=aa;
    if (!--aa.count) aa.active = false;
    if (!--bb.count) bb.active = false;
    if(aa.active)return false; // Source recipe targets are single-count items.
    CaseItem value=target;value.id=result;value.count=1;value.loaded=0;value.active=true;value.attachedTo=~0u;
    if(OriginalItemFootprint(result).width && !copy.fitsBoard(first,result,value.x,value.y,value.rotation,value.space))return false;
    copy.items_[first]=value;
  }
  *this = std::move(copy); return true;
}
bool InventoryCase::consumeHealing(unsigned slot, unsigned life, unsigned maxLife) {
  const auto *p = item(slot);
  if (!p || !OriginalHealingAmount(p->id) || !maxLife || life >= maxLife) return false;
  // Yellow mixtures require the max-life upgrade state; do not discard it.
  if (p->id == 0x15 || p->id == 0x16) return false;
  if (!--items_[slot].count) items_[slot].active = false;
  return true;
}
bool InventoryCase::synchronizeWeaponAmmo(unsigned id, unsigned loaded, unsigned reserve) {
  if (OriginalItemInfo(id).type != 1 || loaded > OriginalWeaponCapacity(id) || reserve > 65535) return false;
  const auto at = std::find_if(items_.begin(), items_.end(), [&](const CaseItem &i) { return i.active && weaponId(i.slot) == id; });
  if (at == items_.end()) return false;
  const auto ammo = OriginalWeaponAmmo(id), previous = count(ammo);
  if (at->loaded == loaded && previous == reserve) return true;
  // Slot remains stable even when reserve insertion reallocates the vector.
  const auto slot = at->slot;
  auto copy = *this;
  if (previous > reserve && !copy.consume(ammo, previous - reserve)) return false;
  if (reserve > previous && !copy.addImpl(ammo, reserve - previous)) return false;
  copy.items_[slot].loaded = loaded;
  *this = std::move(copy); return true;
}
unsigned OriginalCaseCommandType(unsigned id,bool attached) {
 const auto type=OriginalItemInfo(id).type;
 if(type==1 || type==3)return 0;
 if(type==4)return 7;
 if(type==9)return attached?6:3;
 if(type==2 || type==5 || type==12)return 3;
 if(type==14)return 9;
 if(type==6){if(id>=8 && id<=10)return 8;if(id==0x19 || id==0x1c || id==0xa8)return 3;}
 for(const auto &r:recipes)if(r.a==id || r.b==id)return 5;
 return 4;
}
std::string InventoryCase::snapshotJson(unsigned pesetas, const std::string &message) const {
  std::ostringstream out;
  out << "{\"casePuzzleVersion\":1,\"width\":" << width() << ",\"height\":" << height()
      << ",\"equipped\":" << equippedId() << ",\"pesetas\":" << pesetas
      << ",\"held\":" << (held_==~0u?-1:int(held_)) << ",\"spaceWidth\":6,\"spaceHeight\":12"
      << ",\"message\":" << json(message) << ",\"items\":[";
  bool first = true;
  for (const auto &i : items_) {
    if (!i.active) continue;
    if (!first) out << ',';
    first = false;
    auto d = OriginalItemFootprint(i.id); if (i.rotation & 1) std::swap(d.width, d.height);
    out << "{\"slot\":" << i.slot << ",\"id\":" << i.id << ",\"num\":" << i.count
        << ",\"loaded\":" << i.loaded << ",\"x\":" << i.x << ",\"y\":" << i.y
        << ",\"rotation\":" << i.rotation << ",\"w\":" << d.width << ",\"h\":" << d.height
        << ",\"space\":" << (i.space?"true":"false") << ",\"held\":" << (i.held?"true":"false")
        << ",\"commandType\":" << OriginalCaseCommandType(i.id,i.attachedTo!=~0u)
        << ",\"kind\":" << OriginalItemInfo(i.id).type << ",\"equipped\":" << (i.slot == equipped_ ? "true" : "false")
        << ",\"weaponId\":" << weaponId(i.slot) << ",\"attachedTo\":" << (i.attachedTo==~0u?-1:int(i.attachedTo))
        << ",\"equippable\":" << (OriginalWeaponCarry(i.id)?"true":"false")
        << ",\"name\":" << json(OriginalItemLabel(i.id)) << '}';
  }
  out << "]}"; return out.str();
}
} // namespace re4
