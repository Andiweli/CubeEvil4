#include "collision_original.h"
#include <atomic>
#include <algorithm>
#include <map>
#include <set>
#include <limits>
namespace re4::port::collision {
thread_local int SEck{};
thread_local u8 polyBit[0x400]{};
thread_local cSat *pBypassAt{};
}
namespace re4::port {
namespace {
std::atomic<u64> collisionIdentity{1};
void finite(Vec v) { if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))throw std::invalid_argument("SAT nonfinite vector"); }
struct SatAsset {
    ResourceLease source;
    collision::cSat sat;
    std::vector<Vec> vertices,normals,edges;
    std::vector<collision::SAT_POLY> polygons;
    std::map<std::size_t,std::unique_ptr<collision::cSatBlock>> blocks;
    std::map<std::size_t,std::vector<u16>> indices;
    std::set<std::size_t> visiting;
    u32 slot{};
    explicit SatAsset(ResourceLease b,int type):source(b) {
        range(0,1);
        if(b.data[0]!=0xff && (b.data[0]&0x80)) {
            if(type<0 || type>255)throw std::invalid_argument("SAT archive type");
            const auto tableEnd=word(4); // First offset bounds the original offset table.
            const auto at=std::size_t(4)+std::size_t(type)*4;
            if(tableEnd<8 || tableEnd>b.size || at+4>tableEnd)throw std::invalid_argument("SAT archive table bounds");
            const auto start=word(at);
            if(start<tableEnd || start>=b.size)throw std::invalid_argument("SAT archive offset");
            auto end=b.size;
            for(std::size_t p=4;p<tableEnd;p+=4){auto offset=word(p);if(offset>start && offset<end)end=offset;}
            source={b.owner,b.data+start,u32(end-start)};
        }
        range(0,20);
        auto &s=sat;s.vertex_num=half(2);s.normal_num=half(4);s.edge_num=half(6);s.polygon_num=half(10);s.bb_num=half(18);
        if(s.polygon_num>0x1fff)throw std::invalid_argument("original SAT dataCheck polygon limit");
        std::size_t at=20;
        auto vectors=[&](std::vector<Vec>&out,u32 n){range(at,std::size_t(n)*12);out.reserve(n);for(u32 i=0;i<n;++i){out.push_back(vector(at));at+=12;}};
        vectors(vertices,s.vertex_num);vectors(normals,s.normal_num);vectors(edges,s.edge_num);
        range(at,std::size_t(s.polygon_num)*20);polygons.resize(s.polygon_num);
        for(auto &p:polygons){for(int n=0;n<3;++n){p.v[n]=half(at+n*2);p.e[n]=half(at+8+n*2);if(p.v[n]>=vertices.size() || p.e[n]>=edges.size())throw std::invalid_argument("SAT polygon vertex/edge reference");}
            p.m_Normal=half(at+6);p.m_Dummy7=half(at+14);p.m_Status=word(at+16);if(p.m_Normal>=normals.size())throw std::invalid_argument("SAT polygon normal reference");at+=20;}
        s.vtx=vertices.data();s.norm_p=normals.data();s.edge_p=edges.data();s.poly_p=polygons.data();
        if(s.bb_num)s.block_p=block(at,0);
        if(blocks.size()!=s.bb_num)throw std::invalid_argument("SAT block count / reachable links");
    }
    void range(std::size_t at,std::size_t size) const {
        if(!source.owner || !source.data || at>source.size || size>source.size-at)throw std::invalid_argument("SAT file bounds");
    }
    u16 half(std::size_t at) const { range(at,2);return u16(u16(source.data[at])<<8|source.data[at+1]); }
    u32 word(std::size_t at) const { range(at,4);auto *p=source.data+at;return u32(p[0])<<24|u32(p[1])<<16|u32(p[2])<<8|p[3]; }
    Vec vector(std::size_t at) const { Vec v;u32 words[3]={word(at),word(at+4),word(at+8)};std::memcpy(&v,words,12);finite(v);return v; }
    collision::cSatBlock *block(std::size_t at,u32 depth) {
        if(depth>256 || at%2 || !visiting.insert(at).second)throw std::invalid_argument("SAT block cycle/depth/alignment");
        if(blocks.count(at))throw std::invalid_argument("SAT shared block link");
        range(at,36);auto b=std::make_unique<collision::cSatBlock>();auto *p=b.get();blocks.emplace(at,std::move(b));
        if(blocks.size()>sat.bb_num)throw std::invalid_argument("SAT excessive block count");
        p->min=vector(at);p->m_Size=vector(at+12);p->m_nFloor=half(at+24);p->m_nSlope=half(at+26);p->m_nWall=half(at+28);p->m_Flag=half(at+30);
        if(p->m_Size.x<0 || p->m_Size.z<0)throw std::invalid_argument("SAT negative block size");
        if(p->m_Flag&1)p->children=block(at+36,depth+1);
        else {auto &idx=indices[at];auto count=u32(p->m_nFloor)+p->m_nSlope+p->m_nWall;range(at+36,std::size_t(count)*2);idx.reserve(count);
            for(u32 n=0;n<count;++n){auto no=half(at+36+n*2);if(no>=sat.polygon_num)throw std::invalid_argument("SAT block polygon reference");idx.push_back(no);}p->idx=idx.data();}
        auto next=word(at+32);
        if(next){if(next>std::numeric_limits<std::size_t>::max()-at)throw std::invalid_argument("SAT block offset overflow");p->m_pList=block(at+next,depth+1);}
        visiting.erase(at);return p;
    }
};
}
struct CollisionRuntime::Impl {
    collision::cSatMgr manager;
    const std::thread::id owner=std::this_thread::get_id();
    const u64 identity=collisionIdentity.fetch_add(1);u64 epoch=1,next=1;
    std::map<u64,std::unique_ptr<SatAsset>> assets;
    std::vector<collision::cSat> empty;
    Impl(NativeGameState &g,bool effect):manager{&g,{},0,effect?1:0}{}
    void check() const { if(owner!=std::this_thread::get_id())throw std::logic_error("SAT frame owner"); }
    SatAsset &get(CollisionHandle h) const {check();if(h.runtime!=identity || h.epoch!=epoch || !assets.count(h.index))throw std::invalid_argument("stale SAT reference");return *assets.at(h.index);}
    void rebuild(){manager.pieces.clear();for(auto &p:empty){p.enabled=false;manager.pieces.push_back(&p);}for(auto &a:assets)manager.pieces[a.second->slot]=&a.second->sat;manager.nArray=u32(manager.pieces.size());collision::pBypassAt=nullptr;}
};
CollisionRuntime::CollisionRuntime(NativeGameState &g,bool e):impl_(std::make_unique<Impl>(g,e)){}
CollisionRuntime::~CollisionRuntime()=default;
void CollisionRuntime::configure(u32 capacity){auto &i=*impl_;i.check();if(!i.assets.empty())throw std::logic_error("SAT live pieces prevent pool resize");if(capacity>0x10000)throw std::invalid_argument("SAT capacity allocation limit");i.empty.resize(capacity);i.rebuild();}
CollisionHandle CollisionRuntime::create(ResourceLease b,int type,Vec pos,Vec rot){auto &i=*impl_;i.check();finite(pos);finite(rot);
    std::vector<bool> used(i.empty.size());for(const auto &e:i.assets)used[e.second->slot]=true;
    auto free=std::find(used.begin(),used.end(),false);if(free==used.end())throw std::runtime_error("SAT original pool capacity exhausted");
    auto a=std::make_unique<SatAsset>(b,type);a->slot=u32(free-used.begin());a->sat.setCoord(&pos,&rot);auto id=i.next++;i.assets.emplace(id,std::move(a));i.rebuild();return {i.identity,i.epoch,id};}
void CollisionRuntime::destroy(CollisionHandle h){auto &i=*impl_;i.get(h);i.assets.erase(h.index);i.rebuild();}
void CollisionRuntime::clear(){auto &i=*impl_;i.check();i.assets.clear();i.empty.clear();++i.epoch;i.rebuild();}
void CollisionRuntime::enable(CollisionHandle h,bool on){impl_->get(h).sat.enabled=on;}
void CollisionRuntime::setCoord(CollisionHandle h,Vec pos,Vec rot){finite(pos);finite(rot);impl_->get(h).sat.setCoord(&pos,&rot);}
CollisionSnapshot CollisionRuntime::snapshot(CollisionHandle h) const {auto &a=impl_->get(h);return {a.sat.vertex_num,a.sat.normal_num,a.sat.edge_num,a.sat.polygon_num,u32(a.blocks.size()),a.sat.enabled};}
CollisionHit CollisionRuntime::hitCheck(Vec a,Vec b,int flags,int mask){auto &i=*impl_;i.check();finite(a);finite(b);CollisionHit h;h.point=b;h.attribute=u32(i.manager.hitCheck(&a,&b,&h.point,&h.normal,flags,mask));finite(h.point);if(h.attribute)finite(h.normal);return h;}
f32 CollisionRuntime::getFloor(Vec pos,f32 above,f32 below,int mask){impl_->check();finite(pos);if(!std::isfinite(above)||!std::isfinite(below)||above<0||below<0)throw std::invalid_argument("SAT floor ray extent");return impl_->manager.getFloor(&pos,nullptr,above,below,mask);}
} // namespace re4::port
