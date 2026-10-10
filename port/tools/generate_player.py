#!/usr/bin/env python3
"""Reviewed whole original player reset and Leon CPU asset helpers, no actor substitute."""
import argparse
import json
import re
from pathlib import Path
from generate_runtime import digest, function


def generate(root, output):
    hashes=json.loads((root/'port/config/player-sources.json').read_text())
    for path, expected in hashes.items():
        if digest(root/path)!=expected: raise ValueError('Original player fingerprint mismatch: '+path)
    output.mkdir(parents=True,exist_ok=True); (output/'include').mkdir(exist_ok=True)
    def read(path): return (root/path).read_text(encoding='utf-8')
    flag=read('include/cFlag.h').split('template <class T, class E>')[1].split('\n#endif')[0]
    flag='template <class T, class E>'+flag
    flag=flag.replace('cFlag','PlayerFlag').replace('pLog->err(0, 0, "cFlag.set() arg stat OVER FLOW %d", stat);',
        'throw std::out_of_range("original module flag index");')
    # The diagnostic string was renamed too. Valid MODULE_CTRL indices remain 0..3.
    flag=flag.replace('pLog->err(0, 0, "PlayerFlag.set() arg stat OVER FLOW %d", stat);',
        'throw std::out_of_range("original module flag index");')
    rh=read('include/read.h')
    enum=re.search(r'enum MODULE_CTRL \{.*?\n\};',rh,re.S)[0]
    slot=re.search(r'struct MODULE_DAT \{.*?\n\};',rh,re.S)[0]
    slot=slot.replace('MODULE_DAT','NativePlayerModule').replace('cFlag<','PlayerFlag<').replace('OSModuleHeader*','void*')
    slot=re.sub(r';(?=\s*(?://|\n))','{};',slot)
    # A native constructor is not a declaration/field.
    slot=slot.replace('ctrl_flag.reset(){};', 'ctrl_flag.reset();')
    bh=read('include/pl_body.h')
    shape=bh[bh.index('typedef struct tagSHAPE_MOT_HEADER'):bh.index('// Player body helper')]
    body=bh[bh.index('class cPlBody {'):bh.index('    cPlBody(cModel* model);')]
    body=body.replace('cPlBody','PlayerBodyCpu').replace('cModelInfo','ModelInfoView').replace('cModel*','ModelCpu*')
    body=body.replace('u32 nowLhandNo','std::uintptr_t nowLhandNo').replace('u32 oldLhandNo','std::uintptr_t oldLhandNo')
    body=re.sub(r';(?=\s*(?://|\n))','{};',body)
    body+='    PlayerBodyCpu(ModelCpu* model);\n    void setKnife(bool on);\n};'
    types=['#pragma once','#include "re4/port/model_types.h"','#include <stdexcept>',
           'namespace re4::port {',flag,enum,slot,shape,body,'}']
    (output/'include/re4_player_types.h').write_text('\n'.join(types)+'\n',encoding='utf-8')
    cpp=['#include "player_original.h"','namespace re4::port {']
    entries=[]
    def add(path,sig,changes=(),member=True):
        src=read(path)
        if path.startswith('include/'):
            src=re.sub(r'^    ','',src,flags=re.M)
        text,line=function(src,sig)
        for old,new in changes:
            if not text.count(old) or (text.count(old)!=1 and not old.startswith('PL_ARC_PTR')): raise ValueError('Player adaptation not unique: '+old)
            text=text.replace(old,new)
        if member:
            name=re.search(r'(\w+)\(',sig)[1]
            text=text.replace(name+'(','PlayerDomain::'+name+'(',1)
            text=text.replace('cPlLeon::PlayerDomain::','PlayerDomain::').replace('cPlayer::PlayerDomain::','PlayerDomain::')
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',text])
        entries.append(dict(source=path,signature=sig,line=line,adaptations=
            (['native domain qualification'] if member else [])+[old for old,_ in changes]))
    add('src/game/player.cpp','static inline void U16Set2(u16& a, u16& b, u16 v)',member=False)
    for sig in ['void PlayerInit()','void PlayerLifeReset()']:
        add('src/game/player.cpp',sig,[(sig+'\n{',sig+'\n{\n    trace.push_back("'+sig.split('(')[0].split()[-1]+':original");')])
    for sig in ['void ReleaseWepData()','void InitModule(MODULE_DAT* m)']:
        add('src/game/read.cpp',sig,[(sig+'\n{',sig+'\n{\n    trace.push_back("'+sig.split('(')[0].split()[-1]+':original");')])
    for sig in ['void cPlayer::keyConfig()','void cPlayer::keyConfigTypeA()']:
        add('src/game/pl_class.cpp',sig)
    add('src/game/pl_body.cpp','cPlBody::cPlBody(cModel* model)',
        [('cPlBody::cPlBody(cModel* model)','PlayerBodyCpu::PlayerBodyCpu(ModelCpu* model)')],member=False)
    add('include/pl_body.h','void setKnife(bool on)',
        [('void setKnife(bool on)','void PlayerBodyCpu::setKnife(bool on)'),('cModelInfo* knife','ModelInfoView* knife')],member=False)
    add('src/game/shape.cpp','void ShapeEnd(void* pModelInfo)',member=False)
    add('src/game/shape.cpp','void SetOriginalShape(cModelInfo* pModelInfo)',member=False)
    for sig in ['void cPlLeon::setModel()','void cPlLeon::setMotion()','void cPlLeon::setWound()',
                'void cPlLeon::setRightHand(int type)','void cPlLeon::setLeftHand(u32 type)','void cPlLeon::setFace(int type)']:
        changes=[]
        text,_=function(read('src/game/pl_leon.cpp'),sig)
        for old in sorted(set(re.findall(r'PL_ARC_PTR\(pG->pPlayer, [^)]*\)',text))):
            # Same original slot and reference. A bounds-checked immutable native lease replaces GC relocation.
            changes.append((old,'arc('+old.split(', ')[1][:-1]+')'))
        if 'setMotion' in sig:
            for old in re.findall(r'PLA_MOT\(this, [^)]*\)',text):
                idx,slotno=old.split(', ')[1:]
                changes.append((old,'m_MotTbl['+idx+'] = arc('+slotno[:-1]+')'))
        if 'setRightHand' in sig: changes.append(('int type)','std::intptr_t type)'))
        if 'setLeftHand' in sig: changes.append(('u32 type)','std::uintptr_t type)'))
        add('src/game/pl_leon.cpp',sig,changes)
    cpp.append('}')
    (output/'player_control.cpp').write_text('\n\n'.join(cpp)+'\n',encoding='utf-8')
    (output/'player-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries,
        layouts='Named native fields; no GC MODULE_DAT, cPlayer or cEm memory overlay. Hand selectors widened to uintptr_t.',
        scope='Complete PlayerInit reset; original Leon CPU model/motion helpers, not cPlayer/cPlLeon construction.'),indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
