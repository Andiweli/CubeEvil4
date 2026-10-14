// Exact bounded native port of original em_sub.cpp/sub2.cpp hit geometry.
// Retain authored float boundary/order behavior; no game or scenario globals.
#include "hit_geometry.h"
#include "ports/motion64/runtime.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace re4 {
namespace {
using namespace motion64;
int LineSphereCrossCk(Vec* a, Vec* b, Vec* c, f32 r, Vec* pCross)
{
    Vec ab;
    Vec ac;
    Vec p;
    Vec q;
    Vec d1;
    Vec d2;
    f32 r2 = r * r;
    f32 dist2;
    f32 len2;
    f32 t;
    f32 h;

    dist2 = (a->x - c->x) * (a->x - c->x) + (a->y - c->y) * (a->y - c->y) + (a->z - c->z) * (a->z - c->z);
    if (dist2 < r2) {
        if (pCross) {
            *pCross = *a;
        }
        return 1;
    }
    len2 = (a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) + (a->z - b->z) * (a->z - b->z);
    if (len2 <= 0.1f) {
        return 0;
    }
    PSVECSubtract(b, a, &ab);
    PSVECSubtract(c, a, &ac);
    t = PSVECDotProduct(&ab, &ac) / len2;
    PSVECScale(&ab, &p, t);
    PSVECAdd(&p, a, &p);
    t = (p.x - c->x) * (p.x - c->x) + (p.y - c->y) * (p.y - c->y) + (p.z - c->z) * (p.z - c->z);
    if (t >= r2) {
        return 0;
    }
    h = SQRTF(r2 - t);
    PSVECSubtract(a, &p, &q);
    PSVECScale(&q, &q, h / PSVECMag(&q));
    PSVECAdd(&q, &p, &q);
    dist2 = (a->x - q.x) * (a->x - q.x) + (a->y - q.y) * (a->y - q.y) + (a->z - q.z) * (a->z - q.z);
    if (dist2 > len2) {
        return 0;
    }
    PSVECSubtract(a, &q, &d1);
    PSVECSubtract(a, b, &d2);
    if (PSVECDotProduct(&d1, &d2) < 0.0f) {
        return 0;
    }
    PSVECSubtract(b, &q, &d1);
    PSVECSubtract(b, a, &d2);
    if (PSVECDotProduct(&d1, &d2) < 0.0f) {
        return 0;
    }
    if (pCross) {
        *pCross = q;
    }
    return 1;
}

int emLinePolyCrossCk(Vec* pPos, Vec* pPos2, Vec* pRect, Vec* pCross)
{
    Vec e1;
    Vec e2;
    Vec n;
    Vec p;
    Vec c;
    f32 da;
    f32 db;
    f32 t;

    PSVECSubtract(&pRect[2], &pRect[1], &e1);
    PSVECSubtract(&pRect[0], &pRect[1], &e2);
    PSVECCrossProduct(&e1, &e2, &n);
    if (n.x == 0.0f && n.y == 0.0f && n.z == 0.0f) {
        return 0;
    }
#line 1552
    VECNormalize(&n, &n);
    da = PSVECDotProduct(&n, pPos) - PSVECDotProduct(&n, &pRect[0]);
    if (da <= 0.0f) {
        return 0;
    }
    db = PSVECDotProduct(&n, pPos2) - PSVECDotProduct(&n, &pRect[0]);
    if (db >= 0.0f) {
        return 0;
    }
    db = fabsf(db);
    t = db / (da + db);
    PSVECSubtract(pPos, pPos2, &p);
    PSVECScale(&p, &p, t);
    PSVECAdd(&p, pPos2, &p);
    PSVECSubtract(&p, &pRect[1], &e1);
    PSVECSubtract(&pRect[0], &pRect[1], &e2);
    PSVECCrossProduct(&e1, &e2, &c);
    if (PSVECDotProduct(&n, &c) < 0.0f) {
        return 0;
    }
    PSVECSubtract(&p, &pRect[2], &e1);
    PSVECSubtract(&pRect[1], &pRect[2], &e2);
    PSVECCrossProduct(&e1, &e2, &c);
    if (PSVECDotProduct(&n, &c) < 0.0f) {
        return 0;
    }
    PSVECSubtract(&p, &pRect[3], &e1);
    PSVECSubtract(&pRect[2], &pRect[3], &e2);
    PSVECCrossProduct(&e1, &e2, &c);
    if (PSVECDotProduct(&n, &c) < 0.0f) {
        return 0;
    }
    PSVECSubtract(&p, &pRect[0], &e1);
    PSVECSubtract(&pRect[3], &pRect[0], &e2);
    PSVECCrossProduct(&e1, &e2, &c);
    if (PSVECDotProduct(&n, &c) < 0.0f) {
        return 0;
    }
    *pCross = p;
    return 1;
}

int emLineCubeCrossCk(Vec* a, Vec* b, Mtx m, Vec* ofs, Vec* pCross, f32 sx, f32 sy, f32 sz)
{
    Mtx mat;
    Vec poly[4];
    Vec c;
    Vec v[8];

    PSMTXMultVec(m, ofs, &c);
    PSMTXCopy(m, mat);
    TransMatrix(mat, &c);
    v[0].x = -sx;
    v[0].y = 0.0f;
    v[0].z = sz;
    v[1].x = sx;
    v[1].y = 0.0f;
    v[1].z = sz;
    v[2].x = sx;
    v[2].y = sy;
    v[2].z = sz;
    v[3].x = -sx;
    v[3].y = sy;
    v[3].z = sz;
    v[4].x = -sx;
    v[4].y = 0.0f;
    v[4].z = -sz;
    v[5].x = sx;
    v[5].y = 0.0f;
    v[5].z = -sz;
    v[6].x = sx;
    v[6].y = sy;
    v[6].z = -sz;
    v[7].x = -sx;
    v[7].y = sy;
    v[7].z = -sz;
    PSMTXMultVec(mat, &v[0], &v[0]);
    PSMTXMultVec(mat, &v[1], &v[1]);
    PSMTXMultVec(mat, &v[2], &v[2]);
    PSMTXMultVec(mat, &v[3], &v[3]);
    PSMTXMultVec(mat, &v[4], &v[4]);
    PSMTXMultVec(mat, &v[5], &v[5]);
    PSMTXMultVec(mat, &v[6], &v[6]);
    PSMTXMultVec(mat, &v[7], &v[7]);
    poly[0] = v[0];
    poly[1] = v[1];
    poly[2] = v[2];
    poly[3] = v[3];
    if (emLinePolyCrossCk(a, b, poly, pCross)) {
        return 1;
    }
    poly[0] = v[1];
    poly[1] = v[5];
    poly[2] = v[6];
    poly[3] = v[2];
    if (emLinePolyCrossCk(a, b, poly, pCross)) {
        return 1;
    }
    poly[0] = v[5];
    poly[1] = v[4];
    poly[2] = v[7];
    poly[3] = v[6];
    if (emLinePolyCrossCk(a, b, poly, pCross)) {
        return 1;
    }
    poly[0] = v[4];
    poly[1] = v[0];
    poly[2] = v[3];
    poly[3] = v[7];
    if (emLinePolyCrossCk(a, b, poly, pCross)) {
        return 1;
    }
    poly[0] = v[3];
    poly[1] = v[2];
    poly[2] = v[6];
    poly[3] = v[7];
    if (emLinePolyCrossCk(a, b, poly, pCross)) {
        return 1;
    }
    poly[0] = v[1];
    poly[1] = v[0];
    poly[2] = v[4];
    poly[3] = v[5];
    if (emLinePolyCrossCk(a, b, poly, pCross)) {
        return 1;
    }
    return 0;
}

int emLineCapsuleCrossCk(Vec* a, Vec* b, Vec* pTop, Vec* pBtm, Vec* pCross, f32 r)
{
    Mtx m;
    Mtx inv;
    Vec d;
    Vec la;
    Vec lb;
    Vec c;
    Vec up;
    Vec e;
    Vec f;
    Vec g;
    f32 len;
    f32 ang;
    f32 dist;
    f32 dxz;
    f32 t;
    f32 h;

    PSMTXIdentity(m);
    PSVECSubtract(pTop, pBtm, &d);
    len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z);
    if (d.x == 0.0f && d.y == 0.0f && d.z == 0.0f) {
        d.y = 1.0f;
    }
#line 1250
    VECNormalize(&d, &d);
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    ang = acosf(PSVECDotProduct(&d, &up));
    if (ang > 0.01f && ang < PI - 0.01f) {
        PSVECCrossProduct(&up, &d, &up);
        PSMTXRotAxisRad(inv, &up, ang);
        PSMTXConcat(inv, m, m);
    }
    TransMatrix(m, pBtm);
    if (PSMTXInverse(m, inv) == 0) {
        PSMTXIdentity(inv);
    }
    PSMTXMultVec(inv, a, &la);
    PSMTXMultVec(inv, b, &lb);
    dist = (a->x - pTop->x) * (a->x - pTop->x) + (a->y - pTop->y) * (a->y - pTop->y) + (a->z - pTop->z) * (a->z - pTop->z);
    if (dist < r * r) {
        *pCross = *a;
        return 1;
    }
    dist = (a->x - pBtm->x) * (a->x - pBtm->x) + (a->y - pBtm->y) * (a->y - pBtm->y) +
           (a->z - pBtm->z) * (a->z - pBtm->z);
    if (dist < r * r) {
        *pCross = *a;
        return 1;
    }
    dist = la.x * la.x + la.z * la.z;
    if (la.y > 0.0f && la.y < len && dist < r * r) {
        *pCross = *a;
        return 1;
    }
    if ((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) + (a->z - b->z) * (a->z - b->z) <= 0.1f) {
        return 0;
    }
    if (LineSphereCrossCk(a, b, pTop, r, &c)) {
        PSMTXMultVec(inv, &c, &d);
        if (d.y < 0.0f || d.y > len) {
            *pCross = c;
            return 1;
        }
    }
    if (LineSphereCrossCk(a, b, pBtm, r, &c)) {
        PSMTXMultVec(inv, &c, &d);
        if (d.y < 0.0f || d.y > len) {
            *pCross = c;
            return 1;
        }
    }
    dxz = (la.x - lb.x) * (la.x - lb.x) + (la.z - lb.z) * (la.z - lb.z);
    if (dxz <= 0.1f) {
        return 0;
    }
    PSVECSubtract(&lb, &la, &e);
    PSVECScale(&la, &f, -1.0f);
    t = (e.x * f.x + e.z * f.z) / dxz;
    PSVECScale(&e, &g, t);
    PSVECAdd(&g, &la, &g);
    if (g.x * g.x + g.z * g.z >= r * r) {
        return 0;
    }
    h = SQRTF(r * r - (g.x * g.x + g.z * g.z));
    PSVECSubtract(&la, &g, &d);
    PSVECScale(&d, &d, h / PSVECMag(&d));
    PSVECAdd(&d, &g, &d);
    // The entry-point distance reuses `dist` (a function-scope pseudo, so its sum schedules ahead of
    // h's) and is computed first.
    dist = (la.x - d.x) * (la.x - d.x) + (la.y - d.y) * (la.y - d.y) + (la.z - d.z) * (la.z - d.z);
    h = (la.x - lb.x) * (la.x - lb.x) + (la.y - lb.y) * (la.y - lb.y) + (la.z - lb.z) * (la.z - lb.z);
    if (dist > h || d.y < 0.0f || d.y > len) {
        return 0;
    }
    PSMTXMultVec(m, &d, &la);
    PSVECSubtract(a, &la, &d);
    PSVECSubtract(a, b, &up);
    if (PSVECDotProduct(&d, &up) < 0.0f) {
        return 0;
    }
    PSVECSubtract(b, &la, &d);
    PSVECSubtract(b, a, &up);
    if (PSVECDotProduct(&d, &up) < 0.0f) {
        return 0;
    }
    *pCross = la;
    return 1;
}

constexpr float coordinateLimit = 1e7f;
void validVector(const HitVector &value) {
  for (float component : value)
    if (!std::isfinite(component) || std::abs(component) > coordinateLimit)
      throw std::runtime_error("Hit geometry: invalid coordinate");
}
void validMatrix(const Matrix34 &matrix) {
  for (float component : matrix)
    if (!std::isfinite(component) || std::abs(component) > coordinateLimit)
      throw std::runtime_error("Hit geometry: invalid part matrix");
}
void validSize(float value) {
  if (!std::isfinite(value) || value < 0 || value > 1e6f)
    throw std::runtime_error("Hit geometry: invalid extent");
}
Vec vector(const HitVector &value) { return {value[0], value[1], value[2]}; }
HitVector vector(const Vec &value) { return {value.x, value.y, value.z}; }
HitVector narrow(const CollisionVec &value) {
  HitVector result{};
  for (unsigned i = 0; i < 3; ++i) {
    if (!std::isfinite(value[i]) || std::abs(value[i]) > coordinateLimit)
      throw std::runtime_error("Hit geometry: invalid ray");
    result[i] = float(value[i]);
  }
  return result;
}
Matrix34 concatenate(const Matrix34 &parent, const Matrix34 &local) {
  Mtx p, l, result;
  std::memcpy(p, parent.data(), sizeof(p));
  std::memcpy(l, local.data(), sizeof(l));
  PSMTXConcat(p, l, result);
  Matrix34 value;
  std::memcpy(value.data(), result, sizeof(result));
  validMatrix(value);
  return value;
}
bool ignoresHandgun(unsigned weapon) {
  // Original HandgunCk, em_sub.cpp3313 (TMP and related IDs included).
  switch (weapon) {
  case 1: case 2: case 3: case 4: case 0x11: case 0x26: case 0x2b:
    return true;
  default:
    return false;
  }
}
} // namespace

std::optional<HitVector> HitLineCapsule(const HitVector &from,
                                       const HitVector &to,
                                       const HitVector &top,
                                       const HitVector &bottom, float radius) {
  for (const auto *point : {&from, &to, &top, &bottom})
    validVector(*point);
  validSize(radius);
  auto a = vector(from), b = vector(to), t = vector(top), q = vector(bottom);
  Vec result{};
  if (!emLineCapsuleCrossCk(&a, &b, &t, &q, &result, radius))
    return {};
  auto hit = vector(result);
  validVector(hit);
  return hit;
}
std::optional<HitVector> HitLineCube(const HitVector &from, const HitVector &to,
                                    const Matrix34 &matrix,
                                    const HitVector &offset, float width,
                                    float height, float extent) {
  for (const auto *point : {&from, &to, &offset})
    validVector(*point);
  validMatrix(matrix);
  for (float size : {width, height, extent})
    validSize(size);
  auto a = vector(from), b = vector(to), origin = vector(offset);
  Mtx m;
  std::memcpy(m, matrix.data(), sizeof(m));
  Vec result{};
  if (!emLineCubeCrossCk(&a, &b, m, &origin, &result, width, height, extent))
    return {};
  auto hit = vector(result);
  validVector(hit);
  return hit;
}
std::optional<AuthoredHitResult> HitAuthoredShapes(
    const std::vector<AuthoredHitShape> &shapes,
    const std::vector<Matrix34> &rawJointMatrices,
    const Matrix34 &actorPlacement, const CollisionVec &from,
    const CollisionVec &to, float maxLenSq, unsigned weaponNo) {
  if (shapes.size() > 256 || rawJointMatrices.size() > 248 ||
      !std::isfinite(maxLenSq) || maxLenSq < 0 || maxLenSq > 1e16f ||
      weaponNo > 0x2d)
    throw std::runtime_error("Hit geometry: Data budget or weapon index");
  validMatrix(actorPlacement);
  for (const auto &matrix : rawJointMatrices)
    validMatrix(matrix);
  const auto a = narrow(from), b = narrow(to);
  std::optional<AuthoredHitResult> closest;
  float best = maxLenSq;
  for (std::size_t index = 0; index < shapes.size(); ++index) {
    const auto &shape = shapes[index];
    validVector(shape.offset);
    for (float size : {shape.radius, shape.height, shape.extent})
      validSize(size);
    if (shape.partsNo < 0 || std::size_t(shape.partsNo) > rawJointMatrices.size() ||
        (shape.flags & ~(HitOn | HitXAxis | HitZAxis | HitCube |
                         HitIgnoreHandguns | HitThrough | HitNoMark |
                         HitNoScenarioBomb | HitDamagePosition)))
      throw std::runtime_error("Hit geometry: invalid hit part");
    if (!(shape.flags & HitOn) ||
        ((shape.flags & HitIgnoreHandguns) && ignoresHandgun(weaponNo)))
      continue;
    const auto matrix = shape.partsNo == 0
                            ? actorPlacement
                            : concatenate(actorPlacement,
                                          rawJointMatrices[shape.partsNo - 1]);
    std::optional<HitVector> hit;
    if (shape.flags & HitCube) {
      hit = HitLineCube(a, b, matrix, shape.offset, shape.radius, shape.height,
                        shape.extent);
    } else {
      Vec bottom = vector(shape.offset), top = bottom;
      if (shape.flags & HitXAxis)
        top.x += shape.height;
      else if (shape.flags & HitZAxis)
        top.z += shape.height;
      else
        top.y += shape.height;
      Mtx m;
      std::memcpy(m, matrix.data(), sizeof(m));
      PSMTXMultVec(m, &top, &top);
      PSMTXMultVec(m, &bottom, &bottom);
      Vec radius{0, 0, shape.radius};
      PSMTXMultVecSR(m, &radius, &radius);
      hit = HitLineCapsule(a, b, vector(top), vector(bottom), PSVECMag(&radius));
    }
    if (!hit)
      continue;
    auto delta = vector(a), point = vector(*hit);
    PSVECSubtract(&delta, &point, &delta);
    const float lenSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    if (!std::isfinite(lenSq))
      throw std::runtime_error("Hit geometry: invalid hit distance");
    // Literal emLineAtCk: equal distances replace the preceding hit part.
    if (lenSq > best)
      continue;
    best = lenSq;
    closest = AuthoredHitResult{index, lenSq, *hit};
  }
  return closest;
}
std::vector<AuthoredHitShape> NormalGanadoHitShapes() {
  // em10.cpp3354–3399: the normal (non typeA/D, non heavy type7..C) branch.
  return {
      {{0, 0, 0}, 160, 110, 0, HitOn, 5},
      {{0, -30, 0}, 210, 290, 0, HitOn, 2},
      {{-20, -400, 0}, 150, 400, 0, HitOn, 0x14},
      {{20, -400, 0}, 150, 400, 0, HitOn, 0x18},
      {{-350, 0, 0}, 100, 350, 0, HitOn | HitXAxis, 9},
      {{0, 0, 0}, 100, 350, 0, HitOn | HitXAxis, 0x0f},
      {{-20, -300, 0}, 170, 300, 0, HitOn, 0x13},
      {{20, -300, 0}, 170, 300, 0, HitOn, 0x17},
      {{-300, 0, 0}, 120, 300, 0, HitOn | HitXAxis, 8},
      {{0, 0, 0}, 120, 300, 0, HitOn | HitXAxis, 0x0e},
      {{0, 0, 0}, 300, 0, 0, 0, 5}};
}
} // namespace re4
