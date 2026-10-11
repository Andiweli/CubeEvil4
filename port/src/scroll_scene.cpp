#include "scroll_original.h"
#include "re4/port/byte_order.h"
#include "re4/port/sha256.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace re4::port::scroll {
namespace {
void range(ResourceLease b,std::size_t at,std::size_t n) {
    if(!b.owner||!b.data||at>b.size||n>b.size-at)throw std::invalid_argument("SMD resource boundary");
}
u32 word(ResourceLease b,u32 at) { range(b,at,4);return *BigEndianView(b.data,b.size).u32(at); }
u16 half(ResourceLease b,u32 at) { range(b,at,2);return *BigEndianView(b.data,b.size).u16(at); }
float real(ResourceLease b,u32 at) { auto w=word(b,at);float f;std::memcpy(&f,&w,4);if(!std::isfinite(f))throw std::invalid_argument("SMD nonfinite coordinate");return f; }
Vec vector(ResourceLease b,u32 at) { return {real(b,at),real(b,at+4),real(b,at+8)}; }
ResourceLease slice(ResourceLease b,u32 at,u32 n) { range(b,at,n);return {b.owner,b.data+at,n}; }
ResourceLease emptyTpl() {
    static auto bytes=std::make_shared<const std::vector<u8>>(std::initializer_list<u8>{0,0x20,0xaf,0x30,0,0,0,0,0,0,0,12});
    return {bytes,bytes->data(),12};
}
}
cSmd::cSmd(ResourceLease b):bytes(b) {
    range(b,0,16);Version=b.data[0];Flag=b.data[1];nModel=half(b,2);
    if(!Version||Flag&~1)throw MissingGameConsumer("SMD unsupported version/flags");
    BinTblOfs=word(b,4);TplTblOfs=word(b,8);MotTblOfs=word(b,12);
    u32 base=16;
    if(Flag&1) {
        grp.nGroup=word(b,16);if(grp.nGroup>32)throw std::invalid_argument("SMD group capacity");
        range(b,20,u64(grp.nGroup)*4);base=20+grp.nGroup*4;
        for(u32 i=0;i<grp.nGroup;++i){auto n=word(b,20+4*i);if(n>65535)throw std::invalid_argument("SMD group count");grp.num.push_back(n);}
    }
    if(base>BinTblOfs||u64(nModel)*72>BinTblOfs-base||BinTblOfs>TplTblOfs||TplTblOfs>MotTblOfs||MotTblOfs>b.size)
        throw std::invalid_argument("SMD work/table extent");
    work.resize(nModel);
    for(u32 i=0;i<nModel;++i){u32 at=base+72*i;auto &w=work[i];
        w.Pos=vector(b,at);w.Ang=vector(b,at+12);w.Scale=vector(b,at+24);
        w.BinId=b.data[at+36];w.TplId=b.data[at+37];w.MotId=b.data[at+38];w.WorkNo=b.data[at+39];
        w.Radius=real(b,at+40);w.Flag=word(b,at+44);std::copy(b.data+at+48,b.data+at+68,w.pad_30);w.flags=word(b,at+68);
        if(w.WorkNo>=250&&w.WorkNo!=254&&w.WorkNo!=255)throw std::invalid_argument("SMD scroll ID capacity");
    }
}
ResourceLease cSmd::entry(u32 table,u32 limit,int id) {
    if(table>=limit||limit>bytes.size||id<0)throw std::invalid_argument("SMD table extent");
    auto first=word(bytes,table);if(first<4||first%4||first>limit-table||u32(id)>=first/4)throw std::invalid_argument("SMD table index");
    auto start=word(bytes,table+u32(id)*4),stop=limit-table;
    for(u32 i=0;i<first/4;++i){auto o=word(bytes,table+4*i);if(!o)continue;if(o<first||o>limit-table)throw std::invalid_argument("SMD table offset");if(o>start)stop=std::min(stop,o);}
    if(start<first||start>=stop)throw std::invalid_argument("SMD empty entry");auto b=slice(bytes,table+start,stop-start);entries.emplace(b.data,b);return b;
}
cSmxData::cSmxData(ResourceLease b) {
    range(b,0,16);nData=b.data[1];range(b,16,u64(nData)*144);work.resize(nData);
    for(u32 i=0;i<nData;++i){auto at=16+144*i;auto &w=work[i];w.ModelNo=b.data[at];w.Id=b.data[at+1];w.OtType=b.data[at+2];w.CullMode=b.data[at+3];
        w.LitSelectMask=word(b,at+4);w.Flag=word(b,at+8);w.MaterialColor=word(b,at+12);std::copy(b.data+at+16,b.data+at+136,w.Free);
        w.SpecularColor=word(b,at+132);w.TexU=real(b,at+136);w.TexV=real(b,at+140);
        if(w.CullMode>2||w.OtType>5||w.Id==15)throw MissingGameConsumer("SMD unsupported SMX cull/OT/mirror");
    }
}
ObjectManager::ObjectManager(Domain *d,u32 n):domain(d),nArray(n),storage(std::make_unique<cObj[]>(n)),pArray(storage.get()) {}
bool ObjectManager::owns(cObj *p) const {
    auto a=reinterpret_cast<std::uintptr_t>(p),b=reinterpret_cast<std::uintptr_t>(pArray);
    return p&&a>=b&&a-b<std::size_t(nArray)*sizeof(cObj)&&(a-b)%sizeof(cObj)==0;
}
void ObjectManager::memClear(cObj *p,std::size_t n) {
    if(!owns(p)||n!=sizeof(cObj)||p->handle)throw std::logic_error("scroll allocation ownership");
    *p=cObj{};p->domain=domain;
}
int ObjectManager::construct(cObj *p,int id) { if(id!=ID_SCROLL)throw std::invalid_argument("scroll class ID");p->initializeObject();p->initializeScroll();return 1; }
void ObjectManager::addListBack(cObj *p) { auto **link=&head;while(*link)link=&(*link)->next;*link=p; }
u32 ObjectManager::countActiveWork() { u32 n=0;for(u32 i=0;i<nArray;++i)n+=!pArray[i].isEmpty();return n; }
void ObjectManager::destroy(cObj *p) {
    if(!owns(p)||!p->isAlive())throw std::logic_error("scroll destroy ownership");
    if(p->handle)domain->engine.releaseModel(*p->handle);
    p->handle.reset();auto **link=&head;while(*link&&*link!=p)link=&(*link)->next;if(*link)*link=p->next;
    for(auto &id:domain->ids)if(id==p)id=nullptr;
    for(auto &id:domain->works)if(id==p)id=nullptr;
    p->be_flag=0;p->pModelInfo=nullptr;p->next=nullptr;
}
Domain::Domain(EngineConsumers &e,u32 count):engine(e),ObjMgr(this,count) {}
Domain::~Domain() { while(ObjMgr.head)ObjMgr.destroy(ObjMgr.head); }
void Domain::checkBlockRelease(int block) {
    for(auto *o=ObjMgr.head;o;o=o->next)if(o->blk==block&&o->handle)engine.checkModelRelease(*o->handle);
}
std::optional<ConsumerHandle> Domain::lightParent(u8 type,u16 no) {
    if(type==4){if(no>=ObjMgr.nArray)return {};auto *o=ObjMgr.pArray+no;if(!o->isAlive()||!o->handle)return {};return o->handle;}
    if(type!=2)throw MissingGameConsumer("original light parent manager type "+std::to_string(type)+" (live EmMgr/Etc/ObjMgr required)");
    auto *o=SmdGetGroupObjPtr2(no);if(!o||!o->isAlive()||!o->handle)return {};
    (void)engine.model(*o->handle);return o->handle;
}
ResourceLease Domain::lease(void *p) const {
    for(auto &a:assets){auto it=a->entries.find(p);if(it!=a->entries.end())return it->second;}
    throw std::invalid_argument("unresolved scroll resource pointer");
}
void Domain::clearPointerTable(cObj **p,std::size_t n) { std::fill(p,p+n,nullptr); }
void Domain::storeColor(u8 *dst,u32 value) { for(u32 i=0;i<4;++i)dst[i]=u8(value>>(24-8*i)); }
void Domain::MotionSetCore(cObj *o,ModelMotion*,void *mot,void *seq,int hokan,int flags,int frame) {
    if(seq)throw MissingGameConsumer("scroll native SEQ binding");engine.initializeMotion(*o->handle,lease(mot),frame,flags,hokan);o->Motion=o->cpu().Motion;o->be_flag=o->cpu().be_flag;
}
void ModelInfo::addTplAddr(void *p) { additional=domain->lease(p);(void)decodeGxTpl(additional); }
ModelCpu &cObj::cpu() { if(!handle)throw std::logic_error("scroll model not initialized");return domain->engine.model(*handle); }
int cObj::modelInit(void *b,void *t) {
    bin=domain->lease(b);tpl=t?domain->lease(t):emptyTpl();handle=domain->engine.initializeModel(bin,tpl,"original SmdSetParam",false,kindid,be_flag);
    auto &m=cpu();be_flag|=m.be_flag;info.domain=domain;info.native=m.pModelInfo;info.bound=m.pModelInfo->bound;info.color=m.pModelInfo->color;info.color2=m.pModelInfo->color2;pModelInfo=&info;
    return 1;
}
void cObj::matUpdate() {
    auto &m=cpu();m.pos=pos;m.ang=ang;m.scale=scale;m.be_flag=be_flag;
    // The original cCoord::matCalc and model parts bodies operate on their owned native storage.
    m.matCalc();m.partsMatCalc();m.partsWorldCalc();updateLight();
}
void cObj::updateLight() {
    auto &m=cpu();gx::cModel view;gx::LightPart root;C_MTXCopy(m.mat,view.mat);view.pos=m.pos;view.ang=m.ang;view.scale=m.scale;
    if(m.pList){C_MTXCopy(m.getPartsPtr(0)->mat,root.mat);root.world=m.getPartsPtr(0)->world;view.pList=&root;}else view.pList=nullptr;
    view.LightInfo=LightInfo;
    view.LightInfo.updateMatrix(&view);LightInfo=view.LightInfo;
}

std::unique_ptr<Domain> appendRoomScrollGeometry(EngineLoader&,EngineConsumers &engine,const RoomArchive &archive,GxScene &scene,bool retain) {
    ResourceLease body{archive.body,archive.body->data(),u32(archive.body->size())};auto entry=archive.find("SMD");if(!entry)throw MissingGameConsumer("room has no SMD");
    auto primary=std::make_unique<cSmd>(slice(body,entry->offset,entry->size));u32 count=u32(primary->getWorkNum());
    if(!count||count>65535)throw std::invalid_argument("scroll model capacity");
    auto constants=originalRoomConstants(body);
    auto limit=[&](u32 base){u64 n=u64(base)+count;if(n>65535)throw std::invalid_argument("original model/object pool capacity");return u32(n);};
    auto owner=std::make_unique<Domain>(engine,limit(constants[1]));auto &domain=*owner;auto *main=primary.get();scene.smdHash=sha256(main->bytes.data,main->bytes.size);domain.assets.push_back(std::move(primary));
    cSmd *common=nullptr;if(auto *e=archive.find("SMD",1)){auto a=std::make_unique<cSmd>(slice(body,e->offset,e->size));common=a.get();domain.assets.push_back(std::move(a));}
    if(auto *e=archive.find("SMX"))domain.smx=std::make_unique<cSmxData>(slice(body,e->offset,e->size));
    std::vector<std::pair<int,cSmd*>> blocks;
    for(std::size_t i=0;i<archive.blocks.size();++i){auto &f=archive.blocks[i];auto &index=archive.blockIndices.at(i);auto *s=index.find("SMD",0);if(!s)throw MissingGameConsumer("block has no SMD");
        ResourceLease b{f,f->bytes.data(),u32(f->bytes.size())};auto a=std::make_unique<cSmd>(slice(b,s->offset,s->size));
        auto path=f->identity.path;auto dot=path.rfind('.'),under=path.rfind('_');if(under==std::string::npos||dot!=under+3)throw std::invalid_argument("original block name");
        int no=std::stoi(path.substr(under+1,2),nullptr,16);
        if(main->Flag&1){if(u32(no)>=main->grp.nGroup||a->nModel!=main->grp.num.at(no))throw std::invalid_argument("SMD/BLK group count mismatch");}
        blocks.emplace_back(no,a.get());domain.assets.push_back(std::move(a));scene.blockHashes.push_back(f->identity.sha256);
    }
    if((main->Flag&1)&&blocks.size()!=main->grp.nGroup)throw std::invalid_argument("SMD missing block group");
    u32 infos=0,parts=0;
    for(auto *a: [&]{std::vector<cSmd*> v{main};for(auto &b:blocks)v.push_back(b.second);return v;}())for(auto &w:a->work)if(w.WorkNo!=255){
        auto *src=w.flags&16?common:a;if(!src)throw std::invalid_argument("SMD common BIN absent");auto b=src->entry(src->BinTblOfs,src->TplTblOfs,w.BinId);range(b,0,64);++infos;parts+=u32(b.data[25])+1;
    }
    auto infoCapacity=limit(constants[7]),partCapacity=limit(constants[6]);
    if(infos>infoCapacity||parts>partCapacity)throw std::invalid_argument("original CNS model/parts pool exhausted by SMD");
    engine.configureModelPools(infoCapacity,partCapacity);
    scene.originalPoolSizes={domain.ObjMgr.nArray,infoCapacity,partCapacity};
    domain.SmdInit(main,domain.smx.get(),common);domain.SmdSetup(-1);scene.scrollFunctions.push_back("SmdInit/SmdSetup/setObj/SmdSetParam/smxInit");
    for(auto &b:blocks){domain.BlockCreate(b.first,b.second);scene.activeBlocks.push_back(b.first);}
    if(!blocks.empty())scene.scrollFunctions.push_back("BlockCreate");
    std::map<std::pair<const u8*,const u8*>,std::pair<u32,u32>> textures;
    u32 i=0;
    for(auto *o=domain.ObjMgr.head;o;o=o->next,++i){auto &m=o->cpu();auto &info=*o->pModelInfo;auto binding=std::make_pair(o->tpl.data,info.additional.data);
        if(!textures.count(binding)){auto decoded=decodeGxTpl(o->tpl);if(info.additional.owner){auto add=decodeGxTpl(info.additional);decoded.insert(decoded.end(),std::make_move_iterator(add.begin()),std::make_move_iterator(add.end()));}
            u32 base=u32(scene.textures.size());textures.emplace(binding,std::make_pair(base,u32(decoded.size())));for(auto &t:decoded){if(m.pModelInfo->model_addr->flags&0x80000000)t.wrapS=t.wrapT=1;t.minFilter=t.minLod==t.maxLod?1:5;t.magFilter=1;scene.textures.push_back(std::move(t));}}
        auto skin=skinModelCpu(m,*m.pModelInfo);Mtx world;auto *root=skin.staticBypass?m.getPartsPtr(o->bin.data[word(o->bin,0)]):m.pList;C_MTXConcat(root->mat,m.pModelInfo->mat,world);
        GxDraw id;id.cpuOwner=o->handle;id.workIndex=i;id.workId=254;
        // Recover immutable resource IDs from the actual current SMD placement; no renumbering of model tables.
        auto *a=o->blk<0?main:std::find_if(blocks.begin(),blocks.end(),[&](auto &b){return b.first==o->blk;})->second;
        auto &w=a->work.at(o->localIndex);id.workIndex=o->localIndex;
        if(o->blk>=0){id.workIndex+=main->nModel;for(auto &b:blocks){if(b.first==o->blk)break;id.workIndex+=b.second->nModel;}}
        id.workId=w.WorkNo;id.binId=w.BinId;id.tplId=w.TplId;id.blockNo=o->blk;
        id.cull=std::array<u8,3>{1,2,0}.at(o->CullMode);id.otType=o->ot_type;id.alphaOverride=o->alpha_omit;id.attribute=o->Attribute;
        id.blendMode=info.blend_mode;if(id.blendMode>4)throw MissingGameConsumer("scroll blend mode");
        id.materialColor=u32(info.color[0])<<24|u32(info.color[1])<<16|u32(info.color[2])<<8|255;
        std::copy(info.color2,info.color2+4,id.color2.begin());id.uvScrollU=info.uvScrollU;id.uvScrollV=info.uvScrollV;
        if(domain.smx&&w.WorkNo!=254)for(auto &x:domain.smx->work)if(x.ModelNo==w.WorkNo){id.smxFlags=x.Flag;break;}
        C_MTXCopy(m.mat,id.modelMatrix);id.modelScaleForSpecular=m.scale;id.modelStateFlags=m.be_flag;id.modelPosition=m.pos;id.modelAngle=m.ang;id.modelScale=m.scale;
        auto &bound=info.bound;id.lightSize=bound.size;id.lightOffset=bound.offset;id.lightSelectMask=o->LightInfo.SelectMask;
        id.rootWorld=m.getPartsPtr(0)->world;C_MTXCopy(m.getPartsPtr(0)->mat,id.rootMatrix);C_MTXMultVec(m.getPartsPtr(0)->mat,&bound.offset,&id.lightPosition);
        id.radius=std::sqrt(bound.size.x*bound.size.x+bound.size.y*bound.size.y+bound.size.z*bound.size.z)*std::max({m.scale.x,m.scale.y,m.scale.z});
        auto tex=textures.at(binding);decodeGxModel(o->bin,world,tex.first,tex.second,scene,id,skin.staticBypass?nullptr:&skin);++scene.models;if(!skin.staticBypass)++scene.skinnedModels;
    }
    scene.scrollWorkCount=count;
    if(retain){scene.scrollFunctions.push_back("persistent original SMD models retained through light/OT capture");return owner;}
    // Actual original block destruction, followed by a second creation and destruction,
    // verifies the same model pools and shared-resource owners before GPU submission.
    for(auto &b:blocks)domain.BlockDestroy(b.first);
    for(auto &b:blocks)domain.BlockCreate(b.first,b.second);
    for(auto &b:blocks)domain.BlockDestroy(b.first);
    if(!blocks.empty())scene.scrollFunctions.push_back("BlockDestroy/BlockCreate/BlockDestroy");
    domain.BlockDestroy(-1);
    if(engine.modelCount()||domain.ObjMgr.countActiveWork())throw std::logic_error("scroll model release");
    scene.scrollWorkCount=count;scene.scrollFunctions.push_back("original models released; GPU immutable leases retained");
    return {};
}
} // namespace re4::port::scroll
