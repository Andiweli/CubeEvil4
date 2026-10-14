#ifndef RE4_FORMAT_SAMPLES_H
#define RE4_FORMAT_SAMPLES_H
// Synthetic samples of the disc formats, big-endian as on the disc: models
// (BIN), textures (TPL), scenery (SMD, SMX), collision (SAT, EAT), area events
// (AEV), lights (LIT), cameras (CAM), motion curves (FCV), archives and DVD
// containers, enemy lists (ESL), events (EVD) and the room jump table. Used by
// re4_format_compare (main.cpp) and by the app's host check of the core mode
// renderer (port/android-decomp/tools/test_core_scene_gl.cpp).
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace re4samples {

using Bytes = std::vector<std::uint8_t>;


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
inline Bytes makeBin(bool ext, bool colored, bool smallNormals)
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

// A rigid model as room scenery uses (one part, no skinning weights): a
// 4000 x 4000 floor square at height 0 made of two triangles, texture 0.
inline Bytes makeRigidBin()
{
    Be m;
    m.zeros(0x60);
    std::size_t head = m.size();
    m.u8(0), m.u8(0xFF), m.u8(0), m.u8(0), m.vec(0, 0, 0);
    m.pad(16);
    std::size_t vtx = m.size();
    const int pos[4][3] = { { -2000, 0, -2000 }, { 2000, 0, -2000 }, { 2000, 0, 2000 }, { -2000, 0, 2000 } };
    for (const auto& v : pos) {
        m.u16(unsigned(v[0]) & 0xFFFF), m.u16(unsigned(v[1]) & 0xFFFF), m.u16(unsigned(v[2]) & 0xFFFF), m.u16(0);
    }
    std::size_t nrm = m.size();
    for (unsigned i = 0; i < 4; i++) {
        m.u16(0), m.u16(16384), m.u16(0), m.u16(0);
    }
    m.pad(16);
    std::size_t tex = m.size();
    const unsigned uv[4][2] = { { 0, 0 }, { 0x100, 0 }, { 0x100, 0x100 }, { 0, 0x100 } };
    for (const auto& c : uv) {
        m.u16(c[0]), m.u16(c[1]);
    }
    m.pad(16);
    std::size_t blend = m.size();
    m.u32(0);
    m.pad(16);
    std::size_t flip = m.size();
    m.u32(0);
    m.pad(32);
    std::size_t parts = m.size();
    m.zeros(32);  // cPolyHeader: texture 0, no alpha texture
    m.put32(parts + 0x18, 64);  // buff_size
    m.put32(parts + 0x1C, 2);   // nPoly
    std::size_t stream = m.size();
    const unsigned corners[6] = { 0, 2, 1, 0, 3, 2 };
    m.u8(0x90), m.u16(6);
    for (unsigned c : corners) {
        m.u16(c), m.u16(c), m.u16(c);
    }
    while (m.size() < stream + 64) {
        m.u8(0);
    }
    m.put32(0x00, std::uint32_t(head));
    m.put32(0x10, std::uint32_t(tex));
    m.b[0x19] = 1;
    m.put16(0x1A, 1);
    m.put32(0x1C, std::uint32_t(parts));
    m.put32(0x24, 1);
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
inline Bytes makeTpl()
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

// `bin`: the model of both placements (default: the skinned sample).
inline Bytes makeSmd(const Bytes& bin = makeBin(false, true, false))
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
    s.append(bin);
    s.pad(32);
    std::size_t tplTbl = s.size();
    s.u32(0x20);
    s.pad(32);
    s.append(makeTpl());
    s.put32(4, std::uint32_t(binTbl));
    s.put32(8, std::uint32_t(tplTbl));
    s.pad(32);
    s.put32(12, std::uint32_t(s.size()));  // empty motion table at the end (as in r100)
    return s.b;
}

inline Bytes makeSmx()
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

inline Bytes makeSatFile(float shift)
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

inline Bytes makeEat()
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
inline void sceRecord(Be& a, unsigned id, unsigned no)
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

inline Bytes makeAev()
{
    Be a;
    a.text("AEV", 4), a.u16(0x104), a.u16(3), a.zeros(8);
    sceRecord(a, 1, 0);
    sceRecord(a, 0x10, 1);
    sceRecord(a, 8, 2);
    return a.b;
}

inline void lightWork(Be& l, unsigned type, unsigned id, unsigned parent)
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

inline Bytes makeLit()
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

inline Bytes makeCam()
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
inline Bytes makeCurve(unsigned maxFrame)
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

inline Bytes makeArchive(const std::vector<std::pair<const char*, Bytes>>& entries)
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

inline Bytes makeEsl()
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

inline Bytes makeEvd()
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

inline Bytes makeRoomInfo()
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

// A DVD container with one part: the archive at 0x400 (RoomArchive::LoadDrs).
inline Bytes makeDrs(const Bytes& archive)
{
    Be d;
    d.zeros(0x400);
    d.put32(0x24, std::uint32_t(archive.size()));
    d.put32(0x2C, 0x400);
    d.append(archive);
    return d.b;
}

}  // namespace re4samples

#endif
