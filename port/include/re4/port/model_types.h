#pragma once
#include "re4/port/engine_loader.h"
#include "re4_runtime_types.h"
#include "re4/port/actor_collision.h"
#include <array>

namespace re4::port {
using Mtx = f32[3][4];
using MtxPtr = f32 (*)[4];
struct LightCpu;
struct ModelCpu;
struct CameraCpu;
struct ModelLightInfo {
    Mtx imat{};
    LightCpu *pLight[8]{};
    u8 EnableMask{},Flag{};
    s8 PartsNo{};
    u8 x53{};
    u32 SelectMask{};
    Vec Offset{},Size{};
    f32 Radius{};
    ModelLightInfo();
    int init2(int,int,const Vec*,const Vec*,int);
    u32 getLightNum();
    void updateMatrix(ModelCpu*);
    u8 getType() const {return Flag&3;}
};
// Native counterparts of the named original fields; never wire-layout overlays.
struct ModelUnit {
    u32 be_flag = 1;
    ModelUnit *next{};
    int isEmpty() const { return !(be_flag & 0x601); }
    int isAlive() const { return (be_flag & 0x201) == 1; }
    int setDieRequest() { return be_flag |= 0x600; }
    int setDieRequest2() { return be_flag |= 0x200; }
    ModelUnit *getNext() const { return next; }
    void setNext(ModelUnit *n) { next = n; }
};
struct ModelJoint { u8 partsNo{}, parentNo{}, x2{}, x3{}; Vec center{}; };
struct ModelDataView {
    ResourceLease source; // Native ownership metadata, never serialized into the original BIN.
    ModelJoint *pHead{};
    u8 nParts{}, shift{}, weight_palette_num{};
    u16 weight_ext_num{};
    u16 nVtx{}, nNrm{}, displist_num{};
    u32 version{}, shapeOfs{}, flags{}, nTex{};
    const u8 *pClr{}, *pTex{}, *pWeight{}, *pParts{}, *nrmOrig{};
    s16 *vtxOrig{};
    u16 *blendTbl{};
    u8 *flipTbl{};
    bool resolved{};
};
struct TextureHeaderView {
    u16 height{}, width{};
    u32 format{}, wrapS{}, wrapT{}, minFilter{}, magFilter{};
    f32 LODBias{};
    u8 edgeLODEnable{}, minLOD{}, maxLOD{}, unpacked{};
    ResourceLease data;
};
struct TexturePaletteView {
    ResourceLease bytes;
    u32 version{}, numDescriptors{};
    std::vector<TextureHeaderView> descriptors;
    bool resolved{};
};
struct ModelBoundingBox { Vec offset{}, size{}; };
struct ShapeData;
struct ShapeKey { f32 rate{}; ShapeData *data{}; };
struct ModelInfoView : ModelUnit {
    ModelInfoView();
    ModelDataView *model_addr{};
    void *tpl_addr{}; // Original declared void*; points to checked native TexturePaletteView.
    ModelInfoView *pList{};
    ModelBoundingBox bound;
    Mtx mat{};
    union { u8 color[4]; u32 colorWord; };
    u8 color2[4]{}; // Original secondary material colour; zeroed by model-pool allocation.
    f32 invisible_factor{};
    ShapeData *pShape{};
    ShapeKey shape[5]{};
    u32 shapeFlags{};
    s16 shape_frame{};
};
struct SequenceKey { u16 Frame{}; u8 Se{}, Free{}; };
struct MotionDataView;
struct MotionCamera {
    u8 parts[5]{}, type{}, frame{}, pad{};
    Mtx *p_mat{};
    Mtx mat{};
    Vec camera_data[5]{};
    u16 history[5][3]{};
};
// All named MOTION_INFO fields from include/model.h; pointer widths are native.
struct ModelMotion {
    MotionDataView *pMot{};
    const u8 **pHermite_data{};
    u16 Key_hist[4][3]{};
    f32 Mot_frame_max{}, Mot_frame{}, Mot_frame_sav{}, Mot_frame_old{};
    u8 Joint_num{};
    u8 *pJoint_no{};
    u16 *pJoint_kind{};
    u16 Null_pos{}, Null_rot{}, Mot_attr{}, Mot_state{};
    u32 Mot_flag{};
    Vec Pos{}, Pos_old{}, Pos_dist{}, Pos_world{}, Pos_move_old{}, Ang{}, Ang_old{}, Ang_dist{};
    SequenceKey *pSeq_top{};
    SequenceKey Seq{}, Seq_old{}, Seq_old2{};
    f32 Seq_frame{};
    u16 Seq_frame_num{};
    f32 Seq_speed{};
    u8 Hokan_frame{}, Hokan_cnt{};
    f32 Brate{};
    MotionCamera *pAttachCam{};
};
struct ModelCoord : ModelUnit {
    ModelCoord();
    Vec pos{}, ang{}, scale{}, r_scale{}, world{}, world_old{}, world_old2{};
    Mtx mat{}, l_mat{}, prevMat{};
    ModelCoord *pParent{};
    void matCalc();
};
} // namespace re4::port
#include "re4_pendulum_types.h"
namespace re4::port {
struct ModelPart : ModelCoord {
    PendulumState Pen;
    ModelPart *pList{};
    Mtx lt_inv_mat{};
    Vec inv_offset{};
    f32 length{};
    Vec ik_dir{}, up_vector{};
    Mtx ik_inv_mat{};
    struct { Vec pos{}, rot{}, scale{}; f32 ikAng{}; u16 hist[6][3]{}; u32 flags{}; } motParts;
    u16 (*getKeyHist())[3] { return motParts.hist; }
};
class ModelInfoManager;
class ModelPartsManager;
struct ModelCpu : ModelPart {
    ModelCpu();
    u32 guid{};
    u8 nParts{}, kindid{}, ot_type{}, z_mode{}, alpha_omit{}, TevScaleGroup{}, CullMode{},
        Shader_type{}, Refract_pow{}, Fix_parts{}, r_no_0{}, r_no_1{}, r_no_2{}, r_no_3{}, id{}, type{};
    Vec speed{}, pos_old{}, Wall_norm{}, Fix_pos{};
    Vec *pFloor_norm{};
    ActorCollisionInfo atari;
    Vec nativeFloorNormal{}; // Stable copy of the borrowed SAT normal, never serialized.
    void *pCldShMd{}, *pPath{}, *pTexChg{};
    u32 Shd_color{}, inscreen_pos{};
    u8 invisible_trg{}, invisible_old{}, invisible_mode{}, invisible_busy{}, invisible_timer{};
    f32 invisible_factor{}, invisible_factor2{};
    ModelLightInfo LightInfo;
    ModelInfoView *pModelInfo{}, *pShadowModelInfo{};
    ModelMotion Motion{}, *pMotionB{};
    u16 *pDblJnt{}, *pXFlip{};
    ModelInfoManager *mm{};
    ModelPartsManager *pm{};
    bool systemInvisible{};
    CameraCpu *attachCameraRegistry{}; // Native borrower, removed before model destruction.
    ModelInfoView *modelInit(void *, void *); // Original int address return becomes typed pointer.
    int initJoint(void *);
    void releaseJoint();
    void setPartsOffset(void *);
    void partsMatCalc();
    void partsWorldCalc();
    void matBlend(f32);
    void addModel(ModelInfoView *);
    int deleteModelInfo(ModelInfoView *);
    void releaseModelInfo();
    int makePartsList(int);
    void setJointInfo(void *);
    void releasePartsList(int);
    void setPartsParent();
    ModelPart *getPartsPtr(int);
    void setNoSuspend(int);
    int isTrans();
    void updateOldPos();
    void setPos(Vec*);
    void matUpdate();
    void setRno(int,int,int,int);
};
} // namespace re4::port
