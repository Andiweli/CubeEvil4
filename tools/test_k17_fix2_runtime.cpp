// Real JNI inputs, original disc resources and GLES framebuffer regressions.
#define RE4_K16_ENTRY previous_k16_fix2_main
#include "test_k16_runtime.cpp"
#undef RE4_K16_ENTRY
namespace {
void ppm(const std::filesystem::path &path,const std::vector<std::uint8_t> &pixels) {
  std::ofstream out(path,std::ios::binary);out<<"P6\n960 540\n255\n";
  for(int y=539;y>=0;--y)for(unsigned x=0;x<960;++x)out.write(reinterpret_cast<const char *>(pixels.data()+(y*960+x)*4),3);
}
}
int main(int argc,char **argv){using namespace save_test;try {
  if(argc!=2 && argc!=3)throw std::runtime_error("Usage: k17_fix2_runtime_check FIXTURE_ROOT [CAPTURE_DIR]");
  const auto root=std::filesystem::path(argv[1]),files=root/"files";
  const auto js=[&](const std::filesystem::path &p){return string(p.string().c_str());};
  Egl egl;SaveJni jni;jlong session=17502;
  Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,js(files/"etc/core.das"));
  Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,js(files/"em/em12.drs"),js(files/"etc/emleon00.esl"));
  Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,js(files/"em/wep02.drs"),js(files/"em/wep07.drs"));
  const auto create=[&](bool campaign) {
    Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
    Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,campaign?JNI_TRUE:JNI_FALSE,0,session);
    Java_com_ast_re4decomp_NativeBridge_configureTestProfile(&jni.env,nullptr,campaign?0:1,session);
    Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(&jni.env,nullptr,JNI_TRUE);
    Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,js(root/"decoded/r101.dat"),js(files/"em/pl00.drs"),js(files/"em/wep01.drs"),js(files/"Debug/roominfo.dat"),string("r101"),session);
    frozenPixels(jni);unpause(jni);controls(jni);
    require(javaException.empty() && actorGpuReady && leon,"Production village starts");
  };
  create(false);
  Java_com_ast_re4decomp_NativeBridge_configureSubscreen(&jni.env,nullptr,js(files),1);
  for(unsigned id:{0x2eu,0x2fu,0x6bu,0x6cu,0x99u,0x51u,0x35u,0x17u,0x6du,0xabu}) {
    const auto name="scope_"+std::to_string(id);
    const auto layout=text(Java_com_ast_re4decomp_NativeBridge_subscreenLayout(&jni.env,nullptr,string(name.c_str())));
    require(layout.find("\"rotationX\"")!=std::string::npos && scopeArtwork.count(name) && !menuArt(name).textures.empty(),"All ten scope layouts and original texture registrations cross JNI");
  }
  Java_com_ast_re4decomp_NativeBridge_setCheatFlags(&jni.env,nullptr,3);
  for(unsigned id:{0x36u,0xabu,0x35u,0x17u,0x6du,0x52u,1u,2u,0xeu}) {
    inventoryCase=re4::InventoryCase(3);require(inventoryCase.add(0x7f,1),"Original XL case owner");
    if(id==0xab) {
      require(leon->equipWeapon(id) && leon->setWeaponAmmo(id,5,20),"Scoped mine source profile");weaponGpuDirty=true;
    } else Java_com_ast_re4decomp_NativeBridge_giveTestWeapon(&jni.env,nullptr,jint(id));
    require(leon->equippedWeaponId()==id,"Projectile weapon equips through production controller");
    if(playerProjectiles)playerProjectiles->clear();
    frozenPixels(jni);
    require(walkProbe->placeAuthored({-38027,141,11114}),"Original village road placement");
    leon->setPlacement(walkProbe->position(),1.72f);re4::SceneFollowHeading(leon->heading());re4::SceneSeedFollowView(0,0);
    controls(jni,0,1);for(unsigned n=0;n<120 && leon->action()!=re4::PlayerAction::Aim;++n)ticks(jni,1);
    require(leon->action()==re4::PlayerAction::Aim,"Physical LT completes original projectile draw");
    const auto before=leon->shots();controls(jni,0,1,1);
    for(unsigned n=0;n<20 && leon->shots()==before;++n)ticks(jni,1);
    controls(jni,0,1);require(leon->shots()==before+1 && playerProjectiles->active(),"One real trigger emits a finite-flight actor");
    unsigned visible{};
    for(unsigned n=0;n<5 && playerProjectiles->active();++n) {
      const auto draws=playerProjectiles->draws();require(!draws.empty() && draws.front().rig,"Flight submits its actual motion pose");
      auto stored=std::move(playerProjectiles);const auto absent=frozenPixels(jni);playerProjectiles=std::move(stored);
      const auto present=frozenPixels(jni);visible=std::max(visible,pixelChanges(absent,present));
      if(argc==3 && id==0x36 && visible>3){std::filesystem::create_directories(argv[2]);ppm(std::filesystem::path(argv[2])/"mine-flight.ppm",present);ppm(std::filesystem::path(argv[2])/"mine-without-flight.ppm",absent);}
      if(visible>3)break;
      ticks(jni,1);
    }
    std::cout<<"Projectile "<<id<<" visible changed RGB pixels "<<visible<<'\n'<<std::flush;
    require(visible>3,("Real projectile contributes visible scene pixels: "+std::to_string(id)).c_str());
    require(glGetError()==GL_NO_ERROR,"Projectile GL owner survives actual weapon uploads");
    controls(jni);for(unsigned n=0;n<300 && leon->action()!=re4::PlayerAction::Footwork;++n)ticks(jni,1);
    playerProjectiles->clear();
  }
  require(roomLadders && roomLadders->objectLadders().size()==3,"Source post-house ladder prerequisites");
  const auto placements=roomLadders->objectLadders();
  for(const auto &placement:placements) {
    bool found=false;
    for(float z:{600.f,800.f,1000.f}) {
      if(!walkProbe->place(point(placement,0,0,z)))continue;
      leon->setPlacement(walkProbe->position(),placement.rotation[1]+3.1415927f);
      const auto offer=roomLadders->offer(*leon,*walkProbe);
      if(offer && offer->object && offer->no==placement.slot){found=true;break;}
    }
    require(found,"Each original house ladder has a reachable approach");
    re4::SceneFollowHeading(leon->heading());re4::SceneSeedFollowView(0,0);controls(jni);frozenPixels(jni);
    controls(jni,re4::Bottom);ticks(jni,1);controls(jni);
    require(leon->objectLadderActive(),"Physical A starts the original climb");
    unsigned climbing{},visibleSamples{};
    while(leon->ladderActive() && climbing++<400) {
      calls.clear();ticks(jni,1);
      if(climbing%20==0 && leon->ladderActive()) {
        bool drawn=false;for(const auto &draw:objectDraws())if(draw.roomObjectSlot==int(placement.slot) && submitted(draw))drawn=true;
        if(drawn)++visibleSamples;
        if(argc==3 && placement.slot==5 && climbing==40)ppm(std::filesystem::path(argv[2])/"ladder-climb.ppm",frozenPixels(jni));
      }
    }
    require(visibleSamples>=2,"Raised ladder remains submitted at several actual climb heights");
    require(!leon->ladderActive() && !leon->ladderLandingFailed() && leon->feet()[1]>2000,"Climb finishes on original roof");
  }
  ++session;create(true);
  for(unsigned n=0;n<180 && r100EventPhase==12;++n)advanceR100RoomScript(1./30,false);
  require(r100EventPhase==13 && r100RadioTerm,"Real campaign radio terminal");finishR100Radio();frozenPixels(jni);
  require(!paused && !eventUiActive.load() && !leon->roomScriptActive(),"Radio closes without K17 freeze regression");
  // Actual death/drop path must retain the uploaded room scene for ordinary glow drops.
  auto *victim=enemyGroup->find(0x14);require(victim && victim->alive(),"Original initial village enemy");
  bool dropSeed=false;
  for(unsigned seed=1;seed<1000 && !dropSeed;++seed) {
    auto state=enemyDropState;state.randomState=std::uint16_t(seed);
    const auto authored=re4::FindAuthoredEnemyDrop(*pickupArchive,victim->listIndex(),scenarioState,activeRoomId);
    auto choice=authored?re4::ResolveAuthoredEnemyDrop(*authored,currentEnemyDropContext,state):re4::OriginalEnemyRandomDrop(0x15,currentEnemyDropContext,state);
    if(choice && re4::OriginalItemEffectType(choice->id)!=8){enemyDropState.randomState=std::uint16_t(seed);dropSeed=true;}
  }
  require(dropSeed,"Original deterministic RNG fixture selects an ordinary glow drop");
  const auto originalDrops=roomPickups->items().size();auto p=victim->feet();p[1]+=800;
  require(victim->applyBlast({0,0,90001,18,p,8000,1},*leon,*walkProbe),"Real source rocket blast kills the enemy");
  campaignSceneDirty=false;
  for(unsigned n=0;n<360 && !r100DroppedIndices.count(victim->listIndex());++n){victim->tick(leon->feet());afterWorldTick(nullptr);}
  require(r100DroppedIndices.count(victim->listIndex()),"Actual death completion resolves its drop exactly once");
  require(roomPickups->items().size()>originalDrops,"Original drop remains available for pickup");
  for(std::size_t i=originalDrops;i<roomPickups->items().size();++i)require(!roomPickups->visible(int(i)),"Ordinary drop owns a glow rather than an ITM body");
  require(!campaignSceneDirty,"Ordinary enemy drop does not rebuild/upload the entire room");
  frozenPixels(jni);require(!campaignSceneDirty && glGetError()==GL_NO_ERROR,"Drop returns to a healthy retained renderer");
  beginVillageEvent(re4::VillageEvent::Bell);require(activeEvent && activeEvent->data().name()=="r101s30","Original bell event starts");
  unsigned rendered{},frames{};int previousCut=-1;
  while(activeEvent && frames++<1500) {
    advanceR100Event(1./30);
    if(!activeEvent)break;
    const int cut=int(activeEvent->sourceCut());
    if(cut!=previousCut) {
      previousCut=cut;frozenPixels(jni);++rendered;
      for(const auto &actor:activeEvent->actors())if(actor->name()=="scr0000" && actor->visible()) {
        const auto pose=actor->rig()->jointMatrices()[0];require(std::hypot(pose[3],pose[11])>10000,"Actual bell render retains external door world position across cuts");
      }
      require(glGetError()==GL_NO_ERROR,"Bell door/models/effects render after every camera cut");
    }
  }
  require(!activeEvent && rendered>=8,"Entire source bell event finishes after all real cuts");
  frozenPixels(jni);
  require(r100EventPhase==12 && r100RadioCall==2,"Bell queues its original following radio on the next live render tick");
  for(unsigned n=0;n<180 && r100EventPhase==12;++n)ticks(jni,1);
  require(r100EventPhase==13 && r100RadioTerm,"Following radio completes its original draw animation");
  ticks(jni,35);controls(jni,re4::Start);ticks(jni,1);controls(jni);ticks(jni,3);
  require(!eventUiActive.load() && !leon->roomScriptActive() && !paused,"Bell and following radio restore live control");
  std::cout<<"K17 Fix2 real JNI/GLES flight pixels,10 scopes,3 complete ladder climbs,drop retention and bell/radio PASS: "<<checks<<" checks\n";
}catch(const std::exception &e){std::cerr<<"K17 Fix2 runtime failure: "<<e.what()<<'\n';return 1;}}
