#pragma once
#include "re4/port/collision.h"
#include "motion_original.h"
#include <vector>
namespace re4::port::collision {
struct cSatBlock {
    Vec min{},m_Size{};
    u16 m_nFloor{},m_nSlope{},m_nWall{},m_Flag{};
    cSatBlock *m_pList{},*children{};
    u16 *idx{};
    int lineOverlap(Vec*,Vec*,Vec*);
    int hitCheckSphere(Vec*,Vec*,f32);
};
struct cSat {
    Vec *vtx{},*norm_p{},*edge_p{};
    SAT_POLY *poly_p{};
    u16 vertex_num{},normal_num{},edge_num{},polygon_num{},bb_num{};
    cSatBlock *block_p{};
    Mtx mat{},imat{};
    bool enabled=true;
    int isAlive() { return enabled; }
    void setCoord(Vec*,Vec*);
    void setMatrix(Mtx);
};
struct cSatMgr {
    NativeGameState *pG;
    std::vector<cSat*> pieces;
    u32 nArray{};
    int type{};
    cSat *fastAt(u32 n) { return pieces.at(n); }
    int hitCheck2(Vec*,Vec*,Vec*,Vec**,int,int);
    int hitCheck(Vec*,Vec*,Vec*,Vec*,int,int);
    f32 getFloor(Vec*,Vec**,f32,f32,int);
    int polySphereCk(Vec*,Vec*,f32,int,Vec*,int);
    int check(ModelCpu*,int);
    int checkRect(ModelCpu*);
    int checkAir(ModelCpu*,int);
    f32 scrAtCheckSphere(ModelCpu*,ActorCollisionInfo*,int);
    f32 scrAtCheckSphereAir(ModelCpu*,ActorCollisionInfo*,int);
    void wallAdjust(Vec*,Vec*,Vec*,f32,int,int);
    void adjust(Vec*,Vec*,Vec*,f32,int,int);
};
using cAtariInfo=ActorCollisionInfo;
extern thread_local cSatMgr *activeManager;
struct CollisionLog { u64 warnings{}; void warn(int,int,const char*,f32,f32,f32); };
extern thread_local CollisionLog *pLog;
int atck(Vec*,Vec*,Vec*,ModelCpu*,int);
void at_pos_calc(ModelCpu*,Vec*);
[[noreturn]] inline void Draw_line3d(Vec*,Vec*,u32,int) { throw MissingGameConsumer("SAT floor debug line renderer"); }
[[noreturn]] inline void Draw_sphere(Vec*,f32,u32,int,int) { throw MissingGameConsumer("SAT sphere debug renderer"); }
extern thread_local NativeGameState *pG;
extern thread_local int SEck;
extern thread_local u8 polyBit[0x400];
extern thread_local cSat *pBypassAt;
f32 C_VECDotProduct(const Vec*,const Vec*);
f32 GetDistance(Vec*,Vec*);
u32 Get_poly_attr(SAT_POLY*);
void InterVectorXYZ(Vec*,Vec*,Vec*,f32);
u32 At_poly_line_ck(cSat*,Vec*,SAT_POLY*,Vec*,Vec*,u32,u32);
int blkPolyLineCk(cSat*,cSatBlock*,Vec*,Vec*,int,int,Vec*,Vec**);
int blkPolyLineCkCore(cSat*,cSatBlock*,Vec*,Vec*,int,int,Vec*,Vec**);
f32 C_VECSquareDistance(const Vec*,const Vec*);
f32 At_surface_point_rel(Vec*,Vec*,Vec*);
int At_surface_line_ck(Vec*,Vec*,Vec*,Vec*,Vec*);
int At_poly_point_rel(Vec*,Vec*,Vec*);
u32 At_poly_sphere_ck2(Vec*,Vec*,u32,Vec*,Vec*,f32,u32,u32);
u32 At_poly_sphere_ck(cSat*,SAT_POLY*,Vec*,Vec*,f32,u32,u32);
void polyBitSet(u32);
int polyBitCk(u32);
int blkPolySphereCkCore(cSat*,cSatBlock*,Vec*,Vec*,f32,int,Vec*,int);
int blkPolySphereCk(cSat*,cSatBlock*,Vec*,Vec*,f32,int,Vec*,int);
} // namespace re4::port::collision
namespace re4::port { void RotVector(Vec*,Vec*,Vec*); void low_RotMatrix(Mtx,Vec*); }
#undef PSVECDotProduct
#define PSVECDotProduct C_VECDotProduct
#define PSVECSquareDistance C_VECSquareDistance
#define PSVECSquareMag C_VECSquareMag
#define PSVECNormalize C_VECNormalize
#define DbgFlagChk(g,n) (((g)->Debug_flg[(n)/32] & (0x80000000u>>((n)%32)))!=0)
#define SINF std::sin
#define COSF std::cos
