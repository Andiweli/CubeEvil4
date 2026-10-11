"""Guarded original cObjMgr/cManager lifecycle and native typed work views."""
import argparse
import json
import re
from pathlib import Path
from generate_runtime import digest, function


def generate(root, output):
    hashes=json.loads((root/'port/config/object-sources.json').read_text())
    for path,expected in hashes.items():
        if digest(root/path)!=expected: raise ValueError('Original object fingerprint mismatch: '+path)
    output.mkdir(parents=True,exist_ok=True);(output/'include').mkdir(exist_ok=True)
    def read(path):return (root/path).read_text(encoding='utf-8')
    def record(path,name,native):
        text=re.search(r'struct '+name+r' \{.*?\n\};',read(path),re.S)[0]
        return text.replace(name,native).replace('cEffectCore','NativeEffectCore').replace('cModel*','ModelCpu*').replace('cCoord*','ModelCoord*')
    types=['#pragma once','#include "re4/port/model_types.h"','namespace re4::port {','struct cEspSeqTbl;']
    core=record('include/esp.h','cEffectCore','NativeEffectCore')
    main_destructors=re.search(r'EspgenDestructFunc EspgenDestructTbl\[ESPGEN_APP_ID\] = \{(.*?)\};',read('src/game/espgen.cpp'),re.S)[1]
    app_destructors=re.search(r'static EspgenDestructFunc EspgenDestructTblApp\[6\] = \{(.*?)\};',read('src/game/espgen.cpp'),re.S)[1]
    if main_destructors.split()!=['NULL,'] or not re.match(r'\s*NULL,\s*NULL,',app_destructors):
        raise ValueError('Original generator NULL destructor coverage changed')
    # C++ rejects an unnamed type declaration inside an anonymous union.
    # Name the same four byte fields outside the union; no packing/GC overlay.
    byte_struct=re.search(r'        struct \{(.*?)\n        \} b;',core,re.S)
    if not byte_struct:raise ValueError('Original effect call-byte union changed')
    core=core.replace(byte_struct[0],'        CallBytes b;').replace('    union {','    struct CallBytes {'+byte_struct[1]+'\n    };\n    union {')
    types.append(core)
    for path,name,native in [('include/obj00.h','FREE_OBJ00','NativeFreeObj00'),
                            ('include/obj10.h','FREE_WEP_ITEM','NativeFreeWepItem'),
                            ('include/obj04.h','OBJ04_FREE','NativeObj04Free'),
                            ('include/obj05.h','OBJ05_FREE','NativeObj05Free'),
                            ('include/obj09.h','OBJ09_FREE','NativeObj09Free')]:
        types.append(record(path,name,native))
    enum=re.search(r'    enum ID \{.*?\n    \};',read('include/obj.h'),re.S)[0].replace('enum ID','enum NativeObjectId')
    types.extend([enum,'}'])
    (output/'include/re4_object_types.h').write_text('\n'.join(types)+'\n',encoding='utf-8')
    cpp=['#include "object_original.h"','#include <cmath>','namespace re4::port {']
    entries=[]
    def add(path,sig,changes=()):
        src=read(path)
        if path.startswith('include/'):src=re.sub(r'^    ','',src,flags=re.M)
        text,line=function(src.replace(sig+' {',sig+'\n{'),sig)
        for old,new in changes:
            if old not in text:raise ValueError('Object adaptation missing: '+old)
            text=text.replace(old,new)
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',text])
        entries.append(dict(source=path,signature=sig,line=line,adaptations=[old for old,_ in changes]))
    add('src/game/obj.cpp','cObj::cObj()', [('cObj::cObj()','void NativeObject::initializeObject()')])
    add('include/cManager.h','virtual ~cUnit()', [('virtual ~cUnit()','NativeObject::~NativeObject()')])
    add('src/game/model.cpp','void cModel::push()', [('cModel::','NativeObject::')])
    cpp.extend(['struct ObjectLog { template<class... A> [[noreturn]] void err(int,int,const char* m,A...) {throw std::invalid_argument(m);} };',
                'static ObjectLog objectLog;static ObjectLog* pLog=&objectLog;'])
    for sig in ['cLightInfo::cLightInfo()',
                'int cLightInfo::init2(int type, int partsNo, const Vec* pOffset, const Vec* pSize, int mask)',
                'u32 cLightInfo::getLightNum()']:
        changes=[('cLightInfo::','ModelLightInfo::')]
        if sig.startswith('cLightInfo::'):changes.append(('ModelLightInfo::cLightInfo()','ModelLightInfo::ModelLightInfo()'))
        if 'init2' in sig:changes += [('!VALID_PTR(pOffset) || !VALID_PTR(pSize)','!pOffset || !pSize'),('SQRTF(','std::sqrt(')]
        add('src/game/lightInfo.cpp',sig,changes)
    add('src/game/objWep.cpp','cObjWep::cObjWep()', [('cObjWep::cObjWep()','void NativeWeapon::initializeWeapon()')])
    # Selected classes have implicit original constructors: cObj's constructor
    # plus zeroed FREE work. Every other switch branch fails by class name.
    supported={'cObj00':'NativeProp','cObj04':'NativeEffect4','cObj05':'NativeEffect5',
               'cObj09':'NativeEffect9','cWepItem':'NativeThrown','cObjWep':'NativeWeapon'}
    sig='int cObjMgr::construct(cObj* pObj, ID id)'
    body,_=function(read('src/game/obj.cpp'),sig)
    changes=[(sig,'int NativeObjectManager::constructTyped(NativeObject* pObj, u32 id)')]
    for cls in re.findall(r'new \(pObj\) (\w+)\(\)',body):
        old='new (pObj) '+cls+'()'
        new='place<'+supported[cls]+'>(pObj)' if cls in supported else 'unsupported(pObj,id,"'+cls+'")'
        changes.append((old,new))
    changes += [('HALT();','throw std::out_of_range("original ObjMgr ID > 0x3F");'),
                ('ObjInitFunc[id](pObj);','unsupported(pObj,id,"ObjInitFunc/native weapon module registration");')]
    add('src/game/obj.cpp',sig,changes)
    add('src/game/obj.cpp','int cObjMgr::construct(cObj* pObj, u32 id)',
        [('cObjMgr::construct(cObj*','NativeObjectManager::construct(NativeObject*'),
         ('construct(pObj, (ID) id)','constructTyped(pObj, id)')])
    sigs=['void cManager<T>::setName(const char* n)','int cManager<T>::roomInit()',
          'int cManager<T>::arrayFree()','int cManager<T>::arrayAlloc(u32 n)',
          'int cManager<T>::dieCheck()','u32 cManager<T>::countActiveWork()',
          'T* cManager<T>::create(int id)','T* cManager<T>::create()',
          'T* cManager<T>::create(int id, u32 no)','T* cManager<T>::createBack(int id)',
          'int deleteList(T* p)','void addListFront(T* p)','void addListBack(T* p)',
          'void cManager<T>::destroyAll()','inline void cManager<T>::destroy(T* p)']
    for sig in sigs:
        text,_=function(re.sub(r'^    ','',read('include/cManager.h'),flags=re.M).replace(sig+' {',sig+'\n{'),sig)
        native=sig.replace('inline ','').replace('cManager<T>::','NativeObjectManager::').replace('T*','NativeObject*')
        if 'cManager<T>::' not in sig:native=native.replace(' '+sig.split('(')[0].split()[-1]+'(',
                                                    ' NativeObjectManager::'+sig.split('(')[0].split()[-1]+'(')
        if '::destroy(' in native:native=native.replace('::destroy(','::destroyBase(')
        changes=[(sig,native)]
        adapted=text.replace(sig,native)
        for old,new in [('T*','NativeObject*'),('(T*)','(NativeObject*)'),
                        ('(NativeObject*)((u8*)pArray + size * i)','fastAt(i)'),
                        ('(NativeObject*)((u8*)pArray + size * no)','fastAt(no)'),
                        ('delete p;\n            p->be_flag = 0;','destruct(p);'),
                        ('delete p;\n                p->be_flag = 0;','destruct(p);'),
                        ('delete p;\n    p->be_flag = 0;','destruct(p);'),
                        ('(u32)p < 0x80000000 || (u32)p > 0x82FFFFFF','!owns(p)')]:
            if old in adapted:
                changes.append((old,new));adapted=adapted.replace(old,new)
        if 'create(' in sig or 'createBack(' in sig:
            if 'addList' in text:changes.append(('addList','/* Native placement lifetime: resolve the reconstructed base. */\n            p=fastAt('+('no' if 'u32 no' in sig else 'i')+');\n            addList'))
        add('include/cManager.h',sig,changes)
    add('src/game/obj.cpp','void cObjMgr::destroy(cObj* pObj)',
        [('cObjMgr::destroy(cObj*','NativeObjectManager::destroy(NativeObject*'),
         ('cManager<cObj>::destroy(pObj);','destroyBase(pObj);')])
    add('src/game/event.cpp','void cManager<T>::destroyNow(T* p)',
        [('cManager<T>::','NativeObjectManager::'),('T*','NativeObject*'),
         ('destroy(p);','try { destroy(p); } catch(...) { flag=f;throw; }')])
    cpp.append('#include "effect_original.h"')
    # The include must live outside the namespace; the header owns its namespace.
    cpp[-1]='}\n#include "effect_original.h"\nnamespace re4::port {'
    for sig in ['cEsp::cEsp()','cEsp::~cEsp()','void cEsp::Destruct()']:
        native=sig.replace('cEsp::cEsp()','EffectSprite::EffectSprite()').replace('cEsp::~cEsp()','EffectSprite::~EffectSprite()').replace('cEsp::','EffectSprite::')
        add('src/game/esp_sub.cpp',sig,[(sig,native)])
    for path,sig in [('src/game/esp.cpp','void PushEsp(cEsp* pEsp)'),
                     ('src/game/est.cpp','void EspDelete(int a, int b, void* c, cModel* pMod)'),
                     ('src/game/est.cpp','void EffectEspDelete(int a, int b, void* c, cModel* pMod)'),
                     ('src/game/est.cpp','void EffectEspgenDelete(int a, int b, void* c)'),
                     ('src/game/est.cpp','void EffectEfmDelete(int a, int b, void* c)'),
                     ('src/game/espgen.cpp','u32 GetEspgenIdMax()'),
                     ('src/game/espgen.cpp','void PushEspgen(cEspgen* pEspgen)'),
                     ('src/game/espgen.cpp','void EspgenDelete(int a, int b, void* c)'),
                     ('src/game/esp_efm.cpp','void EfmDelete(int a, int b, void* c)'),
                     ('src/game/esp_efm.cpp','void EfmDeleteSub(cObj* pObj)')]:
        src,_=function(read(path),sig)
        name=sig.split('(')[0].split()[-1]
        native=sig.replace(name+'(','EffectDomain::'+name+'(')
        changes=[(sig,native)]
        adapted=src.replace(sig,native)
        for old,new in [('cEspSystem*','System*'),('cEsp*','EffectSprite*'),('cEspgen*','EffectGenerator*'),
                        ('cModel*','ModelCpu*'),('cObj*','NativeObject*'),
                        ('(EffectSprite*) (sys->EspArray + i * 0x150)','sys->EspArray + i'),
                        ('g_Core_pEm = (ModelCpu*) c;','g_Core_pEm = c;'),
                        ('ObjMgr.applyFunc(EfmDeleteSub);','ObjMgr.applyFunc([this](NativeObject* p) {EfmDeleteSub(p);});'),
                        ('OBJ04_FREE*','NativeObj04Free*'),('OBJ05_FREE*','NativeObj05Free*'),('OBJ09_FREE*','NativeObj09Free*'),
                        ('EFM04_WK((cObj04*) pObj)','effect4(pObj)'),('EFM05_WK((cObj05*) pObj)','effect5(pObj)'),('EFM09_WK((cObj09*) pObj)','effect9(pObj)')]:
            if old in adapted:changes.append((old,new));adapted=adapted.replace(old,new)
        if name=='EfmDeleteSub':changes.append(('    if (pObj->id == 4)', '    if(pObj->isEmpty()) return; // Native retired cell has no concrete FREE work.\n    if (pObj->id == 4)'))
        add(path,sig,changes)
    cpp.append('}')
    (output/'object_control.cpp').write_text('\n\n'.join(cpp)+'\n',encoding='utf-8')
    (output/'object-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries,
        scope='Original list/allocation/deferred deletion algorithms, native typed object lifetimes. Six constructor kinds; missing movement/classes throw. No full player actor claim.'),indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
