// Original supplied disc resources only. No original game data is bundled.
#include "leon_preview.h"
#include "ports/rnd.h"
#include "room_ladders.h"
#include "room_interactions.h"
#include "weapon_preview.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks{};
void require(bool yes, const char *message) {
  if (!yes) throw std::runtime_error(message);
  ++checks;
}
std::vector<std::uint8_t> entry(const re4::RoomArchive &archive, const char *tag) {
  const auto *resource = archive.find(tag);
  require(resource != nullptr, "Original room resource exists");
  return archive.copy(resource - archive.entries().data());
}
re4::CollisionVec point(const re4::RoomObjectPlacement &p, double x,
                       double y, double z) {
  const auto c = std::cos(p.rotation[1]), s = std::sin(p.rotation[1]);
  return {p.position[0]+c*x+s*z, p.position[1]+y, p.position[2]-s*x+c*z};
}
void equal(const re4::CollisionVec &a, const re4::CollisionVec &b) {
  for (unsigned k = 0; k < 3; ++k)
    require(std::abs(a[k]-b[k]) < .02, "Presented camera matches original transformed profile");
}
void stationaryPresentation(re4::LeonPreview &leon) {
  const auto feet = leon.feet(); const auto bones = leon.rig().bones();
  const auto status = leon.rig().status(); const auto random = RndState();
  const auto displayed = leon.renderJointMatrices();
  for (unsigned repeat = 0; repeat < 3; ++repeat) {
    require(leon.renderJointMatrices() == displayed, "Display queries are deterministic");
    leon.actionCamera(); leon.renderPlacement();
  }
  require(leon.feet() == feet && leon.rig().bones() == bones &&
          leon.rig().status().sequenceFrame == status.sequenceFrame &&
          RndState() == random, "Display consumes no root, FCV, Hokan, clock or RNG state");
}
void objectLadders(const char *path, const char *player, const char *weapon) {
  const auto archive = re4::RoomArchive::Load(path);
  const re4::Sat sat(entry(archive, "SAT")); const re4::SatCollision world(sat);
  const re4::RoomFlow flow(0x101, entry(archive,"AEV"));
  re4::RoomSaveState state; state.room.set(8);
  const auto placements = re4::ReadRoomObjectPlacements(entry(archive,"ETS"));
  re4::RoomLadders ladders(flow, archive, placements, state, player);
  require(ladders.objectLadders().size() == 3, "Post-house original village has three object ladders");
  re4::WalkConfig config; config.originalScenerySphere = true; config.stepUp=600; config.drop=999;
  re4::WalkProbe probe(world,config);
  probe.setObjectCollision(ladders.collision()); probe.setObjectBodies(ladders.objectBodies());
  re4::LeonPreview leon(player,weapon); leon.setRenderInterpolation(true);
  for (const auto &placement : ladders.objectLadders()) {
    bool offered = false;
    for (double z : {600.,800.,1000.}) {
      if (!probe.place(point(placement,0,0,z))) continue;
      leon.setPlacement(probe.position(),float(placement.rotation[1]+3.14159265358979323846));
      const auto offer = ladders.offer(leon,probe);
      if (offer && offer->object && offer->no == placement.slot) { offered = true; break; }
    }
    require(offered && ladders.interact(leon,probe), "Original ladder offers and starts its true climb");
    unsigned ticks = 0;
    while (leon.ladderActive() && ticks++ < 400) {
      const auto before = leon.feet();
      leon.step(probe,0,0,false,false,1./30);
      if (!leon.ladderActive()) break;
      const auto current = leon.feet();
      equal(leon.renderFeet(),before);
      leon.step(probe,0,0,false,false,1./60);
      require(leon.feet() == current, "Half display tick does not advance climb simulation");
      re4::CollisionVec middle{};
      for (unsigned k=0;k<3;++k) middle[k]=(before[k]+current[k])*.5;
      equal(leon.renderFeet(),middle);
      const auto camera = leon.actionCamera();
      require(camera && camera->fovDegrees == 55, "House ladder uses original 55-degree action camera");
      const auto part = re4::WeaponAttachmentMatrix(leon.renderJointMatrices().at(0),leon.renderPlacement());
      const auto from = re4::TransformPoint(part,{0,300,-1800});
      const auto at = re4::TransformPoint(part,{0,300,0});
      equal(camera->eye,{from[0],from[1],from[2]});
      equal(camera->target,{at[0],at[1],at[2]});
      stationaryPresentation(leon);
      // Finish the fractional tick, then cancel its accumulated display-only
      // half on the next pass: one simulation step still occurs per loop.
      leon.step(probe,0,0,false,false,1./60);
      leon.drainLadderSounds();
    }
    require(!leon.ladderActive() && !leon.ladderLandingFailed(), "Smoothed display preserves genuine roof landing");
    require(!leon.actionCamera(), "Object camera releases when source climb ends");
  }
}
void jumps(const char *path, const char *player, const char *weapon) {
  const auto archive = re4::RoomArchive::Load(path);
  const re4::Sat sat(entry(archive,"SAT")); const re4::SatCollision world(sat);
  re4::WalkProbe probe(world); re4::LeonPreview leon(player,weapon);
  leon.setRenderInterpolation(true); leon.setUnlimitedHealth(true);
  unsigned completed = 0;
  for (std::size_t i=0;i<sat.polygons.size() && completed<3;++i) {
    const auto &polygon = sat.polygons[i];
    if (!(polygon.attributes & 0x100010)) continue;
    const auto normal = sat.normals[polygon.normal];
    if (std::abs(normal[1]) > .5) continue;
    re4::CollisionVec approach{};
    for (const auto &corner : world.triangle(i))
      for (unsigned k=0;k<3;++k) approach[k]+=corner[k]/3;
    for (unsigned k=0;k<3;++k) approach[k]+=normal[k]*600;
    const auto floor = world.ground(approach,1000,1500); if (!floor) continue;
    approach[1]=floor->point[1]; if (!probe.place(approach)) continue;
    leon.setPlacement(probe.position(),float(std::atan2(-normal[0],-normal[2])));
    const auto offer = leon.jumpOffer(probe); if (!offer) continue;
    const auto view = re4::PoseFollowCamera({},&world,leon.feet(),leon.heading());
    leon.beginJump(probe,*offer,&view);
    unsigned ticks = 0;
    while (leon.jumpActive() && ticks++<260) {
      leon.step(probe,0,0,false,false,1./30);
      if (!leon.jumpActive()) break;
      const auto position = leon.feet(); const auto motion = leon.jumpState().ticks;
      leon.step(probe,0,0,false,false,1./60);
      require(leon.feet() == position && leon.jumpState().ticks == motion,
              "Fractional drop display does not advance authored fall/landing");
      stationaryPresentation(leon);
      if (const auto camera = leon.actionCamera()) {
        require(std::abs(camera->eye[1]-leon.renderFeet()[1]) < 4000,
                "Falling camera captures the current placement rather than the upper roof start");
        const auto &parts = leon.renderJointMatrices();
        const auto a = re4::TransformPoint(leon.renderPlacement(),{parts[1][3],parts[1][7],parts[1][11]});
        const auto b = re4::TransformPoint(leon.renderPlacement(),{parts[2][3],parts[2][7],parts[2][11]});
        double da = 0, db = 0;
        for (unsigned k=0;k<3;++k) {
          da += std::pow(camera->target[k]-a[k],2);
          db += std::pow(camera->target[k]-b[k],2);
        }
        require(std::min(da,db)<.001, "Original falling camera tracks the rendered source torso part");
      }
      leon.step(probe,0,0,false,false,1./60);
    }
    require(!leon.jumpActive() && !leon.jumpState().failed,
            "Smoothed original drop/landing completes on true lower SAT ground");
    ++completed;
  }
  require(completed == 3, "Three distinct real village drop approaches tested");
}
void windows(const char *path, const char *player, const char *weapon) {
  const auto archive = re4::RoomArchive::Load(path);
  const re4::Sat sat(entry(archive,"SAT")); const re4::SatCollision world(sat);
  const auto placements = re4::ReadRoomObjectPlacements(entry(archive,"ETS"));
  re4::RoomInteractions interior(0x101,archive,placements,player,re4::RoomSaveState{});
  re4::WalkConfig config; config.originalScenerySphere=true; config.stepUp=600; config.drop=999;
  re4::WalkProbe probe(world,config); probe.setObjectCollision(interior.collision());
  re4::LeonPreview leon(player,weapon); leon.setRenderInterpolation(true);
  leon.installWindowMotions(interior.windowMotions());
  unsigned directions = 0;
  for (const auto &window : interior.windows()) for (bool front : {false,true}) {
    std::optional<re4::InteriorWindowOffer> offer;
    for (double x : {-350.,0.,350.}) for (double z : {450.,650.,850.}) {
      if (offer || !probe.place(point(window.placement,x,-1000,front?z:-z))) continue;
      leon.setPlacement(probe.position(),float(window.placement.rotation[1]+(front?3.14159265358979323846:0)));
      const auto candidate=interior.windowOffer(leon,probe);
      if (candidate && candidate->slot==window.placement.slot) offer=candidate;
    }
    if (!offer || (directions & (1u<<offer->direction))) continue;
    directions |= 1u<<offer->direction; leon.beginWindow(probe,*offer);
    unsigned ticks = 0;
    while (leon.windowActive() && ticks++<180) {
      leon.step(probe,0,0,false,false,1./30);
      if (!leon.windowActive()) break;
      for (unsigned fraction=0;fraction<2;++fraction) {
        const auto camera=leon.actionCamera();
        require(camera && camera->fovDegrees>1 && camera->fovDegrees<179,
                "First and fractional window poses never blend an unsampled zero FOV");
        double squared = 0;
        for (unsigned k=0;k<3;++k) squared+=std::pow(camera->eye[k]-camera->target[k],2);
        require(squared>1, "First and fractional FCV cameras keep a nonzero optical axis");
        stationaryPresentation(leon);
        leon.step(probe,0,0,false,false,1./60);
      }
    }
    require(!leon.windowActive() && !leon.actionCamera(), "Window action returns camera ownership at real landing");
  }
  require(directions != 0, "At least one original village window direction exercised");
}
}
int main(int argc,char **argv) {
  try {
    if(argc != 4) throw std::runtime_error("Usage: m33_traversal_camera_check R101.dat PL00.drs WEP01.drs");
    objectLadders(argv[1],argv[2],argv[3]); jumps(argv[1],argv[2],argv[3]);
    windows(argv[1],argv[2],argv[3]);
    std::cout << "M33 source action cameras and fractional traversal display PASS: " << checks << " checks\n";
    return 0;
  } catch(const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
