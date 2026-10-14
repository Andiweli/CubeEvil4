#include "archive.h"
#include "ports/rnd.h"
#include "world_effects.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks;
void require(bool value, const char *message) {
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}
void put(WaterRecord &r, unsigned p, unsigned value) {
  for (unsigned k = 0; k < 4; ++k)
    r.bytes.at(p + k) = std::uint8_t(value >> (24 - k * 8));
}
void real(WaterRecord &r, unsigned p, float value) {
  unsigned bits;
  std::memcpy(&bits, &value, 4);
  put(r, p, bits);
}
WorldEffectsData fixture() {
  WorldEffectsData data;
  data.sprites.signature = 24;
  data.sequences = {3};
  WaterAnimation a;
  a.id = 0x72;
  a.width = a.height = 4;
  a.loop = 1;
  a.pattern = {0, 1};
  a.duration = {1, 1};
  Texture t;
  t.width = t.height = 4;
  t.rgba.assign(64, 255);
  a.images = {t, t};
  data.sprites.animations = {a};
  WaterRecord r;
  r.bytes[0] = 1;
  r.bytes[1] = 8;
  r.bytes[7] = 0xfe;
  put(r, 8, 1);
  real(r, 12, 10);
  real(r, 16, 20);
  real(r, 20, 30);
  real(r, 48, 1);
  real(r, 88, 90);
  real(r, 136, 100);
  real(r, 140, 200);
  real(r, 152, 1);
  for (unsigned k = 0; k < 4; ++k) {
    r.bytes[156 + k] = 255;
    real(r, 160 + k * 4, 1);
  }
  r.bytes[200] = 10;
  put(r, 204, 1);
  WaterTarget target;
  target.sequence = 3;
  target.records = {r};
  data.sprites.targets = {target};
  return data;
}
} // namespace
int main() {
  try {
    auto data = fixture();
    RndInit(0x9842);
    auto random = RndState();
    WorldEffectsSimulation simulation(data);
    require(RndState() == random, "visual pre-run does not alter combat RNG");
    require(simulation.state().frames == 200,
            "original room pre-run 200 ticks");
    require(simulation.state().sprites.size() == 1,
            "one permanent world sprite");
    require(simulation.state().sprites[0].age == 200,
            "source sprite life ticks");
    float scroll = 0;
    unsigned counter = 0, pattern = 0;
    for (unsigned i = 0; i < 200; ++i) {
      scroll += .001f;
      counter += 32;
      while ((counter >> 5) > 1) {
        pattern = (pattern + 1) % 2;
        counter -= 32;
      }
    }
    require(simulation.state().sprites[0].scroll[0] == scroll,
            "Esp08 original accumulated UV scroll");
    require(simulation.state().sprites[0].pattern == pattern &&
                simulation.state().sprites[0].animationCounter == counter,
            "ANM strict greater-than pattern progression");
    auto state = simulation.state();
    WorldEffectCamera camera;
    camera.eye = {0, 20, 100};
    auto quads = WorldEffectQuads(data, state, camera);
    require(quads.size() == 1 && quads[0].sequence == 3,
            "authored source sequence provenance");
    require(quads[0].tiled && quads[0].repeats[0] == 2 &&
                quads[0].repeats[1] == 1,
            "Esp08 repeats from original Work8");
    require(quads[0].position[0][0] == -40,
            "original animation x pivot preserved");
    for (auto &p : quads[0].position)
      require(std::abs(p[1] - 20) < .00002f,
              "90-degree authored plane remains at world floor height");
    require(std::abs(quads[0].position[0][2] - 130) < .00002f &&
                std::abs(quads[0].position[3][2] + 70) < .00002f,
            "original DEG2RAD ground-plane orientation");
    auto camera2 = camera;
    camera2.right = {0, 0, 1};
    camera2.up = {1, 0, 0};
    require(WorldEffectQuads(data, state, camera2)[0].position ==
                quads[0].position,
            "world-oriented effects do not face camera");
    for (unsigned i = 0; i < 117; ++i)
      simulation.tick();
    auto next = simulation.state();
    require(simulation.restore(state), "context snapshot restored");
    require(simulation.state().frames == state.frames &&
                WorldEffectQuads(data, simulation.state(), camera)[0].scroll ==
                    quads[0].scroll,
            "restore preserves visible phase");
    for (unsigned i = 0; i < 117; ++i)
      simulation.tick();
    require(simulation.state().sprites[0].scroll == next.sprites[0].scroll &&
                simulation.state().sprites[0].pattern ==
                    next.sprites[0].pattern,
            "restore/replay matches uninterrupted 30 Hz execution");
    auto corrupt = state;
    corrupt.signature++;
    require(!simulation.restore(corrupt), "foreign room snapshot rejected");
    corrupt = state;
    corrupt.sprites[0].speed[1] = std::numeric_limits<float>::infinity();
    require(!simulation.restore(corrupt), "non-finite restored speed rejected");
    corrupt = state;
    corrupt.sprites[0].pattern = 255;
    require(!simulation.restore(corrupt), "invalid animation state rejected");
    corrupt = state;
    corrupt.sprites[0].flags |= 0x4000;
    require(!simulation.restore(corrupt), "masked unknown state rejected");
    corrupt = state;
    corrupt.sprites[0].size[0] = 0;
    require(!simulation.restore(corrupt), "invalid restored size rejected");
    auto billboard = fixture();
    billboard.sprites.signature = 25;
    put(billboard.sprites.targets[0].records[0], 8, 0);
    WorldEffectsSimulation billSimulation(billboard);
    auto first = WorldEffectQuads(billboard, billSimulation.state(), camera);
    auto second = WorldEffectQuads(billboard, billSimulation.state(), camera2);
    require(first[0].position != second[0].position,
            "billboard geometry follows actual camera axes");
    auto plain = fixture();
    plain.sprites.signature = 28;
    auto &plainRecord = plain.sprites.targets[0].records[0];
    plainRecord.bytes[1] = 0;
    put(plainRecord, 8, 1);
    real(plainRecord, 88, 0);
    WorldEffectsSimulation plainSimulation(plain);
    auto plainQuad =
        WorldEffectQuads(plain, plainSimulation.state(), camera)[0];
    require(plainQuad.position[0][1] == 120 &&
                plainQuad.position[3][1] == -80 && plainQuad.uv[0][1] == 0 &&
                plainQuad.uv[3][1] == 1,
            "EspInit display-list upper edge matches TPL texture top");
    plainRecord.animation = 0;
    auto fade = fixture();
    fade.sprites.signature = 26;
    fade.sprites.targets[0].records[0].bytes[198] = 30;
    fade.sprites.targets[0].records[0].bytes[199] = 5;
    WorldEffectsSimulation fadeSimulation(fade);
    require(WorldEffectQuads(fade, fadeSimulation.state(), camera).empty(),
            "original near camera fade hides nearby source sprite");
    camera.eye = {10, 20, 2000};
    auto middle = WorldEffectQuads(fade, fadeSimulation.state(), camera);
    require(middle.size() == 1 && middle[0].color[3] > .5f &&
                middle[0].color[3] < .65f,
            "source near/far fade is smooth");

    auto startup = fixture();
    startup.sprites.signature = 101;
    auto pyre = startup.sprites.targets[0];
    pyre.sequence = 0;
    auto &jitter = pyre.records[0];
    jitter.bytes[1] = 11;
    put(jitter, 8, 0x80000);
    real(jitter, 88, 0);
    real(jitter, 224, 10);
    startup.sprites.targets.push_back(pyre);
    startup.sequences.push_back(0);
    startup.targetPrerun = {200, 0};
    WorldEffectsSimulation sourceStart(startup);
    auto initial = sourceStart.state();
    require(initial.streams.size() == 2 && initial.streams[0].frames == 200 &&
                initial.streams[1].frames == 0 && initial.sprites[1].age == 0,
            "explicit EstSet starts at zero beside SstSet pre-run");
    WorldEffectCamera view;
    view.eye = {10, 20, 130};
    view.forward = {0, 0, -1};
    sourceStart.tick(&view);
    auto jittered = sourceStart.state();
    require(jittered.sprites[1].position ==
                    std::array<float, 3>({10, 20, 40}) &&
                jittered.sprites[1].cameraOffset ==
                    std::array<float, 3>({0, 0, 10}),
            "Esp0b pulls original world position toward actual camera");
    sourceStart.tick(&view);
    require(sourceStart.state().sprites[1].position ==
                jittered.sprites[1].position,
            "Esp0b removes old camera offset before updating");
    auto movedView = view;
    movedView.eye = {110, 20, 30};
    movedView.forward = {-1, 0, 0};
    sourceStart.tick(&movedView);
    require(sourceStart.state().sprites[1].position ==
                std::array<float, 3>({20, 20, 30}),
            "moving camera does not leave accumulated Esp0b drift");
    auto firstPan = WorldEffectQuads(startup, jittered, view);
    auto secondPan = WorldEffectQuads(startup, jittered, movedView);
    const auto findPyre = [](const auto &quads) -> const WorldEffectQuad & {
      for (const auto &quad : quads)
        if (quad.sequence == 0)
          return quad;
      throw std::runtime_error("missing test pyre quad");
    };
    require(findPyre(firstPan).position != findPyre(secondPan).position,
            "Tool_flg 0x80000 uses original camera Y-pan");
    require(sourceStart.restore(jittered),
            "two source clocks restore together");
    auto invalid = jittered;
    invalid.streams[1].frames++;
    require(!sourceStart.restore(invalid),
            "wrong explicit EstSet clock rejected");
    invalid = jittered;
    invalid.sprites[1].position[0]++;
    require(!sourceStart.restore(invalid),
            "aggregate/stream disagreement rejected");
    invalid = jittered;
    invalid.streams[1].sprites[0].cameraOffset[2] =
        std::numeric_limits<float>::quiet_NaN();
    require(!sourceStart.restore(invalid), "non-finite camera offset rejected");
    require(sourceStart.state().sprites[1].position ==
                    jittered.sprites[1].position &&
                sourceStart.state().streams[1].random ==
                    jittered.streams[1].random,
            "failed source-clock restore is atomic");

    auto filterData = fixture();
    filterData.sprites.signature = 102;
    filterData.targetPrerun = {0};
    auto &filter = filterData.sprites.targets[0].records[0];
    filter.bytes[1] = 70;
    filter.animation = ~0u;
    filter.bytes[156] = 200;
    filter.bytes[157] = 100;
    filter.bytes[158] = 50;
    filter.bytes[159] = 128;
    filter.bytes[200] = 2;
    filter.bytes[201] = 100;
    filter.bytes[202] = 1;
    WorldEffectsSimulation filters(filterData);
    filters.tick();
    auto requests = WorldEffectGlowRequests(filterData, filters.state());
    require(requests.size() == 1 && requests[0].level == 2 &&
                requests[0].priority == 100 && requests[0].special == 1 &&
                requests[0].color == std::array<std::uint8_t, 3>({100, 50, 25}),
            "Esp46 publishes original alpha-scaled Filter03 request");
    require(WorldEffectQuads(filterData, filters.state(), view).empty(),
            "Esp46 never draws an invented texture or screen rectangle");
    require(filters.state().sprites[0].sizeMultiplier == 1e22f,
            "Esp46 source size prevents scale culling");
    require(filters.restore(filters.state()),
            "texture-free Esp46 state restores");
    require(RndState() == random, "world updates do not alter combat RNG");
    std::cout << "world_effects_tests: " << checks << " checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
