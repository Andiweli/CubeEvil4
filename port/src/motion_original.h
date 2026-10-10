#pragma once
#include "model_original.h"
#include "re4/port/motion.h"
#include <cstring>
namespace re4::port {
using cModel=ModelCpu;
using MOTION_INFO=ModelMotion;
using MotionData=MotionDataView;
using ATTACH_CAMERA=MotionCamera;
using SEQUENCE_DATA=SequenceKey;
struct HERMITE_SET { f32 Frame{},Frame_max{}; u32 Attr{}; u8 Data_fmt{}; u8 *pData{}; };
using FccGetData=void(*)(u8*,int,int,f32*,f32*);
u16 motionBE16(const u8*);
f32 motionBEFloat(const u8*);
f32 motionBES16(const u8*);
void IKInit(cModel*,MOTION_INFO*);
void VecRadLimit(f32*);
f32 hermite(f32*,f32*,f32);
void SetOrientationZY(Vec*,Vec*,Mtx);
int HermiteInterpolation(HERMITE_SET*,Vec*,u16*);
int Fcc_next_axis_addr(int,int);
void C_MTXTranspose(const Mtx,Mtx);
u32 C_MTXInverse(const Mtx,Mtx);
void C_VECSubtract(const Vec*,const Vec*,Vec*);
void C_VECNormalize(const Vec*,Vec*);
void C_VECCrossProduct(const Vec*,const Vec*,Vec*);
f32 C_VECMag(const Vec*);
f32 C_VECSquareMag(const Vec*);
inline constexpr Vec vecZero{0,0,0};
inline void memclr_asm(void *p,std::size_t n) { std::memset(p,0,n); }
struct MotionCameraBoundary {
    [[noreturn]] void registAttachCamera(ATTACH_CAMERA*,cModel*) { throw MissingGameConsumer("MotionSetCore/CameraControl::registAttachCamera"); }
    [[noreturn]] void deleteAttachCamera(ATTACH_CAMERA*,cModel*) { throw MissingGameConsumer("MotionSetCore/CameraControl::deleteAttachCamera"); }
};
inline MotionCameraBoundary CamCtrl;
#define FCC_DECL(name) void name(u8*,int,int,f32*,f32*)
FCC_DECL(Fcc_get_data_000); FCC_DECL(Fcc_get_data_001); FCC_DECL(Fcc_get_data_002);
FCC_DECL(Fcc_get_data_010); FCC_DECL(Fcc_get_data_011); FCC_DECL(Fcc_get_data_012);
FCC_DECL(Fcc_get_data_020); FCC_DECL(Fcc_get_data_021); FCC_DECL(Fcc_get_data_022);
FCC_DECL(Fcc_get_data_033); FCC_DECL(dummy);
#undef FCC_DECL
} // namespace re4::port
#define FCC_F32(dst,i) (dst)=motionBEFloat(d+(i))
#define FCC_S16(dst,i) (dst)=motionBES16(d+(i))
#define FCC_S8(dst,i) (dst)=f32(s8(d[(i)]))*0.0001f
#define IK_FLAGS(p) ((p)->motParts.flags)
#define BIND_X(p) ((p)->lt_inv_mat[0][3])
#define BIND_Y(p) ((p)->lt_inv_mat[1][3])
#define BIND_Z(p) ((p)->lt_inv_mat[2][3])
#define PSVECSubtract C_VECSubtract
#define PSVECMag C_VECMag
#define PSVECCrossProduct C_VECCrossProduct
#define VECNormalize C_VECNormalize
#define PSMTXTranspose C_MTXTranspose
#define PSMTXInverse C_MTXInverse
#define MTX_COPY_DOWN(src,dst) C_MTXCopy(src,dst)
#define PI 3.1415927f
#define PI2 6.2831855f
#define SET_REG_FIELD(line,reg,width,shift,value) ((reg)=((reg)&~(((u32(1)<<(width))-1)<<(shift)))|((u32(value)&((u32(1)<<(width))-1))<<(shift)))
#define GX_WRITE_RAS_REG(reg) writeRas(reg)
