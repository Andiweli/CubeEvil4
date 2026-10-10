"""Preserve complete original camera/SDK bodies with checked native FCV references."""
import argparse
import json
from pathlib import Path
from generate_runtime import digest, function

def generate(root, output):
    hashes=json.loads((root/'port/config/gx-sources.json').read_text())
    for path, expected in hashes.items():
        if digest(root/path)!=expected: raise ValueError('GX fingerprint mismatch: '+path)
    output.mkdir(parents=True,exist_ok=True)
    code=['#include "gx_original.h"','namespace re4::port {']
    records=[]
    for path,sig,changes in [
        ('include/math_sub.h','static inline void getColumn(Mtx m, int c, Vec* v)',[('static inline ','')]),
        ('include/math_sub.h','static inline void MTXSetColumns(Mtx mat, Vec& v0, Vec& v1, Vec& v2, Vec& v3)',[('static inline ','')]),
        ('src/lib/mtx.c','void C_MTXRotAxisRad(Mtx m, const Vec* axis, f32 rad)',[]),
        ('src/lib/mtx.c','void C_MTXLookAt(Mtx m, const Point3d* camPos, const Vec* camUp, const Point3d* target)',[]),
        ('src/lib/mtx44.c','void C_MTXPerspective(Mtx44 m, f32 fovY, f32 aspect, f32 n, f32 f)',[]),
        ('src/game/cam_sys.cpp','void CameraSetOrientationRoll(CAMERA* pCam)',[]),
        ('src/game/cam_sys.cpp','void CameraSetOrientationUp(CAMERA* pCam)',[]),
        ('src/game/cam_motion.cpp','void CameraMotion::move()',[
            ('void CameraMotion::move()','void DiagnosticCameraMotion::move()')]),
        ('src/game/cam_motion.cpp','u32 CameraSequenceCtrl(MOTION_INFO* pInfo)',[])]:
        source=(root/path).read_text().replace(sig+' {',sig+'\n{')
        body,line=function(source,sig)
        for a,b in changes:
            if body.count(a)!=1: raise ValueError('GX adaptation changed: '+a)
            body=body.replace(a,b)
        code += [f'#line {line} "{(root/path).as_posix()}"',body]
        records.append(dict(source=path,signature=sig,adaptations=[a for a,b in changes]))
    code.append('}')
    (output/'gx_control.cpp').write_text('\n\n'.join(code)+'\n')
    (output/'gx-manifest.json').write_text(json.dumps(dict(functions=records),indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
