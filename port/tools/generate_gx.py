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
    import re
    state=re.search(r'class cModelState \{.*?\n\};',(root/'include/model.h').read_text(),re.S)[0]
    (output/'include').mkdir(parents=True,exist_ok=True)
    (output/'include/gx_state_types.h').write_text('#pragma once\nnamespace re4::port::gx {\n'+state+'\n}\n')
    render=['#include "gx_render_original.h"','namespace re4::port::gx {']
    render_records=[]
    signatures=[
        ('src/game/camera.cpp','void CameraGetLookVecInverse(CAMERA* pCam, Vec* look_inv)',[]),
        ('src/game/trans_ot.cpp','static inline OtWork* otWork(int type)',[]),
        ('src/game/trans_ot.cpp','void InitOt()',[]),
        ('src/game/trans_ot.cpp','void ClearOt()',[]),
        ('src/game/trans_ot.cpp','int AddOtModelPosRadius(void* data, void (*func)(void*), Vec* pos, f32 radius, u16 kind, f32 zlimit)', [
            ('(u16) (z * 0.01f)','depthBucket(z * 0.01f)')]),
        ('src/game/trans_ot.cpp','int AddOtWorldPosRadius(void* data, void (*func)(void*), Vec* pos, f32 radius, u16 kind, f32 zlimit)', [
            ('(u16) (z * OT_MUL)','depthBucket(z * OT_MUL)')]),
        ('src/game/trans_ot.cpp','void clearOtWork(OtWork* w)',[
            ('p--;','p = q > w->list ? p - 1 : nullptr;')]),
        ('src/game/trans_ot.cpp','OtData* MakeOtData(void* data)',[
            ('(u32) p < 0x80000000 || (u32) p > 0x82FFFFFF','p == nullptr')]),
        ('src/game/trans_ot.cpp','int AddOtDirect(int ot, void* data, void (*func)(), u32 no, u16 flag, Vec* pos, f32 radius)',[
            ('void (*func)()','void (*func)(void*)'),
            ('(void (*)(void*)) func','func')]),
        ('src/game/trans_ot.cpp','int ExecOt(int type)',[]),
        ('src/game/trans_ot.cpp','u16 OtGetPrevKind()',[]),
        ('src/game/trans_ot.cpp','void CrearOtMirrorWork()',[]),
        ('src/game/trans_ot.cpp','void DeleteOtData(u32 type, u32 no)',[
            ('q->next = q->next->next;','if (!q->next || !q->next->data) throw std::invalid_argument("OT deletion without entry");\n    q->next = q->next->next;')]),
    ]
    source=(root/'src/game/trans_ot.cpp').read_text()
    begin=source.index('static int Ot_max_tbl[');end=source.index('};',begin)+2
    render.append(source[begin:end])
    for name in ['getTexCoord','getTexMtx','getTexMap','getKColor','getKColorSel','getKAlphaSel',
                 'getTevReg','checkTevReg','getTevRegNext','getTevRegC','getTevRegA','getTevRegCur','getTevRegCurC']:
        source=(root/'src/game/trans.cpp').read_text()
        sig=next(line for line in source.splitlines() if line.startswith('static inline ') and name+'(' in line)
        signatures.append(('src/game/trans.cpp',sig,[]))
    signatures += [
        ('src/game/trans.cpp','static inline void loadBlendTex(cPolyHeader* part, u8* tbl, int map)',[]),
        ('src/game/trans.cpp','void TextureBlend(cPolyHeader* part, cModelInfo* info, int colIn, int alphaIn)',[]),
        ('src/game/trans.cpp','void TextureBlend2(cPolyHeader* part, cModelInfo* info, int colIn, int alphaIn)',[]),
        ('src/game/trans.cpp','void TextureBlend3(cPolyHeader* part, cModelInfo* info, int colIn, int alphaIn)',[]),
        ('src/game/trans.cpp','void materialSetup(cPolyHeader* part, cModelInfo* info, int colIn, int alphaIn)',[]),
        ('src/game/trans.cpp','void alphaSetup(cModel* m, cPolyHeader* part, cModelInfo* info, int thermo)',[]),
        ('src/game/trans_lit.cpp','void LightSetInit()',[]),
        ('src/game/trans_lit.cpp','void LightSetModel(cModel* pMod)',[
            ('pG->Camera.v_mat','pG->Camera.view')]),
        ('src/game/trans_lit.cpp','void lightSetAmbient(GXColor* col0)',[]),
        ('src/game/trans_lit.cpp','void LightDisable()',[]),
        ('src/game/model.cpp','inline cModelState::cModelState()', [('inline cModelState::','cModelState::')]),
        ('src/game/lightInfo.cpp','cLightInfo::cLightInfo()',[]),
        ('src/game/lightInfo.cpp','int cLightInfo::init2(int type, int partsNo, const Vec* pOffset, const Vec* pSize, int mask)',[]),
        ('src/game/lightInfo.cpp','void cLightInfo::updateMatrix(cModel* pMod)',[]),
        ('src/game/lightInfo.cpp','cCoord* cLightInfo::getPos(cModel* m, Vec* out)',[]),
        ('src/game/light.cpp','int cLightMgr::checkKind(u8 kind)', [('cLightMgr::','LightEnv::'),('1 << (kind & 31)','1u << (kind & 31)')]),
        ('src/game/light.cpp','void cLightMgr::setModel2(cModel* pMod)', [
            ('cLightMgr::','LightEnv::'),('cModel* parent;','ModelCpu* parent;'),
            ('1 << i','1u << i'),('(*(u32*) &l->DispCol & 0xFFFFFF00) == 0','!(l->DispCol.r || l->DispCol.g || l->DispCol.b)')]),
    ]
    for sig in ['int lightHitCheck(cModel* pMod, cLight* pLight)',
                'int lightHitCheckSphere(cModel* pMod, cLight* pLight)',
                'int lightHitCheckCylinder(cModel* pMod, cLight* pLight)',
                'int lightHitCheckBBox(cModel* pMod, cLight* pLight)']:
        signatures.append(('src/game/light.cpp',sig,[]))
    for sig in ['void lightSetConstant(cLight* pLi, GXLightObj* pLo)',
                'void lightSetLinear(cLight* pLi, GXLightObj* pLo)',
                'static void lightSetQuadratic(cLight* pLi, GXLightObj* pLo)',
                'void lightSetSpotlight(cLight* pLi, GXLightObj* pLo)',
                'void lightSetCustom(cLight* pLi, GXLightObj* pLo)',
                'void lightSetParallel(cLight* pLi, GXLightObj* pLo)',
                'void lightSetSpotQuad(cLight* pLi, GXLightObj* pLo)',
                'void lightSetLocalAmb(cLight* pLi, GXColor* lamb)',
                'void lightSetColor(GXLightObj* lobj, cLight* pLi, cEm* pMod)']:
        changes=[]
        if sig.startswith('static '):changes.append(('static void ','void '))
        body,_=function((root/'src/game/trans_lit.cpp').read_text(),sig)
        if 'pG->Camera.v_mat' in body:
            # Multiple uses in a complete directional callback, all the same field adaptation.
            changes.append(('pG->Camera.v_mat','pG->Camera.view'))
        signatures.append(('src/game/trans_lit.cpp',sig,changes))
    for sig in ['void GXInitLightSpot(GXLightObj* lt_obj, f32 cutoff, GXSpotFn spot_func)',
                'void GXInitLightDistAttn(GXLightObj* lt_obj, f32 ref_dist, f32 ref_br, GXDistAttnFn dist_func)',
                'void GXInitLightAttnK(GXLightObj* lt_obj, f32 k0, f32 k1, f32 k2)']:
        signatures.append(('src/lib/GXLight.c',sig,[]))
    signatures += [
        ('src/lib/mtxvec.c','void C_MTXMultVecSR(const Mtx m, const Vec* src, Vec* dst)',[]),
        ('src/lib/vec.c','void C_VECScale(const Vec* src, Vec* dst, f32 scale)',[]),
        ('src/lib/vec.c','f32 C_VECDotProduct(const Vec* a, const Vec* b)',[]),
        ('src/game/sub2.cpp','f32 GetDistance3(Vec* v0, Vec* v1)',[]),
        ('src/game/sub2.cpp','f32 RootSumSquare3(Vec* v)',[]),
        ('src/game/sub2.cpp','void RotVector(Vec* vec0, Vec* ang, Vec* vec_ans)',[]),
        ('src/game/math_sub.cpp','void low_RotMatrix(Mtx m, Vec* vec)',[]),
    ]
    for path,sig,changes in signatures:
        body,line=function((root/path).read_text().replace(sig+' {',sig+'\n{'),sig)
        for a,b in changes:
            if body.count(a)<1 or (a!='pG->Camera.v_mat' and body.count(a)!=1): raise ValueError('GX render adaptation changed: '+a)
            body=body.replace(a,b)
        render += [f'#line {line} "{(root/path).as_posix()}"',body]
        render_records.append(dict(source=path,signature=sig,adaptations=[a for a,b in changes]))
    # Preserve shaderSetup's complete final scale/registration segment. The
    # preceding shadow/specular/indirect branches are explicit unsupported paths.
    shader,line=function((root/'src/game/trans.cpp').read_text(),'void shaderSetup(cModel* m, cModelInfo* info, cPolyHeader* part, Mtx mv)')
    marker='    st = TEV_STAGE_ID();\n    GXSetTevOrder(st, 0xFF, 0xFF, 4);'
    if shader.count(marker)!=1:raise ValueError('original shaderSetup final segment changed')
    offset=shader.index(marker)
    render.append(f'#line {line+shader[:offset].count(chr(10))} "{(root/"src/game/trans.cpp").as_posix()}"\nvoid finishModelScale(cModel* m)\n{{\n    int st;\n    int scale;\n'+shader[offset:])
    render_records.append(dict(source='src/game/trans.cpp',signature='shaderSetup: final scale/registration segment',adaptations=['native standalone segment, not full shaderSetup']))
    # No guessed priority list: preserve the actual ExecOt statements in Render.
    source,line=function((root/'src/game/trans.cpp').read_text(),'void Render()')
    import re
    calls=re.findall(r'^    ExecOt\(([^)]+)\);',source,re.M)
    enum=(root/'include/trans_ot.h').read_text()
    names=re.search(r'enum OT_TYPE\s*{(.*?)};',enum,re.S).group(1)
    values={};value=0
    for name in re.findall(r'\b(OT_TYPE_\w+)\s*(?:=\s*(0x[0-9A-Fa-f]+|\d+))?\s*[,\n]',names):
        if name[1]:value=int(name[1],0)
        values[name[0]]=value;value+=1
    resolved=[values[c] if c in values else int(c,0) for c in calls]
    render.append('const std::array<int,22> originalRenderOrder={'+','.join(map(str,resolved))+'};')
    render.append('}')
    (output/'gx_render_control.cpp').write_text('\n\n'.join(render)+'\n')
    (output/'gx-render-manifest.json').write_text(json.dumps(dict(functions=render_records,
        render_order=resolved,scope='original complete OT/material/light callback and selection bodies; shaderSetup final segment and Render dispatch slice, neither complete shaderSetup nor complete Render'),indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
