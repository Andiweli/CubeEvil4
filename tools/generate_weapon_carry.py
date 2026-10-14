#!/usr/bin/env python3
"""Export frozen weapon identity/carry motions, without shipping DVD assets."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
src=root/'upstream/re4/src'
# cItemMgr::wep_info, ReadWepData (Leon) and each module's setMotion/init.
# Weapon families with a different attack controller remain explicit.
def body(text,name):
 m=re.search(r'void \w+::'+name+r'\([^)]*\)\s*\{',text)
 if not m:return ''
 tail=text[m.end():];depth=1
 for i,c in enumerate(tail):
  depth+=(c=='{')-(c=='}')
  if not depth:return tail[:i]
 return ''
shared={2:'wep/objRuger.cpp',3:'wep02/objMauser.cpp',11:'wep/objMachinegun.cpp',13:'game/objRocket.cpp'}
# no, model/palette in WEP or PL archive, right grip WEP, left grip PL or WEP,
# parent part, idle WEP ARC. No invented weapon meshes or poses.
models={
 1:(6,5,False,10,24,False,10,0x34),2:(6,5,False,10,24,False,10,0x36),
 3:(6,5,False,10,24,False,10,0x34),4:(6,5,False,10,24,False,10,0x34),
 5:(6,5,False,8,9,True,10,0x34),6:(6,5,False,8,9,True,10,0x34),
 7:(5,6,False,7,22,False,10,0x31),8:(5,6,False,7,25,False,10,0x31),
 9:(10,9,False,13,22,False,10,0x23),10:(10,9,False,13,22,False,10,0x22),
 11:(6,5,False,12,23,False,10,0x2a),12:(6,5,False,12,22,False,10,0x29),
 13:(0x76,0x75,True,7,24,False,10,0x1d),14:(8,7,False,9,24,False,9,0x21),
 15:(6,5,False,8,9,True,10,0x34),17:(6,5,False,9,24,False,10,0x36),
 19:(0x6a,0x6b,True,7,20,False,10,0),
 28:(5,6,False,0x12,7,True,16,0x2d),
 33:(5,6,False,7,22,False,10,0x31)}
itemtext=(src/'game/item.cpp').read_text().split('const ITEM_WEAPON_INFO wep_info[] = {',1)[1].split('};',1)[0]
rows=[]
for m in re.finditer(r'\{(0x[0-9A-Fa-f]+),\s*0,\s*(0x[0-9A-Fa-f]+),\s*(\d+),',itemtext):
 item,no,variant=int(m[1],16),int(m[2],16),int(m[3])
 if item>255 or item in (8,9,10,0x38):continue
 if no not in models and no not in (22,23):continue
 asset=no if no not in (22,23) else 19
 if no==3 and variant==2:asset=18
 if no==11 and variant>1:asset=20
 if no==9 and variant:asset=21 if variant==1 else 24
 if no==10 and variant:asset=31 if variant==1 else 32
 if no==14 and variant:asset=25
 if no==13 and variant==2:asset=30
 module=src/f'wep{asset:02d}'
 files=list(module.glob('*.cpp'))
 text='\n'.join(p.read_text() for p in files)
 if asset in shared:text+='\n'+(src/shared[asset]).read_text()
 # Infinite launcher data slot30 still runs cObjLauncher, not wep30's
 # unrelated Ada grenade module. Its physical archive is selected below.
 carry=body((src/shared[no]).read_text(),'setMotion') if no in (2,3,13) else body(text,'setMotion')
 pairs={int(a,0):int(b,0) for a,b in re.findall(r'WEP_MOT\((?:pl|pEm),\s*(0x\w+|\d+),\s*(0x\w+|\d+)\)',carry)}
 if not pairs:
  base=19 if no in (22,23) else no
  text='\n'.join(p.read_text() for p in (src/f'wep{base:02d}').glob('*.cpp'))
  if base in shared:text+='\n'+(src/shared[base]).read_text()
  carry=body(text,'setMotion')
  pairs={int(a,0):int(b,0) for a,b in re.findall(r'WEP_MOT\((?:pl|pEm),\s*(0x\w+|\d+),\s*(0x\w+|\d+)\)',carry)}
  if not pairs:raise ValueError(('No original motion table',item,no,asset))
 # Locomotion names are screen/controller directions. PadRead maps physical
 # right to Key4, whose pl_R1_Turn uses table0xD; left uses table0xF.
 # Preserve the default handgun's Left/Right order for every loaded profile.
 feet=[pairs[k] for k in (0,2,6,8,15,13,11)]
 config=list(models[no if no not in (22,23) else 19])
 if no in (22,23):config[1]=0x6d if no==22 else 0x6f
 if no in (1,2,3,4,6) and variant==1:config[0]=7
 # Dedicated accessory archives retain the module's selected model.
 if no==11:config[0]=6+variant
 # Logical weapon-data slot30 uses em/wep46.das with the wep13 controller.
 physical=46 if asset==30 else asset
 seq=[pairs.get(k,0) for k in (1,3,7,9,16,14,12)]
 rows.append((item,no,variant,physical,config,feet,seq,[pairs.get(k,0) for k in (0x57,0x58,0x5b,0x5c,0x55,0x56,0x59,0x5a)]))
lines=['// Generated from frozen item.cpp/read.cpp and weapon setMotion methods.',
'#pragma once','#include <array>','namespace re4 {',
'struct WeaponCarrySource { unsigned item,number,variant,archive,model,palette; bool playerModel; unsigned rightHand,leftHand; bool weaponLeft; unsigned parentPart,idle; std::array<unsigned,7> locomotion,footSequence; std::array<unsigned,8> knife; };',
'inline const WeaponCarrySource *OriginalWeaponCarry(unsigned item) {',
'  static constexpr WeaponCarrySource data[]{']
for item,no,variant,asset,c,foot,seq,knife in rows:
 vals=[str(item),str(no),str(variant),str(asset),str(c[0]),str(c[1]),str(c[2]).lower(),str(c[3]),str(c[4]),str(c[5]).lower(),str(c[6]),str(c[7]),'{'+','.join(map(str,foot))+'}','{'+','.join(map(str,seq))+'}','{'+','.join(map(str,knife))+'}']
 lines.append('    {'+','.join(vals)+'},')
lines+=['  };','  for(const auto &v:data)if(v.item==item)return &v;','  return nullptr;','}','} // namespace re4','']
(root/'app/src/main/cpp/weapon_carry_source.h').write_text('\n'.join(lines))
print('Original weapon carry profiles:',len(rows))
