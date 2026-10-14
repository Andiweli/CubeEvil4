#include "r100_secondary.h"
#include "asset_reader.h"
#include "laser_sight.h"
#include "forest_animals.h"
#include "dog_traps.h"
#include "enemy_encounter.h"
#include "weapon_preview.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks{};
void require(bool v,const char *m) {++checks;if(!v)throw std::runtime_error(m);}
std::vector<std::uint8_t> read(const std::string &p) {
 std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Actual file missing: "+p);
 std::vector<std::uint8_t> b(std::size_t(f.tellg()));f.seekg(0);
 if(!f.read(reinterpret_cast<char*>(b.data()),b.size()))throw std::runtime_error("Actual file truncated");
 return b;
}
std::vector<std::uint8_t> sub(const RoomArchive &room,const char *tag) {
 const auto *e=room.find(tag);require(e,"Actual r100 resource");return room.copy(e-room.entries().data());
}
const GenericEventActor &actor(const R100Secondary &room,const char *name) {
 for(const auto &a:room.actors())if(a->name()==name)return *a;
 throw std::runtime_error(std::string("Actual persistent actor missing: ")+name);
}
void event(const std::string &files,const char *name,unsigned assets,unsigned packets,unsigned frames,unsigned cuts,unsigned voices) {
 for(unsigned language:{1u,0u}) {
  OriginalEvent e(files,name,language);require(e.data().assets().size()==assets && e.data().packets().size()==packets,"Actual optional EVD inventory");
  require(e.totalFrames()==frames && e.data().cutFrames().size()==cuts,"Actual optional cut duration");
  unsigned n=0,audio=0,caption=0;
  while(!e.finished()) {
   if(!e.cutFrame())++n;
   const auto f=e.sourceFrame();const auto cam=e.camera();const auto count=e.effectsState().frames;
   require(cam.valid,"Original optional camera executes");e.tick30Hz(false);
   if(std::string(name)=="r100s44") {
    const auto b=e.data().asset("event/r100/s44/cam/cam_s44_000.fcv");const AssetReader raw{b};
    require(raw.u16(0)==149 && b[2]==4 && raw.u32(16)==0,"Actual dense s44 camera retains its zero reserved size word");
    for(unsigned channel=0;channel<4;++channel) {
     require((raw.u16(3+channel*2)>>12)==15,"Actual s44 camera uses dense source format15");
     unsigned p=raw.u32(20+channel*4);
     for(unsigned axis=0;axis<3;++axis) {
      const unsigned n=raw.u16(p);const unsigned values=p+2+n*2;
      require(n==150 && raw.u16(p+2+f*2)==f,"Dense source camera has one authored key per frame");
      const auto expected=raw.f32(values+f*4);
      const auto actual=channel==0?cam.eye[axis]:channel==1?cam.target[axis]:channel==2?cam.roll:cam.fovRadians;
      if(channel<2 || axis==1)require(actual==expected,"Dense source camera equals independent raw float key exactly");
      p=values+n*4;
     }
    }
   }
   require(e.sourceFrame()==f && e.camera().eye==cam.eye && e.effectsState().frames==count,"Paused optional scene owns no clock advancement");
   require(e.fadeAlpha()==0 && e.drawsRoomBackground(),"Optional scene keeps source room/background with no invented fade");
   for(const auto &a:e.actors()) {
    require(a->mesh().fallbackTriangles==0 && a->mesh().unresolvedAlphaTriangles==0,"Actual optional model TPL resolves");
    for(float v:a->worldMatrix())require(std::isfinite(v),"Optional actor world pose finite");
    if(a->rig())for(const auto &bone:a->rig()->bones())for(float v:bone)require(std::isfinite(v),"Original optional motion pose finite");
   }
   for(const auto &cue:e.takeAudioCues()) {
    require(cue.stream && cue.block==1 && cue.number==voices,"Original optional DSP stream");
    require(e.audioStream(cue) && !e.audioStream(cue)->left.empty(),"Actual optional DSP contains samples");++audio;
   }
   if(e.subtitle().remainingFrames && e.subtitle().message>=0) {
    ++caption;
    require(language?e.subtitle().words.empty():!e.subtitle().words.empty(),"Optional event follows source movie caption language policy");
   }
   e.tick30Hz();
  }
  require(n==cuts && audio==1,"Every optional camera/speech cue executes once");
  require((std::string(name)=="r100s30")==(!caption),"Only source s30 has no movie MesSet");
  require(e.unsupported().empty(),"No optional EVD packets skipped");
  const auto end=e.endPlayerPlacement();
  require((std::string(name)!="r100s44")==bool(end),"Only optional scenes with source Leon owner return final player placement");
  e.reset();e.finish();require(e.finished() && e.takeAudioCues().empty(),"Optional skip releases bounded source timeline/audio");
 }
 std::cout<<name<<" actual full timeline PASS; frames="<<frames<<" cuts="<<cuts<<'\n';
}
CollisionVec center(const RoomDoor &d) {
 CollisionVec p{0,d.area.floor+std::min(500.,d.area.height*.5),0};
 if(d.area.type==2) {p[0]=d.area.points[0][0];p[2]=d.area.points[0][1];}
 else for(const auto &v:d.area.points) {p[0]+=v[0]*.25;p[2]+=v[1]*.25;}
 return p;
}
struct OfferPoint {CollisionVec feet{};double heading{};R100SecondaryOffer offer;};
OfferPoint offerAt(const R100Scenario &scenario,const RoomSaveState &save,unsigned area,bool press) {
 const auto p=center(scenario.authoredArea(area));
 for(unsigned i=0;i<720;++i) {
  const double h=i*6.283185307179586/720;
  if(const auto o=scenario.secondaryOffer(save,p,h,press,press,p);o && o->area==area)return {p,h,*o};
 }
 throw std::runtime_error("No actual secondary offer at source area"+std::to_string(area));
}
void triggers(const R100Scenario &scenario) {
 RoomSaveState before;
 for(const auto &data:{std::pair<unsigned,unsigned>{0x19,6},{0x1a,7},{0x1b,8}}) {
  const auto p=offerAt(scenario,before,data.first,true);
  require(p.offer.kind==R100SecondaryOffer::EngineEvent && p.offer.phase==data.second,"Source optional EVD binding");
  if(data.first==0x1b) {
   require(p.offer.playerPosition==CollisionVec{-112251,-193,-4420},"Bridge sequence uses original Leon placement");
   require(bool(scenario.secondaryOffer(before,p.feet,p.heading,false,false,p.feet)),"Original bridge event uses automatic trigger1");
  } else require(!scenario.secondaryOffer(before,p.feet,p.heading,false,false,p.feet),"Officer conversation requires authored action8 press");
  auto after=before;after.room.set(10);
  require(!scenario.secondaryOffer(after,p.feet,p.heading,true,true,p.feet),"RSF10 disables dead officers/initial bridge offers");
 }
 const auto house=offerAt(scenario,before,0x15,false);
 require(house.offer.kind==R100SecondaryOffer::HouseScare && house.offer.cameraCut==10,"Actual house automatic area15 binding");
 require(scenario.beginSecondary(before,house.offer) && before.room.test(15),"House consumes only source RSF15");
 require(!scenario.beginSecondary(before,house.offer) && !scenario.secondaryOffer(before,house.feet,house.heading,false,false,house.feet),"Original house scare runs once");
 RoomSaveState after;after.room.set(10);
 for(unsigned area:{1u,0x18u}) {
  const auto p=offerAt(scenario,after,area,true);
  require(p.offer.phase==9 && p.offer.preservePlayerPosition,"First post-truck ravine action uses s30 and retains source player feet");
 }
 const auto first=offerAt(scenario,after,1,true);
 require(scenario.beginSecondary(after,first.offer) && after.room.test(14),"Ravine EVD consumes original RSF14");
 require(!scenario.beginSecondary(after,first.offer),"Source RSF14 prevents repeated s30");
 const auto ravine=offerAt(scenario,after,1,true),bridge=offerAt(scenario,after,0x18,true);
 require(ravine.offer.kind==R100SecondaryOffer::RoomCamera && ravine.offer.cameraCut==9 && ravine.offer.message==0x28,"Repeated ravine action uses source CAM9/MDT28");
 require(bridge.offer.kind==R100SecondaryOffer::RoomCamera && bridge.offer.cameraCut==8 && bridge.offer.message==0xe,"Repeated bridge action uses source CAM8/MDT0e");
}
void camera(const std::vector<std::uint8_t> &cam,unsigned cut) {
 const AssetReader r{cam};const unsigned base=16+cam.at(5)*64;unsigned record=~0u;
 for(unsigned i=0;i<cam.at(4);++i)if(cam.at(base+i*52+1)==cut)record=base+i*52;
 require(record!=~0u && r.u32(record+32)==2,"Actual secondary camera has two source Hermite keys");
 const auto pFrame=r.u32(record+16);const double last=pFrame?r.u16(pFrame+2):30;
 R100RoomCamera runtime(cam,cut);require(runtime.maximumFrame()==last,"Original HermiteExport final header uses last authored pFrame");
 for(unsigned f=0;f<=unsigned(last)+1;++f) {
  require(runtime.frame()==f && runtime.camera().valid,"Raw camera advances one source frame");
  const auto before=runtime.camera();runtime.tick30Hz(false);require(runtime.camera().eye==before.eye && runtime.frame()==f,"Forced room camera pause");
  for(unsigned channel=0;channel<4;++channel)for(unsigned axis=0;axis<(channel<2?3u:1u);++axis) {
   const unsigned width=channel<2?12:4,p=r.u32(record+36+channel*4);
   double first=r.f32(p+(channel<2?axis*4:0)),final=r.f32(p+width+(channel<2?axis*4:0));
   if(channel==3) {first=float(first)*.017453292f;final=float(final)*.017453292f;}
   const double u=f/last,h00=2*u*u*u-3*u*u+1,h01=-2*u*u*u+3*u*u,h11=u*u*u-u*u;
   const double oracle=f>=last+1?final:h00*first+h01*final+h11*(final-first);
   const auto actual=channel==0?before.eye[axis]:channel==1?before.target[axis]:channel==2?before.roll:before.fovRadians;
   require(std::abs(actual-oracle)<(channel<2?.035:1e-5),"Forced CAM curve matches independent source-export two-key oracle");
  }
  require(runtime.finished()==(f==unsigned(last)+1),"Source CameraSequenceCtrl ends only at maxFrame+1");
  runtime.tick30Hz();
 }
 runtime.reset();require(!runtime.finished() && runtime.frame()==0,"Forced CAM reset reconstructs actual first key");
}
void persistent(const RoomArchive &arc,const EnemySpawns &spawns,const std::string &files) {
 RoomSaveState save;R100Secondary room(arc,files+"/em/em12.drs",spawns,save,1);
 require(room.actors().size()==8,"All source policecar/cop/addModel and non-Japanese hanging actors exist");
 for(const auto &a:room.actors()) {
  require(a->visible() && a->lightMask()==16,"Original SetObjSmd uses display2 and scroll light mask10");
  require(!a->mesh().vertices.empty() && !a->mesh().fallbackTriangles && !a->mesh().unresolvedAlphaTriangles,"Persistent original BIN/TPL geometry resolves");
  for(float v:a->worldMatrix())require(std::isfinite(v),"Persistent source world matrix finite");
 }
 const auto &car=actor(room,"scr_r100_car");
 for(unsigned part:{11u,13u,15u})require(car.rig()->partScale(part)==std::array<float,3>{0,0,0},"Original intact car masks source parts11/13/15");
 require(car.rig()->partRotation(10)==std::array<float,3>{.436332315f,0,0},"Original R100Init opens car part10 exactly");
 require(room.roomObjectVisible(0x23) && room.roomObjectVisible(0x44) && !room.roomObjectVisible(0x37) && !room.roomObjectVisible(0x38),"Before-truck source SMD display flags");
 require(room.roomObjectVisible(0x11) && !(room.effectDisplayMask()&(1u<<17)) && (room.effectDisplayMask()&(1u<<18)),"SST17/18 flags are separate from SMD work indices");
 const auto &cop=actor(room,"scr_r100_cop0");const auto frame=cop.rig()->status().sequenceFrame;
 require(!room.actorVisibleDuringSuspend(car) && room.actorVisibleDuringSuspend(actor(room,"scr_r100_hanging")),"ModelTrans source suspend gate keeps only defaultNoSuspend hanging scenery at entry");
 room.beginSourceEvent(6);
 require(room.actorVisibleDuringSuspend(car) && room.actorVisibleDuringSuspend(cop) && !room.actorVisibleDuringSuspend(actor(room,"scr_r100_cop1")),"Source MesCar00 keeps persistent car/cop0 during authored passenger scene");
 const auto driverFrame=cop.rig()->status().sequenceFrame,passengerFrame=actor(room,"scr_r100_cop1").rig()->status().sequenceFrame;
 room.tickSuspended30Hz();require(cop.rig()->status().sequenceFrame!=driverFrame && actor(room,"scr_r100_cop1").rig()->status().sequenceFrame==passengerFrame,"Only native NoSuspend cop motion advances during optional EVD");
 room.endSourceEvent(6);require(!room.actorVisibleDuringSuspend(car) && !room.actorVisibleDuringSuspend(cop),"Source officer task restores its NoSuspend flags");
 room.beginSourceEvent(10);room.endSourceEvent(10);require(room.actorVisibleDuringSuspend(actor(room,"scr_r100_car_sub")),"HouseEvent retains exact source carSub NoSuspend flag after cleanup");
 const auto p=center(R100Scenario(sub(arc,"AEV")).authoredArea(0x24));
 room.tick30Hz(save,p,0,true,false,false);require(!room.takeOfficerMessage(),"Action beside car does not replace source weapon DontFire requirement");
 require(cop.rig()->status().sequenceFrame!=frame,"Persistent actual cop idle FCV advances on world ticks");
 room.tick30Hz(save,p,0,true,true,false);require(room.takeOfficerMessage() && room.officerTalkRemaining()>0,"Source weapon prohibition plus press starts officer FCV and MDT33");
 const auto cue=room.takeSounds();require(cue.size()==1 && cue[0].kind==R100RoomSound::Sample && cue[0].block==6 && cue[0].number==6,"Original driver voice is block6/sample6");
 room.prepareMessages(files,1);const auto message=room.roomMessage(0x33);
 require(message.number==0x33 && message.attributes==0x52 && message.x==100 && message.y==270 && message.fontHeight==32 && message.lineGap==25,"Exact R100Main common layout0 officer caption");
 require(message.words.size()>=4 && message.words[1]==9 && message.words[2]==90 && message.glyphs.empty(),"Actual English MDT33 intentionally contains source90-frame wait rather than invented text");
 R100MessageRuntime officer(message);for(unsigned tick=0;tick<91;++tick)officer.tick30Hz();
 require(!officer.finished(),"Source timed90 wait retains two-stage code01 end");
 officer.tick30Hz();require(officer.finished(),"Original officer MDT timer finishes on native control timeline");
 room.setAfterTruck(true);
 require(!actor(room,"scr_r100_cop0").visible() && !actor(room,"scr_r100_cop1_face").visible(),"Truck sequence removes source cops and inherited addModels");
 for(unsigned part:{11u,12u,13u,14u,16u})require(car.rig()->partScale(part)==std::array<float,3>{0,0,0},"Original wreck masks source subparts");
 require(car.rig()->partScale(15)==std::array<float,3>{1,1,1},"Original wreck exposes source part15");
 require(car.worldMatrix()[3]==-120225 && car.worldMatrix()[7]==-14319 && car.worldMatrix()[11]==-2131,"Original post-truck car position");
 require(!room.roomObjectVisible(0x23) && !room.roomObjectVisible(0x44) && room.roomObjectVisible(0x37) && room.roomObjectVisible(0x38) && (room.effectDisplayMask()&(1u<<17)),"Post-truck source room scroll/effect flags");
 R100Secondary jp(arc,files+"/em/em12.drs",spawns,save,0);require(jp.actors().size()==6 && !(jp.effectDisplayMask()&(1u<<18)),"Source Japanese country profile omits both hanging models and SST18");
 jp.prepareMessages(files,0);const auto japanese=jp.roomMessage(0x33);require(!japanese.words.empty() && japanese.y==273,"Japanese room messages select stage1 font and source layout");
 const Sat sat(sub(arc,"SAT"));const SatCollision floor(sat);WalkProbe probe(floor);LeonPreview player(files+"/em/pl00.drs",files+"/em/wep01.drs");
 require(probe.place(R100EntryPlayerPosition()),"Actual entry floor initializes source action owner");player.setPlacement(probe.position(),R100EntryPlayerHeading);
 room.installPlayerMotion(player);room.beginHouse(player,probe);
 require(player.roomScriptActive() && player.feet()==CollisionVec{-80540,-8,-12073} && player.heading()==2.99f && room.cameraActive(),"Original house camera uses real Leon owner/ARC34 placement");
 while(!room.cameraFinished()) {player.step(probe,1,1,true,true,1./30);room.tickCamera30Hz();require(player.roomScriptActive(),"Source camera owns Leon motion despite movement controls");}
 require(room.cameraFrame()==151,"House source CAM10 uses exact exported151-frame end");
 room.finishHouse(player);require(!player.roomScriptActive() && !room.cameraActive(),"House cleanup returns actual player and room camera");
 for(unsigned call:{0u,1u}) {
  const auto held=player.leftHandType();const auto oldFeet=player.feet();
  room.beginRadioIntro(player,probe,call);
  require(room.radioIntroFrame()==1 && player.rig().status().sequenceFrame==1,
      "Original hidden SceSleep1 commits the first radio pose before display");
  require(player.eyeMode()==1 && player.rig().partRotation(0x21)==player.rig().partRotation(0x20),
      "Original radio eye mode is active during the hidden first motion tick");
  require(player.roomScriptActive() && (!call || (player.feet()==CollisionVec{-81500,860,-38900} && player.heading()==1.6f)),"Original radio intro uses actual Leon motion and call1 placement");
  require(call || player.feet()==oldFeet,"Call0 leaves source player placement untouched");
  for(unsigned frame=2;frame<=22;++frame) {
   room.tickRadioIntro30Hz(player,probe);
   require(room.radioIntroFrame()==frame && room.radioHandVisible()==(frame>=22),"OpeSetOpenTerm SceSleep1 plus21 iterations attach radio at tick22");
   require(player.leftHandType()==(frame>=22?2u:held),"Radio intro changes source left grip only after attachment");
  }
  const auto matrix=room.radioMatrix(player);
  // Independent complete literal SDK C_MTXConcat (src/lib/mtx.c78),
  // including its translation association. TransformPoint has another
  // float addition order and is not a bit-exact matrix-concat oracle.
  const auto sourceConcat=[](const Matrix34 &a,const Matrix34 &b) {
   Matrix34 r{};
   for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)
    r[row*4+col]=(col==3?a[row*4+3]:0.f)+(a[row*4+2]*b[8+col]+(a[row*4]*b[col]+a[row*4+1]*b[4+col]));
   return r;
  };
  SmdPlacement radioLocal;radioLocal.position={111,-22,66};radioLocal.rotation={-.48869219f,.31415927f,-.73303829f};radioLocal.scale={1,1,1};
  const auto expected=sourceConcat(sourceConcat(player.placement(),player.rig().jointMatrices().at(16)),PlacementMatrix(radioLocal));
  for(unsigned k=0;k<12;++k)require(matrix[k]==expected[k],"Actual radio uses complete source SDK parent-part16/local-offset matrix");
  require(room.radioActor().rig() && room.radioActor().visible() && room.radioActor().mesh().fallbackTriangles==0,"Original PLARC77/78 radio BIN/TPL/rest skeleton resolved");
  unsigned guard=22;
  while(!room.radioIntroFinished(player) && guard<200) {room.tickRadioIntro30Hz(player,probe);++guard;}
  require(guard==71,"Original PLARC79 max70 motion hands off at71ticks");
  room.finishRadioIntro(player);require(!player.roomScriptActive() && player.leftHandType()==held,"Radio cleanup restores source old grip and real player ownership");
 }
 const Sat sourceEat(sub(arc,"EAT"));const SatCollision eat(sourceEat,SatQueryMode::Effect);
 EnemyEncounter corpse(floor,eat,R100FirstGanadoSpawn(),files+"/em/em12.drs",true);
 RoomSaveState after;after.room.set(10);
 require(!room.corpseOffer(after,corpse,corpse.feet(),0,true,corpse.feet()),"Living firstGanado has no fabricated corpse examination");
 for(unsigned attempt=0;attempt<50 && corpse.alive();++attempt) {
  const auto shapes=NormalGanadoHitShapes();const auto &body=shapes.at(1);auto offset=body.offset;offset[1]+=body.height*.5f;
  const auto center=TransformPoint(WeaponAttachmentMatrix(corpse.actor().rig().jointMatrices().at(body.partsNo-1),corpse.placement()),offset);
  bool queued=false;
  for(unsigned axis:{0u,2u,1u})for(double sign:{-1.,1.})if(!queued) {
   CollisionVec from{center[0],center[1],center[2]},to=from;from[axis]-=sign*800;to[axis]+=sign*800;
   if(const auto hit=HitAuthoredShapes(shapes,corpse.actor().rig().jointMatrices(),corpse.placement(),from,to,2560000,0x10))
    if(shapes.at(hit->partIndex).partsNo!=5) {
     PlayerMeleeEvent knife;knife.serial=1000+attempt;knife.rays.push_back({from,to});corpse.queueMelee(knife,player.feet());queued=true;
    }
  }
  require(queued,"Actual firstGanado body capsule accepts source knife ray");
  for(unsigned tick=0;tick<21;++tick)corpse.tick(player.feet());
 }
 require(!corpse.alive(),"Source damage kills actual500HP firstGanado for dynamic corpse interaction");
 for(unsigned tick=0;tick<100;++tick)corpse.tick(player.feet());
 const auto dynamic=OriginalR100CorpseArea(corpse.placement());auto point=center(dynamic);
 const auto offer=room.corpseOffer(after,corpse,point,0,true,point);
 require(offer && offer->kind==R100SecondaryOffer::CorpseMessage && offer->message==0xd && dynamic.hitType==1 && dynamic.triggerType==8 && dynamic.priority==1 && dynamic.area.height==1000,"Dead source model supplies original dynamic examine AEV and MDT D");
 require(!room.corpseOffer(after,corpse,point,0,false,point),"Source dynamic corpse action requires a new press");
 room.beginCorpseCamera(player,corpse);const auto camera=room.camera();
 const auto a=TransformPoint(WeaponAttachmentMatrix(player.rig().jointMatrices().at(32),player.placement()),{0,0,0});
 const auto b=TransformPoint(WeaponAttachmentMatrix(player.rig().jointMatrices().at(33),player.placement()),{0,0,0});
 const auto chest=TransformPoint(WeaponAttachmentMatrix(corpse.actor().rig().jointMatrices().at(2),corpse.placement()),{0,0,0});
 for(unsigned k=0;k<3;++k)require(camera.eye[k]==(a[k]+b[k])*.5f && camera.target[k]==chest[k],"Original lookdown uses real player32/33 and dead enemypart2");
 require(camera.fovRadians==45.f*.017453292f && camera.roll==0 && room.cameraFinished(),"Lookdown source camera is fixed45degrees with no invented timer");
 for(unsigned tick=0;tick<100;++tick)room.tickCamera30Hz();
 require(room.camera().eye==camera.eye && room.camera().target==camera.target,"CameraLookDownEm::move leaves exact camera fixed");
 room.endCamera();require(!room.cameraActive(),"EndLookDownEm releases native fixed camera");
}
void animalLaser(const RoomArchive &arc,const EnemySpawns &spawns,const std::string &files) {
 const Sat sourceSat(sub(arc,"SAT"));const SatCollision sat(sourceSat);
 ForestAnimals forest(sat,read(files+"/etc/emleon00.esl"),files+"/em/em23.drs");
 unsigned crowHits=0;
 for(const auto &animal:forest.animals()) {
  const auto feet=animal.feet();PlayerShot ray;
  ray.from={feet[0]-2000,feet[1],feet[2]};ray.to={feet[0]+2000,feet[1],feet[2]};
  const auto expected=forest.target(ray.from,ray.to,0);require(bool(expected),"Actual crow source YARARE_INFO intersects authored pose");
  const auto sight=ResolveLaserSight(ray,nullptr,nullptr,nullptr,nullptr,19000,nullptr,&forest);
  require(sight.hitResult==2 && sight.point && !sight.targetDontFire,"Actual crow is a laser mark target without invented no-fire status");
  for(unsigned k=0;k<3;++k)require(sight.target[k]==expected->cross[k],"Crow laser ends at original posed capsule hit");
  ++crowHits;
 }
 ScenarioState scenario;DogTraps traps(sat,spawns,files+"/em/em21.drs",files+"/em/em2a.drs",scenario);traps.releaseAfterTruck();
 unsigned trapHits=0;
 for(const auto &a:traps.actors()) {
  const auto m=a->worldMatrix();bool found=false;
  for(int y=-500;y<=2000 && !found;y+=100)for(int z=-500;z<=500 && !found;z+=100) {
   PlayerShot ray;ray.from={m[3]-2000,m[7]+y,m[11]+z};ray.to={m[3]+2000,m[7]+y,m[11]+z};
   if(const auto expected=traps.target(ray.from,ray.to,0)) {
    const auto sight=ResolveLaserSight(ray,nullptr,nullptr,nullptr,nullptr,19000,nullptr,nullptr,&traps);
    require(sight.hitResult==expected->targetResult && !sight.targetDontFire,"Dog/trap laser uses exact original hit result and no-fire status");
    require(sight.point==(expected->targetResult==2 && sight.distanceSquared>250000),"Original NO_MARK suppresses the dot without losing the line hit");
    for(unsigned k=0;k<3;++k)require(sight.target[k]==expected->hit.cross[k],"Dog/trap laser ends at actual posed source hit shape");
    found=true;++trapHits;
   }
  }
 }
 require(crowHits==5 && trapHits>=2,"Actual forest crow and dog/trap laser owners execute");
}
}
int main(int argc,char **argv) {
 try {
  if(argc!=4)throw std::runtime_error("Usage: m37_secondary_event_check DISC_FILES R100.dat ESL");
  event(argv[1],"r100s30",18,22,340,3,10);event(argv[1],"r100s41",18,23,160,1,16);
  event(argv[1],"r100s43",17,22,110,1,18);event(argv[1],"r100s44",9,14,150,1,19);
  const auto arc=RoomArchive::Load(argv[2]);const EnemySpawns spawns(read(argv[3]));
  triggers(R100Scenario(sub(arc,"AEV")));for(unsigned cut:{7u,8u,9u,10u,11u,13u,14u,15u})camera(sub(arc,"CAM"),cut);
  persistent(arc,spawns,argv[1]);
  animalLaser(arc,spawns,argv[1]);
  const unsigned carAttr=0x800080;
  require(!OriginalR100DontFire(1,carAttr,false,249999) && OriginalR100DontFire(1,carAttr,false,250000),"Source500-unit early return boundary");
  require(OriginalR100DontFire(1,carAttr,false,399999999) && !OriginalR100DontFire(1,carAttr,false,400000000),"Source20000-unit flagged surface strict boundary");
  require(!OriginalR100DontFire(2,carAttr,false,1000000) && !OriginalR100DontFire(1,0x80,false,1000000),"Only winning EAT ROOM1 surface enables police no-fire");
  require(OriginalR100DontFire(2,0,true,500000000) && !OriginalR100DontFire(2,0,true,0),"Original flagged enemy overrides surface range after early-return boundary");
  std::cout<<"M37 actual secondary events/cameras/persistent actors PASS: "<<checks<<" checks\n";return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
