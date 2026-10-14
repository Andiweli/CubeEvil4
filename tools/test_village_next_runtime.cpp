#define RE4_K16_ENTRY previous_village_baseline
#include "test_k16_runtime.cpp"
#undef RE4_K16_ENTRY
void verifyShelfRuntime(save_test::SaveJni &jni) {
 using namespace save_test;
 require(roomItemEvents && roomPickups,"Production authored item event owners installed");
 for(unsigned n=0;n<roomItemEvents->shelves().size();++n) {
  const auto &s=roomItemEvents->shelves()[n];
  re4::CollisionVec feet{0,s.area.area.floor+100,0};
  for(auto p:s.area.area.points){feet[0]+=p[0]*.25;feet[2]+=p[1]*.25;}
  require(walkProbe->placeAuthored(feet),"Original shelf action area has source SAT floor");feet=walkProbe->position();
  bool found=false;float angle=0;
  for(unsigned h=0;h<360;++h) {
   angle=float(h)*.017453292f;auto center=feet;center[1]+=250;auto front=center;
   front[0]+=std::sin(angle)*550;front[2]+=std::cos(angle)*550;
   if(const auto hit=collisionWorld->line(center,front,re4::SatSubset::All,0))front=hit->point;
   if(const auto offer=roomItemEvents->offer(feet,angle,front);offer && *offer==n){found=true;break;}
  }
  require(found,"Authored shelf is reachable with actual SAT-clipped action ray");
  leon->setPlacement(feet,angle);controls(jni);frozenPixels(jni);
  controls(jni,re4::Bottom);ticks(jni,1);controls(jni);
  require(roomItemEvents->active() && eventUiActive.load(),"Actual action input starts the shelf camera and suspends gameplay");
  const auto player=leon->feet();const auto worldTicks=enemyGroup?enemyGroup->worldTicks():0;
  ticks(jni,3);require(leon->feet()==player && (!enemyGroup || enemyGroup->worldTicks()==worldTicks),"Shelf cut owns the player and suspends combat");
  const auto eventTicks=roomItemEvents->ticks();paused=true;ticks(jni,3);
  require(roomItemEvents->ticks()==eventTicks,"Paused shelf cut does not advance");paused=false;
  unsigned frames=0;while(roomItemEvents->active()){ticks(jni,1);require(++frames<300,"Original shelf camera finishes");}
  require(!eventUiActive.load() && leon->alive(),"Shelf completion returns live input ownership");
  require(glGetError()==GL_NO_ERROR,"Dynamic original hinges and CAM render without GLES errors");
  const auto save=re4::DecodeNativeSave(re4::EncodeNativeSave(captureNativeGame()));
  require(save.scenario.room(activeRoomId).room.test(s.flag),"Production native save retains opened shelf");
 }
 std::cout<<"production shelves, action input, pause, GLES and native save PASS\n";
}
int main(int argc,char**argv){using namespace save_test;try{
 require(argc==2,"fixture");const auto root=std::filesystem::path(argv[1]),files=std::filesystem::canonical(root/"files");auto js=[&](const std::filesystem::path&p){return string(p.string());};Egl egl;SaveJni jni;long session=19100;
 Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,js(files/"etc/core.das"));
 Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,js(files/"em/em12.drs"),js(files/"etc/emleon00.esl"));
 Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,js(files/"em/wep02.drs"),js(files/"em/wep07.drs"));
 for(unsigned id:{0x103,0x111,0x113,0x106}){char name[12];snprintf(name,sizeof name,"r%03x",id);++session;
 Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
 Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,JNI_TRUE,0,session);
 Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(&jni.env,nullptr,JNI_TRUE);
 Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,js(root/"decoded"/(std::string(name)+".dat")),js(files/"em/pl00.drs"),js(files/"em/wep01.drs"),js(files/"Debug/roominfo.dat"),string(name),session);
 if(!javaException.empty())throw std::runtime_error(javaException);
 require(leon && actorGpuReady && walking,"New room has actual CPU and GPU owners");require(enemyGroup && enemyGroup->actors().size()==(id==0x103?5u:id==0x106?7u:8u),"Exact authored automatic Ganado population");
 require(originalSaveMetadata.enemyListNo==((id==0x103 || id==0x106)?0:1),"Original day/night list selected");
 frozenPixels(jni);unpause(jni);controls(jni);leon->setUnlimitedHealth(true);ticks(jni,100);
 require(enemyGroup->worldTicks()>=100,"New room advances actors and player together");
 if(id==0x103)require(villageAnimals && villageAnimals->animals().size()==12,"All six hens and six cattle installed");
 if(id==0x106||id==0x111||id==0x113)require(dogTraps && dogTraps->released(),"Night traps installed and active");
 require(roomAreaEffects && roomAreaEffects->state().frames>=300,"Room SST effects advance with actual world ticks");
 if(roomItemEvents)verifyShelfRuntime(jni);
 if(id==0x106){std::cout<<name<<" production room shelves verified\n";continue;}
 r100DeadIndices.insert(id==0x103?95:160);
 const auto save=captureNativeGame();
 require(save.enemyDeaths && re4::OriginalSaveEnemyDead(*save.enemyDeaths,id==0x103?0:1,id==0x103?95:160),"Current enemy deaths saved to correct list");
 if(id!=0x103)require(!re4::OriginalSaveEnemyDead(*save.enemyDeaths,0,160),"Night death flag never contaminates day list");
 const auto decoded=re4::DecodeNativeSave(re4::EncodeNativeSave(save));
 require(decoded.enemyListNo==save.enemyListNo && re4::OriginalSaveEnemyDead(*decoded.enemyDeaths,id==0x103?0:1,id==0x103?95:160),"Day/night enemy flags survive actual original save encoding");
 std::cout<<name<<" production room and native save verified\n";
 }
}catch(const std::exception&e){std::cerr<<"Village runtime: "<<e.what()<<'\n';return 1;}}
