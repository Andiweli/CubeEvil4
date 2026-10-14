#pragma once
#include "event_data.h"
#include "leon_preview.h"
#include "room_lighting.h"
#include <map>
namespace re4 {
// One original SetOm body or SetParts attachment. Attachments borrow the
// parent's posed skeleton and placement, as cModel::addModel does.
class GenericEventActor {
 public:
  GenericEventActor(std::string name,std::string binName,
      std::vector<std::uint8_t> bin,std::vector<std::uint8_t> tpl,
      GenericEventActor *parent=nullptr);
  ~GenericEventActor();
  void prepareMotions(const std::map<std::string,std::vector<std::uint8_t>> &motions);
  // Explicit ordinary modelInit rest pose for source actors with no motion.
  void prepareStaticPose();
  void prepareSpecular(const std::vector<Texture> &bank){AttachSceneSpecular(mesh_,bank);}
  void prepareGlobalIllumination(const Texture &texture) {
    if(globalIlluminationTexture_<0) {globalIlluminationTexture_=int(mesh_.textures.size());mesh_.textures.push_back(texture);}
  }
  int globalIlluminationTexture() const {return globalIlluminationTexture_;}
  float globalIlluminationScale() const {return model_->mesh.sourceFlags&0x20000000u?.25f:.5f;}
  void setPartsOffsets(const GenericEventActor &part);
  void startMotion(const std::string &asset,unsigned sourceFrame);
  void clearMotion();
  // External SetMod(type5) scroll owners keep their solved parts matrices
  // after MotionClear until an actual new motion evaluates those parts.
  void preservePoseOnMotionClear(bool value){preserveClearedPose_=value;}
  void sampleMotion(unsigned sourceFrame);
  void startShape(std::shared_ptr<const EventCurve> shape,unsigned sourceFrame,float weightScale=1);
  void sampleShape(unsigned sourceFrame);
  void clearShape();
  // SetObj18 assigns pos/ang directly; only an actual Pos packet rebuilds
  // LightInfo.imat. Keep its original zero-filled pool state until then.
  void resetEventPlacement();
  void setPlacement(const SmdPlacement &placement);
  void attach(GenericEventActor *parent,unsigned part);
  void forceHidden(bool hidden) {hidden_=hidden;}
  // Obj18CmfOn(...,5) keeps the callback's be_flag visibility independent
  // of ControlTransFlag and a parent's motion. be_flag bit2 means drawn.
  void setDisplayOverride(std::optional<bool> visible) {displayOverride_=visible;}
  void setLightMaskOverride(std::optional<unsigned> mask) {lightMaskOverride_=mask;}
  void setPartScale(unsigned part,const std::array<float,3> &scale);
  void setPartRotation(unsigned part,const std::array<float,3> &rotation);
  void setAlphaOverride(unsigned alpha);
  void setMaterialColor(const std::array<float,4> &color){materialColor_=color;}
  void setDepthModeOverride(unsigned mode);
  void setOrderTypeOverride(unsigned type);
  unsigned orderType() const {return sharedSkeleton_?parent_->orderType():orderType_;}
  const GenericEventActor *orderOwner() const {return sharedSkeleton_?parent_->orderOwner():this;}
  const std::array<float,4> &materialColor() const {return materialColor_;}
  unsigned depthMode() const {return depthMode_;}
  void clearPresentationOverrides();
  const std::string &name() const {return name_;}
  const std::string &binName() const {return binName_;}
  const StaticScene &mesh() const {return mesh_;}
  const std::vector<SkinCorner> &skin() const {return skin_;}
  AnimationRig *rig();
  const AnimationRig *rig() const;
  Matrix34 worldMatrix() const;
  bool visible() const;
  bool attachment() const {return parent_!=nullptr && sharedSkeleton_;}
  std::uint64_t meshRevision() const {return revision_;}
  float motionFrame() const {return motionFrame_;}
  unsigned lightMask() const;
  std::array<float,3> lightPosition() const;
  std::array<float,3> lightRootPosition() const;
  float lightRadius() const;
  float lightSize() const;
  RoomLightVolume lightVolume() const;
 private:
  void updateLightVolume();
  std::string name_,binName_;
  std::vector<std::uint8_t> bin_;
  std::unique_ptr<SkinnedModel> model_;
  StaticScene mesh_;
  std::vector<SkinCorner> skin_;
  std::vector<unsigned> cornerPositions_;
  std::vector<std::array<std::int16_t,3>> originalPositions_;
  std::map<std::string,unsigned> motionIndices_;
  std::vector<AnimationBytes> clips_;
  std::unique_ptr<AnimationRig> rig_;
  GenericEventActor *parent_{};
  unsigned parentPart_{};
  bool sharedSkeleton_{},hidden_{},motionActive_{};
  bool preserveClearedPose_{};
  std::optional<bool> displayOverride_;
  std::optional<unsigned> lightMaskOverride_;
  std::map<unsigned,std::array<float,3>> partScales_;
  std::map<unsigned,std::array<float,3>> originalPartRotations_;
  std::array<float,4> materialColor_{1,1,1,1};
  unsigned depthMode_{};
  unsigned orderType_{};
  int globalIlluminationTexture_=-1;
  unsigned motionStart_{},motionIndex_{};
  float motionFrame_{};
  SmdPlacement placement_;
  RoomLightVolume placedLightVolume_;
  std::shared_ptr<const EventCurve> shape_;
  unsigned shapeStart_{};
  float shapeWeightScale_=1;
  std::uint64_t revision_{};
};
}
