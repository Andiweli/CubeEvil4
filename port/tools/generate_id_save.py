#!/usr/bin/env python3
"""Source-guarded original ID pool, room state and save-image address management."""
import argparse
import json
import re
from pathlib import Path
from generate_runtime import digest, function


def generate(root, output):
    hashes=json.loads((root/'port/config/id-save-sources.json').read_text())
    for path, expected in hashes.items():
        if digest(root/path)!=expected:
            raise ValueError('Original ID/save fingerprint mismatch: '+path)
    output.mkdir(parents=True,exist_ok=True)
    (output/'include').mkdir(exist_ok=True)
    def read(path): return (root/path).read_text(encoding='utf-8')
    def struct(path,name):
        return re.search(r'struct '+name+r' \{.*?\n\};',read(path),re.S)[0]
    types=['#pragma once','#include "re4/port/model_types.h"','#include "re4_runtime_types.h"','namespace re4::port {',
           'struct HERMITE_1_PTR;',struct('include/id_sys.h','ID_UNIT'),
           re.search(r'enum ID_CLASS \{.*?\n\};',read('include/id_sys.h'),re.S)[0],
           struct('include/room_data.h','ROOM_DATA'),struct('include/room_data.h','StageTbl'),
           struct('include/room_data.h','RoomSaveHdr')]
    room=struct('include/room_data.h','ROOM_SAVE_DATA')
    room=re.sub(r'    union \{.*?\n    \};','    u16 RoomNo; // Native scalar stage<<8|room, not a little-endian union.',room,flags=re.S)
    types.extend([room,struct('include/sscrn.h','SSCRN_SAVE_DATA'),
                  struct('include/game.h','SAVE_DATA_HEAD')])
    for name in ['ItemSaveWork','_ITEM_SAVE_DATA']:
        types.append(struct('include/item.h',name))
    for name in ['StockEntry','LevelEntry','STOCK_INFO','LEVEL_INFO','MERCHANT_DATA']:
        types.append(struct('include/merchant.h',name))
    capacity=re.search(r'IdSys.gameInit\((0x[0-9A-Fa-f]+)\);',read('src/game/main.cpp'))[1]
    types.extend([f'constexpr int ORIGINAL_MAIN_ID_CAPACITY={capacity};',
                  'static_assert(sizeof(ROOM_SAVE_DATA)==0xD8 && sizeof(RoomSaveHdr)==0x10);',
                  'static_assert(sizeof(GameSaveBlock)==0x36F8 && sizeof(_ITEM_SAVE_DATA)==0x1204);',
                  'static_assert(sizeof(SAVE_DATA_HEAD)<=0x40, "original first save section");',
                  '} // namespace re4::port'])
    (output/'include/re4_id_save_types.h').write_text('\n'.join(types)+'\n',encoding='utf-8')
    cpp=['#include "id_save_original.h"','namespace re4::port {',
         '#define MEM_ALLOC(n,f,h) allocatePool(n,f,h)\n#define MEM_CALLOC(n,f,h) mem_calloc(n,f,h)\n#define ID_UNIT(i) (m_IdUnit+(i))\n#define ALIGN32(n) (((n)+31u)&~31u)\n#define MERCHANT_NUM 1\n#define VEC_COPY(d,s) ((d)=(s))',
         'static inline int IdBitChk(u32 *tbl,u8 n) { return (tbl[n/32] & (0x80000000u>>(n%32))) ? 1 : 0; }',
         '#define FlagOffVar(p,n) ((p)[(n)/32] &= ~(0x80000000u>>((n)%32)))\n#define FlagOnVar(p,n) ((p)[(n)/32] |= (0x80000000u>>((n)%32)))']
    entries=[]
    def add(path,sig,changes=(),trace=False):
        src=read(path)
        if path.startswith('include/'): src=re.sub(r'^    ','',src,flags=re.M)
        src=src.replace(sig+' {',sig+'\n{')
        body,line=function(src,sig)
        changes=list(changes)
        if trace: changes.append(('\n{','\n{\n    trace.push_back('+json.dumps(sig)+');'))
        for before,after in changes:
            expected=5 if before=='(u32) head + p' else 1
            if body.count(before)!=expected: raise ValueError('ID/save adaptation changed: '+before)
            body=body.replace(before,after)
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',body])
        entries.append(dict(source=path,signature=sig,line=line,adaptations=[a for a,_ in changes]))
    for sig in ['void IDSystem::gameInit(int maxId)','void IDSystem::roomInit()',
                'void IDSystem::free()','int IDSystem::setCk(ID_CLASS classNo)',
                'void IDSystem::dispSw(ID_CLASS classNo, int sw)','void IDSystem::unitPush(ID_UNIT* pIdUnit)',
                'ID_UNIT* IDSystem::unitPull()','void IDSystem::unitLevel(ID_UNIT* u, u8 level)',
                'void IDSystem::unitParent(ID_UNIT* pParent, ID_UNIT* pChild)',
                'ID_UNIT* IDSystem::unitPtr(u8 markNo, ID_CLASS classNo)',
                'void IDSystem::kill(u8 markNo, ID_CLASS classNo)','void IDSystem::stop()',
                'void IDSystem::beMove(ID_UNIT* pParent, int on_off)',
                'void IDSystem::setTime(ID_UNIT* pParent, s16 time)']:
        changes=[(sig,sig.replace('IDSystem::','IdSystemCpu::'))]
        if sig=='void IDSystem::roomInit()':
            changes.append(('memclr_asm(&m_IdUnit[i], sizeof(ID_UNIT));','clearUnit(&m_IdUnit[i]);'))
        if 'unitPull' in sig:
            changes.append(('memclr_asm(u, sizeof(ID_UNIT));','clearUnit(u);'))
        add('src/game/id_sys.cpp',sig,changes,trace=sig in ['void IDSystem::gameInit(int maxId)','void IDSystem::roomInit()','void IDSystem::free()'])
    add('src/game/cockpit.cpp','void Cockpit::gameInit()',
        [('void Cockpit::gameInit()','void IdSystemCpu::cockpitGameInit()'),('IdSys.roomInit();','roomInit();')],trace=True)
    source=read('src/game/roomdata.cpp')
    for name in ['St0_data_tbl','St1_data_tbl','St2_data_tbl','St3_data_tbl','St4_data_tbl']:
        cpp.append(re.search(r'ROOM_DATA '+name+r'\[\d+\] = \{.*?\n\};',source,re.S)[0])
    cpp.append(re.search(r'static StageTbl Room_data_tbl\[10\] = \{.*?\n\};',source,re.S)[0])
    for sig in ['void cRoomData::init()','void cRoomData::save(void* p)','void cRoomData::load(void* p)',
                'void cRoomData::clear(void* p)','u8* cRoomData::getRoomSavePtr(u16 room_no)',
                'int cRoomData::checkRoomRange(u8 stage, u8 room)',
                'int cRoomData::checkPassed(u16 room_no, int part_no)',
                'void cRoomData::setPassed(u16 room_no, int part_no)']:
        body,_=function(source,sig)
        changes=[(sig,sig.replace('cRoomData::','RoomSaveCpu::'))]
        old='(ROOM_SAVE_DATA*) (i * sizeof(ROOM_SAVE_DATA) + (u32) m_pRoomSaveData)'
        if sig=='void cRoomData::load(void* p)':
            changes.append((old,'reinterpret_cast<ROOM_SAVE_DATA*>(m_pRoomSaveData) + i'))
        if sig=='void cRoomData::init()':
            changes.append(('                *(u8*) (ofs + (u32) m_pRoomSaveData) = stage;\n                rec = (u8*) (ofs + (u32) m_pRoomSaveData);\n                rec[1] = i;',
                            '                rec = m_pRoomSaveData + ofs;\n                reinterpret_cast<ROOM_SAVE_DATA*>(rec)->RoomNo = u16((stage << 8) | u32(i)); // Original BE stage/room union as native scalar.'))
        if sig=='void cRoomData::clear(void* p)':
            changes.append(('dst = (ROOM_SAVE_DATA*) (i * sizeof(ROOM_SAVE_DATA) + (u32) m_pRoomSaveData);','dst = reinterpret_cast<ROOM_SAVE_DATA*>(m_pRoomSaveData) + i;'))
            changes.append(('((ROOM_SAVE_DATA*) (i * sizeof(ROOM_SAVE_DATA) + (u32) m_pRoomSaveData))->RoomNo = id;','(reinterpret_cast<ROOM_SAVE_DATA*>(m_pRoomSaveData) + i)->RoomNo = id;'))
        add('src/game/roomdata.cpp',sig,changes,trace=sig=='void cRoomData::init()')
    add('include/room_data.h','u32 getSaveDataSize()', [('u32 getSaveDataSize()','u32 RoomSaveCpu::getSaveDataSize()')])
    for sig in ['void cGameSave::checkAddr(SAVE_DATA_HEAD* head)',
                'void cGameSave::calcOffset(SAVE_DATA_HEAD* head, u32 headaddr)',
                'void cGameSave::calcAddr(SAVE_DATA_HEAD* head)','SAVE_DATA_HEAD* cGameSave::alloc()']:
        new=sig.replace('cGameSave::','SaveImageCpu::').replace('u32 headaddr','std::uintptr_t headaddr')
        changes=[(sig,new)]
        body,_=function(read('src/game/game.cpp'),sig)
        for old,newtext in [('u32 p;','std::uintptr_t p;'),('(u32) base','(std::uintptr_t) base'),
                            ('(u32) head;','(std::uintptr_t) head;'),('(u32) head + p','(std::uintptr_t) head + p')]:
            if old in body: changes.append((old,newtext))
        for name in ['pGlobal','pRm','pSs','pMr','pItm']:
            if '(u32) head->'+name in body: changes.append(('(u32) head->'+name,'(std::uintptr_t) head->'+name))
        if sig=='SAVE_DATA_HEAD* cGameSave::alloc()':
            for scalar,cast in [('globalOfs','GameSaveBlock*'),('roomOfs','void*'),('sscrnOfs','SSCRN_SAVE_DATA*'),('merchantOfs','void*'),('itemOfs','void*')]:
                changes.append(('('+cast+') '+scalar,'('+cast+') (std::uintptr_t) '+scalar))
        add('src/game/game.cpp',sig,changes,trace=sig=='SAVE_DATA_HEAD* cGameSave::alloc()')
    add('src/game/sscrn.cpp','int SscrnDataSize()', [('int SscrnDataSize()','int SaveImageCpu::SscrnDataSize()')])
    add('src/game/merchant.cpp','int MerchantDataSize()', [('int MerchantDataSize()','int SaveImageCpu::MerchantDataSize()')])
    add('src/game/item.cpp','int cItemMgr::saveDataSize()', [('int cItemMgr::saveDataSize()','int SaveImageCpu::ItemSize::saveDataSize()')])
    add('src/game/game.cpp','bool cGameSave::save(SAVE_DATA_HEAD* data, int mode)', [
        ('bool cGameSave::save(SAVE_DATA_HEAD* data, int mode)','bool SaveImageCpu::save(SAVE_DATA_HEAD* data, int mode)'),
        ('*data->pGlobal = *(GameSaveBlock*) pG->save_data_start_addr;',
         '*data->pGlobal = static_cast<const GameSaveBlock&>(*pG);')], trace=True)
    add('src/game/game.cpp','bool cGameSave::load(SAVE_DATA_HEAD* head)', [
        ('bool cGameSave::load(SAVE_DATA_HEAD* head)','bool SaveImageCpu::load(SAVE_DATA_HEAD* head)'),
        ('u32* save = (u32*) &pG->save_data_start_addr;', 'auto *save = static_cast<GameSaveBlock *>(pG);')], trace=True)
    cpp.append('} // namespace re4::port')
    (output/'id_save_control.cpp').write_text('\n\n'.join(cpp)+'\n',encoding='utf-8')
    (output/'id-save-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries),indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True); p.add_argument('--output',type=Path,required=True)
    a=p.parse_args(); generate(a.root.resolve(),a.output.resolve())
