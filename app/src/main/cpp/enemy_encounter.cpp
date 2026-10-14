#include "enemy_encounter.h"
#include "grab_rules.h"
#include "melee_rules.h"
#include "object_actions.h"
#include "ports/motion64/runtime.h"
#include "weapon_preview.h"
#include "ports/rnd.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
namespace re4 {
namespace {
constexpr float pi = 3.14159265358979323846f;
bool headMotion(EnemyMotion motion) {
  return motion == EnemyMotion::HitHead || motion == EnemyMotion::HeadLeft ||
         motion == EnemyMotion::HeadRight;
}
bool blowMotion(EnemyMotion motion) {
  return motion >= EnemyMotion::BlowFrontFaceUp &&
         motion <= EnemyMotion::BlowDeathFaceDown;
}
EnemyMotion blowMotion(unsigned entry) {
  switch (entry) {
  case 59:
    return EnemyMotion::BlowFrontFaceUp;
  case 63:
    return EnemyMotion::BlowFrontFaceDown;
  case 71:
    return EnemyMotion::BlowBackFaceUp;
  case 73:
    return EnemyMotion::BlowBackFaceDown;
  case 55:
    return EnemyMotion::BlowDeathFaceUp;
  case 61:
    return EnemyMotion::BlowDeathFaceDown;
  default:
    throw std::runtime_error("Ganado: unknown blow selection");
  }
}
class RandomSession {
public:
  explicit RandomSession(std::uint16_t &state)
      : owner_(state), previous_(RndState()) {
    RndInit(owner_);
  }
  ~RandomSession() {
    owner_ = RndState();
    RndInit(previous_);
  }

private:
  std::uint16_t &owner_;
  std::uint16_t previous_;
};
void validPoint(const CollisionVec &point) {
  for (auto coordinate : point)
    if (!std::isfinite(coordinate) || std::abs(coordinate) > 1e8)
      throw std::runtime_error("Ganado: invalid position");
}
} // namespace
EnemyEncounter::EnemyEncounter(const SatCollision &sat, const SatCollision &eat,
                               const EnemySpawn &spawn,
                               const std::string &enemyDrs, bool melee,
                               const std::optional<TestPlacement> &placement)
    : sat_(sat), eat_(eat), probe_(sat, {400, 1600, 2600, 250, 600}),
      actor_(enemyDrs,spawn,melee),
      shapes_(NormalGanadoHitShapes()), heading_(float(spawn.heading)),
      hp_(spawn.hp), maxHp_(spawn.hp) {
  // em10InitYarare widens leg capsules for heavy models0xb/0xc.
  if(spawn.type==11 || spawn.type==12) {
    shapes_.at(2).radius=180;shapes_.at(3).radius=180;
    shapes_.at(6).radius=200;shapes_.at(7).radius=200;
  }
  villageGanado_=(spawn.room==0x101 && spawn.id==0x15) ||
      ((spawn.room==0x103 || spawn.room==0x106) && spawn.id==0x12) ||
      ((spawn.room==0x111 || spawn.room==0x113) && spawn.id==0x17) ||
      (spawn.room==0x11a && spawn.id==0x12);
  firstHouse_=spawn.room==0x100 && spawn.id==0x12 && spawn.listIndex==0xff && spawn.type==0 && spawn.setNo==0x13 && spawn.hp==500 &&
       (spawn.flags==0x21000020u || spawn.flags==0x21000021u);
  r100Ambush_=spawn.room==0x100 && spawn.id==0x12 && spawn.hp==500 && (spawn.listIndex==3 || spawn.listIndex==4 ||
      spawn.listIndex==5 || spawn.listIndex==0x12 || spawn.listIndex==0x13 ||
      spawn.listIndex==0x26 || spawn.listIndex==0x27);
  r100Cliff_=spawn.room==0x100 && spawn.id==0x12 && spawn.hp==1000 && (
      (spawn.listIndex==6 && spawn.type==1 && spawn.setNo==2 && spawn.flags==0x00400030u) ||
      (spawn.listIndex==7 && spawn.type==3 && spawn.setNo==3 && spawn.flags==0x00000030u) ||
      (spawn.listIndex==8 && spawn.type==0 && spawn.setNo==4 && spawn.flags==0x00800030u));
  const bool test=spawn.listIndex==TestSpawn && spawn.type==0 && spawn.flags==0x24;
  if (!hp_ || (!villageGanado_ && (spawn.room != 0x100 || spawn.id != 0x12 ||
      (!test && !firstHouse_ && !r100Ambush_ && !r100Cliff_))))
    throw std::runtime_error("Ganado: unsupported original spawn profile");
  if (placement) {
    validPoint(placement->position);
    if (!std::isfinite(placement->heading))
      throw std::runtime_error("Ganado test: invalid angle");
    heading_ = placement->heading;
  }
  if (r100Cliff_)cliffPosition_=placement ? placement->position : spawn.position;
  else if (!(villageGanado_?probe_.placeAuthored(placement ? placement->position : spawn.position):
      probe_.place(placement ? placement->position : spawn.position)))
    throw std::runtime_error("Ganado test: Original position has no SAT clearance");
  RandomSession random(randomState_);
  // em10_R0_Init, ordinary type0; emset_no is the ESL LIST INDEX.
  switch (spawn.listIndex & 15) {
  case 0:
  case 4:
  case 9:
  case 11:
    scale_ = fRand1_1() * .01f + 1.01f;
    break;
  case 7:
    scale_ = 1.1f;
    break;
  default:
    scale_ = fRand1_1() * .01f + 1.03f;
    break;
  }
  if(villageGanado_ && (spawn.type==11 || spawn.type==12))scale_=1;
  originalScale_ = scale_;
  flags_ = spawn.flags;
  melee_ = melee || firstHouse_ || r100Ambush_ || villageGanado_ || r100Cliff_;
  listIndex_=unsigned(spawn.listIndex);initialSet_=spawn.setNo;
  guardPosition_=spawn.position;guardRadius_=spawn.guardRadius>0?spawn.guardRadius:20000;
  keeper_=spawn.setNo==0 && (spawn.character==1 || spawn.character==3);
  sourceCharacter_=spawn.character;
  foundPlayer_=firstHouse_;
  if(firstHouse_ || r100Ambush_ || villageGanado_) {
    // Exact ordinary em10InitRtnSet draws: Walk, DownCnt, Dash, Breath,
    // Jcatch, Csaw, Claw. The actor's RNG remains a native private owner.
    sourceWalkType_=Rnd()&1;
    (void)Rnd();dashWait_=Rnd()%450+150;breathTimer_=Rnd()%90+90;
    (void)Rnd();(void)Rnd();(void)Rnd();
    actor_.setOriginalWalking(sourceWalkType_,foundPlayer_);
    if(r100Ambush_ || villageGanado_) {
      const auto frames=unsigned(actor_.rig().maxFrame(unsigned(EnemyMotion::Idle)))&0x3fff;
      actor_.start(EnemyMotion::Idle,false,0,10,frames?Rnd()%frames:0);
    }
  }
  if(r100Cliff_) {
    // EmSetFromList2 initializes r_no_2=0 before r100TrapSet installs evtMot.
    // Source phase0 evaluates the ordinary ARC5 wait once and disables HP.
    actor_.start(EnemyMotion::Idle,false,0,0);actor_.advance();hp_=0;
    scriptSuspended_=true;visible_=false;
  }
}
void EnemyEncounter::installR100CliffMotion(const std::vector<std::uint8_t> &motion) {
  if(!r100Cliff_ || cliffPhase_!=1)
    throw std::runtime_error("Ganado: no waiting original r100 boulder actor");
  actor_.installRoomEventMotion(motion);
  if(actor_.rig().maxFrame(*actor_.roomEventAction())<10)
    throw std::runtime_error("Ganado: Original boulder motion too short");
}
void EnemyEncounter::releaseR100CliffActor() {
  if(!r100Cliff_)return;
  if(!actor_.roomEventAction())throw std::runtime_error("Ganado: original boulder motion is missing");
  scriptSuspended_=false;visible_=true;
}
void EnemyEncounter::tickR100Cliff(const CollisionVec &player) {
  if(scriptSuspended_ || cliffPhase_>=3)return;
  if(!actor_.roomEventAction())throw std::runtime_error("Ganado: original boulder motion is missing");
  const auto move=[&] {
    const auto root=actor_.advance();auto position=cliffPosition_;
    const float s=std::sin(heading_),c=std::cos(heading_);
    const float x=float(root.translation[0])*scale_,y=float(root.translation[1])*scale_,z=float(root.translation[2])*scale_;
    position[0]=float(position[0])+c*x+s*z;
    position[1]=float(position[1])+y;
    position[2]=float(position[2])+c*z-s*x;
    cliffPosition_=position;
    heading_+=float(root.yaw);
  };
  if(cliffPhase_==1) {
    const float dx=float(player[0])-float(feet()[0]),dz=float(player[2])-float(feet()[2]);
    // MotionSetCore(...,0,5,0) is deliberately restarted every wait tick.
    actor_.start(EnemyMotion::RoomEvent);move();
    cliffTimer_=(unsigned(actor_.rig().maxFrame(*actor_.roomEventAction()))&0x3fff)-10;
    if(dx*dx+dz*dz<784000000.f || (flags_&1)) {
      cliffPhase_=2;flags_|=1;
      if(initialSet_==2)emitSound(4,EnemySoundChannel::Contact,false,false,6);
      // Source companion scan only marks actors with HP>0; all three
      // initialized r100Cliff actors already have HP0 and are excluded.
    }
  } else {
    move();
    if(cliffTimer_)--cliffTimer_;
    else {
      const float opacity=actor_.opacity()-.1f;
      actor_.setOpacity(std::max(0.f,opacity));
      if(opacity<=0) {
        cliffPhase_=3;sourceDeathInactive_=true;deadSettled_=true;visible_=false;
      }
    }
  }
}
void EnemyEncounter::releaseVillageAlarm() {
  if(!villageGanado_)return;
  foundPlayer_=true;scriptSuspended_=false;
  actor_.setOriginalWalking(sourceWalkType_,true);
}
void EnemyEncounter::resetVillageSpawn(const EnemySpawn &source) {
  if(!villageGanado_ || source.listIndex!=listIndex_)throw std::runtime_error("r101: wrong reset owner");
  if(!probe_.placeAuthored(source.position))throw std::runtime_error("r101: reset position has no floor");
  hp_=maxHp_=source.hp;heading_=float(source.heading);flags_=source.flags;
  visible_=true;scriptSuspended_=false;sourceDeathInactive_=deadSettled_=false;
  pendingHit_.reset();pendingSource_=DamageSource::None;hasSerial_=false;
  downState_=blowClamp_=blowLanded_=reactionGuard_=false;airHeight_=0;
  damageTimer_=reactionTimer_=sourceDeathTimer_=sourceKillCount_=0;
  attackWait_=axeTurnTimer_=0;axeAttackHit_=false;releasePending_.reset();chainsawCatch_=false;
  foundPlayer_=false;sightCount_=0;keeperInvisible_=keeperLeft_=false;
  roofWaiting_=false;roofTurnTimer_=blockedTicks_=dashCycles_=0;locomotionCycle_=false;
  roofDamage_=false;roofPhase_=roofAirTicks_=0;roofVelocityY_=roofTurnRemaining_=0;
  blowFalling_=false;blowFallPhase_=blowAirTicks_=0;blowFallSpeed_={};
  keeper_=source.setNo==0 && (source.character==1 || source.character==3);
  guardPosition_=source.position;guardRadius_=source.guardRadius>0?source.guardRadius:20000;
  scriptWalkPhase_=0;actor_.restoreWeapon();actor_.setOpacity(1);
  actor_.setOriginalWalking(sourceWalkType_,false);startMotion(EnemyMotion::Idle);
}
void EnemyEncounter::releaseR100Ambush() {
  if(!r100Ambush_)return;
  scriptSuspended_=false;
  if(listIndex_==4 || listIndex_==5) {
    flags_|=1;
  }
}
void EnemyEncounter::releaseR100HouseAmbush() {
  if(r100Ambush_ && (listIndex_==3 || listIndex_==4 || listIndex_==5))flags_|=0x80;
}
void EnemyEncounter::beginR100HouseScare() {
  if(listIndex_!=0x25 || initialSet_!=0x1b || actor_.type()!=0 || flags_!=0x24 ||
      hp_!=1000 || r100HouseScare_)
    throw std::runtime_error("Ganado: no original temporary house event");
  RandomSession random(randomState_);
  // Exact random calls in this type0 em10InitRtnSet branch. The existing
  // bounded native encounter owns its RNG; complete EmMgr RNG sharing is
  // outside this scripted routine adapter.
  const auto walkType=Rnd()&1;
  for(unsigned i=0;i<6;++i)(void)Rnd(); // Down/Dash/Breath/Jcatch/Csaw/Claw.
  actor_.prepareHouseScareMotions(walkType);
  const auto frames=unsigned(actor_.rig().maxFrame(unsigned(EnemyMotion::Idle)))&0x3fff;
  actor_.start(EnemyMotion::Idle,false,0,0,frames?Rnd()%frames:0);actor_.advance();
  actor_.start(EnemyMotion::Idle,false,0,0,frames?Rnd()%frames:0);
  // em10_R0_Init immediately invokes source phase0/1 once before returning.
  advanceEnemy();houseScareTimer_=89;houseScarePhase_=1;r100HouseScare_=true;
  scriptSuspended_=false;visible_=true;damageTimer_=0x80;
}
void EnemyEncounter::tickR100HouseScare() {
  if(scriptSuspended_)return;
  damageTimer_=0x80;
  if(houseScarePhase_==1) {
    if(houseScareTimer_) {advanceEnemy();--houseScareTimer_;return;}
    actor_.start(EnemyMotion::HouseTurn);houseScarePhase_=3;
  }
  if(houseScarePhase_==3) {
    advanceEnemy();
    if(actor_.rig().status().state)houseScarePhase_=4;
    return;
  }
  if(houseScarePhase_==4) {actor_.start(EnemyMotion::HouseWalk);houseScarePhase_=5;}
  advanceEnemy();
}
void EnemyEncounter::setDifficulty(unsigned level,bool hard) {
  if(level>10)throw std::runtime_error("Ganado: Original rank outside 0..10 bounds");
  gameLevel_=level;hardMode_=hard;
}
Matrix34 EnemyEncounter::placement() const {
  SmdPlacement placement;
  const auto position = feet();
  for (unsigned k = 0; k < 3; ++k)
    placement.position[k] = float(position[k]);
  placement.rotation[1] = heading_;
  placement.scale = {scale_, scale_, scale_};
  return PlacementMatrix(placement);
}
std::optional<AuthoredHitResult>
EnemyEncounter::target(const CollisionVec &from, const CollisionVec &to,unsigned weapon) const {
  const auto hit = targetWithoutObjects(from, to,weapon);
  if (hit && objects_ && objects_->target(from, to, weapon, hit->lenSq))
    return {};
  return hit;
}
std::optional<AuthoredHitResult>
EnemyEncounter::targetWithoutObjects(const CollisionVec &from,
                                    const CollisionVec &to,unsigned weapon) const {
  if(r100HouseScare_)return {};
  validPoint(from);
  validPoint(to);
  if (!alive() || keeperInvisible_ || pendingHit_ || damageTimer_)
    return {};
  float maximum = 0;
  for (unsigned k = 0; k < 3; ++k) {
    const float distance = float(to[k]) - float(from[k]);
    maximum += distance * distance;
  }
  if (auto wall = eat_.line(from, to, SatSubset::All, 0, weapon==0x10?0:0x00400000)) {
    maximum = 0;
    for (unsigned k = 0; k < 3; ++k) {
      const float distance = float(wall->point[k]) - float(from[k]);
      maximum += distance * distance;
    }
  }
  return HitAuthoredShapes(shapes_, actor_.rig().jointMatrices(), placement(),
                           from, to, maximum, weapon);
}
void EnemyEncounter::tick(const CollisionVec &player, const PlayerShot *shot) {
  RandomSession random(randomState_);
  tickInside(player, shot);
}
void EnemyEncounter::countSourceDeath() {
  if((firstHouse_ || r100Ambush_ || villageGanado_) && !alive() && deadSettled_ && !sourceKillCount_)
    ++sourceKillCount_;
}
GanadoPeerState EnemyEncounter::sourcePeerState() const {
  GanadoPeerState state;state.position=feet();state.hp=int(hp_);
  state.active=alive() || !scenarioDeathReady();state.routine=1;
  const auto motion=actor_.motion();
  if(motion==EnemyMotion::Find)state.subroutine=0xd;
  else if(motion==EnemyMotion::Death){state.routine=3;state.subroutine=2;}
  else if(motion==EnemyMotion::DownDeathFaceUp || motion==EnemyMotion::DownDeathFaceDown) {
    state.routine=3;state.subroutine=1;
  } else if(blowMotion(motion) || headMotion(motion) || motion==EnemyMotion::HitBody ||
      motion==EnemyMotion::BodyAlt || motion==EnemyMotion::BodyRear ||
      motion==EnemyMotion::DownHitFaceUp || motion==EnemyMotion::DownHitFaceDown)
    state.routine=2;
  return state;
}
bool EnemyEncounter::sourceSight(const CollisionVec &player) {
  // EM10_ROUTE_SIGHT_CK alternates a ±200 local-X shoulder probe.
  const float side=(sightCount_++&1)?200.f:-200.f;
  const auto p=feet();const float c=std::cos(heading_),s=std::sin(heading_);
  CollisionVec from{float(p[0])+c*side,float(p[1])+1500.f,float(p[2])-s*side};
  auto to=player;to[1]=float(to[1])+1500.f;
  return !eat_.line(from,to,SatSubset::All,0,0x4000);
}
bool EnemyEncounter::detectPlayer(const CollisionVec &player,float playerHeading) {
  const auto p=feet();const float x=float(player[0])-float(p[0]),z=float(player[2])-float(p[2]);
  GanadoAwarenessInput input;input.type=actor_.type();input.flags=flags_;input.found=foundPlayer_;
  input.lineVisible=sourceSight(player);input.playerFootstep=playerFootstep_;input.playerFired=playerFired_;
  input.somebodyDamaged=somebodyDamaged_;input.damaged=hp_<maxHp_;
  input.distanceSquared=x*x+z*z;
  input.viewAngle=std::abs(std::remainder(std::atan2(x,z)-heading_,2*pi));
  input.heightDelta=float(p[1])-float(player[1]);
  input.routeDistance=routes_?routes_->distance(p,player):std::sqrt(input.distanceSquared);
  if(burstPosition_) {
    input.burst=true;float square=0;
    for(unsigned k=0;k<3;++k){const float delta=float(p[k])-float((*burstPosition_)[k]);square+=delta*delta;}
    input.burstDistanceSquared=square;
  }
  if(OriginalGanadoFind(input)) {
    foundPlayer_=true;keeperLeft_=!keeper_;actor_.setOriginalWalking(sourceWalkType_,true);
    if(!otherFinding_) {
      // em10SetRtnFind selects Walk for a close confrontation, Find for a
      // distant front discovery, and Turn180's alternate ARC16 behind.
      if(input.viewAngle<1.5707964f) {
        const float facing=std::abs(std::remainder(std::atan2(-x,-z)-playerHeading,2*pi));
        if(!(input.distanceSquared<9000000.f || (input.distanceSquared<49000000.f && facing<.7853982f)))
          startMotion(EnemyMotion::Find);
      } else {
        const float direction=std::remainder(std::atan2(x,z)-heading_,2*pi);
        startMotion(EnemyMotion::Turn180Alternate,direction<0);
        discoveryTurnTarget_=std::remainder(heading_+pi,2*pi);
      }
    }
    return true;
  }
  return false;
}
bool EnemyEncounter::tickDiscovery(const CollisionVec &player) {
  const auto motion=actor_.motion();
  if(motion<EnemyMotion::Find || motion>EnemyMotion::Turn180Alternate)return false;
  const auto p=feet();const float angle=std::remainder(std::atan2(float(player[0]-p[0]),float(player[2]-p[2]))-heading_,2*pi);
  const auto free=actor_.rig().sequenceFree();
  if(motion==EnemyMotion::Find) {
    heading_=std::remainder(heading_+std::clamp(angle,-.19634955f,.19634955f),2*pi);
    if((free&0x20) && !discoveryVoiceDone_) {
      discoveryVoiceDone_=true;emitSound(voiceCue(4),EnemySoundChannel::Voice,true);
      emitSound(0,EnemySoundChannel::Breath,false,true);
    }
  } else {
    if(free&8) {
      const float a=std::clamp(std::remainder(std::atan2(float(player[0]-p[0]),float(player[2]-p[2]))-discoveryTurnTarget_,2*pi),-.09817477f,.09817477f);
      discoveryTurnTarget_=std::remainder(discoveryTurnTarget_+a,2*pi);
      heading_=std::remainder(heading_+a,2*pi);
    }
    if(free&4)heading_=std::remainder(heading_+std::clamp(angle,-.09817477f,.09817477f),2*pi);
  }
  previousSoundFree_=free;advanceEnemy();
  if(actor_.rig().status().ended)startMotion(motion==EnemyMotion::Turn180Alternate?EnemyMotion::Idle:EnemyMotion::Walk);
  return true;
}
void EnemyEncounter::stepPlayer(LeonPreview &player, WalkProbe &probe,
                                double turn, double forward, bool runHeld,
                                bool runPressed, double seconds,
                                const PlayerActionInput &input,
                                const std::function<void()> &beforeWorldTick,
                                const PlayerSimulationTick &afterWorldTick) {
  RandomSession random(randomState_);
  auto controls = input;
  if (melee_ && player.hasMeleeMotions()) {
    // Edges may arrive on a render frame without a 30-Hz simulation tick.
    // RT while aiming is also accepted by the original contextual action.
    if (kickAvailable(player) &&
        (input.interactPressed || (input.aimHeld && input.firePressed))) {
      kickPending_ = true;
      controls.firePressed = controls.fireHeld = false;
    }
  }
  auto contact = [&] {
    // Original paired catch changes both actors to PRI_LV1. Its half-metre
    // spacing must not be blocked by the unrelated upright contact adapter.
    const auto motion = actor_.motion();
    const bool paired = motion == EnemyMotion::NeckHang ||
                        motion == EnemyMotion::ReleaseFailed ||
                        motion == EnemyMotion::ThrowOff || blowMotion(motion) ||
                        motion == EnemyMotion::DownIdle;
    probe_.setActorBlocker(player.alive() && !player.grabbed() && !player.traversalActive() && !paired && !downState_
                              ? std::optional<ActorBlocker>(ActorBlocker{player.feet(),200,1600}) : std::nullopt);
    probe.setActorBlocker(alive() && !keeperInvisible_ && !paired && !downState_
                              ? std::optional<ActorBlocker>(ActorBlocker{
                                    feet(), 400 * scale_, 1600 * scale_})
                              : std::nullopt);
  };
  contact();
  if (player.hasGrabMotions()) {
    // game.cpp orders EmMgr before PlMove. Queue a real player shot after its
    // solved pose; apply it at the beginning of the following enemy tick.
    player.step(
        probe, turn, forward, runHeld, runPressed, seconds, controls,
        [&](const PlayerShot *shot) {
          queueShot(shot);
          if (player.lastMeleeEvent())
            queueMelee(*player.lastMeleeEvent(), player.feet());
          if (afterWorldTick)
            afterWorldTick(shot);
        },
        [&] {
          if (beforeWorldTick)
            beforeWorldTick();
          tickCombat(player, probe);
          if (kickPending_) {
            kickPending_ = false;
            if (kickAvailable(player))
              player.beginKick(feet());
          }
          contact();
        });
    return;
  }
  player.step(probe, turn, forward, runHeld, runPressed, seconds, input,
              [&](const PlayerShot *shot) {
                tickInside(player.feet(), shot);
                contact();
                if (afterWorldTick)
                  afterWorldTick(shot);
              }, beforeWorldTick);
}
bool EnemyEncounter::applyBlast(const ObjectBlast &blast, LeonPreview &player,
                                WalkProbe &probe) {
  if(r100HouseScare_)return false;
  validPoint(blast.position);
  // obj2d uses 6000, em2aTrap2Bomb emits two 3000-unit spheres. Both
  // register the same original weapon kind 0x13 with PlWepHitCheck2.
  if ((blast.weaponNo!=18 && blast.weaponNo!=19 && blast.weaponNo!=20 && blast.weaponNo!=23) ||
      !std::isfinite(blast.radius) || blast.radius<=0 || blast.radius>15000 || blast.origin>1)
    throw std::runtime_error("Ganado: unsupported source explosion");
  if(blast.origin) {
    if(blast.serial<=playerBlastSerial_)return false;
    playerBlastSerial_=blast.serial;
  } else {
    if(hasBlastSerial_ && blast.serial<=blastSerial_)return false;
    hasBlastSerial_=true;blastSerial_=blast.serial;
  }
  if (!alive() || pendingHit_ || damageTimer_)
    return false;
  const auto hit = SphereAuthoredShapes(shapes_, actor_.rig().jointMatrices(),
                                        placement(), blast.position,
                                        blast.radius, blast.weaponNo, 2000);
  if (!hit || (MeleeSphereNeedsScenarioCheck(*hit, blast.radius) &&
               eat_.line(blast.position,
                         {hit->center[0], hit->center[1], hit->center[2]},
                         SatSubset::All, 0,
                         0x00400000)))
    return false;
  pendingHit_ = AuthoredHitResult{hit->partIndex, hit->lenSq, hit->center};
  pendingSource_ = blast.weaponNo==23?DamageSource::Flash:DamageSource::Blast;
  pendingWeaponNo_=blast.weaponNo;
  pendingFrom_ = blast.position;
  // A blast can interrupt the bounded paired-contact adapter. Player damage
  // was already applied by the room runtime and must never be applied twice.
  if (player.blastActive()) {
    releasePending_.reset();
    catchTurn_ = 0;
    catchAdjustment_ = {};
    probe.setActorBlocker(std::nullopt);
  }
  return true;
}
bool EnemyEncounter::kickAvailable(const LeonPreview &player) const {
  if (!melee_ || !player.hasMeleeMotions() || !alive() || !player.alive() ||
      player.grabbed() || player.action() == PlayerAction::Kick ||
      player.action() == PlayerAction::GrabRelease ||
      player.action() == PlayerAction::GrabEscape ||
      player.action() == PlayerAction::Recover ||
      (!headMotion(actor_.motion()) && !headPromptTick_ &&
       !(actor_.motion()>=EnemyMotion::FlashStart && actor_.motion()<=EnemyMotion::FlashFinish)) ||
      !KickPromptGeometry(player.feet(), player.heading(), feet()))
    return false;
  auto from = player.feet(), to = feet();
  from[1] += 1500;
  to[1] += 1500;
  return !sat_.line(from, to, SatSubset::All, 0, 0);
}
void EnemyEncounter::queueMelee(const PlayerMeleeEvent &event,
                                const CollisionVec &playerFeet) {
  if(r100HouseScare_)return;
  validPoint(playerFeet);
  // Original damage flag protects a queued hit, not a whole swing serial.
  // A missed early sweep may hit later; an accepted knife hit starts timer17.
  if (event.kind == PlayerMeleeKind::Knife) {
    bool canDamageEnemy = melee_ && alive() && !pendingHit_ && !damageTimer_;
    if (!canDamageEnemy && !objects_)
      return;
    for (const auto &ray : event.rays) {
      validPoint(ray.first);
      validPoint(ray.second);
      float maximum = 1e16f;
      auto to = ray.second;
      if (const auto wall = eat_.line(ray.first, to, SatSubset::All, 0, 0)) {
        maximum = 0;
        for (unsigned k = 0; k < 3; ++k) {
          const float delta = float(wall->point[k]) - float(ray.first[k]);
          maximum += delta * delta;
        }
      }
      const auto hit = canDamageEnemy
                           ? HitAuthoredShapes(
                                 shapes_, actor_.rig().jointMatrices(),
                                 placement(), ray.first, ray.second, maximum,
                                 0x10)
                           : std::nullopt;
      // GetWepTargetList2 exempts cEmBox / cEmItem from the EAT limit for
      // weapon 0x10. The authored blade sweep still determines the hit;
      // select its nearest eligible actor as the source max=1 list does.
      const auto object = objects_
                              ? objects_->target(ray.first, ray.second, 0x10,
                                                 hit ? hit->lenSq : 1e16f)
                              : std::nullopt;
      if (object && (!hit || object->hit.lenSq <= hit->lenSq)) {
        objects_->applyKnife(*object, event.serial);
        continue;
      }
      if (hit) {
        pendingHit_ = *hit;
        pendingSource_ = DamageSource::Knife;
        pendingFrom_ = ray.first;
        ++knifeHits_;
        if (!objects_)
          return;
        canDamageEnemy = false;
      }
    }
  } else {
    if (!melee_ || !alive() || pendingHit_ || damageTimer_)
      return;
    for (const auto &center : event.centers) {
      validPoint(center);
      const auto hit =
          SphereAuthoredShapes(shapes_, actor_.rig().jointMatrices(),
                               placement(), center, event.radius);
      if (!hit)
        continue;
      const CollisionVec capsule{hit->center[0], hit->center[1],
                                 hit->center[2]};
      if (MeleeSphereNeedsScenarioCheck(*hit, event.radius) &&
          eat_.line(center, capsule, SatSubset::All, 0, 0x00400000))
        continue;
      pendingHit_ = AuthoredHitResult{hit->partIndex, hit->lenSq, hit->center};
      pendingSource_ = DamageSource::Kick;
      pendingFrom_ = center;
      ++kickHits_;
      return;
    }
  }
}
void EnemyEncounter::tickInside(const CollisionVec &player,
                                const PlayerShot *shot) {
  validPoint(player);
  if(r100Cliff_) {tickR100Cliff(player);return;}
  if(r100HouseScare_) {tickR100HouseScare();return;}
  if (shot) {
    validPoint(shot->from);
    validPoint(shot->to);
    double lengthSq = 0;
    for (unsigned k = 0; k < 3; ++k) {
      const double delta = shot->to[k] - shot->from[k];
      lengthSq += delta * delta;
    }
    if (lengthSq > 1e16)
      throw std::runtime_error("Ganado: invalid firing range");
  }
  // Original order: enemy manager before player. A shot queued this tick is
  // applied next tick; ordinary bullet damage then leaves timer1 until the
  // following enemy tick. This is not a ten-tick or whole-flinch immunity.
  countSourceDeath();
  soundTick();
  applyPendingDamage();
  if(tickRoofDamage()){queueShot(shot);return;}
  if(tickBlowFall()){queueShot(shot);return;}
  if(tickFlash()){queueShot(shot);return;}
  if (!deadSettled_) {
    const auto motion = actor_.motion();
    if (alive() &&
        (motion == EnemyMotion::Idle || motion == EnemyMotion::Walk)) {
      const auto position = feet();
      const float dx = float(player[0] - position[0]);
      const float dz = float(player[2] - position[2]);
      const bool approach = std::hypot(dx, dz) > 1800;
      const auto next = approach ? EnemyMotion::Walk : EnemyMotion::Idle;
      if (next != motion)
        startMotion(next);
      if (approach) {
        const float angle =
            std::remainder(std::atan2(dx, dz) - heading_, 2 * pi);
        heading_ = std::remainder(
            heading_ + std::clamp(angle * .3f, -.15707964f, .15707964f),
            2 * pi);
      }
    }
    advanceEnemy();
    if (!alive()) {
      deadSettled_ = actor_.rig().status().ended;
      if(firstHouse_ || r100Ambush_ || villageGanado_) {
        if(deadSettled_)sourceDeathInactive_=true;
        else if(motion==EnemyMotion::Death && sourceDeathTimer_ && --sourceDeathTimer_==0)
          sourceDeathInactive_=true;
      }
    } else if (actor_.motion() == EnemyMotion::HitHead ||
               actor_.motion() == EnemyMotion::HitBody) {
      if (actor_.rig().status().ended ||
          (actor_.motion() == EnemyMotion::HitBody &&
           (actor_.rig().sequenceFree() & 4)))
        startMotion(EnemyMotion::Idle);
    }
  }
  queueShot(shot);
}
void EnemyEncounter::applyPendingDamage() {
  if (damageTimer_)
    --damageTimer_;
  if (!pendingHit_)
    return;
  if(pendingSource_==DamageSource::Flash) {
    actor_.start(EnemyMotion::FlashStart,(flags_&0x01000000u)!=0,0,6,Rnd()%5);
    flashCycles_=2;reactionGuard_=true;downState_=false;
    reactionTimer_=15;damageTimer_=15;foundPlayer_=true;
    emitSound(voiceCue(4),EnemySoundChannel::Voice);
    lastDamage_=0;lastDamageSource_=DamageSource::Flash;
    pendingHit_.reset();pendingSource_=DamageSource::None;return;
  }
  lastHead_ = shapes_.at(pendingHit_->partIndex).partsNo == 5;
  lastDamageSource_ = pendingSource_;
  const auto firearm=pendingSource_==DamageSource::Bullet
      ?GanadoFirearmHitDamage(pendingWeaponNo_,pendingPowerLevel_,pendingHit_->lenSq,lastHead_):GanadoFirearmDamage{};
  lastCritical_=pendingSource_==DamageSource::Bullet?firearm.critical:
      pendingSource_==DamageSource::Blast && pendingWeaponNo_==19 && lastHead_ && Rnd()%12==6;
  const unsigned uncappedDamage =
      pendingSource_ == DamageSource::Knife  ? KnifeDamage(lastHead_)
      : pendingSource_ == DamageSource::Kick ? KickDamage(lastHead_)
      : pendingSource_ == DamageSource::Blast
          ? (pendingWeaponNo_==19 ? (lastHead_ ? (lastCritical_ ? 25000 : 3000) : 2500) :
             pendingWeaponNo_==18?9999u:200u)
      : firearm.damage;
  // em10SetDmVal clamps its final weapon/part result before LifeDownSet2;
  // a barrel's2500*10 head critical therefore becomes9999, not25000.
  const unsigned damage = std::min(uncappedDamage, 9999u);
  // LifeDownSet2 consumes these even with rnd=0, before reaction RNG.
  (void)Rnd();
  (void)Rnd();
  lastDamage_ = std::min(hp_, damage);
  hp_ -= lastDamage_;
  deadSettled_ = false;
  emitDamageFeedback();
  if(!roofDamage_ && !downState_ && (!alive() || !reactionGuard_) && beginRoofDamage()) {
    // em10RoofDmCk precedes the ordinary head/body/death reaction. Walking
    // keeps the roof edge stop; this separate routine may cross that edge.
  } else if (pendingSource_ == DamageSource::Kick ||
      (pendingSource_ == DamageSource::Blast && melee_ &&
       (!alive() || (!reactionGuard_ && !downState_)))) {
    // Original em10DmSetWep09 selects Dm_Blow. Close-range dead-part
    // separation needs its separate gib actor/model; this selected adapter
    // keeps the complete body's original authored death-blow motion.
    const auto choice =
        SelectMeleeBlow(feet(), heading_, pendingFrom_, alive());
    heading_ = choice.heading;
    downFacing_ = choice.downFacing;
    blowFallSpeed_=choice.fallSpeed;blowFalling_=false;blowFallPhase_=blowAirTicks_=0;
    blowClamp_ = blowLanded_ = downState_ = false;
    reactionGuard_ = true;
    startMotion(blowMotion(choice.motionEntry), choice.mirrored,
                 choice.rootScale);
  } else if (melee_ && alive() && reactionGuard_) {
    // Be_flg8 precedes Be_flg0x10: accept HP damage while the previous
    // protected handler remains active, with no MotionSet/reaction RNG.
    wakeTimer_ = 0;
  } else if (melee_ && downState_) {
    wakeTimer_ = 0;
    if (alive()) {
      startMotion(downFacing_ ? EnemyMotion::DownHitFaceUp
                               : EnemyMotion::DownHitFaceDown,
                   actor_.mirrored());
      reactionGuard_ = true;
    } else
      startMotion(downFacing_ ? EnemyMotion::DownDeathFaceUp
                               : EnemyMotion::DownDeathFaceDown,
                   false);
  } else if (pendingSource_ == DamageSource::Knife && alive() && !lastHead_ &&
             !KnifeBodyFlinches()) {
    // Source's one-in-three body branch accepts HP damage without a MotionSet.
  } else {
    auto reaction = !hp_        ? EnemyMotion::Death
                    : lastHead_ ? EnemyMotion::HitHead
                                : EnemyMotion::HitBody;
    bool mirrored = !lastHead_ && !actor_.mirrored();
    if (melee_ && lastHead_ && alive()) {
      const auto position = feet();
      const float angle = std::remainder(
          std::atan2(float(pendingFrom_[0]) - float(position[0]),
                     float(pendingFrom_[2]) - float(position[2])) -
              heading_,
          2 * pi);
      const unsigned selection =
          std::abs(angle) < pi / 2 ? Rnd() % 3 : Rnd() & 1;
      reaction = selection == 1   ? EnemyMotion::HeadLeft
                 : selection == 2 ? EnemyMotion::HeadRight
                                  : EnemyMotion::HitHead;
      mirrored = (flags_ & 0x1000000) != 0;
    }
    if (melee_ && alive() && !lastHead_) {
      const auto position = feet();
      const float angle = std::remainder(
          std::atan2(float(pendingFrom_[0]) - float(position[0]),
                     float(pendingFrom_[2]) - float(position[2])) -
              heading_,
          2 * pi);
      // All ordinary non-head parts retain their authored HP/geometry. This
      // selected adapter maps arm/leg flinches to the original torso branch;
      // limb-specific knee/drop states are not implemented yet.
      if (std::abs(angle) < pi / 2)
        reaction = Rnd() % 3 == 0 ? EnemyMotion::HitBody : EnemyMotion::BodyAlt;
      else
        reaction = EnemyMotion::BodyRear;
      // DM_SMALL_WEP_MOT(159/15A,15B/15C) restores the held hoe's
      // authored mirror instead of the ordinary alternating torso mirror.
      if(actor_.weaponType()==1)mirrored=(flags_&0x01000000u)!=0;
    }
    startMotion(reaction, mirrored);
    reactionGuard_ =
        melee_ && (headMotion(reaction) || reaction == EnemyMotion::BodyRear);
    reactionTimer_ = headMotion(reaction)                ? 15
                     : reaction == EnemyMotion::BodyRear ? 10
                                                         : 0;
  }
  pendingHit_.reset();
  damageTimer_ = pendingSource_ == DamageSource::Knife ? 17
                 : pendingSource_ == DamageSource::Blast ? (pendingWeaponNo_==20?15:30) : 1;
  pendingSource_ = DamageSource::None;
}
bool EnemyEncounter::tickFlash() {
  const auto motion=actor_.motion();
  if(motion<EnemyMotion::FlashStart || motion>EnemyMotion::FlashFinish || !alive())return false;
  if(reactionTimer_ && !--reactionTimer_)reactionGuard_=false;
  advanceEnemy();
  if(motion==EnemyMotion::FlashStart && actor_.rig().status().ended) {
    startMotion(EnemyMotion::FlashRecover,(flags_&0x01000000u)!=0);
    flashCycles_=Rnd()%5+5;reactionGuard_=false;
  } else if(motion==EnemyMotion::FlashRecover && actor_.rig().status().state) {
    if(flashCycles_)--flashCycles_;
    else startMotion(EnemyMotion::FlashFinish,(flags_&0x01000000u)!=0);
  } else if(motion==EnemyMotion::FlashFinish && actor_.rig().status().ended)startMotion(EnemyMotion::Walk);
  return true;
}
std::vector<std::optional<SatHit>> EnemyEncounter::takeShotScenery(unsigned serial) {
  if(serial!=sceneryShotSerial_)return {};
  auto result=std::move(shotScenery_);shotScenery_.clear();return result;
}
void EnemyEncounter::queueShot(const PlayerShot *shot,bool skipObjects) {
  if(r100HouseScare_)return;
  if(shot && shot->projectile())return;
  if (shot) {
    validPoint(shot->from);
    validPoint(shot->to);
    if(shot->powerLevel>6)throw std::runtime_error("Ganado: invalid original weapon rank");
    if(shot->pellets.size()>13)throw std::runtime_error("Ganado: original shotgun ray budget");
  }
  if (shot && (shot->flightImpact || !hasSerial_ || shot->serial > lastSerial_)) {
    hasSerial_ = true;
    lastSerial_ = shot->serial;
    ++shots_;
    const auto weaponNo=shot->weaponNumber();
    sceneryShotSerial_=shot->serial;shotScenery_.clear();
    auto ray=[&](const CollisionVec &from,const CollisionVec &to,bool secondary) {
      validPoint(from);validPoint(to);
      double lengthSq=0;for(unsigned k=0;k<3;++k){const double d=to[k]-from[k];lengthSq+=d*d;}
      if(!std::isfinite(lengthSq) || lengthSq>1e16)
        throw std::runtime_error("Ganado: invalid firing range");
      const auto wall=eat_.line(from,to,SatSubset::All,0,0x00400000);
      float clipping=1e16f;
      if(wall) {clipping=0;for(unsigned k=0;k<3;++k){const float d=float(wall->point[k])-float(from[k]);clipping+=d*d;}}
      const auto hit=targetWithoutObjects(from,to,weaponNo);
      const auto object=objects_ && !skipObjects && !secondary
          ?objects_->target(from,to,weaponNo,hit?hit->lenSq:clipping):std::nullopt;
      shotScenery_.push_back(hit || object?std::nullopt:wall);
      if(object)objects_->applyShot(*object,shot->serial,weaponNo);
      else if(hit) {
        pendingHit_=*hit;pendingSource_=DamageSource::Bullet;
        pendingWeaponNo_=weaponNo;pendingPowerLevel_=shot->powerLevel;
        pendingFrom_=from;++hits_;
      }
    };
    if(shot->pellets.empty())ray(shot->from,shot->to,false);
    else for(unsigned i=0;i<shot->pellets.size();++i)ray(shot->pellets[i][0],shot->pellets[i][1],i!=0);
  }
}
void EnemyEncounter::advanceEnemy(bool coupled) {
  if (coupled) {
    CollisionVec delta{};
    for (unsigned k : {0u, 2u}) {
      delta[k] = catchAdjustment_[k] * .2;
      catchAdjustment_[k] -= delta[k];
    }
    probe_.moveDelta(delta);
  }
  const float rootHeading = heading_ + (coupled ? catchTurn_ : 0);
  const auto previousFeet=feet();
  const auto previousBody=TransformPoint(WeaponAttachmentMatrix(actor_.rig().jointMatrices().at(0),placement()),{0,0,0});
  const auto root = actor_.advance();
  // MotionMove returns 1/2 at a loop boundary and 4/8 at a clamp end.
  // root.ended intentionally represents only the latter, so it cannot
  // gate the original Walk->Dash decision on a looping walking clip.
  locomotionCycle_=actor_.rig().status().state!=0;
  const float c = std::cos(rootHeading), s = std::sin(rootHeading);
  const double previousY = feet()[1];
  const CollisionVec delta{scale_ * (c * root.translation[0] + s * root.translation[2]),
       root.translation[1],
       scale_ * (-s * root.translation[0] + c * root.translation[2])};
  if(villageGanado_ && !coupled &&
      (actor_.motion()==EnemyMotion::Idle || actor_.motion()==EnemyMotion::Walk || actor_.motion()==EnemyMotion::Dash ||
       actor_.motion()==EnemyMotion::AxeAttack || actor_.motion()==EnemyMotion::HoeAttack ||
       actor_.motion()==EnemyMotion::HoeAttackHigh || actor_.motion()==EnemyMotion::HoeAttackLow)) {
    const auto body=TransformPoint(WeaponAttachmentMatrix(actor_.rig().jointMatrices().at(0),placement()),{0,0,0});
    probe_.moveGanadoRoot(delta,{previousBody[0],previousBody[1],previousBody[2]},{body[0],body[1],body[2]});
  } else if(villageGanado_ && blowMotion(actor_.motion())) {
    // Dm_Blow disables scenery adjustment while airborne. Applying the
    // grounded WalkProbe here used to pin knocked roof guards to the edge.
    auto p=previousFeet;for(unsigned k=0;k<3;++k)p[k]+=delta[k];
    probe_.setScriptPosition(p);airHeight_=0;
  } else probe_.moveDelta(delta);
  const double intended=std::hypot(delta[0],delta[2]),moved=std::hypot(feet()[0]-previousFeet[0],feet()[2]-previousFeet[2]);
  if(intended>.01 && moved<intended*.5)++blockedTicks_;else if(blockedTicks_)--blockedTicks_;
  if (blowMotion(actor_.motion()) && !villageGanado_)
    airHeight_ = std::max(0., previousY + scale_ * root.translation[1] -
                                  probe_.position()[1]);
  const float turn = coupled ? catchTurn_ * .3f : 0;
  // EmCatchMotionMove restores old yaw + smoothed Catch_dir, discarding
  // the temporary MotionMove root yaw while the pair remains coupled.
  heading_ =
      std::remainder(heading_ + turn + (coupled ? 0 : float(root.yaw)), 2 * pi);
  catchTurn_ -= turn;
  sequenceFootstep();
}
void EnemyEncounter::sequenceFootstep() {
  // cEm10::move calls em10FootSe after the original MotionMove and pose.
  // r100 uses ordinary types0/1/3/4 on dry ground: Se0..3 select the room
  // material bank, at the authored left/right foot parts, not EM12 bank8.
  const auto sound=actor_.rig().consumeSequenceSound();
  if(!sound || sound->se>4)return;
  const unsigned se=sound->se-1,part=(se&1)?0x19:0x15;
  const auto &matrix=actor_.rig().jointMatrices().at(part);
  const auto point=TransformPoint(placement(),{matrix[3],matrix[7],matrix[11]});
  if(soundEvents_.size()==128)soundEvents_.erase(soundEvents_.begin());
  soundEvents_.push_back({5,0x10+se,{point[0],point[1],point[2]},
      EnemySoundChannel::Impact,false,false});
}
std::vector<EnemySoundEvent> EnemyEncounter::drainSounds() {
  auto events = std::move(soundEvents_);
  soundEvents_.clear();
  return events;
}
std::vector<EnemyEffectEvent> EnemyEncounter::drainEffects() {
  auto events = std::move(effectEvents_);
  effectEvents_.clear();
  return events;
}
void EnemyEncounter::setFeedbackCamera(const CollisionVec &eye) {
  validPoint(eye);
  feedbackCamera_ = eye;
}
CollisionVec EnemyEncounter::soundPosition(EnemySoundChannel channel) const {
  if(channel!=EnemySoundChannel::Voice)return feet();
  // ctrl11.cpp::Ctrl11SetSe2 follows parts0.world throughout playback.
  const auto origin=TransformPoint(WeaponAttachmentMatrix(actor_.rig().jointMatrices().at(0),placement()),{0,0,0});
  return {origin[0],origin[1],origin[2]};
}
void EnemyEncounter::emitSound(unsigned number, EnemySoundChannel channel,
                               bool replace, bool stop, unsigned block) {
  // Fixtures/paused consumers may leave events undrained. Audio must never
  // block original simulation or grow without bound in that situation.
  if (soundEvents_.size() == 128) soundEvents_.erase(soundEvents_.begin());
  const auto source = soundPosition(channel);
  soundEvents_.push_back({block, number, source, channel, stop, replace});
}
bool EnemyEncounter::beginRoofDamage() {
  if(!villageGanado_)return false;
  // em10RoofDmCk: the mask is a returned surface attribute, not the SAT
  // exclusion mask. There is no unconditional "all roof kills fall" rule.
  if(actor_.weaponType()==4 && Rnd()%3)return false;
  if(actor_.type()==10 || actor_.type()==13 || actor_.type()==2 || actor_.type()==22)return false;
  if(gameLevel_>6 && Rnd()%100>75)return false;
  if(gameLevel_==10)return false;
  const auto p=feet();CollisionVec from=p;from[1]+=500;
  float target{};bool back=false,found=false;
  for(const std::array<float,3> local:std::array<std::array<float,3>,4>{{{0,500,800},{800,500,0},{-800,500,0},{0,500,-800}}}) {
    const auto point=TransformPoint(placement(),local);
    const auto first=sat_.line(from,{point[0],point[1],point[2]},SatSubset::All,0,0);
    if(!first || !(first->attributes&0x00182810))continue;
    const float h=std::atan2(float(-first->normal[0]),float(-first->normal[2]));
    const CollisionVec to{p[0]+std::sin(h)*800,p[1]+500,p[2]+std::cos(h)*800};
    const auto second=sat_.line(from,to,SatSubset::All,0,0);
    if(!second || !(second->attributes&0x00182810))continue;
    back=local[2]>=0;target=back?h:std::atan2(float(first->normal[0]),float(first->normal[2]));
    found=true;break;
  }
  if(!found)return false;
  roofDamage_=true;roofPhase_=1;roofAirTicks_=0;roofVelocityY_=0;
  roofTurnRemaining_=std::remainder(target-heading_,2*pi);
  flags_&=~0x400u;roofWaiting_=false;airHeight_=0;reactionGuard_=true;
  const bool mirror=(Rnd()&1)==0;
  startMotion(back?EnemyMotion::RoofBack:EnemyMotion::RoofFront,mirror);
  actor_.dropWeapon();emitSound(voiceCue(14),EnemySoundChannel::Voice,true);
  emitSound(0,EnemySoundChannel::Contact,false,true);
  return true;
}
bool EnemyEncounter::tickRoofDamage() {
  if(!roofDamage_)return false;
  damageTimer_=2;
  auto p=probe_.position();const auto previous=p;
  if(roofPhase_==1) {
    const float change=roofTurnRemaining_*.1f;roofTurnRemaining_-=change;
    heading_=std::remainder(heading_+change,2*pi);
  } else if(roofPhase_==3) {p[1]+=roofVelocityY_;roofVelocityY_-=20;}
  const auto root=actor_.advance();const auto free=actor_.rig().sequenceFree();
  const float c=std::cos(heading_),s=std::sin(heading_);
  p[0]+=scale_*(c*root.translation[0]+s*root.translation[2]);
  p[1]+=root.translation[1];p[2]+=scale_*(-s*root.translation[0]+c*root.translation[2]);
  heading_=std::remainder(heading_+float(root.yaw),2*pi);
  if(roofPhase_==1 || roofPhase_==3) {
    if(roofPhase_==3 || (free&4)) {
      ++roofAirTicks_;auto query=p;query[1]=previous[1];
      const auto floor=sat_.ground(query,600,100000);const double floorY=floor?floor->point[1]:-100000;
      if(p[1]<floorY) {
        p[1]=floorY;roofVelocityY_=0;roofPhase_=4;
        hp_-=std::min(hp_,roofAirTicks_*50u);
        if(roofAirTicks_>60 || p[1]<-99000)hp_=0;
        startMotion(EnemyMotion::RoofLand,actor_.mirrored());
        emitSound(0x77,EnemySoundChannel::Impact);
      } else if(roofPhase_==1 && root.ended) {roofPhase_=3;roofVelocityY_=-300;}
    }
  } else if(roofPhase_==4 && root.ended) {
    roofDamage_=false;downFacing_=true;downState_=true;reactionGuard_=false;
    startMotion(EnemyMotion::DownIdle);deadSettled_=!alive();
    wakeTimer_=0;wakeInitialized_=false;
  }
  probe_.setScriptPosition(p);return true;
}
bool EnemyEncounter::tickBlowFall() {
  if(!blowFalling_)return false;
  damageTimer_=2;reactionGuard_=downState_=true;
  auto p=feet();const auto old=p;
  if(!blowFallPhase_) {
    startMotion(downFacing_?EnemyMotion::BlowAirUp:EnemyMotion::BlowAirDown,actor_.mirrored());
    blowFallPhase_=1;
  }
  if(blowFallPhase_==1) {
    ++blowAirTicks_;blowFallSpeed_[1]-=20;
    const float c=std::cos(heading_),s=std::sin(heading_);
    p[0]+=c*blowFallSpeed_[0]+s*blowFallSpeed_[2];p[1]+=blowFallSpeed_[1];
    p[2]+=-s*blowFallSpeed_[0]+c*blowFallSpeed_[2];
    const auto root=actor_.advance();p[0]+=scale_*(c*root.translation[0]+s*root.translation[2]);
    p[1]+=root.translation[1];p[2]+=scale_*(-s*root.translation[0]+c*root.translation[2]);
    heading_=std::remainder(heading_+float(root.yaw),2*pi);
    auto query=p;query[1]=old[1];
    const auto floor=sat_.ground(query,600,100000);const double y=floor?floor->point[1]:-100000;
    if(p[1]<y) {
      p[1]=y;blowFallSpeed_[1]=0;blowFallPhase_=2;
      hp_-=std::min(hp_,blowAirTicks_*50u);if(blowAirTicks_>60 || y<-99000)hp_=0;
      startMotion(downFacing_?EnemyMotion::BlowLandUp:EnemyMotion::BlowLandDown,actor_.mirrored());
      if(blowAirTicks_<=60)emitSound(0x77,EnemySoundChannel::Impact);
    }
  }else {
    const auto root=actor_.advance();const float c=std::cos(heading_),s=std::sin(heading_);
    p[0]+=scale_*(c*root.translation[0]+s*root.translation[2]);p[1]+=root.translation[1];
    p[2]+=scale_*(-s*root.translation[0]+c*root.translation[2]);heading_=std::remainder(heading_+float(root.yaw),2*pi);
    if(root.ended) {blowFalling_=false;startMotion(EnemyMotion::DownIdle);deadSettled_=!alive();wakeTimer_=0;wakeInitialized_=false;}
  }
  probe_.setScriptPosition(p);return true;
}
void EnemyEncounter::startMotion(EnemyMotion motion, bool mirrored,
                                 double rootScale) {
  const bool wasContact = actor_.motion() == EnemyMotion::NeckHang ||
      (actor_.weaponType()==4 && actor_.motion()==EnemyMotion::AxeAttack);
  if(!alive())actor_.dropWeapon(); // Die_Normal/Die_Down detach on entry.
  if((firstHouse_ || r100Ambush_ || villageGanado_) && motion==EnemyMotion::Death)sourceDeathTimer_=0x49;
  actor_.start(motion, mirrored, rootScale);
  if(motion==EnemyMotion::Find)discoveryVoiceDone_=false;
  previousSoundFree_ = 0;
  if (wasContact && motion != EnemyMotion::NeckHang && motion!=EnemyMotion::AxeAttack)
    emitSound(0, EnemySoundChannel::Contact, false, true);
  voiceDelay_ = 0;
  unsigned cue = ~0u;
  switch (motion) {
  case EnemyMotion::Catch: cue = voiceCue(5); break;
  case EnemyMotion::AxeAttack:
  case EnemyMotion::HoeAttack:
  case EnemyMotion::HoeAttackHigh:
  case EnemyMotion::HoeAttackLow: cue = voiceCue(12); break;
  case EnemyMotion::ReleaseFailed: cue = voiceCue(13); break;
  case EnemyMotion::Wake:
  case EnemyMotion::WakeFaceDown: cue = voiceCue(11); break;
  case EnemyMotion::Death:
  case EnemyMotion::DownDeathFaceUp:
  case EnemyMotion::DownDeathFaceDown:
    cue = voiceCue(0); break;
  case EnemyMotion::HitHead:
  case EnemyMotion::HeadLeft:
  case EnemyMotion::HeadRight:
    // Original Dm_Head Timer3=2, decremented once in its entry case.
    delayedVoiceCue_ = voiceCue(2); voiceDelay_ = 1; break;
  case EnemyMotion::HitBody:
  case EnemyMotion::BodyAlt:
  case EnemyMotion::BodyRear:
    delayedVoiceCue_ = voiceCue(8); voiceDelay_ = 1; break;
  case EnemyMotion::DownHitFaceUp:
  case EnemyMotion::DownHitFaceDown:
    cue = voiceCue(alive() ? 8 : 0); break;
  case EnemyMotion::NeckHang:
    // One owned contact voice is sufficient for this selected single actor;
    // the source starts the same cue twice and restarts its owned one later.
    emitSound(0x85, EnemySoundChannel::Contact, true); break;
  default:
    if (blowMotion(motion)) cue = voiceCue(alive() ? 8 : 0);
    break;
  }
  if (cue != ~0u) {
    emitSound(cue, EnemySoundChannel::Voice, true);
    emitSound(0, EnemySoundChannel::Breath, false, true);
    // The audio adapter has its own original RNG: enabling sound must not
    // change the established combat/drop random sequence.
    RandomSession audioRandom(feedbackRandom_);
    (void)Rnd(); // Original CTRL11 cooldown20..39 selection.
    breathTimer_ = Rnd() % 120 + 120;
  }
}
void EnemyEncounter::soundTick() {
  if (bloodBigEffectTimer_) --bloodBigEffectTimer_;
  if (voiceDelay_ && --voiceDelay_ == 0) {
    emitSound(alive() ? delayedVoiceCue_ : voiceCue(0),
              EnemySoundChannel::Voice, true);
    emitSound(0, EnemySoundChannel::Breath, false, true);
    RandomSession audioRandom(feedbackRandom_);
    (void)Rnd();
    breathTimer_ = Rnd() % 120 + 120;
  }
  const auto motion = actor_.motion();
  if (alive() && motion == EnemyMotion::Walk) {
    if (breathTimer_) --breathTimer_;
    else {
      emitSound(voiceCue(7), EnemySoundChannel::Breath, true);
      RandomSession audioRandom(feedbackRandom_);
      breathTimer_ = Rnd() % 120 + 120;
      (void)Rnd();
    }
  }
  const auto free = actor_.rig().sequenceFree();
  if (motion == EnemyMotion::ThrowOff && (free & 1) &&
      !(previousSoundFree_ & 1))
    emitSound(voiceCue(alive() ? 8 : 0),
               EnemySoundChannel::Voice, true);
  previousSoundFree_ = free;
}
void EnemyEncounter::emitDamageFeedback() {
  const auto &hit = *pendingHit_;
  const auto &shape = shapes_.at(hit.partIndex);
  const unsigned weapon = pendingSource_ == DamageSource::Knife ? 0x10
                          : pendingSource_ == DamageSource::Kick ? 0
                          : pendingSource_ == DamageSource::Blast ? pendingWeaponNo_
                                                                 : pendingWeaponNo_;
  const bool near = hit.lenSq < 36000000.f;
  emitSound(OriginalShotgun(weapon) && near ? 0x7a : 0xc, EnemySoundChannel::Impact);
  const auto posed = shape.partsNo
      ? WeaponAttachmentMatrix(actor_.rig().jointMatrices().at(unsigned(shape.partsNo - 1)),
                                 placement()) : placement();
  const auto pose = GanadoDamagePose(shape, hit.cross, posed, pendingFrom_);
  const auto eye = feedbackCamera_.value_or(pendingFrom_);
  const auto root = TransformPoint(WeaponAttachmentMatrix(
      actor_.rig().jointMatrices().at(0), placement()), {0, 0, 0});
  float distance = 0;
  for (unsigned axis = 0; axis < 3; ++axis) {
    const float delta = float(eye[axis]) - root[axis];
    distance += delta * delta;
  }
  const auto sequence = GanadoBloodSequence(weapon, near,
                                              bloodBigEffectTimer_ != 0, distance);
  if (!sequence) return;
  if (effectEvents_.size() == 128) effectEvents_.erase(effectEvents_.begin());
  effectEvents_.push_back({*sequence, pose.position, pose.rotation, ++effectSerial_});
  if (*sequence == 2 || *sequence == 0x36) bloodBigEffectTimer_ = 0x2d;
}
bool EnemyEncounter::canCatch(const LeonPreview &player) const {
  if (!alive() || !player.alive() || player.grabbed() || player.damageWait() ||
      !(actor_.rig().sequenceFree() & 2))
    return false;
  const auto position = feet(), target = player.feet();
  const float dx = float(target[0] - position[0]);
  const float dz = float(target[2] - position[2]);
  const float c = std::cos(heading_), s = std::sin(heading_);
  if (!GrabBoxContains({(c * dx - s * dz) / scale_,
                        (target[1] - position[1]) / scale_,
                        (s * dx + c * dz) / scale_}))
    return false;
  auto a = position, b = target;
  a[1] += 500;
  b[1] += 500;
  if (eat_.line(a, b, SatSubset::All, 0, 0))
    return false;
  const float yaw = std::atan2(dx, dz), cy = std::cos(yaw), sy = std::sin(yaw);
  a = {position[0] + cy * 300, position[1] + 500, position[2] - sy * 300};
  b = {a[0] + sy * 500, a[1], a[2] + cy * 500};
  // The source repeats this same +300 side probe twice; retain its bounds.
  return !eat_.line(a, b, SatSubset::All, 0, 0) &&
         !eat_.line(a, b, SatSubset::All, 0, 0);
}
bool EnemyEncounter::axeAttackAvailable(const LeonPreview &player) const {
  if(!actor_.holdsWeapon() || actor_.weaponType()==1 || !player.alive() || player.grabbed() ||
      player.damageWait() || attackWait_)return false;
  const auto p=feet(),q=player.feet();
  const float dx=float(q[0]-p[0]),dz=float(q[2]-p[2]);
  const auto angle=std::remainder(std::atan2(dx,dz)-heading_,2*pi);
  if(dx*dx+dz*dz>2890000.f || std::abs(angle)>.7853982f ||
     std::abs(p[1]-q[1])>1500)return false;
  auto a=p,b=q;a[1]+=1500;b[1]+=1500;
  return !eat_.line(a,b,SatSubset::All,0,0x4000);
}
bool EnemyEncounter::weaponAttackAvailable(const LeonPreview &player) const {
  if(groupAttackBlocked_)return false;
  if(actor_.weaponType()!=1)return axeAttackAvailable(player);
  if(!actor_.holdsWeapon() || !player.alive() || player.grabbed() ||
     player.damageWait() || attackWait_)return false;
  const auto p=feet(),q=player.feet();
  const float dx=float(q[0]-p[0]),dz=float(q[2]-p[2]);
  const auto angle=std::remainder(std::atan2(dx,dz)-heading_,2*pi);
  if(dx*dx+dz*dz>4000000.f || std::abs(angle)>.7853982f ||
     std::abs(p[1]-q[1])>1500)return false;
  auto a=p,b=q;a[1]+=1500;b[1]+=1500;
  return !eat_.line(a,b,SatSubset::All,0,0x4000);
}
bool EnemyEncounter::receiveWeaponHit(LeonPreview &player,const CollisionVec &point,
                                       const CollisionVec &base,unsigned damage) {
  static const std::vector<AuthoredHitShape> shapes{
    {{0,-30,0},200,100,0,HitOn,2},{{0,0,0},210,130,0,HitOn,3},
    {{0,0,0},120,80,0,HitOn,5},{{-20,-300,0},170,300,0,HitOn,0x13},
    {{20,-300,0},170,300,0,HitOn,0x17}};
  const auto root=TransformPoint(player.placement(),TransformPoint(player.rig().jointMatrices().at(0),{0,0,0}));
  if(eat_.line({root[0],root[1],root[2]},point,SatSubset::All,0))return false;
  if(!SphereAuthoredShapes(shapes,player.rig().jointMatrices(),player.placement(),point,250,0x18))return false;
  if(actor_.weaponType()==4) {beginChainsawCatch(player);axeAttackHit_=true;++axeHits_;return true;}
  if((flags_&4)&&!hardMode_)damage=damage/2+1;
  const auto type=GanadoAxeDamageType(player.feet(),player.heading(),point,base);
  if(!player.receiveGanadoAxeHit(feet(),damage,type))return false;
  axeAttackHit_=true;++axeHits_;
  attackWait_=gameLevel_<=3?60:gameLevel_>6?30:45;
  emitSound(0x3e,EnemySoundChannel::Impact);
  return true;
}
void EnemyEncounter::tickAxeAttack(LeonPreview &player) {
  const auto free=actor_.rig().sequenceFree();
  if((axeTurnTimer_ && --axeTurnTimer_) || (free&8)) {
    const auto p=feet(),q=player.feet();
    const float limit=actor_.weaponType()==4?.09817477f:(free&8)?.049087387f:.19634955f;
    const auto angle=std::remainder(std::atan2(float(q[0]-p[0]),float(q[2]-p[2]))-heading_,2*pi);
    heading_=std::remainder(heading_+std::clamp(angle,-limit,limit),2*pi);
  }
  if(free&0x20)emitSound(0x3d,EnemySoundChannel::Impact);
  if((free&1) && !axeAttackHit_ && player.alive() && !player.damageWait() && !player.grabbed()) {
    // player.cpp cPlayer::init's five original YARARE capsules. Source
    // emSphereAtCk samples the sphere at each sweep endpoint; the base is
    // used for hit direction/part ordering, not an analytic swept capsule.
    const auto weapon=WeaponAttachmentMatrix(actor_.weaponMatrix(),placement());
    auto point=[&](float z) {const auto p=TransformPoint(weapon,{0,0,z});return CollisionVec{p[0],p[1],p[2]};};
    const auto base=point(-1000);
    if(actor_.weaponType()==4) {
      for(float z:{500.f,0.f,-200.f}) {auto p=point(z);if(z==-200.f)p[1]=feet()[1]+500;
        if(receiveWeaponHit(player,p,base))break;}
    } else for(float z:{-200.f,50.f})if(receiveWeaponHit(player,point(z),base))break;
    if(chainsawCatch_)return;
  }
  advanceEnemy();
  if(actor_.rig().status().ended) {
    if(axeAttackHit_)attackWait_=gameLevel_<=1?90:gameLevel_<=3?45:15;
    startMotion(EnemyMotion::Walk);
  }
}
void EnemyEncounter::tickHoeAttack(LeonPreview &player) {
  const auto free=actor_.rig().sequenceFree();
  if(free&0x20)emitSound(0x3d,EnemySoundChannel::Impact);
  if(axeTurnTimer_) {
    --axeTurnTimer_;
    const auto p=feet(),q=player.feet();
    const auto angle=std::remainder(std::atan2(float(q[0]-p[0]),float(q[2]-p[2]))-heading_,2*pi);
    heading_=std::remainder(heading_+std::clamp(angle,-.19634955f,.19634955f),2*pi);
  }
  if((free&1) && !axeAttackHit_ && player.alive() && !player.damageWait() && !player.grabbed()) {
    const auto weapon=WeaponAttachmentMatrix(actor_.weaponMatrix(),placement());
    auto point=[&](float z) {const auto p=TransformPoint(weapon,{0,0,z});return CollisionVec{p[0],p[1],p[2]};};
    const auto base=point(-2000);
    for(float z:{-1000.f,-700.f,-400.f,-100.f,300.f,600.f})
      if(receiveWeaponHit(player,point(z),base))break;
  }
  advanceEnemy();
  if(actor_.rig().status().ended) {
    if(axeAttackHit_)attackWait_=gameLevel_<=1?90:gameLevel_<=3?45:15;
    startMotion(EnemyMotion::Walk);
  }
}
void EnemyEncounter::beginChainsawCatch(LeonPreview &player) {
  const auto alignment=AlignCatch(feet(),heading_,player.feet(),player.heading(),-15.17f,853.48f);
  chainsawCatch_=true;chainsawPhase_=0;chainsawTimer_=19;chainsawTimer2_=41;resistance_=0;++grabs_;
  scale_=1;catchTurn_=alignment.enemyTurn;catchAdjustment_=alignment.enemyAdjustment;
  actor_.startChainsawCatch(0);player.beginChainsawGrab(alignment.playerTurn,alignment.playerAdjustment);
  damageTimer_=0x80;emitSound(0x4f,EnemySoundChannel::Contact,true,false,6);
}
void EnemyEncounter::tickChainsawCatch(LeonPreview &player) {
  damageTimer_=2;
  if(chainsawPhase_==0) {
    resistance_+=player.takeResistPresses();
    if(chainsawTimer_)--chainsawTimer_;else if(chainsawTimer2_)--chainsawTimer2_;else player.setFatalLife();
    if(player.alive()) {if(chainsawTimer2_)--chainsawTimer2_;else chainsawTimer2_=10;}
    if(!player.alive()) {
      player.endChainsawGrab(false);actor_.startChainsawCatch(2);chainsawPhase_=2;emitSound(0x54,EnemySoundChannel::Contact,true,false,6);return;
    }
    advanceEnemy(true);
    if(actor_.rig().status().ended || resistance_>30 || (player.unlimitedHealth() && !chainsawTimer_ && !chainsawTimer2_)) {
      player.endChainsawGrab(true);actor_.startChainsawCatch(1);chainsawPhase_=1;++escapes_;emitSound(0x54,EnemySoundChannel::Contact,true,false,6);
    }
  } else {
    advanceEnemy();
    if(chainsawPhase_==1 && actor_.rig().status().ended) {
      chainsawCatch_=false;emitSound(0,EnemySoundChannel::Contact,false,true);
      attackWait_=gameLevel_<=1?90:gameLevel_<=3?45:15;startMotion(EnemyMotion::Walk);
    }
  }
}
void EnemyEncounter::tickCombat(LeonPreview &player, WalkProbe &probe) {
  validPoint(player.feet());
  if(scriptSuspended_)return;
  if(r100Cliff_) {tickR100Cliff(player.feet());return;}
  if(r100HouseScare_) {tickR100HouseScare();return;}
  if(dashWait_)dashWait_=dashWait_>1+unsigned(gameLevel_>6)+unsigned(gameLevel_>9)?
      dashWait_-1-unsigned(gameLevel_>6)-unsigned(gameLevel_>9):0;
  countSourceDeath();
  if((firstHouse_ || r100Ambush_ || villageGanado_) && deadSettled_ && !alive())sourceDeathInactive_=true;
  headPromptTick_ = false;
  soundTick();
  applyPendingDamage();
  if(tickRoofDamage())return;
  if(tickBlowFall())return;
  if(tickFlash())return;
  if(r100Ambush_ && initialSet_==0x1f && alive() && hp_==maxHp_) {
    // em10_R1_R100WalkStay (0x4C), including the original fallthrough and
    // nonzero Mot_state loop boundary. It does not wait for FCV ended.
    damageTimer_=2;
    if(scriptWalkPhase_==0) {
      actor_.start(EnemyMotion::Walk,false,0,0);scriptWalkPhase_=1;
    }
    if(scriptWalkPhase_==1) {
      advanceEnemy();
      if(actor_.rig().status().state)scriptWalkPhase_=2;
      return;
    }
    if(scriptWalkPhase_==2) {
      const auto frames=unsigned(actor_.rig().maxFrame(unsigned(EnemyMotion::Idle)))&0x3fff;
      actor_.start(EnemyMotion::Idle,false,0,10,frames?Rnd()%frames:0);
      scriptWalkPhase_=3;
    }
    advanceEnemy();
    if(flags_&1) {flags_&=~1u;initialSet_=0;}
    return;
  }
  if(chainsawCatch_) {tickChainsawCatch(player);return;}
  if (attackWait_)
    --attackWait_;
  if (releasePending_) {
    const bool escaped = *releasePending_;
    releasePending_.reset();
    player.releaseGrab(escaped);
    startMotion(escaped ? EnemyMotion::ThrowOff : EnemyMotion::ReleaseFailed);
    releaseTicks_ = 0;
    if (escaped) {
      ++escapes_;
      unsigned damage = 100;
      if (resistance_ > 30)
        damage = 9999;
      // Independent source if: >30 still consumes the >20 random branch.
      if (resistance_ > 20 && (Rnd() & 1))
        damage = 9999;
      (void)Rnd();
      (void)Rnd();
      lastDamage_ = std::min(hp_, damage);
      lastDamageSource_ = DamageSource::Escape;
      lastHead_ = lastCritical_ = false;
      hp_ -= lastDamage_;
    }
  }
  auto motion = actor_.motion();
  if (motion == EnemyMotion::Catch && canCatch(player)) {
    const auto alignment =
        AlignCatch(feet(), heading_, player.feet(), player.heading());
    startMotion(EnemyMotion::NeckHang);
    scale_ = 1;
    catchTurn_ = alignment.enemyTurn;
    catchAdjustment_ = alignment.enemyAdjustment;
    player.beginGrab(alignment.playerTurn, alignment.playerAdjustment);
    probe.setActorBlocker(std::nullopt);
    resistance_ = 0;
    grabStartLife_ = player.life();
    ++grabs_;
    motion = actor_.motion();
  }
  if (motion == EnemyMotion::NeckHang) {
    resistance_ += player.takeResistPresses();
    player.takeDamage(GrabHoldDamage(flags_), true);
    advanceEnemy(true);
    const auto frame = actor_.rig().sequenceFrame();
    if (actor_.rig().status().ended || (frame > 34.7f && frame < 35.3f)) {
      const bool escaped =
          resistance_ > 9 && !(player.life() <= 1 && grabStartLife_ <= 199);
      if (!escaped && player.life() <= 1)
        player.setFatalLife();
      // Enemy r2 changes now; the player's case1 still advances the hold and
      // copies that r2. Both release MotionSets occur on the following tick.
      releasePending_ = escaped;
      damageTimer_ = 2;
    }
    return;
  }
  if (motion == EnemyMotion::ReleaseFailed || motion == EnemyMotion::ThrowOff) {
    damageTimer_ = 2;
    if (motion == EnemyMotion::ThrowOff && releaseTicks_ >= 55)
      scale_ = scale_ * .9f + originalScale_ * .1f;
    advanceEnemy(motion == EnemyMotion::ReleaseFailed && releaseTicks_ < 48);
    ++releaseTicks_;
    if (actor_.rig().status().ended) {
      if (motion == EnemyMotion::ThrowOff) {
        startMotion(EnemyMotion::DownIdle);
        downFacing_ = downState_ = true;
        reactionGuard_ = false;
        deadSettled_ = !alive();
        wakeTimer_ = 0;
        wakeInitialized_ = false;
      } else {
        startMotion(EnemyMotion::Idle);
        attackWait_ = 30;
      }
    }
    return;
  }
  scale_ = scale_ * .9f + originalScale_ * .1f;
  if (motion == EnemyMotion::DownIdle) {
    downState_ = true;
    reactionGuard_ = false;
    if (alive() && !wakeInitialized_) {
      wakeTimer_ = Rnd() % 60 + 60;
      wakeInitialized_ = true;
    }
    if (alive() && (melee_ ? wakeTimer_ == 0 : wakeTimer_ && --wakeTimer_ == 0))
      startMotion(downFacing_ ? EnemyMotion::Wake : EnemyMotion::WakeFaceDown,
                   melee_ && actor_.mirrored());
    else {
      if (melee_ && wakeTimer_)
        --wakeTimer_;
      return;
    }
    motion = actor_.motion();
  }
  if (deadSettled_)
    return;
  if(motion==EnemyMotion::AxeAttack) {tickAxeAttack(player);return;}
  if(motion==EnemyMotion::HoeAttack || motion==EnemyMotion::HoeAttackHigh ||
     motion==EnemyMotion::HoeAttackLow) {tickHoeAttack(player);return;}
  if(tickDiscovery(player.feet()))return;
  if (motion == EnemyMotion::Idle || motion == EnemyMotion::Walk || motion==EnemyMotion::Dash) {
    const auto position = feet(), target = player.feet();
    const float dx = float(target[0] - position[0]);
    const float dz = float(target[2] - position[2]);
    const float angle = std::remainder(std::atan2(dx, dz) - heading_, 2 * pi);
    const float distance = std::hypot(dx, dz);
    // Original RTP and Walk/Dash clips supply route movement. Dynamic
    // obstacle actions and side/rear/Dash catches have separate owners.
    if(r100Ambush_ || villageGanado_) {
      if(!foundPlayer_ && distance>=50000 && !(flags_&0x40)) {
        if(actor_.opacity()>0)actor_.setOpacity(std::max(0.f,actor_.opacity()-.1f));
        else {keeperInvisible_=true;visible_=false;}
        return;
      }
      if(keeperInvisible_) {keeperInvisible_=false;damageTimer_=0;}
      actor_.setOpacity(1);visible_=true;
      if(!foundPlayer_ && (flags_&8))heading_=std::remainder(heading_+angle,2*pi);
      if(!foundPlayer_ && !detectPlayer(target,player.heading())) {
        advanceEnemy();
        return;
      }
      if(tickDiscovery(target))return;
      // Keeper leaves its post only after discovery and when the player is
      // inside the original route distance from Keep_pos/Guard_r.
      const float guard=routes_?routes_->distance(guardPosition_,target):
          std::hypot(float(target[0]-guardPosition_[0]),float(target[2]-guardPosition_[2]));
      if(keeper_ && !keeperLeft_ && guard>=guardRadius_ && !(flags_&0x40)) {
        heading_=std::remainder(heading_+std::clamp(angle,-.024543693f,.024543693f),2*pi);
        if(actor_.motion()!=EnemyMotion::Idle)startMotion(EnemyMotion::Idle);
        actor_.setOpacity(1);visible_=true;advanceEnemy();return;
      }
      keeperLeft_=true;
      actor_.setOpacity(1);visible_=true;
    }
    if(villageGanado_ && (flags_&0x400)) {
      // Original br_Walk/RoofWait: roof guards do not jump off while
      // goto_mode0 is active. After blocked pursuit they watch Leon and
      // resume walking when he moves >45 degrees from their facing.
      if(!roofWaiting_ && motion==EnemyMotion::Walk && blockedTicks_>30 && std::abs(angle)<.5235988f) {
        roofWaiting_=true;roofTurnTimer_=10;startMotion(EnemyMotion::Idle);
      }
      if(roofWaiting_) {
        if(std::abs(angle)>.7853982f)roofWaiting_=false;
        else if(roofTurnTimer_) {
          --roofTurnTimer_;heading_=std::remainder(heading_+std::clamp(angle,-.09817477f,.09817477f),2*pi);
        }
      }
    }
    if((firstHouse_ || r100Ambush_ || villageGanado_) && weaponAttackAvailable(player)) {
      EnemyMotion attack=EnemyMotion::AxeAttack;
      if(actor_.weaponType()==1) {
        const float dy=float(target[1]-position[1]);
        attack=dy> -100?EnemyMotion::HoeAttackHigh:dy< -800?EnemyMotion::HoeAttackLow:EnemyMotion::HoeAttack;
      }
      startMotion(attack);++axeAttacks_;
      if(actor_.weaponType()==4)emitSound(0x50,EnemySoundChannel::Contact,true,false,6);
      axeAttackHit_=false;axeTurnTimer_=actor_.weaponType()==4?5:gameLevel_<=3?5:gameLevel_>6?30:20;
      if(actor_.weaponType()==1)tickHoeAttack(player);else tickAxeAttack(player);
      return;
    } else if (!actor_.holdsWeapon() && !firstHouse_ && player.alive() && !player.grabbed() && !player.damageWait() &&
        !attackWait_ && distance <= 1100 && std::abs(angle) < 1.3089969f) {
      startMotion(EnemyMotion::Catch);
      motion = actor_.motion();
    } else {
      const bool approach =
          player.alive() && distance > 800 && !player.grabbed() && !roofWaiting_;
      auto next=approach?EnemyMotion::Walk:EnemyMotion::Idle;
      if(approach && villageGanado_) {
        const float routeDistance=routes_?routes_->distance(position,target):distance;
        if(motion==EnemyMotion::Dash) {
          if(locomotionCycle_ || blockedTicks_>60){if(dashCycles_)--dashCycles_;}
          if(dashCycles_ && routeDistance>float(gameLevel_<=3?5000:2500))next=EnemyMotion::Dash;
        } else if(motion==EnemyMotion::Walk && locomotionCycle_ && !dashWait_ && foundPlayer_ &&
            gameLevel_>1 && sourceCharacter_!=3 && actor_.type()!=10 && actor_.type()!=13 &&
            !(flags_&0x40) && !(flags_&0x400) && distance>=float(gameLevel_>6?2000:3500) &&
            nearPlayerCount_<=unsigned(gameLevel_>9?6:gameLevel_>6?3:gameLevel_>3?1:0) &&
            (gameLevel_>6 || std::abs(std::remainder(std::atan2(-dx,-dz)-player.heading(),2*pi))<=.3926991f)) {
          next=EnemyMotion::Dash;dashCycles_=Rnd()%3+5;dashWait_=Rnd()%150+150;
        }
      }
      if (next != motion)
        startMotion(next);
      auto go=target;
      // em10Init's cAtariInfo::init adds scenery/body bits0x300 to0x2001.
      if(approach && routes_)go=routes_->step(position,target,target[1]>position[1]+1000?1:0,villageGanado_?0x2301:0).destination;
      const float goAngle=std::remainder(std::atan2(float(go[0]-position[0]),float(go[2]-position[2]))-heading_,2*pi);
      heading_=std::remainder(heading_+(next==EnemyMotion::Dash?
          std::clamp(goAngle,-.19634955f,.19634955f):std::clamp(goAngle*.3f,-.15707964f,.15707964f)),2*pi);
      motion = actor_.motion();
    }
  }
  if (motion == EnemyMotion::Catch && (actor_.rig().sequenceFree() & 8)) {
    const auto position = feet(), target = player.feet();
    const float angle =
        std::remainder(std::atan2(float(target[0] - position[0]),
                                  float(target[2] - position[2])) -
                           heading_,
                       2 * pi);
    heading_ = std::remainder(
        heading_ + std::clamp(angle, -.31415927f, .31415927f), 2 * pi);
  }
  if (motion == EnemyMotion::DownHitFaceUp ||
      motion == EnemyMotion::DownHitFaceDown) {
    advanceEnemy();
    if (actor_.rig().status().ended) {
      startMotion(downFacing_ ? EnemyMotion::Wake : EnemyMotion::WakeFaceDown,
                   actor_.mirrored());
    }
    return;
  }
  if (motion == EnemyMotion::DownDeathFaceUp ||
      motion == EnemyMotion::DownDeathFaceDown) {
    advanceEnemy();
    deadSettled_ = actor_.rig().status().ended;
    return;
  }
  if (motion == EnemyMotion::Wake || motion == EnemyMotion::WakeFaceDown) {
    downState_ = (actor_.rig().sequenceFree() & 0x80) != 0;
    reactionGuard_ = melee_ && downState_;
  }
  if (blowMotion(motion)) {
    reactionGuard_ = true;
    if (!blowLanded_)
      damageTimer_ = 2;
    const auto oldFree = actor_.rig().sequenceFree();
    downState_ = (oldFree & 0x80) != 0;
    blowClamp_ |= (oldFree & 0x10) != 0;
    advanceEnemy();
    if(villageGanado_) {
      auto p=feet();const auto floor=sat_.ground(p,600,100000);const double y=floor?floor->point[1]:-100000;
      if(blowClamp_ && !blowLanded_ && p[1]<y+50){p[1]=y;probe_.setScriptPosition(p);}
      if(!actor_.rig().status().ended && (actor_.rig().sequenceFree()&4)) {
        bool edge=p[1]>y+300;
        for(const std::array<float,3> local:std::array<std::array<float,3>,2>{{{0,0,-100},{50,0,-50}}})if(!edge) {
          const auto point=TransformPoint(placement(),local);const auto ground=sat_.ground({point[0],p[1],point[2]},600,100000);
          edge=!ground || p[1]>ground->point[1]+300;
        }
        if(edge){blowFalling_=true;blowFallPhase_=blowAirTicks_=0;flags_&=~0x400u;roofWaiting_=false;}
        else blowLanded_=true;
      }
    }else {
      if(blowClamp_ && !blowLanded_ && airHeight_<50)airHeight_=0;
      if(actor_.rig().sequenceFree()&4)blowLanded_=airHeight_<=300;
    }
    if (actor_.rig().status().ended) {
      airHeight_ = 0;
      startMotion(EnemyMotion::DownIdle);
      deadSettled_ = !alive();
      wakeTimer_ = 0;
      wakeInitialized_ = false;
    }
    return;
  }
  if (headMotion(motion) || motion == EnemyMotion::BodyRear) {
    if (reactionTimer_)
      --reactionTimer_;
    else
      reactionGuard_ = false;
  }
  advanceEnemy();
  headPromptTick_ = headMotion(motion);
  if (!alive()) {
    deadSettled_ = actor_.rig().status().ended;
    // Die_Normal can clear ACTIVE while its original 103-frame collapse is
    // still playing. The truck event waits on this source flag, not motion end.
    if((firstHouse_ || r100Ambush_ || villageGanado_) && motion==EnemyMotion::Death && !deadSettled_ &&
       sourceDeathTimer_ && --sourceDeathTimer_==0)sourceDeathInactive_=true;
  } else if (((motion == EnemyMotion::HitBody || motion == EnemyMotion::BodyAlt ||
             motion == EnemyMotion::BodyRear) &&
            (actor_.rig().sequenceFree() & 4)) ||
           ((headMotion(motion) || motion == EnemyMotion::HitBody ||
             motion == EnemyMotion::BodyAlt ||
             motion == EnemyMotion::BodyRear || motion == EnemyMotion::Catch ||
             motion == EnemyMotion::Wake ||
             motion == EnemyMotion::WakeFaceDown) &&
            actor_.rig().status().ended)) {
    startMotion(EnemyMotion::Idle);
    reactionGuard_ = downState_ = false;
    if (motion == EnemyMotion::Catch)
      attackWait_ = 30;
  }
}
std::string EnemyEncounter::status() const {
  std::ostringstream text;
  text << "Ganado test · HP " << hp_ << '/' << maxHp_ << " · "
       << EnemyMotionLabel(actor_.motion()) << " · Hits " << hits_ << '/'
       << shots_;
  if (knifeHits_ || kickHits_)
    text << " · Knife " << knifeHits_ << " · Kicks " << kickHits_;
  if (lastDamageSource_ != DamageSource::None) {
    const char *source = lastDamageSource_ == DamageSource::Bullet  ? "Shot"
                         : lastDamageSource_ == DamageSource::Knife ? "Knife"
                         : lastDamageSource_ == DamageSource::Blast ? "Explosion"
                         : lastDamageSource_ == DamageSource::Kick
                             ? "Kick"
                             : "Break free";
    text << " · " << source;
    if (lastDamageSource_ != DamageSource::Escape)
      text << ' ' << (lastHead_ ? "Head" : "Body");
    text << " −" << lastDamage_ << (lastCritical_ ? " (critical)" : "");
  }
  if (pendingHit_)
    text << " · New hit pending";
  if (grabs_)
    text << " · Grabs " << grabs_ << " · Repelled " << escapes_
         << " · Resistance " << resistance_ << "/10";
  return text.str();
}
} // namespace re4
