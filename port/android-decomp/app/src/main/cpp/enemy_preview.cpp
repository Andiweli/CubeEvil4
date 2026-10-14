#include "enemy_preview.h"
#include "archive.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace re4 {
namespace {
constexpr std::array<EnemyMotionSource, 30> sources{{
    {1, 0, 10, 5},     // init / em10_R1_Wait: ARC5, no SEQ.
    {4, 5, 7, 5},      // em10SetWalkMotion: ARC8 / ARC9, normal no-weapon walk.
    {40, 41, 6, 1},    // em10_R1_Dm_Head: front, normal gun reaction ARC2c/2d.
    {34, 35, 6, 1},    // em10_R1_Dm_Small: front torso variant ARC26/27.
    {99, 100, 15, 1},  // em10_R1_Die: standing death ARC67/68.
    {130, 131, 10, 1}, // em10_R1_Catch: front approach ARC86/87.
    {651, 0, 5, 1},    // em10_R1_NeckHang: ARC28f, no SEQ.
    {652, 653, 5, 1},  // Failed resistance: ARC290/291.
    {654, 655, 5, 1},  // Successful resistance: ARC292/293.
    {105, 106, 15, 1}, // DownWake after ThrowOff: ARC6d/6e.
    {654, 655, 0, 1},  // Logical DownIdle alias; no runtime MotionSet.
    {59, 60, 3, 1},    // Dm_Blow front, face-up: ARC3f/40.
    {63, 64, 3, 1},    // Dm_Blow front, face-down: ARC43/44.
    {71, 72, 3, 1},    // Dm_Blow rear, face-up: ARC4b/4c.
    {73, 74, 3, 1},    // Dm_Blow rear, face-down: ARC4d/4e.
    {55, 56, 3, 1},    // Dm_Blow killing fall, face-up: ARC3b/3c.
    {61, 62, 3, 1},    // Dm_Blow killing fall, face-down: ARC41/42.
    {107, 108, 15, 1}, // DownWake face-down: ARC6f/70.
    {42, 43, 6, 1},   // Dm_Head front/rear alternate: ARC2e/2f.
    {44, 45, 6, 1},   // Dm_Head front third variant: ARC30/31.
    {75, 0, 5, 1},    // Dm_Down face-up: ARC4f, no SEQ, previous mirror.
    {76, 0, 5, 1},    // Dm_Down face-down: ARC50, no SEQ, previous mirror.
    {103, 0, 6, 1},   // Die_Down face-up: ARC6b, no SEQ, fixed flags 1.
    {104, 0, 6, 1},   // Die_Down face-down: ARC6c, no SEQ, fixed flags 1.
    {36, 37, 6, 1},   // Dm_Small front torso alternate: ARC28/29.
    {65, 66, 6, 1},   // Dm_Small rear torso: ARC45/46.
    {124, 125, 10, 1}, // em10_R1_AxeAtk: ARC80/81.
    {345, 346, 10, 1}, // em10_R1_SukiAtk: ARC15d/15e, target -800..-100.
    {347, 348, 10, 1}, // ARC15f/160, target above -100.
    {349, 350, 10, 1}, // ARC161/162, target below -800.
}};
bool isBlow(EnemyMotion motion) {
  return motion >= EnemyMotion::BlowFrontFaceUp &&
         motion <= EnemyMotion::BlowDeathFaceDown;
}
bool isDownReaction(EnemyMotion motion) {
  return motion >= EnemyMotion::DownHitFaceUp &&
         motion <= EnemyMotion::DownDeathFaceDown;
}
unsigned index(EnemyMotion motion) {
  const auto value = static_cast<unsigned>(motion);
  if (value >= sources.size())
    throw std::runtime_error("EM12: Motion index");
  return value;
}
std::vector<std::uint8_t> asset(const RoomArchive &archive, unsigned entry,
                                const std::string &tag) {
  if (entry >= archive.entries().size())
    throw std::runtime_error("EM12: Asset is missing");
  const auto &info = archive.entries()[entry];
  if (!std::equal(tag.begin(), tag.end(), info.tag.begin()))
    throw std::runtime_error("EM12: Asset type mismatch");
  return archive.copy(entry);
}
void appendMesh(const SkinnedModel &model, unsigned entry, StaticScene &out,
                std::vector<SkinCorner> &skin) {
  const auto &mesh = model.mesh;
  for (const auto &primitive : mesh.primitives) {
    if (primitive.triangles.empty())
      continue;
    const auto &header = mesh.materialHeaders.at(primitive.displayList);
    SceneDraw draw;
    draw.work = entry;
    draw.first = out.vertices.size() / 3;
    draw.count = primitive.triangles.size() * 3;
    draw.palette = 0;
    draw.image = header[12];
      SetSceneSpecular(draw,header,mesh.sourceFlags);
    draw.repeat = mesh.colored;
    draw.vertexColors = mesh.colored && mesh.vertexAmbient;
    draw.alphaTest = (header[11] & 4) != 0;
    draw.alphaImage = header[14];
    draw.alphaReference = header[20];
    out.draws.push_back(draw);
    for (const auto &triangle : primitive.triangles)
      for (auto corner : triangle) {
        const auto &reference = primitive.vertices.at(corner);
        const auto &point = mesh.positions.at(reference.position);
        const auto &normal = mesh.normals.at(reference.normal);
        const auto &uv = mesh.texcoords.at(reference.texcoord);
        out.vertices.insert(out.vertices.end(), point.begin(), point.end());
        out.normals.insert(out.normals.end(), normal.begin(), normal.end());
        out.uv.insert(out.uv.end(), uv.begin(), uv.end());
        const auto color =
            draw.vertexColors ? mesh.colors.at(reference.color)
                              : std::array<std::uint8_t, 4>{255, 255, 255, 255};
        out.colors.insert(out.colors.end(), color.begin(), color.end());
        skin.push_back(
            {model.weights.at(model.positionWeights.at(reference.position)),
             model.weights.at(model.normalWeights.at(reference.normal))});
      }
    out.triangles += primitive.triangles.size();
  }
  ++out.included;
}
} // namespace

const char *EnemyMotionLabel(EnemyMotion motion) {
  switch (motion) {
  case EnemyMotion::Idle: return "Wait";
  case EnemyMotion::Walk: return "Walk";
  case EnemyMotion::Dash: return "Dash";
  case EnemyMotion::RoofBack:
  case EnemyMotion::RoofFront: return "Roof fall";
  case EnemyMotion::RoofLand: return "Roof landing";
  case EnemyMotion::BlowAirUp:
  case EnemyMotion::BlowAirDown: return "Ledge fall";
  case EnemyMotion::BlowLandUp:
  case EnemyMotion::BlowLandDown: return "Ledge landing";
  case EnemyMotion::Find: return "Leon spotted";
  case EnemyMotion::Turn180:
  case EnemyMotion::Turn180Alternate: return "Turn around";
  case EnemyMotion::FlashStart: return "Blinded";
  case EnemyMotion::FlashRecover: return "Flash recovery";
  case EnemyMotion::FlashFinish: return "Flash recovery complete";
  case EnemyMotion::HitHead: return "Head hit";
  case EnemyMotion::HeadLeft:
  case EnemyMotion::HeadRight: return "Head hit";
  case EnemyMotion::HitBody: return "Body hit";
  case EnemyMotion::BodyAlt:
  case EnemyMotion::BodyRear: return "Body hit";
  case EnemyMotion::Death: return "Death";
  case EnemyMotion::AxeAttack: return "Axe attack";
  case EnemyMotion::HoeAttack:
  case EnemyMotion::HoeAttackHigh:
  case EnemyMotion::HoeAttackLow: return "Hoe swing";
  case EnemyMotion::RoomEvent: return "Boulder sequence";
  case EnemyMotion::HouseTurn: return "House event: Turn";
  case EnemyMotion::HouseWalk: return "House event: Walk";
  case EnemyMotion::Catch: return "Grab";
  case EnemyMotion::NeckHang: return "Chokehold";
  case EnemyMotion::ReleaseFailed: return "Knockdown";
  case EnemyMotion::ThrowOff: return "Break free";
  case EnemyMotion::Wake: return "Get up";
  case EnemyMotion::DownIdle: return "On the ground";
  case EnemyMotion::BlowFrontFaceUp:
  case EnemyMotion::BlowFrontFaceDown:
  case EnemyMotion::BlowBackFaceUp:
  case EnemyMotion::BlowBackFaceDown: return "Kicked away";
  case EnemyMotion::BlowDeathFaceUp:
  case EnemyMotion::BlowDeathFaceDown: return "Fatal kick";
  case EnemyMotion::WakeFaceDown: return "Get up";
  case EnemyMotion::DownHitFaceUp:
  case EnemyMotion::DownHitFaceDown: return "Ground hit";
  case EnemyMotion::DownDeathFaceUp:
  case EnemyMotion::DownDeathFaceDown: return "Death on ground";
  }
  return "?";
}

GanadoAppearance GanadoAppearanceForSpawn(const EnemySpawn &spawn) {
  if((spawn.id!=0x12 && spawn.id!=0x15 && spawn.id!=0x17) ||
      (spawn.type!=0 && spawn.type!=1 && spawn.type!=3 &&
       !(spawn.id==0x12 && spawn.type==4) && !(spawn.id==0x15 && (spawn.type==4 || spawn.type==11)) &&
       !(spawn.id==0x17 && spawn.type==12)))
    throw std::runtime_error("Ganado: unsupported original EM12 type profile");
  GanadoAppearance result{spawn.type,0,spawn.flags,spawn.id};
  if(spawn.flags&0x80000000u)result.weaponType=1;
  if((spawn.flags&0x20000000u) && !(spawn.flags&0x2000))result.weaponType=2;
  if((spawn.flags&0x08000000u) && !(spawn.flags&0x2000))result.weaponType=3;
  if(spawn.flags&0x10000000u)result.weaponType=4;
  return result;
}
EnemyPreview::EnemyPreview(const std::string &path,unsigned emsetNo,bool melee,bool axe)
    :EnemyPreview(path,emsetNo,melee,GanadoAppearance{0,axe?2u:0u,axe?0x01000000u:0u}) {}
EnemyPreview::EnemyPreview(const std::string &path,const EnemySpawn &spawn,bool melee)
    :EnemyPreview(path,unsigned(spawn.listIndex),melee,GanadoAppearanceForSpawn(spawn)) {}
EnemyPreview::EnemyPreview(const std::string &enemyDrs,unsigned emsetNo,bool melee,
                           GanadoAppearance appearance)
    : emsetNo_(emsetNo), melee_(melee || appearance.weaponType),
      axe_(appearance.weaponType==2),appearance_(appearance) {
  const auto archive = RoomArchive::LoadDrs(enemyDrs);
  if(melee_) {
    ordinaryWalkSources_[0]={asset(archive,10,"FCV"),asset(archive,11+emsetNo_%5,"SEQ")};
    ordinaryWalkSources_[1]={asset(archive,212,"FCV"),asset(archive,213+emsetNo_%5,"SEQ")};
    if(appearance_.weaponType==4)
      ordinaryWalkSources_[2]={asset(archive,235,"FCV"),asset(archive,236,"SEQ")};
    discoverySources_[0]={asset(archive,111,"FCV"),asset(archive,112,"SEQ")};
    discoverySources_[1]={asset(archive,20,"FCV"),asset(archive,21,"SEQ")};
    discoverySources_[2]={asset(archive,18,"FCV"),asset(archive,19,"SEQ")};
    if(appearance_.enemyId==0x15 || appearance_.enemyId==0x17 || appearance_.enemyId==0x12) {
      const auto dash=motionSource(EnemyMotion::Dash);
      dashSource_={asset(archive,dash.motionEntry,"FCV"),asset(archive,dash.sequenceEntry,"SEQ")};
      for(unsigned i=0;i<3;++i) {
        const unsigned motion=i==0?93:i==1?91:95;
        roofSources_[i]={asset(archive,motion,"FCV"),asset(archive,motion+1,"SEQ")};
      }
      roofSources_[3]={asset(archive,0x5b - 4,"FCV"),asset(archive,0x5e - 4,"SEQ")};
      roofSources_[4]={asset(archive,0x53 - 4,"FCV"),{}};
      roofSources_[5]={asset(archive,0x5b - 4,"FCV"),asset(archive,0x5d - 4,"SEQ")};
      roofSources_[6]={asset(archive,0x54 - 4,"FCV"),{}};
    }
  }
  if(emsetNo_==0x25 && appearance_.type==0 && !appearance_.weaponType) {
    // em10_R1_R100TurnWalk ARC18/19 and alternate Walk_type1 ARCe/f..
    // Cache on the loader; starting the event performs no filesystem read.
    houseTurnSource_={asset(archive,20,"FCV"),asset(archive,21,"SEQ")};
    houseAltWalkSource_={asset(archive,10,"FCV"),asset(archive,11+emsetNo_%5,"SEQ")};
  }
  // Em12Set(type 0): ARC1bc body, ARC1be head, ARC1c0 right hand,
  // ARC1c5 left hand; ARC1bd supplies all three textures. ARC includes
  // the four DRS header words, so entry number is ARC minus four.
  const unsigned bodyEntry=appearance_.type==12?457:appearance_.type==11?453:appearance_.type==1?468:appearance_.type==3?472:appearance_.type==4?476:440;
  const unsigned paletteEntry=bodyEntry+1,headEntry=bodyEntry+2;
  SkinnedModel body(asset(archive, bodyEntry, "BIN"));
  if (body.mesh.joints.size() != 34)
    throw std::runtime_error("EM12: Incorrect default skeleton");
  body.validateSkeleton(34);
  std::vector<AnimationBytes> clips;
  for (unsigned i = 0; i < (melee_ ? sources.size() : 10u); ++i) {
    const auto selected = motionSource(static_cast<EnemyMotion>(i));
    clips.push_back({asset(archive, selected.motionEntry, "FCV"),
                     selected.sequenceEntry
                         ? asset(archive, selected.sequenceEntry, "SEQ")
                         : std::vector<std::uint8_t>{}});
  }
  rig_ = std::make_unique<AnimationRig>(body, clips);
  for(unsigned i=0;i<3;++i)flashActions_[i]=rig_->appendActions({
      {asset(archive,0x2ae - 4+i*2,"FCV"),asset(archive,0x2af - 4+i*2,"SEQ")}});
  if(appearance_.weaponType==4) {
    const unsigned entries[]={247,250,249};
    for(unsigned i=0;i<3;++i) {
      auto ids=rig_->appendActions({{asset(archive,entries[i],"FCV"),i==1?asset(archive,251,"SEQ"):std::vector<std::uint8_t>{}}});
      chainsawActions_[i]=ids;
    }
  }
  // Original cEm10::pXFlip, first 34 entries for this 34-part owner.
  rig_->setFlipTable({0, 1, 2, 3, 4, 11, 12, 13, 14, 15, 16, 5, 6, 7, 8, 9,
                     10, 17, 22, 23, 24, 25, 18, 19, 20, 21, 26, 27, 28, 29,
                     30, 31, 32, 33});
  appendMesh(body, bodyEntry, mesh_, skin_);
  auto appendPart = [&](unsigned entry, StaticScene &mesh,
                        std::vector<SkinCorner> &skin) {
    SkinnedModel part(asset(archive, entry, "BIN"));
    part.validateSkeleton(34);
    if (part.mesh.joints.size() != body.mesh.joints.size())
      throw std::runtime_error("EM12: Attachment skeleton is missing");
    // Original addModel shares the body's parts. Different village heads
    // retain unused bind centers from their own authoring skeleton; only
    // joints actually referenced by each attachment's skin must agree.
    const auto ownerBind = RestJointCenters(body.mesh);
    const auto partBind = RestJointCenters(part.mesh);
    for (unsigned joint = 0; joint < ownerBind.size(); ++joint) {
      if (part.mesh.joints[joint].parent != body.mesh.joints[joint].parent)
        throw std::runtime_error("EM12: Attachment hierarchy mismatch");
    }
    for(const auto &weight:part.weights)
      for(unsigned i=0;i<weight.count;++i)
        for(unsigned axis=0;axis<3;++axis)
          if(appearance_.enemyId==0x12 && std::abs(ownerBind[weight.joints[i]][axis]-partBind[weight.joints[i]][axis])>.001f)
            throw std::runtime_error("EM12: attachment bind pose mismatch");
    appendMesh(part, entry, mesh, skin);
  };
  for (unsigned entry : {headEntry, 444u, 449u})
    appendPart(entry, mesh_, skin_);
  std::vector<std::uint8_t> emptyHeader(16);
  emptyHeader[0] = 64;
  Smd palettes(emptyHeader);
  palettes.palettes.push_back(asset(archive, paletteEntry, "TPL"));
  AttachSceneTextures(palettes, mesh_);
  renderMesh_ = mesh_;
  renderSkin_ = skin_;
  // em10HandSet(type 1): original mot[7]/mot[12], sharing the body parts.
  // Original type 0 keeps mot[6]/mot[11], already in the first M19 vertices.
  for (unsigned entry : {445u, 450u})
    appendPart(entry, renderMesh_, renderSkin_);
  if (appearance_.weaponType) {
    // em10HandSet while the mirrored type2 axe is held keeps the relaxed
    // right hand mot6, and selects the left weapon grip mot14 (ARC1c8).
    appendPart(452, renderMesh_, renderSkin_);
    appendPart(447, renderMesh_, renderSkin_);
  }
  AttachSceneTextures(palettes, renderMesh_);
  if (mesh_.vertices.empty() || mesh_.fallbackTriangles ||
      mesh_.unresolvedAlphaTriangles || mesh_.textures.size() != (appearance_.type==12?5u:3u) ||
      renderMesh_.fallbackTriangles || renderMesh_.unresolvedAlphaTriangles ||
      renderMesh_.textures.size() != (appearance_.type==12?5u:3u))
    throw std::runtime_error("Ganado: Geometry/texture profile type="+std::to_string(appearance_.type)+" textures="+std::to_string(mesh_.textures.size())+" fallback="+std::to_string(mesh_.fallbackTriangles)+" alpha="+std::to_string(mesh_.unresolvedAlphaTriangles));
  // Em17Set mot23..25 and em10AccSet: type12's authored hood shares
  // the body parts. Its palette is separate from the body and weapon.
  if(appearance_.enemyId==0x17 && appearance_.type==12 && (appearance_.flags&0x00440000u)) {
    const unsigned palette=unsigned(palettes.palettes.size());
    palettes.palettes.push_back(asset(archive,(appearance_.flags&0x00400000u)?464:465,"TPL"));
    for(auto target:{std::make_pair(&mesh_,&skin_),std::make_pair(&renderMesh_,&renderSkin_)}) {
      const auto first=target.first->draws.size();appendPart(463,*target.first,*target.second);
      for(std::size_t d=first;d<target.first->draws.size();++d)target.first->draws[d].palette=palette;
      AttachSceneTextures(palettes,*target.first);
      if(target.first->fallbackTriangles || target.first->unresolvedAlphaTriangles)
        throw std::runtime_error("EM17: Original hood texture is missing");
    }
  }
  if (appearance_.weaponType) {
    const unsigned entry=weaponBinEntry();
    SkinnedModel axe(asset(archive, entry, "BIN"));
    const auto firstVertex=renderMesh_.vertices.size()/3;
    const auto firstDraw=renderMesh_.draws.size();
    appendMesh(axe,entry,renderMesh_,renderSkin_);
    SmdPlacement local;local.scale={1,1,1};
    local.position=appearance_.weaponType==1?std::array<float,3>{-843.58f,5.81f,866.87f}:
        std::array<float,3>{-313.85f,-21.2f,102.21f};
    if(appearance_.weaponType==4)local.position={-304.24f,-17.86f,-57.43f};
    local.rotation={0,appearance_.weaponType==1?-.7166758f:-1.0402162f,0};
    if(appearance_.weaponType==4)local.rotation={1.4835298f,0,-1.5707964f};
    if(appearance_.flags&0x01000000u) {
      local.position[0]=-local.position[0];
      if(appearance_.weaponType==1)local.rotation[1]=-local.rotation[1];
      else local.rotation[2]=3.1415927f;
    }
    auto transform=PlacementMatrix(local);
    const auto part=weaponParentPart();const auto bind=rig_->bindCenters().at(part);
    for(unsigned r=0;r<3;++r)transform[r*4+3]+=bind[r];
    BoneWeight hand;hand.count=1;hand.joints[0]=part;hand.rates[0]=1;
    for(std::size_t v=firstVertex;v<renderMesh_.vertices.size()/3;++v) {
      std::array<float,3> p,n;
      for(unsigned r=0;r<3;++r) {p[r]=transform[r*4+3];n[r]=0;
        for(unsigned c=0;c<3;++c) {
          p[r]+=transform[r*4+c]*renderMesh_.vertices[v*3+c];
          n[r]+=transform[r*4+c]*renderMesh_.normals[v*3+c];
        }
      }
      for(unsigned r=0;r<3;++r) {renderMesh_.vertices[v*3+r]=p[r];renderMesh_.normals[v*3+r]=n[r];}
      renderSkin_[v]={hand,hand};
    }
    for(std::size_t d=firstDraw;d<renderMesh_.draws.size();++d)renderMesh_.draws[d].palette=unsigned(palettes.palettes.size());
    palettes.palettes.push_back(asset(archive,entry+1,"TPL"));
    AttachSceneTextures(palettes,renderMesh_);
    if(renderMesh_.fallbackTriangles || renderMesh_.unresolvedAlphaTriangles)
      throw std::runtime_error("EM12: original axe texture is missing");
  }
  rig_->sample(0, 0);
  mirrored_=(appearance_.flags&0x01000000u)!=0;
  rig_->startActionFlags(0,0,0,motionSource(EnemyMotion::Idle).flags|(mirrored_?0x40:0));
}
EnemyPreview::~EnemyPreview() = default;
EnemyMotionSource EnemyPreview::source(EnemyMotion motion, unsigned emsetNo) {
  if(motion==EnemyMotion::Dash)return {0xae - 4,0xaf - 4+emsetNo%5,5,5};
  if(motion>=EnemyMotion::FlashStart && motion<=EnemyMotion::FlashFinish)
    return {0x2ae - 4+(unsigned(motion)-unsigned(EnemyMotion::FlashStart))*2,
        0x2af - 4+(unsigned(motion)-unsigned(EnemyMotion::FlashStart))*2,6,
        std::uint16_t(motion==EnemyMotion::FlashRecover?5:1)};
  if(motion==EnemyMotion::Find)return {111,112,10,1};
  if(motion==EnemyMotion::Turn180)return {20,21,10,1};
  if(motion==EnemyMotion::Turn180Alternate)return {18,19,10,1};
  // This metadata names the held source, not a new authored down animation.
  if (motion == EnemyMotion::DownIdle)
    return {654, 655, 0, 1};
  auto selected = sources[index(motion)];
  if (motion == EnemyMotion::Walk)
    selected.sequenceEntry += emsetNo % 5;
  return selected;
}
EnemyMotionSource EnemyPreview::motionSource(EnemyMotion motion) const {
  if(motion>=EnemyMotion::RoofBack && motion<=EnemyMotion::BlowLandDown) {
    constexpr std::array<unsigned,7> entry{93,91,95,87,79,87,80},seq{94,92,96,90,0,89,0};
    const unsigned i=unsigned(motion)-unsigned(EnemyMotion::RoofBack);return {entry[i],seq[i],3,1};
  }
  if(motion==EnemyMotion::Dash) {
    const unsigned base=appearance_.weaponType==4?0xf1:appearance_.weaponType==1?0x151:
        appearance_.weaponType==6?0x140:appearance_.weaponType==7 || appearance_.weaponType==9?0xde:0xae;
    return {base-4,base-3+(appearance_.weaponType==6?0:emsetNo_%5),5,5};
  }
  if(motion==EnemyMotion::HouseTurn)return {20,21,10,1};
  if(motion==EnemyMotion::HouseWalk)return houseWalkType_?EnemyMotionSource{10,11+emsetNo_%5,7,5}:source(EnemyMotion::Walk,emsetNo_);
  auto result=source(motion,emsetNo_);
  if(appearance_.weaponType==4) {
    if(motion==EnemyMotion::Idle)return {226,0,10,5};
    if(motion==EnemyMotion::Walk)return originalFound_?EnemyMotionSource{235,236,7,5}:EnemyMotionSource{233,234,7,5};
  }
  if(motion==EnemyMotion::Walk && originalWalking_) {
    if(appearance_.weaponType && originalFound_)result={212,213+emsetNo_%5,7,5};
    else if(ordinaryWalkType_)result={10,11+emsetNo_%5,7,5};
  }
  if(appearance_.weaponType==1) {
    if(motion==EnemyMotion::Idle)result.motionEntry=326;
    if(motion==EnemyMotion::Walk) {result.motionEntry=327;result.sequenceEntry=328+emsetNo_%5;}
    if(motion==EnemyMotion::HitBody || motion==EnemyMotion::BodyAlt) {
      result.motionEntry=341;result.sequenceEntry=342;
    }
    if(motion==EnemyMotion::BodyRear) {result.motionEntry=343;result.sequenceEntry=344;}
  }
  if(appearance_.weaponType==4 && motion==EnemyMotion::AxeAttack)result={229,230,10,1};
  return result;
}
unsigned EnemyPreview::weaponBinEntry() const {
  return appearance_.weaponType==4?256:appearance_.weaponType==1?592:appearance_.weaponType==2?616:appearance_.weaponType==3?614:0;
}
void EnemyPreview::start(EnemyMotion motion, bool mirrored, double rootScale,
                          std::optional<unsigned> blendFrames,unsigned startFrame) {
  if (!std::isfinite(rootScale) || rootScale < 0 || rootScale > 2)
    throw std::runtime_error("EM12: invalid blow root factor");
  if(motion>=EnemyMotion::RoofBack && motion<=EnemyMotion::BlowLandDown) {
    const unsigned i=unsigned(motion)-unsigned(EnemyMotion::RoofBack);
    if(roofSources_[i].motion.empty() || rootScale)throw std::runtime_error("Ganado: roof assets missing");
    if(!roofActions_[i])roofActions_[i]=rig_->appendActions({roofSources_[i]});
    rig_->startActionFlags(*roofActions_[i],blendFrames.value_or(3),startFrame,1|(mirrored?0x40:0));
    motion_=motion;mirrored_=mirrored;blowDownPose_=false;blowRootScale_=1;return;
  }
  if(motion==EnemyMotion::Dash) {
    if(!melee_ || rootScale || dashSource_.motion.empty())throw std::runtime_error("Ganado: dash assets missing");
    if(!dashAction_)dashAction_=rig_->appendActions({dashSource_});
    mirrored_=mirrored || (appearance_.flags&0x01000000u);
    rig_->startActionFlags(*dashAction_,blendFrames.value_or(5),startFrame,5|(mirrored_?0x40:0));
    motion_=motion;blowDownPose_=false;blowRootScale_=1;return;
  }
  if(motion>=EnemyMotion::FlashStart && motion<=EnemyMotion::FlashFinish) {
    const auto selected=source(motion,emsetNo_);
    rig_->startActionFlags(flashActions_[unsigned(motion)-unsigned(EnemyMotion::FlashStart)],
        blendFrames.value_or(selected.blendFrames),startFrame,std::uint16_t(selected.flags|(mirrored?0x40:0)));
    motion_=motion;mirrored_=mirrored;blowDownPose_=false;blowRootScale_=1;return;
  }
  if(motion>=EnemyMotion::Find && motion<=EnemyMotion::Turn180Alternate) {
    const unsigned action=unsigned(motion)-unsigned(EnemyMotion::Find);
    if(!discoveryActions_[action] || rootScale || startFrame)throw std::runtime_error("EM12: original find/turn motion is missing");
    const auto selected=motionSource(motion);
    mirrored_=mirrored || (motion==EnemyMotion::Find && (appearance_.flags&0x01000000u));
    rig_->startActionFlags(*discoveryActions_[action],blendFrames.value_or(selected.blendFrames),0,
        std::uint16_t(selected.flags|(mirrored_?0x40:0)));
    motion_=motion;blowDownPose_=false;blowRootScale_=1;return;
  }
  if(motion==EnemyMotion::RoomEvent) {
    if(!roomEventAction_ || mirrored || rootScale || blendFrames.value_or(0) || startFrame)
      throw std::runtime_error("EM12: invalid original room motion");
    rig_->startActionFlags(*roomEventAction_,0,0,5);
    motion_=motion;mirrored_=false;blowDownPose_=false;blowRootScale_=1;
    return;
  }
  if(motion==EnemyMotion::HouseTurn || motion==EnemyMotion::HouseWalk) {
    const auto action=motion==EnemyMotion::HouseTurn?houseTurnAction_:houseWalkAction_;
    if(!action || mirrored || rootScale || startFrame)throw std::runtime_error("EM12: original house motion is missing");
    const auto selected=motionSource(motion);
    rig_->startActionFlags(*action,blendFrames.value_or(selected.blendFrames),0,selected.flags);
    motion_=motion;mirrored_=false;blowDownPose_=false;blowRootScale_=1;return;
  }
  if (motion == EnemyMotion::DownIdle) {
    if ((motion_ != EnemyMotion::ThrowOff && motion_ != EnemyMotion::DownIdle &&
         !isBlow(motion_) && !isDownReaction(motion_) && motion_!=EnemyMotion::RoofLand &&
         motion_!=EnemyMotion::BlowLandUp && motion_!=EnemyMotion::BlowLandDown) ||
        !rig_->status().ended)
      throw std::runtime_error("EM12: Ground pose without a completed fall");
    blowDownPose_ = blowDownPose_ || isBlow(motion_) || isDownReaction(motion_);
    motion_ = motion;
    return;
  }
  const auto clip=motion==EnemyMotion::Walk?walkingClip():index(motion);
  if (clip >= 11 && !melee_)
    throw std::runtime_error("EM12: Melee motions not loaded");
  const auto source = motionSource(motion);
  // The selected front catch and paired NeckHang motions are fixed flags 1.
  // Wake preserves the previous 0x40 mapping, as the source DownWake does.
  const bool sourceMirror = (mirrored || ((appearance_.flags&0x01000000u) &&
      (motion==EnemyMotion::Idle || motion==EnemyMotion::Walk ||
       motion==EnemyMotion::AxeAttack || motion==EnemyMotion::HoeAttack ||
       motion==EnemyMotion::HoeAttackHigh || motion==EnemyMotion::HoeAttackLow))) &&
      (clip < 4 || motion==EnemyMotion::Idle || motion==EnemyMotion::Walk || motion == EnemyMotion::Wake ||
       motion == EnemyMotion::WakeFaceDown || isBlow(motion) ||
       motion == EnemyMotion::HeadLeft || motion == EnemyMotion::HeadRight ||
       motion == EnemyMotion::DownHitFaceUp || motion == EnemyMotion::DownHitFaceDown ||
       motion == EnemyMotion::BodyAlt || motion == EnemyMotion::BodyRear ||
       motion == EnemyMotion::AxeAttack || motion==EnemyMotion::HoeAttack ||
       motion==EnemyMotion::HoeAttackHigh || motion==EnemyMotion::HoeAttackLow);
  const auto flags = static_cast<std::uint16_t>(
      source.flags | (sourceMirror ? 0x40 : 0));
  if(appearance_.weaponType==4 && motion==EnemyMotion::AxeAttack)startFrame=30;
  rig_->startActionFlags(clip, blendFrames.value_or(source.blendFrames), startFrame, flags);
  motion_ = motion;
  blowDownPose_ = false;
  mirrored_ = sourceMirror;
  blowRootScale_ = rootScale ? rootScale :
      (motion == EnemyMotion::BlowDeathFaceUp && !sourceMirror ? 2 : 1);
}
RootMotionStep EnemyPreview::advance(double playbackRate) {
  if (isBlow(motion_))
    return rig_->advanceBlowAction(playbackRate, blowRootScale_);
  return rig_->advanceRootAction(playbackRate, blowDownPose_ ||
                                 motion_ == EnemyMotion::WakeFaceDown ||
                                 isDownReaction(motion_) ||
                                 motion_ == EnemyMotion::BodyAlt ||
                                 motion_ == EnemyMotion::BodyRear);
}
void EnemyPreview::sample(EnemyMotion motion, float frame) {
  if(motion>=EnemyMotion::RoofBack && motion<=EnemyMotion::BlowLandDown) {
    const unsigned i=unsigned(motion)-unsigned(EnemyMotion::RoofBack);
    if(roofSources_[i].motion.empty())throw std::runtime_error("Ganado: roof assets missing");
    if(!roofActions_[i])roofActions_[i]=rig_->appendActions({roofSources_[i]});
    rig_->sample(*roofActions_[i],frame);motion_=motion;blowDownPose_=false;mirrored_=false;return;
  }
  if(motion==EnemyMotion::Dash) {
    if(dashSource_.motion.empty())throw std::runtime_error("Ganado: dash assets missing");
    if(!dashAction_)dashAction_=rig_->appendActions({dashSource_});
    rig_->sample(*dashAction_,frame);motion_=motion;blowDownPose_=false;mirrored_=false;return;
  }
  if(motion>=EnemyMotion::FlashStart && motion<=EnemyMotion::FlashFinish) {
    rig_->sample(flashActions_[unsigned(motion)-unsigned(EnemyMotion::FlashStart)],frame);
    motion_=motion;blowDownPose_=false;mirrored_=false;return;
  }
  if (motion == EnemyMotion::DownIdle)
    throw std::runtime_error("EM12: Floor pose has no FCV of its own");
  const auto special=motion>=EnemyMotion::Find && motion<=EnemyMotion::Turn180Alternate?
      discoveryActions_[unsigned(motion)-unsigned(EnemyMotion::Find)]:motion==EnemyMotion::RoomEvent?roomEventAction_:motion==EnemyMotion::HouseTurn?houseTurnAction_:
      motion==EnemyMotion::HouseWalk?houseWalkAction_:std::nullopt;
  const auto clip=special?*special:motion==EnemyMotion::Walk?walkingClip():index(motion);
  if (clip >= 11 && !melee_)
    throw std::runtime_error("EM12: Melee motions not loaded");
  rig_->sample(clip, frame);
  motion_ = motion;
  blowDownPose_ = false;
  mirrored_ = false;
}
void EnemyPreview::installRoomEventMotion(const std::vector<std::uint8_t> &motion) {
  if(roomEventAction_)throw std::runtime_error("EM12: Room motion already installed");
  const auto action=rig_->clipCount();
  rig_->appendActions({AnimationBytes{motion,{}}});
  roomEventAction_=action;
}
void EnemyPreview::setOpacity(float opacity) {
  if(!std::isfinite(opacity) || opacity<0 || opacity>1)
    throw std::runtime_error("EM12: invalid visibility factor");
  opacity_=opacity;
}
void EnemyPreview::prepareHouseScareMotions(unsigned walkType) {
  if(houseTurnAction_ || walkType>1 || houseTurnSource_.motion.empty())
    throw std::runtime_error("EM12: invalid original house event");
  houseTurnAction_=rig_->appendActions({houseTurnSource_});houseWalkType_=walkType;
  houseWalkAction_=walkType?rig_->appendActions({houseAltWalkSource_}):1;
}
unsigned EnemyPreview::walkingClip() const {
  if(!originalWalking_ || appearance_.weaponType==1)return index(EnemyMotion::Walk);
  if(appearance_.weaponType==4)return originalFound_?*ordinaryWalkActions_[2]:index(EnemyMotion::Walk);
  if(appearance_.weaponType && originalFound_)return *ordinaryWalkActions_[1];
  return ordinaryWalkType_?*ordinaryWalkActions_[0]:index(EnemyMotion::Walk);
}
void EnemyPreview::setOriginalWalking(unsigned walkType,bool found) {
  if(walkType>1 || ordinaryWalkSources_[0].motion.empty())
    throw std::runtime_error("EM12: Original-Walk_type/Assets");
  if(!ordinaryWalkActions_[0])for(unsigned i=0;i<ordinaryWalkActions_.size();++i)
    if(!ordinaryWalkSources_[i].motion.empty())ordinaryWalkActions_[i]=rig_->appendActions({ordinaryWalkSources_[i]});
  if(!discoveryActions_[0])for(unsigned i=0;i<3;++i)
    discoveryActions_[i]=rig_->appendActions({discoverySources_[i]});
  ordinaryWalkType_=walkType;originalWalking_=true;originalFound_=found;
}
bool EnemyPreview::openHands() const {
  return motion_ == EnemyMotion::NeckHang ||
         motion_ == EnemyMotion::ReleaseFailed ||
         motion_ == EnemyMotion::ThrowOff;
}
void EnemyPreview::startChainsawCatch(unsigned phase) {
  if(appearance_.weaponType!=4 || phase>2)throw std::runtime_error("Chainsaw: Invalid paired motion");
  motion_=phase==0?EnemyMotion::NeckHang:phase==1?EnemyMotion::ThrowOff:EnemyMotion::ReleaseFailed;
  mirrored_=phase!=0 && (appearance_.flags&0x01000000u);
  rig_->startActionFlags(chainsawActions_[phase],5,0,1|(mirrored_?0x40:0));
}
bool EnemyPreview::visiblePart(unsigned entry) const {
  if (appearance_.weaponType && entry==weaponBinEntry())return holdsWeapon();
  const bool both=appearance_.weaponType==1 || appearance_.weaponType==4,left=(appearance_.flags&0x01000000u)!=0;
  if(entry==447)return holdsWeapon()&&(both||!left);
  if(entry==452)return holdsWeapon()&&(both||left);
  if(holdsWeapon() && (entry==445 || entry==450 ||
      ((both||left)&&entry==449) || ((both||!left)&&entry==444)))return false;
  if (entry == 444 || entry == 449)
    return !openHands();
  if (entry == 445 || entry == 450)
    return openHands();
  return true;
}
Matrix34 EnemyPreview::weaponMatrix() const {
  if(!appearance_.weaponType)throw std::runtime_error("Ganado: no tool attachment");
  SmdPlacement local;local.scale={1,1,1};
  local.position=appearance_.weaponType==1?std::array<float,3>{-843.58f,5.81f,866.87f}:
      std::array<float,3>{-313.85f,-21.2f,102.21f};
  local.rotation={0,appearance_.weaponType==1?-.7166758f:-1.0402162f,0};
  if(appearance_.flags&0x01000000u) {
    local.position[0]=-local.position[0];
    if(appearance_.weaponType==1)local.rotation[1]=-local.rotation[1];
    else local.rotation[2]=3.1415927f;
  }
  const auto offset=PlacementMatrix(local),hand=rig_->jointMatrices().at(weaponParentPart());
  Matrix34 result{};
  for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c) {
    result[r*4+c]=c==3?hand[r*4+3]:0;
    for(unsigned k=0;k<3;++k)result[r*4+c]+=hand[r*4+k]*offset[k*4+c];
  }
  return result;
}
std::string EnemyPreview::describe() const {
  std::ostringstream out;
  out << "Ganado EM12 / Type "<<appearance_.type<<": " << rig_->jointMatrices().size()
      << " joints · " << mesh_.triangles << " triangles · "
      << mesh_.textures.size() << " textures · Original FCV/SEQ";
  return out.str();
}
} // namespace re4
