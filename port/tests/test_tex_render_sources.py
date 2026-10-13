"""Actual TexRender CPU-body provenance and changed-original rejection."""
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'port/tools'))
import generate_tex_render

class Sources(unittest.TestCase):
    def test_actual_cpu_chain(self):
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)
            generate_tex_render.generate(ROOT,out)
            m=json.loads((out/'tex-render-manifest.json').read_text())
            self.assertEqual(len(m['functions']),7)
            code=(out/'tex_render_control.cpp').read_text()
            self.assertIn('g_RndMgrNum == 8',code)
            self.assertIn('SetWHSize(size, size)',code)
            self.assertIn('owner->allocate(this,GetTexBufSize())',code)
            self.assertNotIn('GXInitTexObj',code)
    def test_changed_source_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)/'root'
            hashes=json.loads((ROOT/'port/config/tex-render-sources.json').read_text())
            for path in [*hashes,'port/config/tex-render-sources.json']:
                dest=root/path;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/path,dest)
            source=root/'src/game/TexRender.cpp'
            source.write_text(source.read_text().replace('g_RndMgrNum == 8','g_RndMgrNum == 9'))
            with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):
                generate_tex_render.generate(root,Path(folder)/'output')

if __name__=='__main__':unittest.main()
