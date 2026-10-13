#pragma once
#include "consumer_original.h"
#include <cmath>
#include <memory>
#include <new>

namespace re4::port {
using cModelData = ModelDataView;
using cModelInfo = ModelInfoView;
using cBoundingBox = ModelBoundingBox;
using TEXPalette = TexturePaletteView;
void calcModelAddr(ModelDataView *);
void calcTplAddr(TexturePaletteView *);
void getBoundingBox(ModelDataView *, ModelBoundingBox *);
void RotMatrix(Mtx, Vec *);
void TransMatrix(Mtx, Vec *);
void ScaleMatrix(Mtx, Vec *);
void C_MTXIdentity(Mtx);
void C_MTXCopy(const Mtx, Mtx);
void C_MTXConcat(const Mtx, const Mtx, Mtx);
void C_MTXTrans(Mtx, f32, f32, f32);
void C_MTXScale(Mtx, f32, f32, f32);
void C_MTXRotRad(Mtx, char, f32);
void C_MTXRotTrig(Mtx, char, f32, f32);
void C_MTXMultVec(const Mtx, const Vec *, Vec *);
void C_VECAdd(const Vec*,const Vec*,Vec*);
void C_VECSubtract(const Vec*,const Vec*,Vec*);
f32 C_VECMag(const Vec*);

// Native storage/lifetime adapter, with original cManager allocation/list bodies.
template <class T> class ModelPool {
public:
    explicit ModelPool(u32 count) : pArray(nullptr), nArray(count), size(sizeof(T)),
        storage(count ? std::make_unique<T[]>(count) : nullptr) {
        pArray=storage.get();
        for (u32 i=0;i<count;++i) pArray[i].be_flag=0;
    }
    T *pArray; u32 nArray; std::size_t size;
    u8 flag{}; T *pAlive{}; u32 maxAlive{}, warnDiv=10; const char *name="native model pool";
    T *create(int); T *create(); T *create(int,u32);
    u32 countActiveWork(); int deleteList(T *); void addListFront(T *); void destroy(T *);
    T *fastAt(u32 n) { return n<nArray ? pArray+n : nullptr; }
    bool owns(T *p) const {
        auto a=reinterpret_cast<std::uintptr_t>(p), b=reinterpret_cast<std::uintptr_t>(pArray);
        return p && a>=b && a-b<std::size_t(nArray)*size && (a-b)%size==0;
    }
    void memClear(T *p,std::size_t) { *p=T{}; p->be_flag=0; }
    int construct(T *p,u32) { new(p) T{}; return 1; }
    template<class... A> void log(const char *, A...) {} // Original diagnostic-only warning sink.
private:
    std::unique_ptr<T[]> storage;
};
class ModelInfoManager : public ModelPool<ModelInfoView> {
public:
    using ModelPool::ModelPool;
    ModelInfoView *create(void *,void *);
};
class ModelPartsManager : public ModelPool<ModelPart> {
public:
    using ModelPool::ModelPool;
    ModelPart *createSequential(u32);
};
ModelPart *PartsMgrWork(ModelPartsManager *,u32);
[[noreturn]] inline void notBinData() { throw std::invalid_argument("original BIN version rejected"); }

class ModelRuntime {
public:
    struct Asset;
    struct Object;
    ModelRuntime(u32,u32);
    ~ModelRuntime();
    std::unique_ptr<Object> initialize(ResourceLease,ResourceLease,bool,u8,u32,std::shared_ptr<ModelCpu> = {});
    ModelInfoView *createAttachment(Object &,ResourceLease,ResourceLease,bool resetOffsets);
    ModelInfoManager infos;
    ModelPartsManager parts;
};
struct ModelRuntime::Object {
    std::shared_ptr<ModelCpu> owner;
    ModelCpu &cpu;
    explicit Object(std::shared_ptr<ModelCpu>);
    std::unique_ptr<Asset> asset;
    std::vector<std::unique_ptr<Asset>> attachments;
    ~Object();
};
} // namespace re4::port
#define PSMTXIdentity C_MTXIdentity
#define PSMTXCopy C_MTXCopy
#define PSMTXConcat C_MTXConcat
#define PSMTXTrans C_MTXTrans
#define PSMTXScale C_MTXScale
#define PSMTXRotRad C_MTXRotRad
#define PSMTXMultVec C_MTXMultVec
#define ASSERTMSGLINE(line,condition,message) do { if (!(condition)) throw std::invalid_argument(message); } while(false)
#define U8Set(target,value) ((target)=(value))
#ifndef FALSE
#define FALSE 0
#endif
