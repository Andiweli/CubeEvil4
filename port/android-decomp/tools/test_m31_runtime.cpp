// Production JNI, real GLES3 and real supplied disc files; no test hooks in app.
#define RE4_M27_ENTRY previous_m27_main
#include "test_m27_runtime.cpp"
#undef RE4_M27_ENTRY
#include <filesystem>

namespace m31 {
using namespace m27_test;
bool laserSubmitted(){for(const auto &c:calls)if(c.mode==GL_LINES&&c.count==2)return true;return false;}
std::vector<re4::SceneDraw> windowDraws() {
  auto scene=re4::BuildStaticScene(*previewSmd,-1,previewCommon.get());
  re4::AttachSceneTextures(*previewSmd,scene,previewCommon.get());
  if(previewSmx)re4::ApplySceneSmx(*previewSmx,scene);
  if(previewEnvironment)re4::AttachSceneEnvironment(*previewEnvironment,scene);
  if(previewBlocks)re4::AppendStaticSceneLayer(scene,previewBlocks->mesh);
  re4::ApplyRoomScriptVisibility(activeRoomId,static_cast<const re4::ScenarioState &>(scenarioState).room(activeRoomId),scene);
  re4::AppendRoomObjectScene(scene,*previewObjects);return scene.draws;
}
bool approach(unsigned slot,bool front=false) {
  for(const auto &w:roomInteractions->windows())if(w.placement.slot==slot)
    for(float x:{-350.f,0.f,350.f})for(float z:{450.f,650.f,850.f}) {
      if(!walkProbe->place(point(w.placement,x,-1000,front?z:-z)))continue;
      leon->setPlacement(walkProbe->position(),w.placement.rotation[1]+(front?3.1415927f:0));
      const auto offer=roomInteractions->windowOffer(*leon,*walkProbe);
      if(offer&&offer->slot==slot){re4::SceneFollowHeading(leon->heading());re4::SceneSeedFollowView(0,0);return true;}
    }
  return false;
}
}
int main(int argc,char **argv){using namespace m31;try{
  if(argc!=11&&argc!=12)throw std::runtime_error("Usage: m31_runtime_check R100 R101 R400 PL00 WEP01 roominfo CORE EM12 emleon00 OUTPUT_PREFIX [ALL_127_DECODED_ROOMS]");
  Egl egl;Jni jni;jlong session=3101;
  Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[7]));
  Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[8]),string(argv[9]));
  const auto mode=[&](bool explore){Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,explore?JNI_TRUE:JNI_FALSE,session);};
  const auto create=[&](const char *path,const char *name){
    Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(path),string(argv[4]),string(argv[5]),string(argv[6]),string(name),session);
    frozenPixels(jni);require(javaException.empty()&&previewSmd&&previewLighting,"Real direct-room bridge owns source geometry and lighting");
  };
  mode(true);create(argv[1],"r100");unpause(jni);
  require(leon&&roomInteractions&&walking&&!encounter&&exploreMode,"Direct explore starts r100 with controller-ready player and no enemy");
  require(previewEnvironment->sky.size()==1&&previewBlocks->count==5,"r100 retains its real sky, water and five original block archives");
  controls(jni);frozenPixels(jni);controls(jni,0,1);
  bool early=false;
  for(unsigned t=0;t<24;++t){
    calls.clear();tick(jni);
    if(leon->action()==re4::PlayerAction::Draw){
      const bool ready=leon->rig().sequenceFrame()>=4;
      require(laserSubmitted()==ready,"Actual laser pass appears at frame four, not the end of the draw motion");early|=ready;
    }
  }
  require(early&&leon->aiming(),"Physical LT produces an early visible beam before sustained aim");
  for(const auto &c:calls)if(c.mode==GL_LINES&&c.count==2)require(c.lighting==0&&c.fog==0&&!c.depthWrite,"Actual early laser stays unlit, unfogged and does not write depth");
  require(laserSightData->lineColor[0]>laserSightData->lineColor[1]&&laserSightData->pointColor[0]>laserSightData->pointColor[1],"Laser retains the original red CORE line and marker color");
  save(std::string(argv[10])+"-early-laser.ppm");controls(jni);ticks(jni,45);
  require(approach(0),"Real r100 lower window has a valid controller approach");
  calls.clear();const auto intact=frozenPixels(jni);unsigned glass=0;
  for(const auto &d:windowDraws())if(d.roomObjectSlot==0&&d.roomObjectVariant==0){
    require(d.orderType==1&&d.zMode==1,"Original intact window belongs to translucent sorted order");
    for(const auto &c:calls)if(c.vao&&c.first<=GLint(d.first)&&c.first+c.count>=GLint(d.first+d.count))
      require(!c.depthWrite,"Production window pass preserves geometry behind the transparent pane");
    ++glass;
  }
  require(glass>0,"Actual source window geometry verified in the renderer");save(std::string(argv[10])+"-window-intact.ppm");
  controls(jni,re4::Bottom);tick(jni);controls(jni);
  require(leon->windowActive(),"Physical A/Kreuz prioritizes original window event over generic SAT jump");
  ticks(jni,6);pause(jni);
  auto *owner=roomInteractions.get();auto *player=leon.get();const auto ticksBefore=leon->windowTicks();
  const auto feet=leon->feet();const auto bones=leon->rig().bones();const auto state=roomInteractions->state();
  const auto pausedImage=frozenPixels(jni);ticks(jni,3);
  require(leon->windowTicks()==ticksBefore&&leon->rig().bones()==bones&&roomInteractions->drainEvents().empty(),"Pause freezes original vault and produces no duplicate sounds");
  egl.fresh();mode(true);create(argv[1],"r100");
  require(roomInteractions.get()==owner&&leon.get()==player&&leon->feet()==feet&&leon->rig().bones()==bones&&
          leon->windowTicks()==ticksBefore&&roomInteractions->state().etc==state.etc&&pixelChanges(pausedImage,frozenPixels(jni))==0,
          "Actual EGL recreation preserves vault owner, pose, flags, frame and exact framebuffer");
  unpause(jni);controls(jni);
  for(unsigned t=0;t<180&&leon->windowActive();++t)tick(jni);
  require(!leon->windowActive()&&leon->life()==1200&&roomInteractions->windows()[0].hp==0,"Resumed vault finishes source animation and breaks the correct room window");
  calls.clear();frozenPixels(jni);unsigned variants=0;
  for(const auto &d:windowDraws())if(d.roomObjectSlot==0&&d.roomObjectVariant>=0){
    require(submitted(d)==(d.roomObjectVariant==1),"Actual r100 draw list selects only the original broken pane");++variants;
  }
  require(variants>=2,"Both actual original window variants inspected");save(std::string(argv[10])+"-window-landed.ppm");
  requestRoom(0x101,0,{-121,-87.3,20449},3.1415927f);finish(jni,"r101",argv[2]);
  require(activeRoomId==0x101&&!encounter&&exploreMode,"Explore room transfer cannot create a test enemy");
  requestRoom(0x100,0,{-99685,-484,-1343},2.246f);finish(jni,"r100",argv[1]);
  require(roomInteractions&&roomInteractions->room()==0x100&&roomInteractions->windows()[0].hp==0,"r100 window flags survive a round trip independently of r101");
  ++session;mode(false);create(argv[3],"r400");
  require(!exploreMode&&roomRequest.combat&&roomRequest.room==0x400,"Explicit test mode queues the implemented original Ganado fixture");
  finish(jni,"r400",argv[3]);require(encounter&&leon&&leon->unlimitedHealth(),"Combat test activates a real enemy with unlimited player HP");
  pause(jni);auto *enemy=encounter.get();const auto enemyBefore=enemySnapshot();
  egl.fresh();mode(true);create(argv[3],"r400");
  require(!exploreMode&&encounter.get()==enemy&&enemySnapshot()==enemyBefore,"Same-session context creation preserves deliberately selected combat mode");
  ++session;mode(true);create(argv[3],"r400");
  require(exploreMode&&!encounter&&!roomRequest.token,"New explore activity does not retain previous combat fixture");
  if(argc==12) {
    unsigned audited=0,walkingRooms=0;
    for(const auto &file:std::filesystem::directory_iterator(argv[11])) {
      const auto name=file.path().stem().string();if(file.path().extension()!=".dat"||name.size()!=4||name[0]!='r')continue;
      if(name=="r313"||name=="r314"||name=="r322"||name=="r323")continue;
      ++session;mode(true);calls.clear();create(file.path().c_str(),name.c_str());
      require(!encounter&&previewLighting->present[0],"Catalog explore submits source initial lights without synthetic enemies");
      require(glGetError()==GL_NO_ERROR&&!calls.empty(),"Catalog room source material draws execute with no GLES error");
      walkingRooms+=walking; ++audited;
    }
    require(audited==123,"All 123 supported catalog rooms opened through actual production JNI/GLES");
    std::cout<<"Catalog actual JNI/GLES: "<<audited<<" rooms rendered; "<<walkingRooms<<" grounded player starts; four unsupported unregistered model archives excluded.\n";
  }
  require(pixelChanges(intact,pausedImage)>0,"Real original vault changes its rendered pose");
  std::cout<<"M31 actual JNI/GLES PASS: "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
