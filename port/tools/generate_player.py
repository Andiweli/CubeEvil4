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
    body+='    PlayerBodyCpu(ModelCpu* model);\n    ModelCpu* modelOwner() const { return m_pMod; }\n    void setKnife(bool on);\n    void waistSet(f32);\n    void waistMove();\n    void move();\n};'
    ph=read('include/player.h')
    actorflag=re.search(r'    enum FLAG \{.*?\n    \};',ph,re.S)[0].replace('enum FLAG','enum PlayerActorFlag')
    maho=ph[ph.index('struct cPlMahoWork {'):ph.index('extern cPlMaho* pMaho;')]
    maho=maho.replace('cPlMahoWork','PlayerMahoWork').replace('cPlMaho','PlayerMaho')
    maho=maho.replace('void (*pFunc)();','std::function<void()> pFunc;').replace('void (*func)()','std::function<void()> func')
    maho=maho.replace('work[30];','work[30]{};').replace('u32 nWork;','u32 nWork{};')
    maho=maho.replace('    PlayerMaho();','    PlayerMaho();\n    u32 count() const { return nWork; }')
    waist=ph[ph.index('class cPlWaist {'):ph.index('// Three-way motion blend')].replace('cPlWaist','PlayerWaistCpu')
    waist=waist.replace('Vec m_Ang;','Vec m_Ang{};')
    push=read('include/pl_push.h');push=push[push.index('class cPlPush {'):push.index('    void getWHY(')]
    push=push.replace('cPlPush','PlayerPushCpu').replace('cPlayer*','ModelCpu*').replace('cEmRack*','void*')
    push=re.sub(r';(?=\s*(?://|\n))','{};',push)
    push=re.sub(r'^(    )(?!private|public)(.+?\{\};)',r'\1[[maybe_unused]] \2',push,flags=re.M)
    push+='public:\n    PlayerPushCpu(ModelCpu* pl);\n};'
    base=read('include/cMotBase.h');base=base[base.index('class cMotBase {'):base.index('    void set(')]
    base=base.replace('cMotBase','PlayerMotionBaseCpu').replace('cModel*','ModelCpu*')
    base=re.sub(r';(?=\s*(?://|\n))','{};',base).replace('PlayerMotionBaseCpu(){};','PlayerMotionBaseCpu();')+'};'
    base=re.sub(r'^(    )(?!private|public)(.+?\{\};)',r'\1[[maybe_unused]] \2',base,flags=re.M)
    wep=read('include/pl_wep.h');wep=wep[wep.index('class cPlWep {'):wep.index('    f32 getAngle();')]
    wep=wep.replace('cPlWep','PlayerWeaponCpu').replace('cObjWep*','NativePlayerObject*')
    wep=re.sub(r';(?=\s*(?://|\n))','{};',wep).replace('PlayerWeaponCpu(){};','PlayerWeaponCpu();')+'};'
    esp=read('include/esp.h')
    effects=[re.search(r'enum '+n+r' \{.*?\n\};',esp,re.S)[0] for n in ['ESP_OWNER','ESP_CORE_KIND']]
    types=['#pragma once','#include "re4/port/model_types.h"','#include <stdexcept>',
           '#include <functional>','namespace re4::port {','struct NativePlayerObject;',flag,enum,slot,shape,body,
           actorflag,maho,waist,push,base,wep,*effects,'}']
    (output/'include/re4_player_types.h').write_text('\n'.join(types)+'\n',encoding='utf-8')
    cpp=['#include "player_original.h"','namespace re4::port {']
    entries=[]
    def add(path,sig,changes=(),member=True):
        src=read(path)
        if path.startswith('include/'):
            src=re.sub(r'^    ','',src,flags=re.M)
        text,line=function(src.replace(sig+' {',sig+'\n{'),sig)
        for old,new in changes:
            allowed=text.count(old)==2 and old=='cPlayer::F_NO_WEP_EFF'
            if not text.count(old) or (text.count(old)!=1 and not old.startswith('PL_ARC_PTR') and not allowed): raise ValueError('Player adaptation not unique: '+old)
            text=text.replace(old,new)
        if member:
            name=re.search(r'(\w+)\(',text.split('\n{')[0])[1]
            text=text.replace(name+'(','PlayerDomain::'+name+'(',1)
            text=text.replace('cPlLeon::PlayerDomain::','PlayerDomain::').replace('cPlayer::PlayerDomain::','PlayerDomain::')
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',text])
        entries.append(dict(source=path,signature=sig,line=line,adaptations=
            (['native domain qualification'] if member else [])+[old for old,_ in changes]))
    add('src/game/player.cpp','static inline void U16Set2(u16& a, u16& b, u16 v)',member=False)
    for sig,native in [('cDmgInfo::cDmgInfo()','ActorDamageCpu::ActorDamageCpu()'),
                       ('void cDmgInfo::clear()','void ActorDamageCpu::clear()'),
                       ('cEm::cEm()','ActorModelCpu::ActorModelCpu()'),
                       ('int cEm::initWork()','int ActorModelCpu::initWork()')]:
        add('src/game/em.cpp',sig,[(sig,native)],member=False)
    for sig in ['void cDmgInfo::set(int flag, int timer, u8 wep, Vec* pos, f32 dist, YARARE_INFO* pYarare)',
                'void cDmgInfo::set(int flag, int timer)','void cDmgInfo::move()']:
        add('src/game/em.cpp',sig,[('cDmgInfo::','ActorDamageCpu::'),*([('YARARE_INFO*','DamagePart*')] if 'YARARE_INFO' in sig else [])],member=False)
    add('src/game/pl_wep.cpp','cPlWep::cPlWep()', [('cPlWep::cPlWep()','PlayerWeaponCpu::PlayerWeaponCpu()')],member=False)
    add('src/game/pl_class.cpp','cPlWaist::cPlWaist()', [('cPlWaist::cPlWaist()','PlayerWaistCpu::PlayerWaistCpu()')],member=False)
    add('src/game/pl_class.cpp','f32 cPlWaist::set(f32 dir, f32 rate)', [('cPlWaist::','PlayerWaistCpu::')],member=False)
    add('src/game/cMotBase.cpp','cMotBase::cMotBase()', [('cMotBase::cMotBase()','PlayerMotionBaseCpu::PlayerMotionBaseCpu()')],member=False)
    add('include/pl_push.h','cPlPush(cPlayer* pl)', [('cPlPush(cPlayer* pl)','PlayerPushCpu::PlayerPushCpu(ModelCpu* pl)')],member=False)
    for sig in ['cPlMaho::cPlMaho()','void cPlMaho::reset()','void cPlMaho::regist(const char* code, void (*func)())']:
        changes=[('cPlMaho::','PlayerMaho::')]
        if sig.startswith('cPlMaho::'):changes.append(('PlayerMaho::cPlMaho()','PlayerMaho::PlayerMaho()'))
        if '::regist' in sig:changes += [('void (*func)()','std::function<void()> func'),('cPlMahoWork*','PlayerMahoWork*'),
                                     ('    PlayerMahoWork* e', '    if(nWork>=30) throw std::out_of_range("original maho work capacity");\n    PlayerMahoWork* e')]
        add('src/game/pl_debug.cpp',sig,changes,member=False)
    cheats=['mahoSkelOn','mahoSkelOff','mahoThroughOn','mahoThroughOff','mahoKaiou4','mahoKaiou3','mahoKaiou2','mahoKaiouOff','mahoMuteki','mahoInfBul','mahoCallSc']
    for n in cheats:
        add('src/game/pl_debug.cpp',('static ' if n=='mahoKaiouOff' else '')+'void '+n+'()',
            ([('static void mahoKaiouOff()','void mahoKaiouOff()')] if n=='mahoKaiouOff' else
             [('pSUB->setPos(&pPL->pos);','setPartnerPosition(pSUB, &pPL->pos);')] if n=='mahoCallSc' else []))
    add('src/game/pl_debug.cpp','void cPlayer::debugInit()',
        [('pMaho = new cPlMaho;','mahoOwner = std::make_unique<PlayerMaho>();\n    pMaho = mahoOwner.get();')]+
        [(n+');','[this] { requireConstructionOwner(); '+n+'(); });') for n in cheats])
    add('src/game/player.cpp','cPlayer::cPlayer()',[
        ('cPlayer::cPlayer()','void constructPlayer()'),('pPL = this;','pPL = actorModel.get();'),
        ('hp = pG->pl_life;','actorModel->hp = pG->pl_life;'),
        ('subArc = (PlArc*)0x807EC000;','subArc = const_cast<u8*>(archive.data); // Owned original player archive.'),
        ('PL_MEM_ALLOC(0x1B4, 373)','allocateMotionTable(0x6D, 373)'),('PL_MEM_ALLOC(0x30, 376)','allocateMotionTable(12, 376)'),
        ('memclr_asm(m_MotTbl, 0x1B4);','clearMotionTable(m_MotTbl, 0x6D);'),('memclr_asm(m_MotTbl2, 0x30);','clearMotionTable(m_MotTbl2, 12);')])
    add('src/game/player.cpp','void cPlayer::init0()',[
        ('cPlPush* push;','PlayerPushCpu* push;'),('Wep = new cPlWep;','weaponOwner = std::make_unique<PlayerWeaponCpu>(); Wep = weaponOwner.get();'),
        ('Body = new cPlBody(this);','bodyOwner = std::make_unique<PlayerBodyCpu>(actorModel.get()); Body = bodyOwner.get();'),
        ('push = new cPlPush(this);','pushOwner = std::make_unique<PlayerPushCpu>(actorModel.get()); push = pushOwner.get();'),
        ('Waist = new cPlWaist;','waistOwner = std::make_unique<PlayerWaistCpu>(); Waist = waistOwner.get();'),
        ('MotBase = new cMotBase;','motionBaseOwner = std::make_unique<PlayerMotionBaseCpu>(); MotBase = motionBaseOwner.get();')])
    add('src/game/pl_leon.cpp','cPlLeon::cPlLeon()',[
        ('cPlLeon::cPlLeon()','void constructCharacter()'),('PlArc* arc;','void* arc;'),('arc = pG->pPlayer;','arc = subArc;'),
        ('(u32) PL_ARC_PTR(arc, 0x1A)','arcEntry(arc, 0x1A)'),('pFsdTbl = pl_fs_tbl;','bindFootShadowTable();')])
    add('src/game/pl_wep.cpp','void cPlayer::weaponRelease()', [('cPlayer::F_NO_WEP_EFF','F_NO_WEP_EFF'),('objCur->id','ObjMgr.id(objCur)')])
    for sig in ['void PlayerInit()','void PlayerLifeReset()']:
        add('src/game/player.cpp',sig,[(sig+'\n{',sig+'\n{\n    trace.push_back("'+sig.split('(')[0].split()[-1]+':original");')])
    for sig in ['void ReleaseWepData()','void InitModule(MODULE_DAT* m)']:
        add('src/game/read.cpp',sig,[(sig+'\n{',sig+'\n{\n    trace.push_back("'+sig.split('(')[0].split()[-1]+':original");')])
    for sig in ['void cPlayer::keyConfig()','void cPlayer::keyConfigTypeA()']:
        add('src/game/pl_class.cpp',sig)
    add('src/game/pl_body.cpp','cPlBody::cPlBody(cModel* model)',
        [('cPlBody::cPlBody(cModel* model)','PlayerBodyCpu::PlayerBodyCpu(ModelCpu* model)')],member=False)
    for sig in ['void cPlBody::waistSet(f32 y)','void cPlBody::waistMove()','void cPlBody::move()']:
        add('src/game/pl_body.cpp',sig,[('cPlBody::','PlayerBodyCpu::')],member=False)
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
        scope='Complete PlayerInit reset; preserved cPlayer/cPlLeon constructor bodies on a typed construction domain; publication/startUp require live object, weapon and effect consumers; missing consumers throw.'),indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
