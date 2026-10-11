#pragma once
#include "gx_original.h"
#include "re4/port/gx_pipeline.h"
#include "re4/port/lights.h"
#include "gx_state_types.h"
#include <deque>
#include <cstdarg>

namespace re4::port::gx {
constexpr int OT_MAX=23;
struct OtData { void *data{};void (*func)(void*){};OtData *next{};u16 kind{}; };
struct OtPrim { OtData ot;void *data{}; };
struct OtWork { OtData *list{};u16 max{},prev_kind{}; };
struct OtMirrorWork { std::array<u8,28> bytes{}; };
extern thread_local OtWork g_OtWork[OT_MAX];
extern thread_local OtMirrorWork g_OtMirrirWk[2];
extern thread_local int g_NowExecOtType;
extern thread_local float OT_MUL;
struct Log { [[noreturn]] void err(int,int,const char*,...);[[noreturn]] void warn(int,int,const char*,...); };
extern thread_local Log *pLog;
void *allocateBuckets(std::size_t);
void *GetPrimBuff(int);
using re4::port::memclr_asm;
void InitOt();void ClearOt();void clearOtWork(OtWork *);void CrearOtMirrorWork();
OtData *MakeOtData(void*);
int AddOtDirect(int,void*,void(*)(void*),u32,u16,Vec*,float);
int AddOtModelPosRadius(void*,void(*)(void*),Vec*,float,u16,float);
int AddOtWorldPosRadius(void*,void(*)(void*),Vec*,float,u16,float);
void CameraGetLookVecInverse(GxCamera*,Vec*);
float C_VECDotProduct(const Vec*,const Vec*);
u16 depthBucket(float);
int ExecOt(int);u16 OtGetPrevKind();void DeleteOtData(u32,u32);
struct GEOM_SPHERE { Vec pos{};float radius{}; };
struct GEOM_HEXAHEDRON { GxCamera *camera{}; };
struct Context { GxCamera Camera;NativeGameState state; };
extern thread_local Context *pG;
GEOM_HEXAHEDRON *CameraViewFrustumPtr(GxCamera *);
int collision_sphere_hexahedron(GEOM_SPHERE*,GEOM_HEXAHEDRON*);

struct cPolyHeader { u8 flag{},material_tex{},alpha_tex{},alpha_omit{},bump_tex{},specular_tex{},specular_r{},specular_g{},specular_b{},specular_type{},specular_decay{},specTexOrg{}; };
struct ModelTexInfo {
    u8 flags{},blendType{};u16 blendRatio{},frame{};
    float u{},v{};u8 *blendTbl{},*anim{};
};
struct cModelData { u32 flags{}; };
struct cModelInfo { ModelTexInfo tex;u8 flagsDC{};u8 color[4]{255,255,255,255},color2[4]{};u32 be_flag{};cModelData *model_addr{}; };
struct cModel;
struct cLight:LightFields { u32 be_flag{};void getPos(Vec *dst){*dst=World;}int getNormal(Vec*,Vec*);int isParent(cModel*); };
struct GXLightObj { GXColor color{};float a[3]{},k[3]{},lpos[3]{},ldir[3]{}; };
using __GXLightObjInt_struct=GXLightObj;
using GXSpotFn=int;using GXDistAttnFn=int;
constexpr int GX_SP_OFF=0,GX_SP_FLAT=1,GX_SP_COS=2,GX_SP_COS2=3,GX_SP_SHARP=4,GX_SP_RING1=5,GX_SP_RING2=6;
constexpr int GX_DA_OFF=0,GX_DA_GENTLE=1,GX_DA_MEDIUM=2,GX_DA_STEEP=3;
struct cCoord { Mtx mat{};Vec world{},pos{}; };
struct cLightInfo {
    Mtx imat{};cLight *pLight[8]{};u8 Flag{},EnableMask{},x53{};s8 PartsNo{};u32 SelectMask{};Vec Offset{},Size{};float Radius{};
    cLightInfo();int init2(int,int,const Vec*,const Vec*,int);void updateMatrix(cModel*);cCoord *getPos(cModel*,Vec*);int getType()const{return Flag&3;}
};
using LightPart=cCoord;
struct cModel:cCoord {
    u32 be_flag{};u8 alpha_omit=255,TevScaleGroup{};
    cLightInfo LightInfo;LightPart *pList{};cModelInfo *pModelInfo{};
    u8 AddAmb_r{},AddAmb_g{},AddAmb_b{};
    u8 id=2,kindid=2,Attribute{};Vec ang{},scale{1,1,1};
    u8 Shader_type{},Refract_ratio=255;
    ModelCpu *source{};cModelState State;
    cCoord *getPartsPtr(int n){if(n!=0||!pList)throw MissingGameConsumer("GX diagnostic additional model part");return pList;}
    [[noreturn]] void error(){throw std::runtime_error("original light selection exceeds eight lights");}
};
using cEm=cModel;
using cObjScr=cModel;
struct LightEnv {
    cLightEnv environment{};std::vector<cLight> slots;u32 nArray{},kindFlags[8]{};
    cLightEnv *getEnvPtr(){return &environment;}cLight *fastAt(u32 n){return &slots.at(n);}
    int checkKind(u8);void setModel2(cModel*);
};
extern thread_local LightEnv LightMgr;
extern thread_local Vec obj_pos;
extern thread_local float obj_size;
extern thread_local int obj_flag;
extern thread_local GXLightObj lightObjBlack;
inline constexpr GXColor colZero{0,0,0,0};
void LightSetInit();void LightSetModel(cModel*);void LightDisable();void lightSetAmbient(GXColor*);
void GXInitLightColor(GXLightObj*,GXColor);void GXInitLightPos(GXLightObj*,float,float,float);
void GXInitLightDir(GXLightObj*,float,float,float);void GXInitLightAttn(GXLightObj*,float,float,float,float,float,float);
void GXInitLightSpot(GXLightObj*,float,GXSpotFn);void GXInitLightDistAttn(GXLightObj*,float,float,GXDistAttnFn);void GXInitLightAttnK(GXLightObj*,float,float,float);
void GXLoadLightObjImm(GXLightObj*,u32);
void GXSetNumChans(int);void GXSetChanCtrl(int,int,int,int,u32,int,int);
void GXSetChanAmbColor(int,GXColor);void GXSetChanMatColor(int,GXColor);
void lightSetConstant(cLight*,GXLightObj*);void lightSetLinear(cLight*,GXLightObj*);
void lightSetQuadratic(cLight*,GXLightObj*);void lightSetSpotlight(cLight*,GXLightObj*);
void lightSetCustom(cLight*,GXLightObj*);void lightSetParallel(cLight*,GXLightObj*);
void lightSetSpotQuad(cLight*,GXLightObj*);void lightSetLocalAmb(cLight*,GXColor*);
void lightSetColor(GXLightObj*,cLight*,cEm*);
int lightHitCheck(cModel*,cLight*);int lightHitCheckSphere(cModel*,cLight*);int lightHitCheckCylinder(cModel*,cLight*);int lightHitCheckBBox(cModel*,cLight*);
inline int StaFlagChk(Context *c,int n){if(n<0||n>=64)throw std::invalid_argument("GX status flag");return c->state.Status_flg[n/32]&(0x80000000u>>(n%32));}
inline int DbgFlagChk(Context *c,int n){if(n<0||n>=128)throw std::invalid_argument("GX debug flag");return c->state.Debug_flg[n/32]&(0x80000000u>>(n%32));}
void checkSdkCall();
float GetDistance3(Vec*,Vec*);void RotVector(Vec*,Vec*,Vec*);
float RootSumSquare3(Vec*);void low_RotMatrix(Mtx,Vec*);
void C_MTXMultVecSR(const Mtx,const Vec*,Vec*);void C_VECScale(const Vec*,Vec*,float);

extern thread_local int tev_stage,tev_reg,tev_kcolor,tex_map,tex_coord,ind_stage,g_material_tex_coord;
extern thread_local u8 gxCsScale[4];
inline int stageId(){if(tev_stage<0||tev_stage>=16)throw MissingGameConsumer("GX TEV stage capacity");return tev_stage;}
void TextureBlend(cPolyHeader*,cModelInfo*,int,int);
void TextureBlend2(cPolyHeader*,cModelInfo*,int,int);
void TextureBlend3(cPolyHeader*,cModelInfo*,int,int);
void materialSetup(cPolyHeader*,cModelInfo*,int,int);
void alphaSetup(cModel*,cPolyHeader*,cModelInfo*,int);
void finishModelScale(cModel*);
void shaderSetup(cModel*,cModelInfo*,cPolyHeader*,Mtx);
void specularSetup(cPolyHeader*,cModelInfo*,int);void specularSetup2(cPolyHeader*,int);void bumpSetup(cPolyHeader*,cModelInfo*);
void GlobalIlluminationSetup(cPolyHeader*,int);void updateMatrices(Mtx,Mtx,cModel*);
struct GXTexObj { u32 texture{}; bool valid{}; };
extern thread_local GXTexObj Specular[9],IndTex[2],GlobalIlmTex[5];
extern thread_local Mtx specular_mat;
extern thread_local int g_specular_tev_stage,isSelfUse,g_SelfShdNum;
struct ShadowMng { cModel *pModel[2]{}; };
extern thread_local ShadowMng *g_pShdMng;
ShadowMng *GetSelfShadowMng(u32);
void ThermoShaderSetup(cModel*,cModelInfo*,cPolyHeader*);void RefractShaderSetup(cModel*,cModelInfo*,cPolyHeader*,Mtx);
void SelfShadowSetup(cPolyHeader*,cModel*,ShadowMng*);void ShadowCastSetup(cPolyHeader*,cModel*);
inline int DpfFlagChk(Context *c,int n){if(n<0||n>=32)throw std::invalid_argument("GX display flag");return c->state.Disp_flg&(0x80000000u>>n);}
constexpr int STA_THERMO_GRAPH=37,STA_SELF_SHADOW=31,DPF_GLB_ILM=12;
void GXLoadTexObj(GXTexObj*,int);
void GXSetIndTexMtx(int,const float[2][3],s8);
void GXSetIndTexOrder(int,int,int);void GXSetIndTexCoordScale(int,int,int);
void GXSetTevIndirect(int,int,int,int,int,int,int,int,int,int);
void GXSetTevIndBumpXYZ(int,int,int);void GXSetTevIndWarp(int,int,u8,u8,int);
void __GXSetIndirectMask(u32);
using GXTevStageID=int;using GXIndTexStageID=int;using GXIndTexMtxID=int;using GXIndTexWrap=int;
constexpr int GX_ITF_8=0,GX_ITB_NONE=0,GX_ITB_STU=7,GX_ITW_OFF=0,GX_ITW_0=6,GX_FALSE=0,GX_ITBA_OFF=0;
inline int indStageId(){if(ind_stage<0||ind_stage>=4)throw MissingGameConsumer("GX indirect capacity");return ind_stage;}
void org_LoadTexObj(u32,int);
void GXSetTevOrder(int,int,int,int);
void GXSetTevColorIn(int,int,int,int,int);void GXSetTevAlphaIn(int,int,int,int,int);
void GXSetTevColorOp(int,int,int,int,int,int);void GXSetTevAlphaOp(int,int,int,int,int,int);
void GXSetTevKColor(int,GXColor);void GXSetTevKColorSel(int,int);void GXSetTevKAlphaSel(int,int);
void GXSetTexCoordGen2(int,int,int,u32,int,u32);void GXLoadTexMtxImm(Mtx,u32,int);
void GXSetAlphaCompare(int,int,int,int,int);void GXSetZCompLoc(int);
void GXSetNumTevStages(int);void GXSetNumTexGens(int);void GXSetNumIndStages(int);
extern const std::array<int,22> originalRenderOrder;
} // namespace re4::port::gx
#define MEM_ALLOC(bytes,align,heap) allocateBuckets(bytes)
#define MODEL_TEX(info) (&(info)->tex)
#define TEV_STAGE_ID() stageId()
#define IRef(x) (x)
#define ISET0(x) ((x)=0)
#define IND_STAGE_ID() indStageId()
#define isBit(value,mask) (((value)&(mask))!=0)
#define MAX(a,b) ((a)>(b)?(a):(b))
#define PSMTXMultVec C_MTXMultVec
#undef PSMTXMultVecSR
#define PSMTXMultVecSR C_MTXMultVecSR
#define PSMTXInverse C_MTXInverse
#undef PSVECScale
#define PSVECScale C_VECScale
#undef PSVECAdd
#define PSVECAdd re4::port::C_VECAdd
#undef VALID_PTR
#define VALID_PTR(p) ((p)!=nullptr)
#define SQRTF std::sqrt
#define PSVECDotProduct C_VECDotProduct
#define SINF std::sin
#define COSF std::cos
#define CHECK_GXBEGIN(line,message) checkSdkCall()
#define LIGHT_FUNC_TABLE static void (*funcLightParam[16])(cLight*,GXLightObj*)={lightSetConstant,lightSetLinear,lightSetQuadratic,lightSetSpotlight,lightSetCustom,lightSetParallel,lightSetSpotQuad,lightSetConstant,lightSetConstant,lightSetConstant,lightSetConstant,lightSetConstant,lightSetConstant,lightSetConstant,lightSetConstant,lightSetConstant}
