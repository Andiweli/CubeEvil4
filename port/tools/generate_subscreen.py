#!/usr/bin/env python3
"""Whole original SubScreen/inventory boot bodies; no disc bytes embedded."""
import argparse
import json
import re
from pathlib import Path
from generate_runtime import digest, function


def generate(root, output):
    hashes = json.loads((root/'port/config/subscreen-sources.json').read_text())
    for path, expected in hashes.items():
        if digest(root/path) != expected:
            raise ValueError('Original SubScreen fingerprint mismatch: '+path)
    output.mkdir(parents=True, exist_ok=True)
    (output/'include').mkdir(exist_ok=True)
    item = (root/'include/item.h').read_text()
    types = ['#pragma once', '#include "types.h"',
             '#ifdef _MSC_VER\n#pragma warning(push)\n#pragma warning(disable:4244 4458) // Original narrow item bit setters and parameter name.\n#endif',
             'namespace re4::port {', 'typedef u16 ITEM_ID;']
    for name in ['cItem', 'ITEM_ORDER', 'ITEM_INFO']:
        types.append(re.search(r'struct '+name+r' \{.*?\n\};', item, re.S)[0])
    ss = (root/'include/sscrn.h').read_text()
    for name in ['SS_OPEN_FLAG', 'SS_ATTR_FLAG']:
        types.append(re.search(r'enum '+name+r' \{.*?\n\};', ss, re.S)[0])
    types += ['static_assert(sizeof(cItem)==14, "original pointer-free inventory slot");',
              '} // namespace re4::port', '#ifdef _MSC_VER\n#pragma warning(pop)\n#endif']
    (output/'include/re4_subscreen_types.h').write_text('\n'.join(types)+'\n')
    cpp = ['#include "subscreen_original.h"', 'namespace re4::port {',
           '#define MEM_ALLOC(n,f,h) mem_alloc(n,f,h)\n#define DVD_READ_N(n,d,a,b,c,m) DvdReadN(n,d,a,b,c,m)\n#define SS_ARAM 0xD00000\n#define SS_ARAM_SIZE 0x300000\n#define BULLET(p) ((p)->bullet & 0x1FFF)']
    entries = []
    def add(path, sig, changes=()):
        src = (root/path).read_text()
        if path.startswith('include/'):
            src = re.sub(r'^    ', '', src, flags=re.M)
        src = src.replace(sig+' {', sig+'\n{')
        body, line = function(src, sig)
        for before, after in changes:
            if body.count(before) != 1:
                raise ValueError('SubScreen adaptation changed: '+before)
            body = body.replace(before, after)
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"', body])
        entries.append(dict(source=path, signature=sig, line=line, adaptations=[a for a,_ in changes]))
    for sig in ['void SubScreenAramRead()', 'void sscrnSetLanguage(SubScreenWork* pSscrn, int language)',
                'void sscrnDataFilename(SubScreenWork* pSscrn, const char* name)',
                'void SubScreenGameInit()', 'void SubScreenRoomInit()', 'void SubScreenWait(int frame)',
                'void SscrnDataSave(SSCRN_SAVE_DATA* dst)', 'void SscrnDataLoad(SSCRN_SAVE_DATA* pData)']:
        out = sig.replace('void ', 'void SubScreenContext::', 1)
        changes = [(sig, out)]
        if 'AramRead' in sig:
            changes += [('(void**) &wk->p_module', '&wk->p_module')]
        if sig=='void SubScreenGameInit()':
            changes += [('    SubScreenRoomInit();', '    SubScreenRoomInit();\n    trace.push_back("SubScreenGameInit:original-complete");')]
        add('src/game/sscrn.cpp', sig, changes)
    sig='int cManager<T>::roomInit()'
    add('include/cManager.h', sig, [(sig, 'int MapManagerCpu::roomInit()')])
    for sig in ['int cItemMgr::init()', 'void cItemMgr::clear()', 'void cItemMgr::flagclear()',
                'void cItemMgr::roomInit()', 'cItem* cItemMgr::search(u16 id)',
                'cItem* cItemMgr::at(int i)', 'int cItemMgr::searchAt(cItem* p)',
                'void cItemMgr::construct(cItem* p, ITEM_ID id)', 'int cItemMgr::get(ITEM_ID id, int num)',
                'int cItemMgr::countFiles()', 'void cItemMgr::save(void* pData)', 'void cItemMgr::load(void* pData)',
                'void cItemMgr::gameInit()', 'int cItemMgr::set_game(int trial_flag)',
                'int cItemMgr::arm(cItem* p)', 'u16 cItemMgr::weaponId(cItem* p)',
                'int cItemMgr::dump(ITEM_ID id)', 'int cItemMgr::dump(cItem* p)', 'void cItemMgr::erase(cItem* p)',
                'u16 cItemMgr::num(int id, u8 type)']:
        changes=[(sig, sig.replace('cItemMgr::', 'InventoryCpu::'))]
        if 'construct' in sig:
            changes.append(('asm volatile("");', '/* Codeless original PPC compiler anchor. */'))
        if sig=='int cItemMgr::init()':
            changes.extend([('m_pItem = (cItem*) MEM_ALLOC(0x180 * sizeof(cItem), 1, 13);','m_pItem = allocateItems(0x180); // Start native object lifetimes in the original heap allocation.'),
                            ('m_p_order_tbl = (ITEM_ORDER*) MEM_ALLOC(m_array_num * sizeof(ITEM_ORDER), 1, 13);','m_p_order_tbl = allocateOrders(m_array_num); // Native pointer stride, original entry count.')])
        if 'at(' in sig:
            changes.append(('if (i < m_array_num)', 'if (i >= 0 && i < m_array_num)'))
        if 'countFiles' in sig:
            changes.extend([('ItemMgr.m_array_num', 'm_array_num'), ('ItemMgr.at(i)', 'at(i)')])
        if sig=='void cItemMgr::save(void* pData)':
            changes += [('register int m1 asm("r0");','int m1;')]
        add('src/game/item.cpp', sig, changes)
    sig='void itemInfo(ITEM_ID id, ITEM_INFO* info)'
    add('src/game/item.cpp', sig)
    for sig in ['inline u8 itemType(ITEM_ID id)', 'inline u8 itemDefNum(ITEM_ID id)', 'inline u16 itemMaxNum(ITEM_ID id)']:
        add('include/item.h', sig, [(sig, sig.replace('inline ', ''))])
    # Original weapon table is source code, not copied assets.
    src=(root/'src/game/item.cpp').read_text()
    cpp.insert(2, re.search(r'struct ITEM_WEAPON_INFO \{.*?\n\};', src, re.S)[0])
    cpp.insert(3, re.search(r'const ITEM_WEAPON_INFO wep_info\[\] = \{.*?\n\};', src, re.S)[0])
    cpp.insert(4, re.search(r'struct ITEM_COMBINATION \{.*?\n\};', src, re.S)[0])
    cpp.insert(5, re.search(r'const ITEM_COMBINATION combination_info\[\] = \{.*?\n\};', src, re.S)[0])
    cpp.insert(6, 'u16 bareHand();\nint itemCombine(ITEM_ID,ITEM_ID,u16*);')
    add('src/game/item.cpp','u16 bareHand()')
    add('src/game/item.cpp','int itemCombine(ITEM_ID srcA, ITEM_ID srcB, u16* dst)')
    add('src/game/pl_sub.cpp','int PlSetCostume()', [('int PlSetCostume()', 'int SubScreenContext::PlSetCostume()')])
    add('src/game/item.cpp', 'static inline void setBullet(cItem* p, u16 n)', [('static inline ', '')])
    sig='u16 WeaponId2ChargeNum(ITEM_ID id, int level)'
    add('src/game/item.cpp', sig, [(sig, 'u16 InventoryCpu::WeaponId2ChargeNum(ITEM_ID id, int level)')])
    sig='int addMoney(int n)'
    add('src/game/item.cpp', sig, [(sig, 'int InventoryCpu::addMoney(int n)')])
    add('src/Sscrn/ss_map.cpp','void mapAreaFilename(int area, char* name)')
    cpp.append('} // namespace re4::port')
    (output/'subscreen_control.cpp').write_text('\n\n'.join(cpp)+'\n')
    (output/'subscreen-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries),indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True); p.add_argument('--output',type=Path,required=True)
    a=p.parse_args(); generate(a.root.resolve(),a.output.resolve())
