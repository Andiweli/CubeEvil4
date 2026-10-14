// Actual JNI/GLES regression for the reported opening and three-Ganado path.
// Requires the user's original Disc1 resources; no game assets are bundled.
#define RE4_M36_ENTRY previous_render_fight_runtime
#include "test_m36_runtime.cpp"
#undef RE4_M36_ENTRY
#include "room_lighting_gl.h"
#include <fstream>

namespace {
void saveFrame(m35_test::Jni &jni,const char *name) {
  const auto frame=m35_test::frozenPixels(jni);
  std::ofstream out(name,std::ios::binary);out<<"P6\n960 540\n255\n";
  for(int y=539;y>=0;--y)for(int x=0;x<960;++x)
    out.write(reinterpret_cast<const char*>(frame.data()+(y*960+x)*4),3);
  m35_test::require(bool(out),"Native framebuffer export");
}
std::pair<re4::CollisionVec,re4::CollisionVec> bodyRay(const re4::EnemyEncounter &enemy) {
  const auto shapes=re4::NormalGanadoHitShapes();
  const auto &body=shapes.at(1);
  auto offset=body.offset;offset[1]+=body.height*.5f;
  const auto center=re4::TransformPoint(re4::WeaponAttachmentMatrix(
      enemy.actor().rig().jointMatrices().at(unsigned(body.partsNo-1)),enemy.placement()),offset);
  for(unsigned axis:{0u,2u,1u})for(double side:{1.,-1.}) {
    re4::CollisionVec from{center[0],center[1],center[2]},to=from;
    from[axis]-=side*800;to[axis]+=side*800;
    const auto hit=re4::HitAuthoredShapes(re4::NormalGanadoHitShapes(),
        enemy.actor().rig().jointMatrices(),enemy.placement(),from,to,2560000,0x10);
    if(hit && re4::NormalGanadoHitShapes().at(hit->partIndex).partsNo!=5)return {from,to};
  }
  throw std::runtime_error("No original body ray for regression");
}
void deathVoiceTail(m35_test::AudioJni &jni,const char *enemyArchive) {
  using namespace m35_test;
  require(activeEvent && r100EventPhase==2 && encounter && roomAcoustics,"Actual death gate has entered the truck EVD");
  const auto banks=readAudio(enemyArchive);const re4::SoundCue *voice=nullptr;unsigned number{},argument{};
  for(const auto &bank:banks)if(bank->block()==8)for(unsigned candidate:bank->randomChoices(24)) {
    const auto *cue=bank->cue(candidate);
    if(cue && cue->sample && !cue->chained() && !cue->pitchLow && !cue->pitchHigh &&
        (!voice || double(cue->sample->pcm.size())/cue->sample->rate>double(voice->sample->pcm.size())/voice->sample->rate))
      {voice=cue;number=candidate;argument=bank->argument();}
  }
  require(voice && double(voice->sample->pcm.size())/voice->sample->rate>2,"Actual normal Ganado death group supplies a long voice tail");
  paused=false;soundMixer.clear();eventMixer.clear();pendingMenuAudio.clear();soundMixer.installBanks(banks);soundMixer.setPaused(false);
  re4::SoundMixer reference;reference.installBanks(banks);reference.setPaused(false);
  const auto source=encounter->soundPosition(re4::EnemySoundChannel::Voice);auto beforeEye=source;beforeEye[2]+=2000;
  const re4::SoundPoint right{1,0,0};
  // Source random selection retries a repeat at most five times. Its first
  // two requests still select the short group member; the third is long.
  // Preserve that metadata and give the reference identical preceding calls.
  for(unsigned prior=0;prior<2;++prior) {
    soundMixer.stop(soundMixer.play(8,number,1,-1,false,argument));
    reference.stop(reference.play(8,number,1,-1,false,argument));
  }
  const auto handle=soundMixer.playSpatial(8,number,*roomAcoustics,source,leon->feet(),beforeEye,right,false,argument);
  const auto expectedHandle=reference.playSpatial(8,number,*roomAcoustics,source,leon->feet(),beforeEye,right,false,argument);
  require(handle && expectedHandle,"Actual death sample starts in production and uninterrupted reference mixers");
  enemyVoiceHandles[unsigned(re4::EnemySoundChannel::Voice)]=handle;
  Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_TRUE);
  Java_com_ast_re4decomp_NativeBridge_subscreenAudio(&jni.env,nullptr,JNI_FALSE);
  bool heard=false;unsigned blocks=0;
  while(soundMixer.voiceActive(handle) && blocks++<400) {
    if(blocks==1 || blocks%32==0)frozenPixels(jni);
    audioGameActive.store(true);syncAudioPause();Pcm actual;std::array<jshort,2048> expected{};
    require(Java_com_ast_re4decomp_NativeBridge_renderAudio(&jni.env,nullptr,reinterpret_cast<jshortArray>(&actual))==1024,
        "Truck handoff renders a complete production PCM block");reference.render(expected.data(),1024);
    require(actual.data==expected,"Every death sample including its tail equals uninterrupted playback through the EVD camera");
    heard|=std::any_of(actual.data.begin(),actual.data.end(),[](auto sample){return sample!=0;});
  }
  require(heard && blocks>90 && !soundMixer.voiceActive(handle) && !reference.voiceActive(expectedHandle),
      "Long original death voice reaches its natural sample end");
  enemyVoiceHandles={};staticAudioProfile.clear();installStaticAudio();soundMixer.installBanks(readRoomAudio(0x100),true);installWeaponAudio();paused=false;
}
}
int main(int argc,char **argv) {
  using namespace m35_test;
  try {
    if(argc!=12)throw std::runtime_error("Usage: r100_render_runtime_check R120 R100 R101 PL00 WEP01 WEP02 WEP07 ROOMINFO CORE EM12 ESL");
    Egl egl;AudioJni jni;jlong session=37200;
    using DebugCallback = void(*)(GLenum,GLenum,GLuint,GLenum,GLsizei,const char*,const void*);
    using InstallDebug = void(*)(DebugCallback,const void*);
    const auto debug=reinterpret_cast<InstallDebug>(eglGetProcAddress("glDebugMessageCallbackKHR"));
    if(debug && std::string(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS))).find("GL_KHR_debug")!=std::string::npos) {
      glEnable(0x92e0);glEnable(0x8242);
      debug([](GLenum,GLenum type,GLuint,GLenum,GLsizei,const char *message,const void*) {
        if(type==0x824c)std::cerr<<"GLES validation: "<<message<<'\n';
      },nullptr);
    }
    Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[9]));
    Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[10]),string(argv[11]));
    Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,string(argv[6]),string(argv[7]));
    const auto create=[&](const char *path,const char *room,bool canceled) {
      Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(&jni.env,nullptr,canceled);
      Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
      Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,JNI_TRUE,0,session);
      Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(path),string(argv[4]),string(argv[5]),string(argv[8]),string(room),session);
      require(javaException.empty() && actorGpuReady && leon,"Real campaign CPU/GPU owners commit");
      controls(jni);frozenPixels(jni);
    };
    const auto captureOpening=[&] {
      if(!std::getenv("RE4_CAPTURE_OPENING_ONLY") || !activeEvent)return;
      const auto frame=activeEvent->cutFrame();
      const bool cabin=r100EventPhase==3 && activeEvent->sourceCut()==6;
      if(frame!=30 && frame!=90 && !(cabin && (frame==150 || frame==210 || frame==258)))return;
      const auto name="r120-phase"+std::to_string(r100EventPhase)+"-cut"+
          std::to_string(activeEvent->sourceCut())+"-frame"+std::to_string(frame)+".ppm";
      saveFrame(jni,name.c_str());
      require(re4::RoomAtmosphere().cinematicFrame &&
          re4::RoomAtmosphere().modelTextureViewport==std::array<float,2>{512,337},
          "Actual opening presents the original active cinematic rows with their native texture gradients");
    };
    if(!std::getenv("RE4_CHECK_TRUCK_HANDOFF_ONLY")) {
    create(argv[1],"r120",false);
    std::cerr<<"Regression: source opening\n";
    for(unsigned frame=0;activeEvent && activeEvent->sourceCut()==0 && frame<240;++frame) {
      captureOpening();advanceR100Event(1./30);
    }
    require(activeEvent && activeEvent->sourceCut()==0,"Original first driving landscape cut");
    paused=true;saveFrame(jni,"r120-cut0.ppm");
    for(unsigned frame=0;activeEvent && activeEvent->sourceCut()==0 && frame<3000;++frame) {
      captureOpening();advanceR100Event(1./30);
    }
    require(activeEvent && activeEvent->sourceCut()==1,"Opening landscape source cut1");
    paused=true;saveFrame(jni,"r120-drive-before.ppm");
    for(unsigned frame=0;activeEvent && activeEvent->sourceCut()<8 && frame<10000;++frame) {
      captureOpening();advanceR100Event(1./30);
    }
    require(activeEvent && activeEvent->sourceCut()==8,"Opening landscape source cut8");
    saveFrame(jni,"r120-cut8.ppm");
    auto mesh=re4::BuildStaticScene(*previewSmd,-1,previewCommon.get());
    re4::AttachSceneTextures(*previewSmd,mesh,previewCommon.get());
    if(previewSmx)re4::ApplySceneSmx(*previewSmx,mesh);
    std::ofstream audit("r120-materials.tsv");audit<<"work\tbin\ttexture\tflags\tmaterial\tambient\tblend\tmeanRGB\n";
    for(const auto &d:mesh.draws) {
      const auto &w=previewSmd->placements.at(d.object);
      const auto *smx=previewSmx?previewSmx->find(d.work):nullptr;
      double mean=0;if(d.texture>=0) {
        const auto &rgba=mesh.textures.at(d.texture).rgba;
        for(std::size_t i=0;i<rgba.size();i+=4)mean+=(rgba[i]+rgba[i+1]+rgba[i+2])/3.;
        if(!rgba.empty())mean/=rgba.size()/4;
      }
      audit<<d.work<<'\t'<<unsigned(w.bin)<<'\t'<<d.image<<'\t'<<(smx?smx->flags:0)<<'\t'<<(smx?smx->material:0)<<'\t'<<d.vertexColors<<'\t'<<d.blendMode<<'\t'<<mean<<'\n';
    }
    if(std::getenv("RE4_CAPTURE_OPENING_ONLY")) {
      std::set<std::pair<unsigned,unsigned>> captured;
      for(unsigned frame=0;activeEvent && !transferPending() && frame<20000;++frame) {
        captureOpening();
        advanceR100Event(1./30);
        if(activeEvent && activeEvent->sourceFrame()>=30 && captured.insert({r100EventPhase,activeEvent->sourceCut()}).second) {
          const auto name="r120-phase"+std::to_string(r100EventPhase)+"-cut"+std::to_string(activeEvent->sourceCut())+".ppm";
          saveFrame(jni,name.c_str());
        }
      }
      return 0;
    }
    }
    ++session;create(argv[2],"r100",true);paused=false;
    require(previewBlocks && previewBlocks->visibleMask(walkProbe->position())==7,
        "Original r100 entry displays blocks0,1,2; distant cached blocks3,4 are hidden");
    saveFrame(jni,"r100-entry.ppm");
    std::cerr<<"Regression: first house\n";
    const auto &area=r100Scenario->firstLookArea().area;
    re4::CollisionVec house{};for(const auto &p:area.points){house[0]+=p[0]/4;house[2]+=p[1]/4;}house[1]=area.floor;
    require(walkProbe->place(house,1600),"Source first-look floor");
    leon->setPlacement(walkProbe->position(),1.75f);ticks(jni,2);
    require(activeEvent && r100EventPhase==1,"Actual house event");
    activeEvent->finish();finishR100Event();
    std::cerr<<"Regression: first enemy\n";
    for(unsigned i=0;i<20 && encounter->alive();++i) {
      re4::PlayerMeleeEvent hit;hit.serial=100+i;hit.rays.push_back(bodyRay(*encounter));
      encounter->queueMelee(hit,leon->feet());ticks(jni,21);
    }
    for(unsigned i=0;i<150 && !activeEvent;++i)ticks(jni,1);
    require(activeEvent && r100EventPhase==2,"Original death gate starts truck");
    deathVoiceTail(jni,argv[10]);
    std::cerr<<"Regression: truck to radio\n";
    bool sawTerminalPlayerPose=false;
    for(unsigned frame=0;activeEvent && frame<20000;++frame) {
      for(const auto &actor:activeEvent->actors()) if(!actor->attachment() && actor->rig()) {
        require(actor->rig()->sequenceFrame()==actor->motionFrame(),
            "Every visible event FCV keeps its requested absolute frame, including the final key");
        if(activeEvent->sourceFrame()+1==activeEvent->totalFrames() &&
            actor->name()=="pl0000" && actor->visible() && actor->motionFrame()>0)
          sawTerminalPlayerPose=true;
      }
      if(std::getenv("RE4_CAPTURE_WINDOW_TAIL") && r100EventPhase==2 &&
          activeEvent->sourceFrame()+45>=activeEvent->data().totalFrames()) {
        const auto name="r100-window-tail-"+std::to_string(activeEvent->sourceFrame())+".ppm";
        saveFrame(jni,name.c_str());
      }
      if(activeEvent->sourceCut()+1==activeEvent->data().cutFrames().size() &&
          activeEvent->cutFrame()+2==activeEvent->data().cutFrames().back())saveFrame(jni,"r100-truck-last.ppm");
      advanceR100Event(1./30);
    }
    require(sawTerminalPlayerPose,"Natural truck event actually displays Leon in its final source frame");
    require(!activeEvent && r100EventPhase==12 && eventUiActive.load(),"Truck hands directly to radio without a room frame");
    require(r100Secondary->radioIntroFrame()==1 && leon->rig().status().sequenceFrame==1,
        "First visible radio pose includes the original hidden SceSleep1 tick");
    // A fresh source ARC0x79 motion is independent of the truck's posed
    // actor and of the scenario handoff. Re-evaluating MotionMove(0) on the
    // live rig is not a refresh: it overwrites the eye update from cPlayer.
    re4::LeonPreview sourceRadio(argv[4],argv[5]);
    re4::WalkProbe sourceProbe(*collisionWorld,walkProbe->config());
    require(sourceProbe.place(re4::R100EntryPlayerPosition()),"Independent radio reference has a validated original room floor");
    const auto sourceArc=re4::RoomArchive::LoadDrs(argv[4]);
    const auto radioMotion=sourceRadio.installRoomScriptMotion({sourceArc.copy(0x79-4),{}});
    sourceRadio.setRoomScriptEyeMode(1);
    sourceRadio.beginRoomScriptMotion(sourceProbe,{-81500,860,-38900},1.6f,radioMotion,0,0x201);
    sourceRadio.step(sourceProbe,0,0,false,false,1./30);
    require(leon->rig().jointMatrices()==sourceRadio.rig().jointMatrices(),
        "First displayed radio pose equals the fresh source FCV with its hidden tick and eye mode");
    frozenPixels(jni);
    saveFrame(jni,"r100-radio-first.ppm");
    for(unsigned i=0;i<200 && r100EventPhase!=13;++i)ticks(jni,1);
    require(r100RadioTerm && r100EventPhase==13,"Posttruck radio prepared");
    ticks(jni,35);controls(jni,re4::Start);frozenPixels(jni);controls(jni);ticks(jni,2);
    require(!r100RadioTerm && r100EventPhase==0 && !paused,"Radio returns to playable room");
    if(std::getenv("RE4_CHECK_TRUCK_HANDOFF_ONLY")) {
      std::cout<<"Original natural truck/radio handoff and complete death PCM PASS: "<<checks<<" checks\n";return 0;
    }
    require(walkProbe->place({-72262.2,158.2,-25858.3},1600),"Reported failing position has source floor");
    leon->setPlacement(walkProbe->position(),1.6f);re4::SceneFollowHeading(leon->heading());
    re4::SceneSeedFollowView(0,0);ticks(jni,2);
    for(unsigned index:{3u,4u,5u}) {
      std::cerr<<"Regression: outdoor enemy "<<index<<'\n';
      auto *enemy=enemyGroup->find(index);require(enemy && enemy->alive(),"Original outdoor ambush actor");
      for(unsigned attempt=0;attempt<30 && enemy->alive();++attempt) {
        re4::PlayerMeleeEvent hit;hit.serial=1000+index*100+attempt;hit.rays.push_back(bodyRay(*enemy));
        enemy->queueMelee(hit,leon->feet());ticks(jni,21);
        require(!paused && actorGpuReady && glGetError()==GL_NO_ERROR,"Combat/drop render remains live");
      }
      require(!enemy->alive(),"Authored damage kills outdoor actor");ticks(jni,90);
      require(!paused && actorGpuReady && glGetError()==GL_NO_ERROR,"Death/drop geometry rebuild has no GPU error");
    }
    for(unsigned i=0;i<4;++i) {
      const auto before=re4::SaveSceneView();campaignSceneDirty=true;frozenPixels(jni);
      require(!paused && actorGpuReady && glGetError()==GL_NO_ERROR,"Repeated live effect/room rebuild");
      require(re4::SaveSceneView().followCamera.yawRatioRadians==before.followCamera.yawRatioRadians,"Rebuild preserves camera");
    }
    paused=true;saveFrame(jni,"r100-three-dead.ppm");paused=false;
    require(dogTraps && dogTraps->released(),"Original posttruck traps exist");
    re4::DogTrapHit wire;wire.actor=1;
    require(dogTraps->applyShot(wire,50001,1),"Actual firearm trap hit accepted");
    for(unsigned i=0;i<240;++i) {
      ticks(jni,1);
      require(!paused && actorGpuReady && glGetError()==GL_NO_ERROR,"Trap explosion keeps simulation and GPU running");
    }
    require(!dogTraps->actors()[1]->visible(),"Original hit wire detonates and is removed");
    std::cout<<"Original r120 landscape / r100 combat, trap and death-audio runtime PASS: "<<checks<<" checks\n";
  }catch(const std::exception &e){std::cerr<<"Render regression failure: "<<e.what()<<'\n';return 1;}
}
