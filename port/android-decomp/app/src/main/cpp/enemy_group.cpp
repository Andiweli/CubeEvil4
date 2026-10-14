#include "enemy_group.h"
#include "object_actions.h"
#include "room_interactions.h"
#include "ports/rnd.h"
#include "save_stats.h"
#include <algorithm>
#include <limits>
#include <cmath>
#include <stdexcept>
namespace re4 {
namespace {
bool pairedOrDown(const EnemyEncounter &enemy) {
  const auto motion=enemy.actor().motion();
  return motion==EnemyMotion::NeckHang || motion==EnemyMotion::ReleaseFailed ||
      motion==EnemyMotion::ThrowOff || motion==EnemyMotion::DownIdle ||
      (motion>=EnemyMotion::BlowFrontFaceUp && motion<=EnemyMotion::BlowDeathFaceDown);
}
struct RandomOwner {
  std::uint16_t &owner;std::uint16_t previous;
  explicit RandomOwner(std::uint16_t &state):owner(state),previous(RndState()) {RndInit(owner);}
  ~RandomOwner() {owner=RndState();RndInit(previous);}
};
}
EnemyGroup::EnemyGroup(const SatCollision &sat,const SatCollision &eat,std::string path)
    :sat_(sat),eat_(eat),enemyDrs_(std::move(path)) {}
EnemyEncounter &EnemyGroup::spawn(const EnemySpawn &source) {
  if(find(unsigned(source.listIndex)) || actors_.size()>=64)
    throw std::runtime_error("Ganado group: duplicate or excess original actor");
  auto enemy=std::make_unique<EnemyEncounter>(sat_,eat_,source,enemyDrs_,true);
  enemy->setDifficulty(rank_,hard_);enemy->setObjectActions(objects_);
  enemy->setRoutes(routes_);
  actors_.push_back(std::move(enemy));return *actors_.back();
}
EnemyEncounter *EnemyGroup::find(unsigned index) {
  for(auto &actor:actors_)if(actor->listIndex()==index)return actor.get();
  return nullptr;
}
const EnemyEncounter *EnemyGroup::find(unsigned index) const {
  for(const auto &actor:actors_)if(actor->listIndex()==index)return actor.get();
  return nullptr;
}
void EnemyGroup::setDifficulty(unsigned rank,bool hard) {
  if(rank>10)throw std::runtime_error("Ganado group: Rank outside 0..10 bounds");
  rank_=rank;hard_=hard;
  for(auto &actor:actors_)actor->setDifficulty(rank,hard);
}
void EnemyGroup::setObjectActions(ObjectActions *objects) {
  objects_=objects;for(auto &actor:actors_)actor->setObjectActions(objects);
}
void EnemyGroup::setRoutes(std::shared_ptr<const EnemyRoutes> routes) {
  routes_=std::move(routes);for(auto &actor:actors_)actor->setRoutes(routes_);
}
void EnemyGroup::setFeedbackCamera(const CollisionVec &eye) {
  for(auto &actor:actors_)actor->setFeedbackCamera(eye);
}
unsigned EnemyGroup::villageAlive() const {
  unsigned count=0;for(const auto &e:actors_)if(e->villageGanado() && e->alive() && !e->scriptSuspended())++count;
  return count;
}
bool EnemyGroup::villageFoundPlayer() const {
  for(const auto &e:actors_)if(e->villageGanado() && e->alive() && !e->scriptSuspended() && e->foundPlayer())return true;
  return false;
}
void EnemyGroup::releaseVillageAlarm() {
  for(auto &e:actors_)if(e->villageGanado() && e->alive() && !e->scriptSuspended())e->releaseVillageAlarm();
}
void EnemyGroup::releaseR100Ambush() {for(auto &actor:actors_)actor->releaseR100Ambush();}
void EnemyGroup::releaseR100HouseAmbush() {for(auto &actor:actors_)actor->releaseR100HouseAmbush();}
void EnemyGroup::releaseR100CliffActors() {for(auto &actor:actors_)actor->releaseR100CliffActor();}
void EnemyGroup::setScriptSuspended(unsigned index,bool value) {
  auto *actor=find(index);if(!actor)throw std::runtime_error("Ganado group: Script actor is missing");
  actor->setScriptSuspended(value);
}
std::vector<EnemyEncounter*> EnemyGroup::ordered(EnemyEncounter *primary) const {
  std::vector<EnemyEncounter*> result;
  if(primary)result.push_back(primary);
  for(const auto &actor:actors_)if(actor.get()!=primary)result.push_back(actor.get());
  return result;
}
void EnemyGroup::contacts(WalkProbe &player,const LeonPreview &leon,const std::vector<EnemyEncounter*> &actors) {
  auto blocks=[&](const EnemyEncounter &enemy) {
    return enemy.alive() && enemy.sourceCollisionActive() && !enemy.scriptSuspended_ && !enemy.downState_ && !pairedOrDown(enemy);
  };
  std::vector<ActorBlocker> playerBlocks;
  for(const auto *enemy:actors)if(blocks(*enemy))
    playerBlocks.push_back({enemy->feet(),400*enemy->scale_,1600*enemy->scale_});
  player.setActorBlockers(std::move(playerBlocks));
  for(auto *enemy:actors) {
    std::vector<ActorBlocker> others;
    if(leon.alive() && !leon.grabbed() && !leon.traversalActive() && blocks(*enemy))
      others.push_back({leon.feet(),200,1600});
    for(const auto *other:actors)if(other!=enemy && blocks(*other))
      others.push_back({other->feet(),enemy->villageGanado_?250:400*other->scale_,1600*other->scale_});
    enemy->probe_.setActorBlockers(std::move(others));
  }
}
bool EnemyGroup::kickAvailable(const LeonPreview &player,const EnemyEncounter *primary) const {
  if(primary && primary->kickAvailable(player))return true;
  for(const auto &enemy:actors_)if(enemy->kickAvailable(player))return true;
  return false;
}
std::optional<AuthoredHitResult> EnemyGroup::target(const CollisionVec &from,const CollisionVec &to,
                                                    unsigned weapon,const EnemyEncounter *primary) const {
  std::optional<AuthoredHitResult> nearest;
  for(const auto *enemy:ordered(const_cast<EnemyEncounter*>(primary)))if(!enemy->scriptSuspended_) {
    const auto hit=enemy->targetWithoutObjects(from,to,weapon);
    if(hit && (!nearest || hit->lenSq<nearest->lenSq))nearest=hit;
  }
  if(nearest && objects_ && objects_->target(from,to,weapon,nearest->lenSq))return {};
  if(nearest && externalTarget_) {
    const auto hit=externalTarget_(from,to,weapon);
    if(hit && hit->lenSq<=nearest->lenSq)return {};
  }
  if(nearest && interiors_) {
    const auto hit=interiors_->target(from,to,weapon);
    // cEmWindow's YAT_FLAG_THROUGH reserves an additional target without
    // shortening the ray; a closed cEmDoor leaf/lock does shorten it.
    if(hit && hit->kind!=InteriorHit::Window && hit->hit.lenSq<=nearest->lenSq)return {};
  }
  return nearest;
}
void EnemyGroup::queueShot(const PlayerShot &shot,EnemyEncounter *primary) {
  queuePlayerHit(&shot,nullptr,{},ordered(primary));
}
void EnemyGroup::queueMelee(const PlayerMeleeEvent &melee,const CollisionVec &player,
                           EnemyEncounter *primary) {
  queuePlayerHit(nullptr,&melee,player,ordered(primary));
}
std::vector<std::optional<SatHit>> EnemyGroup::takeShotScenery(unsigned serial) {
  if(serial!=sceneryShotSerial_)return {};
  auto result=std::move(shotScenery_);shotScenery_.clear();return result;
}
std::vector<bool> EnemyGroup::takeShotExternal(unsigned serial) {
  if(serial!=sceneryShotSerial_)return {};
  auto result=std::move(shotExternal_);shotExternal_.clear();return result;
}
std::vector<bool> EnemyGroup::takeMeleeExternal(unsigned serial) {
  if(serial!=meleeExternalSerial_)return {};
  auto result=std::move(meleeExternal_);meleeExternal_.clear();return result;
}
void EnemyGroup::queuePlayerHit(const PlayerShot *shot,const PlayerMeleeEvent *melee,
                                const CollisionVec &player,const std::vector<EnemyEncounter*> &actors) {
  if(shot && shot->projectile())shot=nullptr;
  if(shot) {
    if(shot->pellets.size()>13)throw std::runtime_error("Ganado group: original shotgun ray budget");
    sceneryShotSerial_=shot->serial;shotScenery_.clear();shotExternal_.clear();
    auto ray=[&](const CollisionVec &from,const CollisionVec &to,bool secondary) {
      const auto weaponNo=shot->weaponNumber();
      std::vector<std::pair<EnemyEncounter*,AuthoredHitResult>> hits;
      for(auto *enemy:actors)if(!enemy->scriptSuspended_ && enemy->alive()) {
        const auto hit=enemy->targetWithoutObjects(from,to,weaponNo);
        if(hit)hits.push_back({enemy,*hit});
      }
      std::stable_sort(hits.begin(),hits.end(),[](const auto &a,const auto &b){return a.second.lenSq<b.second.lenSq;});
      double lengthSq=0;
      for(unsigned k=0;k<3;++k) {const double d=to[k]-from[k];lengthSq+=d*d;}
      if(!std::isfinite(lengthSq) || lengthSq>1e16)
        throw std::runtime_error("Ganado group: invalid firing range");
      float clipping=float(lengthSq);
      const auto wall=eat_.line(from,to,SatSubset::All,0,0x00400000);
      if(wall) {
        float distance=0;for(unsigned k=0;k<3;++k){const float d=float(wall->point[k])-float(from[k]);distance+=d*d;}
        clipping=std::min(clipping,distance);
      }
      auto interior=interiors_?interiors_->target(from,to,weaponNo):std::nullopt;
      if(interior && interior->hit.lenSq>clipping)interior.reset();
      if(interior && interior->kind!=InteriorHit::Window)clipping=interior->hit.lenSq;
      hits.erase(std::remove_if(hits.begin(),hits.end(),[&](const auto &hit){return hit.second.lenSq>=clipping;}),hits.end());
      const auto external=externalTarget_?externalTarget_(from,to,weaponNo):std::nullopt;
      const bool externalInRay=external && std::isfinite(external->lenSq) && external->lenSq>=0 && external->lenSq<clipping;
      if(externalInRay && OriginalShotTargets(weaponNo)==1)
        hits.erase(std::remove_if(hits.begin(),hits.end(),[&](const auto &hit){return hit.second.lenSq>=external->lenSq;}),hits.end());
      // Secondary shotgun pellets set flag4: ordinary cEmBox/Barrel ids3/4
      // are skipped by original GetWepTargetList2. Through-glass remains a
      // separate interior target; non-through door parts clip these rays.
      const auto object=objects_ && !secondary ? objects_->target(from,to,weaponNo,clipping):std::nullopt;
      struct Candidate {float distance;EnemyEncounter *enemy;const AuthoredHitResult *hit;bool object;};
      std::vector<Candidate> candidates;
      for(const auto &hit:hits)candidates.push_back({hit.second.lenSq,hit.first,&hit.second,false});
      if(externalInRay)candidates.push_back({external->lenSq,nullptr,nullptr,false});
      if(object)candidates.push_back({object->hit.lenSq,nullptr,nullptr,true});
      std::stable_sort(candidates.begin(),candidates.end(),[](const auto &a,const auto &b){return a.distance<b.distance;});
      const unsigned targetCount=unsigned(candidates.size())+unsigned(bool(interior));
      shotScenery_.push_back(targetCount?std::nullopt:wall);
      if(OriginalShotCounters(weaponNo,secondary?4u:0u,targetCount).hit)++accuracyHits_;
      const unsigned maximum=OriginalShotTargets(weaponNo);
      bool selectedExternal=false;
      for(unsigned i=0;i<std::min(maximum,unsigned(candidates.size()));++i)
        if(!candidates[i].enemy && !candidates[i].object)selectedExternal=true;
      shotExternal_.push_back(selectedExternal);
      for(unsigned i=0;i<std::min(maximum,unsigned(candidates.size()));++i) {
        const auto &candidate=candidates[i];
        if(candidate.object)objects_->applyShot(*object,shot->serial,weaponNo);
        else if(candidate.enemy) {
          PlayerShot selected=*shot;selected.from=from;selected.to=to;selected.pellets.clear();
          candidate.enemy->queueShot(&selected,secondary);
        }
      }
    };
    if(shot->pellets.empty())ray(shot->from,shot->to,false);
    else for(unsigned i=0;i<shot->pellets.size();++i)ray(shot->pellets[i][0],shot->pellets[i][1],i!=0);
  }
  if(melee) {
    if(melee->kind==PlayerMeleeKind::Knife) {
      meleeExternalSerial_=melee->serial;meleeExternal_.clear();
      // Source GetWepTargetList2 max=1 chooses the nearest actor for each
      // authored blade ray. A miss may hit on a later frame of this swing.
      for(const auto &ray:melee->rays) {
        EnemyEncounter *nearest=nullptr;float distance=std::numeric_limits<float>::max();
        for(auto *enemy:actors)if(enemy->alive() && !enemy->scriptSuspended_) {
          const auto hit=enemy->targetWithoutObjects(ray.first,ray.second,0x10);
          if(hit && hit->lenSq<distance) {nearest=enemy;distance=hit->lenSq;}
        }
        double rayLength=0;for(unsigned k=0;k<3;++k){const double d=ray.second[k]-ray.first[k];rayLength+=d*d;}
        if(!std::isfinite(rayLength) || rayLength>1e16)
          throw std::runtime_error("Ganado group: invalid knife range");
        float clipping=float(rayLength);
        if(const auto wall=eat_.line(ray.first,ray.second,SatSubset::All,0,0)) {
          float len=0;for(unsigned k=0;k<3;++k){const float d=float(wall->point[k])-float(ray.first[k]);len+=d*d;}
          clipping=std::min(clipping,len);
        }
        const auto interior=interiors_?interiors_->target(ray.first,ray.second,0x10):std::nullopt;
        if(interior && interior->kind!=InteriorHit::Window)clipping=std::min(clipping,interior->hit.lenSq);
        if(nearest && distance>=clipping){nearest=nullptr;distance=std::numeric_limits<float>::max();}
        const auto object=objects_?objects_->target(ray.first,ray.second,0x10,nearest?distance:1e16f):std::nullopt;
        const auto external=externalTarget_?externalTarget_(ray.first,ray.second,0x10):std::nullopt;
        const bool selected=external && std::isfinite(external->lenSq) && external->lenSq>=0 &&
            external->lenSq<clipping && external->lenSq<distance &&
            (!object || external->lenSq<object->hit.lenSq);
        meleeExternal_.push_back(selected);
        if(selected)continue;
        if(nearest) {auto selected=*melee;selected.rays={ray};nearest->queueMelee(selected,player);}
        else if(objects_ && !actors.empty()) {auto selected=*melee;selected.rays={ray};actors.front()->queueMelee(selected,player);}
      }
    } else for(auto *enemy:actors)if(!enemy->scriptSuspended_)enemy->queueMelee(*melee,player);
  }
}
void EnemyGroup::stepPlayer(LeonPreview &player,WalkProbe &probe,double turn,double forward,
                            bool runHeld,bool runPressed,double seconds,const PlayerActionInput &input,
                            const std::function<void()> &before,const PlayerSimulationTick &after,
                            EnemyEncounter *primary) {
  auto controls=input;
  if(input.interactPressed || (input.aimHeld && input.firePressed)) {
    for(auto *enemy:ordered(primary))if(enemy->kickAvailable(player)) {
      kickPending_=enemy->listIndex();controls.firePressed=controls.fireHeld=false;break;
    }
  }
  contacts(probe,player,ordered(primary));
  player.step(probe,turn,forward,runHeld,runPressed,seconds,controls,
    [&](const PlayerShot *shot) {
      const auto actors=ordered(primary);
      queuePlayerHit(shot,player.lastMeleeEvent()?&*player.lastMeleeEvent():nullptr,player.feet(),actors);
      if(after)after(shot);
    },[&] {
      if(before)before();
      ++worldTicks_;if(attackWait_)--attackWait_;
      const auto actors=ordered(primary);contacts(probe,player,actors);
      for(auto *enemy:actors) {
        std::vector<GanadoPeerState> peers;
        for(const auto *other:actors)if(other!=enemy && !other->scriptSuspended_)
          peers.push_back(other->sourcePeerState());
        const auto awareness=OriginalGanadoPeers(enemy->feet(),enemy->heading(),peers,
            enemy->villageGanado_?0x101:0x100);
        enemy->setPeerAwareness(awareness.somebodyDamaged,awareness.otherFinding);
        const auto attacks=enemy->axeAttacks_;
        const auto hits=enemy->axeHits_;
        enemy->groupAttackBlocked_=attackWait_!=0;
        enemy->nearPlayerCount_=unsigned(std::count_if(actors.begin(),actors.end(),[&](const auto *other) {
          return other!=enemy && other->alive() && other->sourceCollisionActive() &&
              std::hypot(other->feet()[0]-player.feet()[0],other->feet()[2]-player.feet()[2])<3500;
        }));
        RandomOwner random(enemy->randomState_);enemy->tickCombat(player,probe);
        enemy->groupAttackBlocked_=false;
        if(enemy->axeAttacks_!=attacks) {
          if(rank_<=3)attackWait_=60;
          else if(rank_<=9)attackWait_=30;
        }
        // em10AtkCk renews the shared CTRL12 attack lock on a real hit,
        // independently of the earlier start-of-attack group lock.
        if(enemy->axeHits_!=hits)attackWait_=rank_<=3?60:rank_>6?30:45;
      }
      if(kickPending_) {
        const auto index=*kickPending_;kickPending_.reset();
        for(auto *enemy:actors)if(enemy->listIndex()==index && enemy->kickAvailable(player)) {
          player.beginKick(enemy->feet());break;
        }
      }
      contacts(probe,player,actors);
    });
}
void EnemyGroup::tickScriptActors(LeonPreview &player,WalkProbe &probe,const std::vector<unsigned> &only) {
  for(auto &enemy:actors_)if(!enemy->scriptSuspended_ && (only.empty() || std::find(only.begin(),only.end(),enemy->listIndex())!=only.end())) {
    RandomOwner random(enemy->randomState_);enemy->tickCombat(player,probe);
  }
}
} // namespace re4
