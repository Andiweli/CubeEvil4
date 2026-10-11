#pragma once
#include "gx_render_original.h"
#include <optional>

namespace re4::port::scroll {
// Native views of scroll.h fields. Wire offsets are decoded once, never overlaid.
struct cSmdWork {
    Vec Pos{}, Ang{}, Scale{};
    u8 BinId{}, TplId{}, MotId{}, WorkNo{};
    float Radius{}; u32 Flag{}; u8 pad_30[20]{}; u32 flags{};
};
struct cSmd {
    ResourceLease bytes;
    u8 Version{}, Flag{}; u16 nModel{};
    u32 BinTblOfs{}, TplTblOfs{}, MotTblOfs{};
    struct { u32 nGroup{}; std::vector<u32> num; } grp;
    std::vector<cSmdWork> work;
    std::map<const void*,ResourceLease> entries;
    explicit cSmd(ResourceLease);
    cSmdWork *getWorkPtr(int);
    int getWorkNum();
    void *getBinPtr(int); void *getTplPtr(int); void *getMotPtr(int);
    ResourceLease entry(u32,u32,int);
};
struct cSmxWork {
    u8 ModelNo{},Id{},OtType{},CullMode{};
    u32 LitSelectMask{},Flag{},MaterialColor{};
    // smxInit copies 0x78 raw bytes, including the BE specular word. Keep
    // those bytes separate from its typed colour view on little-endian hosts.
    u8 Free[120]{}; u32 SpecularColor{}; float TexU{},TexV{};
};
struct cSmxData {
    u8 nData{}; std::vector<cSmxWork> work;
    explicit cSmxData(ResourceLease);
    cSmxWork *at(u32 n) { return n==0 ? work.data() : &work.at(n); }
};
struct Domain;
struct ModelInfo {
    Domain *domain{}; ModelInfoView *native{};
    ModelBoundingBox bound; u8 *color{},*color2{};
    u8 blend_mode{},flagsDC{}; float uvScrollU{},uvScrollV{};
    ResourceLease additional;
    void addTplAddr(void*);
};
struct cObj {
    Domain *domain{};
    std::optional<ConsumerHandle> handle;
    ResourceLease bin,tpl;
    u32 be_flag{}; u8 kindid{},type{},ot_type=3,Attribute{},CullMode{},alpha_omit=255;
    int blk=-2; u32 localIndex{}; Vec pos{},ang{},scale{1,1,1}; u8 free[120]{};
    cObj *next{}; ModelInfo info; ModelInfo *pModelInfo{};
    gx::cLightInfo LightInfo;
    void (*CallBackFunc)(cObj*){};
    ModelMotion Motion{};
    ModelCpu &cpu();
    int isEmpty() const { return !(be_flag&0x601); }
    int isAlive() const { return (be_flag&0x201)==1; }
    void initializeObject(); void initializeScroll();
    void setNoSuspend(int);
    int modelInit(void*,void*);
    void matUpdate();
    void updateLight();
};
using cObjScr=cObj;
using cBoundingBox=ModelBoundingBox;
using cModelInfo=ModelInfo;
struct ObjectManager {
    static constexpr int ID_SCROLL=2;
    Domain *domain; u32 nArray; std::size_t size=sizeof(cObj);
    const char *name="native scroll objects";
    std::unique_ptr<cObj[]> storage; cObj *pArray{},*head{};
    ObjectManager(Domain*,u32);
    cObj *createBack(int); cObj *getPrevWork(cObj*);
    cObj *getActiveWork() { return head; }
    cObj *getNext(cObj *p) { return p->next; }
    void memClear(cObj*,std::size_t); int construct(cObj*,int);
    void addListBack(cObj*); void destroy(cObj*); u32 countActiveWork();
    template<class... T> void log(const char*,T...) {}
    bool owns(cObj*) const;
};
using cObjMgr=ObjectManager;
struct Domain {
    EngineConsumers &engine;
    ObjectManager ObjMgr;
    cSmd *pSmd{},*pSmdComn{}; cSmxData *pSmx{};
    gx::Log log; gx::Log *pLog=&log;
    std::vector<cObj*> ids,works; cObj **scrObjTbl{},**scrTbl{}; int nScrWork{};
    std::vector<std::unique_ptr<cSmd>> assets;
    std::unique_ptr<cSmxData> smx;
    explicit Domain(EngineConsumers&,u32);
    ~Domain();
    ResourceLease lease(void*) const;
    void clearPointerTable(cObj**,std::size_t);
    int SmdInit(cSmd*,cSmxData*,cSmd*);
    void SmdClear(int); void SmdSetup(int); int setObj(int);
    void workInit(cObj*); int SmdSetParam(cObj*,cSmdWork*);
    void SmxSetFlag(cObj*,u32); void smxInit(cObj*,u8); void smxInit(cObj*,cSmxWork*);
    void BlockCreate(int,cSmd*); void BlockDestroy(int);
    cObj *SmdGetGroupNext(cObj*);
    void MotionSetCore(cObj*,ModelMotion*,void*,void*,int,int,int);
    void storeColor(u8*,u32);
};
void appendRoomScrollGeometry(EngineLoader&,EngineConsumers&,const RoomArchive&,GxScene&);
} // namespace re4::port::scroll
