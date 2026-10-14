// Actual JNI, GLES scene publication, authored animals and audio owners.
#define RE4_K16_ENTRY previous_k16_fix4_main
#include "test_k16_runtime.cpp"
#undef RE4_K16_ENTRY
namespace {
void renderPcm(unsigned callbacks) {
  std::array<std::int16_t,480> out{};soundMixer.setPaused(false);
  while(callbacks--)soundMixer.render(out.data(),240); // 5ms at host output48kHz.
}
void culling(const std::shared_ptr<const re4::RoomLighting> &original) {
  auto lighting=std::make_shared<re4::RoomLighting>(*original);
  lighting->areas.clear();for(auto &cut:lighting->cuts) {
    cut.screenAmbient=cut.actorAmbient={255,255,255,255};cut.lights.clear();cut.fog.type=0;
    cut.fog.start=0;cut.fog.end=200000;cut.farRatio=0;cut.focusLevel=cut.blurRate=0;cut.contrast={0,0,0};
  }
  const auto sample=[&](bool two,bool wrongRoot) {
    re4::StaticScene scene;scene.lighting=lighting;
    const auto triangle=[&](float x) {
      re4::SceneDraw d;d.first=scene.vertices.size()/3;d.count=3;d.authored=true;d.orderType=0;
      d.noDiffuseLight=true;d.materialRGB={220,70,35};d.sortCenter={wrongRoot?60000:x,0,-10000};
      d.lightVolume.center={x,0,-10000};d.lightVolume.size={1000,1000,1};d.lightSize=1500;
      scene.draws.push_back(d);
      scene.vertices.insert(scene.vertices.end(),{x-1000,-1000,-10000,x+1000,-1000,-10000,x,1000,-10000});
      scene.normals.insert(scene.normals.end(),{0,0,1,0,0,1,0,0,1});scene.uv.insert(scene.uv.end(),{0,0,1,0,.5f,1});
      scene.colors.insert(scene.colors.end(),12,255);++scene.triangles;
    };
    if(two)triangle(60000);
    triangle(0);scene.minimum={-1000,-1000,-10000};scene.maximum={61000,1000,-10000};
    re4::CreateSceneGl(scene);re4::EventCamera camera;camera.eye={0,0,0};camera.target={0,0,-1};camera.fovRadians=1.57079633f;camera.valid=true;
    re4::SceneProbe probe;probe.showBody=false;probe.roomCamera=&camera;
    re4::DrawSceneGl(960,540,0,0,0,false,&probe,nullptr,0);
    std::array<std::uint8_t,4> pixel{};glReadPixels(480,270,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
    save_test::require(glGetError()==GL_NO_ERROR,"Culling fixture GLES healthy");re4::DestroySceneGl();return pixel;
  };
  const auto single=sample(false,false),multiple=sample(true,false),foot=sample(false,true);
  save_test::require(single[0]>100,"Independent visible model reaches physical framebuffer");
  save_test::require(single==multiple,"Offscreen first model cannot hide same-textured second model");
  save_test::require(single==foot,"Visible crown uses its bound center even when model root is offscreen");
}
}
int main(int argc,char **argv){using namespace save_test;try {
  require(argc==2,"Usage: k17_fix4_runtime_check FIXTURE_ROOT");
  const auto root=std::filesystem::path(argv[1]),files=std::filesystem::canonical(root/"files");
  const auto js=[&](const std::filesystem::path &p){return string(p.string().c_str());};
  Egl egl;SaveJni jni;const jlong session=17504;
  Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,js(files/"etc/core.das"));
  Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,js(files/"em/em12.drs"),js(files/"etc/emleon00.esl"));
  Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,js(files/"em/wep02.drs"),js(files/"em/wep07.drs"));
  Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
  Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,JNI_TRUE,0,session);
  Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(&jni.env,nullptr,JNI_TRUE);
  Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,js(root/"decoded/r101.dat"),js(files/"em/pl00.drs"),js(files/"em/wep01.drs"),js(files/"Debug/roominfo.dat"),string("r101"),session);
  frozenPixels(jni);unpause(jni);controls(jni);
  for(unsigned i=0;i<180 && r100EventPhase==12;++i)ticks(jni,1);
  require(r100EventPhase==13 && r100RadioTerm,"Entry radio reaches terminal");finishR100Radio();frozenPixels(jni);
  ticks(jni,12);require(!r100RadioFadeFrame,"Source radio fade completes before measuring visible animals");
  Java_com_ast_re4decomp_NativeBridge_setCheatFlags(&jni.env,nullptr,3);
  require(villageAnimals && villageAnimals->animals().size()==3,"Production r101 owns one cow and two chickens independently of Ganados");
  require(enemyGroup->actors().size()==43,"Animals do not change the 43 authored Ganado/wave owners");
  require(soundMixer.hasCue(8,4,0x26) && soundMixer.hasCue(8,2,0x28),"Original EM26/EM28 sound banks reach production mixer");
  for(const auto &animal:villageAnimals->animals()) {
    const auto p=animal->feet();unsigned pixels{};
    for(unsigned view=0;view<16 && pixels<=5;++view) {
      const float heading=float(view)*3.14159265f/8;
      require(walkProbe->placeAuthored({p[0]-std::sin(heading)*900,p[1],p[2]-std::cos(heading)*900}),"Viewpoint uses real village floor");
      leon->setPlacement(walkProbe->position(),heading);re4::SceneFollowHeading(heading);re4::SceneSeedFollowView(0,-.05);re4::SceneFollowDistance(1800);
      const auto visible=frozenPixels(jni);auto owner=std::move(villageAnimals);const auto absent=frozenPixels(jni);villageAnimals=std::move(owner);
      pixels=std::max(pixels,pixelChanges(visible,absent));
    }
    std::cout<<"Animal slot "<<animal->source().listIndex<<" original visible RGB pixels "<<pixels<<'\n';
    require(pixels>5,"Every authored animal contributes real skinned scene pixels");
  }
  // Real knife input restores the carried gun on source down10 frame4.
  require(walkProbe->placeAuthored({-38027,141,11114}),"Knife test uses quiet original entry approach");
  leon->setPlacement(walkProbe->position(),0);re4::SceneFollowHeading(0);
  controls(jni,re4::Lb);for(unsigned i=0;i<80 && leon->action()!=re4::PlayerAction::KnifeReady;++i)ticks(jni,1);
  if(leon->action()!=re4::PlayerAction::KnifeReady)
    throw std::runtime_error("Production knife ready: "+leon->describe()+" event "+std::to_string(r100EventPhase));
  controls(jni);
  bool switched=false;
  for(unsigned i=0;i<80 && leon->action()!=re4::PlayerAction::Footwork;++i) {
    ticks(jni,1);if(leon->action()==re4::PlayerAction::KnifeHolster && !leon->bladeVisible()) {
      switched=true;require(leon->weaponVisible(),"Production gun visible while original knife return is still animating");
    }
  }
  require(switched,"Production knife return crosses source visibility handoff");
  // An actual animal hit survives the native GameCube save codec.
  const auto &cow=*villageAnimals->animals().front();const auto p=cow.feet();
  villageAnimals->queueExplosion(p,2000,19);beforeWorldTick();afterWorldTick(nullptr);
  const auto snapshot=captureNativeGame();const auto restored=re4::DecodeNativeSave(re4::EncodeNativeSave(snapshot,originalSaveBasis?&*originalSaveBasis:nullptr));
  require(restored.enemyDeaths && re4::OriginalSaveEnemyDead(*restored.enemyDeaths,0,0x34),"Cow death reaches original native save bit");
  // Source post-bell revisit music starts once and fades after the last enemy.
  scenarioState.room(0x101).room.set(7);scenarioState.scenario.set(10);
  villageScenario->tick(scenarioState,leon->feet(),leon->heading(),2,true);drainVillageCues();
  require(soundMixer.streamPlaying(0,2),"Post-bell Ganado encounter starts original battle stream");renderPcm(620);
  require(soundMixer.streamVolumeFixed(0,2)>0,"Original battle fade reaches audible volume");
  villageScenario->tick(scenarioState,leon->feet(),leon->heading(),0,false);drainVillageCues();renderPcm(620);
  require(soundMixer.streamVolumeFixed(0,2)==0,"After last post-bell attacker the original 3-second fade reaches silence");
  // Bell removal must destroy looping per-enemy audio, including a cue
  // queued just before the source RemoveGanados transition.
  auto *chainsaw=enemyGroup->find(0x1c);require(chainsaw && chainsaw->actor().weaponType()==4,"Authored chainsaw owner");
  require(soundMixer.hasCue(6,0x50),"Original r101 DVD bank contains the chainsaw attack cue");
  const auto handle=playRoomSound(6,0x50,chainsaw->feet());groupVoiceHandles[chainsaw][unsigned(re4::EnemySoundChannel::Contact)]=handle;
  require(handle && soundMixer.voiceActive(handle),"Original chainsaw attack cue has a real audio handle");
  scenarioState.scenario.set(10,false);scenarioState.room(0x101).room.set(7,false);scenarioState.room(0x101).room.set(8,false);
  re4::R101Scenario bellScenario([&]{auto a=re4::RoomArchive::Load((root/"decoded/r101.dat").string());auto *e=a.find("AEV");return a.copy(e-a.entries().data());}());
  bellScenario.tick(scenarioState,leon->feet(),leon->heading(),11,true);
  for(unsigned i=0;i<15000 && !scenarioState.room(0x101).room.test(7);++i)bellScenario.tick(scenarioState,leon->feet(),leon->heading(),11,true);
  require(scenarioState.room(0x101).room.test(7),"Source fight timeout starts bell");
  bellScenario.begin(re4::VillageEvent::Bell,scenarioState,11);
  auto originalScenario=std::move(villageScenario);villageScenario=std::make_unique<re4::R101Scenario>(std::move(bellScenario));drainVillageCues();villageScenario=std::move(originalScenario);
  require(!soundMixer.voiceActive(handle),"Bell removes looping chainsaw audio with its Ganado owner");
  culling(previewLighting);
  require(glGetError()==GL_NO_ERROR && javaException.empty(),"Fix4 production JNI/GLES/audio paths healthy");
  std::cout<<"K17 Fix4 production JNI/GLES animals,knife,save,audio and frustum PASS: "<<checks<<" checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"K17 Fix4 runtime FAILED: "<<e.what()<<'\n';return 1;}}
