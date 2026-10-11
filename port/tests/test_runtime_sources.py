#!/usr/bin/env python3
"""Prove extraction provenance and refusal of changes; no copyrighted fixtures."""
import importlib.util
import json
import shutil
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('runtime_generator', ROOT/'port/tools/generate_runtime.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class Sources(unittest.TestCase):
    def test_merchant_and_collision_original_bodies_guard(self):
        import sys
        sys.path.insert(0,str(ROOT/'port/tools'))
        from generate_merchant import generate as merchant
        from generate_collision import generate as collision
        for name,generator,filename,signature in [('merchant',merchant,'merchant.cpp','void MerchantDomain::MerchantGameInit()'),('collision',collision,'at_sub.cpp','u32 At_poly_line_ck(')]:
            with tempfile.TemporaryDirectory() as folder:
                out=Path(folder)/'out';generator(ROOT,out)
                code=(out/(name+'_control.cpp')).read_text();self.assertIn(signature,code)
                generator(ROOT,out);self.assertEqual(code,(out/(name+'_control.cpp')).read_text())
                copy=Path(folder)/'root';hashes=json.loads((ROOT/f'port/config/{name}-sources.json').read_text())
                for relative in [*hashes,f'port/config/{name}-sources.json']:
                    target=copy/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/relative,target)
                target=copy/'src/game'/filename;target.write_text(target.read_text()+'\n// changed fingerprint\n')
                with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):generator(copy,Path(folder)/'bad')

    def test_original_bodies_and_native_views(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            generator.generate(ROOT, output)
            manifest = json.loads((output/'runtime-manifest.json').read_text())
            self.assertGreaterEqual(len(manifest['functions']), 26)
            source = (output/'runtime_control.cpp').read_text()
            self.assertIn('u32 bit;', source)
            self.assertIn('i < 64; bit <<= 1', source)
            self.assertIn('TaskSleep(1)', source)
            self.assertNotIn('asm("r16")', source)
            self.assertNotIn('(u32) p', source)
            self.assertIn('std::nullopt', source)
            self.assertIn('u32 Scenario_flg[8]{}', (output/'include/re4_runtime_types.h').read_text())

    def test_fingerprint_rejects_changes(self):
        with tempfile.TemporaryDirectory() as directory:
            copy = Path(directory)/'root'
            hashes = json.loads((ROOT/'port/config/runtime-sources.json').read_text())
            for relative in [*hashes, 'port/config/runtime-sources.json']:
                target = copy/relative
                target.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(ROOT/relative,target)
            path = copy/'src/game/pad.cpp'
            path.write_text(path.read_text().replace('dead = 10;', 'dead = 11;'))
            with self.assertRaisesRegex(ValueError, 'fingerprint mismatch'):
                generator.generate(copy,Path(directory)/'output')

    def test_player_provenance_and_changed_source_refusal(self):
        import sys
        sys.path.insert(0,str(ROOT/'port/tools'))
        from generate_player import generate
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)/'output';generate(ROOT,output)
            functions=json.loads((output/'player-manifest.json').read_text())['functions']
            self.assertEqual(len(functions),21)
            signatures={f['signature'] for f in functions}
            self.assertIn('void PlayerInit()',signatures)
            self.assertIn('void cPlLeon::setModel()',signatures)
            self.assertIn('void cPlLeon::setMotion()',signatures)
            self.assertNotIn('cPlLeon::cPlLeon()',signatures)
            copy=Path(directory)/'root'
            hashes=json.loads((ROOT/'port/config/player-sources.json').read_text())
            for relative in [*hashes,'port/config/player-sources.json']:
                target=copy/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/relative,target)
            p=copy/'src/game/player.cpp';p.write_text(p.read_text().replace('1200);','1201);'))
            with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):
                generate(copy,Path(directory)/'bad-output')


if __name__ == '__main__': unittest.main()
