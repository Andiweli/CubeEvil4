"""Checked complete original SMD creation/registration bodies for native views."""
import argparse
import json
from pathlib import Path
from generate_runtime import digest, function


def generate(root, output):
    hashes=json.loads((root/'port/config/scroll-sources.json').read_text())
    for path, expected in hashes.items():
        if digest(root/path)!=expected:raise ValueError('Scroll fingerprint mismatch: '+path)
    records=[];code=['#include "scroll_original.h"','namespace re4::port::scroll {']
    def add(path,sig,changes=()):
        body,line=function((root/path).read_text(encoding='utf-8'),sig)
        for before,after in changes:
            if body.count(before)!=1:raise ValueError('Scroll adaptation no longer unique: '+before)
            body=body.replace(before,after)
        code.extend([f'#line {line} "{(root/path).as_posix()}"',body])
        records.append(dict(source=path,signature=sig,line=line,adaptations=[a for a,b in changes]))
    for sig in ['int cSmd::getWorkNum()']:
        add('src/game/scroll.cpp',sig)
    add('src/game/model.cpp','inline void cModel::setNoSuspend(int onoff)',[
        ('inline void cModel::','void cObj::')])
    add('src/game/scroll.cpp','cSmdWork* cSmd::getWorkPtr(int id)',[
        ('(cSmdWork*) ((u8*) this + grp.nGroup * 4 + 0x14)','work.data()'),
        ('&work[id]','&work.at(id)')])
    add('src/game/obj.cpp','cObj::cObj()', [('cObj::cObj()','void cObj::initializeObject()')])
    add('src/game/obj02.cpp','cObjScr::cObjScr()', [('cObjScr::cObjScr()','void cObj::initializeScroll()')])
    for kind,field,limit in [('Bin','BinTblOfs','TplTblOfs'),('Tpl','TplTblOfs','MotTblOfs'),('Mot','MotTblOfs','bytes.size')]:
        sig=f'void* cSmd::get{kind}Ptr(int id)'
        before=f'    u8* tbl = (u8*) this + {field};\n    return tbl + ((u32*) tbl)[id];'
        add('src/game/scroll.cpp',sig,[(before,f'    auto value=entry({field},{limit},id);\n    return const_cast<u8*>(value.data);')])
    for sig in ['void workInit(cObj* pObj)','void SmdSetup(int blockNo)','int setObj(int blkNo)',
                'void SmxSetFlag(cObj* pObj, u32 flag)','void smxInit(cObj* obj, u8 id)',
                'void BlockCreate(int blkNo, cSmd* pBlock)','void BlockDestroy(int blkNo)',
                'cObj* SmdGetGroupNext(cObj* pObj00)', 'cObj* SmdGetGroupObjPtr2(u32 idx)']:
        parts=sig.split(' ',1);out=parts[0]+' Domain::'+parts[1]
        # Pointer return's star remains attached to the native return type.
        if sig.startswith('cObj* '):out='cObj* Domain::'+sig[6:]
        changes=[(sig,out)]
        if 'BlockDestroy(' in sig:
            changes.append(('\n{','\n{\n    checkBlockRelease(blkNo); // Validate all native borrowers before retiring any group member.'))
        if 'setObj' in sig:
            changes.append(('scrTbl[i] = obj;','scrTbl[i] = obj; obj->localIndex=u32(i); // Preserve wire work index across unused entries.'))
            # The original probes slot 0xFE before checking its sentinel even
            # though the ID table has only 250 entries. Its value cannot affect
            # the result; avoid that out-of-bounds read on native allocations.
            changes.append(('scrObjTbl[w->WorkNo] == NULL && w->WorkNo != 0xFE',
                            'w->WorkNo != 0xFE && scrObjTbl[w->WorkNo] == NULL'))
        add('src/game/scroll.cpp',sig,changes)
    sig='int SmdInit(cSmd* pSh, cSmxData* pSmxh, cSmd* pShCmn)'
    add('src/game/scroll.cpp',sig,[(sig,'int Domain::'+sig[4:]),
        ('scrObjTbl = (cObj**) MEM_ALLOC(250 * sizeof(cObj*), 1, 13);','ids.assign(250,nullptr); scrObjTbl=ids.data();'),
        ('scrTbl = (cObj**) MEM_ALLOC(nScrWork * sizeof(cObj*), 1, 13);','works.assign(nScrWork,nullptr); scrTbl=works.data();')])
    sig='void SmdClear(int mode)'
    add('src/game/scroll.cpp',sig,[(sig,'void Domain::'+sig[5:]),
        ('memclr_asm(scrObjTbl, 250 * sizeof(cObj*));','clearPointerTable(scrObjTbl,250);'),
        ('memclr_asm(scrTbl, nScrWork * sizeof(cObj*));','clearPointerTable(scrTbl,nScrWork);'),
        ('(cObj**) (i * sizeof(cObj*) + (u32) scrObjTbl)','scrObjTbl+i')])
    sig='int SmdSetParam(cObj* pObj, cSmdWork* pSw)'
    add('src/game/scroll.cpp',sig,[(sig,'int Domain::'+sig[4:]),
        ('tpl = DmyZeroTpl;','tpl = nullptr; // Native immutable empty TPL lease, resolved by modelInit.'),
        ('        u8* tbl = (u8*) pSmdComn + pSmdComn->TplTblOfs;\n        pObj->pModelInfo->addTplAddr(tbl + *(u32*) tbl);',
         '        pObj->pModelInfo->addTplAddr(pSmdComn->getTplPtr(0));'),
        ('pObj->LightInfo.updateMatrix(pObj);','pObj->updateLight();')])
    sig='void smxInit(cObj* obj, cSmxWork* w)'
    add('src/game/scroll.cpp',sig,[(sig,'void Domain::'+sig[5:]),
        ('(u32) obj < 0x80000000 || (u32) obj > 0x82FFFFFF','!ObjMgr.owns(obj)'),
        ('*(u32*) mi->color = col;','storeColor(mi->color,col);'),
        ('*(u32*) mi->color2 = col;','storeColor(mi->color2,col);')])
    sig='T* cManager<T>::createBack(int id)'
    add('include/cManager.h',sig,[(sig,'cObj* ObjectManager::createBack(int id)'),
        ('T* p = (T*)((u8*)pArray + size * i);','cObj* p = pArray+i;')])
    sig='T* cManager<T>::getPrevWork(T* p)'
    add('include/cManager.h',sig,[(sig,'cObj* ObjectManager::getPrevWork(cObj* p)'),
        ('    p = (T*)((u8*)p - size);\n    if ((u32)p < (u32)pArray || (u32)p >= (u32)pArray + size * (nArray - 1)) {',
         '    if (!owns(p) || p==pArray) return nullptr;\n    --p;\n    if (p >= pArray+nArray-1) {')])
    code.append('}')
    output.mkdir(parents=True,exist_ok=True)
    (output/'scroll_control.cpp').write_text('\n\n'.join(code)+'\n',encoding='utf-8')
    (output/'scroll-manifest.json').write_text(json.dumps(dict(functions=records),indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
