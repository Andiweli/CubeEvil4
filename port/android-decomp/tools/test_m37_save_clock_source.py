#!/usr/bin/env python3
"""Frozen absolute OS clock/results counter oracle.
Usage: test_m37_save_clock_source.py CXX CPP UPSTREAM
"""
from pathlib import Path
import subprocess
import sys
import tempfile


def body(source, signature):
    start = source.index(signature)
    first = source.index('{', start)
    depth, end = 1, first + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


cxx, cpp, upstream = sys.argv[1:]
cpp, upstream = Path(cpp).resolve(), Path(upstream).resolve()
main = (upstream / 'src/game/main_sub.cpp').read_text()
weapon = (upstream / 'src/game/pl_wep.cpp').read_text()
code = r'''
#include "save_clock.h"
#include "save_stats.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using u32=std::uint32_t;using OSTime=std::uint64_t;
OSTime millis;OSTime OSGetTime(){return millis;}
u32 OSTicksToSeconds(OSTime t){return u32(t/1000);}
struct Global{u32 play_time,game_start_time,c_hit_cnt,g_hit_cnt,c_shot_cnt,g_shot_cnt;}g;Global*pG=&g;
struct Player{unsigned id;}player;Player*pl=&player;unsigned weaponNo;
unsigned PlGetWeaponNo(){return weaponNo;}
'''
for signature in ['void SecToTime(', 'void InitGameTime()', 'u32 GetGameTime(', 'void SetGameTime()']:
    code += body(main, signature) + '\n'
code += 'void sourceCounters(unsigned flag,unsigned n){\n' + body(weapon, 'if (pl != 0 && pl->id == 0)') + '}\n'
code += r'''
int main(){try{unsigned checks=0;re4::OriginalSaveClock clock;g.play_time=3723;millis=101999;InitGameTime();clock.reset(g.play_time,OSTicksToSeconds(millis),true);bool running=true;
 for(unsigned frame=0;frame<10000;++frame){millis+=13+(frame*571)%1357;const auto now=OSTicksToSeconds(millis);
  if(frame%11==0){if(running)SetGameTime();else InitGameTime();running=!running;clock.setRunning(now,running);}
  if(running&&frame%17==0){SetGameTime();clock.fold(now);}
  const auto expected=running?GetGameTime(nullptr,nullptr,nullptr):g.play_time;++checks;
  if(clock.seconds(now)!=expected)throw std::runtime_error("frozen/native OS time/fold/option baseline mismatch");
 }
 for(weaponNo=0;weaponNo<0x30;++weaponNo)for(unsigned flag=0;flag<8;++flag)for(unsigned n=0;n<5;++n)for(player.id=0;player.id<2;++player.id){
  g.c_hit_cnt=g.g_hit_cnt=g.c_shot_cnt=g.g_shot_cnt=0;sourceCounters(flag,n);
  const auto native=re4::OriginalShotCounters(weaponNo,flag,n,player.id);++checks;
  if(g.c_hit_cnt!=unsigned(native.hit)||g.g_hit_cnt!=unsigned(native.hit)||g.c_shot_cnt!=unsigned(native.shot)||g.g_shot_cnt!=unsigned(native.shot))
   throw std::runtime_error("frozen/native player weapon accuracy/shot exclusion mismatch");
 }
 std::cout<<"M37 frozen Init/Get/SetGameTime and PlWepHitCheck2 counter tail: "<<checks<<" checks PASS\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
'''
with tempfile.TemporaryDirectory(prefix='re4-m37-clock-source-') as temp:
    temp = Path(temp)
    source, binary = temp/'clock.cpp', temp/'clock'
    source.write_text(code)
    subprocess.run([cxx, '-std=c++17', '-O2', '-I', str(cpp), str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
