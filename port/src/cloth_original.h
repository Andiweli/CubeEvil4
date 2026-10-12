#pragma once
#include "re4_cloth_types.h"
#include <cmath>
namespace re4::port {
void C_MTXMultVec(const Mtx,const Vec*,Vec*);
void motionMultVecSR(const Mtx,const Vec*,Vec*);
void C_VECAdd(const Vec*,const Vec*,Vec*);
void C_VECSubtract(const Vec*,const Vec*,Vec*);
void C_VECNormalize(const Vec*,Vec*);
f32 ikDistance(Vec*,Vec*);
struct ClothDomain {
    NativeGameState *pG;
    ClothInfo leonHair{},leonJacket{},leonHolster{};
    explicit ClothDomain(NativeGameState &g):pG(&g){}
    void validate(ModelCpu*,const ClothInfo*) const;
    void PenClothSet(ModelCpu*,ClothInfo*,f32);
    void PenClothFixSet(ModelCpu*,ClothInfo*,int,Vec*);
    void PenClothFixClear(ModelCpu*,ClothInfo*,int);
    void PlClothSetLeon(ModelCpu*,ClothInfo*,ClothInfo*,ClothInfo*);
    void testHairSetLeon(ModelCpu*,ClothInfo*);
    void testJacketSetLeon(ModelCpu*,ClothInfo*);
    void testHolsterSetLeon(ModelCpu*,ClothInfo*);
    void initialize(ModelCpu*);
    void clear(){leonHair={};leonJacket={};leonHolster={};}
};
} // namespace re4::port
