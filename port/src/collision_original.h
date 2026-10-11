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
};
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
} // namespace re4::port::collision
#undef PSVECDotProduct
#define PSVECDotProduct C_VECDotProduct
#define DbgFlagChk(g,n) (((g)->Debug_flg[(n)/32] & (0x80000000u>>((n)%32)))!=0)
