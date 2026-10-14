#include "save_native.h"
#include "asset_reader.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
unsigned checks;
void check(bool value,const char *message) {++checks;if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F f,const char *message) {bool caught=false;try {f();}catch(const std::exception&){caught=true;}check(caught,message);}
std::vector<std::uint8_t> read(const std::string &p) {
  std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("fixture missing");
  const auto n=f.tellg();std::vector<std::uint8_t>b(static_cast<std::size_t>(n));f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),n);return b;
}
}
int main(int argc,char **argv) {
  try {
    if(argc<2 || argc>3)throw std::runtime_error("usage: save_check DISC_FILES [FIXTURE_OUTPUT]");
    using namespace re4;const std::string files=argv[1];
    InventoryCase bag;bag.resetGameStart();check(bag.add(0x57,1),"treasure capture");
    NativeSaveSnapshot snapshot;snapshot.inventory=bag.items();snapshot.caseSize=bag.size();snapshot.equippedSlot=bag.equippedSlot();
    snapshot.scenario.pesetas=12890;snapshot.position={-71920.5f,-20.75f,-13769.125f};snapshot.heading=-2.49f;
    snapshot.life=760;snapshot.maxLife=1200;snapshot.ashleyLife=451;snapshot.ashleyMaxLife=800;snapshot.chapter=0;snapshot.saveCount=8;snapshot.playSeconds=3723;
    snapshot.radioHeard={0xc0000000,0x80000000,1};snapshot.radioMessageNo=1;
    snapshot.scenario.scenario.set(17);snapshot.scenario.scenario.set(255);snapshot.scenario.key.set(63);
    snapshot.scenario.globalItems.set(127);snapshot.scenario.globalItemsFound.set(64);
    auto &r=snapshot.scenario.room(0x100);r.room.set(10);r.room.set(13);r.room.set(15);r.items.set(1);r.itemsFound.set(127);r.etc[63]=0x8045;
    snapshot.scenario.room(0x120);snapshot.passedRooms[0x100]=0x80;
    snapshot.enemyList=read(files+"/etc/emleon00.esl");snapshot.enemyDeaths.emplace();
    SetOriginalSaveEnemyDead(*snapshot.enemyDeaths,0,37);SetOriginalSaveEnemyDead(*snapshot.enemyDeaths,0,255);
    SetOriginalSaveEnemyDead(*snapshot.enemyDeaths,11,0);
    auto sections=EncodeNativeSave(snapshot);const AssetReader global{sections.global};
    check(sections.rooms.size()==0x6628 && OriginalSaveRoomIds().size()==121,"actual source table size");
    check(global.u32(0x9c+4)==0x04000000 && global.u32(0x9c+28)==1,"original death word/end bit");
    check(global.u32(0x23c)==0x4000 && global.u32(0x23c+28)==1,"original scenario words");
    check(global.u16(0x1c)==0x100 && global.u16(0x24)==760 && global.u32(0x18)==12890,"global BE mapping");
    check(global.f32(0x40)==snapshot.position[0] && global.f32(0x4c)==snapshot.heading,"global source float bits");
    check(global.u32(0x3370)==0xc0000000 && global.u32(0x3374)==0x80000000 && global.u32(0x3378)==1 && global.u32(0x3380)==1,"original radio heard MSB words and selected message");
    check(sections.inventory[12]==2 && sections.inventory[13]==1 && sections.inventory[15]==1,"source handgun half-cell center");
    check(sections.merchant[0x300]==50,"source merchant initialization");
    auto initialSnapshot=snapshot;initialSnapshot.radioHeard={};initialSnapshot.radioMessageNo=24;
    const auto initial=EncodeNativeSave(initialSnapshot);
    check(AssetReader{initial.global}.u32(0x3380)==0x18,"source radio initialization before a selected message");
    auto rawBasis=sections;rawBasis.global[0x50]=0x9a;rawBasis.global[0x33b0]=0x56;
    rawBasis.rooms[16+0xa8]=0x78;rawBasis.merchant[0x301]=0x4a;rawBasis.subscreen[2]=0x9b;
    auto preserved=EncodeNativeSave(snapshot,&rawBasis);
    check(preserved.global[0x50]==0x9a && preserved.global[0x33b0]==0x56,"unknown global preserved");
    check(preserved.rooms[16+0xa8]==0x78 && preserved.merchant==rawBasis.merchant && preserved.subscreen==rawBasis.subscreen,"BGM merchant subscreen preserved");
    const auto art=LoadOriginalSaveCardArt(files),jpArt=LoadOriginalSaveCardArt(files,true);
    check(art.banner!=jpArt.banner && art.icon==jpArt.icon && art.palette==jpArt.palette,"source regional banner choice");
    check(std::any_of(art.banner.begin(),art.banner.end(),[](auto n){return n!=0;}) &&
      std::any_of(art.palette.begin(),art.palette.end(),[](auto n){return n!=0;}),"actual card artwork");
    const OriginalSaveCalendar calendar{21,37,18,6,9,2026,2,278,123,456};
    auto card=EncodeOriginalSaveCard(preserved,art,calendar,19);const auto decoded=DecodeOriginalSaveCard(card);
    check(card.size()==0x10000 && decoded.sections.rooms.size()==0x6628,"card block padding and room image");
    check(decoded.metadata.saveCount==9 && decoded.metadata.mode==1 && decoded.metadata.difficulty==5,"source increment/mode/header");
    check(decoded.metadata.room==0x100 && decoded.metadata.playSeconds==3723 && decoded.metadata.calendar.year==2026,"source listing metadata");
    check(decoded.art.banner==art.banner && decoded.art.icon==art.icon && decoded.art.palette==art.palette,"raw art card roundtrip");
    const auto loaded=DecodeNativeSave(decoded.sections);
    check(loaded.position==snapshot.position && loaded.heading==snapshot.heading && loaded.life==760 && loaded.saveCount==9,"native source snapshot roundtrip");
    check(loaded.ashleyLife==451 && loaded.ashleyMaxLife==800 && loaded.playerType==0,"Original Ashley health BE fields survive native save/load");
    check(loaded.radioHeard==snapshot.radioHeard && loaded.radioMessageNo==1,"original radio status survives save/load");
    check(loaded.scenario.scenario.test(17) && loaded.scenario.key.test(63) && loaded.scenario.globalItems.test(127),"global source flags roundtrip");
    check(loaded.scenario.room(0x100).room.test(13) && loaded.scenario.room(0x100).itemsFound.test(127) && loaded.scenario.room(0x100).etc[63]==0x8045,"room source flags roundtrip");
    check(loaded.enemyList->size()==8192 && OriginalSaveEnemyDead(*loaded.enemyDeaths,0,37) && OriginalSaveEnemyDead(*loaded.enemyDeaths,11,0),"source ESL and all death lists");
    InventoryCase recovered;recovered.restoreSnapshot(loaded.caseSize,loaded.inventory,loaded.equippedSlot);
    check(recovered.count(0x57)==1 && recovered.equippedId()==0x23 && recovered.item(0)->loaded==10,"case exact reconstruction");
    check(recovered.item(2)->x==3 && recovered.item(2)->y==1 && recovered.item(2)->slot==2,"case source positions retained");
    auto sparse=loaded.inventory;for(auto &i:sparse)if(i.slot==4)i.slot=12;
    recovered.restoreSnapshot(0,sparse,0);check(!recovered.item(4) && recovered.item(12)->id==0x57,"case slot gaps retained");
    const auto before=recovered.snapshotJson(0);auto invalid=sparse;invalid[1].x=0;invalid[1].y=0;
    rejects([&]{recovered.restoreSnapshot(0,invalid,0);},"overlapping case rejected");
    check(before==recovered.snapshotJson(0),"case invalid-load transaction");
    auto broken=card;broken[0x2044]^=1;rejects([&]{DecodeOriginalSaveCard(broken);},"header corruption rejected");
    broken=card;broken[0x7000]^=1;rejects([&]{DecodeOriginalSaveCard(broken);},"room corruption rejected");
    broken=card;broken.resize(0xeafa);rejects([&]{DecodeOriginalSaveCard(broken);},"truncated file rejected");
    broken=card;broken.resize(0xeafc);check(DecodeOriginalSaveCard(broken).metadata.room==0x100,"source dev-file unpadded image accepted");
    for(unsigned slot=0;slot<20;++slot)check(OriginalSaveSlotName(slot).size()==10,"all source slot names");
    check(OriginalSaveSlotName(0)=="bh4_data00" && OriginalSaveSlotName(19)=="bh4_data19","source slot endpoints");
    rejects([&]{OriginalSaveSlotName(20);},"slot20 rejected");
    for(unsigned mode=1;mode<=3;++mode) {
      auto image=DecodeOriginalSaveCard(EncodeOriginalSaveCard(sections,art,calendar,0,mode));
      check(image.metadata.mode==mode && AssetReader{image.sections.global}.u32(0x33d8)==mode,"source SaveKind normal/chapter/newround");
    }
    auto countWrap=sections;countWrap.global[12]=countWrap.global[13]=255;
    check(DecodeOriginalSaveCard(EncodeOriginalSaveCard(countWrap,art,calendar,0)).metadata.saveCount==0,"original u16 save-count wrap");
    check(std::string(reinterpret_cast<const char*>(card.data()+32))=="FILE 20","original memory-card comments");
    rejects([&]{auto bad=calendar;bad.month=12;EncodeOriginalSaveCard(sections,art,bad,0);},"bad calendar rejected");
    rejects([&]{auto bad=snapshot;bad.position[0]=std::numeric_limits<float>::infinity();EncodeNativeSave(bad);},"bad player float rejected");
    rejects([&]{auto bad=sections;bad.rooms[3]++;ValidateOriginalSaveSections(bad);},"bad room header rejected");
    auto badDas=read(files+"/etc/memcard.das");badDas[0x420]=0xff;
    rejects([&]{ParseOriginalSaveCardArt(badDas);},"bad original art rejected");
    RoomPickupRecord money;money.runtime=true;money.id=0x8f;money.count=1;money.position={-10.9,500.9,30.8};money.effectType=1;
    RoomPickupRecord herb=money;herb.id=6;RoomPickupRecord taken=money;taken.runtimeTaken=true;
    const auto drops=CaptureOriginalPersistentDrops({{0x100,{money,herb,taken}}});
    check(drops.size()==1 && drops[0].id==0x8f,"only original persistent drops");
    snapshot.looseItems=drops;auto dropSections=EncodeNativeSave(snapshot);auto dropState=DecodeNativeSave(dropSections);
    check(dropState.looseItems->size()==1 && (*dropState.looseItems)[0].position[0]==-10 && (*dropState.looseItems)[0].position[1]==400,"source signed drop /10 truncation");
    auto restored=RestoreOriginalPersistentDrops(*dropState.looseItems);check(restored[0x100][0].position[1]==500 && !restored[0x100][0].lifetimeTicks,"source drop reappearance offset");
    rejects([&]{SetOriginalSaveEnemyDead(*snapshot.enemyDeaths,12,0);},"bad enemy list rejected");
    rejects([&]{OriginalSaveEnemyDead(*snapshot.enemyDeaths,0,256);},"bad enemy index rejected");
    auto checkpoint=snapshot;checkpoint.roomContinues=3;checkpoint.chapterContinues=4;checkpoint.totalContinues=5;checkpoint.saveKind=-1;
    const auto checkpointImage=EncodeNativeSave(checkpoint);const AssetReader checkpointGlobal{checkpointImage.global};
    check(checkpointGlobal.u16(0x10)==3 && checkpointGlobal.u16(0x33b8)==4 && checkpointGlobal.u16(0x33ba)==5 && checkpointGlobal.u32(0x33d8)==0xffffffffu,"original Continue/save-kind BE offsets");
    auto continued=DecodeNativeSave(checkpointImage);
    check(continued.roomContinues==3 && continued.chapterContinues==4 && continued.totalContinues==5 && continued.saveKind==-1,"Continue counters/save-kind roundtrip");
    auto dead=checkpoint;dead.life=0;dead.playSeconds+=300;dead.roomContinues=8;dead.chapterContinues=9;dead.totalContinues=10;
    dead.scenario.pesetas=1;dead.inventory.clear();dead.position={0,0,0};
    continued=RestoreNativeCheckpoint(checkpoint,dead);
    check(continued.life==760 && continued.position==checkpoint.position && continued.inventory.size()==checkpoint.inventory.size() && continued.scenario.pesetas==12890,"Continue restores live checkpoint rather than dead world");
    check(continued.playSeconds==dead.playSeconds && continued.roomContinues==9 && continued.chapterContinues==10 && continued.totalContinues==11,"Continue preserves current clock and increments current counters");
    check(continued.difficultyPoint==4700 && continued.gameLevel==4,"original LVADD_DIE rank5 loss800");
    checkpoint.difficulty=6;checkpoint.difficultyPoint=10999;continued=RestoreNativeCheckpoint(checkpoint,dead);
    check(continued.difficultyPoint==10999 && continued.gameLevel==10,"Professional rank remains10");
    checkpoint.difficulty=1;checkpoint.difficultyPoint=5500;continued=RestoreNativeCheckpoint(checkpoint,dead);
    check(continued.gameLevel==1,"easy mode rank uses point/2750");
    checkpoint.difficulty=5;checkpoint.language=1;checkpoint.difficultyPoint=1100;continued=RestoreNativeCheckpoint(checkpoint,dead);
    check(continued.difficultyPoint==1000 && continued.gameLevel==1,"non-Japanese Continue point floor1000");
    checkpoint.language=0;continued=RestoreNativeCheckpoint(checkpoint,dead);
    check(continued.difficultyPoint==620 && continued.gameLevel==0,"Japanese rank1 uses -.6*800.5 truncation");
    dead.roomContinues=dead.chapterContinues=dead.totalContinues=65535;continued=RestoreNativeCheckpoint(checkpoint,dead);
    check(!continued.roomContinues && !continued.chapterContinues && !continued.totalContinues,"original Continue u16 wrap");
    if(argc==3) {
      std::ofstream f(argv[2],std::ios::binary);f.write(reinterpret_cast<const char*>(card.data()),card.size());
      if(!f)throw std::runtime_error("cannot emit fixture");
    }
    std::cout<<"M37 original save codec/native mapper: "<<checks<<" checks PASS\n";
  }catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
