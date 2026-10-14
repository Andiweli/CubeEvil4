// re4_format_compare: the Formatumwandler (port/whole/runtime/formats) against
// the checked big-endian readers of RE4-Android-Decomp.
//
//   re4_format_compare                    synthetic samples of every compared
//                                         format (no disc data; cloud test)
//   re4_format_compare --data DIR [--room r100 ...] [-v]
//                                         the disc files: room archives of the
//                                         rooms (St1/r100.das), etc/*.esl,
//                                         debug/roominfo.dat, the rooms' events
//                                         (*/r100*.evd), etc/core.das, em/*.drs
//
// For every file both sides report the fields they read (facts.h): the app
// side with its readers on the big-endian bytes (app_side.cpp), the game side
// after converting a copy to host order, through the original structures
// (game_side.cpp). Every key must have the same value on both sides. A part
// the app's reader rejects is listed as "nicht vergleichbar" (with the
// reader's message); in the synthetic test that is a failure as well.
//
// Exit code: 0 all equal, 1 differences (or an incomplete conversion), 2 usage.
#include "app_side.h"
#include "facts.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace re4cmp;

namespace {

enum Kind { kArchive, kEsl, kEvd, kRoomInfo };

struct Input {
    std::string label;
    Kind kind;
    Bytes bytes;
    std::string error;  // the file could not be prepared (missing, container)
};

struct Result {
    std::size_t equal = 0;
    std::vector<std::string> diffs;
    std::vector<std::string> skipped;
    std::vector<std::string> incomplete;
    std::string appError;
    bool gameRejected = false;
};

void sinkList(void* ctx, const char* key, const char* value)
{
    static_cast<FactList*>(ctx)->emplace_back(key, value);
}

const char* kindName(Kind k)
{
    switch (k) {
    case kArchive:
        return "Archiv";
    case kEsl:
        return "Gegnerliste";
    case kEvd:
        return "Ereignis";
    case kRoomInfo:
        return "Raumsprungtabelle";
    }
    return "?";
}

bool startsWith(const std::string& s, const std::string& p)
{
    return s.compare(0, p.size(), p) == 0;
}

Result compare(const Input& in)
{
    Result r;
    FactList app, game;
    try {
        switch (in.kind) {
        case kArchive:
            appArchive(in.bytes, app);
            break;
        case kEsl:
            appEsl(in.bytes, app);
            break;
        case kEvd:
            appEvd(in.bytes, app);
            break;
        case kRoomInfo:
            appRoomInfo(in.bytes, app);
            break;
        }
    } catch (const std::exception& e) {
        r.appError = e.what();
        return r;
    }
    Re4Facts facts = { sinkList, &game };
    int ok = 0;
    switch (in.kind) {
    case kArchive:
        ok = re4cmp_game_archive(in.bytes.data(), unsigned(in.bytes.size()), in.label.c_str(), &facts);
        break;
    case kEsl:
        ok = re4cmp_game_esl(in.bytes.data(), unsigned(in.bytes.size()), in.label.c_str(), &facts);
        break;
    case kEvd:
        ok = re4cmp_game_evd(in.bytes.data(), unsigned(in.bytes.size()), in.label.c_str(), &facts);
        break;
    case kRoomInfo:
        ok = re4cmp_game_roominfo(in.bytes.data(), unsigned(in.bytes.size()), in.label.c_str(), &facts);
        break;
    }
    r.gameRejected = ok == 0;
    std::map<std::string, std::string> a, g;
    for (const auto& kv : app) {
        if (!a.emplace(kv).second) {
            r.diffs.push_back(kv.first + ": doppelt auf der App-Seite");
        }
    }
    for (const auto& kv : game) {
        if (kv.first.find("!incomplete") != std::string::npos) {
            r.incomplete.push_back(kv.first.substr(0, kv.first.find("!incomplete")));
            continue;
        }
        if (!g.emplace(kv).second) {
            r.diffs.push_back(kv.first + ": doppelt auf der Kern-Seite");
        }
    }
    // parts the app's reader rejected: left out on both sides
    std::vector<std::string> prefixes;
    for (const auto& kv : a) {
        if (kv.first.size() >= 4 && kv.first.compare(kv.first.size() - 4, 4, "skip") == 0) {
            std::string prefix = kv.first.substr(0, kv.first.size() - 4);
            prefixes.push_back(prefix);
            r.skipped.push_back((prefix.empty() ? std::string("(Datei)") : prefix) + " (App: " + kv.second + ")");
        }
    }
    auto skipped = [&](const std::string& key) {
        for (const auto& p : prefixes) {
            if (startsWith(key, p)) {
                return true;
            }
        }
        return false;
    };
    for (auto it = a.begin(); it != a.end();) {
        it = skipped(it->first) ? a.erase(it) : std::next(it);
    }
    for (auto it = g.begin(); it != g.end();) {
        it = skipped(it->first) ? g.erase(it) : std::next(it);
    }
    for (const auto& kv : a) {
        auto it = g.find(kv.first);
        if (it == g.end()) {
            r.diffs.push_back(kv.first + ": App " + kv.second + " · Kern (fehlt)");
        } else if (it->second != kv.second) {
            r.diffs.push_back(kv.first + ": App " + kv.second + " · Kern " + it->second);
        } else {
            r.equal++;
        }
    }
    for (const auto& kv : g) {
        if (!a.count(kv.first)) {
            r.diffs.push_back(kv.first + ": App (fehlt) · Kern " + kv.second);
        }
    }
    return r;
}

// ---- synthetic samples (big-endian, as on the disc) -------------------------------

struct Be {
    Bytes b;

    std::size_t size() const { return b.size(); }
    void pad(std::size_t n)
    {
        while (b.size() % n) {
            b.push_back(0);
        }
    }
    void zeros(std::size_t n) { b.insert(b.end(), n, 0); }
    void u8(unsigned v) { b.push_back(std::uint8_t(v)); }
    void u16(unsigned v)
    {
        u8(v >> 8);
        u8(v);
    }
    void u32(std::uint32_t v)
    {
        u16(v >> 16);
        u16(v & 0xFFFF);
    }
    void f32(float f)
    {
        std::uint32_t v;
        std::memcpy(&v, &f, 4);
        u32(v);
    }
    void vec(float x, float y, float z)
    {
        f32(x);
        f32(y);
        f32(z);
    }
    void text(const char* s, std::size_t n)
    {
        std::size_t len = std::strlen(s);
        for (std::size_t i = 0; i < n; i++) {
            u8(i < len ? s[i] : 0);
        }
    }
    void put32(std::size_t at, std::uint32_t v)
    {
        b[at] = std::uint8_t(v >> 24);
        b[at + 1] = std::uint8_t(v >> 16);
        b[at + 2] = std::uint8_t(v >> 8);
        b[at + 3] = std::uint8_t(v);
    }
    void put16(std::size_t at, unsigned v)
    {
        b[at] = std::uint8_t(v >> 8);
        b[at + 1] = std::uint8_t(v);
    }
    void append(const Bytes& other) { b.insert(b.end(), other.begin(), other.end()); }
};

// A model (cModelData version 0x20030818): two parts, four vertices, one
// display list, skinning weights (cWeight or cWeightExt), blend / flip tables.
Bytes makeBin(bool ext, bool colored, bool smallNormals)
{
    Be m;
    m.zeros(0x60);  // header (0x48), padded
    std::size_t head = m.size();
    m.u8(0), m.u8(0xFF), m.u8(0), m.u8(0), m.vec(0, 95.5f, 0);
    m.u8(1), m.u8(0), m.u8(0), m.u8(0), m.vec(12.25f, 40, -3);
    std::size_t vtx = m.size();
    const int pos[4][3] = { { 160, -32, 4000 }, { -2, 32767, -32768 }, { 0, 0, 1 }, { 1200, -1200, 77 } };
    for (unsigned i = 0; i < 4; i++) {
        m.u16(unsigned(pos[i][0]) & 0xFFFF), m.u16(unsigned(pos[i][1]) & 0xFFFF), m.u16(unsigned(pos[i][2]) & 0xFFFF);
        m.u16(i & 1);  // weight index
    }
    std::size_t nrm = m.size();
    for (unsigned i = 0; i < 4; i++) {
        if (smallNormals) {
            m.u8(i == 0 ? 64 : 0), m.u8(i == 1 ? 0xC0 : 0), m.u8(i >= 2 ? 45 : 0), m.u8(i & 1);
        } else {
            m.u16(i == 0 ? 16384 : 0), m.u16(i == 1 ? 0xC000 : 0), m.u16(i >= 2 ? 11585 : 0), m.u16(i & 1);
        }
    }
    m.pad(16);
    std::size_t clr = 0;
    if (colored) {
        clr = m.size();
        for (unsigned i = 0; i < 4; i++) {
            m.u8(10 * i), m.u8(200), m.u8(30 + i), m.u8(255);
        }
    }
    std::size_t tex = m.size();
    for (unsigned i = 0; i < 4; i++) {
        m.u16(colored ? (0xFF00 + i * 0x40) & 0xFFFF : 0x8000 + i * 0x1000), m.u16(colored ? 0x0180 : 0x4000 - i);
    }
    std::size_t wgt = m.size();
    unsigned nWeights = ext ? 256 : 2;
    for (unsigned i = 0; i < nWeights; i++) {
        unsigned n = ext ? 1 + i % 3 : 1 + i;
        if (ext) {
            m.u16(i % 2), m.u16(1 - i % 2), m.u16(0), m.u16(n), m.u8(n == 1 ? 100 : 30), m.u8(30), m.u8(0), m.u8(0);
        } else {
            m.u8(0), m.u8(1), m.u8(0), m.u8(n), m.u8(n == 1 ? 100 : 60), m.u8(40), m.u8(0), m.u8(0);
        }
    }
    m.pad(16);
    std::size_t blend = m.size();
    m.u32(1), m.u16(0), m.u16(1), m.u16(0), m.u16(50);
    m.pad(16);
    std::size_t flip = m.size();
    m.u32(2), m.u16(1), m.u16(0);
    m.pad(32);
    std::size_t parts = m.size();
    m.zeros(32);  // cPolyHeader
    m.b[parts + 0x0B] = 0x04;
    m.b[parts + 0x0C] = 0x00;
    m.put32(parts + 0x18, 32);  // buff_size
    m.put32(parts + 0x1C, 1);   // nPoly
    std::size_t stream = m.size();
    m.u8(0x90), m.u16(3);
    for (unsigned i = 0; i < 3; i++) {
        m.u16(i), m.u16(i);
        if (colored) {
            m.u16(i);
        }
        m.u16(3 - i);
    }
    while (m.size() < stream + 32) {
        m.u8(0);
    }
    m.put32(0x00, std::uint32_t(head));
    m.put32(0x0C, std::uint32_t(clr));
    m.put32(0x10, std::uint32_t(tex));
    m.put32(0x14, std::uint32_t(wgt));
    m.b[0x18] = ext ? 0 : 2;
    m.b[0x19] = 2;
    m.put16(0x1A, 1);
    m.put32(0x1C, std::uint32_t(parts));
    m.put32(0x20, (colored ? 0x80000000u : 0) | (smallNormals ? 0x20000000u : 0));
    m.put32(0x24, 1);
    m.b[0x28] = 4;
    m.put16(0x2A, ext ? 256 : 0);
    m.put32(0x30, std::uint32_t(vtx));
    m.put32(0x34, std::uint32_t(nrm));
    m.put16(0x38, 4);
    m.put16(0x3A, 4);
    m.put32(0x3C, 0x20030818);
    m.put32(0x40, std::uint32_t(blend));
    m.put32(0x44, std::uint32_t(flip));
    return m.b;
}

// TPL with one 8x4 I8 image
Bytes makeTpl()
{
    Be t;
    t.u32(0x0020AF30), t.u32(1), t.u32(0x0C);
    t.u32(0x20), t.u32(0);  // descriptor: texture header, no CLUT
    t.pad(32);
    t.u16(4), t.u16(8), t.u32(1), t.u32(0x60), t.u32(0), t.u32(0), t.u32(1), t.u32(1), t.f32(0), t.u8(0), t.u8(0), t.u8(0),
        t.u8(0);
    t.pad(32);
    while (t.size() < 0x60) {
        t.u8(0);
    }
    for (unsigned i = 0; i < 32; i++) {
        t.u8(i * 7);
    }
    return t.b;
}

Bytes makeSmd()
{
    Be s;
    s.u8(0x40), s.u8(1), s.u16(2);
    s.u32(0), s.u32(0), s.u32(0);  // table offsets, set below
    s.u32(1), s.u32(2);            // one group of two works
    const float place[2][9] = { { 100, 200, -300, 0, 1.5f, 0, 1, 1, 1 }, { -1, -2, -3, 0.25f, 0, 0, 2, 2, 0.5f } };
    for (unsigned i = 0; i < 2; i++) {
        for (float f : place[i]) {
            s.f32(f);
        }
        if (i == 0) {
            s.u8(0), s.u8(0), s.u8(0xFF), s.u8(3);
        } else {
            s.u8(0xFF), s.u8(0xFF), s.u8(0xFF), s.u8(0xFF);
        }
        s.f32(50);  // Radius
        s.u32(0);   // Flag
        s.zeros(0x44 - 0x30);
        s.u32(i == 0 ? 0x00000021 : 0);
    }
    s.pad(32);
    std::size_t binTbl = s.size();
    s.u32(0x20);
    s.pad(32);
    s.append(makeBin(false, true, false));
    s.pad(32);
    std::size_t tplTbl = s.size();
    s.u32(0x20);
    s.pad(32);
    s.append(makeTpl());
    s.put32(4, std::uint32_t(binTbl));
    s.put32(8, std::uint32_t(tplTbl));
    return s.b;
}

Bytes makeSmx()
{
    Be x;
    x.u8(0x10), x.u8(3), x.u8(0), x.u8(0), x.u32(0), x.u32(0), x.u32(0);
    for (unsigned i = 0; i < 3; i++) {
        unsigned type = i == 0 ? 1 : i == 1 ? 2 : 0;
        x.u8(3 + i), x.u8(type), x.u8(2), x.u8(i), x.u32(0x0F), x.u32(0x2 << i), x.u32(0x80604020 + i);
        std::size_t free = x.size();
        if (type == 1) {
            x.vec(1.5f, -2.0f, 0.25f), x.u8(1);
        } else if (type == 2) {
            for (unsigned k = 0; k < 13; k++) {
                x.f32(0.5f * k - 3);
            }
        }
        while (x.size() < free + 116) {
            x.u8(0);
        }
        x.u32(0x11223344 + i), x.f32(0.5f), x.f32(-0.25f * i);
    }
    return x.b;
}

Bytes makeSatFile(float shift)
{
    Be s;
    s.u8(0xFF), s.u8(0), s.u16(4), s.u16(2), s.u16(3), s.u16(0), s.u16(2), s.u16(1), s.u16(0), s.u16(1), s.u16(2);
    const float v[4][3] = { { 0, 0, 0 }, { 1000, 0, 0 }, { 0, 0, 1000 }, { 1000, 500, 1000 } };
    for (const auto& p : v) {
        s.vec(p[0] + shift, p[1], p[2]);
    }
    s.vec(0, 1, 0), s.vec(0.707f, 0, -0.707f);
    s.vec(1, 0, 0), s.vec(0, 0, 1), s.vec(-0.5f, 0, 0.5f);
    s.u16(0), s.u16(1), s.u16(2), s.u16(0), s.u16(0), s.u16(1), s.u16(2), s.u16(0), s.u32(0x00000001);
    s.u16(1), s.u16(2), s.u16(3), s.u16(1), s.u16(1), s.u16(2), s.u16(0x2000), s.u16(0), s.u32(0x80000002);
    // root block (child follows), then the leaf with both polygons
    s.vec(0, 0, 0), s.vec(1000, 500, 1000), s.u16(0), s.u16(0), s.u16(0), s.u16(1), s.u32(0);
    s.vec(0, 0, 0), s.vec(1000, 500, 1000), s.u16(1), s.u16(0), s.u16(1), s.u16(0), s.u32(0), s.u16(0), s.u16(1);
    s.pad(4);
    return s.b;
}

Bytes makeEat()
{
    Bytes a = makeSatFile(0), b = makeSatFile(250);
    Be e;
    e.u8(0x81), e.u8(0), e.u8(0), e.u8(2);
    e.u32(12), e.u32(std::uint32_t(12 + a.size()));
    e.append(a);
    e.append(b);
    return e.b;
}

// one SCE_AT_DATA record (0x9C)
void sceRecord(Be& a, unsigned id, unsigned no)
{
    std::size_t r = a.size();
    a.u32(0);
    a.u8(1), a.u8(1), a.u16(0), a.f32(-50), a.f32(1800), a.f32(25);
    const float pts[4][2] = { { 0, 0 }, { 400, 0 }, { 400, 300 }, { 0, 300 } };
    for (const auto& p : pts) {
        a.f32(p[0] + no), a.f32(p[1]);
    }
    a.u8(1), a.u8(id), a.u8(no), a.u8(3), a.u8(id == 1 ? 0x88 : 0x01), a.u8(1), a.u8(2), a.u8(0);
    a.u32(0x100 + no), a.u32(0x200 + no);
    a.u8(7), a.u8(1), a.u8(0), a.u8(0), a.u8(0xD3), a.u8(30), a.u8(4), a.u8(0);
    a.u32(0), a.u16(0xFFFF), a.u8(3), a.u8(1);
    a.zeros(8);
    if (id == 1) {
        a.vec(1500, -10, 2500), a.f32(3.0f), a.u8(1), a.u8(1), a.u8(1), a.u8(5), a.u32(0x80001234), a.u8(2), a.u8(0x10),
            a.u8(3), a.u8(1), a.u32(0x80005678);
    } else if (id == 0x10) {
        a.vec(-700, 120, 33), a.f32(-1.5f), a.u8(0xFD), a.u8(1), a.u8(2), a.u8(1), a.u8(2), a.u8(3);
    } else {
        a.u32(2);
    }
    while (a.size() < r + 0x9C) {
        a.u8(0);
    }
}

Bytes makeAev()
{
    Be a;
    a.text("AEV", 4), a.u16(0x104), a.u16(3), a.zeros(8);
    sceRecord(a, 1, 0);
    sceRecord(a, 0x10, 1);
    sceRecord(a, 8, 2);
    return a.b;
}

void lightWork(Be& l, unsigned type, unsigned id, unsigned parent)
{
    std::size_t w = l.size();
    l.u8(3), l.u8(type), l.u8(id), l.u8(1);
    l.vec(10, 20, 30), l.f32(1000), l.u8(255), l.u8(200), l.u8(100), l.u8(255), l.f32(1.5f);
    l.u8(parent), l.u8(2), l.u8(1), l.u8(0), l.u32(0x00010002), l.u16(40), l.u16(0), l.u32(0);
    if (type == 3) {
        l.vec(0, -1, 0), l.f32(30), l.f32(100);
    } else {
        l.vec(1, 0, 0), l.f32(1), l.f32(0.5f), l.f32(0.25f), l.f32(2), l.f32(3), l.f32(4);
    }
    while (l.size() < w + 0x6C) {
        l.u8(0);
    }
    if (id == 4) {
        l.u16(2), l.u8(1), l.u8(0xFF), l.u16(0xFFD3), l.u16(90), l.u8(40), l.u8(1), l.u8(2), l.u8(0), l.vec(1, 2, 3),
            l.u8(5);
    } else {
        l.f32(0.1f), l.f32(0.2f), l.f32(0.3f), l.f32(0.4f);
    }
    while (l.size() < w + 0x12C) {
        l.u8(0);
    }
}

Bytes makeLit()
{
    Be l;
    l.u16(2), l.u8(0x2C), l.u8(2), l.u32(0x10), l.u32(0);
    l.pad(16);
    std::size_t e = l.size();
    l.u8(40), l.u8(50), l.u8(60), l.u8(255), l.u32(2);
    l.u32(2), l.f32(100), l.f32(5000), l.u8(10), l.u8(20), l.u8(30), l.u8(255);
    l.u32(4), l.f32(50), l.f32(600), l.u8(1), l.u8(2), l.u8(3), l.u8(4);
    l.u32(30000), l.u8(0), l.u8(3), l.u8(1), l.u8(128);
    while (l.size() < e + 0x40) {
        l.u8(0);
    }
    l.u8(1), l.u8(2), l.u8(0), l.u8(0), l.f32(0.25f), l.u8(10);
    while (l.size() < e + 0xF0) {
        l.u8(0);
    }
    l.u8(1), l.u8(0xFE), l.u8(0), l.u8(5), l.u8(1), l.u8(1), l.u8(2), l.u8(3), l.f32(-0.5f);
    l.u8(5), l.u8(6), l.u8(7), l.u8(8), l.u8(9), l.u8(10), l.u8(11), l.u8(12);
    lightWork(l, 3, 4, 0);
    lightWork(l, 4, 2, 7);
    return l.b;
}

Bytes makeCam()
{
    Be c;
    c.text("B404", 4), c.u8(1), c.u8(1), c.u8(1), c.zeros(9);
    c.u8(2), c.zeros(7), c.u32(0x20), c.u32(0x50);
    c.u8(1), c.u8(3), c.u8(0), c.u8(0x40), c.f32(1.25f), c.u8(2), c.u8(0xFF);
    c.zeros(0x20 - 0x0A);
    c.f32(2000), c.f32(-100), c.u32(3), c.u32(0x94);
    c.u8(1), c.u8(3), c.u8(0), c.u8(1), c.vec(0, 100, 0), c.u32(0xB8), c.f32(0.5f);
    c.zeros(8);
    c.u32(2), c.u32(0xBC), c.u32(0xD4), c.u32(0xEC), c.u32(0xF4);
    c.u8(1), c.u8(3), c.u8(0), c.u8(4), c.u8(0), c.u8(0), c.zeros(2), c.u32(30), c.zeros(4);
    c.vec(0, 0, 0), c.vec(500, 0, 0), c.vec(0, 0, 500);
    c.u16(0), c.u16(60);
    c.vec(1, 2, 3), c.vec(4, 5, 6);
    c.vec(7, 8, 9), c.vec(10, 11, 12);
    c.f32(0), c.f32(0.1f);
    c.f32(45), c.f32(50);
    return c.b;
}

// MotionData curve: channel 0 three float columns, channel 1 three s16 columns
Bytes makeCurve(unsigned maxFrame)
{
    Be m;
    m.u16(maxFrame), m.u8(2), m.u16(0x0000), m.u16(0x5004), m.u8(0), m.u8(1);
    m.pad(4);
    m.u32(0);
    std::size_t offs = m.size();
    m.u32(0), m.u32(0);
    std::size_t c0 = m.size();
    for (unsigned axis = 0; axis < 3; axis++) {
        m.u16(2), m.u16(0), m.u16(maxFrame);
        for (unsigned k = 0; k < 2; k++) {
            m.f32(axis * 10.0f + k), m.f32(0.5f), m.f32(-0.5f);
        }
    }
    std::size_t c1 = m.size();
    for (unsigned axis = 0; axis < 3; axis++) {
        m.u16(3), m.u16(0), m.u16(maxFrame / 2), m.u16(maxFrame);
        for (unsigned k = 0; k < 3; k++) {
            m.u16(0x1000 * axis + k), m.u16(0xFFF0), m.u16(7);
        }
    }
    m.pad(4);
    m.put32(offs, std::uint32_t(c0));
    m.put32(offs + 4, std::uint32_t(c1));
    return m.b;
}

Bytes makeArchive(const std::vector<std::pair<const char*, Bytes>>& entries)
{
    Be a;
    a.u32(std::uint32_t(entries.size())), a.u32(0), a.u32(0), a.u32(0);
    std::size_t offs = a.size();
    a.zeros(entries.size() * 4);
    for (const auto& e : entries) {
        a.text(e.first, 4);
    }
    for (std::size_t i = 0; i < entries.size(); i++) {
        a.pad(32);
        a.put32(offs + i * 4, std::uint32_t(a.size()));
        a.append(entries[i].second);
    }
    a.pad(32);
    return a.b;
}

Bytes makeEsl()
{
    Be e;
    for (unsigned i = 0; i < 255; i++) {
        if (i == 0) {
            e.u8(1), e.u8(0x12), e.u8(0), e.u8(1), e.u32(0x100), e.u16(500), e.u8(0), e.u8(2);
            e.u16(0xFF88), e.u16(0), e.u16(3400), e.u16(0), e.u16(0x2000), e.u16(0), e.u16(0x100), e.u16(15), e.zeros(4);
        } else if (i == 1) {
            e.u8(3), e.u8(0x15), e.u8(1), e.u8(2), e.u32(0x80000001), e.u16(65535), e.u8(9), e.u8(0);
            e.u16(0x7FFF), e.u16(0x8000), e.u16(1), e.u16(0xC000), e.u16(1), e.u16(0xFFFF), e.u16(0x101), e.u16(0xFFFE),
                e.zeros(4);
        } else {
            e.zeros(32);
        }
    }
    return e.b;
}

Bytes makeEvd()
{
    Be e;
    e.text("event", 8), e.zeros(0x18), e.text("r100", 8), e.text("s03", 12), e.u32(0x80000001), e.zeros(8);
    e.u32(0x50), e.u32(0), e.u32(2), e.u32(0);
    auto packet = [&](unsigned id, unsigned cut, unsigned frame, unsigned size) {
        e.u32(id), e.u32(id == 9 ? 0x80000000u : 0), e.u16(cut), e.u16(frame), e.u16(size), e.u16(0);
    };
    packet(6, 0, 0, 64), e.text("cam.fcv", 48);
    packet(9, 0, 0, 0x44), e.text("leon", 12), e.text("", 12), e.u32(100), e.u32(0xFFFFFF38), e.u32(300), e.u32(0),
        e.u32(90), e.u32(0), e.u32(0xFFFFFFFF);
    packet(29, 0, 1, 28), e.u32(1), e.u32(2), e.u32(30);
    packet(8, 1, 0, 28), e.u32(5), e.u32(0), e.u32(0);
    packet(13, 1, 2, 36), e.text("eff", 12), e.u32(7), e.u8(0), e.u8(0), e.u8(0), e.u8(2);
    packet(27, 1, 3, 16);
    std::size_t table = e.size();
    e.put32(0x44, std::uint32_t(table - 0x50));
    e.put32(0x4C, std::uint32_t(table));
    e.text("cam.fcv", 48), e.u32(0), e.zeros(12);
    e.text("light.lit", 48), e.u32(0), e.zeros(12);
    e.pad(32);
    e.put32(table + 48, std::uint32_t(e.size()));
    e.append(makeCurve(10));
    e.pad(32);
    e.put32(table + 64 + 48, std::uint32_t(e.size()));
    e.append(makeLit());
    return e.b;
}

Bytes makeRoomInfo()
{
    Be r;
    r.u32(2), r.u32(0), r.u32(12);
    r.u32(2);
    r.u16(1), r.u16(0x100), r.vec(100, 0, -2000), r.f32(3.14f), r.u32(0), r.u32(0), r.u32(0);
    r.u16(0), r.u16(0x100), r.vec(-5, 6, 7), r.f32(-1), r.u32(0), r.u32(0), r.u32(0);
    std::size_t t1 = r.size();
    r.text("r100 start", 11);
    std::size_t t2 = r.size();
    r.text("leon", 5);
    std::size_t t3 = r.size();
    r.text("r100 bridge", 12);
    r.pad(4);
    r.put32(16 + 20, std::uint32_t(t1));
    r.put32(16 + 24, std::uint32_t(t2));
    r.put32(16 + 32 + 20, std::uint32_t(t3));
    return r.b;
}

std::vector<Input> syntheticInputs()
{
    std::vector<Input> in;
    in.push_back({ "synthetisch: Raumarchiv",
                   kArchive,
                   makeArchive({ { "SMD", makeSmd() },
                                 { "SMX", makeSmx() },
                                 { "SAT", makeSatFile(0) },
                                 { "EAT", makeEat() },
                                 { "AEV", makeAev() },
                                 { "LIT", makeLit() },
                                 { "CAM", makeCam() },
                                 { "BIN", makeBin(true, false, true) },
                                 { "BIN", makeBin(false, true, false) },
                                 { "FCV", makeCurve(30) } }),
                   "" });
    in.push_back({ "synthetisch: Gegnerliste", kEsl, makeEsl(), "" });
    in.push_back({ "synthetisch: Ereignis", kEvd, makeEvd(), "" });
    in.push_back({ "synthetisch: Raumsprungtabelle", kRoomInfo, makeRoomInfo(), "" });
    return in;
}

// ---- disc files ------------------------------------------------------------------

bool readFile(const std::string& path, Bytes& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

bool isDir(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::vector<std::string> listDir(const std::string& dir)
{
    std::vector<std::string> names;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            if (std::strcmp(e->d_name, ".") != 0 && std::strcmp(e->d_name, "..") != 0) {
                names.push_back(e->d_name);
            }
        }
        closedir(d);
    }
    std::sort(names.begin(), names.end());
    return names;
}

// Case-insensitive path below `root` ("st1/r100.das"); empty when missing.
std::string findPath(const std::string& root, const std::string& rel)
{
    std::string path = root;
    std::size_t start = 0;
    while (start <= rel.size()) {
        std::size_t slash = rel.find('/', start);
        std::string part = rel.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        std::string found;
        for (const auto& name : listDir(path)) {
            if (strcasecmp(name.c_str(), part.c_str()) == 0) {
                found = name;
                break;
            }
        }
        if (found.empty()) {
            return "";
        }
        path += "/" + found;
        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }
    return path;
}

bool endsWithNoCase(const std::string& s, const char* ext)
{
    std::size_t n = std::strlen(ext);
    return s.size() >= n && strcasecmp(s.c_str() + s.size() - n, ext) == 0;
}

// The files folder of the first disc: <dir>/disc0/files (port layout),
// <dir>/disc1/files (app layout) or <dir>/files.
std::string filesFolder(const std::string& dir)
{
    for (const char* sub : { "/disc0/files", "/disc1/files", "/files" }) {
        if (isDir(dir + sub)) {
            return dir + sub;
        }
    }
    return "";
}

// DVD container (.drs / core.das): the first part at 0x400 is the archive
// (archive.cpp RoomArchive::LoadDrs).
bool drsBody(const Bytes& file, Bytes& out, std::string& error)
{
    auto word = [&](std::size_t p) {
        return std::uint32_t(file[p]) << 24 | std::uint32_t(file[p + 1]) << 16 | std::uint32_t(file[p + 2]) << 8 |
               file[p + 3];
    };
    if (file.size() < 0x400) {
        error = "DVD-Container zu kurz";
        return false;
    }
    std::uint32_t length = word(0x24), offset = word(0x2C);
    if (word(0x20) != 0 || offset != 0x400 || length < 16 || length > file.size() - offset) {
        error = "kein DVD-Container mit Archiv an 0x400";
        return false;
    }
    out.assign(file.begin() + offset, file.begin() + offset + length);
    return true;
}

void collect(const std::string& dir, const std::string& rel, int depth, std::vector<std::string>& out,
             bool (*want)(const std::string&))
{
    for (const auto& name : listDir(dir)) {
        std::string path = dir + "/" + name;
        std::string r = rel.empty() ? name : rel + "/" + name;
        if (isDir(path)) {
            if (depth < 4) {
                collect(path, r, depth + 1, out, want);
            }
        } else if (want(r)) {
            out.push_back(r);
        }
    }
}

std::vector<std::string> g_rooms;

bool wantEvd(const std::string& rel)
{
    std::string base = rel.substr(rel.rfind('/') == std::string::npos ? 0 : rel.rfind('/') + 1);
    if (!endsWithNoCase(base, ".evd")) {
        return false;
    }
    for (const auto& room : g_rooms) {
        if (strncasecmp(base.c_str(), room.c_str(), room.size()) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<Input> discInputs(const std::string& dir)
{
    std::vector<Input> in;
    std::string files = filesFolder(dir);
    if (files.empty()) {
        in.push_back({ dir, kArchive, {}, "kein Ordner disc0/files, disc1/files oder files gefunden" });
        return in;
    }
    for (const auto& room : g_rooms) {
        unsigned id = unsigned(std::strtoul(room.c_str() + 1, nullptr, 16));
        std::string rel = "st" + std::to_string(id >> 8) + "/" + room + ".das";
        std::string path = findPath(files, rel);
        Input i{ rel, kArchive, {}, "" };
        Bytes file;
        if (path.empty() || !readFile(path, file)) {
            i.error = "Datei fehlt";
        } else {
            try {
                i.bytes = appUnpackYz2(file);
            } catch (const std::exception& e) {
                i.error = std::string("Yz2 (App): ") + e.what();
            }
        }
        in.push_back(i);
    }
    for (const auto& name : listDir(findPath(files, "etc"))) {
        if (endsWithNoCase(name, ".esl")) {
            Input i{ "etc/" + name, kEsl, {}, "" };
            if (!readFile(findPath(files, "etc/" + name), i.bytes)) {
                i.error = "nicht lesbar";
            }
            in.push_back(i);
        }
    }
    {
        Input i{ "debug/roominfo.dat", kRoomInfo, {}, "" };
        std::string path = findPath(files, "debug/roominfo.dat");
        if (path.empty() || !readFile(path, i.bytes)) {
            i.error = "Datei fehlt";
        }
        in.push_back(i);
    }
    std::vector<std::string> evds;
    collect(files, "", 0, evds, wantEvd);
    for (const auto& rel : evds) {
        Input i{ rel, kEvd, {}, "" };
        if (!readFile(files + "/" + rel, i.bytes)) {
            i.error = "nicht lesbar";
        }
        in.push_back(i);
    }
    std::vector<std::string> containers;
    if (!findPath(files, "etc/core.das").empty()) {
        containers.push_back(findPath(files, "etc/core.das"));
    }
    std::string em = findPath(files, "em");
    for (const auto& name : listDir(em)) {
        if (endsWithNoCase(name, ".drs")) {
            containers.push_back(em + "/" + name);
        }
    }
    for (const auto& path : containers) {
        Input i{ path.substr(files.size() + 1), kArchive, {}, "" };
        Bytes file;
        if (!readFile(path, file)) {
            i.error = "nicht lesbar";
        } else {
            drsBody(file, i.bytes, i.error);
        }
        in.push_back(i);
    }
    return in;
}

int usage()
{
    std::fprintf(stderr,
                 "usage: re4_format_compare                      synthetic samples\n"
                 "       re4_format_compare --data DIR [--room r100 ...] [-v]\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv)
{
    std::string data;
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
            data = argv[++i];
        } else if (std::strcmp(argv[i], "--room") == 0 && i + 1 < argc) {
            g_rooms.push_back(argv[++i]);
        } else if (std::strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else {
            return usage();
        }
    }
    if (g_rooms.empty()) {
        g_rooms.push_back("r100");
    }
    for (auto& room : g_rooms) {
        std::transform(room.begin(), room.end(), room.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    }
    bool synthetic = data.empty();
    std::vector<Input> inputs = synthetic ? syntheticInputs() : discInputs(data);
    std::printf("Formatvergleich: App-Leser (port/android-decomp) gegen Kern-Umwandler (port/whole/runtime/formats)\n");
    if (!synthetic) {
        std::printf("Disc-Daten: %s\n", data.c_str());
    }
    std::size_t files = 0, failed = 0, notComparable = 0, values = 0;
    for (const auto& in : inputs) {
        if (!in.error.empty()) {
            std::printf("  FEHLT    %s: %s\n", in.label.c_str(), in.error.c_str());
            notComparable++;
            if (synthetic) {
                failed++;
            }
            continue;
        }
        Result r = compare(in);
        files++;
        if (!r.appError.empty()) {
            std::printf("  N/V      %s (%s): App-Leser lehnt die Datei ab: %s\n", in.label.c_str(), kindName(in.kind),
                        r.appError.c_str());
            notComparable++;
            if (synthetic) {
                failed++;
            }
            continue;
        }
        bool bad = !r.diffs.empty() || !r.incomplete.empty() || r.gameRejected || (synthetic && !r.skipped.empty());
        values += r.equal;
        std::printf("  %-8s %s (%s): %zu Werte gleich, %zu abweichend, %zu Teile nicht vergleichbar\n",
                    bad ? "FEHLER" : "OK", in.label.c_str(), kindName(in.kind), r.equal, r.diffs.size(),
                    r.skipped.size());
        if (r.gameRejected) {
            std::printf("           Kern: Datei nicht angenommen\n");
        }
        for (const auto& p : r.incomplete) {
            std::printf("           Kern: Umwandlung unvollständig: %s\n", p.empty() ? "(Datei)" : p.c_str());
        }
        std::size_t shown = 0;
        for (const auto& d : r.diffs) {
            if (!verbose && shown++ >= 25) {
                std::printf("           ... %zu weitere (alle mit -v)\n", r.diffs.size() - 25);
                break;
            }
            std::printf("           %s\n", d.c_str());
        }
        for (const auto& s : r.skipped) {
            std::printf("           nicht vergleichbar: %s\n", s.c_str());
        }
        if (bad) {
            failed++;
        }
    }
    std::printf("Ergebnis: %zu Dateien verglichen, %zu Werte gleich, %zu mit Fehlern, %zu nicht vergleichbar -> %s\n",
                files, values, failed, notComparable, failed ? "FEHLER" : "OK");
    return failed ? 1 : 0;
}
