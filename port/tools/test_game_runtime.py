#!/usr/bin/env python3
"""Private real-file tests through the shared frame runtime, not a playable game."""
import argparse
import json
import struct
import uuid
import sys
import re
from pathlib import Path
from original_import.pipeline import external_root, sha, write_json
from test_engine_loading import prepare, run


from retroid_adb import hardware_sequence

@hardware_sequence()
def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--data', required=True)
    p.add_argument('--output', required=True)
    p.add_argument('--host-exe')
    p.add_argument('--adb')
    p.add_argument('--serial')
    p.add_argument('--repeat', type=int, default=2)
    args = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    data = external_root(args.data,root,[])
    output = external_root(args.output,root,[])
    if data == output or not 1 <= args.repeat <= 10 or not (args.host_exe or args.adb):
        raise ValueError('separate output / repeat 1..10 / host or adb required')
    cases = prepare(data,output,False)
    manifest = json.loads((data/'manifest.json').read_text())
    sys.path.insert(0,str(root/'tools'))
    import drs
    from motion import fcv
    registrations=re.findall(r'EvtMgr.SetBin\("([^"]+)", PL_ARC_PTR\(pG->pPlayer, ([\dxA-F]+)\), 0, 2\);',
                             (root/'src/game/event.cpp').read_text())
    if len(registrations)!=6: raise ValueError('original player event bin registration changed')
    player_files={f['disc_number']:f for f in manifest['files'] if f['path']=='em/pl00.drs'}
    player_bytes=(data/'blobs'/player_files[0]['sha256']).read_bytes()
    player_drs=drs.Drs(player_bytes)
    external={name:player_drs.entries[int(index,0)-4][1] for name,index in registrations}
    files = {(f['disc_number'],f['path'].lower()):f for f in manifest['files']}
    # Named packages are evidenced by the original source references, not guessed room rules.
    refs=json.loads((data/'source-references.json').read_text())
    referenced={identity for row in refs['literals'] if row['source']=='src/st1/r120.cpp'
                for identity in row.get('resolved',[])}
    packages=[f for f in manifest['files'] if f['id'] in referenced and f.get('analysis',{}).get('format')=='event']
    if len(packages)!=2: raise ValueError('original opening EVD source references differ')
    # Additional model packages are selected universally by FST inventory and
    # model presence: the smallest non-opening package on each disc.
    additional=[]
    for disc in sorted({f['disc_number'] for f in manifest['files']}):
        candidates=[f for f in manifest['files'] if f['disc_number']==disc and f not in packages
                    and f.get('analysis',{}).get('format')=='event'
                    and any(e['tag']=='BIN' for e in f['analysis']['entries'])]
        if not candidates: raise ValueError('no additional disc model package')
        additional.append(min(candidates,key=lambda f:(f['size'],f['path'])))
    digests = {r['source_sha256'] for group in cases.values() for r in group}
    digests.update(b['sha256'] for group in cases.values() for r in group for b in r['blocks'])
    # All original ESL variants, including the two that the original table never selects.
    digests.update(f['sha256'] for f in manifest['files'] if f['path'].lower().endswith('.esl'))
    digests.update(f['sha256'] for f in packages)
    digests.update(f['sha256'] for f in additional)
    digests.update(f['sha256'] for f in player_files.values())
    weapon_files=[f for f in manifest['files'] if f['path'] in ['em/wep01.drs','em/wep02.drs','em/wep03.drs']]
    digests.update(f['sha256'] for f in weapon_files)
    boot_files=[f for f in manifest['files'] if f['path'].lower() in ['etc/core.das','font/common_p.fnt','rel/sscrn.rel'] or re.fullmatch(r'font/(stage|event)\d_j\.fnt',f['path'].lower()) or f['path'].lower().startswith('ss/eng/') or f['path'].lower().startswith('ss/cmn/map_obj')]
    digests.update(f['sha256'] for f in boot_files)
    for digest in digests:
        if sha((data/'blobs'/digest).read_bytes()) != digest: raise ValueError('source blob differs')
    catalog = output/'engine-catalog.tsv'
    results = []
    consumer_results=[]
    model_results=[]
    message_results=[]
    light_results=[]
    def verify_messages(actual,disc,bits,new_game=False):
        if actual['pointer_bits']!=bits or actual['kind']!='actual_CoreDataRead_MessageControl_init_gameInit_Font_create':
            raise ValueError('message original execution/ABI')
        cf=files[(disc,'etc/core.das')]
        source=(data/'blobs'/cf['sha256']).read_bytes()
        core=drs.Drs(source)
        if actual['core_body_sha256']!=cf['analysis']['body_sha256']: raise ValueError('CoreDataRead body integrity')
        def tpl(blob):
            v,n,table=struct.unpack_from('>III',blob)
            if v!=0x20af30: raise ValueError('original TPL reference signature')
            result=[]
            for i in range(n):
                at,cl=struct.unpack_from('>II',blob,table+8*i)
                h,w,fmt,pixel=struct.unpack_from('>HHII',blob,at)
                x,y,z={0:(8,8,32),1:(8,4,32),2:(8,4,32),3:(4,4,32),4:(4,4,32),5:(4,4,32),6:(4,4,64),8:(8,8,32),9:(8,4,32),10:(4,4,32),14:(8,8,32)}[fmt]
                r=dict(width=w,height=h,format=fmt,sha256=sha(blob[pixel:pixel+((w+x-1)//x)*((h+y-1)//y)*z]))
                if cl:
                    entries=struct.unpack_from('>H',blob,cl)[0]; pf,off=struct.unpack_from('>II',blob,cl+4)
                    r.update(palette_entries=entries,palette_format=pf,palette_sha256=sha(blob[off:off+2*entries]))
                result.append(r)
            return result
        expected=[]
        for slot in [4,17,18,16]:
            for r in tpl(core.entries[slot-4][1]): expected.append({k:r[k] for k in ['width','height','format','sha256']})
        if actual['core_textures']!=expected: raise ValueError('actual original SpecularInit/GlobalIlmTexInit state')
        if actual['thermal_sha256']!=tpl(core.entries[19-4][1])[0]['palette_sha256']: raise ValueError('original Thermo TLUT')
        ff=files[(disc,'font/common_p.fnt')]; font=(data/'blobs'/ff['sha256']).read_bytes()
        at,width=struct.unpack_from('>II',font)
        if actual['font_textures']!=tpl(font[at:width]) or actual['widths_sha256']!=sha(font[width:]): raise ValueError('actual original English FNT sheets/CLUT/width data')
        glyphs=[]
        for n in range((len(font)-width)//2):
            l,r=struct.unpack_from('>bb',font,width+(n if n else 1)*2); glyphs.append([l,r,r-l])
        if actual['glyphs']!=glyphs: raise ValueError('original MessageFont::getSize bearings')
        layout=[[22,32,65535,25],[23,32,0,25],[18,24,65535,19],[21,25,0,20],[21,28,0,25],[23,23,0,28],[16,21,65535,0],[20,25,65535,21],[20,25,65535,21]]
        if actual['layouts']!=layout: raise ValueError('original English layout presets')
        for type,r in enumerate(actual['messages']):
            blob=core.entries[(21 if type==3 else 10)-4][1]
            at=struct.unpack_from('>I',blob,8)[0]; count=struct.unpack_from('>I',blob,at+4)[0]
            end=min([n for n in struct.unpack_from('>6I',blob,4) if n>at]+[len(blob)])
            offsets=struct.unpack_from('>'+str(count)+'I',blob,at+8)
            hashes=[sha(blob[at+off:min([at+n for n in offsets if n>off]+[end])]) for off in offsets]
            if r!=dict(type=type,count=count,payload_hashes=hashes): raise ValueError('original MessageData references/spans')
        ss=actual['subscreen']; address=0xD00000
        for r,path in zip(ss['resources'],['rel/sscrn.rel','ss/eng/ss_cmmn.dat','ss/eng/ss_pzzl.dat']):
            f=files[(disc,path)]; dma=(f['size']+31)&~31
            if r!=dict(path=f['path'],size=f['size'],sha256=f['sha256'],aram=address,dma_bytes=dma):
                raise ValueError('original SubScreen DMA resource/hash/offset')
            address+=dma
        if len(ss['resources'])!=3 or ss['free_offset']!=address-0xD00000 or ss['inventory_slots']!=384 or len(ss['item_info'])!=256 or len(ss['map_paths'])!=18:
            raise ValueError('original SubScreen inventory/map state')
        if ss['trace'].count('SubScreenGameInit:original-complete')!=2:
            raise ValueError('original SubScreen repeated initialization incomplete')
        expected_archives=[f for f in boot_files if f['disc_number']==disc and
                           (f['path'].lower().startswith('ss/cmn/map_obj') or (f['path'].lower().startswith('ss/eng/') and f['path'].lower().endswith('.dat')))]
        if [r['path'] for r in ss['archives']]!=[f['path'] for f in expected_archives]: raise ValueError('SubScreen resource coverage')
        for r,f in zip(ss['archives'],expected_archives):
            if r['sha256']!=f['sha256']: raise ValueError('SubScreen archive hash')
            from original_import.formats import body_index
            blob=(data/'blobs'/f['sha256']).read_bytes(); entries,_=body_index(blob)
            if len(r['entries'])!=len(entries): raise ValueError('SubScreen block count')
            for value,entry in zip(r['entries'],entries):
                if any(value[k]!=entry[k] for k in ['index','tag','offset','size']) or value['sha256']!=sha(blob[entry['offset']:entry['offset']+entry['size']]): raise ValueError('SubScreen original block integrity')
        if len(ss['model_consumers'])!=6 or any(r['rounds']!=2 for r in ss['model_consumers']): raise ValueError('original inventory/map model initialization')
        ids=actual['id_system']; saves=actual['save_state']; layout=actual['native_layout']
        if ids['capacity']!=128 or ids['alive_after_Cockpit']!=0 or ids['trace'].count('void Cockpit::gameInit()')!=1:
            raise ValueError('original Cockpit/ID initialization')
        if layout!={'ID_UNIT':312 if actual['pointer_bits']==32 else 344,'SAVE_DATA_HEAD':28 if actual['pointer_bits']==32 else 56}:
            raise ValueError('native ID/save ABI layout')
        tables=(root/'src/game/roomdata.cpp').read_text()
        expected_rooms=[]; total=0
        for stage in range(5):
            declaration=re.search(r'ROOM_DATA St'+str(stage)+r'_data_tbl\[(\d+)\] = \{(.*?)\n\};',tables,re.S)
            entries=re.findall(r'\{(\d+),\s*0,\s*\d+,\s*0,\s*0\}',declaration[2])
            if len(entries)!=int(declaration[1]): raise ValueError('original room table entry count')
            total+=len(entries); expected_rooms.extend((stage<<8)|n for n,flag in enumerate(entries) if flag=='1')
        room_bytes=16+216*len(expected_rooms)
        ss_offset=0x3740+((room_bytes+31)&~31); merchant_offset=ss_offset+32
        item_offset=merchant_offset+((0x304+31)&~31); image_size=item_offset+((0x1204+31)&~31)
        if saves['rooms']!=total or saves['room_ids']!=expected_rooms or saves['saved_rooms']!=len(expected_rooms) or saves['room_bytes']!=room_bytes or saves['sections']!=[64,0x3740,ss_offset,merchant_offset,item_offset] or saves['image_bytes']!=image_size or not saves['aggregate_save_load'] or saves['new_round_load']:
            raise ValueError('original room/save allocation coverage')
        path_entry=cf['analysis']['entries'][0x0F-4]
        lb=actual['light_boot']
        if lb['path_sha256']!=path_entry['sha256'] or lb['paths']!=11 or not lb['initialized'] or lb['trace']!=['cLightMgr::init:original','cLightMgr::initPath:original']:
            raise ValueError('actual original light startup/path reference')
        if actual['blocker']!='Original consumer unavailable: gameRoomInit/model/light/player' or not actual['repeated_init_released'] or actual['checks']<1500:
            raise ValueError('actual GameTask progress or message lifecycle')
        if not saves['aggregate_save'] or not saves['aggregate_load'] or actual['game_stage_init']!={'original_complete':True,'next_state':2,'save_kind':-1,'disc_after':0}:
            raise ValueError('actual aggregate save / original stage transition')
        if actual['merchant_boot']!={'original_complete':True,'friendship':50,'character_bound':False} or actual['game_init']!={'original_complete':True,'next_state':1,'presentation':'isolated_CPU_recording_probe','new_game_inventory_branch':new_game}:
            raise ValueError('original Merchant/gameInit execution scope')
        player=actual['player']
        inputs=player['action_inputs']
        if len(inputs)!=3:raise ValueError('defined central PadRead input coverage')
        for case,(name,mask,routine) in zip(inputs,[('forward',1,1),('aim',0x10,6),('knife',0x800,11)]):
            if case['name']!=name or not case['key_on']&mask or case['routines']!=[0,routine,0,0] or case['scope']!='isolated-PadRead-original-ActionSelect-prefix':
                raise ValueError('actual original ActionSelect/controller decision')
        construction=player['construction']
        idle_binding=next(b for b in player['weapon_cases'][-1]['weapon']['motions'] if b['index']==1)
        idle_file=files[(disc,idle_binding['path'])]
        idle_archive=drs.Drs((data/'blobs'/idle_file['sha256']).read_bytes())
        idle_sequence=fcv.parse_seq(idle_archive.entries[idle_binding['slot']-4][1])
        idle_count=len(idle_sequence.keys)
        if construction!={'returned_actor':False,'em_mgr_dispatch_bound':True,'player_constructor':True,
            'subobjects':True,'scheduler_task':True,'weapon_release_complete':True,'virtual_moves':1,'motion_moves':1,'action_selections':1,'eye_moves':1,'actor_collision_checks':1,'pending_actions':0,'action_attribute':0,'action_status':1,'subscreen_enabled':True,'floor_y':construction['floor_y'],'primary_motion':True,'neck_motion':True,'sequence_frames':idle_count,'sequence_frame':1%idle_count,'startUp_executed':False,'rolled_back':True,'parts':119,'model_infos':8,
            'motion_slots':109,'registered_motion_slots':12,'debug_callbacks':11,
            'init1_reached':True,'hitboxes':5,'light_flags':1,
            'blocker':'Original consumer unavailable: PlClothMoveLeon/PenClothMove native simulation owner'}:
            raise ValueError('actual preserved Leon construction prefix/blocker/rollback')
        proof=player['cloth_body']
        if any(proof[k] is not True for k in ['cloth_initialized','body_initialized','camera_allocated','startUp_entered']) or proof['simulation_executed'] or proof['links']!=[24,0,21] or proof['volumes']!=[6,0,4]:
            raise ValueError('original init1 Cloth/Body/attach-camera execution scope')
        import math
        from test_collision import parse as collision_parse,oracle as collision_oracle
        room_file=files[(actual['game_stage_init']['disc_after'],'st1/r120.das')]
        room_bytes=(data/'blobs'/room_file['analysis']['body_sha256']).read_bytes()
        sat_entry=next(e for e in room_file['analysis']['entries'] if e['tag']=='SAT')
        sat_bytes=room_bytes[sat_entry['offset']:sat_entry['offset']+sat_entry['size']]
        ground=collision_oracle(collision_parse(sat_bytes),[0.,600.,0.],[0.,-100000.,0.],64,0,False)
        if not ground['attribute'] or not math.isfinite(construction['floor_y']) or abs(construction['floor_y']-ground['point'][1])>.002:
            raise ValueError('original r120 SAT floor differs from independent triangle reference at original zero start input')
        if not 0<=proof['length_max_abs_error']<=.002 or not 0<=proof['normal_max_abs_error']<=.000005 or not all(math.isfinite(proof[k]) for k in ['length_max_abs_error','normal_max_abs_error']):
            raise ValueError('cloth differs from independent scalar reference')
        player_archive=drs.Drs((data/'blobs'/player_files[disc]['sha256']).read_bytes())
        if proof['player_effect_sha256']!=sha(player_archive.entries[0x1a-4][1]):raise ValueError('original pl00 effect resource identity')
        weapon_cases=player['weapon_cases']
        if [(c['equipped_weapon'],c['type']) for c in weapon_cases]!=[(no,kind) for no,kind in [(2,0),(1,0),(3,0),(2,1),(2,0)] for repeat in range(2)]:
            raise ValueError('original weapon variant/repeated lifecycle coverage')
        for c in weapon_cases:
            no,kind=c['equipped_weapon'],c['type'];w=c['weapon'];name={1:'wep01',2:'wep02',3:'wep03'}[no]
            source=files[(disc,'em/'+name+'.drs')];archive=drs.Drs((data/'blobs'/source['sha256']).read_bytes())
            # Drs.body_size includes original embedded REL; independent header
            # and archive indices are read from the immutable imported bytes.
            body=(data/'blobs'/source['analysis']['body_sha256']).read_bytes()
            rel=struct.unpack_from('>I',body,4)[0];module=struct.unpack_from('>I',body,rel)[0]
            model_slot={1:6,2:7 if kind==1 else 6,3:6}[no];bin=archive.entries[model_slot-4][1];tex=archive.entries[1][1]
            if c['scope']!='isolated-original-constructor' or not w['read'] or not w['initialized'] or [w[k] for k in ['module','mapped_weapon','path','source_sha256','body_sha256','body_bytes','rel_offset']]!=[module,no,source['path'],source['sha256'],source['analysis']['body_sha256'],len(body),rel]:
                raise ValueError('actual original ReadWepData/native REL prolog identity')
            if [w[k] for k in ['object_id','item_id','parts','parent_part','bin_sha256','tpl_sha256']]!=[{1:0x32,2:0x21,3:0x27}[no],{1:0x21,2:0x24 if kind==1 else 0x23,3:0x25}[no],bin[25],10,sha(bin),sha(tex)]:
                raise ValueError('original weapon class/model/texture/parent differs')
            object_source={1:'src/wep01/objFn57.cpp',2:'src/wep/objRuger.cpp',3:'src/wep02/objMauser.cpp'}[no]
            from generate_runtime import function
            cls={1:'cObjFn57',2:'cObjRuger',3:'cObjMauser'}[no]
            motionbody,_=function((root/object_source).read_text(),f'void {cls}::setMotion(cPlayer* pl)')
            binding={}
            for macro,index,slot in re.findall(r'(WEP|PLA)_MOT\(pl, (0x[0-9A-Fa-f]+), (0x[0-9A-Fa-f]+)\)',motionbody):
                index,slot=int(index,0),int(slot,0);path=source['path'] if macro=='WEP' else 'em/pl00.drs'
                srcarc=archive if macro=='WEP' else player_drs
                binding[index]=dict(index=index,slot=slot,path=path,sha256=sha(srcarc.entries[slot-4][1]))
            if w['motions']!=[binding[i] for i in sorted(binding)]:raise ValueError('original weapon/player motion reference mapping')
            eff=archive.entries[0][1];offsets=struct.unpack_from('>12I',eff);lists=[]
            for at in offsets[1:6]:
                n=struct.unpack_from('>I',eff,at)[0];lists.append([struct.unpack_from('>H',eff,at+4+i*8)[0] for i in range(n)])
            anms=offsets[7];frames=[struct.unpack_from('>H',eff,anms+struct.unpack_from('>I',eff,anms+4+4*i)[0]+8)[0] for i in range(len(lists[0]))]
            expected_effect=dict(owner=53 if no==1 else 54,loads=1,sha256=sha(eff),est=len(lists[1]),sst=len(lists[2]),paths=len(lists[3]),tex_objects=sum(frames),texture_ids=sorted(lists[0]),texture_frames=[f for _,f in sorted(zip(lists[0],frames))],model_ids=sorted(lists[4]))
            if w['effect']!=expected_effect:raise ValueError('original EFF owners, EST/SST/path/texture/model registration differs')
        if not player['gameTask_PlayerInit_complete'] or player['actor_constructed'] or [player[k] for k in ['life','max_life','weapon','weapon_type','flag']]!=[1200,1200,2,0,1] or player['trace']!=['PlayerInit:original','PlayerLifeReset:original','ReleaseWepData:original','InitModule:original']:
            raise ValueError('actual original PlayerInit state/order/scope')
        plfile=player_files[disc];plsource=(data/'blobs'/plfile['sha256']).read_bytes();plarc=drs.Drs(plsource)
        text=(root/'src/game/pl_leon.cpp').read_text();motiontext=text[text.index('void cPlLeon::setMotion()'):text.index('// Just cPlayer::move')]
        pairs=[(int(i,0),int(slot,0)) for i,slot in re.findall(r'PLA_MOT\(this, (0x[0-9A-F]+), (0x[0-9A-F]+)\)',motiontext)]
        pairs += [(int(i,0),int(slot,0)) for i,slot in re.findall(r'm_MotTbl\[(0x[0-9A-F]+)\] = PL_ARC_PTR\(pG->pPlayer, (0x[0-9A-F]+)\)',motiontext)]
        expected_motions=[dict(motion_index=i,archive_slot=slot,sha256=sha(plarc.entries[slot-4][1])) for i,slot in sorted(pairs)]
        if len(player['leon_assets'])!=2: raise ValueError('missing repeated Leon asset initialization')
        slots=[(4,5),(10,5),(13,5),(8,7),(6,7),(9,7),(18,17),(21,17)]
        expected_models=[dict(parts=plarc.entries[b-4][1][0x19],textures=struct.unpack_from('>I',plarc.entries[t-4][1],4)[0],bin_sha256=sha(plarc.entries[b-4][1]),tpl_sha256=sha(plarc.entries[t-4][1])) for b,t in slots]
        for record in player['leon_assets']:
            if record['motions']!=expected_motions or record['parts']!=plarc.entries[0][1][0x19] or record['models']!=8 or record['model_sources']!=expected_models or record['archive_body_sha256']!=plfile['analysis']['body_sha256']:
                raise ValueError('original Leon model/TPL/motion registration differs from disc data/source')
            expected_sequences=[]
            for idx,slot in sorted(pairs):
                if slot&1:continue
                motion=fcv.parse(plarc.entries[slot-4][1]);sequence=fcv.parse_seq(plarc.entries[slot+1-4][1])
                keys=[[k.frame,k.se,k.free] for k in sequence.keys]
                blocked=False # Complete original IK, isolated explicit actor view / empty SAT.
                ticks=[] if blocked else [dict(sequence_frame=min(t+1,len(keys)-1),motion_frame=(keys[0] if t==0 else keys[min(t,len(keys)-1)])[0]/64,
                    key=keys[min(t+1,len(keys)-1)],state=4 if t+1>=len(keys) else 0) for t in range(16)]
                expected_sequences.append(dict(motion_index=idx,max_frame=motion.max_frame,joints=len(motion.joints),sequence_count=len(keys),sequence_flags=sequence.flags,first_key=keys[0],mot_attr=1|(0x1000 if sequence.flags&1 else 0),sequence_keys=keys,ticks=ticks,ik_blocked=blocked))
            if record['sequence_initializations']!=expected_sequences:
                raise ValueError('actual original MotionSetCore SEQ state differs from independent FCV/SEQ parser')
        stable={k:v for k,v in actual.items() if k not in ['pointer_bits','native_layout']}
        # Numeric reference residuals are separately bounded above; semantic
        # parity must not require compiler-dependent sqrt rounding equality.
        stable=json.loads(json.dumps(stable))
        stable['player']['cloth_body'].pop('length_max_abs_error')
        stable['player']['cloth_body'].pop('normal_max_abs_error')
        stable['player']['construction'].pop('floor_y') # Separately checked numeric SAT residual, not an ABI-dependent action state.
        return sha(json.dumps(stable,sort_keys=True).encode())
    def verify_lights(actual,disc,bits,expected):
        import struct
        if actual['pointer_bits']!=bits or actual['kind']!='isolated_actual_original_light_consumers' or actual['gpu_lighting'] or actual['full_lightMove']:
            raise ValueError('light test scope/ABI')
        core=files[(disc,'etc/core.das')]
        ce=core['analysis']['entries'][0xF-4];cb=(data/'blobs'/core['analysis']['body_sha256']).read_bytes();pb=cb[ce['offset']:ce['offset']+ce['size']]
        path_ids=[n for n in range(pb[0]) if struct.unpack_from('>I',pb,4+n*4)[0]]
        if [p['id'] for p in actual['paths']]!=path_ids:raise ValueError('original path index coverage')
        for p in actual['paths']:
            at=struct.unpack_from('>I',pb,4+p['id']*4)[0];end=pb.index(255,at);values=list(pb[at:end]);length=len(values)+1
            if p['offset']!=at or p['length']!=length:raise ValueError('original path reference/terminator')
            expected_sequences=[]
            for flags in range(4):
                sequence=[]
                for n in range(length+3):
                    v=(values[n] if n<len(values) else 255) if flags&1 else values[n%len(values)]
                    if flags&2 and v!=255:v=200-v
                    sequence.append(v)
                expected_sequences.append(sequence)
            if p['sequences']!=expected_sequences:raise ValueError('original path loop/stop/inversion differs')
        if len(actual['archives'])!=len(expected): raise ValueError('light archive coverage')
        for a,f in zip(actual['archives'],expected):
            if a['path']!=f['path'] or a['body_sha256']!=f['analysis']['body_sha256'] or len(a['rounds'])!=2:
                raise ValueError('light original archive integrity/rounds')
            room_body=(data/'blobs'/f['analysis']['body_sha256']).read_bytes()
            cns=next((e for e in f['analysis']['entries'] if e['tag']=='CNS'),None)
            capacity=100 # src/game/cons.cpp ConsRoomDefault[CONS_R_NLIGHT]
            if cns:
                b=room_body[cns['offset']:cns['offset']+cns['size']];n=struct.unpack_from('>I',b)[0]
                bit=struct.unpack_from('>I',b,4)[0]
                if n>5 and bit&(1<<5):capacity=struct.unpack_from('>I',b,4+4*((n>>5)+1)+5*4)[0]
            resources=[(core,core['analysis']['entries'][0xB-4])]+[(f,e) for e in f['analysis']['entries'] if e['tag']=='LIT'][:2]
            if len(resources)!=3: raise ValueError('original gameRoomInit requires two room LIT occurrences')
            for r in a['rounds']:
                if r['capacity']!=capacity or len(r['blocks'])!=3:raise ValueError('original CONS light capacity')
                for no,(got,(owner,e)) in enumerate(zip(r['blocks'],resources)):
                    body=(data/'blobs'/owner['analysis']['body_sha256']).read_bytes();b=body[e['offset']:e['offset']+e['size']]
                    cuts=struct.unpack_from('>H',b)[0];present=records=0;scalars=bytearray()
                    for j in range(cuts):
                        at=struct.unpack_from('>I',b,4+j*4)[0]
                        if at:
                            present+=1;count=struct.unpack_from('>I',b,at+4)[0];records+=count
                            for k in range(count):
                                w=at+260+k*300;scalars.extend(struct.pack('>II',j,k))
                                for off in [4,8,12,16,24,32]:scalars.extend(b[w+off:w+off+4])
                                scalars.extend(struct.pack('>I',struct.unpack_from('>H',b,w+36)[0]))
                                for off in [44,48,52]:scalars.extend(b[w+off:w+off+4])
                    if got['sha256']!=sha(b) or got['source_version']!=b[2] or got['native_version']!=(0x2C if no<2 else b[2]) or got['cut_count']!=cuts or got['present_cuts']!=present or got['records']!=records or got['created']+got['blocked_model_owners']!=records or sum(got['handler_calls'])!=got['created']:
                        raise ValueError('original LIT cuts/records/version/handler coverage')
                    if got['scalar_sha256']!=sha(scalars):raise ValueError('native light positions/radii/normal/parent scalar BE conversion')
                    if not re.fullmatch('[0-9a-f]{64}',got['dynamic_state_sha256']):raise ValueError('missing actual dynamic light color/state trace')
        return sha(json.dumps({'archives':actual['archives'],'paths':actual['paths']},sort_keys=True).encode())
    def verify_consumers(actual,expected,bits,opening=True):
        if actual['pointer_bits']!=bits or len(actual['packages'])!=len(expected): raise ValueError('consumer ABI/package count')
        for value,f in zip(actual['packages'],expected):
            if value['path']!=f['path'] or value['sha256']!=f['sha256'] or not value['retired']: raise ValueError('consumer original file/lifetime')
            if value['original_init_blocker']: raise ValueError('original CPU init dependency')
            if opening:
                if value['callback_mode']!=3 or value['flags_after']!=(value['flags_before']|0x10): raise ValueError('actual original event cancel persistence')
            elif value['callback_mode']!=-1 or value['flags_after']!=value['flags_before']: raise ValueError('diagnostic may not execute other-room callbacks')
            if len(value['bins'])!=len(f['analysis']['entries']): raise ValueError('EVD entry count')
            source=(data/'blobs'/f['sha256']).read_bytes()
            for observed,entry in zip(value['bins'],f['analysis']['entries']):
                if any(observed[k]!=entry[k] for k in ['name','offset','size','sha256']): raise ValueError('EVD bin identity/reference differs')
                if entry['tag']=='BIN':
                    if observed['original_model_parts']!=entry['validation']['parts']: raise ValueError('original model joint count')
                    raw=source[entry['offset']:entry['offset']+entry['size']]
                    joint=int.from_bytes(raw[:4],'big')
                    parents=[raw[joint+i*16+1] for i in range(raw[0x19])]
                    if observed['parents']!=parents: raise ValueError('original model parent indices')
                if entry['tag']=='MDT' and observed['original_english_count']!=entry['validation']['english']['message_count']:
                    raise ValueError('original event English MDT lookup differs')
            if value['native_models']!=sum(e['tag']=='BIN' for e in f['analysis']['entries']): raise ValueError('model registry count')
            at,size=struct.unpack_from('>II',source,0x40); end=at+size
            refs=[]
            def text(pos,n):
                raw=source[pos:pos+n]
                if b'\0' not in raw: raise ValueError('unbounded reference')
                return raw.split(b'\0')[0].decode('ascii')
            entries={e['name']:e for e in f['analysis']['entries']}
            expected_external=[dict(name=name,size=len(blob),sha256=sha(blob)) for name,blob in external.items()] if opening else []
            if value['external_bins']!=expected_external: raise ValueError('original PL_ARC resource bindings/hash')
            if value['player_archive_sha256']!=(player_files[f['disc_number']]['sha256'] if opening else ''):
                raise ValueError('player archive identity differs')
            def resource(name):
                if name in entries:
                    e=entries[name]; return source[e['offset']:e['offset']+e['size']]
                return external.get(name) if opening else None
            while at<end:
                kind=struct.unpack_from('>I',source,at)[0]; n=struct.unpack_from('>h',source,at+12)[0]
                if n<16 or at+n>end: raise ValueError('reference packet boundary')
                if kind in (3,4,11):
                    p=at+(40 if kind==4 else 28)
                    refs.append(dict(offset=at,type=kind,model=text(at+16,12),parent=text(at+28,12) if kind==4 else '',
                                     bin=text(p,48),tpl=text(p+48,48) if kind!=11 else '',bin_in_package=text(p,48) in entries))
                at+=n
            if refs!=value['event_resource_references']: raise ValueError('original event model/motion references differ')
            pairs=list(dict.fromkeys((r['bin'],r['tpl']) for r in refs if r['type']!=11))
            if len(pairs)!=len(value['model_initializations']): raise ValueError('model pair count')
            for (bn,tn),init in zip(pairs,value['model_initializations']):
                if (init['bin'],init['tpl'])!=(bn,tn): raise ValueError('model/TPL association changed')
                raw,rawtex=resource(bn),resource(tn)
                if raw is None or rawtex is None:
                    if init['status']!='external-resource-not-bound': raise ValueError('missing resource falsely bound')
                    continue
                if bn not in entries and next(r for r in refs if r['bin']==bn)['type']==4:
                    if init['status']!='original-SetPartsSub-in-registration-pass': raise ValueError('sparse attachment used as standalone skeleton')
                    continue
                if init['status']!='original-modelInit-two-rounds-released': raise ValueError('original model execution missing')
                n=struct.unpack_from('>H',raw,0x38)[0]; off=struct.unpack_from('>I',raw,0x30)[0]
                vertices=[struct.unpack_from('>hhh',raw,off+8*i) for i in range(n)]
                if not vertices: raise ValueError('empty reference bbox must be reviewed')
                low=[min(v[k] for v in vertices)/(1<<raw[0x28]) for k in range(3)]
                high=[max(v[k] for v in vertices)/(1<<raw[0x28]) for k in range(3)]
                box=[(low[k]+high[k])/2 for k in range(3)]+[(high[k]-low[k])/2 for k in range(3)]
                if init['parts']!=raw[0x19] or init['vertices']!=n or init['textures']!=struct.unpack_from('>I',rawtex,4)[0]: raise ValueError('model counts/TPL descriptors')
                if any(abs(a-b)>max(1e-4,abs(b)*1e-5) for a,b in zip(init['bounding_box'],box)): raise ValueError('original bounding box differs from independent vertices')
            bodies=list(dict.fromkeys(r['model'] for r in refs if r['type']==3 and resource(r['bin']) is not None and resource(r['tpl']) is not None))
            parts=list(dict.fromkeys(r['model'] for r in refs if r['type']==4))
            registered=value['event_registration']
            if [r['name'] for r in registered]!=bodies+parts: raise ValueError('original SetMod/SetPartsSub registration coverage')
            for slot,r in enumerate(registered):
                if r['slot']!=slot or r['type']!=(2 if slot<len(bodies) else 3): raise ValueError('original model table type/order')
                if r['type']==3:
                    ref=next(ref for ref in refs if ref['model']==r['name'] and ref['type']==4)
                    if r['parent']!=ref['parent'] or r['parts']!=resource(ref['bin'])[25]: raise ValueError('attached hierarchy/model references')
            expected_motions=[r for r in refs if r['type']==11 and r['model'] in bodies and resource(r['bin'])[:2]!=b'\0\0']
            if len(value['motion_initializations'])!=len(expected_motions): raise ValueError('actual MotionSetCore coverage')
            def axis_value(axis,fmt,frame,max_frame):
                values=[fcv.to_float(fcv.FCC_FMT[fmt][0],k[0]) for k in axis.keys]
                if frame>=max_frame: return values[-1]
                if frame in axis.frames: return values[axis.frames.index(frame)]
                i=next(i for i in range(len(axis.frames)-1) if axis.frames[i]<frame<axis.frames[i+1])
                t=(frame-axis.frames[i])/(axis.frames[i+1]-axis.frames[i]); t2=t*t; t3=t2*t
                tangents=fcv.FCC_FMT[fmt][1]
                a=fcv.to_float(tangents,axis.keys[i][2]); b=fcv.to_float(tangents,axis.keys[i+1][1])
                return (2*t3-3*t2+1)*values[i]+(-2*t3+3*t2)*values[i+1]+(t3-2*t2+t)*a+(t3-t2)*b
            for got,ref in zip(value['motion_initializations'],expected_motions):
                motion=fcv.parse(resource(ref['bin']))
                if got['model']!=ref['model'] or got['bin']!=ref['bin'] or got['joints']!=len(motion.joints) or got['max_frame']!=motion.max_frame:
                    raise ValueError('original motion identity/frame/joint count')
                for field,kind in [('null_pos',1),('null_rot',0x40)]:
                    expected=next((i for i,j in enumerate(motion.joints) if j.kind==kind),65535)
                    if got[field]!=expected: raise ValueError('original MotionSetCore root assignment')
                if not motion.joints:
                    if got['samples']: raise ValueError('empty player FCV must not fabricate samples')
                    continue
                first=motion.joints[0]
                for n,sample in enumerate(got['samples']):
                    reference=[axis_value(a,first.fcc_type,motion.max_frame*n/2,motion.max_frame) for a in first.axes]
                    if any(abs(a-b)>max(2e-5,abs(b)*2e-5) for a,b in zip(sample,reference)):
                        raise ValueError('actual original Hermite differs from independent FCV curves')
            player_motions=[(i,blob) for i,(tag,blob) in enumerate(player_drs.entries) if tag==b'FCV\0'] if opening else []
            if len(value['player_motion_initializations'])!=len(player_motions): raise ValueError('player FCV execution coverage')
            for got,(index,blob) in zip(value['player_motion_initializations'],player_motions):
                motion=fcv.parse(blob)
                if got['index']!=index or got['sha256']!=sha(blob) or got['joints']!=len(motion.joints) or got['max_frame']!=motion.max_frame:
                    raise ValueError('player FCV identity/ABI data')
                for field,kind in [('null_pos',1),('null_rot',0x40)]:
                    expected=next((i for i,j in enumerate(motion.joints) if j.kind==kind),65535)
                    if got[field]!=expected: raise ValueError('player original motion root assignment')
                if not motion.joints:
                    if got['samples']: raise ValueError('empty player FCV must not fabricate samples')
                    continue
                first=motion.joints[0]
                for n,sample in enumerate(got['samples']):
                    reference=[axis_value(a,first.fcc_type,motion.max_frame*n/2,motion.max_frame) for a in first.axes]
                    if any(abs(a-b)>max(2e-5,abs(b)*2e-5) for a,b in zip(sample,reference)):
                        raise ValueError('player original Hermite differs from independent FCV curves')
        return sha(json.dumps(actual['packages'],sort_keys=True).encode())
    def verify(actual,reference,disc):
        if actual.get('isolated_collision_reset') is not True:
            raise ValueError('real SAT/EAT runtime reset lifecycle was not verified')
        if len(actual['rooms']) != len(reference): raise ValueError('room result count')
        for value,expected in zip(actual['rooms'],reference):
            for key in ['id','source_sha256','body_sha256','body_size']:
                if value[key] != expected[key]: raise ValueError('original archive result differs: '+key)
            if value['entries'] != len(expected['entries']): raise ValueError('archive entry count')
            if value['blocks'] != [{k:b[k] for k in ['path','sha256']} for b in expected['blocks']]: raise ValueError('block result differs')
            esl = files[(disc,value['esl_path'].lower())]
            if value['esl_sha256'] != esl['sha256'] or value['esl_size'] != esl['size'] or not value['original_read_observed']:
                raise ValueError('actual original readEmList differs')
            archive=next(f for f in manifest['files'] if f['disc_number']==disc and f['sha256']==value['source_sha256'])
            for consumer in value['consumers']:
                entry=next(e for e in archive['analysis']['entries'] if e['index']==consumer['index'])
                if consumer['sha256']!=entry['sha256'] or consumer['tag']!=entry['tag']: raise ValueError('CAM/MDT source identity')
                if consumer['tag']=='MDT':
                    english=expected['english_mdt']
                    if consumer['status']!=('open' if english['status']=='open' else 'indexed'): raise ValueError('English MDT status differs')
                    if consumer['status']=='indexed' and consumer['count']!=english['count']: raise ValueError('original English message count')
                elif consumer['status']!='indexed' and consumer.get('reason')!='CAM pFrame outside immutable resource; relocation semantics unresolved':
                    raise ValueError('unexpected CAM validation failure: '+consumer.get('reason',''))
        if actual['scenario_blocker'] != 'Original consumer unavailable: SceAtSetSaveItem': raise ValueError('shared scenario missing-dependency behavior')
        return sha(json.dumps(actual['rooms'],sort_keys=True).encode())
    def execute(command,abi,bits,targetRoot,targetCatalog,exeHash,fixtureRoot):
        for repeat in range(1,args.repeat+1):
            isolated = json.loads(run([*command,'self-test',str(fixtureRoot)+'/game-fixtures-'+abi]))
            if isolated['pointer_bits'] != bits or isolated['failed'] or isolated['checks'] < 634:
                raise ValueError('isolated native runtime test/ABI')
            for disc in sorted(int(d) for d in cases):
                for new_game in [False,True]:
                    boot=json.loads(run([*command,'new-game' if new_game else 'messages',targetRoot,targetCatalog,str(disc)]))
                    boot_trace=verify_messages(boot,disc,bits,new_game)
                    message_results.append(dict(abi=abi,round=repeat,disc=disc,new_game=new_game,trace_sha256=boot_trace,actual=boot,executable_sha256=exeHash))
                    write_json(output/f'messages-{abi}.json',dict(results=[r for r in message_results if r['abi']==abi]))
                    print(f'PASS {abi} round {repeat} disc {disc} new_game={new_game}: actual GameTask gameStageInit, aggregate save and room-entry transition',flush=True)
            for disc in sorted({f['disc_number'] for f in packages}):
                expected=[f for f in packages if f['disc_number']==disc]
                actual=json.loads(run([*command,'consumers',targetRoot,targetCatalog,str(disc),*[f['path'] for f in expected]]))
                trace=verify_consumers(actual,expected,bits)
                consumer_results.append(dict(abi=abi,round=repeat,disc=disc,trace_sha256=trace,actual=actual,executable_sha256=exeHash))
                write_json(output/f'consumers-{abi}.json',dict(results=[r for r in consumer_results if r['abi']==abi]))
                print(f'PASS {abi} round {repeat}: {len(expected)} real EVD packages, original modelInit/TPL/matrices and cancel callbacks; Event::Init remains open',flush=True)
            for disc in sorted({f['disc_number'] for f in additional}):
                expected=[f for f in additional if f['disc_number']==disc]
                actual=json.loads(run([*command,'models',targetRoot,targetCatalog,str(disc),*[f['path'] for f in expected]]))
                trace=verify_consumers(actual,expected,bits,False)
                model_results.append(dict(abi=abi,round=repeat,disc=disc,trace_sha256=trace,actual=actual,executable_sha256=exeHash))
                write_json(output/f'models-{abi}.json',dict(results=[r for r in model_results if r['abi']==abi]))
                print(f'PASS {abi} round {repeat} disc {disc}: additional original models from '+','.join(f['path'] for f in expected),flush=True)
            for disc,group in cases.items():
                expected=[]
                for r in group:
                    f=next(f for f in manifest['files'] if f['disc_number']==int(disc) and f['sha256']==r['source_sha256'])
                    if f not in expected:expected.append(f)
                room_ids=list(dict.fromkeys(r['id'] for r in group))
                actual_lights=json.loads(run([*command,'lights',targetRoot,targetCatalog,disc,*room_ids]))
                lt=verify_lights(actual_lights,int(disc),bits,expected)
                light_results.append(dict(abi=abi,round=repeat,disc=int(disc),trace_sha256=lt,actual=actual_lights,executable_sha256=exeHash))
                write_json(output/f'lights-{abi}.json',dict(results=[r for r in light_results if r['abi']==abi]))
                print(f'PASS {abi} round {repeat} disc {disc}: actual original light CPU consumers, {actual_lights["checks"]} checks',flush=True)
                actual = json.loads(run([*command,'data',targetRoot,targetCatalog,disc,*[r['id'] for r in group]]))
                if actual['pointer_bits'] != bits: raise ValueError('actual ABI')
                trace = verify(actual,group,int(disc))
                results.append(dict(abi=abi,round=repeat,disc=int(disc),passed=len(group),failed=0,
                    trace_sha256=trace,isolated=isolated,actual=actual,executable_sha256=exeHash))
                write_json(output/f'game-runtime-{abi}.json',dict(results=[r for r in results if r['abi']==abi],catalog_sha256=sha(catalog.read_bytes())))
                print(f'PASS {abi} round {repeat} disc {disc}: {len(group)} real archive/ESL preparations; full scenario explicitly blocked',flush=True)
    if args.host_exe:
        exe=Path(args.host_exe).resolve()
        probe=json.loads(run([exe,'self-test',output/'host-probe']))
        execute([exe],'host-'+str(probe['pointer_bits']),probe['pointer_bits'],data,catalog,sha(exe.read_bytes()),output)
    if args.adb:
        if not args.serial: raise ValueError('serial required')
        def adb(*parts): return run([args.adb,'-s',args.serial,*parts])
        supported=adb('shell','getprop','ro.product.cpu.abilist').split(',')
        remote='/data/local/tmp/re4-game-'+uuid.uuid4().hex
        adb('shell','mkdir','-p',remote+'/blobs'); adb('push',catalog,remote+'/catalog.tsv')
        if adb('shell','sha256sum',remote+'/catalog.tsv').split()[0] != sha(catalog.read_bytes()): raise ValueError('catalog transfer')
        for digest in sorted(digests):
            adb('push',data/'blobs'/digest,remote+'/blobs/'+digest)
            if adb('shell','sha256sum',remote+'/blobs/'+digest).split()[0] != digest: raise ValueError('blob transfer')
        write_json(output/'device.json',dict(serial=args.serial,android=adb('shell','getprop','ro.build.version.release'),
            model=adb('shell','getprop','ro.product.model'),supported_abis=supported,remote=remote))
        for abi,bits in [('armeabi-v7a',32),('arm64-v8a',64)]:
            if abi not in supported: raise ValueError('unsupported ABI')
            exe=root/f'port/build/android-{abi}/re4_game_tool'
            target=remote+'/game-'+abi
            adb('push',exe,target); adb('shell','chmod','755',target)
            digest=sha(exe.read_bytes())
            if adb('shell','sha256sum',target).split()[0] != digest: raise ValueError('executable transfer')
            execute([args.adb,'-s',args.serial,'shell',target],abi,bits,remote,remote+'/catalog.tsv',digest,remote)
    for disc in cases:
        if len({r['trace_sha256'] for r in results if r['disc']==int(disc)}) != 1: raise ValueError('ABI/repeat difference')
    if len({r['trace_sha256'] for r in consumer_results})!=1: raise ValueError('consumer ABI/repeat difference')
    for new_game in (False,True):
        if len({r['trace_sha256'] for r in message_results if r['new_game']==new_game})!=1:
            raise ValueError('message ABI/repeat difference for startup input '+str(new_game))
    for disc in {r['disc'] for r in model_results}:
        if len({r['trace_sha256'] for r in model_results if r['disc']==disc})!=1: raise ValueError('additional model ABI/repeat difference')


if __name__ == '__main__':
    raise SystemExit(main())
