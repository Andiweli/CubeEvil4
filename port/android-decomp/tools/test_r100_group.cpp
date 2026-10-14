#include "enemy_group.h"
#include "r100_scenario.h"
#include "archive.h"
#include "weapon_preview.h"
#include "room_interactions.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks;
void require(bool value,const char *message) {++checks;if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const char *path) {
 std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("Original file missing");
 std::vector<std::uint8_t> b(std::size_t(file.tellg()));file.seekg(0);
 if(!file.read(reinterpret_cast<char*>(b.data()),b.size()))throw std::runtime_error("Original file truncated");
 return b;
}
std::vector<std::uint8_t> sub(const RoomArchive &arc,const char *tag) {
 const auto *entry=arc.find(tag);require(entry,"Original r100 entry");return arc.copy(entry-arc.entries().data());
}
void same(const EnemyGroup &a,const EnemyGroup &b) {
 require(a.worldTicks()==b.worldTicks(),"One shared source world clock");
 for(unsigned i=0;i<a.actors().size();++i) {
  const auto &x=*a.actors()[i],&y=*b.actors()[i];
  require(x.actor().motion()==y.actor().motion() && x.hp()==y.hp(),"30/60Hz same actor phase");
  require(x.actor().rig().status().sequenceFrame==y.actor().rig().status().sequenceFrame &&
    x.actor().rig().status().state==y.actor().rig().status().state,"30/60Hz same SEQ state");
  require(x.feet()==y.feet() && x.heading()==y.heading(),"30/60Hz same actor placement");
 }
}
}
int main(int argc,char **argv) {
 try {
  if(argc!=6)throw std::runtime_error("Usage: r100_group_check R100.dat ESL EM12 PL00 WEP01");
  const auto arc=RoomArchive::Load(argv[1]);const EnemySpawns spawns(read(argv[2]));
  const Sat sat(sub(arc,"SAT")),eat(sub(arc,"EAT"));const SatCollision floor(sat),effects(eat);
  const R100Scenario scenario(sub(arc,"AEV"));
  require(scenario.houseFlagArea().no==9 && scenario.houseFlagArea().raw[0x35]==4 &&
     scenario.houseFlagArea().raw[0x5f]==64,"Original house event flag64 record");
  require(scenario.houseInsideHit(R100FirstPlayerPosition(),R100FirstPlayerHeading),"Original post-s03 player inside house");
  require(!scenario.houseInsideHit({-99685,-484,-1343},2.246),"Original gate outside house flag polygon");
  RoomSaveState save;require(!ReleaseR100HouseAmbush(save,false),"House cannot release before truck");
  save.room.set(10);require(!ReleaseR100HouseAmbush(save,true),"Inside house keeps ambush house flag");
  require(ReleaseR100HouseAmbush(save,false) && save.room.test(4) && !ReleaseR100HouseAmbush(save,false),"House escape sourceRSF4 fires once");
  const auto records=R100AmbushSpawns(spawns,save);require(records.size()==7,"Seven exact original ambush actors");
  const unsigned types[]{4,3,1,3,4,1,0},tools[]{2,1,3,1,2,3,2};
  EnemyGroup group(floor,effects,argv[3]);
  for(unsigned i=0;i<records.size();++i) {
   auto &enemy=group.spawn(*records[i]);enemy.setVisible(false);enemy.setScriptSuspended(true);
   const auto &actor=enemy.actor();
   require(actor.type()==types[i] && actor.weaponType()==tools[i],"Authored EM12 body and tool profile");
   require(actor.holdsWeapon() && actor.visiblePart(actor.weaponBinEntry()),"Each original actor has own authored weapon mesh");
   require(actor.rig().jointMatrices().size()==34 && actor.renderMesh().fallbackTriangles==0,"Every family resolves real skeleton/geometry");
   require(actor.renderMesh().textures.size()==4,"Original body/head/hand and tool TPL complete");
   unsigned corners=0;for(const auto &draw:actor.renderMesh().draws)if(draw.work==actor.weaponBinEntry()) {
    corners+=draw.count;for(unsigned vertex=draw.first;vertex<draw.first+draw.count;++vertex) {
     const auto &skin=actor.renderSkin().at(vertex);
     require(skin.position.count==1 && skin.position.joints[0]==actor.weaponParentPart() && skin.position.rates[0]==1,"Exact source tool parent part");
    }
   }
   require(corners>0 && !enemy.visible() && enemy.scriptSuspended(),"Independent visibility and script suspension");
  }
  for(bool externalFront:{false,true}) {
    EnemyGroup mixed(floor,effects,argv[3]);auto source=*records[0];
    source.position={-79116,860,-38890};source.heading=0;auto &enemy=mixed.spawn(source);
    const auto shape=NormalGanadoHitShapes().at(1);auto offset=shape.offset;offset[1]+=shape.height*.5f;
    const auto center=TransformPoint(WeaponAttachmentMatrix(enemy.actor().rig().jointMatrices().at(1),enemy.placement()),offset);
    PlayerMeleeEvent blade;blade.serial=95;blade.rays.push_back({
        {center[0],center[1],center[2]-800},{center[0],center[1],center[2]+1800}});
    mixed.setExternalTarget([&](const CollisionVec &from,const CollisionVec &,unsigned weapon){
      require(weapon==0x10,"Actual source knife weapon reaches external owner query");
      const CollisionVec p{center[0],center[1],center[2]+(externalFront?-400:500)};float len=0;
      for(unsigned k=0;k<3;++k){const float d=float(p[k]-from[k]);len+=d*d;}
      return std::optional<AuthoredHitResult>{{0,len,{float(p[0]),float(p[1]),float(p[2])}}};
    });
    mixed.queueMelee(blade,blade.rays[0].first);
    require(mixed.takeMeleeExternal(blade.serial+1).empty(),"Wrong knife serial cannot consume external target decision");
    const auto selected=mixed.takeMeleeExternal(blade.serial);
    require(selected.size()==1 && selected[0]==externalFront,"Knife max1 keeps front actor or nearer external owner before pending damage");
    require(enemy.knifeHits()==unsigned(!externalFront),"External-winning knife must not queue damage into farther Ganado");
    require(mixed.takeMeleeExternal(blade.serial).empty(),"Knife target decisions drain once");
  }
  bool duplicate=false;try {group.spawn(*records.front());}catch(const std::exception &){duplicate=true;}
  require(duplicate,"Duplicate authored list identity rejected");
  LeonPreview player(argv[4],argv[5],argv[3],true);WalkProbe probe(floor);
  require(probe.place(R100FirstPlayerPosition()),"Original player placement valid");player.setPlacement(probe.position(),R100FirstPlayerHeading);
  const auto pose=player.rig().jointMatrices();
  const auto inactiveFrame=group.find(3)->actor().rig().status().sequenceFrame;
  for(unsigned tick=0;tick<15;++tick)group.tickScriptActors(player,probe);
  require(player.rig().jointMatrices()==pose && group.find(4)->actor().motion()==EnemyMotion::Idle,"Suspended actors/player do not evaluate motions");
  for(const auto index:R100S20LiveActorIndices) {group.setScriptSuspended(index,false);group.find(index)->setVisible(true);}
  bool walking4=false,walking5=false,idle4=false,idle5=false;
  for(unsigned tick=0;tick<180;++tick) {
   group.tickScriptActors(player,probe);
   walking4|=group.find(4)->actor().motion()==EnemyMotion::Walk;walking5|=group.find(5)->actor().motion()==EnemyMotion::Walk;
   idle4|=group.find(4)->actor().motion()==EnemyMotion::Idle;idle5|=group.find(5)->actor().motion()==EnemyMotion::Idle;
  }
  require(walking4 && walking5 && idle4 && idle5,"Source R100WalkStay walks once to loop state then original wait FCV");
  require(player.rig().jointMatrices()==pose && group.find(3)->actor().rig().status().sequenceFrame==inactiveFrame,"S20 cut2 advances only original indices4/5, preserving original randomized idle phase");
  require(group.worldTicks()==0,"Event actor updates do not advance Leon/group gameplay clock");
  group.releaseR100Ambush();for(auto &enemy:group.actors())enemy->setVisible(true);
  EnemyGroup thirty(floor,effects,argv[3]),sixty(floor,effects,argv[3]);
  for(const auto *record:records) {thirty.spawn(*record);sixty.spawn(*record);}
  thirty.releaseR100Ambush();sixty.releaseR100Ambush();
  LeonPreview p30(argv[4],argv[5],argv[3],true),p60(argv[4],argv[5],argv[3],true);
  WalkProbe w30(floor),w60(floor);require(w30.place(R100FirstPlayerPosition()) && w60.place(R100FirstPlayerPosition()),"Group test player SAT placements");
  p30.setPlacement(w30.position(),R100FirstPlayerHeading);p60.setPlacement(w60.position(),R100FirstPlayerHeading);
  for(unsigned tick=0;tick<120;++tick) {
   thirty.stepPlayer(p30,w30,0,0,false,false,1./30,{});
   sixty.stepPlayer(p60,w60,0,0,false,false,1./60,{});sixty.stepPlayer(p60,w60,0,0,false,false,1./60,{});
   same(thirty,sixty);require(p30.feet()==p60.feet() && p30.rig().status().sequenceFrame==p60.rig().status().sequenceFrame,"Leon evaluated once independent of seven actor count");
  }
  const auto ticks=thirty.worldTicks();const auto before=thirty.find(4)->actor().rig().jointMatrices();
  thirty.stepPlayer(p30,w30,0,0,false,false,0,{});
  require(thirty.worldTicks()==ticks && thirty.find(4)->actor().rig().jointMatrices()==before,"Render-only ticks preserve every group clock/pose");
  // Deliberately aligned test placements preserve original EM12 family,
  // flags/HP and SAT geometry. They exercise real posed nearest-ray dispatch,
  // without assuming that two authored ESL placements happen to line up.
  for(unsigned weapon:{0x23u,0x21u,0x2cu}) {
    EnemyGroup targets(floor,effects,argv[3]);
    auto front=*records[0],back=*records[4];
    front.position={-79116,860,-38890};back.position={-79116,860,-37890};front.heading=back.heading=0;
    auto &a=targets.spawn(front);auto &b=targets.spawn(back);
    const auto shape=NormalGanadoHitShapes().at(1);auto offset=shape.offset;offset[1]+=shape.height*.5f;
    const auto center=TransformPoint(WeaponAttachmentMatrix(a.actor().rig().jointMatrices().at(1),a.placement()),offset);
    PlayerShot shot;shot.serial=1;shot.weaponItemId=weapon;
    shot.from={center[0],center[1],center[2]-800};shot.to={center[0],center[1],center[2]+1800};
    require(targets.target(shot.from,shot.to,weapon==0x2c?7:weapon==0x23?2:1).has_value(),"Real test ray intersects first posed actor before EAT");
    if(weapon==0x2c)for(unsigned pellet=0;pellet<13;++pellet)shot.pellets.push_back({shot.from,shot.to});
    targets.queueShot(shot);
    require(targets.takeShotScenery(shot.serial+1).empty(),"Unrelated source serial does not consume gun scenery");
    const auto scenery=targets.takeShotScenery(shot.serial);
    require(scenery.size()==(shot.pellets.empty()?1u:13u) && !scenery.front(),"Original primary actor target clears scenery before pending damage mutates eligibility");
    require(targets.takeShotScenery(shot.serial).empty(),"Original per-ray scenery capture drains once");
    const auto external=targets.takeShotExternal(shot.serial);
    require(external.size()==scenery.size() && std::none_of(external.begin(),external.end(),[](bool hit){return hit;}),"No external owner selected by actor-only gun list");
    require(a.hp()==500 && b.hp()==500,"Source gun rays queue damage until next enemy tick");
    targets.stepPlayer(player,probe,0,0,false,false,1./30,{});
    if(weapon==0x23)require(a.hp()==350 && b.hp()==500 && a.hits()==1 && b.hits()==0,"Original150-power handgun chooses exactly nearest actor");
    if(weapon==0x21)require(a.hp()==365 && b.hp()==365 && a.hits()==1 && b.hits()==1,"Original Punisher135-power ray selects source two-target priority");
    if(weapon==0x2c)require(a.hp()==0 && b.hp()==0 && a.hits()==1 && b.hits()==1 && a.lastDamage()==500 && b.lastDamage()==500,"Shotgun13 rays select each actor once with source600near power and pending-hit skip");
  }
  // An external owner stays query-only here, exactly as the borrowed owner
  // contract specifies. Use two genuinely posed source Ganados around its
  // measured candidate to prove the sorted source target limit, including
  // a later pellet after the front actor acquired pending damage.
  for(unsigned weapon:{0x23u,0x21u,0x2cu}) {
    EnemyGroup mixed(floor,effects,argv[3]);auto front=*records[0],back=*records[4];
    front.position={-79116,860,-38890};back.position={-79116,860,-37890};front.heading=back.heading=0;
    auto &a=mixed.spawn(front);mixed.spawn(back);
    const auto shape=NormalGanadoHitShapes().at(1);auto offset=shape.offset;offset[1]+=shape.height*.5f;
    const auto center=TransformPoint(WeaponAttachmentMatrix(a.actor().rig().jointMatrices().at(1),a.placement()),offset);
    PlayerShot shot;shot.serial=93;shot.weaponItemId=weapon;
    shot.from={center[0],center[1],center[2]-800};shot.to={center[0],center[1],center[2]+1800};
    mixed.setExternalTarget([&](const CollisionVec &from,const CollisionVec &,unsigned){
      const CollisionVec p{center[0],center[1],center[2]+500};float len=0;
      for(unsigned k=0;k<3;++k){const float d=float(p[k]-from[k]);len+=d*d;}
      return std::optional<AuthoredHitResult>{{0,len,{float(p[0]),float(p[1]),float(p[2])}}};
    });
    if(weapon==0x2c)for(unsigned pellet=0;pellet<13;++pellet)shot.pellets.push_back({shot.from,shot.to});
    mixed.queueShot(shot);
    require(mixed.takeShotExternal(shot.serial+1).empty(),"Wrong serial cannot consume selected external list");
    const auto selected=mixed.takeShotExternal(shot.serial);
    require(selected.size()==(weapon==0x2c?13u:1u),"One captured external decision per source ray");
    require(selected[0]==(weapon==0x21),"Handgun front actor shields external owner; Punisher second sorted target includes it");
    if(weapon==0x2c)require(!selected[0] && selected[1],"Secondary pellet captures eligible external after source pending actor exclusion");
    require(mixed.takeShotExternal(shot.serial).empty(),"Selected external list drains once independently of scenery");
    require(mixed.takeShotScenery(shot.serial).size()==selected.size(),"External drain does not consume per-ray scenery");
    // A query outside the authored segment must not be emitted merely
    // because there are no actor hits on a later shot.
    mixed.setExternalTarget([](const CollisionVec &,const CollisionVec &,unsigned){return std::optional<AuthoredHitResult>{{0,1e15f,{0,0,0}}};});
    shot.serial=94;shot.pellets.clear();mixed.queueShot(shot);
    const auto missed=mixed.takeShotExternal(shot.serial);
    require(missed.size()==1 && !missed[0],"External miss beyond actual segment is not selected");
  }
  // GetWepTargetList2's first scan treats the actual ET00 window as
  // YAT_FLAG_THROUGH, reserving its own damage target while retaining the
  // enemy behind it. A non-through ET0D door leaf stops the same ray. Test
  // original r100 assets/hit dimensions at deliberately aligned positions.
  for(bool throughGlass:{false,true}) {
    EnemyGroup targets(floor,effects,argv[3]);auto source=*records[0];
    source.position={-79116,860,-38890};source.heading=0;
    auto &enemy=targets.spawn(source);
    const auto shape=NormalGanadoHitShapes().at(1);auto offset=shape.offset;offset[1]+=shape.height*.5f;
    const auto center=TransformPoint(WeaponAttachmentMatrix(enemy.actor().rig().jointMatrices().at(1),enemy.placement()),offset);
    auto placements=ReadRoomObjectPlacements(sub(arc,"ETS"));
    RoomObjectPlacement *barrier=nullptr;
    for(auto &placement:placements) {
      placement.position={center[0]+20000,860,center[2]};
      placement.rotation={0,0,0};
      if(!barrier && (throughGlass?placement.id==0:placement.id==0x0d))barrier=&placement;
    }
    require(barrier,"Actual source door/window profile present");
    barrier->position={center[0]+(throughGlass?0:650),860,center[2]-500};
    // The literal r100 initializer keeps the door open before RSF3 (first
    // enemy look). This closed-barrier fixture needs its actual later flags.
    RoomSaveState afterTruck;afterTruck.room.set(3);afterTruck.room.set(10);
    RoomInteractions interiors(0x100,arc,placements,argv[4],afterTruck,&effects);
    targets.setRoomInteractions(&interiors);
    PlayerShot shot;shot.serial=1;shot.weaponItemId=0x23;
    shot.from={center[0],center[1],center[2]-800};shot.to={center[0],center[1],center[2]+1000};
    const auto interior=interiors.target(shot.from,shot.to,2);
    require(interior.has_value() && interior->kind==(throughGlass?InteriorHit::Window:InteriorHit::DoorLeaf),"Original aligned interior target found");
    require(targets.target(shot.from,shot.to,2).has_value()==throughGlass,"Through pane keeps enemy target; non-through door clips");
    targets.queueShot(shot);
    const auto scenery=targets.takeShotScenery(shot.serial);
    require(scenery.size()==1 && !scenery[0],"Source door/window target clears scenery even after queued actor/glass damage");
    if(throughGlass)require(interiors.damage(*interior,2,shot.serial) && interiors.windows()[interior->index].hp==500,"Original glass consumes500HP on same source handgun shot");
    targets.stepPlayer(player,probe,0,0,false,false,1./30,{});
    require(enemy.hp()==(throughGlass?350u:500u) && enemy.hits()==(throughGlass?1u:0u),"Source same ray damages glass and enemy but cannot pass closed door");
    EnemyGroup bladeTargets(floor,effects,argv[3]);auto &bladeEnemy=bladeTargets.spawn(source);
    bladeTargets.setRoomInteractions(&interiors);
    bladeTargets.setExternalTarget([&](const CollisionVec &from,const CollisionVec &,unsigned){
      const CollisionVec p{center[0],center[1],center[2]-400};float len=0;
      for(unsigned k=0;k<3;++k){const float d=float(p[k]-from[k]);len+=d*d;}
      return std::optional<AuthoredHitResult>{{0,len,{float(p[0]),float(p[1]),float(p[2])}}};
    });
    PlayerMeleeEvent blade;blade.serial=96;blade.rays={{shot.from,shot.to}};
    bladeTargets.queueMelee(blade,shot.from);const auto knifeExternal=bladeTargets.takeMeleeExternal(blade.serial);
    require(knifeExternal.size()==1 && knifeExternal[0]==throughGlass,
        "Knife external owner is clipped by closed door, reserved through-window target remains eligible");
    require(!bladeEnemy.knifeHits(),"Blocked/nearer external knife does not hit farther Ganado");
  }
  // Empty target lists preserve the actual EAT point/normal/attributes. This
  // proves a missed gun still produces its source scenery effect, rather
  // than suppressing every shot to avoid the pending-damage artifact.
  bool missedWall=false;
  for(std::size_t p=0;p<eat.polygons.size() && !missedWall;++p) {
    const auto triangle=effects.triangle(p);CollisionVec center{},from{},to{};
    for(unsigned k=0;k<3;++k){center[k]=(triangle[0][k]+triangle[1][k]+triangle[2][k])/3;
      const auto n=eat.normals.at(eat.polygons[p].normal)[k];from[k]=center[k]+n*100;to[k]=center[k]-n*100;}
    const auto expected=effects.line(from,to,SatSubset::All,0,0x00400000);
    if(!expected)continue;
    EnemyGroup noActors(floor,effects,argv[3]);PlayerShot shot;shot.serial=77;shot.from=from;shot.to=to;
    noActors.queueShot(shot);const auto capture=noActors.takeShotScenery(shot.serial);
    require(capture.size()==1 && capture[0] && capture[0]->point==expected->point &&
        capture[0]->normal==expected->normal && capture[0]->attributes==expected->attributes,
        "Original miss retains exact EAT scenery point/normal/attributes");
    missedWall=true;
  }
  require(missedWall,"Actual r100 EAT miss coverage");
  const unsigned voiceTypes[]{0,1,3,4},voiceFirst[]{0x16,0x1e,0x2e,0x36},voiceHurt[]{0x47,0x51,0x65,0x6f},voiceDeath[]{0x4b,0x55,0x69,0x73};
  for(unsigned i=0;i<4;++i) {
   require(GanadoVoiceCue(0,voiceTypes[i])==voiceFirst[i] && GanadoVoiceCue(8,voiceTypes[i])==voiceHurt[i] &&
     GanadoVoiceCue(12,voiceTypes[i])==voiceDeath[i],"Original per-model Ganado voice cue table");
  }
  std::cout<<"M36 seven actual r100 actors/group PASS: "<<checks<<" checks; worldticks="<<ticks<<" families=4 tools=3 sourceWalkStay=2\n";return 0;
 } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
