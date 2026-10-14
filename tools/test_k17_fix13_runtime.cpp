// Real Android bridge, source SS animations and real Ganado hit→blood path.
#define RE4_K16_ENTRY previous_k16_fix6_main
#include "test_k16_runtime.cpp"
#undef RE4_K16_ENTRY
#include "generic_event_actor.h"
#include "model_textures_gl.h"
namespace {
using namespace save_test;
re4::Texture settledPortrait(const re4::OriginalMenuModel &m,int w,int h) {
 re4::RenderSubscreenPortraitGl(m,w,h);glFinish();
 re4::RenderSubscreenPortraitGl(m,w,h);glFinish();
 return re4::RenderSubscreenPortraitGl(m,w,h);
}
unsigned edgeCoverage(const re4::Texture &t){unsigned n=0;for(unsigned y=1;y+1<t.height;++y)for(unsigned x=1;x+1<t.width;++x){auto k=(y*t.width+x)*4+3;auto a=t.rgba[k];if(a && a<255 && (!t.rgba[k-4] || !t.rgba[k+4] || !t.rgba[k-4*t.width] || !t.rgba[k+4*t.width]))++n;}return n;}
unsigned alphaPixels(const re4::Texture &t){unsigned n=0;for(unsigned i=3;i<t.rgba.size();i+=4)if(t.rgba[i])++n;return n;}
unsigned textureChanges(const re4::Texture &a,const re4::Texture &b){require(a.width==b.width && a.height==b.height,"Portrait dimensions stable");return pixelChanges(a.rgba,b.rgba);}
}
int main(int argc,char **argv){using namespace save_test;try {
 require(argc==2,"Usage: k17_fix13_runtime_check FIXTURE_ROOT");
 const auto root=std::filesystem::path(argv[1]),files=std::filesystem::canonical(root/"files");
 const auto js=[&](const std::filesystem::path &p){return string(p.string());};Egl egl;SaveJni jni;const jlong session=17505;
 require(re4::requestedMsaa==0 && re4::userAf==8 && re4::userMipmapping && re4::userRoomFps.load(),"All four requested default values");
 Java_com_ast_re4decomp_NativeBridge_configureGraphics(&jni.env,nullptr,0,8,JNI_FALSE,JNI_FALSE,JNI_FALSE,JNI_FALSE);
 require(!re4::userMipmapping && Java_com_ast_re4decomp_NativeBridge_roomFpsVisible(&jni.env,nullptr)==JNI_FALSE,"Native menu applies mipmapping and Room/FPS OFF");
 Java_com_ast_re4decomp_NativeBridge_configureGraphics(&jni.env,nullptr,0,8,JNI_TRUE,JNI_TRUE,JNI_FALSE,JNI_FALSE);
 require(re4::userMipmapping && Java_com_ast_re4decomp_NativeBridge_roomFpsVisible(&jni.env,nullptr)==JNI_TRUE,"Native menu restores both ON defaults");
 {
  re4::ModelTextureLodGl lod;lod.ranges.push_back({0,1});GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
  const unsigned char pixel[]{255,255,255,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
  for(int af:{1,2,4,8}) {re4::userAf=af;lod.bind({texture});
    if(lod.anisotropySupported){float actual=0;glGetTexParameterfv(GL_TEXTURE_2D,0x84fe,&actual);require(actual==std::min(float(af),lod.maximumAnisotropy),"AF selection changes actual texture sampler");}
    require(glGetError()==GL_NO_ERROR,"AF override has no GLES error");
  }
  re4::userAf=8;glDeleteTextures(1,&texture);
 }
 for(int count:{0,2,4}) {
   re4::requestedMsaa=count;
   {re4::MultisampleFrameGl frame(320,240);
    GLint samples=0;glGetIntegerv(GL_SAMPLES,&samples);
    if(frame.active)require(samples==count,"MSAA renderbuffer has requested sample count");
    else require(count==0 || !re4::SupportsSamplesGl(GL_RGBA8,count) || !re4::SupportsSamplesGl(GL_DEPTH_COMPONENT24,count),"Unsupported exact sample count falls back");
    glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(.25f,.5f,.75f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
   }
   const auto error=glGetError();std::cout<<"MSAA requested "<<count<<" actual "<<re4::msaaActual<<" error "<<error<<std::endl;
   require(error==GL_NO_ERROR,"MSAA allocation and resolve have no GLES error");
 }
 re4::requestedMsaa=0;re4::DestroyMsaaGl();
 for(unsigned id:{0x21u,0x2eu,0x2fu,0x36u,0x34u,0x0eu}) {
  const auto still=re4::LoadOriginalMenuPortrait(files.string(),id);
  auto live=re4::LoadOriginalMenuPortrait(files.string(),id,true);
  const auto source=settledPortrait(still,640,480),initial=settledPortrait(live,640,480);
  require(alphaPixels(initial)>5000,"Original animated SS model is visibly rendered");
  const auto initialDifference=textureChanges(source,initial);
  std::cout<<"Menu boundary partial coverage "<<edgeCoverage(source)<<" -> "<<edgeCoverage(initial)<<'\n';
  if(id==0x21)require(edgeCoverage(initial)>edgeCoverage(source)+100,"Inventory MSAA adds smooth outer-edge coverage pixels");
  std::cout<<"SS profile "<<id<<" static/reference difference "<<initialDifference<<" opaque "<<alphaPixels(initial)<<'\n';
  unsigned outlineDifference=0,nonEdgeDifference=0,interior=0;unsigned long rgbDifference=0;
  for(unsigned y=1;y+1<source.height;++y)for(unsigned x=1;x+1<source.width;++x) {
    const auto pixel=(y*source.width+x)*4;const bool reference=source.rgba[pixel+3]>=128;
    if(reference!=(initial.rgba[pixel+3]>=128)) {
      ++outlineDifference;bool boundary=false;
      for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
        const auto n=(std::size_t(int(y)+dy)*source.width+unsigned(int(x)+dx))*4+3;
        if((source.rgba[n]>=128)!=reference)boundary=true;
      }
      if(!boundary)++nonEdgeDifference;
    }
    if(source.rgba[pixel+3]==255 && initial.rgba[pixel+3]==255) {
      ++interior;for(unsigned k=0;k<3;++k)rgbDifference+=unsigned(std::abs(int(source.rgba[pixel+k])-int(initial.rgba[pixel+k])));
    }
  }
  // AA deliberately changes outer coverage. Compare the original silhouette
  // within one pixel and lighting only in the fully covered model interior.
  // A 4-sample texel average is not the old pixel-center lookup: bound mean
  // RGB differences to 4/255 rather than require identical texture samples.
  std::cout<<"AA silhouette changes "<<outlineDifference<<" away from edge "<<nonEdgeDifference<<" interior RGB error "<<double(rgbDifference)/std::max(1u,interior*3)<<'\n';
  require(nonEdgeDifference<40 && interior>5000 && double(rgbDifference)/(interior*3)<4.,
      "Antialiased frame0 preserves source pose and interior lighting");
  const auto phase=live.actors.front()->rig()->sequenceFrame();
  const auto paused=settledPortrait(live,640,480);
  require(textureChanges(initial,paused)==0 && live.actors.front()->rig()->sequenceFrame()==phase,"Portrait draw never advances independent animation clock");
  live.advance(25);const auto moved=settledPortrait(live,640,480);
  require(textureChanges(initial,moved)>30,"Original SS idle produces visible breathing pixels");
  re4::DestroySubscreenPortraitGl(&live);const auto restored=settledPortrait(live,640,480);
  require(textureChanges(moved,restored)==0,"Persistent menu uploads rebuild exact animated pose");
  egl.fresh();re4::ForgetSubscreenPortraitGl();const auto fresh=settledPortrait(live,640,480);
  require(textureChanges(moved,fresh)==0,"Original menu pose restores exactly after EGL context loss");
  // The midpoint is a presentation pose, independent of original 30-Hz state.
  const auto tick=live.actors.front()->rig()->sequenceFrame();
  live.renderFraction=0;const auto before=settledPortrait(live,640,480);
  live.renderFraction=.5;const auto middle=settledPortrait(live,640,480);
  live.renderFraction=1;const auto end=settledPortrait(live,640,480);
  require(textureChanges(before,middle)>0 && textureChanges(middle,end)>0,"Original SS poses render distinct smooth intermediate frames");
  require(live.actors.front()->rig()->sequenceFrame()==tick,"GL interpolation leaves 30-Hz menu clock unchanged");
  re4::DestroySubscreenPortraitGl(&live);
 }
 Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,js(files/"etc/core.das"));
 Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,js(files/"em/em12.drs"),js(files/"etc/emleon00.esl"));
 Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,js(files/"em/wep02.drs"),js(files/"em/wep07.drs"));
 Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
 Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,JNI_TRUE,0,session);
 Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(&jni.env,nullptr,JNI_TRUE);
 Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,js(root/"decoded/r101.dat"),js(files/"em/pl00.drs"),js(files/"em/wep01.drs"),js(files/"Debug/roominfo.dat"),string("r101"),session);
 re4::requestedMsaa=4;
 frozenPixels(jni);unpause(jni);controls(jni);
 for(unsigned i=0;i<180 && r100EventPhase==12;++i)ticks(jni,1);
 require(r100EventPhase==13 && r100RadioTerm,"Production entry radio reaches terminal");finishR100Radio();frozenPixels(jni);ticks(jni,12);
 Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_TRUE);
 Java_com_ast_re4decomp_NativeBridge_configureSubscreen(&jni.env,nullptr,js(files),1);
 Java_com_ast_re4decomp_NativeBridge_setSubscreenOpen(&jni.env,nullptr,JNI_TRUE);
 const auto realWidth=width,realHeight=height;width=1920;height=1080;prepareSubscreenPortrait(1,.5);
 require(subscreenPortraits.at(leon->equippedWeaponId()).width==640 && subscreenPortraits.at(leon->equippedWeaponId()).height==480,"Physical 1080p viewport still transfers authored 640x480 portrait");
 width=realWidth;height=realHeight;
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
 Java_com_ast_re4decomp_NativeBridge_gyroAvailable(nullptr,nullptr,JNI_FALSE);
 Java_com_ast_re4decomp_NativeBridge_configureGraphics(&jni.env,nullptr,0,8,JNI_TRUE,JNI_TRUE,JNI_TRUE,JNI_FALSE);
 require(!gyroEnabled.load(),"Missing sensor rejects Gyroscope ON");
 Java_com_ast_re4decomp_NativeBridge_gyroAvailable(nullptr,nullptr,JNI_TRUE);
 Java_com_ast_re4decomp_NativeBridge_configureGraphics(&jni.env,nullptr,0,8,JNI_TRUE,JNI_TRUE,JNI_TRUE,JNI_FALSE);
 require(gyroEnabled.load(),"Available sensor enables Gyroscope");
 Java_com_ast_re4decomp_NativeBridge_gyroRate(nullptr,nullptr,20.f,NAN);
 require(gyroYawRate==3.f && gyroPitchRate==0.f,"Sensor rates bounded and nonfinite rejected");
 re4::PlayerActionInput aim;aim.aimHeld=true;
 for(unsigned t=0;t<90 && leon->action()!=re4::PlayerAction::Aim;++t)leon->step(*walkProbe,0,0,false,false,1./30,aim);
 require(leon->action()==re4::PlayerAction::Aim,"Real weapon drawn for gyro test");
 const auto gyroHeading=leon->heading(),gyroPitch=leon->aimRate();aim.gyroYaw=.2;aim.gyroPitch=.15;
 for(unsigned t=0;t<30;++t)leon->step(*walkProbe,0,0,false,false,1./30,aim);
 require(leon->heading()>gyroHeading && leon->aimRate()>gyroPitch,"Fine gyro motion changes actual real weapon aim");
 aim.gyroYaw=aim.gyroPitch=0;const auto stoppedHeading=leon->heading(),stoppedPitch=leon->aimRate();
 for(unsigned t=0;t<30;++t)leon->step(*walkProbe,0,0,false,false,1./30,aim);
 require(leon->heading()==stoppedHeading && std::abs(leon->aimRate()-stoppedPitch)<.064f,"Stationary gyro adds no yaw drift and retains bounded original weapon sway");
 aim.aimHeld=false;
 for(unsigned t=0;t<90 && leon->action()!=re4::PlayerAction::Footwork;++t)leon->step(*walkProbe,0,0,false,false,1./30,aim);
 require(leon->action()==re4::PlayerAction::Footwork,"Weapon holstered after gyro aim");
 const auto walkingHeading=leon->heading();aim.gyroYaw=aim.gyroPitch=1;
 for(unsigned t=0;t<30;++t)leon->step(*walkProbe,0,0,false,false,1./30,aim);
 require(leon->heading()==walkingHeading,"Gyro cannot turn ordinary walking actor");
 Java_com_ast_re4decomp_NativeBridge_configureGraphics(&jni.env,nullptr,0,8,JNI_TRUE,JNI_TRUE,JNI_FALSE,JNI_FALSE);
 require(!gyroEnabled.load(),"Gyroscope OFF applied");
 require(!re4::userDepthOfField,"DoF default is OFF");
 Java_com_ast_re4decomp_NativeBridge_configureGraphics(&jni.env,nullptr,0,8,JNI_TRUE,JNI_TRUE,JNI_FALSE,JNI_TRUE);
 require(re4::userDepthOfField,"Native DoF ON applied");
 subscreenOpen=false;paused=false;activeEvent=nullptr;r100EventPhase=0;Java_com_ast_re4decomp_NativeBridge_draw(&jni.env,nullptr);
 require(glGetError()==GL_NO_ERROR,"DoF runs on real gameplay scene/depth");
 for(int option=0;option<3;++option){
  Java_com_ast_re4decomp_NativeBridge_configureGraphics(&jni.env,nullptr,0,8,JNI_TRUE,JNI_TRUE,option!=0?JNI_TRUE:JNI_FALSE,option!=1?JNI_TRUE:JNI_FALSE);
  for(int frame=0;frame<10;++frame){Java_com_ast_re4decomp_NativeBridge_draw(&jni.env,nullptr);require(!paused && glGetError()==GL_NO_ERROR,"DoF/gyro separate and together keep real gameplay running");}
 }
 Java_com_ast_re4decomp_NativeBridge_configureGraphics(&jni.env,nullptr,0,8,JNI_TRUE,JNI_TRUE,JNI_FALSE,JNI_FALSE);
 require(!re4::userDepthOfField,"Native DoF OFF applied");
 // Natural event completion keeps both owned one-shots and streamed speech.
 audioUiActive.store(true);audioGameActive.store(true);audioSubscreenActive.store(false);syncAudioPause();
 const auto speech=re4::SoundStream::Load(files.string()+"/bgm/bio4str.hed",files.string()+"/bgm/bio4evt.sbb",1,2);
 eventMixer.installStream(speech,true);const auto sound=soundMixer.play(0,4);require(sound!=0,"Real core one-shot starts");eventVoiceHandles.push_back(sound);
 r100EventPhase=0;finishR100Event();
 require(soundMixer.voiceActive(sound) && eventMixer.streamPlaying(1,2),"Natural event end retains original one-shot and speech tail");
 m35_test::Pcm nativePcm;m35_test::AudioJni audioJni;
 const auto pumpAudio=[&](){require(Java_com_ast_re4decomp_NativeBridge_renderAudio(&audioJni.env,nullptr,reinterpret_cast<jshortArray>(&nativePcm))==1024,"Production native audio block emitted");};
 pumpAudio();
 require(!eventUiActive.load() && eventMixer.streamRenderedFrames()==1024,"Production audio JNI continues speech after visual event ends");
 std::vector<std::int16_t> audio(1024);std::uint64_t rendered=1024;
 while(eventMixer.streamPlaying(1,2) && rendered<60*re4::SoundMixer::OutputRate){eventMixer.render(audio.data(),512);rendered+=512;}
 require(!eventMixer.streamPlaying(1,2) && eventMixer.streamRenderedFrames()==std::uint64_t(std::ceil(double(speech->left.size())*re4::SoundMixer::OutputRate/speech->rate)),"Original house speech consumes all source samples");
 eventMixer.installStream(speech,true);eventVoiceHandles.push_back(sound);finishR100Event(true);
 require(!soundMixer.voiceActive(sound) && !eventMixer.streamPlaying(1,2),"Explicit event cancel still stops sounds");
 startupMixer.clear();startupMixer.installBanks(re4::SoundBank::LoadDvd((files/"etc/core.das").string()));startupMixer.installBanks(re4::SoundBank::LoadDvd((files/"ss/Cmn/title.snd").string()));startupMixer.setPaused(false);
 const auto titleMusic=startupMixer.play(6,4);require(titleMusic!=0,"Original title music remains independently active");
 startupVoiceComplete.store(true);audioStartupActive.store(true);pendingStartupAudio.clear();
 Java_com_ast_re4decomp_NativeBridge_startupCue(&jni.env,nullptr,0,0x38);
 require(Java_com_ast_re4decomp_NativeBridge_startupVoiceEnded(&jni.env,nullptr)==JNI_FALSE,"Queued load chime blocks startup drain");
 pumpAudio();const auto chime=startupLoadVoice.load();require(chime!=0,"Original load chime exists");
 require(Java_com_ast_re4decomp_NativeBridge_startupVoiceEnded(&jni.env,nullptr)==JNI_FALSE,"Playing load chime blocks startup drain");
 rendered=1024;while(startupMixer.voiceActive(chime) && rendered<5*re4::SoundMixer::OutputRate){pumpAudio();rendered+=1024;}
 require(!startupMixer.voiceActive(chime) && Java_com_ast_re4decomp_NativeBridge_startupVoiceEnded(&jni.env,nullptr)==JNI_TRUE,"Load drain releases only after complete original chime");
 require(startupMixer.voiceActive(titleMusic),"Looping title music does not block chime drain");audioStartupActive.store(false);
 require(javaException.empty() && glGetError()==GL_NO_ERROR,"Fix6 production JNI/GLES healthy");
 std::cout<<"K17 Fix13 production SS/blood/pause/context checks PASS: "<<checks<<" checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"K17 Fix13 runtime FAILED: "<<e.what()<<'\n';return 1;}}
