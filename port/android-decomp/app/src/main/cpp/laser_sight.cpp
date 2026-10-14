#include "laser_sight.h"
#include "leon_preview.h"
#include "enemy_encounter.h"
#include "enemy_group.h"
#include "object_actions.h"
#include "room_interactions.h"
#include "forest_animals.h"
#include "dog_traps.h"
#include "village_animals.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace re4 {
LaserSightView ResolveLaserSight(const PlayerShot &ray,
                                 const SatCollision *effect,
                                 const EnemyEncounter *enemy,
                                 const ObjectActions *objects,
                                 const RoomInteractions *interiors,
                                 double maxLineLength, const EnemyGroup *group,
                                 const ForestAnimals *forest,const DogTraps *traps,const VillageAnimals *village) {
  if (!std::isfinite(maxLineLength) || maxLineLength <= 0 || maxLineLength > 100000)
    throw std::runtime_error("Laser: invalid line limit");
  for (const auto *v : {&ray.from, &ray.to})
    for (auto x : *v)
      if (!std::isfinite(x) || std::abs(x) > 1e8)
        throw std::runtime_error("Laser: invalid weapon marker");
  LaserSightView view;
  view.target = ray.to;
  auto lengthSquared = [&](const CollisionVec &cross) {
    double sum = 0;
    for (unsigned k = 0; k < 3; ++k) {
      const auto d = cross[k] - ray.from[k];
      sum += d * d;
    }
    return sum;
  };
  double nearest = lengthSquared(ray.to);
  auto accept = [&](const CollisionVec &cross,unsigned result,std::uint32_t attr=0) {
    const auto distance = lengthSquared(cross);
    if (distance <= nearest) {
      view.target = cross;
      nearest = distance;
      view.hitResult=result;
      view.attributes=attr;view.targetDontFire=false;
    }
  };
  if (effect)
    if (const auto hit = effect->line(ray.from, ray.to, SatSubset::All, 0, 0x00400000))
      accept(hit->point,1,hit->attributes);
  if (enemy)
    if (const auto hit = enemy->target(ray.from, ray.to))
      accept({hit->cross[0], hit->cross[1], hit->cross[2]},2);
  if(group)for(const auto &entry:group->actors())if(entry->visible()&&!entry->scriptSuspended())
    if(const auto hit=entry->target(ray.from,ray.to))
      accept({hit->cross[0],hit->cross[1],hit->cross[2]},2);
  if (objects)
    if (const auto hit = objects->target(ray.from, ray.to,0,float(nearest)))
      accept({hit->hit.cross[0], hit->hit.cross[1], hit->hit.cross[2]},
          objects->objects().at(hit->index).shapes.at(hit->hit.partIndex).flags&HitNoMark?3:2);
  if (interiors)
    if (const auto hit = interiors->target(ray.from, ray.to))
      accept({hit->hit.cross[0], hit->hit.cross[1], hit->hit.cross[2]},2);
  // These are real EmMgr actors with the original posed YARARE_INFO;
  // scroll SetObjSmd police are not EmMgr actors and have no hit shapes.
  if(forest)if(const auto hit=forest->target(ray.from,ray.to,0))
    accept({hit->cross[0],hit->cross[1],hit->cross[2]},2);
  if(traps)if(const auto hit=traps->target(ray.from,ray.to,0,float(nearest)))
    accept({hit->hit.cross[0],hit->hit.cross[1],hit->hit.cross[2]},hit->targetResult);
  if(village)if(const auto hit=village->target(ray.from,ray.to,0,float(nearest)))
    accept({hit->hit.cross[0],hit->hit.cross[1],hit->hit.cross[2]},2);
  // cObjWep::drawLaserSight's early return is before the line's 500-unit
  // muzzle trim. The dot is requested for source result2; result3 is the
  // authored NO_MARK hit, which still clips the line.
  view.distanceSquared=nearest;
  if (nearest < 250000) return view;
  const auto length = std::sqrt(nearest);
  view.line = length > 500;
  view.point = view.hitResult==2 && nearest > 250000;
  for (unsigned k = 0; k < 3; ++k) {
    const auto direction = (view.target[k] - ray.from[k]) / length;
    view.lineStart[k] = ray.from[k] + direction * 500;
    view.lineEnd[k] = view.lineStart[k] + direction * std::min(length - 500, maxLineLength);
  }
  return view;
}
} // namespace re4
