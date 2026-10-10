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
    cpp=['#include "consumer_original.h"','namespace re4::port {']
    entries=[]
    def add(path, signature, changes):
        body,line=function((root/path).read_text(encoding='utf-8'),signature)
        for before, after in changes:
            if body.count(before)!=1: raise ValueError('consumer adaptation no longer unique: '+before)
            body=body.replace(before,after)
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',body])
        entries.append(dict(source=path,signature=signature,line=line,adaptations=[a for a,_ in changes]))
    for sig in ['void cModel::setPartsParent()', 'inline void cModel::setNoSuspend(int onoff)',
                'inline int cModel::isTrans()', 'void cModel::updateOldPos()',
                'inline cParts* cModel::getPartsPtr(int idx)']:
        add('src/game/model.cpp',sig,[(sig,sig.replace('inline ','').replace('cModel::','ModelCpu::').replace('cParts*','ModelPart*'))])
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
