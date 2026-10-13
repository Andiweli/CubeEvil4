#include "merchant_original.h"
#include <thread>

namespace re4::port {
struct MerchantRuntime::Impl {
    MerchantDomain original;
    std::thread::id owner=std::this_thread::get_id();
    bool initialized{};
    std::vector<std::string> trace;
    Impl(NativeGameState &g,u8 &language):original(g,language) {}
    void check() const { if(owner!=std::this_thread::get_id())throw std::logic_error("merchant owner thread"); }
};
MerchantRuntime::MerchantRuntime(NativeGameState &g,u8 &language):impl_(std::make_unique<Impl>(g,language)) {}
MerchantRuntime::~MerchantRuntime()=default;
void MerchantRuntime::gameInit() {
    auto &i=*impl_;i.check();i.initialized=false;i.original.MerchantGameInit();
    i.initialized=true;i.trace.push_back("MerchantGameInit:original-complete");
}
void MerchantRuntime::secondRoundInit() {
    auto &i=*impl_;i.check();if(!i.initialized)throw std::logic_error("merchant gameInit required");
    i.original.Merchant2ndRoundInit();i.trace.push_back("Merchant2ndRoundInit:original-complete");
}
void MerchantRuntime::clear() {
    auto &i=*impl_;i.check();i.original.merchantData[0]={};
    i.original.merchantChar.setChar(nullptr,nullptr,nullptr,nullptr,nullptr);i.initialized=false;i.trace.clear();
}
MerchantSnapshot MerchantRuntime::snapshot() const {
    auto &i=*impl_;i.check();return {i.original.merchantData[0],i.initialized,i.original.merchantChar.m_p_data!=nullptr};
}
void MerchantRuntime::save(MERCHANT_DATA &d) {
    auto &i=*impl_;i.check();if(!i.initialized)throw std::logic_error("merchant not initialized");
    i.original.MerchantDataSave(&d);
}
void MerchantRuntime::load(const MERCHANT_DATA &d) {
    auto &i=*impl_;i.check();if(!i.initialized)throw std::logic_error("merchant not initialized");
    i.original.MerchantDataLoad(const_cast<MERCHANT_DATA *>(&d));
}
bool MerchantRuntime::usesState(const NativeGameState &g) const { impl_->check();return impl_->original.pG==&g; }
const std::vector<std::string> &MerchantRuntime::trace() const { impl_->check();return impl_->trace; }
} // namespace re4::port
