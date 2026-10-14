#!/usr/bin/env python3
"""Native tables from frozen controllers, damage tables and marker positions.

ARC numbers are the original four-header-word indices, never asset guesses.
Baseline speed/reload levels are used by the current test inventory.
"""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
src = root / 'upstream/re4/src'
def table(path, name):
    text = (src / path).read_text()
    match = re.search(r'\b' + name + r'\s*\[[^=]+?=\s*\{(.*?)\};', text, re.S)
    if not match:
        raise ValueError(name)
    return match[1].strip()
def rows(path, name):
    return re.findall(r'\{([^{}]+)\}', table(path, name))
def firsts(name):
    return [float(row.split(',')[0].strip().removesuffix('f'))
            for row in rows('game/pl_class.cpp', name)]
shots, releases = firsts('PlShotFrameTbl'), firsts('PlReloadEndTbl')
# pl_handgun / pl_shotgun / pl_rifle / pl_machine / pl_rocket /
# pl_grenade / pl_bow; Wep17_move and Wep14_move are separate families.
# draw, down, idle pitch triple, fire pitch triple, base reload.
controllers = {
 'Handgun': [0x22,0x24,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2d],
 'Shotgun': [0x18,0x1f,0x1a,0x20,0x22,0x1e,0x21,0x23,0x2a],
 'Rifle': [0x14,0x16,0x15,0x15,0x15,0x15,0x15,0x15,0x17],
 'Machine': [0x1a,0x1d,0x1b,0x1f,0x21,0x1c,0x20,0x22,0x1e],
 'Rocket': [0x18,0x19,0xf,0x12,0x14,0x11,0x13,0x15,0x18],
 'Mine': [0x12,0x15,0x13,0x17,0x19,0x14,0x18,0x1a,0x16],
 'Matilda': [0x11,0x15,0x12,0x17,0x19,0x3d,0x3e,0x3f,0x40],
 'Grenade': [0xf,0x10,0x11,0x14,0x17,0x12,0x15,0x18,0xf],
 'Bow': [0x1f,0x20,0x21,0x24,0x27,0x22,0x25,0x28,0x1f],
}
families = {**dict.fromkeys([1,2,3,4,5,6,15], 'Handgun'),
            **dict.fromkeys([7,8,33], 'Shotgun'),9:'Rifle',10:'Rifle',
            11:'Machine',12:'Machine',13:'Rocket',14:'Mine',17:'Matilda',
            19:'Grenade',22:'Grenade',23:'Grenade',28:'Bow'}
# cObj*::{resetMotion,moveFire,moveReload}, nonempty and empty variants.
local = {
 1:[0x34,0x32,0x36,0x39,0x37,0x33],
 2:[0x36,0x34,0x38,0x3b,0x39,0x35],
 3:[0x34,0x32,0x36,0x38,0x37,0x33],
 4:[0x34,0x32,0x36,0x39,0x37,0x33],
 5:[0x34,0x32,0x33,0x34,0x32,0x33],
 6:[0x34,0x32,0x36,0x3a,0x35,0x33],
 7:[0x31,0x30,0x2b,0x31,0x32,0x2b],
 8:[0x31,0x30,0x2b,0x31,0x30,0x2b],
 9:[0x23,0x23,0x22,0x23,0x23,0x22],
 10:[0x22,0x22,0x21,0x22,0x22,0x21],
 11:[0x2a,0x27,0x29,0x2f,0x2d,0x28],
 12:[0x29,0x27,0x28,0x29,0x2a,0x28],
 13:[0x1d,0x1d,0x1d,0x1d,0x1d,0x1d],
 14:[0x21,0x1e,0x1f,0x21,0x1e,0x1f],
 15:[0x34,0x32,0x33,0x34,0x32,0x33],
 17:[0x36,0x35,0x26,0x38,0x37,0x25],
 19:[0,0,0,0,0,0],22:[0,0,0,0,0,0],23:[0,0,0,0,0,0],
 28:[0x2d,0x2f,0x2d,0x2d,0x2c,0x2d],
 33:[0x31,0x30,0x2b,0x31,0x32,0x2b],
}
commit = {1:31,2:31,3:44,4:31,5:75,6:44,7:30,8:35,
          9:10,10:34,11:40,12:40,13:0,14:74,15:34,17:31,
          19:0,22:0,23:0,28:0,33:30}
marker = rows('game/objWep.cpp','ofs')
lines = ['// Generated from frozen weapon controllers, pl_class, objWep and em_dm_val.',
         '#pragma once', '#include <array>', 'namespace re4 {',
         'enum class WeaponFamily { Handgun, Shotgun, Rifle, Machine, Rocket, Mine, Matilda, Grenade, Bow };',
         'struct WeaponActionSource { unsigned number; WeaponFamily family; std::array<unsigned,9> player; std::array<unsigned,6> object; unsigned drawReady,fireReady,reloadCommit,reloadRelease; std::array<float,3> marker; };',
         'inline const WeaponActionSource *OriginalWeaponAction(unsigned number) {',
         ' static constexpr WeaponActionSource data[]{']
for no,family in sorted(families.items()):
    ready = 8 if no==8 else 10 if family=='Shotgun' else 5 if family=='Machine' else 4
    fire = 32 if no==8 else 40 if family=='Shotgun' else int(shots[no])-2 if family=='Handgun' else int(shots[no])
    if family=='Grenade':fire=30
    if family=='Matilda':fire=11
    if family=='Bow':fire=35
    action = controllers[family].copy()
    vals = [str(no),'WeaponFamily::'+family,
            '{'+','.join(map(str,action))+'}',
            '{'+','.join(map(str,local[no]))+'}',str(ready),str(fire),
            str(commit[no]),str(int(releases[no])),
            '{'+marker[no].strip()+'}']
    lines.append('  {'+','.join(vals)+'},')
lines += [' };',' for(const auto &v:data)if(v.number==number)return &v;',
          ' return nullptr;','}',
          'inline unsigned OriginalWeaponNumber(unsigned item) {',
          ' const auto *v=OriginalWeaponCarry(item); return v?v->number:0;','}',
          'inline bool OriginalShotgun(unsigned number) {return number==7 || number==8 || number==33;}',
          'inline unsigned OriginalShotTargets(unsigned number) {return number==1?2:number==5 || number==6?3:number==9 || number==10 || number==15?5:1;}',
          'inline unsigned OriginalWeaponSize(unsigned number) {',
          ' switch(number) {',
          ' case 5:case 6:case 7:case 8:case 9:case 10:case 13:case 14:case 15:case 18:case 19:case 21:case 22:case 30:case 33:case 40:case 41:case 44:return 1;',
          ' case 16:case 26:return 2;',
          ' case 0:case 20:case 34:case 35:case 36:case 37:return 3;',
          ' default:return 0;',' }','}',
          'inline constexpr unsigned OriginalGanadoBaseDamage[46] = {'+table('game/em_dm_val.cpp','Dmg_tbl_em10')+'};',
          'inline constexpr float OriginalWeaponPower[46][7] = {'+table('game/em_dm_val.cpp','WeaponLevelTbl')+'};',
          'inline constexpr float OriginalShotgunFarPower[3][7] = {'+table('game/em_dm_val.cpp','WeaponLevelTblShotGun')+'};',
          '} // namespace re4','']
lines.insert(3,'#include "weapon_carry_source.h"')
(root/'app/src/main/cpp/weapon_action_source.h').write_text('\n'.join(lines))
print('Original weapon action profiles:',len(families))
