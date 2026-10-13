#pragma once
#include "re4_cloth_types.h"
#include "re4/port/game_runtime.h"
#include <cmath>
#include <functional>
namespace re4::port {
void C_MTXMultVec(const Mtx,const Vec*,Vec*);
void motionMultVecSR(const Mtx,const Vec*,Vec*);
void C_VECAdd(const Vec*,const Vec*,Vec*);
void C_VECSubtract(const Vec*,const Vec*,Vec*);
void C_VECNormalize(const Vec*,Vec*);
f32 ikDistance(Vec*,Vec*);
void motionVecScale(const Vec*,Vec*,f32);
void C_VECCrossProduct(const Vec*,const Vec*,Vec*);
f32 ikDotProduct(const Vec*,const Vec*);
void ikRotAxisRad(Mtx,const Vec*,f32);
void C_MTXConcat(const Mtx,const Mtx,Mtx);
u32 C_MTXInverse(const Mtx,Mtx);
void RotMatrix(Mtx,Vec*);void TransMatrix(Mtx,Vec*);void ScaleMatrix(Mtx,Vec*);
void RotVector(Vec*,Vec*,Vec*);
f32 motionLimitAngle(f32);
// The original locked-cache payload has no pointers on disc. Only its native
// frame owner changes: bounded typed storage replaces address 0xE0000000.
struct ClothAtData {int Type{};Mtx m{},im{};Vec Pos{},Pos2{};f32 R{},Len{};};
static_assert(sizeof(ClothAtData)==0x84,"original scalar cloth collision payload");
struct ClothAtWork {int num{};ClothAtData *pAt{};std::array<ClothAtData,256> at{};};
struct ClothDomain {
    NativeGameState *pG;
    ClothInfo leonHair{},leonJacket{},leonHolster{};
    ClothAtWork scratch;
    std::function<f32(Vec*,int,f32,f32,int)> floor;
    Vec GlobalWind{0,0,20};f32 GlobalWindAdd=1.0471976f;
    u32 moves{},resets{},volumeBuilds{};
    explicit ClothDomain(NativeGameState &g):pG(&g){}
    void validate(ModelCpu*,const ClothInfo*) const;
    void PenClothSet(ModelCpu*,ClothInfo*,f32);
    void PenClothFixSet(ModelCpu*,ClothInfo*,int,Vec*);
    void PenClothFixClear(ModelCpu*,ClothInfo*,int);
    void PlClothSetLeon(ModelCpu*,ClothInfo*,ClothInfo*,ClothInfo*);
    void testHairSetLeon(ModelCpu*,ClothInfo*);
    void testJacketSetLeon(ModelCpu*,ClothInfo*);
    void testHolsterSetLeon(ModelCpu*,ClothInfo*);
    void PlClothMoveLeon(ModelCpu*,ClothInfo*,ClothInfo*,ClothInfo*);
    void testHairMoveLeon(ModelCpu*,ClothInfo*);
    void testJacketMoveLeon(ModelCpu*,ClothInfo*);
    void testHolsterMoveLeon(ModelCpu*,ClothInfo*);
    void PenClothMove(ModelCpu*,ClothInfo*);void PenClothMove3(ModelCpu*,ClothInfo*);
    void PenClothReset(ModelCpu*,ClothInfo*);
    void PenWindSet(f32,f32,f32);
    ClothAtWork *penClothAtMake(ModelCpu*,ClothAtSet*,int);
    int penClothAtCk(Vec*,Vec*,ClothAtWork*);
    int penClothAtCkBorder(Vec*,Vec*,ClothAtWork*);
    void penClothAtCkParallel(Vec*,Vec*,ClothAtWork*);
    void penWindScale(Vec *v,f32 rate){motionVecScale(&GlobalWind,v,rate);}
    f32 getFloor(Vec *v,int id,f32 above,f32 below,int mask) const {
        if(!floor)throw MissingGameConsumer("PenClothMove/native SAT floor owner");
        return floor(v,id,above,below,mask);
    }
    static void PosToPos(Vec *a,Vec *b,Vec *out,f32 rate);
    template<class... A> [[noreturn]] static void Draw_line3d(A...){throw MissingGameConsumer("cloth debug line presentation");}
    template<class... A> [[noreturn]] static void Draw_sphere(A...){throw MissingGameConsumer("cloth debug sphere presentation");}
    template<class... A> [[noreturn]] static void Draw_cylinderMtx(A...){throw MissingGameConsumer("cloth debug cylinder presentation");}
    void initialize(ModelCpu*);
    void clear(){leonHair={};leonJacket={};leonHolster={};scratch={};moves=resets=volumeBuilds=0;}
};
} // namespace re4::port
