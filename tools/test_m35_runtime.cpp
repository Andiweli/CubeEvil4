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
    if(argc!=11)throw std::runtime_error("Usage: m35_runtime_check R100 R101 PL00 WEP01 WEP02 WEP07 roominfo CORE EM12 emleon00");
    Egl egl;AudioJni jni;jlong session=35001;
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
    const auto create=[&](const char *path,const char *name,bool campaign) {
      Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
      Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,campaign?JNI_TRUE:JNI_FALSE,0,session);
      Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(path),string(argv[3]),string(argv[4]),string(argv[7]),string(name),session);
      frozenPixels(jni);unpause(jni);controls(jni);
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
    require(activeEvent && r100EventPhase==1 && !encounter,"Authored first-look now starts the actual EVD event");
    controls(jni,re4::Start);frozenPixels(jni);controls(jni);frozenPixels(jni);
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
    ++session;create(argv[2],"r101",false);
    require(!campaignMode && leon->unlimitedHealth() && !encounter,
            "Developer explore room remains separate from mortal campaign state");
    require(roomFootsteps && roomInteractions && roomLadders,"Production room owns FSE and reciprocal live-pane/ladder collision");
    std::cout<<"M35 real title JNI/audio, campaign loadout/AEV/axe rendering, pause/EGL and explore isolation PASS: "<<checks<<" checks\n";
  } catch(const std::exception &e) { std::cerr<<"M35 runtime failure: "<<e.what()<<'\n';return 1; }
}
