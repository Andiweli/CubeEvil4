"""Guarded original Leon cloth setup, simulation and collision kernels."""
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
    cpp=['#include "cloth_original.h"','#include "rnd.h"','namespace re4::port {',*arrays];entries=[]
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
    # Preserve the original constraint/floor/attachment macros; compiler-only
    # PPC register anchors emit no code and have no runtime semantics.
    pendulum=read('src/game/pendulum.cpp')
    helpers=pendulum[pendulum.index('#define PEN_PARTS'):pendulum.index('// Set up the links')]
    helpers=helpers[:helpers.index('// wind =')]+helpers[helpers.index('// Collision of the link'):]
    helpers=helpers.replace('cParts','ModelPart').replace('cModel','ModelCpu').replace('CLOTH_INFO','ClothInfo').replace('PEN_INFO','PendulumState')
    # Rename only the macro-local upper link to avoid compiler shadowing
    # warnings. Its lexical scope and every arithmetic operation are intact.
    start=helpers.index('#define PEN_FLOOR_CK(');end=helpers.index('// Same with',start)
    helpers=helpers[:start]+re.sub(r'\buw\b','floorUpper',helpers[start:end])+helpers[end:]
    helpers=re.sub(r'#define PEN_ANCHOR\(v\).*', '#define PEN_ANCHOR(v) ((void)(v))',helpers)
    cpp.extend(['#define PSVECSubtract C_VECSubtract', '#define PSVECAdd C_VECAdd',
        '#define PSVECScale motionVecScale', '#define PSVECNormalize C_VECNormalize', '#define VECNormalize C_VECNormalize',
        '#define PSVECDotProduct ikDotProduct', '#define PSVECCrossProduct C_VECCrossProduct',
        '#define PSMTXMultVec C_MTXMultVec', '#define PSMTXMultVecSR motionMultVecSR', '#define PSMTXConcat C_MTXConcat',
        '#define PSMTXInverse C_MTXInverse', '#define PSMTXRotAxisRad ikRotAxisRad',
        '#define GetDistance3 ikDistance', '#define LIMIT_ANGLE(x) motionLimitAngle(x)', '#define SQRTF(x) sqrtf(x)', '#define PI 3.1415927f',
        '#define DbgFlagChk(g,n) ((g)->Debug_flg[(n)/32] & (0x80000000u >> ((n)%32)))',helpers])
    # Same source helper: no animation or collision calculation is invented.
    add('src/game/pendulum.cpp','static inline void penPartsWorldPos(cParts* p, const Vec* ofs, Vec* out)',
        'static inline void penPartsWorldPos(ModelPart* p, const Vec* ofs, Vec* out)')
    for sig in ['void PenClothMove(cModel* m, CLOTH_INFO* c)', 'void PenClothMove3(cModel* m, CLOTH_INFO* c)',
                'static void PenClothReset(cModel* m, CLOTH_INFO* c)',
                'PenAtWork* penClothAtMake(cModel* m, CLOTH_AT_SET* at, int n)',
                'int penClothAtCk(Vec* pos, Vec* up, PenAtWork* wk)',
                'int penClothAtCkBorder(Vec* pos, Vec* up, PenAtWork* wk)',
                'void penClothAtCkParallel(Vec* pos, Vec* up, PenAtWork* wk)']:
        body,_=function(pendulum,sig)
        native=sig.replace('static ','').replace('cModel','ModelCpu').replace('CLOTH_INFO','ClothInfo').replace('CLOTH_AT_SET','ClothAtSet').replace('PenAtWork','ClothAtWork')
        native=re.sub(r'\b(PenCloth\w+|penCloth\w+)(?=\()',r'ClothDomain::\1',native)
        changes=[]
        for a,b in [('cParts','ModelPart'),('cModel','ModelCpu'),('cCoord','ModelCoord'),('PEN_INFO','PendulumState'),
                    ('CLOTH_AT_DATA','ClothAtData'),('PenAtWork','ClothAtWork'),('SatMgr.getFloor','getFloor')]:
            if a in body.replace(sig,native,1):changes.append((a,b))
        if 'Move(' in sig or 'Move3(' in sig:
            changes.append(('    Mtx mtx;','    validate(m,c);\n    ++moves;\n    Mtx mtx;'))
        if 'Reset(' in sig:changes.append(('    cParts* parts;','    ++resets;\n    ModelPart* parts;'))
        if 'AtMake(' in sig:
            changes.extend([('    wk = (PenAtWork*) 0xE0000000;',
                '    if(n<0 || n>int(scratch.at.size()))throw std::out_of_range("native cloth scratch capacity");\n    for(int j=0;j<n;++j)if(at[j].P1>=m->nParts || at[j].P2>=m->nParts)throw std::out_of_range("cloth volume joint");\n    ++volumeBuilds;\n    wk = &scratch;'),('a = wk->at;','a = wk->at.data();')])
        if 'wk->pAt - 1' in body:
            changes.append(('a = wk->pAt - 1;\n    while (n--) {\n        a++;','a = wk->pAt;\n    for (; n--; a++) {'))
        # Append specific edits before broad type substitutions.
        changes.sort(key=lambda pair:len(pair[0]),reverse=True)
        # Strip only register declarations and empty compiler anchors. Every
        # assignment (including pv = pa) and calculation remains intact.
        for match in re.finditer(r'\s+asm\(""\s*:[^;]+\);',body):changes.insert(0,(match[0],''))
        for match in re.finditer(r'\s+asm\("r\d+"\)',body):changes.insert(0,(match[0],''))
        if 'register ' in body:changes.append(('register ',''))
        add('src/game/pendulum.cpp',sig,native,changes)
    for name in ['PlClothMoveLeon','testJacketMoveLeon','testHolsterMoveLeon','testHairMoveLeon']:
        sig=(f'void {name}(cModel* pl, CLOTH_INFO* pCloth1, CLOTH_INFO* pCloth2, CLOTH_INFO* pCloth3)' if name=='PlClothMoveLeon' else f'void {name}(cModel* pl, CLOTH_INFO* pCloth)')
        add('src/game/pl_cloth.cpp',sig,sig.replace('void '+name,'void ClothDomain::'+name).replace('cModel','ModelCpu').replace('CLOTH_INFO','ClothInfo'))

    add('src/game/sub2.cpp','void PosToPos(Vec* pos1, Vec* pos2, Vec* pos3, f32 per)',
        'void ClothDomain::PosToPos(Vec* pos1, Vec* pos2, Vec* pos3, f32 per)')
    add('src/game/pendulum.cpp','void PenWindSet(f32 dir, f32 power, f32 x)',
        'void ClothDomain::PenWindSet(f32 dir, f32 power, f32 x)')
    cpp.append('}')
    (out/'cloth_control.cpp').write_text('\n\n'.join(cpp)+'\n')
    (out/'cloth-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries,scope='Original Leon setup, Move/Move3, reset and volume collision; bounded native scratch; debug presentation rejects.'),indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
