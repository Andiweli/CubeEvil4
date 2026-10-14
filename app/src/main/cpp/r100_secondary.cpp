#include "r100_secondary.h"
#include "asset_reader.h"
#include "enemy_encounter.h"
#include "weapon_preview.h"
#include "ports/rnd.h"
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace re4 {
namespace {
std::vector<std::uint8_t> entry(const RoomArchive &room,const char *tag,unsigned occurrence=0) {
  const auto *found=room.find(tag,occurrence);
  if(!found)throw std::runtime_error(std::string("r100: Original resource missing: ")+tag);
  return room.copy(found-room.entries().data());
}
std::vector<std::uint8_t> arc(const RoomArchive &room,unsigned source,const char *tag) {
  if(source<4 || source-4>=room.entries().size() || !std::equal(tag,tag+3,room.entries()[source-4].tag.begin()))
    throw std::runtime_error("r100: Invalid original ARC number/type");
  return room.copy(source-4);
}
void put16(std::vector<std::uint8_t> &b,unsigned value) {b.push_back(value>>8);b.push_back(value);}
void put32(std::vector<std::uint8_t> &b,unsigned value) {put16(b,value>>16);put16(b,value);}
void set32(std::vector<std::uint8_t> &b,unsigned at,unsigned value) {
  b.at(at)=value>>24;b.at(at+1)=value>>16;b.at(at+2)=value>>8;b.at(at+3)=value;
}
void putFloat(std::vector<std::uint8_t> &b,float value) {unsigned bits;std::memcpy(&bits,&value,4);put32(b,bits);}
std::vector<std::uint8_t> exportCamera(const std::vector<std::uint8_t> &cam,unsigned wanted) {
  const AssetReader r{cam};r.range(0,16);
  if(r.u32(0)!=0x42343034)throw std::runtime_error("r100: Original B404 camera is missing");
  const unsigned cuts=cam[4],areas=cam[5],base=16+areas*64;
  r.range(base,cuts*52);unsigned found=~0u;
  for(unsigned i=0;i<cuts;++i)if(cam[base+i*52+1]==wanted) {found=base+i*52;break;}
  if(found==~0u || cam[found+2]!=6)throw std::runtime_error("r100: original motion CAM cut is missing");
  const unsigned count=r.u32(found+32),frames=r.u32(found+16);
  if(count<2 || count>26)throw std::runtime_error("r100: CAM Hermite key budget");
  std::vector<unsigned> times;for(unsigned k=0;k<count;++k) {
    const auto t=frames?r.u16(frames+k*2):k*30;
    if(k && t<=times.back())throw std::runtime_error("r100: CAM key order");
    times.push_back(t);
  }
  // HermiteExport initially writes (nPoint-1)*30, then overwrites the
  // header with the final authored frame at cam_ctrl.cpp178.
  std::vector<std::uint8_t> out;put16(out,times.back());out.push_back(4);
  for(unsigned i=0;i<4;++i)put16(out,i<2?4:2);
  for(unsigned i=0;i<4;++i)out.push_back(i);
  out.push_back(0);put32(out,0);for(unsigned i=0;i<4;++i)put32(out,0);
  for(unsigned channel=0;channel<4;++channel) {
    set32(out,20+channel*4,out.size());
    const unsigned source=r.u32(found+36+channel*4);
    r.range(source,count*(channel<2?12:4));
    for(unsigned axis=0;axis<3;++axis) {
      put16(out,count);for(unsigned t:times)put16(out,t);
      const auto value=[&](unsigned key) {
        float v=r.f32(source+key*(channel<2?12:4)+(channel<2?axis*4:0));
        if(channel==3)v*=0.017453292f; // source DEG2RAD
        return v;
      };
      for(unsigned k=0;k<count;++k) {
        const auto k0=k?k-1:k,k1=std::min(k+1,count-1);
        const float v=value(k),v0=value(k0),v1=value(k1);
        const float dt0=float(times[k]-times[k0]),dt1=float(times[k1]-times[k]);
        float tangent=k==0?(v1-v)/dt1:k+1==count?(v-v0)/dt0:
          (dt1*((v-v0)/dt0)+dt0*((v1-v)/dt1))/(dt0+dt1);
        tangent*=dt0;putFloat(out,v);putFloat(out,tangent);putFloat(out,tangent);
      }
    }
    while(out.size()%4)out.push_back(0);
  }
  set32(out,16,out.size());return out;
}
SmdPlacement placement(std::array<float,3> pos,std::array<float,3> angle) {
  SmdPlacement result;result.position=pos;result.rotation=angle;result.scale={1,1,1};return result;
}
}
R100RoomCamera::R100RoomCamera(const std::vector<std::uint8_t> &cam,unsigned cut)
    :curve_(std::make_unique<EventCurve>(exportCamera(cam,cut))) {
  // CutCall searches CUT_INFO by CAMERA_DATA.No; LightMgr receives its
  // linked AREA_DATA.No, which need not equal the camera's number.
  const AssetReader r{cam};bool found=false;
  for(unsigned i=0;i<cam.at(5);++i) {
    const auto record=16+i*16,area=r.u32(record+8),camera=r.u32(record+12);
    r.range(camera,52);r.range(area,48);
    if(cam[camera+1]==cut){lightArea_=cam[area+1];found=true;break;}
  }
  if(!found || lightArea_==0xff)throw std::runtime_error("r100: Original CAM light surface is missing");
  reset();
}
RoomDoor OriginalR100CorpseArea(const Matrix34 &matrix) {
  // SceAtCreateExecAt / sceAtGetArea: source four local corners follow
  // W->em's complete model matrix; height is not multiplied by model scale.
  RoomDoor d;d.sourceRoom=0x100;d.no=0xff;d.enabled=7;d.hitType=1;
  d.triggerType=8;d.targetType=1;d.priority=1;d.kind=2;d.area.enabled=1;d.area.type=1;
  d.area.height=1000;
  const std::array<std::array<float,3>,4> local{{{750,0,-750},{750,0,750},{-750,0,750},{-750,0,-750}}};
  for(unsigned i=0;i<4;++i) {
    const auto p=TransformPoint(matrix,local[i]);d.area.points[i]={p[0],p[2]};if(!i)d.area.floor=p[1];
  }
  return d;
}
EventCamera OriginalR100CorpseCamera(const Matrix34 &player,
    const std::vector<Matrix34> &playerParts,const Matrix34 &corpse,
    const std::vector<Matrix34> &corpseParts) {
  // StartLookDownEm averages real Leon parts32/33; CameraLookDownEm's
  // constructor looks at the original corpse part2 and stays fixed.
  const auto a=TransformPoint(WeaponAttachmentMatrix(playerParts.at(32),player),{0,0,0});
  const auto b=TransformPoint(WeaponAttachmentMatrix(playerParts.at(33),player),{0,0,0});
  const auto target=TransformPoint(WeaponAttachmentMatrix(corpseParts.at(2),corpse),{0,0,0});
  EventCamera c;c.valid=true;c.roll=0;c.fovRadians=45.f*.017453292f;
  for(unsigned k=0;k<3;++k) {c.eye[k]=(a[k]+b[k])*.5f;c.target[k]=target[k];}
  return c;
}
void R100RoomCamera::sample() {
  camera_={};
  for(unsigned i=0;i<curve_->channels().size();++i) {
    const auto v=curve_->sample(i,float(frame_));
    switch(curve_->channels()[i].index) {
      case 0:camera_.eye=v;break;case 1:camera_.target=v;break;
      case 2:camera_.roll=v[1];break;case 3:camera_.fovRadians=v[1];break;
    }
  }
  camera_.valid=camera_.fovRadians>0 && camera_.fovRadians<3.1415927f;
  if(!camera_.valid)throw std::runtime_error("r100: Invalid original CAM projection");
}
bool OriginalR100DontFire(unsigned result,std::uint32_t attributes,bool target,double distance) {
  const unsigned type=((attributes&0x800000)?1:0)+((attributes&0x8000)?2:0)+((attributes&0x80)?4:0);
  // The original early-return precedes StaFlagOn, including enemy flags.
  return distance>=250000 && (target || (result==1 && type==5 && distance<400000000));
}
R100MessageRuntime::R100MessageRuntime(R100RoomMessage message):message_(std::move(message)) {
  message_.glyphs.clear();speed_=(message_.attributes&0x40)?0:1;
}
void R100MessageRuntime::clearPage() {
  message_.glyphs.clear();line_=speedCount_=wait_=0;inputWait_=instant_=false;
  message_.pageStart=cursor_;
}
void R100MessageRuntime::tick30Hz(bool confirm,bool advance) {
  if(!advance || finished_)return;
  if(confirm)instant_=true;
  for(unsigned budget=0;budget<16384;++budget) {
    if(cursor_>=message_.words.size())throw std::runtime_error("r100: Original message has no end");
    const auto code=message_.words[cursor_];
    if(code>=0x80) {
      if(!(message_.attributes&0xc0) && !instant_ && speedCount_++<speed_)return;
      message_.glyphs.push_back({unsigned(code-0x80),line_,color_});speedCount_=0;++cursor_;continue;
    }
    switch(code) {
      case 0:clearPage();++cursor_;break;
      case 1:if(endPending_)finished_=true;else endPending_=true;return;
      case 3:++line_;++cursor_;break;
      case 4:++cursor_;clearPage();return;
      case 5:speed_=message_.words.at(cursor_+1);speedCount_=0;cursor_+=2;break;
      case 6:color_=message_.words.at(cursor_+1);cursor_+=2;break;
      case 8:
        if(message_.attributes&0x02000000) {++cursor_;break;}
        if(!inputWait_) {inputWait_=true;return;}
        if(!confirm)return;
        inputWait_=false;++cursor_;break;
      case 9:
        if(!wait_) {wait_=message_.words.at(cursor_+1);return;}
        if(wait_==0xffff)return;
        if(--wait_)return;
        cursor_+=2;break;
      default:throw std::runtime_error("r100: Unimplemented original message opcode "+std::to_string(code));
    }
  }
  throw std::runtime_error("r100: Original message commands exceed budget");
}
void R100RoomCamera::reset() {frame_=0;finished_=false;sample();}
void R100RoomCamera::tick30Hz(bool advance) {
  if(!advance || finished_)return;
  ++frame_;sample();
  // CameraSequenceCtrl compares against maxFrame+1 after this evaluation.
  if(frame_>=curve_->maxFrame()+1)finished_=true;
}
unsigned R100Secondary::random() {
  const auto prior=RndState();RndInit(randomState_);const auto value=Rnd();randomState_=RndState();RndInit(prior);return value;
}
R100Secondary::R100Secondary(const RoomArchive &room,const std::string &enemy,
    const EnemySpawns &spawns,const RoomSaveState &state,unsigned country)
    :scenario_(entry(room,"AEV")),country_(country) {
  if(country>1)throw std::runtime_error("r100: Invalid original country profile");
  const auto *source=spawns.find(0x25);
  if(!source || source->room!=0x100 || source->id!=0x12 || source->setNo!=0x1b)
    throw std::runtime_error("r100: Original house scene Ganado is missing");
  houseSpawn_=*source;playerMotion_.motion=arc(room,0x34,"FCV");messageTable_=entry(room,"MDT");
  const auto player=RoomArchive::LoadDrs(enemy.substr(0,enemy.find_last_of('/'))+"/pl00.drs");
  radioIntroMotion_.motion=arc(player,0x79,"FCV");
  radioActor_=std::make_unique<GenericEventActor>("scr_r100_radio","PL_ARC77",arc(player,0x77,"BIN"),arc(player,0x78,"TPL"));
  radioActor_->prepareStaticPose();radioActor_->setDisplayOverride(true);radioActor_->setLightMaskOverride(1);
  const auto add=[&](const char *name,unsigned bin,unsigned tpl,GenericEventActor *parent=nullptr) {
    auto actor=std::make_unique<GenericEventActor>(name,name,arc(room,bin,"BIN"),arc(room,tpl,"TPL"),parent);
    auto *result=actor.get();result->setLightMaskOverride(16);
    // addModel belongs to the same displayed cModel as its owner. Giving
    // the separate native mesh an independent override would leave the
    // face floating after the source removes the officer body.
    if(!parent)result->setDisplayOverride(true);
    actors_.push_back(std::move(actor));return result;
  };
  car_=add("scr_r100_car",0x1d,0x1e);car_->prepareStaticPose();
  carSub_=add("scr_r100_car_sub",0x1f,0x20);carSub_->prepareStaticPose();
  car_->setPlacement(placement({-102497,-687,1953},{0,.812328577f,0}));
  carSub_->setPlacement(placement({-60821,-55,-17230},{0,-1.50098312f,0}));
  car_->setPartRotation(10,{.436332315f,0,0});
  for(unsigned part:{11u,13u,15u})car_->setPartScale(part,{0,0,0});
  for(unsigned i=0;i<2;++i) {
    cops_[i]=add(i?"scr_r100_cop1":"scr_r100_cop0",i?0x26:0x24,0x25);
    add(i?"scr_r100_cop1_face":"scr_r100_cop0_face",i?0x29:0x27,i?0x2a:0x28,cops_[i]);
    cops_[i]->prepareMotions({{"idle",arc(room,i?0x2c:0x2b,"FCV")},{"talk",arc(room,i?0x36:0x35,"FCV")}});
    cops_[i]->setPlacement(placement({-102497,-687,1953},{0,.812328577f,0}));
    startCop(i,0,0);cops_[i]->rig()->advanceAction();
  }
  if(country) {
    auto *body=add("scr_r100_hanging",0x31,0x32);body->prepareStaticPose();
    body->setPlacement(placement({45442,-430,-8800},{0,0,0}));
    const auto em=RoomArchive::LoadDrs(enemy);
    auto hanging=std::make_unique<GenericEventActor>("scr_r100_hanging_detail","ROOM_ARC33",arc(room,0x33,"BIN"),arc(em,0x255,"TPL"));
    hanging->prepareStaticPose();hanging->setPlacement(placement({45442,-430,-8800},{0,0,0}));
    hanging->setLightMaskOverride(16);hanging->setDisplayOverride(true);actors_.push_back(std::move(hanging));
  }
  const auto cam=entry(room,"CAM");
  const unsigned camBase=16+cam.at(5)*64;
  for(unsigned i=0;i<cam.at(4);++i)if(cam.at(camBase+i*52+2)==6) {
    const unsigned cut=cam.at(camBase+i*52+1);
    cameras_[cut]=std::make_unique<R100RoomCamera>(cam,cut);
  }
  villageTimer_=random()%45+60;wasFirstLook_=state.room.test(3);
  if(wasFirstLook_ && !state.room.test(10)) {streamArmed_=false;streamHold_=true;}
  setAfterTruck(state.room.test(10));
}
void R100Secondary::startCop(unsigned i,unsigned clip,unsigned blend) {
  cops_.at(i)->rig()->startActionFlags(clip,blend,0,5);
}
void R100Secondary::setAfterTruck(bool after) {
  afterTruck_=after;
  if(!after)return;
  car_->setPlacement(placement({-120225,-14319,-2131},{-.159366012f,.893207133f,3.46660805f}));
  carSub_->setPlacement(placement({-120022,-14319.2725f,5740},{5.04974413f,.090861842f,-1.1017915f}));
  for(unsigned part:{11u,12u,13u,14u,16u})car_->setPartScale(part,{0,0,0});
  car_->setPartScale(15,{1,1,1});
  for(auto *cop:cops_)cop->setDisplayOverride(false);
}
bool R100Secondary::roomObjectVisible(unsigned work) const {
  if(work==0x37 || work==0x38)return afterTruck_;
  if(work==0x23 || work==0x44)return !afterTruck_;
  return true;
}
void R100Secondary::beginSourceEvent(unsigned phase,unsigned cameraCut) {
  if(phase>=6 && phase<=8) {
    noSuspend_[0]=true;noSuspend_[2]=phase==6;noSuspend_[3]=phase==7;
  } else if(phase==9 || (phase==11 && cameraCut==9)) {
    noSuspend_[0]=noSuspend_[1]=true;
  } else if(phase==10)noSuspend_[1]=true;
}
void R100Secondary::endSourceEvent(unsigned phase) {
  // MesCar00/MesCar01/EventBrige explicitly restore all four owners.
  // GakeEvent and HouseEvent do not clear their car NoSuspend flags.
  if(phase>=6 && phase<=8)noSuspend_.fill(false);
}
bool R100Secondary::actorVisibleDuringSuspend(const GenericEventActor &a) const {
  const auto &name=a.name();
  if(name=="scr_r100_car")return noSuspend_[0];
  if(name=="scr_r100_car_sub")return noSuspend_[1];
  if(name.rfind("scr_r100_cop0",0)==0)return noSuspend_[2];
  if(name.rfind("scr_r100_cop1",0)==0)return noSuspend_[3];
  // R100Init's hanging SetObjSmd models retain the default NoSuspend1.
  return true;
}
void R100Secondary::tickSuspended30Hz() {
  if(!afterTruck_)for(unsigned i=0;i<2;++i)if(noSuspend_[i+2])cops_[i]->rig()->advanceAction();
}
std::uint32_t R100Secondary::effectDisplayMask() const {
  return (~(std::uint32_t(1)<<17 | std::uint32_t(1)<<18)) |
    (afterTruck_?std::uint32_t(1)<<17:0) | (country_?std::uint32_t(1)<<18:0);
}
void R100Secondary::prepareMessages(const std::string &files,unsigned language) {
  if(language>5)throw std::runtime_error("r100: Invalid original message language");
  messageLanguage_=language;
  if(language)messageFont_=LoadOriginalMenuFont(files);
  else {
    // MesSet font4 + attr2 selects the actual stage font on Japanese.
    std::ifstream input(files+"/font/stage1_j.fnt",std::ios::binary|std::ios::ate);
    if(!input)throw std::runtime_error("r100: Original message font is missing");
    std::vector<std::uint8_t> bytes(std::size_t(input.tellg()));input.seekg(0);
    if(!input.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))throw std::runtime_error("r100: Truncated original message font");
    messageFont_=DecodeSubscreenFont(bytes,28,28);
  }
}
R100RoomMessage R100Secondary::roomMessage(unsigned number,unsigned attributes,unsigned x,unsigned y) const {
  if(messageFont_.sheets.empty())throw std::runtime_error("r100: Original message font not prepared");
  R100RoomMessage result;result.number=number;result.attributes=attributes;result.x=x;
  // MessageControl::gameInit2 sets layout0. R100Main obtains the logical
  // height/gap from this layout, independently of the atlas cell size.
  result.fontWidth=messageLanguage_?22:25;result.fontHeight=messageLanguage_?32:27;
  result.lineGap=messageLanguage_?25:27;result.charGap=messageLanguage_?-1:1;
  result.y=y?y:0x147-result.fontHeight-result.lineGap;result.font=4;
  result.words=ReadSubscreenMessage(messageTable_,number,messageLanguage_);
  result.glyphs=DecodeSubscreenGlyphs(result.words);
  for(const auto &glyph:result.glyphs)if(glyph.glyph>=messageFont_.bearings.size())
    throw std::runtime_error("r100: Original message glyph outside font bounds");
  return result;
}
void R100Secondary::installPlayerMotion(LeonPreview &player) {
  if(playerMotionOwner_==&player && playerMotionIndex_ && radioIntroMotionIndex_)return;
  playerMotionIndex_=player.installRoomScriptMotion(playerMotion_);
  radioIntroMotionIndex_=player.installRoomScriptMotion(radioIntroMotion_);
  playerMotionOwner_=&player;
}
void R100Secondary::beginRadioIntro(LeonPreview &player,WalkProbe &probe,unsigned callNo) {
  if(callNo>1 || !radioIntroMotionIndex_)throw std::runtime_error("r100: Original radio start not prepared");
  radioIntroTicks_=0;
  // OpeSetOpenTerm sets PlSetEyeMode(1) before MotionSetCore and the
  // hidden SceSleep(1), so the first displayed pose uses the FCV eyes.
  player.setRoomScriptEyeMode(1);
  player.beginRoomScriptMotion(probe,callNo?CollisionVec{-81500,860,-38900}:player.feet(),
      callNo?1.6f:player.heading(),*radioIntroMotionIndex_,0,0x201);
  // OpeSetOpenTerm keeps ScreenStop through SceSleep(1). Its first
  // visible radio pose has already received one player/motion tick; frame0
  // must never replace the final EVD picture during the window handoff.
  tickRadioIntro30Hz(player,probe);
}
void R100Secondary::tickRadioIntro30Hz(LeonPreview &player,WalkProbe &probe) {
  player.step(probe,0,0,false,false,1./30);++radioIntroTicks_;
  if(radioIntroTicks_==22)player.setRoomScriptLeftHand(2);
}
bool R100Secondary::radioIntroFinished(const LeonPreview &player) const {
  return radioIntroTicks_>=22 && player.roomScriptMotionEnded();
}
void R100Secondary::finishRadioIntro(LeonPreview &player) {
  if(radioHandVisible())player.setRoomScriptLeftHand(0x63);
  player.setRoomScriptEyeMode(0);
  player.endRoomScriptMotion();radioIntroTicks_=0;
}
Matrix34 R100Secondary::radioMatrix(const LeonPreview &player) const {
  const auto local=PlacementMatrix(placement({111,-22,66},{-.48869219f,.31415927f,-.73303829f}));
  // cObjWep::parentSet uses the real parent cParts matrix. Preserve the
  // original SDK concat's float association, especially its translation.
  const auto concat=[](const Matrix34 &a,const Matrix34 &b) {
    motion64::Mtx first{},second{},result{};
    std::memcpy(first,a.data(),sizeof(first));std::memcpy(second,b.data(),sizeof(second));
    motion64::PSMTXConcat(first,second,result);
    Matrix34 matrix{};std::memcpy(matrix.data(),result,sizeof(result));return matrix;
  };
  return concat(concat(player.placement(),player.rig().jointMatrices().at(16)),local);
}
void R100Secondary::beginHouse(LeonPreview &player,WalkProbe &probe) {
  if(!playerMotionIndex_)throw std::runtime_error("r100: Original Leon house scene motion is missing");
  beginCamera(10);player.beginRoomScriptMotion(probe,{-80540,-8,-12073},2.99f,*playerMotionIndex_,10,1);
}
void R100Secondary::finishHouse(LeonPreview &player) {player.endRoomScriptMotion();endCamera();}
void R100Secondary::beginCamera(unsigned cut) {
  const auto found=cameras_.find(cut);if(found==cameras_.end())throw std::runtime_error("r100: Original CAM UpCut is missing");
  corpseCameraActive_=false;activeCamera_=found->second.get();activeCamera_->reset();
}
void R100Secondary::beginCorpseCamera(const LeonPreview &player,const EnemyEncounter &corpse) {
  activeCamera_=nullptr;corpseCamera_=OriginalR100CorpseCamera(player.placement(),player.rig().jointMatrices(),
      corpse.placement(),corpse.actor().rig().jointMatrices());corpseCameraActive_=true;corpseNoSuspend_=true;
}
std::optional<R100SecondaryOffer> R100Secondary::corpseOffer(const RoomSaveState &state,
    const EnemyEncounter &corpse,const CollisionVec &feet,double heading,bool pressed,
    const std::optional<CollisionVec> &front) const {
  if(!state.room.test(10) || !pressed || !corpse.firstHouseEncounter() || corpse.alive() ||
      !corpse.visible() || !RoomFlow::hit(OriginalR100CorpseArea(corpse.placement()),feet,heading,front))return {};
  R100SecondaryOffer offer;offer.kind=R100SecondaryOffer::CorpseMessage;offer.area=0xff;offer.message=0xd;return offer;
}
void R100Secondary::endCamera() {activeCamera_=nullptr;corpseCameraActive_=false;}
std::optional<unsigned> R100Secondary::cameraCut() const {
  if(!activeCamera_)return {};
  return activeCamera_->lightArea();
}
const EventCamera &R100Secondary::camera() const {
  if(corpseCameraActive_)return corpseCamera_;
  if(!activeCamera_)throw std::runtime_error("r100: No active original CAM scene");
  return activeCamera_->camera();
}
unsigned R100Secondary::cameraFrame() const {return activeCamera_?activeCamera_->frame():0;}
void R100Secondary::tickCamera30Hz(bool advance) {if(activeCamera_)activeCamera_->tick30Hz(advance);}
void R100Secondary::tick30Hz(RoomSaveState &state,const CollisionVec &feet,double heading,
    bool pressed,bool dontFire,bool event,const std::vector<R100Threat> &ambush) {
  if(state.room.test(10)!=afterTruck_)setAfterTruck(state.room.test(10));
  if(!state.room.test(0) && RoomFlow::hit(scenario_.authoredArea(6),feet,heading))state.room.set(0);
  if(!state.room.test(1) && (RoomFlow::hit(scenario_.authoredArea(7),feet,heading) || RoomFlow::hit(scenario_.authoredArea(8),feet,heading)))state.room.set(1);
  if(!state.room.test(12) && state.room.test(3) && !RoomFlow::hit(scenario_.authoredArea(0xd),feet,heading) && !event) {
    state.room.set(12);for(unsigned number:{4u,5u,6u})sounds_.push_back({R100RoomSound::StreamFade,1,number,400,0,{}});
  }
  const bool skullDown=RoomFlow::hit(scenario_.authoredArea(0x22),feet,heading);
  const bool skullUp=RoomFlow::hit(scenario_.authoredArea(0x23),feet,heading);
  if(skullDown && !skullDownInside_)sounds_.push_back({R100RoomSound::RoomStreamVolume,0,0,400,0x2d,{}});
  if(skullUp && !skullUpInside_)sounds_.push_back({R100RoomSound::RoomStreamReset,0,0,400,0,{}});
  skullDownInside_=skullDown;skullUpInside_=skullUp;
  if(!afterTruck_) {
    for(auto *cop:cops_)cop->rig()->advanceAction();
    if(talkRemaining_) {
      if(!--talkRemaining_) {startCop(0,0,10);startCop(1,0,10);}
    } else if(dontFire && pressed) {
      const bool driver=RoomFlow::hit(scenario_.authoredArea(0x24),feet,heading);
      startCop(driver?0:1,1,10);talkRemaining_=unsigned(cops_[0]->rig()->maxFrame(driver?1:0));
      sounds_.push_back({R100RoomSound::Sample,6,driver?6u:5u,0,0,{-102497,-687,1953}});officerMessage_=true;
    }
  }
  const bool looked=state.room.test(3);
  if(!looked) {
    if(RoomFlow::hit(scenario_.houseFlagArea(),feet,heading) && villageTimer_ && !--villageTimer_) {
      villageTimer_=random()%30*5+180;
      sounds_.push_back({R100RoomSound::Sample,6,0,0,0,{-78589,860,-36700}});
    }
  } else if(!afterTruck_) {streamArmed_=false;streamHold_=true;}
  else if(!streamCooldown_) {
    bool found=false;for(const auto &actor:ambush)found=found||(actor.foundPlayer && actor.distanceSquared<900000000.f);
    if(state.room.test(4) || (!streamArmed_ && found))streamArmed_=true;
    if(streamArmed_ && !found)streamHold_=false;
    if(found || streamHold_) {sounds_.push_back({R100RoomSound::RoomStreamStart,1,0,0,0,{}});streamPlaying_=true;}
    else if(streamPlaying_) {sounds_.push_back({R100RoomSound::RoomStreamStop,0,3,0,0,{}});streamPlaying_=false;streamCooldown_=150;}
  } else --streamCooldown_;
  wasFirstLook_=looked;
}
std::vector<R100RoomSound> R100Secondary::takeSounds() {auto out=std::move(sounds_);sounds_.clear();return out;}
bool R100Secondary::takeOfficerMessage() {const bool value=officerMessage_;officerMessage_=false;return value;}
}
