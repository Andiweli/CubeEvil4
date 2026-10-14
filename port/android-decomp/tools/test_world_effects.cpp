#include "archive.h"
#include "ports/rnd.h"
#include "world_effects.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks;
void require(bool value, const char *message) {
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}
unsigned word(const std::vector<std::uint8_t> &bytes, unsigned p) {
  return (unsigned(bytes.at(p)) << 24) | (unsigned(bytes.at(p + 1)) << 16) |
         (unsigned(bytes.at(p + 2)) << 8) | bytes.at(p + 3);
}
void put(std::vector<std::uint8_t> &bytes, unsigned p, unsigned value) {
  for (unsigned k = 0; k < 4; ++k)
    bytes.at(p + k) = std::uint8_t(value >> (24 - k * 8));
}
RoomArchive archive(std::vector<std::uint8_t> bytes) {
  std::vector<std::uint8_t> out(24 + bytes.size());
  put(out, 0, 1);
  put(out, 16, 24);
  out[20] = 'E';
  out[21] = 'F';
  out[22] = 'F';
  std::copy(bytes.begin(), bytes.end(), out.begin() + 24);
  return RoomArchive(std::move(out));
}
void checkPyre(const RoomArchive &room,
               const std::vector<std::uint8_t> &coreEff) {
  auto data = BuildRoomWorldEffects(room, 0x101, coreEff);
  std::cout << data.describe() << '\n';
  require(data.originalPyre, "r101 explicit EST0 decoded");
  require(data.sequences.size() >= 3 && data.sequences[0] == 3 &&
              data.sequences[1] == 13 && data.sequences[2] == 0 &&
              data.targetPrerun[2] == 0,
          "pyre separate from global SST warmup");
  require(data.sprites.targets[2].records.size() == 13,
          "13 original r101 EST0 records");
  WorldEffectsSimulation simulation(data);
  auto initial = simulation.state();
  require(
      initial.streams.size() == 2 && initial.streams[1].frames == 0 &&
          initial.streams[1].sprites.size() == 9 &&
          initial.streams[1].emitters.size() == 4,
      "source startup starts unadvanced, filter plus eight permanent layers");
  auto glow = WorldEffectGlowRequests(data, initial);
  require(glow.size() == 1 && glow[0].sequence == 0,
          "pyre Esp46 CPU request retained");
  WorldEffectCamera camera;
  camera.eye = {-3400, 2200, 12000};
  std::set<unsigned> flameFrames;
  unsigned maxSprites = 0;
  auto checkpoint = initial;
  for (unsigned frame = 0; frame < 1800; ++frame) {
    const auto &state = simulation.state();
    require(state.streams[1].frames == frame, "explicit pyre source clock");
    maxSprites = std::max(maxSprites, unsigned(state.sprites.size()));
    bool smoke = false;
    for (const auto &sprite : state.sprites) {
      if (sprite.target != 2)
        continue;
      if (sprite.record == 1)
        flameFrames.insert(sprite.pattern);
      if (sprite.record == 12) {
        smoke = true;
        require(sprite.age <= 160, "original smoke lifetime bounded");
        require(sprite.position[1] > 1800,
                "source smoke rises above generator");
        require(sprite.sizeMultiplier >= 1, "source smoke expands");
      }
    }
    if (frame > 0)
      require(smoke, "original finite smoke emitter remains active");
    for (const auto &quad : WorldEffectQuads(data, state, camera)) {
      require(quad.image <
                  data.sprites.animations.at(quad.animation).images.size(),
              "original fire TPL/ANM frame range");
      for (const auto &point : quad.position)
        for (float x : point)
          require(std::isfinite(x), "finite original pyre world geometry");
    }
    if (frame == 500)
      checkpoint = state;
    simulation.tick(&camera);
  }
  require(flameFrames.size() == 30,
          "all 30 original room flame texture frames play");
  require(maxSprites < 100, "source finite emitters keep pyre pool bounded");
  auto uninterrupted = simulation.state();
  require(simulation.restore(checkpoint), "pyre independent clocks restore");
  for (unsigned frame = 500; frame < 1800; ++frame)
    simulation.tick(&camera);
  auto replay = simulation.state();
  require(replay.sprites.size() == uninterrupted.sprites.size(),
          "pyre replay population");
  for (unsigned i = 0; i < replay.sprites.size(); ++i) {
    const auto &a = replay.sprites[i], &b = uninterrupted.sprites[i];
    require(a.position == b.position && a.cameraOffset == b.cameraOffset &&
                a.color == b.color && a.pattern == b.pattern &&
                a.age == b.age && a.serial == b.serial,
            "pyre replay exact camera-relative emitter phase");
  }
  std::cout << "r101 pyre peak sprites=" << maxSprites
            << " flame patterns=" << flameFrames.size() << '\n';
}
void checkExplosion(const WorldEffectsData &data) {
  require(data.triggerBanks.size() == 1 &&
              data.triggerBanks[0].objectId == 0x2d,
          "r400 original ET2D immutable explosion bank");
  const auto target = data.triggerBanks[0].target;
  require(data.sprites.targets[target].records.size() == 22 &&
              data.triggerBanks[0].unsupported == 10,
          "source EST0 provenance retained for every record");
  WorldEffectsSimulation simulation(data);
  auto baseline = simulation.state();
  require(baseline.instances.empty(), "explosions never activate at room load");
  const std::array<float, 3> pos{-3400, 50, 2650}, rot{0, 1.5707964f, 0};
  require(simulation.trigger(0x2d, pos, rot, 1),
          "actual barrel event starts original EST0");
  require(!simulation.trigger(0x2d, pos, rot, 1),
          "duplicate event does not restart explosion");
  require(!simulation.trigger(0x12, pos, rot, 2),
          "unregistered effect owner rejected");
  auto initial = simulation.state();
  require(initial.instances.size() == 1 &&
              initial.instances[0].state.frames == 0,
          "triggered EstSet has no room pre-run");
  bool positioned = false;
  for (const auto &sprite : initial.sprites)
    if (sprite.target == target && sprite.record == 9)
      positioned = sprite.position == std::array<float, 3>({-3400, 650, 2650});
  require(positioned, "original explicit barrel placement overrides EST header "
                      "preview position");
  WorldEffectCamera camera;
  camera.eye = {-3400, 2000, 12000};
  unsigned peak = 0;
  bool fire = false, smoke = false;
  WorldEffectsState checkpoint;
  for (unsigned frame = 0; frame < 300; ++frame) {
    auto state = simulation.state();
    if (frame == 1 || frame == 3) {
      unsigned radial = 0, flameBurst = 0;
      for (const auto &sprite : state.sprites)
        if (sprite.target == target) {
          radial += sprite.record == 3;
          flameBurst += sprite.record == 4;
        }
      require(radial == (frame == 1 ? 2u : 4u),
              "gen00 source N+1 interval with radial two-particle bursts");
      require(flameBurst == 4, "source one-tick flame controller stops after "
                               "its first four children");
    }
    if (frame == 10) {
      const WaterSprite *newest = nullptr;
      for (const auto &sprite : state.sprites)
        if (sprite.target == target && sprite.record == 19 &&
            (!newest || sprite.serial > newest->serial))
          newest = &sprite;
      require(
          newest && std::abs(newest->speed[1] - 31.4f) < .0001f,
          "source D_speed curve uses Time_cnt/Life_max before motion damping");
    }
    peak = std::max(peak, unsigned(state.sprites.size()));
    require(state.sprites.size() <= 512, "shared source particle pool bounded");
    for (const auto &sprite : state.sprites)
      if (sprite.target == target) {
        const auto &record =
            data.sprites.targets[target].records[sprite.record];
        require(record.enabled,
                "unimplemented id never rendered as generic sprite");
        fire |= record.bytes[2] == 0x1f || record.bytes[2] == 0x2c;
        smoke |= record.bytes[2] == 0x52;
        for (float x : sprite.position)
          require(std::isfinite(x),
                  "source explosion world position remains finite");
      }
    if (frame == 8)
      checkpoint = state;
    simulation.tick(&camera);
  }
  require(fire && smoke, "original explosion fire and smoke assets emit");
  require(simulation.state().instances.empty(),
          "finite explosion controllers and sprites expire");
  require(simulation.restore(checkpoint), "active explosion snapshot restores");
  auto corrupted = checkpoint;
  corrupted.instances[0].startFrame++;
  require(!simulation.restore(corrupted),
          "wrong explosion relative clock rejected");
  corrupted = checkpoint;
  corrupted.acceptedEvents.clear();
  require(!simulation.restore(corrupted),
          "unowned explosion snapshot rejected");
  for (unsigned i = 8; i < 300; ++i)
    simulation.tick(&camera);
  require(simulation.state().instances.empty(),
          "restored explosion expires identically");
  require(!simulation.trigger(0x2d, pos, rot, 1),
          "completed event stays deduplicated");
  require(simulation.trigger(0x2d, pos, rot, 2),
          "next distinct barrel starts a fresh source phase");
  std::cout << "ET2D explosion peak sprites=" << peak << '\n';
}
} // namespace
int main(int argc, char **argv) {
  if (argc != 3 && argc != 4) {
    std::cerr << "Usage: world_effects_check CORE_DAS R400_DAT [R101_DAT]\n";
    return 2;
  }
  try {
    auto core = RoomArchive::LoadDrs(argv[1]);
    auto *entry = core.find("EFF");
    require(entry, "core effect archive present");
    auto coreEff = core.copy(entry - core.entries().data());
    auto room = RoomArchive::Load(argv[2]);
    auto data = BuildRoomWorldEffects(room, 0x400, coreEff);
    std::cout << data.describe() << '\n';
    require(data.sprites.warnings.size() == 1 && data.triggerBanks.size() == 1,
            "supported source subset decoded with explicit explosion backend "
            "limits");
    require(data.sequences.size() == 3 && data.sequences[0] == 3 &&
                data.sequences[1] == 13,
            "only original globally active fireplace and ambient sequences");
    require(data.sprites.targets[0].records.size() == 6 &&
                data.sprites.targets[1].records.size() == 7,
            "all 13 original supported world records");
    require(data.sprites.animations.size() == 10,
            "four ambient and six original explosion TPL/ANM sets");
    RndInit(0x2145);
    auto random = RndState();
    WorldEffectsSimulation simulation(data);
    require(simulation.state().sprites.size() == 13,
            "all original permanent sprites alive after source pre-run");
    auto checkpoint = simulation.state();
    WorldEffectCamera camera;
    camera.eye = {-7972, 1600, 8000};
    for (unsigned frame = 0; frame < 600; ++frame) {
      auto quads = WorldEffectQuads(data, simulation.state(), camera);
      require(!quads.empty(), "original source sprites remain visible");
      for (const auto &q : quads) {
        require(q.animation < data.sprites.animations.size() &&
                    q.image <
                        data.sprites.animations[q.animation].images.size(),
                "source animation frame bound");
        for (const auto &p : q.position)
          for (float x : p)
            require(std::isfinite(x), "finite original world vertex");
      }
      simulation.tick();
    }
    auto uninterrupted = simulation.state();
    require(simulation.restore(checkpoint),
            "original source context state restores");
    for (unsigned frame = 0; frame < 600; ++frame)
      simulation.tick();
    const auto &replay = simulation.state();
    require(replay.frames == uninterrupted.frames, "source replay clock");
    for (unsigned i = 0; i < replay.sprites.size(); ++i) {
      const auto &a = replay.sprites[i], &b = uninterrupted.sprites[i];
      require(a.position == b.position && a.color == b.color &&
                  a.scroll == b.scroll && a.pattern == b.pattern &&
                  a.animationCounter == b.animationCounter,
              "source pause/context replay exact phase");
    }
    require(RndState() == random,
            "world simulation leaves combat RNG untouched");
    require(BuildRoomWorldEffects(room, 0x300, coreEff).sprites.targets.empty(),
            "other rooms never receive r400 effect profile");
    auto *roomEff = room.find("EFF");
    auto bytes = room.copy(roomEff - room.entries().data());
    auto ids = word(bytes, 12), offsets = word(bytes, 36);
    unsigned selected = 0;
    for (unsigned i = 0; i < word(bytes, ids); ++i)
      if (bytes.at(ids + 4 + i * 8) == 0 && bytes.at(ids + 5 + i * 8) == 3)
        selected = offsets + word(bytes, offsets + 4 + i * 4);
    require(selected != 0,
            "original fireplace SST located for rejection tests");
    for (unsigned p : {0u, 12u, 36u}) {
      auto invalid = bytes;
      put(invalid, p, 0xffffffff);
      auto malformed =
          BuildRoomWorldEffects(archive(std::move(invalid)), 0x400, coreEff);
      require(!malformed.sprites.warnings.empty(),
              "invalid EFF version/offset diagnosed without relocation");
    }
    for (unsigned size :
         {0u, 16u, 47u, 48u, selected + 47, selected + 48 + 6 * 300 - 1}) {
      auto truncated = bytes;
      truncated.resize(size);
      auto malformed =
          BuildRoomWorldEffects(archive(std::move(truncated)), 0x400, coreEff);
      require(!malformed.sprites.warnings.empty(),
              "truncated original effect tables are diagnosed");
    }
    for (unsigned p : {12u, 48u, 88u, 136u, 160u}) {
      auto invalid = bytes;
      put(invalid, selected + 48 + p, 0x7fc00000);
      auto malformed =
          BuildRoomWorldEffects(archive(std::move(invalid)), 0x400, coreEff);
      require(malformed.sequences == std::vector<unsigned>({13}) &&
                  !malformed.sprites.warnings.empty(),
              "non-finite fireplace record safely rejected as a whole");
    }
    auto areaOnly = bytes;
    for (unsigned i = 0; i < word(bytes, ids); ++i)
      if (bytes.at(ids + 5 + i * 8) == 3)
        areaOnly.at(ids + 7 + i * 8) = 0;
    require(BuildRoomWorldEffects(archive(std::move(areaOnly)), 0x400, coreEff)
                    .sequences == std::vector<unsigned>({13}),
            "area-only data is never forced globally active");
    checkExplosion(data);
    if (argc == 4)
      checkPyre(RoomArchive::Load(argv[3]), coreEff);
    require(RndState() == random,
            "source event, fire and smoke updates isolate combat RNG");
    std::cout << "world_effects_check: " << checks
              << " original-data checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
