#include "gx_original.h"
#include "re4/port/sha256.h"
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
ResourceLease tableEntry(ResourceLease smd,u32 table,u32 limit,u32 id) {
    if(table>limit || limit>smd.size)throw std::invalid_argument("SMD table extent");
    auto first=word(smd,table);if(first<4 || first%4 || first>limit-table || id>=first/4)throw std::invalid_argument("SMD table index");
    u32 start=word(smd,table+id*4),stop=limit-table;
    for(u32 i=0;i<first/4;++i){auto o=word(smd,table+i*4);if(!o)continue; // Zero alignment words are not table entries.
        if(o<first||o>limit-table)throw std::invalid_argument("SMD table offset");if(o>start)stop=std::min(stop,o);}
    if(start<first)throw std::invalid_argument("SMD used entry points into offset table");
    if(start>=stop)throw std::invalid_argument("SMD empty or inverted entry");return slice(smd,table+start,stop-start);
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

void decodeGxModel(ResourceLease b,const Mtx world,u32 base,u32 count,GxScene &scene,GxDraw id) {
    range(b,0,64);auto ver=word(b,60);if(ver!=0x20010801&&ver!=0x20030818)throw std::invalid_argument("GX BIN version");
    auto flags=word(b,32),vtx=word(b,48),nrm=word(b,52),clr=word(b,12),tex=word(b,16),part=word(b,28);
    auto nv=half(b,56),nn=half(b,58),np=half(b,26);auto shift=b.data[40];if(shift>=31)throw std::invalid_argument("GX fixed point exponent");
    if(!(flags&0x80000000))throw MissingGameConsumer("GX BIN VAT without CLR0 requires descriptor-driven FIFO decoding");
    range(b,vtx,std::size_t(nv)*8);range(b,nrm,std::size_t(nn)*((flags&0x20000000)?4:8));
    auto sectionEnd=[&](u32 start){u32 end=b.size;for(auto field:{0u,12u,16u,20u,28u,44u,48u,52u}){auto o=word(b,field);if(o>start)end=std::min(end,o);}if(ver==0x20030818)for(auto field:{64u,68u}){auto o=word(b,field);if(o>start)end=std::min(end,o);}return end;};
    auto ce=sectionEnd(clr),te=sectionEnd(tex),pe=sectionEnd(part);
    Mtx inv{},normal{};if(!C_MTXInverse(world,inv))throw std::invalid_argument("GX singular model matrix");C_MTXTranspose(inv,normal);
    for(u32 p=0;p<np;++p){range(b,part,32);auto draw=id;draw.partIndex=p;draw.firstIndex=u32(scene.indices.size());draw.flags=b.data[part+11];draw.alphaRef=draw.alphaOverride==255?b.data[part+20]:draw.alphaOverride;
        if(draw.flags&~4u)throw MissingGameConsumer("GX unsupported material flags "+std::to_string(draw.flags));
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
        draw.indexCount=u32(scene.indices.size())-draw.firstIndex;scene.draws.push_back(draw);part=end;
    }
    scene.owners.push_back(b);
}

GxScene loadDiagnosticScene(EngineLoader &loader,EngineConsumers &engine,u16 room,const std::string &eventPath,u32 frame) {
    if(!loader.loadRoom(room,false))throw std::runtime_error("diagnostic room already loading");waitRoom(loader);auto archive=loader.room();GxScene scene;scene.roomHash=archive->source->identity.sha256;
    ResourceLease body{archive->body,archive->body->data(),u32(archive->body->size())};auto entry=archive->find("SMD");if(!entry)throw MissingGameConsumer("room has no SMD");auto smd=slice(body,entry->offset,entry->size);scene.smdHash=sha256(smd.data,smd.size);
    range(smd,0,16);if(smd.data[0]<32 || (smd.data[1]&1))throw MissingGameConsumer("SMD old version/grouped works need original group creation");u32 n=half(smd,2),bt=word(smd,4),tt=word(smd,8),mt=word(smd,12);range(smd,16,u64(n)*72);
    engine.configureModelPools(1,4);std::map<u32,std::pair<u32,u32>> textures;
    auto smxEntry=archive->find("SMX");ResourceLease smx;if(smxEntry){smx=slice(body,smxEntry->offset,smxEntry->size);range(smx,0,16);range(smx,16,u64(smx.data[1])*144);}
    for(u32 i=0;i<n;++i){u32 at=16+i*72;u8 bin=smd.data[at+36],tpl=smd.data[at+37],mot=smd.data[at+38],work=smd.data[at+39];if(work==255)continue;auto flag=word(smd,at+68);
        if(flag&0x10)throw MissingGameConsumer("SMD shared BIN/TPL table binding");if(mot!=255)throw MissingGameConsumer("SMD animated scroll object requires original MotionMove");
        auto b=tableEntry(smd,bt,tt,bin),t=tableEntry(smd,tt,mt,tpl);
        if(b.data[25]!=1 || b.data[24]>1 || half(b,42)>255)throw MissingGameConsumer("GX scroll model needs original skinning palette");
        auto handle=engine.initializeModel(b,t,"SMD original CPU model");auto &model=engine.model(handle);
        model.pos={real(smd,at),real(smd,at+4),real(smd,at+8)};model.ang={real(smd,at+12),real(smd,at+16),real(smd,at+20)};model.scale={real(smd,at+24),real(smd,at+28),real(smd,at+32)};
        RotMatrix(model.mat,&model.ang);TransMatrix(model.mat,&model.pos);ScaleMatrix(model.mat,&model.scale);model.partsMatCalc();model.partsWorldCalc();Mtx world;C_MTXConcat(model.getPartsPtr(b.data[word(b,0)])->mat,model.pModelInfo->mat,world);
        if(!textures.count(tpl)){auto decoded=decodeGxTpl(t);u32 base=u32(scene.textures.size());textures.emplace(tpl,std::make_pair(base,u32(decoded.size())));for(auto &image:decoded){ // Original commonModelTrans overrides BIN S16 UV wrapping to repeat.
            if(word(b,32)&0x80000000)image.wrapS=image.wrapT=1;
            // Original model path overrides min/mag filters to linear/trilinear.
            image.minFilter=image.minLod==image.maxLod?1:5;image.magFilter=1;scene.textures.push_back(std::move(image));}}
        GxDraw id{};id.workIndex=i;id.workId=work;id.binId=bin;id.tplId=tpl;id.cull=1; // Model default CullMode0 -> GX_FRONT.
        if(smx.owner)for(u32 k=0;k<smx.data[1];++k){u32 p=16+k*144;if(smx.data[p]!=work)continue;auto c=smx.data[p+3];if(c>2)throw std::invalid_argument("SMX cull mode");id.cull=c==0?1:c==1?2:0;
            auto col=word(smx,p+12);id.blendMode=u8(col);if(id.blendMode>4)throw std::invalid_argument("SMX blend mode");id.materialColor=(col&~255u)?(col|255u):0xffffffff;
            id.otType=smx.data[p+2];id.smxFlags=word(smx,p+8);if(id.smxFlags&8)id.alphaOverride=128;
            id.uvScrollU=real(smx,p+136);id.uvScrollV=real(smx,p+140);break;}
        auto texPair=textures.at(tpl);decodeGxModel(b,world,texPair.first,texPair.second,scene,id);++scene.models;engine.releaseModel(handle);
    }
    auto req=loader.request(eventPath);auto end=std::chrono::steady_clock::now()+std::chrono::seconds(60);while(loader.status(req)==LoadStatus::Pending||loader.status(req)==LoadStatus::Reading){loader.frame();std::this_thread::yield();if(std::chrono::steady_clock::now()>end)throw std::runtime_error("camera EVD load timeout");}
    std::shared_ptr<const LoadedFile> loaded;if(loader.readCheck(req,loaded)!=1||!loaded)throw std::runtime_error("camera EVD read");auto evd=indexEventPackage({loaded,loaded->bytes.data(),u32(loaded->bytes.size())});u32 cameraCut=0;bool found=false;
    for(u32 at=evd.packetOffset;at<evd.packetOffset+evd.packetSize;){auto size=half(evd.bytes,at+12);if(size<16||size>evd.packetOffset+evd.packetSize-at)throw std::invalid_argument("camera packet boundary");auto type=word(evd.bytes,at);
        if(type==6&&!found){if(size<64||half(evd.bytes,at+10)!=0)throw MissingGameConsumer("first event camera packet does not start at frame0");scene.cameraName=text(evd.bytes,at+16,48);cameraCut=half(evd.bytes,at+8);found=true;}
        if(type==9&&found&&half(evd.bytes,at+8)==cameraCut&&text(evd.bytes,at+16,12)=="cam0000")throw MissingGameConsumer("event camera parent/base matrix required");at+=size;
    }
    if(!found)throw MissingGameConsumer("event has no original camera packet");auto cameraBin=std::find_if(evd.bins.begin(),evd.bins.end(),[&](const auto &b){return b.name==scene.cameraName;});if(cameraBin==evd.bins.end())throw std::invalid_argument("event camera bin reference");
    ModelCpu cameraValidation;cameraValidation.nParts=4;MotionAsset motion(cameraBin->bytes,cameraValidation);if(motion.data.nParts!=4||frame>(motion.data.maxFrame&0x3fff))throw std::invalid_argument("camera track count/frame");
    auto numbers=motion.data.numbers;std::sort(numbers.begin(),numbers.end());if(numbers!=std::vector<u8>{0,1,2,3})throw std::invalid_argument("camera requires exactly one position/target/roll/FOV track");
    DiagnosticCameraMotion camera;auto &w=camera.m_info;w.Joint_num=motion.data.nParts;w.pJoint_kind=motion.data.kinds.data();w.pJoint_no=motion.data.numbers.data();w.pHermite_data=motion.data.streams.data();w.Mot_frame=float(frame);w.Mot_frame_max=float((motion.data.maxFrame&0x3fff)+1);w.Mot_attr=8;
    camera.move();scene.camera=camera;scene.cameraFrame=frame;if(scene.camera.param.Fovy<=0||scene.camera.param.Fovy>=180)throw std::invalid_argument("original camera FOV");
    C_MTXLookAt(scene.camera.view,&scene.camera.param.Campos,&scene.camera.Up,&scene.camera.param.Target);C_MTXPerspective(scene.camera.projection,scene.camera.param.Fovy,1.3333334f,100.f,1000000.f); // camera.cpp/MainInit constants.
    for(u32 r=0;r<4;++r)for(u32 c=0;c<4;++c){float v=0;for(u32 k=0;k<4;++k)v+=scene.camera.projection[r][k]*(k<3?scene.camera.view[k][c]:(c==3?1.f:0.f));scene.viewProjection[c*4+r]=v;}
    scene.owners.push_back(body);scene.owners.push_back(cameraBin->bytes);return scene;
}
std::string GxScene::report() const {
    Sha256 geometry;geometry.update(reinterpret_cast<const u8*>(vertices.data()),vertices.size()*sizeof(GxVertex));geometry.update(reinterpret_cast<const u8*>(indices.data()),indices.size()*4);
    std::ostringstream o;o<<"{\"pointer_bits\":"<<sizeof(void*)*8<<",\"kind\":\"isolated_original_SMD_GX_geometry_unlit_materials\",\"models\":"<<models<<",\"vertices\":"<<vertices.size()<<",\"triangles\":"<<indices.size()/3<<",\"primitives\":"<<primitives<<",\"draws\":"<<draws.size()<<",\"textures\":"<<textures.size()<<",\"room_sha256\":"<<std::quoted(roomHash)<<",\"smd_sha256\":"<<std::quoted(smdHash)<<",\"geometry_sha256\":"<<std::quoted(geometry.finish())<<",\"camera\":"<<std::quoted(cameraName)<<",\"frame\":"<<cameraFrame<<",\"fovy\":"<<camera.param.Fovy<<",\"texture_hashes\":[";
    for(std::size_t i=0;i<textures.size();++i){if(i)o<<',';Sha256 h;for(auto &m:textures[i].levels)h.update(m.rgba.data(),m.rgba.size());o<<std::quoted(h.finish());}
    o<<"],\"camera_point\":["<<std::setprecision(9)<<camera.param.Campos.x<<','<<camera.param.Campos.y<<','<<camera.param.Campos.z<<','<<camera.param.Target.x<<','<<camera.param.Target.y<<','<<camera.param.Target.z<<','<<camera.param.Roll<<','<<camera.param.Fovy<<"],\"vp\":[";
    for(u32 i=0;i<16;++i){if(i)o<<',';o<<viewProjection[i];}
    o<<"],\"materials\":[";for(std::size_t i=0;i<draws.size();++i){auto &d=draws[i];if(i)o<<',';o<<'['<<d.workIndex<<','<<d.workId<<','<<d.binId<<','<<d.tplId<<','<<d.partIndex<<','<<d.texture<<','<<d.alphaTexture<<','<<unsigned(d.flags)<<','<<unsigned(d.alphaRef)<<','<<unsigned(d.cull)<<','<<d.indexCount<<','<<unsigned(d.blendMode)<<','<<d.materialColor<<','<<unsigned(d.otType)<<','<<d.smxFlags<<','<<d.uvScrollU<<','<<d.uvScrollV<<']';}
    o<<"],\"limitations\":[\"unlit diagnostic: GX lighting/TEV channel computation absent\",\"SMD file order; original OT integration absent\",\"no event model draw, shadows, fog, mirror, indirect TEV or postprocessing\"]}";return o.str();
}
} // namespace re4::port
