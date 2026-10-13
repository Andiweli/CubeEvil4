"""Extract actual CPU-only TexRender bodies, no rendering-success substitutions."""
import argparse
import json
from pathlib import Path
from generate_runtime import digest,function

def generate(root,output):
    hashes=json.loads((root/'port/config/tex-render-sources.json').read_text())
    for path,expected in hashes.items():
        if digest(root/path)!=expected: raise ValueError('TexRender fingerprint mismatch: '+path)
    output.mkdir(parents=True,exist_ok=True)
    cpp=['#include "consumer_original.h"','#include "re4/port/tex_render.h"','namespace re4::port {','using TexRenderMng = TexRenderTarget;']
    entries=[]
    for sig in ['void TexRenderMgrInit()','void TexRenderMgrRoomInit()',
                'int GetTexRenderMgr(TexRenderMng** ppMgr)','void TexRenderMng::Init()',
                'int TexRenderMng::AllocBuf()','void TexRenderMng::ReAllocBuf()',
                'void TexRenderInit(TexRenderMng** ppMgr, int size, int repType)']:
        body,line=function((root/'src/game/TexRender.cpp').read_text(),sig)
        native=sig.replace('TexRenderMng','TexRenderTarget')
        if '::' not in native:
            native=native.replace('TexRenderMgrInit','TexRenderRuntime::TexRenderMgrInit').replace('TexRenderMgrRoomInit','TexRenderRuntime::TexRenderMgrRoomInit').replace('GetTexRenderMgr','TexRenderRuntime::GetTexRenderMgr').replace('void TexRenderInit','void TexRenderRuntime::TexRenderInit')
        changes=[(sig,native)]
        if 'AllocBuf()' in sig and 'ReAlloc' not in sig:
            changes += [('MEM_ALLOC(GetTexBufSize(), 1, 13)','owner->allocate(this,GetTexBufSize())')]
        if 'ReAlloc' in sig: changes += [('Mem_free(m_Texture_buffer);','owner->release(m_Texture_buffer);\n        m_Texture_buffer=nullptr; // Defined native failure cleanup.')]
        for a,b in changes:
            if body.count(a)!=1: raise ValueError('TexRender adaptation not unique: '+a)
            body=body.replace(a,b)
        cpp += [f'#line {line} "{(root/"src/game/TexRender.cpp").as_posix()}"',body]
        entries.append(dict(source='src/game/TexRender.cpp',signature=sig,line=line,adaptations=[a for a,_ in changes]))
    cpp += ['} // namespace re4::port']
    (output/'tex_render_control.cpp').write_text('\n\n'.join(cpp)+'\n')
    (output/'tex-render-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries),indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();generate(a.root,a.output)
