#include "ambient_animals.h"
#include "archive.h"
#include "ports/rnd.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
unsigned checks;
void require(bool value, const char *message) {
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}
std::vector<std::uint8_t> readEsl(const std::string &path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  require(bool(input), "ESL missing");
  require(input.tellg() == std::streampos(re4::EnemySpawns::FileBytes),
          "ESL size mismatch");
  std::vector<std::uint8_t> bytes(re4::EnemySpawns::FileBytes);
  input.seekg(0);
  require(
      bool(input.read(reinterpret_cast<char *>(bytes.data()), bytes.size())),
      "ESL truncated");
  return bytes;
}
void matrices(const re4::AmbientAnimals &animals) {
  for (const auto &animal : animals.animals()) {
    require(animal.rig().bones().size() == 19, "Chicken skeleton count");
    for (const auto &bone : animal.rig().bones())
      for (auto v : bone)
        require(std::isfinite(v), "Nonfinite chicken pose");
    for (auto v : animal.placement())
      require(std::isfinite(v), "Nonfinite chicken placement");
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    require(argc == 4,
            "Usage: test_ambient_animals EM28.DRS OMAKE01.ESL R400.DAT");
    const auto archive = re4::RoomArchive::Load(argv[3]);
    const auto *record = archive.find("SAT");
    require(record, "r400 SAT missing");
    const auto entry = std::size_t(record - archive.entries().data());
    const re4::Sat satData(archive.copy(entry));
    const re4::SatCollision sat(satData);
    const auto esl = readEsl(argv[2]);
    re4::AmbientAnimals a(sat, esl, argv[1]), b(sat, esl, argv[1]);
    require(a.animals().size() == 6, "Original r400 has six chickens");
    const std::array<unsigned, 6> indices{0, 1, 110, 111, 112, 113};
    const std::array<re4::CollisionVec, 6> positions{{{-16910, 400, -9890},
                                                      {3620, 0, 2080},
                                                      {21200, 1430, 14940},
                                                      {18740, 1430, 16690},
                                                      {22290, 1430, 16090},
                                                      {19790, 1430, 15600}}};
    unsigned variants[2]{};
    for (unsigned i = 0; i < 6; ++i) {
      const auto &animal = a.animals()[i];
      const auto &source = animal.source();
      require(source.listIndex == indices[i], "Original chicken ESL index");
      require(source.position == positions[i],
              "Original chicken ESL placement");
      require(source.room == 0x400 && source.id == 0x28 && source.hp == 1000 &&
                  source.alive() && !source.alreadySet(),
              "Original chicken ESL semantics");
      require(source.raw == re4::EnemySpawns(esl).find(indices[i])->raw,
              "Original chicken raw metadata retained");
      ++variants[animal.variant()];
      const auto feet = animal.feet();
      require(feet[0] == source.position[0] && feet[2] == source.position[2],
              "Chicken X/Z never relocated");
      require(std::abs(feet[1] - source.position[1]) <= 250,
              "Chicken Y never snaps to unrelated floor");
      if (animal.onOriginalFloor())
        require(sat.ground(feet, 1, 1).has_value(), "Chicken SAT floor valid");
      else
        require(feet == source.position, "Chicken authored perch retained");
    }
    require(variants[0] == 4 && variants[1] == 2,
            "Both original chicken variants");
    for (unsigned variant = 0; variant < 2; ++variant) {
      const auto &mesh = a.mesh(variant);
      require(mesh.triangles > 0 && mesh.textures.size() == 1,
              "Chicken original textured geometry");
      require(mesh.fallbackTriangles == 0 && mesh.unresolvedAlphaTriangles == 0,
              "Chicken no missing textures or masks");
      require(mesh.vertices.size() / 3 == a.skin().size(),
              "Chicken skin corner count");
      require(mesh.draws.size() == 107, "Chicken original draw count");
    }
    matrices(a);
    // 30 and 60 render Hz must produce identical source animation/RNG paths.
    // Capture the current global RNG: ambient owns its own deterministic
    // stream and must not change the combat stream.
    RndInit(0x1234);
    unsigned moving = 0, pecking = 0;
    for (unsigned frame = 0; frame < 1800; ++frame) {
      a.step(1. / 30);
      b.step(1. / 60);
      b.step(1. / 60);
      require(RndState() == 0x1234, "Ambient RNG leaked into combat");
      require(a.ticks() == b.ticks(),
              "Ambient tick count depends on render rate");
      for (unsigned i = 0; i < 6; ++i) {
        const auto &x = a.animals()[i], &y = b.animals()[i];
        require(x.feet() == y.feet(),
                "Chicken placement depends on render rate");
        require(x.heading() == y.heading() && x.motion() == y.motion(),
                "Chicken source state depends on render rate");
        require(x.rig().bones() == y.rig().bones(),
                "Chicken pose depends on render rate");
        const auto motion = x.motion();
        moving += motion == re4::AnimalMotion::Walk ||
                  motion == re4::AnimalMotion::WalkAlt;
        pecking += motion == re4::AnimalMotion::Peck ||
                   motion == re4::AnimalMotion::PeckAlt;
        const auto feet = x.feet();
        if (x.onOriginalFloor())
          require(sat.ground(feet, 1, 1).has_value(),
                  "Wandering chicken lost floor");
        else
          require(feet == x.source().position,
                  "Authored perch wandered into empty space");
      }
      if (!(frame % 120))
        matrices(a);
    }
    require(a.ticks() == 1800, "Thirty-Hz tick count");
    unsigned translated = 0;
    for (const auto &animal : a.animals()) {
      const auto original = animal.source().position, now = animal.feet();
      translated +=
          std::hypot(now[0] - original[0], now[2] - original[2]) > 500;
    }
    require(translated >= 3,
            "Chicken walking failed to apply original root motion");
    require(moving > 0 && pecking > 0,
            "Original chicken walk and peck states run");
    const auto old = a.animals()[0].rig().bones();
    a.step(1. / 60);
    require(a.ticks() == 1800, "Fractional tick advanced pose");
    a.suspendClock();
    a.step(1. / 60);
    require(a.ticks() == 1800 && a.animals()[0].rig().bones() == old,
            "Pause retained stale fractional tick");
    const auto badStep = [&](double dt) {
      bool rejected = false;
      try {
        a.step(dt);
      } catch (const std::runtime_error &) {
        rejected = true;
      }
      require(rejected, "Invalid ambient step accepted");
    };
    badStep(-1);
    badStep(1);
    badStep(std::numeric_limits<double>::infinity());
    badStep(std::numeric_limits<double>::quiet_NaN());
    bool rejected = false;
    try {
      (void)a.mesh(2);
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    require(rejected, "Invalid texture variant accepted");
    std::cout << a.describe() << '\n'
              << "Original r400 chickens, skin, textured variants, 30/60-Hz "
              << "motion/SAT/RNG/pause PASS (" << checks << " checks)\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Ambient animals FAIL: " << e.what() << '\n';
    return 1;
  }
}
