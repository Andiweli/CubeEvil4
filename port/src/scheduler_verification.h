#pragma once
#include "re4/port/verification.h"
namespace re4::port {
void checkSchedulerStack(CheckGroup& group);
void checkSchedulerOrder(CheckGroup& group);
void checkSchedulerLifecycle(CheckGroup& group);
void checkSchedulerModules(CheckGroup& group);
}
