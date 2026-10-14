#include "leon_preview.h"
#include "room_interactions.h"
#include "archive.h"
#include "grab_rules.h"
#include "object_actions.h"
#include "ports/rnd.h"
#include "turn_rules.h"
#include "walk_probe.h"
#include "weapon_preview.h"
#include "weapon_carry_source.h"
#include "inventory_case.h"
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <stdexcept>
namespace re4 {
const char *PlayerActionLabel(PlayerAction action) {
  switch (action) {
  case PlayerAction::Footwork:
    return "Motion";
  case PlayerAction::Draw:
    return "Raise weapon";
  case PlayerAction::Aim:
    return "Aim";
  case PlayerAction::Fire:
    return "Shooting animation";
  case PlayerAction::Reload:
    return "Reload";
  case PlayerAction::Holster:
    return "Lower weapon";
  case PlayerAction::Grabbed:
    return "Grabbed · Move LS or press A/Cross, B/Circle";
  case PlayerAction::GrabRelease:
    return "Grab not repelled";
  case PlayerAction::GrabEscape:
    return "Grab repelled";
  case PlayerAction::Recover:
    return "Leon gets up";
  case PlayerAction::Dead:
    return "Leon dead · Continue from checkpoint";
  case PlayerAction::KnifeDraw:
    return "Draw knife";
  case PlayerAction::KnifeReady:
    return "Knife ready · RT/R2 to attack";
  case PlayerAction::KnifeSlash:
    return "Knife attack";
  case PlayerAction::KnifeHolster:
    return "Sheathe knife";
  case PlayerAction::Kick:
    return "Kick";
  case PlayerAction::BlastBlow:
    return "Explosion · Leon falls";
  case PlayerAction::BlastStagger:
    return "Explosion · Leon staggers";
  case PlayerAction::BlastRecover:
    return "Explosion · Leon gets up";
  case PlayerAction::BlastDead:
    return "Explosion · Leon dies";
  case PlayerAction::DoorOpen:
    return "Open door · Press A/Cross again: kick";
  case PlayerAction::DoorKick:
    return "Kick door";
  case PlayerAction::DoorBlockedKick:
    return "Door locked · Hit the lock";
  case PlayerAction::LadderUp: return "Climb ladder";
  case PlayerAction::LadderDown: return "Climb down ladder";
  case PlayerAction::WindowVault: return "Jump through window";
  case PlayerAction::ItemExam: return "Examine item";
  case PlayerAction::LadderTip: return "Topple ladder";
  case PlayerAction::LadderReset: return "Raise ladder";
  case PlayerAction::FenceVault: return "Jump over obstacle";
  case PlayerAction::JumpFall: return "Jump over gap";
  case PlayerAction::JumpFallLanding: return "Landing";
  case PlayerAction::Hurt: return "Hit reaction";
  case PlayerAction::HurtDead: return "Fatal hit";
  case PlayerAction::DogRescue: return "Free dog";
  case PlayerAction::BearTrapBite: return "Release bear trap";
  case PlayerAction::RoomScript: return "Cutscene";
  case PlayerAction::JumpDown: return "Jump down";
  case PlayerAction::JumpLanding: return "Landing";
  case PlayerAction::PushTakeHold: return "Grab furniture · Hold LS forward";
  case PlayerAction::Push: return "Pushing furniture · Release LS: release";
  case PlayerAction::PushStopped: return "Furniture blocked · Release LS";
  case PlayerAction::PushRelease: return "Release furniture";
  }
  return "Player action";
}
namespace {
constexpr unsigned drawClip = 7, holsterClip = 8, reloadClip = 15;
struct WeaponRandomOwner {
  std::uint16_t &owner;std::uint16_t previous;
  explicit WeaponRandomOwner(std::uint16_t &state):owner(state),previous(RndState()) {RndInit(owner);}
  ~WeaponRandomOwner() {owner=RndState();RndInit(previous);}
};
constexpr unsigned holdClip = 16, failedClip = 17, escapeClip = 18,
                   recoverClip = 19;
constexpr unsigned knifeDrawClip = 20, knifeHolsterClip = 27;
constexpr unsigned knifeToGunClip = 28, kickClip = 29;
constexpr std::array<unsigned, 3> knifeReadyClips{21, 22, 23};
constexpr std::array<unsigned, 3> knifeSlashClips{24, 25, 26};
constexpr std::array<unsigned, 3> aimClips{9, 10, 11}, fireClips{12, 13, 14};
std::vector<std::uint8_t> asset(const RoomArchive &arc, unsigned index,
                                const std::string &tag) {
  if (index >= arc.entries().size())
    throw std::runtime_error("Leon: Asset is missing");
  auto &entry = arc.entries()[index];
  if (!std::equal(tag.begin(), tag.end(), entry.tag.begin()))
    throw std::runtime_error("Leon: Asset type mismatch at ARC " + std::to_string(index+4) + ": expected " + tag + ", found " + std::string(entry.tag.begin(),entry.tag.begin()+3));
  return arc.copy(index);
}
} // namespace
LeonPreview::LeonPreview(const std::string &playerDrs,
                         const std::string &weaponDrs,
                         const std::string &enemyDrs, bool enableMelee) {
  playerDrs_=playerDrs;
  weaponRandom_=RndState();
  if (enableMelee && enemyDrs.empty())
    throw std::runtime_error("Leon: Knife/kick require the EM12 archive");
  auto player = RoomArchive::LoadDrs(playerDrs);
  auto weapon = RoomArchive::LoadDrs(weaponDrs);
  SkinnedModel body(asset(player, 0, "BIN"));
  if (body.mesh.parts != 119)
    throw std::runtime_error("Leon: Incorrect skeleton");
  std::vector<AnimationBytes> clips(16);
  // cObjFn57::setMotion and cPlayer::{idle,walk,run}. PL/WEP_ARC
  // pointers include four header words: subtract 4 for archive entry indices.
  for (unsigned i = 0; i < 7; ++i) {
    unsigned index[7]{7, 13, 17, 15, 11, 9, 19};
    clips[i] = {asset(weapon, index[i], "FCV"),
                asset(weapon, index[i] + 1, "SEQ")};
  }
  clips[drawClip] = {asset(weapon, 30, "FCV"), asset(weapon, 31, "SEQ")};
  clips[holsterClip] = {asset(weapon, 32, "FCV"), asset(weapon, 33, "SEQ")};
  for (unsigned i = 0; i < 3; ++i) {
    clips[aimClips[i]] = {asset(weapon, 34 + i, "FCV"), {}};
    clips[fireClips[i]] = {asset(weapon, 37 + i, "FCV"), {}};
  }
  clips[reloadClip] = {asset(weapon, 41, "FCV"), {}};
  if (!enemyDrs.empty()) {
    auto enemy = RoomArchive::LoadDrs(enemyDrs);
    // plem10_NeckHang uses the catcher's subArc, not the PL00 archive.
    clips.push_back({asset(enemy, 659, "FCV"), {}}); // ARC297, hold.
    clips.push_back({asset(enemy, 660, "FCV"), {}}); // ARC298, failed resist.
    clips.push_back(
        {asset(enemy, 661, "FCV"), {}}); // ARC299, successful resist.
    clips.push_back({asset(player, 75, "FCV"), asset(player, 76, "SEQ")});
    hasGrabMotions_ = true;
    if (enableMelee) {
      // game/pl_knife: PL_ARC has four header words. These are the original
      // draw, pitch idle, pitch slash and sheath curves, in cMot3 m0/m1/m2
      // order.
      clips.push_back({asset(player, 31, "FCV"), asset(player, 32, "SEQ")});
      for (unsigned entry : {125u, 127u, 129u, 126u, 128u, 130u, 131u})
        clips.push_back({asset(player, entry, "FCV"), {}});
      clips.push_back({asset(weapon, 45, "FCV"), {}});
      clips.push_back({asset(enemy, 665, "FCV"), {}});
      hasMeleeMotions_ = true;
    }
  }
  // pl_dmg damageBlow / damageBlast. Append after optional melee clips so
  // all existing reference clip indices remain stable. PL_ARC subtracts four
  // header words: living0x51+0x67, dead0x4e+0x66, getup0x4f+0x50, outer0x52.
  const std::array<std::array<unsigned, 2>, 4> blastAssets{
      std::array<unsigned, 2>{77, 99}, {74, 98}, {75, 76}, {78, 0}};
  for (unsigned i = 0; i < blastAssets.size(); ++i) {
    blastClips_[i] = static_cast<unsigned>(clips.size());
    const auto &indices = blastAssets[i];
    clips.push_back({asset(player, indices[0], "FCV"),
                     indices[1] ? asset(player, indices[1], "SEQ")
                                : std::vector<std::uint8_t>{}});
  }
  // PL_ARC 0x1e/0x1c/0x1d, four header words before the offset directory.
  // Append so all pre-existing movement/combat clip numbers stay stable.
  for (unsigned i = 0; i < doorClips_.size(); ++i) {
    const unsigned entries[]{26, 24, 25};
    doorClips_[i] = static_cast<unsigned>(clips.size());
    clips.push_back({asset(player, entries[i], "FCV"), {}});
  }
  std::vector<AnimationBytes> ladderAssets;
  for (unsigned i = 0; i < ladderClips_.size(); ++i) {
    ladderClips_[i] = static_cast<unsigned>(clips.size());
    AnimationBytes motion{asset(player, 35 + i, "FCV"), {}};
    clips.push_back(motion);
    ladderAssets.push_back(std::move(motion));
  }
  // An independent temporary player-local replay determines the actual root
  // exit, including both down turns and its immediate exit tick. No guessed
  // flat floor, fixed-speed elevator or campaign flag is introduced.
  AnimationRig predictor(body, ladderAssets);
  constexpr std::array<unsigned, 6> predictorClips{0, 1, 2, 3, 4, 5};
  for (unsigned path = 0; path < 2; ++path) {
    predictor.reset();
    auto playback = StartLadderMotion(predictor, predictorClips, path ? -9 : 9);
    double heading = 0;
    std::vector<LadderSound> ignored;
    while (!playback.complete) {
      const auto root = AdvanceLadderMotion(predictor, predictorClips, playback, ignored);
      ignored.clear();
      auto &travel = ladderTravel_[path];
      const double c = std::cos(heading), s = std::sin(heading);
      travel[0] += c * root.translation[0] + s * root.translation[2];
      travel[1] += root.translation[1];
      travel[2] += -s * root.translation[0] + c * root.translation[2];
      heading += root.yaw;
    }
  }
  std::vector<AnimationBytes> jumpAssets;
  for(unsigned i=0;i<2;++i) {
    jumpClips_[i]=static_cast<unsigned>(clips.size());
    AnimationBytes motion{asset(player,41+i*2,"FCV"),asset(player,42+i*2,"SEQ")};
    clips.push_back(motion); jumpAssets.push_back(std::move(motion));
  }
  AnimationRig jumpPredictor(body,jumpAssets);
  jumpPredictor.startActionFlags(0,3,0,5);
  for(unsigned i=0;i<120;++i) {
    RootMotionStep root;
    if(i<10) { jumpPredictor.advanceAction(); root=jumpPredictor.rootMotionSpeed(); }
    else root=jumpPredictor.advanceRootAction();
    jumpPath_.push_back({root,jumpPredictor.sequenceFrame(),jumpPredictor.jointMatrices().at(4)[7]});
  }
  jumpPredictor.startActionFlags(1,0,0,5);
  for(unsigned i=0;i<128;++i) {
    const auto root=jumpPredictor.advanceRootAction();
    for(unsigned k=0;k<3;++k) jumpLandingTravel_[k]+=root.translation[k];
    if(root.ended || (jumpPredictor.status().state&3)) break;
    if(i==127) throw std::runtime_error("Leon: Landing motion without an end");
  }
  // pl_R1_ObjPush PL_ARC 0x20/21/31/22; header directory subtracts four.
  const unsigned pushEntries[]{28, 29, 45, 30};
  for (unsigned i = 0; i < pushClips_.size(); ++i) {
    pushClips_[i] = static_cast<unsigned>(clips.size());
    clips.push_back({asset(player, pushEntries[i], "FCV"), {}});
  }
  // pl_R1_Fance wide PL_ARC53/54 and narrow55/56. Predict with the exact
  // MotBase first-ten-tick extraction order; only a SAT-proved exit is offered.
  std::vector<AnimationBytes> fenceAssets;
  for (unsigned i = 0; i < fenceClips_.size(); ++i) {
    fenceClips_[i] = static_cast<unsigned>(clips.size());
    AnimationBytes motion{asset(player, 79 + i * 2, "FCV"),
                          asset(player, 80 + i * 2, "SEQ")};
    clips.push_back(motion); fenceAssets.push_back(std::move(motion));
  }
  AnimationRig fencePredictor(body, fenceAssets);
  for (unsigned clip = 0; clip < fenceAssets.size(); ++clip) {
    fencePredictor.reset(); fencePredictor.startActionFlags(clip, 3, 0, 5);
    for (unsigned tick = 0; tick < 128; ++tick) {
      const auto priorFrame = fencePredictor.sequenceFrame();
      RootMotionStep root;
      if (tick < 10) {
        fencePredictor.advanceAction(); root = fencePredictor.rootMotionSpeed();
      } else root = fencePredictor.advanceRootAction();
      fencePaths_[clip].push_back({root, priorFrame});
      if (root.ended || (fencePredictor.status().state & 3)) break;
      if (tick == 127) throw std::runtime_error("Leon: Obstacle motion without an end");
    }
  }
  std::vector<AnimationBytes> jumpFallAssets;
  for(unsigned i=0;i<2;++i) {
    jumpFallClips_[i]=static_cast<unsigned>(clips.size());
    AnimationBytes motion{asset(player,90+i,"FCV"),asset(player,92+i,"SEQ")};
    clips.push_back(motion); jumpFallAssets.push_back(std::move(motion));
  }
  AnimationRig jumpFallPredictor(body,jumpFallAssets);
  jumpFallPredictor.startActionFlags(0,3,0,0x201);
  for(unsigned i=0;i<128;++i) {
    const auto frame=jumpFallPredictor.sequenceFrame();
    const auto pelvis=jumpFallPredictor.jointMatrices().at(4)[7];
    const auto root=jumpFallPredictor.advanceRootAction();
    jumpFallPath_.push_back({root,frame,pelvis});
  }
  jumpFallPredictor.startActionFlags(1,3,0,0x201);
  for(unsigned i=0;i<128;++i) {
    const auto root=jumpFallPredictor.advanceRootAction();
    for(unsigned k=0;k<3;++k) jumpFallLandingTravel_[k]+=root.translation[k];
    if(jumpFallPredictor.sequenceFrame()>=20) break;
    if(i==127) throw std::runtime_error("Leon: Long landing motion without an end");
  }
  // damageNormal front/back, mid/low front/back and fatal. The source dead
  // manager uses the same final PL4c/4d pair for a fatal middle/low axe hit.
  const unsigned hurtEntries[][2]{{68,0},{69,0},{70,96},{71,97},{72,73}};
  for(unsigned i=0;i<hurtClips_.size();++i) {
    hurtClips_[i]=static_cast<unsigned>(clips.size());
    clips.push_back({asset(player,hurtEntries[i][0],"FCV"),
        hurtEntries[i][1]?asset(player,hurtEntries[i][1],"SEQ"):std::vector<std::uint8_t>{}});
  }
  if(!enemyDrs.empty()) {
    auto enemy=RoomArchive::LoadDrs(enemyDrs.substr(0,enemyDrs.find_last_of('/'))+"/em15.drs");
    for(unsigned i=0;i<2;++i) {chainsawClips_[i]=unsigned(clips.size());clips.push_back({asset(enemy,253+i,"FCV"),{}});}
  }
  // cPlLeon::setMotion, PL_ARC32..3f; cPlayer::motionSet/dmMotCk.
  // Injured movement uses its own authored root speed and footstep SEQ.
  // The damaged table uses the same source key convention: screen-left
  // selects the 0x69/right-table motion, screen-right the 0x67/left-table one.
  const unsigned injuredEntries[]{46,48,52,50,58,56,54};
  for(unsigned i=0;i<injuredClips_.size();++i) {
    injuredClips_[i]=unsigned(clips.size());
    clips.push_back({asset(player,injuredEntries[i],"FCV"),asset(player,injuredEntries[i]+1,"SEQ")});
  }
  rig_ = std::make_unique<AnimationRig>(body, clips);
  // Exact cPlayer::pl00_mirror[80]. Remaining attachment-only parts have no
  // source mirror mapping; 0xffff explicitly leaves those parts unchanged.
  constexpr std::array<std::uint16_t, 80> mirror{
      0,1,2,3,4,11,12,13,14,15,16,5,6,7,8,9,10,17,22,23,24,25,18,19,20,21,
      27,26,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
      48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,
      70,71,72,73,74,75,76,77,78,79};
  std::vector<std::uint16_t> mirrorParts(rig_->bones().size(), 0xffff);
  std::copy(mirror.begin(), mirror.end(), mirrorParts.begin());
  rig_->setFlipTable(mirrorParts);
  weapons_[0] = std::make_unique<WeaponPreview>(weaponDrs);
  weapon_ = weapons_[0].get();
  for (unsigned i=0;i<16;++i) weaponClips_[0][i]=i;
  weaponClips_[0][16]=knifeToGunClip;
  weaponClips_[0][17]=rig_->appendActions({{asset(weapon,44,"FCV"),{}}});
  std::vector<std::uint8_t> empty(16);
  empty[0] = 64;
  Smd palettes(empty);
  struct Part {
    bool weapon;
    unsigned bin, tpl;
  };
  // cPlLeon::setModel, cObjFn57::setMotion: normal costume, grip hands.
  // Player entry 9 is the normally hidden knife, not a face attachment.
  const Part parts[]{{false, 0, 1},  {false, 6, 1}, {false, 4, 3},
                     {false, 2, 3},  {false, 5, 3}, {true, 6, 13},
                     {false, 20, 13}, {false, 21, 13}};
  std::map<unsigned, unsigned> paletteIds;
  auto appendPart = [&](const Part &part, StaticScene &mesh,
                        std::vector<SkinCorner> &skin) {
    auto model = part.weapon ? SkinnedModel(asset(weapon, part.bin, "BIN"))
                             : SkinnedModel(asset(player, part.bin, "BIN"));
    model.validateSkeleton(rig_->bones().size());
    if (!part.weapon && part.bin == 9) {
      if (model.mesh.joints.size() != body.mesh.joints.size())
        throw std::runtime_error("Leon: Knife skeleton is missing");
      const auto ownerBind = RestJointCenters(body.mesh);
      const auto knifeBind = RestJointCenters(model.mesh);
      for (unsigned joint = 0; joint < ownerBind.size(); ++joint) {
        if (model.mesh.joints[joint].parent != body.mesh.joints[joint].parent)
          throw std::runtime_error("Leon: Knife hierarchy mismatch");
      }
      // cPlBody addModel uses the owner's parts. BIN9 carries a different
      // unused face-part32 rest center; only the joints its skin actually
      // references must share the player's bind centers (original blade=10).
      for (const auto &weight : model.weights)
        for (unsigned i = 0; i < weight.count; ++i)
          for (unsigned axis = 0; axis < 3; ++axis)
            if (std::abs(ownerBind[weight.joints[i]][axis] -
                         knifeBind[weight.joints[i]][axis]) > .001f)
              throw std::runtime_error(
                  "Leon: Knife attachment bind pose mismatch");
    }
    if (!paletteIds.count(part.tpl)) {
      paletteIds[part.tpl] = static_cast<unsigned>(palettes.palettes.size());
      palettes.palettes.push_back(asset(player, part.tpl, "TPL"));
    }
    const auto &m = model.mesh;
    for (const auto &prim : m.primitives) {
      if (prim.triangles.empty())
        continue;
      const auto &header = m.materialHeaders.at(prim.displayList);
      SceneDraw draw;
      draw.first = mesh.vertices.size() / 3;
      draw.count = prim.triangles.size() * 3;
      draw.work = part.weapon ? 600 : part.bin;
      draw.palette = paletteIds.at(part.tpl);
      draw.image = header[12];
      SetSceneSpecular(draw,header,m.sourceFlags);
      draw.repeat = m.colored;
      draw.vertexColors = m.colored && m.vertexAmbient;
      draw.alphaTest = (header[11] & 4) != 0;
      draw.alphaImage = header[14];
      draw.alphaReference = header[20];
      mesh.draws.push_back(draw);
      for (const auto &triangle : prim.triangles)
        for (auto corner : triangle) {
          const auto &ref = prim.vertices.at(corner);
          const auto &p = m.positions.at(ref.position),
                     &n = m.normals.at(ref.normal);
          mesh.vertices.insert(mesh.vertices.end(), p.begin(), p.end());
          mesh.normals.insert(mesh.normals.end(), n.begin(), n.end());
          auto uv = m.texcoords.at(ref.texcoord);
          mesh.uv.insert(mesh.uv.end(), uv.begin(), uv.end());
          auto color = draw.vertexColors
                           ? m.colors.at(ref.color)
                           : std::array<std::uint8_t, 4>{255, 255, 255, 255};
          mesh.colors.insert(mesh.colors.end(), color.begin(), color.end());
          skin.push_back(
              {model.weights.at(model.positionWeights.at(ref.position)),
               model.weights.at(model.normalWeights.at(ref.normal))});
        }
      mesh.triangles += prim.triangles.size();
    }
    ++mesh.included;
  };
  for (const auto &part : parts)
    appendPart(part, mesh_, skin_);
  AttachSceneTextures(palettes, mesh_);
  renderMesh_ = mesh_;
  renderSkin_ = skin_;
  if (hasMeleeMotions())
    appendPart({false, 9, 1}, renderMesh_, renderSkin_);
  // The s04 radio event uses this actual grip even when no shotgun exists.
  // Preload it once so changing hands never mutates an uploaded actor mesh.
  // Wep00 uses PL_ARC12, the empty right hand.
  const auto firstUnarmedDraw=renderMesh_.draws.size();
  appendPart({false,14,13},renderMesh_,renderSkin_);
  for(std::size_t i=firstUnarmedDraw;i<renderMesh_.draws.size();++i)renderMesh_.draws[i].work=1190;
  const auto firstRadioDraw=renderMesh_.draws.size();
  appendPart({false,18,13},renderMesh_,renderSkin_);
  for(std::size_t i=firstRadioDraw;i<renderMesh_.draws.size();++i)renderMesh_.draws[i].work=618;
  AttachSceneTextures(palettes, renderMesh_);
  if (mesh_.fallbackTriangles || mesh_.unresolvedAlphaTriangles ||
      renderMesh_.fallbackTriangles || renderMesh_.unresolvedAlphaTriangles)
    throw std::runtime_error("Leon: Texture source is missing");
}
LeonPreview::~LeonPreview() = default;
void LeonPreview::installWeapon(const std::string &path, unsigned profile) {
  if (!profile || profile >= weapons_.size()) throw std::runtime_error("Leon: Weapon profile");
  if (weapons_[profile]) return;
  const auto kind = profile == 2 ? WeaponKind::ShotgunWep07 : WeaponKind::HandgunWep02;
  const auto *carry=profile>2?OriginalWeaponCarry(profile-3):nullptr;
  auto object = carry?std::make_unique<WeaponPreview>(path,profile-3,playerDrs_):std::make_unique<WeaponPreview>(path,kind);
  auto archive = RoomArchive::LoadDrs(path);
  const auto *identity=OriginalWeaponCarry(object->itemId());
  const auto &actions=object->actions();
  auto motion=[&](unsigned arc,unsigned sequence=0) {
    const auto entry=arc-4;
    // Only bind SEQs named by the controller. Adjacent SEQs can belong to
    // another motion (grenade ARC19 is followed by a longer knife SEQ).
    return AnimationBytes{asset(archive,entry,"FCV"),sequence?asset(archive,sequence-4,"SEQ"):std::vector<std::uint8_t>{},false};
  };
  std::vector<AnimationBytes> clips;
  for(unsigned i=0;i<identity->locomotion.size();++i)
    clips.push_back(motion(identity->locomotion[i],identity->footSequence[i]));
  for(unsigned i=0;i<actions.player.size();++i) {
    const auto arc=actions.player[i];
    clips.push_back(motion(arc,actions.family==WeaponFamily::Handgun && i<2?arc+1:0));
  }
  // Krauser's bow does not replace Leon's default knife table. Retain it
  // explicitly when testing that bonus weapon on Leon.
  for(unsigned i:{0u,2u})clips.push_back(identity->knife[i]
      ?motion(identity->knife[i],identity->knife[i+1])
      :AnimationBytes{asset(RoomArchive::LoadDrs(weapons_[0]->sourcePath()),i?44:45,"FCV"),{}});
  if(actions.family==WeaponFamily::Rocket) {
    clips.push_back(motion(identity->knife[4],identity->knife[5]));
    clips.push_back(motion(identity->knife[6],identity->knife[7]));
    if(!object->infinite())clips.push_back(motion(0x16));
  } else if(actions.family==WeaponFamily::Grenade) {
    for(unsigned arc:{0x13u,0x16u,0x19u})clips.push_back(motion(arc));
  } else if(actions.number==9) {
    clips.push_back(motion(0x1b));
    clips.push_back(motion(0x1c));
  }
  // cPlLeon::{setRightHand,setLeftHand}: equipped right grip from the
  // module's WEP_ARC, PL00 ARC16 left shotgun grip / ARC18 handgun grip.
  // Validate grips before extending the live rig/mesh. Extra profiles load
  // lazily; the bridge uploads this enlarged actor mesh on the next draw.
  const auto player=RoomArchive::LoadDrs(playerDrs_);
  const auto palette=asset(player,13,"TPL");
  StaticScene grips;std::vector<SkinCorner> gripSkin;
  auto appendHand=[&](const std::vector<unsigned char> &bin,unsigned work) {
    const SkinnedModel model(bin);model.validateSkeleton(rig_->bones().size());
    // A hand BIN intentionally has a partial joint header but weights in
    // the owning 119-part Leon skeleton, so the standalone static parser
    // cannot validate it against its own private joint count.
    StaticScene mesh;
    std::vector<unsigned char> empty(16);empty[0]=64;Smd palettes(empty);
    palettes.palettes.push_back(palette);
    std::vector<SkinCorner> skin;
    const auto &m=model.mesh;
    for(const auto &prim:m.primitives) {
      if(prim.triangles.empty()) continue;
      const auto &header=m.materialHeaders.at(prim.displayList);
      SceneDraw draw;draw.first=mesh.vertices.size()/3;draw.count=prim.triangles.size()*3;
      draw.work=work;draw.palette=0;draw.image=header[12];SetSceneSpecular(draw,header,m.sourceFlags);draw.repeat=m.colored;
      draw.vertexColors=m.colored&&m.vertexAmbient;draw.alphaTest=(header[11]&4)!=0;
      draw.alphaImage=header[14];draw.alphaReference=header[20];mesh.draws.push_back(draw);
      for(const auto &triangle:prim.triangles)for(auto corner:triangle) {
        const auto &v=prim.vertices.at(corner);const auto &p=m.positions.at(v.position);
        const auto &n=m.normals.at(v.normal);const auto &uv=m.texcoords.at(v.texcoord);
        mesh.vertices.insert(mesh.vertices.end(),p.begin(),p.end());
        mesh.normals.insert(mesh.normals.end(),n.begin(),n.end());mesh.uv.insert(mesh.uv.end(),uv.begin(),uv.end());
        const auto color=draw.vertexColors?m.colors.at(v.color):std::array<unsigned char,4>{255,255,255,255};
        mesh.colors.insert(mesh.colors.end(),color.begin(),color.end());
        skin.push_back({model.weights.at(model.positionWeights.at(v.position)),model.weights.at(model.normalWeights.at(v.normal))});
      }
      mesh.triangles+=prim.triangles.size();
    }
    mesh.included=1;AttachSceneTextures(palettes,mesh);
    if(mesh.fallbackTriangles || mesh.unresolvedAlphaTriangles) throw std::runtime_error("Leon: Weapon grip texture");
    if(skin.size()!=mesh.vertices.size()/3) throw std::runtime_error("Leon: Weapon grip geometry");
    AppendStaticSceneLayer(grips,mesh);gripSkin.insert(gripSkin.end(),skin.begin(),skin.end());
  };
  if(carry) {
    const bool playerRight=carry->number==28;
    appendHand(asset(playerRight?player:archive,carry->rightHand-4,"BIN"),600+profile);
    if(carry->number==14)appendHand(asset(archive,0xa-4,"BIN"),1300+profile);
    appendHand(asset(carry->weaponLeft?archive:player,carry->leftHand-4,"BIN"),900+profile);
    weaponAmmo_[profile]={object->capacity(),0};
  } else appendHand(asset(archive,profile==2?3:6,"BIN"),600+profile);
  const auto base=rig_->appendActions(clips);
  for(unsigned i=0;i<clips.size();++i)weaponClips_[profile][i]=base+i;
  AppendStaticSceneLayer(renderMesh_,grips);renderSkin_.insert(renderSkin_.end(),gripSkin.begin(),gripSkin.end());
  weapons_[profile]=std::move(object);
}
bool LeonPreview::prepareWeapon(unsigned id) {
  const auto profile=weaponSlot(id);
  if(profile>=weapons_.size())return false;
  if(weapons_[profile])return true;
  const auto *source=OriginalWeaponCarry(id);if(!source)return false;
  std::ostringstream path;path<<playerDrs_.substr(0,playerDrs_.find_last_of('/'))<<"/wep"<<std::setw(2)<<std::setfill('0')<<source->archive<<".drs";
  if(!std::ifstream(path.str(),std::ios::binary))return false;
  try {installWeapon(path.str(),profile);}
  catch(const std::exception &e) {throw std::runtime_error("Original weapon " + std::to_string(id) + ": " + e.what());}
  return true;
}
bool LeonPreview::weaponAttackImplemented() const {return !unarmed_ && weapon_->attackImplemented();}
void LeonPreview::installHandgun(const std::string &path) { installWeapon(path,1); }
void LeonPreview::installShotgun(const std::string &path) { installWeapon(path,2); }
unsigned LeonPreview::equippedWeaponId() const { return unarmed_?0xffff:weapon_->itemId(); }
unsigned LeonPreview::magazineCapacity() const { return weapon_->capacity(); }
unsigned LeonPreview::fireReadyTicks() const { return weapon_->fireReadyTicks(); }
bool LeonPreview::hasRounds() const {return !unarmed_ && (unlimitedAmmo_ || weapon_->infinite() ||
    (weapon_->family()==WeaponFamily::Bow?reserve_:magazine_));}
void LeonPreview::setUnlimitedAmmo(bool value) {
  unlimitedAmmo_=value;
  if(value) {
    for(unsigned i=0;i<weapons_.size();++i)if(weapons_[i])weaponAmmo_[i][0]=weapons_[i]->capacity();
    magazine_=magazineCapacity();
  }
}
bool LeonPreview::equipWeapon(unsigned id) {
  if(id==0xffff) {
    if(!unarmedClips_[0]) {
      // wep00::cObjHand::setMotion: original unarmed idle/walk/run/back/turns.
      const auto archive=RoomArchive::LoadDrs(playerDrs_.substr(0,playerDrs_.find_last_of('/'))+"/wep00.drs");
      const unsigned arcs[]{4,5,6,5,9,8,7};
      std::vector<AnimationBytes> clips;
      for(auto arc:arcs)clips.push_back({asset(archive,arc-4,"FCV"),{}});
      clips.push_back({asset(archive,0x17-4,"FCV"),{}});
      const auto first=rig_->appendActions(clips);unarmedKnifeHolster_=first+7;
      for(unsigned i=0;i<7;++i)unarmedClips_[i]=first+i;
    }
    weaponAmmo_[weaponProfile_]={magazine_,reserve_};
    weaponProfile_=0;weapon_=weapons_[0].get();
    magazine_=weaponAmmo_[0][0];reserve_=weaponAmmo_[0][1];
    unarmed_=true;leftHandNow_=4;scopeShot_.reset();
    const auto heading=heading_;resetMotion();heading_=heading;
    return true;
  }
  unsigned profile=weaponSlot(id);
  if(!prepareWeapon(id) || grabbed() || blastActive() || hurtActive() || doorActive() ||
      ladderActive() || ladderPropActive() || fenceActive() || jumpActive() || windowActive() || pushActive() || itemExamActive() || action_==PlayerAction::Kick || !alive()) return false;
  if(profile==weaponProfile_ && !unarmed_) return true;
  unarmed_=false;
  weaponAmmo_[weaponProfile_]={magazine_,reserve_};
  weaponProfile_=profile;weapon_=weapons_[profile].get();
  rocketDiscarding_=rocketDiscarded_=false;scopeShot_.reset();
  leftHandOld_=leftHandNow_;leftHandNow_=profile>2?0:profile==2?2:4;
  magazine_=weaponAmmo_[profile][0];reserve_=weaponAmmo_[profile][1];
  syncLocomotion();
  const auto heading=heading_;resetMotion();heading_=heading;
  return true;
}
bool LeonPreview::setWeaponAmmo(unsigned id,unsigned loaded,unsigned rounds) {
  const unsigned profile=weaponSlot(id);
  if(profile>=weapons_.size() || !OriginalWeaponCarry(id) || loaded>OriginalWeaponCapacity(id) || rounds>65535-loaded) return false;
  if(unlimitedAmmo_ && weapons_[profile])loaded=weapons_[profile]->capacity();
  weaponAmmo_[profile]={loaded,rounds};
  if(profile==weaponProfile_) {magazine_=loaded;reserve_=rounds;if(loaded)rocketDiscarded_=false;}
  return true;
}
bool LeonPreview::setWeaponPowerLevel(unsigned id, unsigned level) {
  const unsigned profile = weaponSlot(id);
  if (profile >= weapons_.size() || level > 6) return false;
  weaponPowerLevels_[profile] = level;
  return true;
}
unsigned LeonPreview::weaponPowerLevel(unsigned id) const {
  const unsigned profile = weaponSlot(id);
  return profile < weapons_.size() ? weaponPowerLevels_[profile] : 0;
}
unsigned LeonPreview::handgunMagazine() const { return weaponProfile_==2?weaponAmmo_[weapons_[1]?1:0][0]:magazine_; }
unsigned LeonPreview::handgunReserve() const { return weaponProfile_==2?weaponAmmo_[weapons_[1]?1:0][1]:reserve_; }
unsigned LeonPreview::shotgunMagazine() const { return weaponProfile_==2?magazine_:weaponAmmo_[2][0]; }
unsigned LeonPreview::shotgunReserve() const { return weaponProfile_==2?reserve_:weaponAmmo_[2][1]; }
bool LeonPreview::canEquip(unsigned id) const {
  const unsigned profile=weaponSlot(id);
  return id==0xffff || (profile<weapons_.size() && (bool(weapons_[profile]) || OriginalWeaponCarry(id)!=nullptr));
}
std::optional<std::pair<unsigned,unsigned>> LeonPreview::weaponAmmo(unsigned id) const {
  const unsigned profile=weaponSlot(id);
  if(profile>=weapons_.size() || !OriginalWeaponCarry(id)) return std::nullopt;
  if(profile==weaponProfile_) return std::pair<unsigned,unsigned>{magazine_,reserve_};
  return std::pair<unsigned,unsigned>{weaponAmmo_[profile][0],weaponAmmo_[profile][1]};
}
unsigned LeonPreview::addShotgunAmmo(unsigned count) {
  const auto loaded=shotgunMagazine(),rounds=shotgunReserve();
  const auto accepted=std::min(count,65535-loaded-rounds);
  setWeaponAmmo(0x2c,loaded,rounds+accepted);return accepted;
}
void LeonPreview::syncLocomotion() {
  injuredLocomotion_=life_<800 && equippedWeaponId()!=0x6d;
  std::array<unsigned,7> foot{};
  if(injuredLocomotion_)foot=injuredClips_;
  else if(unarmed_)foot=unarmedClips_;
  else std::copy_n(weaponClips_[weaponProfile_].begin(),7,foot.begin());
  rig_->setLocomotionClips(foot);
}
void LeonPreview::resetMotion() {
  syncLocomotion();
  rig_->enableAttachedCamera(false);
  rig_->reset();
  rig_->setWaistYaw(0);
  state_ = Locomotion::Idle;
  heading_ = 3.7415927f;
  motionAccumulator_ = 0;
  quickTurnPending_ = quickTurning_ = false;
  runPressTicks_ = 0;
  action_ = PlayerAction::Footwork;
  firePending_ = reloadPending_ = neutralGate_ = reloadCommitted_ = false;
  aimRate_ = waistYaw_ = waistTarget_ = aimAcceleration_ = 0;
  drawCameraYawRemaining_ = 0;
  aimBlendTicks_ = 0;
  fireTicks_ = 0;
  fireRecoilTail_ = fireAimPending_ = false;
  queuedGunAction_ = PlayerAction::Footwork;
  hurtTicks_=hurtType_=0; hurtMotionPending_=false;
  jumpFallPlayback_={};
  returningFromReload_ = false;
  weaponFrame_ = 0;
  scopeSway_={};rifleReloadScope_=false;
  weaponNewFrame_ = false;
  weaponEmptyVariant_ = magazine_ == 0;
  weapon_->sample(WeaponMotion::Idle, 0, weaponEmptyVariant_);
  resistPending_ = grabTicks_ = 0;
  bladeVisible_ = knifeFinishing_ = knifeToGun_ = knifeFirePending_ = false;
  meleeEvent_.reset();
  kickTicks_ = 0;
  previousBladeTip_ = kickTarget_ = {};
  catchTurn_ = 0;
  blastAirHeight_ = 0;
  catchAdjustment_ = partnerDelta_ = {};
  doorAdjustment_ = {};
  doorHeading_ = 0;
  doorTicks_ = 0;
  previousRenderFootwork_ = false;
  previousRenderTraversal_ = false;
  previousRenderJoints_.clear(); previousRenderAttached_ = {};
  jumpPlayback_ = {};
  jumpCameraActive_ = false; jumpCameraWorld_ = nullptr;
  sequenceSounds_.clear();
  fencePlayback_ = {};
  ladderPlayback_ = {};
  objectLadderPlayback_ = {};
  objectLadder_ = false;
  ladderSounds_.clear();
  ladderLandingFailed_ = false;
  if (!alive())
    action_ = PlayerAction::Dead;
}
void LeonPreview::reset() {
  weaponAmmo_={{{10,60},{10,20},{6,0}}};
  weaponPowerLevels_ = {};
  magazine_ = weaponAmmo_[weaponProfile_][0];
  reserve_ = weaponAmmo_[weaponProfile_][1];
  shots_ = 0;
  life_ = MaxLife;
  damageWait_ = 0;
  resetMotion();
}
void LeonPreview::setPlacement(const std::array<double, 3> &feet,
                               float heading) {
  for (auto p : feet)
    if (!std::isfinite(p))
      throw std::runtime_error("Leon: Position");
  if (!std::isfinite(heading))
    throw std::runtime_error("Leon: Direction");
  resetMotion();
  feet_ = feet;
  heading_ = float(std::remainder(double(heading), 6.283185307179586));
  previousRenderFeet_ = feet_; previousRenderHeading_ = heading_;
  previousRenderFootwork_ = false;
  previousRenderTraversal_ = false;
}
void LeonPreview::setRenderInterpolation(bool value) {
  renderInterpolation_ = value;
  previousRenderFeet_ = feet_; previousRenderHeading_ = heading_;
  previousRenderFootwork_ = false;
  previousRenderTraversal_ = false;
}
CollisionVec LeonPreview::renderFeet() const {
  if (!renderInterpolation_ ||
      (!traversalActive() && action_ != PlayerAction::Footwork) ||
      (!previousRenderTraversal_ && (!previousRenderFootwork_ || action_ != PlayerAction::Footwork)))
    return feet_;
  auto result = feet_;
  const auto alpha = std::clamp(motionAccumulator_, 0., 1.);
  for (unsigned k = 0; k < 3; ++k)
    result[k] = previousRenderFeet_[k] + alpha * (feet_[k] - previousRenderFeet_[k]);
  return result;
}
float LeonPreview::renderHeading() const {
  if (!renderInterpolation_ ||
      (!traversalActive() && action_ != PlayerAction::Footwork) ||
      (!previousRenderTraversal_ && (!previousRenderFootwork_ || action_ != PlayerAction::Footwork)))
    return heading_;
  const auto delta = std::remainder(double(heading_) - previousRenderHeading_, 6.283185307179586);
  return float(previousRenderHeading_ + std::clamp(motionAccumulator_, 0., 1.) * delta);
}
Matrix34 LeonPreview::renderPlacement() const {
  SmdPlacement pose; const auto feet = renderFeet();
  for (unsigned k = 0; k < 3; ++k) pose.position[k] = float(feet[k]);
  pose.rotation[1] = renderHeading(); pose.scale = {1,1,1};
  return PlacementMatrix(pose);
}
const std::vector<Matrix34> &LeonPreview::renderJointMatrices() const {
  const auto &current = rig_->jointMatrices();
  if (!renderInterpolation_ || !previousRenderTraversal_ ||
      (!traversalActive() && action_ != PlayerAction::Footwork) ||
      previousRenderJoints_.size() != current.size()) return current;
  interpolatedRenderJoints_.resize(current.size());
  const auto alpha = std::clamp(motionAccumulator_, 0., 1.);
  for (unsigned i = 0; i < current.size(); ++i)
    interpolatedRenderJoints_[i] =
        InterpolateMotionMatrix(previousRenderJoints_[i], current[i], alpha);
  return interpolatedRenderJoints_;
}
void LeonPreview::suspendClock() {
  previousRenderFootwork_ = false;
  previousRenderTraversal_ = false;
  motionAccumulator_ = 0;
  quickTurnPending_ = false;
  runPressTicks_ = 0;
  firePending_ = reloadPending_ = false;
  knifeFirePending_ = false;
  meleeEvent_.reset();
  resistPending_ = 0;
  partnerDelta_ = {};
  neutralGate_ = true;
}
void LeonPreview::inheritInventory(const LeonPreview &previous) {
  unlimitedHealth_ = previous.unlimitedHealth_;
  damageRank_=previous.damageRank_;
  renderInterpolation_ = previous.renderInterpolation_;
  if(previous.weaponProfile_ || previous.unarmed_)
    equipWeapon(previous.equippedWeaponId());
  weaponAmmo_=previous.weaponAmmo_;
  weaponPowerLevels_=previous.weaponPowerLevels_;
  weaponAmmo_[previous.weaponProfile_]={previous.magazine_,previous.reserve_};
  magazine_=weaponAmmo_[weaponProfile_][0];reserve_=weaponAmmo_[weaponProfile_][1];
  shots_ = previous.shots_;
  meleeSerial_ = previous.meleeSerial_;
  weaponRandom_=previous.weaponRandom_;
  eyeState_=previous.eyeState_;
  weaponSway_.amplitude=previous.weaponSway_.amplitude;
  weaponEmptyVariant_ = magazine_ == 0;
  weapon_->sample(WeaponMotion::Idle, 0, weaponEmptyVariant_);
  life_=unlimitedHealth_?MaxLife:previous.life_;
  syncLocomotion();
  if(!alive()) { action_=PlayerAction::Dead; damageWait_=0x80; }
}
unsigned LeonPreview::addHandgunAmmo(unsigned count) {
  constexpr unsigned maximum = 65535; // cItemMgr::bulletNumTotal returns u16.
  const unsigned loaded=handgunMagazine(), rounds=handgunReserve();
  const unsigned available = maximum - loaded - rounds;
  const unsigned accepted = std::min(count, available);
  setWeaponAmmo(weapons_[1]?0x23:0x21,loaded,rounds+accepted);
  return accepted;
}
unsigned LeonPreview::heal(unsigned amount) {
  if (!alive())
    return 0;
  const unsigned accepted = std::min(amount, MaxLife - life_);
  life_ += accepted;
  return accepted;
}
void LeonPreview::beginGrab(float turn,
                            const std::array<double, 3> &adjustment) {
  if (!hasGrabMotions_ || !alive() || grabbed() || ladderActive() || ladderPropActive() || fenceActive() || jumpActive() || windowActive() || itemExamActive())
    throw std::runtime_error("Leon: Grab start unavailable");
  if (!std::isfinite(turn))
    throw std::runtime_error("Leon: Grab direction");
  for (auto value : adjustment)
    if (!std::isfinite(value))
      throw std::runtime_error("Leon: Grab correction");
  quickTurnPending_ = quickTurning_ = false;
  runPressTicks_ = 0;
  drawCameraYawRemaining_ = 0;
  firePending_ = reloadPending_ = reloadCommitted_ = false;
  knifeFirePending_ = bladeVisible_ = knifeFinishing_ = knifeToGun_ = false;
  meleeEvent_.reset();
  kickTicks_ = 0;
  chainsawGrab_=false;
  action_ = PlayerAction::Grabbed;
  state_ = Locomotion::Idle;
  waistYaw_ = waistTarget_ = aimRate_ = aimAcceleration_ = 0;
  rig_->setWaistYaw(0);
  rig_->startActionFlags(holdClip, 5, 0, 1);
  catchTurn_ = turn;
  catchAdjustment_ = adjustment;
  partnerDelta_ = {};
  resistPending_ = grabTicks_ = 0;
  weaponFrame_ = 0;
  weaponNewFrame_ = false;
  weaponEmptyVariant_ = magazine_ == 0;
  weapon_->sample(WeaponMotion::Idle, 0, weaponEmptyVariant_);
}
void LeonPreview::beginChainsawGrab(float turn,const CollisionVec &adjustment) {
  beginGrab(turn,adjustment);chainsawGrab_=true;
  if(!unlimitedHealth_)life_=1;
  damageWait_=0x80;rig_->startActionFlags(chainsawClips_[0],5,0,1);
}
void LeonPreview::endChainsawGrab(bool escaped) {
  if(!chainsawGrab_ || action_!=PlayerAction::Grabbed)return;
  if(escaped || unlimitedHealth_) {
    action_=PlayerAction::GrabEscape;rig_->startActionFlags(chainsawClips_[1],5,0,1);grabTicks_=resistPending_=0;
  } else {
    setFatalLife();chainsawGrab_=false;partnerDelta_=catchAdjustment_={};catchTurn_=0;
    action_=PlayerAction::HurtDead;hurtType_=6;hurtMotionPending_=true;hurtTicks_=0;damageWait_=0x80;
  }
}
void LeonPreview::releaseGrab(bool escaped) {
  if (action_ != PlayerAction::Grabbed)
    throw std::runtime_error("Leon: invalid grab end");
  action_ = escaped ? PlayerAction::GrabEscape : PlayerAction::GrabRelease;
  rig_->startActionFlags(escaped ? escapeClip : failedClip, 5, 0, 1);
  grabTicks_ = 0;
  resistPending_ = 0;
}
void LeonPreview::finishGrab() {
  if (!grabbed())
    return;
  chainsawGrab_=false;
  const bool gettingUp = action_ == PlayerAction::Recover;
  action_ = alive() ? PlayerAction::Footwork : PlayerAction::Dead;
  damageWait_ = gettingUp ? 5 : 30;
  firePending_ = reloadPending_ = false;
  resistPending_ = 0;
  partnerDelta_ = catchAdjustment_ = {};
  catchTurn_ = 0;
  neutralGate_ = true;
}
void LeonPreview::followCatch(const std::array<double, 3> &delta) {
  for (auto value : delta)
    if (!std::isfinite(value))
      throw std::runtime_error("Leon: Grab partner motion");
  partnerDelta_ = delta;
}
unsigned LeonPreview::takeResistPresses() {
  const auto presses = resistPending_;
  resistPending_ = 0;
  return presses;
}
void LeonPreview::takeDamage(unsigned damage, bool keepOne) {
  const auto reduced=GrabPlayerLifeDown(life_,damage,keepOne,damageRank_);
  life_=unlimitedHealth_?MaxLife:reduced; // Keep the original damage RNG calls.
}
void LeonPreview::setDamageDifficulty(unsigned rank) {
  if(rank>10) throw std::runtime_error("Leon: unsupported damage rank");
  damageRank_=rank;
}
void LeonPreview::setFatalLife() { if (!unlimitedHealth_) life_ = 0; }
void LeonPreview::receiveContinuousFlame() {
  if(!alive() || damageWait_ || grabbed() || hurtActive() || blastActive() || traversalActive())return;
  takeDamage(10,true);continuousFlameTicks_+=0x18;
  if(continuousFlameTicks_>0xfe){continuousFlameTicks_=0;receiveGanadoAxeHit(feet_,0,0);}
}
bool LeonPreview::receiveGanadoAxeHit(const CollisionVec &attacker,
                                    unsigned damage, unsigned damageType) {
  for(double value:attacker) if(!std::isfinite(value) || std::abs(value)>1e8)
    throw std::runtime_error("Leon: invalid axe attacker");
  if(damage>32767 || damageType>5)
    throw std::runtime_error("Leon: unsupported axe damage");
  if(!alive() || damageWait_ || grabbed() || blastActive() || hurtActive() ||
      traversalActive() || itemExamActive()) return false;
  takeDamage(damage,false);
  // PlSetDamage skips the setDamage call on a fatal middle/low hit. Its
  // Die0 entry uses the same PL4c/4d motion. Other kinds call setDamage's
  // unconditional LifeDownSet2(0,keep1), preserving the source RNG stream.
  if(alive() || damageType<2) takeDamage(0,true);
  hurtType_=alive()?damageType:6;
  hurtMotionPending_=true;
  action_=alive()?PlayerAction::Hurt:PlayerAction::HurtDead;
  damageWait_=30;
  state_=Locomotion::Idle;
  quickTurnPending_=quickTurning_=false; runPressTicks_=0;
  firePending_=reloadPending_=reloadCommitted_=false;
  fireRecoilTail_=fireAimPending_=false;
  queuedGunAction_=PlayerAction::Footwork;
  knifeFirePending_=bladeVisible_=knifeFinishing_=knifeToGun_=false;
  meleeEvent_.reset();
  drawCameraYawRemaining_=waistYaw_=waistTarget_=aimRate_=aimAcceleration_=0;
  rig_->setWaistYaw(0);
  return true;
}
void LeonPreview::hurtTick(WalkProbe &probe, bool movementHeld) {
  if(hurtMotionPending_) {
    const unsigned clip=hurtType_==6?4:hurtType_<2?hurtType_:2+(hurtType_&1);
    rig_->startActionFlags(hurtClips_[clip],5,0,1);
    hurtTicks_=hurtType_==6?999:hurtType_==0?30:35;
    hurtMotionPending_=false;
  }
  damageWait_|=0x80;
  if(hurtTicks_) --hurtTicks_;
  const auto root=rig_->advanceRootAction();
  const auto c=std::cos(heading_),s=std::sin(heading_);
  probe.moveDelta({c*root.translation[0]+s*root.translation[2],root.translation[1],
      -s*root.translation[0]+c*root.translation[2]});
  feet_=probe.position();
  heading_=float(std::remainder(double(heading_)+root.yaw,6.283185307179586));
  if(!root.ended && !(rig_->status().state&3) && !(hurtTicks_==0 && movementHeld)) return;
  if(hurtType_==6) { action_=PlayerAction::Dead; damageWait_=0x80; }
  else { action_=PlayerAction::Footwork; damageWait_=5; neutralGate_=true; }
}
CollisionVec LeonPreview::parts0World() const {
  const auto &parts = rig_->jointMatrices();
  if (parts.empty())
    throw std::runtime_error("Leon: original root part missing");
  const auto &root = parts.front();
  const auto world = TransformPoint(placement(), {root[3], root[7], root[11]});
  return {world[0], world[1], world[2]};
}
CollisionVec LeonPreview::sceneryBodyCenter() const {
  auto center=parts0World(); center[1]=feet_[1]+400;
  return center;
}
bool LeonPreview::applyBlast(const ObjectPlayerBlastHit &hit,
                            const CollisionVec &center, WalkProbe &probe) {
  for (double value : center)
    if (!std::isfinite(value) || std::abs(value) > 1e8)
      throw std::runtime_error("Leon: invalid blast center");
  if (hit.baseDamage != (hit.inner ? 1200u : 0u))
    throw std::runtime_error("Leon: unsupported blast damage");
  if (!alive() || damageWait_ || grabbed() || blastActive() || ladderActive() || ladderPropActive() || fenceActive() || jumpActive() || windowActive() || itemExamActive())
    return false;
  if (hit.inner)
    takeDamage(hit.baseDamage, life_ >= 501);
  // PlSetDamage(...,0) still enters cPlayer::setDamage, whose first call is
  // LifeDownSet2(arg0,keep1). Preserve its unconditional RNG consumption.
  takeDamage(0, true);
  quickTurnPending_ = quickTurning_ = false;
  runPressTicks_ = 0;
  drawCameraYawRemaining_ = 0;
  firePending_ = reloadPending_ = reloadCommitted_ = false;
  knifeFirePending_ = bladeVisible_ = knifeFinishing_ = knifeToGun_ = false;
  meleeEvent_.reset();
  kickTicks_ = 0;
  resistPending_ = grabTicks_ = 0;
  catchTurn_ = 0;
  catchAdjustment_ = partnerDelta_ = {};
  waistYaw_ = waistTarget_ = aimRate_ = aimAcceleration_ = 0;
  rig_->setWaistYaw(0);
  state_ = Locomotion::Idle;
  blastAirHeight_ = 0;
  // PlSetDamage supplies direction123 (keep). Do not turn Leon towards the
  // barrel: original damageBlow / damageBlast preserve his current heading.
  action_ = !alive() ? PlayerAction::BlastDead
            : hit.inner ? PlayerAction::BlastBlow
                        : PlayerAction::BlastStagger;
  rig_->startActionFlags(blastClips_[!alive() ? 1 : hit.inner ? 0 : 3],
                         5, 0, 1);
  damageWait_ = 0x80 | 30;
  probe.setActorBlocker(std::nullopt);
  return true;
}
void LeonPreview::blastTick(WalkProbe &probe) {
  damageWait_ = 0x80 | 30; // Original damage handler's protected high bit.
  const auto root = rig_->advanceRootAction();
  const float c = std::cos(heading_), s = std::sin(heading_);
  const double previousY = probe.position()[1] + blastAirHeight_;
  probe.moveDelta({c * root.translation[0] + s * root.translation[2],
                   root.translation[1],
                   -s * root.translation[0] + c * root.translation[2]});
  // Preserve authored airborne root displacement over the SAT floor. This
  // selected flat-floor adapter does not implement cPlayer's ledge-fall state.
  blastAirHeight_ = std::max(0., previousY + root.translation[1] -
                                    probe.position()[1]);
  feet_ = probe.position();
  feet_[1] += blastAirHeight_;
  heading_ = float(std::remainder(double(heading_) + root.yaw,
                                  6.283185307179586));
  // Original cPlayer::motionMove treats the flagged recovery's first loop
  // state as completion too; PL0x4f/0x50 is started with source flags5.
  const bool recoveryLoop = action_ == PlayerAction::BlastRecover &&
                            (rig_->status().state & 3);
  if (!root.ended && !recoveryLoop)
    return;
  if (action_ == PlayerAction::BlastBlow) {
    action_ = PlayerAction::BlastRecover;
    blastAirHeight_ = 0;
    feet_ = probe.position();
    rig_->startActionFlags(blastClips_[2], 3, 0, 5);
  } else if (action_ == PlayerAction::BlastDead) {
    action_ = PlayerAction::Dead; // Original Pl_R0_Die2 holds the final pose.
    damageWait_ = 0x80;
  } else {
    action_ = PlayerAction::Footwork;
    blastAirHeight_ = 0;
    feet_ = probe.position();
    damageWait_ = 5;
    neutralGate_ = true;
  }
}
void LeonPreview::step(WalkProbe &probe, double turn, double forward,
                       bool runHeld, bool runPressed, double seconds) {
  step(probe, turn, forward, runHeld, runPressed, seconds, {});
}
void LeonPreview::step(WalkProbe &probe, double turn, double forward,
                       bool runHeld, bool runPressed, double seconds,
                       const PlayerActionInput &source,
                       const PlayerSimulationTick &afterTick,
                       const std::function<void()> &beforeTick) {
  if (!std::isfinite(turn) || !std::isfinite(forward) ||
      !std::isfinite(seconds) || seconds < 0 || !std::isfinite(source.yaw) ||
      !std::isfinite(source.pitch) ||
      (source.hasCameraLook && (!std::isfinite(source.cameraYaw) ||
                                !std::isfinite(source.cameraPitch))))
    throw std::runtime_error("Leon: invalid controls");
  if (!probe.ready())
    return;
  turn = std::clamp(turn, -1., 1.);
  forward = std::clamp(forward, -1., 1.);
  const auto moveKeys = GameCubeMappedDirections(turn, forward);
  auto control = source;
  control.yaw = std::clamp(control.yaw, -1., 1.);
  control.pitch = std::clamp(control.pitch, -1., 1.);
  if (neutralGate_) {
    // suspendClock can run while the world has no neutral simulation tick
    // (inventory, room/event handoff). A fresh shoulder edge already proves
    // the old held button was released. Keep blocking buttons held THROUGH
    // the interruption, but accept the first new press immediately.
    if ((!control.aimHeld && !control.fireHeld && !control.knifeHeld) ||
        control.aimPressed || control.knifePressed)
      neutralGate_ = false;
    if (neutralGate_) control = {};
  }
  if (action_ == PlayerAction::Grabbed)
    resistPending_ += GrabPressCount(control.resistPressed) +
                      std::min(source.resistancePresses, 64u);
  if (ladderActive() || ladderPropActive() || fenceActive() || jumpActive() || windowActive() || itemExamActive() || hurtActive() || dogTrapActive() || roomScriptActive()) control = {};
  if (hasMeleeMotions() && control.knifeHeld)
    knifeFirePending_ |= control.firePressed;
  else if (!knifeActive())
    knifeFirePending_ = false;
  if (control.aimHeld) {
    firePending_ |= control.firePressed;
    reloadPending_ |= control.reloadPressed;
  } else {
    firePending_ = reloadPending_ = false;
  }
  if (runPressed && !quickTurning_ && action_ == PlayerAction::Footwork &&
      !control.aimHeld && !control.knifeHeld)
    quickTurnPending_ = true;
  feet_ = probe.position();
  if (itemExamActive()) {
    // SceUpCutStart/STA_ITEM_GET suspend Leon's motion, root, input and
    // authored attack clocks for the entire item confirmation/case flow.
    motionAccumulator_ = 0;
    previousRenderFootwork_ = false;
    return;
  }
  if (blastActive() || action_ == PlayerAction::Dead)
    feet_[1] += blastAirHeight_;
  CollisionVec previousSceneryCenter{};
  const auto completeTick = [&](const PlayerShot *shot) {
    // CameraMove follows the player's move once per original simulation
    // tick, even when fire00 also calls the stick controller.
    if(scopeReady()) {
      WeaponRandomOwner random(weaponRandom_);
      float x=0,y=0,gain=0;
      if(!scopeSway_.countdown){x=fRand1_1();y=fRand1_1();gain=fRand0_1();}
      OriginalScopeSway(scopeSway_,x,y,gain);
    }
    // cPlayer::move: MotBase/Neck, moveEye, Body/partsWorldCalc, contact.
    eyeTick();
    // cPlayer::move resolves scenery after Motion/Body/partsWorldCalc,
    // including idle rotation and weapon poses with no actor root movement.
    // Traversal/grab/blow adapters retain their existing atari-off contracts.
    if(probe.config().originalScenerySphere && !previousRenderTraversal_ &&
        (action_==PlayerAction::Footwork || action_==PlayerAction::Draw ||
         action_==PlayerAction::Aim || action_==PlayerAction::Fire ||
         action_==PlayerAction::Reload || action_==PlayerAction::Holster ||
         knifeActive() || action_==PlayerAction::Kick)) {
      const auto before=feet_;
      probe.resolvePlayerScenery(previousSceneryCenter,sceneryBodyCenter());
      feet_=probe.position();
      // Presentation may interpolate authored root travel, but must not
      // interpolate back into contact removed by this native collision tick.
      for(unsigned k=0;k<3;++k) previousRenderFeet_[k]+=feet_[k]-before[k];
    }
    emitSequenceSound();
    if (afterTick) afterTick(shot);
  };
  motionAccumulator_ += std::min(seconds, .05) * 30;
  while (motionAccumulator_ + 1e-9 >= 1) {
    motionAccumulator_ = std::max(0., motionAccumulator_ - 1);
    ++nativeTicks_;
    previousRenderFeet_ = feet_; previousRenderHeading_ = heading_;
    previousRenderFootwork_ = action_ == PlayerAction::Footwork;
    previousRenderTraversal_ = traversalActive();
    if (previousRenderTraversal_) {
      previousRenderJoints_ = rig_->jointMatrices();
      previousRenderAttached_ = rig_->attachedCamera();
    }
    // cPlayer::keyConfigTypeA keeps the run-button trigger for two original
    // ticks, so pressing B just before moving the stick back still turns 180.
    if (runPressTicks_)
      --runPressTicks_;
    if (quickTurnPending_) {
      runPressTicks_ = 2;
      quickTurnPending_ = false;
    }
    meleeEvent_.reset();
    if (damageWait_)
      --damageWait_;
    if (beforeTick)
      beforeTick();
    // Body rectangles may correct a re-enabled moving leaf before root motion.
    feet_ = probe.position();
    if (blastActive() || action_ == PlayerAction::Dead) feet_[1] += blastAirHeight_;
    previousSceneryCenter=sceneryBodyCenter();
    if (blastActive()) {
      quickTurnPending_ = firePending_ = reloadPending_ = false;
      runPressTicks_ = 0;
      blastTick(probe);
      completeTick(nullptr);
      continue;
    }
    if(hurtActive()) {
      hurtTick(probe, moveKeys!=0 || runHeld);
      completeTick(nullptr);
      continue;
    }
    if(dogTrapActive()) {
      quickTurnPending_=firePending_=reloadPending_=knifeFirePending_=false;
      runPressTicks_=0;dogTrapTick(probe);completeTick(nullptr);continue;
    }
    if(roomScriptActive()) {
      quickTurnPending_=firePending_=reloadPending_=knifeFirePending_=false;
      runPressTicks_=0;roomScriptTick(probe);completeTick(nullptr);continue;
    }
    if (grabbed() || action_ == PlayerAction::Dead) {
      quickTurnPending_ = firePending_ = reloadPending_ = false;
      runPressTicks_ = 0;
      if (grabbed())
        grabTick(probe);
      completeTick(nullptr);
      continue;
    }
    if (windowActive()) {
      quickTurnPending_ = firePending_ = reloadPending_ = knifeFirePending_ = false;
      runPressTicks_ = 0;
      windowTick(probe);
      completeTick(nullptr);
      continue;
    }
    if (jumpActive()) {
      quickTurnPending_ = firePending_ = reloadPending_ = knifeFirePending_ = false;
      runPressTicks_ = 0;
      if(jumpFallActive()) jumpFallTick(probe); else jumpTick(probe);
      completeTick(nullptr);
      continue;
    }
    if (ladderActive()) {
      quickTurnPending_ = firePending_ = reloadPending_ = knifeFirePending_ = false;
      runPressTicks_ = 0;
      ladderTick(probe);
      completeTick(nullptr);
      continue;
    }
    if (ladderPropActive()) {
      quickTurnPending_ = firePending_ = reloadPending_ = knifeFirePending_ = false;
      runPressTicks_ = 0;
      ladderPropTick(probe);
      completeTick(nullptr);
      continue;
    }
    if (fenceActive()) {
      quickTurnPending_ = firePending_ = reloadPending_ = knifeFirePending_ = false;
      runPressTicks_ = 0;
      fenceTick(probe, moveKeys != 0 || runHeld);
      completeTick(nullptr);
      continue;
    }
    if (doorActive()) {
      quickTurnPending_ = firePending_ = reloadPending_ = knifeFirePending_ = false;
      runPressTicks_ = 0;
      doorTick(probe);
      completeTick(nullptr);
      continue;
    }
    if (pushActive()) {
      quickTurnPending_ = firePending_ = reloadPending_ = knifeFirePending_ = false;
      runPressTicks_ = 0;
      pushTick(probe, (moveKeys & MoveForward) != 0);
      completeTick(nullptr);
      continue;
    }
    syncLocomotion();
    if (meleeTick(probe, control, turn, forward)) {
      quickTurnPending_ = false;
      runPressTicks_ = 0;
      completeTick(nullptr);
      continue;
    }
    const unsigned previousShots = shots_;
    if (actionTick(control, moveKeys != 0)) {
      quickTurnPending_ = false;
      runPressTicks_ = 0;
      weaponTick();
      if (previousShots != shots_) {
        const auto shot = shotRay(true);
        // Both original fire00 routines kick the stored aim AFTER tracing
        // the bullet. The fresh default lockRandCtr is zero, but the two
        // original random calls still occur and the centers are restored.
        // pl_rifle's fire00 traces CameraScope directly. PlWepLockRand is
        // a third-person muzzle owner; applying it here overwrites the
        // scope pitch with its unrelated (usually zero) stored center.
        if(!usesScope())weaponSway(2);
        if(!weapon_->shotgun()) {
          // Handgun fire's dispatcher runs PlWepLockCtrl after fire00;
          // shotgun fire00 waits until pump frame10 before doing so.
          aimTick(control);
          if(weapon_->family()!=WeaponFamily::Rifle)rig_->setBlend3Rate(aimRate_);
        }
        completeTick(afterTick?&shot:nullptr);
      } else completeTick(nullptr);
      continue;
    }
    if ((moveKeys & MoveBack) && runPressTicks_ && !quickTurning_) {
      quickTurning_ = true;
    }
    const auto previousState = state_;
    if (quickTurning_) {
      state_ = Locomotion::QuickTurn;
    } else {
      state_ = GameCubeLocomotion(moveKeys, runHeld);
    }
    if (state_ == Locomotion::Run && previousState != state_) {
      waistYaw_ = waistTarget_ = 0;
      rig_->setWaistYaw(0);
    }
    // Source movement clips run at authored speed. Stick magnitude chooses
    // digital sectors; it does not slow the gait or in-place turn animation.
    auto motion = rig_->tick(state_);
    auto c = std::cos(heading_), s = std::sin(heading_);
    probe.moveDelta({c * motion.translation[0] + s * motion.translation[2],
                     motion.translation[1],
                     -s * motion.translation[0] + c * motion.translation[2]});
    heading_ = GameCubeSteer(float(heading_ + motion.yaw), state_, moveKeys);
    feet_ = probe.position();
    // pl_R1_Turn180: Leon permits held movement/aim to end at frame19;
    // otherwise the authored clip runs to completion. Never retrigger on B.
    if (motion.ended || (quickTurning_ && rig_->sequenceFrame() >= 19 &&
                         (moveKeys || control.aimHeld))) {
      quickTurning_ = false;
      runPressTicks_ = 0;
    }
    weaponTick();
    completeTick(nullptr);
  }
}
std::optional<JumpDownOffer> LeonPreview::jumpOffer(const WalkProbe &probe) const {
  if(!alive() || action_!=PlayerAction::Footwork) return {};
  return FindJumpDown(probe.world(),probe,heading_,jumpPath_,jumpLandingTravel_);
}
std::optional<JumpFallOffer> LeonPreview::jumpFallOffer(const WalkProbe &probe) const {
  if(!alive() || action_!=PlayerAction::Footwork) return {};
  return FindJumpFall(probe.world(),probe,heading_,jumpFallPath_,jumpFallLandingTravel_);
}
void LeonPreview::beginJumpFall(WalkProbe &probe, const JumpFallOffer &offer) {
  const auto checked=jumpFallOffer(probe);
  if(!checked || checked->wall.polygon!=offer.wall.polygon ||
      checked->landing!=offer.landing || checked->heading!=offer.heading ||
      checked->heightAdjustment!=offer.heightAdjustment)
    throw std::runtime_error("Leon: long jump edge or landing unavailable");
  jumpFallRollback_=probe.position(); jumpFallRollbackHeading_=heading_;
  jumpFallPlayback_={};
  jumpFallPlayback_.feet=probe.position();
  jumpFallPlayback_.heading=offer.heading;
  jumpFallPlayback_.heightAdjustment=offer.heightAdjustment;
  heading_=offer.heading;
  rig_->setWaistYaw(0); rig_->startActionFlags(jumpFallClips_[0],3,0,0x201);
  const auto pelvis=TransformPoint(placement(),TransformPoint(rig_->jointMatrices().at(4),{0,0,0}));
  jumpFallPlayback_.previousPelvis={pelvis[0],pelvis[1],pelvis[2]};
  action_=PlayerAction::JumpFall; state_=Locomotion::Idle; damageWait_=0x80;
  previousRenderTraversal_=previousRenderFootwork_=false;
  jumpCameraActive_=false;
  quickTurnPending_=quickTurning_=firePending_=reloadPending_=knifeFirePending_=false;
  runPressTicks_=0; bladeVisible_=false; meleeEvent_.reset();
  drawCameraYawRemaining_=waistYaw_=waistTarget_=aimRate_=aimAcceleration_=0;
}
void LeonPreview::jumpFallTick(WalkProbe &probe) {
  auto &state=jumpFallPlayback_;
  ++state.ticks;
  if(state.phase==0) {
    damageWait_=0x80;
    if(state.ticks>=5 && state.ticks<=14) state.feet[1]+=state.heightAdjustment*.1f;
    if(rig_->sequenceFrame()>=7) {
      if(const auto floor=probe.world().line({state.feet[0],state.previousPelvis[1],state.feet[2]},
                                           state.feet,SatSubset::All,0)) {
        state.feet[1]=floor->point[1];
        rig_->startActionFlags(jumpFallClips_[1],3,0,0x201);
        state.phase=1; damageWait_=0; action_=PlayerAction::JumpFallLanding;
      }
    }
  }
  const auto root=rig_->advanceRootAction();
  const auto c=std::cos(state.heading),s=std::sin(state.heading);
  state.feet[0]+=c*root.translation[0]+s*root.translation[2];
  state.feet[1]+=root.translation[1];
  state.feet[2]+=-s*root.translation[0]+c*root.translation[2];
  state.heading=float(std::remainder(double(state.heading)+root.yaw,6.283185307179586));
  feet_=state.feet; heading_=state.heading;
  probe.setScriptPosition(feet_);
  const auto pelvis=TransformPoint(placement(),TransformPoint(rig_->jointMatrices().at(4),{0,0,0}));
  state.previousPelvis={pelvis[0],pelvis[1],pelvis[2]};
  if(state.phase==1 && rig_->sequenceFrame()>=20) state.phase=2;
  if(state.ticks>=256) { state.phase=2; state.failed=true; }
  if(state.phase!=2) return;
  const auto landing=probe.traversalLanding(feet_);
  if(state.failed || !landing || !probe.place(*landing,5)) {
    if(!probe.place(jumpFallRollback_)) throw std::runtime_error("Long jump: Starting and destination floors blocked");
    heading_=jumpFallRollbackHeading_; state.failed=true;
  }
  feet_=probe.position(); state.feet=feet_; state.heading=heading_;
  damageWait_=0; action_=PlayerAction::Footwork;
  neutralGate_=true;
}
std::optional<FenceOffer> LeonPreview::fenceOffer(const WalkProbe &probe) const {
  if (!alive() || action_ != PlayerAction::Footwork) return {};
  return FindFence(probe.world(), probe, heading_, parts0World(), fencePaths_);
}
void LeonPreview::beginFence(WalkProbe &probe, const FenceOffer &offer) {
  const auto checked = fenceOffer(probe);
  if (!checked || checked->wall.polygon != offer.wall.polygon ||
      checked->alignment != offer.alignment || checked->landing != offer.landing ||
      checked->wide != offer.wide || !offer.adjustSides)
    throw std::runtime_error("Leon: Obstacle or landing unavailable");
  startFence(probe, offer);
}
std::optional<FenceOffer> LeonPreview::brokenWindowOffer(const WalkProbe &probe,
    const CollisionVec &alignment, float heading) const {
  if (!alive() || action_ != PlayerAction::Footwork || !probe.ready() ||
      !std::isfinite(heading)) return {};
  for (auto value : alignment) if (!std::isfinite(value)) return {};
  FenceOffer offer;
  offer.alignment = alignment;
  offer.alignment[1] = probe.position()[1]; // windowCheck keeps player's Y.
  offer.heading = heading;
  offer.wide = false;
  offer.adjustSides = false;
  auto playback = StartFencePlacement(probe.position(), heading_, offer);
  for (const auto &sample : fencePaths_[1])
    AdvanceFencePlacement(probe.world(), playback, sample.root, sample.frame);
  const auto landing = probe.traversalLanding(playback.feet);
  if (!landing) return {};
  offer.landing = *landing;
  return offer;
}
void LeonPreview::beginBrokenWindow(WalkProbe &probe, const FenceOffer &offer) {
  const auto checked = brokenWindowOffer(probe, offer.alignment, offer.heading);
  if (!checked || offer.wide || offer.adjustSides ||
      checked->alignment != offer.alignment || checked->landing != offer.landing)
    throw std::runtime_error("Leon: invalid broken window or landing");
  startFence(probe, offer);
}
void LeonPreview::startFence(const WalkProbe &probe, const FenceOffer &offer) {
  fenceRollbackFeet_ = probe.position(); fenceRollbackHeading_ = heading_;
  previousRenderTraversal_ = previousRenderFootwork_ = false;
  fencePlayback_ = StartFencePlacement(probe.position(), heading_, offer);
  rig_->setWaistYaw(0);
  rig_->startActionFlags(fenceClips_[offer.wide ? 0 : 1], 3, 0, 5);
  action_ = PlayerAction::FenceVault; state_ = Locomotion::Idle; damageWait_ = 2;
  quickTurnPending_ = quickTurning_ = firePending_ = reloadPending_ =
      knifeFirePending_ = bladeVisible_ = false;
  runPressTicks_ = 0; meleeEvent_.reset(); ladderSounds_.clear();
  drawCameraYawRemaining_ = waistYaw_ = waistTarget_ = aimRate_ = aimAcceleration_ = 0;
}
void LeonPreview::fenceTick(WalkProbe &probe, bool movementHeld) {
  damageWait_ = 2;
  const auto priorFrame = rig_->sequenceFrame();
  RootMotionStep root;
  if (fencePlayback_.adjustment) {
    rig_->advanceAction(); root = rig_->rootMotionSpeed();
  } else root = rig_->advanceRootAction();
  AdvanceFencePlacement(probe.world(), fencePlayback_, root, priorFrame);
  feet_ = fencePlayback_.feet; heading_ = fencePlayback_.heading;
  probe.setScriptPosition(feet_);
  const auto frame = rig_->sequenceFrame();
  if (frame > 27.7f && frame < 28.3f) ladderSounds_.push_back({5, 0x0e});
  if (frame > 29.7f && frame < 30.3f) ladderSounds_.push_back({5, 0x0d});
  if (!root.ended && !(rig_->status().state & 3) && !(movementHeld && frame >= 30)) return;
  // Authored root exit is checked again; input-shortened exits use their
  // current real floor rather than teleporting to the predicted complete path.
  const auto landing = probe.traversalLanding(feet_);
  if (!landing || !probe.place(*landing, 5)) {
    if (!probe.place(fenceRollbackFeet_))
      throw std::runtime_error("Obstacle: Starting and destination floors blocked");
    heading_ = fenceRollbackHeading_;
  }
  feet_ = probe.position(); action_ = PlayerAction::Footwork;
  damageWait_ = 2; neutralGate_ = true;
}
void LeonPreview::beginJump(WalkProbe &probe, const JumpDownOffer &offer,
                             const FollowCameraPose *view) {
  const auto checked=jumpOffer(probe);
  if(!checked || checked->wall.polygon!=offer.wall.polygon ||
      checked->alignment!=offer.alignment || checked->landing!=offer.landing)
    throw std::runtime_error("Leon: Jump edge or landing unavailable");
  jumpRollbackFeet_=probe.position(); jumpRollbackHeading_=heading_;
  previousRenderTraversal_ = previousRenderFootwork_ = false;
  jumpPlayback_=StartJumpPlacement(probe.position(),heading_,offer);
  const auto seed = view ? *view :
      PoseFollowCamera({}, &probe.world(), probe.position(), heading_);
  jumpView_ = {seed.eye, seed.target, 0, seed.fovDegrees};
  const auto viewC = std::cos(heading_), viewS = std::sin(heading_);
  const auto dx = seed.eye[0] - probe.position()[0];
  const auto dz = seed.eye[2] - probe.position()[2];
  jumpViewLocalEye_ = {viewC * dx - viewS * dz,
      seed.eye[1] - probe.position()[1], viewS * dx + viewC * dz};
  jumpCameraWorld_ = &probe.world(); jumpCameraActive_ = false;
  action_=PlayerAction::JumpDown; state_=Locomotion::Idle;
  rig_->setWaistYaw(0); rig_->startActionFlags(jumpClips_[0],3,0,5);
  damageWait_=0x80;
  quickTurnPending_=quickTurning_=firePending_=reloadPending_=knifeFirePending_=false;
  runPressTicks_=0; bladeVisible_=false; meleeEvent_.reset();
  drawCameraYawRemaining_=waistYaw_=waistTarget_=aimRate_=aimAcceleration_=0;
}
void LeonPreview::jumpTick(WalkProbe &probe) {
  damageWait_=0x80;
  const auto &world=probe.world();
  auto oldPelvis=TransformPoint(placement(),TransformPoint(rig_->jointMatrices().at(4),{0,0,0}));
  if(jumpPlayback_.phase==0 && rig_->sequenceFrame()<=10) {
    // Original +/-400 root-part ledge checks move 40 units away from a
    // missing side. Apply that external change to MotBase's target as well.
    const auto root=parts0World(); const auto h=jumpPlayback_.targetHeading;
    for(double side : {-1.,1.}) {
      const CollisionVec d{side*std::cos(h),0,-side*std::sin(h)};
      auto from=root,to=root;
      for(unsigned k=0;k<3;++k) { from[k]+=d[k]*400; to[k]=from[k]; }
      to[0]+=std::sin(h)*1000; to[2]+=std::cos(h)*1000;
      const auto hit=world.line(from,to,SatSubset::All,0);
      if(!hit || !(hit->attributes&0x100000))
        for(unsigned k=0;k<3;++k) { jumpPlayback_.feet[k]-=d[k]*40; jumpPlayback_.base[k]-=d[k]*40; }
    }
  }
  RootMotionStep root;
  if(jumpPlayback_.adjustment) { rig_->advanceAction(); root=rig_->rootMotionSpeed(); }
  else root=rig_->advanceRootAction();
  AdvanceJumpPlacement(jumpPlayback_,root,rig_->sequenceFrame());
  feet_=jumpPlayback_.feet; heading_=jumpPlayback_.heading;
  if(jumpPlayback_.phase==0 && rig_->sequenceFrame()>20) {
    const CollisionVec from{feet_[0],oldPelvis[1],feet_[2]};
    if(const auto floor=world.line(from,feet_,SatSubset::Ground,0)) {
      feet_[1]=floor->point[1];
      const auto snapped=world.ground(feet_,600,100000);
      if(snapped && snapped->normal[1]>=.5) {
        feet_[1]=snapped->point[1];
        rig_->startActionFlags(jumpClips_[1],0,0,5);
        const auto landing=rig_->advanceRootAction(); // Source's immediate tick.
        const auto c=std::cos(heading_),s=std::sin(heading_);
        feet_[0]+=c*landing.translation[0]+s*landing.translation[2];
        feet_[1]+=landing.translation[1];
        feet_[2]+=-s*landing.translation[0]+c*landing.translation[2];
        heading_=float(std::remainder(double(heading_)+landing.yaw,6.283185307179586));
        action_=PlayerAction::JumpLanding; jumpPlayback_.phase=1;
      }
    }
  } else if(jumpPlayback_.phase==1 && (root.ended || (rig_->status().state&3)))
    jumpPlayback_.phase=2;
  if(jumpPlayback_.ticks>=256) { jumpPlayback_.phase=2; jumpPlayback_.failed=true; }
  probe.setScriptPosition(feet_);
  if(jumpPlayback_.phase==2) {
    auto landing=feet_;
    if(const auto floor=world.ground(landing,600,100000)) landing[1]=floor->point[1];
    if(jumpPlayback_.failed || !probe.place(landing,5)) {
      if(!probe.place(jumpRollbackFeet_)) throw std::runtime_error("Jump: Starting and destination floors blocked");
      heading_=jumpRollbackHeading_; jumpPlayback_.failed=true;
    }
    feet_=probe.position(); damageWait_=0; action_=PlayerAction::Footwork;
    firePending_=reloadPending_=knifeFirePending_=false; neutralGate_=true;
  }
  jumpPlayback_.feet=feet_; jumpPlayback_.heading=heading_;
  // CameraControl::Main enters CameraLookAt when part1's world height
  // relative to actor.pos drops to500 or less. It keeps the eye and follows
  // one of the two original torso parts, selected once by the source byte RNG.
  // No animation/weapon point drives this camera outside that source condition.
  if (jumpActive() && rig_->jointMatrices().at(1)[7] <= 500) {
    if (!jumpCameraActive_) {
      jumpCameraPart_ = (Rnd() & 1) ? 1 : 2;
      // Capture the regular eye at the current falling/landing placement.
      // Holding the start-of-drop world eye would leave it on the upper roof
      // while Leon has already reached the lower floor.
      const auto c = std::cos(heading_), s = std::sin(heading_);
      jumpView_.eye = {feet_[0] + c * jumpViewLocalEye_[0] + s * jumpViewLocalEye_[2],
          feet_[1] + jumpViewLocalEye_[1],
          feet_[2] - s * jumpViewLocalEye_[0] + c * jumpViewLocalEye_[2]};
      jumpCameraActive_ = true;
    }
    const auto &part = rig_->jointMatrices().at(jumpCameraPart_);
    const auto target = TransformPoint(placement(), {part[3], part[7], part[11]});
    const CollisionVec at{target[0], target[1], target[2]};
    if (const auto hit = world.line(at, jumpView_.eye, SatSubset::All, 0x8000, 0x1c2810))
      jumpView_.eye = hit->point;
  } else jumpCameraActive_ = false;
}
void LeonPreview::beginLadder(WalkProbe &probe, const CollisionVec &alignment,
                              float heading, int level) {
  if (!alive() || action_ != PlayerAction::Footwork || !probe.ready() ||
      !std::isfinite(heading) || std::abs(heading) > 100 || (level != 9 && level != -9))
    throw std::runtime_error("Leon: Ladder action unavailable");
  for (auto value : alignment)
    if (!std::isfinite(value) || std::abs(value) > 1e8)
      throw std::runtime_error("Leon: invalid ladder entry");
  ladderRollbackFeet_ = probe.position();
  previousRenderTraversal_ = previousRenderFootwork_ = false;
  objectLadder_ = false;
  ladderRollbackHeading_ = heading_;
  ladderLandingFailed_ = false;
  ladderSounds_.clear();
  ladderPlayback_ = StartLadderMotion(*rig_, ladderClips_, level);
  action_ = level > 0 ? PlayerAction::LadderUp : PlayerAction::LadderDown;
  heading_ = std::remainder(heading, 6.2831855f);
  feet_ = alignment;
  probe.setScriptPosition(feet_);
  state_ = Locomotion::Idle;
  damageWait_ = 0x80; // PlSetLadder: invulnerable / atari off until dismount.
  quickTurnPending_ = quickTurning_ = false;
  runPressTicks_ = 0;
  firePending_ = reloadPending_ = knifeFirePending_ = false;
  bladeVisible_ = false;
  meleeEvent_.reset();
  drawCameraYawRemaining_ = waistYaw_ = waistTarget_ = aimRate_ = aimAcceleration_ = 0;
}
void LeonPreview::ladderTick(WalkProbe &probe) {
  damageWait_ = 0x80;
  if (objectLadder_ && objectLadderPlayback_.phase == 2 &&
      objectLadderPlayback_.type == 0 && objectLadderPlayback_.exitTicks >= 23) {
    // obj13's dismount counter increments before this floor correction.
    if (const auto floor = probe.world().ground(feet_, 600, 100000))
      if (feet_[1] < floor->point[1]) feet_[1] = feet_[1] * .9 + floor->point[1] * .1;
  }
  const auto root = objectLadder_
      ? AdvanceObjectLadderMotion(*rig_, objectLadderClips_, objectLadderPlayback_, ladderSounds_)
      : AdvanceLadderMotion(*rig_, ladderClips_, ladderPlayback_, ladderSounds_);
  const double c = std::cos(heading_), s = std::sin(heading_);
  feet_[0] += c * root.translation[0] + s * root.translation[2];
  feet_[1] += root.translation[1];
  feet_[2] += -s * root.translation[0] + c * root.translation[2];
  heading_ = float(std::remainder(double(heading_) + root.yaw, 6.283185307179586));
  probe.setScriptPosition(feet_);
  if (root.ended) {
    if (objectLadder_) {
      if (const auto floor = probe.world().ground(feet_, 600, 100000))
        if (feet_[1] < floor->point[1]) feet_[1] = floor->point[1];
    }
    if (!probe.place(feet_)) {
      // A changed/damaged destination must never leave the actor suspended.
      // The room still owns the original start floor/capsule checkpoint.
      if (!probe.place(ladderRollbackFeet_))
        throw std::runtime_error("Ladder: Starting and destination floors are no longer walkable");
      heading_ = ladderRollbackHeading_;
      ladderLandingFailed_ = true;
    }
    feet_ = probe.position();
    damageWait_ = 0;
    action_ = PlayerAction::Footwork;
    firePending_ = reloadPending_ = knifeFirePending_ = false;
    neutralGate_ = true;
  }
}
void LeonPreview::installObjectLadderMotions(const std::vector<AnimationBytes> &clips) {
  if (clips.size() != 4 && clips.size() != 7)
    throw std::runtime_error("Leon: Object ladder motions are missing");
  if (!objectLadderMotions_.empty()) {
    for (unsigned i = 0; i < std::min(objectLadderMotions_.size(), clips.size()); ++i)
      if (objectLadderMotions_[i].motion != clips[i].motion ||
          objectLadderMotions_[i].sequence != clips[i].sequence)
        throw std::runtime_error("Leon: Object ladder profile changed");
    if (clips.size() <= objectLadderMotions_.size()) return;
    const std::vector<AnimationBytes> extra(clips.begin() + 4, clips.end());
    const auto first = rig_->appendActions(extra);
    for (unsigned i = 0; i < 3; ++i) ladderPropClips_[i] = first + i;
  } else {
    const unsigned first = rig_->appendActions(clips);
    for (unsigned i = 0; i < 4; ++i) objectLadderClips_[i] = first + i;
    if (clips.size() == 7)
      for (unsigned i = 0; i < 3; ++i) ladderPropClips_[i] = first + 4 + i;
  }
  objectLadderMotions_ = clips;
}
void LeonPreview::beginObjectLadder(WalkProbe &probe, const CollisionVec &alignment,
                                    float heading, unsigned rungs, unsigned type,
                                    const CollisionVec &landing) {
  if (!alive() || action_ != PlayerAction::Footwork || !probe.ready() ||
      objectLadderMotions_.size() < 4 || !std::isfinite(heading) || type > 1 ||
      !((rungs == 6 && type == 0) || (rungs == 8 && type == 1)))
    throw std::runtime_error("Leon: Object ladder action unavailable");
  for (const auto &point : {alignment, landing}) for (double value : point)
    if (!std::isfinite(value) || std::abs(value) > 1e8)
      throw std::runtime_error("Leon: Invalid object ladder position");
  ladderRollbackFeet_ = probe.position(); ladderRollbackHeading_ = heading_;
  previousRenderTraversal_ = previousRenderFootwork_ = false;
  objectLadderLanding_ = landing; ladderLandingFailed_ = false;
  objectLadder_ = true; ladderSounds_.clear();
  objectLadderPlayback_ = StartObjectLadderMotion(*rig_, objectLadderClips_, rungs, type);
  action_ = PlayerAction::LadderUp; state_ = Locomotion::Idle;
  heading_ = std::remainder(heading, 6.2831855f); feet_ = alignment;
  probe.setScriptPosition(feet_); damageWait_ = 0x80;
  quickTurnPending_ = quickTurning_ = false; runPressTicks_ = 0;
  firePending_ = reloadPending_ = knifeFirePending_ = bladeVisible_ = false;
  drawCameraYawRemaining_ = waistYaw_ = waistTarget_ = aimRate_ = aimAcceleration_ = 0;
  rig_->setWaistYaw(0); meleeEvent_.reset();
}
void LeonPreview::beginLadderProp(WalkProbe &probe,
                                 const CollisionVec &alignment, float heading,
                                 unsigned motionIndex, bool flip, bool reset) {
  if (!alive() || action_ != PlayerAction::Footwork || !probe.ready() ||
      objectLadderMotions_.size() != 7 || !std::isfinite(heading) ||
      motionIndex < 4 || motionIndex > 6 ||
      (reset ? motionIndex != 4 : (motionIndex == 4 || flip)))
    throw std::runtime_error("Leon: Ladder raising/toppling unavailable");
  for (auto value : alignment)
    if (!std::isfinite(value) || std::abs(value) > 1e8)
      throw std::runtime_error("Leon: invalid ladder action position");
  ladderRollbackFeet_ = probe.position(); ladderRollbackHeading_ = heading_;
  previousRenderTraversal_ = previousRenderFootwork_ = false;
  feet_ = alignment; heading_ = std::remainder(heading, 6.2831855f);
  probe.setScriptPosition(feet_);
  ladderPropTicks_ = 0; ladderPropReset_ = reset;
  ladderPropType_ = motionIndex == 5;
  ladderPropClip_ = ladderPropClips_[motionIndex - 4];
  rig_->setWaistYaw(0);
  rig_->startActionFlags(ladderPropClip_, 5, 0, flip ? 0x41 : 1);
  action_ = reset ? PlayerAction::LadderReset : PlayerAction::LadderTip;
  state_ = Locomotion::Idle; damageWait_ = 15;
  quickTurnPending_ = quickTurning_ = firePending_ = reloadPending_ =
      knifeFirePending_ = bladeVisible_ = false;
  runPressTicks_ = 0;
  drawCameraYawRemaining_ = waistYaw_ = waistTarget_ = aimRate_ = aimAcceleration_ = 0;
  ladderSounds_.clear(); meleeEvent_.reset();
}
void LeonPreview::ladderPropTick(WalkProbe &probe) {
  damageWait_ = 15;
  const auto frame = rig_->sequenceFrame();
  if (!ladderPropReset_ && std::abs(frame - (ladderPropType_ ? 17.f : 13.f)) < .3f)
    ladderSounds_.push_back({6, ladderPropType_ ? 0x40u : 0x44u});
  const auto root = rig_->advanceRootAction();
  const auto c = std::cos(heading_), s = std::sin(heading_);
  feet_[0] += c * root.translation[0] + s * root.translation[2];
  feet_[1] += root.translation[1];
  feet_[2] += -s * root.translation[0] + c * root.translation[2];
  heading_ = float(std::remainder(double(heading_) + root.yaw, 6.283185307179586));
  probe.setScriptPosition(feet_); ++ladderPropTicks_;
  if (!root.ended) return;
  if (!probe.place(feet_)) {
    if (!probe.place(ladderRollbackFeet_))
      throw std::runtime_error("Ladder action: Starting and destination floors blocked");
    heading_ = ladderRollbackHeading_;
  }
  feet_ = probe.position(); action_ = PlayerAction::Footwork;
  damageWait_ = 30; neutralGate_ = true;
}
bool LeonPreview::beginItemExam() {
  if (!alive() || action_ != PlayerAction::Footwork) return false;
  action_ = PlayerAction::ItemExam;
  motionAccumulator_ = 0; previousRenderFootwork_ = previousRenderTraversal_ = false;
  quickTurnPending_ = quickTurning_ = false; runPressTicks_ = 0;
  firePending_ = reloadPending_ = knifeFirePending_ = false;
  meleeEvent_.reset(); return true;
}
void LeonPreview::endItemExam(bool accepted) {
  if (!itemExamActive()) return;
  // Acceptance controls the inventory transaction, not a different motion:
  // ITEM_CANCEL restores the same cut/control state as a successful pickup.
  (void)accepted;
  action_ = PlayerAction::Footwork; state_ = Locomotion::Idle;
  motionAccumulator_ = 0; previousRenderFootwork_ = previousRenderTraversal_ = false;
  firePending_ = reloadPending_ = knifeFirePending_ = false;
  neutralGate_ = true;
}
std::vector<LadderSound> LeonPreview::drainLadderSounds() {
  std::vector<LadderSound> result;
  result.swap(ladderSounds_);
  return result;
}
std::vector<PlayerSequenceSound> LeonPreview::drainSequenceSounds() {
  auto events = std::move(sequenceSounds_);
  sequenceSounds_.clear();
  return events;
}
std::vector<PlayerWeaponSound> LeonPreview::drainWeaponSounds() {
  std::vector<PlayerWeaponSound> result;result.swap(weaponSounds_);return result;
}
void LeonPreview::emitWeaponSound(unsigned block,unsigned number,unsigned part) {
  const auto &m=rig_->jointMatrices().at(part);
  const auto position=TransformPoint(placement(),{m[3],m[7],m[11]});
  weaponSounds_.push_back({block,number,part,{position[0],position[1],position[2]},nativeTicks_,rig_->sequenceFrame()});
}
void LeonPreview::emitSequenceSound() {
  const auto source = rig_->consumeSequenceSound();
  if (!source) return;
  const unsigned number = source->se - 1;
  unsigned part = 0;
  switch (number) {
  case 0: case 2: case 0xd: part = 0x15; break;
  case 1: case 3: case 0xe: case 0x14: part = 0x19; break;
  default: break;
  }
  const auto root = parts0World();
  const auto &matrix = rig_->jointMatrices().at(part);
  const auto point = TransformPoint(placement(), {matrix[3], matrix[7], matrix[11]});
  sequenceSounds_.push_back({number, unsigned(source->free & 7), part,
      {point[0], point[1], point[2]}, root});
}
void LeonPreview::beginDoor(bool kick, bool blocked,
                            const CollisionVec &alignment, float heading,
                            unsigned frame) {
  if (!alive() || (action_ != PlayerAction::Footwork &&
                   action_ != PlayerAction::DoorOpen) || frame > 6 ||
      !std::isfinite(heading) || std::abs(heading) > 100 || (blocked && !kick))
    throw std::runtime_error("Leon: Door action unavailable");
  for (auto value : alignment)
    if (!std::isfinite(value) || std::abs(value) > 1e7)
      throw std::runtime_error("Leon: Invalid door orientation");
  for (unsigned k = 0; k < 3; ++k)
    doorAdjustment_[k] = alignment[k] - feet_[k];
  doorAdjustment_[1] = 0;
  doorHeading_ = heading;
  doorTicks_ = 0;
  quickTurnPending_ = quickTurning_ = false;
  runPressTicks_ = 0;
  firePending_ = reloadPending_ = knifeFirePending_ = false;
  bladeVisible_ = false;
  meleeEvent_.reset();
  waistYaw_ = waistTarget_ = aimRate_ = aimAcceleration_ = 0;
  rig_->setWaistYaw(0);
  action_ = !kick ? PlayerAction::DoorOpen
                 : blocked ? PlayerAction::DoorBlockedKick : PlayerAction::DoorKick;
  state_ = Locomotion::Idle;
  rig_->startActionFlags(doorClips_[!kick ? 0 : blocked ? 2 : 1], 5, frame, 1);
  // plemDoorOpen sets dmg wait 0x26 and turns off object-body collision.
  // The room owner excludes only this door from the dynamic body adapter;
  // static room walls remain active throughout the animation.
  damageWait_ = 0x26;
}
void LeonPreview::doorTick(WalkProbe &probe) {
  if (action_ == PlayerAction::DoorOpen) {
    constexpr float pi = 3.1415927f;
    const auto turn = std::remainder(doorHeading_ - heading_, 2 * pi);
    heading_ = std::remainder(heading_ + std::clamp(turn, -pi / 16, pi / 16), 2 * pi);
    CollisionVec alignment{};
    for (unsigned k = 0; k < 3; ++k) {
      alignment[k] = doorAdjustment_[k] * .1;
      doorAdjustment_[k] -= alignment[k];
    }
    probe.moveDelta(alignment);
  }
  const auto root = rig_->advanceRootAction();
  const auto c = std::cos(heading_), s = std::sin(heading_);
  probe.moveDelta({c * root.translation[0] + s * root.translation[2],
                   root.translation[1],
                   -s * root.translation[0] + c * root.translation[2]});
  heading_ = std::remainder(heading_ + float(root.yaw), 6.2831855f);
  feet_ = probe.position();
  ++doorTicks_;
  if (root.ended) {
    action_ = PlayerAction::Footwork;
    damageWait_ = 10;
    neutralGate_ = true;
    // Keep the last authored pose for blending into the next idle/walk tick.
  }
}
void LeonPreview::installWindowMotions(const std::vector<AnimationBytes> &clips) {
  if (clips.size() != 3) throw std::runtime_error("Leon: Window motion count");
  if (!windowMotions_.empty()) {
    for (unsigned i=0; i<3; ++i)
      if (windowMotions_[i].motion != clips[i].motion ||
          windowMotions_[i].sequence != clips[i].sequence)
        throw std::runtime_error("Leon: Unverified window motion change");
    return;
  }
  windowClipBase_ = rig_->appendActions(clips);
  windowMotions_ = clips;
}
void LeonPreview::beginWindow(WalkProbe &probe, const InteriorWindowOffer &offer) {
  if (!alive() || action_ != PlayerAction::Footwork || windowMotions_.size()!=3 ||
      offer.direction>2 || !probe.ready())
    throw std::runtime_error("Leon: Window action unavailable");
  for (const auto &v : {offer.start, offer.landing}) for (auto x:v)
    if (!std::isfinite(x)) throw std::runtime_error("Leon: Window destination");
  if (!std::isfinite(offer.heading)) throw std::runtime_error("Leon: Window direction");
  windowLanding_=offer.landing; windowSlot_=offer.slot;
  windowDirection_=offer.direction; windowTicks_=0;
  previousRenderTraversal_ = previousRenderFootwork_ = false;
  heading_=offer.heading; feet_=offer.start;
  windowStart_=offer.start; windowStartHeading_=offer.heading;
  probe.setScriptPosition(feet_); action_=PlayerAction::WindowVault;
  state_=Locomotion::Idle; quickTurnPending_=quickTurning_=false;
  firePending_=reloadPending_=knifeFirePending_=bladeVisible_=false;
  waistYaw_=waistTarget_=aimRate_=aimAcceleration_=0; rig_->setWaistYaw(0);
  // ExeWindowEvent: FCV + MotionSetCore(..., flags=0x201, frame=0).
  // Root placement is applied once by this native world owner.
  rig_->enableAttachedCamera(true);
  rig_->startActionFlags(windowClipBase_+windowDirection_,0,0,0x201);
}
void LeonPreview::windowTick(WalkProbe &probe) {
  const auto root=rig_->advanceRootAction();
  const auto c=std::cos(heading_),s=std::sin(heading_);
  feet_[0]+=c*root.translation[0]+s*root.translation[2];
  feet_[1]+=root.translation[1];
  feet_[2]+=-s*root.translation[0]+c*root.translation[2];
  heading_=float(std::remainder(double(heading_)+root.yaw,6.283185307179586));
  probe.setScriptPosition(feet_); ++windowTicks_;
  if (!root.ended) return;
  if (!probe.place(windowLanding_,5)) throw std::runtime_error("Window: Landing floor is no longer clear");
  feet_=probe.position(); action_=PlayerAction::Footwork;
  rig_->enableAttachedCamera(false);
  damageWait_=10; neutralGate_=true;
}
std::optional<WindowCameraPose> LeonPreview::windowCamera() const {
  if (!windowActive() || !windowTicks_) return {};
  auto attached = rig_->attachedCamera();
  if (!attached.type || !attached.hasEye || !attached.hasTarget) return {};
  // MotionSetCore registers channel slots before their first MotionMove.
  // That unsampled camera contains zero values, not a preceding FCV pose.
  if (windowTicks_ > 1 && renderInterpolation_ && previousRenderTraversal_ &&
      previousRenderAttached_.type == attached.type &&
      previousRenderAttached_.hasEye && previousRenderAttached_.hasTarget) {
    const auto alpha = std::clamp(motionAccumulator_, 0., 1.);
    for (unsigned k = 0; k < 3; ++k) {
      attached.eye[k] = float(previousRenderAttached_.eye[k] +
          alpha * (double(attached.eye[k]) - previousRenderAttached_.eye[k]));
      attached.target[k] = float(previousRenderAttached_.target[k] +
          alpha * (double(attached.target[k]) - previousRenderAttached_.target[k]));
    }
    if (attached.hasRoll && previousRenderAttached_.hasRoll)
      attached.roll = float(previousRenderAttached_.roll +
          alpha * std::remainder(double(attached.roll) - previousRenderAttached_.roll,
                                 6.283185307179586));
    if (attached.hasFov && previousRenderAttached_.hasFov)
      attached.fovRadians = float(previousRenderAttached_.fovRadians +
          alpha * (double(attached.fovRadians) - previousRenderAttached_.fovRadians));
  }
  // CameraAttachedToMotion::move always applies ac->p_mat. Type1 tracks
  // the current actor matrix, type2 captures the starting actor matrix.
  // 0x200 skips the normal camera-wall fix; it does not skip that transform.
  const auto origin = attached.type == 1 ? renderFeet() : windowStart_;
  const auto heading = attached.type == 1 ? renderHeading() : windowStartHeading_;
  const double c = std::cos(heading), s = std::sin(heading);
  auto point = [&](const std::array<float, 3> &p) -> CollisionVec {
    return {origin[0] + c * p[0] + s * p[2], origin[1] + p[1],
            origin[2] - s * p[0] + c * p[2]};
  };
  WindowCameraPose result;
  result.eye = point(attached.eye); result.target = point(attached.target);
  if (attached.hasRoll) result.roll = attached.roll;
  if (attached.hasFov) result.fovDegrees = attached.fovRadians * 180. / 3.14159265358979323846;
  if (!std::isfinite(result.fovDegrees) || result.fovDegrees < 1 || result.fovDegrees > 179 ||
      !std::isfinite(result.roll)) throw std::runtime_error("Window: Invalid original camera");
  return result;
}
std::optional<ActionCameraPose> LeonPreview::actionCamera() const {
  if(dogTrapActive()) {
    if(action_==PlayerAction::BearTrapBite && dogTrapTicks_>=120)return {};
    auto matrix=renderPlacement();ActionCameraPose camera;camera.fovDegrees=55;
    if(action_==PlayerAction::BearTrapBite)
      for(unsigned k=0;k<3;++k)matrix[k*4+3]=float(trapCameraOrigin_[k]);
    const auto eye=TransformPoint(matrix,action_==PlayerAction::DogRescue
        ?std::array<float,3>{2500,2000,0}:std::array<float,3>{-1200,1300,-1000});
    const auto target=TransformPoint(matrix,action_==PlayerAction::DogRescue
        ?std::array<float,3>{0,1000,1000}:std::array<float,3>{0,500,-300});
    camera.eye={eye[0],eye[1],eye[2]};camera.target={target[0],target[1],target[2]};
    return camera;
  }
  if (objectLadderActive()) {
    // objLadderClimbCamMove uses animated part0, not actor.pos. The ordinary
    // shoulder profile placed the eye above a climber instead of following it.
    const auto matrix = WeaponAttachmentMatrix(renderJointMatrices().at(0),
                                                renderPlacement());
    return PoseObjectLadderCamera(matrix, ObjectLadderCamera::Climb);
  }
  if (ladderPropActive())
    return PoseObjectLadderCamera(renderPlacement(), ladderPropReset_
        ? ObjectLadderCamera::Reset : ObjectLadderCamera::Tip);
  if (jumpActive() && jumpCameraActive_) {
    auto camera = jumpView_;
    const auto part = WeaponAttachmentMatrix(
        renderJointMatrices().at(jumpCameraPart_), renderPlacement());
    camera.target = {part[3], part[7], part[11]};
    if (jumpCameraWorld_)
      if (const auto hit = jumpCameraWorld_->line(camera.target, camera.eye,
          SatSubset::All, 0x8000, 0x1c2810)) camera.eye = hit->point;
    return camera;
  }
  return windowCamera();
}
void LeonPreview::installDogTrapMotions(const std::vector<AnimationBytes> &clips) {
  if(clips.size()!=2 || dogTrapActive())throw std::runtime_error("Leon: Dog/trap motion profile");
  if(dogTrapMotions_)return;
  const auto base=rig_->appendActions(clips);dogTrapClips_={base,base+1};dogTrapMotions_=true;
}
void LeonPreview::beginDogRescue(WalkProbe &probe,const CollisionVec &alignment,float heading) {
  if(!dogTrapMotions_ || !alive() || dogTrapActive())throw std::runtime_error("Leon: Dog rescue unavailable");
  probe.setScriptPosition(alignment);setPlacement(alignment,heading);
  action_=PlayerAction::DogRescue;state_=Locomotion::Idle;damageWait_=2;dogTrapTicks_=0;
  quickTurnPending_=quickTurning_=false;runPressTicks_=0;
  firePending_=reloadPending_=knifeFirePending_=bladeVisible_=false;meleeEvent_.reset();
  waistYaw_=waistTarget_=aimRate_=aimAcceleration_=0;rig_->setWaistYaw(0);
  rig_->startActionFlags(dogTrapClips_[0],0,0,1);
}
bool LeonPreview::beginBearTrapBite(WalkProbe &probe,const CollisionVec &trap) {
  feet_=probe.position();
  if(!dogTrapMotions_ || !alive() || damageWait_ || dogTrapActive() ||
      grabbed() || blastActive() || hurtActive() || traversalActive() || itemExamActive())return false;
  action_=PlayerAction::BearTrapBite;state_=Locomotion::Idle;dogTrapTicks_=0;damageWait_=2;
  trapCameraOrigin_=trap;
  const float toward=std::atan2(float(trap[0]-feet_[0]),float(trap[2]-feet_[2]));
  trapCatchTurn_=GameCubeLimitAngle(toward-heading_);
  quickTurnPending_=quickTurning_=false;runPressTicks_=0;
  firePending_=reloadPending_=knifeFirePending_=bladeVisible_=false;meleeEvent_.reset();
  waistYaw_=waistTarget_=aimRate_=aimAcceleration_=0;rig_->setWaistYaw(0);
  rig_->startActionFlags(dogTrapClips_[1],5,0,1);takeDamage(300,true);
  return true;
}
void LeonPreview::setBearTrapCameraOrigin(const CollisionVec &trap) {
  if(action_!=PlayerAction::BearTrapBite)return;
  for(double value:trap)if(!std::isfinite(value) || std::abs(value)>10000000)
    throw std::runtime_error("Leon: Invalid trap camera origin");
  trapCameraOrigin_=trap;
}
void LeonPreview::dogTrapTick(WalkProbe &probe) {
  const bool bite=action_==PlayerAction::BearTrapBite;
  ++dogTrapTicks_;damageWait_=2;
  // Original EmCatchMotionMove turns the leg-caught player by 30% of the
  // remaining catch angle, while type0 leaves his catch translation zero.
  const float previousHeading=heading_;
  if(bite)heading_=GameCubeLimitAngle(heading_+trapCatchTurn_);
  const auto step=rig_->advanceRootAction();
  const auto matrix=placement();CollisionVec position=feet_;
  for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)position[r]+=matrix[r*4+c]*float(step.translation[c]);
  if(bite) {
    const float d=trapCatchTurn_*.3f;
    heading_=GameCubeLimitAngle(previousHeading+d);trapCatchTurn_-=d;
  }
  feet_=position;probe.setScriptPosition(feet_);
  if(step.ended) {
    const auto end=probe.traversalLanding(feet_);
    if(end) {probe.setScriptPosition(*end);feet_=*end;}
    action_=PlayerAction::Footwork;damageWait_=30;neutralGate_=true;
    rig_->startAction(activeClip(0),6,0,true);
  }
}
unsigned LeonPreview::installRoomScriptMotion(const AnimationBytes &clip) {
  if(roomScriptActive())throw std::runtime_error("Leon: Room motion is already playing");
  return rig_->appendActions({clip});
}
void LeonPreview::beginRoomScriptMotion(WalkProbe &probe,const CollisionVec &alignment,
    float heading,unsigned clip,unsigned blendFrames,unsigned flags) {
  if(!alive() || clip>=rig_->clipCount() || blendFrames>255 || flags>65535)
    throw std::runtime_error("Leon: Original room motion unavailable");
  probe.setScriptPosition(alignment);setPlacement(alignment,heading);
  action_=PlayerAction::RoomScript;state_=Locomotion::Idle;
  quickTurnPending_=quickTurning_=false;runPressTicks_=0;
  firePending_=reloadPending_=knifeFirePending_=bladeVisible_=false;meleeEvent_.reset();
  waistYaw_=waistTarget_=aimRate_=aimAcceleration_=0;rig_->setWaistYaw(0);
  rig_->startActionFlags(clip,blendFrames,0,std::uint16_t(flags));
  // Commit an unblended scenario pose before rendering the handoff frame.
  // R100Event_Truck's ScreenStop must not reveal the preceding idle pose.
  if(!blendFrames)rig_->advanceAction(0);
}
void LeonPreview::roomScriptTick(WalkProbe &probe) {
  const auto step=rig_->advanceRootAction();const auto matrix=placement();
  for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)feet_[r]+=matrix[r*4+c]*float(step.translation[c]);
  heading_=GameCubeLimitAngle(heading_+float(step.yaw));probe.setScriptPosition(feet_);
}
void LeonPreview::endRoomScriptMotion() {
  if(!roomScriptActive())return;
  action_=PlayerAction::Footwork;neutralGate_=true;rig_->startAction(activeClip(0),6,0,true);
}
void LeonPreview::setRoomScriptLeftHand(unsigned type) {
  if(type==0x63)type=leftHandOld_;
  // K17's additional weapon profiles keep their loaded left grip at work
  // 900+weaponProfile_; type0 selects it. Radio frame22 temporarily selects
  // the PL00 grip (type2), and 0x63 must restore that weapon-owned grip too.
  // Both meshes already exist, so this changes visibility without a GPU upload.
  if((type!=0 && type!=2 && type!=4 && type!=5) || (type==0 && weaponProfile_<=2))
    throw std::runtime_error("Leon: room-script left hand is unavailable");
  const auto previous=leftHandNow_;leftHandNow_=type;leftHandOld_=previous;
}
void LeonPreview::setRoomScriptEyeMode(unsigned mode) {
  if(mode>1)throw std::runtime_error("Leon: Unknown original eye mode");
  eyeMode_=mode;
}
void LeonPreview::eyeTick() {
  if(!alive())return;
  if(eyeMode_==1) {
    rig_->setPartRotation(0x21,rig_->partRotation(0x20));
    return;
  }
  WeaponRandomOwner random(weaponRandom_);
  auto lid=rig_->partRotation(0x1c);
  const auto angles=OriginalPlayerEyeUpdate(eyeState_,[]{return unsigned(Rnd());},lid[0]);
  lid[0]=angles.lidPitch;
  rig_->setPartRotation(0x1c,lid);
  for(unsigned part:{0x20u,0x21u}) {
    auto eye=rig_->partRotation(part);eye[1]=angles.eyeYaw;
    rig_->setPartRotation(part,eye);
  }
}
void LeonPreview::beginPush(float heading) {
  if (!alive() || action_ != PlayerAction::Footwork || !std::isfinite(heading))
    throw std::runtime_error("Leon: Furniture action unavailable");
  pushHeading_ = std::remainder(heading, 6.2831855f);
  quickTurnPending_ = quickTurning_ = false;
  runPressTicks_ = 0;
  firePending_ = reloadPending_ = knifeFirePending_ = false;
  bladeVisible_ = false; meleeEvent_.reset();
  waistYaw_ = waistTarget_ = aimRate_ = aimAcceleration_ = 0;
  rig_->setWaistYaw(0);
  action_ = PlayerAction::PushTakeHold;
  state_ = Locomotion::Idle;
  rig_->startActionFlags(pushClips_[0], 6, 0, 5);
}
void LeonPreview::stopPush() {
  if (action_ != PlayerAction::Push) return;
  action_ = PlayerAction::PushStopped;
  rig_->startActionFlags(pushClips_[2], 3, 0, 5);
}
void LeonPreview::pushTick(WalkProbe &probe, bool forward) {
  if (!forward && action_ == PlayerAction::PushTakeHold) {
    // pl_R1_ObjPush case0x32 aborts a catch before the push loop without
    // playing the release of an object that was never actually pushed.
    action_ = PlayerAction::Footwork;
    state_ = Locomotion::Idle;
    feet_ = probe.position();
    neutralGate_ = true;
    rig_->startActionFlags(0, 15, 0, 5);
    return;
  }
  if (!forward && action_ != PlayerAction::PushRelease) {
    action_ = PlayerAction::PushRelease;
    rig_->startActionFlags(pushClips_[3], 5, 0, 5);
  }
  if (action_ == PlayerAction::PushTakeHold) {
    constexpr float pi = 3.1415927f;
    heading_ = std::remainder(heading_ + std::clamp(std::remainder(pushHeading_ - heading_, 2*pi), -pi/12, pi/12), 2*pi);
  }
  const auto root = rig_->advanceRootAction();
  const auto c = std::cos(heading_), s = std::sin(heading_);
  probe.moveDelta({c*root.translation[0]+s*root.translation[2], root.translation[1],
                   -s*root.translation[0]+c*root.translation[2]});
  feet_ = probe.position();
  if (!root.ended && !(rig_->status().state & 3)) return;
  if (action_ == PlayerAction::PushTakeHold) {
    action_ = PlayerAction::Push;
    heading_ = pushHeading_;
    rig_->startActionFlags(pushClips_[1], 0, 0, 5);
  } else if (action_ == PlayerAction::PushRelease) {
    action_ = PlayerAction::Footwork;
    neutralGate_ = true;
  }
}
void LeonPreview::beginKick(const CollisionVec &target) {
  // pl_R1_ObjPush owns the actor while holding a target.
  if (pushActive()) throw std::runtime_error("Leon: Kick during furniture action");
  if (!hasMeleeMotions() || !alive() || grabbed() || ladderActive() || ladderPropActive() || fenceActive() || jumpActive() || windowActive() || itemExamActive() ||
      action_ == PlayerAction::Kick)
    throw std::runtime_error("Leon: Kick unavailable");
  for (auto value : target)
    if (!std::isfinite(value))
      throw std::runtime_error("Leon: Kick target");
  kickTarget_ = target;
  kickTicks_ = 0;
  ++meleeSerial_;
  meleeEvent_.reset();
  bladeVisible_ = knifeFinishing_ = knifeToGun_ = knifeFirePending_ = false;
  quickTurnPending_ = quickTurning_ = false;
  runPressTicks_ = 0;
  drawCameraYawRemaining_ = 0;
  firePending_ = reloadPending_ = reloadCommitted_ = false;
  returningFromReload_ = false;
  action_ = PlayerAction::Kick;
  state_ = Locomotion::Idle;
  waistYaw_ = waistTarget_ = aimRate_ = aimAcceleration_ = 0;
  rig_->setWaistYaw(0);
  rig_->startActionFlags(kickClip, 6, 0, 1);
  weaponFrame_ = 0;
  weaponNewFrame_ = false;
  weaponEmptyVariant_ = magazine_ == 0;
  weapon_->sample(WeaponMotion::Idle, 0, weaponEmptyVariant_);
  damageWait_ = 30;
  // plem10Kick chooses one of four cut-in cameras. The camera adapter does
  // not install these cuts yet; its original random consumption still holds.
  (void)(Rnd() & 3);
}
bool LeonPreview::meleeTick(WalkProbe &probe, const PlayerActionInput &sourceInput,
                            double turn, double forward) {
  auto input=sourceInput;if(!weaponAttackImplemented())input.aimHeld=false;
  if (!hasMeleeMotions())
    return false;
  const bool moving = GameCubeMappedDirections(turn, forward) != 0;
  if (action_ == PlayerAction::Footwork && input.knifeHeld && !quickTurning_) {
    beginAction(PlayerAction::KnifeDraw);
    // knife_r3_ready00 calls the source camera pitch getter, which returns
    // zero. A lowered exploration camera must not lower the drawn knife.
  }
  if (action_ == PlayerAction::Kick) {
    damageWait_ = 30;
    ++kickTicks_;
    // plem10Kick falls through into its first routine tick immediately.
    // Its ten Muku calls use the current target, before the hit and MotionMove.
    if (kickTicks_ <= 10) {
      const float dx = float(kickTarget_[0]) - float(feet_[0]);
      const float dz = float(kickTarget_[2]) - float(feet_[2]);
      const float desired = std::atan2(dx, dz);
      const float difference =
          float(std::remainder(double(desired - heading_), 6.283185307179586));
      heading_ = float(std::remainder(
          double(heading_ + std::clamp(difference, -.19634955f, .19634955f)),
          6.283185307179586));
    }
    if (kickTicks_ == 17) {
      PlayerMeleeEvent event;
      event.kind = PlayerMeleeKind::Kick;
      event.serial = meleeSerial_;
      event.flag = 0xA;
      event.radius = 1200;
      const auto matrix = placement();
      for (unsigned i = 0; i < 2; ++i) {
        const auto center =
            TransformPoint(matrix, {0, i ? 1000.f : 1500.f, 300});
        for (unsigned k = 0; k < 3; ++k)
          event.centers[i][k] = center[k];
      }
      meleeEvent_ = std::move(event);
    }
    const auto root = rig_->advanceRootAction();
    const float c = std::cos(heading_), s = std::sin(heading_);
    probe.moveDelta({c * root.translation[0] + s * root.translation[2],
                     root.translation[1],
                     -s * root.translation[0] + c * root.translation[2]});
    heading_ =
        float(std::remainder(double(heading_) + root.yaw, 6.283185307179586));
    feet_ = probe.position();
    if (root.ended || (kickTicks_ >= 34 && (input.aimHeld || moving))) {
      beginAction(PlayerAction::Footwork);
      damageWait_ = 15;
    }
    return true;
  }
  if (!knifeActive())
    return false;
  firePending_ = reloadPending_ = false;
  if (action_ == PlayerAction::KnifeDraw) {
    if (!input.knifeHeld) {
      bladeVisible_ = knifeFirePending_ = false;
      beginAction(PlayerAction::Footwork);
      rig_->tick(Locomotion::Idle);
      return true;
    }
    if (rig_->crossedFrame(4))
      bladeVisible_ = true;
    // Set40 finishes this draw while allowing fire / directional cancellation.
    const bool reachedReady = rig_->sequenceFrame() >= 10;
    rig_->advanceAction();
    if (reachedReady) {
      action_ = PlayerAction::KnifeReady;
      knifeFinishing_ = true;
    }
    return true;
  }
  if (action_ == PlayerAction::KnifeHolster) {
    const float change = -waistYaw_ * .4f;
    waistYaw_ += change;
    heading_ =
        float(std::remainder(double(heading_ - change), 6.283185307179586));
    waistTarget_ = 0;
    rig_->setWaistYaw(waistYaw_);
    if (rig_->crossedFrame(4))
      bladeVisible_ = false;
    const auto status = rig_->advanceAction();
    if (status.ended) {
      const bool gun = knifeToGun_ && input.aimHeld;
      bladeVisible_ = false;
      beginAction(gun ? PlayerAction::Aim : PlayerAction::Footwork);
    } else if (moving || knifeToGun_ != input.aimHeld) {
      bladeVisible_ = false;
      beginAction(PlayerAction::Footwork);
    }
    return true;
  }
  const bool slashing = action_ == PlayerAction::KnifeSlash;
  if(slashing && rig_->crossedFrame(3))emitWeaponSound(1,3,4);
  if (!knifeFinishing_)
    rig_->setBlend3Rate(aimRate_);
  const auto status = rig_->advanceAction();
  if (slashing) {
    // Original motionMove solves parts before hitCheck, then the lock/aim
    // routine runs. Keep these ray matrices at that exact solved pose.
    knifeHitCheck();
    aimTick(input);
    if (status.sequenceFrame >=
            rig_->cycle(knifeSlashClips[0]).sequenceFrames - 2.f ||
        status.ended || (status.state & 3)) {
      action_ = PlayerAction::KnifeReady;
      knifeFinishing_ = true;
    }
    return true;
  }
  if (knifeFinishing_ && (status.ended || (status.state & 3) || moving))
    beginAction(PlayerAction::KnifeReady);
  aimTick(input);
  if (!input.knifeHeld) {
    knifeToGun_ = input.aimHeld;
    beginAction(PlayerAction::KnifeHolster);
  } else if (input.fireHeld || knifeFirePending_) {
    beginAction(PlayerAction::KnifeSlash);
    // knife_r3_fire00 consumes its first MotionMove as soon as it starts.
    rig_->advanceAction();
  }
  return true;
}
void LeonPreview::knifeHitCheck() {
  const float frame = rig_->sequenceFrame();
  if (frame < 6 || frame > 10)
    return;
  PlayerMeleeEvent event;
  event.kind = PlayerMeleeKind::Knife;
  event.serial = meleeSerial_;
  event.flag = (frame >= 7 && frame <= 8) ? 0 : 4;
  if (frame > 5.7f && frame < 6.3f)
    event.flag |= 8;
  // hitCheck uses parts 2 and 9 matrices with a 750-unit local blade. The
  // BIN stays on the player's authored parts; no hand attachment is guessed.
  const auto world = placement();
  const auto &chest = rig_->jointMatrices().at(2);
  const auto &hand = rig_->jointMatrices().at(9);
  auto point = [&](const Matrix34 &joint, const std::array<float, 3> &local) {
    const auto p = TransformPoint(WeaponAttachmentMatrix(joint, world), local);
    return CollisionVec{p[0], p[1], p[2]};
  };
  auto from = point(chest, {0, 0, 0});
  auto tip = point(hand, {-750, 0, 0});
  if (!(event.flag & 8)) {
    CollisionVec delta{};
    for (unsigned k = 0; k < 3; ++k)
      delta[k] = float((float(tip[k]) - float(previousBladeTip_[k])) * .2f);
    for (unsigned i = 0; i < 4; ++i) {
      for (unsigned k = 0; k < 3; ++k)
        previousBladeTip_[k] =
            float(float(previousBladeTip_[k]) + float(delta[k]));
      event.rays.push_back({from, previousBladeTip_});
    }
  }
  previousBladeTip_ = tip;
  event.rays.push_back({from, tip});
  from[1] = float(float(from[1]) + 200.f);
  tip[1] = float(float(tip[1]) + 400.f);
  event.rays.push_back({from, tip});
  from[1] = float(float(from[1]) - 300.f);
  tip[1] = float(float(tip[1]) - 600.f);
  event.rays.push_back({from, tip});
  meleeEvent_ = std::move(event);
}
void LeonPreview::grabTick(WalkProbe &probe) {
  const bool coupled = action_ == PlayerAction::Grabbed ||
                       (action_ == PlayerAction::GrabEscape && grabTicks_ < 14);
  if (coupled) {
    auto delta = partnerDelta_;
    delta[1] = 0;
    for (unsigned k = 0; k < 3; ++k) {
      const auto correction = k == 1 ? 0 : catchAdjustment_[k] * .2;
      delta[k] += correction;
      catchAdjustment_[k] -= correction;
    }
    probe.moveDelta(delta);
  }
  partnerDelta_ = {};
  const float rootHeading = heading_ + (coupled ? catchTurn_ : 0);
  const auto root = rig_->advanceRootAction();
  const float c = std::cos(rootHeading), s = std::sin(rootHeading);
  probe.moveDelta({c * root.translation[0] + s * root.translation[2],
                   root.translation[1],
                   -s * root.translation[0] + c * root.translation[2]});
  const float turn = coupled ? catchTurn_ * .3f : 0;
  heading_ = float(std::remainder(heading_ + turn + (coupled ? 0 : root.yaw),
                                  6.283185307179586));
  catchTurn_ -= turn;
  feet_ = probe.position();
  ++grabTicks_;
  // plem10_NeckHang's failed-resistance branch uses ARC298. ARC299 is
  // successful resistance, despite the historical source comment.
  if (!chainsawGrab_ && action_ == PlayerAction::GrabRelease && rig_->sequenceFrame() > 36.7f &&
      rig_->sequenceFrame() < 37.3f)
    takeDamage(GrabFailedReleaseDamage(0), life_ > 50);
  if (root.ended && action_ == PlayerAction::GrabRelease && alive()) {
    action_ = PlayerAction::Recover;
    rig_->startActionFlags(recoverClip, 3, 0, 5);
  } else if ((root.ended || (action_ == PlayerAction::Recover &&
                             (rig_->status().state & 3))) &&
             action_ != PlayerAction::Grabbed)
    finishGrab();
}
void LeonPreview::beginAction(PlayerAction next) {
  const bool wasAiming=aiming();
  const bool rifleReloadDown=next==PlayerAction::Holster && (action_==PlayerAction::Reload || rifleCycling_) && weapon_->number()==9;
  if(rifleCycling_ && next==PlayerAction::Aim) {
    weaponFrame_=0;weaponNewFrame_=true;weapon_->sample(WeaponMotion::Idle,0,weaponEmptyVariant_);
  }
  if(next!=PlayerAction::Fire)rifleCycling_=false;
  action_ = next;
  queuedGunAction_ = PlayerAction::Footwork;
  state_ = Locomotion::Idle;
  returningFromReload_ = false;
  rifleReloadScope_=false;
  aimBlendTicks_ = 0;
  switch (next) {
  case PlayerAction::DogRescue:
  case PlayerAction::BearTrapBite:
  case PlayerAction::RoomScript:
    throw std::runtime_error("Leon: Dog/trap action requires an explicit start");
  case PlayerAction::ItemExam:
    throw std::runtime_error("Leon: Pickup action requires beginItemExam");
  case PlayerAction::WindowVault:
    throw std::runtime_error("Leon: Window action requires beginWindow");
  case PlayerAction::JumpDown:
  case PlayerAction::JumpLanding:
  case PlayerAction::JumpFall:
  case PlayerAction::JumpFallLanding:
    throw std::runtime_error("Leon: Jump requires beginJump");
  case PlayerAction::Hurt:
  case PlayerAction::HurtDead:
    throw std::runtime_error("Leon: Hit reaction requires receiveGanadoAxeHit");
  case PlayerAction::LadderUp:
  case PlayerAction::LadderDown:
  case PlayerAction::LadderTip:
  case PlayerAction::LadderReset:
    throw std::runtime_error("Leon: Ladder requires beginLadder");
  case PlayerAction::FenceVault:
    throw std::runtime_error("Leon: Obstacle action requires beginFence");
  case PlayerAction::DoorOpen:
  case PlayerAction::DoorKick:
  case PlayerAction::DoorBlockedKick:
    throw std::runtime_error("Leon: Door motion requires beginDoor");
  case PlayerAction::BlastBlow:
  case PlayerAction::BlastStagger:
  case PlayerAction::BlastRecover:
  case PlayerAction::BlastDead:
    throw std::runtime_error("Leon: blast motions require applyBlast");
  case PlayerAction::Draw:
    burstShots_=0;
    aimRate_ = waistTarget_ = aimAcceleration_ = 0;
    weaponSway_.pitchCenter=weaponSway_.yawCenter=0;
    scopeSway_.pitch=scopeSway_.yaw=0;
    drawCameraYawRemaining_ = 0;
    rig_->startActionFlags(activeClip(drawClip), 3, 0, weapon_->shotgun() ? 5 : 4);
    break;
  case PlayerAction::Aim:
    burstShots_=0;
    if(usesScope()) {
      const auto &parts=rig_->jointMatrices();
      for(unsigned k=0;k<3;++k)scopeEyeLocal_[k]=(parts.at(32)[k*4+3]+parts.at(33)[k*4+3])*.5f;
      if(weapon_->family()==WeaponFamily::Rifle)rig_->startActionFlags(activeClip(aimClips[0]),5,0,4);
      else rig_->startBlend3(activeBlend(aimClips),aimRate_,3);
    } else rig_->startBlend3(activeBlend(aimClips), aimRate_, 3);
    break;
  case PlayerAction::Fire:
    if (!hasRounds())
      throw std::runtime_error("Weapon: Empty magazine");
    throwReleased_=false;rifleCycling_=false;rocketDiscarding_=false;
    scopeShot_.reset();
    if(usesScope() || weapon_->family()==WeaponFamily::Mine)scopeShot_=shotRay(false);
    if(weapon_->family()!=WeaponFamily::Grenade) {
      if(!unlimitedAmmo_ && !weapon_->infinite()) {
        if(weapon_->family()==WeaponFamily::Bow)--reserve_;else --magazine_;
      }
      ++shots_;
    }
    if(weapon_->family()==WeaponFamily::Matilda)++burstShots_;
    fireTicks_ = 0;
    fireRecoilTail_ = fireAimPending_ = false;
    firePending_ = false;
    waistYaw_=waistYaw_*.6f+waistTarget_*.4f;
    rig_->setWaistYaw(waistYaw_);
    rig_->startBlend3(weapon_->family()==WeaponFamily::Rifle
        ?std::array<unsigned,3>{activeClip(aimClips[0]),activeClip(aimClips[0]),activeClip(aimClips[0])}
        :weapon_->family()==WeaponFamily::Grenade && magazine_==1 && !unlimitedAmmo_
        ?std::array<unsigned,3>{activeClip(18),activeClip(19),activeClip(20)}
        :activeBlend(fireClips), usesScope()?0:aimRate_, weapon_->family()==WeaponFamily::Grenade?3:0);
    weaponFrame_ = 0;
    weaponNewFrame_ = true;
    weaponEmptyVariant_ = magazine_ == 0;
    weapon_->sample(WeaponMotion::Fire, 0, weaponEmptyVariant_);
    break;
  case PlayerAction::Reload:
    reloadPending_ = firePending_ = false;
    reloadCommitted_ = false;
    if(weapon_->family()==WeaponFamily::Rifle)aimRate_=aimAcceleration_=0;
    // Rifle's MotionSetCore is flags5, exactly as pl_rifle::reload step0.
    rig_->startActionFlags(activeClip(reloadClip), 3, 0,
        weapon_->shotgun() || weapon_->family()==WeaponFamily::Rifle ? 5 : 1);
    weaponFrame_ = 0;
    weaponNewFrame_ = true;
    weaponEmptyVariant_ = magazine_ == 0;
    weapon_->sample(WeaponMotion::Reload, 0, weaponEmptyVariant_);
    break;
  case PlayerAction::Holster: {
    const float change = -waistYaw_ * .4f;
    waistYaw_ += change;
    heading_ =
        float(std::remainder(double(heading_ - change), 6.283185307179586));
    waistTarget_ = 0;
    rig_->setWaistYaw(waistYaw_);
    // Source uses frame3/hokan5 after set/fire/reload, frame0/hokan3 for
    // immediate ready release. Root flag1 is owned by the stationary adapter.
    if(weapon_->family()==WeaponFamily::Rifle) {
      rig_->startActionFlags(activeClip(rifleReloadDown?19:holsterClip),weapon_->number()==10?5:0,0,4);
      if(rifleReloadDown) {
        weaponFrame_=0;weaponNewFrame_=true;
        weapon_->sample(WeaponMotion::RifleReloadDown,0,weaponEmptyVariant_);
      }
    } else rig_->startActionFlags(activeClip(holsterClip), weapon_->family()==WeaponFamily::Handgun ? 5 : 3,
                           weapon_->family()==WeaponFamily::Handgun ? 3 : 0, 5);
    firePending_ = reloadPending_ = false;
    break;
  }
  case PlayerAction::Footwork:
    // Original down applies only one waist/body-yaw compensation. The
    // residual waist persists until the next draw or run entry.
    rig_->setWaistYaw(waistYaw_);
    waistTarget_ = aimRate_ = aimAcceleration_ = 0;
    drawCameraYawRemaining_ = 0;
    break;
  case PlayerAction::KnifeDraw:
    firePending_ = reloadPending_ = false;
    quickTurnPending_ = quickTurning_ = false;
    runPressTicks_ = 0;
    drawCameraYawRemaining_ = 0;
    knifeFinishing_ = knifeToGun_ = false;
    bladeVisible_ = false;
    aimAcceleration_ = waistTarget_ = aimRate_ = 0;
    weaponSway_.pitchCenter=weaponSway_.yawCenter=0;
    {
    const auto draw=weapon_->family()==WeaponFamily::Rocket && weapon_->infinite()?activeClip(19):
        weapon_->family()==WeaponFamily::Rocket && wasAiming && hasRounds()?activeClip(18):knifeDrawClip;
    rig_->startBlend3({draw, draw, draw}, aimRate_,
                      3);
    }
    weaponFrame_ = 0;
    weaponNewFrame_ = false;
    weaponEmptyVariant_ = magazine_ == 0;
    weapon_->sample(WeaponMotion::Idle, 0, weaponEmptyVariant_);
    break;
  case PlayerAction::KnifeReady:
    knifeFinishing_ = false;
    rig_->startBlend3(knifeReadyClips, aimRate_, 3);
    break;
  case PlayerAction::KnifeSlash:
    knifeFinishing_ = false;
    knifeFirePending_ = false;
    ++meleeSerial_;
    previousBladeTip_ = {};
    rig_->startBlend3(knifeSlashClips, aimRate_, 3);
    break;
  case PlayerAction::KnifeHolster:
    firePending_ = reloadPending_ = knifeFirePending_ = false;
    knifeFinishing_ = false;
    rig_->startActionFlags(knifeToGun_ ? activeClip(16) :
        (life_ >= 800 || equippedWeaponId()==0x6d ? activeClip(17) : knifeHolsterClip), 5,
                           0, 0);
    break;
  case PlayerAction::Kick:
    throw std::runtime_error("Leon: Kick requires a verified target");
  case PlayerAction::PushTakeHold:
  case PlayerAction::Push:
  case PlayerAction::PushStopped:
  case PlayerAction::PushRelease:
    throw std::runtime_error("Leon: Furniture action requires a verified destination");
  case PlayerAction::Grabbed:
  case PlayerAction::GrabRelease:
  case PlayerAction::GrabEscape:
  case PlayerAction::Recover:
  case PlayerAction::Dead:
    throw std::runtime_error("Leon: Grab has a separate action chain");
  }
}
void LeonPreview::aimTick(const PlayerActionInput &input) {
  // Each original action tick is 1/30 s. Fine aiming is independent of the
  // accelerating stick curve, and scope magnification reduces its gain.
  if(action_==PlayerAction::Aim && input.aimHeld && !knifeActive()) {
    const float gain=.35f*(usesScope()?1.f-.9f*scopeZoom_:1.f)/30.f;
    if(std::isfinite(input.gyroYaw))heading_=std::remainder(heading_+float(std::clamp(input.gyroYaw,-3.,3.))*gain,6.283185307f);
    if(std::isfinite(input.gyroPitch))aimRate_=std::clamp(aimRate_+float(std::clamp(input.gyroPitch,-3.,3.))*gain/.7853982f,-1.f,1.f);
  }
  if(usesScope() && !knifeActive()) {
    // CameraScope::move, with the port's mapped right stick for aiming
    // and D-pad up/down replacing the GameCube C-stick zoom control.
    scopeZoom_=std::clamp(scopeZoom_+float(input.scopeZoom)*.1f,0.f,1.f);
    const float gain=1.f-.9f*scopeZoom_;
    heading_=std::remainder(heading_+float(input.yaw)*gain*.08726646f,6.283185307f);
    aimRate_=std::clamp(aimRate_+float(input.pitch)*gain*(5.f/70.f),-1.f,1.f);
    waistTarget_=waistYaw_=0;rig_->setWaistYaw(0);return;
  }
  if (input.yaw != 0 || input.pitch != 0)
    aimAcceleration_ = std::min(7.f, aimAcceleration_ + 1);
  else
    aimAcceleration_ = 0;
  // Original clamped main stick's axial range72; modern RS owns aim instead.
  float x = float(input.yaw) * 72, y = float(input.pitch) * 72;
  // Preserve the original octagonal gate (axial72, diagonal40).
  const float gate = 40 * std::max(std::abs(x), std::abs(y)) +
                     32 * std::min(std::abs(x), std::abs(y));
  if (gate > 2880) {
    float scale = 2880 / gate;
    x *= scale;
    y *= scale;
  }
  // PlWepLockCtrl's weapon7 profile: six-degree waist limit and 0.8x
  // pitch/body-turn speed. The local waist increment itself is unscaled.
  const auto number=weapon_->number();
  const float speed=number==4?1.5f:number==7?.8f:number==8?.9f:number==33?1.04f:
      number==11 || number==12?1.15f:number==22 || number==23?1.2f:1.f;
  float pitchDelta = speed * y * aimAcceleration_ * .15f / 200 / 10;
  if (aimRate_ > 0)
    pitchDelta *= .8f;
  aimRate_ = std::clamp(aimRate_ + pitchDelta, -1.f, 1.f);
  // InputMapper already maps physical RS-right to negative X. The aim camera
  // looks along the posed hand with right=(-dir.z,0,dir.x), so negative world
  // yaw moves the muzzle screen-right. Adapt the original physical-stick
  // formula to this mapped axis without reversing the working camera input.
  const float yawDelta = x * aimAcceleration_ * 3.14159265358979323846f / 40000;
  waistTarget_ += yawDelta;
  const float limit=number==4?.062831853f:number==7 || number==33?.10471976f:
      number==8?.052359879f:number==11 || number==12 || number==22 || number==23?.0065449847f:.20943952f;
  if (waistTarget_ > limit || waistTarget_ < -limit * .8f) {
    waistTarget_ = std::clamp(waistTarget_, -limit * .8f, limit);
    heading_ =
        float(std::remainder(double(heading_ + (number==4?1.2f:number==22 || number==23?1.5f:speed) * yawDelta), 6.283185307179586));
  }
  weaponSway((pitchDelta!=0 || yawDelta!=0 || input.gyroYaw!=0 || input.gyroPitch!=0)?1u:0u);
  waistYaw_ = waistYaw_ * .6f + waistTarget_ * .4f;
  rig_->setWaistYaw(waistYaw_);
}
void LeonPreview::weaponSway(unsigned flags) {
  WeaponRandomOwner random(weaponRandom_);
  const float pitch=(flags&1)?0:fRand1_1();
  const float yaw=(flags&1)?0:fRand1_1();
  OriginalWeaponSway(weaponSway_,OriginalDefaultWeaponSwayAbility(),flags,pitch,yaw,aimRate_,waistTarget_);
}
bool LeonPreview::actionTick(const PlayerActionInput &input, bool movementHeld) {
  // A gun action finishes through its existing holster before the knife draw.
  // This keeps recoil/reload commits and the original waist compensation.
  auto gunInput = input;
  if (hasMeleeMotions() && gunInput.knifeHeld)
    gunInput.aimHeld = false;
  if(!weaponAttackImplemented() || (weapon_->family()==WeaponFamily::Rocket && !hasRounds()))gunInput.aimHeld=gunInput.fireHeld=gunInput.firePressed=gunInput.reloadPressed=false;
  const auto &control = gunInput;
  const bool shotgun=weapon_->shotgun();
  const auto family=weapon_->family();
  const auto drawReady=family==WeaponFamily::Rocket?32u:family==WeaponFamily::Rifle || family==WeaponFamily::Bow || family==WeaponFamily::Mine
      ?rig_->cycle(activeClip(drawClip)).sequenceFrames-1:weapon_->actions().drawReady;
  if (action_ == PlayerAction::Footwork) {
    if (!control.aimHeld || quickTurning_)
      return false;
    beginAction(PlayerAction::Draw);
    if (control.hasCameraLook) {
      drawCameraYawRemaining_ = float(std::clamp(
          control.cameraYaw, -1.0471975803375244, 1.0471975803375244));
      // PlSetLockPitch / shotgun ready00 read getCameraPitch()==0. Keep
      // beginAction's reset; exploration look and weapon elevation differ.
    }
  }
  if (action_ == PlayerAction::Holster) {
    // pl_R1_Footwork's down motion calls actionSelect on every tick. A
    // movement, new draw or knife press can interrupt this recovery pose.
    if(movementHeld || control.aimHeld || control.knifeHeld || control.interactPressed) {
      beginAction(PlayerAction::Footwork);
      if(control.aimHeld)beginAction(PlayerAction::Draw);
      else return false;
    } else {
    auto status = rig_->advanceAction();
    if (status.ended || (status.state & 3))
      beginAction(PlayerAction::Footwork);
    return true;
    }
  }
  if (action_ == PlayerAction::Fire && queuedGunAction_ != PlayerAction::Footwork) {
    // The set dispatcher changes r2; its fire00/reload00 callback executes
    // on the next original tick. Starting it recursively here would make
    // held fire one tick faster than PlShotFrameTbl's authored cadence.
    const auto next = queuedGunAction_;
    beginAction(next);
    rig_->advanceAction();
    return true;
  }
  if (action_ == PlayerAction::Fire && fireRecoilTail_ && fireAimPending_) {
    // set40 changes r3 to zero; set00 installs the idle on the NEXT tick.
    beginAction(PlayerAction::Aim);
  }
  if (action_ == PlayerAction::Fire) {
    ++fireTicks_;
    if(family==WeaponFamily::Grenade) {
      const auto status=rig_->advanceAction();
      if(!throwReleased_ && rig_->crossedFrame(5)) {
        if(!unlimitedAmmo_)--magazine_;
        ++shots_;throwReleased_=true;
      }
      if((!magazine_ && status.sequenceFrame>=15) || status.sequenceFrame>=30)
        beginAction(control.aimHeld && magazine_?PlayerAction::Aim:PlayerAction::Holster);
      return true;
    }
    if(family==WeaponFamily::Machine || family==WeaponFamily::Matilda) {
      const auto status=rig_->advanceAction();
      aimTick(control);rig_->setBlend3Rate(aimRate_);
      if(status.sequenceFrame>=3 && control.aimHeld && control.fireHeld && hasRounds() &&
          (family==WeaponFamily::Machine || burstShots_<3))queuedGunAction_=PlayerAction::Fire;
      else if(status.sequenceFrame>=float(family==WeaponFamily::Machine?3:11))
        beginAction(control.aimHeld?PlayerAction::Aim:PlayerAction::Holster);
      return true;
    }
    if(family==WeaponFamily::Rifle && weapon_->number()==9 && rifleCycling_) {
      if(!control.aimHeld && rig_->sequenceFrame()>=25) {
        beginAction(PlayerAction::Holster);rig_->advanceAction();return true;
      }
      const auto status=rig_->advanceAction();
      if(status.ended || (status.state&3))beginAction(control.aimHeld?PlayerAction::Aim:PlayerAction::Holster);
      return true;
    }
    if(family==WeaponFamily::Rifle) {
      rig_->advanceAction();aimTick(control);
      if(weapon_->number()==10 && fireTicks_>10 && !control.aimHeld)
        beginAction(PlayerAction::Holster);
      else if(fireTicks_>fireReadyTicks()) {
        if(weapon_->number()==9 && hasRounds()) {
          // pl_rifle::fire20 + objSniper::moveFire: matched player/gun FCVs.
          rifleCycling_=true;rig_->startActionFlags(activeClip(18),3,0,5);
          rig_->advanceAction();
          weaponFrame_=0;weaponNewFrame_=true;
          weapon_->sample(WeaponMotion::RifleBoltCycle,0,weaponEmptyVariant_);
        } else beginAction(control.aimHeld?PlayerAction::Aim:PlayerAction::Holster);
      }
      return true;
    }
    if(family==WeaponFamily::Rocket) {
      const auto status=rig_->advanceAction();
      if(rocketDiscarding_) {
        if(status.sequenceFrame>=18){rocketDiscarded_=true;beginAction(PlayerAction::Footwork);}
        return true;
      }
      if(!weapon_->infinite() && !hasRounds() && status.sequenceFrame>=39) {
        rocketDiscarding_=true;rig_->startActionFlags(activeClip(20),7,0,1);return true;
      }
      const bool released=weapon_->infinite() && !control.aimHeld && status.sequenceFrame>=45;
      if(released || status.ended || (status.state&3))
        beginAction(control.aimHeld && hasRounds()?PlayerAction::Aim:PlayerAction::Holster);
      return true;
    }
    if(family==WeaponFamily::Mine) {
      const auto status=rig_->advanceAction();
      if(rig_->crossedFrame(23))setRoomScriptLeftHand(5);
      if(rig_->crossedFrame(30))setRoomScriptLeftHand(4);
      if(usesScope())aimTick(control);
      if(status.ended || (status.state&3))beginAction(control.aimHeld?PlayerAction::Aim:PlayerAction::Holster);
      return true;
    }
    if(family==WeaponFamily::Bow) {
      const auto status=rig_->advanceAction();aimTick(control);rig_->setBlend3Rate(aimRate_);
      if(status.ended || (status.state&3))beginAction(control.aimHeld && hasRounds()?PlayerAction::Aim:PlayerAction::Holster);
      else if(!control.aimHeld && status.sequenceFrame>=5)beginAction(PlayerAction::Holster);
      return true;
    }
    if (!fireRecoilTail_) {
      // pl_shotgun::fire10 owns the whole pump until sequence frame40.
      // Handgun fire10 uses MotionCheckCrossFrame(PlShotFrameTbl - 2).
      // Counted render/adapter ticks cannot replace these source frames.
      if (shotgun && rig_->sequenceFrame() >= 10 && control.aimHeld) {
        aimTick(control);
        rig_->setBlend3Rate(aimRate_);
      }
      const auto status = rig_->advanceAction();
      if(!shotgun) {aimTick(control);rig_->setBlend3Rate(aimRate_);}
      if(shotgun && status.sequenceFrame>=float(fireReadyTicks()) && control.aimHeld && control.fireHeld && !magazine_)
        emitWeaponSound(2,3,4);
      fireRecoilTail_ = shotgun ? status.sequenceFrame >= float(fireReadyTicks())
          : rig_->crossedFrame(float(fireReadyTicks())) || status.ended || (status.state&3);
      if(fireRecoilTail_ && family==WeaponFamily::Rifle && weapon_->number()==9 && magazine_) {
        // pl_rifle::fire20 + objSniper::moveFire: matched player/gun FCVs.
          rifleCycling_=true;rig_->startActionFlags(activeClip(18),3,0,5);
          rig_->advanceAction();
          weaponFrame_=0;weaponNewFrame_=true;
          weapon_->sample(WeaponMotion::RifleBoltCycle,0,weaponEmptyVariant_);
      }
      return true;
    }
    // Both original set40 routines continue the existing recoil motion.
    // set's release check then enters down directly, without a new idle or
    // draw pose. In particular, a shotgun release during its pump cannot
    // briefly re-aim before holstering.
    const auto status = rig_->advanceAction();
    if (!control.aimHeld) {
      beginAction(PlayerAction::Holster);
      rig_->advanceAction();
      return true;
    }
    aimTick(control);
    rig_->setBlend3Rate(aimRate_);
    if(firePending_ && !magazine_ && !reserve_) {
      emitWeaponSound(2,shotgun?3:0x17,4);firePending_=false;
    }
    if (magazine_ == magazineCapacity() || !reserve_)
      reloadPending_ = false;
    if ((control.fireHeld || firePending_) && hasRounds()) {
      queuedGunAction_ = PlayerAction::Fire;
    } else if ((firePending_ || reloadPending_) && reserve_ &&
               magazine_ < magazineCapacity()) {
      queuedGunAction_ = PlayerAction::Reload;
    } else if (status.ended || (status.state & 3) || movementHeld ||
               control.interactPressed) {
      fireAimPending_ = true;
    }
    return true;
  }
  if (action_ == PlayerAction::Reload) {
    if(family==WeaponFamily::Rifle) {
      // Rifle does not use handgun reloadRelease/pitch interruption. Keep
      // ARC17 playing, then restore the scope five frames before the end.
      const auto before=rig_->status();
      const float scopeFrame=float(rig_->cycle(activeClip(reloadClip)).sequenceFrames)-5.f;
      if(!control.aimHeld && before.curveFrame>=float(weapon_->actions().reloadRelease)) {
        beginAction(PlayerAction::Holster);
      } else {
        if(control.aimHeld && !rifleReloadScope_ && before.sequenceFrame>=scopeFrame) {
          rifleReloadScope_=true;aimRate_=0;scopeZoom_=0;
          scopeSway_.pitch=scopeSway_.yaw=0;
          const auto &parts=rig_->jointMatrices();
          for(unsigned k=0;k<3;++k)scopeEyeLocal_[k]=(parts.at(32)[k*4+3]+parts.at(33)[k*4+3])*.5f;
          emitWeaponSound(2,9,0);
        }
        const auto status=rig_->advanceAction();
        if(status.ended || (status.state&3))
          beginAction(control.aimHeld?PlayerAction::Aim:PlayerAction::Holster);
      }
      return true;
    }
    auto status = rig_->advanceAction();
    const float releaseFrame = float(weapon_->actions().reloadRelease);
    if (status.curveFrame >= releaseFrame &&
        (!control.aimHeld || std::abs(aimRate_) > .1f)) {
      if (!control.aimHeld)
        beginAction(PlayerAction::Holster);
      else {
        beginAction(PlayerAction::Aim);
        if(family!=WeaponFamily::Rifle) {
          rig_->startBlend3(activeBlend(aimClips), aimRate_, 9);
          returningFromReload_ = true;
          aimBlendTicks_ = 8;
        }
      }
    } else if (status.ended || (status.state&3)) {
      beginAction(control.aimHeld ? PlayerAction::Aim : PlayerAction::Holster);
    }
    return true;
  }
  if (!control.aimHeld) {
    if (action_ == PlayerAction::Draw && rig_->sequenceFrame() < drawReady) {
      beginAction(PlayerAction::Footwork);
      rig_->tick(Locomotion::Idle);
    } else {
      beginAction(PlayerAction::Holster);
      rig_->advanceAction();
    }
    return true;
  }
  const bool drawing = action_ == PlayerAction::Draw;
  if (drawing) {
    if(rig_->sequenceFrame()>1.7f && rig_->sequenceFrame()<2.3f)
      emitWeaponSound(1,0x28,0); // Dry r100's original m_Work2==0.
    // Frozen upstream wep/pl_handgun.cpp wep02_r3_ready10. Keep the
    // world-space look heading constant while Leon adopts the shoulder look.
    heading_ = GameCubeLimitAngle(
        heading_ +
        GameCubeDrawTurn(rig_->sequenceFrame(), drawCameraYawRemaining_));
    waistYaw_ *= .6f;
    rig_->setWaistYaw(waistYaw_);
  }
  const bool willShoot=hasRounds() && (control.fireHeld || firePending_) &&
      (!drawing || rig_->sequenceFrame()>=drawReady);
  // cMot3's solved rate belongs to this MotionMove; PlWepLockCtrl updates
  // the following pose. Shotgun ready10 is the explicit before-motion case.
  if(drawing && shotgun && rig_->sequenceFrame()>=5 && !willShoot)
    aimTick(control);
  auto status = rig_->advanceAction();
  if(drawing && shotgun && status.sequenceFrame>=drawReady)
    emitWeaponSound(2,9,0xa);
  if(!willShoot && (!drawing || (!shotgun && rig_->sequenceFrame()>=drawReady))) {
    aimTick(control);
    if(action_==PlayerAction::Aim && family!=WeaponFamily::Rifle)rig_->setBlend3Rate(aimRate_);
  }
  if (returningFromReload_) {
    if (aimBlendTicks_ && --aimBlendTicks_ == 0) {
      returningFromReload_ = false;
      beginAction(PlayerAction::Aim);
    }
    return true;
  }
  bool ready = action_ == PlayerAction::Aim ||
               (action_ == PlayerAction::Draw && status.sequenceFrame >= drawReady && !usesScope() && family!=WeaponFamily::Mine && family!=WeaponFamily::Bow) ||
               (action_ == PlayerAction::Fire && fireTicks_ >= fireReadyTicks()+2);
  if (magazine_ == magazineCapacity() || !reserve_)
    reloadPending_ = false;
  if(ready && firePending_ && !hasRounds() && !reserve_) {
    emitWeaponSound(2,shotgun?3:0x17,4);firePending_=false;
  }
  if (ready && (control.fireHeld || firePending_) &&
      (hasRounds() || (reserve_ && firePending_))) {
    if (hasRounds()) {
      beginAction(PlayerAction::Fire);
      rig_->advanceAction();
    } else if (reserve_ && firePending_) {
      beginAction(PlayerAction::Reload);
      rig_->advanceAction();
    }
  } else if (ready && reloadPending_ && magazine_ < magazineCapacity() && reserve_) {
    beginAction(PlayerAction::Reload);
    rig_->advanceAction();
  } else if (action_ == PlayerAction::Draw && ((status.state & 3) || ((shotgun || family==WeaponFamily::Rocket) && status.sequenceFrame>=drawReady))) {
    beginAction(PlayerAction::Aim);
  } else if (action_ == PlayerAction::Fire && (status.state & 3)) {
    beginAction(PlayerAction::Aim);
  }
  return true;
}
void LeonPreview::weaponTick() {
  auto motion = weapon_->motion();
  // The original weapon object tests the last solved motion BEFORE advancing
  // the next pose. Commit once after crossing31, before evaluating frame32.
  if (motion == WeaponMotion::Reload && !reloadCommitted_ &&
      weaponFrame_ >= weapon_->reloadCommitFrame()) {
    unsigned rounds = std::min(magazineCapacity() - magazine_, reserve_);
    magazine_ += rounds;
    reserve_ -= rounds;
    reloadCommitted_ = true;
  }
  if (weaponNewFrame_)
    weaponNewFrame_ = false;
  else
    weaponFrame_ += 1;
  if (motion == WeaponMotion::Idle || motion==WeaponMotion::RifleBoltCycle)
    weaponFrame_ =
        std::min(weaponFrame_, weapon_->maxFrame(motion, weaponEmptyVariant_));
  if (weaponFrame_ > weapon_->maxFrame(motion, weaponEmptyVariant_)) {
    motion = WeaponMotion::Idle;
    weaponFrame_ = 0;
    weaponEmptyVariant_ = magazine_ == 0;
  }
  weapon_->sample(motion, weaponFrame_, weaponEmptyVariant_);
  weapon_->sampleCarry(action_,rig_->sequenceFrame(),hasRounds());
}
Matrix34 LeonPreview::placement() const {
  SmdPlacement placement;
  for (int k = 0; k < 3; ++k)
    placement.position[k] = float(feet_[k]);
  placement.scale = {1, 1, 1};
  placement.rotation[1] = heading_;
  return PlacementMatrix(placement);
}
std::array<double, 3> LeonPreview::muzzle() const {
  const auto mat =
      WeaponAttachmentMatrix(rig_->jointMatrices().at(10), placement());
  const double offset=weapon_->shotgun()?-15.5:234.5;
  return {mat[0] * offset + mat[1] * -24 + mat[2] * 38.33 + mat[3],
          mat[4] * offset + mat[5] * -24 + mat[6] * 38.33 + mat[7],
          mat[8] * offset + mat[9] * -24 + mat[10] * 38.33 + mat[11]};
}
std::array<double, 3> LeonPreview::aimDirection() const {
  const auto mat =
      WeaponAttachmentMatrix(rig_->jointMatrices().at(10), placement());
  std::array<double, 3> result{-mat[0], -mat[4], -mat[8]};
  const double length = std::sqrt(
      result[0] * result[0] + result[1] * result[1] + result[2] * result[2]);
  if (length > 1e-9)
    for (auto &v : result)
      v /= length;
  return result;
}
PlayerShot LeonPreview::shotRay(bool spread) const {
  WeaponRandomOwner random(weaponRandom_);
  const auto mat =
      WeaponAttachmentMatrix(rig_->jointMatrices().at(10), placement());
  const bool shotgun=weapon_->shotgun();
  float ringAngle = shotgun && spread ? fRand1_1() : 0;
  const float y = spread ? fRand1_1() * (shotgun ? 100.f : 200.f) : 0.f;
  const float z = spread ? fRand1_1() * (shotgun ? 100.f : 200.f) : 0.f;
  PlayerShot shot;
  shot.serial = shots_;
  shot.weaponItemId=equippedWeaponId();
  shot.powerLevel=weaponPowerLevels_[weaponProfile_];
  if(action_==PlayerAction::Fire && scopeShot_) {
    shot.from=scopeShot_->from;shot.to=scopeShot_->to;return shot;
  }
  if(usesScope()) {
    const auto camera=scopeCamera();shot.from=camera.eye;
    const auto length=weapon_->number()==10?200000.:300000.;
    for(unsigned k=0;k<3;++k)shot.to[k]=shot.from[k]+(camera.target[k]-camera.eye[k])*(length/300000.);
    return shot;
  }
  if(shot.projectile()) {
    for(unsigned k=0;k<3;++k) {
      shot.from[k]=mat[k*4+3];shot.to[k]=shot.from[k]-mat[k*4]*50000.;
    }
    return shot;
  }
  for (unsigned row = 0; row < 3; ++row) {
    const unsigned k = row * 4;
    const float origin =
        mat[k] * (shotgun?-15.5f:234.5f) + mat[k + 1] * -24.f + mat[k + 2] * 38.33f + mat[k + 3];
    const float direction = mat[k] * -50000.f + mat[k + 1] * y + mat[k + 2] * z;
    shot.from[row] = origin;
    shot.to[row] = direction + origin;
  }
  if(shotgun) {
    shot.pellets.push_back({shot.from,shot.to});
    // Frozen pl_shotgun::wep07_r3_fire00: 6 source origins per ring,
    // widening three-pellet direction rings and original random rotation.
    constexpr std::array<std::array<float,2>,6> offsets{{{.5f,.866f},{.5f,-.866f},{-.5f,.866f},{-.5f,-.866f},{1,0},{-1,0}}};
    for(unsigned i=1;i<13;++i) {
      const float r=float((i-1)/6)+1, r2=float((i-1)/3)+1;
      const auto &d=offsets[(i-1)%6];
      ringAngle+=(spread?fRand0_1():.5f)+3.1415927f;
      const std::array<float,3> local{234.5f,-24+d[0]*r*150,38.33f+d[1]*r*150};
      const std::array<float,3> direction{-50000,std::sin(ringAngle)*r2*1500,std::cos(ringAngle)*r2*1500};
      std::array<CollisionVec,2> pellet{};
      for(unsigned row=0;row<3;++row) {
        const unsigned k=row*4;
        pellet[0][row]=mat[k+3];
        for(unsigned col=0;col<3;++col) {
          pellet[0][row]+=mat[k+col]*local[col];
          pellet[1][row]+=mat[k+col]*direction[col];
        }
        pellet[1][row]+=pellet[0][row];
      }
      shot.pellets.push_back(pellet);
    }
  }
  return shot;
}
PlayerShot LeonPreview::laserRay() const {
  // objWep::getMarkerPos selects part1 for weapon_no 0x0e. objMine's
  // partsSet hangs this aiming part on the right hand, independently of
  // the left-held launcher. Part0's X axis points up in the ready pose.
  const auto posed=weapon_->posedParts(rig_->jointMatrices(),action_,rig_->sequenceFrame());
  const auto mat=WeaponAttachmentMatrix(posed.at(weapon_->number()==14?1:0),placement());
  PlayerShot ray;
  ray.serial = shots_;
  ray.weaponItemId=equippedWeaponId();
  ray.powerLevel=weaponPowerLevels_[weaponProfile_];
  const auto marker=weapon_->markerOffset();
  for (unsigned row = 0; row < 3; ++row) {
    const unsigned k = row * 4;
    const float origin = mat[k] * marker[0] + mat[k + 1] * marker[1] +
                         mat[k + 2] * marker[2] + mat[k + 3];
    ray.from[row] = origin;
    ray.to[row] = origin + mat[k] * (weapon_->number()==28?50000.f:-50000.f);
  }
  return ray;
}
bool LeonPreview::usesScope() const {
  return weapon_->family()==WeaponFamily::Rifle || weapon_->family()==WeaponFamily::Rocket || equippedWeaponId()==0xab;
}
bool LeonPreview::weaponVisible() const {
  // setWepTrans switches at knife ready/down frame4. Number13 stays shown.
  const bool knifeGunVisible=!knifeActive() || weapon_->number()==13 ||
      ((action_==PlayerAction::KnifeDraw || action_==PlayerAction::KnifeHolster) && !bladeVisible_);
  return !unarmed_ && !rocketDiscarded_ && !grabbed() && !blastActive() && !hurtActive() && !dogTrapActive() && !roomScriptActive() && knifeGunVisible && !doorActive() && !ladderActive() && !ladderPropActive() && !fenceActive() && !jumpActive() && !windowActive() && !pushActive() &&
      action_!=PlayerAction::Kick && action_!=PlayerAction::Dead;
}
bool LeonPreview::scopeReady() const {
  return usesScope() && (action_==PlayerAction::Aim ||
      (action_==PlayerAction::Reload && rifleReloadScope_) ||
      (action_==PlayerAction::Fire && weapon_->family()!=WeaponFamily::Rocket && !rifleCycling_));
}
ActionCameraPose LeonPreview::scopeCamera() const {
  ActionCameraPose camera;
  const auto eye=TransformPoint(placement(),scopeEyeLocal_);
  const double pitch=double(aimRate_)*70.*3.14159265358979323846/180.-scopeSway_.pitch;
  const double yaw=double(heading_)+scopeSway_.yaw;
  for(unsigned k=0;k<3;++k)camera.eye[k]=eye[k];
  camera.target={camera.eye[0]+std::sin(yaw)*std::cos(pitch)*300000.,
      camera.eye[1]+std::sin(pitch)*300000.,
      camera.eye[2]+std::cos(yaw)*std::cos(pitch)*300000.};
  const auto *carry=OriginalWeaponCarry(equippedWeaponId());
  camera.fovDegrees=45.+scopeZoom_*((weapon_->family()==WeaponFamily::Rifle && carry && carry->variant==1?3.:9.)-45.);
  return camera;
}
bool LeonPreview::laserReady() const {
  if(usesScope() || weapon_->family()==WeaponFamily::Grenade)return false;
  return action_==PlayerAction::Aim ||
      (action_==PlayerAction::Draw && rig_->sequenceFrame()>=4 && !weapon_->shotgun()) ||
      (action_==PlayerAction::Fire && fireTicks_>=fireReadyTicks());
}
void LeonPreview::update(const std::array<double, 3> &feet, double distance,
                         float heading, bool run, double seconds) {
  for (auto p : feet)
    if (!std::isfinite(p))
      throw std::runtime_error("Leon: Position");
  if (!std::isfinite(distance) || distance < 0 || !std::isfinite(heading))
    throw std::runtime_error("Leon: Motion value");
  feet_ = feet;
  state_ = seconds > 0 && distance > seconds * 10
               ? (run ? Locomotion::Run : Locomotion::Walk)
               : Locomotion::Idle;
  if (state_ != Locomotion::Idle)
    heading_ = heading;
  rig_->update(state_, seconds);
}
std::string LeonPreview::describe() const {
  std::ostringstream s;
  s << "Leon: " << rig_->bones().size() << " joints · " << mesh_.included
    << " models · " << mesh_.triangles << " triangles · "
    << mesh_.textures.size() << " textures · "
    << rig_->clipCount()
    << " original player FCV + IK · GPU skinning · " << weapon_->describe();
  return s.str();
}
std::string LeonPreview::motionInfo() const {
  std::ostringstream s;
  if (action_ != PlayerAction::Footwork) {
    s << PlayerActionLabel(action_) << " · Original-FCV · Frame "
      << int(rig_->curveFrame()) << " · Ammo " << magazine_ << '/' << magazineCapacity() << " + "
      << reserve_;
    return s.str();
  }
  s << LocomotionLabel(state_) << " · Original-Root-Motion · 30 Hz · Frame "
    << int(rig_->sequenceFrame()) << '/'
    << rig_->cycle(activeClip(unsigned(state_))).sequenceFrames;
  return s.str();
}
} // namespace re4
