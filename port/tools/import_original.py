#!/usr/bin/env python3
"""Import immutable G4BE08 images into a private, content-addressed local store."""
import argparse
import json
import sys
from pathlib import Path
sys.dont_write_bytecode = True
sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
from original_import.pipeline import run, native, sha, external_root, write_json


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    commands=parser.add_subparsers(dest="command",required=True)
    build=commands.add_parser("import")
    build.add_argument("--disc",action="append",required=True)
    build.add_argument("--output",required=True)
    build.add_argument("--native-tool",required=True)
    build.add_argument("--oracle-all",action="store_true",help="compare every unique compressed body with tools/motion/yz2.py")
    verify=commands.add_parser("verify-native")
    verify.add_argument("--data",required=True)
    verify.add_argument("--native-tool",required=True)
    verify.add_argument("--report",required=True)
    report=commands.add_parser("report")
    report.add_argument("--data",required=True)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[2]
    if args.command=="import":
        run(root,args.output,args.disc,args.native_tool,args.oracle_all)
    elif args.command=="report":
        from original_import.reports import enrich
        enrich(external_root(args.data,root,[]))
    else:
        import tempfile
        data=external_root(args.data,root,[])
        report=Path(args.report).resolve()
        if root == report or root in report.parents or report.parent != data:
            raise ValueError("verification report must be directly inside private data root")
        cases=json.loads((data/"native-cases.json").read_text())
        passed=0
        for case in cases:
            path=data/"blobs"/case["blob"]
            if sha(path.read_bytes())!=case["blob"]:
                raise ValueError("case input SHA-256 mismatch")
            if case["mode"]=="decode":
                with tempfile.NamedTemporaryFile(dir=data,delete=False) as stream: destination=Path(stream.name)
                try:
                    result=native(args.native_tool,"decode",path,destination)
                    result["sha256"]=sha(destination.read_bytes())
                finally:destination.unlink(missing_ok=True)
            else:
                result=native(args.native_tool,case["mode"],path)
            if result!=case["expected"]: raise ValueError(f"native verification differs: {case['blob']}")
            passed+=1
            if passed%50==0:print(f"native corpus: {passed}/{len(cases)} pass",flush=True)
        result=dict(passed=passed,failed=0,executable_sha256=sha(Path(args.native_tool).read_bytes()),
                    case_manifest_sha256=sha((data/"native-cases.json").read_bytes()))
        write_json(report,result)
        print(json.dumps(result,indent=2))


if __name__=="__main__":
    main()
