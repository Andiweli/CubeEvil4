#pragma once
#include "re4/port/scheduler.h"
namespace re4::port::verification {
// ISOLATED ADAPTER TEST ONLY. This binds the actual one-line DvdReadProc
// wrapper to a test operation. No GameCube cDvd/queue/SDK behavior is claimed.
struct DvdProbe {
    TaskEntry operation;
    void ReadProc() { if (!operation) throw std::logic_error("No DVD probe operation"); operation(); }
};
extern thread_local DvdProbe Dvd;
void DvdReadProc();
}
