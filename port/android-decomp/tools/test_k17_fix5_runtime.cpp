// Real Android bridge, source SS animations and real Ganado hit→blood path.
#define RE4_K16_ENTRY previous_k16_fix5_main
#include "test_k16_runtime.cpp"
#undef RE4_K16_ENTRY
#include "generic_event_actor.h"
namespace {
using namespace save_test;
unsigned alphaPixels(const re4::Texture &t){unsigned n=0;for(unsigned i=3;i<t.rgba.size();i+=4)if(t.rgba[i])++n;return n;}
unsigned textureChanges(const re4::Texture &a,const re4::Texture &b){require(a.width==b.width && a.height==b.height,"Portrait dimensions stable");return pixelChanges(a.rgba,b.rgba);}
}
int main(int argc,char **argv){using namespace save_test;try {
 require(argc==2,"Usage: k17_fix5_runtime_check FIXTURE_ROOT");
 const auto root=std::filesystem::path(argv[1]),files=std::filesystem::canonical(root/"files");
 const auto js=[&](const std::filesystem::path &p){return string(p.string());};Egl egl;SaveJni jni;const jlong session=17505;
 for(unsigned id:{0x21u,0x2eu,0x2fu,0x36u,0x34u,0x0eu}) {
  const auto still=re4::LoadOriginalMenuPortrait(files.string(),id);
  auto live=re4::LoadOriginalMenuPortrait(files.string(),id,true);
  const auto source=re4::RenderSubscreenPortraitGl(still,640,480),initial=re4::RenderSubscreenPortraitGl(live,640,480);
  require(alphaPixels(initial)>5000,"Original animated SS model is visibly rendered");
  const auto initialDifference=textureChanges(source,initial);
  std::cout<<"SS profile "<<id<<" static/reference difference "<<initialDifference<<" opaque "<<alphaPixels(initial)<<'\n';
  unsigned outlineDifference=0;unsigned long rgbDifference=0;
  for(unsigned pixel=0;pixel<source.rgba.size();pixel+=4) {
    if(bool(source.rgba[pixel+3])!=bool(initial.rgba[pixel+3]))++outlineDifference;
    for(unsigned k=0;k<3;++k)rgbDifference+=unsigned(std::abs(int(source.rgba[pixel+k])-int(initial.rgba[pixel+k])));
  }
  // GPU skinning blends inverse-normal matrices, while the frozen CPU pose
  // transforms blended normals first. Compare silhouette and mean lighting
  // error; exact RGB equivalence would incorrectly reject rounding/shading.
  require(outlineDifference<40 && double(rgbDifference)/(source.width*source.height*3)<.5,
      "Original frame0 silhouette and lighting agree with independent frozen SS reference");
  const auto phase=live.actors.front()->rig()->sequenceFrame();
  const auto paused=re4::RenderSubscreenPortraitGl(live,640,480);
  require(textureChanges(initial,paused)==0 && live.actors.front()->rig()->sequenceFrame()==phase,"Portrait draw never advances independent animation clock");
  live.advance(25);const auto moved=re4::RenderSubscreenPortraitGl(live,640,480);
  require(textureChanges(initial,moved)>30,"Original SS idle produces visible breathing pixels");
  re4::DestroySubscreenPortraitGl(&live);const auto restored=re4::RenderSubscreenPortraitGl(live,640,480);
  require(textureChanges(moved,restored)==0,"Persistent menu uploads rebuild exact animated pose");
  egl.fresh();re4::ForgetSubscreenPortraitGl();const auto fresh=re4::RenderSubscreenPortraitGl(live,640,480);
  require(textureChanges(moved,fresh)==0,"Original menu pose restores exactly after EGL context loss");
  re4::DestroySubscreenPortraitGl(&live);
 }
 Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,js(files/"etc/core.das"));
 Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,js(files/"em/em12.drs"),js(files/"etc/emleon00.esl"));
 Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,js(files/"em/wep02.drs"),js(files/"em/wep07.drs"));
 Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
 Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,JNI_TRUE,0,session);
 Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(&jni.env,nullptr,JNI_TRUE);
 Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,js(root/"decoded/r101.dat"),js(files/"em/pl00.drs"),js(files/"em/wep01.drs"),js(files/"Debug/roominfo.dat"),string("r101"),session);
 frozenPixels(jni);unpause(jni);controls(jni);
 for(unsigned i=0;i<180 && r100EventPhase==12;++i)ticks(jni,1);
 require(r100EventPhase==13 && r100RadioTerm,"Production entry radio reaches terminal");finishR100Radio();frozenPixels(jni);ticks(jni,12);
 Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_TRUE);
 Java_com_ast_re4decomp_NativeBridge_configureSubscreen(&jni.env,nullptr,js(files),1);
 Java_com_ast_re4decomp_NativeBridge_setSubscreenOpen(&jni.env,nullptr,JNI_TRUE);
 require(subscreenLeon && !subscreenLeon->actors.empty(),"Production menu uses independent live SS skeleton");
 const auto id=leon->equippedWeaponId();const auto portrait=subscreenPortraits.at(id);
 const auto feet=leon->feet();const auto playerFrame=leon->rig().sequenceFrame(),heading=leon->heading();const auto worldTick=enemyGroup->worldTicks();
 ticks(jni,25);const auto after=subscreenPortraits.at(id);
 require(textureChanges(portrait,after)>30,"Native paused menu publishes changing original portrait frames");
 require(leon->feet()==feet && leon->heading()==heading && leon->rig().sequenceFrame()==playerFrame && enemyGroup->worldTicks()==worldTick,"SS loop leaves room player and enemy simulation paused");
 const auto stopped=subscreenLeon->actors.front()->rig()->sequenceFrame();
 Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_FALSE);ticks(jni,4);
 require(subscreenLeon->actors.front()->rig()->sequenceFrame()==stopped,"Background application pauses its independent menu clock");
 Java_com_ast_re4decomp_NativeBridge_setSubscreenOpen(&jni.env,nullptr,JNI_FALSE);
 Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_TRUE);controls(jni);ticks(jni,2);
 require(enemyEffects && enemyEffectsData && enemyEffectsData->triggerBanks.size()==8,"Production village loads all eight source blood profiles");
 auto *enemy=enemyGroup->find(20);require(enemy && enemy->alive(),"Authored active village victim");const auto p=enemy->feet();
 re4::PlayerShot shot;shot.weaponItemId=0x2e;shot.serial=0x50001;shot.powerLevel=0;
 bool hit=false;
 for(unsigned h=400;h<=1600 && !hit;h+=100)for(unsigned d=0;d<16 && !hit;++d) {
  const float a=float(d)*3.14159265f/8;
  shot.from={p[0]+std::sin(a)*1800,p[1]+h,p[2]+std::cos(a)*1800};
  shot.to={p[0]-std::sin(a)*1800,p[1]+h,p[2]-std::cos(a)*1800};hit=bool(enemy->target(shot.from,shot.to,9));
 }
 require(hit,"Real posed village Ganado capsule accepts Rifle ray");
 require(walkProbe->placeAuthored(shot.from),"Rifle observer stands on real floor");leon->setPlacement(walkProbe->position(),0);leon->setUnlimitedHealth(true);
 const auto before=enemyEffects->state().acceptedEvents.size();
 enemy->queueProjectileImpact(shot);enemy->stepPlayer(*leon,*walkProbe,0,0,false,false,1./30,{});
 drainEnemyFeedback(*enemy,groupVoiceHandles[enemy]);
 require(enemy->shots()>0 && enemyEffects->state().acceptedEvents.size()>before,"Actual Rifle damage reaches native blood renderer with valid source event id");
 re4::WorldEffectCamera camera;camera.eye={float(p[0]),float(p[1]+1600),float(p[2]+3000)};camera.forward={0,0,-1};
 bool floorBlood=false;
 for(unsigned tick=0;tick<120;++tick) {
  enemyEffects->tick(&camera,collisionWorld.get());
  const auto quads=re4::WorldEffectQuads(*enemyEffectsData,enemyEffects->state(),camera);
  for(const auto &q:quads)if(q.type==16)floorBlood=true;
 }
 require(floorBlood,"Real Rifle hit emits original Esp10 blood-on-floor quads");
 require(javaException.empty() && glGetError()==GL_NO_ERROR,"Fix5 production JNI/GLES healthy");
 std::cout<<"K17 Fix5 production SS/blood/pause/context checks PASS: "<<checks<<" checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"K17 Fix5 runtime FAILED: "<<e.what()<<'\n';return 1;}}
