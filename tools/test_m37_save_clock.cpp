#include "save_clock.h"
#include <iostream>
#include <stdexcept>
int main(){try{unsigned checks=0;const auto check=[&](bool v,const char*s){++checks;if(!v)throw std::runtime_error(s);};
 re4::OriginalSaveClock clock;clock.reset(3723,100,true);
 check(clock.seconds(100)==3723,"source absolute seconds baseline");
 check(clock.seconds(120)==3743,"EVD and room-loading elapsed time counted without gameplay ticks");
 clock.setRunning(121,false);check(clock.seconds(500)==3744,"source option/Android background excluded after fold");
 clock.setRunning(500,true);check(clock.seconds(511)==3755,"source option exit resets segment baseline");
 clock.fold(512);check(clock.seconds(519)==3763,"repeated source saves preserve total elapsed seconds");
 clock.reset(10,0xfffffff0,true);check(clock.seconds(0x10)==42,"source u32 OS-second subtraction wraps correctly");
 clock.reset(0,100,false);clock.setRunning(999,false);check(clock.seconds(1000)==0,"repeated paused notification is inert");
 clock.setRunning(1000,true);clock.setRunning(1000,true);check(clock.seconds(1001)==1,"repeated running notification never restarts clock");
 std::cout<<"M37 source save clock: "<<checks<<" checks PASS\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
