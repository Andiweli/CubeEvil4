"""Check production command rules against exported original common ID units.
Usage: python3 tools/test_k17_fix15_menu_art.py ORIGINAL_COMMON_LAYOUT_JSON
"""
import json,re,sys
from pathlib import Path
source=Path(__file__).resolve().parents[1]/'app/src/main/java/com/ast/re4decomp/CaseMenuRules.java'
s=source.read_text()
base=[int(x,0) for x in re.search(r'BASE=\{([^}]+)',s)[1].split(',')]
height=[int(x) for x in re.search(r'HEIGHT=\{([^}]+)',s)[1].split(',')]
actions=json.loads(re.search(r'ACTIONS=(.*);',s)[1].replace('{','[').replace('}',']'))
units={q['mark']:q for q in json.loads(Path(sys.argv[1]).read_text())['quads'] if q['group']==13}
# Authored source texture ids: Equip, Reload, Combine, Remove, Use, Examine, Discard.
textures={0:71,1:34,2:56,3:92,4:79,5:31,6:75}
checks=0
for t,row in enumerate(actions):
 root=units[base[t]];assert root['kind']==1
 for n,action in enumerate(row):
  assert units[base[t]+5+2*n]['texture']==textures[action],(t,n,action)
  checks+=1
 background=units[base[t]+len(row)*2+5]['vertices'][1::2]
 assert max(background)-min(background)==height[t],t
 checks+=1
assert units[0x86]['texture']==94 and units[0x88]['texture']==95
checks+=1
print(f'Fix15 original command artwork PASS: {checks} archive/rule checks')
