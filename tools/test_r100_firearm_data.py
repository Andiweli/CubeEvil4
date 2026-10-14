#!/usr/bin/env python3
"""Source GetWepDmVal/em10SetDmVal/Rnd oracle for supported village firearms.
Usage: test_r100_firearm_data.py r100_firearm_tool
The original unmodified function bodies compile in a bounded native host.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def body(text, signature):
    start = text.index(signature)
    pos = text.index('{', start)
    depth = 1
    end = pos + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    root = Path(__file__).resolve().parent.parent
    tool = Path(sys.argv[1]).resolve()
    actual = subprocess.check_output([str(tool)])
    damage_source = (root / 'upstream/re4/src/game/em_dm_val.cpp').read_text()
    damage_source = re.sub(r'^#include.*$', '', damage_source, flags=re.M)
    enemy_source = (root / 'upstream/re4/src/em10/em10.cpp').read_text()
    rng_source = (root / 'upstream/re4/src/game/rnd.cpp').read_text()
    stub = r'''
#include <cstdint>
#include <iostream>
using u8=std::uint8_t;using u16=std::uint16_t;using u32=std::uint32_t;using f32=float;
struct YARARE_INFO {float len{};int parts_no{};};
struct Damage {unsigned m_Wep{};YARARE_INFO* m_pDamageYarare{};};
struct cEm {unsigned id=0x12;};
struct FREE_EM10 {unsigned Ganado{};void* pCore{};void* pParasite{};unsigned Wep_type=2;};
struct cEm10:cEm {unsigned type{};Damage dmg{};FREE_EM10 work{};};
FREE_EM10* EM10_WK(cEm10* em) {return &em->work;}
struct Global {unsigned weapon_lv_power{},weapon_no{},pl_type{};};
Global globals;Global* pG=&globals;
int em10ArmorCk(cEm10*,int) {return 0;}
static u16 Random;
'''
    main_cpp = r'''
void word(unsigned value) {for(int k=3;k>=0;--k)std::cout.put(char(value>>(k*8)));}
int main() {
 word(0x4d333647);word(3*7*4*2*258);
 cEm10 enemy;YARARE_INFO part;enemy.dmg.m_pDamageYarare=&part;
 for(unsigned weapon:{1u,2u,7u})for(unsigned level=0;level<7;++level)
  for(float distance:{0.f,35999996.f,36000000.f,36000004.f})for(bool head:{false,true})
   for(unsigned seedIndex=0;seedIndex<258;++seedIndex) {
    const unsigned seed=seedIndex<256?seedIndex:seedIndex==256?0x4e19:0xffff;
    globals.weapon_no=weapon;globals.weapon_lv_power=level;
    enemy.dmg.m_Wep=weapon;part.len=distance;part.parts_no=head?5:2;
    RndInit(u16(seed));const unsigned base=GetWepDmVal(&enemy,weapon,distance<36000000.f);
    const unsigned damage=em10SetDmVal(&enemy);
    const unsigned normal=head?unsigned(float(base)*1.2f):base;
    word(base);word(damage);word(head && damage!=normal);word(Random);
   }
}
'''
    original = stub + body(rng_source, 'void RndInit(') + '\n' + body(rng_source, 'u8 Rnd()')
    original += '\n' + damage_source + '\n' + body(enemy_source, 'int em10SetDmVal(cEm10* em)\n{') + '\n' + main_cpp
    with tempfile.TemporaryDirectory(prefix='re4-gun-damage-source-') as tmp:
        path = Path(tmp) / 'original.cpp'
        path.write_text(original)
        out = Path(tmp) / 'original'
        subprocess.run(['c++', '-std=c++17', '-fno-fast-math', '-ffp-contract=off',
                        str(path), '-o', str(out)], check=True, capture_output=True)
        expected = subprocess.check_output([str(out)])
    assert actual == expected, next((i, a, b) for i, (a, b) in enumerate(zip(actual, expected)) if a != b)
    print('M36 original firearm damage/RNG PASS: 43,344 supported profiles; 173,376 exact integer comparisons')


if __name__ == '__main__':
    main()
