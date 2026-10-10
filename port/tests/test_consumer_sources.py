"""Provenance, complete original callback bodies, reproducible native views."""
import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'port/tools'))
import generate_consumers

class Sources(unittest.TestCase):
    def test_original_methods_and_no_pointer_casts(self):
        with tempfile.TemporaryDirectory() as folder:
            output=Path(folder)
            generate_consumers.generate(ROOT,output)
            manifest=json.loads((output/'consumer-manifest.json').read_text())
            self.assertEqual(len(manifest['functions']),87)
            code=(output/'consumer_control.cpp').read_text()
            self.assertIn('const EventFunction *fn',code)
            self.assertIn('blk->ofs[no]',code)
            self.assertIn('rec->parentNo',code)
            self.assertNotIn('return (int) info',code)
            self.assertIn('ModelInfoView* ModelCpu::modelInit',code)
            self.assertIn('void C_MTXConcat',code)
            self.assertIn('ModelPartsManager::createSequential',code)
            self.assertIn('EventCpu::ExePacket_SetPartsSub',code)
            self.assertIn('void MotionSetCore',code)
            self.assertIn('GraphicsState::setCopyClear',code)
            self.assertNotIn('(u32) w->pMot',code)
            self.assertNotIn('asm("r27")',code)
    def test_changed_original_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)/'root'
            hashes=json.loads((ROOT/'port/config/consumer-sources.json').read_text())
            for p in [*hashes,'port/config/consumer-sources.json']:
                dest=root/p; dest.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(ROOT/p,dest)
            source=root/'src/game/model.cpp'
            source.write_text(source.read_text().replace('be_flag |= 0x800;','be_flag |= 0x400;'))
            with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):
                generate_consumers.generate(root,Path(folder)/'output')
if __name__=='__main__': unittest.main()
