#!/usr/bin/env python3
"""Generation determinism, original identities and deliberately corrupted source/config probes."""
import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('generator', ROOT/'port/tools/generate_modules.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class Modules(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(dir=ROOT/'port/build')
        cls.folder = Path(cls.temp.name)
        cls.manifest = generator.generate(ROOT, cls.folder/'first')

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_actual_original_identities_and_variants(self):
        modules = {m['name']: m for m in self.manifest['modules']}
        rooms = {r['id']: r for r in self.manifest['rooms']}
        self.assertEqual((len(modules), len(rooms)), (114, 122))
        self.assertEqual(rooms[0x120]['module'], rooms[0x100]['module'])
        self.assertEqual(rooms[0x120]['file_index'], 146)
        self.assertTrue(rooms[0x120]['resident_imports'])
        self.assertFalse(rooms[0x10d]['resident_imports'])
        self.assertEqual(rooms[0x328]['module'], modules['st3_3']['module_id'])
        self.assertEqual(rooms[0x10d]['module'], modules['st1_2']['module_id'])
        self.assertEqual(set(rooms[0x102]['variants']), {'st1_1', 'st1_3'})
        self.assertTrue(rooms[0x120]['compiled'])
        self.assertFalse(rooms[0x100]['compiled'])
        self.assertEqual(sum(r['compiled'] for r in rooms.values()), 3)

    def test_deterministic_output(self):
        generator.generate(ROOT, self.folder/'second')
        for p in (self.folder/'first').iterdir():
            self.assertEqual(p.read_bytes(), (self.folder/'second'/p.name).read_bytes())

    def test_module_scopes_preserve_collisions(self):
        self.assertEqual(len(self.manifest['symbol_collisions']['_prolog']), 114)
        cpp = (self.folder/'first/module_stages.cpp').read_text()
        for m in self.manifest['modules']:
            if m['name'].startswith(('st1_', 'st2_', 'st3_', 'st4_')):
                self.assertIn('namespace '+m['name']+' {', cpp)
        self.assertNotIn('extern "C" void _prolog()', cpp)

    def fixture(self, target):
        root = self.folder/target
        shutil.copytree(ROOT/'config/G4BE08', root/'config/G4BE08')
        shutil.copytree(ROOT/'port/config', root/'port/config')
        for path in json.loads((ROOT/'port/config/module-sources.json').read_text()) | {'src/game/dvd.cpp': ''}:
            p = root/path; p.parent.mkdir(parents=True, exist_ok=True); shutil.copyfile(ROOT/path, p)
        return root

    def test_all_reviewed_source_changes_rejected(self):
        root = self.fixture('source-probes')
        for path in json.loads((ROOT/'port/config/module-sources.json').read_text()):
            p = root/path; original = p.read_bytes(); p.write_bytes(original+b'\n// modified probe\n')
            with self.assertRaisesRegex(ValueError, 'fingerprint mismatch'):
                generator.generate(root, root/'output')
            p.write_bytes(original)

    def test_invalid_module_graph_rejected(self):
        root = self.fixture('graph-probes')
        p = root/'config/G4BE08/modules/st1_0/rel.json'
        original = json.loads(p.read_text())
        altered = dict(original, links=['not-a-module'])
        p.write_text(json.dumps(altered))
        with self.assertRaisesRegex(ValueError, 'unresolved import module'): generator.generate(root, root/'output')
        altered = dict(original, module_id=78)
        p.write_text(json.dumps(altered))
        with self.assertRaisesRegex(ValueError, 'duplicate module identity'): generator.generate(root, root/'output')


if __name__ == '__main__': unittest.main()
