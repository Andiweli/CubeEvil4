#include "item_effects.h"
#include "archive.h"
#include "asset_reader.h"
#include "ports/rnd.h"
#include "room_pickups.h"
#include "object_actions.h"
#include "item_rules.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
namespace {
using ITEM_ID=unsigned;
using u16=std::uint16_t;
struct ITEM_INFO { unsigned type; };
void itemInfo(ITEM_ID id,ITEM_INFO *info){info->type=re4::OriginalItemInfo(id).type;}
#include "../tests/source_item_effect_oracle.inc"
unsigned checks;
void require(bool value,const char *message) {++checks;if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> eff(const std::string &path) {
  auto a=re4::RoomArchive::LoadDrs(path);const auto *e=a.find("EFF");require(e,"original CORE EFF");
  return a.copy(e-a.entries().data());
}
void same(const re4::ItemEffects &a,const re4::ItemEffects &b,const re4::WorldEffectCamera &camera) {
  const auto x=a.effectModels(),y=b.effectModels();require(x.size()==y.size(),"model count after restore");
  for(unsigned i=0;i<x.size();++i) {
    require(x[i].model.matrix==y[i].model.matrix,"original model matrix after restore");
    require(x[i].model.color==y[i].model.color,"original model color after restore");
    require(x[i].model.serial==y[i].model.serial && x[i].blend==y[i].blend && x[i].zMode==y[i].zMode,"model ownership after restore");
  }
  const auto p=a.quads(camera),q=b.quads(camera);require(p.size()==q.size(),"spark count after restore");
  for(unsigned i=0;i<p.size();++i) {
    require(p[i].position==q[i].position && p[i].uv==q[i].uv,"source path/jitter geometry after restore");
    require(p[i].color==q[i].color && p[i].image==q[i].image && p[i].animation==q[i].animation,"source sprite appearance after restore");
    require(p[i].serial==q[i].serial,"nested EST ownership after restore");
  }
}
}
int main(int argc,char **argv) {try {
  using namespace re4;
  require(argc==3,"Usage: m37_item_effects_check CORE.DAS R100.DAT");
  const auto core=eff(argv[1]);const auto data=BuildCoreItemWorldEffects(core);
  require(data.sprites.targets.size()==15,"source item/child EST bank count");
  unsigned sourceRecords=0,models=0,controllers=0,pathEmitters=0;
  for(const auto &target:data.sprites.targets) {
    const auto original=ReadOriginalEffectSequence(core,target.sequence);
    require(original.size()==target.records.size(),"source EST record count");
    for(unsigned j=0;j<target.records.size();++j) {
      const auto &r=target.records[j];require(r.bytes==original[j].bytes,"edited original item record");++sourceRecords;
      if(r.bytes[1]==255){++models;require(r.externalSeedDraws==16,"source Efm04 seed count");}
      if(r.bytes[1]==12){++controllers;require(r.externalSeedDraws==16,"source Esp0c seed count");}
      if(r.bytes[265]==2){++pathEmitters;require(r.externalSeedDraws==17,"source Espgen02 path+sprite seed count");}
    }
  }
  require(sourceRecords==41 && models==13 && controllers==2 && pathEmitters==6,"original bank record inventory");
  require(OriginalItemEffectType(0x8c)==8 && OriginalItemEffectType(0x78)==3 &&
      OriginalItemEffectType(0x57)==3 && OriginalItemEffectType(0x23)==5 && OriginalItemEffectType(6)==4,"source itemInfo color table");
  for(unsigned id=0;id<256;++id)
    require(OriginalItemEffectType(id)==unsigned(sceAtCheckItemEffectCol(id)),"frozen whole source color function differs");
  require(ResolveOriginalItemEffectType(0x78,0,false)==0 && ResolveOriginalItemEffectType(0x78,0,true)==3 &&
      ResolveOriginalItemEffectType(0x78,6,false)==3 && ResolveOriginalItemEffectType(0x57,3,true)==3,"static/runtime/authored effect distinction");
  const std::vector<std::vector<unsigned>> kinds{{},{0x21},{0x33,0x21},{0x2c},{0x31},{0x2f},{},{0x46,0x21},{0x33,0x21},{0x4d}};
  const std::vector<std::vector<unsigned>> fades{{},{},{0x34},{0x2e},{0x32},{0x30},{},{0x47},{0x34},{}};
  ItemEffectSource s;s.key=1;s.id=0x78;s.position={100,200,300};s.offset={10,20,30};
  for(unsigned type=0;type<=9;++type) {
    s.effectType=int(type);auto requests=OriginalItemEffectRequests(s);
    require(requests.size()==kinds[type].size(),"sceAtItemEffSet dispatch count");
    for(unsigned i=0;i<requests.size();++i) {
      require(requests[i].sequence==kinds[type][i],"sceAtItemEffSet exact CORE id");
      require(requests[i].position==std::array<float,3>{110,220-(type==8 && i==0?800.f:0),330},"sceAtItemEffSet source offset");
    }
    require(OriginalItemEffectRequests(s,true).empty(),"shooting mode suppresses item effects");
    s.disappearing=true;requests=OriginalItemEffectRequests(s);s.disappearing=false;
    require(requests.size()==fades[type].size(),"sceAtItemDisappearEffSet dispatch count");
    if(!requests.empty())require(requests[0].sequence==fades[type][0],"sceAtItemDisappearEffSet exact CORE id");
  }
  s.effectType=1;s.onModel=true;s.parentParts={{1,0,0,100,0,1,0,200,0,0,1,300}};
  require(OriginalItemEffectRequests(s)[0].sequence==0x2d && OriginalItemEffectRequests(s)[0].attached &&
      OriginalItemEffectRequests(s)[0].position==s.offset,"source model-relative plain glow");
  const auto room=RoomArchive::Load(argv[2]);ScenarioState scenario;RoomPickups pickups(0x100,room,scenario);
  for(const auto &source:pickups.effectSources())require(source.effectType==0,"r100 authored static effect0 was replaced");
  const auto *itaEntry=room.find("ITA");require(itaEntry,"r100 ITA");const auto ita=room.copy(itaEntry-room.entries().data());AssetReader r{ita};
  require(r.u16(6)==24 && ita[16+9*156+0x80]==3 && ita[16+10*156+0x80]==5,"authored crow Spinel/grenade colors");
  require(r.f32(16+18*156+0x70)==50 && r.f32(16+22*156+0x70)==50,"r100 original two fifty-unit offsets");
  for(unsigned i=0;i<24;++i)require(ita[16+i*156+0x80]==(i==9?3:i==10?5:0),"all r100 source ITA effect types");
  const auto *ets=room.find("ETS");require(ets,"r100 ETS");
  ObjectActions boxes(0x100,room,ReadRoomObjectPlacements(room.copy(ets-room.entries().data())),nullptr,&scenario);
  require(boxes.effectSources().empty(),"intact source containers have active glow");
  unsigned boxesWithItems=0,visibleBoxes=0,firstBox=0;
  for(unsigned index=0;index<boxes.objects().size();++index) {
    const auto &box=boxes.objects()[index];if(!box.hasItem)continue;
    ++boxesWithItems;
    require((box.item.controlFlags&1) && !box.item.effect,"r100 fixed container source effect/control inventory");
    const float height=box.shapes[0].height*.5f;
    const auto from=TransformPoint(box.matrix,{0,height,-2000}),to=TransformPoint(box.matrix,{0,height,2000});
    const CollisionVec a{from[0],from[1],from[2]},b{to[0],to[1],to[2]};
    const auto hit=HitAuthoredShapes(box.shapes,{},box.matrix,a,b,1e16f,1);
    require(bool(hit),"source container original hit shape");
    require(boxes.applyShot({index,box.placement.slot,*hit},index+1),"source container shot break");
    require(boxes.itemModelVisible(index)==boxes.states()[index].itemActive,"fixed r100 ITA original model visibility");
    if(boxes.states()[index].itemActive){if(!visibleBoxes)firstBox=index;++visibleBoxes;}
  }
  require(boxesWithItems && visibleBoxes && boxes.effectSources().size()==visibleBoxes,"r100 container live sources count");
  for(const auto &source:boxes.effectSources())require(source.effectType==0,"fixed container effect0 replaced by invented glow");
  require(boxes.takePickup(firstBox).has_value(),"r100 container normal pickup");
  require(boxes.effectSources().size()==visibleBoxes-1,"container pickup retained effect owner");
  const auto n=pickups.addRuntimeDrop(0x57,0,{-85630,100,-4640},3);
  require(pickups.items()[n].position==CollisionVec{-85630,200,-4640} && pickups.items()[n].effectType==3,"authored runtime +100/sourcecolor");
  require(!pickups.visible(int(n)),"SceAtCreateItemAt hidden model was rendered");
  const auto saved=pickups.runtimeDrops();RoomPickups restoredPickups(0x100,room,scenario);restoredPickups.restoreRuntimeDrops(saved);
  require(restoredPickups.runtimeDrops()[0].position==saved[0].position && restoredPickups.runtimeDrops()[0].effectType==3,"room restore double-applied item height/color");
  require(pickups.persistentRuntimeDrops().empty(),"temporary Spinel was saved across room exit");
  RoomPickups transition(0x100,room,scenario);
  transition.addRuntimeDrop(0x57,0,{0,0,0});transition.addRuntimeDrop(0x78,0,{0,0,0});
  unsigned keyId=0;for(unsigned id=1;id<256;++id)if(OriginalItemInfo(id).type==5){keyId=id;break;}
  require(keyId,"source persistent type5 key exists");transition.addRuntimeDrop(keyId,0,{0,0,0});
  require(transition.runtimeDrops().size()==3 && transition.persistentRuntimeDrops().size()==1 &&
      transition.persistentRuntimeDrops()[0].id==keyId,"same-room drops versus original SAVE_ITEM room-exit selection");
  const auto runtimeSources=pickups.effectSources();ItemEffects a(core),b(core);RndInit(0x1471);
  a.sync(runtimeSources);b.sync(runtimeSources);
  WorldEffectCamera camera;camera.eye={-85630,1600,-8000};camera.forward={0,0,1};
  require(a.modelMeshes().size()==1 && a.modelMeshes().at(0x25).fallbackTriangles==0,"real CORE0x25 BIN/TPL");
  const auto &mesh=a.modelMeshes().at(0x25);
  require(mesh.vertices.size()==mesh.normals.size() && mesh.uv.size()==mesh.vertices.size()/3*2 &&
      mesh.colors.size()==mesh.vertices.size()/3*4,"original CORE model mesh attributes");
  require(a.effectModels().size()==1 && a.effectModels()[0].model.color[3]==0,"source Efm04 starts transparent");
  unsigned sparks=0;
  for(unsigned frame=0;frame<600;++frame) {
    a.tick(&camera);b.tick(&camera);require(RndState()==0x1471,"item visual RNG leaked into game RNG");
    if(frame<16) {
      const auto model=a.effectModels()[0].model;
      const float expected=85.f*(float(std::min(frame,15u))/15.f)/255.f;
      require(std::abs(model.color[3]-expected)<1e-7f,"literal cObj04 fade-in envelope");
    }
    sparks+=unsigned(a.quads(camera).size());
    if(frame%37==0) {const auto state=a.snapshot();require(b.restore(state),"original item state restore");same(a,b,camera);}
  }
  require(sparks && a.instanceCount()==1,"source Espgen02 animated sparks missing");same(a,b,camera);
  const auto clock=a.ticks();const auto before=a.snapshot().random;
  for(unsigned i=0;i<120;++i)a.sync(runtimeSources);
  require(a.ticks()==clock && a.snapshot().random==before,"sync/pause restarted source effects");
  auto invalid=a.snapshot();invalid.signature^=1;require(!a.restore(invalid),"foreign effect snapshot accepted");
  const auto intact=a.snapshot();
  invalid=intact;require(!invalid.instances.front().models.empty(),"restore model validation fixture");
  invalid.instances.front().models.front().state.angle[0]=std::numeric_limits<float>::quiet_NaN();
  require(!a.restore(invalid),"nonfinite original model snapshot accepted");
  require(a.snapshot().random==intact.random && a.ticks()==intact.ticks,"rejected restore consumed visual RNG/clock");same(a,b,camera);
  invalid=intact;require(!invalid.instances.front().pathSprites.empty(),"restore path validation fixture");
  invalid.instances.front().pathSprites.front().pattern=65535;
  require(!a.restore(invalid),"foreign original path pattern accepted");
  require(a.snapshot().random==intact.random && a.ticks()==intact.ticks,"rejected path restore mutated visual RNG/clock");same(a,b,camera);
  s=runtimeSources.back();s.disappearing=true;a.sync({s});require(a.effectModels().size()==1 && a.quads(camera).empty(),"source fade variant retained old generator");
  for(unsigned frame=0;frame<180;++frame)a.tick(&camera);
  require(a.effectModels().empty() && a.quads(camera).empty(),"source disappear model outlived fade");
  ItemEffectSource money;money.key=2;money.id=0x78;money.effectType=2;money.position={-86000,100,-4600};
  a.sync({money});for(unsigned frame=0;frame<300;++frame)a.tick(&camera);
  require(a.instanceCount()>2 && !a.quads(camera).empty() && !a.effectModels().empty(),"nested CORE21→24/25 sparkle missing");
  const auto copy=a.snapshot();require(b.restore(copy),"nested EST/model state restore");same(a,b,camera);
  for(unsigned frame=0;frame<120;++frame){a.tick(&camera);b.tick(&camera);same(a,b,camera);}
  a.sync({});require(a.sourceCount()==0 && a.instanceCount()==0 && a.quads(camera).empty() && a.effectModels().empty(),"pickup did not remove all nested kinds");
  require(pickups.take(n).has_value(),"ordinary inventory pickup for source glow");
  for(const auto &source:pickups.effectSources())require(source.key!=0x20000000ULL+n+1,"taken item retained glow source");
  RoomPickups expiry(0x100,room,scenario);const auto e=expiry.addRuntimeDrop(1,0,{0,0,0});
  for(unsigned tick=0;tick<55*30;++tick)expiry.tick();
  require(expiry.items()[e].lifetimeTicks==6*30,"original six-second remaining phase");
  bool fading=false;for(const auto &source:expiry.effectSources())if(source.id==1)fading=source.disappearing;
  require(fading,"source expiry fade transition missing");
  for(unsigned tick=0;tick<6*30;++tick)expiry.tick();
  require(expiry.items()[e].runtimeTaken,"source temporary drop remained after61 seconds");
  RoomPickups persistent(0x100,room,scenario);const auto p=persistent.addRuntimeDrop(0x1b,0,{0,0,0});
  require(sceAtCheckSaveItem(0x1b)==1 && sceAtCheckSaveItem(0x78)==0,"frozen whole source save-type function");
  for(unsigned tick=0;tick<2000;++tick)persistent.tick();
  require(!persistent.items()[p].runtimeTaken && !persistent.items()[p].lifetimeTicks,"original type5 key persistence");
  std::cout<<"M37 original item effects: "<<checks<<" checks PASS; raw CORE banks, Efm04 model, source paths, nested sparkle, pickup/removal/expiry/restore.\n";
  return 0;
}catch(const std::exception &e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;}}
