#include "gx_original.h"
#include "scroll_original.h"
#include "re4/port/gx_pipeline.h"
#include "re4/port/sha256.h"
#include "re4/port/event_skinning.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <map>
#include <sstream>

namespace re4::port {
namespace {
void range(ResourceLease b,std::size_t at,std::size_t n) {
    if(!b.owner || !b.data || at>b.size || n>b.size-at) throw std::invalid_argument("GX resource boundary");
}
u16 half(ResourceLease b,std::size_t p) { range(b,p,2);return u16(u16(b.data[p])<<8|b.data[p+1]); }
u32 word(ResourceLease b,std::size_t p) { range(b,p,4);return u32(b.data[p])<<24|u32(b.data[p+1])<<16|u32(b.data[p+2])<<8|b.data[p+3]; }
float real(ResourceLease b,std::size_t p) { auto w=word(b,p);float f;std::memcpy(&f,&w,4);if(!std::isfinite(f))throw std::invalid_argument("GX nonfinite scalar");return f; }
ResourceLease slice(ResourceLease b,u32 p,u32 n) { range(b,p,n);return {b.owner,b.data+p,n}; }
std::string text(ResourceLease b,u32 p,u32 n) {
    range(b,p,n);auto end=std::find(b.data+p,b.data+p+n,u8(0));
    if(end==b.data+p+n)throw std::invalid_argument("GX name terminator");
    return {reinterpret_cast<const char*>(b.data+p),std::size_t(end-(b.data+p))};
}
u32 levelBytes(u32 fmt,u32 w,u32 h) {
    u32 x=4,y=4,sz=32;
    switch(fmt){case 0:case 8:case 14:x=y=8;break;case 1:case 2:case 9:x=8;break;case 3:case 4:case 5:case 10:break;case 6:sz=64;break;default:throw MissingGameConsumer("GX unsupported texture format "+std::to_string(fmt));}
    return ((w+x-1)/x)*((h+y-1)/y)*sz;
}
using Pixel=std::array<u8,4>;
u8 c5(u32 x){return u8((x<<3)|(x>>2));} u8 c6(u32 x){return u8((x<<2)|(x>>4));}
Pixel rgb565(u16 x){return {c5(x>>11),c6((x>>5)&63),c5(x&31),255};}
Pixel rgb5a3(u16 x){if(x&0x8000)return {c5((x>>10)&31),c5((x>>5)&31),c5(x&31),255};auto a=u32(x>>12);return {u8(((x>>8)&15)*17),u8(((x>>4)&15)*17),u8((x&15)*17),u8((a<<5)|(a<<2)|(a>>1))};}
GxMip decodeLevel(ResourceLease d,u32 fmt,u32 w,u32 h,ResourceLease clut,u32 cf) {
    range(d,0,levelBytes(fmt,w,h));GxMip out{w,h,std::vector<u8>(std::size_t(w)*h*4)};
    auto put=[&](u32 x,u32 y,Pixel p){if(x<w&&y<h)std::copy(p.begin(),p.end(),out.rgba.begin()+(std::size_t(y)*w+x)*4);};
    if(fmt==14){u32 at=0;for(u32 y=0;y<h;y+=8)for(u32 x=0;x<w;x+=8)for(u32 block=0;block<4;++block,at+=8){
        u16 a=half(d,at),b=half(d,at+2);Pixel pal[4]={rgb565(a),rgb565(b),{}, {}};
        for(u32 k=0;k<3;++k){pal[2][k]=a>b?u8((5u*pal[0][k]+3u*pal[1][k])>>3):u8((u32(pal[0][k])+pal[1][k])/2);pal[3][k]=a>b?u8((3u*pal[0][k]+5u*pal[1][k])>>3):pal[2][k];}
        pal[2][3]=255;pal[3][3]=a>b?255:0;
        for(u32 r=0;r<4;++r)for(u32 c=0;c<4;++c)put(x+(block%2)*4+c,y+(block/2)*4+r,pal[(d.data[at+4+r]>>(6-c*2))&3]);
    }return out;}
    u32 tw=(fmt==0||fmt==1||fmt==2||fmt==8||fmt==9)?8:4,th=(fmt==0||fmt==8)?8:4,at=0;
    for(u32 y=0;y<h;y+=th)for(u32 x=0;x<w;x+=tw){
        for(u32 r=0;r<th;++r)for(u32 c=0;c<tw;++c){u32 i=r*tw+c;Pixel px{};u32 index=0;
            switch(fmt){
            case 0:{u8 v=u8(((d.data[at+i/2]>>(i%2?0:4))&15)*17);px={v,v,v,v};break;}
            case 1:{u8 v=d.data[at+i];px={v,v,v,v};break;}
            case 2:{u8 v=u8((d.data[at+i]&15)*17),a=u8((d.data[at+i]>>4)*17);px={v,v,v,a};break;}
            case 3:{u8 v=d.data[at+i*2+1];px={v,v,v,d.data[at+i*2]};break;}
            case 4:px=rgb565(half(d,at+i*2));break;
            case 5:px=rgb5a3(half(d,at+i*2));break;
            case 6:px={d.data[at+i*2+1],d.data[at+32+i*2],d.data[at+33+i*2],d.data[at+i*2]};break;
            case 8:index=(d.data[at+i/2]>>(i%2?0:4))&15;break;
            case 9:index=d.data[at+i];break;
            case 10:index=half(d,at+i*2)&0x3fff;break;
            default:throw std::invalid_argument("GX texture decoder unreachable format");
            }
            if(fmt>=8){auto v=half(clut,index*2);if(cf==0){u8 a=u8(v>>8),k=u8(v);px={k,k,k,a};}else px=cf==1?rgb565(v):rgb5a3(v);}
            put(x+c,y+r,px);
        }at+=fmt==6?64:32;
    }return out;
}
void waitRoom(EngineLoader &loader){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(60);while(loader.roomBusy()){loader.frame();std::this_thread::yield();if(std::chrono::steady_clock::now()>end)throw std::runtime_error("diagnostic room load timeout");}if(!loader.room())throw std::runtime_error(loader.roomError());}
} // namespace

std::vector<GxTexture> decodeGxTpl(ResourceLease b) {
    if(word(b,0)!=0x20af30)throw std::invalid_argument("GX TPL signature");
    auto n=word(b,4),table=word(b,8);if(n>248 || table<12)throw std::invalid_argument("GX TPL capacity");range(b,table,std::size_t(n)*8);
    std::vector<GxTexture> result;
    for(u32 i=0;i<n;++i){u32 at=word(b,table+i*8),cl=word(b,table+i*8+4);range(b,at,36);
        GxTexture t;t.source=b;t.format=word(b,at+4);t.wrapS=word(b,at+12);t.wrapT=word(b,at+16);t.minFilter=word(b,at+20);t.magFilter=word(b,at+24);t.bias=real(b,at+28);t.minLod=b.data[at+33];t.maxLod=b.data[at+34];
        u32 h=half(b,at),w=half(b,at+2),pixel=word(b,at+8);
        if(!w||!h||w>1024||h>1024||t.wrapS>2||t.wrapT>2||t.minFilter>5||t.magFilter>1||t.minLod>t.maxLod||t.maxLod>10||b.data[at+35])throw std::invalid_argument("GX TPL dimensions/sampler/relocation");
        ResourceLease palette;u32 cf=0;if(cl){range(b,cl,12);auto count=half(b,cl);cf=word(b,cl+4);if(!count||cf>2||b.data[cl+2])throw std::invalid_argument("GX TLUT header");palette=slice(b,word(b,cl+8),u32(count)*2);}
        if(t.format>=8&&t.format<=10&&!palette.owner)throw std::invalid_argument("GX indexed image lacks TLUT");
        for(u32 level=0;level<=t.maxLod;++level){u32 size=levelBytes(t.format,w,h);t.levels.push_back(decodeLevel(slice(b,pixel,size),t.format,w,h,palette,cf));pixel+=size;w=std::max(1u,w/2);h=std::max(1u,h/2);}
        result.push_back(std::move(t));
    }return result;
}

void decodeGxModel(ResourceLease b,const Mtx world,u32 base,u32 count,GxScene &scene,GxDraw id,const SkinningResult *skin) {
    range(b,0,64);auto ver=word(b,60);if(ver!=0x20010801&&ver!=0x20030818)throw std::invalid_argument("GX BIN version");
    auto flags=word(b,32),vtx=word(b,48),nrm=word(b,52),clr=word(b,12),tex=word(b,16),part=word(b,28);
    auto nv=half(b,56),nn=half(b,58),np=half(b,26);auto shift=b.data[40];if(shift>=31)throw std::invalid_argument("GX fixed point exponent");
    if(skin && (skin->source.data!=b.data || skin->source.owner!=b.owner))throw std::invalid_argument("GX skin/BIN ownership mismatch");
    if(!(flags&0x80000000))throw MissingGameConsumer("GX BIN VAT without CLR0 requires descriptor-driven FIFO decoding");
    range(b,vtx,std::size_t(nv)*8);range(b,nrm,std::size_t(nn)*((flags&0x20000000)?4:8));
    auto sectionEnd=[&](u32 start){u32 end=b.size;for(auto field:{0u,12u,16u,20u,28u,44u,48u,52u}){auto o=word(b,field);if(o>start)end=std::min(end,o);}if(ver==0x20030818)for(auto field:{64u,68u}){auto o=word(b,field);if(o>start)end=std::min(end,o);}return end;};
    auto ce=sectionEnd(clr),te=sectionEnd(tex),pe=sectionEnd(part);
    Mtx inv{},normal{};if(!C_MTXInverse(world,inv))throw std::invalid_argument("GX singular model matrix");C_MTXTranspose(inv,normal);
    for(u32 p=0;p<np;++p){range(b,part,32);auto draw=id;draw.partIndex=p;draw.firstIndex=u32(scene.indices.size());draw.flags=b.data[part+11];draw.alphaRef=draw.alphaOverride==255?b.data[part+20]:draw.alphaOverride;
        if(draw.flags&~0x97u)throw MissingGameConsumer("GX unsupported material flags "+std::to_string(draw.flags));
        draw.textureBase=base;draw.textureCount=count;
        draw.specular={b.data[part+13],b.data[part+15],b.data[part+16],b.data[part+17],b.data[part+18],b.data[part+19],b.data[part+21],b.data[part+23],0};
        u32 ti=b.data[part+12],ai=b.data[part+14];if(ti>=count || ((draw.flags&4)&&ai>=count))throw std::invalid_argument("GX material TPL index");draw.texture=base+ti;draw.alphaTexture=base+((draw.flags&4)?ai:ti);
        u32 size=word(b,part+24);if(size%32 || part>pe || 32u+std::uint64_t(size)>pe-part)throw std::invalid_argument("GX display list extent");u32 end=part+32+size,cursor=part+32;
        std::map<std::array<u16,4>,u32> vertices;
        while(cursor<end){u8 op=b.data[cursor++];if(!op)continue;if(op!=0x80&&op!=0x90&&op!=0x98)throw MissingGameConsumer("GX unsupported FIFO opcode "+std::to_string(op));
            if(end-cursor<2)throw std::invalid_argument("GX truncated primitive header");
            auto num=half(b,cursor);cursor+=2;if(u64(num)*8>end-cursor)throw std::invalid_argument("GX truncated primitive");if((op==0x80&&num%4)||(op==0x90&&num%3)||(op==0x98&&num<3))throw std::invalid_argument("GX invalid primitive count");
            std::vector<u32> ids;ids.reserve(num);
            for(u32 j=0;j<num;++j){std::array<u16,4> idx{half(b,cursor),half(b,cursor+2),half(b,cursor+4),half(b,cursor+6)};cursor+=8;
                if(idx[0]>=nv||idx[1]>=nn||u64(clr)+u64(idx[2])*4+4>ce||u64(tex)+u64(idx[3])*4+4>te)throw std::invalid_argument("GX vertex array index");
                auto found=vertices.find(idx);if(found!=vertices.end()){ids.push_back(found->second);continue;}
                GxVertex vertex{};Vec pos{},no{},wp{};auto at=vtx+u32(idx[0])*8;float s=1.f/float(u32(1)<<shift);pos={float(s16(half(b,at)))*s,float(s16(half(b,at+2)))*s,float(s16(half(b,at+4)))*s};C_MTXMultVec(world,&pos,&wp);vertex.position[0]=wp.x;vertex.position[1]=wp.y;vertex.position[2]=wp.z;
                at=nrm+u32(idx[1])*((flags&0x20000000)?4:8);if(flags&0x20000000)no={float(s8(b.data[at]))/64,float(s8(b.data[at+1]))/64,float(s8(b.data[at+2]))/64};else no={float(s16(half(b,at)))/16384,float(s16(half(b,at+2)))/16384,float(s16(half(b,at+4)))/16384};
                if(skin){pos=skin->position(idx[0]);no=skin->normal(idx[1]);C_MTXMultVec(world,&pos,&wp);vertex.position[0]=wp.x;vertex.position[1]=wp.y;vertex.position[2]=wp.z;}
                vertex.rawNormal[0]=no.x;vertex.rawNormal[1]=no.y;vertex.rawNormal[2]=no.z;
                for(u32 r=0;r<3;++r)vertex.normal[r]=normal[r][0]*no.x+normal[r][1]*no.y+normal[r][2]*no.z;
                at=tex+u32(idx[3])*4;s=(flags&0x80000000)?1.f/256:1.f/32768;for(u32 r=0;r<2;++r)vertex.uv[r]=float((flags&0x80000000)?s32(s16(half(b,at+r*2))):s32(half(b,at+r*2)))*s;
                std::copy(b.data+clr+u32(idx[2])*4,b.data+clr+u32(idx[2])*4+4,vertex.color.begin());
                if(scene.vertices.size()>=UINT32_MAX)throw std::overflow_error("GX vertex count");auto k=u32(scene.vertices.size());scene.vertices.push_back(vertex);vertices.emplace(idx,k);ids.push_back(k);
            }
            auto tri=[&](u32 a,u32 c,u32 d){scene.indices.insert(scene.indices.end(),{ids[a],ids[c],ids[d]});};
            if(op==0x90)for(u32 j=0;j<num;j+=3)tri(j,j+1,j+2);
            if(op==0x80)for(u32 j=0;j<num;j+=4){tri(j,j+1,j+2);tri(j,j+2,j+3);}
            if(op==0x98)for(u32 j=0;j+2<num;++j){if(j&1)tri(j+1,j,j+2);else tri(j,j+1,j+2);}
            ++scene.primitives;
        }
        draw.indexCount=u32(scene.indices.size())-draw.firstIndex;draw.modelFlags=flags;
        GxMaterialInput material;material.flags=draw.flags;material.materialTexture=u8(ti);material.alphaTexture=u8(ai);material.alphaRef=b.data[part+20];material.alphaOverride=draw.alphaOverride;
        if(draw.uvScrollU!=0||draw.uvScrollV!=0)material.texFlags=1; // smxInit enables scrolling; initial offsets remain zero.
        // Specular/indirect states require the camera and core textures, resolved
        // by captureOriginalRoomLighting before this scene can be submitted.
        scene.materials.push_back((draw.flags&0x13)?GxMaterialState{}:captureOriginalMaterial(material,base,count));scene.draws.push_back(draw);part=end;
    }
    scene.owners.push_back(b);
}

GxScene loadDiagnosticScene(EngineLoader &loader,EngineConsumers &engine,u16 room,const std::string &eventPath,u32 frame,const std::string &eventModel,u32 requestedCut,u32 debugView) {
    if(debugView>6)throw std::invalid_argument("GX debug view 0..6");
    if(!loader.loadRoom(room,true))throw std::runtime_error("diagnostic room already loading");waitRoom(loader);auto archive=loader.room();GxScene scene;scene.roomHash=archive->source->identity.sha256;
    ResourceLease body{archive->body,archive->body->data(),u32(archive->body->size())};auto entry=archive->find("SMD");if(!entry)throw MissingGameConsumer("room has no SMD");auto smd=slice(body,entry->offset,entry->size);scene.smdHash=sha256(smd.data,smd.size);
    scene.roomId=room;scene.debugView=debugView;auto models=scroll::appendRoomScrollGeometry(loader,engine,*archive,scene,true);u32 n=scene.scrollWorkCount;
    auto req=loader.request(eventPath);auto end=std::chrono::steady_clock::now()+std::chrono::seconds(60);while(loader.status(req)==LoadStatus::Pending||loader.status(req)==LoadStatus::Reading){loader.frame();std::this_thread::yield();if(std::chrono::steady_clock::now()>end)throw std::runtime_error("camera EVD load timeout");}
    std::shared_ptr<const LoadedFile> loaded;if(loader.readCheck(req,loaded)!=1||!loaded)throw std::runtime_error("camera EVD read");auto evd=indexEventPackage({loaded,loaded->bytes.data(),u32(loaded->bytes.size())});u32 cameraCut=0;bool found=false;
    for(u32 at=evd.packetOffset;at<evd.packetOffset+evd.packetSize;){auto size=half(evd.bytes,at+12);if(size<16||size>evd.packetOffset+evd.packetSize-at)throw std::invalid_argument("camera packet boundary");auto type=word(evd.bytes,at);
        if(type==6&&!found&&(requestedCut==UINT32_MAX || half(evd.bytes,at+8)==requestedCut)){if(size<64||half(evd.bytes,at+10)!=0)throw MissingGameConsumer("event camera packet does not start at frame0");scene.cameraName=text(evd.bytes,at+16,48);cameraCut=half(evd.bytes,at+8);found=true;}
        if(type==9&&found&&half(evd.bytes,at+8)==cameraCut&&text(evd.bytes,at+16,12)=="cam0000")throw MissingGameConsumer("event camera parent/base matrix required");at+=size;
    }
    if(!found)throw MissingGameConsumer("event has no original camera packet");auto cameraBin=std::find_if(evd.bins.begin(),evd.bins.end(),[&](const auto &b){return b.name==scene.cameraName;});if(cameraBin==evd.bins.end())throw std::invalid_argument("event camera bin reference");
    // ExePacket_Lit names an EVD resource and calls LightMgr.update(0,-1).
    // An event camera cut is not an index into the room's LIT resource.
    ResourceLease selectedLit;int latestLightFrame=-1;
    for(u32 at=evd.packetOffset;at<evd.packetOffset+evd.packetSize;){auto size=half(evd.bytes,at+12);auto type=word(evd.bytes,at);int pc=s16(half(evd.bytes,at+8)),pf=s16(half(evd.bytes,at+10));
        if(type==14&&pc==int(cameraCut)&&pf>=0&&u32(pf)<=frame&&pf>=latestLightFrame){if(size<64)throw std::invalid_argument("event LIT packet size");scene.lightName=text(evd.bytes,at+16,48);latestLightFrame=pf;}at+=size;
    }
    if(latestLightFrame>=0){auto lightBin=std::find_if(evd.bins.begin(),evd.bins.end(),[&](const auto &b){return b.name==scene.lightName;});if(lightBin==evd.bins.end())throw std::invalid_argument("event LIT resource reference");selectedLit=lightBin->bytes;scene.eventLight=true;}
    else throw MissingGameConsumer("diagnostic event has no applicable Lit packet; live room-camera/light state required");
    ModelCpu cameraValidation;cameraValidation.nParts=4;MotionAsset motion(cameraBin->bytes,cameraValidation);if(frame>(motion.data.maxFrame&0x3fff))throw std::invalid_argument("camera frame");
    // CameraMotion::move explicitly initializes roll to zero when track 2 is absent.
    auto numbers=motion.data.numbers;std::sort(numbers.begin(),numbers.end());if(numbers!=std::vector<u8>{0,1,2,3}&&numbers!=std::vector<u8>{0,1,3})throw std::invalid_argument("camera requires unique position/target/FOV and optional roll");
    DiagnosticCameraMotion camera;auto &w=camera.m_info;w.Joint_num=motion.data.nParts;w.pJoint_kind=motion.data.kinds.data();w.pJoint_no=motion.data.numbers.data();w.pHermite_data=motion.data.streams.data();w.Mot_frame=float(frame);w.Mot_frame_max=float((motion.data.maxFrame&0x3fff)+1);w.Mot_attr=8;
    camera.move();scene.camera=camera;scene.cameraFrame=frame;if(scene.camera.param.Fovy<=0||scene.camera.param.Fovy>=180)throw std::invalid_argument("original camera FOV");
    C_MTXLookAt(scene.camera.view,&scene.camera.param.Campos,&scene.camera.Up,&scene.camera.param.Target);C_MTXPerspective(scene.camera.projection,scene.camera.param.Fovy,1.3333334f,100.f,1000000.f); // camera.cpp/MainInit constants.
    for(u32 r=0;r<4;++r)for(u32 c=0;c<4;++c){float v=0;for(u32 k=0;k<4;++k)v+=scene.camera.projection[r][k]*(k<3?scene.camera.view[k][c]:(c==3?1.f:0.f));scene.viewProjection[c*4+r]=v;}
    if(!eventModel.empty()) {
        EngineConsumers eventModels(&loader.scheduler());
        auto snapshot=evaluateEventSkinning(loader,eventModels,eventPath,cameraCut,frame);
        auto found=std::find_if(snapshot.models.begin(),snapshot.models.end(),[&](const auto &m){return m.name==eventModel;});
        if(found==snapshot.models.end())throw MissingGameConsumer("original event model pose not resolved: "+eventModel);
        auto &rootModel=*found;
        std::vector<const EventSkinModel*> family;
        for(auto &m:snapshot.models)if(m.name==eventModel||m.parent==eventModel)family.push_back(&m);
        std::sort(family.begin(),family.end(),[](auto a,auto b){return a->infoOrder<b->infoOrder;});
        std::map<const u8*,std::pair<u32,u32>> paletteBindings;
        Sha256 hash;
        for(auto *item:family){auto &m=*item;
            // A model without a Mot packet retains the actual modelInit bind
            // pose. Report its empty motion name; never synthesize animation.
            for(auto &dep:snapshot.dependencies)if(dep=="ShapeSet/ShapeMove morph packet: "+m.name)throw MissingGameConsumer(dep);
            if(!paletteBindings.count(m.texture.data)){auto decoded=decodeGxTpl(m.texture);auto base=u32(scene.textures.size());paletteBindings.emplace(m.texture.data,std::make_pair(base,u32(decoded.size())));
                for(auto &t:decoded){if(word(m.skin.source,32)&0x80000000)t.wrapS=t.wrapT=1;t.minFilter=t.minLod==t.maxLod?1:5;t.magFilter=1;scene.textures.push_back(std::move(t));}}
            auto binding=paletteBindings.at(m.texture.data);
            GxDraw id;id.workIndex=n;id.workId=n;id.otType=m.otType;
            C_MTXCopy(m.modelMatrix,id.modelMatrix);id.modelScaleForSpecular=m.scale;id.modelStateFlags=m.modelFlags;id.color2=m.color2;
            const u8 cullModes[3]={1,2,0};if(m.cullMode>2)throw std::invalid_argument("original model cull mode");id.cull=cullModes[m.cullMode];
            id.lightOffset=rootModel.boundOffset;id.lightSize=rootModel.boundSize;id.modelPosition=m.position;id.modelAngle=m.angle;id.modelScale=m.scale;
            C_MTXCopy(m.rootMatrix,id.rootMatrix);id.rootWorld=m.rootWorld;
            C_MTXMultVec(m.rootMatrix,&rootModel.boundOffset,&id.lightPosition);id.radius=std::sqrt(rootModel.boundSize.x*rootModel.boundSize.x+rootModel.boundSize.y*rootModel.boundSize.y+rootModel.boundSize.z*rootModel.boundSize.z)*std::max({m.scale.x,m.scale.y,m.scale.z});
            decodeGxModel(m.skin.source,m.world,binding.first,binding.second,scene,id,&m.skin);++scene.models;if(!m.skin.staticBypass)++scene.skinnedModels;
            scene.eventHierarchies.push_back(m.name);
            hash.update(reinterpret_cast<const u8*>(m.skin.positions.data()),m.skin.positions.size()*2);hash.update(reinterpret_cast<const u8*>(m.skin.normals8.data()),m.skin.normals8.size());hash.update(reinterpret_cast<const u8*>(m.skin.normals16.data()),m.skin.normals16.size()*2);
        }
        scene.eventModel=rootModel.name;scene.eventMotion=rootModel.motion;scene.skinHash=hash.finish();
    }
    captureOriginalRoomLighting(scene,loader,engine,body,selectedLit,0,[&](u8 type,u16 no){return models->lightParent(type,no);},scene.eventLight);
    buildOriginalOrderingTables(scene);scene.materialLightingReady=scene.lightDependencyError.empty();
    scene.owners.push_back(body);scene.owners.push_back(cameraBin->bytes);
    models.reset();if(engine.modelCount())throw std::logic_error("persistent SMD model release after capture");
    for(auto &d:scene.draws)d.cpuOwner.reset();
    scene.scrollFunctions.push_back("persistent SMD models released after light and OT capture; GPU snapshot owned");return scene;
}
GxScene loadOriginalScrollGeometry(EngineLoader &loader,EngineConsumers &engine,u16 room) {
    if(!loader.loadRoom(room,true))throw std::runtime_error("scroll room already loading");waitRoom(loader);auto archive=loader.room();
    GxScene scene;scene.roomHash=archive->source->identity.sha256;scroll::appendRoomScrollGeometry(loader,engine,*archive,scene);return scene;
}
std::string GxScene::report() const {
    Sha256 geometry;geometry.update(reinterpret_cast<const u8*>(vertices.data()),vertices.size()*sizeof(GxVertex));geometry.update(reinterpret_cast<const u8*>(indices.data()),indices.size()*4);
    std::ostringstream o;o<<"{\"pointer_bits\":"<<sizeof(void*)*8<<",\"kind\":"<<std::quoted(materialLightingReady?"isolated_original_SMD_GX_OT_TEV_LIT_cut":debugSubmissionReady?"isolated_original_geometry_unlit_DEBUG":"original_SMD_registration_geometry_only")<<",\"models\":"<<models<<",\"vertices\":"<<vertices.size()<<",\"triangles\":"<<indices.size()/3<<",\"primitives\":"<<primitives<<",\"draws\":"<<draws.size()<<",\"textures\":"<<textures.size()<<",\"room_sha256\":"<<std::quoted(roomHash)<<",\"smd_sha256\":"<<std::quoted(smdHash)<<",\"geometry_sha256\":"<<std::quoted(geometry.finish())<<",\"camera\":"<<std::quoted(cameraName)<<",\"frame\":"<<cameraFrame<<",\"fovy\":"<<camera.param.Fovy<<",\"texture_hashes\":[";
    for(std::size_t i=0;i<textures.size();++i){if(i)o<<',';Sha256 h;for(auto &m:textures[i].levels)h.update(m.rgba.data(),m.rgba.size());o<<std::quoted(h.finish());}
    o<<"],\"light_resource\":"<<std::quoted(lightName)<<",\"event_light_packet\":"<<(eventLight?"true":"false")<<",\"original_pool_sizes\":["<<originalPoolSizes[0]<<','<<originalPoolSizes[1]<<','<<originalPoolSizes[2]<<"],\"camera_point\":["<<std::setprecision(9)<<camera.param.Campos.x<<','<<camera.param.Campos.y<<','<<camera.param.Campos.z<<','<<camera.param.Target.x<<','<<camera.param.Target.y<<','<<camera.param.Target.z<<','<<camera.param.Roll<<','<<camera.param.Fovy<<"],\"vp\":[";
    for(u32 i=0;i<16;++i){if(i)o<<',';o<<viewProjection[i];}
    o<<"],\"view_matrix\":[";for(u32 r=0;r<3;++r)for(u32 k=0;k<4;++k){if(r||k)o<<char(44);o<<camera.view[r][k];}
    o<<"],\"materials\":[";for(std::size_t i=0;i<draws.size();++i){auto &d=draws[i];if(i)o<<',';o<<'['<<d.workIndex<<','<<d.workId<<','<<d.binId<<','<<d.tplId<<','<<d.partIndex<<','<<d.texture<<','<<d.alphaTexture<<','<<unsigned(d.flags)<<','<<unsigned(d.alphaRef)<<','<<unsigned(d.cull)<<','<<d.indexCount<<','<<unsigned(d.blendMode)<<','<<d.materialColor<<','<<unsigned(d.otType)<<','<<d.smxFlags<<','<<d.uvScrollU<<','<<d.uvScrollV<<']';}
    o<<"],\"material_lighting_ready\":"<<(materialLightingReady?"true":"false")<<",\"scroll_work_count\":"<<scrollWorkCount<<",\"active_blocks\":[";for(std::size_t i=0;i<activeBlocks.size();++i){if(i)o<<char(44);o<<activeBlocks[i];}
    o<<"],\"block_hashes\":[";for(std::size_t i=0;i<blockHashes.size();++i){if(i)o<<char(44);o<<std::quoted(blockHashes[i]);}
    o<<"],\"scroll_functions\":[";for(std::size_t i=0;i<scrollFunctions.size();++i){if(i)o<<char(44);o<<std::quoted(scrollFunctions[i]);}
    o<<"],\"render_order\":[";for(std::size_t i=0;i<renderOrder.size();++i){if(i)o<<',';o<<renderOrder[i];}
    o<<"],\"ot_execution\":[";for(std::size_t i=0;i<otExecution.size();++i){if(i)o<<',';o<<otExecution[i];}
    static_assert(sizeof(GxMaterialState)==3312, "Native GX state has no ABI padding");
    o<<"],\"material_state_sha256\":"<<std::quoted(sha256(reinterpret_cast<const u8*>(materials.data()),materials.size()*sizeof(GxMaterialState)));
    o<<",\"lighting_calls\":"<<lightingCalls<<",\"lit_sha256\":"<<std::quoted(lightHash)<<",\"lit_cut\":"<<litCut<<",\"selected_light_records\":"<<selectedLightRecords<<",\"tev_states\":[";
    for(std::size_t i=0;i<materials.size();++i){if(i)o<<',';auto &s=materials[i];o<<"{\"stages\":"<<s.meta[0]<<",\"ambient_source\":"<<s.meta[1]<<",\"alpha_source\":"<<s.meta[2]<<",\"fog\":"<<s.meta[3]<<",\"scale\":"<<(s.meta[0]>0?s.stages[s.meta[0]-1].colorOp[2]:0)<<",\"light_mask\":"<<u32(s.fog[3])<<",\"raster\":[";for(int k=0;k<4;++k){if(k)o<<',';o<<s.raster[k];}o<<"],\"ambient\":[";for(int k=0;k<4;++k){if(k)o<<',';o<<s.ambient[k];}o<<"]}";}
    o<<"],\"skinned_models\":"<<skinnedModels<<",\"event_model\":"<<std::quoted(eventModel)<<",\"event_motion\":"<<std::quoted(eventMotion)<<",\"skin_sha256\":"<<std::quoted(skinHash);
    o<<",\"event_hierarchies\":[";for(std::size_t i=0;i<eventHierarchies.size();++i){if(i)o<<',';o<<std::quoted(eventHierarchies[i]);}o<<"],\"core_texture_bindings\":["<<specularTextureBase<<','<<specularTextureCount<<','<<indirectTextureBase<<','<<indirectTextureCount<<','<<globalTextureBase<<','<<globalTextureCount<<"],\"material_programs\":[";
    auto array=[&](const auto &a){o<<'[';for(std::size_t k=0;k<a.size();++k){if(k)o<<',';o<<a[k];}o<<']';};
    for(std::size_t i=0;i<materials.size();++i){if(i)o<<',';auto &s=materials[i];auto &d=draws[i];o<<"{\"binding\":["<<d.textureBase<<char(44)<<d.textureCount<<"],\"specular\":[";for(u32 k=0;k<8;++k){if(k)o<<',';o<<unsigned(d.specular[k]);}o<<"],\"model_matrix\":[";for(u32 r=0;r<3;++r)for(u32 k=0;k<4;++k){if(r||k)o<<',';o<<d.modelMatrix[r][k];}o<<"],\"model_scale\":["<<d.modelScaleForSpecular.x<<','<<d.modelScaleForSpecular.y<<','<<d.modelScaleForSpecular.z<<"],\"model_flags\":"<<d.modelFlags<<",\"textures\":";array(s.textures);o<<",\"indirect_meta\":";array(s.indirectMeta);o<<",\"program\":[";
        for(int n=0;n<s.meta[0];++n){if(n)o<<',';auto &t=s.stages[n];o<<'[';array(t.colorIn);o<<',';array(t.alphaIn);o<<',';array(t.colorOp);o<<',';array(t.alphaOp);o<<',';array(t.order);o<<',';array(t.indirect);o<<']';}
        o<<"],\"texgens\":[";for(int n=0;n<s.indirectMeta[1];++n){if(n)o<<',';auto &g=s.texgens[n];o<<'[';array(g.meta);for(auto &row:g.rows){o<<',';array(row);}o<<']';}
        o<<"],\"indirect_matrices\":[";for(int n=0;n<6;++n){if(n)o<<',';array(s.indirectMatrices[n]);}o<<"],\"indirect_order\":[";for(int n=0;n<s.indirectMeta[0];++n){if(n)o<<',';array(s.indirectOrder[n]);}o<<"]}";
    }
    o<<']';
    o<<",\"light_state\":[";for(std::size_t i=0;i<materials.size();++i){if(i)o<<',';const auto &s=materials[i];o<<"{\"fog\":";array(s.fog);o<<",\"fog_color\":";array(s.fogColor);o<<",\"objects\":[";for(std::size_t n=0;n<s.lights.size();++n){if(n)o<<',';array(s.lights[n]);}o<<"]}";}o<<']';
    o<<",\"debug_view\":"<<debugView<<",\"debug_submission_ready\":"<<(debugSubmissionReady?"true":"false")<<",\"light_dependency_error\":"<<std::quoted(lightDependencyError);
    o<<",\"limitations\":[\"isolated SMD/event-pose diagnostic: not complete Trans/Render or GameTask\","<<std::quoted(materialLightingReady?"frozen event LIT target; live LightMgr.update/SAR/actor states not integrated":debugSubmissionReady?"explicit unlit DEBUG GPU snapshot; missing live light parents":"registration/geometry only: no camera, OT, material-light capture or GPU submission")<<",\"all-resident original block creation; player-dependent cBlock.check activation not integrated\",\"active shape morphs, cloth, IK, Hokan, shadows, mirror, other indirect formats and postprocessing not integrated\",\"cut-isolated neutral morph state; no event state carry across preceding cuts\"]}";return o.str();
}
} // namespace re4::port
