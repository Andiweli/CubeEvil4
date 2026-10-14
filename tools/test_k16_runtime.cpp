// Production r101 campaign, original EVD/GLES, checkpoints and SELECT cheats.
#define RE4_M37_SAVE_ENTRY previous_k15_save_main
#include "test_m37_save_runtime.cpp"
#undef RE4_M37_SAVE_ENTRY
#ifndef RE4_K16_ENTRY
#define RE4_K16_ENTRY main
#endif
int RE4_K16_ENTRY(int argc,char **argv){using namespace save_test;try {
 if(argc!=2 && argc!=3)throw std::runtime_error("Usage: k16_runtime_check FIXTURE_ROOT [rooms]");
 const bool roomsOnly=argc==3 && std::string(argv[2])=="rooms";
 const auto root=std::filesystem::path(argv[1]);const auto files=root/"files";
 const auto js=[&](const std::filesystem::path &p){return string(p.string().c_str());};
 Egl egl;SaveJni jni;jlong session=15001;
 Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,js(files/"etc/core.das"));
 Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,js(files/"em/em12.drs"),js(files/"etc/emleon00.esl"));
 Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,js(files/"em/wep02.drs"),js(files/"em/wep07.drs"));
 const auto create=[&](){
  Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
  Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,JNI_TRUE,0,session);
  Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(&jni.env,nullptr,JNI_TRUE);
  Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,js(root/"decoded/r101.dat"),js(files/"em/pl00.drs"),js(files/"em/wep01.drs"),js(files/"Debug/roominfo.dat"),string("r101"),session);
  if(!javaException.empty())throw std::runtime_error(javaException);
  require(leon && actorGpuReady && walking && villageScenario && enemyGroup,"Real r101 owns player/GLES/scenario and enemy group");
 };
 create();frozenPixels(jni);unpause(jni);controls(jni);ticks(jni,2);
 Java_com_ast_re4decomp_NativeBridge_configureSubscreen(&jni.env,nullptr,js(files),1);
 const auto binocularLayout=text(Java_com_ast_re4decomp_NativeBridge_subscreenLayout(&jni.env,nullptr,string("binocular")));
 require(binocularArtwork && binocularLayout.find("\"units\"")!=std::string::npos && !menuArt("binocular").textures.empty(),"Original binocular ID textures and all animation curves decode through production JNI");
 require(enemyGroup->actors().size()==43 && enemyGroup->villageAlive()==11,"All43 authentic actor owners, exactly11 initially active");
 require(r100EventPhase==12 && villageRadioIntro && eventUiActive.load(),"Source first-entry radio12 precedes combat");
 require(!leon->unlimitedHealth() && leon->life()==1200,"Campaign remains mortal with original1200HP");
 for(unsigned n=0;n<160 && r100EventPhase==12;++n)ticks(jni,1);
 require(r100RadioTerm && r100EventPhase==13,"Original player radio take-out leads to English radio terminal");
 finishR100Radio();frozenPixels(jni);controls(jni);
 require(!eventUiActive.load() && entryCheckpoint && entryCheckpoint->room==0x101,"Radio completion commits the r101 entry checkpoint");
 if(!roomsOnly) {
 const auto ordinary=inventoryCase;
 Java_com_ast_re4decomp_NativeBridge_setCheatFlags(&jni.env,nullptr,3);
 require(leon->unlimitedHealth() && leon->unlimitedAmmo(),"Both native session cheats enabled");
 constexpr unsigned ids[]={0x38,0x23,0x21,0x40,0x25,0x27,0x3,0x29,0x2a,0x37,0x2c,0x94,0x2d,0x2e,0x2f,0x30,0x3e,0x34,0x36,0x35,0x17,0x6d,0x52,0x1,0x2,0xe,0x3f,0x42,0x43,0x44,0x45,0xaa,0xc5};
 for(unsigned id:ids){
  inventoryCase=re4::InventoryCase(3);
  const auto message=text(Java_com_ast_re4decomp_NativeBridge_giveTestWeapon(&jni.env,nullptr,jint(id)));
  if(!inventoryCase.count(id))throw std::runtime_error("Cheat item failed: "+std::to_string(id)+": "+message);
  require(inventoryCase.count(id)==1,"Every original selectable weapon/accessory enters the real case");
  if(leon->canEquip(id)) {
    if(inventoryCase.equippedId()!=id || leon->equippedWeaponId()!=id)throw std::runtime_error("Cheat equip failed for item "+std::to_string(id)+" case="+std::to_string(inventoryCase.equippedId())+" player="+std::to_string(leon->equippedWeaponId()));
    frozenPixels(jni);require(glGetError()==GL_NO_ERROR,"All carry models and newly uploaded grips render");
    const auto portrait=re4::RenderSubscreenPortraitGl(re4::LoadOriginalMenuPortrait(files.string(),id),320,180);
    require(portrait.width==320 && portrait.height==180 && glGetError()==GL_NO_ERROR,"Original equipped weapon appears on the menu's posed player model");
    const auto chosen=inventoryCase.items().front().slot;
    require(inventoryCase.add(0x23,1),"Case also holds a handgun for the inventory round trip");
    unsigned handgun=~0u;for(const auto &item:inventoryCase.items())if(item.id==0x23)handgun=item.slot;
    Java_com_ast_re4decomp_NativeBridge_caseAction(&jni.env,nullptr,1,jint(handgun),0,0,0);
    Java_com_ast_re4decomp_NativeBridge_caseAction(&jni.env,nullptr,1,jint(chosen),0,0,0);
    require(leon->equippedWeaponId()==id && inventoryCase.equippedId()==id,"Ingame Equip switches back to each cheat weapon");
    frozenPixels(jni);require(glGetError()==GL_NO_ERROR,"Inventory weapon round trip preserves GLES owners");
  }
 }
 const auto before=inventoryCase.items();
 require(text(Java_com_ast_re4decomp_NativeBridge_giveTestWeapon(&jni.env,nullptr,0xffff))=="Unknown GameCube weapon." && inventoryCase.items().size()==before.size(),"Unknown IDs rejected without inventory mutation");
 inventoryCase=ordinary;leon->equipWeapon(0x23);applyCaseAmmo();weaponGpuDirty=true;frozenPixels(jni);
 const auto ownedHandguns=inventoryCase.count(0x23);
 require(text(Java_com_ast_re4decomp_NativeBridge_giveTestWeapon(&jni.env,nullptr,0x23))=="Weapon equipped." &&
     inventoryCase.count(0x23)==ownedHandguns && leon->equippedWeaponId()==0x23,
     "Selecting an owned combat weapon equips it without duplicating the item");
 inventoryCase=re4::InventoryCase(0);while(inventoryCase.add(6,1)){}
 const auto fullCase=inventoryCase.items();
 require(text(Java_com_ast_re4decomp_NativeBridge_giveTestWeapon(&jni.env,nullptr,0x37))=="There is not enough space in the case." &&
     inventoryCase.items().size()==fullCase.size() && !inventoryCase.count(0x37),
     "Full-case weapon grant fails without discarding inventory");
 inventoryCase=ordinary;require(leon->equipWeapon(0x23),"Restore the campaign handgun after cheats");applyCaseAmmo();
 // Real event ownership/rendering at its critical cuts; no room or player swap.
 auto *player=leon.get();auto *group=enemyGroup.get();
 for(auto event:{re4::VillageEvent::Binoculars,re4::VillageEvent::House}){
  if(event==re4::VillageEvent::Binoculars) {
    const re4::CollisionVec look{-38027,141,11114};
    require(villageScenario->binocularAvailable(scenarioState,look,1.72),"Original AEV7 offers LOOK at the authored binocular position and direction");
    require(!villageScenario->offer(scenarioState,look,1.72,false,true),"Holding A does not invent an automatic binocular event after the radio");
    require(villageScenario->offer(scenarioState,look,1.72,true,true)==event,"A press starts the original LOOK event");
    require(!villageScenario->binocularAvailable(scenarioState,look,-1.42),"LOOK respects the original facing restriction");
  }
  beginVillageEvent(event);unsigned iterations=0;
  while(activeEvent && iterations++<1500){advanceR100Event(1./30);if(iterations%40==0)frozenPixels(jni);}
  frozenPixels(jni);
  if(event==re4::VillageEvent::Binoculars) {
    require(villageBinocular && r100EventPhase==16 && eventUiActive.load() && leon->damageWait()==0x80,"Intro hands off to the interactive original binocular camera");
    const auto eye=villageBinocular->camera().eye;
    require(eye==std::array<float,3>{-38027,1891,11114},"Binocular eye matches the original English scene");
    re4::InputFrame zoom{};zoom.ry=1;zoom.held=re4::MenuLeft;
    for(unsigned n=0;n<16 && villageBinocular;++n)advanceR100RoomScript(1./30,false,false,false,&zoom);
    require(villageBinocular && villageBinocular->fov()==3 && villageBinocular->camera().target!=std::array<float,3>{-3573,2400,2113},"Live binocular zoom/pan reaches its original limits");
    frozenPixels(jni);finishVillageBinoculars();
    require(!villageBinocular && !eventUiActive.load() && leon->damageWait()==0,"B exit releases the protected player and camera");
    beginVillageEvent(event);require(!activeEvent && villageBinocular,"Repeated LOOK skips the already-seen intro and opens the camera");
    require(villageScenario->binocularAvailable(scenarioState,leon->feet(),leon->heading()),"RSF5 does not disable repeated LOOK");
    finishVillageBinoculars();
  }
  require(!activeEvent && !eventUiActive.load() && leon.get()==player && enemyGroup.get()==group,"Real village event completes while preserving live owners");
  require(glGetError()==GL_NO_ERROR,"Original village EVD models/effects render without GLES errors");
 }
 require(scenarioState.room(0x101).room.test(8) && scenarioState.room(0x101).room.test(6) && enemyGroup->villageAlive()==14,"House event enables original11 indoor actors plus3 alarm actors");
 // The source deadline is tested separately in the pure30Hz scenario test.
 for(unsigned n=0;n<10;++n)villageScenario->tick(scenarioState,leon->feet(),leon->heading(),14,true);
 villageScenario->tick(scenarioState,leon->feet(),leon->heading(),9,true);
 drainVillageCues();
 for(unsigned n=0;n<100;++n){villageScenario->tick(scenarioState,leon->feet(),leon->heading(),9,true);drainVillageCues();if(villageWaveCamera)break;}
 require(villageWaveCamera && villageWaveActors.size()==5,"Five real actors participate in original CAM4/5 wave cut");
 frozenPixels(jni);finishVillageWave();
 for(unsigned n=0;n<3;++n){villageScenario->tick(scenarioState,leon->feet(),leon->heading(),0,true);drainVillageCues();}
 require(scenarioState.room(0x101).room.test(7) && scenarioState.transient.test(3),"Original kill threshold enters delayed bell state");
 for(unsigned n=0;n<60;++n)villageScenario->tick(scenarioState,leon->feet(),leon->heading(),0,true);
 const auto offered=villageScenario->offer(scenarioState,leon->feet(),leon->heading(),false,false);
 require(offered==re4::VillageEvent::Bell,"Bell is offered after its original60-frame delay");
 beginVillageEvent(*offered);unsigned iterations=0;
 while(activeEvent && iterations++<1500){advanceR100Event(1./30);if(iterations%40==0)frozenPixels(jni);}
 frozenPixels(jni);
 require(!activeEvent && !scenarioState.transient.test(3) && scenarioState.transient.test(2) && enemyGroup->villageAlive()==0 && (r100RadioQueued==2 || r100RadioCall==2),"Bell removes combat actors, clears event lock and queues original radio2");
 require(glGetError()==GL_NO_ERROR,"Complete bell event and live return render without GLES errors");
 if(r100EventPhase>=12)finishR100Radio();
 // The authored tower projectile, trail, floor impact, fire and GLES owners.
 require(villageBottles && villageBottles->throws()==0,"Village bottle owner starts empty");
 villageBottles->throwAt(villageScenario->towerTarget(),*effectWorld);
 re4::WorldEffectCamera bottleCamera;const auto camera=re4::SceneFollowPose();
 for(unsigned k=0;k<3;++k){bottleCamera.eye[k]=float(camera.eye[k]);bottleCamera.up[k]=float(camera.up[k]);bottleCamera.right[k]=float(camera.right[k]);bottleCamera.forward[k]=float(camera.look[k]);}
 for(unsigned n=0;n<300;++n){villageBottles->tick(*leon,*effectWorld,bottleCamera);if(n%15==0)frozenPixels(jni);}
 require(villageBottles->throws()==1 && villageBottles->explosions()==1 && glGetError()==GL_NO_ERROR,"Authored bottle flight/impact/flames render without GLES error");
 Java_com_ast_re4decomp_NativeBridge_setCheatFlags(&jni.env,nullptr,0);
 require(!leon->unlimitedHealth() && !leon->unlimitedAmmo(),"Cheats can be turned off without a new scene");
 // Reproduce the submitted explosion/limp/death/Continue sequence.
 require(leon->restoreLife(1200),"Restore full life for the real blast regression");
 require(leon->applyBlast({true,1200},{leon->feet()[0]+100,leon->feet()[1]+500,leon->feet()[2]+100},*walkProbe),"First original blast accepted");
 require(leon->life()==1 && leon->alive(),"Original strong blast leaves one HP, rather than a delayed zero-HP death");
 for(unsigned n=0;n<500 && leon->blastActive();++n)leon->step(*walkProbe,0,0,false,false,1./30,{});
 require(leon->action()==re4::PlayerAction::Footwork,"Survivable blast recovery completes");
 const auto pl=re4::RoomArchive::LoadDrs((files/"em/pl00.drs").string());
 re4::SkinnedModel body(pl.copy(0));std::vector<re4::AnimationBytes> injured;
 for(unsigned entry:{46u,48u,52u,50u,56u,58u,54u})injured.push_back({pl.copy(entry),pl.copy(entry+1)});
 re4::AnimationRig expectedLimp(body,injured);
 const auto place=leon->feet();leon->setPlacement(place,0);expectedLimp.reset();
 for(unsigned n=0;n<10;++n){expectedLimp.tick(re4::Locomotion::Walk);leon->step(*walkProbe,0,1,false,false,1./30,{});}
 const auto &actualPose=leon->rig().jointMatrices(),referencePose=expectedLimp.jointMatrices();
 require(actualPose.size()==referencePose.size(),"Injured locomotion uses the complete player hierarchy");
 for(unsigned part=0;part<actualPose.size();++part)for(unsigned axis=0;axis<12;++axis)
   require(std::abs(actualPose[part][axis]-referencePose[part][axis])<.02f,"One-HP walking matches the actual original damaged FCV/SEQ pose");
 for(unsigned n=0;n<200 && leon->damageWait();++n)leon->step(*walkProbe,0,0,false,false,1./30,{});
 require(leon->applyBlast({true,1200},{leon->feet()[0]+100,leon->feet()[1]+500,leon->feet()[2]+100},*walkProbe),"Second original blast accepted");
 require(leon->life()==0 && !leon->alive() && leon->action()==re4::PlayerAction::BlastDead,"Fatal blast begins the original death motion on the same damage tick");
 ticks(jni,91);require(deathRequested.load() && entryCheckpoint,"Fatal blast reaches the native Continue screen");
 Java_com_ast_re4decomp_NativeBridge_continueConfirmed(&jni.env,nullptr);
 require(Java_com_ast_re4decomp_NativeBridge_continueGame(&jni.env,nullptr)==JNI_TRUE,"Explosion Continue restores the native checkpoint");
 const auto blastRetry=roomRequest;Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(&jni.env,nullptr,jlong(blastRetry.token),js(root/"decoded/r101.dat"),string(""));frozenPixels(jni);
 require(leon->alive() && leon->life()==1200 && activeRoomId==0x101 && actorGpuReady && !deathRequested.load(),"Explosion checkpoint rebuild returns directly to live r101");
 // Original EM15 paired chainsaw hit, fatal player motion and real Continue.
 leon->beginChainsawGrab(0,{});require(leon->life()==1 && leon->grabbed(),"Chainsaw catch leaves mortal Leon at source1HP");
 leon->endChainsawGrab(false);require(!leon->alive() && leon->action()==re4::PlayerAction::HurtDead,"Failed chainsaw escape starts the original fatal player motion");
 ticks(jni,91);require(deathRequested.load() && entryCheckpoint,"Village fatal reaction reaches the native Continue screen");
 Java_com_ast_re4decomp_NativeBridge_setCheatFlags(&jni.env,nullptr,2);
 Java_com_ast_re4decomp_NativeBridge_continueConfirmed(&jni.env,nullptr);
 require(Java_com_ast_re4decomp_NativeBridge_continueGame(&jni.env,nullptr)==JNI_TRUE,"Village Continue requests its real entry checkpoint");
 const auto request=roomRequest;Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(&jni.env,nullptr,jlong(request.token),js(root/"decoded/r101.dat"),string(""));frozenPixels(jni);
 require(!deathRequested.load() && !subscreenOpen && leon->alive() && leon->life()==1200 && enemyGroup->villageAlive()==11 && !scenarioState.room(0x101).room.test(7),"Village Continue restores player and original starting actors, without the title menu");
 require(leon->unlimitedAmmo() && !leon->unlimitedHealth() && actorGpuReady && glGetError()==GL_NO_ERROR,"Session ammo cheat survives Continue and real GPU replacement");
 Java_com_ast_re4decomp_NativeBridge_setCheatFlags(&jni.env,nullptr,0);
 // The submitted freeze happens when a real EM15 type11 reaches its Find
 // voice slot. Reproduce that motion with the production enemy/HUD owners.
 beginVillageEvent(re4::VillageEvent::Binoculars);
 if(activeEvent) {
   unsigned limit=0;while(activeEvent && ++limit<1500)advanceR100Event(1./30);
 }
 re4::EnemyEncounter *discoverer=nullptr;
 for(const auto &actor:enemyGroup->actors()) {
   if(!discoverer && actor->actor().type()==11 && !actor->scriptSuspended())discoverer=actor.get();
   actor->setScriptSuspended(true);
 }
 require(discoverer && villageBinocular,"A real initially active EM15 type11 is available for the Logcat regression");
 const auto enemyFeet=discoverer->feet();const float direction=discoverer->heading();
 const re4::CollisionVec discoveryPosition{enemyFeet[0]+8000*std::sin(direction),enemyFeet[1],enemyFeet[2]+8000*std::cos(direction)};
 walkProbe->setScriptPosition(discoveryPosition);leon->setPlacement(discoveryPosition,direction+3.14159265f);
 discoverer->setScriptSuspended(false);discoverer->setAwarenessInputs(false,false,true);
 advanceR100RoomScript(1./30,false);
 require(discoverer->foundPlayer() && discoverer->actor().motion()==re4::EnemyMotion::Find,"Original type11 discovery enters its authored Find motion");
 require(!villageBinocular && !eventUiActive.load() && leon->damageWait()==0 && scenarioState.room(0x101).room.test(6),"Enemy discovery ends binocular view and releases the player into the village alarm");
 for(unsigned n=0;n<150;++n) {
   enemyGroup->tickScriptActors(*leon,*walkProbe,{discoverer->listIndex()});
   drainEnemyFeedback(*discoverer,groupVoiceHandles[discoverer]);
 }
 frozenPixels(jni);
 require(discoverer->actor().motion()!=re4::EnemyMotion::Find && glGetError()==GL_NO_ERROR,"Type11 discovery voice and completed motion return to live GLES without an exception");
 require(!villageScenario->binocularAvailable(scenarioState,leon->feet(),leon->heading()),"The original alarm flag disables further LOOK");
 // Ordinary room transfers use the production decoder, scene/GPU upload and
 // source room starts. The lake's boat arrival is deliberately not a floor.
 }
 unsigned visited=0;
 for(unsigned room=0x102;room<=0x11f;++room)if(re4::IsVillageRoom(room)) {
  std::cout << "Loading original village room r" << std::hex << room << std::dec << std::endl;
  const auto *start=roomInfo->find(std::uint16_t(room),0);
  require(start && start->valid(),"Every connected village room has an original editor start");
  re4::CollisionVec destination=start->position;
  requestRoom(std::uint16_t(room),0,destination,float(start->heading));const auto transfer=roomRequest;
  char file[16];std::snprintf(file,sizeof file,"r%03x.dat",room);
  Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(&jni.env,nullptr,jlong(transfer.token),js(root/"decoded"/file),string(""));frozenPixels(jni);
  if(room==0x10d) {
   require(activeRoomId==0x10c && !transferPending() && walking && actorGpuReady,
       "Boat-only original arrival rejects a floorless transfer and retains the live room");
   continue;
  }
  if(activeRoomId!=room)throw std::runtime_error("Village room transfer failed: "+std::to_string(room)+": "+flowInfo);
  require(walking && actorGpuReady && leon->alive() && glGetError()==GL_NO_ERROR,"Connected original village room loads a playable native/GLES scene");++visited;
 }
 require(visited==25,"All25 remaining village rooms with static walking starts load through production JNI");
 std::cout<<"K16 original village campaign JNI/GLES and cheats passed: "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"K16 runtime failure: "<<e.what()<<'\n';return 1;}}
