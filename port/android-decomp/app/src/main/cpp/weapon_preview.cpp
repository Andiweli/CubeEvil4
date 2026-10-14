#include "weapon_preview.h"
#include "archive.h"
#include "weapon_carry_source.h"
#include "inventory_case.h"
#include "generic_event_actor.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace re4 {
namespace {
std::vector<std::uint8_t> asset(const RoomArchive &archive, unsigned index,
                                const std::string &tag) {
  if (index >= archive.entries().size())
    throw std::runtime_error("WEP01: Asset is missing");
  const auto &entry = archive.entries()[index];
  if (!std::equal(tag.begin(), tag.end(), entry.tag.begin()))
    throw std::runtime_error("WEP01: Asset type mismatch");
  return archive.copy(index);
}
} // namespace

Matrix34 WeaponAttachmentMatrix(const Matrix34 &hand,
                                const Matrix34 &actorPlacement) {
  for (const auto *matrix : {&hand, &actorPlacement})
    for (auto value : *matrix)
      if (!std::isfinite(value))
        throw std::runtime_error("WEP01: invalid attachment matrix");
  Matrix34 result{};
  for (unsigned row = 0; row < 3; ++row)
    for (unsigned col = 0; col < 4; ++col) {
      float value = col == 3 ? actorPlacement[row * 4 + 3] : 0;
      for (unsigned k = 0; k < 3; ++k)
        value += actorPlacement[row * 4 + k] * hand[k * 4 + col];
      if (!std::isfinite(value))
        throw std::runtime_error("WEP01: Attachment matrix overflow");
      result[row * 4 + col] = value;
    }
  return result;
}

WeaponPreview::WeaponPreview(const std::string &weaponDrs, WeaponKind kind) : kind_(kind),itemId_(kind==WeaponKind::ShotgunWep07?0x2c:kind==WeaponKind::HandgunWep02?0x23:0x21),sourcePath_(weaponDrs) {
  const auto archive = RoomArchive::LoadDrs(weaponDrs);
  effects_ = asset(archive, 0, "EFF");
  // cObjFn57::init: normal model WEP_ARC_PTR(6), palette PTR(5).
  // Four header words precede the archive table's entry zero.
  const bool shotgun = kind_ == WeaponKind::ShotgunWep07;
  SkinnedModel model(asset(archive, shotgun ? 1 : 2, "BIN"));
  if (model.mesh.joints.size() < 4 || model.mesh.joints.size() > 16)
    throw std::runtime_error("WEP01: Incorrect local skeleton");
  model.validateSkeleton(model.mesh.joints.size());
  std::vector<AnimationBytes> clips;
  // cObjWep::resetMotion / cObjFn57::{moveFire,moveReload}, normal
  // reload tune. Each local weapon motion is an FCV without a SEQ.
  // Nonempty: ARC34/32/36; empty: ARC39/37/33.
  const std::array<unsigned, 6> indices = shotgun
      ? std::array<unsigned, 6>{45, 44, 39, 45, 46, 39}
      : kind_ == WeaponKind::HandgunWep02
          ? std::array<unsigned, 6>{50, 48, 52, 55, 53, 49}
          : std::array<unsigned, 6>{48, 46, 50, 53, 51, 47};
  for (unsigned index : indices)
    clips.push_back({asset(archive, index, "FCV"), {}});
  rig_ = std::make_unique<AnimationRig>(model, clips);

  std::vector<std::uint8_t> emptyHeader(16);
  emptyHeader[0] = 64;
  Smd palettes(emptyHeader);
  palettes.palettes.push_back(asset(archive, shotgun ? 2 : 1, "TPL"));
  const auto &mesh = model.mesh;
  for (const auto &primitive : mesh.primitives) {
    if (primitive.triangles.empty())
      continue;
    const auto &header = mesh.materialHeaders.at(primitive.displayList);
    SceneDraw draw;
    draw.first = mesh_.vertices.size() / 3;
    draw.count = primitive.triangles.size() * 3;
    draw.palette = 0;
    draw.image = header[12];
      SetSceneSpecular(draw,header,mesh.sourceFlags);
    draw.repeat = mesh.colored;
    draw.vertexColors = mesh.colored && mesh.vertexAmbient;
    draw.alphaTest = (header[11] & 4) != 0;
    draw.alphaImage = header[14];
    draw.alphaReference = header[20];
    mesh_.draws.push_back(draw);
    for (const auto &triangle : primitive.triangles)
      for (auto corner : triangle) {
        const auto &reference = primitive.vertices.at(corner);
        const auto &point = mesh.positions.at(reference.position);
        const auto &normal = mesh.normals.at(reference.normal);
        const auto &uv = mesh.texcoords.at(reference.texcoord);
        mesh_.vertices.insert(mesh_.vertices.end(), point.begin(), point.end());
        mesh_.normals.insert(mesh_.normals.end(), normal.begin(), normal.end());
        mesh_.uv.insert(mesh_.uv.end(), uv.begin(), uv.end());
        const auto color =
            draw.vertexColors ? mesh.colors.at(reference.color)
                              : std::array<std::uint8_t, 4>{255, 255, 255, 255};
        mesh_.colors.insert(mesh_.colors.end(), color.begin(), color.end());
        skin_.push_back(
            {model.weights.at(model.positionWeights.at(reference.position)),
             model.weights.at(model.normalWeights.at(reference.normal))});
      }
    mesh_.triangles += primitive.triangles.size();
  }
  mesh_.included = 1;
  AttachSceneTextures(palettes, mesh_);
  if (mesh_.vertices.empty() || mesh_.fallbackTriangles ||
      mesh_.unresolvedAlphaTriangles)
    throw std::runtime_error("WEP01: Geometry or texture source is missing");
  sample(WeaponMotion::Idle, 0);
}

WeaponPreview::WeaponPreview(const std::string &path,unsigned id,const std::string &playerPath)
    :kind_(WeaponKind::OriginalCarry),itemId_(id),sourcePath_(path) {
  const auto *source=OriginalWeaponCarry(id);
  if(!source)throw std::runtime_error("Weapon: Original carry profile missing");
  const auto archive=RoomArchive::LoadDrs(path);
  const auto player=RoomArchive::LoadDrs(playerPath);
  const auto &geometry=source->playerModel?player:archive;
  SkinnedModel model(asset(geometry,source->model-4,"BIN"));
  model.validateSkeleton(model.mesh.joints.size());
  parentPart_=source->parentPart;
  const auto *action=OriginalWeaponAction(source->number);
  if(!action)throw std::runtime_error("Weapon: Original action profile missing");
  std::vector<AnimationBytes> clips;
  for(unsigned arc:action->object)if(arc)clips.push_back({asset(archive,arc-4,"FCV"),{},false});
  if(action->number==9) {
    clips.push_back({asset(archive,0x24-4,"FCV"),{},false});
    clips.push_back({asset(archive,0x21-4,"FCV"),{},false});
  }
  if(action->family==WeaponFamily::Mine || action->family==WeaponFamily::Rocket) {
    const std::array<unsigned,3> arcs=action->family==WeaponFamily::Mine
        ?std::array<unsigned,3>{0x22,0x20,0x23}:std::array<unsigned,3>{0x20,0x1d,0x1f};
    for(unsigned k=0;k<arcs.size();++k){carryClips_[k]=unsigned(clips.size());clips.push_back({asset(archive,arcs[k]-4,"FCV"),{},false});}
  }
  if(action->family==WeaponFamily::Bow) {
    carryClips_[0]=unsigned(clips.size());clips.push_back({asset(archive,0x2e - 4,"FCV"),{},false});
  }
  for(const auto &joint:model.mesh.joints)parents_.push_back(joint.parent);
  if(action->family==WeaponFamily::Rocket || action->family==WeaponFamily::Bow) {
    secondary_=std::make_unique<GenericEventActor>("Loaded projectile","PL_ARC70",asset(player,0x70-4,"BIN"),asset(player,0x71-4,"TPL"));
    secondary_->prepareStaticPose();
  }
  staticPose_=clips.empty();
  rig_=staticPose_?std::make_unique<AnimationRig>(model):std::make_unique<AnimationRig>(model,clips);
  if(!staticPose_)rig_->sample(0,0);
  const auto effectArc=source->number==9 || source->number==10?8u:
      source->number==13 || source->number==14?6u:4u;
  effects_=asset(archive,effectArc-4,"EFF");
  std::vector<std::uint8_t> empty(16);empty[0]=64;Smd palettes(empty);
  palettes.palettes.push_back(asset(geometry,source->palette-4,"TPL"));
  const auto &mesh=model.mesh;
  for(const auto &prim:mesh.primitives) {
    if(prim.triangles.empty())continue;
    const auto &header=mesh.materialHeaders.at(prim.displayList);
    SceneDraw draw;draw.first=mesh_.vertices.size()/3;draw.count=prim.triangles.size()*3;
    draw.image=header[12];SetSceneSpecular(draw,header,mesh.sourceFlags);
    draw.repeat=mesh.colored;draw.vertexColors=mesh.colored&&mesh.vertexAmbient;
    draw.alphaTest=(header[11]&4)!=0;draw.alphaImage=header[14];draw.alphaReference=header[20];
    if(id==0x17)draw.materialRGB={0xa0,0xd0,0xe0};
    mesh_.draws.push_back(draw);
    for(const auto &tri:prim.triangles)for(auto corner:tri) {
      const auto &v=prim.vertices.at(corner);const auto &p=mesh.positions.at(v.position);
      const auto &n=mesh.normals.at(v.normal);const auto &uv=mesh.texcoords.at(v.texcoord);
      mesh_.vertices.insert(mesh_.vertices.end(),p.begin(),p.end());
      mesh_.normals.insert(mesh_.normals.end(),n.begin(),n.end());mesh_.uv.insert(mesh_.uv.end(),uv.begin(),uv.end());
      const auto color=draw.vertexColors?mesh.colors.at(v.color):std::array<std::uint8_t,4>{255,255,255,255};
      mesh_.colors.insert(mesh_.colors.end(),color.begin(),color.end());
      skin_.push_back({model.weights.at(model.positionWeights.at(v.position)),model.weights.at(model.normalWeights.at(v.normal))});
    }
    mesh_.triangles+=prim.triangles.size();
  }
  mesh_.included=1;AttachSceneTextures(palettes,mesh_);
  if(mesh_.vertices.empty() || mesh_.fallbackTriangles || mesh_.unresolvedAlphaTriangles)
    throw std::runtime_error("Weapon: Original geometry/texture missing");
}
WeaponPreview::~WeaponPreview()=default;
unsigned WeaponPreview::capacity() const {return OriginalWeaponCapacity(itemId_);}

void WeaponPreview::sampleCarry(PlayerAction action,float frame,bool hasRounds) {
  if(family()==WeaponFamily::Mine) {
    if(action==PlayerAction::Draw)rig_->sample(carryClips_[0],std::min(frame,rig_->maxFrame(carryClips_[0])));
    else if(action==PlayerAction::Aim)rig_->sample(carryClips_[1],std::min(frame,rig_->maxFrame(carryClips_[1])));
    else if(action==PlayerAction::Holster)rig_->sample(carryClips_[2],std::min(frame,rig_->maxFrame(carryClips_[2])));
  } else if(family()==WeaponFamily::Rocket && !infinite()) {
    if(action==PlayerAction::Draw && frame<11)rig_->sample(carryClips_[0],std::min(frame,rig_->maxFrame(carryClips_[0])));
    else if(action==PlayerAction::Footwork || (action==PlayerAction::Holster && frame>=13))
      rig_->sample(carryClips_[2],rig_->maxFrame(carryClips_[2]));
    else rig_->sample(carryClips_[1],rig_->maxFrame(carryClips_[1]));
  } else if(family()==WeaponFamily::Bow) {
    if(action==PlayerAction::Draw)rig_->sample(carryClips_[0],std::min(frame,rig_->maxFrame(carryClips_[0])));
    const bool nocked=hasRounds && (action==PlayerAction::Aim ||
        (action==PlayerAction::Draw && frame>=11) || (action==PlayerAction::Fire && frame>=35));
    rig_->setPartScale(4,nocked?std::array<float,3>{1,1,1}:std::array<float,3>{0,0,0});
  }
}
Matrix34 WeaponPreview::attachment(const std::vector<Matrix34> &parts,PlayerAction action,float frame,bool spare) const {
  unsigned parent=parentPart_;
  SmdPlacement offset;offset.scale={1,1,1};
  if(family()==WeaponFamily::Grenade) {
    parent=spare?17:10;
    offset.position=spare?std::array<float,3>{-145,-75,-120}:std::array<float,3>{-95,-30,5};
    offset.rotation=spare?std::array<float,3>{2.7925267f,1.134464f,2.6179938f}:std::array<float,3>{1.5707964f,0,0};
  } else if(family()==WeaponFamily::Rocket && !infinite() &&
      (action==PlayerAction::Footwork || (action==PlayerAction::Draw && frame<11) ||
       (action==PlayerAction::Holster && frame>=13)))parent=2;
  return WeaponAttachmentMatrix(PlacementMatrix(offset),parts.at(parent));
}
std::vector<Matrix34> WeaponPreview::posedParts(const std::vector<Matrix34> &parts,PlayerAction action,float frame,bool spare) const {
  const auto mount=attachment(parts,action,frame,spare);
  auto posed=rig_->jointMatrices();for(auto &part:posed)part=WeaponAttachmentMatrix(part,mount);
  if(family()==WeaponFamily::Mine && (action==PlayerAction::Aim || action==PlayerAction::Fire || action==PlayerAction::Reload)) {
    // objMine::partsSet reparents part1 and its complete subtree to Leon's
    // right hand, while part0 remains on his left hand. Preserve each
    // descendant's already solved local motion and the dart's scale.
    const auto old=posed.at(1);const auto normals=NormalMatrix(old);Matrix34 inverse{};
    for(unsigned r=0;r<3;++r){for(unsigned c=0;c<3;++c)inverse[r*4+c]=normals[c*3+r];for(unsigned c=0;c<3;++c)inverse[r*4+3]-=inverse[r*4+c]*old[c*4+3];}
    SmdPlacement local;local.scale=rig_->partScale(1);
    const auto right=WeaponAttachmentMatrix(PlacementMatrix(local),parts.at(10));
    for(unsigned i=2;i<posed.size();++i){unsigned p=parents_.at(i);for(unsigned depth=0;p!=255 && p!=1 && depth<parents_.size();++depth)p=parents_.at(p);if(p==1)posed[i]=WeaponAttachmentMatrix(WeaponAttachmentMatrix(posed[i],inverse),right);}
    posed[1]=right;
  }
  return posed;
}

unsigned WeaponPreview::clipIndex(WeaponMotion motion, bool empty) {
  if(motion==WeaponMotion::RifleReloadDown)return 6;
  if(motion==WeaponMotion::RifleBoltCycle)return 7;
  const auto index = static_cast<unsigned>(motion);
  if (index > static_cast<unsigned>(WeaponMotion::Reload))
    throw std::runtime_error("WEP01: Motion index");
  return index + (empty ? 3 : 0);
}
void WeaponPreview::sample(WeaponMotion motion, float frame, bool empty) {
  if (!std::isfinite(frame) || frame < 0)
    throw std::runtime_error("WEP01: invalid motion frame");
  if(staticPose_){motion_=motion;frame_=0;empty_=empty;return;}
  const auto index = clipIndex(motion, empty);
  frame_ = std::min(frame, rig_->maxFrame(index));
  rig_->sample(index, frame_);
  motion_ = motion;
  empty_ = empty;
}
float WeaponPreview::maxFrame(WeaponMotion motion, bool empty) const {
  if(staticPose_)return 0;
  return rig_->maxFrame(clipIndex(motion, empty));
}
std::string WeaponPreview::describe() const {
  std::ostringstream out;
  if(kind_==WeaponKind::OriginalCarry) {
    out << "Original weapon " << itemId_ << ": " << rig_->bones().size() << " local joints · "
        << mesh_.triangles << " triangles · " << mesh_.textures.size() << " textures · Parent part #" << parentPart_;
    return out.str();
  }
  out << (kind_ == WeaponKind::ShotgunWep07 ? "WEP07" : kind_ == WeaponKind::HandgunWep02 ? "WEP02" : "WEP01") << ": " << rig_->bones().size() << " local joints · "
      << mesh_.triangles << " triangles · " << mesh_.textures.size()
      << " textures · Original FCV · Right hand #10";
  return out.str();
}
std::array<float, 3> WeaponPreview::markerOffset() const {
  return actions().marker;
}
} // namespace re4
