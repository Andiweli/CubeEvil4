// Production JNI/GLES optional r100 scenes, persistent source models and
// transient original house event, including pause and EGL restoration.
#define RE4_M36_ENTRY previous_m37_secondary_fight_runtime
#include "test_m36_runtime.cpp"
#undef RE4_M36_ENTRY
#include "room_shadows_gl.h"
int main(int argc,char **argv) {
 using namespace m35_test;
 try {
  if(argc!=11)throw std::runtime_error("Usage: m37_secondary_runtime_check R100 R101 PL00 WEP01 WEP02 WEP07 roominfo CORE EM12 ESL");
  Egl egl;AudioJni jni;const jlong session=37170;
  Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[8]));
  Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[9]),string(argv[10]));
  Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,string(argv[5]),string(argv[6]));
  Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(&jni.env,nullptr,JNI_TRUE);
  const auto create=[&] {
   Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
   Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,JNI_TRUE,0,session);
   Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(argv[1]),string(argv[3]),string(argv[4]),string(argv[7]),string("r100"),session);
   require(javaException.empty() && leon && actorGpuReady && walking && r100Secondary,"Actual secondary room resources commit through production loader");
   frozenPixels(jni);controls(jni);
  };
  create();unpause(jni);
  require(r100Secondary->actors().size()==8 && r100HouseEnemy,"Original cars/cop/addModels/hanging resources and transient ESL25 prepared");
  const auto before=frozenPixels(jni);require(std::count_if(before.begin(),before.end(),[](auto v){return v>20;})>100,"Persistent original room is visibly rendered");
  for(unsigned phase:{6u,7u,8u}) {
   re4::R100SecondaryOffer offer;offer.area=phase==6?0x19:phase==7?0x1a:0x1b;offer.phase=phase;
   if(phase==8)offer.playerPosition=re4::CollisionVec{-112251,-193,-4420};
   beginR100Secondary(offer);require(activeEvent && r100EventPhase==phase && eventUiActive,"Production optional event receives exact prepared source EVD");
   const auto f=activeEvent->sourceFrame();advanceR100Event(1./30);require(activeEvent->sourceFrame()>f,"Optional source timeline advances natively");
   paused=true;suspendSimulationInput();const auto pixels=frozenPixels(jni);const auto clock=activeEvent->sourceFrame();ticks(jni,2);
   require(activeEvent->sourceFrame()==clock,"Optional source EVD pauses independent of rendering");
   egl.fresh();create();const auto recovered=frozenPixels(jni);const auto changed=pixelChanges(pixels,recovered);
   if(changed) {
    std::cerr<<"Optional EGL mismatch phase="<<phase<<" frame="<<clock<<" source="<<(activeEvent?activeEvent->sourceFrame():~0u)<<" pixels="<<changed<<'\n';
    for(unsigned image=0;image<2;++image) {
     const auto &data=image?recovered:pixels;
     std::ofstream out("/tmp/m37-secondary-egl-"+std::to_string(phase)+(image?"-after.ppm":"-before.ppm"),std::ios::binary);
     out<<"P6\n960 540\n255\n";
     for(unsigned y=0;y<540;++y)for(unsigned x=0;x<960;++x)out.write(reinterpret_cast<const char*>(data.data()+((539-y)*960+x)*4),3);
    }
   }
   require(activeEvent && activeEvent->sourceFrame()==clock && changed==0,"Optional EVD frame/actors restore exactly after EGL loss");
   paused=false;controls(jni);while(activeEvent)advanceR100Event(1./30);
   require(leon && actorGpuReady && !eventUiActive && re4::OriginalOpeningCanceled(scenarioState),"Optional completion retains playable player and opening cancellation value");
  }
  re4::R100SecondaryOffer house;house.area=0x15;house.kind=re4::R100SecondaryOffer::HouseScare;house.cameraCut=10;
  beginR100Secondary(house);require(r100EventPhase==10 && r100Secondary->cameraActive() && r100HouseEnemy->r100HouseScare() && leon->roomScriptActive(),"Production house event uses original CAM10, ESL25 AI and real player motion");
  require(previewShadows && previewShadows->receivers && !previewShadows->vertices.empty(),"Original SHD house receivers are loaded outside ordinary room geometry");
  require(r100Secondary->cameraCut()==10 && eventMixer.streamPlaying(1,0x22),"Source CAM10 selects its linked lighting and starts original house sound stream");
  advanceR100RoomScript(1./30,false);const auto housePixels=frozenPixels(jni);const auto cameraFrame=r100Secondary->cameraFrame();
  paused=true;ticks(jni,3);require(r100Secondary->cameraFrame()==cameraFrame,"House raw camera is frozen while paused");
  egl.fresh();create();require(r100EventPhase==10 && pixelChanges(housePixels,frozenPixels(jni))==0,"Hidden-player source house scene restores exact framebuffer after context loss");
  paused=false;
  for(unsigned n=0;n<65 && r100EventPhase==10;++n)advanceR100RoomScript(1./30,false);
  require(r100EventPhase==10,"Original house camera remains active at authored Ganado reveal");
  const auto shadowed=frozenPixels(jni);re4::DestroyRoomShadowsGl();const auto unshadowed=frozenPixels(jni);
  const auto shadowPixels=pixelChanges(shadowed,unshadowed);std::cout<<"House original shadow pixel delta: "<<shadowPixels<<'\n';
  require(shadowPixels>0,"Actual original Ganado projects a visible source-light shadow on SHD house/window receivers");
  re4::CreateRoomShadowsGl(previewShadows);require(frozenPixels(jni)==shadowed,"Recreating house shadow resources restores exact pixels");
  while(r100EventPhase==10)advanceR100RoomScript(1./30,false);
  require(!r100HouseEnemy && !leon->roomScriptActive() && !r100Secondary->cameraActive() && scenarioState.room(0x100).room.test(15),"House destroys temporary actor and restores player/camera with source once flag");
  re4::R100SecondaryOffer text;text.kind=re4::R100SecondaryOffer::RoomMessage;text.area=0x10;text.message=0x11;
  beginR100Secondary(text);require(r100EventPhase==11 && r100RoomMessage,"Production plain MDT is native controlled text rather than a placeholder");
  for(unsigned frame=0;frame<400 && r100EventPhase==11;++frame)advanceR100RoomScript(1./30,frame%3==0);
  require(r100EventPhase==0 && !r100RoomMessage && leon && actorGpuReady,"Native source MDT pages/input waits restore gameplay after final code01");
  scenarioState.room(0x100).room.set(10);r100Secondary->setAfterTruck(true);
  re4::R100SecondaryOffer ravine;ravine.area=1;ravine.phase=9;ravine.preservePlayerPosition=true;
  const auto feet=leon->feet();beginR100Secondary(ravine);require(activeEvent && activeEvent->data().name()=="r100s30","Post-truck first ravine action uses original s30");
  while(activeEvent)advanceR100Event(1./30);
  require(leon->feet()==feet && scenarioState.room(0x100).room.test(14),"Original ravine completion restores source pre-event position");
  for(unsigned call:{0u,1u}) {
   const auto position=leon->feet();const auto heading=leon->heading();const auto grip=leon->leftHandType();
   beginR100Radio(call);require(r100EventPhase==12 && leon->roomScriptActive() && leon->eyeMode()==1 && r100RadioMessageNo==call && (r100RadioHeard[0]&(0x80000000u>>call)),"Native radio handoff selects source heard metadata and real Leon intro");
   for(unsigned f=0;f<22;++f)advanceR100RoomScript(1./30,false);
   require(r100Secondary->radioHandVisible() && leon->leftHandType()==2,"Production radio appears in the real left hand at original tick22");
   paused=true;const auto hand=frozenPixels(jni);const auto motionFrame=r100Secondary->radioIntroFrame();
   egl.fresh();create();require(r100EventPhase==12 && r100Secondary->radioIntroFrame()==motionFrame && pixelChanges(hand,frozenPixels(jni))==0,"Native radio hand/model/intro restore exactly after EGL loss");
   paused=false;while(r100EventPhase==12)advanceR100RoomScript(1./30,false);
   require(r100EventPhase==13 && r100RadioTerm && r100RadioTerm->actors().size()==11,"Source completed radio intro opens fully authored ss_term UI and11original actors");
   for(unsigned f=0;f<40;++f)advanceR100RoomScript(1./30,false);
   const auto stream=0x8c+call;require(eventMixer.streamPlaying(1,stream),"Source radio voice starts after native request/readiness wait");
   require(eventMixer.streamStopping(1,3),"Original OpeMesMove stops intro jingle on first voice request");
   paused=true;const auto screen=frozenPixels(jni);const auto frame=r100RadioTerm->frame();
   ticks(jni,2);egl.fresh();create();require(r100RadioTerm->frame()==frame && pixelChanges(screen,frozenPixels(jni))==0,"Radio screen/actors/native caption frame recover exactly while paused");
   paused=false;advanceR100RoomScript(1./30,false,true);require(r100RadioTerm->manual() && eventMixer.streamStopping(1,stream),"SourceBswitches radio to manual dialogue and stops speech");
   advanceR100RoomScript(1./30,false,false,true);require(r100EventPhase==14,"SourceSTARTcloses native terminal and waits original final sleep");
   advanceR100RoomScript(1./30,false);require(!eventUiActive && r100EventPhase==0 && !leon->roomScriptActive() && leon->eyeMode()==0 && leon->leftHandType()==grip && r100RadioFadeFrame==0,"Radio cleanup restores real player/grip and source concurrent end fades");
   require(!call || (leon->feet()==position && leon->heading()==heading),"Call1 restores exact original pre-call player placement");
  }
  std::cout<<"M37 production secondary JNI/GLES/EGL/player/camera/MDT PASS: "<<checks<<" checks\n";
 }catch(const std::exception &e){std::cerr<<"M37 secondary runtime failure: "<<e.what()<<'\n';return 1;}
}
