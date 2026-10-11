#!/usr/bin/env python3
"""Checked original SAT/EAT segment and floor-query bodies; native storage only."""
import argparse
import json
import re
from generate_runtime import digest, function
from pathlib import Path

def generate(root, output):
    hashes=json.loads((root/'port/config/collision-sources.json').read_text())
    for path, expected in hashes.items():
        if digest(root/path)!=expected:raise ValueError('Collision fingerprint mismatch: '+path)
    output.mkdir(parents=True,exist_ok=True)
    (output/'include').mkdir(exist_ok=True)
    header=(root/'include/at_sub.h').read_text()
    types=['#pragma once','#include "re4_runtime_types.h"','namespace re4::port::collision {']
    for name in ['SAT_TYPE','SAT_ATTR','EAT_ATTR']:
        types.append(re.search(r'enum '+name+r' \{.*?\n\};',header,re.S)[0])
    types.append(re.search(r'struct SAT_POLY \{.*?\n\};',header,re.S)[0])
    types+=['static_assert(sizeof(SAT_POLY)==20);','}']
    (output/'include/re4_collision_types.h').write_text('\n'.join(types)+'\n')
    code=['#include "collision_original.h"','namespace re4::port::collision {']
    entries=[]
    functions=[
        ('src/lib/vec.c','f32 C_VECDotProduct(const Vec* a, const Vec* b)',[]),
        ('src/game/sub2.cpp','f32 GetDistance(Vec* v0, Vec* v1)',[]),
        ('src/game/at_sub.cpp','u32 Get_poly_attr(SAT_POLY* poly)',[
            ('return (((u16*) poly)[8] << 16) | ((u16*) poly)[9];','return poly->m_Status; // Native endian scalar, not BE halfword overlay.')]),
        ('src/game/at_sub.cpp','void InterVectorXYZ(Vec* cross, Vec* a, Vec* b, f32 rate)',[]),
        ('src/lib/vec.c','f32 C_VECSquareDistance(const Vec* a, const Vec* b)',[]),
        ('src/game/at_sub.cpp','f32 At_surface_point_rel(Vec* a, Vec* norm, Vec* point)',[]),
        ('src/game/at_sub.cpp','int At_surface_line_ck(Vec* cross, Vec* a, Vec* norm, Vec* point1, Vec* point2)',[]),
        ('src/game/at_sub.cpp','int At_poly_point_rel(Vec* poly, Vec* nrm, Vec* p)',[]),
        ('src/game/at_sub.cpp','u32 At_poly_sphere_ck2(Vec* tri, Vec* n, u32 attr, Vec* oldPos, Vec* pos, f32 r, u32 flag, u32 mask)',[
            ('eprintf(80, 160, 0, 0, "HIT ERROR %d", hit);','throw std::runtime_error("original sphere HIT ERROR");')]),
        ('src/game/at_sub.cpp','u32 At_poly_sphere_ck(cSat* atp, SAT_POLY* poly, Vec* pos0, Vec* pos1, f32 r, u32 flag, u32 mask)',[]),
        ('src/game/atari.cpp','void polyBitSet(u32 pno)',[]),
        ('src/game/atari.cpp','int polyBitCk(u32 pno)',[]),
        ('src/game/atari.cpp','int blkPolySphereCkCore(cSat* pAt, cSatBlock* pBlock, Vec* pos0, Vec* pos1, f32 radius, int flag, Vec* pNorm, int mask)',[
            ('pAt->disp(*idx, 0x40FF0000, 1);','throw MissingGameConsumer("SAT debug polygon rendering");')]),
        ('src/game/atari.cpp','int blkPolySphereCk(cSat* pAt, cSatBlock* pBlock, Vec* pos0, Vec* pos1, f32 radius, int flag, Vec* pNorm, int mask)',[
            ('(cSatBlock*) pBlock->idx','pBlock->children')]),
        ('src/game/atari.cpp','int cSatMgr::polySphereCk(Vec* pos0, Vec* pos1, f32 radius, int flag, Vec* pNorm, int mask)',[]),
        ('src/game/at_sub.cpp','u32 At_poly_line_ck(cSat* atp, Vec* cross, SAT_POLY* poly, Vec* vert0, Vec* vert1, u32 flag, u32 mask)',[]),
        ('src/game/atari.cpp','int cSatBlock::lineOverlap(Vec* center, Vec* w, Vec* v)',[]),
        ('src/game/atari.cpp','static inline int lineCross(Vec* a, Vec* b, Vec* c, Vec* d)',[]),
        ('src/game/atari.cpp','int cSatBlock::hitCheckSphere(Vec* pos0, Vec* pos1, f32 radius)',[]),
        ('src/game/atari.cpp','int blkPolyLineCkCore(cSat* pAt, cSatBlock* pBlock, Vec* pos0, Vec* pos1, int flag, int mask, Vec* pCross, Vec** ppNorm)',[
            ('    idx--;','    // Avoid forming a pointer before the native array.'),
            ('        idx++;\n        no = *idx;','        no = *idx++;'),
            ('pAt->poly_p[*idx].m_Normal','pAt->poly_p[no].m_Normal')]),
        ('src/game/atari.cpp','int blkPolyLineCk(cSat* pAt, cSatBlock* pBlock, Vec* pos0, Vec* pos1, int flag, int mask, Vec* pCross, Vec** ppNorm)',[
            ('(cSatBlock*) pBlock->idx','pBlock->children')]),
        ('src/game/atari.cpp','int cSatMgr::hitCheck2(Vec* pos0, Vec* pos1, Vec* pCross, Vec** ppNorm, int flag, int mask)',[
            ('    Vec* pn;','    Vec* pn = nullptr;'),
            ('((SEckView*) &SEck)->v = type;','SEck = type; // Thread-local native query context.')]),
        ('src/game/atari.cpp','int cSatMgr::hitCheck(Vec* pos0, Vec* pos1, Vec* pCross, Vec* pNorm, int flag, int mask)',[]),
        ('src/game/atari.cpp','f32 cSatMgr::getFloor(Vec* pos, Vec** ppNorm, f32 above_limit, f32 below_limit, int mask)',[]),
        ('src/game/atari.cpp','void cSat::setCoord(Vec* pos, Vec* ang)',[]),
        ('src/game/atari.cpp','void cSat::setMatrix(Mtx mat0)',[]),
    ]
    for path,sig,changes in functions:
        try:body,line=function((root/path).read_text().replace(sig+' {',sig+'\n{'),sig)
        except ValueError as e:raise ValueError('Missing original collision body: '+path+' '+sig) from e
        for a,b in changes:
            if a not in body:raise ValueError('Collision adaptation missing: '+a)
            body=body.replace(a,b)
        if sig.startswith('int cSatMgr::polySphereCk('):
            # The two PPC counter blocks observe profiling only. Fail explicitly
            # in that debug mode; never emulate nonexistent hardware counters.
            pattern=r'    if \(pG->debug_mode == 0x14\) \{.*?\n    \}'
            body,count=re.subn(pattern,'    if (pG->debug_mode == 0x14) throw MissingGameConsumer("SAT PPC performance counters");',body,flags=re.S)
            if count!=2:raise ValueError('SAT profiling blocks changed')
            body=body.replace('    u32 idx = 0;','')
            changes=changes+[('two PPC profiling-only blocks','explicit unsupported debug profiling mode')]
        code += [f'#line {line} "{(root/path).as_posix()}"',body]
        entries.append(dict(source=path,signature=sig,line=line,adaptations=[a for a,b in changes]))
    code.append('}')
    (output/'collision_control.cpp').write_text('\n\n'.join(code)+'\n')
    (output/'collision-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries),indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
