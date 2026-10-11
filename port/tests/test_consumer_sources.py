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
import generate_subscreen
import generate_id_save
import generate_lights
import generate_skinning

class Sources(unittest.TestCase):
    def test_original_skinning_bodies_and_guard(self):
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)/'out';generate_skinning.generate(ROOT,out)
            code=(out/'skinning_control.cpp').read_text()
            for signature in ['void calcWeightMat(', 'int MakeWeightPalette(', 'int MakeWeightPaletteExt(']:
                self.assertIn(signature,code)
            self.assertIn('1.0f - total',code)
            self.assertIn('std::uintptr_t wa',code)
            self.assertNotIn('0xE0000000',code)
            generate_skinning.generate(ROOT,out);self.assertEqual(code,(out/'skinning_control.cpp').read_text())
            root=Path(folder)/'root'
            for p in ['src/game/trans.cpp','src/lib/psmtx.c','port/config/consumer-sources.json']:
                dest=root/p;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/p,dest)
            p=root/'src/game/trans.cpp';p.write_text(p.read_text().replace('rate = 1.0f - total','rate = 0.5f - total'))
            with self.assertRaisesRegex(ValueError,'fingerprint changed'):generate_skinning.generate(root,out)
    def test_original_light_bodies(self):
        with tempfile.TemporaryDirectory() as folder:
            output=Path(folder);generate_lights.generate(ROOT,output)
            code=(output/'light_control.cpp').read_text()
            for s in ['void LightManagerCpu::init','int LightManagerCpu::roomInit',
                      'int cLit::versionUp','LightCpu& LightCpu::operator=',
                      'int cLightPath::movePath','void Light07_Move','LightFuncTbl[17]',
                      'LightConstants::ConsGetRoomValue','kindFlags[kind >> 5] |= 1u']:
                self.assertIn(s,code)
            self.assertNotIn('(u32) pLitPath',code)
            self.assertNotIn('delete p;',code)
            self.assertNotIn('int LightManagerCpu::setEnv',code)
    def test_changed_original_light_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)/'root';hashes=json.loads((ROOT/'port/config/light-sources.json').read_text())
            for p in [*hashes,'port/config/light-sources.json']:
                dest=root/p;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/p,dest)
            p=root/'src/game/light.cpp';p.write_text(p.read_text().replace('ElecPower = 1.0f','ElecPower = 0.0f'))
            with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):generate_lights.generate(root,Path(folder)/'output')
    def test_original_id_save_bodies(self):
        with tempfile.TemporaryDirectory() as folder:
            output=Path(folder); generate_id_save.generate(ROOT,output)
            manifest=json.loads((output/'id-save-manifest.json').read_text())
            self.assertEqual(len(manifest['functions']),31)
            code=(output/'id_save_control.cpp').read_text()
            self.assertIn('void IdSystemCpu::cockpitGameInit()',code)
            self.assertIn('m_IdUnit[i].be_flag = 0xFF;',code)
            self.assertIn('void RoomSaveCpu::load(void* p)',code)
            self.assertIn('SAVE_DATA_HEAD* SaveImageCpu::alloc()',code)
            self.assertIn('std::uintptr_t headaddr',code)
            self.assertNotIn('(u32) head',code)
            self.assertNotIn('(u32) m_pRoomSaveData',code)
            self.assertNotIn('bool cGameSave::save',code)
    def test_changed_original_id_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)/'root'
            hashes=json.loads((ROOT/'port/config/id-save-sources.json').read_text())
            for p in [*hashes,'port/config/id-save-sources.json']:
                dest=root/p; dest.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(ROOT/p,dest)
            path=root/'src/game/id_sys.cpp'; path.write_text(path.read_text().replace('be_flag = 0xFF','be_flag = 0xFE'))
            with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):
                generate_id_save.generate(root,Path(folder)/'output')
    def test_original_subscreen_inventory_bodies(self):
        with tempfile.TemporaryDirectory() as folder:
            output=Path(folder); generate_subscreen.generate(ROOT,output)
            manifest=json.loads((output/'subscreen-manifest.json').read_text())
            self.assertEqual(len(manifest['functions']),25)
            code=(output/'subscreen_control.cpp').read_text()
            for text in ['SubScreenContext::SubScreenGameInit','SubScreenAramRead();',
                         'InventoryCpu::init','InventoryCpu::search','InventoryCpu::get',
                         'int MapManagerCpu::roomInit()', 'void mapAreaFilename(int area, char* name)',
                         'p->isAlive(m_char)', 'wk->pFreeOffs += size;']:
                self.assertIn(text,code)
            self.assertNotIn('(void**) &wk->p_module',code)
            self.assertIn('allocateOrders(m_array_num)',code)
            self.assertIn('sizeof(cItem)==14',(output/'include/re4_subscreen_types.h').read_text())
    def test_changed_original_subscreen_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)/'root'
            hashes=json.loads((ROOT/'port/config/subscreen-sources.json').read_text())
            for p in [*hashes,'port/config/subscreen-sources.json']:
                dest=root/p; dest.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(ROOT/p,dest)
            p=root/'src/game/sscrn.cpp'; p.write_text(p.read_text().replace('SS_ARAM 0xD00000','SS_ARAM 0xC00000'))
            with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):
                generate_subscreen.generate(root,Path(folder)/'output')
    def test_original_methods_and_no_pointer_casts(self):
        with tempfile.TemporaryDirectory() as folder:
            output=Path(folder)
            generate_consumers.generate(ROOT,output)
            manifest=json.loads((output/'consumer-manifest.json').read_text())
            self.assertEqual(len(manifest['functions']),111)
            code=(output/'consumer_control.cpp').read_text()
            self.assertIn('void InverseKinematics(',code)
            self.assertIn('void ikCalc(',code)
            self.assertIn('int MotionCheckCrossFrame(',code)
            self.assertNotIn('requireNoActiveIk(pEm, 1);',code)
            self.assertIn('const EventFunction *fn',code)
            self.assertIn('blk->ofs[no]',code)
            self.assertIn('rec->parentNo',code)
            self.assertNotIn('return (int) info',code)
            self.assertIn('ModelInfoView* ModelCpu::modelInit',code)
            self.assertIn('void C_MTXConcat',code)
            self.assertIn('ModelPartsManager::createSequential',code)
            self.assertIn('EventCpu::ExePacket_SetPartsSub',code)
            self.assertIn('void MotionSetCore',code)
            self.assertIn('u32 MotionMove(',code)
            self.assertIn('u16 MotionSequenceCtrl(',code)
            self.assertIn('void ModelCpu::matBlend(',code)
            self.assertIn('void ResetShape(',code)
            self.assertIn('MotionSequenceView* seq',code)
            self.assertIn('int ModelCpu::deleteModelInfo',code)
            self.assertIn('GraphicsState::setCopyClear',code)
            self.assertNotIn('(u32) w->pMot',code)
            self.assertNotIn('asm("r27")',code)
            message=(output/'message_control.cpp').read_text()
            self.assertEqual(len(manifest['messages']),33)
            self.assertIn('void MessageControlCpu::gameInit()',message)
            self.assertIn('void MessageControlCpu::CoreDataRead()',message)
            self.assertIn('SpecularInit((TEXPalette*) CORE_ARC(CORE_SPECULAR_0)',message)
            self.assertIn('GXInitTexObjCI(&t->tex',message)
            self.assertIn('requireResolvedPalette(addr)',message)
            self.assertIn('blk->ofs[no]',code)
            self.assertNotIn('(u32) addr',message)
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
