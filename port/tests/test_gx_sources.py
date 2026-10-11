"""New camera/SDK extraction must remain reproducible and reject altered originals."""
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'port/tools'))
import generate_gx

class Sources(unittest.TestCase):
    def test_complete_original_camera_and_sdk_bodies(self):
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder);generate_gx.generate(ROOT,out)
            code=(out/'gx_control.cpp').read_text();manifest=json.loads((out/'gx-manifest.json').read_text())
            self.assertEqual(len(manifest['functions']),9)
            for body in ['void DiagnosticCameraMotion::move()', 'param.Fovy = fov.y * 180.0f / PI;',
                         'void C_MTXLookAt(', 'void C_MTXPerspective(', 'CameraSetOrientationRoll(this);',
                         'CameraSequenceCtrl(&m_info) == 4']:
                self.assertIn(body,code)
            self.assertNotIn('(u32) w->',code)
            generate_gx.generate(ROOT,out)
            self.assertEqual(code,(out/'gx_control.cpp').read_text())
            render=(out/'gx_render_control.cpp').read_text()
            meta=json.loads((out/'gx-render-manifest.json').read_text())
            self.assertEqual(meta['render_order'],list(range(22)))
            for signature in ['int ExecOt(', 'u16 OtGetPrevKind()', 'void TextureBlend3(',
                              'void LightEnv::setModel2(', 'void lightSetColor(',
                              'void lightSetParallel(', 'void GXInitLightSpot(']:
                self.assertIn(signature,render)
            for signature in ['void shaderSetup(', 'void specularSetup(', 'void specularSetup2(',
                              'void bumpSetup(', 'void updateMatrices(', 'void GXSetTevIndBumpXYZ(']:
                self.assertIn(signature,render)
            self.assertNotIn('(u32) p < 0x80000000',render)
            self.assertNotIn('(*(u32*) &l->DispCol',render)
            self.assertTrue((out/'include/gx_state_types.h').is_file())
    def test_camera_original_change_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)/'root';hashes=json.loads((ROOT/'port/config/gx-sources.json').read_text())
            for path in [*hashes,'port/config/gx-sources.json']:
                dest=root/path;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/path,dest)
            p=root/'src/game/cam_motion.cpp';p.write_text(p.read_text().replace('param.Roll = roll.y','param.Roll = roll.x'))
            with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):generate_gx.generate(root,Path(folder)/'out')

if __name__=='__main__':unittest.main()
