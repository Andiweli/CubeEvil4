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
    def test_weapon_and_effect_original_bodies_guard(self):
        import sys
        sys.path.insert(0,str(ROOT/'port/tools'))
        from generate_weapons import generate
        with tempfile.TemporaryDirectory() as directory:
            out=Path(directory)/'out';generate(ROOT,out)
            code=(out/'weapon_control.cpp').read_text();effects=(out/'effect_data_control.cpp').read_text()
            generate(ROOT,out);self.assertEqual(code,(out/'weapon_control.cpp').read_text())
            self.assertEqual(effects,(out/'effect_data_control.cpp').read_text())
            self.assertIn('void PlayerDomain::ReadWepData(u32 no, u32 type)',code)
            self.assertIn('void WeaponDomain::Wep02_init(PlayerDomain* pl)',code)
            self.assertIn('void NativeRuger::init(ModelCpu* parent)',code)
            self.assertIn('int EffectDomain::EspDataLoad(',effects)
            self.assertIn('sys->ownerCnt[owner]++',effects)
            self.assertNotIn('(u32) data',code)
            copy=Path(directory)/'root';hashes=json.loads((ROOT/'port/config/weapon-sources.json').read_text())
            for relative in [*hashes,'port/config/weapon-sources.json']:
                target=copy/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/relative,target)
            target=copy/'src/wep02/wep02.cpp';target.write_text(target.read_text()+'\n// changed source\n')
            with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):generate(copy,Path(directory)/'bad')
    def test_object_lifecycle_bodies_and_native_stride(self):
        import sys
        sys.path.insert(0,str(ROOT/'port/tools'))
        from generate_objects import generate
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)/'output';generate(ROOT,output)
            source=(output/'object_control.cpp').read_text()
            generate(ROOT,output);self.assertEqual(source,(output/'object_control.cpp').read_text())
            self.assertIn('NativeObjectManager::createBack(int id)',source)
            self.assertIn('p->be_flag |= 0x400;',source)
            self.assertIn('ModelLightInfo::init2(',source)
            self.assertIn('EffectDomain::EspDelete(',source)
            self.assertIn('EffectDomain::EfmDeleteSub(',source)
            self.assertNotIn('(u32)p',source)
            self.assertNotIn('(u8*)pArray + size',source)
            self.assertNotIn('delete p;',source)
            self.assertNotIn('i * 0x150',source)
            self.assertIn('void* Core_pEm',(output/'include/re4_object_types.h').read_text())
            copy=Path(directory)/'root'
            hashes=json.loads((ROOT/'port/config/object-sources.json').read_text())
            for relative in [*hashes,'port/config/object-sources.json']:
                target=copy/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/relative,target)
            p=copy/'src/game/obj.cpp';p.write_text(p.read_text()+'\n// changed fingerprint\n')
            with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):generate(copy,Path(directory)/'bad')

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
            self.assertEqual(len(functions),68)
            signatures={f['signature'] for f in functions}
            self.assertIn('void PlayerInit()',signatures)
            self.assertIn('void cPlLeon::setModel()',signatures)
            self.assertIn('void cPlLeon::setMotion()',signatures)
            self.assertIn('cPlLeon::cPlLeon()',signatures)
            self.assertIn('cPlayer::cPlayer()',signatures)
            self.assertIn('void cPlayer::weaponRelease()',signatures)
            self.assertIn('void cPlBody::makeSpaeData()',signatures)
            self.assertIn('void cPlayer::startUp()',signatures)
            self.assertIn('void cMotBase::move()',signatures)
            copy=Path(directory)/'root'
            hashes=json.loads((ROOT/'port/config/player-sources.json').read_text())
            for relative in [*hashes,'port/config/player-sources.json']:
                target=copy/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/relative,target)
            p=copy/'src/game/player.cpp';p.write_text(p.read_text().replace('1200);','1201);'))
            with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):
                generate(copy,Path(directory)/'bad-output')

    def test_cloth_original_bodies_tables_and_guard(self):
        import sys
        sys.path.insert(0,str(ROOT/'port/tools'))
        from generate_cloth import generate
        with tempfile.TemporaryDirectory() as directory:
            out=Path(directory)/'out';generate(ROOT,out)
            code=(out/'cloth_control.cpp').read_text();generate(ROOT,out)
            self.assertEqual(code,(out/'cloth_control.cpp').read_text())
            self.assertEqual(len(json.loads((out/'cloth-manifest.json').read_text())['functions']),7)
            self.assertIn('ClothDomain::PenClothSet(',code)
            self.assertIn('w->Length_l = ikDistance(&w->Pos, &p->Pen.Pos) * 0.5f;',code)
            self.assertIn('w->Flag &= ~1;',code)
            self.assertIn('ClothAtSet leonJacketAt[6]',code)
            self.assertNotIn('0xE0000000',code)
            copy=Path(directory)/'root';hashes=json.loads((ROOT/'port/config/cloth-sources.json').read_text())
            for relative in [*hashes,'port/config/cloth-sources.json']:
                target=copy/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/relative,target)
            target=copy/'src/game/pendulum.cpp';target.write_text(target.read_text()+'\n// changed source\n')
            with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):generate(copy,Path(directory)/'bad')


    def test_live_actor_damage_and_player_bodies_guard(self):
        import sys
        sys.path.insert(0,str(ROOT/'port/tools'))
        from generate_actors import generate as actors
        from generate_damage import generate as damage
        from generate_player_live import generate as live
        for name,generate,filename,source in [('actor',actors,'actor_control.cpp','src/game/em.cpp'),
                ('damage',damage,'damage_control.cpp','src/game/dmg.cpp'),
                ('player-live',live,'player_live_control.cpp','src/game/pl_class.cpp')]:
            with tempfile.TemporaryDirectory() as folder:
                out=Path(folder)/'out';generate(ROOT,out);code=(out/filename).read_text()
                generate(ROOT,out);self.assertEqual(code,(out/filename).read_text())
                self.assertNotIn('(u32)p',code);self.assertNotIn('(u8*)pArray + size',code)
                if name=='actor':
                    self.assertIn('NativeLeonActor::move()',code)
                    self.assertIn('NativeActorManager::construct(',code)
                    self.assertIn('p=fastAt(i);',code)
                    self.assertIn('NativePlayerActor::moveMatCalcBefore() {}',code)
                elif name=='damage':
                    self.assertIn('created<DamageCylinder>(create(ID_CYLINDER))',code)
                    self.assertIn('p=fastAt(i); // Refresh',code)
                else:
                    self.assertIn('PlayerDomain::moveOriginal()',code)
                    self.assertIn('PlayerDomain::NeckCpu::motSet(',code)
                    self.assertIn('ownedMotion(motR)',code)
                    self.assertNotIn('VALID_PTR2',code)
                    self.assertIn('playerActorCollision(this,actorModel.get());',code)
                    tables=(out/'include/re4_player_live_types.h').read_text()
                    self.assertIn('"pl_R1_Footwork","pl_R1_Walk","pl_R1_Back","pl_R1_Run"',tables)
                    self.assertIn('"Pl_R0_Damage","Pl_R0_Die","EmScenario","<null>"',tables)
                copy=Path(folder)/'root';config=f'port/config/{name}-sources.json'
                hashes=json.loads((ROOT/config).read_text())
                for relative in [*hashes,config]:
                    target=copy/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/relative,target)
                p=copy/source;p.write_text(p.read_text()+'\n// changed original\n')
                with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):generate(copy,Path(folder)/'bad')

    def test_original_actions_provenance_and_guard(self):
        import sys
        sys.path.insert(0,str(ROOT/'port/tools'))
        from generate_actions import generate
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)/'out';generate(ROOT,out);code=(out/'actions_control.cpp').read_text()
            generate(ROOT,out);self.assertEqual(code,(out/'actions_control.cpp').read_text())
            entries=json.loads((out/'actions-manifest.json').read_text())['functions']
            signatures={entry['signature'] for entry in entries}
            for sig in ['int cPlayer::actionSelect()','void cActionButton::move()',
                        'int cPlPush::catchCheck()','void EmAtCheck(cModel* pMod)',
                        'SCE_AT_DATA_FIELD_INFO* SceAtCheckFieldInfo(Vec* pos)']:
                self.assertIn(sig,signatures)
            self.assertIn('tag = next(w)',code);self.assertIn('std::intptr_t d',code)
            self.assertNotIn('(tag | 0x80000000)',code);self.assertNotIn('(u32) pMod',code)
            self.assertNotIn('((cEm*) m)',code);self.assertIn('windowAction(win)',code)
            self.assertIn('if (!scenarioInitialized)',code)
            copy=Path(folder)/'root';config='port/config/actions-sources.json';hashes=json.loads((ROOT/config).read_text())
            for relative in [*hashes,config]:
                target=copy/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/relative,target)
            source=copy/'src/game/act_btn.cpp';source.write_text(source.read_text()+'\n// original changed\n')
            with self.assertRaisesRegex(ValueError,'fingerprint mismatch'):generate(copy,Path(folder)/'bad')

if __name__ == '__main__': unittest.main()
