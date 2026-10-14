#!/usr/bin/env python3
"""Inventory original Stage 1 without changing campaign/runtime data.

Uses the supplied US Disc 1 extraction, catalog hashes and original sources.
Optionally cross-checks doors/starts and static scene parsing with host tools.
No DVD payloads, native GC pointers or inferred spawn permissions are exported.
"""
import argparse
import collections
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess


def digest(data):
    return hashlib.sha256(data).hexdigest()


def u16(data, offset):
    return struct.unpack_from('>H', data, offset)[0]


def u32(data, offset):
    return struct.unpack_from('>I', data, offset)[0]


def floats(data, offset, count):
    values = list(struct.unpack_from('>' + str(count) + 'f', data, offset))
    if not all(math.isfinite(x) for x in values):
        raise ValueError('Non-finite original data')
    return values


def room_name(number):
    return f'r{number:03x}'


def catalog(path):
    with path.open(encoding='utf-8') as stream:
        return [dict(zip(('disc', 'path', 'size', 'decoded_sha256', 'source_sha256'), row))
                for row in csv.reader(stream, delimiter='\t') if row and not row[0].startswith('#')]


def archive(data):
    if not 16 <= len(data) <= 64 * 1024 * 1024:
        raise ValueError('Room archive budget')
    count = u32(data, 0)
    if count > 4096 or 16 + count * 8 > len(data):
        raise ValueError('Room archive directory')
    offsets = [u32(data, 16 + i * 4) for i in range(count)]
    if any(x < 16 + count * 8 or x > len(data) for x in offsets):
        raise ValueError('Room archive entry bounds')
    boundaries = sorted(set(offsets + [len(data)]))
    result = []
    occurrences = collections.Counter()
    for i, offset in enumerate(offsets):
        tag = data[16 + count * 4 + i * 4:20 + count * 4 + i * 4][:3].decode('ascii')
        end = next((x for x in boundaries if x > offset), len(data))
        result.append({'index': i, 'tag': tag, 'occurrence': occurrences[tag],
                       'offset': offset, 'extent_with_padding': end - offset,
                       'payload': data[offset:end]})
        occurrences[tag] += 1
    return result


def entry(entries, tag):
    return next((x['payload'] for x in entries if x['tag'] == tag), None)


def room_starts(data):
    result = collections.defaultdict(list)
    stages = u32(data, 0)
    if not 1 <= stages <= 16:
        raise ValueError('RoomInfo stage budget')
    for stage in range(stages):
        offset = u32(data, 4 + stage * 4)
        if not offset:
            continue
        count = u32(data, offset)
        if offset + 4 + 32 * count > len(data):
            raise ValueError('RoomInfo record bounds')
        for i in range(count):
            p = offset + 4 + 32 * i
            number = u16(data, p + 2)
            if number >> 8 != stage:
                raise ValueError('RoomInfo stage mismatch')
            names = []
            for q in (20, 24, 28):
                pointer = u32(data, p + q)
                names.append(data[pointer:data.index(0, pointer)].decode('shift_jis') if pointer else '')
            result[room_name(number)].append({
                'point': len(result[room_name(number)]), 'offset': p,
                'flags': u16(data, p), 'valid': bool(u16(data, p) & 1),
                'position': floats(data, p + 4, 3), 'heading': floats(data, p + 16, 1)[0],
                'editor_name': names[0], 'raw_record_sha256': digest(data[p:p + 32])})
    return result


def areas(data, tag):
    if data is None:
        return []
    version = 0x104 if tag == 'AEV' else 0x105
    if data[:4] != (tag + '\0').encode() or u16(data, 4) != version:
        raise ValueError(f'Unsupported {tag} version')
    count = u16(data, 6)
    if count > 256 or 16 + count * 156 > len(data):
        raise ValueError(f'{tag} record bounds')
    result = []
    for i in range(count):
        p = 16 + 156 * i
        raw = data[p:p + 156]
        result.append({'record_index': i, 'number': raw[0x36], 'type': raw[0x35],
                       'enabled': raw[0x34], 'hit_type': raw[0x37], 'trigger_type': raw[0x38],
                       'target_type': raw[0x39], 'task_level': raw[0x3a], 'priority': raw[0x44],
                       'kind': raw[0x45], 'waiting_type': raw[0x46], 'waiting_no': raw[0x47],
                       'parameter_pointer': u32(raw, 0x3c), 'function_pointer': u32(raw, 0x40),
                       'parent_pointer': u32(raw, 0x4c), 'country': raw[0x52],
                       'area_enabled': raw[4], 'area_type': raw[5],
                       'floor': floats(raw, 8, 1)[0], 'height': floats(raw, 12, 1)[0],
                       'radius': floats(raw, 16, 1)[0], 'points': floats(raw, 20, 8),
                       'raw_record_sha256': digest(raw), '_raw': raw})
    if len({x['number'] for x in result}) != count:
        raise ValueError(f'Duplicate {tag} area number')
    return result


def door_fields(area):
    raw = area['_raw']
    return {**{k: v for k, v in area.items() if k != '_raw'},
            'destination_room': room_name(u16(raw, 0x6c)),
            'destination_part': raw[0x74], 'position': floats(raw, 0x5c, 3),
            'heading': floats(raw, 0x68, 1)[0], 'key_id': raw[0x6e], 'key_flag': raw[0x6f],
            'exit_function_pointer': u32(raw, 0x70), 'exit_parameter_pointer': u32(raw, 0x78),
            'fade': raw[0x77], 'hit_direction': struct.unpack_from('b', raw, 0x48)[0],
            'hit_open_angle': struct.unpack_from('b', raw, 0x49)[0]}


def source_inventory(path, root):
    text = path.read_text(encoding='utf-8')
    lines = text.splitlines()
    # Lexical inventory only. Not a complete C++ call graph or scenario proof.
    categories = {
        'transition': r'SceAtExecRoomJump|SceAtSetRoom|SceAtSetDoor|SceSetChapterEnd|StageEnd|GameStageEnd',
        'scenario_area': r'SceAt(?:DataSet|Set|ItemEvent|SetEnable|Add|Del|RoomDoor)',
        'collision': r'SatMgr\.|EatMgr\.|createSat|createBoxSat|createFloorSat',
        'enemy': r'EmSet|EmList|EmRead|EmSetChange|\.setEm\(|->setEm\(',
        'object': r'SmdSet|SmdGet|DoorReplace|OpenBox|cEmRack|cEmWindow|Obj.*Set|->setDelete',
        'flags': r'Rsf(?:Check|Set|Clear)|ScfFlag|Key_flg|Item_flg|RoomData\.',
        'partner': r'PlNpc|Npc|Ashley|pAS|PL_SUB|SubInit|subInit',
        'event': r'EvtMgr\.|SceEvent|Event\.Flg|SetCallBack|Evt.*CallBack',
        'render_audio': r'TexRender|LightMgr|Snd(?:Room|Str)|Thunder|AtEffTbl|EstSet',
    }
    references = []
    for n, line in enumerate(lines, 1):
        code = line.split('//', 1)[0].strip()
        if not code or code.startswith(('#', '/*', '*')):
            continue
        for category, pattern in categories.items():
            if re.search(pattern, code):
                references.append({'category': category, 'line': n, 'expression': code})
    functions = re.findall(r'^\s*(?:static\s+)?(?:inline\s+)?[\w:*<>]+(?:\s*\*)?\s+([\w:]+)\([^;\n]*\)\s*\n?\{', text, re.M)
    shared = sorted(set(re.findall(r'\b(r1[0-9a-f]{2}_[A-Za-z0-9_]+)\s*\(', text, re.I)))
    shared = [x for x in shared if x[:4].lower() != path.stem.lower() and x not in functions]
    asset_literals = sorted(set(re.findall(r'"((?:evd|evt|em|etc|snd|bio4evt)/[^"\n]+)"', text)))
    return {'path': path.relative_to(root).as_posix(), 'sha256': digest(path.read_bytes()),
            'lines': len(lines), 'function_definitions': functions,
            'shared_room_symbols': shared, 'asset_literals': asset_literals,
            'cross_room_named_definitions': [x for x in functions if re.match(r'r1[0-9a-f]{2}_', x, re.I) and x[:4].lower() != path.stem.lower()],
            'references': references}


def enemy_lists(game, scope):
    result = collections.defaultdict(list)
    # Main Leon campaign lists selected by original game/stage.cpp, not omake modes.
    for list_no in (0, 1):
        path = game / f'etc/emleon{list_no:02d}.esl'
        data = path.read_bytes()
        if len(data) != 255 * 32:
            raise ValueError('ESL must contain 255 original records')
        for i in range(255):
            raw = data[i * 32:(i + 1) * 32]
            room = room_name(u16(raw, 24))
            if room not in scope:
                continue
            result[room].append({'list_no': list_no, 'list_index': i, 'id': raw[1],
                                 'be_flags': raw[0], 'type': raw[2], 'set_no': raw[3],
                                 'flags': u32(raw, 4), 'hp': u16(raw, 8),
                                 'emset_no': raw[10], 'character': raw[11],
                                 'position': [x * 10 for x in struct.unpack_from('>3h', raw, 12)],
                                 'rotation': [x * math.pi / 16384 for x in struct.unpack_from('>3h', raw, 18)],
                                 'guard_radius': struct.unpack_from('>h', raw, 26)[0] * 1000,
                                 'raw_record_sha256': digest(raw)})
    return result


def native_check(tool, path, room, info_path, doors, starts):
    run = subprocess.run([str(tool), str(path), room[1:], str(info_path)],
                         text=True, capture_output=True, timeout=30, check=False)
    result = {'exit_code': run.returncode, 'door_count': 0,
              'door_field_values_checked': 0, 'start_records_checked': 0,
              'static_ground_samples': [], 'available_door_numbers': []}
    if run.returncode:
        result['error'] = run.stderr.strip()
        return result
    if room == 'r120':
        # The ordinary tool path stops at the absent AEV; validate the intro's
        # original RoomInfo records through its independent info entry point.
        info_run = subprocess.run([str(tool), '--info', str(info_path)],
                                  text=True, capture_output=True, timeout=30, check=True)
        run.stdout += '\n' + '\n'.join(line for line in info_run.stdout.splitlines()
                                      if line.startswith('S ') and int(line.split()[1]) == 0x120)
    seen = set()
    start_seen = set()
    for line in run.stdout.splitlines():
        values = line.split()
        if not values:
            continue
        if values[0] == 'D':
            d = next(x for x in doors if x['number'] == int(values[2]))
            expected = [int(room[1:], 16), d['number'], d['record_index'],
                        int(d['destination_room'][1:], 16), d['destination_part'],
                        d['key_id'], d['key_flag'], d['enabled'], d['hit_type'],
                        d['trigger_type'], d['target_type'], d['priority'],
                        d['area_enabled'], d['area_type'], d['floor'], d['height'],
                        d['radius'], *d['points'], *d['position'], d['heading'],
                        d['hit_direction'], d['hit_open_angle']]
            actual = [float(x) for x in values[1:-2]]
            if actual != expected:
                raise ValueError(f'{room}: native door fields differ: area {d["number"]}')
            if digest(bytes.fromhex(values[-1])) != d['raw_record_sha256']:
                raise ValueError(f'{room}: native raw door differs')
            d['native_available_at_baseline'] = bool(int(values[-2]))
            if d['native_available_at_baseline']:
                result['available_door_numbers'].append(d['number'])
            result['door_field_values_checked'] += len(expected)
            seen.add(d['number'])
        elif values[0] == 'S':
            point = int(values[3])
            s = next(x for x in starts if x['point'] == point)
            expected = [int(room[1:], 16), s['flags'], point, s['offset'],
                        *s['position'], s['heading']]
            if [float(x) for x in values[1:-1]] != expected or digest(bytes.fromhex(values[-1])) != s['raw_record_sha256']:
                raise ValueError(f'{room}: native RoomInfo differs')
            start_seen.add(point)
        elif values[0] == 'G':
            sample = {'point': int(values[2]), 'ground_found': bool(int(values[3]))}
            if len(values) > 4:
                sample.update({'delta_y': float(values[4]), 'capsule_clear': bool(int(values[5])),
                               'polygon': int(values[6])})
            result['static_ground_samples'].append(sample)
    if seen != {d['number'] for d in doors}:
        raise ValueError(f'{room}: native door count differs')
    # room_flow_tool deliberately returns before RoomInfo for archives without AEV (r120).
    if doors or room == 'r120':
        if start_seen != {s['point'] for s in starts}:
            raise ValueError(f'{room}: native start count differs')
    result['door_count'] = len(seen)
    result['start_records_checked'] = len(start_seen)
    return result


def write_tsv(path, fields, rows):
    with path.open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fields, delimiter='\t', lineterminator='\n', extrasaction='ignore')
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-files', type=Path, required=True, help='Disc 1 extraction/files')
    parser.add_argument('--decoded-rooms', type=Path, required=True)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--room-flow-tool', type=Path)
    parser.add_argument('--room-resource-tool', type=Path)
    args = parser.parse_args()
    root, game = args.project.resolve(), args.game_files.resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    selected = [x for x in catalog(root / 'app/src/main/assets/rooms.tsv')
                if re.fullmatch(r'St1/r1[0-9a-f]{2}\.das', x['path'])]
    selected.sort(key=lambda x: x['path'])
    scope = [Path(x['path']).stem for x in selected]
    stage = root / 'upstream/re4/src/st1/st1.cpp'
    registered = sorted(room_name(int(x) + 0x100) for x in re.findall(r'set\((\d+),\s*R1', stage.read_text()))
    if scope != sorted(registered) or len(scope) != 29:
        raise ValueError('Stage registration/catalog mismatch; review scope before extending')
    info_path = game / 'Debug/roominfo.dat'
    info_bytes = info_path.read_bytes()
    starts = room_starts(info_bytes)
    enemies = enemy_lists(game, scope)
    block_catalog = catalog(root / 'app/src/main/assets/blocks.tsv')
    version = re.search(r'versionName\s+[\'"]([^\'"]+)', (root / 'app/build.gradle').read_text()).group(1)
    report = {'schema': 1, 'scope': 'US original Stage 1, main Leon campaign',
              'baseline': version + '; preparation only',
              'upstream_commit': json.loads((root / 'docs/upstream-audit.json').read_text())['upstream_commit'],
              'audit_script_sha256': digest(Path(__file__).read_bytes()),
              'host_tools': {key: digest(path.read_bytes()) for key, path in
                             [('room_flow_tool', args.room_flow_tool), ('room_resource_tool', args.room_resource_tool)] if path},
              'roominfo_sha256': digest(info_bytes), 'stage_registration_sha256': digest(stage.read_bytes()),
              'excluded_missing_ids': ['r110', 'r114', 'r115', 'r116'],
              'limitations': [
                  'AEV graph is authored data, not proof of reachable story states.',
                  'Script jumps, rewritten destinations, events and chapter restarts require code review.',
                  'ESL rows include inactive/event actors and props; row count is not enemy count. Main lists 00/01 only.',
                  'Source references are lexical candidates, not a complete C++ call graph.',
                  'Static start ground checks exclude room-init and moving collision; not walkability acceptance.',
                  'Native scene checks exclude block streaming, scripts and device/GLES visual comparison.'],
              'rooms': [], 'script_jump_candidates': []}
    report['shared_original_files'] = [
        {'path': name, 'size': (game / name).stat().st_size,
         'sha256': digest((game / name).read_bytes())}
        for name in ('Debug/roominfo.dat', 'etc/emleon00.esl', 'etc/emleon01.esl')]
    report['stage_rel_reference_checks'] = []
    for line in (root / 'app/src/main/assets/references.tsv').read_text().splitlines():
        if not line or line.startswith('#'):
            continue
        disc, path, offset, size, sha1 = line.split('\t')
        if disc != '1' or not re.fullmatch(r'files/Rel/st1_[0-3]\.rel', path):
            continue
        rel = (game / path.removeprefix('files/')).read_bytes()
        if int(offset) != 0 or len(rel) != int(size) or hashlib.sha1(rel).hexdigest() != sha1:
            raise ValueError(f'Original Stage 1 REL reference mismatch: {path}')
        report['stage_rel_reference_checks'].append({'path': path, 'size': len(rel), 'sha1': sha1, 'sha256': digest(rel)})
    if len(report['stage_rel_reference_checks']) != 4:
        raise ValueError('Incomplete original Stage 1 REL references')
    for row in selected:
        room = Path(row['path']).stem
        decoded_path = args.decoded_rooms / (room + '.dat')
        data = decoded_path.read_bytes()
        source = (game / row['path']).read_bytes()
        if len(data) != int(row['size']) or digest(data) != row['decoded_sha256'] or digest(source) != row['source_sha256']:
            raise ValueError(f'{room}: original asset hash mismatch')
        entries = archive(data)
        aev, ita = areas(entry(entries, 'AEV'), 'AEV'), areas(entry(entries, 'ITA'), 'ITA')
        doors = [door_fields(x) for x in aev if x['type'] == 1]
        ladders = [{**{k: v for k, v in x.items() if k != '_raw'},
                    'position': floats(x['_raw'], 0x5c, 3), 'heading': floats(x['_raw'], 0x68, 1)[0],
                    'level': struct.unpack_from('b', x['_raw'], 0x6c)[0],
                    'ladder_type': x['_raw'][0x6d], 'position_set': x['_raw'][0x6e]}
                   for x in aev if x['type'] == 0x10]
        items = [{**{k: v for k, v in x.items() if k != '_raw'},
                  'source_id': u16(x['_raw'], 0x78), 'item_flag': u16(x['_raw'], 0x7a),
                  'auto_flag': u16(x['_raw'], 0x7e), 'effect': x['_raw'][0x80],
                  'control_flags': x['_raw'][0x84], 'authored_position': floats(x['_raw'], 0x5c, 3)}
                 for x in ita if x['type'] == 3]
        blocks = []
        for block in block_catalog:
            if Path(block['path']).stem.startswith(room + '_'):
                raw_block = (game / block['path']).read_bytes()
                if digest(raw_block) != block['source_sha256'] or digest(raw_block) != block['decoded_sha256']:
                    raise ValueError(f'{room}: original block hash mismatch')
                blocks.append({'path': block['path'], 'size': len(raw_block), 'sha256': digest(raw_block)})
        src = source_inventory(root / f'upstream/re4/src/st1/{room}.cpp', root)
        for ref in src['references']:
            if ref['category'] == 'transition':
                report['script_jump_candidates'].append({'room': room, 'path': src['path'], **ref})
        r = {'id': room, 'disc': int(row['disc']), 'path': row['path'],
             'decoded_size': len(data), 'decoded_sha256': row['decoded_sha256'],
             'source_sha256': row['source_sha256'], 'starts': starts.get(room, []),
             'archive_entries': [{k: v for k, v in x.items() if k != 'payload'} for x in entries],
             'aev_type_counts': dict(sorted(collections.Counter(x['type'] for x in aev).items())),
             'aev_records': [{k: v for k, v in x.items() if k != '_raw'} for x in aev],
             'doors': doors, 'ladders': ladders, 'ita_records': len(ita), 'items': items,
             'blocks': blocks, 'campaign_esl_records': enemies.get(room, []), 'original_source': src}
        if args.room_flow_tool:
            r['native_flow_check'] = native_check(args.room_flow_tool.resolve(), decoded_path.resolve(), room,
                                                  info_path, doors, r['starts'])
        if args.room_resource_tool:
            run = subprocess.run([str(args.room_resource_tool.resolve()), str(decoded_path.resolve())],
                                 capture_output=True, text=True, check=False, timeout=30)
            summary = next((x for x in run.stdout.splitlines() if x.startswith('S ')), None)
            fields = ['included', 'skipped', 'triangles', 'textured_triangles', 'fallback_triangles',
                      'common_texture_triangles', 'common_alpha_triangles', 'common_placements']
            r['native_static_scene_check'] = {'exit_code': run.returncode,
                'scene': dict(zip(fields, map(int, summary.split()[1:]))) if summary else None,
                'error': run.stderr.strip() or None}
        report['rooms'].append(r)
    pairs = {(r['id'], d['destination_room']) for r in report['rooms'] for d in r['doors']}
    transition_rows, room_rows, source_rows = [], [], []
    for r in report['rooms']:
        counts = collections.Counter(x['tag'] for x in r['archive_entries'])
        esl = collections.Counter(x['list_no'] for x in r['campaign_esl_records'])
        room_rows.append({'room': r['id'], 'editor_name': next((x['editor_name'] for x in r['starts'] if x['editor_name']), ''),
            'disc': r['disc'], 'starts': len(r['starts']), 'aev_records': len(r['aev_records']),
            'door_records': len(r['doors']), 'ladders': len(r['ladders']), 'ita_records': r['ita_records'],
            'scr_at_records': r['aev_type_counts'].get(11, 0), 'sat_entries': counts['SAT'], 'blocks': len(r['blocks']),
            'esl00_records': esl[0], 'esl01_records': esl[1],
            'targets': ','.join(sorted(set(d['destination_room'] for d in r['doors']))),
            'source_functions': len(r['original_source']['function_definitions']),
            'native_available_doors': ','.join(str(x) for x in r.get('native_flow_check', {}).get('available_door_numbers', []))})
        for d in r['doors']:
            d['reciprocal_aev_pair_present'] = (d['destination_room'], r['id']) in pairs
            transition_rows.append({'source': r['id'], 'area_no': f'0x{d["number"]:02x}',
                'target': d['destination_room'], 'part': d['destination_part'], 'key_id': f'0x{d["key_id"]:02x}',
                'key_flag': d['key_flag'], 'enabled': d['enabled'], 'trigger': d['trigger_type'],
                'parented': bool(d['parent_pointer']),
                'authored_callback': any(d[x] for x in ('function_pointer', 'parameter_pointer', 'exit_function_pointer', 'exit_parameter_pointer')),
                'reciprocal_aev_pair_present': d['reciprocal_aev_pair_present'],
                'native_available_at_baseline': d.get('native_available_at_baseline', 'NOT_CHECKED'),
                'destination_x': d['position'][0], 'destination_y': d['position'][1], 'destination_z': d['position'][2],
                'destination_heading': d['heading']})
        for ref in r['original_source']['references']:
            source_rows.append({'room': r['id'], 'source': r['original_source']['path'], **ref})
    report['totals'] = {'rooms': len(scope), 'aev_door_records': len(transition_rows),
        'distinct_aev_directed_pairs': len(pairs),
        'internal_aev_door_records': sum(x['target'] in scope for x in transition_rows),
        'external_aev_targets': sorted({x['target'] for x in transition_rows if x['target'] not in scope}),
        'same_room_aev_door_records': sum(x['source'] == x['target'] for x in transition_rows),
        'ladder_records': sum(len(x['ladders']) for x in report['rooms']),
        'ita_records': sum(x['ita_records'] for x in report['rooms']),
        'scr_at_records': sum(x['aev_type_counts'].get(11, 0) for x in report['rooms']),
        'blocks': sum(len(x['blocks']) for x in report['rooms']),
        'campaign_esl_records': sum(len(x['campaign_esl_records']) for x in report['rooms']),
        'native_door_field_values_checked': sum(x.get('native_flow_check', {}).get('door_field_values_checked', 0) for x in report['rooms']),
        'native_start_records_checked': sum(x.get('native_flow_check', {}).get('start_records_checked', 0) for x in report['rooms']),
        'native_flow_failures': [x['id'] for x in report['rooms'] if x.get('native_flow_check', {}).get('exit_code', 0)],
        'native_static_scene_failures': [x['id'] for x in report['rooms'] if x.get('native_static_scene_check', {}).get('exit_code', 0)]}
    (args.out / 'VILLAGE-AUDIT.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    write_tsv(args.out / 'VILLAGE-ROOMS.tsv', list(room_rows[0]), room_rows)
    write_tsv(args.out / 'VILLAGE-TRANSITIONS.tsv', list(transition_rows[0]), transition_rows)
    write_tsv(args.out / 'VILLAGE-SOURCE.tsv', list(source_rows[0]), source_rows)
    text = ['# Original Stage 1: reproducible data audit', '',
            'Preparation inventory; no runtime or campaign completion is implied.', '',
            '## Totals', '', *[f'- {k}: {v}' for k, v in report['totals'].items()], '',
            '## Rooms and authored AEV targets', '',
            '| Room | Original editor name | AEV doors | Ladders | ITA | ESL 00 / 01 | AEV targets |',
            '| --- | --- | ---: | ---: | ---: | --- | --- |']
    text += [f'| {x["room"]} | {x["editor_name"]} | {x["door_records"]} | {x["ladders"]} | {x["ita_records"]} | {x["esl00_records"]} / {x["esl01_records"]} | {x["targets"] or "scripted entry"} |' for x in room_rows]
    text += ['', '## Scope and interpretation', '', *['- ' + x for x in report['limitations']], '',
             'AEV type 0x10 is a ladder, not a room edge. Reciprocal pair presence does not establish that either direction is enabled.',
             'r120 has no AEV; its r120.cpp jump to r100 is part of the scripted opening.',
             'r108 targets r117 in authored data, while r117 targets r118: review day/night scenario switching, not a missing reverse door.',
             'r10e has two self-room transitions with distinct parts; preserve their entry context.',
             'r10f -> r200 is the Stage 2 boundary and needs a transition contract although Stage 2 interiors are outside this scope.', '',
             '## Native diagnostics', '',
             'Native availability comes from the supplied current room_flow_tool, not an audit allowlist.',
             'Ground samples use only base SAT, radius 300, height 1600, search +/-250.01; scene counts use default static placement.', '']
    for r in report['rooms']:
        for key in ('native_flow_check', 'native_static_scene_check'):
            check = r.get(key, {})
            if check.get('exit_code'):
                text.append(f'- {r["id"]} / {key}: {check.get("error")}')
    (args.out / 'VILLAGE-AUDIT.md').write_text('\n'.join(text) + '\n', encoding='utf-8')
    print(json.dumps(report['totals'], ensure_ascii=False))


if __name__ == '__main__':
    main()
