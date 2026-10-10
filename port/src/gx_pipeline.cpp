#include "gx_render_original.h"
#include "re4/port/sha256.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <chrono>

namespace re4::port::gx {
thread_local OtWork g_OtWork[OT_MAX];
thread_local OtMirrorWork g_OtMirrirWk[2];
thread_local int g_NowExecOtType=OT_MAX;
thread_local float OT_MUL=.05f;
thread_local Log *pLog{};
thread_local Context *pG{};
thread_local int tev_stage{},tev_reg{},tev_kcolor{},tex_map{},tex_coord{},ind_stage{},g_material_tex_coord{};
thread_local u8 gxCsScale[4]={2,2,2,2};
thread_local LightEnv LightMgr;
thread_local GXLightObj lightObjBlack;
thread_local Vec obj_pos;
thread_local float obj_size;
thread_local int obj_flag;
namespace {
struct Capture {
    Log log;
    Context context;
    std::deque<std::vector<OtData>> buckets;
    std::deque<OtPrim> prims;
    GxMaterialState state;
    std::array<std::array<float,4>,10> matrices{};
    std::array<std::array<float,4>,8> coordinates{};
    u32 base{},count{};
};
thread_local Capture *active{};
struct Scope {
    Capture &c;
    explicit Scope(Capture &v):c(v){if(active)throw std::logic_error("nested GX control capture");active=&c;pLog=&c.log;pG=&c.context;}
    ~Scope(){active=nullptr;pLog=nullptr;pG=nullptr;for(auto &w:g_OtWork)w={};g_NowExecOtType=OT_MAX;LightMgr.slots.clear();LightMgr.nArray=0;}
};
Capture &capture(){if(!active)throw std::logic_error("GX control frame owner");return *active;}
GxTevStage &stage(int n){if(n<0||n>=16)throw std::invalid_argument("GX TEV stage index");return capture().state.stages[n];}
void operation(std::array<s32,4> &to,int op,int bias,int scale,int clamp,int out){
    if(op<0||op>1||bias<0||bias>2||scale<0||scale>3||clamp<0||clamp>1||out<0||out>3)
        throw MissingGameConsumer("GX TEV comparison/op/bias/scale");
    to={op,bias,scale,(clamp?4:0)|out};
}
std::array<s32,4> rgba(GXColor c){return {c.r,c.g,c.b,c.a};}
}
void Log::err(int,int,const char *fmt,...){va_list args;va_start(args,fmt);char b[512]{};std::vsnprintf(b,sizeof(b),fmt,args);va_end(args);throw std::runtime_error(b);}
void Log::warn(int,int,const char *fmt,...){va_list args;va_start(args,fmt);char b[512]{};std::vsnprintf(b,sizeof(b),fmt,args);va_end(args);throw std::runtime_error(b);}
void *allocateBuckets(std::size_t n){if(n%sizeof(OtData)||n>4096*sizeof(OtData))throw std::invalid_argument("OT allocation");auto &b=capture().buckets;b.emplace_back(n/sizeof(OtData));return b.back().data();}
void *GetPrimBuff(int n){if(n!=sizeof(OtPrim))throw std::invalid_argument("OT primitive allocation type");auto &p=capture().prims;if(p.size()>=65536)return nullptr;p.emplace_back();return &p.back();}
void checkSdkCall(){(void)capture();}
GEOM_HEXAHEDRON *CameraViewFrustumPtr(GxCamera *camera){static thread_local GEOM_HEXAHEDRON view;view.camera=camera;return &view;}
int collision_sphere_hexahedron(GEOM_SPHERE *s,GEOM_HEXAHEDRON *h){
    // Six normalized clip planes of the original GX SDK perspective matrix.
    // GX's clip depth is -w <= z <= 0, unlike Vulkan's 0 <= z <= w.
    float m[4][4]{};auto &cam=*h->camera;
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)for(int k=0;k<4;++k)m[r][c]+=cam.projection[r][k]*(k<3?cam.view[k][c]:(c==3?1.f:0.f));
    for(int plane=0;plane<6;++plane){float p[4];for(int j=0;j<4;++j)p[j]=plane<4?m[3][j]+((plane&1)?-1.f:1.f)*m[plane/2][j]:plane==4?m[3][j]+m[2][j]:-m[2][j];
        float n=std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);if(!(n>0))throw std::invalid_argument("OT degenerate camera plane");
        if(p[0]*s->pos.x+p[1]*s->pos.y+p[2]*s->pos.z+p[3]<-s->radius*n)return 0;
    }return 1;
}
void GXSetTevOrder(int n,int coord,int map,int chan){if((coord<0||coord>7)&&coord!=255)throw std::invalid_argument("GX texcoord");if((map<0||map>7)&&map!=255)throw std::invalid_argument("GX texmap");if(chan!=4&&chan!=255)throw MissingGameConsumer("GX raster channel");auto &s=stage(n);s.order[0]=coord;s.order[1]=map;}
void GXSetTevColorIn(int n,int a,int b,int c,int d){for(int x:{a,b,c,d})if(x<0||x>15)throw std::invalid_argument("GX color input");stage(n).colorIn={a,b,c,d};}
void GXSetTevAlphaIn(int n,int a,int b,int c,int d){for(int x:{a,b,c,d})if(x<0||x>7)throw std::invalid_argument("GX alpha input");stage(n).alphaIn={a,b,c,d};}
void GXSetTevColorOp(int n,int op,int bias,int scale,int clamp,int out){operation(stage(n).colorOp,op,bias,scale,clamp,out);}
void GXSetTevAlphaOp(int n,int op,int bias,int scale,int clamp,int out){operation(stage(n).alphaOp,op,bias,scale,clamp,out);}
void GXSetTevKColor(int n,GXColor c){if(n<0||n>3)throw std::invalid_argument("GX KColor");capture().state.konst[n]=rgba(c);}
void GXSetTevKColorSel(int n,int k){if(k<12||k>15)throw MissingGameConsumer("GX konst color selector");stage(n).order[2]=k-12;}
void GXSetTevKAlphaSel(int n,int k){if(k<28||k>31)throw MissingGameConsumer("GX konst alpha selector");stage(n).order[3]=k-28;}
void GXLoadTexMtxImm(Mtx m,u32 id,int type){if(id<30||id>57||(id-30)%3||type!=1||m[0][1]!=0||m[1][0]!=0||m[0][3]!=0||m[1][3]!=0)throw MissingGameConsumer("GX texture matrix form");capture().matrices[(id-30)/3]={m[0][2],m[1][2],m[0][0],m[1][1]};}
void GXSetTexCoordGen2(int coord,int type,int source,u32 matrix,int normalize,u32 post){if(coord<0||coord>=8||type!=1||source!=4||normalize||post!=125)throw MissingGameConsumer("GX texture generation");auto &c=capture();if(matrix==60)c.coordinates[coord]={0,0,1,1};else{if(matrix<30||matrix>57||(matrix-30)%3)throw std::invalid_argument("GX texture matrix ID");c.coordinates[coord]=c.matrices[(matrix-30)/3];}}
void org_LoadTexObj(u32 texture,int map){auto &c=capture();if(texture>=c.count)throw MissingGameConsumer("GX model/external TexRender texture ID "+std::to_string(texture));if(map<0||map>=8)throw std::invalid_argument("GX texture map");c.state.textures[map]=c.base+texture;}
void GXSetAlphaCompare(int a,int r,int op,int b,int r2){if(a<0||a>7||b<0||b>7||r<0||r>255||r2<0||r2>255||op<0||op>3)throw std::invalid_argument("GX alpha compare");auto &s=capture().state;s.alpha={a,r,op,b};s.fog[2]=float(r2);}
void GXSetZCompLoc(int early){if(early!=0)throw MissingGameConsumer("GX early alpha/depth testing");}
void GXSetNumTevStages(int n){if(n<1||n>16)throw std::invalid_argument("GX TEV count");auto &c=capture();c.state.meta[0]=n;for(int i=0;i<n;++i){auto &s=c.state.stages[i];if(s.order[0]!=255)s.uv=c.coordinates[s.order[0]];}}
void GXSetNumTexGens(int n){if(n<0||n>8)throw std::invalid_argument("GX texgen count");}
void GXSetNumIndStages(int n){if(n)throw MissingGameConsumer("GX indirect texture stages");}
void GXInitLightColor(GXLightObj *o,GXColor c){o->color=c;}
void GXInitLightPos(GXLightObj *o,float x,float y,float z){o->lpos[0]=x;o->lpos[1]=y;o->lpos[2]=z;}
void GXInitLightDir(GXLightObj *o,float x,float y,float z){o->ldir[0]=-x;o->ldir[1]=-y;o->ldir[2]=-z;}
void GXInitLightAttn(GXLightObj *o,float a,float b,float c,float d,float e,float f){o->a[0]=a;o->a[1]=b;o->a[2]=c;o->k[0]=d;o->k[1]=e;o->k[2]=f;}
void GXLoadLightObjImm(GXLightObj *o,u32 mask){if(!mask||mask>128||(mask&(mask-1)))throw std::invalid_argument("GX light slot");u32 n=0;while((1u<<n)!=mask)++n;auto &l=capture().state.lights;
    l[n*5]={o->lpos[0],o->lpos[1],o->lpos[2],0};l[n*5+1]={o->ldir[0],o->ldir[1],o->ldir[2],0};l[n*5+2]={o->a[0],o->a[1],o->a[2],0};l[n*5+3]={o->k[0],o->k[1],o->k[2],0};l[n*5+4]={float(o->color.r),float(o->color.g),float(o->color.b),float(o->color.a)};
}
void GXSetNumChans(int n){if(n!=1)throw MissingGameConsumer("GX channel count");}
void GXSetChanCtrl(int chan,int enable,int ambSource,int matSource,u32 mask,int diff,int attn){
    if(diff!=2||(attn!=1&&attn!=2)||enable<0||enable>1||ambSource<0||ambSource>1||matSource!=0&&matSource!=1)throw MissingGameConsumer("GX light channel parameters");
    if(mask>255)throw std::invalid_argument("GX light channel mask");auto &s=capture().state;
    if(chan==0){if(!enable)throw MissingGameConsumer("GX disabled RGB channel");s.meta[1]=ambSource;s.fog[3]=float(mask);}
    else if(chan==2){if(enable)throw MissingGameConsumer("GX lit alpha channel");s.meta[2]=matSource;}
    else throw MissingGameConsumer("GX extra channel");
}
void GXSetChanAmbColor(int chan,GXColor color){if(chan!=4)throw MissingGameConsumer("GX ambient channel");capture().state.ambient=rgba(color);}
void GXSetChanMatColor(int chan,GXColor color){if(chan!=4)throw MissingGameConsumer("GX material channel");capture().state.raster=rgba(color);}
int cLight::getNormal(Vec *source,Vec *result){if(ParentType)throw MissingGameConsumer("original model-bound light direction");*result=*source;return 1;}
int cLight::isParent(cModel *m){return m->source&&pParent==m->source;}
} // namespace re4::port::gx

namespace re4::port {
GxMaterialState captureOriginalMaterial(const GxMaterialInput &in,u32 base,u32 count,u8 scale){
    gx::Capture cap;gx::Scope scope(cap);cap.base=base;cap.count=count;cap.state.textures.fill(base);gx::tev_stage=gx::tev_reg=gx::tev_kcolor=gx::tex_map=gx::tex_coord=gx::ind_stage=0;
    if(!count)throw std::invalid_argument("GX empty model texture binding");if(in.flags&~4u)throw MissingGameConsumer("GX specular/bump/indirect material flags "+std::to_string(in.flags));
    if(in.texFlags&~15u)throw MissingGameConsumer("GX material texture flags");
    auto blend=in.blendTable,animation=in.animation;
    if(in.texFlags&4){if(blend.size()<4||u64(blend[0])*2+4>blend.size())throw std::invalid_argument("GX blend table range");}
    if(in.texFlags&2){if(animation.size()<4||u64(in.animationFrame)+4>=animation.size())throw std::invalid_argument("GX animated texture table range");}
    gx::cPolyHeader part{in.flags,in.materialTexture,in.alphaTexture,in.alphaRef};gx::cModelInfo info;info.flagsDC=in.texFlags;info.tex.flags=in.texFlags;info.tex.blendType=in.blendType;info.tex.blendRatio=in.blendRatio;info.tex.frame=in.animationFrame;info.tex.u=in.u;info.tex.v=in.v;info.tex.blendTbl=blend.empty()?nullptr:blend.data();info.tex.anim=animation.empty()?nullptr:animation.data();
    gx::cModel model;model.alpha_omit=in.alphaOverride;model.TevScaleGroup=in.scaleGroup;
    gx::GXSetAlphaCompare(7,0,1,7,0);gx::materialSetup(&part,&info,10,5);if(in.flags&4)gx::alphaSetup(&model,&part,&info,0);
    if(scale>2)throw std::invalid_argument("GX environment TEV scale");gx::gxCsScale[0]=gx::gxCsScale[1]=scale;
    gx::finishModelScale(&model);return cap.state;
}
void buildOriginalOrderingTables(GxScene &scene){
    gx::Capture cap;gx::Scope scope(cap);cap.context.Camera=scene.camera;gx::InitOt();scene.renderOrder.clear();scene.otExecution.clear();
    struct ModelSubmission { GxScene *scene;std::vector<u32> parts; };
    std::deque<ModelSubmission> submissions;std::map<u32,std::size_t> works;
    for(u32 i=0;i<scene.draws.size();++i){auto &d=scene.draws[i];if(!works.count(d.workIndex)){works[d.workIndex]=submissions.size();submissions.push_back({&scene,{}});}submissions[works[d.workIndex]].parts.push_back(i);}
    auto callback=[](void *p){auto &model=*static_cast<ModelSubmission*>(p);for(auto i:model.parts)model.scene->renderOrder.push_back(i);};
    // Smd::setObj createsBack, which appends to the active list; Trans walks that
    // list. Registration thus follows SMD order, then the original OT's LIFO.
    for(auto &model:submissions){auto &d=scene.draws[model.parts[0]];u32 table,bucket;
        switch(d.otType){case 2:table=11;bucket=4;break;case 3:table=11;bucket=2;break;case 4:table=11;bucket=1;break;case 5:table=16;bucket=0;break;
            default:throw MissingGameConsumer("ModelTrans depth/mirror/item-camera OT type "+std::to_string(d.otType));}
        gx::AddOtDirect(int(table),&model,callback,bucket,2,&d.lightPosition,d.radius);
    }
    for(auto type:gx::originalRenderOrder){scene.otExecution.push_back(u32(type));gx::ExecOt(type);}
}
void captureOriginalRoomLighting(GxScene &scene,EngineLoader &loader,EngineConsumers &consumers,ResourceLease roomBody,ResourceLease lit,u32 cut){
    // Same original core file used by CoreDataRead, through Dvd/ReadReq and CAS hashes.
    auto request=loader.request("etc/core.das",0x8001);auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    while(loader.status(request)==LoadStatus::Pending||loader.status(request)==LoadStatus::Reading){loader.frame();std::this_thread::yield();if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("GX core light read timeout");}
    std::shared_ptr<const LoadedFile> core;if(loader.readCheck(request,core)!=1||!core)throw std::runtime_error("GX core original disc read");
    ResourceLease coreBody{core,core->bytes.data()+core->bodyOffset,core->bodySize};GameplayHeaps heaps;NativeGameState state;LightRuntime lights(heaps,loader.scheduler(),consumers,state);lights.initialize();lights.bindArchives(coreBody,roomBody);
    auto env=lights.cut(1,int(cut));lights.loadRenderCut(1,int(cut));auto pool=lights.renderPool();
    gx::Capture cap;gx::Scope scope(cap);cap.context.Camera=scene.camera;gx::LightMgr.environment=env;gx::LightMgr.slots.clear();gx::LightMgr.nArray=u32(pool.size());auto snapshot=lights.snapshot();for(int i=0;i<8;++i)gx::LightMgr.kindFlags[i]=snapshot.kindFlags[i];
    for(auto &input:pool){gx::cLight l{};static_cast<LightFields&>(l)=input.fields;l.be_flag=input.flags;gx::LightMgr.slots.push_back(l);}
    gx::LightSetInit();scene.lightHash=sha256(lit.data,lit.size);scene.litCut=cut;scene.selectedLightRecords=env.nLight;scene.lightingCalls=0;
    if(scene.materials.size()!=scene.draws.size())throw std::logic_error("GX material/light draw pairing");
    for(u32 i=0;i<scene.draws.size();++i){auto &d=scene.draws[i];gx::cModelData data{d.modelFlags};gx::cModelInfo info;info.model_addr=&data;for(int k=0;k<4;++k)info.color[k]=u8(d.materialColor>>(24-8*k));gx::LightPart root;root.world=d.rootWorld;C_MTXCopy(d.rootMatrix,root.mat);gx::cModel model;model.be_flag=7;model.pList=&root;model.pModelInfo=&info;model.pos=d.modelPosition;model.ang=d.modelAngle;model.scale=d.modelScale;model.Attribute=d.attribute;
        model.LightInfo.init2(2,1,&d.lightOffset,&d.lightSize,0x10);model.LightInfo.SelectMask=d.lightSelectMask;model.LightInfo.updateMatrix(&model);gx::LightMgr.setModel2(&model);
        if(d.smxFlags&16)model.be_flag|=0x8000;cap.state=scene.materials[i];gx::LightSetModel(&model);auto &state=cap.state;
        state.meta[3]=s32(env.Fog.Type);if(state.meta[3]!=0&&state.meta[3]!=2&&state.meta[3]!=4&&state.meta[3]!=5&&state.meta[3]!=6&&state.meta[3]!=7)throw MissingGameConsumer("GX fog type "+std::to_string(state.meta[3]));state.fog[0]=env.Fog.Start;state.fog[1]=env.Fog.End;if(state.meta[3]&&!(env.Fog.End>env.Fog.Start))throw std::invalid_argument("GX fog interval");state.fogColor={env.Fog.Color.r/255.f,env.Fog.Color.g/255.f,env.Fog.Color.b/255.f,1.f};
        // LightMgr.setEnv's scale channels apply before shaderSetup on each model.
        auto &last=state.stages[state.meta[0]-1];if(env.tev_scale[0]>2)throw std::invalid_argument("LIT TEV scale");last.colorOp[2]=env.tev_scale[0];scene.materials[i]=state;++scene.lightingCalls;
    }
    for(auto &texture:scene.textures){if(texture.maxLod>texture.minLod){texture.minLod=env.min_lod;texture.maxLod=u8(std::min<u32>(env.max_lod,u32(texture.levels.size()-1)));texture.bias=env.lod_bias;if(texture.minLod>texture.maxLod)throw std::invalid_argument("original LIT mip range vs TPL");}}
    scene.owners.push_back(lit);scene.owners.push_back(coreBody);gx::LightMgr.slots.clear();gx::LightMgr.nArray=0;lights.clear();
}
int verifyOriginalGxControl(){
    int checks=0;auto check=[&](bool b){++checks;if(!b)throw std::runtime_error("original GX control assertion "+std::to_string(checks));};
    {gx::Capture cap;gx::Scope scope(cap);gx::InitOt();std::vector<std::array<int,3>> result;struct Data{std::vector<std::array<int,3>> *out;int id;};Data a{&result,1},b{&result,2},c{&result,3};
        auto fn=[](void *raw){auto &v=*static_cast<Data*>(raw);v.out->push_back({v.id,gx::g_NowExecOtType,gx::OtGetPrevKind()});};
        check(gx::AddOtDirect(11,&a,fn,1,4,nullptr,0)==1);check(gx::AddOtDirect(11,&b,fn,2,5,nullptr,0)==2);check(gx::AddOtDirect(11,&c,fn,2,6,nullptr,0)==2);check(gx::ExecOt(11)==3);
        check(result==std::vector<std::array<int,3>>{{3,11,0},{2,11,6},{1,11,5}});check(gx::OtGetPrevKind()==0);gx::ClearOt();check(gx::ExecOt(11)==0);
        gx::AddOtDirect(11,&a,fn,999,1,nullptr,0);gx::AddOtDirect(11,&b,fn,999,2,nullptr,0);gx::DeleteOtData(11,5);result.clear();check(gx::ExecOt(11)==1&&result[0][0]==1);
    }
    GxMaterialInput m;auto s=captureOriginalMaterial(m,5,3);check(s.meta[0]==2);check(s.stages[0].colorIn==std::array<s32,4>{15,8,10,15});check(s.stages[1].colorOp[2]==2);check(s.textures[0]==5);
    m.flags=4;m.alphaTexture=2;m.alphaRef=128;s=captureOriginalMaterial(m,5,3);check(s.meta[0]==3);check(s.textures[1]==7);check(s.alpha==std::array<s32,4>{4,128,1,4});
    m.texFlags=5;m.u=.25;m.v=.5;m.blendRatio=128;m.blendTable={1,0,0,0,0,1};
    for(u8 type=0;type<3;++type){m.blendType=type;s=captureOriginalMaterial(m,5,3);check(s.meta[0]>=5);check(s.stages[0].uv[0]==.25f&&s.stages[0].uv[1]==.5f);check(s.konst[0][0]==128);}
    m.flags=16;bool rejected=false;try{captureOriginalMaterial(m,0,3);}catch(const MissingGameConsumer&){rejected=true;}check(rejected);
    // Isolated native owners, actual original light callbacks and SDK coefficients.
    {gx::Capture cap;gx::Scope scope(cap);C_MTXIdentity(cap.context.Camera.view);
        gx::cLight l{};l.World={0,0,0};l.Radius=100;l.Intensity=.5f;l.normal={10,0,0};
        gx::obj_pos={0,0,0};gx::obj_size=1;gx::obj_flag=1;gx::GXLightObj object;
        gx::lightSetConstant(&l,&object);check(object.a[0]==.5f&&object.k[0]==1);
        gx::obj_pos.x=50;gx::lightSetLinear(&l,&object);check(object.a[0]==.25f);
        gx::obj_pos.x=0;gx::lightSetQuadratic(&l,&object);check(std::fabs(object.k[2]-.0004f)<1e-8f);
        l.cone.Normal={0,0,1};l.cone.CutOff=45;l.cone.Edge=10;
        gx::lightSetSpotlight(&l,&object);check(object.ldir[2]==-1&&std::isfinite(object.a[2])&&object.k[1]>0);
        gx::lightSetSpotQuad(&l,&object);check(object.k[0]==2&&std::isfinite(object.k[2]));
        l.spot.A0=.25f;l.spot.A1=.5f;l.spot.A2=.75f;l.spot.K0=1;l.spot.K1=2;l.spot.K2=3;
        gx::lightSetCustom(&l,&object);check(object.a[2]==.75f&&object.k[2]==3);
        l.dir.Pos={0,0,1000};l.dir.Flag=0;l.dir.Edge=10;gx::lightSetParallel(&l,&object);check(object.lpos[2]==1000&&object.a[0]==.5f);
        l.DispCol={200,100,40,128};l.normal.x=10;GXColor ambient{};gx::lightSetLocalAmb(&l,&ambient);check(ambient.r==200&&ambient.b==40);
        gx::cModel model;l.LitIndex=7;model.State.SetLightIgnore();model.State.SetLightIgnoreUse(1);model.State.SetLightNo(7);model.State.SetLightPow(.25f);
        gx::lightSetColor(&object,&l,&model);check(object.color.r==50&&object.color.g==25&&object.color.b==10);
        model.State.SetLightNo(8);gx::lightSetColor(&object,&l,&model);check(object.color.r==200);
        gx::GXInitLightSpot(&object,0,2);check(object.a[0]==1&&object.a[1]==0&&object.a[2]==0);
        gx::GXInitLightDistAttn(&object,10,.5f,1);check(object.k[0]==1&&std::fabs(object.k[1]-.1f)<1e-7f);
    }
    return checks;
}
} // namespace re4::port
