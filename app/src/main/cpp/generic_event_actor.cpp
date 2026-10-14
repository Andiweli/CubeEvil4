#include "generic_event_actor.h"
#include "asset_reader.h"
#include "weapon_preview.h"
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace re4 {
GenericEventActor::GenericEventActor(std::string name,std::string binName,
    std::vector<std::uint8_t> bin,std::vector<std::uint8_t> tpl,GenericEventActor *parent)
    :name_(std::move(name)),binName_(std::move(binName)),bin_(std::move(bin)),
     model_(std::make_unique<SkinnedModel>(bin_)),parent_(parent),sharedSkeleton_(parent!=nullptr) {
  placement_.scale={1,1,1};const auto &m=model_->mesh;AssetReader r{bin_};
  const auto positions=std::size_t(r.u32(48));
  for(unsigned i=0;i<m.positions.size();++i)
    originalPositions_.push_back({std::int16_t(r.u16(positions+i*8)),std::int16_t(r.u16(positions+i*8+2)),std::int16_t(r.u16(positions+i*8+4))});
  for(const auto &primitive:m.primitives) {
    if(primitive.triangles.empty()) continue;
    const auto &header=m.materialHeaders.at(primitive.displayList);SceneDraw d;
    d.first=mesh_.vertices.size()/3;d.count=primitive.triangles.size()*3;
    d.image=header[12];d.repeat=m.colored;d.vertexColors=m.colored&&m.vertexAmbient;
    SetSceneSpecular(d,header,m.sourceFlags);
    d.alphaTest=(header[11]&4)!=0;d.alphaImage=header[14];d.alphaReference=header[20];
    mesh_.draws.push_back(d);
    for(const auto &triangle:primitive.triangles) for(const auto corner:triangle) {
      const auto &ref=primitive.vertices.at(corner);const auto &p=m.positions.at(ref.position),&n=m.normals.at(ref.normal);
      const auto &uv=m.texcoords.at(ref.texcoord);mesh_.vertices.insert(mesh_.vertices.end(),p.begin(),p.end());
      mesh_.normals.insert(mesh_.normals.end(),n.begin(),n.end());mesh_.uv.insert(mesh_.uv.end(),uv.begin(),uv.end());
      const auto color=d.vertexColors?m.colors.at(ref.color):std::array<std::uint8_t,4>{255,255,255,255};
      mesh_.colors.insert(mesh_.colors.end(),color.begin(),color.end());
      skin_.push_back({model_->weights.at(model_->positionWeights.at(ref.position)),model_->weights.at(model_->normalWeights.at(ref.normal))});
      cornerPositions_.push_back(ref.position);
    }
    mesh_.triangles+=primitive.triangles.size();
  }
  mesh_.included=1;mesh_.minimum=m.minimum;mesh_.maximum=m.maximum;
  resetEventPlacement();
  std::vector<std::uint8_t> empty(16);empty[0]=64;Smd palettes(empty);palettes.palettes.push_back(std::move(tpl));
  AttachSceneTextures(palettes,mesh_);
  if(mesh_.vertices.empty() || mesh_.fallbackTriangles || mesh_.unresolvedAlphaTriangles)
    throw std::runtime_error("EVD actor: Geometry/texture missing: "+name_);
}
GenericEventActor::~GenericEventActor()=default;
void GenericEventActor::prepareStaticPose() {
  if (parent_ || rig_) throw std::runtime_error("Actor: Bind pose already prepared");
  rig_=std::make_unique<AnimationRig>(*model_);
}
void GenericEventActor::prepareMotions(const std::map<std::string,std::vector<std::uint8_t>> &motions) {
  if(sharedSkeleton_) return;
  motionIndices_.clear();clips_.clear();
  for(const auto &m:motions) {
    AssetReader r{m.second};
    // ExePacket_Mot explicitly ignores an authored empty weapon motion.
    if(!r.u16(0)) continue;
    motionIndices_.emplace(m.first,clips_.size());clips_.push_back({m.second,{}});
  }
  if(!clips_.empty()) rig_=std::make_unique<AnimationRig>(*model_,clips_);
  model_->validateSkeleton(model_->mesh.parts);
}
void GenericEventActor::setPartsOffsets(const GenericEventActor &part) {
  if(part.model_->mesh.joints.size()<model_->mesh.joints.size() || rig_)
    throw std::runtime_error("EVD: incomplete/late part bind pose");
  for(unsigned i=0;i<model_->mesh.joints.size();++i)
    model_->mesh.joints[i].center=part.model_->mesh.joints[i].center;
}
void GenericEventActor::startMotion(const std::string &asset,unsigned sourceFrame) {
  auto i=motionIndices_.find(asset);
  if(i==motionIndices_.end()) {motionActive_=false;return;}
  if(!rig_) throw std::runtime_error("EVD: Motion without a skeleton");
  rig_->clearEventMotion();
  motionIndex_=i->second;motionStart_=sourceFrame;motionActive_=true;motionFrame_=0;
  rig_->sample(motionIndex_,0);
}
void GenericEventActor::clearMotion() {
  motionActive_=false;
  if(rig_ && !sharedSkeleton_)rig_->clearEventMotion(!preserveClearedPose_);
  clearShape();
}
void GenericEventActor::sampleMotion(unsigned sourceFrame) {
  if(!motionActive_ || !rig_) return;
  const auto age=sourceFrame-motionStart_;motionFrame_=std::min(float(age),rig_->maxFrame(motionIndex_));
  rig_->sample(motionIndex_,motionFrame_);
  // ControlTransFlag hides only after the nonlooping sequence has ended.
  if(float(age)>rig_->maxFrame(motionIndex_)) motionActive_=false;
}
void GenericEventActor::resetEventPlacement() {
  placement_={};placement_.scale={1,1,1};
  placedLightVolume_={};placedLightVolume_.axes.fill(0);
  for(unsigned k=0;k<3;++k)
    placedLightVolume_.size[k]=(model_->mesh.maximum[k]-model_->mesh.minimum[k])*.5f;
  // cObjMgr::createBack clears the pool block before construction.
  // cLightInfo::init2 sets the bound/masks, but does not write imat.
  // SetObj18/modelInit also never call updateMatrix. With no Pos packet
  // lightHitCheckBBox therefore transforms a lamp to{0,0,0}; it accepts
  // every otherwise eligible lamp, independent of the animated root.
}
void GenericEventActor::setPlacement(const SmdPlacement &placement) {
  placement_=placement;
  // ExePacket_Pos calls setPos/setAng -> LightInfo.updateMatrix. MotionMove
  // and obj18SetOya subsequently pose the parts without rebuilding imat.
  updateLightVolume();
}
void GenericEventActor::attach(GenericEventActor *parent,unsigned part) {
  if(!parent || !parent->rig() || part>=parent->rig()->jointMatrices().size()) throw std::runtime_error("EVD: invalid parent parts");
  parent_=parent;parentPart_=part;sharedSkeleton_=false;
}
AnimationRig *GenericEventActor::rig() {return sharedSkeleton_?parent_->rig():rig_.get();}
const AnimationRig *GenericEventActor::rig() const {return sharedSkeleton_?parent_->rig():rig_.get();}
Matrix34 GenericEventActor::worldMatrix() const {
  if(sharedSkeleton_) return parent_->worldMatrix();
  const auto local=PlacementMatrix(placement_);
  if(!parent_) return local;
  auto matrix=WeaponAttachmentMatrix(local,WeaponAttachmentMatrix(parent_->rig()->jointMatrices().at(parentPart_),parent_->worldMatrix()));
  // ExePacket_Pos(0x40000000) uses OyaSetObj18, whose obj18SetOya
  // concatenates the posed parent parts and removes scale from each axis.
  // Weapon/passenger offsets keep their authored world translation.
  for(unsigned col=0;col<3;++col) {
    motion64::Vec axis{matrix[col],matrix[4+col],matrix[8+col]},unit{};
    motion64::PSVECNormalize(&axis,&unit);
    if(!std::isfinite(unit.x)||!std::isfinite(unit.y)||!std::isfinite(unit.z))
      throw std::runtime_error("EVD: invalid parent parts axis");
    matrix[col]=unit.x;matrix[4+col]=unit.y;matrix[8+col]=unit.z;
  }
  return matrix;
}
bool GenericEventActor::visible() const {
  if(hidden_) return false;
  if(displayOverride_) return *displayOverride_;
  if(parent_) return parent_->visible();
  return motionActive_;
}
unsigned GenericEventActor::lightMask() const {
  if(lightMaskOverride_)return *lightMaskOverride_;
  if(sharedSkeleton_)return parent_->lightMask();
  // Event::ExePacket_SetOm's prefix table -> SetObj18, with pl_type0.
  // The two policemen use pl07/PLXX and require0x40, not OBM mask4.
  if(name_.compare(0,4,"pl00")==0 || name_.compare(0,3,"wep")==0)return 1;
  if(name_.compare(0,2,"pl")==0 || name_.compare(0,5,"evm90")==0 || name_.compare(0,5,"evm82")==0)return 0x40;
  if(name_.compare(0,5,"evm72")==0)return 1;
  // More-specific prefixes precede EMXX/OBMXX in the original table.
  if(name_.compare(0,7,"em3300a")==0 || name_.compare(0,5,"evm52")==0)return 4;
  if(name_.compare(0,6,"em3300")==0 || name_.compare(0,5,"evm51")==0 || name_.compare(0,5,"evm53")==0)return 0x20;
  if(name_.compare(0,2,"em")==0 || (name_.compare(0,2,"ev")==0 && name_.compare(0,3,"evm")!=0))return 2;
  if(name_.compare(0,3,"eff")==0)return 8;
  if(name_.compare(0,5,"evm50")==0 || name_.compare(0,5,"evm54")==0 || name_.compare(0,5,"evm35")==0)return 2;
  if(name_.compare(0,3,"scr")==0)return 16;
  return 4;
}
void GenericEventActor::setPartScale(unsigned part,const std::array<float,3> &scale) {
  if(sharedSkeleton_ || !rig_)throw std::runtime_error("EVD: Part scaling has no independent body");
  rig_->setPartScale(part,scale);partScales_[part]=scale;
}
void GenericEventActor::setPartRotation(unsigned part,const std::array<float,3> &rotation) {
  if(sharedSkeleton_ || !rig_)throw std::runtime_error("Actor: Part rotation has no independent body");
  originalPartRotations_.emplace(part,rig_->partRotation(part));
  rig_->setPartRotation(part,rotation);
}
void GenericEventActor::clearPresentationOverrides() {
  hidden_=false;displayOverride_.reset();lightMaskOverride_.reset();
  materialColor_={1,1,1,1};depthMode_=orderType_=0;
  if(rig_)for(const auto &part:partScales_)rig_->setPartScale(part.first,{1,1,1});
  partScales_.clear();
  if(rig_)for(const auto &part:originalPartRotations_)rig_->setPartRotation(part.first,part.second);
  originalPartRotations_.clear();
}
void GenericEventActor::setAlphaOverride(unsigned alpha) {
  if(alpha>255)throw std::runtime_error("EVD: Material alpha exceeds original budget");
  materialColor_[3]=alpha/255.f;
}
void GenericEventActor::setDepthModeOverride(unsigned mode) {
  if(mode>2)throw std::runtime_error("EVD: Material Z mode exceeds original budget");
  depthMode_=mode;
}
void GenericEventActor::setOrderTypeOverride(unsigned type) {
  if(type>6)throw std::runtime_error("EVD: original model OT exceeds budget");
  orderType_=type;
}
float GenericEventActor::lightRadius() const {
  if(sharedSkeleton_)return parent_->lightRadius();
  float length=0;for(unsigned axis=0;axis<3;++axis) {
    const float extent=(model_->mesh.maximum[axis]-model_->mesh.minimum[axis])*.5f;
    length+=extent*extent;
  }
  return std::sqrt(length);
}
RoomLightVolume GenericEventActor::lightVolume() const {
  if(sharedSkeleton_)return parent_->lightVolume();
  if(parent_) {
    // ExePacket_Pos with flag0x40000000 changes LightInfo.Flag to1.
    // lightHitCheckSphere uses the live posed centre and unscaled Size.x.
    auto volume=placedLightVolume_;volume.type=1;volume.center=lightPosition();
    for(unsigned k=0;k<3;++k)volume.size[k]=(model_->mesh.maximum[k]-model_->mesh.minimum[k])*.5f;
    return volume;
  }
  return placedLightVolume_;
}
float GenericEventActor::lightSize() const {
  if(sharedSkeleton_)return parent_->lightSize();
  // trans_lit.cpp::LightSetModel uses max(Size.x, Size.y), not Radius.
  return std::max(model_->mesh.maximum[0]-model_->mesh.minimum[0],
      model_->mesh.maximum[1]-model_->mesh.minimum[1])*.5f;
}
void GenericEventActor::updateLightVolume() {
  auto &volume=placedLightVolume_;volume={};const auto matrix=PlacementMatrix(placement_);
  std::array<float,3> offset{};
  for(unsigned k=0;k<3;++k)offset[k]=(model_->mesh.minimum[k]+model_->mesh.maximum[k])*.5f-model_->mesh.joints.at(0).center[k];
  // lightHitCheckBBox uses updateMatrix: model angles rotate the offset,
  // while the animated root supplies only its world translation. getPos
  // is a separate operation for the diffuse-light position.
  volume.center=TransformPoint(matrix,offset);
  if(rig_) {
    const auto root=WeaponAttachmentMatrix(rig_->jointMatrices().at(0),matrix);
    for(unsigned k=0;k<3;++k)volume.center[k]+=root[k*4+3]-matrix[k*4+3];
  } else {
    const auto root=TransformPoint(matrix,model_->mesh.joints.at(0).center);
    for(unsigned k=0;k<3;++k)volume.center[k]+=root[k]-matrix[k*4+3];
  }
  for(unsigned axis=0;axis<3;++axis) {
    float scale=0;for(unsigned k=0;k<3;++k)scale+=matrix[k*4+axis]*matrix[k*4+axis];
    scale=std::sqrt(scale);
    volume.size[axis]=(model_->mesh.maximum[axis]-model_->mesh.minimum[axis])*.5f*scale;
    for(unsigned k=0;k<3;++k)volume.axes[axis*3+k]=scale?matrix[k*4+axis]/scale:0;
  }
}
std::array<float,3> GenericEventActor::lightPosition() const {
  if(sharedSkeleton_)return parent_->lightPosition();
  std::array<float,3> offset{};
  for(unsigned axis=0;axis<3;++axis)
    offset[axis]=(model_->mesh.minimum[axis]+model_->mesh.maximum[axis])*.5f-model_->mesh.joints.at(0).center[axis];
  // LightInfo init2(2,1): PartsNo1 addresses model rootparts0.
  if(rig_)return TransformPoint(WeaponAttachmentMatrix(rig_->jointMatrices().at(0),worldMatrix()),offset);
  for(unsigned axis=0;axis<3;++axis)offset[axis]+=model_->mesh.joints.at(0).center[axis];
  return TransformPoint(worldMatrix(),offset);
}
std::array<float,3> GenericEventActor::lightRootPosition() const {
  if(sharedSkeleton_)return parent_->lightRootPosition();
  if(rig_) {
    const auto root=WeaponAttachmentMatrix(rig_->jointMatrices().at(0),worldMatrix());
    return {root[3],root[7],root[11]};
  }
  return TransformPoint(worldMatrix(),model_->mesh.joints.at(0).center);
}
void GenericEventActor::startShape(std::shared_ptr<const EventCurve> shape,unsigned sourceFrame,float weightScale) {
  if(!std::isfinite(weightScale) || weightScale<0 || weightScale>2)throw std::runtime_error("Shape: Original weight factor");
  const AssetReader r{bin_};const auto p=std::size_t(r.u32(44));
  if(!p) throw std::runtime_error("EVD: Shape without a morph table: "+name_);
  const auto n=r.u32(p);if(n>4096) throw std::runtime_error("EVD: Morph table budget");r.range(p+4,n*8);
  for(const auto &channel:shape->channels()) if(channel.kind&4) {
    if(channel.index>=n) throw std::runtime_error("EVD: Shape index outside BIN bounds");
    const auto entry=p+4+channel.index*8,offset=p+4+r.u32(entry);const auto count=r.u32(entry+4);
    if(count>model_->mesh.positions.size()) throw std::runtime_error("EVD: Morph delta budget");
    r.range(offset,count*8);
    for(unsigned i=0;i<count;++i) if(r.u16(offset+i*8)>=originalPositions_.size()) throw std::runtime_error("EVD: Morph vertex index");
  }
  shape_=std::move(shape);shapeStart_=sourceFrame;shapeWeightScale_=weightScale;
}
void GenericEventActor::sampleShape(unsigned sourceFrame) {
  if(!shape_) return;
  auto positions=originalPositions_;const AssetReader r{bin_};const auto table=std::size_t(r.u32(44))+4;
  // ShapeMove flags2 holds nFrame-2, matching the source increment/clamp.
  const auto maximum=shape_->maxFrame()>1?shape_->maxFrame()-2:0;
  const auto age=std::min(sourceFrame-shapeStart_,maximum);
  for(unsigned k=0;k<shape_->channels().size();++k) {
    const auto &c=shape_->channels()[k];if(!(c.kind&4)) continue;
    auto weight=shape_->sample(k,float(age))[1]/100.f;weight*=shapeWeightScale_;if(weight==0) continue;
    const auto entry=table+c.index*8,offset=table+r.u32(entry);const auto count=r.u32(entry+4);
    for(unsigned i=0;i<count;++i) {
      const auto p=offset+i*8;const auto index=r.u16(p);
      for(unsigned axis=0;axis<3;++axis) {
        const auto delta=std::int16_t(r.u16(p+2+axis*2));
        const auto scaled=int(std::clamp(float(delta)*weight,-32768.f,32767.f));
        positions[index][axis]=std::int16_t(std::uint16_t(int(positions[index][axis])+scaled));
      }
    }
  }
  const auto scale=1.f/float(1u<<model_->mesh.shift);bool changed=false;
  for(unsigned corner=0;corner<cornerPositions_.size();++corner) for(unsigned axis=0;axis<3;++axis) {
    const auto value=positions[cornerPositions_[corner]][axis]*scale;
    auto &dst=mesh_.vertices[corner*3+axis];if(dst!=value) {dst=value;changed=true;}
  }
  if(changed) ++revision_;
}
void GenericEventActor::clearShape() {
  if(!shape_) return;
  shape_.reset();bool changed=false;
  for(unsigned corner=0;corner<cornerPositions_.size();++corner) for(unsigned axis=0;axis<3;++axis) {
    const auto value=model_->mesh.positions[cornerPositions_[corner]][axis];
    auto &dst=mesh_.vertices[corner*3+axis];if(dst!=value) {dst=value;changed=true;}
  }
  if(changed) ++revision_;
}
}
