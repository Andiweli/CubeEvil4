// re4_format_compare, app side: reports what the checked big-endian readers
// of RE4-Android-Decomp see (archive.cpp, smd.cpp, smx.cpp, sat.cpp,
// room_flow.cpp, enemy_spawn.cpp, event_data.cpp, model_skin.cpp and
// room_lighting.cpp), with the keys of game_side.cpp. Values come from the
// readers' results; where a reader keeps raw bytes (display lists, colours,
// a record's raw copy) those bytes are reported.
#include "app_side.h"

#include "archive.h"
#include "enemy_spawn.h"
#include "event_data.h"
#include "model_skin.h"
#include "ports/yz2.h"
#include "room_flow.h"
#include "room_lighting.h"
#include "sat.h"
#include "smd.h"
#include "smx.h"

#include "facts.h"

#include <cstring>
#include <stdexcept>

namespace re4cmp {
namespace {

void sinkList(void* ctx, const char* key, const char* value)
{
    static_cast<FactList*>(ctx)->emplace_back(key, value);
}

std::uint32_t beWord(const Bytes& b, std::size_t p)
{
    if (p + 4 > b.size()) {
        return 0;
    }
    return std::uint32_t(b[p]) << 24 | std::uint32_t(b[p + 1]) << 16 | std::uint32_t(b[p + 2]) << 8 | b[p + 3];
}

std::uint32_t color(const std::array<unsigned char, 4>& c)
{
    return std::uint32_t(c[0]) << 24 | std::uint32_t(c[1]) << 16 | std::uint32_t(c[2]) << 8 | c[3];
}

// Runs one reader; its exception becomes "<prefix>skip".
template <class F> void guarded(const Re4Out& o, F&& f)
{
    try {
        f();
    } catch (const std::exception& e) {
        o.s(e.what(), "skip");
    }
}

// ---- models -----------------------------------------------------------------

template <std::size_t N>
void floatList(const Re4Out& o, const char* name, const std::vector<std::array<float, N>>& list)
{
    unsigned hash = RE4CMP_HASH_START;
    for (std::size_t i = 0; i < list.size(); ++i) {
        for (std::size_t k = 0; k < N; ++k) {
            hash = re4cmp_hash_u32(hash, re4cmp_float_bits(list[i][k]));
            if (i < RE4CMP_LIST) {
                o.f(list[i][k], "%s.%u.%u", name, unsigned(i), unsigned(k));
            }
        }
    }
    o.u(list.size(), "%s.n", name);
    o.u(hash, "%s.hash", name);
}

// `bytes` is what the Smd reader parsed; m.offset is the BIN inside it.
void modelFacts(const Re4Out& o, const re4::SmdModel& m, const Bytes& bytes)
{
    o.u(m.version, "version");
    o.u(m.parts, "parts");
    o.u(m.weightPalette, "wpal");
    o.u(m.weightExt, "wext");
    o.u(m.displayLists, "lists");
    o.u(m.vertices, "vtx");
    o.u(m.shift, "shift");
    o.u(m.sourceFlags, "flags");
    for (std::size_t j = 0; j < m.joints.size(); ++j) {
        o.u(m.joints[j].attachment, "joint.%u.att", unsigned(j));
        o.u(m.joints[j].parent, "joint.%u.parent", unsigned(j));
        for (unsigned k = 0; k < 3; ++k) {
            o.f(m.joints[j].center[k], "joint.%u.c.%u", unsigned(j), k);
        }
    }
    floatList(o, "pos", m.positions);
    floatList(o, "nrm", m.normals);
    floatList(o, "uv", m.texcoords);
    unsigned hash = RE4CMP_HASH_START;
    for (const auto& c : m.colors) {
        hash = re4cmp_hash(hash, c.data(), 4);
    }
    o.u(m.colors.size(), "clr.n");
    o.u(m.colors.empty() ? 0 : hash, "clr.hash");
    // display lists: the reader keeps each 32-byte header; the streams follow
    // them back to back from the parts offset
    std::size_t part = m.offset + beWord(bytes, m.offset + 0x1C);
    hash = RE4CMP_HASH_START;
    for (std::size_t i = 0; i < m.materialHeaders.size(); ++i) {
        const auto& h = m.materialHeaders[i];
        std::uint32_t size = std::uint32_t(h[24]) << 24 | std::uint32_t(h[25]) << 16 | std::uint32_t(h[26]) << 8 | h[27];
        std::uint32_t npoly = std::uint32_t(h[28]) << 24 | std::uint32_t(h[29]) << 16 | std::uint32_t(h[30]) << 8 | h[31];
        o.u(size, "dl.%u.size", unsigned(i));
        o.u(npoly, "dl.%u.npoly", unsigned(i));
        o.u(h[0x0B], "dl.%u.flag", unsigned(i));
        o.u(h[0x0C], "dl.%u.tex", unsigned(i));
        if (part + 32 + size <= bytes.size()) {
            hash = re4cmp_hash(hash, bytes.data() + part + 32, size);
        }
        part += 32 + size;
    }
    o.u(hash, "dl.hash");
}

void skinFacts(const Re4Out& o, const re4::SkinnedModel& s)
{
    unsigned hash = RE4CMP_HASH_START;
    o.u(s.weights.size(), "skin.n");
    for (std::size_t i = 0; i < s.weights.size(); ++i) {
        const auto& w = s.weights[i];
        hash = re4cmp_hash_u32(hash, w.count);
        if (i < RE4CMP_LIST) {
            o.u(w.count, "skin.%u.n", unsigned(i));
        }
        for (unsigned j = 0; j < w.count && j < 3; ++j) {
            hash = re4cmp_hash_u32(hash, w.joints[j]);
            hash = re4cmp_hash_u32(hash, re4cmp_float_bits(w.rates[j]));
            if (i < RE4CMP_LIST) {
                o.u(w.joints[j], "skin.%u.j%u", unsigned(i), j);
                o.f(w.rates[j], "skin.%u.r%u", unsigned(i), j);
            }
        }
    }
    o.u(hash, "skin.hash");
    hash = RE4CMP_HASH_START;
    for (auto index : s.positionWeights) {
        hash = re4cmp_hash_u32(hash, index);
    }
    o.u(s.positionWeights.size(), "pw.n");
    o.u(hash, "pw.hash");
    hash = RE4CMP_HASH_START;
    for (auto index : s.normalWeights) {
        hash = re4cmp_hash_u32(hash, index);
    }
    o.u(s.normalWeights.size(), "nw.n");
    o.u(hash, "nw.hash");
    o.u(s.blendJoints.size(), "blend.n");
    for (std::size_t i = 0; i < s.blendJoints.size() && i < RE4CMP_LIST; ++i) {
        for (unsigned k = 0; k < 4; ++k) {
            o.u(s.blendJoints[i][k], "blend.%u.%u", unsigned(i), k);
        }
    }
}

bool isModel(const Bytes& b)
{
    auto v = beWord(b, 0x3C);
    return b.size() >= 0x40 && (v == 0x20010801 || v == 0x20030817 || v == 0x20030818);
}

// A standalone BIN: the mesh through the SMD reader (as SkinnedModel does,
// with its 32-byte resource prefix), then the skinning palette.
void binFacts(const Re4Out& o, const Bytes& bin)
{
    if (!isModel(bin)) {
        return;
    }
    guarded(o, [&] {
        Bytes resource(32);
        resource[0] = 0x40;
        resource[7] = 16;
        resource[19] = 16;
        resource.insert(resource.end(), bin.begin(), bin.end());
        re4::Smd parsed(resource, { 1, 0, 0 });
        modelFacts(o, parsed.models.at(0), resource);
    });
    Re4Out skin = o.sub("skin.");
    try {
        re4::SkinnedModel model(bin);
        skinFacts(o, model);
    } catch (const std::exception& e) {
        // drops the "skin." keys and the two vertex weight lists
        o.s(e.what(), "skin.skip");
        o.s(e.what(), "pw.skip");
        o.s(e.what(), "nw.skip");
        o.s(e.what(), "blend.skip");
    }
}

// ---- SMD / SMX / SAT -----------------------------------------------------------

void smdFacts(const Re4Out& o, const Bytes& b)
{
    re4::Smd s(b);
    o.u(s.version, "version");
    o.u(s.flags, "flags");
    o.u(s.placements.size(), "count");
    o.u(s.groups.size(), "groups.n");
    for (std::size_t i = 0; i < s.groups.size(); ++i) {
        o.u(s.groups[i], "groups.%u", unsigned(i));
    }
    o.u(s.workOffset, "works");
    for (std::size_t i = 0; i < s.placements.size(); ++i) {
        const auto& w = s.placements[i];
        for (unsigned k = 0; k < 3; ++k) {
            o.f(w.position[k], "place.%u.pos.%u", unsigned(i), k);
            o.f(w.rotation[k], "place.%u.rot.%u", unsigned(i), k);
            o.f(w.scale[k], "place.%u.scale.%u", unsigned(i), k);
        }
        o.u(w.bin, "place.%u.bin", unsigned(i));
        o.u(w.tpl, "place.%u.tpl", unsigned(i));
        o.u(w.motion, "place.%u.mot", unsigned(i));
        o.u(w.work, "place.%u.work", unsigned(i));
        o.u(w.flags, "place.%u.flags", unsigned(i));
    }
    const std::vector<std::size_t>* tables[3] = { &s.binOffsets, &s.tplOffsets, &s.motionOffsets };
    const char* names[3] = { "bins", "tpls", "mots" };
    for (unsigned t = 0; t < 3; ++t) {
        o.u(tables[t]->size(), "%s.n", names[t]);
        for (std::size_t i = 0; i < tables[t]->size(); ++i) {
            o.u((*tables[t])[i], "%s.%u", names[t], unsigned(i));
        }
    }
    for (std::size_t i = 0; i < s.models.size(); ++i) {
        modelFacts(o.sub("model.%u.", unsigned(i)), s.models[i], b);
    }
}

void smxFacts(const Re4Out& o, const Bytes& b)
{
    re4::Smx x(b);
    o.u(x.records.size(), "n");
    for (std::size_t i = 0; i < x.records.size(); ++i) {
        const auto& r = x.records[i];
        Re4Out a = o.sub("rec.%u.", unsigned(i));
        a.u(r.work, "work");
        a.u(r.type, "type");
        a.u(r.order, "order");
        a.u(r.cull, "cull");
        a.u(r.lightMask, "lmask");
        a.u(r.flags, "flags");
        a.u(r.material, "mat");
        a.u(r.specular, "spec");
        a.f(r.uvSpeed[0], "u");
        a.f(r.uvSpeed[1], "v");
        // objectWork: the 116 bytes of Free, then the specular colour
        if (r.type == 1 || r.type == 2) {
            unsigned n = r.type == 1 ? 3 : 13;
            for (unsigned k = 0; k < n; ++k) {
                Bytes word(r.objectWork.begin() + k * 4, r.objectWork.begin() + k * 4 + 4);
                std::uint32_t bits = beWord(word, 0);
                float value;
                std::memcpy(&value, &bits, 4);
                a.f(value, "free.%u", k);
            }
        } else {
            a.u(re4cmp_hash(RE4CMP_HASH_START, r.objectWork.data(), 116), "free.hash");
        }
    }
}

void satFacts(const Re4Out& o, const Bytes& b)
{
    re4::Sat s(b);
    o.u(s.offsetSlots, "slots");
    const std::vector<std::array<float, 3>>* arrays[3] = { &s.vertices, &s.normals, &s.edges };
    const char* names[3] = { "vtx", "nrm", "edge" };
    for (unsigned a = 0; a < 3; ++a) {
        unsigned hash = RE4CMP_HASH_START;
        for (const auto& v : *arrays[a]) {
            for (float f : v) {
                hash = re4cmp_hash_u32(hash, re4cmp_float_bits(f));
            }
        }
        o.u(arrays[a]->size(), "%s.n", names[a]);
        o.u(hash, "%s.hash", names[a]);
    }
    unsigned hash = RE4CMP_HASH_START;
    for (std::size_t i = 0; i < s.polygons.size(); ++i) {
        const auto& p = s.polygons[i];
        const unsigned long fields[9] = { p.vertices[0], p.vertices[1], p.vertices[2], p.normal, p.edges[0],
                                          p.edges[1],    p.edges[2],    p.reserved,    p.attributes };
        static const char* const kNames[9] = { "v0", "v1", "v2", "n", "e0", "e1", "e2", "res", "attr" };
        for (unsigned k = 0; k < 9; ++k) {
            hash = re4cmp_hash_u32(hash, unsigned(fields[k]));
            if (i < RE4CMP_LIST) {
                o.u(fields[k], "poly.%u.%s", unsigned(i), kNames[k]);
            }
        }
    }
    o.u(s.polygons.size(), "poly.n");
    o.u(hash, "poly.hash");
    o.u(s.floor, "floor");
    o.u(s.slope, "slope");
    o.u(s.wall, "wall");
    o.u(s.blocks, "blocks");
    o.u(s.blockReferences, "refs");
    hash = RE4CMP_HASH_START;
    for (auto poly : s.wallPolygonOrder) {
        hash = re4cmp_hash_u32(hash, unsigned(poly));
    }
    o.u(s.wallPolygonOrder.size(), "wallorder.n");
    o.u(hash, "wallorder.hash");
}

// ---- AEV ----------------------------------------------------------------------

void doorFacts(const Re4Out& a, const re4::RoomDoor& d, unsigned kind)
{
    a.u(kind, "kind");
    a.u(d.no, "no");
    a.u(d.enabled, "en");
    a.u(d.hitType, "hit");
    a.u(d.raw[0x38], "trg");  // the reader normalises ladders and typewriters
    a.u(d.targetType, "tgt");
    a.u(d.taskLevel, "lvl");
    a.u(d.parameterPointer, "param");
    a.u(d.functionPointer, "func");
    a.u(d.raw[0x44], "prio");
    a.u(d.kind, "skind");
    a.u(d.waitingType, "wtype");
    a.u(d.waitingNo, "wno");
    a.i(d.hitDirection, "hdir");
    a.i(d.hitOpenAngle, "hopen");
    a.u(d.parentPointer, "parent");
    a.u(d.country, "country");
    a.u(d.area.enabled, "a.en");
    a.u(d.area.type, "a.type");
    a.f(float(d.area.floor), "a.floor");
    a.f(float(d.area.height), "a.height");
    a.f(float(d.area.radius), "a.radius");
    for (unsigned j = 0; j < 4; ++j) {
        a.f(float(d.area.points[j][0]), "a.p%u.x", j);
        a.f(float(d.area.points[j][1]), "a.p%u.z", j);
    }
}

void aevFacts(const Re4Out& o, const Bytes& b)
{
    re4::RoomFlow flow(0, b);
    o.u(flow.recordCount(), "count");
    for (const auto& d : flow.doors()) {
        Re4Out a = o.sub("rec.%u.", unsigned(d.recordIndex));
        doorFacts(a, d, 1);
        for (unsigned k = 0; k < 3; ++k) {
            a.f(float(d.destination[k]), "dest.%u", k);
        }
        a.f(float(d.destinationHeading), "head");
        a.u(d.destinationRoom, "room");
        a.u(d.keyId, "key");
        a.u(d.keyFlag, "keyflg");
        a.u(d.exitFunctionPointer, "exitfn");
        a.u(d.destinationPart, "part");
        a.u(d.fadeEffect, "fade");
        a.u(d.exitParameterPointer, "exitparam");
    }
    for (const auto& l : flow.ladders()) {
        Re4Out a = o.sub("rec.%u.", unsigned(l.trigger.recordIndex));
        doorFacts(a, l.trigger, 0x10);
        for (unsigned k = 0; k < 3; ++k) {
            a.f(float(l.position[k]), "dest.%u", k);
        }
        a.f(float(l.heading), "head");
        a.i(l.level, "level");
        a.u(l.type, "ltype");
        a.u(l.posSet, "posset");
        for (unsigned k = 0; k < 3; ++k) {
            a.u(l.cameraCuts[k], "cam%u", k);
        }
    }
    for (const auto& t : flow.typewriters()) {
        doorFacts(o.sub("rec.%u.", unsigned(t.recordIndex)), t, 8);
    }
}

// ---- LIT / CAM -------------------------------------------------------------------

void litFacts(const Re4Out& o, const Bytes& b)
{
    re4::RoomLighting l(b);
    o.u(l.sourceVersion, "version");
    o.u(l.cuts.size(), "cuts.n");
    for (std::size_t c = 0; c < l.cuts.size(); ++c) {
        Re4Out cut = o.sub("cut.%u.", unsigned(c));
        bool unavailable = false;
        for (auto u : l.unavailableCuts) {
            unavailable = unavailable || u == c;
        }
        if (unavailable) {
            cut.s("the reader marks the cut unavailable", "skip");
            continue;
        }
        if (!l.present[c]) {
            cut.u(0, "present");
            continue;
        }
        const auto& k = l.cuts[c];
        cut.u(1, "present");
        cut.u(color(k.screenAmbient), "amb.scr");
        cut.u(color(k.actorAmbient), "amb.act");
        cut.u(color(k.effectAmbient), "amb.eff");
        cut.u(k.fog.type, "fog.type");
        cut.f(k.fog.start, "fog.start");
        cut.f(k.fog.end, "fog.end");
        cut.u(color(k.fog.color), "fog.clr");
        cut.u(k.mirrorFog.type, "mfog.type");
        cut.u(color(k.mirrorFog.color), "mfog.clr");
        if (k.mirrorFog.type & 7) {
            cut.f(k.mirrorFog.start, "mfog.start");
            cut.f(k.mirrorFog.end, "mfog.end");
        }
        cut.i(k.focusDepth, "focus.depth");
        cut.u(k.focusLevel, "focus.level");
        cut.u(k.focusMode, "focus.mode");
        cut.u(k.blurRate, "blur.rate");
        cut.u(k.blurType, "blur.type");
        cut.i(k.blurPower, "blur.power");
        cut.f(k.lodBias, "lodbias");
        cut.u(k.lights.size(), "lights.n");
        for (std::size_t i = 0; i < k.lights.size(); ++i) {
            const auto& x = k.lights[i];
            Re4Out li = cut.sub("light.%u.", unsigned(i));
            if ((x.flags & 3) != 3) {
                li.u(0, "active");
                continue;
            }
            li.u(x.flags, "flags");
            li.u(x.type, "type");
            li.u(x.id, "id");
            li.u(x.enableMask, "mask");
            li.u(x.parent, "parent");
            li.u(x.parentNo, "parentno");
            li.u(x.kind, "kind");
            li.u(x.attribute, "attr");
            for (unsigned a = 0; a < 3; ++a) {
                li.f(x.position[a], "pos.%u", a);
            }
            li.f(x.radius, "radius");
            li.u(color(x.color), "clr");
            li.f(x.intensity, "intensity");
            if (x.type >= 3 && x.type <= 6) {
                for (unsigned a = 0; a < 3; ++a) {
                    li.f(x.direction[a], "dir.%u", a);
                }
            } else {
                li.f(x.direction[0], "dir.0");
            }
            if (x.type == 3 || x.type == 6) {
                li.f(x.cutoff, "cutoff");
                li.f(x.edge, "edge");
            } else if (x.type == 5) {
                li.u(x.directionFlags, "dflags");
                li.f(x.edge, "edge");
            } else if (x.type == 4) {
                for (unsigned a = 0; a < 6; ++a) {
                    li.f(x.customAttenuation[a], "att.%u", a);
                }
            }
            if (x.id == 4) {
                const auto& s = x.shadow;
                li.u(s.flags, "sh.flags");
                li.u(s.kind, "sh.kind");
                li.u(s.texture, "sh.tex");
                li.i(s.angleX, "sh.ax");
                li.i(s.angleY, "sh.ay");
                li.u(s.fovy, "sh.fovy");
                li.u(s.selfLevel, "sh.self");
                li.u(s.softLevel, "sh.soft");
                li.u(s.fovySub, "sh.fovysub");
                if (s.flags & 2) {
                    for (unsigned a = 0; a < 3; ++a) {
                        li.f(s.sourcePosition[a], "sh.src.%u", a);
                    }
                }
            }
        }
    }
}

// CAM areas are read by the lighting (RoomLighting::readCameraAreas), so the
// room's LIT is parsed first.
void camFacts(const Re4Out& o, const Bytes& lit, const Bytes& cam)
{
    re4::RoomLighting l(lit);
    l.readCameraAreas(cam);
    o.u(l.areas.size(), "areas.n");
    for (std::size_t i = 0; i < l.areas.size(); ++i) {
        const auto& a = l.areas[i];
        Re4Out r = o.sub("area.%u.", unsigned(i));
        r.u(a.flags, "flags");
        r.u(a.number, "no");
        r.u(a.attributes, "attr");
        r.u(a.character, "char");
        if (a.attributes & 0x40) {
            r.f(a.direction, "dir");
        }
        r.f(a.height, "height");
        r.f(a.floor, "floor");
        unsigned hash = RE4CMP_HASH_START;
        for (const auto& v : a.vertices) {
            for (float f : v) {
                hash = re4cmp_hash_u32(hash, re4cmp_float_bits(f));
            }
        }
        r.u(a.vertices.size(), "v.n");
        r.u(hash, "v.hash");
    }
}

// ---- curves (EventCurve) -------------------------------------------------------------

void curveFacts(const Re4Out& o, const Bytes& b)
{
    re4::EventCurve curve(b);
    o.u(curve.maxFrame(), "max");
    o.u(curve.channels().size(), "ch.n");
    static const unsigned kWidths[16][3] = { { 4, 4, 4 }, { 4, 2, 2 }, { 4, 1, 1 }, {}, { 2, 4, 4 }, { 2, 2, 2 },
                                             { 2, 1, 1 }, {},          { 1, 4, 4 }, { 1, 2, 2 }, { 1, 1, 1 }, {},
                                             {},          {},          {},          { 4, 0, 0 } };
    for (std::size_t c = 0; c < curve.channels().size(); ++c) {
        const auto& ch = curve.channels()[c];
        Re4Out co = o.sub("ch.%u.", unsigned(c));
        co.u(ch.kind, "kind");
        co.u(ch.index, "idx");
        // the reader's key copy: per axis u16 count, u16 frames, values
        // (little-endian, as the motion64 evaluator reads them)
        const auto& k = ch.keys;
        std::size_t p = 0;
        auto le = [&](unsigned width) {
            std::uint32_t v = 0;
            for (unsigned i = 0; i < width && p + i < k.size(); ++i) {
                v |= std::uint32_t(k[p + i]) << (8 * i);
            }
            p += width;
            return v;
        };
        const unsigned* w = kWidths[ch.kind >> 12];
        for (unsigned axis = 0; axis < 3; ++axis) {
            unsigned count = le(2);
            unsigned fh = RE4CMP_HASH_START, vh = RE4CMP_HASH_START;
            for (unsigned i = 0; i < count; ++i) {
                fh = re4cmp_hash_u32(fh, le(2));
            }
            for (unsigned i = 0; i < count; ++i) {
                for (unsigned x = 0; x < 3; ++x) {
                    if (w[x]) {
                        vh = re4cmp_hash_u32(vh, le(w[x]));
                    }
                }
            }
            co.u(count, "ax%u.n", axis);
            co.u(fh, "ax%u.fh", axis);
            co.u(vh, "ax%u.vh", axis);
        }
    }
}

std::string tagKey(const std::array<std::uint8_t, 4>& tag)
{
    std::string s;
    for (unsigned i = 0; i < 3; ++i) {
        auto c = tag[i];
        s += c >= 32 && c < 127 && c != '.' ? char(c) : '_';
    }
    return s;
}

bool hasExt(const std::string& name, const char* ext)
{
    auto dot = name.rfind('.');
    return dot != std::string::npos && strcasecmp(name.c_str() + dot, ext) == 0;
}

}  // namespace

void appArchive(const Bytes& bytes, FactList& out)
{
    re4::RoomArchive arc(bytes);
    Re4Facts facts = { sinkList, &out };
    Re4Out o = re4cmp_out(&facts);
    const auto& entries = arc.entries();
    o.u(entries.size(), "arc.count");
    const re4::ArchiveEntry* lit = arc.find("LIT");
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        std::string tag = tagKey(e.tag);
        o.s(tag.c_str(), "arc.%u.tag", unsigned(i));
        o.u(e.offset, "arc.%u.offset", unsigned(i));
        Re4Out u = o.sub("%s%u.", tag.c_str(), unsigned(e.occurrence));
        Bytes data = arc.copy(i);
        if (tag == "SMD") {
            guarded(u, [&] { smdFacts(u, data); });
        } else if (tag == "SMX") {
            guarded(u, [&] { smxFacts(u, data); });
        } else if (tag == "SAT" || tag == "EAT") {
            guarded(u, [&] { satFacts(u, data); });
        } else if (tag == "AEV") {
            guarded(u, [&] { aevFacts(u, data); });
        } else if (tag == "LIT") {
            guarded(u, [&] { litFacts(u, data); });
        } else if (tag == "CAM") {
            if (!lit) {
                u.s("no LIT in the archive (the app reads CAM areas with the lighting)", "skip");
            } else {
                Bytes litBytes = arc.copy(std::size_t(lit - entries.data()));
                guarded(u, [&] { camFacts(u, litBytes, data); });
            }
        } else if (tag == "BIN") {
            binFacts(u, data);
        } else if (tag == "FCV") {
            guarded(u, [&] { curveFacts(u, data); });
        }
    }
}

void appEsl(const Bytes& bytes, FactList& out)
{
    re4::EnemySpawns spawns(bytes);
    Re4Facts facts = { sinkList, &out };
    Re4Out o = re4cmp_out(&facts);
    for (const auto& s : spawns.records()) {
        Re4Out r = o.sub("rec.%u.", unsigned(s.listIndex));
        r.u(s.beFlags, "be");
        r.u(s.id, "id");
        r.u(s.type, "type");
        r.u(s.setNo, "set");
        r.u(s.flags, "flag");
        r.u(s.hp, "hp");
        r.u(s.emsetNo, "emset");
        r.u(s.character, "char");
        for (unsigned k = 0; k < 3; ++k) {
            r.d(s.position[k], "pos.%u", k);
            r.d(s.rotation[k], "rot.%u", k);
        }
        r.u(s.room, "room");
        r.d(s.guardRadius, "guard");
    }
}

void appRoomInfo(const Bytes& bytes, FactList& out)
{
    re4::RoomInfo info(bytes);
    Re4Facts facts = { sinkList, &out };
    Re4Out o = re4cmp_out(&facts);
    o.u(info.stageCount(), "stages");
    for (std::size_t i = 0; i < info.starts().size(); ++i) {
        const auto& s = info.starts()[i];
        Re4Out e = o.sub("start.%u.", unsigned(i));
        e.u(s.flags, "flags");
        e.u(s.room, "room");
        e.u(s.point, "point");
        for (unsigned k = 0; k < 3; ++k) {
            e.f(float(s.position[k]), "pos.%u", k);
        }
        e.f(float(s.heading), "head");
        e.s(s.name.c_str(), "name");
        e.s(s.person.c_str(), "person");
        e.s(s.person2.c_str(), "person2");
    }
}

void appEvd(const Bytes& bytes, FactList& out)
{
    re4::EventData evd{ Bytes(bytes) };
    Re4Facts facts = { sinkList, &out };
    Re4Out o = re4cmp_out(&facts);
    o.s(evd.name().c_str(), "name");
    o.u(evd.soundFlags(), "snd");
    const auto& packets = evd.packets();
    std::size_t start = packets.empty() ? 0 : packets.front().offset;
    std::size_t stop = packets.empty() ? 0 : packets.back().offset + packets.back().bytes.size();
    o.u(start, "pac.start");
    o.u(stop - start, "pac.size");
    const auto& assets = evd.assets();
    o.u(assets.size(), "a.n");
    for (std::size_t i = 0; i < assets.size(); ++i) {
        const auto& a = assets[i];
        o.s(a.name.c_str(), "a.%u.name", unsigned(i));
        o.u(a.offset, "a.%u.ofs", unsigned(i));
        o.u(a.extent, "a.%u.ext", unsigned(i));
        Re4Out ao = o.sub("asset.%s.", a.name.c_str());
        Bytes data = evd.asset(a.name);
        if (hasExt(a.name, ".fcv")) {
            guarded(ao, [&] { curveFacts(ao, data); });
        } else if (hasExt(a.name, ".lit")) {
            guarded(ao, [&] { litFacts(ao, data); });
        } else if (hasExt(a.name, ".bin")) {
            binFacts(ao, data);
        }
    }
    for (std::size_t i = 0; i < packets.size(); ++i) {
        const auto& p = packets[i];
        Re4Out pk = o.sub("pk.%u.", unsigned(i));
        pk.u(p.type, "type");
        pk.u(p.flags, "flags");
        pk.i(p.cut, "cut");
        pk.i(p.frame, "frame");
        pk.u(p.bytes.size(), "len");
        pk.u(p.offset, "off");
        switch (p.type) {
        case 8:   // CamDammy
        case 15:  // Str
        case 16:  // Se
        case 17:  // Mes
        case 18:  // Func
        case 29:  // Fade
            for (unsigned x = 0; x < 3 && 16 + (x + 1) * 4 <= p.bytes.size(); ++x) {
                pk.i(p.integer(16 + x * 4), "v%u", x);
            }
            break;
        case 9:  // Pos: pos[3], rot[3], partsNo
            for (unsigned x = 0; x < 7; ++x) {
                pk.i(p.integer(0x28 + x * 4), "p%u", x);
            }
            break;
        case 13:  // Esp: type
            pk.i(p.integer(0x1C), "esp");
            break;
        default:
            break;
        }
    }
    o.u(packets.size(), "packets.n");
    o.u(evd.cutFrames().size(), "cuts.n");
    for (std::size_t i = 0; i < evd.cutFrames().size(); ++i) {
        o.u(evd.cutFrames()[i], "cut.%u", unsigned(i));
    }
    o.u(evd.totalFrames(), "total");
}

Bytes appUnpackYz2(const Bytes& source)
{
    return re4::DecodeYz2(source).bytes;
}

}  // namespace re4cmp
