#!/usr/bin/env python3
"""Summarize physical-event evidence without claiming virtual input is physical."""
import argparse
import json
import re
from functools import reduce
from operator import or_
from pathlib import Path


def analyze(text, bits):
    native=[]; raw=[]; keys={}; axes=[]; device=[]
    for line in text.splitlines():
        match=re.search(r'\b(RE4Controller(?:Native|Raw|Device))\s*:\s*(.*)',line)
        if not match: continue
        tag,value=match.groups()
        if tag.endswith('Native'):
            item=json.loads(value)
            if item['pointer_bits']==bits: native.append(item)
        elif tag.endswith('Raw'):
            raw.append(value)
            key=re.search(r'key=(\S+) action=([01])',value)
            if key: keys.setdefault(key[1],set()).add(int(key[2]))
            motion=re.search(r'axes=([\d.eE+,-]+)',value)
            if motion: axes.append([float(v) for v in motion[1].split(',')])
        else: device.append(value)
    if not native: raise ValueError('no matching actual native process ABI')
    return dict(pointer_bits=bits,raw_events=len(raw),native_snapshots=len(native),device=device,
        native_joy_bits_observed=reduce(or_,(v['joy_on'] for v in native),0),
        key_actions={key:sorted(actions) for key,actions in sorted(keys.items())},
        android_axis_ranges=[[min(a[i] for a in axes),max(a[i] for a in axes)] for i in range(8)] if axes else [],
        native_stick_ranges=[[min(v['stick'][i] for v in native),max(v['stick'][i] for v in native)] for i in range(4)],
        native_trigger_values=[sorted({v['triggers'][i] for v in native}) for i in range(2)],
        press_samples=sum(bool(v['joy_trg']) for v in native),release_samples=sum(bool(v['joy_rel']) for v in native),
        repeat_samples=sum(bool(v['key_rep']) and not v['key_trg'] for v in native))


def require_standard_controls(result):
    required=['A','B','X','Y','L1','R1','START']
    for button in required:
        if result['key_actions'].get('KEYCODE_BUTTON_'+button) != [0,1]:
            raise ValueError('physical press/release not captured: '+button)
    if len(result['android_axis_ranges']) != 8:
        raise ValueError('missing physical motion events')
    for axis,(low,high) in enumerate(result['android_axis_ranges']):
        if low > (0 if axis >= 6 else -0.9) or high < 0.9:
            raise ValueError('physical axis range not captured: '+str(axis))
    header=(Path(__file__).resolve().parents[2]/'include/joy.h').read_text()
    mask=0
    for name in ['LEFT','RIGHT','DOWN','UP','Z','R','L','A','B','X','Y','START']:
        bit=re.search(r'^#define JOY_'+name+r'\s+(0x[0-9A-Fa-f]+)',header,re.M)
        if not bit: raise ValueError('original JOY definition not found: '+name)
        mask |= int(bit[1],16)
    if result['native_joy_bits_observed'] & mask != mask:
        raise ValueError('original PadRead did not observe every required button/hat')
    for values in result['native_trigger_values']:
        if 0 not in values or 150 not in values or not any(0<v<150 for v in values):
            raise ValueError('original trigger clamp lacks neutral, intermediate or full value')
    if not result['human_confirmed']:
        raise ValueError('physical capture requires human confirmation')
    result['required_controls_verified']=True


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--log',required=True,type=Path)
    p.add_argument('--bits',type=int,choices=[32,64],required=True)
    p.add_argument('--output',required=True,type=Path)
    p.add_argument('--human-confirmed',action='store_true')
    p.add_argument('--require-standard-controls',action='store_true')
    args=p.parse_args()
    result=analyze(args.log.read_text(encoding='utf-8-sig'),args.bits)
    result['human_confirmed']=args.human_confirmed
    result['evidence_scope']='Android input dispatch to original native PadRead; diagnostic mode, no playable room'
    if args.require_standard_controls: require_standard_controls(result)
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k not in ['device','native_trigger_values']},indent=2))
