// Production JNI/world/GLES integration with the supplied original disc.
#define RE4_M27_ENTRY previous_m35_m27_main
#include "test_m27_runtime.cpp"
#undef RE4_M27_ENTRY
#include <filesystem>

namespace m35_test {
using namespace m27_test;
struct Pcm { std::array<jshort,2048> data{}; };
jsize JNICALL length(JNIEnv *,jarray) { return 2048; }
jshort *JNICALL pcm(JNIEnv *,jshortArray a,jboolean *) { return reinterpret_cast<Pcm *>(a)->data.data(); }
void JNICALL commit(JNIEnv *,jshortArray,jshort *,jint mode) { require(mode==0,"PCM array commits"); }
struct AudioJni:Jni {
  AudioJni() {table.GetArrayLength=length;table.GetShortArrayElements=pcm;table.ReleaseShortArrayElements=commit;}
};
}
int main(int argc,char **argv) {
  using namespace m35_test;
  try {
    if(argc!=11)throw std::runtime_error("Usage: m37_runtime_check R100 R101 PL00 WEP01 WEP02 WEP07 roominfo CORE EM12 emleon00");
    Egl egl;AudioJni jni;jlong session=37001;
    const auto root=std::filesystem::path(argv[3]).parent_path().parent_path().string();
    Java_com_ast_re4decomp_NativeBridge_startupConfigure(&jni.env,nullptr,string(root.c_str()),1);
    if(!javaException.empty())throw std::runtime_error("Title configure: "+javaException);
    require(javaException.empty() && startupArt,"Real original title loads through production JNI");
    for(unsigned group:{6u,7u,8u,9u,10u,11u,13u}) {
      const auto data=text(Java_com_ast_re4decomp_NativeBridge_startupLayout(&jni.env,nullptr,jint(group)));
      require(data.find("\"units\":[")!=std::string::npos,"Every original title group exposes authored units");
    }
    Java_com_ast_re4decomp_NativeBridge_startupAudio(&jni.env,nullptr,JNI_TRUE);
    Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_TRUE);
    Java_com_ast_re4decomp_NativeBridge_startupCue(&jni.env,nullptr,6,4);
    bool audible=false;
    for(unsigned n=0;n<8;++n) {
      Pcm output;
      require(Java_com_ast_re4decomp_NativeBridge_renderAudio(&jni.env,nullptr,reinterpret_cast<jshortArray>(&output))==1024,"Title renders bounded real stereo PCM");
      audible=audible||std::any_of(output.data.begin(),output.data.end(),[](auto sample){return sample!=0;});
    }
    require(audible,"Original title cue6/4 is audible through actual JNI/audio gate");
    Java_com_ast_re4decomp_NativeBridge_startupAudio(&jni.env,nullptr,JNI_FALSE);
    Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[8]));
    Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[9]),string(argv[10]));
    Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,string(argv[5]),string(argv[6]));
    Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(&jni.env,nullptr,JNI_TRUE);
    const auto create=[&](const char *path,const char *name,bool campaign) {
      Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
      Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,campaign?JNI_TRUE:JNI_FALSE,0,session);
      Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(path),string(argv[3]),string(argv[4]),string(argv[7]),string(name),session);
      frozenPixels(jni);if(!activeEvent)unpause(jni);controls(jni);
      require(javaException.empty()&&walking&&actorGpuReady&&leon,"Production scene remains playable with full original resources");
    };
    create(argv[1],"r100",true);
    require(campaignMode && !leon->unlimitedHealth() && leon->life()==1200 && !encounter && r100Scenario,
            "Title campaign starts mortal Leon and no invented enemy before authored trigger");
    require(inventoryCase.equippedId()==0x23 && leon->magazine()==10 && leon->reserve()==20,
            "Campaign uses original new-game handgun and packed ammo");
    const auto &area=r100Scenario->firstLookArea().area;
    re4::CollisionVec center{};
    for(const auto &p:area.points){center[0]+=p[0]/4;center[2]+=p[1]/4;}
    center[1]=area.floor;
    require(walkProbe->place(center,1600),"Authored first-look area has genuine SAT floor");
    leon->setPlacement(walkProbe->position(),1.75f);controls(jni);ticks(jni,2);
    require(activeEvent && r100EventPhase==1 && !encounter,"Actual s03 starts before first combat actor becomes active");
    require(enemyGroup && enemyGroup->actors().size()==10,"All ten authored ambush CPU actors are prepared once");
    for(const auto &actor:enemyGroup->actors())require(!actor->visible()&&actor->scriptSuspended(),"Future room actors do not appear before s20");
    require(Java_com_ast_re4decomp_NativeBridge_eventActive(&jni.env,nullptr)==JNI_TRUE,"Nonblocking event UI mailbox exposes the source event");
    bool speechAudible=false;
    for(unsigned block=0;block<80;++block) {
      Pcm output;Java_com_ast_re4decomp_NativeBridge_renderAudio(&jni.env,nullptr,reinterpret_cast<jshortArray>(&output));
      speechAudible=speechAudible||std::any_of(output.data.begin(),output.data.end(),[](auto sample){return sample!=0;});
    }
    require(speechAudible && eventMixer.streamRenderedFrames()>0,"Actual EVD voice renders through independent native event mixer");
    auto *event=activeEvent;const auto eventFrame=event->sourceFrame();
    const auto playerFrame=leon->rig().sequenceFrame();const auto playerFeet=leon->feet();
    const auto eventPixels=frozenPixels(jni);ticks(jni,9);
    require(activeEvent==event && event->sourceFrame()>eventFrame,"Actual bridge advances the EVD source clock");
    require(leon->feet()==playerFeet && leon->rig().sequenceFrame()==playerFrame,"Ordinary Leon is frozen while original event actors animate");
    require(pixelChanges(eventPixels,frozenPixels(jni))>100,"Original event produces visible moving pixels instead of a black/loading screen");
    paused=true;suspendSimulationInput();const auto frozenFrame=event->sourceFrame();
    const auto freeze=frozenPixels(jni);ticks(jni,3);
    const auto speechFrame=eventMixer.streamRenderedFrames();Pcm muted;
    Java_com_ast_re4decomp_NativeBridge_renderAudio(&jni.env,nullptr,reinterpret_cast<jshortArray>(&muted));
    require(eventMixer.streamRenderedFrames()==speechFrame && std::all_of(muted.data.begin(),muted.data.end(),[](auto sample){return sample==0;}),"Pause freezes and mutes both native event and room streams");
    require(event->sourceFrame()==frozenFrame,"Pause preserves original event frame");
    egl.fresh();create(argv[1],"r100",true);
    require(activeEvent==event && event->sourceFrame()==frozenFrame,"EGL recovery preserves event owner and source frame");
    require(pixelChanges(freeze,frozenPixels(jni))==0,"Paused cutscene framebuffer recovers exactly after EGL loss");
    paused=false;controls(jni);event->finish();finishR100Event();frozenPixels(jni);

    require(Java_com_ast_re4decomp_NativeBridge_eventActive(&jni.env,nullptr)==JNI_FALSE && soundMixer.hasMusic(),"s03 completion clears UI event gate and preserves original combat music");
    require(encounter && encounter->firstHouseEncounter() && encounter->maxHp()==500 && scenarioState.room(0x100).room.test(3),
            "Actual AEV crossing activates the original event-sentinel first Ganado with500HP");
    require(actorGpuReady && glGetError()==GL_NO_ERROR,"First Ganado and mirrored original axe upload/render cleanly");
    storeInteractionState();
    require(scenarioState.room(0x100).room.test(3),"Interior ETC synchronization preserves live campaign RSF flags");
    auto *enemy=encounter.get();auto *player=leon.get();auto *script=r100Scenario.get();
    pause(jni);const auto hp=leon->life();const auto enemyHp=encounter->hp();
    const auto pixels=frozenPixels(jni);ticks(jni,3);
    require(leon->life()==hp && encounter->hp()==enemyHp,"Paused campaign does not advance combat");
    egl.fresh();create(argv[1],"r100",true);
    require(encounter.get()==enemy && leon.get()==player && r100Scenario.get()==script,
            "EGL recovery preserves first-encounter owners without duplicate spawn");
    pause(jni);require(pixelChanges(pixels,frozenPixels(jni))==0,"Paused original encounter has exact EGL framebuffer recovery");
    unpause(jni);controls(jni);
    leon->setUnlimitedHealth(true);
    // Reproduce the reported user control path with a genuine handgun miss.
    const auto playerOwner=leon.get();const auto enemyOwner=encounter.get();
    const auto shotsBefore=leon->shots(),ammoBefore=leon->magazine();
    const auto previousHeading=leon->heading();
    leon->setPlacement(walkProbe->position(),previousHeading+3.1415927f);
    re4::SceneSeedFollowView(0,0);controls(jni,0,1,0);ticks(jni,8);
    require(leon->laserReady(),"Actual LT controller input completes original handgun draw");
    controls(jni,0,1,1);ticks(jni,3);
    require(leon.get()==playerOwner && encounter.get()==enemyOwner && actorGpuReady && !paused,
      "Actual missed gunshot keeps Leon, combat actor, GPU and simulation");
    require(leon->shots()==shotsBefore+1 && leon->magazine()==ammoBefore-1,
      "One controller press fires exactly one real shot and consumes one round");
    controls(jni);ticks(jni,35);
    require(leon->action()==re4::PlayerAction::Footwork,"Leon regains ordinary controls after holstering");
    leon->setPlacement(walkProbe->position(),previousHeading);re4::SceneFollowHeading(previousHeading);
    const auto bodyRay=[&] {
      const auto shapes=re4::NormalGanadoHitShapes();const auto &body=shapes.at(1);
      auto offset=body.offset;offset[1]+=body.height*.5f;
      const auto center=re4::TransformPoint(re4::WeaponAttachmentMatrix(encounter->actor().rig().jointMatrices().at(unsigned(body.partsNo-1)),encounter->placement()),offset);
      for(unsigned axis:{0u,2u,1u})for(double side:{1.,-1.}) {
        re4::CollisionVec from{center[0],center[1],center[2]},to=from;from[axis]-=side*800;to[axis]+=side*800;
        const auto hit=re4::HitAuthoredShapes(shapes,encounter->actor().rig().jointMatrices(),encounter->placement(),from,to,2560000,0x10);
        if(hit && shapes.at(hit->partIndex).partsNo!=5)return std::make_pair(from,to);
      }
      throw std::runtime_error("No authored first-enemy body ray");
    };
    for(unsigned attempt=0;attempt<20 && encounter->alive();++attempt) {
      re4::PlayerMeleeEvent knife;knife.serial=100+attempt;knife.rays.push_back(bodyRay());
      encounter->queueMelee(knife,leon->feet());ticks(jni,21);
    }
    require(!encounter->alive(),"Real original body damage kills first500HP enemy");
    for(unsigned i=0;i<150 && !activeEvent;++i)ticks(jni,1);
    require(activeEvent && r100EventPhase==2 && !scenarioState.room(0x100).room.test(10),"Original ACTIVE-clear death gate starts s20 before posttruck flags");
    const auto alivePlayerFrame=leon->rig().sequenceFrame();
    while(activeEvent && activeEvent->sourceCut()<2)advanceR100Event(1./30);
    require(activeEvent && activeEvent->sourceCut()==2,"Actual truck event reaches original live-Ganado callback cut2");
    for(unsigned index:re4::R100S20LiveActorIndices)require(enemyGroup->find(index)&&!enemyGroup->find(index)->scriptSuspended(),"Only source cut2 actors are unsuspended");
    require(leon->rig().sequenceFrame()==alivePlayerFrame,"Truck event does not tick player");
    frozenPixels(jni);controls(jni,re4::Start);frozenPixels(jni);controls(jni);frozenPixels(jni);
    require(!activeEvent && scenarioState.room(0x100).room.test(10)&&scenarioState.scenario.test(17),"Skip performs original s20 post-event flag transaction");
    for(const auto &actor:enemyGroup->actors())require(actor->visible()&&!actor->scriptSuspended(),"All actual ambush actors remain visible/playable after truck");
    require(forestAnimals && forestAnimals->animals().size()==5 && dogTraps && dogTraps->released(),
      "Original five crows, rescuedog and authored trap set coexist after truck");
    for(unsigned n=0;n<160 && r100EventPhase!=13;++n)ticks(jni,1);
    require(r100EventPhase==13 && r100RadioTerm,"Source post-truck radio runs before ordinary combat resumes");
    ticks(jni,35);controls(jni,re4::Start);frozenPixels(jni);controls(jni);ticks(jni,2);
    require(r100EventPhase==0 && !r100RadioTerm && (r100RadioHeard[0]&0x40000000u),
      "START closes the actual second radio call and preserves its original heard bit");
    const auto forestTicks=forestAnimals->ticks();
    const auto worldTicks=enemyGroup->worldTicks();ticks(jni,2);
    require(forestAnimals->ticks()==forestTicks+2,"Crows advance once per original30Hz worldtick");
    require(enemyGroup->worldTicks()==worldTicks+2 && glGetError()==GL_NO_ERROR,"Group ticks once per source world frame and renders cleanly");
    const auto afterGroupOwner=enemyGroup.get();const auto groupShots=leon->shots();
    controls(jni,0,1,0);ticks(jni,8);controls(jni,0,1,1);ticks(jni,3);
    require(leon && actorGpuReady && enemyGroup.get()==afterGroupOwner && !paused && leon->shots()==groupShots+1,
      "A group-mode gunshot with no target also preserves the runtime");
    controls(jni);ticks(jni,35);
    ++session;create(argv[2],"r101",false);
    require(!campaignMode && leon->unlimitedHealth() && !encounter,
            "Developer explore room remains separate from mortal campaign state");
    require(roomFootsteps && roomInteractions && roomLadders,"Production room owns FSE and reciprocal live-pane/ladder collision");
    std::cout<<"M37 real title JNI/audio, campaign loadout/AEV/axe rendering, pause/EGL and explore isolation PASS: "<<checks<<" checks\n";
  } catch(const std::exception &e) { std::cerr<<"M37 runtime failure: "<<e.what()<<'\n';return 1; }
}
