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
    cpp=['#include "motion_original.h"','#include "re4/port/graphics.h"','namespace re4::port {']
    entries=[]
    def add(path, signature, changes):
        source=(root/path).read_text(encoding='utf-8')
        if path.startswith('include/'):
            source=re.sub(r'^    ', '', source, flags=re.M)
        source=source.replace(signature+' {',signature+'\n{')
        body,line=function(source,signature)
        for before, after in changes:
            count=2 if signature=='void InverseKinematics(cModel* pEm, int arm_flag)' and before=='SatMgr.getFloor(&target, 0, 500.0f, 100000.0f, 0)' else 1
            if body.count(before)!=count: raise ValueError('consumer adaptation occurrence count changed: '+before)
            body=body.replace(before,after)
        if path=='src/game/event.cpp':
            body=body.replace('pLog->err(', 'eventLog.err(').replace('pLog->warn(', 'eventLog.warn(')
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',body])
        adaptations=[a for a,_ in changes]
        if path=='src/game/event.cpp' and ('pLog->err(' in function(source,signature)[0] or 'pLog->warn(' in function(source,signature)[0]):
            adaptations.append('nonthrowing bounded event diagnostic log; preserve original return/error paths')
        entries.append(dict(source=path,signature=signature,line=line,adaptations=adaptations))
    for sig in ['void cModel::setPartsParent()', 'inline void cModel::setNoSuspend(int onoff)',
                'inline int cModel::isTrans()', 'void cModel::updateOldPos()',
                'inline cParts* cModel::getPartsPtr(int idx)']:
        add('src/game/model.cpp',sig,[(sig,sig.replace('inline ','').replace('cModel::','ModelCpu::').replace('cParts*','ModelPart*'))])
    add('include/model.h','cCoord() : cUnit(1)', [('cCoord() : cUnit(1)','ModelCoord::ModelCoord()')])
    add('src/game/model.cpp','cModel::cModel()', [('cModel::cModel()','ModelCpu::ModelCpu()'),
        ('memclr_asm(&Motion, 0xD0);','Motion = {}; // Native pointer fields, never a 0xD0 byte overlay.')])
    add('src/game/model.cpp','cModelInfo::cModelInfo() : cUnit(1)',
        [('cModelInfo::cModelInfo() : cUnit(1)','ModelInfoView::ModelInfoView()')])
    add('src/game/model.cpp','int cModel::modelInit(void* bin, void* tpl)',[
        ('int cModel::modelInit','ModelInfoView* ModelCpu::modelInit'),
        ('SysFlagChk(pG, SYS_INVISIBLE)','systemInvisible'),('return (int) info;','return info;')])
    for sig in ['int cModel::initJoint(void* bin)','void cModel::releaseJoint()',
                'void cModel::setPartsOffset(void* bin)','void cModel::partsMatCalc()',
                'void cModel::partsWorldCalc()','void cModel::addModel(cModelInfo* pInfo)',
                'void cModel::releaseModelInfo()','int cModel::makePartsList(int n)',
                'void cModel::setJointInfo(void* pHead)','void cModel::releasePartsList(int idx)']:
        changes=[(sig,sig.replace('cModel::','ModelCpu::'))]
        if 'setJointInfo' in sig:
            changes.append(('(u16*) (d->flipTbl + 4)','(u16*) (d->flipTbl + 4)'))
        add('src/game/model.cpp',sig,changes)
    add('src/game/model.cpp','int cModel::deleteModelInfo(cModelInfo* addr)',[('cModel::deleteModelInfo','ModelCpu::deleteModelInfo')])
    add('include/model.h','void matCalc()', [('void matCalc()','void ModelCoord::matCalc()')])
    # Complete original blend body; only the matching compiler anchors are removed.
    body,line=function((root/'src/game/model.cpp').read_text(),'void cModel::matBlend(f32 rate)')
    body=body.replace('cModel::matBlend','ModelCpu::matBlend')
    body=re.sub(r'\basm(?: volatile)?\(""[^;]*;','',body)
    body=re.sub(r'\bregister ', '',body);body=re.sub(r' asm\("r\d+"\)','',body)
    cpp.extend([f'#line {line} "{(root/"src/game/model.cpp").as_posix()}"',body])
    entries.append(dict(source='src/game/model.cpp',signature='void cModel::matBlend(f32 rate)',line=line,adaptations=['native class name; remove codeless compiler anchors']))
    add('src/game/model.cpp','cModelInfo* cModInfoMgr::create(void* bin, void* tpl)',[
        ('cModelInfo* cModInfoMgr::create','ModelInfoView* ModelInfoManager::create'),
        ('cManager<cModelInfo>::create()','ModelPool<ModelInfoView>::create()')])
    add('src/game/model.cpp','cParts* cPartsMgr::createSequential(u32 n)',[
        ('cParts* cPartsMgr::createSequential','ModelPart* ModelPartsManager::createSequential'),
        ('    u32 i;','    if (!n || n >= nArray) return nullptr; // Bound original unsigned subtraction.\n    u32 i;')])
    add('src/game/model.cpp','static inline cParts* PartsMgrWork(cPartsMgr* m, u32 no)',[
        ('static inline cParts* PartsMgrWork(cPartsMgr* m','ModelPart* PartsMgrWork(ModelPartsManager* m')])
    add('src/game/model.cpp','void getBoundingBox(cModelData* d, cBoundingBox* pBox)',[])
    for sig in ['void ScaleMatrix(Mtx m, Vec* vec)','void TransMatrix(Mtx m, Vec* pos)',
                'void RotMatrix(Mtx m, Vec* vec)']:
        add('src/game/math_sub.cpp',sig,[])
    for sig in ['void C_MTXIdentity(Mtx m)','void C_MTXCopy(const Mtx src, Mtx dst)',
                'void C_MTXConcat(const Mtx a, const Mtx b, Mtx ab)',
                'void C_MTXTrans(Mtx m, f32 xT, f32 yT, f32 zT)',
                'void C_MTXScale(Mtx m, f32 xS, f32 yS, f32 zS)',
                'void C_MTXRotRad(Mtx m, char axis, f32 rad)',
                'void C_MTXRotTrig(Mtx m, char axis, f32 sinA, f32 cosA)']:
        add('src/lib/mtx.c',sig,[])
    add('src/lib/mtxvec.c','void C_MTXMultVec(const Mtx m, const Vec* src, Vec* dst)',[])
    for sig in ['void C_MTXTranspose(const Mtx src, Mtx xPose)', 'u32 C_MTXInverse(const Mtx src, Mtx inv)']:
        add('src/lib/mtx.c',sig,[])
    for sig in ['void C_VECSubtract(const Vec* a, const Vec* b, Vec* a_b)',
                'void C_VECNormalize(const Vec* src, Vec* unit)',
                'void C_VECCrossProduct(const Vec* a, const Vec* b, Vec* axb)', 'f32 C_VECMag(const Vec* v)',
                'f32 C_VECSquareMag(const Vec* v)']:
        add('src/lib/vec.c',sig,[])
    for sig in ['void C_VECScale(const Vec* src, Vec* dst, f32 scale)']:
        add('src/lib/vec.c',sig,[('C_VECScale','motionVecScale')])
    add('src/lib/mtxvec.c','void C_MTXMultVecSR(const Mtx m, const Vec* src, Vec* dst)', [('C_MTXMultVecSR','motionMultVecSR')])
    add('src/lib/mtx.c','void C_MTXQuat(Mtx m, const Quaternion* q)',[])
    for sig in ['void C_QUATMtx(Quaternion* r, const Mtx m)','void C_QUATSlerp(const Quaternion* p, const Quaternion* q, Quaternion* r, f32 t)']:
        add('src/lib/quat.c',sig,[])
    for sig in ['void SetOrientationZY(Vec* z, Vec* y, Mtx m)', 'void VecRadLimit(f32* p)',
                'f32 hermite(f32* x, f32* v, f32 t)']:
        add('src/game/math_sub.cpp',sig,[])
    add('src/game/ik.cpp','void IKInit(cModel* pEm, MOTION_INFO* pInfo)',[])
    for sig in ['void SetOrientationZX(Vec* z, Vec* x, Mtx m)','f32 VecAngle(Vec* vec_a, Vec* vec_b)']:
        add('src/game/math_sub.cpp',sig,[])
    add('src/lib/vec.c','f32 C_VECDotProduct(const Vec* a, const Vec* b)',[('C_VECDotProduct','ikDotProduct')])
    add('src/lib/mtx.c','void C_MTXRotAxisRad(Mtx m, const Vec* axis, f32 rad)',[('C_MTXRotAxisRad','ikRotAxisRad')])
    add('src/game/sub2.cpp','f32 GetDistance3(Vec* v0, Vec* v1)',[('GetDistance3','ikDistance'),('RootSumSquare3(&d)','std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z)')])
    add('src/game/ik.cpp','void ikCalc(cParts* root, cParts* joint, cParts* eff)',[('GetDistance3','ikDistance')])
    add('src/game/ik.cpp','static void heel2toe(Mtx m, cParts* p, Vec* pos)',[])
    ik=(root/'src/game/ik.cpp').read_text()
    cpp.append(ik[ik.index('#define IK_LOCAL'):ik.index('// Per-frame leg IK')])
    add('src/game/ik.cpp','void InverseKinematics(cModel* pEm, int arm_flag)',[
        ('    cEm* em = (cEm*) pEm;','    // Actor status is read through a typed owner, never a cEm byte overlay.'),
        ('em->checkStatus(EM_STATUS_IK_OFF)','motionActorContext(pEm).ikOff'),
        ('em->atari.m_flag','motionActorContext(pEm).atariFlags'),
        ('SatMgr.getFloor(&target, 0, 500.0f, 100000.0f, 0)','motionFloor(&target, 0, 500.0f, 100000.0f, 0)'),
        ('SatMgr.getFloor(&target, 0, 6000.0f, 100000.0f, 0)','motionFloor(&target, 0, 6000.0f, 100000.0f, 0)'),
        ('GetDistance3','ikDistance')])
    for sig in ['void ResetShape(cModelInfo* info, void* dst)','void ClrShape(cModel* pMod)']:
        add('src/game/shape.cpp',sig,[])
    add('src/game/motion.cpp','void MotionClear(cModel* pEm, int flag)',[])
    motion=(root/'src/game/motion.cpp').read_text()
    def add_motion(sig,changes=()):
        body,line=function(motion,sig)
        if sig=='u16 MotionSequenceCtrl(MOTION_INFO* w)':
            if body.count('pG->Speed')!=3:raise ValueError('MotionSequenceCtrl speed anchors')
            body=body.replace('pG->Speed','motionFrameSpeed()')
        for before,after in changes:
            if body.count(before)!=1: raise ValueError('motion adaptation changed: '+before)
            body=body.replace(before,after)
        # Matching-only compiler anchors; these have no gameplay effects.
        body=re.sub(r'\basm(?: volatile)?\(""[^;]*;','',body)
        body=re.sub(r'\bregister (?=int)','',body)
        body=re.sub(r' asm\("r\d+"\)','',body)
        body=re.sub(r'    Fcc(?:F32|S16) c[fs];\n','',body)
        cpp.extend([f'#line {line} "{(root/"src/game/motion.cpp").as_posix()}"',body])
        entries.append(dict(source='src/game/motion.cpp',signature=sig,line=line,
                            adaptations=[a for a,_ in changes]+(['three original pG->Speed reads use the bounded native frame speed'] if sig=='u16 MotionSequenceCtrl(MOTION_INFO* w)' else [])+['remove codeless PPC anchors; BE scalar macros']))
    add_motion('void MotionSetCore(cModel* m, void* w_, void* data_, void* seq_, int hokan, int flags, int frame)',[
        ('DbgFlagChk(pG, DBG_TEST_MODE)','false'),
        ('u16* seq = (u16*) seq_;','MotionSequenceView* seq = (MotionSequenceView*) seq_;'),
        ('(SEQUENCE_DATA*) (seq + 2)','seq->keys.data()'),
        ('seq[0]','seq->count'),
        ('((u8*) seq)[2]','seq->flags'), ('    u32* tbl;','    const u8** tbl;'),
        ('w->pJoint_kind = (u16*) ((u8*) w->pMot + 3);','w->pJoint_kind = data->kinds.data();'),
        ('w->pJoint_no = (u8*) w->pMot + (w->Joint_num * 2 + 3);','w->pJoint_no = data->numbers.data();'),
        ('''    tbl = (u32*) ((u32) w->pJoint_no + w->Joint_num);
    tbl = (u32*) (((u32) tbl + 3) & ~3);
    tbl++;
    if ((s32) tbl[0] >= 0) {
        for (i = 0; i < w->Joint_num; i++) {
            tbl[i] += (u32) w->pMot;
        }
    }''','    tbl = data->streams.data(); // Checked native references; original file remains immutable.'),
        ('    u32 zero = 0;','')])
    add_motion('void MotionMoveCore(cModel* pEm, MOTION_INFO* w, CAMERA* pCamera)', [('pPL == pEm','pEm->kindid == 0 && pEm->id == 0')])
    add_motion('u16 MotionSequenceCtrl(MOTION_INFO* w)')
    add_motion('int MotionCheckCrossFrame(MOTION_INFO* w, f32 frame)')
    add_motion('void MotionGetSpeed(cModel* pEm, MOTION_INFO* w, int flg, Vec* Pos_move, Vec* Ang_move)')
    add_motion('void MotionAddSpeed(cModel* pEm, MOTION_INFO* w, Vec* Pos_move, Vec* Ang_move)')
    add_motion('u32 MotionMove(cModel* pEm, CAMERA* pCamera)', [
        ('*(s32*) pEm->pDblJnt','motionBlendCount(pEm->pDblJnt)')])
    add_motion('int HermiteInterpolation(HERMITE_SET* prm, Vec* out, u16* hist)',[
        ('u16* hp = hist - 1;','u16* hp = hist;'), ('u16* frames;','u8* frames;'),
        ('n = *(u16*) p;','n = motionBE16(p);'), ('frames = (u16*) (p + 2);','frames = p + 2;'),
        ('hp++;','hp = hist + axis;'), ('u16* fp = (u16*) (idx * 2 + (u32) frames);','u8* fp = frames + idx * 2;'),
        ('f0 = (f32) *fp;','f0 = (f32) motionBE16(fp);'), ('f1 = (f32) frames[nx];','f1 = (f32) motionBE16(frames + nx * 2);'),
        ('if (frame < f1) {','if (frame < f1) {\n                        if (prm->Data_fmt == 15) throw MissingGameConsumer("FCC15 between-key interpolation has no original tangent definition");'),
        ('fp--;','fp -= 2;'), ('fp++;','fp += 2;'),
        ('fp = (u16*) (last * 2 + (u32) frames);','fp = frames + last * 2;')])
    for name in ['000','001','002','010','011','012','020','021','022','033']:
        add_motion('void Fcc_get_data_'+name+'(u8* d, int i, int j, f32* x, f32* t)')
    add_motion('void dummy(u8* d, int i, int j, f32* x, f32* t)')
    add_motion('int Fcc_next_axis_addr(int fmt, int n)')
    # Exact original pool loops/list ordering, with defined native C++ object lifetimes.
    for sig in ['T* cManager<T>::create(int id)','T* cManager<T>::create()',
                'T* cManager<T>::create(int id, u32 no)','u32 cManager<T>::countActiveWork()',
                'int deleteList(T* p)','void addListFront(T* p)','inline void cManager<T>::destroy(T* p)']:
        native=sig.replace('inline ','').replace('cManager<T>::','ModelPool<T>::')
        if '::' not in native:
            native=native.replace('deleteList','ModelPool<T>::deleteList').replace('addListFront','ModelPool<T>::addListFront')
        changes=[(sig,'template<class T> '+native)]
        if 'destroy' in sig:
            changes += [('(u32)p < 0x80000000 || (u32)p > 0x82FFFFFF','!owns(p)'),
                        ('delete p;','p->be_flag &= ~0x601u; // Keep a live native slot object; original destructor mask.')]
        add('include/cManager.h',sig,changes)
    cpp += ['template class ModelPool<ModelPart>;','template class ModelPool<ModelInfoView>;']
    event_source=(root/'src/game/event.cpp').read_text()
    begin,_=function(event_source,'void Event::ExeBeginEvt(Event* pEvt, int FlagCommon)')
    player_bins=re.findall(r'EvtMgr.SetBin\("([^"]+)", PL_ARC_PTR\(pG->pPlayer, ([\dxA-F]+)\), 0, 2\);',begin)
    if len(player_bins)!=6: raise ValueError('Original event player registration changed')
    cpp.append('struct PlayerEventBin { const char *name; u32 index; };\n'
               'extern const std::array<PlayerEventBin,6> originalPlayerEventBins = {{'+
               ','.join('{'+json.dumps(n)+','+i+'}' for n,i in player_bins)+'}};')
    for sig in ['int DatTbl::SetDat(const char* nm, void* dat, u8 type, void* dat2, u8 flag, int* wkNo)',
                'int DatTbl::GetDat(void** pDat, u8* pEtc, const char* pName, int* pNoWork)',
                'int DatTbl::ChkDat(const char* pName)', 'int DatTbl::DelDat(const char* pName)',
                'int DatTbl::DelDatWkNo(int noWork)']:
        changes=[(sig,sig.replace('DatTbl::','NativeDatTbl::'))]
        if 'SetDat' in sig:
            changes.append(('pWork[i].Count++;','if (pWork[i].Count == 0xFFFF) throw std::overflow_error("event reference count");\n            pWork[i].Count++;'))
        if 'DelDatWkNo' in sig:
            changes.append(('noWork >= NumDatTbl','noWork < 0 || noWork >= NumDatTbl'))
        add('src/game/event.cpp',sig,changes)
    add('src/game/event.cpp','int EventMgr::SetBin(char* nm, void* data, void* dat2, int flag)',[
        ('int EventMgr::SetBin(char* nm','int EventResources::SetBin(const char* nm'),
        ('(int) data >= 0','!valid(data)')])
    add('src/game/event.cpp','int EventMgr::GetBin(void** pAddr, const char* pName, int flagGet)',[
        ('int EventMgr::GetBin','int EventResources::GetBin'),
        ('strcat(path, pName);','if (strlen(pName) > 47) return 0;\n            strcat(path, pName);')])
    add('src/game/event.cpp','int EventMgr::SetEvd(char* nm, void* data, void* dat2, int flag)',[
        ('int EventMgr::SetEvd(char* nm','int EventResources::SetEvd(const char* nm'),
        ('EvtHeader* hdr = (EvtHeader*) data;','EventPackageView* hdr = (EventPackageView*) data;'),
        ('EvtBinEntry* e;','EventBinView* e;'),('(int) hdr >= 0','!valid(hdr)'),
        ("*(u32*) hdr->tag != 0x6576656E || hdr->tag[4] != 't'",'memcmp(hdr->bytes.data, "event", 5) != 0'),
        ('hdr->nBin','static_cast<int>(hdr->bins.size())'),
        ('e = (EvtBinEntry*) (i * sizeof(EvtBinEntry) + (hdr->binOfs + (u32) hdr));','e = &hdr->bins[static_cast<std::size_t>(i)];'),
        ('SetBin(e->name, (u8*) (e->ofs + (u32) hdr), 0, flag)','SetBin(e->name.c_str(), const_cast<u8*>(e->bytes.data), 0, flag)')])
    add('src/game/event.cpp','int Event::SetMod(char* nm, void* mod, u8 type, void* dat2, u8 flag, int* wkNo)',[
        ('int Event::SetMod(char* nm','int EventCpu::SetMod(const char* nm')])
    add('src/game/event.cpp','int Event::ExePacket_SetPartsSub(char* nm, char* bin, char* tpl, char* oya)',[
        ('int Event::ExePacket_SetPartsSub(char* nm, char* bin, char* tpl, char* oya)',
         'int EventCpu::ExePacket_SetPartsSub(const char* nm, const char* bin, const char* tpl, const char* oya)'),
        ('EvtMgr.GetBin(&b, bin, 0)','manager->eventResources().GetBin(&b, bin, 0)'),
        ('EvtMgr.GetBin(&t, tpl, 0)','manager->eventResources().GetBin(&t, tpl, 0)'),
        ('    if (strcmp(nm, "ev0002")','    bool resetOffsets=false;\n    if (strcmp(nm, "ev0002")'),
        ('m->setPartsOffset(b);','resetOffsets=true; // Apply the same original call after resolving the native BIN view.'),
        ('ModInfoMgr.create(b, t)','manager->createModelInfoFor(*m, bin, tpl, resetOffsets)')])
    # Remove only compiler register-allocation anchors, which emit no game instructions.
    event_source=(root/'src/game/event.cpp').read_text()
    sig='int Event::GetMod(void** mod, char* nm, u8* type, int* wkNo)'
    getmod,line=function(event_source,sig)
    getmod=getmod.replace(sig,sig.replace('Event::','EventCpu::').replace('char* nm','const char* nm'))
    getmod=getmod.replace('DbgFlagChk(pG, DBG_ADA_OMAKE_EV)','manager->debugAdaOverride()')
    getmod=re.sub(r'\s*register int pin asm\("r27"\);[^\n]*\n\s*asm volatile\("" : "=r"\(pin\)\);','',getmod)
    getmod=getmod.replace('    asm("" : "+m"(no));','')
    getmod=getmod.replace('pLog->err(', 'eventLog.err(')
    cpp.extend([f'#line {line} "{(root/"src/game/event.cpp").as_posix()}"',getmod])
    entries.append(dict(source='src/game/event.cpp',signature=sig,line=line,
                        adaptations=['native const name / explicit debug flag / remove codeless PPC register anchors',
                                     'nonthrowing bounded event diagnostic log; preserve original return/error paths']))
    add('src/game/event.cpp','int Event::ExeFunc(int mode, int param)',[
        ('int Event::ExeFunc','int EventCpu::ExeFunc'),('void* fn;','const EventFunction *fn;'),
        ('EvtMgr.GetFunc(&fn, nm)','manager->GetFunc(&fn, nm)'),
        ('((EvtFunc) fn)(this, param);','(*fn)(this, static_cast<u32>(param));')])
    cpp.append('struct MesTblBlock { u32 x0,count,ofs[1]; };')
    add('src/lib/GXFrameBuf.c','void GXSetCopyClear(GXColor clear_clr, u32 clear_z)',[
        ('void GXSetCopyClear','void GraphicsState::setCopyClear'),
        ('CHECK_GXBEGIN(1596, "GXSetCopyClear");','checkOutsidePrimitive();'),
        ('__GXData->bpSentNot = 0;','bpSentNot = 0;')])
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
    messages=generate_messages(root,output)
    (output/'consumer-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries,messages=messages),indent=2)+'\n')


def generate_messages(root,output):
    """Whole original initializers; wire relocation is a checked native view."""
    cpp=['#include "message_original.h"', '#include <cstdio>',
         'namespace re4::port {',
         'using TEXPalette=MessageTpl; using TEXHeader=MessageTexHeader; using TEXDescriptor=MessageTexDescriptor; using CLUTHeader=MessageClutHeader; using FONT_TEX=MessageFontTexture;',
         'void C_MTXScale(Mtx,f32,f32,f32);\n#define PSMTXScale C_MTXScale\n#define CORE_ARC(n) coreArc(n)\n#define CORE_DATA_MAX 0x234000',
         (root/'include/arc/core.h').read_text(encoding='utf-8')]
    entries=[]
    def add(path,signature,changes=()):
        source=(root/path).read_text(encoding='utf-8')
        if path.startswith('include/'): source=re.sub(r'^    ','',source,flags=re.M)
        source=source.replace(signature+' {',signature+'\n{')
        body,line=function(source,signature)
        for before,after in changes:
            if body.count(before)!=1: raise ValueError('message adaptation no longer unique: '+before)
            body=body.replace(before,after)
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',body])
        entries.append(dict(source=path,signature=signature,line=line,adaptations=[a for a,_ in changes]))
    for sig in ['void MessageControl::init()', 'void MessageControl::gameInit()',
                'void MessageControl::loadCommonFont()', 'void MessageControl::loadSystemFont()',
                'int MessageControl::loadFont(int w, int h, const char* name, int no)',
                'void MessageControl::releaseFont(int no)', 'void MessageControl::setLanguage(int lang)',
                'void MessageControl::setLayout(int no, int type)',
                'void MessageControl::setFontSize(int no, s16 font_w, s16 font_h)',
                'void MessageControl::setState(u32 b)', 'void MessageControl::unsetState(u32 b)',
                'int MessageControl::checkState(u32 b)', 'void MessageControl::Delete(int no)']:
        changes=[(sig,sig.replace('MessageControl::','MessageControlCpu::'))]
        if 'setLayout' in sig: changes.append(('static s8* p_layout;', 's8* p_layout; // Invocation-local cursor; original lookup unchanged.'))
        if 'gameInit' in sig: changes.append(('    MesData.registData(0,', '    trace.push_back("MessageControl::gameInit:original");\n    MesData.registData(0,'))
        add('src/game/mes.cpp',sig,changes)
    sig='void MessageControl::setupFont(int char_w, int char_h, u8* addr, int no)'
    add('src/game/mes.cpp',sig,[(sig,sig.replace('MessageControl::','MessageControlCpu::')),
        ('MesFont[no].create(char_w, char_h, (TEXPalette*) (addr + *(u32*) addr), addr + *(u32*) (addr + 4));',
         'setupFontView(char_w, char_h, addr, no); // BE FNT offsets, owned native TPL/CLUT view, then original MessageFont::create.')])
    sig='void MessageFont::create(int char_w, int char_h, TEXPalette* addr, u8* size)'
    src=(root/'src/game/mes.cpp').read_text(encoding='utf-8')
    relocation=re.search(r'    if \(\(s32\) addr->descriptorArray >= 0\) \{.*?\n    \}',function(src,sig)[0],re.S)[0]
    add('src/game/mes.cpp',sig,[(sig,sig.replace('MessageFont::','MessageFontCpu::')),
        (relocation,'    requireResolvedPalette(addr); // Native relocation verified before executing the original sheet loop.')])
    for sig in ['void MessageFont::destroy()', 's16 MessageFont::getSize(s16 mes, s8* L, s8* R)']:
        changes=[(sig,sig.replace('MessageFont::','MessageFontCpu::'))]
        if 'destroy' in sig: changes.append(('calcTplOffset(m_tpl);','releasePalette(); // Immutable wire offsets never changed; release this view only.'))
        add('src/game/mes.cpp',sig,changes)
    for sig in ['void setLanguage(u32 lang)', 'u32 getLanguage()', 'void registData(int type, u8* p)']:
        add('include/mes.h',sig,[(sig,sig.replace(' ', ' MessageIndexCpu::',1))])
    for sig in ['void setDie()', 'void setFontSize(s16 w, s16 h)', 'void setLineGap(u16 gap)',
                'void setFontGap(u16 gap)', 'void registQueue(MSG_QUE* q)']:
        out=sig.replace('void ','void MessageSlotCpu::').replace('MSG_QUE','MessageGlyph')
        add('include/mes.h',sig,[(sig,out)])
    for sig in ['void Clear()', 'void MesRegistQueue(int no, MSG_QUE* q)',
                'void MesReleaseQueue(int no)', 'void setLineGap(int no, u16 gap)',
                'void setFontGap(int no, u16 gap)']:
        add('include/mes.h',sig,[(sig,sig.replace('void ','void MessageControlCpu::').replace('MSG_QUE','MessageGlyph'))])
    for sig in ['void SpecularInit(TEXPalette* spec, TEXPalette* ind, TEXPalette* ind2, TEXPalette* thermo)',
                'void GlobalIlmTexInit(TEXPalette* tpl)']:
        add('src/game/trans.cpp',sig,[(sig,sig.replace('void ','void MessageControlCpu::')),
            *([('calcTplAddr(spec);','requireMessageTpl(spec);'),('calcTplAddr(ind);','requireMessageTpl(ind);'),
               ('calcTplAddr(ind2);','requireMessageTpl(ind2);'),('CalcTplAddrC8(thermo);','requireMessageTpl(thermo);')]
              if 'Specular' in sig else [('calcTplAddr(tpl);','requireMessageTpl(tpl);')])])
    sig='void CoreDataRead()'
    add('src/game/read.cpp',sig,[(sig,'void MessageControlCpu::CoreDataRead()'),
        ('DVD_READINFO info;','ReadInfo info;'),('pG->pCore = (ArcFile*) CORE_DATA_ADDR;','coreReadBegin(); // Native owner, no 0x80578000 pointer.'),
        ('req = DVD_READ(3, CORE_DATA_ADDR, 0, 0, 0, 0x8001);','req = readCore(3, 0x8001);')])
    cpp.append('} // namespace re4::port')
    (output/'message_control.cpp').write_text('\n\n'.join(cpp)+'\n',encoding='utf-8')
    return entries


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True); p.add_argument('--output',type=Path,required=True)
    a=p.parse_args(); generate(a.root.resolve(),a.output.resolve())
