"""Embed compiler output only; no original assets in generated headers."""
import struct
import sys
from pathlib import Path
target=Path(sys.argv[1])
lines=['#pragma once','#include <cstdint>','namespace re4::port {']
for name,path in zip(['gxVertexSpirv','gxFragmentSpirv'],sys.argv[2:]):
    blob=Path(path).read_bytes()
    if len(blob)%4 or struct.unpack_from('<I',blob)[0]!=0x07230203: raise ValueError('SPIR-V header')
    lines.append('inline constexpr std::uint32_t '+name+'[]={'+','.join(hex(v) for v in struct.unpack('<'+'I'*(len(blob)//4),blob))+'};')
lines.append('}')
target.write_text('\n'.join(lines)+'\n')
