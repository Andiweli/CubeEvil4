#define RE4_M36_ENTRY previous_m37_save_runtime
#include "test_m36_runtime.cpp"
#undef RE4_M36_ENTRY
namespace save_test {
using namespace m35_test;
struct Bytes {std::vector<jbyte> data;};
std::vector<std::unique_ptr<Bytes>> arrays;
std::map<jarray,Bytes*> byteArrays;
std::array<jint,10> calendar{21,37,18,6,9,2026,2,278,0,0};
jsize JNICALL arrayLength(JNIEnv *,jarray a){if(a==reinterpret_cast<jarray>(&calendar))return 10;if(auto at=byteArrays.find(a);at!=byteArrays.end())return jsize(at->second->data.size());return 2048;}
jbyteArray JNICALL newBytes(JNIEnv *,jsize count){arrays.push_back(std::make_unique<Bytes>());auto *p=arrays.back().get();p->data.resize(count);auto a=reinterpret_cast<jbyteArray>(p);byteArrays[a]=p;return a;}
void JNICALL getBytes(JNIEnv *,jbyteArray a,jsize start,jsize n,jbyte *out){const auto &v=byteArrays.at(a)->data;require(start>=0&&n>=0&&std::size_t(start+n)<=v.size(),"Bounded JNI byte read");std::copy_n(v.begin()+start,n,out);}
void JNICALL setBytes(JNIEnv *,jbyteArray a,jsize start,jsize n,const jbyte *in){auto &v=byteArrays.at(a)->data;require(start>=0&&n>=0&&std::size_t(start+n)<=v.size(),"Bounded JNI byte write");std::copy_n(in,n,v.begin()+start);}
void JNICALL getInts(JNIEnv *,jintArray a,jsize start,jsize n,jint *out){require(a==reinterpret_cast<jintArray>(&calendar)&&start==0&&n==10,"Original calendar has10 source fields");std::copy(calendar.begin(),calendar.end(),out);}
jboolean JNICALL exception(JNIEnv *){return javaException.empty()?JNI_FALSE:JNI_TRUE;}
struct SaveJni:AudioJni {SaveJni(){table.GetArrayLength=arrayLength;table.NewByteArray=newBytes;table.GetByteArrayRegion=getBytes;table.SetByteArrayRegion=setBytes;table.GetIntArrayRegion=getInts;table.ExceptionCheck=exception;}};
}
#ifndef RE4_M37_SAVE_ENTRY
#define RE4_M37_SAVE_ENTRY main
#endif
int RE4_M37_SAVE_ENTRY(int argc,char **argv){using namespace save_test;try {
 if(argc!=11)throw std::runtime_error("Usage: m37_save_runtime_check R100 R101 PL00 WEP01 WEP02 WEP07 roominfo CORE EM12 ESL");
 Egl egl;SaveJni jni;jlong session=37400;
 Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[8]));
 Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[9]),string(argv[10]));
 Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,string(argv[5]),string(argv[6]));
 const auto create=[&](){Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,JNI_TRUE,0,session);Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(&jni.env,nullptr,JNI_TRUE);Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(argv[1]),string(argv[3]),string(argv[4]),string(argv[7]),string("r100"),session);require(javaException.empty()&&leon&&actorGpuReady&&walking,"Original r100 is playable for saving");};
 create();frozenPixels(jni);unpause(jni);controls(jni);
 require(cockpitValues.size()==14 && cockpitValues[9]==0,"Initial native cockpit publication has no fabricated Ashley actor");
 originalSaveMetadata.ashleyLife=451;originalSaveMetadata.ashleyMaxLife=800;
 activePartnerId=3;activePartnerCondition=0x12;ticks(jni,1);
 require(cockpitValues[9]==3 && cockpitValues[10]==451 && cockpitValues[11]==800 && cockpitValues[12]==0x12,
   "Partner owner publishes actual actor presence, separate health maximum and source condition bits");
 activePartnerId=2;ticks(jni,1);require(cockpitValues[9]==2,"Luis actor is distinguishable from Ashley by the native UI boundary");
 activePartnerId=activePartnerCondition=0;ticks(jni,1);require(cockpitValues[9]==0,"Partner exit removes Ashley at the next native publication");
 // Source Cckpt.move continues at30Hz inside an otherwise paused subscreen.
 const auto healingInventory=inventoryCase;
 require(inventoryCase.add(6,1) && leon->restoreLife(400),"Inventory healing fixture uses an original green herb and injured player");
 Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_TRUE);
 Java_com_ast_re4decomp_NativeBridge_setSubscreenOpen(&jni.env,nullptr,JNI_TRUE);
 unsigned herbSlot=0;for(const auto &item:inventoryCase.items())if(item.id==6){herbSlot=item.slot;break;}
 Java_com_ast_re4decomp_NativeBridge_caseAction(&jni.env,nullptr,2,herbSlot,0,0,0);
 require(leon->life()==1000,"Production inventory command heals exactly600HP while the subscreen is open");
 const auto healedFeet=leon->feet();const auto healedHeading=leon->heading();const auto uiTick=cockpitTicks;
 ticks(jni,30);
 require(cockpitValues[0]==1000 && cockpitTicks>=uiTick+30 && cockpitTicks<=uiTick+45,
   "Paused inventory publishes healed life and advances only its bounded30Hz wall clock");
 require(leon->feet()==healedFeet && leon->heading()==healedHeading && subscreenOpen,
   "Life-meter clock does not resume player movement or close the inventory");
 Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_FALSE);const auto hiddenTick=cockpitTicks;ticks(jni,2);
 require(cockpitTicks==hiddenTick,"Background Activity freezes the paused cockpit clock");
 Java_com_ast_re4decomp_NativeBridge_setSubscreenOpen(&jni.env,nullptr,JNI_FALSE);
 inventoryCase=healingInventory;require(leon->restoreLife(1200),"Healing test restores the source start state");
 const auto &trigger=r100Scenario->authoredArea(0x14);re4::CollisionVec center{};
 for(const auto &p:trigger.area.points){center[0]+=p[0]/4;center[2]+=p[1]/4;}center[1]=trigger.area.floor+1000;
 bool placed=false;
 for(unsigned i=0;i<32&&!placed;++i)for(unsigned radius=0;radius<=1250&&!placed;radius+=50){
   const float yaw=float(i)*.1963495408f;auto feet=center;
   feet[0]-=std::sin(yaw)*radius;feet[2]-=std::cos(yaw)*radius;
   if(!walkProbe->place(feet,3000))continue;
   leon->setPlacement(walkProbe->position(),yaw);
   auto origin=leon->feet();origin[1]+=250;auto front=origin;
   front[0]+=std::sin(yaw)*550;front[2]+=std::cos(yaw)*550;
   if(const auto hit=collisionWorld->line(origin,front,re4::SatSubset::All,0))front=hit->point;
   placed=re4::RoomFlow::hit(trigger,leon->feet(),yaw,front);
 }
 require(placed,"Authored typewriter area has reachable original SAT/front geometry");
 ticks(jni,1);require(cockpitValues[5]==47 && cockpitValues[6]==1,"Real typewriter predicate publishes original ACT_SAVE/A prompt");
 controls(jni,re4::Bottom);ticks(jni,2);controls(jni);
 require(saveMenuRequested.load()&&subscreenOpen,"Real pressed A at source typewriter opens save UI mailbox and pauses gameplay");
 require(leon->restoreLife(760),"Genuine player health captured");scenarioState.pesetas=12890;scenarioState.scenario.set(12);scenarioState.room(0x100).room.set(10);r100DeadIndices.insert(0x10);r100UsedTraps.insert(10);require(inventoryCase.add(0x57,1),"Source inventory treasure capture");
 auto image=Java_com_ast_re4decomp_NativeBridge_saveGame(&jni.env,nullptr,19,reinterpret_cast<jintArray>(&calendar));require(image!=nullptr,"Production native snapshot produces original card bytes");
 const auto &raw=byteArrays.at(image)->data;std::vector<std::uint8_t> bytes(raw.begin(),raw.end());const auto card=re4::DecodeOriginalSaveCard(bytes);const auto decoded=re4::DecodeNativeSave(card.sections);
 const auto feet=leon->feet();const auto heading=leon->heading();require(decoded.room==0x100&&decoded.life==760&&decoded.scenario.pesetas==12890&&card.metadata.saveCount==1,"Actual native health/money/room/count survive source codec");
 require(re4::OriginalSaveEnemyDead(*decoded.enemyDeaths,0,16)&&re4::OriginalSaveEnemyDead(*decoded.enemyDeaths,0,10),"Actual source crow/trap death words captured");
 const auto failedWrite=Java_com_ast_re4decomp_NativeBridge_saveWriteId(&jni.env,nullptr);
 require(failedWrite!=0 && originalSaveMetadata.saveCount==0 && entryCheckpoint->life==1200,"Prepared image leaves live counter and pre-save checkpoint unchanged");
 require(Java_com_ast_re4decomp_NativeBridge_completeSaveWrite(&jni.env,nullptr,failedWrite,JNI_FALSE)==JNI_FALSE && originalSaveMetadata.saveCount==0 && entryCheckpoint->life==1200,"Failed durable write keeps original counter and checkpoint");
 const auto retryImage=Java_com_ast_re4decomp_NativeBridge_saveGame(&jni.env,nullptr,19,reinterpret_cast<jintArray>(&calendar));
 const auto retryWrite=Java_com_ast_re4decomp_NativeBridge_saveWriteId(&jni.env,nullptr);
 require(retryImage!=nullptr && retryWrite!=failedWrite,"Failed file write can prepare a fresh transaction");
 require(Java_com_ast_re4decomp_NativeBridge_completeSaveWrite(&jni.env,nullptr,failedWrite,JNI_TRUE)==JNI_FALSE && pendingSaveWriteId==std::uint64_t(retryWrite),"Late previous callback cannot commit a newer candidate");
 require(Java_com_ast_re4decomp_NativeBridge_completeSaveWrite(&jni.env,nullptr,retryWrite,JNI_TRUE)==JNI_TRUE && originalSaveMetadata.saveCount==1 && entryCheckpoint->life==760,"Successful durable write commits new source counter and checkpoint once");
 require(Java_com_ast_re4decomp_NativeBridge_completeSaveWrite(&jni.env,nullptr,retryWrite,JNI_TRUE)==JNI_FALSE && originalSaveMetadata.saveCount==1,"Duplicate callback cannot increment saved count twice");
 Java_com_ast_re4decomp_NativeBridge_finishSave(&jni.env,nullptr);require(!subscreenOpen&&!saveMenuRequested.load(),"Closing original save screen resumes the same world");
 ++session;Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,JNI_TRUE,0,session);Java_com_ast_re4decomp_NativeBridge_configureSavedGame(&jni.env,nullptr,image);require(javaException.empty(),"Original card accepted through production JNI load");create();
 require(leon->life()==760&&scenarioState.pesetas==12890&&inventoryCase.count(0x57)==1,"New session restores exact health money and treasure");require(scenarioState.scenario.test(12)&&scenarioState.room(0x100).room.test(10)&&r100UsedTraps.count(10),"Original source scenario/room/trap state restored");
 for(unsigned k=0;k<3;++k){if(std::abs(leon->feet()[k]-double(decoded.position[k]))>=.02)std::cerr<<"Save axis "<<k<<": before="<<feet[k]<<", sourceFloat="<<decoded.position[k]<<", restored="<<leon->feet()[k]<<'\n';require(std::abs(leon->feet()[k]-double(decoded.position[k]))<.02,"Saved source float placement restored on SAT");}require(leon->heading()==heading&&!activeEvent,"Loading restores heading without replaying intro");
 auto *owner=leon.get();egl.fresh();create();require(leon.get()==owner&&leon->life()==760,"EGL recovery retains loaded owner rather than applying card twice");
 // Real fatal reaction -> source90-frame diedemo -> transactional Continue.
 require(entryCheckpoint && entryCheckpoint->saveKind==1 && entryCheckpoint->life==760,"Loaded card remains the Continue checkpoint");
 const auto checkpointMoney=scenarioState.pesetas;scenarioState.pesetas=2;
 scenarioState.room(0x100).items.set(9);r100DeadIndices.insert(17);
 require(inventoryCase.add(0x57,1),"Post-checkpoint inventory mutation");
 require(leon->receiveGanadoAxeHit({leon->feet()[0]+1000,leon->feet()[1],leon->feet()[2]},4000,1) && !leon->alive(),"Production fatal axe damage");
 ticks(jni,89);require(!deathRequested.load(),"Original90-frame death delay is not shortened");
 ticks(jni,2);require(deathRequested.load() && subscreenOpen && deathPicture.size()==2+640*480,"Death ID uses completed real game framebuffer");
 Java_com_ast_re4decomp_NativeBridge_continueConfirmed(&jni.env,nullptr);frozenPixels(jni);
 require(continueConfirmed.load() && deathPicture.size()==2+640*480,"Confirmed Continue captures DPF_PL/DPF_OBJ scene");
 auto *deadOwner=leon.get();
 require(Java_com_ast_re4decomp_NativeBridge_continueGame(&jni.env,nullptr)==JNI_TRUE,"Production Continue starts original saved room request");
 auto request=roomRequest;
 Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(&jni.env,nullptr,jlong(request.token),string(""),string("injected decode failure"));
 frozenPixels(jni);require(leon.get()==deadOwner && !leon->alive() && scenarioState.pesetas==2 && deathRequested.load(),"Failed Continue retains dead scene and current metadata");
 require(Java_com_ast_re4decomp_NativeBridge_continueGame(&jni.env,nullptr)==JNI_TRUE,"Failed Continue can retry the same checkpoint");
 request=roomRequest;Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(&jni.env,nullptr,jlong(request.token),string(argv[1]),string(""));
 frozenPixels(jni);
 require(!deathRequested.load() && !subscreenOpen && leon->alive() && leon->life()==760 && actorGpuReady && glGetError()==GL_NO_ERROR,"Real Continue replaces dead player and restores playable GPU state");
 require(scenarioState.pesetas==checkpointMoney && !scenarioState.room(0x100).items.test(9) && !r100DeadIndices.count(17) && inventoryCase.count(0x57)==1,"Continue restores scenario/deaths/items/money from checkpoint");
 require(originalSaveMetadata.roomContinues==1 && originalSaveMetadata.chapterContinues==1 && originalSaveMetadata.totalContinues==1 && originalSaveMetadata.difficultyPoint==4700,"Successful Continue increments once and applies LVADD_DIE");
 require(entryCheckpoint->saveKind==1 && entryCheckpoint->difficultyPoint==5500,"SYS_LOAD_GAME preserves disk checkpoint point");
 owner=leon.get();
 auto corrupt=newBytes(&jni.env,jsize(raw.size()));byteArrays.at(corrupt)->data=raw;byteArrays.at(corrupt)->data[0x5000]^=1;++session;Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,JNI_TRUE,0,session);Java_com_ast_re4decomp_NativeBridge_configureSavedGame(&jni.env,nullptr,corrupt);require(!javaException.empty()&&!pendingSavedGame&&leon.get()==owner,"Corrupt load rejected before replacing live player");
 std::cout<<"M37 original save JNI/GLES checks passed: "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"M37 save runtime failure: "<<e.what()<<'\n';return 1;}}
