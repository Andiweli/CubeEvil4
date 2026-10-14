#include "archive.h"
#include "environment.h"
#include "scene.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
using namespace re4;
namespace {
unsigned checks;
void require(bool b, const char *text) {
  ++checks;
  if (!b)
    throw std::runtime_error(text);
}
void put(std::vector<std::uint8_t> &b, unsigned p, unsigned x) {
  for (unsigned k = 0; k < 4; ++k)
    b.at(p + k) = std::uint8_t(x >> (24 - 8 * k));
}
void put(WaterRecord &r, unsigned p, unsigned x) {
  for (unsigned k = 0; k < 4; ++k)
    r.bytes.at(p + k) = std::uint8_t(x >> (24 - 8 * k));
}
void real(WaterRecord &r, unsigned p, float x) {
  unsigned u;
  std::memcpy(&u, &x, 4);
  put(r, p, u);
}
WaterRecord record(unsigned id = 8) {
  WaterRecord r;
  r.bytes[0] = 1;
  r.bytes[1] = std::uint8_t(id);
  r.bytes[7] = 253;
  put(r, 8, 0x18000);
  real(r, 12, 256);
  real(r, 16, 224);
  real(r, 48, 1);
  real(r, 136, 448);
  real(r, 140, 448);
  real(r, 152, 1);
  for (unsigned k = 0; k < 4; ++k) {
    r.bytes[156 + k] = 255;
    real(r, 160 + k * 4, 1);
  }
  return r;
}
EnvironmentData fixture() {
  EnvironmentData d;
  d.room = 0x100;
  d.signature = 22;
  WaterAnimation a;
  a.id = 1;
  a.width = a.height = 4;
  a.loop = 1;
  a.pattern = {0, 1};
  a.duration = {1, 1};
  Texture t;
  t.width = t.height = 4;
  t.rgba = std::vector<std::uint8_t>(64, 255);
  a.images = {t, t};
  d.animations = {a};
  WaterTarget target;
  target.records = {record()};
  d.targets = {target};
  return d;
}
std::vector<std::uint8_t> tpl() {
  std::vector<std::uint8_t> b(128);
  put(b, 0, 0x20af30);
  put(b, 4, 1);
  put(b, 8, 12);
  put(b, 12, 32);
  b[33] = b[35] = 1;
  put(b, 36, 4);
  put(b, 40, 96);
  b[96] = 0xf8;
  return b;
}
std::vector<std::uint8_t> eff() {
  // One original-format sequence/TPL/ANM, all offsets relative to their table.
  std::vector<std::uint8_t> b(768);
  put(b, 0, 11);
  unsigned p = 48;
  for (unsigned f = 4; f <= 44; f += 4) {
    put(b, f, p);
    put(b, p, (f == 4 || f == 8 || f == 24 || f == 28 || f == 32) ? 1 : 0);
    p += (f < 24 ? 12 : 8);
  }
  auto u = [&](unsigned at) {
    return (unsigned(b[at]) << 24) | (unsigned(b[at + 1]) << 16) |
           (unsigned(b[at + 2]) << 8) | b[at + 3];
  };
  b[u(4) + 5] = 1;
  put(b, u(24) + 4, 160 - u(24));
  put(b, u(28) + 4, 288 - u(28));
  put(b, u(32) + 4, 320 - u(32));
  auto t = tpl();
  std::copy(t.begin(), t.end(), b.begin() + 160);
  b[289] = b[291] = 1;
  b[297] = 1;
  b[299] = 1;
  b[321] = 1;
  b[356] = 16;
  auto r = record();
  r.bytes[2] = 1;
  std::copy(r.bytes.begin(), r.bytes.end(), b.begin() + 368);
  return b;
}
RoomArchive archive(const std::vector<std::uint8_t> &eff,
                    bool duplicate = false) {
  unsigned n = duplicate ? 2 : 1, offset = 16 + n * 8;
  std::vector<std::uint8_t> b(offset + eff.size() + (duplicate ? 48 : 0));
  put(b, 0, n);
  put(b, 16, offset);
  for (unsigned k = 0; k < n; ++k) {
    if (k)
      put(b, 20, offset + unsigned(eff.size()));
    b[16 + n * 4 + k * 4] = 'E';
    b[17 + n * 4 + k * 4] = 'F';
    b[18 + n * 4 + k * 4] = 'F';
  }
  std::copy(eff.begin(), eff.end(), b.begin() + offset);
  return RoomArchive(std::move(b));
}
EnvironmentData parse(const std::vector<std::uint8_t> &bytes,
                      bool duplicate = false) {
  std::vector<std::uint8_t> b(16);
  b[0] = 64;
  Smd s(b);
  SmdPlacement p;
  p.work = 2;
  p.motion = 255;
  p.scale = {1, 1, 1};
  s.placements = {p};
  SmdModel m;
  m.joints = {{0, 255, {0, 0, 0}}};
  s.models = {m};
  b[0] = 16;
  Smx x(b);
  SmxRecord rec;
  rec.work = 2;
  x.records = {rec};
  return BuildRoomEnvironment(archive(bytes, duplicate), s, x, 0x100, {});
}
void contracts() {
  auto b = eff();
  auto parsed = parse(b, true);
  require(parsed.targets.size() == 1 && parsed.warnings.empty(),
          "Primary EFF occurrence / bounded TPL parsing");
  // No Data_num: Frames padding is unused, even when it contains 0xff.
  b[304] = 255;
  require(parse(b).targets.size() == 1,
          "ANM unused padding is not an image-index map");
  for (unsigned at : {0u, 4u, 8u, 24u, 28u, 32u}) {
    auto bad = b;
    put(bad, at, 0xffffffff);
    require(!parse(bad).warnings.empty(),
            "Malformed EFF must use diagnosed fallback");
  }
  auto bad = b;
  bad[320] = 1;
  require(!parse(bad).warnings.empty(), "EST cannot cross next resource/end");
  bad = b;
  put(bad, 380, 0x7fc00000);
  require(!parse(bad).warnings.empty(), "NaN effect parameter");
  bad = b;
  put(bad, 368 + 204, 0x7fffffff);
  require(!parse(bad).warnings.empty(), "Unbounded scroll rejected");
  for (unsigned length = 0; length < 668; ++length) {
    bad = b;
    bad.resize(length);
    auto d = parse(bad);
    require(d.targets.empty(), "Every truncated header/TPL/ANM/EST is bounded");
  }
  std::mt19937 rng(0x22ef);
  for (unsigned i = 0; i < 1000; ++i) {
    bad = b;
    auto at = rng() % bad.size();
    bad[at] ^= std::uint8_t(rng());
    auto d = parse(bad);
    require(d.targets.size() <= 1, "Mutated resource bounded");
  }
  auto d = fixture();
  EnvironmentSimulation sim(d);
  require(sim.state().sprites.size() == 1, "Static effect initialised");
  sim.tick();
  require(sim.state().sprites[0].pattern == 0,
          "Original ANM uses strict greater-than");
  sim.tick();
  require(sim.state().sprites[0].pattern == 1, "ANM advances at boundary+1");
  auto saved = sim.state();
  for (unsigned i = 0; i < 60; ++i)
    sim.tick();
  auto end = WaterQuads(d, sim.state());
  require(sim.restore(saved), "Valid CPU snapshot");
  for (unsigned i = 0; i < 60; ++i)
    sim.tick();
  auto again = WaterQuads(d, sim.state());
  require(end.size() == again.size() && end[0].position == again[0].position &&
              end[0].color == again[0].color && end[0].image == again[0].image,
          "Exact deterministic resume");
  saved.signature++;
  require(!sim.restore(saved), "Snapshot cannot cross rooms");
  saved = sim.state();
  saved.sprites[0].target = 1000;
  require(!sim.restore(saved), "Snapshot rejects invalid reference");
  d = fixture();
  auto &r = d.targets[0].records[0];
  r = record(0);
  r.bytes[264] = 1;
  r.bytes[267] = 2;
  r.bytes[268] = 2;
  r.bytes[269] = 6;
  r.bytes[270] = 2;
  r.bytes[5] = 3;
  EnvironmentSimulation delayed(d);
  require(delayed.state().sprites.empty(), "Delayed emitter starts empty");
  for (unsigned i = 0; i < 3; ++i)
    delayed.tick();
  require(delayed.state().sprites.empty(), "Set_time honoured");
  delayed.tick();
  require(delayed.state().sprites.size() == 7, "Original Set_num+1 burst");
  require(delayed.state().emitters[0].seed != 0x1234567a,
          "Fixed per-emitter seed advances");
  for (unsigned i = 0; i < 3; ++i)
    delayed.tick();
  require(delayed.state().sprites.size() == 14,
          "Interval counts intervening frames");
  d = fixture();
  d.targets[0].records.clear();
  for (unsigned part : {251u, 249u, 253u, 248u, 252u, 250u}) {
    auto rec = record();
    rec.bytes[7] = std::uint8_t(part);
    d.targets[0].records.push_back(rec);
  }
  EnvironmentSimulation buckets(d);
  auto q = WaterQuads(d, buckets.state());
  for (unsigned i = 0; i < 6; ++i)
    require(q[i].part ==
                std::array<unsigned, 6>{248, 250, 249, 253, 252, 251}[i],
            "Source OT bucket order");
  d = fixture();
  d.targets[0].records[0] = record(0);
  real(d.targets[0].records[0], 36, 2);
  real(d.targets[0].records[0], 64, 1);
  real(d.targets[0].records[0], 48, .5);
  EnvironmentSimulation motion(d);
  motion.tick();
  require(motion.state().sprites[0].position[0] == 258 &&
              motion.state().sprites[0].speed[0] == 1.5,
          "CommonMove position before acceleration/decay");
  SkyObject sky;
  sky.placement.scale = {2, 3, 4};
  sky.placement.position = {10, 20, 30};
  sky.joint = {3, 4, 5};
  sky.rotateJoint = true;
  auto pivot = TransformPoint(PlacementMatrix(sky.placement), sky.joint);
  auto delta = SkyDeltaMatrix(sky, {0, .7f, 0});
  auto fixed = TransformPoint(delta, pivot);
  for (unsigned k = 0; k < 3; ++k)
    require(std::abs(fixed[k] - pivot[k]) < .0001,
            "Sky rotation preserves authored joint pivot");
}
void original(const char *core, const char *r100, const char *r300,
              const char *r301) {
  auto c = RoomArchive::LoadDrs(core);
  auto ce = c.find("EFF");
  auto bytes = c.copy(ce - c.entries().data());
  const char *paths[]{r100, r300, r301};
  unsigned ids[]{0x100, 0x300, 0x301};
  for (unsigned room = 0; room < 3; ++room) {
    auto arc = RoomArchive::Load(paths[room]);
    auto copy = [&](const char *tag) {
      auto e = arc.find(tag);
      return arc.copy(e - arc.entries().data());
    };
    Smd s(copy("SMD"));
    Smx x(copy("SMX"));
    auto data =
        BuildRoomEnvironment(arc, s, x, std::uint16_t(ids[room]), bytes);
    require(data.warnings.empty(),
            "Original room effects must match without fallback");
    require(data.sky.size() == (room ? 2 : 1) &&
                data.targets.size() == (room == 1 ? 2 : 1),
            "Original sky/target counts");
    if (room == 1)
      require(data.targets[1].records.size() == 4,
              "World effects excluded from r300 texture target");
    if (room)
      require(data.animations.back().images.size() == 60,
              "Original 60 TPL animation frames");
    EnvironmentSimulation sim(data);
    std::size_t maximum = 0;
    for (unsigned f = 0; f < 10800; ++f) {
      sim.tick();
      maximum = std::max(maximum, sim.state().sprites.size());
      if (f % 30 == 0) {
        auto quads = WaterQuads(data, sim.state());
        for (auto &q : quads) {
          require(q.animation < data.animations.size() &&
                      q.image < data.animations[q.animation].images.size(),
                  "Original animation bound");
          for (auto &p : q.position)
            require(std::isfinite(p[0]) && std::isfinite(p[1]),
                    "Original quad finite");
        }
      }
    }
    require(maximum < 512, "Six-minute original stream within pool budget");
    std::cout << std::hex << ids[room] << std::dec << " " << data.describe()
              << "; max sprites=" << maximum << '\n';
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    contracts();
    if (argc == 5)
      original(argv[1], argv[2], argv[3], argv[4]);
    else if (argc != 1)
      throw std::runtime_error(
          "Usage: environment_tests [CORE.das R100.dat R300.dat R301.dat]");
    std::cout << "Environment contracts PASS: " << checks << " checks\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
