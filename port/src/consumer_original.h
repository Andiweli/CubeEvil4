#pragma once
#include "re4/port/consumers.h"
#include "re4/port/game_runtime.h"
#include <cstring>
namespace re4::port {
using cParts = ModelPart;
using cCoord = ModelCoord;
using ModelDataHead = ModelJoint;
struct ConsumerLog {
    template <class... T> void warn(int, int, const char *, T...) {}
    template <class... T> [[noreturn]] void err(int, int, const char *s, T...) {
        throw std::runtime_error(s);
    }
};
inline ConsumerLog consumerLog;
inline ConsumerLog *pLog = &consumerLog;
struct EventLog {
    std::vector<std::string> diagnostics;
    template<class... T> void err(int,int,const char *message,T...) {
        if(diagnostics.size()==128) diagnostics.erase(diagnostics.begin());
        diagnostics.emplace_back(message);
    }
    template<class... T> void warn(int a,int b,const char *message,T... args) { err(a,b,message,args...); }
};
inline thread_local EventLog eventLog;
inline int HDReadDebugAlloc(const char *, void **, int) {
    throw MissingGameConsumer("EventMgr::GetBin/debug host path not registered in original data catalog");
}
inline void Debug_free(void *) {
    throw std::logic_error("event table must free leases through their owning adapter");
}
} // namespace re4::port
#define VALID_PTR(p) ((p) != nullptr)
