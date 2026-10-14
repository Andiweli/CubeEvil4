#!/usr/bin/env python3
"""Inventory frozen Leon/weapon/item sources; never equate a UI enum with parity.

The definition/table inventory is mechanical. Only the explicit review records
below constitute source-to-port mappings. An unreviewed or unported callable is
conservatively labelled not_transferred, rather than inferred from its name.
No disc assets, credentials or absolute workstation paths enter the report.
"""
import argparse
import hashlib
import json
import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / 'upstream/re4/src'
COMMIT = '9b76ff003081d6ece8b838453fb8310743a972c7'
REVIEW = {}


def reviewed(source, names, port, support, gaps):
    for name in names.split():
        REVIEW[(source, name)] = {
            'status': 'partial_adapter', 'port': port,
            'transferred_subset': support, 'remaining_differences': gaps,
        }


reviewed('game/player.cpp',
         'pl_R0_Move pl_R1_Footwork pl_R1_Walk pl_R1_Back pl_R1_Run '
         'pl_R1_Turn pl_R1_Turn180',
         ['leon_preview.cpp', 'animation.cpp', 'turn_rules.cpp', 'walk_probe.cpp',
          'collision_query.cpp', 'sat.cpp'],
         'Original normal-costume FCV/SEQ root movement, stick sectors, turn '
         'constants, 180-degree clip and 30-Hz motion runtime. M36 ordinary '
         'states resolve source radius400 at posed part0 XZ/actorY+400 after '
         'motion, including stationary poses; actual r101 skin/GLES evidence '
         'is recorded in M36-PLAYER-WALL.md.',
         'Separate native owner; original cAtari/collision manager, floor IK, '
         'injury gaits, neck overlay, breathing and every control transition '
         'are not a complete cPlayer transfer.')
reviewed('game/player.cpp', 'pl_R1_Weapon', ['leon_preview.cpp', 'weapon_preview.cpp'],
         'Playable WEP01, WEP02 and WEP07 normal profiles; fresh power nibble0 '
         'and per-profile source power-level0..6 metadata carried by shots and '
         'retained during room reconstruction. Merchant tuning transactions '
         'and complete item tuning state are not installed.',
         'Other weapon classes, complete tuning, sway/recoil RNG, weapon lock turns, '
         'crouch branches and unarmed movement remain absent.')
reviewed('game/player.cpp', 'pl_R1_Ladder', ['ladder_motion.cpp', 'leon_preview.cpp'],
         'Original fixed tower ascent/descent FCVs and source rung playback.',
         'Adapter offers the tested nine-level tower; all scenario-supplied '
         'ladder lengths/callers are not transferred.')
reviewed('game/player.cpp', 'pl_R1_ObjPush', ['room_interactions.cpp', 'leon_preview.cpp'],
         'Take-hold, push loop, stopped and release FCVs; authored rack range.',
         'Original general cPlPush manager, partner/scenario object ownership '
         'and every registered push target are not transferred.')
reviewed('game/player.cpp', 'pl_R1_Aux', ['room_ladders.cpp', 'leon_preview.cpp'],
         'Specific object-ladder and EM12 kick/catch callbacks.',
         'The arbitrary original pFuncAux dispatcher is not installed.')
reviewed('game/player.cpp', 'pl_R1_Fance fanceWidthCheck fanceAdjust',
         ['room_jumps.cpp', 'leon_preview.cpp'],
         'Original wide/narrow FCVs, bit0x20 offer tests and MotBase replay; '
         'broken-window r3=5 takes narrow path with fanceAdjust disabled.',
         'Landing uses a bounded SAT/contact adapter. Character variants, '
         'scenario-disabled offers and full original atari priority chain '
         'remain outside this runtime.')
reviewed('game/player.cpp', 'pl_R1_Fall fallCheck getHeighAdjust',
         ['room_jumps.cpp', 'leon_preview.cpp'],
         'Original fall/land FCV/SEQ, normal-based offers and real-floor exits.',
         'Bounded safe-drop adapter; automatic out-of-bounds falls, water '
         'splash branch and all character-specific paths are not complete.')
reviewed('game/player.cpp','pl_R1_JumpFall',['room_jumps.cpp','leon_preview.cpp'],
         'Original PL5e/60 jump and PL5f/61 landing, source work5..14 height '
         'adjustment, prior-part4 contact ray before motion, immediate landing '
         'tick and sequence20 completion; eight real r10c paths in both directions.',
         'Normal Leon only; scenario STA_PL_JUMP_OFF, r226 height exception, '
         'shadow/face/splash effects and full cAtari manager are not transferred. '
         'Bounded SAT/contact checks validate the native landing.')
reviewed('game/pl_class.cpp','jumpCheck jumpFallOn',
         ['room_jumps.cpp','leon_preview.cpp'],
         'Both source shoulder rays600 ahead, common SAT bit19, second hit '
         'direction, floor3800 beyond/+1500 height/max1500 and immediate facing.',
         'Campaign disable flag and r226 exception are absent; the native '
         'offer additionally verifies authored root exit against real scenery.')
reviewed('game/pl_class.cpp','cPlayer::seqSeCtrl',
         ['animation.cpp','leon_preview.cpp','sound_footsteps.cpp','android_bridge.cpp'],
         'Original Se-1 event, Free&7, parts21/25/root positions and clothing '
         'companion cues consumed once after each native motion tick. Original '
         'FSE block5 dry/water/floor variants resolved by the sound owner.',
         'Source sand-power effects and STA_PL_FOOT gameplay flag are absent; '
         'the audio owner has only available original FSE/sample routes.')
reviewed('game/pl_class.cpp',
         'cPlayer::actionSelect actWallCheck fanceCheck fanceOn windowCheck '
         'windowOn fallOn cPlayer::checkCtrl cPlayer::keyConfigTypeA',
         ['leon_preview.cpp', 'room_jumps.cpp', 'room_interactions.cpp', 'android_bridge.cpp'],
         'Mapped controller edges/sectors and specific door, fence, fall, '
         'window, push and ladder offers.',
         'Source actor/scenario priority manager, arbitrary event callbacks, '
         'ledge climb and partner commands are not full transfers.')
reviewed('game/pl_class.cpp', 'cMot3::set cMot3::set0 cMot3::move',
         ['animation.cpp'],
         'Native pointer-safe original three-way FCV blend order and sign '
         'crossing, including source Hokan and secondary work state.',
         'The adapter exposes selected uses; all cModel/game integration '
         'and unimplemented actor callbacks are not provided by this alone.')
reviewed('game/pl_leon.cpp', 'cPlLeon::setModel cPlLeon::setRightHand cPlLeon::setLeftHand',
         ['leon_preview.cpp', 'model_skin.cpp', 'actor_gl.cpp'],
         'Normal-costume original model, skin weights, grip/knife submodels. '
         'M36 verifies actual r101 wall clearance and original raw-part/bind '
         'world vertices through production GLES for three grips and knife '
         'without changing FCV parts, frames or ordinary GL depth.',
         'Costumes, dynamic face/neck/cloth state and all equipment variants '
         'are not a complete original cPlLeon.')
reviewed('game/pl_body.cpp', 'cPlBody::waistMove', ['animation.cpp'],
         'Source half/half/counterrotation updates raw attachment matrices.',
         'Other body/IK/neck dynamics are not implied by waist support.')
reviewed('game/pl_dmg.cpp', 'Pl_R0_Damage damageBlow damageBlast',
         ['leon_preview.cpp', 'object_combat.cpp'],
         'Selected explosion blow/stagger/recovery FCVs and damage rules.',
         'Complete damage-area dispatch and all damage/death branches are '
         'missing. Unlimited HP is an explicit optional test override.')
reviewed('game/pl_dmg.cpp','damageNormal Pl_R0_Die',
         ['leon_preview.cpp','grab_rules.cpp'],
         'First r100 axe uses source normal damage types0..5, configured source '
         'rank0..10 unarmoured '
         'LifeDownSet2/RNG, original PL48/49/4a+64/4b+65 and fatal4c+4d, '
         'protected damage timer, work30/35 input cancel and five-tick recovery.',
         'Selected normal Leon axe path only; other damage types, directional '
         'ang!=123 root counterrotation, face/effect/vibration, general hit '
         'area manager, armour, adaptive GameAddPoint rank changes and '
         'complete death/game-over presentation are absent.')
for source, prefix in [('wep/pl_handgun.cpp', 'wep02'),
                       ('wep/pl_shotgun.cpp', 'wep07')]:
    names = ['wepDown', prefix+'_r2_ready', prefix+'_r3_ready00',
             prefix+'_r3_ready10', prefix+'_r2_set', prefix+'_r3_set00',
             prefix+'_r3_set10', prefix+'_r3_set40', prefix+'_r2_fire',
             prefix+'_r3_fire00', prefix+'_r3_fire10', prefix+'_r2_reload']
    names.append('PlHandgunMove' if prefix=='wep02' else 'PlShotgunMove')
    reviewed(source, ' '.join(names), ['leon_preview.cpp', 'weapon_preview.cpp'],
             'Original normal draw/aim/fire/down/reload FCVs, local ammo '
             'commit, source firing threshold and set40 recoil-tail flow. '
             'M36 posed400 scenery body applies to these stationary states; '
             'shots carry original per-profile firepower-level metadata.',
             'Not the complete original module: lock-turn subroutines, '
             'weapon sway/recoil RNG, tuning, crouch and complete water/impact '
             'and shell-ejection effects remain unported.')
for source, cls in [('wep01/objFn57.cpp','cObjFn57'),
                    ('wep02/objMauser.cpp','cObjMauser'),
                    ('wep07/wep07.cpp','cObjShotgun')]:
    reviewed(source, ' '.join(cls+'::'+name for name in
             ['init','moveFire','moveReload','setMotion']),
             ['weapon_preview.cpp','leon_preview.cpp','muzzle_effects.cpp'],
             'Original weapon BIN/TPL, selected local FCV motion and magazine '
             'commit; disc EST flash/smoke and light subset.',
             'Separate native asset owner. Shell/cartridge ejection, weapon '
             'ability sway/recoil, all secondary pins/grip states and complete '
             'original object lifecycle remain absent.')
reviewed('game/pl_knife.cpp',
         'PlKnifeMove knife_r2_ready knife_r3_ready00 knife_r3_ready10 '
         'knife_r2_set knife_r3_set00 knife_r3_set10 knife_r3_set40 '
         'knife_r2_fire knife_r3_fire00 knife_r3_fire10 hitCheck '
         'knife_r2_down knife_r3_down00 knife_r3_down10 setWepTrans',
         ['leon_preview.cpp', 'melee_rules.cpp', 'hit_geometry.cpp'],
         'Original knife FCVs, pitched blend, authored hand sweep/serial and '
         'tested EM12/object/window hit subset.',
         'Original turn-in-aim paths, every damage target/material and '
         'all character/weapon ownership branches are not complete.')
reviewed('game/pl_wep.cpp', 'PlWepLockCtrl PlWepHitCheck2',
         ['leon_preview.cpp', 'object_combat.cpp', 'enemy_encounter.cpp'],
         'Mapped right-stick pitch/yaw; shotgun source0.8 pitch/body speed '
         'and six-degree waist limit; real projectile/13-pellet ray subset.',
         'PlWepLockRand, D-pad lock-turn, auto-track and complete water/scenery '
         'hit dispatch are absent. Modern independent right-stick mapping '
         'and camera pitch are explicit platform adaptations.')
reviewed('game/item.cpp',
         'cItemMgr::get cItemMgr::combine cItemMgr::minimumSearch '
         'cItemMgr::ordering reload_main cItemMgr::use cItemMgr::erase '
         'itemInfo itemCombine itemCombineCheck',
         ['inventory_case.cpp', 'item_rules.cpp', 'room_pickups.cpp'],
         'Original type/default/stack/money/footprint/recipe tables; confirmed '
         'pickups, stack merges, herb/treasure recipes, selected-stack ammo '
         'combine reload in both selection orders, normal healing and discard.',
         'No original item-manager identity/layout. Weapon attachments, '
         'bullet-attribute swaps, tuning, yellow-herb max-HP upgrades, partner '
         'healing, scenario-available key use and unarmed weapon discard are '
         'still missing. Generic session storage is not campaign item parity.')
reviewed('game/sce_at.cpp', 'sceAtSetItem sceAtGetItem sceAtGetItem_NoModel',
         ['room_pickups.cpp', 'inventory_case.cpp', 'android_bridge.cpp', 'SubscreenView.java'],
         'Waiting-type0 loose ITA pickups and tested box/barrel drops; '
         'original area/facing checks, persistent taken flags and confirmed '
         'native case transactions with original examined-item assets.',
         'Shoot-down items, enemy-owned/event-gated pickups, scenario item '
         'callbacks and the complete original itemExam transition/render chain '
         'are not transferred.')
for source, names, port, gaps in [
    ('game/emdoor.cpp', 'plemDoorOpen plemDoorKick emDoorActEvtCk',
     ['room_interactions.cpp','leon_preview.cpp'],
     'Scenario room callbacks, every door type, door pane destruction and '
     'complete lock particles are not present.'),
    ('game/emwindow.cpp', 'cEmWindow::DmCk cEmWindow::SetBreakModel '
     'cEmWindow::SetBreakEsp cEmWindow::ChkBreakDir cEmWindow::ExeWindowEvent',
     ['room_interactions.cpp','leon_preview.cpp','world_effects.cpp'],
     'Original protected event state requires actual scenario ownership; '
     'all enemy/character paths, dust and scene-camera chain remain partial.'),
    ('game/obj13.cpp', 'plobjLadderClimb plobjLadderDown plobjLadderReset '
     'objLadderClimbCamMove objLadderDownCamMove objLadderResetCamMove '
     'cObjLadder::breakWindow cObjLadder::setStand cObjLadder::setDowned',
     ['room_ladders.cpp','leon_preview.cpp','ladder_motion.cpp'],
     'All partner/enemy usages, event transforms/visibility and dynamic '
     'ladder scenario registration are not complete.'),
    ('game/embox.cpp', 'cEmBox::DmCk cEmBox::SetBreakEsp cEmBox::SetItem',
     ['object_actions.cpp','destruction_effects.cpp'],
     'Treasure chests and all event callbacks/drop prerequisites remain '
     'outside the normal breakable-box subset.'),
    ('game/embarrel.cpp', 'cEmBarrel::DmCk cEmBarrel::SetBreakEsp cEmBarrel::SetItem',
     ['object_actions.cpp','destruction_effects.cpp'],
     'Rolling barrels, every scenario blast callback and original complete '
     'material/effect ownership are not transferred.'),
]:
    reviewed(source, names, port,
             'Tested source animation/geometry/HP/flag/effect subset; see '
             'corresponding native owner and original-disc regression tests.', gaps)


def mask_literals(text):
    # Preserve offsets/newlines so locations still refer to the real source.
    return re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda m: re.sub(r'[^\n]', ' ', m.group()), text)


def definitions(text):
    masked = mask_literals(text)
    pattern = re.compile(
        r'^[ \t]*(?:inline\s+|static\s+|extern\s+)?'
        r'(?:[\w:<>,*&]+\s+)+([A-Za-z_]\w*(?:::[~\w]+)*)'
        r'\s*\([^;{}]*\)\s*(?:const\s*)?\{', re.M)
    matches = [(m,m.group(1)) for m in pattern.finditer(masked)
               if m.group(1) not in {'if','switch','for','while','catch'}]
    constructors = re.compile(
        r'^[ \t]*([A-Za-z_]\w*)::(~?\1)\s*\([^;{}]*\)'
        r'\s*(?::[^;{}]*)?\{', re.M)
    matches += [(m,m.group(1)+'::'+m.group(2))
                for m in constructors.finditer(masked)]
    result = []
    for match,name in sorted(matches,key=lambda item:item[0].start()):
        opening = masked.index('{', match.start(), match.end())
        depth, end = 1, opening+1
        while end < len(masked) and depth:
            depth += (masked[end]=='{')-(masked[end]=='}')
            end += 1
        result.append((name, match.start(), end,
                       text.count('\n',0,match.start())+1))
    return result


def sources():
    primary = ['player', 'pl_class', 'pl_leon', 'pl_body', 'pl_event', 'pl_dmg',
               'pl_push', 'pl_knife', 'pl_wep', 'pl_sub', 'pl_npc', 'pl_cloth',
               'item', 'puzzle', 'item_model', 'sce_at', 'emitem', 'sscrn',
               'emdoor', 'emwindow', 'emrack', 'embox', 'embarrel', 'obj13', 'objWep']
    selected = {UPSTREAM/'game'/f'{name}.cpp' for name in primary}
    selected.update((UPSTREAM/'wep').glob('pl_*.cpp'))
    # Every actual equipment-object module, including unported gun classes.
    selected.update(UPSTREAM.glob('wep*/obj*.cpp'))
    selected.update(UPSTREAM.glob('wep*/wep*.cpp'))
    # Every original subscreen route, rather than treating custom item/map
    # widgets as a transfer of the original ss_main/ss_item/puzzle engine.
    selected.update((UPSTREAM/'Sscrn').glob('*.cpp'))
    return sorted(path for path in selected if path.is_file())


def build():
    files, functions, tables, buttons = [], [], [], []
    for path in sources():
        relative = str(path.relative_to(UPSTREAM))
        text = path.read_text(errors='replace')
        masked = mask_literals(text)
        found = definitions(text)
        files.append({'source': relative,
                      'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                      'definition_count': len(found)})
        for name, start, end, line in found:
            record = {'source': relative, 'function': name, 'line': line}
            record.update(REVIEW.get((relative,name), {
                'status':'not_transferred',
                'remaining_differences': 'No complete native callable transfer '
                'or verified mapping is recorded. Do not infer support from '
                'a similarly named UI action or an available FCV.'}))
            functions.append(record)
        for match in re.finditer(
                r'(?:static\s+)?void\s*\(\*\s*(\w+)\s*\[[^\]]*\]\s*\)'
                r'\s*\([^;{}]*\)\s*=\s*\{([^}]+)\}',masked):
            owner=next((name for name,start,end,_ in found
                        if start<=match.start()<end),'<global>')
            entries=[]
            for index, token in enumerate(match.group(2).split(',')):
                token=token.strip()
                if not token:continue
                nameMatch=re.search(r'([A-Za-z_]\w*(?:::[\w]+)*)\s*$',token)
                name=nameMatch.group(1) if nameMatch else None
                status='null_slot' if name is None else REVIEW.get(
                    (relative,name),{}).get('status','not_transferred')
                entries.append({'index':index,'function':name,'status':status})
            tables.append({'source':relative,'owner':owner,'table':match.group(1),
                           'line':text.count('\n',0,match.start())+1,'entries':entries})
        for match in re.finditer(r'ActBtn\.set\s*\(\s*(ACT_[A-Z0-9_]+)([^;]+)',masked):
            callback=re.search(r'\(void\s*\*\)\s*(\w+)',match.group(2))
            buttons.append({'source':relative,'line':text.count('\n',0,match.start())+1,
                            'action':match.group(1),'callback':callback.group(1) if callback else None})
    return {'schema_version':1, 'frozen_upstream_commit':COMMIT,
            'commit_origin':'docs/PORTING.md; individual source SHA256 pins below',
            'all_original_actions_identical':False,
            'scope':'Definition inventory of all selected common Leon, weapon '
                    'dispatch, weapon-object, inventory and interaction sources. '
                    'Explicit mappings are reviewed subsets. This does not '
                    'claim that all campaign/scenario/enemy source files are ported.',
            'status_meaning':{
                'partial_adapter':'Source-backed tested subset under native '
                                  'owner; remaining branches/gaps are explicit.',
                'not_transferred':'Original callable/state path absent or not '
                                  'verified as a transfer; cannot claim parity.',
                'null_slot':'Original table has no callback at this index.'},
            'development_override':'Campaign starts with mortal HP. Unlimited HP remains an '
                                   'optional test override; original full death/game-over '
                                   'presentation cannot be asserted identical.',
            'platform_adaptations':['Independent modern right-stick camera/aim mapping',
                                    'Render interpolation between original30-Hz ticks',
                                    'Pointer-safe native asset/state owners and GLES rendering',
                                    'Bounded SAT landing/contact adapter'],
            'm36_subset_evidence':{
                'document':'docs/M36-PLAYER-WALL.md',
                'wall_test':'tools/test_m36_player_wall.cpp',
                'loading_history_test':'tools/test_m36_loading.cpp',
                'loading_history_review':'docs/M36-LOADING-HISTORY.md',
                'scope':'Selected ordinary source400 posed-body contact, '
                        'original mesh/skin world placement, immutable cycle '
                        'queries and per-profile power nibble metadata. '
                        'Existing partial/absent classifications are unchanged.'},
            'files':files,'dispatch_tables':tables,'action_buttons':buttons,
            'functions':functions,
            'definition_status_counts':dict(Counter(f['status'] for f in functions))}


def markdown(result):
    lines=['# M35 – Leon/interaction source coverage', '',
           '**Not all original actions are identical or implemented.** '
           'This report makes the missing work explicit; opening a room or '
           'having an animation asset is not evidence of full gameplay parity.', '',
           f"Frozen source reference: `{COMMIT}` (`docs/PORTING.md`). Every "
           'enumerated source file is also pinned by SHA256 in the JSON report.', '',
           'The machine inventory covers '+str(len(result['files']))+' source files, '+
           str(len(result['functions']))+' callable definitions and '+
           str(len(result['dispatch_tables']))+' original dispatch tables. '
           'Definition counts are an inventory, **not a completion percentage**.', '',
           '## All top-level movement slots', '',
           '| Slot | Original function | Current native coverage |',
           '| --- | --- | --- |']
    move=next(t for t in result['dispatch_tables']
              if t['source']=='game/player.cpp' and t['table']=='Pl_func_move_tbl')
    descriptions={
        'pl_R1_Footwork':'Original normal idle FCV; neck/injury/whole state machine incomplete.',
        'pl_R1_Walk':'Original normal walk/root/turn; collision/IK adapter.',
        'pl_R1_Back':'Original normal back-step/root/turn; collision/IK adapter.',
        'pl_R1_Run':'Original normal run/root/turn; neck/injury/breathing incomplete.',
        'pl_R1_Turn':'Original normal in-place turn; character/injury variants incomplete.',
        'pl_R1_Turn180':'Original normal quick-turn with frame19 cancel; full camera chain partial.',
        'pl_R1_Weapon':'Three playable gun profiles with source power-nibble metadata; other weapons and complete tuning absent.',
        'pl_R1_LevelUp':'Absent: original ledge-climb dispatcher/motions.',
        'pl_R1_LevelDown':'Absent: original ledge descent/1m dispatcher.',
        'pl_R1_ObjPush':'Tested room racks and original FCVs; general manager incomplete.',
        'pl_R1_Aux':'Specific ladder/EM12 callbacks; arbitrary original callbacks absent.',
        'PlKnifeMove':'Original knife FCVs/sweeps for tested targets; lock turns/others partial.',
        'pl_R1_Fance':'Original wide/narrow fence and broken-window branch; bounded landing adapter.',
        'pl_R1_Fall':'Original tested ledge fall/land; automatic falls/water branch incomplete.',
        'pl_R1_Boat':'Absent: boat control and weapon dispatcher.',
        'pl_R1_Ladder':'Original fixed tower profile; all arbitrary lengths/callers incomplete.',
        'pl_R1_Crouch':'Absent: contextual crouching and crouch weapon flows.',
        'pl_R1_JumpFall':'Original PL5e/60 jump and PL5f/61 landing; both directions in r10c tested; bounded SAT adapter.',
        'pl_R1_Whistle':'Absent: partner-command whistle flow.',
    }
    for entry in move['entries']:
        name=entry['function']
        lines.append(f"| {entry['index']} | `{name or '0'}` | " +
                     (descriptions[name] if name else 'Original unused/null slot.')+' |')
    lines += ['', '## Other required original gameplay work', '',
              '- Original generic scenario/event scheduler and camera-cut control; '
              'context-specific action cameras do not install it.',
              '- Complete enemy grabs/QTEs, all damage dispatch and full fatal/game-over '
              'presentation; normal axe hurt/death is a tested subset, not full parity.',
              '- Source weapon sway/recoil RNG, all weapon classes/tuning, shell '
              'ejection and complete water/scenery/material impact dispatch.',
              '- Original cloth/neck/floor IK; current motion IK floor callback '
              'does not expose the world SAT floor.',
              '- Shoot-down, event/enemy-gated items, keys used by scenario, '
              'yellow-herb max-HP upgrades, partner use and attachment state.',
              '- Treasure chests, rolling barrels and complete room-specific '
              'destruction/event callbacks.',
              '- Native subscreen presentation currently uses original disc '
              'assets through a custom controller UI; full original menu '
              'animation/transition, save/load and campaign state are separate work.', '',
              '## Reviewed M36 subset evidence', '',
              '- Ordinary motion/weapon states resolve the source400 sphere at '
              'posed part0 XZ/actorY+400 after motion, including idle and turn. '
              'The actual r101 screenshot wall is verified against original '
              'FCV parts, real skin weights and production GLES world vertices; '
              'see `M36-PLAYER-WALL.md` for results and remaining collision scope.',
              '- WEP01/WEP02/WEP07 shots carry per-profile source power-level '
              'metadata0..6, start at0 and preserve it across room ownership. '
              'This does not install original tuning purchases or the complete '
              'item-manager attributes.',
              '- Lazy cycle queries are checked during live Hokan, blend, '
              'SEQ/root and attached-camera actions in `test_m36_loading.cpp`; '
              'this is initialization/state-history evidence, not new gameplay dispatch.', '',
              '## Source-backed corrections in M35', '',
              '- Newly raised WEP01/WEP02/WEP07 and knife reset initial pitch to '
              'zero, matching original getCameraPitch(); independent live aim remains.',
              '- Source JumpFall shoulder tests, PL5e/60 jump, PL5f/61 landing, '
              'height/contact ordering and sequence20 release now cover eight '
              'real r10c paths in both directions.',
              '- SEQ Se/Free events are consumed once per native30-Hz tick with '
              'source foot/root positions and original FSE dry/water sound routes.',
              '- First r100 axe attacks use original normal Leon hurt/death '
              'FCVs, source difficulty HP/RNG, protected timers and recovery. Campaign HP is mortal.',
              '', '## Retained source-backed M34 corrections', '',
              '- Gun recoil enters the original set40 tail using source sequence '
              'frames. Releasing aim goes directly to weapon-down without '
              'installing another aim or draw motion.',
              '- Shotgun aim uses source weapon7 pitch/body speed0.8 and '
              'six-degree waist limit instead of the handgun profile.',
              '- Case Combine supports the original selected-ammo-stack reload '
              'in both selection orders, preserving equipped weapon and packing.',
              '- Ammo labels follow the actual weapon-to-ammo table: 0x07 '
              'belongs to the WEP09 rifle; 0x00 belongs to the magnum profiles.',
              '- Broken-window directions0/1 use original narrow fence FCVs '
              'and r3=5 (no fanceAdjust), with original frame30 movement cancel '
              'and source footstep cues. Glass is not re-created or re-broken.', '',
              'Detailed per-function limitations and every extracted dispatch '
              'entry are in `PLAYER-ACTION-COVERAGE-M35.json`. Re-run '
              '`python3 tools/audit_player_actions.py` to regenerate both files.', '']
    return '\n'.join(lines)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true',help='Require checked-in reports to match')
    args=parser.parse_args()
    result=build()
    parser_probe='''void declared(int x);
int Probe::step(int x) { if (x) { return x; } else if (x) {} return 0; }
Probe::Probe(int x) : value(x) { if (x) {} }
Probe::~Probe() {}
'''
    assert [x[0] for x in definitions(parser_probe)]==[
        'Probe::step','Probe::Probe','Probe::~Probe'], 'Definition inventory parser'
    top=next(t for t in result['dispatch_tables']
             if t['source']=='game/player.cpp' and t['table']=='Pl_func_move_tbl')
    assert len(top['entries'])==21, 'Every original movement slot must be inventoried'
    assert len(next(t for t in result['dispatch_tables']
                    if t['source']=='game/player.cpp' and t['table']=='Pl_func_tbl')['entries'])==7
    output=ROOT/'docs/PLAYER-ACTION-COVERAGE-M35.json'
    prose=ROOT/'docs/PLAYER-ACTION-COVERAGE-M35.md'
    reports=[(output,json.dumps(result,ensure_ascii=False,indent=2)+'\n'),
             (prose,markdown(result))]
    for path,content in reports:
        if args.check:
            if not path.is_file() or path.read_text()!=content:
                raise SystemExit('Source coverage report is stale: '+str(path.relative_to(ROOT)))
        else:path.write_text(content)
    print('Leon/source coverage:',len(result['files']),'files,',
          len(result['functions']),'definitions,',len(result['dispatch_tables']),
          'dispatch tables;',result['definition_status_counts'])


if __name__=='__main__':main()
