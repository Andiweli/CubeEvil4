#!/usr/bin/env python3
"""Execute the imported real-data validation corpus on actual Android ABIs.

Only private CAS inputs are transferred to /data/local/tmp. No asset enters an APK.
"""
import argparse
import hashlib
import json
import re
import subprocess
import sys
import uuid
from pathlib import Path
sys.dont_write_bytecode=True
sys.stdout.reconfigure(encoding="utf-8",errors="backslashreplace")
from original_import.pipeline import external_root, write_json, sha


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--adb",required=True)
    parser.add_argument("--serial",required=True)
    parser.add_argument("--data",required=True)
    parser.add_argument("--abi",action="append",choices=["armeabi-v7a","arm64-v8a"])
    parser.add_argument("--repeat",type=int,default=2)
    args=parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_.:-]+",args.serial) or not 1<=args.repeat<=10:
        raise ValueError("invalid serial/repeat")
    root=Path(__file__).resolve().parents[2]
    data=external_root(args.data,root,[])
    cases=json.loads((data/"native-cases.json").read_text())
    for case in cases:
        if not re.fullmatch(r"[0-9a-f]{64}",case["blob"]):raise ValueError("invalid CAS identifier")
    def adb(*parts):
        result=subprocess.run([args.adb,"-s",args.serial,*parts],capture_output=True,text=True,timeout=120)
        if result.returncode:raise RuntimeError(result.stderr.strip() or result.stdout.strip())
        return result.stdout.strip()
    info=dict(serial=args.serial,model=adb("shell","getprop","ro.product.model"),
              android=adb("shell","getprop","ro.build.version.release"),api=adb("shell","getprop","ro.build.version.sdk"),
              supported_abis=adb("shell","getprop","ro.product.cpu.abilist").split(","))
    remote="/data/local/tmp/re4-port-import/"+uuid.uuid4().hex
    adb("shell","mkdir","-p",remote+"/blobs")
    inputs=sorted({case["blob"] for case in cases})
    print(f"{info}: deploying {len(inputs)} verified private blobs",flush=True)
    for i,digest in enumerate(inputs):
        path=data/"blobs"/digest
        if sha(path.read_bytes())!=digest:raise ValueError("input SHA-256 mismatch")
        adb("push",str(path),remote+"/blobs/"+digest)
        # ADB transfer acknowledgement is not the integrity proof.
        if adb("shell","sha256sum",remote+"/blobs/"+digest).split()[0]!=digest:
            raise ValueError("device input SHA-256 mismatch")
        if (i+1)%100==0:print(f"deploy {i+1}/{len(inputs)}",flush=True)
    results=[]
    for abi in args.abi or ["armeabi-v7a","arm64-v8a"]:
        if abi not in info["supported_abis"]:raise ValueError(f"unsupported ABI {abi}")
        executable=root/f"port/build/android-{abi}/re4_asset_tool"
        target=remote+"/re4_asset_tool-"+abi
        adb("push",str(executable),target);adb("shell","chmod","755",target)
        self_test=adb("shell",target,"self-test")
        bits=32 if abi=="armeabi-v7a" else 64
        if f"pointer={bits}" not in self_test:raise ValueError("wrong actual process ABI")
        if adb("shell","sha256sum",target).split()[0]!=sha(executable.read_bytes()):raise ValueError("executable transfer mismatch")
        for round_no in range(1,args.repeat+1):
            digest=hashlib.sha256()
            for i,case in enumerate(cases):
                parts=[target,case["mode"],remote+"/blobs/"+case["blob"]]
                destination=remote+f"/decode-{abi}-{round_no}-{i}"
                if case["mode"]=="decode":parts.append(destination)
                result=json.loads(adb("shell",*parts))
                if case["mode"]=="decode":
                    result["sha256"]=adb("shell","sha256sum",destination).split()[0]
                    adb("shell","rm",destination) # Only our exact newly created file.
                if result!=case["expected"]:raise ValueError(f"{abi} case differs: {case['blob']}")
                digest.update(json.dumps(result,sort_keys=True,separators=(",",":")).encode())
                if (i+1)%100==0:print(f"{abi} round {round_no}: {i+1}/{len(cases)} pass",flush=True)
            results.append(dict(abi=abi,round=round_no,passed=len(cases),failed=0,trace_sha256=digest.hexdigest(),
                                executable_sha256=sha(executable.read_bytes()),self_test=self_test))
            write_json(data/"android-import-tests.json",dict(device=info,remote_directory=remote,results=results,
                        case_manifest_sha256=sha((data/"native-cases.json").read_bytes())))
            print(f"PASS {abi} round {round_no}: {len(cases)} cases; trace {digest.hexdigest()}",flush=True)
    if len({r["trace_sha256"] for r in results})!=1:raise ValueError("ABI/repeat trace mismatch")
    print("PASS: every real-data case and repeated ABI trace matches",flush=True)


if __name__=="__main__":main()
