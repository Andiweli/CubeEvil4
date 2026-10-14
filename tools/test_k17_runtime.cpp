// Production JNI regression plus real framebuffer proof of enemy ownership.
#define RE4_K16_ENTRY previous_k16_runtime_main
#include "test_k16_runtime.cpp"
#undef RE4_K16_ENTRY
int main(int argc,char **argv){using namespace save_test;try {
  if(argc!=2 && argc!=3)throw std::runtime_error("Usage: k17_runtime_check FIXTURE_DISC_ROOT [--weapons-only]");
  if(argc==2 && previous_k16_runtime_main(argc,argv))return 1;
  const auto root=std::filesystem::path(argv[1]),files=root/"files";
  const auto js=[&](const std::filesystem::path &p){return string(p.string().c_str());};
  Egl egl;SaveJni jni;jlong session=17001;
  // The previous campaign suite's EGL context is gone. Forget its handles
  // before any state capture can mistake a reused numeric GL name for history.
  re4::ForgetSceneGl();
  Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,js(files/"etc/core.das"));
  Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,js(files/"em/em12.drs"),js(files/"etc/emleon00.esl"));
  Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,js(files/"em/wep02.drs"),js(files/"em/wep07.drs"));
  Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
  Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,JNI_FALSE,0,session);
  Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,js(root/"decoded/r101.dat"),js(files/"em/pl00.drs"),js(files/"em/wep01.drs"),js(files/"Debug/roominfo.dat"),string("r101"),session);
  frozenPixels(jni);unpause(jni);controls(jni);ticks(jni,2);
  require(leon && actorGpuReady && !activeEvent,"Real JNI scene for weapon input regressions");
  Java_com_ast_re4decomp_NativeBridge_configureSubscreen(&jni.env,nullptr,js(files),1);
  for(unsigned id:{0x2eu,0x2fu,0x6bu,0x6cu,0x99u,0x51u}) {
    const auto name="scope_"+std::to_string(id);
    const auto layout=text(Java_com_ast_re4decomp_NativeBridge_subscreenLayout(&jni.env,nullptr,string(name.c_str())));
    require(scopeArtwork.count(name) && layout.find("\"units\"")!=std::string::npos && !menuArt(name).textures.empty(),"Every original rifle scope variant decodes through production JNI");
  }
  Java_com_ast_re4decomp_NativeBridge_setCheatFlags(&jni.env,nullptr,3);
  re4::EnemyPreview enemy((files/"em/em12.drs").string(),0x25,true);
  re4::CreateEnemyGl(enemy);
  const auto enemyPixels=[&] {
    glViewport(0,0,960,540);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const float matrix[16]={.001f,0,0,0,0,.001f,0,0,0,0,.0001f,0,0,-.85f,0,1};
    const re4::Matrix34 placement{1,0,0,0,0,1,0,0,0,0,1,0};
    re4::DrawEnemyGl(matrix,{0,0,0},1,enemy,placement,true);
    std::vector<std::uint8_t> pixels(960*540*4);glReadPixels(0,0,960,540,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    require(glGetError()==GL_NO_ERROR,"Isolated real enemy draw has no GLES errors");
    unsigned covered=0;for(unsigned i=0;i<pixels.size();i+=4)if(pixels[i] || pixels[i+1] || pixels[i+2])++covered;
    require(covered>600,"Original enemy contributes actual visible framebuffer pixels");return pixels;
  };
  const auto before=enemyPixels();
  constexpr unsigned ids[]={0x23,0x21,0x40,0x25,0x27,3,0x29,0x2a,0x37,0x2c,0x94,0x2d,0x2e,0x2f,0x30,0x3e,0x34,0x36,0x35,0x17,0x6d,0x52,1,2,0xe};
  for(unsigned id:ids) {
    inventoryCase=re4::InventoryCase(3);
    const auto message=text(Java_com_ast_re4decomp_NativeBridge_giveTestWeapon(&jni.env,nullptr,jint(id)));
    require(inventoryCase.count(id) && leon->equippedWeaponId()==id,(("Native Equip failed: "+std::to_string(id))+" / "+message).c_str());
    frozenPixels(jni);
    require(!weaponGpuDirty,"New actor grips, weapon and effect bank uploaded successfully");
    // No call to CreateEnemyGl here: a player/weapon upload must preserve
    // existing enemy GPU resources, including after lazy grip mesh growth.
    require(pixelChanges(before,enemyPixels())==0,"Weapon Equip preserves every actual enemy pixel");
    Java_com_ast_re4decomp_NativeBridge_setSubscreenOpen(&jni.env,nullptr,JNI_TRUE);
    controls(jni);frozenPixels(jni);
    Java_com_ast_re4decomp_NativeBridge_setSubscreenOpen(&jni.env,nullptr,JNI_FALSE);
    controls(jni,0,1);ticks(jni,1);
    require(leon->aiming(),("First fresh analog LT edge aims with item"+std::to_string(id)).c_str());
    for(unsigned n=0;n<100 && leon->action()!=re4::PlayerAction::Aim;++n)ticks(jni,1);
    require(leon->action()==re4::PlayerAction::Aim,"Held LT reaches each weapon's ready state");
    const auto previous=leon->shots();controls(jni,0,1,1);ticks(jni,12);
    require(leon->shots()>previous,("Real Fire input works for item"+std::to_string(id)).c_str());
    require(glGetError()==GL_NO_ERROR,"Live weapon action, particles and projectile models render");
    controls(jni);for(unsigned n=0;n<300 && leon->action()!=re4::PlayerAction::Footwork;++n)ticks(jni,1);
    require(leon->action()==re4::PlayerAction::Footwork,"Released real inputs finish recoil/throw and holster");
    if(playerProjectiles)playerProjectiles->clear();
  }
  // Held-through input remains gated after suspension; a later fresh edge
  // resumes normally, without weakening interruption protection.
  controls(jni,0,1);ticks(jni,50);require(leon->aiming(),"Aim before held-through menu test");
  Java_com_ast_re4decomp_NativeBridge_setSubscreenOpen(&jni.env,nullptr,JNI_TRUE);
  frozenPixels(jni);Java_com_ast_re4decomp_NativeBridge_setSubscreenOpen(&jni.env,nullptr,JNI_FALSE);
  for(unsigned n=0;n<100 && leon->action()!=re4::PlayerAction::Footwork;++n)ticks(jni,1);
  ticks(jni,3);require(leon->action()==re4::PlayerAction::Footwork,"LT held through a menu stays gated after the existing holster finishes");
  controls(jni);ticks(jni,1);controls(jni,0,1);ticks(jni,1);
  require(leon->aiming(),"One fresh LT press after release starts aiming");
  std::cout<<"K17 real JNI25 weapons/first LT edge and unchanged visible enemy pixels across every Equip PASS: "<<checks<<" checks\n";
}catch(const std::exception &e){std::cerr<<"K17 runtime failure: "<<e.what()<<'\n';return 1;}}
