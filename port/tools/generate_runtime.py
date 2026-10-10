#!/usr/bin/env python3
"""Reviewed original bodies and scalar views; never import the legacy libc/layout."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_text(encoding='utf-8').replace('\r\n', '\n').encode()).hexdigest()


def function(source, signature):
    start = source.index(signature + '\n{')
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end], source[:start].count('\n') + 1


def generate(root, output):
    manifest = json.loads((root/'port/config/runtime-sources.json').read_text())
    for path, expected in manifest.items():
        if digest(root/path) != expected:
            raise ValueError('Original runtime fingerprint mismatch: ' + path)
    output.mkdir(parents=True, exist_ok=True)
    (output/'include').mkdir(exist_ok=True)
    read = lambda path: (root/path).read_text(encoding='utf-8')
    joy, main, pad, glob = map(read, ['include/joy.h', 'include/main.h', 'include/dolphin/pad.h', 'include/global.h'])
    types = ['#pragma once\n#include "types.h"\nnamespace re4::port {']
    types.append(re.search(r'typedef struct \{\s*f32 x, y, z;\s*\} Vec;', read('include/vec.h'))[0])
    types.append(re.search(r'typedef struct \{\s*u8 r, g, b, a;\s*\} GXColor;', read('include/gx.h'))[0])
    for text, name in [(joy, 'PAD_MOTOR'), (joy, 'JOY'), (main, 'KEY')]:
        types.append(re.search(r'struct ' + name + r' \{.*?\n\};', text, re.S)[0])
    for name in ['PADStatus', 'PADClampRegion']:
        types.append(re.search(r'typedef struct ' + name + r' \{.*?\} ' + name + ';', pad, re.S)[0])
    types += re.findall(r'^#define (?:JOY_\w+|PAD_CHAN\w+|PAD_ERR_\w+)\s+[^\n]+', joy + '\n' + pad, re.M)
    for name in ['DBG_FLAG', 'STA_FLAG', 'SYS_FLAG', 'SPF_FLAG', 'SCF_FLAG', 'CFG_FLAG', 'DPF_FLAG']:
        types.append(re.search(r'enum ' + name + r' \{.*?\n\};', glob, re.S)[0])
    # Names/types are copied from GLOBAL_WK; these are native views, not wire offsets.
    fields = ['Rno0','Rno1','Rno2','Rno3','RoomNo_next','room_id','room_id_prev','pl_type','game_mode',
              'game_costume','pl_costume','debug_mode','debug_disp','game_country','System_flg',
              'Stop_flg','Status_flg','Debug_flg','Room_flg','Scenario_flg','Frame_cnt',
              'game_start_time','play_time','pl_ang_y','ashley_life','pl_life','pl_life_max','ashley_life_max','weapon_no_old','weapon_no','weapon_type','bullet_type','weapon_lv_power','weapon_lv_speed','weapon_lv_blt','weapon_lv_reload','pl_flag','Speed','SaveKind','em_list_no','game_cnt','Disp_flg','AreaNo','shooting_mode','IsMessageInit','peseta','time_bonus']
    types.append('struct NativeGameState {')
    for name in fields:
        matches = re.findall(r'^\s*((?:u8|u16|u32|s8|s16|s32|f32) ' + name + r'(?:\[\d+\])?);', glob, re.M)
        if len(matches) != 1: raise ValueError('ambiguous scalar field: ' + name)
        types.append('    ' + matches[0] + '{};')
    ope=re.search(r'    u32 ope_x82E8;.*?    u8 pad_8304\[0x832C - 0x8304\];',glob,re.S)[0]
    ope=re.sub(r';(?=\s*(?://|\n|$))','{};',ope)
    types.append(ope)
    types += ['    Vec pl_pos{};',
              '    u8 Em_list[0x2000]{}; // Original ESL bytes; no native overlay.', '};', '} // namespace re4::port']
    (output/'include/re4_runtime_types.h').write_text('\n'.join(types) + '\n', encoding='utf-8')
    bodies = ['// Generated from reviewed sources. Adaptations recorded in runtime-manifest.json.',
              '#include "runtime_original.h"', '#include <cmath>', 'namespace re4::port {']
    entries = []
    def add(path, signature, changes=()):
        body, line = function(read(path), signature)
        for before, after in changes:
            if body.count(before) != 1: raise ValueError('adaptation no longer unique: ' + before)
            body = body.replace(before, after)
        name = re.search(r'(\w+)\(', signature)[1]
        body = body.replace(name + '(', 'OriginalContext::' + name + '(', 1)
        bodies.extend([f'#line {line} "{(root/path).as_posix()}"', body])
        entries.append(dict(source=path, signature=signature, line=line,
                            adaptations=['member qualification'] + [a for a, _ in changes]))
    for signature in ['void PadInit()', 'void PadRead()', 'void KeyStop(u64 un_stop_bit)',
                      'void KeyClear(u64 un_stop_bit)', 'void VibControl()', 'int PadCheckStatus(JOY* joy)']:
        changes = []
        if 'PadRead' in signature:
            changes = [('register int dead asm("r16");', 'int dead;'),
                       ('asm volatile("");', '/* PPC scheduling barrier has no native instruction. */'),
                       ('    Pad_test();', '    /* Original debug overlay is unavailable; no input mutation. */')]
        if 'KeyClear' in signature:
            changes = [('    static u64 un_stop_mask = 0;', '    // un_stop_mask is owned by this native domain.')]
        add('src/game/pad.cpp', signature, changes)
    # The original bit cursor remains u32 even though KEY is u64. Do not silently fix it.
    table = re.search(r'u32 Key_type_tbl\[2\]\[64\] = \{.*?\n\};', read('src/game/pad.cpp'), re.S)[0]
    bodies.append(table.replace('u32 Key_type_tbl', 'const u32 OriginalContext::Key_type_tbl'))
    deg = re.search(r'^#define DEG2RAD [^\n]+', read('include/math_sub_decl.h'), re.M)[0]
    chase = re.search(r'SCC_CHASE = (\d+)', read('include/pl_sub.h'))[1]
    bodies.insert(3, '#define KEY_BIT(n) ((u64) 1 << (n))\n#define STICK_ON 30.0f\n' + deg + '\n#define DEG(x) ((x) * DEG2RAD)\nconstexpr int SCC_CHASE = ' + chase + ';\n#define KeyStopFlagClear() do { StaFlagOff(pG, STA_PL_ACTION); StaFlagOff(pG, STA_PL_CHECK); StaFlagOff(pG, STA_PL_CHECK2); } while (0)')
    for signature in ['void systemScreenInit()', 'void systemWorkInit()', 'void SetSystemVcnt(int v_cnt)', 'int GetSystemVcnt()']:
        add('src/game/main.cpp', ('static ' if signature.startswith('void systemScreen') else '') + signature)
    # Static applies to the original free function, not the member definition.
    bodies = [b.replace('static void OriginalContext::systemScreenInit()', 'void OriginalContext::systemScreenInit()') for b in bodies]
    for signature in ['void SecToTime(u32 sec, u32* ret_time, u32* ret_min, u32* ret_sec)',
                      'void InitGameTime()', 'u32 GetGameTime(u32* ret_time, u32* ret_min, u32* ret_sec)', 'void SetGameTime()']:
        add('src/game/main_sub.cpp', signature)
    add('src/game/game.cpp', 'void DoorFlagInit()')
    add('src/game/game.cpp', 'void gameDebug()')
    bodies.append(re.search(r'union FadeColor \{.*?\n\};',read('src/game/game.cpp'),re.S)[0])
    add('src/game/cloth.cpp','void ClothInit()', [('    int i;', '    trace.push_back("ClothInit:original");\n    int i;')])
    unavailable = ['Game.omake_wep_addr = MEM_ALLOC(0x70000, 1, 13);',
        'ItemMgr.gameInit();','SceAtInitSaveItem();','MerchantGameInit();',
        'GameLoad();','GamePointInit(0);']
    add('src/game/game.cpp','void gameInit()', [(s,'missingGameInit('+json.dumps(s.rstrip(';'))+');') for s in unavailable]+[
        ('LightMgr.init(LightFuncTbl);','LightMgr.init();'),
        ('LightMgr.initPath((LightPathHeader*) CORE_ARC(CORE_LIGHT_PATH));','LightMgr.initPath();')])
    add('src/game/main_sub.cpp','void systemVISetBlack(int sw)')
    add('src/game/game.cpp', 'void GameTask()', [(
        'static void (*game_func_tbl[7])() = {\n        gameInit, gameStageInit, gameRoomInit, gameMainLoop, gameDoordemo, gameEnding, gameOption,\n    };',
        'const std::array<TaskEntry, 7> game_func_tbl = {\n        [this]{gameInit();}, [this]{gameStageInit();}, [this]{gameRoomInit();}, [this]{gameMainLoop();}, [this]{gameDoordemo();}, [this]{gameEnding();}, [this]{gameOption();}\n    };')])
    stage = read('src/game/stage.cpp')
    for name in ['emlist_name', 'emlist_dbg_name']:
        bodies.append(re.search(r'static const char\* ' + name + r'\[11\] = \{.*?\n\};', stage, re.S)[0])
    add('src/game/stage.cpp', 'static inline int emListVillage(int room)', [])
    bodies = [b.replace('static inline int OriginalContext::emListVillage', 'int OriginalContext::emListVillage') for b in bodies]
    for signature in ['int checkEmListNo(u16 room_no)', 'const char* getEmListName(u32 no)',
                      'const char* getEmListDbgName(int no)', 'int getEmListNum()', 'void StageSet()']:
        changes = []
        if 'StageSet' in signature:
            changes = [('GLOBAL_WK* g', 'NativeGameState* g'),
                       ('g->stage_prev != g->stage_no', '(g->room_id_prev >> 8) != (g->room_id >> 8)'),
                       ('pG->stage_no == 1', '(pG->room_id >> 8) == 1')]
        add('src/game/stage.cpp', signature, changes)
    add('src/game/stage.cpp', 'void readEmList(int proc)', [('GLOBAL_WK* g', 'NativeGameState* g')])
    for signature in ['void ScenarioRoomInit()', 'void ScenarioMove()', 'void scenarioLoopBeforeInit()', 'void scenarioLoopAfterInit()']:
        add('src/game/sce_sys.cpp', signature, [('    SceSys.pLadderTask = 0;', '    SceSys.pLadderTask = std::nullopt;')] if 'ScenarioRoomInit' in signature else [])
    add('src/game/sce_com.cpp', 'void SceInitItemEvent()', [('        HALT_FILE_FIRST();', '        throw std::logic_error("unreachable original HALT");')])
    add('src/game/sce_com.cpp', 'int DebugTrg(int)')
    r120 = read('src/st1/r120.cpp')
    event = read('include/event.h')
    bodies.insert(3, re.search(r'enum EvtStatusFlag \{.*?\n\};', event, re.S)[0] +
                  '\n#define EvtStfBit(f) (0x80000000u >> (f))\nconstexpr int EvtReadFlagNone = 0;\nconstexpr int SCE_PRIO_DEF_2 = 2;')
    for signature in ['void R120Init()', 'void R120Main()', 'void R120Event()',
                      'void Evt_R120S00_Func(Event* e, u32)', 'void Evt_R120S01_Func(Event* e, u32)',
                      'void EventCarInit(Event* e)', 'void EvtTexRenderCamTrans(Event* e, int cut)',
                      'static inline void r120_setTrans(int on)']:
        changes=[]
        if 'R120Init' in signature:
            changes=[('MEM_CALLOC(sizeof(R120Work), 1, 0xD)', 'ModuleContext::active().callocObject<R120Work>()'),
                     ('(void*) Evt_R120S00_Func','[this](Event* e,u32 p){ Evt_R120S00_Func(e,p); }'),
                     ('(void*) Evt_R120S01_Func','[this](Event* e,u32 p){ Evt_R120S01_Func(e,p); }'),
                     ('SceExec(0x12, R120Event,','SceExec(0x12, [this]{ R120Event(); },')]
        if 'EvtTexRenderCamTrans' in signature:
            changes=[('Event* e, int cut)', 'Event* e, int)')] # Original cut parameter is unused.
        add('src/st1/r120.cpp',signature,changes)
    bodies = [b.replace('static inline void OriginalContext::r120_setTrans','void OriginalContext::r120_setTrans') for b in bodies]
    bodies.append('} // namespace re4::port')
    (output/'runtime_control.cpp').write_text('\n\n'.join(bodies) + '\n', encoding='utf-8')
    clamp = read('src/lib/Padclamp.c')
    clamp = re.sub(r'^#include[^\n]+\n', '', clamp, flags=re.M)
    (output/'pad_clamp.cpp').write_text('#include "runtime_original.h"\n#include <cmath>\nnamespace re4::port {\n' + clamp + '\n}\n', encoding='utf-8')
    (output/'runtime-manifest.json').write_text(json.dumps(dict(sources=manifest, functions=entries,
        sdk=['PADClamp','PADClampCircle','ClampStick','ClampCircle','ClampTrigger'],
        native_layout='Native scalar views, no GLOBAL_WK memory overlay; POD JOY/KEY/PADStatus.'), indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    generate(args.root.resolve(), args.output.resolve())
