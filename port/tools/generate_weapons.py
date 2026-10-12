"""Preserved ReadWepData, native module hooks and original weapon consumers."""
import argparse
import json
import re
from pathlib import Path
from generate_runtime import digest,function

def generate(root,output):
    hashes=json.loads((root/'port/config/weapon-sources.json').read_text())
    for path,wanted in hashes.items():
        if digest(root/path)!=wanted:raise ValueError('Original weapon fingerprint mismatch: '+path)
    output.mkdir(parents=True,exist_ok=True)
    read=lambda p:(root/p).read_text(encoding='utf-8')
    cpp=['#include "weapon_original.h"','#include "player_original.h"','namespace re4::port {']
    entries=[]
    def add(path,sig,native,changes=()):
        src=read(path)
        if path.startswith('include/'):src=re.sub(r'^    ','',src,flags=re.M)
        body,line=function(src.replace(sig+' {',sig+'\n{'),sig)
        body=body.replace(sig,native,1)
        for old,new in changes:
            if old not in body:raise ValueError('Weapon adaptation missing: '+old)
            body=body.replace(old,new)
        cpp.extend([f'#line {line} "{(root/path).as_posix()}"',body])
        entries.append(dict(source=path,signature=sig,line=line,adaptations=[sig,*[a for a,_ in changes]]))
    names=re.findall(r'\{"([^"\n]+)", 0\}',read('src/game/dvd.cpp').split('};',1)[0])
    cpp.append('const char *OriginalWeaponFileNames[]={'+','.join(json.dumps(n) for n in names)+'};')
    cpp.append('const std::size_t OriginalWeaponFileCount=sizeof(OriginalWeaponFileNames)/sizeof(*OriginalWeaponFileNames);')
    for name in ['leon','ada','hunk','wesker','klauser']:
        table=re.search(r'(?:static )?ReadFile wep_data_'+name+r'\[46\] = \{.*?\n\};',read('src/game/read.cpp'),re.S)[0]
        cpp.append(table.replace('ReadFile ','const WeaponReadFile ').replace('static const','static const'))
    add('src/game/read.cpp','void ReadWepData(u32 no, u32 type)','void PlayerDomain::ReadWepData(u32 no, u32 type)',[
        ('    DVD_READINFO info;','    if(!nativeWeapons || no>=46)throw std::invalid_argument("native weapon services/number");\n    auto &w=*nativeWeapons->domain();\n    auto &FileTbl=w.files;\n    bool useAlternate=false;\n    WeaponReadInfo info;'),
        ('ReadFile* e;','const WeaponReadFile* e;'),('OSModuleHeader* pModule;','WeaponRelView* pModule;'),
        ('data = (u8*) Game.omake_wep_addr;','useAlternate = true;'),
        ('data = (u8*) WEP_DATA_ADDR;','useAlternate = false;'),
        ('    ReleaseWepData();','    ReleaseWepData();\n    data = w.dataAddress(useAlternate);'),
        ('SET_DRS_NAME(name);','w.setDrsName(name);'),
        ('DVD_READ_N(FileTbl[e->file].name, data, 0, 0, 0, 0x8001)','w.read(FileTbl[e->file].name, data, 0, 0, 0, 0x8001)'),
        ('Dvd.ReadCheck(req, &info)','w.readCheck(req, &info)'),
        ('TaskSleep(1);','scheduler.sleep(1);'),('WEP_DATA_MAX','0x70000'),('DLL_BSS_MAX','0x80'),
        ('(PlArc*) info.start_addr[0][0]','info.start_addr[0][0]'),
        ('pModule = (OSModuleHeader*) (*(u32*) (data + 4) + (u32) data);','pModule = w.embedded(data);'),
        ('size = (u32) pModule - (u32) data;','size = pModule->offset;'),
        ('eprintf(100, 100, 0, 0, "BSS SIZE OVER!!!");','throw std::invalid_argument("original weapon BSS limit exceeded");'),
        ('DLL_Link(pModule, bss);','w.link(pModule, bss);'),
        ('DLL_PROLOG(pModule)();','/* Native ModuleRuntime activation already ran the original prolog once. */'),
        ('pG->pWep = (PlArc*) data;','pG->pWep = data;\n    w.mappedWeapon=no;w.bindInitCallback(*this);construction.weaponRead=true;')])
    for name in ['01','02']:
        path=f'src/wep{name}/wep{name}.cpp'
        add(path,'void Wep'+name+'_init(cModel* m)','void WeaponDomain::Wep'+name+'_init(PlayerDomain* pl)',[
            ('    cPlayer* pl = (cPlayer*) m;',''),('cObjWep* obj;','NativeWeapon* obj;'),
            ('(cObjWep*) ObjMgr.createBack','ObjMgr.createBack'),
            *([('cObjMgr::ID_WEP_FN57','0x32')] if name=='01' else []),
            ('obj->init(pl);','obj->init(pl->actorModel.get());'),
            ('(u32) WEP_ARC_PTR(0x4)','arc(0x4)'),
            *[(f'WEP_ARC_PTR(0x{x:X})',f'arc(0x{x:X})') for x in [0x26,0x27,0x28]]])
        add(path,'extern "C" void _prolog()',f'void WeaponDomain::prolog{name}()',[
            (f'WeaponInitFunc = Wep{name}_init;',f'WeaponInitFunc = [this](PlayerDomain* p) {{Wep{name}_init(p);}};'),
            ('WeaponMoveFunc = PlHandgunMove;','WeaponMoveFunc = [this](PlayerDomain* p) {PlHandgunMove(p);};'),
            *[(f'ObjInitFunc[{id}] = {f};',f'ObjInitFunc[{id}] = [this](NativeObject* p) {{return {f}(p);}};')
              for id,f in ([('0x32','ObjFn57_init')] if name=='01' else [('0x21','ObjRuger_init'),('0x27','ObjMauser_init')])]])
        add(path,'extern "C" void _epilog()',f'void WeaponDomain::epilog{name}()')
    for path,cls,native in [('src/wep01/objFn57.cpp','cObjFn57','NativeFn57'),('src/wep/objRuger.cpp','cObjRuger','NativeRuger'),('src/wep02/objMauser.cpp','cObjMauser','NativeMauser')]:
        name=cls.removeprefix('cObj');table=name.lower()+'_tbl'
        cpp.append(re.search(r'const u8 '+table+r'\[3\] = \{.*?\};',read(path))[0])
        add(path,f'void Obj{name}_init(cObj* obj)',f'NativeObject *WeaponDomain::Obj{name}_init(NativeObject* obj)',[
            (f'new (obj) {cls}();',f'return obj->manager->place<{native}>(obj);')])
        sig=f'void {cls}::init(cModel* parent)'
        body,_=function(read(path),sig)
        changes=[(old,'weaponDomain->arc('+old.split('(')[1]) for old in sorted(set(re.findall(r'WEP_ARC_PTR\([^)]*\)',body)))]
        changes += [('pG->','weaponDomain->pG->'),('pLog->err(','weaponDomain->pLog->err('),('AtariFlagsAnd(&atari, 0xFCFF);','atari.m_flag &= 0xFCFF;')]
        add(path,sig,f'void {native}::init(ModelCpu* parent)',changes)
        sig=f'void {cls}::setMotion(cPlayer* pl)';body,_=function(read(path),sig)
        changes=[]
        for macro in ['WEP','PLA']:
            for call in re.findall(macro+r'_MOT\(pl, [^)]*\)',body):
                idx,slot=call.split(', ')[1:]
                value=('weaponDomain->arc' if macro=='WEP' else 'pl->arc')+'('+slot[:-1]+')'
                changes.append((call,'pl->m_MotTbl['+idx+'] = '+value))
        changes.append(('(u32) WEP_ARC_PTR(0xA)','weaponDomain->arc(0xA)'))
        add(path,sig,f'void {native}::setMotion(PlayerDomain* pl)',changes)
    for sig in ['void setAbility(f32 pitch, f32 yaw, f32 pitchStep, f32 yawStep)','void setStat(STAT stat)','void setParent(cModel* pParent)']:
        native=sig.replace('void ','void NativeWeapon::',1).replace('cModel*','ModelCpu*')
        changes=[]
        if 'setParent' in sig:changes.append(('m_pParent = pParent;','weaponDomain->bindParent(*this,pParent);\n    m_pParent = pParent;'))
        add('include/pl_wep.h',sig,native,changes)
    add('src/game/objWep.cpp','void cObjWep::resetMotion()','void NativeWeapon::resetMotion()',[
        ('ItemMgr.bulletNum()','weaponDomain->runtime.subscreen().bulletNum()'),
        ('SndStop(m_StopSeId, 0);','throw MissingGameConsumer("SndStop/weapon SE handle");')])
    cpp.append('}')
    (output/'weapon_control.cpp').write_text('\n\n'.join(cpp)+'\n',encoding='utf-8')
    (output/'weapon-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries,
        scope='Original shared ReadWepData; wep01/wep02 prologs, constructors, init and setMotion. Player publication/startUp remain gated by actual downstream consumers.'),indent=2)+'\n')
    cpp=['#include "effect_original.h"','namespace re4::port {',
         'constexpr int EFF_NONE=NativeEffectNone,EFF_MAX=NativeEffectMax; constexpr u32 EFF_TEXOBJ_MAX=NativeEffectTexMax;',
         '#define OWNER_ERR(o,...) recordResourceError(o)', '#define OWNER_ERR2(o,...) recordResourceError(o)']
    effect_entries_start=len(entries)
    add('src/game/eff_sys.cpp','int EspDataLoad(u32 eff_addr, u32 owner, int MultipleOK)',
        'int EffectDomain::EspDataLoad(EffectDataFile* eff_addr, u32 owner, int MultipleOK)',[
        ('ESP_EFF_HEADER* data = (ESP_EFF_HEADER*) eff_addr;','EffectDataFile* data = eff_addr;'),
        ('cEspSystem*','EffectSystemCpu*'),('EffIdTbl*','EffectIds*'),('EffOfsTbl*','EffectOffsets*'),('void* list;','EffectIds* list;'),
        ('ids = (EffectIds*) ((u8*) data + data->Tpl_id_tbl_offset);','ids = &data->ids[0];'),
        ('tpls = (EffectOffsets*) ((u8*) data + data->Tpl_offset);','tpls = &data->tables[0]; (void)tpls;'),
        ('anms = (EffectOffsets*) ((u8*) data + data->Anm_offset);','anms = &data->tables[1]; (void)anms;'),
        ('TEXPalette* tpl = (TEXPalette*) ((u8*) tpls + tpls->ofs[i]);','MessageTpl* tpl = data->textures.at(i).get();'),
        ('cAnm* anm = (cAnm*) ((u8*) anms + anms->ofs[i]);','EffectAnimation* anm = &data->animations.at(i);'),
        *[(f'(u8*) data + data->{name}_id_tbl_offset',f'&data->ids[{n}]') for name,n in [('Est',1),('Sst',2),('Path',3)]],
        *[(f'(u8*) data + data->{name}_offset',f'&data->tables[{n}]') for name,n in [('Est',2),('Sst',3),('Path',4)]],
        ('efmIds = (EffectIds*) ((u8*) data + data->Obj_id_tbl_offset);','efmIds = &data->ids[4];'),
        ('efms = (EffectOffsets*) ((u8*) data + data->Obj_offset);','efms = &data->tables[5]; (void)efms;'),
        ('EffEfmEnt* e = (EffEfmEnt*) ((u8*) efms + efms->ofs[i]);','const auto* e = &data->models.at(i);'),
        ('void* model = (u8*) e + e->ofsModel;','ResourceLease model = e->model;'),
        ('void* tpl = (u8*) e + e->ofsTpl;','ResourceLease tpl = e->tpl;'),
        ('void* mot = NULL;','ResourceLease mot;'),('void* x;','ResourceLease x;'),('x = NULL;','x = {};'),
        ('e->ofsMot != 0','bool(e->mot.owner)'),('mot = (u8*) e + e->ofsMot;','mot = e->mot;'),
        ('e->ofsX != 0','bool(e->shape.owner)'),('x = (u8*) e + e->ofsX;','x = e->shape;')])
    add('src/game/eff_sys.cpp','int espTexRegist(TEXPalette* tpl, cAnm* anm, u8 id, u32 owner)',
        'int EffectDomain::espTexRegist(MessageTpl* tpl, EffectAnimation* anm, u8 id, u32 owner)',[
        ('cEspSystem*','EffectSystemCpu*'),('ESP_TEX_WK*','EffectTextureWork*'),('TEXDescriptor*','MessageTexDescriptor*'),
        ('TEXHeader*','MessageTexHeader*'),('GXTexObj*','MessageTexture*'),('EspCalcTplAddr(tpl);','requireMessageTpl(tpl);'),('PSMTXIdentity(w->_Mtx);','C_MTXIdentity(w->_Mtx);')])
    add('src/game/eff_sys.cpp','GXTexObj* EspPullTexObj(u32 num)','MessageTexture* EffectDomain::EspPullTexObj(u32 num)',[
        ('cEspSystem*','EffectSystemCpu*'),('GXTexObj* obj;','MessageTexture* obj;')])
    for name in ['est','sst','path']:
        add('src/game/eff_sys.cpp',f'int {name}Regist(void* data, void* list, u32 owner)',
            f'int EffectDomain::{name}Regist(EffectOffsets* data, EffectIds* list, u32 owner)',[
            ('cEspSystem*','EffectSystemCpu*'),('SstTbl*','EffectTableWork*'),('(SstData*) data','data'),('(SstList*) list','list')])
    body,_=function(read('src/game/eff_sys.cpp'),'int efmRegist(void* model, void* tpl, void* mot, void* x, u8 id, u32 owner)')
    warn=re.search(r'        if \(DbgFlagChk.*?\n        \}',body,re.S)[0]
    add('src/game/eff_sys.cpp','int efmRegist(void* model, void* tpl, void* mot, void* x, u8 id, u32 owner)',
        'int EffectDomain::efmRegist(ResourceLease model, ResourceLease tpl, ResourceLease mot, ResourceLease x, u8 id, u32 owner)',[
        ('cEspSystem*','EffectSystemCpu*'),('EspEfmWk*','EffectModelWork*'),(warn,'        recordResourceError(owner);'),('(EspEfmMotTbl*) mot','mot')])
    for name in ['EspData','espTex','est','sst','path','efm']:
        args='u32 owner, int bCountCheck, int warn' if name=='EspData' else 'u32 owner'
        sig=f'int {name}Release({args})';body,_=function(read('src/game/eff_sys.cpp'),sig)
        changes=[(old,new) for old,new in [('cEspSystem*','EffectSystemCpu*'),('ESP_TEX_WK*','EffectTextureWork*'),
            ('SstTbl*','EffectTableWork*'),('EspEfmWk*','EffectModelWork*')] if old in body]
        add('src/game/eff_sys.cpp',sig,f'int EffectDomain::{name}Release({args})',changes)
    for sig in ['int cEspSystem::GetTexObjFlag(u32 no)','void cEspSystem::SetTexObjFlag(u32 no, int flg)']:
        body,_=function(read('src/game/eff_sys.cpp'),sig)
        err=re.search(r'        pLog->err\([^;]+;',body)[0]
        add('src/game/eff_sys.cpp',sig,sig.replace('cEspSystem::','EffectSystemCpu::'),[(err,'        throw std::out_of_range("original effect TexObj pool");')])
    cpp.extend(['#undef OWNER_ERR','#undef OWNER_ERR2','}'])
    (output/'effect_data_control.cpp').write_text('\n\n'.join(cpp)+'\n',encoding='utf-8')
    (output/'effect-data-manifest.json').write_text(json.dumps(dict(sources=hashes,functions=entries[effect_entries_start:],
        scope='Original effect registration/counts/releases and GX texture CPU state. Checked immutable BE views; no effect spawning, rendering or full EspRoomInit.'),indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();generate(a.root.resolve(),a.output.resolve())
