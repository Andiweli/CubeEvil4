"""Complete original matrix/weight bodies; explicit locked-cache and pointer adapters."""
import argparse,json
from pathlib import Path
from generate_runtime import digest,function

def generate(root,out):
    hashes=json.loads((root/'port/config/consumer-sources.json').read_text())
    path='src/game/trans.cpp'
    if digest(root/path)!=hashes[path]:raise ValueError('Skinning original fingerprint changed')
    if digest(root/'src/lib/psmtx.c')!=hashes['src/lib/psmtx.c']:raise ValueError('PSMTXReorder source changed')
    source=(root/path).read_text();entries=[];cpp=['#include "skinning_original.h"','namespace re4::port::skin {']
    for sig in ['void calcWeightMat(cModel* m)','int MakeWeightPalette(cWeight* w0, int n)','int MakeWeightPaletteExt(cWeightExt* w0, int n)']:
        body,line=function(source,sig);changes=[]
        if sig.startswith('void'):
            changes=[('p = p->next','p = p->pList'),('((cModel*) p)->mat','p->mat'),('p->bindMat','p->lt_inv_mat')]
        else:
            changes=[('u32 wa = (u32) w0','std::uintptr_t wa = reinterpret_cast<std::uintptr_t>(w0)'),
                     ('(f32(*)[3]) (0xE0000000 + i * 0x30)','pG->reordered.at(i).lanes')]
        for a,b in changes:
            if body.count(a)!=1:raise ValueError('Skin adapter anchor: '+a)
            body=body.replace(a,b)
        cpp += [f'#line {line} "{(root/path).as_posix()}"',body]
        entries.append(dict(source=path,signature=sig,line=line,adaptations=[a for a,b in changes]))
    entries.extend([dict(source=path,signature=name,adaptations=['scalar paired-single FMA/GQR adapter; bounded indices; no final assembly prefetch']) for name in ['CalcSk1_x','CalcSk1_x2','setupGQR6']])
    entries.append(dict(source='src/lib/psmtx.c',signature='PSMTXReorder',adaptations=['native owned columns; explicit ps_merge lane mapping']))
    cpp+=['}'];out.mkdir(parents=True,exist_ok=True)
    (out/'skinning_control.cpp').write_text('\n'.join(cpp)+'\n')
    (out/'skinning-manifest.json').write_text(json.dumps(entries,indent=2)+'\n')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
