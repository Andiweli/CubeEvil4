#!/usr/bin/env python3
"""Compare all supported brightness values with unchanged Bg_brightness_set."""
import pathlib
import subprocess
import tempfile
root = pathlib.Path(__file__).resolve().parents[1]
source = (root/'upstream/re4/src/game/main_sub.cpp').read_text()
start = source.index('void Bg_brightness_set(f32 bright)')
begin = source.index('{', start)
depth, end = 1, begin + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
routine = source[start:end]
pre = '''#include "display_brightness.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using f32=float;using u8=std::uint8_t;
struct GXRenderModeObj {u8 vfilter[7]{};int aa{};int sample_pattern{};} Rmode;
void GXSetCopyFilter(int,int,int,u8 *) {}
'''
post = '''int main() {for(int value=34;value<=114;++value) {Bg_brightness_set(float(value));const auto port=re4::DisplayBrightnessFilter(value);for(unsigned tap=0;tap<7;++tap)if(port[tap]!=Rmode.vfilter[tap])throw std::runtime_error("original/port brightness coefficient mismatch");}std::cout<<"Unchanged Bg_brightness_set oracle PASS: 567 exact coefficients\\n";}
'''
with tempfile.TemporaryDirectory(prefix='m35-brightness-source-') as temporary:
    work = pathlib.Path(temporary)
    test, executable = work/'reference.cpp', work/'reference'
    test.write_text(pre+routine+post)
    subprocess.run(['c++','-std=c++17','-ffp-contract=off','-I',str(root/'app/src/main/cpp'),str(test),str(root/'app/src/main/cpp/display_brightness.cpp'),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
