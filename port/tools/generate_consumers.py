#!/usr/bin/env python3
"""Source-guarded CPU consumers; every adaptation is recorded, no assets embedded."""
import argparse
import json
import re
from pathlib import Path
from generate_runtime import digest, function


def generate(root, output):
    hashes = json.loads((root/'port/config/consumer-sources.json').read_text())
    for path, expected in hashes.items():
        if digest(root/path) != expected:
            raise ValueError('Original consumer fingerprint mismatch: '+path)
    output.mkdir(parents=True, exist_ok=True)
    cpp=['#include "model_original.h"','namespace re4::port {']
    entries=[]
    def add(path, signature, changes):
        source=(root/path).read_text(encoding='utf-8')
        if path.startswith('include/'):
            source=re.sub(r'^    ', '', source, flags=re.M)
        source=source.replace(signature+' {',signature+'\n{')
        body,line=function(source,signature)
        for before, after in changes:
            if body.count(before)!=1: raise ValueError('consumer adaptation no longer unique: '+before)
            body=body.replace(before,after)
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',body])
        entries.append(dict(source=path,signature=signature,line=line,adaptations=[a for a,_ in changes]))
    for sig in ['void cModel::setPartsParent()', 'inline void cModel::setNoSuspend(int onoff)',
                'inline int cModel::isTrans()', 'void cModel::updateOldPos()',
                'inline cParts* cModel::getPartsPtr(int idx)']:
        add('src/game/model.cpp',sig,[(sig,sig.replace('inline ','').replace('cModel::','ModelCpu::').replace('cParts*','ModelPart*'))])
    add('include/model.h','cCoord() : cUnit(1)', [('cCoord() : cUnit(1)','ModelCoord::ModelCoord()')])
    add('src/game/model.cpp','cModel::cModel()', [('cModel::cModel()','ModelCpu::ModelCpu()'),
        ('memclr_asm(&Motion, 0xD0);','Motion = {}; // Native pointer fields, never a 0xD0 byte overlay.')])
    add('src/game/model.cpp','cModelInfo::cModelInfo() : cUnit(1)',
        [('cModelInfo::cModelInfo() : cUnit(1)','ModelInfoView::ModelInfoView()')])
    add('src/game/model.cpp','int cModel::modelInit(void* bin, void* tpl)',[
        ('int cModel::modelInit','ModelInfoView* ModelCpu::modelInit'),
        ('SysFlagChk(pG, SYS_INVISIBLE)','systemInvisible'),('return (int) info;','return info;')])
    for sig in ['int cModel::initJoint(void* bin)','void cModel::releaseJoint()',
                'void cModel::setPartsOffset(void* bin)','void cModel::partsMatCalc()',
                'void cModel::partsWorldCalc()','void cModel::addModel(cModelInfo* pInfo)',
                'void cModel::releaseModelInfo()','int cModel::makePartsList(int n)',
                'void cModel::setJointInfo(void* pHead)','void cModel::releasePartsList(int idx)']:
        changes=[(sig,sig.replace('cModel::','ModelCpu::'))]
        if 'setJointInfo' in sig:
            changes.append(('(u16*) (d->flipTbl + 4)','(u16*) (d->flipTbl + 4)'))
        add('src/game/model.cpp',sig,changes)
    add('src/game/model.cpp','cModelInfo* cModInfoMgr::create(void* bin, void* tpl)',[
        ('cModelInfo* cModInfoMgr::create','ModelInfoView* ModelInfoManager::create'),
        ('cManager<cModelInfo>::create()','ModelPool<ModelInfoView>::create()')])
    add('src/game/model.cpp','cParts* cPartsMgr::createSequential(u32 n)',[
        ('cParts* cPartsMgr::createSequential','ModelPart* ModelPartsManager::createSequential'),
        ('    u32 i;','    if (!n || n >= nArray) return nullptr; // Bound original unsigned subtraction.\n    u32 i;')])
    add('src/game/model.cpp','static inline cParts* PartsMgrWork(cPartsMgr* m, u32 no)',[
        ('static inline cParts* PartsMgrWork(cPartsMgr* m','ModelPart* PartsMgrWork(ModelPartsManager* m')])
    add('src/game/model.cpp','void getBoundingBox(cModelData* d, cBoundingBox* pBox)',[])
    for sig in ['void ScaleMatrix(Mtx m, Vec* vec)','void TransMatrix(Mtx m, Vec* pos)',
                'void RotMatrix(Mtx m, Vec* vec)']:
        add('src/game/math_sub.cpp',sig,[])
    for sig in ['void C_MTXIdentity(Mtx m)','void C_MTXCopy(const Mtx src, Mtx dst)',
                'void C_MTXConcat(const Mtx a, const Mtx b, Mtx ab)',
                'void C_MTXTrans(Mtx m, f32 xT, f32 yT, f32 zT)',
                'void C_MTXScale(Mtx m, f32 xS, f32 yS, f32 zS)',
                'void C_MTXRotRad(Mtx m, char axis, f32 rad)',
                'void C_MTXRotTrig(Mtx m, char axis, f32 sinA, f32 cosA)']:
        add('src/lib/mtx.c',sig,[])
    add('src/lib/mtxvec.c','void C_MTXMultVec(const Mtx m, const Vec* src, Vec* dst)',[])
    # Exact original pool loops/list ordering, with defined native C++ object lifetimes.
    for sig in ['T* cManager<T>::create(int id)','T* cManager<T>::create()',
                'T* cManager<T>::create(int id, u32 no)','u32 cManager<T>::countActiveWork()',
                'int deleteList(T* p)','void addListFront(T* p)','inline void cManager<T>::destroy(T* p)']:
        native=sig.replace('inline ','').replace('cManager<T>::','ModelPool<T>::')
        if '::' not in native:
            native=native.replace('deleteList','ModelPool<T>::deleteList').replace('addListFront','ModelPool<T>::addListFront')
        changes=[(sig,'template<class T> '+native)]
        if 'destroy' in sig:
            changes += [('(u32)p < 0x80000000 || (u32)p > 0x82FFFFFF','!owns(p)'),
                        ('delete p;','p->be_flag &= ~0x601u; // Keep a live native slot object; original destructor mask.')]
        add('include/cManager.h',sig,changes)
    cpp += ['template class ModelPool<ModelPart>;','template class ModelPool<ModelInfoView>;']
    add('src/game/event.cpp','int Event::ExeFunc(int mode, int param)',[
        ('int Event::ExeFunc','int EventCpu::ExeFunc'),('void* fn;','const EventFunction *fn;'),
        ('EvtMgr.GetFunc(&fn, nm)','manager->GetFunc(&fn, nm)'),
        ('((EvtFunc) fn)(this, param);','(*fn)(this, static_cast<u32>(param));')])
    cpp.append('struct MesTblBlock { u32 x0,count,ofs[1]; };')
    for sig in ['u16* MessageData::getAddr(int no, int data_type)',
                'int MessageData::getMesNum(int data_type)', 'int MessageData::getSpaceWidth()']:
        add('src/game/mes.cpp',sig,[(sig,sig.replace('MessageData::','MessageIndexCpu::'))])
    for sig in ['int CameraControl::CurrentAreaNo()', 'int CameraControl::CurrentCameraNo()',
                'void CameraControl::Disable()', 'void CameraControl::AreaCheckOnOff(int sw)']:
        add('src/game/cam_ctrl.cpp',sig,[(sig,sig.replace('CameraControl::','CameraCpu::'))])
    enums=(root/'include/event.h').read_text()
    cpp.insert(2,re.search(r'enum EvtStatusFlag \{.*?\n\};',enums,re.S)[0])
    cpp.append('} // namespace re4::port')
    (output/'consumer_control.cpp').write_text('\n\n'.join(cpp)+'\n',encoding='utf-8')
    (output/'consumer-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries),indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True); p.add_argument('--output',type=Path,required=True)
    a=p.parse_args(); generate(a.root.resolve(),a.output.resolve())
