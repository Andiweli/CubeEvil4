"""Preserved r103/r106 callback bodies with named native-pointer adaptations."""
import argparse,json,re
from pathlib import Path
from generate_runtime import digest,function

def generate(root,out):
    hashes=json.loads((root/'port/config/shelf-sources.json').read_text())
    for path,expected in hashes.items():
        if digest(root/path)!=expected:raise ValueError('Shelf fingerprint mismatch: '+path)
    code=['#include "shelf_original.h"','namespace re4::port::shelf {'];records=[]
    for room,param in [('103','SHELF_SMD* s'),('106','int type')]:
        path=f'src/st1/r{room}.cpp';src=(root/path).read_text(encoding='utf-8')
        sig=f'void r{room}_openShelf_main({param}, int opened)'
        body,line=function(src,sig)
        changes=[(sig,sig.replace('void r','void Domain::r')),
                 ('register cModel* pa2 asm("r10");','cModel* pa2;'),
                 ('            asm("" : "=m"('+('a' if room=='103' else 'b')+'->be_flag) : "r"(pa2));','')]
        for before,after in changes:
            if body.count(before)!=1:raise ValueError('Shelf adaptation not unique: '+before)
            body=body.replace(before,after)
        code += [f'#line {line} "{(root/path).as_posix()}"',body]
        records.append(dict(source=path,signature=sig,line=line,adaptations=[a for a,b in changes]))
    src=(root/'src/st1/r103.cpp').read_text(encoding='utf-8')
    for n in range(3):code.append(re.search(r'static SHELF_SMD r103_shelf'+str(n)+r' = .*?;',src)[0])
    # Registration identities come from the literal original lookup statements.
    src=(root/'src/st1/r106.cpp').read_text(encoding='utf-8')
    body,_=function(src,'void r106_openShelf_main(int type, int opened)')
    pairs=re.findall(r'case ([01]):\s*a = SmdGetObjPtr\((0x[0-9A-Fa-f]+)\);\s*b = SmdGetObjPtr\((0x[0-9A-Fa-f]+)\);',body)
    if len(pairs)!=2:raise ValueError('Original r106 shelf identities')
    code += ['SHELF_SMD originalShelf(u16 room,u32 selection) {',
             'if(room==0x103){switch(selection){case 0:return r103_shelf0;case 1:return r103_shelf1;case 2:return r103_shelf2;}}',
             'if(room==0x106){switch(selection){']
    code += [f'case {n}:return {{{{{a},{b}}}}};' for n,a,b in pairs]
    code += ['}}throw std::invalid_argument("unregistered original shelf callback selection");','}','}']
    out.mkdir(parents=True,exist_ok=True)
    (out/'shelf_control.cpp').write_text('\n\n'.join(code)+'\n',encoding='utf-8')
    (out/'shelf-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=records,
        scope='Original callbacks; native SMD/model owner, real SceSleep; isolated sound request capture.'),indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
