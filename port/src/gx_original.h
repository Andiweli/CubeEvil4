#pragma once
#include "motion_original.h"
#include "re4/port/gx_scene.h"
namespace re4::port {
using Point3d=Vec;
using Mtx44=float[4][4];
using CAMERA=GxCamera;
void C_MTXRotAxisRad(Mtx,const Vec*,float);
void C_MTXLookAt(Mtx,const Vec*,const Vec*,const Vec*);
void C_MTXPerspective(Mtx44,float,float,float,float);
void CameraSetOrientationRoll(CAMERA*);
void CameraSetOrientationUp(CAMERA*);
void MTXSetColumns(Mtx,Vec&,Vec&,Vec&,Vec&);
void getColumn(Mtx,int,Vec*);
void C_VECAdd(const Vec*,const Vec*,Vec*);
u32 CameraSequenceCtrl(ModelMotion*);
// Native owned work; no GameCube 0x200-byte object overlay or destructor poison.
struct DiagnosticCameraMotion : GxCamera {
    ModelMotion m_info;
    Mtx *m_p_base_mat{};
    int m_state{};
    void move();
};
}
#define PSMTXRotAxisRad C_MTXRotAxisRad
#define VECCrossProduct C_VECCrossProduct
#define CAM_HIST(w,i) ((w)->Key_hist[i])
#define MTXDegToRad(x) ((x)*0.01745329252f)
#ifndef PI
#define PI 3.1415927f
#endif
