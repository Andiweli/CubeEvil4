// Exercise the production JNI/controller/scene path with owned original data.
#define RE4_M27_ENTRY previous_m27_inventory_main
#include "test_m27_runtime.cpp"
#undef RE4_M27_ENTRY

namespace m32_inventory {
using namespace m27_test;
std::uint64_t number(const std::string &json, const char *field) {
  const auto p = json.find(std::string("\"") + field + "\":");
  if (p == std::string::npos) throw std::runtime_error("Missing native snapshot field");
  return std::stoull(json.substr(p + std::string(field).size() + 3));
}
unsigned ownedSlot(unsigned id) {
  for (const auto &item : inventoryCase.items()) if (item.active && item.id == id) return item.slot;
  throw std::runtime_error("Expected case item missing");
}
std::array<std::array<std::uint32_t,4>,2> localTaken() {
  const auto &room = static_cast<const re4::ScenarioState &>(scenarioState).room(activeRoomId);
  return {room.items.words(),room.itemsFound.words()};
}
std::size_t loose(unsigned id) {
  for (std::size_t n = 0; n < roomPickups->items().size(); ++n)
    if (roomPickups->items()[n].id == id) return n;
  throw std::runtime_error("Expected original ITA item missing");
}
bool approach(Jni &jni, std::size_t index) {
  const auto &item = roomPickups->items().at(index);
  for (const double height : {3214., item.position[1], item.position[1]-800., item.position[1]-1500.})
    for (const double distance : {0.,250.,450.,650.,900.})
      for (unsigned heading = 0; heading < 24; ++heading) {
        const auto yaw = float(heading*6.283185307179586/24);
        re4::CollisionVec p{item.position[0]-std::sin(yaw)*distance,height,
                             item.position[2]-std::cos(yaw)*distance};
        if (!walkProbe->place(p)) continue;
        leon->setPlacement(walkProbe->position(),yaw);
        re4::SceneFollowHeading(yaw); re4::SceneSeedFollowView(0,0);
        auto center = leon->feet(); center[1] += 250;
        auto front = center; front[0]+=std::sin(yaw)*550; front[2]+=std::cos(yaw)*550;
        if (auto hit=collisionWorld->line(center,front,re4::SatSubset::All,0))front=hit->point;
        const auto target=roomPickups->target(leon->feet(),yaw,front);
        if (!target || *target != index || leon->jumpOffer(*walkProbe)) continue;
        if (roomInteractions && (roomInteractions->windowOffer(*leon,*walkProbe) || roomInteractions->doorOffer(leon->feet(),yaw))) continue;
        if (roomLadders && roomLadders->offer(*leon,*walkProbe,front)) continue;
        if (roomFlow && roomFlow->select(leon->feet(),yaw,false,false,front).door) continue;
        controls(jni); frozenPixels(jni); return true;
      }
  return false;
}
void answer(Jni &jni,std::uint64_t token,bool take) {
  Java_com_ast_re4decomp_NativeBridge_answerPickup(&jni.env,nullptr,jlong(token),take?JNI_TRUE:JNI_FALSE);
  controls(jni); frozenPixels(jni);
}
}
int main(int argc,char **argv) {
  using namespace m32_inventory;
  try {
    if (argc != 12) throw std::runtime_error("Usage: m32_inventory_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00 WEP02 WEP07 R100 OUTPUT_PREFIX");
    Egl egl; Jni jni; jlong session=320132;
    Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[5]));
    Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[6]),string(argv[7]));
    Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,string(argv[8]),string(argv[9]));
    Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
    Java_com_ast_re4decomp_NativeBridge_configureTestProfile(&jni.env,nullptr,1,session);
    const auto create=[&] {
      Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(argv[1]),string(argv[2]),string(argv[3]),string(argv[4]),string("r101"),session);
      frozenPixels(jni);
      require(leon&&roomPickups&&roomInteractions&&walking&&actorGpuReady&&!encounter,"Original village test loads the full native player and no enemy AI");
    };
    create(); unpause(jni);
    require(leon->equippedWeaponId()==0x23&&inventoryCase.equippedId()==0x23&&leon->magazine()==10&&leon->reserve()==20,
            "Actual app starts with original WEP02 and source 10+20 ammo in its case");
    require(inventoryCase.count(5)==1&&inventoryCase.count(0x7c)==1&&leon->unlimitedHealth(),"Source spray/key-item loadout and unlimited HP remain authoritative");
    const auto gun=loose(0x2c);
    const auto &record=roomPickups->items().at(gun);
    require(record.trigger.no==138,"Village shotgun uses original ITA area 138, not an invented drop");
    require(approach(jni,gun),"Original shotgun has a valid real SAT/controller pickup approach");
    const auto flags=localTaken();const auto globals=scenarioState.globalItems.words();
    const auto before=inventoryCase.snapshotJson(scenarioState.pesetas);
    const auto pose=actionSnapshot();
    controls(jni,re4::Bottom);frozenPixels(jni);controls(jni);
    const auto snapshot=text(Java_com_ast_re4decomp_NativeBridge_pendingPickup(&jni.env,nullptr));
    const auto token=number(snapshot,"token");
    require(token&&number(snapshot,"id")==0x2c&&number(snapshot,"count")==1&&pendingPickup.token==token,
            "Physical A/Kreuz publishes a source shotgun confirmation, not immediate ownership");
    require(paused&&leon->itemExamActive()&&roomPickups->visible(int(gun))&&inventoryCase.count(0x2c)==0,
            "Item examination freezes the source pose while the original item remains in the room");
    const auto frozen=actionSnapshot();const auto image=frozenPixels(jni);
    ticks(jni,4);
    require(actionSnapshot()==frozen&&localTaken()==flags&&scenarioState.globalItems.words()==globals,
            "Paused confirmation cannot advance pose, inventory flags or game clocks");
    auto *owner=leon.get();auto *pickups=roomPickups.get();
    egl.fresh();create();
    require(leon.get()==owner&&roomPickups.get()==pickups&&pendingPickup.token==token&&paused&&actionSnapshot()==frozen,
            "Actual EGL recreation preserves pending pickup token, native owners and exact frozen pose");
    require(pixelChanges(image,frozenPixels(jni))==0,"Pending pickup has exact framebuffer parity after EGL recreation");
    answer(jni,token,false);
    require(!paused&&!leon->itemExamActive()&&!pendingPickup.token&&text(Java_com_ast_re4decomp_NativeBridge_pendingPickup(&jni.env,nullptr)).empty(),
            "Cancel releases the source item examination and clears its published token");
    require(inventoryCase.snapshotJson(scenarioState.pesetas)==before&&localTaken()==flags&&scenarioState.globalItems.words()==globals&&roomPickups->visible(int(gun)),
            "Cancel leaves the case and source taken/found flags unchanged");
    require(leon->feet()==pose.feet&&leon->heading()==pose.heading&&leon->life()==1200&&leon->unlimitedHealth(),
            "Cancel restores ordinary control without moving Leon or weakening unlimited HP");
    controls(jni);ticks(jni,2);require(approach(jni,gun),"Canceled source pickup stays available");
    controls(jni,re4::Bottom);frozenPixels(jni);controls(jni);
    const auto acceptedToken=pendingPickup.token;require(acceptedToken>token,"Second pickup attempt has a fresh transaction token");
    answer(jni,acceptedToken,true);
    require(inventoryCase.count(0x2c)==1&&inventoryCase.item(ownedSlot(0x2c))->loaded==6&&!roomPickups->visible(int(gun)),
            "Accepted source pickup packs exactly one original shotgun with six loaded shells");
    require(scenarioState.carriedItems.empty()&&objectActions->inventory().empty(),"Case is the only inventory owner after original ITA transfer");
    answer(jni,acceptedToken,true);
    require(inventoryCase.count(0x2c)==1,"A stale/double modal answer cannot duplicate the shotgun");
    std::cerr << "M32 inventory phase: source A pickup / cancel / acceptance / exact EGL parity PASS\n";
    controls(jni);ticks(jni,2);controls(jni,0,1);ticks(jni,40);
    require(leon->aiming(),"Original pistol can be raised before opening equipment menu");
    pause(jni);
    const auto feet=leon->feet();const auto heading=leon->heading();
    Java_com_ast_re4decomp_NativeBridge_caseAction(&jni.env,nullptr,1,jint(ownedSlot(0x2c)),0,0,0);
    require(leon->equippedWeaponId()==0x2c&&inventoryCase.equippedId()==0x2c&&leon->magazine()==6,
            "Actual paused case equip holsters aimed pistol and equips the owned source shotgun");
    require(leon->feet()==feet&&leon->heading()==heading&&weaponGpuDirty,"Equipment swap preserves placement and defers GPU work to a context-owning draw");
    egl.release();
    Java_com_ast_re4decomp_NativeBridge_caseAction(&jni.env,nullptr,4,jint(ownedSlot(0x2c)),0,4,0);
    require(glGetString(GL_VERSION)==nullptr,"Paused inventory CPU operation requires no EGL context");
    const auto caseAfter=text(Java_com_ast_re4decomp_NativeBridge_caseSnapshot(&jni.env,nullptr));
    require(caseAfter.find("\"equipped\":44")!=std::string::npos,"Published case JSON exposes the selected source shotgun ID");
    egl.current();frozenPixels(jni);require(!weaponGpuDirty&&glGetError()==GL_NO_ERROR,"Next real draw uploads equipped source weapon once");
    save(std::string(argv[11])+"-equipped-shotgun.ppm");
    std::cerr << "M32 inventory phase: paused equip / CPU operation without EGL / weapon GPU upload PASS\n";
    require(inventoryCase.add(0x18,15),"Reserve fixture obeys source 15-shell stack capacity");applyCaseAmmo();publishCase();
    unpause(jni);controls(jni);ticks(jni,2);controls(jni,0,1);ticks(jni,45);
    const auto shots=leon->shots();controls(jni,0,1,1);
    for(unsigned n=0;n<120&&leon->shots()==shots;++n)tick(jni);
    controls(jni,0,1);ticks(jni,45);
    require(leon->shots()==shots+1&&leon->magazine()==5&&inventoryCase.item(ownedSlot(0x2c))->loaded==5&&inventoryCase.count(0x18)==15,
            "Original shotgun firing removes one loaded shell and preserves packed reserve");
    controls(jni,re4::Left,1);tick(jni);controls(jni,0,1);
    for(unsigned n=0;n<180&&leon->magazine()<6;++n)tick(jni);
    require(leon->magazine()==6&&leon->reserve()==14&&inventoryCase.item(ownedSlot(0x2c))->loaded==6&&inventoryCase.count(0x18)==14,
            "Actual original reload transfers one shell from the case into the magazine without duplication");
    std::cerr << "M32 inventory phase: actual shotgun firing / case reserve reload PASS\n";
    controls(jni);ticks(jni,80);
    const auto doc=loose(0xaf);const auto docRecord=roomPickups->items().at(doc);
    // Reuse the authored ITA bytes in an explicit compatibility fixture whose
    // optional ITM entry is absent. The original disc file is never changed.
    const auto archive = re4::RoomArchive::Load(argv[1]);
    const auto *ita = archive.find("ITA");
    require(ita != nullptr,"Original village archive contains the authored item triggers");
    const auto itaBytes = archive.copy(std::size_t(ita - archive.entries().data()));
    std::vector<std::uint8_t> noModelBytes(32 + itaBytes.size());
    noModelBytes[3] = 1; noModelBytes[19] = 32;
    noModelBytes[20] = 'I'; noModelBytes[21] = 'T'; noModelBytes[22] = 'A';
    std::copy(itaBytes.begin(),itaBytes.end(),noModelBytes.begin()+32);
    auto noModel = std::make_unique<re4::RoomPickups>(0x101,re4::RoomArchive(std::move(noModelBytes)),scenarioState,effectWorld.get());
    require(noModel->items().size()==roomPickups->items().size()&&!noModel->items().at(doc).hasModel,
            "Original ITA still parses when its optional model archive is absent");
    roomPickups = std::move(noModel);
    beginPickup(true,doc,docRecord.id,docRecord.count);const auto docToken=pendingPickup.token;
    require(docToken&&leon->itemExamActive(),"Original non-model document can enter source examination");
    answer(jni,docToken,true);
    require(inventoryCase.count(0xaf)==1&&!inventoryCase.item(ownedSlot(0xaf))->inCase()&&scenarioState.carriedItems.empty(),
            "Source document commits outside the grid without requiring a render model or a second owner");
    Java_com_ast_re4decomp_NativeBridge_caseAction(&jni.env,nullptr,2,jint(ownedSlot(5)),0,0,0);
    require(inventoryCase.count(5)==1&&leon->life()==1200,"Unlimited HP refuses wasteful spray use through real case JNI");
    std::cerr << "M32 inventory phase: document without model / unlimited HP healing refusal PASS\n";
    requestRoom(0x100,0,{-99685,-484,-1343},2.246f);finish(jni,"r100",argv[10]);
    require(inventoryCase.count(0x2c)==1&&inventoryCase.count(0xaf)==1&&leon->equippedWeaponId()==0x2c,
            "Real room transfer preserves case ownership, selected gun and picked-up document");
    requestRoom(0x101,0,{-121,-87.3,20449},3.1415927f);finish(jni,"r101",argv[1]);
    require(inventoryCase.count(0x2c)==1&&!roomPickups->visible(int(loose(0x2c)))&&inventoryCase.count(0xaf)==1,
            "Returning village reconstructs source taken flags without replenishing acquired items");
    std::cout<<"M32 original inventory actual JNI/GLES PASS: "<<checks<<'\n';return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
