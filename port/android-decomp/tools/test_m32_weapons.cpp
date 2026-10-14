#include "leon_preview.h"
#include "weapon_preview.h"
#include "muzzle_effects.h"
#include "archive.h"
#include "walk_probe.h"
#include "asset_reader.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
namespace {
unsigned checks{};
void check(bool value,const char *why){if(!value)throw std::runtime_error(why);++checks;}
std::vector<unsigned char> data(const re4::RoomArchive &a,const char *tag) {
  const auto *e=a.find(tag);if(!e)throw std::runtime_error(tag);return a.copy(e-a.entries().data());
}
void finite(const std::vector<re4::Matrix34> &matrices) {
  for(const auto &m:matrices)for(auto f:m)check(std::isfinite(f),"Real weapon/player pose is finite");
}
}
int main(int argc,char **argv){try {
  if(argc!=7)throw std::runtime_error("Usage: m32_weapons_check PL00 WEP01 WEP02 WEP07 CORE R101");
  re4::LeonPreview leon(argv[1],argv[2]);
  leon.installHandgun(argv[3]);leon.installShotgun(argv[4]);
  const auto core=re4::RoomArchive::LoadDrs(argv[5]);
  const auto coreLighting=re4::ReadRoomLighting(core);
  check(coreLighting && coreLighting->present[8] && !coreLighting->cuts[8].lights.empty(),"Original CORE cut8 light0 exists for Esp11");
  const auto room=re4::RoomArchive::Load(argv[6]);
  re4::Sat sat(data(room,"SAT"));re4::SatCollision world(sat);re4::WalkProbe probe(world);
  check(probe.reset(),"Real village SAT provides a valid start");
  leon.setPlacement(probe.position(),0);leon.setUnlimitedHealth(true);
  check(leon.rig().clipCount()>64 && leon.rig().clipCount()<=128,"Both owned weapon profiles use bounded original FCVs");
  for(unsigned id:{0x23u,0x2cu,0x21u}) {
    check(leon.equipWeapon(id)&&leon.equippedWeaponId()==id,"Real owned profile equips from case item ID");
    check(leon.magazineCapacity()==(id==0x2c?6u:10u),"Capacity comes from original item weapon_tbl");
    unsigned rightGroups=0,leftGroups=0;
    for(const auto &draw:leon.renderMesh().draws) {
      if(draw.work>=600 && draw.work<=602 && leon.visiblePart(draw.work)) {
        check(draw.work==(id==0x21?600u:id==0x23?601u:602u),"Only equipped original weapon grip is visible");++rightGroups;
      }
      if((draw.work==20 || draw.work==618)&&leon.visiblePart(draw.work)) {
        check(draw.work==(id==0x2c?618u:20u),"Original shotgun left grip replaces the handgun left grip");++leftGroups;
      }
    }
    check(rightGroups>0&&leftGroups>0,"Both original authored grip models are resident and drawn");
    check(leon.setWeaponAmmo(id,id==0x2c?6:10,20),"Case authority initializes real ammo");
    check(!leon.setWeaponAmmo(id,leon.magazineCapacity()+1,0),"Impossible loaded capacity is rejected");
    check(!leon.setWeaponAmmo(id,1,65535),"Original u16 ammo total is bounded");
    re4::PlayerActionInput aim;aim.aimHeld=true;
    for(unsigned tick=0;tick<20;++tick)leon.step(probe,0,0,false,false,1./30,aim);
    check(leon.action()==re4::PlayerAction::Aim && leon.laserReady(),"Draw reaches original pitched ready pose and laser");
    const auto oldShots=leon.shots(),oldMagazine=leon.magazine();
    re4::PlayerShot emitted;
    aim.fireHeld=aim.firePressed=true;
    leon.step(probe,0,0,false,false,1./30,aim,[&](const auto *shot){if(shot)emitted=*shot;});
    check(leon.shots()==oldShots+1 && leon.magazine()==oldMagazine-1,"One trigger debits exactly one bullet/shell");
    check(emitted.weaponItemId==id && emitted.serial==leon.shots(),"Simulation hit event names actual equipped gun");
    check(emitted.pellets.size()==(id==0x2c?13u:0u),"Shotgun uses original 13-pellet emission, handgun one ray");
    check(!leon.laserReady(),"Source fire state briefly suppresses sight before its shot frame");
    aim.fireHeld=aim.firePressed=false;
    const auto readyTick=leon.weapon()->fireReadyTicks();
    for(unsigned tick=1;tick<=readyTick;++tick) {
      leon.step(probe,0,0,false,false,1./30,aim);
      check(leon.laserReady()==(tick>=readyTick),"Sight returns at source firing-speed threshold while recoil continues");
    }
    check(leon.action()==re4::PlayerAction::Fire && leon.laserReady(),"Recoil-end no longer prolongs sight suppression");
    for(unsigned tick=0;tick<15;++tick)leon.step(probe,0,0,false,false,1./30,aim);
    aim.reloadPressed=true;leon.step(probe,0,0,false,false,1./30,aim);aim.reloadPressed=false;
    for(unsigned tick=0;tick<110;++tick)leon.step(probe,0,0,false,false,1./30,aim);
    check(leon.magazine()==leon.magazineCapacity() && leon.reserve()==19,"Original local weapon reload commits once at authored frame");
    finite(leon.rig().bones());finite(leon.weapon()->rig().bones());
    const auto effect=re4::BuildWeaponWorldEffects(leon.weapon()->effects(),data(core,"EFF"));
    check(effect.sprites.targets[0].records.size()==(id==0x2c?9u:7u),"Actual WEP EST0 flash and smoke records loaded, no invented sprite");
    re4::MuzzleEffects muzzle(effect);
    check(muzzle.trigger(leon.shots()),"Authored effect starts once per shot");
    check(!muzzle.trigger(leon.shots()),"Render refresh cannot retrigger shot effect");
    const auto attachment=re4::WeaponAttachmentMatrix(leon.rig().jointMatrices().at(10),leon.placement());
    const auto parent=re4::WeaponAttachmentMatrix(leon.weapon()->rig().jointMatrices().at(3),attachment);
    muzzle.tick(parent);
    re4::WorldEffectCamera camera;camera.eye={float(probe.position()[0]),float(probe.position()[1]+1200),float(probe.position()[2]-2000)};
    auto quads=muzzle.quads(camera);check(!quads.empty(),"First authored firing tick emits real muzzle flash quads");
    const auto light=muzzle.lights(*coreLighting);
    check(light.size()==1 && light[0].radius==(id==0x2c?6000.f:4000.f),"Esp11 publishes the original size-derived muzzle light radius");
    check(light[0].flags==67 && light[0].color==std::array<unsigned char,4>{255,205,165,64},"Esp11 preserves CORE light base color and original effect tint/half-alpha");
    check(light[0].enableMask==coreLighting->cuts[8].lights[0].enableMask && light[0].type==2,
          "Transient muzzle light retains original CORE GX class and enable mask");
    for(const auto &q:quads)for(const auto &p:q.position)for(float f:p)check(std::isfinite(f),"Muzzle geometry remains finite");
    const auto saved=muzzle.state();re4::MuzzleEffects restored(effect);
    check(restored.restore(saved),"Pause/EGL snapshot restores current source effect age");
    check(restored.quads(camera).size()==quads.size(),"Effect snapshot preserves quad count");
    check(restored.lights(*coreLighting)[0].position==light[0].position,"Effect snapshot preserves original parented muzzle light position");
    const auto bankSignature=restored.data().sprites.signature;
    restored.clear();
    check(restored.state().instances.empty() && restored.quads(camera).empty() && restored.lights(*coreLighting).empty(),
          "Successful room commit clears old flash, smoke and source transient light");
    check(restored.data().sprites.signature==bankSignature && !restored.trigger(leon.shots()),
          "Room effect clear preserves decoded banks and prevents duplicate triggering");
    check(restored.trigger(leon.shots()+1),"Next original shot can emit normally after room clear");
    restored.tick(parent);check(!restored.quads(camera).empty(),"Room clear needs no texture bank reload to show next flash");
    muzzle.tick(parent);
    check(muzzle.lights(*coreLighting).size()==(id==0x2c?1u:0u),"Esp11 lives exactly one firing tick for handgun and two for shotgun");
    muzzle.tick(parent);check(muzzle.lights(*coreLighting).empty(),"Original transient muzzle light expires without persistent ambient pollution");
    for(unsigned tick=0;tick<180;++tick)muzzle.tick(parent);
    check(muzzle.quads(camera).empty(),"Source flash/smoke finish without permanent billboard");
    check(leon.equipWeapon(id==0x2c?0x23:0x2c),"Case weapon selection safely aborts aim and preserves ammo");
    check(leon.weaponAmmo(id)->first==(id==0x2c?6u:10u)&&leon.weaponAmmo(id)->second==19,"Each gun retains independently loaded ammunition");
  }
  check(leon.equipWeapon(0x2c)&&leon.setWeaponAmmo(0x2c,2,17)&&leon.setWeaponAmmo(0x23,8,23),"Distinct equipped and parked ammo state configured");
  re4::LeonPreview next(argv[1],argv[2]);next.installHandgun(argv[3]);next.installShotgun(argv[4]);next.inheritInventory(leon);
  check(next.equippedWeaponId()==0x2c&&next.weaponAmmo(0x2c)==leon.weaponAmmo(0x2c)&&next.weaponAmmo(0x23)==leon.weaponAmmo(0x23),"Room recreation preserves actual selected shotgun and both ammo states");
  check(next.shots()==leon.shots()&&next.unlimitedHealth(),"Room transfer preserves serial and required unlimited health");
  next.reset();check(next.magazine()==6&&next.reserve()==0,"Explicit actor reset never overfills a six-shell shotgun");
  std::cout<<"M32 actual weapon assets PASS: "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
