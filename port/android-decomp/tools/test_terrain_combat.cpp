#include "archive.h"
#include "enemy_encounter.h"
#include "input.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
}
int main(int argc, char **argv) {
  try {
    require(argc == 6, "Expected r400.dat PL00 WEP01 EM12 emleon00.esl");
    auto archive = re4::RoomArchive::Load(argv[1]);
    const auto *satEntry = archive.find("SAT");
    const auto *eatEntry = archive.find("EAT");
    require(satEntry && eatEntry, "r400 requires original SAT and EAT");
    re4::Sat sat(archive.copy(satEntry - archive.entries().data()));
    re4::Sat eat(archive.copy(eatEntry - archive.entries().data()));
    re4::SatCollision world(sat), effects(eat);
    std::ifstream file(argv[5], std::ios::binary);
    require(bool(file), "Original ESL file readable");
    re4::EnemySpawns spawns(std::vector<std::uint8_t>{std::istreambuf_iterator<char>(file), {}});
    const auto *spawn = spawns.find(re4::EnemyEncounter::TestSpawn);
    require(spawn, "Original ESL actor exists");
    re4::EnemyEncounter enemy(world, effects, *spawn, argv[4], true,
                             re4::EnemyEncounter::TerrainEnemyPlacement());
    re4::LeonPreview player(argv[2], argv[3], argv[4]);
    re4::WalkProbe probe(world);
    require(probe.place(re4::EnemyEncounter::TerrainPlayerPosition()), "r400 player floor is capsule-free");
    player.setPlacement(probe.position(), 3.14159265358979323846f);
    require(std::abs(probe.position()[1] - 2576) < 1 &&
            std::abs(enemy.feet()[1] - 2274) < 1, "r400 uses sloped authored terrain, not a fallback plane");
    auto tick = [&](const re4::PlayerActionInput &input = re4::PlayerActionInput{}) {
      enemy.stepPlayer(player, probe, 0, 0, false, false, 1. / 30, input);
      for (const auto &point : {probe.position(), enemy.feet()})
        for (auto coordinate : point) require(std::isfinite(coordinate), "Finite r400 actor feet");
    };
    for (unsigned frame = 0; frame < 900 && !enemy.grabs(); ++frame) tick();
    require(enemy.grabs() == 1 && player.action() == re4::PlayerAction::Grabbed,
            "Ganado traverses r400 slope and reaches original paired grab");
    re4::InputBuffer buffer;
    re4::RawInput raw;
    raw.lt = 1;
    for (float x : {-.4f, .4f, -.4f, .4f, -.4f, .4f}) {
      raw.lx = x; buffer.submit(raw);
    }
    re4::PlayerActionInput input;
    input.aimHeld = true;
    input.resistancePresses = buffer.consume().resistancePresses;
    tick(input);
    require(enemy.resistance() == 12, "r400 receives all six analog reversals while L2 held");
    for (unsigned frame = 0; frame < 500 && !enemy.escapes(); ++frame) tick();
    require(enemy.escapes() == 1 && player.life() > 0,
            "Original paired resistance escape succeeds on r400 terrain");
    for (unsigned frame = 0; frame < 180 && player.action() != re4::PlayerAction::Footwork; ++frame) tick();
    require(player.action() == re4::PlayerAction::Footwork && player.life() > 0,
            "Leon returns to controllable footwork after sloped-terrain escape");
    std::cout << "Original r400 SAT/EAT combat PASS: grounded slope placement, Ganado approach/catch, six latched LS reversals, escape and recovery; HP="
              << player.life() << '/' << enemy.hp() << '\n';
  } catch (const std::exception &error) {
    std::cerr << "r400 combat FAIL: " << error.what() << '\n';
    return 1;
  }
}
