#pragma once
#include "re4/port/consumers.h"
#include "re4/port/game_runtime.h"
#include <cstring>
namespace re4::port {
using cParts = ModelPart;
using cCoord = ModelCoord;
using ModelDataHead = ModelJoint;
struct ConsumerLog {
    template <class... T> [[noreturn]] void err(int, int, const char *s, T...) {
        throw std::runtime_error(s);
    }
};
inline ConsumerLog consumerLog;
inline ConsumerLog *pLog = &consumerLog;
} // namespace re4::port
#define VALID_PTR(p) ((p) != nullptr)
