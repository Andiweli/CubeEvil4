// Source rules: frozen game/embox.cpp, EtcModel.cpp, sce_at.cpp and item.cpp.
// This runtime executes cEmBox, non-rolling cEmBarrel and waiting-type2 ITA.
// Full room callbacks and the other owned weapon drop branches are absent.
#include "object_actions.h"
#include "asset_reader.h"
#include "item_rules.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace re4 {
namespace {
constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;
constexpr double CoordinateLimit = 1e7;
void hashByte(std::uint64_t &hash, std::uint8_t value) {
  hash = (hash ^ value) * FnvPrime;
}
void hashValue(std::uint64_t &hash, std::uint64_t value) {
  for (unsigned n = 0; n < 8; ++n) {
    hashByte(hash, std::uint8_t(value));
    value >>= 8;
  }
}
template <class Bytes> void hashBytes(std::uint64_t &hash, const Bytes &bytes) {
  hashValue(hash, bytes.size());
  for (auto value : bytes)
    hashByte(hash, value);
}
std::uint64_t snapshotChecksum(const ObjectActionsSnapshot &snapshot) {
  auto hash = FnvOffset;
  hashValue(hash, snapshot.room);
  hashValue(hash, snapshot.profile);
  hashValue(hash, snapshot.states.size());
  for (const auto &state : snapshot.states) {
    hashValue(hash, state.slot);
    hashValue(hash, state.hp);
    hashValue(hash, state.broken);
    hashValue(hash, state.itemActive);
    hashValue(hash, state.itemTaken);
    hashValue(hash, state.knifeSerial);
    hashValue(hash, state.hasKnifeSerial);
    hashValue(hash, state.itemId);
    hashValue(hash, state.itemCount);
    hashValue(hash, state.itemResolved);
    hashValue(hash, state.bombWait);
    hashValue(hash, state.shotSerial);
    hashValue(hash, state.hasShotSerial);
  }
  hashValue(hash, snapshot.inventory.size());
  for (const auto &item : snapshot.inventory) {
    hashValue(hash, item.id);
    hashValue(hash, item.count);
  }
  hashValue(hash, snapshot.shotSerial);
  hashValue(hash, snapshot.hasShotSerial);
  hashValue(hash, snapshot.randomState);
  hashValue(hash, snapshot.noDropCount);
  hashValue(hash, snapshot.noRecoveryDropCount);
  hashValue(hash, snapshot.eventSerial);
  hashValue(hash, snapshot.tick);
  return hash;
}
bool validSnapshotShape(const ObjectActionsSnapshot &snapshot) {
  if (snapshot.states.size() > 64 || snapshot.inventory.size() > 256 ||
      snapshot.checksum != snapshotChecksum(snapshot))
    return false;
  std::array<bool, 64> slots{};
  unsigned taken = 0;
  for (const auto &state : snapshot.states) {
    if (state.slot >= 64 || slots[state.slot] ||
        state.hp != (state.broken ? 0u : 1000u) ||
        state.itemActive != (state.broken && state.itemCount && !state.itemTaken) ||
        (state.itemTaken && (!state.broken || !state.itemCount)) ||
        state.itemId > 255 || state.itemCount > 65535 ||
        (!state.itemResolved && (state.itemId || state.itemCount || state.broken)) ||
        (!state.itemCount && state.itemId) || state.bombWait > 2 ||
        (state.bombWait && !state.broken) ||
        (!state.hasKnifeSerial && state.knifeSerial) ||
        (!state.hasShotSerial && state.shotSerial))
      return false;
    slots[state.slot] = true;
    taken += state.itemTaken;
  }
  if (snapshot.noDropCount > 6 || snapshot.noRecoveryDropCount > 2 ||
      (!snapshot.hasShotSerial && snapshot.shotSerial))
    return false;
  unsigned previous = 0;
  bool first = true;
  std::uint64_t inventoryCount = 0;
  for (const auto &item : snapshot.inventory) {
    if (item.id > 0xff || !item.count || item.count > 65535 ||
        (!first && item.id <= previous))
      return false;
    first = false;
    previous = item.id;
    inventoryCount += item.count;
  }
  return inventoryCount <= std::uint64_t(taken) * 65535;
}
bool finite(const CollisionVec &point) {
  for (auto value : point)
    if (!std::isfinite(value) || std::abs(value) > CoordinateLimit)
      return false;
  return true;
}
double distanceSquared(const CollisionVec &a, const CollisionVec &b) {
  double result = 0;
  for (unsigned k = 0; k < 3; ++k) {
    const auto delta = a[k] - b[k];
    result += delta * delta;
  }
  return result;
}
bool objectType(unsigned id, unsigned &type, bool &explosive) {
  explosive = false;
  switch (id) {
  case 0x12: type = 0; explosive = true; return true;
  case 0x2d: case 0x3c: type = 2; explosive = true; return true;
  default: break;
  }
  switch (id) {
  case 1: type = 0; return true;
  case 2: type = 1; return true;
  case 17: type = 3; return true;
  case 30: type = 5; return true;
  default: return false;
  }
}
std::array<float, 3> sourceSize(unsigned type) {
  switch (type) {
  case 0: return {200, 350, 550};
  case 1: return {450, 600, 1000};
  case 3: return {660, 1250, 660};
  case 5: return {630, 1250, 630};
  default: throw std::runtime_error("Object actions: Unknown crate type");
  }
}
// Literal emBoxYarareInit: main hit cube and inner precision cube. Both are
// relative to the original ETS model matrix, not a world-axis approximation.
std::vector<AuthoredHitShape> sourceShapes(unsigned type) {
  const auto size = sourceSize(type);
  return {{{0, 0, 0}, size[0] * .5f + 50.f, size[1],
           size[2] * .5f + 50.f, HitOn | HitCube, 0},
          {{0, 0, 0}, size[0] * .5f * .5f, size[1] * .8f,
           size[2] * .5f * .8f, HitOn | HitCube, 0}};
}
unsigned defaultCount(unsigned id) {
  const auto info=OriginalItemInfo(id);
  if (!info.type) throw std::runtime_error("Object actions: Unknown ITA item");
  return info.type==1 || info.type==9 ? 1 : id==0x79 ? 30 : info.defNum;
}
void resolveLeonItem(ObjectDrop &item, unsigned authoredCount) {
  switch (item.sourceId) {
  case 0x1001: case 0x1002: item.id = item.count = 0; return;
  case 0x1003: item.id = 0x18; item.count = 15; break;
  case 0x1004: item.id = 0x18; item.count = 10; break;
  case 0x1005: item.id = 4; item.count = 20; break;
  default:
    if (item.sourceId > 0xff)
      throw std::runtime_error("Object actions: random/script item is missing");
    item.id = item.sourceId;
    const auto info = OriginalItemInfo(item.id);
    // cItemMgr::get returns money before itemInfo's stack-size clamp.
    // Its weapon/attachment types1/9 always create one inventory item.
    if (OriginalMoneyValue(item.id, authoredCount))
      item.count = authoredCount ? authoredCount : item.id == 0x79 ? 30 : info.defNum;
    else if (info.type == 1 || info.type == 9)
      item.count = 1;
    else
      item.count = std::min(authoredCount ? authoredCount : info.defNum, info.maxNum);
    break;
  }
  if (!item.count || item.count > 65535)
    throw std::runtime_error("Object actions: ITA item count");
}
std::vector<std::uint8_t> copyTag(const RoomArchive &room, const char *tag) {
  const auto *entry = room.find(tag);
  return entry ? room.copy(entry - room.entries().data())
               : std::vector<std::uint8_t>{};
}
void validHit(const ObjectActionHit &hit,
              const std::vector<ObjectActionObject> &objects) {
  if (hit.index >= objects.size() ||
      hit.slot != objects[hit.index].placement.slot ||
      hit.hit.partIndex >= objects[hit.index].shapes.size() ||
      !std::isfinite(hit.hit.lenSq) || hit.hit.lenSq < 0 ||
      hit.hit.lenSq > 1e16f)
    throw std::runtime_error("Object actions: invalid registered hit");
  for (float value : hit.hit.cross)
    if (!std::isfinite(value) || std::abs(value) > CoordinateLimit)
      throw std::runtime_error("Object actions: invalid hit point");
}
} // namespace

ObjectActions::ObjectActions(
    std::uint16_t room, const RoomArchive &archive,
    const std::vector<RoomObjectPlacement> &placements,
    const SatCollision *effectCollision, ScenarioState *scenario)
    : room_(room), effectCollision_(effectCollision), scenario_(scenario) {
  if (placements.size() > 64)
    throw std::runtime_error("Object actions: ETS slot limit");
  const auto ita = copyTag(archive, "ITA");
  if(const auto *itm=archive.find("ITM"))
    itemModels_=std::make_shared<RoomItemArchive>(archive.copy(itm-archive.entries().data()));
  profile_ = FnvOffset;
  hashValue(profile_, room_);
  hashValue(profile_, 2); // M26 state format / source runtime identity.
  hashBytes(profile_, ita);
  std::set<unsigned> slots;
  for (const auto &placement : placements) {
    if (placement.slot >= 64 || !slots.insert(placement.slot).second)
      throw std::runtime_error("Object actions: duplicate/invalid ETS slot");
    hashBytes(profile_, placement.raw);
    hashValue(profile_, placement.id);
    hashValue(profile_, placement.slot);
    for (const auto &values : {placement.position, placement.rotation})
      for (float value : values) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        hashValue(profile_, bits);
      }
    unsigned type;
    bool explosive;
    if (!objectType(placement.id, type, explosive))
      continue;
    for (float value : placement.position)
      if (!std::isfinite(value) || std::abs(value) > CoordinateLimit)
        throw std::runtime_error("Object actions: invalid ETS position");
    for (float value : placement.rotation)
      if (!std::isfinite(value) || std::abs(value) > 1000)
        throw std::runtime_error("Object actions: invalid ETS angle");
    ObjectActionObject object;
    object.placement = placement;
    object.boxType = type;
    object.explosive = explosive;
    SmdPlacement transform;
    transform.position = placement.position;
    transform.rotation = placement.rotation;
    transform.scale = {1, 1, 1};
    object.matrix = PlacementMatrix(transform);
    object.shapes = explosive
        ? std::vector<AuthoredHitShape>{{{0, 0, 0}, 330, 1250, 330,
                                        HitOn | HitCube, 0}}
        : sourceShapes(type);
    objects_.push_back(std::move(object));
  }
  if (objects_.empty())
    return;
  if (ita.empty()) {
    // emBoxInit starts with itemNo=-1. An unbound box still breaks and
    // produces its original debris; absence of a drop is not a load error.
    for (const auto &object : objects_) {
      ObjectActionState state;
      state.slot = object.placement.slot;
      state.itemResolved = true;
      states_.push_back(state);
    }
    return;
  }
  if (ita.size() < 16 || ita.size() > 1024 * 1024)
    throw std::runtime_error("Object actions: ITA file budget");
  AssetReader r{ita};
  if (r.u32(0) != 0x49544100 || r.u16(4) != 0x105)
    throw std::runtime_error("Object actions: GC ITA version must be 0x105");
  const auto count = r.u16(6);
  if (count > 256)
    throw std::runtime_error("Object actions: ITA count limit");
  r.range(16, std::size_t(count) * 156);
  std::set<unsigned> areaNumbers, boundSlots, unsupportedSlots;
  for (unsigned index = 0; index < count; ++index) {
    const auto p = 16 + std::size_t(index) * 156;
    if (!areaNumbers.insert(ita[p + 0x36]).second)
      throw std::runtime_error("Object actions: Duplicate ITA area number");
    if (ita[p + 0x35] != 3 || !(ita[p + 0x34] & 1) ||
        ita[p + 0x46] != 2)
      continue;
    auto object = std::find_if(objects_.begin(), objects_.end(), [&](auto &o) {
      return o.placement.slot == ita[p + 0x47];
    });
    if (object == objects_.end())
      continue; // Other etc kinds have their own action runtime.
    if (!boundSlots.insert(object->placement.slot).second)
      throw std::runtime_error("Object actions: Ambiguous ITA crate contents");
    // Keep compatible objects in the same room usable. A prerequisite for
    // one item must neither bypass that script nor disable every other box.
    const unsigned sourceId = r.u16(p + 0x78);
    if (r.u32(p + 0x3c) || r.u32(p + 0x40) || r.u32(p + 0x4c) ||
        ita[p + 0x52] || (ita[p + 0x82] && !(ita[p + 0x82] & 1)) ||
        (ita[p + 0x84] & 0x92) || ita[p + 0x39] != 1 ||
        (ita[p + 0x37] & ~3u) || ita[p + 0x44] > 15 ||
        r.u16(p + 0x7a) >= 128 || r.u16(p + 0x7e) >= 128 ||
        (sourceId > 0xff && (sourceId < 0x1001 || sourceId > 0x1005)) ||
        (sourceId <= 0xff && (!OriginalItemInfo(sourceId).type ||
                            sourceId == 0x73 || sourceId == 0x75))) {
      unsupportedSlots.insert(object->placement.slot);
      continue;
    }
    object->hasItem = true;
    auto &item = object->item;
    item.recordIndex = index;
    item.areaNo = ita[p + 0x36] + 0x80; // ITA runtime-number namespace.
    item.sourceId = r.u16(p + 0x78);
    item.itemFlag = r.u16(p + 0x7a);
    item.autoFlag = r.u16(p + 0x7e);
    item.effect = ita[p + 0x80];
    item.playerType = ita[p + 0x82];
    item.controlFlags = ita[p + 0x84];
    item.radius = r.f32(p + 0x88);
    if (item.radius < 0 || item.radius > 100000)
      throw std::runtime_error("Object actions: ITA pickup radius");
    const auto pitch = r.f32(p + 0x8c), yaw = r.f32(p + 0x90);
    const auto open = r.f32(p + 0x94);
    if (std::abs(pitch) > 1000 || std::abs(yaw) > 1000 ||
        std::abs(open) > 1000)
      throw std::runtime_error("Object actions: ITA model angle");
    item.rotation = open > 0 ? std::array<float, 3>{pitch, yaw, 0}
                             : std::array<float, 3>{0, 0, 0};
    item.position = (item.controlFlags & 1)
        ? CollisionVec{r.f32(p + 0x5c), r.f32(p + 0x60), r.f32(p + 0x64)}
        : CollisionVec{object->placement.position[0],
                       double(object->placement.position[1]) + 50,
                       object->placement.position[2]};
    if (!finite(item.position))
      throw std::runtime_error("Object actions: invalid ITA item position");
    std::copy_n(ita.begin() + p, 156, item.raw.begin());
    resolveLeonItem(item, r.u16(p + 0x7c));
  }
  objects_.erase(std::remove_if(objects_.begin(),objects_.end(),[&](const auto &object) {
    return unsupportedSlots.count(object.placement.slot);
  }),objects_.end());
  for (const auto &object : objects_) {
    ObjectActionState state;
    state.slot = object.placement.slot;
    state.itemId = object.item.id;
    state.itemCount = object.item.count;
    state.itemResolved = object.item.sourceId != 0x1001 &&
                         object.item.sourceId != 0x1002;
    states_.push_back(state);
  }
}

std::optional<ObjectActionHit>
ObjectActions::target(const CollisionVec &from, const CollisionVec &to,
                      unsigned weaponNo, float maxLenSq) const {
  if (!finite(from) || !finite(to) || !std::isfinite(maxLenSq) ||
      maxLenSq < 0 || maxLenSq > 1e16f || weaponNo > 0x2d)
    throw std::runtime_error("Object actions: invalid weapon ray");
  auto limit = maxLenSq;
  // GetWepTargetList2's explicit knife exception for cEmBox/cEmItem ignores
  // the EAT-limited distance; the authored blade rays still bound the hit.
  if (effectCollision_)
    if (const auto wall = effectCollision_->line(from, to, SatSubset::All, 0,
                                      weaponNo == 0x10 ? 0 : 0x00400000))
      limit = std::min(limit, float(distanceSquared(from, wall->point)));
  std::optional<ObjectActionHit> result;
  for (std::size_t index = 0; index < objects_.size(); ++index) {
    if (!states_[index].hp)
      continue;
    const auto &object = objects_[index];
    const auto hit = HitAuthoredShapes(object.shapes, {}, object.matrix, from,
        to, weaponNo == 0x10 && !object.explosive ? 1e16f : limit, weaponNo);
    if (hit && (!result || hit->lenSq < result->hit.lenSq))
      result = ObjectActionHit{index, object.placement.slot, *hit};
  }
  return result;
}
std::vector<ObjectBreakEvent> ObjectActions::drainEvents() {
  std::vector<ObjectBreakEvent> result;
  result.swap(events_);
  return result;
}
std::optional<ObjectPlayerBlastHit> ObjectActions::playerBlastHit(
    const ObjectBlast &blast, const CollisionVec &world, bool infiniteLauncherEquipped) const {
  if (!finite(blast.position) || !finite(world) ||
      !std::isfinite(blast.radius) || blast.radius <= 0 ||
      blast.radius > 100000 || (blast.weaponNo != 0x13 && blast.weaponNo != 0x12))
    throw std::runtime_error("Object actions: invalid player explosion");
  const auto distance = distanceSquared(blast.position, world);
  if (distance > 36000000 ||
      distance > (blast.radius + 300) * (blast.radius + 300))
    return {};
  // Literal PlBombHitCk: outer PL_DM_AUTO_SML stagger bypasses the EAT ray.
  const unsigned inner=infiniteLauncherEquipped?1500:2500;
  if (distance > inner * inner)
    return ObjectPlayerBlastHit{false, 0};
  if (effectCollision_ && effectCollision_->line(
          blast.position, world, SatSubset::All, 0, 0x00400000))
    return {};
  return ObjectPlayerBlastHit{true, 1200};
}
std::vector<ObjectBlast> ObjectActions::tick() {
  ++tick_;
  std::vector<ObjectBlast> blasts;
  // First freeze this tick's due blasts. Newly triggered chained barrels
  // receive their own two-tick delay independent of ETS iteration order.
  for (std::size_t index = 0; index < objects_.size(); ++index) {
    auto &state = states_[index];
    if (!state.bombWait || --state.bombWait)
      continue;
    auto center = objects_[index].placement.position;
    center[1] += 500;
    blasts.push_back({index, state.slot, ++eventSerial_, 0x13,
                      {center[0], center[1], center[2]}, 6000});
  }
  for (const auto &blast : blasts)applyExplosion(blast);
  return blasts;
}
void ObjectActions::applyExplosion(const ObjectBlast &blast) {
  if(blast.weaponNo!=18 && blast.weaponNo!=19)return;
  if(!finite(blast.position) || !std::isfinite(blast.radius) || blast.radius<=0 || blast.radius>100000) {
    throw std::runtime_error("Object actions: Invalid external explosion");
  }
    for (std::size_t index = 0; index < objects_.size(); ++index) {
      if (!states_[index].hp)
        continue;
      const auto &object = objects_[index];
      std::optional<std::pair<float, CollisionVec>> nearest;
      for (const auto &shape : object.shapes) {
        if (!(shape.flags & HitOn))
          continue;
        // emSphereAtCk/At_box_sphere_ck use six face planes, not a closest
        // point sphere test. These ETS matrices have source unit rotation.
        HitVector local{};
        for (unsigned axis = 0; axis < 3; ++axis)
          for (unsigned row = 0; row < 3; ++row)
            local[axis] += object.matrix[row * 4 + axis] *
                float(blast.position[row] - object.matrix[row * 4 + 3]);
        for (unsigned axis = 0; axis < 3; ++axis)
          local[axis] -= shape.offset[axis];
        if (local[0] < -shape.radius - blast.radius ||
            local[0] > shape.radius + blast.radius ||
            local[1] < -blast.radius ||
            local[1] > shape.height + blast.radius ||
            local[2] < -shape.extent - blast.radius ||
            local[2] > shape.extent + blast.radius)
          continue;
        auto point = shape.offset;
        point[1] += shape.height * .5f;
        const auto transformed = TransformPoint(object.matrix, point);
        CollisionVec center{transformed[0], transformed[1], transformed[2]};
        if (std::abs(float(center[1] - blast.position[1])) >
            2000 + shape.radius)
          continue; // Blast weapons clamp emSphereAtCk r2 to 2000.
        const auto distance = float(distanceSquared(center, blast.position));
        if (!nearest || distance < nearest->first)
          nearest = std::make_pair(distance, center);
      }
      if (!nearest)
        continue;
      const float inner = blast.radius * .3f;
      if (nearest->first > inner * inner && effectCollision_ &&
          effectCollision_->line(blast.position, nearest->second,
                                 SatSubset::All, 0, 0x00400000))
        continue;
      // Registered source weapon0x13: cEmBox9999/style2; cEmBarrel1000
      // is a new explosion. It cannot damage an already broken object.
      breakObject(index, 2);
    }
}
std::vector<unsigned> ObjectRandomDropModelIds(bool mercenaries) {
  if (mercenaries)
    return {1, 2, 4, 5, 6, 0x0e, 0x18, 0x19, 0x78};
  return {1, 2, 4, 5, 6, 0x0e, 0x19, 0x78};
}
void ObjectActions::setDropContext(const ObjectDropContext &context) {
  if (context.health > 32767 || context.gameLevel > 10 || context.stage > 7 ||
      context.handgunAmmo > 65535 || context.shotgunAmmo > 65535 ||
      context.tmpAmmo > 65535 || context.bowAmmo > 65535 ||
      context.recoveryPoints > 65535 || context.greenHerbs > 65535)
    throw std::runtime_error("Object actions: invalid player drop status");
  dropContext_ = context;
}
unsigned ObjectActions::randomByte() {
  // Literal 16-bit Rnd recurrence, isolated per-room until the campaign owns
  // one global source RNG stream. Snapshotting prevents rerolling a box.
  const auto r = randomState_;
  const auto n = (std::uint32_t(std::uint8_t((r >> 1) + (r >> 8))) << 8) |
                 std::uint8_t(r >> 1);
  randomState_ = std::uint16_t(n == r ? n + 0x101 : n);
  return std::uint8_t(randomState_ >> 8);
}
void ObjectActions::resolveRandomDrop(std::size_t index) {
  auto &state = states_[index];
  if (state.itemResolved)
    return;
  const auto &context = dropContext_;
  const bool force = objects_[index].item.sourceId == 0x1002;
  state.itemResolved = true;
  const auto set = [&](unsigned id, unsigned count) {
    state.itemId = id;
    state.itemCount = count ? count : defaultCount(id);
  };
  const auto money = [&](unsigned base, unsigned big) {
    unsigned count = randomByte() % 6;
    count += randomByte() % 6 + base;
    count += randomByte() % 6;
    count += randomByte() % 6;
    count = count * 5 / 10 * 10;
    if ((randomByte() & 0x3f) == 0x1e) {
      count = big;
      if (randomByte() & 0xf)
        count = 330;
    }
    set(0x78, count);
  };
  const auto bulletDrop = [&] {
    // GetDropBullet: normal Leon, current handgun-only carried weapons.
    // Ammo for unowned rifles/TMP/shotguns is deliberately not selected.
    if (context.mercenaries) {
      const auto roll = randomByte() % 100;
      set(roll <= 0x36 ? 4 : roll <= 0x59 ? 0x18 : 1,
          roll > 0x36 && roll <= 0x59
              ? ((randomByte() & 0xf) != 5
                     ? (randomByte() % 10 > 4 ? 3 : 5) : 10)
              : 0);
      return;
    }
    const unsigned roll = randomByte() % 100;
    if (roll > 0x28 && context.handgunAmmo <= 0x3b) {
      set(4, context.stage > 1 ? 20 : 0);
      return;
    }
    const unsigned classRoll = randomByte() % 100;
    if (classRoll >= 0x5a && classRoll <= 0x63) {
      const unsigned grenade = randomByte() % 3;
      set(grenade == 1 ? 2 : grenade == 2 ? 0x0e : 1, 0);
      return;
    }
    if (randomByte() % 10 > 7) {
      const unsigned grenade = randomByte() % 3;
      set(grenade == 1 ? 2 : grenade == 2 ? 0x0e : 1, 0);
      return;
    }
    set(4, context.stage > 1 ? 20 : 0);
  };
  // Literal RandomItemCk(0x10): optional money, no-drop streak breaker,
  // ammunition/recovery budgets and source decision order.
  const auto roll = randomByte() % 100;
  const unsigned bullet = (context.stage <= 1 ? context.handgunAmmo
                                             : context.handgunAmmo / 2 + 1) +
                          context.shotgunAmmo * 2 + context.tmpAmmo / 5 + 1 +
                          context.bowAmmo / 5 + 1 + 5;
  if (roll <= 0x13) {
    if (context.mercenaries)
      return;
    money(10, 990);
    return;
  }
  if (roll <= 0x3b || (noDropCount_ > 2 && randomByte() % 10 > 4) ||
      noDropCount_ > 5 || force) {
    noDropCount_ = 0;
  } else {
    ++noDropCount_;
    return;
  }
  if (bullet < 30 && randomByte() % 10 > 4) {
    bulletDrop();
    return;
  }
  const unsigned recoveryLimit = context.gameLevel <= 2 ? 3 :
                                 context.gameLevel > 7 ? 0 : 1;
  if (context.recoveryPoints <= recoveryLimit) {
    if (context.recoveryPoints == 0)
      ++noRecoveryDropCount_;
    if (randomByte() % 10 > 7 || noRecoveryDropCount_ > 2) {
      unsigned id;
      if (context.health <= 500)
        id = randomByte() % 100 > 0x18 ? 6 : 5;
      else
        id = context.greenHerbs && randomByte() % 100 > 0x4a ? 0x19 : 6;
      noRecoveryDropCount_ = 0;
      set(id, 0);
      return;
    }
  }
  if (bullet < 150) {
    bulletDrop();
    return;
  }
  if (force)
    money(20, 990);
}
void ObjectActions::breakObject(std::size_t index, unsigned breakKind) {
  auto &state = states_.at(index);
  const auto &object = objects_[index];
  if (!state.hp)
    return;
  resolveRandomDrop(index);
  state.hp = 0;
  state.broken = true;
  state.itemActive = state.itemCount && !state.itemTaken;
  state.bombWait = object.explosive ? 2 : 0;
  if (events_.size() >= 64)
    throw std::runtime_error("Object actions: unconsumed event budget");
  const unsigned sound = object.explosive
      ? object.boxType == 0 ? 0x48 : room_ == 0x404 ? 0x53 : 0x56
      : object.boxType == 3 || object.boxType == 5 ? 0x3c : 0x1a;
  events_.push_back({++eventSerial_, state.slot, object.placement.id,
                    object.boxType, breakKind, index, object.explosive,
                    object.explosive ? 6u : 1u, sound,
                    object.placement.position, object.placement.rotation});
}
bool ObjectActions::applyShot(const ObjectActionHit &hit, unsigned serial, unsigned weaponNo) {
  validHit(hit, objects_);
  if(!OriginalWeaponAction(weaponNo))
    throw std::runtime_error("Object actions: unverified damage weapon");
  auto &state=states_[hit.index];
  if ((state.hasShotSerial && state.shotSerial == serial) || !state.hp)
    return false;
  state.shotSerial=serial;state.hasShotSerial=true;
  shotSerial_ = serial;
  hasShotSerial_ = true;
  // emBoxDmCk: shotguns close to the registered impact select EST kind1;
  // farther than6m retain kind0. Handguns1/2 always select kind0.
  const unsigned breakKind=OriginalShotgun(weaponNo) && hit.hit.lenSq<=36000000.f ? 1 : 0;
  breakObject(hit.index,breakKind);
  return true;
}
bool ObjectActions::applyKnife(const ObjectActionHit &hit, unsigned serial) {
  validHit(hit, objects_);
  auto &state = states_[hit.index];
  if (!state.hp || (state.hasKnifeSerial && state.knifeSerial == serial))
    return false;
  state.knifeSerial = serial;
  state.hasKnifeSerial = true;
  breakObject(hit.index);
  return true;
}
std::optional<ObjectActionHit> ObjectActions::shoot(const PlayerShot &shot,
                                                  float maxLenSq) {
  if (hasShotSerial_ && shotSerial_ == shot.serial)
    return {};
  const unsigned weapon=shot.weaponNumber();
  if(shot.pellets.size()>13 || (!OriginalShotgun(weapon) && !shot.pellets.empty()))
    throw std::runtime_error("Object actions: Shotgun ray budget");
  std::vector<ObjectActionHit> hits;
  // The original registers every pellet before EmMgr applies box damage.
  // Freeze this shot's hits against intact props to avoid a front crate's
  // immediate platform commit exposing another crate to the same burst.
  if(shot.pellets.empty()) {
    if(const auto hit=target(shot.from,shot.to,weapon,maxLenSq)) hits.push_back(*hit);
  } else for(const auto &ray:shot.pellets)
    if(const auto hit=target(ray[0],ray[1],weapon,maxLenSq)) hits.push_back(*hit);
  for(const auto &hit:hits) applyShot(hit,shot.serial,weapon);
  shotSerial_=shot.serial;hasShotSerial_=true;
  return hits.empty()?std::optional<ObjectActionHit>{}:hits.front();
}
std::vector<ObjectActionHit> ObjectActions::knife(const PlayerMeleeEvent &event) {
  std::vector<ObjectActionHit> hits;
  if (event.kind != PlayerMeleeKind::Knife)
    return hits;
  if (event.rays.size() > 64)
    throw std::runtime_error("Object actions: Knife ray budget");
  for (const auto &ray : event.rays)
    if (!finite(ray.first) || !finite(ray.second))
      throw std::runtime_error("Object actions: invalid knife ray");
  for (const auto &ray : event.rays)
    if (const auto hit = target(ray.first, ray.second, 0x10))
      if (applyKnife(*hit, event.serial))
        hits.push_back(*hit);
  return hits;
}

bool ObjectActions::itemVisible(std::size_t index) const {
  if(index>=states_.size() || !states_[index].itemActive || states_[index].itemTaken)
    return false;
  if(!scenario_)return true;
  const auto &item=objects_[index].item;
  // sceAtItemFlgCk gives global item_flg precedence over auto_item_flg.
  if(item.itemFlag)return !scenario_->globalItems.test(item.itemFlag);
  if(item.autoFlag)return !static_cast<const ScenarioState&>(*scenario_).room(room_).items.test(item.autoFlag);
  return true;
}
std::vector<ItemEffectSource> ObjectActions::effectSources() const {
  std::vector<ItemEffectSource> out;
  for(std::size_t index=0;index<objects_.size();++index) {
    if(!itemVisible(index))continue;
    const auto &item=objects_[index].item;const auto &state=states_[index];
    ItemEffectSource s;s.key=0x10000000ULL+objects_[index].placement.slot+1;
    s.id=state.itemId;
    const bool runtime=!(item.controlFlags&1);
    s.effectType=int(ResolveOriginalItemEffectType(s.id,int(item.effect),runtime));
    for(unsigned k=0;k<3;++k)s.position[k]=float(item.position[k]);
    // ctrl0 is emBoxSetItem's original runtime drop at ETS.pos+100;
    // ctrl1 retains the fixed ITA model/effect, including effect0.
    if(runtime)s.position[1]=objects_[index].placement.position[1]+(s.effectType==8?1000:100);
    const std::vector<std::uint8_t> raw(item.raw.begin(),item.raw.end());const AssetReader r{raw};
    s.offset={r.f32(0x6c),r.f32(0x70),r.f32(0x74)};
    s.onModel=itemModels_ && itemModels_->find(s.id) && (!runtime || s.effectType==8);
    if(s.onModel && s.effectType==1)s.parentParts=itemModels_->effectParts(s.id,s.position,item.rotation);
    out.push_back(std::move(s));
  }return out;
}
bool ObjectActions::itemModelVisible(std::size_t index) const {
  if(!itemVisible(index))return false;
  const auto &item=objects_[index].item;
  return (item.controlFlags&1) ||
      ResolveOriginalItemEffectType(states_[index].itemId,int(item.effect),true)==8;
}
std::optional<std::size_t> ObjectActions::pickupTarget(
    const CollisionVec &feet, double heading,
    const std::optional<CollisionVec> &clippedFront) const {
  if (!finite(feet) || !std::isfinite(heading) ||
      (clippedFront && !finite(*clippedFront)))
    return {};
  std::optional<std::size_t> result;
  for (std::size_t index = 0; index < objects_.size(); ++index) {
    if (!itemVisible(index))
      continue;
    const auto &item = objects_[index].item;
    // sceAtCheck_main offsets the model point by +250 Y and its front point
    // by +550 Z. Use the SAT-clipped front supplied by the gameplay caller.
    CollisionVec point = feet;
    point[1] += 250;
    if (item.raw[0x37] & 1)
      point = clippedFront.value_or(
          CollisionVec{feet[0] + std::sin(heading) * 550, feet[1] + 250,
                       feet[2] + std::cos(heading) * 550});
    const auto floor = item.position[1] - 2000;
    const auto radius = item.radius == 0 ? 1500 : item.radius;
    if (point[1] + 100 < floor || point[1] >= floor + 3000 ||
        std::hypot(point[0] - item.position[0],
                   point[2] - item.position[2]) >= radius)
      continue;
    if (item.raw[0x37] & 2) {
      const auto signedValue = [](std::uint8_t v) {
        return v < 128 ? int(v) : int(v) - 256;
      };
      constexpr double radians = 3.14159265358979323846 / 180;
      const auto direction = signedValue(item.raw[0x48]) * 2 * radians;
      const auto opening = signedValue(item.raw[0x49]) * 2 * radians;
      if (std::abs(std::remainder(direction - heading, 2 * radians * 180)) >
          opening)
        continue;
    }
    if (!(item.controlFlags & 4) && effectCollision_) {
      auto a = item.position, b = feet;
      a[1] += 200;
      b[1] = a[1];
      bool visible = !effectCollision_->line(a, b, SatSubset::All, 0, 0);
      if (!visible) {
        a = item.position;
        a[1] += 600;
        b[1] = a[1];
        visible = !effectCollision_->line(a, b, SatSubset::All, 0, 0);
      }
      if (!visible) {
        a = item.position;
        a[1] += 200;
        b = feet;
        b[1] += 1800;
        visible = !effectCollision_->line(a, b, SatSubset::All, 0, 0);
      }
      if (!visible)
        continue;
    }
    // Original OT ordering: higher priority first, disk order for equal
    // priorities. The renderer's ETS index remains stable and independent.
    if (!result || item.raw[0x44] > objects_[*result].item.raw[0x44] ||
        (item.raw[0x44] == objects_[*result].item.raw[0x44] &&
         item.recordIndex < objects_[*result].item.recordIndex))
      result = index;
  }
  return result;
}
std::optional<ObjectPickup> ObjectActions::pickup(
    const CollisionVec &feet, double heading, bool actionPressed,
    const std::optional<CollisionVec> &clippedFront) {
  if (!actionPressed)
    return {};
  const auto index = pickupTarget(feet, heading, clippedFront);
  if (!index)
    return {};
  return takePickup(*index);
}
std::optional<ObjectPickup> ObjectActions::takePickup(std::size_t selected) {
  if (!itemVisible(selected))
    return {};
  const auto index = std::optional<std::size_t>(selected);
  const auto &stateDrop = states_[*index];
  const unsigned itemId = stateDrop.itemId, itemCount = stateDrop.itemCount;
  auto at = std::lower_bound(inventory_.begin(), inventory_.end(), itemId,
                             [](auto &entry, unsigned id) {
                               return entry.id < id;
                             });
  if (at != inventory_.end() && at->id == itemId) {
    if (itemCount > 65535 - at->count)
      return {};
    at->count += itemCount;
  } else {
    inventory_.insert(at, {itemId, itemCount});
  }
  auto &state = states_[*index];
  state.itemActive = false;
  state.itemTaken = true;
  if(scenario_) {
    const auto &item=objects_[selected].item;
    // sceAtGetItem commits sceAtItemFlgOn after acquisition. Found flags
    // belong to separate shoot-down/falling branches, not ordinary taking.
    if(item.itemFlag) {
      scenario_->globalItems.set(item.itemFlag);
    } else if(item.autoFlag) {
      auto &room=scenario_->room(room_);
      room.items.set(item.autoFlag);
    }
  }
  return ObjectPickup{*index, state.slot, itemId, itemCount};
}
bool ObjectActions::consumeItem(unsigned id, unsigned count) {
  if (!count || count > 65535 || id > 0xff)
    return false;
  const auto at = std::lower_bound(inventory_.begin(), inventory_.end(), id,
                                   [](auto &entry, unsigned value) {
                                     return entry.id < value;
                                   });
  if (at == inventory_.end() || at->id != id || at->count < count)
    return false;
  at->count -= count;
  if (!at->count)
    inventory_.erase(at);
  return true;
}
bool ObjectActions::consumeSnapshotItem(ObjectActionsSnapshot &snapshot,
                                        unsigned id, unsigned count) {
  if (!count || count > 65535 || id > 0xff || !validSnapshotShape(snapshot))
    return false;
  const auto at = std::lower_bound(snapshot.inventory.begin(),
                                   snapshot.inventory.end(), id,
                                   [](auto &entry, unsigned value) {
                                     return entry.id < value;
                                   });
  if (at == snapshot.inventory.end() || at->id != id || at->count < count)
    return false;
  at->count -= count;
  if (!at->count)
    snapshot.inventory.erase(at);
  snapshot.checksum = snapshotChecksum(snapshot);
  return true;
}
ObjectActionsSnapshot ObjectActions::snapshot() const {
  ObjectActionsSnapshot result;
  result.room = room_;
  result.profile = profile_;
  result.states = states_;
  result.inventory = inventory_;
  result.shotSerial = shotSerial_;
  result.hasShotSerial = hasShotSerial_;
  result.randomState = randomState_;
  result.noDropCount = noDropCount_;
  result.noRecoveryDropCount = noRecoveryDropCount_;
  result.eventSerial = eventSerial_;
  result.tick = tick_;
  result.checksum = snapshotChecksum(result);
  return result;
}
void ObjectActions::restore(const ObjectActionsSnapshot &snapshot) {
  if (snapshot.room != room_ || snapshot.profile != profile_ ||
      snapshot.states.size() != objects_.size() ||
      !validSnapshotShape(snapshot))
    throw std::runtime_error("Object actions: Snapshot does not belong to this room");
  for (std::size_t index = 0; index < objects_.size(); ++index) {
    const auto &state = snapshot.states[index];
    const auto &object = objects_[index];
    if (state.slot != objects_[index].placement.slot ||
        state.hp != (state.broken ? 0u : 1000u) ||
        state.itemActive != (state.broken && state.itemCount && !state.itemTaken) ||
        (state.itemTaken && (!state.broken || !state.itemCount)) ||
        (state.bombWait && !objects_[index].explosive))
      throw std::runtime_error("Object actions: inconsistent snapshot");
    if (object.item.sourceId == 0x1001 || object.item.sourceId == 0x1002) {
      const auto possible = ObjectRandomDropModelIds(true); // Both source modes.
      if ((state.itemCount &&
           std::find(possible.begin(), possible.end(), state.itemId) ==
               possible.end()) ||
          (!state.broken && state.itemResolved) ||
          (state.itemId == 0x78 && state.itemCount > 990) ||
          (state.itemId == 4 && state.itemCount > 20) ||
          (state.itemId == 0x18 && state.itemCount > 10) ||
          (state.itemId != 0x78 && state.itemId != 4 && state.itemId != 0x18 &&
           state.itemCount > 1))
        throw std::runtime_error("Object actions: invalid random content");
    } else if (!state.itemResolved || state.itemId != object.item.id ||
               state.itemCount != object.item.count)
      throw std::runtime_error("Object actions: modified original content");
  }
  unsigned previous = 0;
  bool first = true;
  std::map<unsigned, unsigned> collected;
  for (std::size_t index = 0; index < objects_.size(); ++index)
    if (snapshot.states[index].itemTaken)
      collected[snapshot.states[index].itemId] +=
          snapshot.states[index].itemCount;
  for (const auto &item : snapshot.inventory) {
    if (item.id > 0xff || !item.count || item.count > 65535 ||
        (!first && item.id <= previous) || item.count > collected[item.id])
      throw std::runtime_error("Object actions: invalid snapshot inventory");
    first = false;
    previous = item.id;
  }
  // Validation finishes before mutation, so a rejected state never alters
  // active room geometry, item visibility or carried inventory.
  states_ = snapshot.states;
  inventory_ = snapshot.inventory;
  shotSerial_ = snapshot.shotSerial;
  hasShotSerial_ = snapshot.hasShotSerial;
  randomState_ = snapshot.randomState;
  noDropCount_ = snapshot.noDropCount;
  noRecoveryDropCount_ = snapshot.noRecoveryDropCount;
  eventSerial_ = snapshot.eventSerial;
  tick_ = snapshot.tick;
  events_.clear(); // Restoring does not replay audio or an already emitted burst.
}
std::string ObjectActions::describe() const {
  unsigned broken = 0, active = 0, taken = 0;
  for (const auto &state : states_) {
    broken += state.broken;
    active += state.itemActive;
    taken += state.itemTaken;
  }
  std::ostringstream out;
  out << "Original containers " << objects_.size() << " · Destroyed " << broken
      << " · Pickups " << active << " · Picked up " << taken;
  for (const auto &item : inventory_)
    out << " · Item " << std::hex << item.id << std::dec << "×" << item.count;
  return out.str();
}
} // namespace re4
