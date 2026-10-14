// Independent production JNI result counters / absolute source clock.
#define RE4_M36_ENTRY previous_m37_save_stats_runtime
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
int main(int argc,char**argv){using namespace save_test;try{
 if(argc!=11)throw std::runtime_error("Usage: m37_save_stats_runtime_check R100 R101 PL00 WEP01 WEP02 WEP07 roominfo CORE EM12 ESL");
 Egl egl;SaveJni jni;constexpr jlong session=37480;
 Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_TRUE);
 Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[8]));
 Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[9]),string(argv[10]));
 Java_com_ast_re4decomp_NativeBridge_configureWeapons(&jni.env,nullptr,string(argv[5]),string(argv[6]));
 Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
 Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(&jni.env,nullptr,JNI_TRUE,0,session);
 Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(&jni.env,nullptr,JNI_TRUE);
 Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(argv[1]),string(argv[3]),string(argv[4]),string(argv[7]),string("r100"),session);
 require(javaException.empty()&&leon&&enemyGroup&&actorGpuReady&&!activeEvent,"Real r100 native campaign owners loaded for stats proof");
 require(!originalSaveMetadata.totalHits&&!originalSaveMetadata.totalShots&&!originalSaveMetadata.totalKills,"Fresh source result counters begin at zero");
 bool primaryTarget=false;
 enemyGroup->setExternalTarget([&](const re4::CollisionVec&from,const re4::CollisionVec&,unsigned){
  return primaryTarget || from[0]==-79990?std::optional<re4::AuthoredHitResult>({0,10000,{}}):std::nullopt;
 });
 re4::PlayerShot shot;shot.serial=100;shot.weaponItemId=0x23;shot.from={-80000,20000,-38000};shot.to={-70000,20000,-38000};
 enemyGroup->queueShot(shot);afterWorldTick(&shot);
 require(originalSaveMetadata.totalShots==1&&originalSaveMetadata.totalHits==0,"Production missed shot increments shots and not accuracy");
 primaryTarget=true;++shot.serial;enemyGroup->queueShot(shot);afterWorldTick(&shot);
 require(originalSaveMetadata.chapterShots==2&&originalSaveMetadata.totalShots==2&&originalSaveMetadata.chapterHits==1&&originalSaveMetadata.totalHits==1,"Source n!=0 candidate counts hit independently of damage/HP");
 primaryTarget=false;shot.weaponItemId=0x2c;++shot.serial;shot.pellets.push_back({shot.from,shot.to});
 auto secondary=shot.from;secondary[0]=-79990;shot.pellets.push_back({secondary,shot.to});enemyGroup->queueShot(shot);afterWorldTick(&shot);
 require(originalSaveMetadata.totalShots==3&&originalSaveMetadata.totalHits==1,"Production secondary-only shotgun target never changes accuracy");
 primaryTarget=true;++shot.serial;shot.pellets.assign(13,{shot.from,shot.to});enemyGroup->queueShot(shot);afterWorldTick(&shot);
 require(originalSaveMetadata.totalShots==4&&originalSaveMetadata.totalHits==2,"Production thirteen shotgun rays count one shot and one primary hit");
 afterWorldTick(nullptr);require(originalSaveMetadata.totalKills==0,"Scripted HP0 cliff actors never increase kill results");
 {
  std::lock_guard<std::mutex> lock(saveClockMutex);originalSaveClock.reset(3723,saveWallSeconds()-11,true);
 }
 Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_FALSE);
 {
  std::lock_guard<std::mutex> lock(saveClockMutex);
  require(!originalSaveClock.running()&&originalSaveClock.seconds(saveWallSeconds()+500)>=3734&&originalSaveClock.seconds(saveWallSeconds()+500)<=3735,"Activity pause folds wall seconds without counting background time");
 }
 Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_TRUE);
 subscreenOpen=true;paused=false;saveMenuRequested.store(true);frozenPixels(jni);
 require(originalSaveClock.running(),"Original case/typewriter screen does not stop foreground play clock");
 {
  std::lock_guard<std::mutex> lock(saveClockMutex);originalSaveClock.reset(3723,saveWallSeconds()-11,true);
 }
 const auto image=Java_com_ast_re4decomp_NativeBridge_saveGame(&jni.env,nullptr,19,reinterpret_cast<jintArray>(&calendar));require(image!=nullptr,"Actual native save serializes result counters and wall clock");
 const auto&raw=byteArrays.at(image)->data;const auto card=re4::DecodeOriginalSaveCard(std::vector<std::uint8_t>(raw.begin(),raw.end()));const auto restored=re4::DecodeNativeSave(card.sections);
 require(restored.chapterShots==4&&restored.totalShots==4&&restored.chapterHits==2&&restored.totalHits==2&&restored.totalKills==0,"Source BE global result fields survive real JNI card snapshot");
 require(restored.playSeconds>=3734&&restored.playSeconds<=3735&&card.metadata.playSeconds==restored.playSeconds,"Original card header and global both contain absolute elapsed seconds");
 require(Java_com_ast_re4decomp_NativeBridge_completeSaveWrite(&jni.env,nullptr,Java_com_ast_re4decomp_NativeBridge_saveWriteId(&jni.env,nullptr),JNI_TRUE)==JNI_TRUE,"Stats snapshot commits only after durable storage acknowledgement");
 Java_com_ast_re4decomp_NativeBridge_finishSave(&jni.env,nullptr);paused=true;frozenPixels(jni);require(!originalSaveClock.running(),"Source explicit option pause folds clock");
 require(roomAreaEffects&&r100FirstEvent,"Actual permanent SST and source EVD owners prepared");
 const auto areaFrame=roomAreaEffects->state().frames;const auto sheet=roomAreaEffects->clothMeshes().at(0).vertices;
 beginR100Event(1);advanceR100Event(1./30);
 require(roomAreaEffects->state().frames==areaFrame+1&&roomAreaEffects->clothMeshes().at(0).vertices!=sheet,"Permanent original SST/cloth moves once on an EVD30Hz tick");
 std::uint32_t expectedAreas=0;for(const auto&a:roomAreaEffectsData->areas)
  if(a.number<32&&re4::OriginalEffectAreaContains(a,activeEvent->camera().eye))expectedAreas|=1u<<a.number;
 require(roomAreaEffects->areaMask()==expectedAreas,"EVD area query uses exact source event eye without player+100 offset");
 const auto sourcePose=re4::EventCameraPose(activeEvent->camera());const auto snapshot=roomAreaEffects->state();
 for(const auto&s:snapshot.sprites)if(s.flags&0x20)for(unsigned k=0;k<3;++k)
  require(std::isfinite(s.position[k])&&std::isfinite(sourcePose.eye[k]),"Permanent event sprite uses finite current source camera basis");
 suspendSimulationInput();const auto pausedAreaFrame=roomAreaEffects->state().frames;const auto pausedEventFrame=activeEvent->sourceFrame();
 const auto pausedPicture=frozenPixels(jni);ticks(jni,3);
 require(roomAreaEffects->state().frames==pausedAreaFrame&&activeEvent->sourceFrame()==pausedEventFrame,"Outer pause guard freezes both EVD and permanent room effect clocks");
 require(frozenPixels(jni)==pausedPicture,"Paused permanent EVD effects render deterministically");
 activeEvent->finish();finishR100Event();
 const auto scriptFrame=roomAreaEffects->state().frames;beginR100Radio(0);advanceR100RoomScript(1./30,false);
 require(roomAreaEffects->state().frames==scriptFrame+1,"Radio introduction phase12 keeps permanent room effects moving");
 for(unsigned n=0;n<240&&r100EventPhase==12;++n)advanceR100RoomScript(1./30,false);
 require(r100EventPhase==13&&r100RadioTerm,"Source radio intro reaches the real message terminal");
 const auto radioFrame=roomAreaEffects->state().frames;advanceR100RoomScript(1./30,false);
 require(roomAreaEffects->state().frames==radioFrame+1,"Screen radio phase13 keeps underlying source SST clock");
 r100EventPhase=14;const auto closeFrame=roomAreaEffects->state().frames;advanceR100RoomScript(1./30,false);
 require(roomAreaEffects->state().frames==closeFrame+1&&r100EventPhase==0,"Radio close phase14 owns exactly one final source effects tick");
 std::cout<<"M37 original save counters/clock JNI/GLES: "<<checks<<" checks PASS\n";
 }catch(const std::exception&e){std::cerr<<"M37 save stats runtime: "<<e.what()<<'\n';return 1;}}
