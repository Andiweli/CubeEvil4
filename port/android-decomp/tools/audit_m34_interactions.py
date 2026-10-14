#!/usr/bin/env python3
"""Catalogue all GC etc/obj classes and supplied room records; no game assets copied.

This is a source/data inventory, not a runtime test or proof of game equivalence.
Usage: audit_m34_interactions.py DECODED_ROOMS [OUTPUT_JSON]
"""
from __future__ import annotations
import collections
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from('>H', data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from('>I', data, offset)[0]


def archive_entries(data: bytes) -> dict[str, bytes]:
    count = u32(data, 0)
    offsets = [u32(data, 16 + i * 4) for i in range(count)]
    boundaries = sorted(set(offsets + [len(data)]))
    entries = {}
    for i, start in enumerate(offsets):
        tag = data[16 + count * 4 + i * 4:19 + count * 4 + i * 4].decode('ascii')
        end = next((value for value in boundaries if value > start), len(data))
        entries.setdefault(tag, data[start:end])
    return entries


def etc_definitions() -> dict[int, dict]:
    result = {}
    for name in ('EtcModel.cpp', 'et00.cpp'):
        source = ROOT / 'upstream/re4/src/game' / name
        text = source.read_text()
        for match in re.finditer(r'(?:static\s+)?int\s+Et([0-9a-f]{2})_init\([^;{}]+\)\s*\{', text):
            end, depth = match.end(), 1
            while depth and end < len(text):
                depth += (text[end] == '{') - (text[end] == '}')
                end += 1
            body = text[match.end():end]
            classes = sorted(set(re.findall(r'\b(c(?:Em|Obj)\w+)\s*\*', body)))
            factories = sorted(set(re.findall(r'\b(Set[A-Z]\w+)\(', body)))
            if name == 'et00.cpp':
                classes = ['cEmWindow']
                factories = ['EtXX_init / SetWindow']
            result[int(match[1], 16)] = {
                'source': 'upstream/re4/src/game/' + name,
                'initializer': 'Et' + match[1] + '_init',
                'classes': classes,
                'factories': factories,
            }
    if set(result) != set(range(104)):
        raise RuntimeError('GC registry must contain all 104 Et00..Et67 initializers')
    return result


def current_etc_scope(identifier: int) -> str:
    if identifier in (0, 7, 0x25):
        return 'Shared compatible-asset window runtime; room callbacks and other window kinds pending'
    if identifier in (3, 0x0d):
        return 'Shared compatible-asset interior door runtime; door-part destruction and general door scripts pending'
    if identifier in (4, 5):
        return 'Movable rack only in r101 slots13/15/17; other placements and rack types pending'
    if identifier in (6, 8):
        return 'Object ladders only in r101/r400; other rooms and scripted ladder initial states pending'
    if identifier in (1, 2, 0x11, 0x1e):
        return 'Shared cEmBox types0/1/3/5: eligible bound ITA with authoritative global/local taken flags or original empty boxes, source hitboxes/break effect; scenario-owned records suppressed'
    if identifier in (0x12, 0x2d, 0x3c):
        return 'Non-rolling explosive barrel subset; original two-tick blast delay, hitboxes and break effect'
    return 'Model may render; its original live object/interact/destruction runtime is not implemented'


def obj_registry() -> list[dict]:
    text = (ROOT / 'upstream/re4/include/obj.h').read_text()
    block = text[text.index('enum ID {'):text.index('ID_NUM = 68')]
    factory = (ROOT / 'upstream/re4/src/game/obj.cpp').read_text()
    constructors = dict(re.findall(r'case\s+(ID_\w+):\s*pObj\s*=\s*new\s*\(pObj\)\s*(\w+)\(\);', factory))
    result = []
    for name, identifier in re.findall(r'\b(ID_\w+)\s*=\s*(\d+)', block):
        number = int(identifier)
        if number >= 64:
            continue
        scope = 'Original factory/runtime is not implemented'
        if number == 5:
            scope = 'Efm05 destruction subset only'
        elif number == 19:
            scope = 'Object ladder subset only in r101/r400'
        elif number == 25:
            scope = 'Static ITA pickup models and selected waiting0 pickup transaction'
        elif number == 57:
            scope = 'Tower climb actions only for checked r101/r400 fixed ladders'
        elif number in (11, 33, 36, 43):
            scope = 'Current player handgun/knife/shotgun model and motion subset; not full original weapon-object manager'
        result.append({'id': f'{number:02x}', 'source_enum': name,
                       'original_constructor': constructors.get(name, 'ObjInitFunc[id] registered factory'),
                       'current_scope': scope})
    if len(result) != 64:
        raise RuntimeError('GC object registry must contain all 64 factory IDs')
    return result


def aev_registry(counts: collections.Counter) -> list[dict]:
    header = (ROOT / 'upstream/re4/include/sce_at.h').read_text()
    block = header[header.index('enum SCEAT_ID {'):header.index('SCEAT_ID_ADA_WIRE = 21')]
    source = (ROOT / 'upstream/re4/src/game/sce_at.cpp').read_text()
    handlers = re.findall(r'\{(sceAtFunc_\w+),\s*[01]\}',
                          source[source.index('static SceAtFuncTbl sceAtFunc_tbl[21]'):])[:21]
    registry = []
    for name, identifier in re.findall(r'\b(SCEAT_ID_\w+)\s*=\s*(\d+)', block):
        number = int(identifier)
        scope = 'Parsed area data; original handler/event runtime is not implemented'
        if number == 1:
            scope = 'Original destination/area/facing parsed; selected unlocked room-transition subset'
        elif number == 3:
            scope = 'Selected waiting0 ITA pickup transaction; original acquisition/UI/event branches partial'
        elif number == 13:
            scope = 'Selected generated window fields; full campaign field manager pending'
        elif number == 16:
            scope = 'Fixed ladder actions only for checked r101/r400 records'
        registry.append({'id': f'{number:02x}', 'source_enum': name,
                         'original_handler': handlers[number],
                         'aev_records': counts[number], 'current_scope': scope})
    if len(registry) != 21:
        raise RuntimeError('GC area handler registry must contain all21 kinds')
    return registry


def main() -> None:
    if len(sys.argv) not in (2, 3):
        raise SystemExit(__doc__)
    decoded = Path(sys.argv[1])
    selection = {}
    for line in (ROOT / 'app/src/main/assets/rooms.tsv').read_text().splitlines():
        if not line or line.startswith('#'):
            continue
        disc, source, size, decoded_hash, source_hash = line.replace('\\t', '\t').split('\t')
        selection[Path(source).stem] = {'source_disc': int(disc), 'source_path': source,
                                      'source_sha256': source_hash,
                                      'decoded_size': int(size), 'decoded_sha256': decoded_hash}
    counts = collections.Counter()
    room_counts = collections.Counter()
    aev_kinds = collections.Counter()
    item_ids = collections.Counter()
    waiting = collections.Counter()
    rooms = []
    for path in sorted(decoded.glob('r[0-9a-f][0-9a-f][0-9a-f].dat')):
        data = path.read_bytes()
        selected = selection[path.stem]
        digest = hashlib.sha256(data).hexdigest()
        if len(data) != selected['decoded_size'] or digest != selected['decoded_sha256']:
            raise RuntimeError(f'{path.stem}: archive differs from rooms.tsv selected original')
        entries = archive_entries(data)
        per_room = collections.Counter()
        ets = entries.get('ETS', b'')
        if ets:
            for i in range(u16(ets, 0)):
                per_room[u16(ets, 16 + i * 40)] += 1
        counts.update(per_room)
        room_counts.update(per_room.keys())
        kinds = collections.Counter()
        aev = entries.get('AEV', b'')
        if aev:
            for i in range(u16(aev, 6)):
                kinds[aev[16 + i * 156 + 0x35]] += 1
        aev_kinds.update(kinds)
        ita = entries.get('ITA', b'')
        item_count = 0
        if ita:
            item_count = u16(ita, 6)
            for i in range(item_count):
                record = 16 + i * 156
                item_ids[u16(ita, record + 0x78)] += 1
                waiting[ita[record + 0x46]] += 1
        rooms.append({'room': path.stem, 'sha256': digest, **selected,
                      'ets': {f'{key:02x}': value for key, value in sorted(per_room.items())},
                      'aev_kind_counts': {f'{key:02x}': value for key, value in sorted(kinds.items())},
                      'ita_records': item_count})
    if {room['room'] for room in rooms} != set(selection):
        raise RuntimeError('Decoded directory must contain every rooms.tsv selected room')
    registry = etc_definitions()
    etc = []
    for identifier in range(104):
        etc.append({'id': f'{identifier:02x}', **registry[identifier],
                    'placements': counts[identifier], 'rooms': room_counts[identifier],
                    'current_scope': current_etc_scope(identifier)})
    result = {
        'scope': 'M34 source/data inventory; not a runtime coverage or equivalence claim',
        'room_count': len(rooms), 'ets_records': sum(counts.values()),
        'ets_distinct_ids': len(counts), 'etc_registry': etc,
        'obj_registry': obj_registry(),
        'aev_registry': aev_registry(aev_kinds),
        'aev_kind_counts': {f'{key:02x}': value for key, value in sorted(aev_kinds.items())},
        'ita_records': sum(item_ids.values()),
        'ita_distinct_ids': len(item_ids),
        'ita_item_counts': {f'{key:04x}': value for key, value in sorted(item_ids.items())},
        'ita_waiting_types': {f'{key:02x}': value for key, value in sorted(waiting.items())},
        'rooms': rooms,
    }
    output = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
    if len(sys.argv) == 3:
        Path(sys.argv[2]).write_text(output)
    else:
        print(output, end='')


if __name__ == '__main__':
    main()
