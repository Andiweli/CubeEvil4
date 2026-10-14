#include "event_playback.h"
#include "room_objects.h"
#include "archive.h"
#include "asset_reader.h"
#include "weapon_preview.h"
#include "ports/yz2.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
namespace re4 {
namespace {
bool suffix(const std::string &s,const std::string &tail) {return s.size()>=tail.size() && s.compare(s.size()-tail.size(),tail.size(),tail)==0;}
std::vector<std::uint8_t> eventFontFile(const std::string &path) {
  std::ifstream file(path,std::ios::binary|std::ios::ate);
  const auto length=file.tellg();
  if(!file || length<8 || length>16*1024*1024)throw std::runtime_error("EVD: Font file missing/size budget");
  std::vector<std::uint8_t> bytes(std::size_t(length),0);file.seekg(0);
  if(!file.read(reinterpret_cast<char*>(bytes.data()),length))throw std::runtime_error("EVD: Truncated font file");
  return bytes;
}
}
OriginalEvent::OriginalEvent(const std::string &files,const std::string &name,unsigned language)
    :data_(EventData::Load(files+"/Evd/"+name+".evd")),language_(language) {
  if(data_.name()!=name || (name!="r100s03" && name!="r100s20" && name!="r100s40" &&
      name!="r120s00" && name!="r120s01" && name!="r100s30" && name!="r100s41" &&
      name!="r100s43" && name!="r100s44" && name!="r101s00" && name!="r101s21" && name!="r101s30") || language>5)
    throw std::runtime_error("EVD: unsupported event/language");
  font_=language==0?DecodeSubscreenFont(eventFontFile(files+"/font/event1_j.fnt"),28,28):LoadOriginalMenuFont(files);
  for(const auto &a:data_.assets()) {
    if(suffix(a.name,".fcv")) {
      // Camera and shape curves use EventCurve. Skeletal FCVs are parsed
      // by AnimationRig with their own motion flags and channel rules.
      const bool curve=std::any_of(data_.packets().begin(),data_.packets().end(),[&](const auto &p){
        return (p.type==6 && p.text(16,48)==a.name) || (p.type==12 && p.text(28,48)==a.name);
      });
      if(!curve)continue;
      const auto b=data_.asset(a.name);
      if(AssetReader{b}.u16(0))try {curves_[a.name]=std::make_shared<EventCurve>(b);}
      catch(const std::exception &error) {throw std::runtime_error("EVD "+name+" "+a.name+": "+error.what());}
    } else if(suffix(a.name,".lit")) {
      try {lights_[a.name]=std::make_shared<RoomLighting>(data_.asset(a.name));}
      catch(const std::exception &error) {throw std::runtime_error("EVD "+name+" "+a.name+": "+error.what());}
    }
    else if(suffix(a.name,".fcs")) {
      auto bytes=data_.asset(a.name);const AssetReader r{bytes};r.range(0,0x810);
      for(const auto base:{0u,0x404u}) {
        const auto count=r.u32(base);if(count>64)throw std::runtime_error("EVD: Focus key budget");
        for(unsigned k=0;k<count;++k) {
          const auto offset=base+4+k*16;const auto t=r.f32(offset);
          if(k && t<=r.f32(offset-16))throw std::runtime_error("EVD: Focus key order");
          for(unsigned field=0;field<4;++field)if(!std::isfinite(r.f32(offset+field*4)))
            throw std::runtime_error("EVD: non-finite focus curve");
        }
      }
      if(!std::isfinite(r.f32(0x808)) || !std::isfinite(r.f32(0x80c)))throw std::runtime_error("EVD: Focus level");
      focusAssets_.emplace(a.name,std::move(bytes));
    }
  }
  const RoomArchive room(LoadYz2(files+"/St1/"+name.substr(0,4)+".das").bytes);
  prepareActors(files,room);
  for(const auto &p:data_.packets()) if(p.type==15) {
    const auto block=unsigned(p.integer(16)),number=unsigned(p.integer(20));
    const auto key=std::make_pair(block,number);
    if(!streams_.count(key)) streams_[key]=SoundStream::Load(files+"/bgm/bio4str.hed",
        files+"/bgm/"+(block?"bio4evt.sbb":"bio4bgm.sbb"),block,number);
  }
  const auto core=RoomArchive::LoadDrs(files+"/etc/core.das");
  const auto *eff=core.find("EFF");if(!eff)throw std::runtime_error("EVD: CORE EFF is missing");
  const auto *roomEffect=room.find("EFF");
  const auto roomEffectBytes=roomEffect?room.copy(roomEffect-room.entries().data()):std::vector<std::uint8_t>{};
  std::vector<std::vector<std::uint8_t>> modelBanks;
  if(name.rfind("r101",0)==0)if(const auto *etm=room.find("ETM"))modelBanks.push_back(RoomObjectArchive(room.copy(etm-room.entries().data())).copy("et00.eff"));
  unsigned effectPackets=0;
  for(const auto &p:data_.packets()) if(p.type==28) {
    if(++effectPackets!=1)throw std::runtime_error("EVD: multiple effect banks are not supported");
    try {
      effectsData_=BuildEventWorldEffects(data_.asset(p.text(16,48)),core.copy(eff-core.entries().data()),roomEffectBytes,{},modelBanks);
    } catch(const std::exception &error) {
      throw std::runtime_error("EVD "+data_.name()+": "+error.what());
    }
  }
  for(const auto &packet:data_.packets())if(packet.type==13) {
    if(packet.integer(28)!=0 || !packet.text(16,12).empty() || packet.flags || packet.bytes.size()<=35)
      throw std::runtime_error("EVD: unported model-attached packet 13");
    const unsigned seq=packet.bytes[35];
    if(packetEffectTargets_.count(seq))continue;
    auto extra=BuildEventWorldEffects(roomEffectBytes,core.copy(eff-core.entries().data()),roomEffectBytes,{seq},modelBanks);
    if(extra.sprites.targets.size()!=1)throw std::runtime_error("EVD: room EST is missing");
    const auto target=unsigned(effectsData_.sprites.targets.size()),animation=unsigned(effectsData_.sprites.animations.size());
    packetEffectTargets_[seq]=target;
    for(auto &record:extra.sprites.targets[0].records)if(record.animation!=~0u)record.animation+=animation;
    for(auto &entry:extra.fragmentRecords){entry.target+=target;effectsData_.fragmentRecords.push_back(entry);}
    for(auto &entry:extra.eventModels){entry.target+=target;effectsData_.eventModels.push_back(entry);}
    for(auto &entry:extra.sprites.animations)effectsData_.sprites.animations.push_back(std::move(entry));
    effectsData_.sprites.targets.push_back(std::move(extra.sprites.targets[0]));
    effectsData_.sprites.recordParents.push_back(std::move(extra.sprites.recordParents[0]));
    effectsData_.sprites.recordEmitMatrices.push_back(std::move(extra.sprites.recordEmitMatrices[0]));
    effectsData_.sprites.recordPaths.push_back(std::move(extra.sprites.recordPaths[0]));
    effectsData_.targetPrerun.push_back(extra.targetPrerun[0]);effectsData_.sequences.push_back(seq);
  }
  if(!packetEffectTargets_.empty()) {
    std::vector<unsigned> sequences;for(const auto &entry:packetEffectTargets_)sequences.push_back(entry.first);
    fragmentData_=BuildEventDestructionEffects(roomEffectBytes,core.copy(eff-core.entries().data()),sequences,modelBanks);
    const auto *sat=room.find("SAT");if(sat) {
      effectSat_=std::make_unique<Sat>(room.copy(sat-room.entries().data()));
      effectFloor_=std::make_unique<SatCollision>(*effectSat_);
    }
  }
  effectSimulationData_=effectsData_.sprites;
  preflight();
  reset();
}
OriginalEvent::~OriginalEvent()=default;
void OriginalEvent::prepareActors(const std::string &files,const RoomArchive &room) {
  const auto player=RoomArchive::LoadDrs(files+"/em/pl00.drs");
  const auto specular=LoadCoreSpecular(files+"/etc/core.das");
  std::optional<Texture> globalIllumination;
  {
    // CORE_ARC(0x10), GlobalIlmTex[0]. ExePacket_SetOm enables this
    // material stage on its character/weapon types; EventCarInit also
    // enables it on the two otherwise ordinary PLXX policemen.
    const auto core=RoomArchive::LoadDrs(files+"/etc/core.das");
    globalIllumination=DecodeTpl(core.copy(0x10-4),0);
  }
  const std::pair<const char*,unsigned> playerNames[]={{"em/pl00/pl000a.bin",0},{"em/pl00/pl000a.tpl",1},
    {"em/pl00/pl000d.bin",5},{"em/pl00/pl000b.tpl",3},{"em/pl00/pl000e.bin",6},{"em/pl00/pl000l.bin",12}};
  for(const auto &n:playerNames) external_[n.first]=player.copy(n.second);
  if(data_.name().rfind("r100",0)==0) {
    // R100Init::EvtMgr.SetBin. ROOM_ARC_PTR numbers include the four
    // archive header words; bytewise ArchiveEntry indices start at word4.
    const std::pair<const char*,unsigned> roomNames[]={{"em/pl07/pl0700.bin",32},
      {"em/pl07/pl0700a.bin",34},{"em/pl07/pl0700.tpl",33},
      {"obj/objmodel/obm2a00.bin",25},{"obj/objmodel/obm2a00.tpl",26}};
    for(const auto &n:roomNames) {
      const auto expected=suffix(n.first,".tpl")?"TPL":"BIN";
      if(n.second>=room.entries().size() || !std::equal(expected,expected+3,room.entries()[n.second].tag.begin()))
        throw std::runtime_error("EVD: original r100 officer/car asset is missing");
      external_[n.first]=room.copy(n.second);
    }
  }
  const auto get=[&](const std::string &name) {
    if(data_.contains(name)) return data_.asset(name);
    const auto i=external_.find(name);if(i==external_.end()) throw std::runtime_error("EVD: external asset missing: "+name);
    return i->second;
  };
  std::map<std::string,std::map<std::string,std::vector<std::uint8_t>>> motions;
  for(const auto &p:data_.packets()) {
    if(p.type==3 || p.type==4) {
      const auto sourceName=p.text(16,12);const auto name=sourceName.empty()?"anonymous_"+std::to_string(actors_.size()):sourceName;GenericEventActor *parent=nullptr;
      if(p.type==4) parent=&actor(p.text(28,12));
      const auto bin=p.text(p.type==4?40:28,48),tpl=p.text(p.type==4?88:76,48);
      auto a=std::make_unique<GenericEventActor>(name,bin,get(bin),get(tpl),parent);
      a->prepareSpecular(specular);
      const auto prefix=[&](const char *s){return name.compare(0,std::strlen(s),s)==0;};
      const bool characterGi=prefix("pl00")||prefix("pl01")||prefix("pl02")||prefix("pl03")||prefix("pl04")||prefix("pl0c")||
          prefix("evm90")||prefix("em18")||prefix("evm54")||prefix("em2b")||prefix("evm72")||prefix("wep")||
          prefix("evm35")||prefix("em37")||prefix("em3300")||prefix("evm51")||prefix("evm52")||prefix("evm53")||prefix("em39");
      const bool carCopGi=data_.name().rfind("r120",0)==0&&(name=="pl0700"||name=="pl0700a");
      if(globalIllumination&&(characterGi||carCopGi||(parent&&parent->globalIlluminationTexture()>=0)))
        a->prepareGlobalIllumination(*globalIllumination);
      if(!sourceName.empty() && !actorNames_.emplace(name,a.get()).second) throw std::runtime_error("EVD: Duplicate actor");
      if(p.type==4 && (name=="ev0002" || name=="ev0102" || name=="ev0202" || name=="ev3002" || name=="ev0402"))
        parent->setPartsOffsets(*a);
      if(p.type==3) effectActors_.push_back(a.get());
      actors_.push_back(std::move(a));
    } else if(p.type==11) {
      const auto name=p.text(16,12),file=p.text(28,48);motions[name][file]=get(file);
    }
  }
  if(data_.name()=="r101s30") {
    const auto *smd=room.find("SMD");if(!smd)throw std::runtime_error("r101 bell: scroll SMD missing");
    const auto bytes=room.copy(smd-room.entries().data());const Smd source(bytes);
    const auto p=std::find_if(source.placements.begin(),source.placements.end(),[](const auto &p){return p.work==0x39;});
    if(p==source.placements.end() || (p->flags&0x40000000u))throw std::runtime_error("r101 bell: scroll 39 missing");
    const auto start=source.binOffsets.at(p->bin);auto end=bytes.size();
    for(const auto *table:{&source.binOffsets,&source.tplOffsets,&source.motionOffsets})for(auto offset:*table)if(offset>start)end=std::min(end,offset);
    auto scroll=std::make_unique<GenericEventActor>("scr0000","ROOM_SMD39",std::vector<std::uint8_t>(bytes.begin()+start,bytes.begin()+end),source.palettes.at(p->tpl));
    scroll->preservePoseOnMotionClear(true);
    scroll->prepareSpecular(specular);actorNames_.emplace("scr0000",scroll.get());effectActors_.push_back(scroll.get());actors_.push_back(std::move(scroll));
  }
  if(actors_.size()>96) throw std::runtime_error("EVD: Actor budget");
  for(auto &a:actors_) a->prepareMotions(motions[a->name()]);
  // Obj18 initializes and evaluates BIN parts even when the EVD gives the
  // model no motion. Windows still supply real part matrices to attached
  // lights; they must not be represented by an absent skeleton.
  for(auto &a:actors_)if(!a->attachment() && !a->rig())a->prepareStaticPose();
  // Every shared attachment must refer to the original owner's real parts.
  for(auto &a:actors_) if(a->attachment()) {
    if(!a->rig()) throw std::runtime_error("EVD: Parts without a parent skeleton");
    for(const auto &s:a->skin()) for(const auto *w:{&s.position,&s.normal})
      for(unsigned i=0;i<w->count;++i) if(w->joints[i]>=a->rig()->bones().size()) throw std::runtime_error("EVD: Part weight outside parent bounds");
  }
}
GenericEventActor &OriginalEvent::actor(const std::string &name) {
  const auto i=actorNames_.find(name);if(i==actorNames_.end()) throw std::runtime_error("EVD: Unknown actor: "+name);
  return *i->second;
}
void OriginalEvent::preflight() {
  std::string table;
  for(const auto &p:data_.packets()) {
    switch(p.type) {
      case 0:case 2:case 3:case 4:case 5:case 7:case 8:case 10:
      case 19:case 20:case 21:case 22:case 23:case 24:case 25:case 26:case 27:break;
      case 13:if(!packetEffectTargets_.count(p.bytes.at(35)))throw std::runtime_error("EVD: packet EST missing");break;
      case 6: {
        const auto i=curves_.find(p.text(16,48));
        if(i==curves_.end())throw std::runtime_error("EVD: Camera FCV is missing");
        std::set<unsigned> channels;
        for(unsigned j=0;j<i->second->channels().size();++j) {
          const auto index=i->second->channels()[j].index;
          if(index>3 || !channels.insert(index).second) throw std::runtime_error("EVD: Camera channel index");
          for(const auto frame:{0.f,float(i->second->maxFrame())}) {
            const auto sample=i->second->sample(j,frame);
            if(index==3 && (sample[1]<=0 || sample[1]>=3.1415927f))
              throw std::runtime_error("EVD: invalid camera FOV");
          }
        }
        if(channels.count(0)!=1 || channels.count(1)!=1 || channels.count(3)!=1)
          throw std::runtime_error("EVD: incomplete camera");
        break;
      }
      case 9: {
        const auto name=p.text(16,12),parent=p.text(28,12);
        if(name!="cam0000") actor(name);
        if(p.flags&0x80000000u) throw std::runtime_error("EVD: unsupported relative position rotation");
        if(p.flags&0x40000000u) {
          const auto &a=actor(parent);const auto part=p.integer(64);
          if(part<0 || !a.rig() || unsigned(part)>=a.rig()->jointMatrices().size())
            throw std::runtime_error("EVD: invalid parent parts");
        }
        break;
      }
      case 11:actor(p.text(16,12));break;
      case 12: {
        const auto i=curves_.find(p.text(28,48));if(i==curves_.end())throw std::runtime_error("EVD: Shape FCV is missing");
        auto &a=actor(p.text(16,12));a.startShape(i->second,0);a.clearShape();break;
      }
      case 14:if(!lights_.count(p.text(16,48)))throw std::runtime_error("EVD: LIT is missing");break;
      case 15:if(!streams_.count({unsigned(p.integer(16)),unsigned(p.integer(20))}))throw std::runtime_error("EVD: Stream is missing");break;
      case 31:if(!focusAssets_.count(p.text(16,48)))throw std::runtime_error("EVD: Focus data is missing");break;
      case 16:break; // SndCall's u16 block/number are interpreted by the sound owner.
      case 17:
        if(p.integer(20)<0 || p.integer(20)>100000)throw std::runtime_error("EVD: Subtitle time budget");
        if(p.integer(16)>=0 && language_!=1) {
          if(table.empty())throw std::runtime_error("EVD: Subtitles without MDT");
          const auto words=ReadSubscreenMessage(data_.asset(table),unsigned(p.integer(16)),language_);
          if(language_!=1)for(const auto &glyph:DecodeSubscreenGlyphs(words))
            if(glyph.glyph>=font_.bearings.size())
              throw std::runtime_error("EVD: Subtitle language has no matching original font");
        }
        break;
      case 28:if(!data_.contains(p.text(16,48)))throw std::runtime_error("EVD: EFF is missing");break;
      case 32:table=p.text(16,48);if(!data_.contains(table))throw std::runtime_error("EVD: MDT is missing");break;
      default:throw std::runtime_error("EVD: unimplemented executable packet type "+std::to_string(p.type));
    }
  }
  for(const auto &target:effectsData_.sprites.targets)for(const auto &r:target.records)
    if(r.enabled && r.bytes[7]<0xf8) {
      if(r.bytes[6]>=effectActors_.size() || !effectActors_[r.bytes[6]]->rig() ||
          r.bytes[7]>=effectActors_[r.bytes[6]]->rig()->jointMatrices().size())
        throw std::runtime_error("EVD EST: invalid actor parts");
    }
}
void OriginalEvent::tickEffects() {
  if(!effectsEnabled_)return;
  const auto sequence=(cut_/10)*16+cut_%10;
  unsigned prerun=0;
  if(effectsCutPending_) {
    const auto random=effectSimulation_?effectSimulation_->state().random:std::uint16_t(0x4e22);
    effectSimulation_.reset();effectSimulationData_.initialRandom=random;
    for(unsigned t=0;t<effectsData_.sprites.targets.size();++t) {
      const auto &source=effectsData_.sprites.targets[t];
      auto &target=effectSimulationData_.targets[t];
      target.records=source.sequence==sequence?source.records:std::vector<WaterRecord>{};
      if(source.sequence==sequence)prerun=effectsData_.targetPrerun[t];
    }
  }
  for(unsigned t=0;t<effectSimulationData_.targets.size();++t)
    for(unsigned r=0;r<effectSimulationData_.targets[t].records.size();++r) {
      const auto &record=effectSimulationData_.targets[t].records[r];
      auto &parent=effectSimulationData_.recordParents[t][r];
      if(record.enabled && record.bytes[7]<0xf8) {
        const auto *a=effectActors_.at(record.bytes[6]);
        parent=WeaponAttachmentMatrix(a->rig()->jointMatrices().at(record.bytes[7]),a->worldMatrix());
      } else parent.reset();
    }
  for(auto target:pendingEffectTargets_)effectSimulationData_.targets.at(target).records=effectsData_.sprites.targets.at(target).records;
  pendingEffectTargets_.clear();
  if(effectsCutPending_) {
    effectSimulation_=std::make_unique<EnvironmentSimulation>(effectSimulationData_);
    effectsCutPending_=false;
  }
  if(!effectSimulation_)return;
  EnvironmentCamera view;view.eye=camera_.eye;
  for(unsigned i=0;i<=prerun;++i)effectSimulation_->tick(&view,512,effectFloor_.get());
  if(fragments_) {
    constexpr Matrix34 identity{1,0,0,0,0,1,0,0,0,0,1,0};
    for(const auto &spawn:effectSimulation_->drainExternalSpawns()) {
      const auto &target=effectsData_.sprites.targets.at(spawn.target);
      if(!fragments_->triggerOriginalRecord(0xc4,target.sequence,spawn.record,spawn.target,identity,1,spawn.serial,spawn.seed))
        throw std::runtime_error("EVD: Original fragment spawn rejected");
    }
    fragments_->tick();
  }
  effectsState_={};static_cast<EnvironmentState&>(effectsState_)=effectSimulation_->state();
  // GPU consumes world coordinates. Simulation keeps genuine local physics
  // until CommonMove releases the original model parent.
  for(auto &sprite:effectsState_.sprites) if(sprite.parented) {
    ApplyEnvironmentSpriteMatrix(sprite,*effectSimulationData_.recordParents[sprite.target][sprite.record]);
    sprite.parented=false;
  }
}
void OriginalEvent::reset() {
  pendingEffectTargets_.clear();
  if(!fragmentData_.records.empty())fragments_=std::make_unique<DestructionEffectsSimulation>(fragmentData_,effectFloor_.get(),effectFloor_.get());
  cut_=frame_=totalFrame_=cameraStart_=0;nextPacket_=0;finished_=false;
  cameraCurve_.reset();camera_={};cameraBase_={1,0,0,0,0,1,0,0,0,0,1,0};subtitle_={};audio_.clear();unsupported_.clear();messageTable_.clear();
  lighting_.reset();posedLighting_.reset();effectSimulation_.reset();effectsState_={};
  focus_={};focusData_=nullptr;fadeStart_=fadeFrames_=0;fadeIn_=fadeActive_=false;
  callbackCut_=callbackFrame_=~0u;
  effectsState_.signature=effectsData_.sprites.signature;
  effectsEnabled_=effectsCutPending_=false;
  for(auto &a:actors_) {a->clearMotion();a->forceHidden(false);a->clearPresentationOverrides();a->resetEventPlacement();}
  if(data_.name()=="r120s00")setFade(false,0);
  else if(data_.name()=="r100s40")setFade(true,30);
  while(nextPacket_<data_.packets().size() && data_.packets()[nextPacket_].cut<=0 && data_.packets()[nextPacket_].frame==0)
    execute(data_.packets()[nextPacket_++]);
  evaluate();tickEffects();
}
void OriginalEvent::execute(const EventPacket &p) {
  switch(p.type) {
    case 0:case 2:case 3:case 4:case 5:case 7:case 8:case 10:
    case 19:case 20:case 21:case 22:case 23:case 24:case 25:case 27:
      // Original unused handlers are explicit; setup models were prepared.
      return;
    case 6: {
      const auto name=p.text(16,48);const auto i=curves_.find(name);
      if(i==curves_.end()) throw std::runtime_error("EVD: Camera FCV is missing");
      cameraCurve_=i->second;cameraStart_=totalFrame_;effectsCutPending_=true;focusData_=nullptr;focus_={};
      for(auto &a:actors_) if(!a->attachment()) a->clearMotion();
      break;
    }
    case 9: {
      const auto name=p.text(16,12),parentName=p.text(28,12);SmdPlacement placement;placement.scale={1,1,1};
      for(unsigned i=0;i<3;++i) {placement.position[i]=float(p.integer(40+i*4));placement.rotation[i]=float(p.integer(52+i*4))*3.1415927f/180.f;}
      if(p.flags&0x80000000u) {
        if(parentName.empty() || parentName=="oya0000") throw std::runtime_error("EVD: relative base is missing");
        placement.position=TransformPoint(actor(parentName).worldMatrix(),placement.position);
        unsupported_.push_back("Pos relative rotations: "+name);
      }
      if(name=="cam0000") cameraBase_=PlacementMatrix(placement);
      else {auto &a=actor(name);a.setPlacement(placement);if(p.flags&0x40000000u) a.attach(&actor(parentName),unsigned(p.integer(64)));}
      break;
    }
    case 11: {
      auto &a=actor(p.text(16,12));a.startMotion(p.text(28,48),totalFrame_);
      // ExePacket_Mot ClrShape resets all attached face models of this body.
      for(auto &part:actors_) if(part->attachment() && part->rig()==a.rig()) part->clearShape();
      break;
    }
    case 12: {
      const auto file=p.text(28,48);const auto i=curves_.find(file);
      if(i==curves_.end()) throw std::runtime_error("EVD: Shape FCV is missing");
      actor(p.text(16,12)).startShape(i->second,totalFrame_);break;
    }
    case 14: {
      const auto file=p.text(16,48);const auto i=lights_.find(file);if(i==lights_.end()) throw std::runtime_error("EVD: Light data is missing");
      lighting_=i->second;posedLighting_.reset();break;
    }
    case 15:case 16: {
      EventAudioCue cue;cue.stream=p.type==15;cue.sourceFrame=totalFrame_;cue.eventName=data_.name();
      if(cue.stream) {cue.block=unsigned(p.integer(16));cue.number=unsigned(p.integer(20));cue.synchronized=cue.block!=0 && !(p.flags&0x20000000u);}
      else {cue.block=unsigned(p.integer(16))&0xffff;cue.number=unsigned(p.integer(20))&0xffff;}
      audio_.push_back(std::move(cue));break;
    }
    case 17: {
      subtitle_={};subtitle_.message=p.integer(16);subtitle_.remainingFrames=unsigned(std::max(0,p.integer(20)));subtitle_.tableAsset=messageTable_;
      if(subtitle_.message>=0 && language_!=1) {
        if(messageTable_.empty()) throw std::runtime_error("EVD: Subtitles without MDT");
        subtitle_.words=ReadSubscreenMessage(data_.asset(messageTable_),unsigned(subtitle_.message),language_);
        subtitle_.glyphs=DecodeSubscreenGlyphs(subtitle_.words);
      }
      break;
    }
    case 31:focusData_=&focusAssets_.at(p.text(16,48));break;
    case 26:effectSimulation_.reset();effectsState_.sprites.clear();effectsState_.emitters.clear();break;
    case 28:effectsEnabled_=true;break;
    case 32:messageTable_=p.text(16,48);if(!data_.contains(messageTable_)) throw std::runtime_error("EVD: MDT is missing");break;
    case 13:pendingEffectTargets_.push_back(packetEffectTargets_.at(p.bytes.at(35)));effectsEnabled_=true;break;
    case 1:case 18:case 29:case 30:
      throw std::runtime_error("EVD: packet type not implemented yet "+std::to_string(p.type));
    default:throw std::runtime_error("EVD: Unknown packet type");
  }
}
void OriginalEvent::setFade(bool fadeIn,unsigned frames) {
  fadeIn_=fadeIn;fadeFrames_=frames;fadeStart_=totalFrame_;fadeActive_=true;
}
unsigned OriginalEvent::fadeAlpha() const {
  if(!fadeActive_)return 0;
  const auto age=totalFrame_>=fadeStart_?totalFrame_-fadeStart_:0;
  if(!fadeFrames_ || age>=fadeFrames_)return fadeIn_?0:255;
  // FadeControl uses integer colour interpolation, including truncation.
  return fadeIn_?(255u*(fadeFrames_-age))/fadeFrames_:(255u*age)/fadeFrames_;
}
bool OriginalEvent::drawsRoomBackground() const {
  // Disp_flg are suppression bits: trans.cpp::objTrans returns while
  // DPF_SCR is set. s00 sets it in cut0 and clears it in cut1/default;
  // s01 inherits that cleared state and hides only r120_setTrans's list.
  if(data_.name()=="r120s00")return cut_!=0;
  return true;
}
std::vector<unsigned> OriginalEvent::hiddenRoomObjects() const {
  if(data_.name()=="r101s30")return {0x39};
  if(data_.name()=="r100s40")return {0x44}; // r100_StartEvent hides the source scroll officer until the event ends.
  if(data_.name()!="r120s00" && data_.name()!="r120s01")return {};
  std::vector<unsigned> result;
  for(const auto &range:{std::pair<unsigned,unsigned>{0x1c,0x1f},{0x25,0x29},
      {0x2a,0x2c},{0x30,0x32},{0x20,0x24},{3,0xc}})
    for(unsigned i=range.first;i<=range.second;++i)result.push_back(i);
  if(data_.name()=="r120s01")for(unsigned i=0x17;i<=0x1a;++i)result.push_back(i);
  return result;
}
void OriginalEvent::initCarActors() {
  // r120.cpp::EventCarInit. SetObj18's mask4 is overridden for the
  // dedicated window models; passenger and car parts retain authored FCVs.
  for(const auto *name:{"obm3000a","obm3000b","obm3000c","obm3000d","obm3000e","obm3000f"}) {
    const auto found=actorNames_.find(name);
    if(found!=actorNames_.end()) {
      found->second->setLightMaskOverride(2);
      found->second->setDepthModeOverride(1);
      found->second->setOrderTypeOverride(1);
    }
  }
  const auto car=actorNames_.find("obm1a00");
  if(car!=actorNames_.end()) {
    car->second->setOrderTypeOverride(4);
    for(unsigned part:{0x12u,0x13u,0x14u,0x15u,0x16u,0x17u,0xfu})car->second->setPartScale(part,{0,0,0});
  }
  const auto leon=actorNames_.find("pl0000");
  if(leon!=actorNames_.end())leon->second->setOrderTypeOverride(1);
  const auto clone=actorNames_.find("pl0010");
  if(clone!=actorNames_.end())clone->second->setOrderTypeOverride(4);
  // Both r120 event callbacks explicitly put the RTT mirror surface in
  // OT1. It is not one of EventCarInit's obm3000a..f window attachments.
  const auto mirror=actorNames_.find("obm3010f");
  if(mirror!=actorNames_.end())mirror->second->setOrderTypeOverride(1);
}
void OriginalEvent::roomCallback() {
  if(data_.name()=="r101s30") {actor("scr0000").setDisplayOverride(true);actor("scr0000").setOrderTypeOverride(2);}
  if(callbackCut_==cut_ && callbackFrame_==frame_)return;
  callbackCut_=cut_;callbackFrame_=frame_;
  const auto show=[&](const char *name,bool on) {
    const auto found=actorNames_.find(name);
    if(found!=actorNames_.end())found->second->setDisplayOverride(on);
  };
  const auto mask=[&](const char *name,unsigned value) {
    const auto found=actorNames_.find(name);
    if(found!=actorNames_.end())found->second->setLightMaskOverride(value);
  };
  const auto depth=[&](const char *name,unsigned value) {
    const auto found=actorNames_.find(name);
    if(found!=actorNames_.end())found->second->setDepthModeOverride(value);
  };
  const auto alpha=[&](const char *name,unsigned value) {
    const auto found=actorNames_.find(name);
    if(found!=actorNames_.end())found->second->setAlphaOverride(value);
  };
  if(data_.name()=="r100s03") {
    if(frame_==0)show("wep0200",cut_<=4 || cut_>=13);
    return;
  }
  if(data_.name()=="r100s40") {
    if(cut_==0 && frame_==0)initCarActors();
    return;
  }
  if(data_.name()=="r120s00") {
    if(frame_==0) {
      if(cut_==0) {
        initCarActors();mask("pl0010",0x20);depth("obm3010f",1);
        show("pl0010",true);show("obm3010f",true);
      } else if(cut_==1) {
        show("pl0010",false);show("obm3010f",false);
      }
      if(cut_==0)show("obm3000f",false);
      else if(cut_==2 || cut_==6)show("obm3000a",false);
      else {show("obm3000a",true);show("obm3000f",true);}
    }
    if(cut_==0 && frame_==120)setFade(true,60);
    if(cut_==8 && frame_==50)setFade(false,60);
    return;
  }
  if(data_.name()=="r120s01") {
    if(frame_==0) {
      if(cut_==3) {show("obm3000a",false);show("obm3000e",false);}
      else if(cut_==5)show("obm3000f",false);
      else if(cut_==14)for(const auto *name:{"obm3000a","obm3000b","obm3000e","obm3000f"})show(name,false);
      else for(const auto *name:{"obm3000a","obm3000b","obm3000e","obm3000f"})show(name,true);
      switch(cut_) {
        case 0:
          initCarActors();mask("pl0010",0x20);mask("evm0000",0x20);depth("obm3010f",1);
          show("pl0010",false);show("obm3010f",false);setFade(true,150);break;
        case 2:case 6:
          show("pl0010",true);show("obm3010f",true);show("obm3000f",false);
          if(cut_==6)alpha("obm3010f",0xc0);
          break;
        case 3:case 7:
          show("pl0010",false);show("obm3010f",false);show("obm3000f",true);
          if(cut_==7)alpha("obm3010f",0xff);
          break;
        default:break;
      }
    }
    if(cut_==14 && frame_==370)setFade(false,215);
  }
}
void OriginalEvent::sampleFocus() {
  focus_={}; // Filter01Trans clears both per-frame explicit requests.
  if(!focusData_)return;
  const AssetReader reader{*focusData_};
  const auto sample=[&](unsigned base,float &out) {
    const auto count=reader.u32(base);const auto t=float(frame_);
    if(!count || t<reader.f32(base+4) || t>reader.f32(base+4+(count-1)*16))return false;
    out=0;
    for(unsigned i=0;i+1<count;++i) {
      const auto a=base+4+i*16,b=a+16;
      const auto t0=reader.f32(a),t1=reader.f32(b);
      if(t<t0 || t>t1)continue;
      // Original hermite.cpp::Hermite_1, in the original float order.
      const float dt=t1-t0,s=(t-t0)/dt,s2=s*s,s3=s*s2;
      const float h01=-(s3+s3)+3.f*s2,h11=s3-s2,h10=h11-s2+s,h00=-h01+1.f;
      out=h00*reader.f32(a+4)+h01*reader.f32(b+4)+
          dt*(h10*reader.f32(a+8)+h11*reader.f32(b+12));
      return true;
    }
    return true;
  };
  if(sample(0,focus_.nearDepth)) {focus_.nearEnabled=true;focus_.nearLevel=reader.f32(0x808);}
  if(sample(0x404,focus_.farDepth)) {focus_.farEnabled=true;focus_.farLevel=reader.f32(0x80c);}
}
void OriginalEvent::updateRoomLights() {
  if(data_.name()!="r120s00" || (cut_!=1 && cut_!=8) || !lighting_)return;
  // Evt_R120S00_Func: getKindLight(1)->setParent(obm3000c). The original
  // parts number remains in ParentNo's upper half; convert that real posed
  // part into world coordinates before the generic room GPU light selector.
  auto &parent=actor("obm3000c");if(!parent.rig())throw std::runtime_error("EVD: Light parent has no skeleton");
  if(!posedLighting_)posedLighting_=std::make_shared<RoomLighting>(*lighting_);
  for(unsigned cut=0;cut<lighting_->cuts.size();++cut) {
    const auto &original=lighting_->cuts[cut];auto &posed=posedLighting_->cuts[cut];
    for(unsigned i=0;i<original.lights.size();++i) {
      const auto &source=original.lights[i];if(source.kind!=1)continue;
      const auto part=source.parentNo>>16;
      if(part>=parent.rig()->jointMatrices().size())throw std::runtime_error("EVD: Light parent parts outside original model bounds");
      auto &light=posed.lights[i];
      const auto matrix=WeaponAttachmentMatrix(parent.rig()->jointMatrices()[part],parent.worldMatrix());
      light.position=TransformPoint(matrix,source.position);
      if(source.type==3 || source.type==4 || source.type==6) {
        light.direction={};for(unsigned k=0;k<3;++k)for(unsigned j=0;j<3;++j)light.direction[k]+=matrix[k*4+j]*source.direction[j];
      }
      light.parent=0;light.parentNo=1;
      break; // cLightMgr::getKindLight returns the first alive kind1 light.
    }
  }
}
void OriginalEvent::evaluate() {
  for(auto &a:actors_) if(!a->attachment()) a->sampleMotion(totalFrame_);
  for(auto &a:actors_) a->sampleShape(totalFrame_);
  roomCallback();sampleFocus();updateRoomLights();
  if(cameraCurve_) {
    EventCamera result;bool eye=false,target=false,fov=false;
    const auto age=float(totalFrame_-cameraStart_);
    for(unsigned i=0;i<cameraCurve_->channels().size();++i) {
      const auto value=cameraCurve_->sample(i,age);
      switch(cameraCurve_->channels()[i].index) {
        case 0:result.eye=TransformPoint(cameraBase_,value);eye=true;break;
        case 1:result.target=TransformPoint(cameraBase_,value);target=true;break;
        case 2:result.roll=value[1];break;
        case 3:result.fovRadians=value[1];fov=true;break;
        default:throw std::runtime_error("EVD: Camera channel index");
      }
    }
    result.valid=eye&&target&&fov && result.fovRadians>0 && result.fovRadians<3.1415927f;
    if(!result.valid) throw std::runtime_error("EVD: incomplete/invalid camera");
    camera_=result;
  }
}
void OriginalEvent::tick30Hz(bool advance) {
  if(!advance || finished_) return;
  if(subtitle_.remainingFrames && !--subtitle_.remainingFrames) subtitle_={};
  ++totalFrame_;++frame_;
  if(frame_>=data_.cutFrames().at(cut_)) {frame_=0;++cut_;}
  if(totalFrame_>=data_.totalFrames()) {
    while(nextPacket_<data_.packets().size()) {
      const auto &p=data_.packets()[nextPacket_++];
      if(p.type!=26 && p.type!=27) throw std::runtime_error("EVD: Active packet after the last cut");
      execute(p);
    }
    finished_=true;return;
  }
  while(nextPacket_<data_.packets().size()) {
    const auto &p=data_.packets()[nextPacket_];
    if(p.cut>int(cut_) || (p.cut==int(cut_) && p.frame>int(frame_))) break;
    execute(p);++nextPacket_;
  }
  evaluate();tickEffects();
}
void OriginalEvent::finish() {
  // RunEvtCancel processes the remaining executable packets at their cut
  // boundaries. Preserve final source placement/music commands on skip.
  unsigned previousCut=cut_;
  const auto cutStart=[&](unsigned cut) {
    unsigned frame=0;for(unsigned i=0;i<cut;++i) frame+=data_.cutFrames()[i];return frame;
  };
  while(nextPacket_<data_.packets().size()) {
    const auto &p=data_.packets()[nextPacket_++];
    if(p.cut>=0 && unsigned(p.cut)<data_.cutFrames().size()) {
      if(unsigned(p.cut)!=previousCut) {
        cut_=previousCut;frame_=data_.cutFrames()[cut_]-1;totalFrame_=cutStart(cut_)+frame_;evaluate();
        previousCut=unsigned(p.cut);
      }
      cut_=unsigned(p.cut);frame_=unsigned(p.frame);totalFrame_=cutStart(cut_)+frame_;
    }
    execute(p);
  }
  cut_=unsigned(data_.cutFrames().size()-1);frame_=data_.cutFrames().back()-1;
  totalFrame_=data_.totalFrames()-1;evaluate();
  finished_=true;audio_.clear();subtitle_={};effectSimulation_.reset();
  effectsState_.sprites.clear();effectsState_.emitters.clear();
}
std::vector<EventAudioCue> OriginalEvent::takeAudioCues() {auto out=std::move(audio_);audio_.clear();return out;}
std::shared_ptr<const SoundStream> OriginalEvent::audioStream(const EventAudioCue &cue) const {
  const auto i=streams_.find({cue.block,cue.number});
  return cue.stream && i!=streams_.end()?i->second:nullptr;
}
std::optional<SmdPlacement> OriginalEvent::endPlayerPlacement() const {
  if(data_.name()=="r100s20")return std::nullopt; // r100_Sce_car: PlPosNoSet.
  const auto i=actorNames_.find("pl0000");
  if(i==actorNames_.end() || !i->second->rig() || i->second->rig()->jointMatrices().size()<2)return std::nullopt;
  const auto *a=i->second;SmdPlacement placement;placement.scale={1,1,1};
  const auto root=WeaponAttachmentMatrix(a->rig()->jointMatrices()[0],a->worldMatrix());
  const auto child=WeaponAttachmentMatrix(a->rig()->jointMatrices()[1],a->worldMatrix());
  placement.position={root[3],root[7],root[11]};
  placement.rotation[1]=std::atan2(child[2],child[10]);
  return placement;
}
std::string OriginalEvent::describe() const {
  std::ostringstream out;out<<data_.name()<<": "<<data_.assets().size()<<" assets, "<<data_.packets().size()<<" packets, "
    <<data_.cutFrames().size()<<" cuts, "<<data_.totalFrames()<<" source frames, "<<actors_.size()<<" actors";return out.str();
}
}
