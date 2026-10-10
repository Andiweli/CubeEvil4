#!/usr/bin/env python3
"""Generate native catalog and reviewed original registration components; no assets needed."""
import argparse
import csv
import hashlib
import importlib.util
import json
import re
import sys
from pathlib import Path
sys.dont_write_bytecode = True


def function(text, signature):
    start = text.index(signature + '\n{')
    pos = text.index('{', start)
    depth = 1
    end = pos + 1
    while depth:
        if text[end] == '{': depth += 1
        if text[end] == '}': depth -= 1
        end += 1
    return text[start:end]


def digest(path):
    return hashlib.sha256(path.read_text(encoding='utf-8').replace('\r\n', '\n').encode()).hexdigest()


def generate(root, output):
    cfg = root/'config/G4BE08'
    reviewed = json.loads((root/'port/config/module-sources.json').read_text(encoding='utf-8'))
    for path, expected in reviewed.items():
        if digest(root/path) != expected:
            raise ValueError('Original module fingerprint mismatch: ' + path)
    text = (cfg/'config.yml').read_text(encoding='utf-8')
    listed = re.findall(r'^- object: (.+)\n  name: (\S+)\n', text, re.M)
    if not listed or text.count('\n- object:') != len(listed):
        raise ValueError('unsupported module configuration grammar')
    spec = importlib.util.spec_from_file_location('original_module_units', cfg/'modules.py')
    units = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(units)
    modules = []
    symbols = {}
    sources = {}
    filetbl = re.findall(r'\{"([^"\n]+)", 0\}', (root/'src/game/dvd.cpp').read_text(encoding='utf-8').split('};', 1)[0])
    for obj, name in listed:
        meta = json.loads((cfg/'modules'/name/'rel.json').read_text(encoding='utf-8'))
        if meta['name'] != name or meta['object'] != obj:
            raise ValueError('REL configuration identity differs')
        meta['file_index'] = next((i for i, p in enumerate(filetbl) if p.lower() == obj.removeprefix('files/').lower()), 65535)
        meta['units'] = [dict(unit=u[0], source='src/' + (u[2] if len(u) > 2 and u[2] else u[0]))
                         for u in units.UNITS.get(name, [(f'{name}/{name}.cpp', None)])]
        sources[name] = [u['source'] for u in meta['units']]
        symbols[name] = list(csv.DictReader((cfg/'modules'/name/'sym_map.tsv').open(), delimiter='\t'))
        modules.append(meta)
    byname = {m['name']: m for m in modules}
    if len(byname) != len(modules) or len({m['module_id'] for m in modules}) != len(modules):
        raise ValueError('duplicate module identity')
    for m in modules:
        if any(name not in byname for name in m['links']): raise ValueError('unresolved import module')
        m['imports'] = [byname[n]['module_id'] for n in m['links']]
    roomtext = (root/'src/game/roomdata.cpp').read_text(encoding='utf-8')
    rows = re.findall(r'ROOM_DATA St(\d)_data_tbl\[(\d+)\] = \{(.*?)\n\};', roomtext, re.S)
    if len(rows) != 5: raise ValueError('original stage tables missing')
    roomrows, declarations = {}, []
    for stage, size, content in rows:
        entries = [tuple(map(int, r)) for r in re.findall(r'\{(\d+), (\d+), (\d+), (\d+), (\d+)\}', content)]
        if len(entries) != int(size): raise ValueError('original stage size differs')
        roomrows[int(stage)] = entries
        declarations.append(f'ROOM_DATA St{stage}_data_tbl[{size}] = {{{content}\n}};')
    bindings = {}
    for stage in range(1, 5):
        s = (root/f'src/st{stage}/st{stage}.cpp').read_text(encoding='utf-8')
        bindings[stage] = [(int(n), a, b) for n, a, b in re.findall(r'set\((\d+), (R[0-9a-f]+Init), (R[0-9a-f]+Main)\);', function(s, 'void setTbl()'))]
        if len({n for n, _, _ in bindings[stage]}) != len(bindings[stage]): raise ValueError('duplicate stage binding')
    # Reviewed complete room bodies only: no inferred function replacement.
    small = {}
    for path in reviewed:
        p = root/path
        if not re.fullmatch(r'r[0-9a-f]+\.cpp', p.name): continue
        s = p.read_text(encoding='utf-8')
        room = int(p.stem[1:], 16)
        init, main = 'R'+p.stem[1:]+'Init', 'R'+p.stem[1:]+'Main'
        structure = re.search(r'struct (\w+) \{.*?\n\};', s, re.S)
        pointer = re.search(r'static (\w+)\* (\w+);', s)
        if not structure or not pointer: raise ValueError('reviewed work missing')
        small[room] = dict(path=path, source=s, init=init, main=main, struct=structure[0], work=pointer[1], pointer=pointer[2],
                           imported=room == 0x120)
    rooms = []
    for stage, pairs in bindings.items():
        for n, init, main in pairs:
            row = roomrows[stage][n]
            if row[2] >= len(filetbl): raise ValueError('room FileTbl outside bounds')
            owners = [m for m in modules if m['file_index'] == row[2]]
            if len(owners) != 1: raise ValueError('room REL owner missing/ambiguous')
            owner = owners[0]
            variant = [m['name'] for m in modules if f'src/st{stage}/r{stage}{n:02x}.cpp' in sources[m['name']]]
            # st3 defaults to the large single unit: symbols supply real variants.
            variant = sorted(set(variant + [m['name'] for m in modules if any(r['demangled'] == init for r in symbols[m['name']])]))
            if owner['name'] not in variant: raise ValueError('primary room source/symbol is absent')
            rid = (stage << 8) | n
            rooms.append(dict(id=rid, file_index=row[2], module=owner['module_id'], save=bool(row[0]), init=init, main=main,
                              variants=variant, compiled=rid in small,
                              resident_imports=rid in small and small[rid]['imported']))
    collisions = {}
    for name, rows_ in symbols.items():
        for r in rows_:
            if r['scope'] == 'global': collisions.setdefault(r['name'], []).append(name)
    collisions = {k: sorted(set(v)) for k, v in collisions.items() if len(set(v)) > 1}
    manifest = dict(schema='RE4-NATIVE-MODULES-1', modules=modules, rooms=rooms,
                    stage_rows={str(k): v for k, v in roomrows.items()}, symbol_collisions=collisions,
                    source_hashes={str(p.relative_to(root)).replace('\\', '/'): digest(p)
                                   for p in sorted(set([cfg/'config.yml', cfg/'modules.py', root/'src/game/dvd.cpp', root/'src/game/roomdata.cpp'] +
                                                       [cfg/'modules'/m['name']/f for m in modules for f in ['rel.json', 'splits.txt', 'sym_map.tsv']] +
                                                       [root/p for p in reviewed]))})
    encoded = json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode()
    identity = hashlib.sha256(encoded).hexdigest()
    manifest['sha256'] = identity
    output.mkdir(parents=True, exist_ok=True)
    (output/'module-manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True)+'\n', encoding='utf-8')
    q = json.dumps
    cpp = ['#include "re4/port/modules.h"', '#include "module_original.h"', 'namespace re4::port {',
           'const ModuleCatalog &originalModuleCatalog() { static const ModuleCatalog c{', q(identity)+', {']
    cpp += ['{' + f"{m['module_id']}, {m['file_index']}, {q(m['name'])}, {q(m['object'])}, " + '{'+','.join(map(str, m['imports']))+'},'+f"{m['sections']['.ctors']},{m['sections']['.dtors']},{m['sections'].get('.bss',0)}"+'},' for m in modules]
    cpp += ['}, {']
    cpp += ['{' + f"{r['id']}, {r['file_index']}, {r['module']}, {str(r['save']).lower()}, {str(r['compiled']).lower()}, {str(r['compiled']).lower()}, {str(r['resident_imports']).lower()}, {q(r['init'])}, {q(r['main'])}" + '},' for r in rooms]
    cpp += ['}}; return c; }', '}']
    (output/'module_catalog.cpp').write_text('\n'.join(cpp)+'\n', encoding='utf-8')
    stagecpp = ['#include "module_original.h"', '#include "types.h"', '#include <stdexcept>', 'namespace re4::port::modules_original {']
    stageMods = [m for m in modules if re.fullmatch(r'st[1-4]_\d+', m['name'])]
    for m in stageMods:
        name, stage = m['name'], int(m['name'][2])
        s = (root/f'src/st{stage}/st{stage}.cpp').read_text(encoding='utf-8')
        owned = [r for r in rooms if r['module'] == m['module_id'] and r['compiled']]
        stagecpp += [f'namespace {name} {{']
        for r in owned:
            b = small[r['id']]
            if b['imported']:
                stagecpp += [f"void {b['init']}() {{ ModuleContext::active().importedRoom({r['id']}, true); }}",
                             f"void {b['main']}() {{ ModuleContext::active().importedRoom({r['id']}, false); }}"]
                continue
            stagecpp += [b['struct'], f"{b['work']}*& work_{b['pointer']}();",
                         f"#define {b['pointer']} (work_{b['pointer']}())",
                         '#line 1 ' + q((root/b['path']).as_posix()),
                         function(b['source'], 'void ' + b['init'] + '()'),
                         function(b['source'], 'void ' + b['main'] + '()'),
                         '#undef ' + b['pointer']]
        stagecpp += ['void prologOriginal(); void epilogOriginal();', 'class State final : public NativeModuleState { public:', *declarations,
                     'StageTbl Room_data_tbl[10] = {{St0_data_tbl,67,0},{St1_data_tbl,33,0},{St2_data_tbl,46,0},{St3_data_tbl,52,0},{St4_data_tbl,18,0},{},{},{},{},{}};',
                     'struct Snapshot final : ModuleSnapshot { ModuleContext::Work work; };']
        for r in owned:
            b = small[r['id']]
            if b['imported']: continue
            stagecpp += [f"{b['work']}* {b['pointer']} = nullptr;"]
        stagecpp += ['std::unique_ptr<ModuleSnapshot> save(ModuleContext& c) const override { auto s = std::make_unique<Snapshot>(); s->work = c.work(); return s; }',
                     'void restore(ModuleContext& c, const ModuleSnapshot& base) override { const auto& s = dynamic_cast<const Snapshot&>(base); c.restoreWork(s.work);']
        for r in owned:
            b = small[r['id']]
            if b['imported']: continue
            stagecpp += [f"{b['pointer']} = static_cast<{b['work']}*>(c.roomWork({r['id']}));"]
        stagecpp += ['}', 'void prolog(ModuleContext& c) override { ModuleContext::Scope scope(c); prologOriginal();']
        for n, _, _ in bindings[stage]:
            stagecpp += [f'c.registerRoom({(stage<<8)|n}, St{stage}_data_tbl[{n}].pInit, St{stage}_data_tbl[{n}].pMain);']
        stagecpp += ['}', 'void epilog(ModuleContext& c) override { ModuleContext::Scope scope(c); epilogOriginal(); }', 'void resetRoom(ModuleContext&, std::uint16_t room) override {']
        for r in owned:
            b = small[r['id']]
            if b['imported']:
                stagecpp += [f"if (room == {r['id']}) return;"]
                continue
            stagecpp += [f"if (room == {r['id']}) {{ {b['pointer']} = nullptr; return; }}"]
        stagecpp += ['(void)room; throw std::logic_error("Unbound room reset"); }', 'void invoke(ModuleContext&, std::uint16_t, bool) override;', '};',
                     'State& state() { return static_cast<State&>(ModuleContext::active().state()); }']
        for r in owned:
            b = small[r['id']]
            if b['imported']: continue
            stagecpp += [f"{b['work']}*& work_{b['pointer']}() {{ return state().{b['pointer']}; }}"]
        for n, init, main in bindings[stage]:
            if not any(r['id'] == (stage<<8)|n for r in owned): stagecpp += [f'const RoomFunction {init} = nullptr, {main} = nullptr;']
        stagecpp += [f'#define St{stage}_data_tbl (state().St{stage}_data_tbl)', '#define Room_data_tbl (state().Room_data_tbl)',
                     'void (*_ctors[])() = {nullptr}; void (*_dtors[])() = {nullptr};',
                     f'#line 1 "{(root/f"src/st{stage}/st{stage}.cpp").as_posix()}"', function(s, 'void set(int no, void (*init)(), void (*main)())'), function(s, 'void setTbl()'),
                     function(s, 'extern "C" void _prolog()').replace('extern "C" void _prolog()', 'void prologOriginal()'),
                     function(s, 'extern "C" void _epilog()').replace('extern "C" void _epilog()', 'void epilogOriginal()'),
                     'class cRoomData { public: u16 m_RelNo = 0; int checkRoomRange(u8,u8); void execInitFunc(u16); void execMainFunc(u16); int checkRelRead(u16); };']
        for signature in ['void cRoomData::execInitFunc(u16 room_no)', 'void cRoomData::execMainFunc(u16 room_no)', 'int cRoomData::checkRoomRange(u8 stage, u8 room)', 'int cRoomData::checkRelRead(u16 room_no)']:
            body = function(roomtext, signature).replace('u8 no = room_no;', 'u8 no = static_cast<u8>(room_no);').replace('checkRoomRange(stage, no)', 'checkRoomRange(static_cast<u8>(stage), no)')
            stagecpp += [f'#line 1 "{(root/"src/game/roomdata.cpp").as_posix()}"', body]
        stagecpp += ['void State::invoke(ModuleContext& c, std::uint16_t id, bool init) { cRoomData dispatch;',
                     f'dispatch.m_RelNo = {m["file_index"]}; if (dispatch.checkRelRead(id) != 0) throw std::logic_error("Original REL association mismatch");',
                     'c.observe(init ? "cRoomData::execInitFunc" : "cRoomData::execMainFunc");',
                     'if (init) dispatch.execInitFunc(id); else dispatch.execMainFunc(id); }',
                     f'#undef St{stage}_data_tbl', '#undef Room_data_tbl', '}']
    stagecpp += ['}', 'namespace re4::port {', 'std::vector<NativeModuleDefinition> originalModuleDefinitions() { return {']
    stagecpp += ['{' + f"{m['module_id']}, {{}}, [] {{ return std::make_unique<modules_original::{m['name']}::State>(); }}, " + q('partial original stage registration; only reviewed callbacks executable')+'},' for m in stageMods]
    stagecpp += ['}; }', '}']
    (output/'module_stages.cpp').write_text('\n'.join(stagecpp)+'\n', encoding='utf-8')
    return manifest


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    m = generate(args.root.resolve(), args.output.resolve())
    print(f"Generated {len(m['modules'])} modules, {len(m['rooms'])} room bindings; {sum(r['compiled'] for r in m['rooms'])} reviewed entry pairs (resident dependencies may remain)")
