#pragma once
#include <cstdint>
namespace re4 {
// main_sub.cpp InitGameTime/GetGameTime/SetGameTime operate on absolute
// OSTicksToSeconds values. An option pause folds the segment, then its exit
// resets the baseline. EVD, room loading, case and item messages keep running.
class OriginalSaveClock {
public:
  void reset(std::uint32_t savedSeconds,std::uint32_t now,bool running) {
    saved_=savedSeconds;start_=now;running_=running;
  }
  std::uint32_t seconds(std::uint32_t now) const {
    return saved_+(running_?now-start_:0u);
  }
  void fold(std::uint32_t now) {saved_=seconds(now);start_=now;}
  void setRunning(std::uint32_t now,bool running) {
    if(running_==running)return;
    if(running_)fold(now);
    else start_=now;
    running_=running;
  }
  bool running() const {return running_;}
private:
  std::uint32_t saved_{},start_{};
  bool running_{};
};
} // namespace re4
