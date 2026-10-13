#include "re4/port/shelf_callbacks.h"
#include "shelf_original.h"
#include "gx_original.h"
#include "re4/port/gx_pipeline.h"
#include "re4/port/sha256.h"
#include "re4/port/byte_order.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <thread>

namespace re4::port {
struct ShelfCallbackSession::Impl {
    GameRuntime &game;
    u16 room;
    u32 selection,visits{};
    GxScene source;
    std::unique_ptr<scroll::Domain> models;
    std::unique_ptr<shelf::Domain> original;
    shelf::SHELF_SMD parameter;
    std::array<scroll::cObj*,2> targets{};
    std::array<GxDraw,2> identities{};
    std::vector<std::shared_ptr<const void>> borrows;
    std::optional<TaskHandle> task;
    ModuleToken module{};
    bool returned{},cancelled{},opened{};
    std::array<f32,2> initial{};
    GxCamera camera{};
    std::array<float,16> vp{};
    Impl(GameRuntime &g,u16 r,u32 s):game(g),room(r),selection(s),parameter(shelf::originalShelf(r,s)) {
        auto &loader=game.loader();
        if(!loader.loadRoom(room,true))throw std::logic_error("shelf room load busy");
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
        while(loader.roomBusy()){loader.frame();std::this_thread::yield();if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("shelf room load timeout");}
        auto archive=loader.room();if(!archive)throw std::runtime_error(loader.roomError());
        source.roomId=room;source.roomHash=archive->source->identity.sha256;
        models=scroll::appendRoomScrollGeometry(loader,game.engineConsumers(),*archive,source,true);
        original=std::make_unique<shelf::Domain>(shelf::Domain{*models,{},{},{parameter.door[0],parameter.door[1]}});
        for(u32 n=0;n<2;++n) {
            targets[n]=models->SmdGetObjPtr(parameter.door[n]);auto &cpu=targets[n]->cpu();
            if(!cpu.pList)throw std::logic_error("original shelf has no joint owner");
            initial[n]=cpu.pList->ang.y;
            borrows.push_back(game.engineConsumers().borrowModel(*targets[n]->handle));
            auto i=std::find_if(source.draws.begin(),source.draws.end(),[&](const GxDraw &d){return d.workId==parameter.door[n];});
            if(i==source.draws.end())throw std::runtime_error("original shelf draw identity absent");
            identities[n]=*i;
        }
        auto closed=capture();
        std::array<float,3> low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};
        for(auto &v:closed.vertices)for(u32 a=0;a<3;++a){low[a]=std::min(low[a],v.position[a]);high[a]=std::max(high[a],v.position[a]);}
        Vec center{(low[0]+high[0])*.5f,(low[1]+high[1])*.5f,(low[2]+high[2])*.5f};
        float radius=std::max({high[0]-low[0],high[1]-low[1],high[2]-low[2]});
        if(!std::isfinite(radius)||radius<=0)throw std::runtime_error("empty original shelf camera bound");
        // Explicit diagnostic framing from native geometry, not an original CAM.
        camera.param.Campos={center.x+radius*.35f,center.y+radius*.2f,center.z+radius*2.2f};
        camera.param.Target=center;camera.param.Fovy=45;camera.Up={0,1,0};
        CameraSetOrientationUp(&camera);
        C_MTXLookAt(camera.view,&camera.param.Campos,&camera.Up,&camera.param.Target);
        camera.Look={center.x-camera.param.Campos.x,center.y-camera.param.Campos.y,center.z-camera.param.Campos.z};
        C_MTXPerspective(camera.projection,45,4.f/3.f,10,1000000);
        for(u32 r0=0;r0<4;++r0)for(u32 c=0;c<4;++c){float v=0;for(u32 k=0;k<4;++k)v+=camera.projection[r0][k]*(k<3?camera.view[k][c]:(c==3?1.f:0.f));vp[c*4+r0]=v;}
    }
    ~Impl(){retire();borrows.clear();original.reset();models.reset();}
    void retire(){if(!(module==ModuleToken{})){if(!game.loader().scheduler().invalidateModule(module))std::terminate();module={};}task.reset();}
    std::array<f32,2> angles() const {return {targets[0]->cpu().pList->ang.y,targets[1]->cpu().pList->ang.y};}
    void updateMatrices(){for(auto *obj:targets){obj->be_flag=obj->cpu().be_flag;obj->matUpdate();}}
    GxScene capture() {
        GxScene out;out.roomId=room;out.roomHash=source.roomHash;out.smdHash=source.smdHash;
        out.textures=source.textures;out.originalPoolSizes=source.originalPoolSizes;
        out.camera=camera;out.viewProjection=vp;out.cameraName="DEBUG geometry-bound shelf camera";
        out.debugView=1;out.debugSubmissionReady=true;
        out.lightDependencyError="DEBUG texture-only callback proof: original room lighting and CAM not exercised";
        out.cameraFrame=visits;out.scrollFunctions={"original SmdGetObjPtr/model partsMatCalc/partsWorldCalc", "original shelf callback; presentation reads native state"};
        for(u32 n=0;n<2;++n){auto *obj=targets[n];auto &m=obj->cpu();
            auto skin=skinModelCpu(m,*m.pModelInfo);Mtx world;
            auto *root=skin.staticBypass?m.getPartsPtr(obj->bin.data[*BigEndianView(obj->bin.data,obj->bin.size).u32(0)]):m.pList;
            C_MTXConcat(root->mat,m.pModelInfo->mat,world);
            auto id=identities[n];C_MTXCopy(m.mat,id.modelMatrix);C_MTXCopy(m.pList->mat,id.rootMatrix);
            id.rootWorld=m.pList->world;id.modelStateFlags=m.be_flag;
            C_MTXMultVec(m.pList->mat,&m.pModelInfo->bound.offset,&id.lightPosition);
            decodeGxModel(obj->bin,world,id.textureBase,id.textureCount,out,id,skin.staticBypass?nullptr:&skin);
            ++out.models;if(!skin.staticBypass)++out.skinnedModels;
        }
        if(camera.param.Fovy>0)buildOriginalOrderingTables(out);
        else out.debugSubmissionReady=false; // Constructor's geometry-only bound query.
        return out;
    }
};
ShelfCallbackSession::ShelfCallbackSession(GameRuntime &g,u16 r,u32 s):impl_(std::make_unique<Impl>(g,r,s)){}
ShelfCallbackSession::~ShelfCallbackSession()=default;
void ShelfCallbackSession::start(bool opened) {
    auto &d=*impl_;if(d.task)throw std::logic_error("shelf callback already submitted");
    if(d.cancelled)throw std::logic_error("retired callback requires a fresh session");
    d.opened=opened;d.module=d.game.loader().scheduler().createModuleLifetime();
    d.task=d.game.loader().scheduler().execScenario(18,[&d]{
        if(d.room==0x103)d.original->r103_openShelf_main(&d.parameter,d.opened?1:0);
        else d.original->r106_openShelf_main(int(d.selection),d.opened?1:0);
        d.returned=true;
    },1,2,&d.targets[0]->cpu(),d.module);
    if(!d.task)throw std::runtime_error("original scenario slots exhausted");
}
void ShelfCallbackSession::tick(){auto &d=*impl_;if(!d.task||d.cancelled)throw std::logic_error("retired/unsubmitted shelf tick");++d.visits;d.game.loader().scheduler().runScenarios();d.updateMatrices();}
bool ShelfCallbackSession::suspend(){auto &d=*impl_;return d.task&&d.game.loader().scheduler().suspend(*d.task);}
bool ShelfCallbackSession::resume(){auto &d=*impl_;return d.task&&d.game.loader().scheduler().signal(*d.task);}
void ShelfCallbackSession::cancel(){auto &d=*impl_;d.retire();d.cancelled=true;}
bool ShelfCallbackSession::finished() const {return impl_->returned;}
u32 ShelfCallbackSession::visits() const {return impl_->visits;}
GxScene ShelfCallbackSession::snapshot(){return impl_->capture();}
void ShelfCallbackSession::verifyReleaseBlocked(){auto &d=*impl_;bool blocked=false;try{d.game.engineConsumers().releaseModel(*d.targets[0]->handle);}catch(const std::logic_error&){blocked=true;}if(!blocked)throw std::logic_error("live original callback model was released");}
std::string ShelfCallbackSession::report() const {
    auto &d=*impl_;auto a=d.angles();std::ostringstream out;out<<std::setprecision(9);
    out<<"{\"kind\":\"isolated_original_shelf_callback\",\"pointer_bits\":"<<sizeof(void*)*8<<",\"room\":"<<d.room<<",\"selection\":"<<d.selection<<",\"visits\":"<<d.visits<<",\"returned\":"<<(d.returned?"true":"false")<<",\"cancelled\":"<<(d.cancelled?"true":"false")<<",\"already_opened\":"<<(d.opened?"true":"false")<<",\"initial_angles\":["<<d.initial[0]<<','<<d.initial[1]<<"],\"angles\":["<<a[0]<<','<<a[1]<<"],\"sleeps\":[";
    for(std::size_t i=0;i<d.original->sleeps.size();++i){if(i)out<<',';out<<'['<<d.original->sleeps[i][0]<<','<<d.original->sleeps[i][1]<<']';}
    out<<"],\"models\":[";
    for(u32 i=0;i<2;++i){if(i)out<<',';auto &o=*d.targets[i];auto &m=o.cpu();out<<"{\"work_id\":"<<unsigned(d.parameter.door[i])<<",\"bin_sha256\":"<<std::quoted(sha256(o.bin.data,o.bin.size))<<",\"tpl_sha256\":"<<std::quoted(sha256(o.tpl.data,o.tpl.size))<<",\"parts\":"<<unsigned(m.nParts)<<",\"flags\":"<<m.be_flag<<",\"root_flags\":"<<m.pList->be_flag<<"}";}
    out<<"],\"sound_requests\":[";for(std::size_t i=0;i<d.original->soundRequests.size();++i){if(i)out<<',';auto &q=d.original->soundRequests[i];out<<'['<<q.block<<','<<q.number<<']';}
    out<<"],\"sound_output\":\"isolated request capture; no audible SndCall playback\",\"camera\":\"DEBUG geometry-bound framing\"}";return out.str();
}
std::string verifyShelfCallbacks(GameRuntime &game,u16 room,u32 selection) {
    u32 checks=0;auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("shelf assertion "+std::to_string(checks));};
    std::string animated,opened,cancelled,before,after;
    {
        ShelfCallbackSession s(game,room,selection);s.verifyReleaseBlocked();before=s.snapshot().report();s.start();
        for(u32 i=0;i<5;++i)s.tick();check(!s.finished());check(s.suspend());auto frozen=s.snapshot().report();
        for(u32 i=0;i<3;++i)s.tick();auto frozen2=s.snapshot(); // Ignore diagnostic tick metadata; geometry must remain fixed.
        check(frozen.substr(frozen.find("geometry_sha256"),82)==frozen2.report().substr(frozen2.report().find("geometry_sha256"),82));check(s.resume());
        for(u32 i=0;i<40&&!s.finished();++i)s.tick();check(s.finished());animated=s.report();after=s.snapshot().report();
    }
    check(game.engineConsumers().modelCount()==0);check(game.loader().scheduler().allocatedStacks()==0);
    {ShelfCallbackSession s(game,room,selection);s.start(true);s.tick();check(s.finished());opened=s.report();}
    check(game.engineConsumers().modelCount()==0);
    {ShelfCallbackSession s(game,room,selection);s.start();for(u32 i=0;i<7;++i)s.tick();check(!s.finished());s.cancel();cancelled=s.report();bool blocked=false;try{s.tick();}catch(const std::logic_error&){blocked=true;}check(blocked);}
    check(game.engineConsumers().modelCount()==0);check(game.loader().scheduler().allocatedStacks()==0);
    {ShelfCallbackSession s(game,room,selection);s.start();for(u32 i=0;i<40&&!s.finished();++i)s.tick();check(s.finished());}
    check(game.engineConsumers().modelCount()==0);check(game.loader().scheduler().allocatedStacks()==0);
    return "{\"checks\":"+std::to_string(checks)+",\"animated\":"+animated+",\"opened\":"+opened+",\"cancelled\":"+cancelled+",\"before\":"+before+",\"after\":"+after+"}";
}
}
