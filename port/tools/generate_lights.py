#!/usr/bin/env python3
"""Source-guarded CPU light manager and original LIT/path consumers."""
import argparse
import json
import re
from pathlib import Path
from generate_runtime import digest, function


def generate(root, output):
    hashes=json.loads((root/'port/config/light-sources.json').read_text())
    for path, expected in hashes.items():
        if digest(root/path)!=expected: raise ValueError('Original light fingerprint mismatch: '+path)
    output.mkdir(parents=True,exist_ok=True); (output/'include').mkdir(exist_ok=True)
    def read(path): return (root/path).read_text(encoding='utf-8')
    header=read('include/light.h')
    def record(name): return re.search(r'(?:struct|class) '+name+r' \{.*?\n\};',header,re.S)[0]
    types=['#pragma once','#include "re4/port/model_types.h"','#include <cstddef>',
           'namespace re4::port {','using GXFogType=u32;']
    for name in ['LIT_TYPE03_FREE','LIT_TYPE04_FREE','LIT_TYPE05_FREE','LightSub','LightPath',
                 'LIT01_MOVE_FREE','LIT02_MOVE_FREE','LIT03_MOVE_FREE','LIT04_MOVE_FREE',
                 'LIT06_MOVE_FREE','LIT07_MOVE_FREE','LIT08_MOVE_FREE','LightPathHeader','FOG','cPenWind']:
        types.append(record(name))
    path=re.search(r'class cLightPath \{.*?\n\};',read('include/lightPath.h'),re.S)[0]
    types.extend([path,record('LIT05_MOVE_FREE')])
    types.extend([re.search(r'struct ConsRoom \{.*?\n\};',read('src/game/cons.cpp'),re.S)[0],
        re.search(r'enum CONS_ROOM_INDEX \{.*?\n\};',read('include/cons.h'),re.S)[0]])
    for name in ['CORE_LIGHT','CORE_LIGHT_PATH']:
        value=re.search(r'^#define '+name+r'\s+(0x[0-9A-Fa-f]+)',read('include/arc/core.h'),re.M)[1]
        types.append(f'constexpr u32 ORIGINAL_{name}_SLOT={value};')
    work=record('cLightWork').replace('cLight&','LightCpu&')
    types.extend(['struct LightCpu;',work,record('cLightEnv'),record('cLit')])
    fields=header.split('class cLight : public cUnit {\npublic:\n',1)[1].split('\n    cLight();',1)[0]
    fields=fields.replace('cModel* pParent','ModelCpu* pParent')
    fields=re.sub(r'    union \{\n        u32 ParentNo;.*?\n    \};','    u32 ParentNo; // Native scalar; high/low halves are accessed explicitly.',fields,flags=re.S)
    fields=fields.replace('    union {\n        u8 work','    union alignas(cLightPath) {\n        u8 work')
    types.extend(['#ifdef _MSC_VER\n#pragma warning(push)\n#pragma warning(disable:4324) // Native pointer-bearing work requires ABI padding.\n#endif',
        'struct LightFields {\n'+fields+'\n};',
        '#ifdef _MSC_VER\n#pragma warning(pop)\n#endif',
        'static_assert(sizeof(cLit)==4 && sizeof(LightPathHeader)==4);',
        'static_assert(sizeof(cLightEnv)==0x104 && sizeof(cLightWork)==0x12C);',
        'static_assert(sizeof(LIT05_MOVE_FREE)<=sizeof(LightSub));',
        '} // namespace re4::port'])
    (output/'include/re4_light_types.h').write_text('\n'.join(types)+'\n',encoding='utf-8')
    cpp=['#include "light_original.h"','namespace re4::port {',
        '#define LightMgr (LightCall::active().mgr)\n#define IN_RANGE(p) LightCall::active().ownsData(p)\n#undef VALID_PTR\n#define VALID_PTR(p) ((p)!=nullptr)\n#define pLog (&LightCall::active().log)']
    cpp.extend(re.findall(r'^#define PI2? .*$',read('include/math_sub_decl.h'),re.M))
    entries=[]
    def add(path,sig,changes=()):
        text=read(path).replace(sig+' {',sig+'\n{')
        body,line=function(text,sig)
        for before,after in changes:
            if body.count(before)!=1: raise ValueError('Light adaptation changed: '+before)
            body=body.replace(before,after)
        body=body.replace('cLightMgr::','LightManagerCpu::').replace('cLight::','LightCpu::')
        body=re.sub(r'\bcLight\b','LightCpu',body)
        body=body.replace('cManager<LightCpu>','LightPool<LightCpu>')
        body=re.sub(r'\bparent\.no\b','u16(ParentNo)',body)
        body=re.sub(r'\bparent\.partsNo\b','u16(ParentNo >> 16)',body)
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',body])
        entries.append(dict(source=path,signature=sig,line=line,adaptations=[a for a,_ in changes]))
    cpp.append(re.search(r'struct Light10Work \{.*?\n\};',read('src/game/light10.cpp'),re.S)[0])
    add('src/lib/vec.c','void C_VECAdd(const Vec* a, const Vec* b, Vec* ab)')
    cpp.append(re.search(r'static u32 ConsRoomDefault\[12\] = \{.*?\n\};',read('src/game/cons.cpp'),re.S)[0])
    for sig in ['int ConsInitRoom(ConsRoom* p)','u32 ConsGetRoomValue(u32 id)']:
        add('src/game/cons.cpp',sig,[(sig,sig.replace('ConsInitRoom','LightConstants::ConsInitRoom').replace('ConsGetRoomValue','LightConstants::ConsGetRoomValue'))])
    angle,_=function(read('src/game/math_sub.cpp'),'f32 LIMIT_ANGLE(f32 x)')
    assembly=re.search(r'    asm\(.*?: "cr0"\);',angle,re.S)[0]
    add('src/game/math_sub.cpp','f32 LIMIT_ANGLE(f32 x)',[(assembly,
        '    // Native equivalent of the original fsubs/fadds loop, with f32 rounding.\n    if (x >= max) { do { x = x - step; } while (x >= max); }\n    else { while (x < min) { x = x + step; } }')])
    manager=read('include/cManager.h')
    for sig in ['int cManager<T>::roomInit()','void cManager<T>::init(void (**tbl)(T*))',
                'int cManager<T>::arrayFree()','int cManager<T>::arrayAlloc(u32 n)',
                'u32 cManager<T>::countActiveWork()','T* cManager<T>::create(int id)',
                'T* cManager<T>::createBack(int id)','void cManager<T>::destroyAll()',
                'int cManager<T>::dieCheck()','inline void cManager<T>::destroy(T* p)']:
        changes=[('cManager<T>::','LightPool<T>::')]
        body,_=function(manager,sig)
        if 'delete p;' in body: changes.append(('delete p;','p->retire(); // Preserve pool-object lifetime; original destructor clears 0x601.'))
        if 'if ((u32)p < 0x80000000 || (u32)p > 0x82FFFFFF)' in body:
            changes.append(('if ((u32)p < 0x80000000 || (u32)p > 0x82FFFFFF)','if (!owns(p))'))
        cpp.append('template<class T>')
        add('include/cManager.h',sig,changes)
    # Original in-class linked-list bodies, with native declarations unchanged.
    for sig in ['int deleteList(T* p)','void addListFront(T* p)','void addListBack(T* p)']:
        text=re.sub(r'^    ','',manager,flags=re.M).replace(sig+' {',sig+'\n{')
        body,line=function(text,sig)
        cpp.append('template<class T>\n'+body.replace(sig,sig.split(' ',1)[0]+' LightPool<T>::'+sig.split(' ',1)[1]))
        entries.append(dict(source='include/cManager.h',signature=sig,line=line,adaptations=['native template qualification']))
    for sig in ['void cLightMgr::init(void (**funcTbl)(cLight*))',
                'int cLightMgr::roomInit(cLit* core, cLit* room, cLit* third)',
                'cLight* cLightMgr::create(cLightWork* pLw)',
                'cLight* cLightMgr::createBack(cLightWork* pLw)',
                'f32 cLightMgr::setElecPower(f32 d)','int cLightMgr::onKind(u8 kind)',
                'int cLightMgr::offKind(u8 kind)','int cLightMgr::checkKind(u8 kind)',
                'cLight* cLightMgr::getKindLight(u8 kind)','int cLightMgr::roomLitSet(cLit* lit)',
                'int cLightMgr::roomLitCheck()','cLightEnv* cLightMgr::getEnvPtr()',
                'int cLightMgr::initPath(LightPathHeader* p)','u8* cLightMgr::getPathPtr(u8 id)',
                'LightPathHeader* cLightMgr::getPathHeader()',
                'void cLightMgr::beginEvent()','void cLightMgr::endEvent()',
                'void cLightMgr::setTune(cLightEnv* pLe)',
                'void cLightMgr::setFogStart(f32 datStart)','void cLightMgr::setFogEnd(f32 datEnd)',
                'f32 cLightMgr::getFogStart()','f32 cLightMgr::getFogEnd()',
                'void cLightMgr::deleteScr()','void cLightMgr::offScr(u8 enable)','int cLightMgr::countScr()',
                'cLightEnv* cLightMgr::getCutAddr(int type, int cutNo)']:
        changes=[]
        if sig.startswith('u8* cLightMgr::getPathPtr'):
            changes=[('*(u32*) (id * 4 + (u32) pLitPath + 4)','readNativePathOffset(pLitPath,id)')]
        if 'Kind(' in sig: changes=[('1 << (kind & 31)','1u << (kind & 31)')]
        add('src/game/light.cpp',sig,changes)
    add('src/game/light.cpp','cLight::cLight()',[
        ('cLight::cLight()','LightCpu::LightCpu():ModelUnit{},LightFields{}'),
        ('\n{','\n{\n    spot={};sub={}; // Native value initialization of the full anonymous union storage.')])
    for sig in ['cLightEnv* cLit::getCut(u16 no)','int cLit::getSafeCutNo(int cutNo)',
                'int cLit::versionUp()','u32 cLit::getMaxLight()',
                'cLightWork* cLightEnv::getLightWork(int no)','u32 cLightEnv::getSize()',
                'void Light00_Move(cLight* pLi)','void cLight::move()',
                'cLight& cLight::operator=(cLightWork& w)','cLightWork& cLightWork::operator=(cLight& l)',
                'int cLight::checkScr()','void cLight::setPartsNo(int pno)',
                'int cLight::setParent(u8 type, u32 no)','cModel* cLight::calcParent()',
                'void cLight::setTrans(int on_off)',
                'void cLight::setSpotNormal(Vec* norm)','void cLight::setSpotTarget(Vec* pos)']:
        changes=[]
        if 'cLight::operator=' in sig:
            changes=[('    return *this;','    prepareNativeWork(w); // Expand only pointer-bearing path work; wire bytes remain immutable.\n    return *this;')]
        if 'calcParent()' in sig:
            changes=[('cModel* cLight::calcParent()','ModelCpu* cLight::calcParent()'),
                     ('EmMgr.getEmPtr((u8) parent.no, 0)','LightCall::active().resolve(1,u8(ParentNo))'),
                     ('SmdGetGroupObjPtr(parent.no)','LightCall::active().resolve(2,u16(ParentNo))'),
                     ('getRoomEtcOnLight(parent.no, &pParent, 0)','resolveEtc(u16(ParentNo), &pParent)'),
                     ('(cModel*) ObjMgr.fastAt(parent.no)','LightCall::active().resolve(4,u16(ParentNo))')]
        add('src/game/light.cpp',sig,changes)
    for i in [1,2,3,4,5,6,7,8,10]:
        sig=f'void Light{i:02d}_Move(cLight* '+('pLight' if i==10 else 'pLi')+')'
        changes=[]
        if i==8: changes=[('cEm* em = EmMgr.getEmPtr(w->id, 0);','ModelCpu* em = LightCall::active().enemy(w->id);'),
                          ('cParts* parts = em->getPartsPtr(w->pno);','ModelPart* parts = em->getPartsPtr(w->pno);')]
        if i==10: changes=[('delete pLight;','pLight->retire();')]
        add(f'src/game/light{i:02d}.cpp',sig,changes)
    for sig in ['int cLightPath::setPath(u8* pPath, u8 flag)','int cLightPath::movePath()']:
        add('src/game/lightPath.cpp',sig)
    table=re.search(r'void \(\*LightFuncTbl\[17\]\)\(cLight\*\) = \{.*?\n\};',read('src/game/game.cpp'),re.S)[0]
    cpp.extend([table.replace('cLight*','LightCpu*'),'template struct LightPool<LightCpu>;','} // namespace re4::port'])
    (output/'light_control.cpp').write_text('\n\n'.join(cpp)+'\n',encoding='utf-8')
    (output/'light-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries,
        adapters=['checked immutable BE LIT/path -> native copies','native pool lifetime/stride','explicit model-owner resolver; missing dependencies throw'],
        excluded=['GPU fog/geometry','setEnv/filter/wind/view side effects','lightMove collision/model death','light areas SAR gameplay']),indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
