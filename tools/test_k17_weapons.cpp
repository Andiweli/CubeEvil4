#include "archive.h"
#include "leon_preview.h"
#include "weapon_preview.h"
#include "player_projectiles.h"
#include "enemy_encounter.h"
#include "enemy_feedback.h"
#include "inventory_case.h"
#include "ports/yz2.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
using namespace re4;
unsigned checks{};
void check(bool value,const std::string &label){if(!value)throw std::runtime_error(label);++checks;}
Sat floorData(){
  ScenarioTriangle a,b;
  a.vertices={{{-100000,0,-100000},{100000,0,100000},{100000,0,-100000}}};
  b.vertices={{{-100000,0,-100000},{-100000,0,100000},{100000,0,100000}}};
  a.group=b.group=0;return Sat::ScenarioGeometry({a,b});
}
void pose(const AnimationRig &actual,const AnimationRig &expected) {
  const auto &a=actual.jointMatrices(),&b=expected.jointMatrices();check(a.size()==119 && b.size()==119,"119 original player joints");
  for(unsigned i=0;i<119;++i)for(unsigned k=0;k<12;++k)check(std::abs(a[i][k]-b[i][k])<.001f,"Knife source pose part"+std::to_string(i)+" component"+std::to_string(k));
}
}
int main(int argc,char **argv){using namespace re4;try {
  if(argc!=2)throw std::runtime_error("Usage: k17_weapons_check FIXTURE_DISC_ROOT");
  const auto files=std::filesystem::path(argv[1])/"files";
  const auto pl=(files/"em/pl00.drs").string(),wep=(files/"em/wep01.drs").string(),em=(files/"em/em12.drs").string();
  const auto floor=floorData();SatCollision world(floor),eat(floor,SatQueryMode::Effect);
  WalkProbe probe(world);check(probe.place({0,0,0}),"Test floor supports the authored player capsule");
  LeonPreview player(pl,wep,em,true);player.setPlacement(probe.position(),0);player.setRoomScriptEyeMode(1);
  const auto core=RoomArchive::LoadDrs((files/"etc/core.das").string());
  const auto *eff=core.find("EFF");check(eff!=nullptr,"Original CORE EFF present");
  const auto coreEff=core.copy(eff-core.entries().data());
  PlayerProjectiles projectiles(pl,coreEff);WorldEffectCamera camera;
  unsigned weapons{},shots{};
  for(unsigned id=0;id<256;++id) {
    const auto *source=OriginalWeaponCarry(id);if(!source)continue;
    const auto label=std::string(OriginalItemLabel(id))+" ("+std::to_string(id)+") ";
    try {
    check(player.equipWeapon(id),label+"loads its exact model, grips, FCVs and SEQs");++weapons;
    check(player.weaponAttackImplemented(),label+"has its original attack controller");
    check(player.setWeaponAmmo(id,player.magazineCapacity(),20),label+"accepts its native ammo family");
    PlayerActionInput input;input.aimHeld=input.aimPressed=true;
    player.suspendClock();player.step(probe,0,0,false,false,1./30,input);
    check(player.aiming(),label+"first fresh shoulder edge starts aiming after suspension");
    input.aimPressed=false;for(unsigned n=0;n<100 && player.action()!=PlayerAction::Aim;++n)player.step(probe,0,0,false,false,1./30,input);
    check(player.action()==PlayerAction::Aim,label+"draw finishes in the correct ready state");
    if(player.weapon()->hasMuzzleEffects()) {
      const auto data=BuildWeaponWorldEffects(player.weapon()->effects(),coreEff);
      check(!data.sprites.targets.front().records.empty() && data.weaponPart<player.weapon()->rig().bones().size(),label+"muzzle uses its actual weapon part");
    }
    const auto before=player.shots(),loaded=player.magazine(),reserve=player.reserve();
    input.fireHeld=input.firePressed=true;
    const auto shotCallback=[&](const PlayerShot *shot) {
      if(!shot)return;
      ++shots;
      check(shot->weaponItemId==id,label+"emits its actual weapon damage identity");
      check(shot->pellets.size()==(OriginalShotgun(source->number)?13u:0u),label+"uses the original pellet family");
      if(shot->projectile())projectiles.launch(*shot,player,eat);
      else check(GanadoFirearmHitDamage(source->number,0,1000000,false).damage>0,label+"registered Ganado damage is positive");
    };
    player.step(probe,0,0,false,false,1./30,input,shotCallback);input.fireHeld=input.firePressed=false;
    for(unsigned n=0;n<160 && player.shots()==before;++n)player.step(probe,0,0,false,false,1./30,input,shotCallback);
    check(player.shots()==before+1,label+"one trigger launches exactly one shot/throw");
    if(!player.weapon()->infinite()) {
      check(source->number==28?player.reserve()==reserve-1:player.magazine()==loaded-1,label+"debits one round from the correct owner");
    }
    if(source->number==9 || source->number==10) {
      check(player.scopeReady(),label+"firing retains scope camera");
      const auto ray=player.shotRay();const auto view=player.scopeCamera();
      check(ray.from==view.eye && view.fovDegrees==45,label+"scope ray starts at original eye and45-degree FOV");
    }
    if(source->number==19 || source->number==22 || source->number==23) {
      std::vector<ObjectBlast> blasts;
      for(unsigned n=0;n<360 && blasts.empty();++n){projectiles.tick(eat,camera,{},{});blasts=projectiles.drainBlasts();}
      check(blasts.size()==1,label+"real thrown model produces one due blast");
      // wep19::equipWeapon / objSubWep: weapon22 is incendiary (item2),
      // weapon23 is flash (item0xE), not the inverse.
      check(blasts[0].weaponNo==(source->number==19?19u:source->number==22?20u:23u),label+"explosion distinguishes damage, fire and flash");
    }
    projectiles.clear();input.aimHeld=false;
    for(unsigned n=0;n<300 && player.action()!=PlayerAction::Footwork;++n)player.step(probe,0,0,false,false,1./30,input);
    check(player.action()==PlayerAction::Footwork,label+"release returns to walking");
    if(source->number!=13 && source->number!=19 && source->number!=22 && source->number!=23 && source->number!=28) {
      check(player.setWeaponAmmo(id,0,20),label+"sets real empty magazine");
      input.aimHeld=input.aimPressed=true;player.step(probe,0,0,false,false,1./30,input);input.aimPressed=false;
      for(unsigned n=0;n<100 && player.action()!=PlayerAction::Aim;++n)player.step(probe,0,0,false,false,1./30,input);
      input.reloadPressed=true;player.step(probe,0,0,false,false,1./30,input);input.reloadPressed=false;
      for(unsigned n=0;n<240 && !player.magazine();++n)player.step(probe,0,0,false,false,1./30,input);
      check(player.magazine()>0 && (player.weapon()->infinite() || player.reserve()<20),label+"authored reload commit fills its magazine");
      input.aimHeld=false;for(unsigned n=0;n<360 && player.action()!=PlayerAction::Footwork;++n)player.step(probe,0,0,false,false,1./30,input);
      check(player.action()==PlayerAction::Footwork,label+"reload release completes");
    }
    }catch(const std::exception &e){throw std::runtime_error(label+e.what());}
  }
  check(weapons==38,"All38 source weapon/accessory profiles exercised");check(shots==weapons,"Every equipped original profile fired once");
  for(unsigned id:{0x35u,0x36u,0x52u}) {
    projectiles.clear();check(player.equipWeapon(id),"Finite-flight weapon equipped");
    PlayerShot shot{1000+id,{0,1500,0},{0,1500,100000}};shot.weaponItemId=id;shot.powerLevel=2;
    projectiles.launch(shot,player,eat);
    check(projectiles.active()==1 && projectiles.draws().size()==1,"Launch owns one actual source projectile model");
    check(projectiles.drainBlasts().empty(),"Launching never applies an immediate radial hit");
    unsigned impacts{};std::vector<ObjectBlast> blasts;
    const auto target=[](const CollisionVec &a,const CollisionVec &b,unsigned) -> std::optional<AuthoredHitResult> {
      std::array<float,3> cross{};double length{};
      for(unsigned k=0;k<3;++k){cross[k]=float((a[k]+b[k])*.5);length+=(b[k]-a[k])*(b[k]-a[k])*.25;}
      if(length<.000001)return {};
      return AuthoredHitResult{1,float(length),cross};
    };
    const auto impact=[&](const PlayerShot &ray){
      ++impacts;check(ray.flightImpact && !ray.projectile() && ray.weaponItemId==id,"Finite flight preserves damage identity at one impact");
      const auto damage=GanadoFirearmHitDamage(ray.weaponNumber(),ray.powerLevel,1000,false).damage;
      check(id==0x36?damage==0:damage>0,"Source mine attachment has zero direct damage; its explosion and arrows have separate damage owners");
    };
    for(unsigned n=0;n<340 && projectiles.active();++n) {
      projectiles.tick(eat,camera,target,impact);const auto due=projectiles.drainBlasts();blasts.insert(blasts.end(),due.begin(),due.end());
    }
    check(!projectiles.active(),"Impacted flight expires through its source owner");
    if(id==0x35)check(impacts==0 && blasts.size()==1 && blasts[0].weaponNo==18,"Rocket resolves a radial blast instead of a hitscan bullet");
    else if(id==0x36)check(impacts==1 && blasts.size()==1 && blasts[0].weaponNo==19 && blasts[0].radius==6000,"Mine sticks once then detonates at its selected power radius");
    else check(impacts==1 && blasts.empty(),"Arrow applies one finite-flight impact without an explosion");
  }
  {
    std::ifstream stream(files/"etc/emleon00.esl",std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
    const EnemySpawns spawns(bytes);const auto *spawn=spawns.find(EnemyEncounter::TestSpawn);check(spawn!=nullptr,"Original Ganado ESL for flash response");
    EnemyEncounter enemy(world,eat,*spawn,em,true,EnemyEncounter::TestPlacement{{0,0,0},0});
    player.setPlacement({20000,0,20000},0);enemy.tick(player.feet());const auto hp=enemy.hp();
    ObjectBlast flash{0,0,1,23,{0,1000,0},15000,1};
    check(enemy.applyBlast(flash,player,probe),"Flash reaches the posed original hit shapes");enemy.tick(player.feet());
    check(enemy.hp()==hp && enemy.lastDamageSource()==EnemyEncounter::DamageSource::Flash && enemy.actor().motion()==EnemyMotion::FlashStart,"Flash starts the original reaction without damaging HP");
    for(unsigned n=0;n<2000 && enemy.actor().motion()>=EnemyMotion::FlashStart;++n)enemy.tick(player.feet());
    check(enemy.actor().motion()<EnemyMotion::FlashStart && enemy.hp()==hp,"Original flash start/recovery/finish resumes AI with unchanged HP");
    player.setPlacement(probe.position(),0);
  }
  // Source handgun healthy sheath versus injured PL87, independently replayed
  // with the unchanged original motion solver and the current solved pose.
  const auto playerArchive=RoomArchive::LoadDrs(pl);const auto weaponArchive=RoomArchive::LoadDrs(wep);
  for(unsigned hp:{1200u,700u}) {
    LeonPreview knife(pl,wep,em,true);knife.setPlacement(probe.position(),0);knife.setRoomScriptEyeMode(1);knife.restoreLife(hp);
    PlayerActionInput input;input.knifeHeld=input.knifePressed=true;
    knife.step(probe,0,0,false,false,1./30,input);input.knifePressed=false;
    for(unsigned n=0;n<100 && knife.action()!=PlayerAction::KnifeReady;++n)knife.step(probe,0,0,false,false,1./30,input);
    check(knife.action()==PlayerAction::KnifeReady,"Knife draws with one L1 press");
    input.knifeHeld=false;knife.step(probe,0,0,false,false,1./30,input);
    check(knife.action()==PlayerAction::KnifeHolster,"Knife release enters source sheath");
    // The release dispatcher installs the motion without evaluating it.
    // Copy that solved starting pose, then independently evaluate the source
    // healthy WEP01 ARC30 or injured PL87; this preserves Hokan's input.
    auto reference=std::make_unique<AnimationRig>(SkinnedModel(playerArchive.copy(0)),std::vector<AnimationBytes>{{hp>=800?weaponArchive.copy(0x30-4):playerArchive.copy(0x87-4),{},false}});
    for(unsigned part=0;part<119;++part) {
      reference->setPartPosition(part,knife.rig().partPosition(part));
      reference->setPartRotation(part,knife.rig().partRotation(part));
      reference->setPartScale(part,knife.rig().partScale(part));
    }
    reference->startActionFlags(0,5,0,0);
    // After the original five-frame interpolation, quaternion history from
    // the preceding cMot3 draw cannot affect this authored sheath comparison.
    for(unsigned n=0;n<6;++n) {
      reference->advanceAction();reference->setPartRotation(0x21,reference->partRotation(0x20));
      knife.step(probe,0,0,false,false,1./30,input);reference->setWaistYaw(knife.waistYaw());
    }
    pose(knife.rig(),*reference);
  }
  std::cout<<"K17 original38 weapons, first aim edge, real shots/ammo/reloads, projectile fuses and119-joint healthy/injured knife poses PASS: "<<checks<<" checks\n";
}catch(const std::exception &e){std::cerr<<"K17 weapon failure: "<<e.what()<<'\n';return 1;}}
