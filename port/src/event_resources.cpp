#include "re4/port/event_resources.h"
#include "re4/port/consumers.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace re4::port {
NativeDatTbl::NativeDatTbl(int count) : NumDatTbl(count), pWork(nullptr), storage_([count] {
    if(count <= 0) throw std::invalid_argument("event table capacity");
    return std::size_t(count);
}()) {
    pWork=storage_.data();
}
void NativeDatTbl::clear() { std::fill(storage_.begin(),storage_.end(),DatTblWork{}); }
bool NativeDatTbl::contains(const void *p) const {
    return std::any_of(storage_.begin(),storage_.end(),[p](const auto &w){return (w.FlagBe8&1) && w.Dat==p;});
}
EventResources::EventResources() = default;
struct PlayerEventBin { const char *name; u32 index; };
// Generated from the actual PL_ARC_PTR registrations in Event::ExeBeginEvt.
extern const std::array<PlayerEventBin,6> originalPlayerEventBins;
void EventResources::bindPlayerArchive(ResourceLease body) {
    if(!body.owner || !body.data) throw std::invalid_argument("player archive lease");
    auto index=indexAssetBody(body.data,body.size);
    for(const auto &r:originalPlayerEventBins) {
        if(r.index<4 || r.index-4>=index.entries.size()) throw std::invalid_argument("player PL_ARC index");
        const auto &e=index.entries[r.index-4];
        const auto expected=std::string(r.name).substr(std::strlen(r.name)-3);
        auto tag=std::string(e.tag.begin(),e.tag.begin()+3);
        std::transform(tag.begin(),tag.end(),tag.begin(),[](char c){return char(c>='A'&&c<='Z'?c+32:c);});
        if(tag!=expected) throw std::invalid_argument("player archive tag differs from original event registration");
        registerBin(r.name,{body.owner,body.data+e.offset,e.size});
    }
}
bool EventResources::valid(const void *p) const {
    for(const auto &e:leases_) if(e.second.data==p && e.second.owner && e.second.size) return true;
    for(const auto &e:packages_) {
        if(e.second.get()==p) return true;
        for(const auto &b:e.second->bins) if(b.bytes.data==p && b.bytes.size) return true;
    }
    return false;
}
void EventResources::registerBin(const std::string &name, ResourceLease b) {
    if(name.empty() || name.size()>47 || !b.owner || !b.data || !b.size)
        throw std::invalid_argument("event bin name/lease");
    // DatTbl preserves the first binding; don't replace its lifetime on a repeat registration.
    auto inserted=leases_.emplace(name,b);
    try {
        if(!SetBin(name.c_str(),const_cast<u8*>(inserted.first->second.data),nullptr,2))
            throw std::runtime_error("original SetBin rejected registration");
    } catch(...) { if(inserted.second) leases_.erase(inserted.first); throw; }
}
void EventResources::registerPackage(const std::string &name,const EventPackageView &p) {
    if(name.empty() || name.size()>47) throw std::invalid_argument("event package name");
    if(EvdTbl.ChkDat(name.c_str())) return;
    // Validate the original header and all bin bounds before publishing pointers.
    auto checked=std::make_shared<EventPackageView>(indexEventPackage(p.bytes));
    std::vector<DatTblWork> binSnapshot(BinTbl.pWork,BinTbl.pWork+BinTbl.NumDatTbl);
    auto oldLeases=leases_;
    packages_.emplace(name,checked);
    for(const auto &b:checked->bins) leases_.emplace(b.name,b.bytes);
    try {
        if(!SetEvd(name.c_str(),checked.get(),nullptr,2)) throw std::runtime_error("original SetEvd rejected registration");
    } catch(...) {
        // NativeDatTbl owns its backing array; copy the slots, never its pWork pointer.
        std::copy(binSnapshot.begin(),binSnapshot.end(),BinTbl.pWork);
        leases_=std::move(oldLeases); packages_.erase(name); EvdTbl.DelDat(name.c_str()); throw;
    }
}
ResourceLease EventResources::bin(const std::string &name) {
    void *p=nullptr;
    if(!GetBin(&p,name.c_str(),1)) return {};
    auto found=leases_.find(name);
    if(found==leases_.end() || found->second.data!=p) throw std::logic_error("event bin lease mismatch");
    return found->second;
}
void EventResources::clear() { BinTbl.clear(); EvdTbl.clear(); packages_.clear(); leases_.clear(); }
} // namespace re4::port
