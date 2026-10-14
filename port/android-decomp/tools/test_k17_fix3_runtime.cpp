// Actual production inventory JNI, original flight actor, room publication and
// complete bell title lifecycle. GLES uses the Linux software renderer.
#define RE4_K16_ENTRY previous_k16_fix3_main
#include "test_k16_runtime.cpp"
#undef RE4_K16_ENTRY
namespace {
std::vector<std::unique_ptr<std::vector<jint>>> intArrays;
jintArray JNICALL newIntArray(JNIEnv *,jsize n){intArrays.push_back(std::make_unique<std::vector<jint>>(n));return reinterpret_cast<jintArray>(intArrays.back().get());}
void JNICALL setIntArray(JNIEnv *,jintArray a,jsize start,jsize n,const jint *data){auto &v=*reinterpret_cast<std::vector<jint>*>(a);bridge_test::require(start>=0 && n>=0 && std::size_t(start+n)<=v.size(),"Bounded JNI int publication");std::copy_n(data,n,v.begin()+start);}
std::vector<jint> ints(jintArray a){return *reinterpret_cast<std::vector<jint>*>(a);}
unsigned itemSlot(unsigned id){for(const auto &i:inventoryCase.items())if(i.active && i.id==id)return i.slot;throw std::runtime_error("Missing production case item");}
void ppm(const std::filesystem::path &path,const std::vector<std::uint8_t> &pixels){std::ofstream out(path,std::ios::binary);out<<"P6\n960 540\n255\n";for(int y=539;y>=0;--y)for(unsigned x=0;x<960;++x)out.write(reinterpret_cast<const char*>(pixels.data()+(y*960+x)*4),3);}
}
int main(int argc,char **argv){using namespace save_test;try {
  if(argc<2 || argc>3)throw std::runtime_error("Usage: k17_fix3_runtime_check FIXTURE_ROOT [CAPTURE_DIR]");
  const auto root=std::filesystem::path(argv[1]),files=root/"files";const auto js=[&](const std::filesystem::path &p){return string(p.string().c_str());};
  Egl egl;SaveJni jni;jni.table.NewIntArray=newIntArray;jni.table.SetIntArrayRegion=setIntArray;jlong session=17503;
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
  };
  create(false);
  require(leon && !activeEvent,"Weapon test profile enters production gameplay");
  require(text(Java_com_ast_re4decomp_NativeBridge_roomId(&jni.env,nullptr))=="r101","HUD publishes actual room ID");
  Java_com_ast_re4decomp_NativeBridge_configureSubscreen(&jni.env,nullptr,js(files),1);
  const auto title=text(Java_com_ast_re4decomp_NativeBridge_subscreenLayout(&jni.env,nullptr,string("village_title")));
  require(title.find("\"texture\":151")!=std::string::npos && villageTitleArtwork,"Original bell layout crosses production JNI");
  for(unsigned id:{128u,149u,151u}) {
    const auto pixels=ints(Java_com_ast_re4decomp_NativeBridge_subscreenTexture(&jni.env,nullptr,string("village_title"),jint(id),0));
    require(pixels.size()>2 && pixels.size()==std::size_t(pixels[0]*pixels[1]+2),"Bell title texture reaches Java as bounded ARGB image");
  }
  Java_com_ast_re4decomp_NativeBridge_setCheatFlags(&jni.env,nullptr,3);
  for(const auto pair:std::vector<std::array<unsigned,3>>{{0x36,0xaa,0xab},{0x2e,0x44,0x6b},{0x2f,0x45,0x6c},{0x2e,0xc5,0x99},{0x2f,0xc5,0x51},{0x30,0x43,0x32},{0x21,0x3f,0x22}}) {
    inventoryCase=re4::InventoryCase(3);require(inventoryCase.add(0x7f,1),"XL case");
    Java_com_ast_re4decomp_NativeBridge_giveTestWeapon(&jni.env,nullptr,pair[0]);
    Java_com_ast_re4decomp_NativeBridge_giveTestWeapon(&jni.env,nullptr,pair[1]);require(inventoryCase.count(pair[1])==1,"SELECT grants original accessory as separate case part through production JNI");
    const auto w=itemSlot(pair[0]),p=itemSlot(pair[1]);const auto ammo=leon->magazine();
    Java_com_ast_re4decomp_NativeBridge_caseAction(&jni.env,nullptr,5,w,p,0,0);
    require(leon->equippedWeaponId()==pair[2] && inventoryCase.equippedId()==pair[2] && leon->magazine()==ammo,"Actual Combine equips accessory variant and retains ammo");
    const auto snapshot=captureNativeGame();const auto restored=re4::DecodeNativeSave(re4::EncodeNativeSave(snapshot,originalSaveBasis?&*originalSaveBasis:nullptr));
    re4::InventoryCase loaded;loaded.restoreSnapshot(restored.caseSize,restored.inventory,restored.equippedSlot);
    require(loaded.equippedId()==pair[2],"Production native SAVE retains effective scope/stock/silencer");
    controls(jni,0,1);for(unsigned n=0;n<120 && leon->action()!=re4::PlayerAction::Aim;++n)ticks(jni,1);
    require(leon->action()==re4::PlayerAction::Aim,"Accessory weapon draws through actual controller input");
    if(pair[0]==0x36 || pair[0]==0x2e || pair[0]==0x2f)require(ints(Java_com_ast_re4decomp_NativeBridge_scopeState(&jni.env,nullptr))[0]==jint(pair[2]),"Purchased scope selects its original fullscreen artwork");
    controls(jni);for(unsigned n=0;n<220 && leon->action()!=re4::PlayerAction::Footwork;++n)ticks(jni,1);
    Java_com_ast_re4decomp_NativeBridge_caseAction(&jni.env,nullptr,6,p,0,0,0);
    require(leon->equippedWeaponId()==pair[0] && inventoryCase.item(p)->active && inventoryCase.item(p)->attachedTo==~0u,"Production Remove restores base weapon and keeps part");
    controls(jni);frozenPixels(jni);
    std::cout<<"Production accessory "<<pair[0]<<"/"<<pair[1]<<" -> "<<pair[2]<<" PASS\n"<<std::flush;
  }
  // Keep the unchanged road/renderer/camera and vary the real normal-Mine
  // heading. Compare with/without the actual finite-flight model, never a
  // synthetic marker sprite. The release direction must survive recoil.
  inventoryCase=re4::InventoryCase(3);require(inventoryCase.add(0x7f,1),"Mine XL case");
  Java_com_ast_re4decomp_NativeBridge_giveTestWeapon(&jni.env,nullptr,0x36);
  unsigned flightCases{};
  for(float heading:{.6f,1.1f,1.72f,2.3f}) {
    require(walkProbe->placeAuthored({-38027,141,11114}),"Road placement");leon->setPlacement(walkProbe->position(),heading);re4::SceneFollowHeading(heading);re4::SceneSeedFollowView(0,0);
    if(playerProjectiles)playerProjectiles->clear();
    controls(jni,0,1);for(unsigned n=0;n<120 && leon->action()!=re4::PlayerAction::Aim;++n)ticks(jni,1);
    const auto aimed=leon->shotRay(false);const auto serial=leon->shots();controls(jni,0,1,1);ticks(jni,1);controls(jni,0,1);
    require(leon->shots()==serial+1 && playerProjectiles->active(),"Mine trigger launches one real dart");
    const auto fired=leon->shotRay(false);
    double aimLength{},shotLength{},dot{};for(unsigned k=0;k<3;++k){const double a=aimed.to[k]-aimed.from[k],b=fired.to[k]-fired.from[k];aimLength+=a*a;shotLength+=b*b;dot+=a*b;}
    require(dot/std::sqrt(aimLength*shotLength)>.99985,"Production mine follows the next authored ready pose before recoil");
    unsigned visible{},sample{};
    for(unsigned n=0;n<5 && playerProjectiles->active();++n) {
      auto flights=std::move(playerProjectiles);const auto absent=frozenPixels(jni);playerProjectiles=std::move(flights);
      const auto present=frozenPixels(jni);const auto changed=pixelChanges(absent,present);visible=std::max(visible,changed);if(changed>3)++sample;
      if(argc==3 && heading==1.72f && n==1){std::filesystem::create_directories(argv[2]);ppm(std::filesystem::path(argv[2])/"mine-flight.ppm",present);}
      ticks(jni,1);
    }
    std::cout<<"Mine heading "<<heading<<" RGB pixels "<<visible<<" visible frames "<<sample<<'\n'<<std::flush;
    require(visible>3 && sample>=1,"Mine contributes actual visible scene pixels");++flightCases;
    controls(jni);for(unsigned n=0;n<220 && leon->action()!=re4::PlayerAction::Footwork;++n)ticks(jni,1);
  }
  require(flightCases==4,"Four real normal-mine directions checked");
  ++session;create(true);
  for(unsigned n=0;n<180 && r100EventPhase==12;++n)advanceR100RoomScript(1./30,false);
  require(r100EventPhase==13 && r100RadioTerm,"Campaign first-entry radio reaches terminal");
  finishR100Radio();frozenPixels(jni);
  require(leon && enemyGroup && villageScenario && !activeEvent,"Source village enters production gameplay");
  beginVillageEvent(re4::VillageEvent::Bell);require(activeEvent && activeEvent->sourceFrame()<1100,"Bell starts before title");
  unsigned titleFrames{},eventFrames{};
  while(activeEvent && eventFrames++<1500) {
    const auto frame=activeEvent->sourceFrame();
    if((frame>=1098 && frame<=1102) || frame==1150 || frame==1250 || frame==1348) {
      frozenPixels(jni);const auto state=ints(Java_com_ast_re4decomp_NativeBridge_eventTitleState(&jni.env,nullptr));
      require(state[0]==(frame>=1100?1:0),"Bell overlay appears exactly at original frame1100");
      require(state.size()==3 && state[2]==1,"Bell owns a transparent title view before frame1100 to avoid HUD poll delay");
      if(frame>=1100){require(state[1]==jint(frame-1100),"UWF follows original event clock");++titleFrames;}
    }
    advanceR100Event(1./30);
  }
  frozenPixels(jni);require(!activeEvent && titleFrames>=5,"Full bell and source title finish");
  require(ints(Java_com_ast_re4decomp_NativeBridge_eventTitleState(&jni.env,nullptr))[0]==0,"Title clears on normal end and following radio");
  require(ints(Java_com_ast_re4decomp_NativeBridge_eventTitleState(&jni.env,nullptr))[2]==0,"Normal event end releases title view ownership");
  for(unsigned n=0;n<180 && r100EventPhase==12;++n)ticks(jni,1);
  require(r100EventPhase==13 && r100RadioTerm,"Following radio reaches original terminal");
  finishR100Radio();frozenPixels(jni);require(!eventUiActive.load() && !leon->roomScriptActive() && !paused,"Bell title/radio restore gameplay");
  beginVillageEvent(re4::VillageEvent::Bell);for(unsigned n=0;n<1110 && activeEvent;++n)advanceR100Event(1./30);
  frozenPixels(jni);require(eventTitleValues[0]==1,"Second bell activates title");finishVillageEvent();frozenPixels(jni);
  require(ints(Java_com_ast_re4decomp_NativeBridge_eventTitleState(&jni.env,nullptr))[0]==0,"Event skip clears title owner immediately");
  require(ints(Java_com_ast_re4decomp_NativeBridge_eventTitleState(&jni.env,nullptr))[2]==0,"Skipped event releases title view ownership");
  require(glGetError()==GL_NO_ERROR && javaException.empty(),"New paths leave JNI/GLES healthy");
  std::cout<<"K17 Fix3 production JNI attachments/SAVE,scope selection,mine flight,Room HUD and bell title/radio/skip PASS: "<<checks<<" checks\n";
  return 0;
 }catch(const std::exception &e){std::cerr<<"K17 Fix3 runtime FAILED: "<<e.what()<<'\n';return 1;}}
