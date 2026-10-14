// New ETS object ladders through actual JNI A input and GLES lifecycle.
#define RE4_M27_ENTRY previous_m27_main
#include "test_m27_runtime.cpp"
#undef RE4_M27_ENTRY
int main(int argc,char **argv) {
  using namespace m27_test;
  try {
    if(argc!=8)throw std::runtime_error("Usage: m32_ladder_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00");
    Egl egl;Jni jni;const jlong session=32321;
    Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[5]));
    Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[6]),string(argv[7]));
    const auto create=[&] {
      Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
      Java_com_ast_re4decomp_NativeBridge_configureTestProfile(&jni.env,nullptr,1,session);
      Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(argv[1]),string(argv[2]),
          string(argv[3]),string(argv[4]),string("r101"),session);
      frozenPixels(jni);
      require(javaException.empty()&&actorGpuReady&&roomLadders&&roomInteractions,
              "Actual village-test loader creates both interior and object-ladder owners");
    };
    create();unpause(jni);
    require(walking&&!encounter&&roomLadders->objectLadders().size()==3&&
            scenarioState.room(0x101).room.test(8),
            "Explicit no-enemy post-house village enables original three ladder slots");
    const auto placements=roomLadders->objectLadders();
    for(const auto &placement:placements) {
      bool found=false;
      for(float z:{600.f,800.f,1000.f}) {
        if(!walkProbe->place(point(placement,0,0,z)))continue;
        leon->setPlacement(walkProbe->position(),placement.rotation[1]+3.1415927f);
        const auto offer=roomLadders->offer(*leon,*walkProbe);
        if(offer&&offer->object&&offer->no==placement.slot){found=true;break;}
      }
      require(found,"Shared interior/ladder SAT provides each original approach");
      re4::SceneFollowHeading(leon->heading());re4::SceneSeedFollowView(0,0);
      controls(jni);frozenPixels(jni);controls(jni,re4::Bottom);tick(jni);controls(jni);
      require(leon->objectLadderActive(),"Physical A/Kreuz starts the authored ETS ladder climb");
      ticks(jni,20);pause(jni);
      auto *owner=roomLadders.get();auto *interior=roomInteractions.get();auto *actor=leon.get();
      const auto feet=leon->feet();const auto bones=leon->rig().bones();
      const auto motion=leon->objectLadderState().ticks;
      const auto image=frozenPixels(jni);
      calls.clear();ticks(jni,3);
      require(leon->objectLadderState().ticks==motion&&leon->feet()==feet&&
              leon->rig().bones()==bones,"Pause freezes object ladder tick and posed bones");
      egl.fresh();create();
      require(paused&&roomLadders.get()==owner&&roomInteractions.get()==interior&&leon.get()==actor&&
              leon->objectLadderState().ticks==motion&&leon->feet()==feet&&leon->rig().bones()==bones&&
              pixelChanges(image,frozenPixels(jni))==0,
              "EGL recreation preserves object ladder owner, pose, shared collision and exact pixels");
      unpause(jni);controls(jni);
      unsigned remaining=0;
      while(leon->ladderActive()&&remaining++<400){calls.clear();tick(jni);}
      require(!leon->ladderActive()&&!leon->ladderLandingFailed()&&leon->feet()[1]>2000&&
              leon->life()==1200&&!encounter,
              "Resumed production player dismounts onto real roof with unlimited HP and no enemy");
    }
    std::cout<<"M32 production ETS ladders JNI/GLES PASS: "<<checks<<" checks\n";
    return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
