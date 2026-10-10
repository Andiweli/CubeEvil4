#pragma once
#include "motion_original.h"
#include "re4/port/skinning.h"
namespace re4::port::skin {
using cModel=ModelCpu;
using cPartsWk=ModelPart;
struct cWeight { u8 WeightId[3], nWeight, Weight[3], padding; };
struct cWeightExt { u16 WeightId[3], nWeight; u8 Weight[3], padding; };
static_assert(sizeof(cWeight)==8 && sizeof(cWeightExt)==12);
struct GxWork { Mtx mtx[248]{}; };
struct ReorderedMatrix { float lanes[4][3]{}; };
static_assert(sizeof(ReorderedMatrix)==48);
struct Context {
    GxWork work;
    Mtx (&mtxPalette)[248]=work.mtx;
    std::vector<ReorderedMatrix> reordered;
};
extern thread_local Context *pG;
inline ConsumerLog *pLog=&consumerLog;
inline GxWork *weightWork() { return &pG->work; }
void PSMTXReorder(const Mtx, float (*)[3]);
void calcWeightMat(cModel *);
int MakeWeightPalette(cWeight *,int);
int MakeWeightPaletteExt(cWeightExt *,int);
} // namespace re4::port::skin
#define GXWORK() weightWork()
#define PTR_INVALID(p) (!(p))
#define PSQ_L_U8_TO(dst,src) ((dst)=float(*(src)))
