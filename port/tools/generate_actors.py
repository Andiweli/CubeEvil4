"""Whole original actor manager and typed C++ dispatch; no GC vtable overlay."""
import argparse,json,re
from pathlib import Path
from generate_runtime import digest,function
def generate(root,out):
    hashes=json.loads((root/'port/config/actor-sources.json').read_text())
    for path,wanted in hashes.items():
        if digest(root/path)!=wanted:raise ValueError('Original actor fingerprint mismatch: '+path)
    out.mkdir(parents=True,exist_ok=True);(out/'include').mkdir(exist_ok=True)
    status=re.search(r'enum EM_STATUS \{.*?\n\};',(root/'include/em.h').read_text(),re.S)[0]
    (out/'include/re4_actor_types.h').write_text('#pragma once\nnamespace re4::port {\n'+status+'\n}\n')
    cpp=['#include "actor_original.h"','#include "player_original.h"','namespace re4::port {'];entries=[]
    def add(path,sig,changes):
        src=(root/path).read_text();src=re.sub(r'^    ','',src,flags=re.M) if path.startswith('include/') else src
        body,line=function(src.replace(sig+' {',sig+'\n{'),sig)
        for a,b in changes:
            if a not in body:raise ValueError('Actor adaptation absent: '+a)
            body=body.replace(a,b)
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',body]);entries.append(dict(source=path,signature=sig,line=line,adaptations=[a for a,_ in changes]))
    sig='int cEmMgr::construct(cEm* pEm, u32 id)';body,_=function((root/'src/game/em.cpp').read_text(),sig)
    changes=[(sig,'int NativeActorManager::construct(NativeEmActor* pEm, u32 id)'),('pG->pl_type','player.pG->pl_type')]
    for cls in re.findall(r'new \(pEm\) (\w+)',body):
        changes.append(('new (pEm) '+cls,'placeLeon(pEm)' if cls=='cPlLeon' else 'unsupported(pEm,id,"'+cls+'")'))
    changes += [('PlInitFunc(pEm);','unsupported(pEm,id,"PlInitFunc");'),('pEm->subArc = (PlArc*)EmReadSearch((u8) id, 0, 0);','unsupported(pEm,id,"EmReadSearch/EmInitFunc");'),('EmInitFunc(pEm);','unsupported(pEm,id,"EmInitFunc");')]
    add('src/game/em.cpp',sig,changes)
    add('src/game/pl_leon.cpp','void cPlLeon::move()', [('cPlLeon::move','NativeLeonActor::move'),('cPlayer::move','NativePlayerActor::move')])
    player_header=(root/'include/player.h').read_text()
    for result,name in [('void','moveMatCalcBefore'),('void','moveCloth'),('int','checkXbutton')]:
        original=re.search(r'virtual '+result+' '+name+r'\(\)\s*\{[^}]*\}',player_header)[0]
        cpp.append(original.replace('virtual '+result+' '+name,result+' NativePlayerActor::'+name,1))
        entries.append(dict(source='include/player.h',signature='virtual '+result+' '+name+'()',line=player_header[:player_header.index(original)].count('\n')+1,adaptations=['native class qualification']))
    for name in ['setStatus','clearStatus','checkStatus']:
        sig=('int' if name=='checkStatus' else 'void')+' cEm::'+name+'(int id)'
        if name=='checkStatus':sig='int cEm::checkStatus(int id)'
        add('src/game/em.cpp',sig,[('cEm::','ActorModelCpu::')])
    sigs=['void cManager<T>::setName(const char* n)','int cManager<T>::roomInit()',
          'int cManager<T>::arrayFree()','int cManager<T>::arrayAlloc(u32 n)',
          'int cManager<T>::dieCheck()','u32 cManager<T>::countActiveWork()',
          'T* cManager<T>::create(int id)','T* cManager<T>::create(int id, u32 no)',
          'int deleteList(T* p)','void addListFront(T* p)',
          'void cManager<T>::destroyAll()','inline void cManager<T>::destroy(T* p)']
    for sig in sigs:
        src=re.sub(r'^    ','',(root/'include/cManager.h').read_text(),flags=re.M);text,_=function(src.replace(sig+' {',sig+'\n{'),sig)
        native=sig.replace('inline ','').replace('cManager<T>::','NativeActorManager::').replace('T*','NativeEmActor*')
        if 'cManager<T>::' not in sig:native=native.replace(' '+sig.split('(')[0].split()[-1]+'(',' NativeActorManager::'+sig.split('(')[0].split()[-1]+'(')
        if '::arrayAlloc(' in native:native=native.replace('::arrayAlloc(','::arrayAllocBase(')
        changes=[(sig,native)];adapted=text.replace(sig,native)
        for a,b in [('T*','NativeEmActor*'),('(T*)','(NativeEmActor*)'),('(NativeEmActor*)((u8*)pArray + size * i)','fastAt(i)'),('(NativeEmActor*)((u8*)pArray + size * no)','fastAt(no)'),('delete p;\n            p->be_flag = 0;','destruct(p);'),('delete p;\n                p->be_flag = 0;','destruct(p);'),('delete p;\n    p->be_flag = 0;','destruct(p);'),('(u32)p < 0x80000000 || (u32)p > 0x82FFFFFF','!owns(p)')]:
            if a in adapted:changes.append((a,b));adapted=adapted.replace(a,b)
        if 'create(' in sig:changes.append(('addListFront(p);','p=fastAt('+('no' if 'u32 no' in sig else 'i')+');\n            addListFront(p);'))
        if 'destroy(T* p)' in sig:changes.append((native+'\n{',native+'\n{\n    index(p);'))
        add('include/cManager.h',sig,changes)
    add('src/game/em.cpp','int cEmMgr::arrayAlloc(u32 workNum)',[('cEmMgr::','NativeActorManager::'),('cManager<cEm>::arrayAlloc(workNum);','arrayAllocBase(workNum);'),('pPL = 0;','player.pPL = 0;'),('pSUB = 0;','player.pSUB = 0;')])
    cpp.append('}')
    (out/'actor_control.cpp').write_text('\n\n'.join(cpp)+'\n')
    (out/'actor-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries),indent=2)+'\n')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
