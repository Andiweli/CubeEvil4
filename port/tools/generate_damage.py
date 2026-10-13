"""Original typed damage volumes and cManager bodies, scalar hit predicates."""
import argparse,json,re
from pathlib import Path
from generate_runtime import digest,function
def generate(root,out):
    hashes=json.loads((root/'port/config/damage-sources.json').read_text())
    for p,h in hashes.items():
        if digest(root/p)!=h:raise ValueError('Original damage fingerprint mismatch: '+p)
    out.mkdir(parents=True,exist_ok=True);(out/'include').mkdir(exist_ok=True)
    enum=re.search(r'enum DMG_TYPE \{.*?\n\};',(root/'include/dmg.h').read_text(),re.S)[0]
    (out/'include/re4_damage_types.h').write_text('#pragma once\nnamespace re4::port {\n'+enum+'\n}\n')
    cpp=['#include "damage_original.h"','#include "player_original.h"','namespace re4::port {','constexpr int ID_CYLINDER=0,ID_POINT4=1;'];entries=[]
    def add(path,sig,changes):
        src=(root/path).read_text();src=re.sub(r'^    ','',src,flags=re.M) if path.startswith('include/') else src
        b,line=function(src.replace(sig+' {',sig+'\n{'),sig)
        for a,z in changes:
            if a not in b:raise ValueError('Damage adaptation absent: '+a)
            b=b.replace(a,z)
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',b]);entries.append(dict(source=path,signature=sig,line=line,adaptations=[a for a,_ in changes]))
    for sig in ['int cDmgMgr::construct(cDmg* pDmg, int id)','int cDmgMgr::construct(cDmg* pDmg, u32 id)',
                'void cDmgMgr::move()','int cDmgMgr::set(int type, int time, Vec* pPos, f32 radius, f32 height)',
                'int cDmgMgr::set(int type, int time, Vec* pPos4, f32 height)', 'int cDmgMgr::hitCheck(Vec* pPos, Vec* pFrom)',
                'int cDmgCyl::hitCheck(Vec* pPos, Vec* pFrom)','int cDmgP4::hitCheck(Vec* pPos, Vec* pFrom)','void cDmg::beginEvent(u32 flag)']:
        b,_=function((root/'src/game/dmg.cpp').read_text(),sig);changes=[]
        for a,z in [('cDmgMgr::','DamageManager::'),('cDmgCyl::','DamageCylinder::'),('cDmgP4::','DamageQuad::'),('cDmg::','DamageVolume::'),('cDmgCyl*','DamageCylinder*'),('cDmgP4*','DamageQuad*'),('cDmg*','DamageVolume*'),('new (pDmg) cDmgCyl;','pDmg=place<DamageCylinder>(pDmg);'),('new (pDmg) cDmgP4;','pDmg=place<DamageQuad>(pDmg);'),('DmgMgr.destroy(this);','manager->destroy(this);'),('PSVECAdd','C_VECAdd'),('PSVECScale','motionVecScale')]:
            if a in b:changes.append((a,z));b=b.replace(a,z)
        if sig=='void cDmgMgr::move()':changes.append(('        dieCheck();','        dieCheck();\n        p=fastAt(i); // Refresh the typed lifetime after deferred destruction.'))
        if 'DamageCylinder* p = (DamageCylinder*) create' in b:changes.append(('DamageCylinder* p = (DamageCylinder*) create(ID_CYLINDER);','DamageCylinder* p = created<DamageCylinder>(create(ID_CYLINDER));'))
        if 'DamageQuad* p = (DamageQuad*) create' in b:changes.append(('DamageQuad* p = (DamageQuad*) create(ID_POINT4);','DamageQuad* p = created<DamageQuad>(create(ID_POINT4));'))
        add('src/game/dmg.cpp',sig,changes)
    for sig in ['void cManager<T>::setName(const char* n)','int cManager<T>::roomInit()', 'int cManager<T>::arrayFree()',
                'int cManager<T>::arrayAlloc(u32 n)', 'int cManager<T>::dieCheck()', 'u32 cManager<T>::countActiveWork()',
                'T* cManager<T>::create(int id)','int deleteList(T* p)','void addListFront(T* p)','inline void cManager<T>::destroy(T* p)']:
        src=re.sub(r'^    ','',(root/'include/cManager.h').read_text(),flags=re.M);text,_=function(src.replace(sig+' {',sig+'\n{'),sig)
        native=sig.replace('inline ','').replace('cManager<T>::','DamageManager::').replace('T*','DamageVolume*')
        if 'cManager<T>::' not in sig:native=native.replace(' '+sig.split('(')[0].split()[-1]+'(',' DamageManager::'+sig.split('(')[0].split()[-1]+'(')
        changes=[(sig,native)];adapted=text.replace(sig,native)
        for a,z in [('T*','DamageVolume*'),('(T*)','(DamageVolume*)'),('(DamageVolume*)((u8*)pArray + size * i)','fastAt(i)'),('delete p;\n            p->be_flag = 0;','destruct(p);'),('delete p;\n                p->be_flag = 0;','destruct(p);'),('delete p;\n    p->be_flag = 0;','destruct(p);'),('(u32)p < 0x80000000 || (u32)p > 0x82FFFFFF','!owns(p)')]:
            if a in adapted:changes.append((a,z));adapted=adapted.replace(a,z)
        if 'create(' in sig:changes.append(('addListFront(p);','p=fastAt(i);\n            addListFront(p);'))
        if 'destroy(T* p)' in sig:changes.append((native+'\n{',native+'\n{\n    index(p);'))
        add('include/cManager.h',sig,changes)
    add('src/game/sub2.cpp','int HitCheckPoint4(Vec* pos, Vec* xz)',[])
    cpp.append('}')
    (out/'damage_control.cpp').write_text('\n\n'.join(cpp)+'\n');(out/'damage-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries),indent=2)+'\n')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
