// Frozen game/sce_at.cpp::sceAtSetItem/SceAtItemFlgOn/SceAtItemHitCheck;
// game/item.cpp::itemInfo and cItemMgr::getItem money conversion.
#include "room_pickups.h"
#include "asset_reader.h"
#include "item_rules.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>

namespace re4 {
namespace {
std::int8_t signedByte(unsigned char value) {
  return std::int8_t(value < 128 ? int(value) : int(value) - 256);
}
bool finite(const CollisionVec &point) {
  return std::all_of(point.begin(), point.end(), [](double value) {
    return std::isfinite(value) && std::abs(value) <= 1e7;
  });
}
}
RoomPickups::RoomPickups(std::uint16_t room, const RoomArchive &archive,
                         ScenarioState &state, const SatCollision *effect)
    : room_(room), state_(&state), effect_(effect) {
  const auto *entry = archive.find("ITA");
  if (!entry) return;
  const auto bytes = archive.copy(entry - archive.entries().data());
  const AssetReader r{bytes};
  r.range(0, 16);
  if (bytes.size() > 1024 * 1024 || r.u32(0) != 0x49544100 || r.u16(4) != 0x105)
    throw std::runtime_error("Village items: GC ITA version/budget");
  records_ = r.u16(6);
  if (records_ > 256) throw std::runtime_error("Village items: ITA count limit");
  r.range(16, std::size_t(records_) * 156);
  const auto *itm = archive.find("ITM");
  if (itm) models_ = std::make_shared<RoomItemArchive>(archive.copy(itm - archive.entries().data()));
  const auto &models=models_;
  if(models)for(const auto &asset:models->assets())modelIds_.insert(asset.id);
  std::set<unsigned> areas;
  for (unsigned index = 0; index < records_; ++index) {
    const auto p = 16 + std::size_t(index) * 156;
    if (!areas.insert(bytes[p + 0x36]).second)
      throw std::runtime_error("Village items: Duplicate ITA area number");
    if (bytes[p + 0x35] != 3 || !(bytes[p + 0x34] & 1)) continue;
    if (bytes[p + 0x46]) { ++waiting_; continue; }
    const auto id = r.u16(p + 0x78);
    const auto ctrl = bytes[p + 0x84];
    const auto player = bytes[p + 0x82];
    const auto info = OriginalItemInfo(id);
    // Unsupported records remain disabled, rather than bypassing parent,
    // callback, country, character or falling/shoot-down prerequisites.
    if (id > 255 || !info.type || id == 0x73 || id == 0x75 || (ctrl & 0xd0) ||
        r.u32(p + 0x3c) || r.u32(p + 0x40) || r.u32(p + 0x4c) ||
        bytes[p + 0x52] || (player && !(player & 1)) ||
        bytes[p + 0x39] != 1 || (bytes[p + 0x37] & ~3u)) {
      ++unsupported_; continue;
    }
    RoomPickupRecord item;
    item.id = id; item.controlFlags = ctrl;
    item.itemFlag = r.u16(p + 0x7a); item.autoFlag = r.u16(p + 0x7e);
    if (item.itemFlag >= 128 || item.autoFlag >= 128 ||
        (!item.itemFlag && !item.autoFlag)) {
      ++unsupported_; continue;
    }
    const unsigned count = r.u16(p + 0x7c);
    // cItemMgr::get converts money before its normal inventory branch.
    // Type1 weapons and type9 attachments ignore the authored quantity.
    // Other new inventory entries use the source default and maxNum clamp.
    if (OriginalMoneyValue(id,count))
      item.count = count ? count : id == 0x79 ? 30 : info.defNum;
    else if (info.type == 1 || info.type == 9)
      item.count = 1;
    else
      item.count = std::min(count ? count : info.defNum,info.maxNum);
    if (!item.count || item.count > 65535)
      throw std::runtime_error("Village items: invalid original quantity");
    item.position = {r.f32(p + 0x5c), r.f32(p + 0x60), r.f32(p + 0x64)};
    const auto pitch = r.f32(p + 0x8c), yaw = r.f32(p + 0x90), open = r.f32(p + 0x94);
    const auto radius = r.f32(p + 0x88);
    if (!finite(item.position) || radius < 0 || radius > 100000 ||
        std::abs(pitch) > 100 || std::abs(yaw) > 100 || std::abs(open) > 100)
      throw std::runtime_error("Village items: Position/Angle/Radius");
    item.rotation = open > 0 ? std::array<float, 3>{pitch, yaw, 0}
                             : std::array<float, 3>{0, 0, 0};
    auto &t = item.trigger;
    t.sourceRoom = room; t.recordIndex = index; t.no = bytes[p + 0x36] + 0x80;
    t.enabled = bytes[p + 0x34]; t.hitType = bytes[p + 0x37];
    t.triggerType = 8; t.targetType = 1; t.priority = bytes[p + 0x44];
    t.hitDirection = signedByte(bytes[p + 0x48]); t.hitOpenAngle = signedByte(bytes[p + 0x49]);
    std::copy_n(bytes.begin() + p, 156, t.raw.begin());
    std::copy_n(bytes.begin() + p + 4, 48, t.area.raw.begin());
    if (ctrl & 2) {
      t.area.enabled = bytes[p + 4]; t.area.type = bytes[p + 5];
      t.area.floor = r.f32(p + 8); t.area.height = r.f32(p + 12); t.area.radius = r.f32(p + 16);
      for (unsigned n = 0; n < 8; ++n)
        t.area.points[n / 2][n % 2] = r.f32(p + 20 + n * 4);
    } else {
      // AreaDataInit receives the diameter and stores half as radius.
      t.area.enabled = 1; t.area.type = 2;
      t.area.floor = item.position[1] - 2000; t.area.height = 3000;
      t.area.radius = radius ? radius : 1500;
      t.area.points[0] = {item.position[0], item.position[2]};
    }
    if (t.priority > 15 || t.hitOpenAngle < 0 || t.area.height <= 0 ||
        t.area.radius < 0 || (t.area.type != 1 && t.area.type != 2))
      throw std::runtime_error("Village items: invalid pickup range");
    item.hasModel = models && models->find(id);
    item.effectType=ResolveOriginalItemEffectType(id,bytes[p+0x80],false);
    item.effectOffset={r.f32(p+0x6c),r.f32(p+0x70),r.f32(p+0x74)};
    if(!finite({item.effectOffset[0],item.effectOffset[1],item.effectOffset[2]}))
      throw std::runtime_error("Village items: Original effect offset");
    if (items_.size() == 64) throw std::runtime_error("Village items: Render index budget");
    items_.push_back(std::move(item));
  }
}
bool RoomPickups::taken(const RoomPickupRecord &item) const {
  if(item.runtime)return item.runtimeTaken;
  return item.itemFlag ? state_->globalItems.test(item.itemFlag)
                       : static_cast<const ScenarioState &>(*state_).room(room_).items.test(item.autoFlag);
}
bool RoomPickups::visible(int index) const {
  return index >= 0 && std::size_t(index) < items_.size() &&
         items_[index].eventEnabled && items_[index].hasModel && !taken(items_[index]) &&
         (!items_[index].runtime || items_[index].effectType==8);
}
std::vector<RoomItemPlacement> RoomPickups::renderPlacements() const {
  std::vector<RoomItemPlacement> result;
  for (std::size_t n = 0; n < items_.size(); ++n) {
    const auto &item = items_[n];
    if (!item.hasModel) continue;
    RoomItemPlacement p; p.index = unsigned(n); p.id = item.id; p.rotation = item.rotation;
    for (unsigned k = 0; k < 3; ++k) p.position[k] = float(item.position[k]);
    result.push_back(p);
  }
  return result;
}
bool RoomPickups::lineVisible(const RoomPickupRecord &item, const CollisionVec &feet) const {
  if ((item.controlFlags & 4) || !effect_) return true;
  for (const double height : {200., 600.}) {
    auto a = item.position, b = feet; a[1] += height; b[1] = a[1];
    if (!effect_->line(a, b, SatSubset::All, 0, 0)) return true;
  }
  auto a = item.position, b = feet; a[1] += 200; b[1] += 1800;
  return !effect_->line(a, b, SatSubset::All, 0, 0);
}
std::optional<std::size_t> RoomPickups::target(const CollisionVec &feet, double heading,
    const std::optional<CollisionVec> &front) const {
  if (!finite(feet) || !std::isfinite(heading) || (front && !finite(*front))) return {};
  std::optional<std::size_t> best;
  for (std::size_t n = 0; n < items_.size(); ++n) {
    const auto &item = items_[n];
    if (!item.eventEnabled || taken(item) || !RoomFlow::hit(item.trigger, feet, heading, front) || !lineVisible(item, feet)) continue;
    if (!best || item.trigger.priority > items_[*best].trigger.priority ||
        (item.trigger.priority == items_[*best].trigger.priority &&
         item.trigger.recordIndex < items_[*best].trigger.recordIndex)) best = n;
  }
  return best;
}
std::optional<RoomPickupResult> RoomPickups::take(std::size_t index) {
  const auto &item = items_.at(index);
  if (!item.eventEnabled || taken(item)) return {};
  const unsigned money = OriginalMoneyValue(item.id, item.count);
  if (money) {
    // Original cItemMgr::addMoney clamps to 99999999.
    state_->pesetas = unsigned(std::min(std::uint64_t(99999999), std::uint64_t(state_->pesetas) + money));
  } else {
    const auto at = state_->carriedItems.find(item.id);
    const unsigned previous = at == state_->carriedItems.end() ? 0 : at->second;
    if (item.count > 65535 - previous) return {};
    state_->carriedItems[item.id] = previous + item.count;
  }
  if(item.runtime)items_.at(index).runtimeTaken=true;
  else if (item.itemFlag) {
    state_->globalItems.set(item.itemFlag);
  } else {
    auto &room = state_->room(room_);
    room.items.set(item.autoFlag);
  }
  return RoomPickupResult{index, item.trigger.no, item.id, item.count, money};
}
void RoomPickups::setAuthoredAreaEnabled(unsigned area, bool enabled) {
  auto found=std::find_if(items_.begin(),items_.end(),[&](const auto &item) {
    return !item.runtime && item.trigger.no==area;
  });
  if(found==items_.end())throw std::runtime_error("Item event: linked original ITA area missing");
  found->eventEnabled=enabled;
}
std::size_t RoomPickups::addRuntimeDrop(unsigned id,unsigned count,const CollisionVec &position,int effect) {
  const auto info=OriginalItemInfo(id);
  if(id>255 || !info.type || !finite(position) || items_.size()>=128)
    throw std::runtime_error("Enemy drop: Original ID/position/budget");
  RoomPickupRecord item;item.runtime=true;item.id=id;item.position=position;
  item.effectType=ResolveOriginalItemEffectType(id,effect,true);
  item.position[1]+=item.effectType==8?1000:100;
  item.count=OriginalMoneyValue(id,count)?(count?count:id==0x79?30:info.defNum):
      info.type==1 || info.type==9?1:std::min(count?count:info.defNum,info.maxNum);
  if(!item.count || item.count>65535)throw std::runtime_error("Enemy drop: Original quantity");
  item.hasModel=modelIds_.count(id)!=0;
  // sceAtCheckSaveItem uses literal types5/7. The original money type is8;
  // it therefore retains the source temporary-drop lifetime of61 seconds.
  item.lifetimeTicks=(info.type==5 || info.type==7)?0:61*30;
  auto &t=item.trigger;t.sourceRoom=room_;t.recordIndex=records_+items_.size();
  t.no=0x80+unsigned(items_.size());t.enabled=7;t.hitType=1;t.triggerType=8;
  t.targetType=1;t.priority=8;t.area.enabled=1;t.area.type=2;
  t.area.floor=position[1]-2000;t.area.height=3000;t.area.radius=1500;
  t.area.points[0]={position[0],position[2]};
  items_.push_back(item);return items_.size()-1;
}
std::vector<RoomPickupRecord> RoomPickups::runtimeDrops() const {
  std::vector<RoomPickupRecord> result;
  for(const auto &item:items_)if(item.runtime && !item.runtimeTaken)result.push_back(item);
  return result;
}
std::vector<RoomPickupRecord> RoomPickups::persistentRuntimeDrops() const {
  auto result=runtimeDrops();
  result.erase(std::remove_if(result.begin(),result.end(),[](const auto &item) {
    const auto type=OriginalItemInfo(item.id).type;
    return type!=5 && type!=7;
  }),result.end());
  return result;
}
void RoomPickups::restoreRuntimeDrops(const std::vector<RoomPickupRecord> &saved) {
  for(const auto &item:saved) {
    if(!item.runtime || item.runtimeTaken)throw std::runtime_error("Enemy drop: invalid saved state");
    auto sourcePosition=item.position;sourcePosition[1]-=item.effectType==8?1000:100;
    const auto n=addRuntimeDrop(item.id,item.count,sourcePosition,int(item.effectType));
    items_[n].lifetimeTicks=item.lifetimeTicks;
    items_[n].effectOffset=item.effectOffset;
  }
}
void RoomPickups::tick() {
  for(auto &item:items_)if(item.runtime && !item.runtimeTaken && item.lifetimeTicks && !--item.lifetimeTicks)
    item.runtimeTaken=true;
}
std::vector<ItemEffectSource> RoomPickups::effectSources() const {
  std::vector<ItemEffectSource> out;
  for(std::size_t index=0;index<items_.size();++index) {
    const auto &item=items_[index];if(!item.eventEnabled || taken(item))continue;
    ItemEffectSource s;s.key=0x20000000ULL+index+1;s.id=item.id;s.effectType=int(item.effectType);
    for(unsigned k=0;k<3;++k)s.position[k]=float(item.position[k]);
    s.offset=item.effectOffset;
    s.onModel=item.hasModel && (!item.runtime || item.effectType==8);
    if(s.onModel && item.effectType==1)s.parentParts=models_->effectParts(item.id,s.position,item.rotation);
    s.disappearing=item.runtime && item.lifetimeTicks && item.lifetimeTicks<=6*30;
    out.push_back(std::move(s));
  }
  return out;
}
std::string RoomPickups::describe() const {
  std::ostringstream s;
  s << "ITA " << records_ << " · Available " << items_.size() << " · ITM " << renderPlacements().size()
    << " · Pending " << waiting_ << " · Special cases " << unsupported_;
  return s.str();
}
} // namespace re4
