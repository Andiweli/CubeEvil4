"""Guarded original player cloth setup and pendulum link initialization."""
import argparse,json,re
from pathlib import Path
from generate_runtime import digest,function

def generate(root,out):
    hashes=json.loads((root/'port/config/cloth-sources.json').read_text())
    for path,wanted in hashes.items():
        if digest(root/path)!=wanted:raise ValueError('Original cloth fingerprint mismatch: '+path)
    out.mkdir(parents=True,exist_ok=True);(out/'include').mkdir(exist_ok=True)
    read=lambda p:(root/p).read_text(encoding='utf-8')
    def record(path,name,new):
        body=re.search(r'struct '+name+r' \{.*?\n\};',read(path),re.S)[0]
        body=body.replace(name,new).replace('cParts','ModelPart').replace('cModel','ModelCpu').replace('CLOTH_AT_SET','ClothAtSet')
        return re.sub(r';(?=\s*(?://|\n))','{};',body)
    pen=record('include/model.h','PEN_INFO','PendulumState')
    (out/'include/re4_pendulum_types.h').write_text('#pragma once\nnamespace re4::port {\n'+pen+'\n}\n')
    types=['#pragma once','#include "re4/port/model_types.h"','namespace re4::port {',
           record('include/pl_cloth.h','CLOTH_AT_SET','ClothAtSet'),record('include/pendulum.h','CLOTH_INFO','ClothInfo'),'}']
    (out/'include/re4_cloth_types.h').write_text('\n'.join(types)+'\n')
    src=read('src/game/pl_cloth.cpp');arrays=[]
    for match in re.finditer(r'(?:u8|f32|CLOTH_AT_SET) leon\w+\[\d+\] = \{.*?\};',src,re.S):
        arrays.append(match[0].replace('CLOTH_AT_SET','ClothAtSet'))
    cpp=['#include "cloth_original.h"','namespace re4::port {',*arrays];entries=[]
    def add(path,sig,new,changes=()):
        body,line=function(read(path),sig);body=body.replace(sig,new,1)
        for a,b in changes:
            if a not in body:raise ValueError('Cloth adaptation absent: '+a)
            body=body.replace(a,b)
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',body]);entries.append(dict(source=path,signature=sig,line=line,adaptations=[sig,*[a for a,_ in changes]]))
    sig='void PenClothSet(cModel* m, CLOTH_INFO* c, f32 min_len)'
    add('src/game/pendulum.cpp',sig,'void ClothDomain::PenClothSet(ModelCpu* m, ClothInfo* c, f32 min_len)',[
        ('cParts*','ModelPart*'),('PEN_INFO*','PendulumState*'),('    Vec v;','    validate(m,c);\n    Vec v;'),
        ('PSMTXMultVecSR','motionMultVecSR'),('PSMTXMultVec','C_MTXMultVec'),('PSVECAdd','C_VECAdd'),
        ('PSVECSubtract','C_VECSubtract'),('VECNormalize','C_VECNormalize'),('GetDistance3','ikDistance')])
    for sig in ['void PenClothFixSet(cModel* m, CLOTH_INFO* c, int no, Vec* pos)',
                'void PenClothFixClear(cModel* m, CLOTH_INFO* c, int no)']:
        new=sig.replace('void PenCloth','void ClothDomain::PenCloth').replace('cModel','ModelCpu').replace('CLOTH_INFO','ClothInfo')
        add('src/game/pendulum.cpp',sig,new,[('PEN_INFO*','PendulumState*'),
            ('    if (c->pCloth[no]', '    validate(m,c);\n    if(no<0 || no>=c->Num)throw std::out_of_range("cloth pin link");\n    if (c->pCloth[no]')])
    for name in ['PlClothSetLeon','testJacketSetLeon','testHolsterSetLeon','testHairSetLeon']:
        sig=(f'void {name}(cModel* pl, CLOTH_INFO* pCloth1, CLOTH_INFO* pCloth2, CLOTH_INFO* pCloth3)' if name=='PlClothSetLeon' else f'void {name}(cModel* pl, CLOTH_INFO* pCloth)')
        add('src/game/pl_cloth.cpp',sig,sig.replace('void '+name,'void ClothDomain::'+name).replace('cModel','ModelCpu').replace('CLOTH_INFO','ClothInfo'))
    cpp.append('}')
    (out/'cloth_control.cpp').write_text('\n\n'.join(cpp)+'\n')
    (out/'cloth-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries,scope='Original setup tables and PenClothSet; simulation is separately gated.'),indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
