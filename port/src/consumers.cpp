#include "re4/port/consumers.h"
#include "re4/port/game_runtime.h"
#include "model_original.h"
#include "re4/port/motion.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>

namespace re4::port {
namespace {
void range(ResourceLease b, std::size_t at, std::size_t bytes) {
    if (!b.owner || !b.data || at > b.size || bytes > b.size - at)
        throw std::invalid_argument("consumer resource range");
}
u32 word(ResourceLease b, std::size_t at) {
    range(b, at, 4);
    return (u32(b.data[at]) << 24) | (u32(b.data[at + 1]) << 16) | (u32(b.data[at + 2]) << 8) |
           b.data[at + 3];
}
float real(ResourceLease b, std::size_t at) {
    u32 w = word(b, at);
    float f;
    std::memcpy(&f, &w, 4);
    if (!std::isfinite(f))
        throw std::invalid_argument("nonfinite model coordinate");
    return f;
}
std::string name(ResourceLease b, std::size_t at, std::size_t limit) {
    range(b, at, limit);
    auto *p = b.data + at;
    auto *end = std::find(p, p + limit, u8(0));
    if (end == p + limit || end == p ||
        !std::all_of(p, end, [](u8 c) { return c >= 32 && c < 127; }))
        throw std::invalid_argument("consumer name must be bounded ASCII");
    return {reinterpret_cast<const char *>(p), std::size_t(end - p)};
}
ResourceLease slice(ResourceLease b, u32 at, u32 size) {
    range(b, at, size);
    return {b.owner, b.data + at, size};
}
u64 identity() {
    static std::atomic<u64> sequence{1};
    auto id = sequence.fetch_add(1);
    if (!id)
        throw std::overflow_error("consumer identities exhausted");
    return id;
}
} // namespace
std::vector<EventResourceReference> indexEventResourceReferences(const EventPackageView &e) {
    auto stream=slice(e.bytes,e.packetOffset,e.packetSize);
    std::vector<EventResourceReference> refs;
    for(u32 at=0;at<stream.size;) {
        range(stream,at,16);
        auto type=word(stream,at);
        auto size=(u32(stream.data[at+12])<<8)|stream.data[at+13];
        if(size<16 || size>32767 || size>stream.size-at || type>=33)
            throw std::invalid_argument("EvtPacket id/size boundary");
        auto packet=slice(stream,at,size);
        if(type==3 || type==4 || type==11) {
            EventResourceReference r; r.packetOffset=e.packetOffset+at; r.type=type;
            r.model=name(packet,16,12);
            auto pathAt=type==4 ? 40u:28u;
            if(type==4) r.parent=name(packet,28,12);
            r.bin=name(packet,pathAt,48);
            if(type!=11) r.tpl=name(packet,pathAt+48,48);
            refs.push_back(std::move(r));
        }
        at+=size;
    }
    return refs;
}
EventPackageView indexEventPackage(ResourceLease b) {
    range(b, 0, 0x50);
    if (std::memcmp(b.data, "event", 5))
        throw std::invalid_argument("EVD signature");
    EventPackageView e{b, name(b, 0x20, 8), name(b, 0x28, 12), word(b, 0x40), word(b, 0x44), {}};
    range(b, e.packetOffset, e.packetSize);
    u32 n = word(b, 0x48), table = word(b, 0x4c);
    if (n > b.size / 64)
        throw std::invalid_argument("EVD bin count");
    range(b, table, std::size_t(n) * 64);
    std::size_t previous = std::size_t(table) + std::size_t(n) * 64;
    std::map<std::string, bool> seen;
    for (u32 i = 0; i < n; ++i) {
        auto p = std::size_t(table) + std::size_t(i) * 64;
        auto nm = name(b, p, 48);
        u32 off = word(b, p + 48), size = word(b, p + 52);
        if (off < previous || !seen.emplace(nm, true).second || word(b, p + 56) || word(b, p + 60))
            throw std::invalid_argument("EVD bin ordering/name/padding");
        e.bins.push_back({std::move(nm), slice(b, off, size)});
        previous = std::size_t(off) + size;
    }
    return e;
}
CameraDataView indexCameraData(ResourceLease b) {
    range(b, 0, 16);
    if (std::memcmp(b.data, "B40", 3) || b.data[3] < '2' || b.data[3] > '4')
        throw std::invalid_argument("unported camera version");
    CameraDataView result{b, u8(b.data[3] - '0'), b.data[4], b.data[5], b.data[6]};
    auto cutInfo = std::size_t(16), areas = cutInfo + std::size_t(result.areas) * 16;
    auto cameras = areas + std::size_t(result.areas) * 48;
    auto links = cameras + std::size_t(result.cameras) * 52;
    range(b, cutInfo, links - cutInfo + std::size_t(result.links) * 16);
    for (u32 i = 0; i < result.areas; ++i) {
        auto rec = cutInfo + std::size_t(i) * 16;
        u32 a = word(b, rec + 8), c = word(b, rec + 12);
        if (a < areas || (a - areas) % 48 || (a - areas) / 48 >= result.areas ||
            (c && (c < cameras || (c - cameras) % 52 || (c - cameras) / 52 >= result.cameras)))
            throw std::invalid_argument("camera link outside typed records");
        auto p = areas + std::size_t(i) * 48;
        u32 count = word(b, p + 40);
        if (count > b.size / 12)
            throw std::invalid_argument("camera vertex count");
        range(b, word(b, p + 44), std::size_t(count) * 12);
    }
    for (u32 i = 0; i < result.cameras; ++i) {
        auto p = cameras + std::size_t(i) * 52;
        auto count = word(b, p + 32);
        if (count > b.size / 12)
            throw std::invalid_argument("camera key count");
        for (auto field : {36u, 40u, 44u, 48u})
            range(b, word(b, p + field), std::size_t(count) * (field < 44 ? 12 : 4));
        auto frame = word(b, p + 16);
        if (frame && (frame > b.size || std::size_t(count) * 2 > b.size - frame))
            throw std::invalid_argument(
                "CAM pFrame outside immutable resource; relocation semantics unresolved");
    }
    return result;
}
struct EngineConsumers::Model {
    ResourceLease bin;
    std::string name;
    std::vector<ModelJoint> heads;
    std::vector<ModelPart> parts;
    ModelDataView data;
    ModelInfoView info;
    ModelCpu cpu;
    std::unique_ptr<MotionAsset> motion;
    std::unique_ptr<MotionSequenceView> sequence;
    std::unique_ptr<ModelRuntime::Object> initialized;
};
struct EngineConsumers::Message {
    ResourceLease bytes;
    u32 block{}, count{}, language{};
    std::vector<u32> lookup;
    MessageIndexCpu cpu;
};
EngineConsumers::EngineConsumers(NativeScheduler *domain)
    : domain_(domain), identity_(identity()),
      control_(std::make_shared<Control>(Control{this, owner_})) {}
EngineConsumers::~EngineConsumers() {
    try {
        clear();
    } catch (...) {
        std::terminate();
    } // Never free an executing/sleeping callback.
    control_->owner = nullptr;
}
void EngineConsumers::check() const {
    if (owner_ != std::this_thread::get_id())
        throw std::logic_error("consumer frame owner");
}
void EngineConsumers::validate(ConsumerHandle h) const {
    check();
    if (h.runtime != identity_ || h.epoch != epoch_ || !h.index)
        throw std::logic_error("stale/foreign consumer reference");
}
void EngineConsumers::clear() {
    check();
    for (const auto &call : invocations_)
        if (!call->finished && (!call->task || !domain_ || domain_->state(*call->task)))
            throw std::logic_error("consumer callback stack still live; quiesce before clear");
    if (epoch_ == std::numeric_limits<u64>::max())
        throw std::overflow_error("consumer epoch exhausted");
    ++epoch_; // Invalidate before releasing any borrowed storage.
    invocations_.clear();
    functions_ = {};
    events_.clear();
    eventResources_.clear();
    models_.clear();
    modelRuntime_.reset();
    messages_.clear();
    camera_.reset();
    cameraState = {};
    invocation_ = {};
}
ConsumerHandle EngineConsumers::model(ResourceLease b, std::string nm) {
    check();
    range(b, 0, 0x40);
    u32 version = word(b, 0x3c);
    if (version != 0x20010801 && version != 0x20030818)
        throw std::invalid_argument("model version not supported by original cModInfoMgr::create");
    u32 count = b.data[0x19], offset = word(b, 0);
    if (offset % 4)
        throw std::invalid_argument("model joint alignment");
    range(b, offset, std::size_t(count) * 16);
    auto m = std::make_unique<Model>();
    m->bin = b;
    m->name = std::move(nm);
    m->heads.resize(count);
    m->parts.resize(count);
    for (u32 i = 0; i < count; ++i) {
        auto p = std::size_t(offset) + i * 16;
        auto &h = m->heads[i];
        h.partsNo = b.data[p];
        h.parentNo = b.data[p + 1];
        h.x2 = b.data[p + 2];
        h.x3 = b.data[p + 3];
        if (h.parentNo != 255 && h.parentNo >= count)
            throw std::invalid_argument("model parent index");
        h.center = {real(b, p + 4), real(b, p + 8), real(b, p + 12)};
        m->parts[i].pos = h.center;
        m->parts[i].pList = i + 1 < count ? &m->parts[i + 1] : nullptr;
    }
    // Reject cyclic parent graphs before executing the original pointer traversal.
    for (u32 i = 0; i < count; ++i) {
        auto parent = i;
        for (u32 depth = 0; parent != 255; ++depth) {
            if (depth >= count)
                throw std::invalid_argument("cyclic model hierarchy");
            parent = m->heads[parent].parentNo;
        }
    }
    m->data.pHead=m->heads.data(); m->data.nParts=u8(count); m->data.version=version;
    m->info.model_addr = &m->data;
    m->cpu.nParts = u8(count);
    m->cpu.pModelInfo = &m->info;
    m->cpu.pList = count ? m->parts.data() : nullptr;
    m->cpu.setPartsParent();
    auto id = next_++;
    if (!id)
        throw std::overflow_error("consumer handles exhausted");
    models_.emplace(id, std::move(m));
    return {identity_, epoch_, id};
}
ModelCpu &EngineConsumers::model(ConsumerHandle h) {
    validate(h);
    auto &m=*models_.at(h.index);
    return m.initialized ? m.initialized->cpu : m.cpu;
}
void EngineConsumers::configureModelPools(u32 infos,u32 parts) {
    check();
    if(!models_.empty()) throw std::logic_error("model pool reconfiguration with live models");
    if(!infos || parts<2) throw std::invalid_argument("native model pool capacity");
    modelRuntime_=std::make_unique<ModelRuntime>(infos,parts);
}
void EngineConsumers::releaseModel(ConsumerHandle h) {
    check(); validate(h);
    if(domain_ && (domain_->current() || domain_->allocatedStacks()))
        throw std::logic_error("model release requires quiescent task borrowers");
    for(const auto &e:events_) {
        if(e.second->ModTbl.contains(&model(h))) throw std::logic_error("model is still registered in original event ModTbl");
        for(auto *p=model(h).pModelInfo;p;p=p->pList)
            if(e.second->ModTbl.contains(p)) throw std::logic_error("model info is still registered in original event ModTbl");
    }
    if(!models_.erase(h.index)) throw std::logic_error("retired model reference");
}
ConsumerHandle EngineConsumers::initializeModel(ResourceLease bin,ResourceLease tpl,std::string name,bool invisible) {
    check();
    if(!modelRuntime_) throw std::logic_error("original room model pool counts are required");
    auto model=std::make_unique<Model>(); model->name=std::move(name);
    model->initialized=modelRuntime_->initialize(bin,tpl,invisible);
    auto id=next_++;
    if(!id) throw std::overflow_error("consumer handles exhausted");
    models_.emplace(id,std::move(model)); return {identity_,epoch_,id};
}
std::size_t EngineConsumers::modelCount() const {
    check();
    return models_.size();
}
void EngineConsumers::initializeMotion(ConsumerHandle h,ResourceLease bytes,int frame,int flags,int hokan) {
    initializeMotion(h,bytes,ResourceLease{},frame,flags,hokan);
}
void EngineConsumers::initializeMotion(ConsumerHandle h,ResourceLease bytes,ResourceLease sequence,int frame,int flags,int hokan) {
    check(); validate(h);
    auto &m=*models_.at(h.index);
    if(!m.initialized) throw std::logic_error("Motion requires an actually initialized original model");
    if(frame<0 || frame>65535 || flags<0 || flags>65535 || hokan<0 || hokan>255)
        throw std::invalid_argument("original motion arguments");
    auto asset=std::make_unique<MotionAsset>(bytes,m.initialized->cpu);
    auto seq=sequence.owner ? std::make_unique<MotionSequenceView>(resolveMotionSequence(sequence)) : nullptr;
    if(!sequence.owner && (sequence.data || sequence.size)) throw std::invalid_argument("SEQ owner required");
    if(m.initialized->cpu.Motion.pAttachCam)
        throw MissingGameConsumer("MotionSetCore/attached camera owner not integrated");
    // Publish ownership before the original functions touch any pointer. On failure
    // retain the new asset: the partially initialized original work can reference it.
    m.motion=std::move(asset);
    m.sequence=std::move(seq);
    MotionClear(&m.initialized->cpu,1);
    MotionSetCore(&m.initialized->cpu,&m.initialized->cpu.Motion,&m.motion->data,m.sequence.get(),hokan,flags,frame);
}
Vec EngineConsumers::sampleMotion(ConsumerHandle h,u32 joint,f32 frame) const {
    check(); validate(h); const auto &m=*models_.at(h.index);
    if(!m.motion) throw std::logic_error("motion data not initialized");
    return m.motion->sample(joint,frame);
}
void EngineConsumers::registerEventModel(ConsumerHandle eh,ConsumerHandle mh,const std::string &name,u8 type) {
    check(); validate(eh); validate(mh);
    if(name.empty() || name.size()>11 || !models_.at(mh.index)->initialized)
        throw std::invalid_argument("original event model name/initialized object");
    if(!event(eh).SetMod(name.c_str(),&model(mh),type,nullptr,2,nullptr))
        throw std::runtime_error("original Event::SetMod failed");
}
ModelInfoView *EngineConsumers::createModelInfoFor(ModelCpu &cpu,const char *bin,const char *tpl,bool reset) {
    check();
    for(auto &entry:models_) if(entry.second->initialized && &entry.second->initialized->cpu==&cpu)
        return modelRuntime_->createAttachment(*entry.second->initialized,eventResources_.bin(bin),eventResources_.bin(tpl),reset);
    throw std::logic_error("attachment parent is not an owned initialized model");
}
ModelInfoView *EngineConsumers::createModelInfoFor(ModelCpu &cpu,ResourceLease bin,ResourceLease tpl) {
    check();
    for(auto &entry:models_) if(entry.second->initialized && &entry.second->initialized->cpu==&cpu)
        return modelRuntime_->createAttachment(*entry.second->initialized,bin,tpl,false);
    throw std::logic_error("attachment parent is not an owned initialized model");
}
void EngineConsumers::bindMessage(int type, ResourceLease b, u32 language) {
    check();
    if (type < 0 || type >= 5 || language != 1)
        throw std::invalid_argument("message type/English reference language");
    // Original MessageData::getAddr uses tbl[language+1], never assumes word0 is a language count.
    auto block = word(b, std::size_t(language + 1) * 4), n = word(b, std::size_t(block) + 4);
    if (block % 4)
        throw std::invalid_argument("message block alignment");
    if (n > b.size / 4)
        throw std::invalid_argument("message count");
    range(b, std::size_t(block) + 8, std::size_t(n) * 4);
    auto minimum = std::size_t(8) + std::size_t(n) * 4;
    for (u32 i = 0; i < n; ++i) {
        auto offset = word(b, std::size_t(block) + 8 + std::size_t(i) * 4);
        if (offset < minimum || offset % 2)
            throw std::invalid_argument("message offset");
        range(b, std::size_t(block) + offset, 2);
    }
    auto m = std::make_unique<Message>();
    m->bytes = b;
    m->block = block;
    m->count = n;
    m->language = language;
    m->lookup.resize((std::size_t(b.size) + 3) / 4);
    std::memcpy(m->lookup.data(), b.data, b.size);
    auto *p = reinterpret_cast<u8 *>(m->lookup.data());
    auto store = [&](std::size_t at, u32 value) { std::memcpy(p + at, &value, 4); };
    store(std::size_t(language + 1) * 4, block);
    store(block, word(b, block));
    store(std::size_t(block) + 4, n);
    for (u32 i = 0; i < n; ++i) {
        auto at = std::size_t(block) + 8 + std::size_t(i) * 4;
        store(at, word(b, at));
    }
    m->cpu.m_language = language;
    m->cpu.m_Data[0] = p;
    messages_[type] = std::move(m);
}
int EngineConsumers::messageCount(int type) const {
    check();
    return messages_.at(type)->cpu.getMesNum(0);
}
ResourceLease EngineConsumers::message(int type, int n) const {
    check();
    const auto &m = *messages_.at(type);
    if (n < 0 || u32(n) >= m.count)
        return {};
    auto *address = const_cast<Message &>(m).cpu.getAddr(n, 0);
    auto at = std::size_t(reinterpret_cast<const u8 *>(address) -
                          reinterpret_cast<const u8 *>(m.lookup.data()));
    // Only a checked address/span. Terminator/control-code interpretation remains original mes.cpp
    // work.
    return slice(m.bytes, static_cast<u32>(at), m.bytes.size - static_cast<u32>(at));
}
void EngineConsumers::bindCamera(ResourceLease b) {
    check();
    camera_ = indexCameraData(b);
}
const CameraDataView &EngineConsumers::camera() const {
    check();
    if (!camera_)
        throw MissingGameConsumer("CamCtrl.RoomDataRead/unbound CAM");
    return *camera_;
}
int EngineConsumers::SetFunc(const char *nm, EventFunction fn) {
    check();
    if (!nm || !*nm || std::strlen(nm) > 47 || !fn)
        throw std::invalid_argument("event callback");
    for (auto &row : functions_)
        if (row.first == nm)
            return 1; // DatTbl::SetDat keeps original binding.
    for (auto &row : functions_)
        if (row.first.empty()) {
            auto control = control_;
            auto epoch = epoch_;
            row = {nm, [control, epoch, fn = std::move(fn)](EventCpu *e, u32 p) {
                       if (control->thread != std::this_thread::get_id())
                           throw std::logic_error("event callback frame owner");
                       if (!control->owner)
                           throw std::logic_error("destroyed event callback owner");
                       auto &owner = *control->owner;
                       owner.check();
                       if (owner.epoch_ != epoch || !owner.invocation_)
                           throw std::logic_error("stale/unbound event callback");
                       owner.invocations_.erase(
                           std::remove_if(owner.invocations_.begin(), owner.invocations_.end(),
                                          [](const auto &r) { return r->finished; }),
                           owner.invocations_.end());
                       if (owner.invocations_.size() >= 18)
                           throw std::logic_error("event invocation ledger full");
                       auto record = std::make_unique<Invocation>();
                       record->task = owner.domain_ ? owner.domain_->current() : std::nullopt;
                       record->call = [&fn, e, p] { fn(e, p); };
                       auto *borrow = record.get();
                       owner.invocations_.push_back(std::move(record));
                       try {
                           owner.invocation_(borrow->call);
                           borrow->finished = true;
                       } catch (...) {
                           borrow->finished = true;
                           throw;
                       }
                   }};
            return 1;
        }
    throw std::runtime_error("original FuncTbl capacity 16 exhausted");
}
int EngineConsumers::GetFunc(EventFunction *out, const char *nm) {
    check();
    if (!out)
        return 0;
    *out = {};
    for (auto &row : functions_)
        if (row.first == nm) {
            *out = row.second;
            return 1;
        }
    return 0;
}
int EngineConsumers::GetFunc(const EventFunction **out, const char *nm) {
    check();
    if (!out)
        return 0;
    *out = nullptr;
    for (auto &row : functions_)
        if (row.first == nm) {
            *out = &row.second;
            return 1;
        }
    return 0;
}
ConsumerHandle EngineConsumers::event(ResourceLease b) {
    check();
    auto e = std::make_unique<EventCpu>();
    e->package = indexEventPackage(b);
    eventResources_.registerPackage(e->package.room+e->package.number,e->package);
    e->manager = this;
    auto id = next_++;
    if (!id)
        throw std::overflow_error("consumer handles exhausted");
    events_.emplace(id, std::move(e));
    return {identity_, epoch_, id};
}
EventCpu &EngineConsumers::event(ConsumerHandle h) {
    validate(h);
    return *events_.at(h.index);
}
void EngineConsumers::invoke(ConsumerHandle h, int mode, int arg) {
    validate(h);
    auto *e = &event(h);
    if (!invocation_)
        throw std::logic_error("event callback requires bound module lifetime");
    if (!e->ExeFunc(mode, arg))
        throw MissingGameConsumer("Event::ExeFunc/unregistered original callback");
}
void EngineConsumers::setInvocationBoundary(std::function<void(const TaskEntry &)> f) {
    check();
    invocation_ = std::move(f);
}
bool EventCpu::FlgCkStatus(u32 n) const {
    if (n >= 32)
        throw std::out_of_range("event status bit");
    return (StatusFlag & (0x80000000u >> n)) != 0;
}
void EventCpu::GetRoomNo(char *p) const {
    if (package.room.size() > 7)
        throw std::invalid_argument("event room name");
    std::memcpy(p, package.room.c_str(), package.room.size() + 1);
}
void EventCpu::GetEventNo(char *p) const {
    if (package.number.size() > 7)
        throw std::invalid_argument("event number exceeds original ExeFunc buffer");
    std::memcpy(p, package.number.c_str(), package.number.size() + 1);
}
} // namespace re4::port
