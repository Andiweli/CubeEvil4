"""Complete original Player move body and related consumers on one native ActorWork."""
import argparse,json,re
from pathlib import Path
from generate_runtime import digest,function
def generate(root,out):
    hashes=json.loads((root/'port/config/player-live-sources.json').read_text())
    for p,h in hashes.items():
        if digest(root/p)!=h:raise ValueError('Original live player fingerprint mismatch: '+p)
    out.mkdir(parents=True,exist_ok=True);(out/'include').mkdir(exist_ok=True)
    tables=['#pragma once','namespace re4::port {']
    player_source=(root/'src/game/player.cpp').read_text()
    for original,native,count in [('Pl_func_tbl','OriginalPlayerRoutineNames',7),('Pl_func_move_tbl','OriginalPlayerMoveNames',21)]:
        match=re.search(r'void \(\*'+original+r'\[(\d+)\]\)\(cPlayer\*\) = \{(.*?)\n\};',player_source,re.S)
        if not match or int(match[1])!=count:raise ValueError('Original routine table changed: '+original)
        names=[re.findall(r'\b[A-Za-z_]\w*\b',t)[-1] if t.strip()!='0' else '<null>' for t in match[2].split(',') if t.strip()]
        if len(names)!=count:raise ValueError('Original routine table entries: '+original)
        tables.append('inline constexpr const char *'+native+'[] = {'+','.join(json.dumps(n) for n in names)+'};')
    tables.append('}')
    (out/'include/re4_player_live_types.h').write_text('\n'.join(tables)+'\n')
    cpp=['#include "player_live_original.h"','namespace re4::port {'];entries=[]
    fields=['pos','pos_old','ang','mat','be_flag','pList','Motion','pMotionB','pFloor_norm','Shd_color','invisible_factor','id','atari','dmg','r_no_0','r_no_1','r_no_2','r_no_3']
    def add(path,sig,new,changes=(),member=True):
        b,line=function((root/path).read_text(),sig);b=b.replace(sig,new,1)
        for a,z in changes:
            if a not in b:raise ValueError('Live player adaptation absent: '+a)
            b=b.replace(a,z)
        used=[]
        if member:
            for f in fields:
                pattern=r'(?<![\w>.])\b'+f+r'\b'
                if re.search(pattern,b):b=re.sub(pattern,'actorModel->'+f,b);used.append(f)
            for f in fields:b=b.replace('pEm->'+f,'pEm->actorModel->'+f)
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',b]);entries.append(dict(source=path,signature=sig,line=line,adaptations=[sig,*[a for a,_ in changes]],nativeModelFields=used))
    add('src/game/player.cpp','void cPlayer::move()','void PlayerDomain::moveOriginal()',[
        ('Pl_func_tbl[r_no_0](this);','dispatchRoutine0();'),('partsWorldCalc();','actorModel->partsWorldCalc();'),
        ('partsFixAdjust();','actorModel->partsFixAdjust();'),('PartsWorldPosCalc(this);','PartsWorldPosCalc(actorModel.get());'),
        ('EmAtCheck(this);','playerActorCollision(this,actorModel.get());'),('updateOldPos();','actorModel->updateOldPos();')])
    for name in ['dmgCheck','moveBinocular','visibleCtrl','checkCtrl','seqSeCtrl','subCharLiveCheck']:
        sig=('inline ' if name=='subCharLiveCheck' else '')+'void cPlayer::'+name+'()'
        changes=[]
        if name=='subCharLiveCheck':changes=[('cEm* sub','ActorModelCpu* sub')]
        add('src/game/pl_class.cpp',sig,'void PlayerDomain::'+name+'()',changes)
    add('src/game/pl_class.cpp','void cPlayer::shadowCtrl()','void PlayerDomain::shadowCtrl()',[
        ('pG->Camera.param.Campos.y','shadowCameraPosition().y')])
    add('src/game/pl_class.cpp','int dmMotCk()','int PlayerDomain::dmMotCk()',member=False)
    add('src/game/pl_class.cpp','void cPlayer::motionSet(void* m0, void* seq0, void* m1, void* seq1, int hokan, int frame)',
        'void PlayerDomain::motionSet(void* m0, void* seq0, void* m1, void* seq1, int hokan, int frame)',[
        ('MotionSetCore(this, &this->Motion,','initializeMotionWork(&actorModel->Motion,')])
    add('src/game/player.cpp','void pl_R0_Move(cPlayer* pEm)','void PlayerDomain::pl_R0_Move(PlayerDomain* pEm)',[
        ('Pl_func_move_tbl[pEm->r_no_1](pEm);','pEm->dispatchRoutine1();')])
    sig='void pl_R1_Footwork(cPlayer* pEm)';body,_=function((root/'src/game/player.cpp').read_text(),sig)
    changes=[]
    for expr in sorted(set(re.findall(r'PL_ARC_PTR\(pG->pPlayer, [^)]*\)',body))):changes.append((expr,'arc('+expr.split(', ')[1][:-1]+')'))
    add('src/game/player.cpp',sig,'void PlayerDomain::pl_R1_Footwork(PlayerDomain* pEm)',changes)
    add('src/game/model.cpp','void cModel::partsFixAdjust()','void ModelCpu::partsFixAdjust()',[
        ('cParts*','ModelPart*')],member=False)
    add('src/game/motion.cpp','void PartsWorldPosCalc(cModel* pMod)','void PartsWorldPosCalc(ModelCpu* pMod)',[
        ('cParts*','ModelPart*')],member=False)
    for sig in ['int ShapeMove(cModelInfo* i_pModelInfo)','int SetShape(cModelInfo* info, f32 rate, ShapeData* data)']:
        add('src/game/shape.cpp',sig,sig.replace('cModelInfo*','ModelInfoView*'),member=False)
    add('src/game/pl_wep.cpp','void cPlWep::move()','void PlayerWeaponCpu::move()',member=False)
    add('src/game/cam_qfps.cpp','void CameraQuasiFPS::setPlayerLocation(Mtx mat, Vec* p_norm)',
        'void PlayerDomain::CameraBoundary::LocationWork::setPlayerLocation(Mtx mat, Vec* p_norm)',member=False)
    add('src/game/pl_leon.cpp','int cPlLeon::checkXbutton()','int PlayerDomain::checkXbuttonLeon()',[
        ('SubCharCheckCtrl()','requirePartnerControl()'),('SubCharGetStatus()','requirePartnerStatus()'),('SubCharCtrl(','requirePartnerCommand(')])
    for sig in ['void cPlNeck::init(void* motR, void* motL, int frame)','void cPlNeck::move()',
                'void cPlNeck::motSet(void* data, int frame)','cEm* cPlNeck::getTarget()','void cPlNeck::setMode(int mode)']:
        body,_=function((root/'src/game/pl_class.cpp').read_text(),sig)
        changes=[]
        for a,z in [('cEm*','NativeEmActor*'),('cParts*','ModelPart*'),('cPlayer* p = pPL;','PlayerDomain* p = pl;'),
                    ('pPL->','pl->actorModel->'),('pSUB','pl->pSUB'),('pPL','pl->actorModel.get()'),
                    ('pLog->','pl->pLog->'),('EmMgr.','pl->EmMgr->'),('SatMgr.','pl->SatMgr.'),('VALID_PTR2(','ownedMotion('),
                    ('MotionSetCore(p, &p->m_SubMot,','p->initializeMotionWork(&p->m_SubMot,'),
                    ('p->pMotionB','p->actorModel->pMotionB'),('cPlNeck::','PlayerDomain::NeckCpu::')]:
            if a in body:changes.append((a,z));body=body.replace(a,z)
        if 'p->m_SubMot' in body:
            # Keep the original work in the typed actor, not in an unrelated
            # owner or stack temporary. Only its native address is passed on.
            changes.insert(-1,('p->m_SubMot','p->actorModel->m_SubMot'))
            body=body.replace('p->m_SubMot','p->actorModel->m_SubMot')
        new=body.split('\n{',1)[0]
        # Class fields are owned by the single PlayerDomain; no model casts.
        add('src/game/pl_class.cpp',sig,new,changes[:-1],member=False)
    for sig in ['f32 GetXZAngle(Vec* v0, Vec* v1)','f32 GetXZAngleLocal(Vec* v0, Vec* v1, f32 v0_dir)','f32 GetDistance(Vec* v0, Vec* v1)']:
        add('src/game/sub2.cpp',sig,sig,member=False)
    cpp.append('}')
    (out/'player_live_control.cpp').write_text('\n\n'.join(cpp)+'\n');(out/'player-live-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries),indent=2)+'\n')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
