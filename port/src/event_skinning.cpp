#include "motion_original.h"
#include "re4/port/event_skinning.h"
#include "re4/port/sha256.h"
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <set>
#include <sstream>

namespace re4::port {
namespace {
u16 half(ResourceLease b,u32 p) { if(p>b.size || b.size-p<2)throw std::invalid_argument("skin event extent");return u16(u16(b.data[p])<<8|b.data[p+1]); }
u32 word(ResourceLease b,u32 p) {return u32(half(b,p))<<16|half(b,p+2);}
std::string text(ResourceLease b,u32 p,u32 n) {
    if(p>b.size || n>b.size-p)throw std::invalid_argument("skin event name extent");auto end=std::find(b.data+p,b.data+p+n,u8(0));
    if(end==b.data+p+n)throw std::invalid_argument("skin event name terminator");return {reinterpret_cast<const char*>(b.data+p),std::size_t(end-b.data-p)};
}
std::shared_ptr<const LoadedFile> load(EngineLoader &l,const std::string &path,u16 mode=0x100) {
    auto r=l.request(path,mode);auto end=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    while(l.status(r)==LoadStatus::Pending || l.status(r)==LoadStatus::Reading){l.frame();std::this_thread::yield();if(std::chrono::steady_clock::now()>end)throw std::runtime_error("skin original file deadline");}
    std::shared_ptr<const LoadedFile> file;if(l.readCheck(r,file)!=1 || !file)throw std::runtime_error("skin original file load: "+path);return file;
}
struct Retirement { EngineConsumers &e;~Retirement(){e.clear();} };
}
EventSkinModel captureSkinnedModel(ModelCpu &m,ModelInfoView &info) {
    EventSkinModel out;out.skin=skinModelCpu(m,info);out.position=m.pos;out.angle=m.ang;out.scale=m.scale;out.cullMode=m.CullMode;out.otType=m.ot_type;
    // commonModelTrans: weighted coordinates are relative to the FIRST parts
    // matrix. Only its single-part static branch uses the BIN head parts number.
    auto *root=out.skin.staticBypass?m.getPartsPtr(info.model_addr->pHead[0].partsNo):m.pList;if(!root)throw std::invalid_argument("skin draw root");
    C_MTXConcat(root->mat,info.mat,out.world);C_MTXCopy(root->mat,out.rootMatrix);C_MTXCopy(info.mat,out.infoMatrix);out.rootWorld=root->world;
    out.boundOffset=info.bound.offset;out.boundSize=info.bound.size;
    for(auto *p=m.pList;p;p=p->pList){std::array<float,12> matrix{},bind{};std::memcpy(matrix.data(),p->mat,48);std::memcpy(bind.data(),p->lt_inv_mat,48);out.bones.push_back(matrix);out.bind.push_back(bind);out.localPosition.push_back(p->pos);out.localAngle.push_back(p->ang);out.localScale.push_back(p->scale);}
    return out;
}
EventSkinSnapshot evaluateEventSkinning(EngineLoader &loader,EngineConsumers &engine,const std::string &path,u32 cut,u32 frame) {
    if(engine.modelCount())throw std::logic_error("event skinning requires quiescent model owners");
    if(cut>32767 || frame>65535)throw std::invalid_argument("event skinning cut/frame");
    engine.clear();Retirement retire{engine};EventSkinSnapshot result;result.cut=cut;result.frame=frame;
    auto player=load(loader,"em/pl00.drs",0x8001);engine.eventResources().bindPlayerArchive({player,player->bytes.data()+player->bodyOffset,player->bodySize});
    auto file=load(loader,path);ResourceLease raw{file,file->bytes.data(),u32(file->bytes.size())};result.eventHash=sha256(raw.data,raw.size);
    auto eh=engine.event(raw);auto &evt=engine.event(eh);auto refs=indexEventResourceReferences(evt.package);
    std::set<std::string> names;u32 slots=0,parts=2;
    for(auto &r:refs)if(r.type!=11 && names.insert(r.model).second){auto b=engine.eventResources().bin(r.bin);++slots;if(r.type==3)parts+=b.data[25];}
    engine.configureModelPools(slots,parts);std::map<std::string,ConsumerHandle> bodies;
    std::map<std::string,ModelInfoView*> info;std::map<std::string,EventResourceReference> identity;
    for(auto &r:refs)if(r.type==3 && !bodies.count(r.model)) {
        auto h=engine.initializeModel(engine.eventResources().bin(r.bin),engine.eventResources().bin(r.tpl),r.bin);
        engine.registerEventModel(eh,h,r.model,2);bodies.emplace(r.model,h);info.emplace(r.model,engine.model(h).pModelInfo);identity.emplace(r.model,r);
    }
    for(auto &r:refs)if(r.type==4 && !info.count(r.model)) {
        if(!bodies.count(r.parent))throw MissingGameConsumer("event skinning attachment parent "+r.parent);
        if(!evt.ExePacket_SetPartsSub(r.model.c_str(),r.bin.c_str(),r.tpl.c_str(),r.parent.c_str()))throw std::runtime_error("original SetPartsSub failed");
        void *found{};u8 type{};if(!evt.GetMod(&found,r.model.c_str(),&type,nullptr) || type!=3 || !found)throw std::runtime_error("original skin event reference");
        info.emplace(r.model,static_cast<ModelInfoView*>(found));identity.emplace(r.model,r);
    }
    std::map<std::string,std::string> motions,blocked;
    std::map<std::string,u32> frames;
    // Typed BE packet adapter uses the original EvtPacket fields, not room rules.
    for(u32 at=evt.package.packetOffset;at<evt.package.packetOffset+evt.package.packetSize;){auto n=half(raw,at+12);
        if(n<16 || n>evt.package.packetOffset+evt.package.packetSize-at)throw std::invalid_argument("skin event packet size");
        auto type=word(raw,at);int pc=s16(half(raw,at+8)),pf=s16(half(raw,at+10));
        if(pc==int(cut) && pf>=0 && u32(pf)<=frame && (type==9 || type==11 || type==12)) {
            auto name=text(raw,at+16,12);
            if(type==12)result.dependencies.push_back("ShapeSet/ShapeMove morph packet: "+name);
            if(type==9 && bodies.count(name)) {
                if(n<68)throw std::invalid_argument("skin Pos packet");
                if(word(raw,at+4)&0xc0000000)blocked[name]="Event::ExePacket_Pos/OyaSetObj18 parent transform";
                else {auto &m=engine.model(bodies.at(name));m.pos={float(s32(word(raw,at+40))),float(s32(word(raw,at+44))),float(s32(word(raw,at+48)))};
                    m.ang={float(s32(word(raw,at+52)))*3.1415927f/180.f,float(s32(word(raw,at+56)))*3.1415927f/180.f,float(s32(word(raw,at+60)))*3.1415927f/180.f};}
            }
            if(type==11 && bodies.count(name)) {auto motion=text(raw,at+28,48);auto b=engine.eventResources().bin(motion);
                if(half(b,0)){motions[name]=motion;frames[name]=frame-u32(pf);}}
        }at+=n;
    }
    for(auto &body:bodies){auto &m=engine.model(body.second);RotMatrix(m.mat,&m.ang);TransMatrix(m.mat,&m.pos);ScaleMatrix(m.mat,&m.scale);m.partsMatCalc();m.partsWorldCalc();
        if(motions.count(body.first) && !blocked.count(body.first))try{
            engine.initializeMotion(body.second,engine.eventResources().bin(motions.at(body.first)),int(frames.at(body.first)),1,0);
            ClrShape(&m); // Original ExePacket_Mot clears the neutral morph channels.
            moveModelMotion(m); // actual MotionMove, no invented frame progression
        }catch(const MissingGameConsumer &e){blocked[body.first]=e.what();}
    }
    for(auto &record:info){auto &r=identity.at(record.first);auto parent=r.type==4?r.parent:r.model;auto &m=engine.model(bodies.at(parent));
        if(blocked.count(parent)){result.dependencies.push_back(parent+": "+blocked.at(parent));continue;}
        auto out=captureSkinnedModel(m,*record.second);
        out.name=r.model;out.parent=parent;out.bin=r.bin;out.tpl=r.tpl;out.texture=engine.eventResources().bin(r.tpl);
        out.motion=motions.count(parent)?motions.at(parent):"";out.motionFrame=frames.count(parent)?frames.at(parent):0;
        out.poseStatus=out.motion.empty()?"original bind pose":"original MotionMove forward kinematics and double joints";
        if(std::find(result.dependencies.begin(),result.dependencies.end(),"ShapeSet/ShapeMove morph packet: "+r.model)!=result.dependencies.end())out.poseStatus+="; skeletal/neutral-shape output only, active event morph pending";
        result.models.push_back(std::move(out));
    }
    return result;
}
std::string EventSkinSnapshot::report() const {
    std::ostringstream o;o<<std::setprecision(9)<<"{\"pointer_bits\":"<<sizeof(void*)*8<<",\"kind\":\"isolated_original_event_motion_and_skinning\",\"event_sha256\":"<<std::quoted(eventHash)<<",\"cut\":"<<cut<<",\"frame\":"<<frame<<",\"models\":[";
    for(std::size_t i=0;i<models.size();++i){if(i)o<<',';auto &m=models[i];o<<"{\"name\":"<<std::quoted(m.name)<<",\"parent\":"<<std::quoted(m.parent)<<",\"bin\":"<<std::quoted(m.bin)<<",\"bin_sha256\":"<<std::quoted(sha256(m.skin.source.data,m.skin.source.size))<<",\"motion\":"<<std::quoted(m.motion)<<",\"pose_status\":"<<std::quoted(m.poseStatus)<<",\"motion_frame\":"<<m.motionFrame<<",\"bones\":"<<m.skin.boneCount<<",\"influences\":"<<m.skin.influenceCount<<",\"extended\":"<<(m.skin.extended?"true":"false")<<",\"static_bypass\":"<<(m.skin.staticBypass?"true":"false");
        auto arrays=[&](const char *name,const auto &v){o<<",\""<<name<<"\":[";for(std::size_t j=0;j<v.size();++j){if(j)o<<',';o<<int(v[j]);}o<<']';};
        arrays("positions",m.skin.positions);arrays("normals8",m.skin.normals8);arrays("normals16",m.skin.normals16);
        auto matrices=[&](const char *name,const auto &v){o<<",\""<<name<<"\":[";for(std::size_t j=0;j<v.size();++j){if(j)o<<',';o<<'[';for(u32 k=0;k<12;++k){if(k)o<<',';o<<v[j][k];}o<<']';}o<<']';};
        matrices("palette",m.skin.palette);matrices("bone_matrices",m.bones);matrices("bind_matrices",m.bind);
        std::vector<std::array<float,12>> draw(1),offset(1);std::memcpy(draw[0].data(),m.world,48);std::memcpy(offset[0].data(),m.infoMatrix,48);matrices("draw_matrix",draw);matrices("info_matrix",offset);
        auto vecs=[&](const char *name,const auto &v){o<<",\""<<name<<"\":[";for(std::size_t j=0;j<v.size();++j){if(j)o<<',';o<<'['<<v[j].x<<','<<v[j].y<<','<<v[j].z<<']';}o<<']';};
        vecs("local_positions",m.localPosition);vecs("local_angles",m.localAngle);vecs("local_scales",m.localScale);o<<'}';
    }o<<"],\"dependencies\":[";for(std::size_t i=0;i<dependencies.size();++i){if(i)o<<',';o<<std::quoted(dependencies[i]);}o<<"]}";return o.str();
}
} // namespace re4::port
