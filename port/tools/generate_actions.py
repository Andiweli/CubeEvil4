"""Preserved original ActionSelect/ActBtn and actor-collision consumers.

Only typed ownership, OT links and PPC compiler anchors are adapted. Unsupported
display/scenario/window consumers remain explicit boundaries, not success stubs.
"""
import argparse
import json
import re
from pathlib import Path
from generate_runtime import digest, function


def generate(root, out):
    hashes = json.loads((root/'port/config/actions-sources.json').read_text())
    for path, expected in hashes.items():
        if digest(root/path) != expected:
            raise ValueError('Original actions fingerprint mismatch: '+path)
    out.mkdir(parents=True, exist_ok=True)
    (out/'include').mkdir(exist_ok=True)
    header = (root/'include/act_btn.h').read_text()
    player = (root/'include/player.h').read_text()
    delay = player[player.index('template <class T> class cDelay {'):player.index('extern cDelayF m3r;')]
    types = ['#pragma once', 'namespace re4::port {']
    for name in ['ACTION_TYPE', 'ACTCTR_FLAG', 'DISP_FLAG', 'ACTION_FUNC_TYPE']:
        types.append(re.search(r'enum '+name+r' \{.*?\n\};', header, re.S)[0])
    types.extend([delay, '}'])
    field_id=re.search(r'SCEAT_ID_FIELD_INFO\s*=\s*(\d+)',(root/'include/sce_at.h').read_text())[1]
    window_id=re.search(r'WindowTypeEt07\s*=\s*(\d+)',(root/'include/emwindow.h').read_text())[1]
    types.insert(-1,'inline constexpr u8 SourceFieldInfoId='+field_id+';')
    types.insert(-1,'inline constexpr u8 SourceWindowEt07='+window_id+';')
    (out/'include/re4_action_types.h').write_text('\n'.join(types)+'\n')
    cpp = ['#include "player_live_original.h"', '#include "action_original.h"',
           'namespace re4::port {']
    entries = []
    fields = ['pos', 'ang', 'mat', 'r_no_0', 'r_no_1', 'r_no_2', 'r_no_3', 'dmg']

    def add(path, signature, native, changes=(), member=False):
        source=(root/path).read_text().replace(signature+' {',signature+'\n{')
        body, line = function(source, signature)
        for old, new in changes:
            if old not in body:
                raise ValueError('Action adaptation absent: '+signature+' / '+old)
            body = body.replace(old, new)
        body=native+body[body.index('\n{'):]
        used = []
        if member:
            for field in fields:
                pattern = r'(?<![\w>.])\b'+field+r'\b'
                if re.search(pattern, body):
                    body = re.sub(pattern, 'actorModel->'+field, body)
                    used.append(field)
            for obj in ['pEm', 'pl']:
                for field in fields:
                    body = body.replace(obj+'->'+field, obj+'->actorModel->'+field)
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"', body])
        entries.append(dict(source=path, signature=signature, line=line,
                            adaptations=[signature, *[a for a, _ in changes]], nativeModelFields=used))

    for name, result, params in [('init', 'void', ''), ('pullWork', 'cActionButton::ACT_WORK*', ''),
                                  ('checkButton', 'int', 'ACT_WORK* work'), ('checkPLStatus', 'int', 'ACT_WORK* work')]:
        signature = f'{result} cActionButton::{name}({params})'
        changes = []
        if name == 'init': changes = [('ClearOTagR(m_ot, 16);', 'clearOrdering();')]
        if name == 'checkButton':
            changes = [('register u64 key asm("r9");', 'u64 key;'), ('asm volatile("");', '/* codeless compiler anchor */')]
        if name == 'checkPLStatus':
            changes = [('pPL->hp', 'playerHp()'), ('pPL->actCheck()', 'playerActCheck()'), ('PlGetStatus()', 'playerStatus()')]
        native_result = result.replace('cActionButton::', 'ActionManager::')
        add('src/game/act_btn.cpp', signature, f'{native_result} ActionManager::{name}({params})', changes)
    add('src/game/act_btn.cpp', 'void cActionButton::set(int kind, int slot, void* func, void* arg, int flags, int btn, int type, void* d)',
        'void ActionManager::setOriginal(int kind, int slot, ActionCallback func, void* arg, int flags, int btn, int type, std::intptr_t d)',
        [('w->d = (int) d;', 'w->d = d;'), ('AddPrim(&m_ot[slot], (u32*) w);', 'addWork(slot, w);')])
    body, _ = function((root/'src/game/act_btn.cpp').read_text(), 'void cActionButton::move()')
    sce = body[body.index('                SceExec('):body.index('                break;', body.index('                SceExec('))]
    start = body.index('            case ACT_FUNC_SCE_AT: {')
    end = body.index('\n            }\n            }', start)+len('\n            }')
    area = body[start:end]
    add('src/game/act_btn.cpp', 'void cActionButton::move()', 'void ActionManager::moveOriginal()', [
        ('u32 tag;', 'int tag;'), ('Cckpt.m_ActBttn.setDispFlag(0);', 'resetDisplay();'),
        ('for (tag = m_ot[15]; tag != 0xFFFFFFFF; tag = w->tag)', 'for (tag = first(); tag != -1; tag = next(w))'),
        ('w = (ACT_WORK*) (tag | 0x80000000);', 'w = work(tag);'),
        ('w->func != 0','bool(w->func)'),
        ('        if ((s32) tag >= 0) {\n            continue;\n        }', '        // Native links contain work indices only; GC OT bucket sentinels are not pointers.'),
        (area, '            case ACT_FUNC_SCE_AT: { executeArea(w); break; }'),
        ('((ActBtnFunc) w->func)(w->arg, w->d);', '{ auto selected=*w; selected.func.invoke(selected.arg,selected.d); }'),
        (sce, '                executeScenario(w,flag);\n')])
    # Source inline base virtual method; the central KEY remains the only input.
    add('include/pl_wep.h', 'virtual int keyKamae()', 'int NativeWeapon::keyKamae(const KEY &Key)')
    for name in ['actionSelect', 'actCheck', 'keyReload']:
        changes=[]
        if name == 'actionSelect':
            changes=[('cEmWindow* win;', 'NativeActionWindow* win;'), ('Push->catchCheck()', 'catchPush(Push)'),
                     ('Wep->m_pWep->reloadable()', 'weaponReloadable()'), ('win->ChkStatus()', 'windowStatus(win)')]
            source, _ = function((root/'src/game/pl_class.cpp').read_text(), 'int cPlayer::actionSelect()')
            for callback in sorted(set(re.findall(r'\(void\*\) (\w+)', source))):
                changes.append(('(void*) '+callback, 'windowAction(win)' if callback=='windowOn' else 'actionCallback(&PlayerDomain::'+callback+')'))
            changes += [('fallCheck_80172E38(this)', 'fallCheck(this)')]
        if name == 'keyReload': changes=[('Joy[0]', 'inputJoy()')]
        add('src/game/pl_class.cpp', 'int cPlayer::'+name+'()', 'int PlayerDomain::'+('actionSelectOriginal' if name=='actionSelect' else name)+'()', changes, True)
    add('src/game/pl_sub.cpp', 'u32 PlGetStatus()', 'u32 PlayerDomain::playerStatus()',
        [('cPlayer* pl = pPL;', 'PlayerDomain* pl = this;'), ('cPlayer::F_EVENT', 'F_EVENT')], True)
    for name in ['joyKamae', 'joyLKamae']:
        src, _ = function((root/'src/game/pl_sub.cpp').read_text(), 'int '+name+'()')
        changes=[('cPlayer* pl = pPL;', 'PlayerDomain* pl = this;'), ('CfgFlagChk(pSys, CFG_KNIFE_MODE)', '(configurationFlags() & (0x80000000u >> CFG_KNIFE_MODE))'), ('cPlayer::F_KNIFE', 'F_KNIFE')]
        for old,new in [('cPlWep*','PlayerWeaponCpu*'), ('pPL->Wep','Wep'), ('wep->m_pWep->keyKamae()', 'weaponAimKey()')]:
            if old in src: changes.append((old,new))
        add('src/game/pl_sub.cpp', 'int '+name+'()', 'int PlayerDomain::'+name+'()', changes)
    for name,result in [('actWallCheck','int'),('fanceCheck','int'),('jumpCheck','int'),('upDownCk','u32')]:
        changes=[] if name!='jumpCheck' else [('pG->stage_no','(pG->room_id >> 8)'),('pG->room_no','(pG->room_id & 0xff)')]
        add('src/game/pl_class.cpp', result+' '+name+'(cPlayer* pEm)', result+' PlayerDomain::'+name+'(PlayerDomain* pEm)',changes, member=True)
    add('src/game/pl_class.cpp','static int fallCheck_80172E38(cPlayer* pl)','int PlayerDomain::fallCheck(PlayerDomain* pl)',member=True)
    add('src/game/pl_class.cpp','int windowCheck(cPlayer* pEm, u8* pDir, cEmWindow** pEmWindow_out)',
        'int PlayerDomain::windowCheck(PlayerDomain* pEm, u8* pDir, NativeActionWindow** pEmWindow_out)',[
            ('cEmWindow* win;', 'NativeActionWindow* win;'), ('ChkWindow(pEm,', 'ChkWindow(pEm->actorModel.get(),')],True)
    add('src/game/sce_at.cpp','SCE_AT_DATA_FIELD_INFO* SceAtCheckFieldInfo(Vec* pos)',
        'NativeActionFieldInfo* PlayerDomain::SceAtCheckFieldInfo(Vec* pos)',[
            ('SCE_AT_DATA* w;', 'NativeActionField* w;'), ('pS == 0', '!scenarioInitialized'), ('bitOff(w->be_flg)', '!(w->be_flg & 1)'),
            ('SCEAT_ID_FIELD_INFO', 'SourceFieldInfoId')])
    add('src/game/emwindow.cpp','int ChkWindow(cModel* pModTar, Vec* pos0, Vec* pos1, int field_id, u16* etc_flag, Vec* pNorm, Vec* pCenter, cEmWindow** o_pEm)',
        'int PlayerDomain::ChkWindow(ModelCpu* pModTar, Vec* pos0, Vec* pos1, int field_id, u16* etc_flag, Vec* pNorm, Vec* pCenter, NativeActionWindow** o_pEm)',[
            ('SCE_AT_DATA_FIELD_INFO* info;', 'NativeActionFieldInfo* info;'),('cEmWindow* win;', 'NativeActionWindow* win;'),
            ('(cEmWindow*) info->pParent', 'info->pParent'),('WindowTypeEt07','SourceWindowEt07')])
    add('src/game/sub2.cpp','int Front_check(cModel* a, Vec* b, f32 ang)','int Front_check(ModelCpu* a, Vec* b, f32 ang)')
    for name in ['fanceOn','fallOn','levelUpOn','levelDownOn','level2UpOn','level2DownOn','holdOn','jumpFallOn']:
        source,_=function((root/'src/game/pl_class.cpp').read_text(), 'void '+name+'()')
        changes=[('cPlayer* pl = pPL;', 'PlayerDomain* pl = this;')]
        for old,new in [('pPL->dmg','actorModel->dmg'),('pPL->ang','actorModel->ang'),('pPL->m_Work0','m_Work0')]:
            if old in source and old!=new: changes.append((old,new))
        add('src/game/pl_class.cpp','void '+name+'()','void PlayerDomain::'+name+'()',changes,True)
    add('src/game/pl_push.cpp','int cPlPush::catchCheck()','int PlayerDomain::catchPush(PlayerPushCpu* push)',[
        ('cPlayer* pl = m_pEm;', 'ModelCpu* pl = m_pEm;'), ('m_pEm','push->m_pEm'), ('cEm* em = EmMgr.fastAt(i);','NativeEmActor* em = EmMgr->fastAt(i);'),
        ('EmMgr.getArrayNum()', 'EmMgr->getArrayNum()'), ('(cEmRack*) em','em'), ('At_em_rect_rect_ck(', 'action_collision::At_em_rect_rect_ck('),
        *[(field,'push->'+field) for field in ['m_Target','m_pEm','m_Dir','m_Ctr'] if field!='m_pEm']])
    # m_pEm in the body is a model owner, not a cast to another player layout.
    for name in ['moveEye','moveEyeNormal','moveEyeMotion']:
        changes=[]
        if name!='moveEye':changes=[('cParts*','ModelPart*')]
        if name=='moveEyeNormal': changes += [('static cDelayF eyeDir;', 'auto &eyeDir=eyes.direction;'),('static int timer;', 'auto &timer=eyes.timer;'),('static int eyetime = 0;', 'auto &eyetime=eyes.time;')]
        add('src/game/pl_class.cpp','void cPlayer::'+name+'()','void PlayerDomain::'+('moveEyeOriginal' if name=='moveEye' else name)+'()',changes,True)
    add('src/game/at_mod.cpp','void EmAtCheck(cModel* pMod)','void PlayerDomain::actorCollisionOriginal(ModelCpu* pMod)',[
        ('cEm* m;', 'NativeEmActor* m;'), ('cObj* o;', 'NativeObject* o;'), ('EmMgr.', 'EmMgr->'), ('__em_at_core(', 'action_collision::core(')])
    add('src/game/sub2.cpp','void AddSpeed(cModel* pEm, const Vec* speed)','void AddSpeed(ModelCpu* pEm, const Vec* speed)')
    add('src/game/sub2.cpp','f32 Muku3(f32 src_dir, Vec* v0, f32 dy)','f32 Muku3(f32 src_dir, Vec* v0, f32 dy)')
    cpp.append('namespace action_collision {')
    cpp.append('static bool priorityCheck(ModelCpu*,ModelCpu*); static u32 sphereRectCk(ActorCollisionInfo*,Vec&,f32); f32 RootSumSquare3(Vec*);')
    for path,sig in [('src/game/at_mod.cpp','static bool priorityCheck(cModel* pMod, cModel* pMod2)'),
                     ('src/game/at_mod.cpp','void __em_at_core(cModel* pMod, cModel* pMod2)'),
                     ('src/game/at_mod.cpp','u32 At_em_rect_rect_ck(cModel* pMod, cModel* pMod2)'),
                     ('src/game/at_mod.cpp','int em_rect2_ck_sub(cModel* pMod, cModel* pMod2)'),
                     ('src/game/at_mod.cpp','u32 At_em_sphere_rect_ck(cModel* pMod, cModel* pMod2)'),
                     ('src/game/at_mod.cpp','static u32 sphereRectCk(cAtariInfo* pAt2, Vec& ipos, f32 r)'),
                     ('src/game/at_mod.cpp','u32 At_em_sphere_sphere_ck(cModel* pMod, cModel* pMod2)'),
                     ('src/game/at_sub.cpp','int At_rect_point_ck(Vec* rect, Vec* pnt)'),
                     ('src/game/at_sub.cpp','int At_rect_rect_ck(Vec* rect0, Vec* rect1)'),
                     ('src/game/at_sub.cpp','int Get_ang_dir(f32 ay)'),
                     ('src/game/sub2.cpp','f32 GetDistance(Vec* v0, Vec* v1)'),
                     ('src/game/sub2.cpp','f32 GetDistance3(Vec* v0, Vec* v1)'),
                     ('src/game/sub2.cpp','f32 RootSumSquare3(Vec* v)')]:
        src,_=function((root/path).read_text(),sig);changes=[]
        for old,new in [('cModel*','ModelCpu*'),('cAtariInfo*','ActorCollisionInfo*'),('cParts*','ModelPart*'),('__em_at_core','core'),
                        ('((cEm*) m)->','m->'),('(u32) pMod2 > (u32) pMod','reinterpret_cast<std::uintptr_t>(pMod2) > reinterpret_cast<std::uintptr_t>(pMod)'),
                        ('VECNormalize','C_VECNormalize'),('PSMTXInverse','C_MTXInverse'),('PSMTXMultVecSR','motionMultVecSR'),('SQRTF','std::sqrt')]:
            if old in src: changes.append((old,new));src=src.replace(old,new)
        if path=='src/game/at_mod.cpp' and 'GetDistance(' in src:changes.append(('GetDistance(', 'action_collision::GetDistance('))
        add(path,sig,src.split('\n{',1)[0],changes)
    cpp.extend(['}', '}'])
    # Platform vector adapters already used by the original motion runtime.
    code='\n\n'.join(cpp)+'\n'
    code=code.replace('PSVECAdd(', 'C_VECAdd(').replace('PSVECSubtract(', 'C_VECSubtract(').replace('PSVECScale(', 'motionVecScale(')
    (out/'actions_control.cpp').write_text(code)
    (out/'actions-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries,
        scope='Original ActionSelect/ActBtn, SAT action queries, push selection, eyes and actor collision. Window field owner, display and complete interaction routines remain explicit boundaries.'),indent=2)+'\n')


if __name__ == '__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();generate(args.root.resolve(),args.output.resolve())
