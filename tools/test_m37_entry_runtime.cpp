// Actual production opening→room transaction→entry flow and cancel paths.
#define RE4_M36_ENTRY previous_m37_fight_runtime
#include "test_m36_runtime.cpp"
#undef RE4_M36_ENTRY
int main(int argc,char **argv) {
 using namespace m35_test;
 try {
  if(argc!=12)throw std::runtime_error("Usage: m37_entry_runtime_check R120 R100 R101 PL00 WEP01 WEP02 WEP07 roominfo CORE EM12 ESL");
  Egl egl;AudioJni jni;jlong session=37100;
  Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[9]));
  Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[10]),string(argv[11]));
  Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,string(argv[6]),string(argv[7]));
  const auto create=[&](const char *path,const char *name,bool canceled) {
   Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(&jni.env,nullptr,canceled?JNI_TRUE:JNI_FALSE);
   Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
   Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,JNI_TRUE,0,session);
   Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(path),string(argv[4]),string(argv[5]),string(argv[8]),string(name),session);
   require(javaException.empty() && leon && actorGpuReady && walking,"Original room/player CPU/GPU resources commit");
   frozenPixels(jni);controls(jni);
  };
  create(argv[1],"r120",false);
  require(activeRoomId==0x120 && activeEvent && r100EventPhase==3,"Unskipped FMV enters original r120s00");
  auto *player=leon.get();const auto playerFrame=leon->rig().sequenceFrame();
  const auto totalDrive=activeEvent->totalFrames();
  for(unsigned i=0;i<totalDrive && r100EventPhase==3;++i)advanceR100Event(1./30);
  require(activeEvent && r100EventPhase==4 && activeEvent->data().name()=="r120s01","Natural driving completion starts source exit-car event");
  require(leon.get()==player && leon->rig().sequenceFrame()==playerFrame,"Opening event owns motion without advancing playableLeon");
  for(unsigned i=0;i<100;++i)advanceR100Event(1./30);
  const auto eventFrame=activeEvent->sourceFrame();auto *event=activeEvent;
  paused=true;
  const auto pausedPixels=frozenPixels(jni);ticks(jni,3);
  require(activeEvent->sourceFrame()==eventFrame,"Background pause freezes original opening frame");
  egl.fresh();create(argv[1],"r120",false);
  require(activeEvent==event && pixelChanges(pausedPixels,frozenPixels(jni))==0,"EGL loss restores identical source opening state/framebuffer");
  paused=false;controls(jni);
  for(unsigned i=0;i<activeEvent->totalFrames()+1 && !transferPending();++i)advanceR100Event(1./30);
  require(transferPending() && activeEvent==event && activeRoomId==0x120,"Finished source exit-car frame survives while r100 is prepared");
  finish(jni,"r100",argv[2]);
  require(activeRoomId==0x100 && activeEvent && r100EventPhase==5 && activeEvent->data().name()=="r100s40","Transactional real loader starts gate/officer source event");
  require(scenarioState.room(0x100).room.test(13),"R100StartEvent writes original once-onlyRSF13");
  while(activeEvent)advanceR100Event(1./30);
  require(!activeEvent && !re4::OriginalOpeningCanceled(scenarioState) && leon.get()==player,"Natural full opening reaches mortal gameplay without cancellation");
  require(std::hypot(leon->feet()[0]+99685,leon->feet()[2]+1343)<.01 && std::abs(leon->heading()-2.246f)<.00001,"Original playable gate position/heading after s40");
  require(forestAnimals && dogTraps && !dogTraps->released(),"Initial crows are live while source trap set remains unreleased");
  ++session;create(argv[2],"r100",true);
  require(!activeEvent && re4::OriginalOpeningCanceled(scenarioState) && scenarioState.room(0x100).room.test(13),"FMV skip suppresses all three engine entries and still commits source playable gate");
  ++session;create(argv[1],"r120",false);
  controls(jni,re4::Start);frozenPixels(jni);controls(jni);
  require(transferPending() && re4::OriginalOpeningCanceled(scenarioState),"Skipping driving sets originalSCF59 and requestsr100");
  finish(jni,"r100",argv[2]);
  require(!activeEvent && activeRoomId==0x100,"Driving skip suppresses both exit-car and gate events");
  std::cout<<"M37 actual JNI opening/skip/room transaction/pause/EGL PASS: "<<checks<<" checks\n";
 }catch(const std::exception &e){std::cerr<<"M37 entry runtime failure: "<<e.what()<<'\n';return 1;}
}
