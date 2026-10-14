#include "melee_rules.h"
#include "ports/motion64/runtime.h"
#include "ports/rnd.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

// Frozen executable source oracle from upstream commit
// 9b76ff003081d6ece8b838453fb8310743a972c7. The selected original functions
// below are unmodified; only manager/model stubs replace unrelated systems.
namespace source_oracle {
using namespace re4::motion64;
using ::Rnd;
float LIMIT_ANGLE(float angle) {
  while (angle >= PI) angle -= PI2;
  while (angle < -PI) angle += PI2;
  return angle;
}
f32 GetXZAngle(Vec* v0, Vec* v1)
{
    return LIMIT_ANGLE(atan2f(v1->x - v0->x, v1->z - v0->z));
}

// Angle from `from` to `to` in the XY plane.
f32 GetXYAngle(Vec* v0, Vec* v1)
{
    f32 dx = v1->x - v0->x;
    f32 dy = v1->y - v0->y;
    return LIMIT_ANGLE(atan2f(dy, dx));
}

// Yaw to `to` relative to a facing `ang` (0 = straight ahead, wrapped to -PI..PI).
f32 GetXZAngleLocal(Vec* v0, Vec* v1, f32 v0_dir)
{
    return LIMIT_ANGLE(GetXZAngle(v0, v1) - v0_dir);
}

// Squared distance between two points.
f32 GetDistance(Vec* v0, Vec* v1)
{
    f32 x = v1->x - v0->x;
    f32 y = v1->y - v0->y;
    f32 z = v1->z - v0->z;
    return x * x + y * y + z * z;
}

// Squared distance between two points.
f32 GetDistance(Vec& v0, Vec& v1)
{
    f32 x = v1.x - v0.x;
    f32 y = v1.y - v0.y;
    f32 z = v1.z - v0.z;
    return x * x + y * y + z * z;
}

// Squared XZ distance.
f32 GetDistanceXZ(Vec* v0, Vec* v1)
{
    f32 x = v1->x - v0->x;
    f32 z = v1->z - v0->z;
    return x * x + z * z;
}

// Turn amount toward `target` from facing `ang`: the signed yaw difference clamped to +-limit
// (the usual `ang.y += Muku(...)` turning step).
f32 Muku(Vec* v0, Vec* v1, f32 dir, f32 dy)
{
    f32 d = LIMIT_ANGLE(GetXZAngle(v0, v1) - dir);
    f32 ret;

    if (d > 0.0f) {
        if (d < dy) {
            dy = d;
        }
    } else {
        if (d > -dy) {
            dy = -d;
        }
    }
    ret = dy;
    if (!(d >= 0.0f)) {
        ret = -ret;
    }
    return ret;
}

float RootSumSquare3(Vec* v) {
  return std::sqrt(v->x*v->x + v->y*v->y + v->z*v->z);
}
float sourceSquared(Vec* a, Vec* b) {
  return (a->x-b->x)*(a->x-b->x) + (a->y-b->y)*(a->y-b->y) +
         (a->z-b->z)*(a->z-b->z);
}
#define SQ_DIST(a, b) sourceSquared(a, b)
u32 AtSphereCapsuleCk(Vec* c, f32 sph_r, Vec* p0, Vec* p1, f32 cap_r)
{
    Vec dir;
    Vec p;
    f32 rr;
    f32 len;
    f32 d;   // the two end-point distances through one twice-set local: not tied to the dz chain
    u32 n;

    rr = (sph_r + cap_r) * (sph_r + cap_r);
    d = SQ_DIST(c, p0);
    if (d < rr) {
        return 1;
    }
    d = SQ_DIST(c, p1);
    if (d < rr) {
        return 1;
    }
    PSVECSubtract(p0, p1, &dir);
    len = source_oracle::RootSumSquare3(&dir);
    n = (u32) (len / (cap_r + cap_r)) + 2;
    len = len / (f32) n;
    if (len < 0.01f) {
        return 0;
    }
#line 366 "D:/Bio4/Prog/at_sub.cpp"
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, len);
    if (cap_r < 0.01f) {
        n = 2;
    }
    p = *p1;
    n--;
    while (n-- != 0) {
        PSVECAdd(&p, &dir, &p);
        if (SQ_DIST(c, &p) < rr) {
            return 1;
        }
    }
    return 0;
}
#undef SQ_DIST
struct YARARE_INFO {
  Vec offset{}, cross{};
  float radius{}, height{}, extent{};
  unsigned short flag{};
  short parts_no{};
  float len{}, c_dis{};
  YARARE_INFO* pList{};
};
struct cCoord { Mtx mat{}; };
struct cEm { YARARE_INFO hitInfo{}; std::vector<cCoord> parts; };
enum {
  YAT_FLAG_ON=1, YAT_FLAG_X_AXIS=2, YAT_FLAG_Z_AXIS=4, YAT_FLAG_CUBE=8,
  YAT_FLAG_HANDGUN_MUSHI=16
};
int HandgunCk(int weapon) {
  switch (weapon) {
  case 1: case 2: case 3: case 4: case 0x11: case 0x26: case 0x2b: return 1;
  default: return 0;
  }
}
cCoord* HitParts(cEm* em, YARARE_INFO* hit) {
  return &em->parts.at(unsigned(hit->parts_no));
}
int At_box_sphere_ck(Vec*, Vec*, float) {
  throw std::runtime_error("oracle fixture does not enable cubes");
}
YARARE_INFO* emSphereAtCk(cEm* em, Vec* pos, Vec* pos2, f32 r, f32 r2, int flag)
{
    Vec pTop;
    Vec pBtm;
    Vec center;
    Vec d;
    Vec s;
    YARARE_INFO* p;
    YARARE_INFO* ret;
    cCoord* parts;
    f32 dist;
    f32 bestRad;
    f32 bestDot;
    f32 rr;
    f32 dp;

    dist = (pos->x - pos2->x) * (pos->x - pos2->x) + (pos->y - pos2->y) * (pos->y - pos2->y) +
           (pos->z - pos2->z) * (pos->z - pos2->z);
    if (dist > 100.0f) {
        PSVECSubtract(pos, pos2, &d);
#line 1658
        VECNormalize(&d, &d);
    } else {
        dist = 0.0f;
    }
    bestRad = 1e16f;
    ret = 0;
    bestDot = -PI;
    for (p = &em->hitInfo; p != 0; p = p->pList) {
        if (!(p->flag & YAT_FLAG_ON)) {
            continue;
        }
        if ((p->flag & YAT_FLAG_HANDGUN_MUSHI) && HandgunCk(flag)) {
            continue;
        }
        pBtm = p->offset;
        pTop = p->offset;
        if (p->flag & (YAT_FLAG_X_AXIS | YAT_FLAG_Z_AXIS)) {
            if (p->flag & YAT_FLAG_X_AXIS) {
                pTop.x += p->height;
            } else {
                pTop.z += p->height;
            }
        } else {
            pTop.y += p->height;
        }
        parts = HitParts(em, p);
        PSMTXMultVec(parts->mat, &pTop, &pTop);
        PSMTXMultVec(parts->mat, &pBtm, &pBtm);
        PSVECAdd(&pTop, &pBtm, &center);
        PSVECScale(&center, &center, 0.5f);
        if (p->flag & YAT_FLAG_CUBE) {
            Vec box[8] = {
                {-500.0f, -450.0f, 0.0f},   {500.0f, -450.0f, 0.0f},   {-3000.0f, -800.0f, 15000.0f}, {3000.0f, -800.0f, 15000.0f},
                {-500.0f, 450.0f, 0.0f},    {500.0f, 450.0f, 0.0f},    {-3000.0f, 800.0f, 15000.0f},  {3000.0f, 800.0f, 15000.0f},
            };

            box[0].x = -p->radius;
            box[0].y = 0.0f;
            box[0].z = -p->extent;
            box[1].x = p->radius;
            box[1].y = 0.0f;
            box[1].z = -p->extent;
            box[2].x = -p->radius;
            box[2].y = 0.0f;
            box[2].z = p->extent;
            box[3].x = p->radius;
            box[3].y = 0.0f;
            box[3].z = p->extent;
            box[4].x = -p->radius;
            box[4].y = p->height;
            box[4].z = -p->extent;
            box[5].x = p->radius;
            box[5].y = p->height;
            box[5].z = -p->extent;
            box[6].x = -p->radius;
            box[6].y = p->height;
            box[6].z = p->extent;
            box[7].x = p->radius;
            box[7].y = p->height;
            box[7].z = p->extent;
            PSMTXMultVec(parts->mat, &box[0], &box[0]);
            PSMTXMultVec(parts->mat, &box[1], &box[1]);
            PSMTXMultVec(parts->mat, &box[2], &box[2]);
            PSMTXMultVec(parts->mat, &box[3], &box[3]);
            PSMTXMultVec(parts->mat, &box[4], &box[4]);
            PSMTXMultVec(parts->mat, &box[5], &box[5]);
            PSMTXMultVec(parts->mat, &box[6], &box[6]);
            PSMTXMultVec(parts->mat, &box[7], &box[7]);
            if (At_box_sphere_ck(box, pos, r) == 0) {
                continue;
            }
        } else {
            s.x = 0.0f;
            s.y = 0.0f;
            s.z = p->radius;
            PSMTXMultVecSR(parts->mat, &s, &s);
            rr = PSVECMag(&s);
            if (AtSphereCapsuleCk(pos, r, &pTop, &pBtm, rr) == 0) {
                continue;
            }
        }
        PSVECSubtract(&center, pos, &s);
        if (r != r2) {
            if (fabsf(s.y) > r2 + p->radius) {
                continue;
            }
        }
        p->len = s.x * s.x + s.y * s.y + s.z * s.z;
        if (dist > 0.0f) {
#line 1781
            VECNormalize(&s, &s);
            dp = PSVECDotProduct(&d, &s);
            if (dp < bestDot) {
                if (dp < 0.6f) {
                    continue;
                }
                if (p->len >= bestRad) {
                    continue;
                }
            }
            bestDot = dp;
            bestRad = p->len;
            ret = p;
        } else {
            p->len = (pos->x - center.x) * (pos->x - center.x) + (pos->y - center.y) * (pos->y - center.y) +
                     (pos->z - center.z) * (pos->z - center.z);
            if (p->len >= bestRad) {
                continue;
            }
            bestRad = p->len;
            ret = p;
        }
    }
    return ret;
}
struct FREE_EM10 {
  unsigned Be_flg{};
  int Timer{};
  float TmpF{};
  Vec Spd{};
};
struct RecordedMotion { unsigned motion{}, sequence{}, hokan{}, flags{}; };
struct cEm10 {
  Vec pos{}, ang{};
  short hp{};
  int r_no_3{};
  unsigned subArc{};
  RecordedMotion Motion{};
  struct { Vec m_PosFrom{}; } dmg;
};
#define PL_ARC_PTR(arc, entry) (entry)
void MotionSetCore(cEm10*, RecordedMotion* out, unsigned motion,
                   unsigned sequence, unsigned hokan, unsigned flags, int) {
  *out = {motion-4, sequence-4, hokan, flags};
}
void selectBlow(cEm10* em, FREE_EM10* w) {
  int flag;
  float ang;
        w->Timer = 0;
        if (em->r_no_3) {
            w->Timer = 1;
        }
        em->r_no_3 = Rnd() & 1;
        flag = em->r_no_3 ? 1 : 0x41;
        ang = fabsf(Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI));
        w->TmpF = 1.0f;
        if ((u8) (Rnd() % 10) > 6 || em->hp > 0) {
            if (ang < 1.5707964f) {
                if ((Rnd() & 1) || (w->Be_flg & 0x80)) {
                    MotionSetCore(em, &em->Motion, PL_ARC_PTR(em->subArc, 0x43), PL_ARC_PTR(em->subArc, 0x44), 3, flag, 0);
                    em->ang.y += Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI);
                    w->Be_flg &= ~0x20;
                    w->Spd.x = 0.0f;
                    w->Spd.y = -100.0f;
                    w->Spd.z = -100.0f;
                } else {
                    MotionSetCore(em, &em->Motion, PL_ARC_PTR(em->subArc, 0x3F), PL_ARC_PTR(em->subArc, 0x40), 3, flag, 0);
                    em->ang.y += Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI);
                    w->Be_flg |= 0x20;
                    w->Spd.x = 0.0f;
                    w->Spd.y = -100.0f;
                    w->Spd.z = -100.0f;
                }
            } else {
                if (Rnd() & 1) {
                    MotionSetCore(em, &em->Motion, PL_ARC_PTR(em->subArc, 0x4B), PL_ARC_PTR(em->subArc, 0x4C), 3, flag, 0);
                    em->ang.y += Muku(&em->dmg.m_PosFrom, &em->pos, em->ang.y, PI);
                    w->Be_flg |= 0x20;
                    w->Spd.x = 0.0f;
                    w->Spd.y = -100.0f;
                    w->Spd.z = 100.0f;
                } else {
                    MotionSetCore(em, &em->Motion, PL_ARC_PTR(em->subArc, 0x4D), PL_ARC_PTR(em->subArc, 0x4E), 3, flag, 0);
                    em->ang.y += Muku(&em->dmg.m_PosFrom, &em->pos, em->ang.y, PI);
                    w->Be_flg &= ~0x20;
                    w->Spd.x = 0.0f;
                    w->Spd.y = -100.0f;
                    w->Spd.z = 100.0f;
                }
            }
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        } else {
            if (ang < 1.5707964f && (u8) (Rnd() % 10) > 6) {
                MotionSetCore(em, &em->Motion, PL_ARC_PTR(em->subArc, 0x41), PL_ARC_PTR(em->subArc, 0x42), 3, flag, 0);
                w->Be_flg &= ~0x20;
                w->Spd.x = 0.0f;
                w->Spd.y = -200.0f;
                w->Spd.z = -100.0f;
            } else {
                MotionSetCore(em, &em->Motion, PL_ARC_PTR(em->subArc, 0x3B), PL_ARC_PTR(em->subArc, 0x3C), 3, flag, 0);
                w->Be_flg |= 0x20;
                if (em->r_no_3 == 1) {
                    w->TmpF = 2.0f;
                }
                w->Spd.x = 0.0f;
                w->Spd.y = -200.0f;
                w->Spd.z = -100.0f;
            }
            em->ang.y += Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }

}
#undef PL_ARC_PTR
int Dmg_tbl_em10[0x2E] = {
    0, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 9999, 0, 150,
    100, 150, 9999, 2500, 200, 500, 0, 0, 100, 0, 150, 150, 150, 150, 150, 0,
    0, 150, 0, 0, 400, 2000, 150, 150, 150, 2500, 0, 150, 150, 2500,
};
} // namespace source_oracle

namespace {
using namespace re4;
Matrix34 matrix(float yaw=0, float scale=1, HitVector translation={0,0,0}) {
  re4::motion64::Mtx m;
  re4::motion64::PSMTXRotRad(m, 'y', yaw);
  for (unsigned row=0; row<3; ++row) {
    for (unsigned col=0; col<3; ++col) m[row][col] *= scale;
    m[row][3] = translation[row];
  }
  Matrix34 result;
  std::memcpy(result.data(), m, sizeof(m));
  return result;
}
template <class F> void rejects(F function) {
  bool rejected=false;
  try { function(); } catch (const std::runtime_error&) { rejected=true; }
  assert(rejected);
}
struct OracleSphereResult {
  std::size_t index{};
  float distance{};
  HitVector center{};
};
std::optional<OracleSphereResult> oracleSphere(
    const std::vector<AuthoredHitShape>& shapes,
    const std::vector<Matrix34>& joints, const Matrix34& placement,
    const CollisionVec& center, float radius, unsigned weapon) {
  using namespace source_oracle;
  source_oracle::cEm em;
  em.parts.resize(joints.size()+1);
  std::memcpy(em.parts[0].mat, placement.data(), sizeof(Mtx));
  for (std::size_t i=0; i<joints.size(); ++i) {
    Mtx raw;
    std::memcpy(raw, joints[i].data(), sizeof(raw));
    PSMTXConcat(em.parts[0].mat, raw, em.parts[i+1].mat);
  }
  std::vector<YARARE_INFO> records(shapes.size());
  for (std::size_t i=0; i<shapes.size(); ++i) {
    const auto& shape=shapes[i];
    records[i].offset={shape.offset[0],shape.offset[1],shape.offset[2]};
    records[i].radius=shape.radius;
    records[i].height=shape.height;
    records[i].extent=shape.extent;
    records[i].parts_no=shape.partsNo;
    records[i].flag=shape.flags;
    records[i].pList=i+1<records.size()? &records[i+1] : nullptr;
  }
  if (records.empty()) return {};
  em.hitInfo=records[0];
  Vec c{float(center[0]),float(center[1]),float(center[2])};
  const auto* hit=emSphereAtCk(&em,&c,&c,radius,radius,int(weapon));
  if (!hit) return {};
  const std::size_t index=hit==&em.hitInfo?0:std::size_t(hit-records.data());
  Vec bottom=hit->offset, top=bottom;
  if (hit->flag&YAT_FLAG_X_AXIS) top.x+=hit->height;
  else if (hit->flag&YAT_FLAG_Z_AXIS) top.z+=hit->height;
  else top.y+=hit->height;
  auto* part=HitParts(&em,const_cast<YARARE_INFO*>(hit));
  PSMTXMultVec(part->mat,&top,&top);
  PSMTXMultVec(part->mat,&bottom,&bottom);
  Vec mid;
  PSVECAdd(&top,&bottom,&mid);
  PSVECScale(&mid,&mid,.5f);
  return OracleSphereResult{index,hit->len,{mid.x,mid.y,mid.z}};
}
void compareSphere(const std::vector<AuthoredHitShape>& shapes,
                   const std::vector<Matrix34>& joints,
                   const Matrix34& placement, const CollisionVec& center,
                   float radius, unsigned weapon) {
  const auto expected=oracleSphere(shapes,joints,placement,center,radius,weapon);
  const auto actual=SphereAuthoredShapes(shapes,joints,placement,center,radius,weapon);
  assert(bool(actual)==bool(expected));
  if (actual) {
    assert(actual->partIndex==expected->index);
    assert(actual->lenSq==expected->distance);
    assert(actual->center==expected->center);
    assert(actual->flags==(shapes[actual->partIndex].flags&~HitDamagePosition));
  }
}
void promptBounds() {
  assert(KickPromptGeometry({0,0,0},0,{0,700,1500}));
  assert(KickPromptGeometry({0,0,0},0,{0,-700,1500}));
  assert(!KickPromptGeometry({0,0,0},0,{0,700.001,1500}));
  assert(!KickPromptGeometry({0,0,0},0,{0,0,1500.001}));
  assert(KickPromptGeometry({0,0,0},1.0471976f,{0,0,1000}));
  assert(!KickPromptGeometry({0,0,0},
      std::nextafter(1.0471976f,2.0f),{0,0,1000}));
  assert(!KickPromptGeometry({0,0,0},0,{0,0,-1000}));
  for (unsigned i=0;i<4096;++i) {
    const float heading=-PI + float(i%256)*PI/128;
    const float toward=-PI + float(i/256)*PI/8;
    CollisionVec player{1000,200,-3000};
    CollisionVec enemy{player[0]+std::sin(toward)*1400,
                        player[1]+float(int(i%3)-1)*700,
                        player[2]+std::cos(toward)*1400};
    source_oracle::Vec p{float(player[0]),float(player[1]),float(player[2])};
    source_oracle::Vec e{float(enemy[0]),float(enemy[1]),float(enemy[2])};
    float dx=e.x-p.x,dz=e.z-p.z;
    const bool expected=dx*dx+dz*dz<=2250000 && std::abs(e.y-p.y)<=700 &&
        std::abs(source_oracle::Muku(&p,&e,heading,PI))<=1.0471976f;
    assert(KickPromptGeometry(player,heading,enemy)==expected);
  }
  rejects([]{KickPromptGeometry({0,0,0},std::numeric_limits<float>::infinity(),{0,0,1});});
  rejects([]{KickPromptGeometry({0,std::numeric_limits<double>::quiet_NaN(),0},0,{0,0,1});});
}
void sphereBoundaries() {
  const auto identity=matrix();
  const std::vector<AuthoredHitShape> sphere{{{0,0,0},1,0,0,HitOn,0}};
  assert(!SphereAuthoredShapes(sphere,{},identity,{2,0,0},1));
  assert(SphereAuthoredShapes(sphere,{},identity,
      {std::nextafter(2.0f,0.0f),0,0},1));
  compareSphere(sphere,{},identity,{2,0,0},1,0x14);
  compareSphere(sphere,{},identity,{std::nextafter(2.0f,0.0f),0,0},1,0x14);
  // Original sampled capsule can miss where an analytic capsule would hit.
  const std::vector<AuthoredHitShape> capsule{{{0,0,0},1,10,0,HitOn,0}};
  assert(!SphereAuthoredShapes(capsule,{},identity,{1.99,5.0/7,0},1));
  compareSphere(capsule,{},identity,{1.99,5.0/7,0},1,0x14);
  auto duplicate=sphere;
  duplicate.push_back(sphere[0]);
  assert(SphereAuthoredShapes(duplicate,{},identity,{0,0,0},1)->partIndex==0);
  duplicate[0].flags=0;
  assert(SphereAuthoredShapes(duplicate,{},identity,{0,0,0},1)->partIndex==1);
  duplicate[0].flags=HitOn|HitIgnoreHandguns;
  assert(SphereAuthoredShapes(duplicate,{},identity,{0,0,0},1,1)->partIndex==1);
  assert(SphereAuthoredShapes(duplicate,{},identity,{0,0,0},1,0x10)->partIndex==0);
  auto shape=sphere;
  shape[0].flags=HitOn|HitDamagePosition|HitThrough;
  auto hit=SphereAuthoredShapes(shape,{},identity,{0,0,0},1);
  assert(hit && hit->flags==(HitOn|HitThrough));
  hit->lenSq=129600;
  assert(!MeleeSphereNeedsScenarioCheck(*hit));
  hit->lenSq=std::nextafter(129600.0f,1e6f);
  assert(MeleeSphereNeedsScenarioCheck(*hit));
  hit->flags|=HitNoScenarioBomb;
  assert(!MeleeSphereNeedsScenarioCheck(*hit));
  shape[0].flags=HitOn|HitCube;
  rejects([&]{SphereAuthoredShapes(shape,{},identity,{0,0,0});});
  shape[0].flags=HitCube;
  assert(!SphereAuthoredShapes(shape,{},identity,{0,0,0}));
  shape[0]=sphere[0];
  shape[0].partsNo=1;
  rejects([&]{SphereAuthoredShapes(shape,{},identity,{0,0,0});});
  shape[0]=sphere[0];
  rejects([&]{SphereAuthoredShapes(shape,{},identity,{0,0,0},-1);});
  rejects([&]{SphereAuthoredShapes(shape,{},identity,{0,0,0},1200,0x2e);});
  rejects([&]{SphereAuthoredShapes(shape,{},identity,{0,0,std::numeric_limits<double>::infinity()});});
  auto invalid=identity;
  invalid[0]=std::numeric_limits<float>::quiet_NaN();
  rejects([&]{SphereAuthoredShapes(shape,{},invalid,{0,0,0});});
}
void sphereOracleSweep() {
  // All capsule axes, posed parts, model scaling, skipped parts, tie order
  // and both handgun/knife/kick filters against the unchanged source oracle.
  for (unsigned i=0;i<32768;++i) {
    const float angle=float(i%97)*.061f;
    const float scale=.75f+float(i%19)*.025f;
    const auto placement=matrix(angle,scale,{1200,860,-3900});
    std::vector<Matrix34> joints{
        matrix(-angle*.25f,1,{20,1500,0}),
        matrix(angle*.4f,1,{0,900,0}),
        matrix(angle*.7f,1,{-200,700,0})};
    std::vector<AuthoredHitShape> shapes{
        {{0,0,0},160,110,0,HitOn,1},
        {{0,-30,0},210,290,0,HitOn,2},
        {{-350,0,0},100,350,0,HitOn|HitXAxis,3},
        {{0,0,-400},150,400,0,HitOn|HitZAxis|HitIgnoreHandguns,2},
        {{0,0,0},300,0,0,0,1}};
    const CollisionVec center{1200+float(int(i%41)-20)*87,
        860+float(i%31)*61,-3900+float(int((i/41)%31)-15)*91};
    const float radius=i%2?1200:float(i%17)*51;
    const unsigned weapons[]{1,0x10,0x14};
    compareSphere(shapes,joints,placement,center,radius,weapons[i%3]);
  }
}
void damageAndKnifeRng() {
  assert(KnifeDamage(false)==unsigned(source_oracle::Dmg_tbl_em10[0x10]));
  assert(KnifeDamage(true)==unsigned(float(source_oracle::Dmg_tbl_em10[0x10])*1.2f));
  assert(KickDamage(false)==unsigned(source_oracle::Dmg_tbl_em10[0x14]));
  assert(KickDamage(true)==unsigned(float(source_oracle::Dmg_tbl_em10[0x14])*1.2f));
  assert(KnifeDamage(false)==100 && KnifeDamage(true)==120);
  assert(KickDamage(false)==200 && KickDamage(true)==240);
  for (unsigned seed=0;seed<65536;++seed) {
    RndInit(std::uint16_t(seed));
    (void)Rnd();(void)Rnd(); // Exact two LifeDownSet2 bytes first.
    const bool expected=Rnd()%3!=0;
    const auto end=RndState();
    RndInit(std::uint16_t(seed));
    (void)Rnd();(void)Rnd();
    assert(KnifeBodyFlinches()==expected);
    assert(RndState()==end);
  }
}
void blowSourceOracle() {
  std::array<std::uint64_t,256> counts{};
  const std::array<CollisionVec,4> sources{{{0,0,1000},{0,0,-1000},
                                          {1000,0,0},{-1000,0,0}}};
  std::uint64_t cases=0;
  for (unsigned seed=0;seed<65536;++seed)
    for (bool alive : {false,true})
      for (bool parasite : {false,true})
        for (const auto& from : sources) {
          source_oracle::cEm10 em;
          source_oracle::FREE_EM10 work;
          em.pos={0,0,0};
          em.ang={0,float(int(seed%5)-2)*.13f,0};
          em.hp=alive?1000:0;
          em.dmg.m_PosFrom={float(from[0]),float(from[1]),float(from[2])};
          work.Be_flg=parasite?0x80:0;
          RndInit(std::uint16_t(seed));
          source_oracle::selectBlow(&em,&work);
          const auto end=RndState();
          RndInit(std::uint16_t(seed));
          const auto got=SelectMeleeBlow({0,0,0},float(int(seed%5)-2)*.13f,
                                         from,alive,parasite);
          assert(got.motionEntry==em.Motion.motion);
          assert(got.sequenceEntry==em.Motion.sequence);
          assert(got.blendFrames==em.Motion.hokan);
          assert(got.flags==em.Motion.flags);
          assert(got.mirrored==bool(em.Motion.flags&0x40));
          assert(got.downFacing==bool(work.Be_flg&0x20));
          assert(got.heading==em.ang.y);
          assert(got.rootScale==work.TmpF);
          assert(got.fallSpeed==CollisionVec({work.Spd.x,work.Spd.y,work.Spd.z}));
          assert(RndState()==end);
          ++counts[got.motionEntry];
          ++cases;
        }
  for (unsigned raw : {55,59,61,63,71,73}) assert(counts[raw]>0);
  assert(cases==1048576);
  RndInit(123);
  const auto old=RndState();
  rejects([]{SelectMeleeBlow({0,0,0},0,{0,std::numeric_limits<double>::quiet_NaN(),0},true);});
  assert(RndState()==old);
  std::cout << "Melee rules PASS: original XZ/facing/height boundaries; "
               "32768 exact posed capsule oracle cases; 65536 knife RNG cases; "
            << cases << " original blow selector/RNG cases\n";
}
} // namespace
int main() {
  promptBounds();
  sphereBoundaries();
  sphereOracleSweep();
  damageAndKnifeRng();
  blowSourceOracle();
}
