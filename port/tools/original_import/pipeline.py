import copy
import csv
import hashlib
import json
import re
import struct
import subprocess
import tempfile
from contextlib import contextmanager
from collections import Counter, defaultdict
from pathlib import Path
from .gcm import Disc, bounded
from . import formats
from .source_refs import references

MAX_RESOURCE = 64 * 1024 * 1024


def sha(data):
    return hashlib.sha256(data).hexdigest()


def external_root(path, repository, images):
    path = Path(path).resolve()
    if path == Path(path.anchor) or path == repository or repository in path.parents:
        raise ValueError("output must be a separate private data directory outside the repository")
    for image in images:
        image = Path(image).resolve()
        if path == image or path in image.parents:
            raise ValueError("output must not contain the input images")
    return path


class Store:
    def __init__(self, root):
        self.root = root
        self.blobs = root / "blobs"
        self.blobs.mkdir(parents=True, exist_ok=True)
        if self.blobs.resolve().parent != root:
            raise ValueError("blob directory escapes private output root")

    def put(self, data):
        digest = sha(data)
        path = self.blobs / digest
        if path.exists():
            if sha(path.read_bytes()) != digest:
                raise ValueError("existing blob failed SHA-256 validation")
        else:
            with path.open("xb") as stream:
                stream.write(data)
        return digest

    def extract(self, disc, entry):
        digest = hashlib.sha256()
        with disc.path.open("rb") as stream, tempfile.NamedTemporaryFile(dir=self.blobs, delete=False) as output:
            temporary = Path(output.name)
            stream.seek(entry["offset"])
            remaining = entry["size"]
            while remaining:
                chunk = stream.read(min(remaining, 4 * 1024 * 1024))
                if not chunk:
                    raise ValueError("short resource read")
                digest.update(chunk)
                output.write(chunk)
                remaining -= len(chunk)
        value = digest.hexdigest()
        target = self.blobs / value
        if target.exists():
            check = hashlib.sha256()
            with target.open("rb") as stream:
                for chunk in iter(lambda: stream.read(4 * 1024 * 1024), b""):
                    check.update(chunk)
            if check.hexdigest() != value:
                raise ValueError("existing blob failed integrity check")
            temporary.unlink()
        else:
            temporary.rename(target)
        return value


def native(tool, mode, *paths):
    result = subprocess.run([str(tool), mode, *(str(p) for p in paths)], capture_output=True, text=True, timeout=120)
    if result.returncode:
        raise ValueError(f"native {mode}: {result.stderr.strip()}")
    return json.loads(result.stdout)


class Importer:
    def __init__(self, root, output, tool, oracle_all=False):
        self.root, self.store, self.tool = root, Store(output), Path(tool).resolve(strict=True)
        self.modules = formats.readers(root)
        self.oracle_all = oracle_all
        self.native_cases = []
        self.cache = {}

    def case(self, mode, digest, expected):
        self.native_cases.append(dict(mode=mode, blob=digest, expected=expected))

    def analyse(self, entry):
        path, digest = entry["path"], entry["sha256"]
        ext = Path(path).suffix.lower()
        if entry["size"] > MAX_RESOURCE:
            return dict(status="preserved", format="opaque", detail="larger than metadata parser policy")
        # Identical bytes can have different declared types. Deduplicate storage,
        # not their logical tag identities (e.g. zero-filled FCV versus TPL).
        cache_key=(digest,ext)
        if cache_key in self.cache:
            return copy.deepcopy(self.cache[cache_key])
        data = (self.store.blobs / digest).read_bytes()
        result = dict(status="preserved", format="opaque", entries=[])
        try:
            if ext in (".das", ".drs"):
                result = self.container(data, digest)
            elif data[:5] == b"event":
                entries = formats.event_index(data, self.modules["archive"])
                result = dict(status="indexed", format="event", entries=self.entries(data, entries),
                              scope="named bins; original event packet stream preserved without interpretation")
            elif ext == ".dat" and len(data) >= 16 and data[8:16] == bytes(8) and 0 < formats.u32(data, 0) <= (len(data)-16)//8:
                result = self.body(data, digest, bare=True)
            elif ext == ".rel":
                result = dict(status="indexed", format="REL", rel=formats.validate_rel(data), entries=[])
            elif ext in (".fcv", ".eff", ".tpl", ".bin", ".mdt"):
                tag = ext[1:].upper()
                result = dict(status="indexed", format="standalone", entries=self.entries(data, [dict(index=0,offset=0,size=len(data),tag=tag)]))
        except (ValueError, AssertionError, IndexError, struct.error, OverflowError) as error:
            result = dict(status="open", format=ext.lstrip("."), detail=str(error) or type(error).__name__, entries=[])
        self.cache[cache_key] = copy.deepcopy(result)
        return result

    def body(self, data, digest, bare=False):
        entries, rel = formats.body_index(data)
        expected = dict(rel_offset=rel, entries=[[e["index"], e["offset"], e["size"], e["occurrence"]] for e in entries])
        if native(self.tool, "body", self.store.blobs / digest) != expected:
            raise ValueError("native/Python body index disagreement")
        self.case("body", digest, expected)
        if bare:
            reference = self.modules["archive"].parse_body(data)
            if [(e["tag"], data[e["offset"]:e["offset"]+e["size"]]) for e in entries] != reference:
                raise ValueError("tools/motion/archive.py body disagreement")
        result = dict(status="indexed", format="body", body_sha256=digest, rel_offset=rel, entries=self.entries(data, entries))
        if rel:
            result["rel"] = formats.validate_rel(data[rel:])
        return result

    def container(self, data, digest):
        table = formats.records(data)
        if len(table) != 2:
            raise ValueError("unknown container record count")
        check = native(self.tool, "container", self.store.blobs / digest)
        self.case("container", digest, check)
        body_record, sound = table
        body = data[body_record[3]:body_record[3]+body_record[1]]
        compressed = self.modules["yz2"].is_yz2(body)
        decode = None
        if compressed:
            packed, size, coded_offset = self.modules["yz2"].parse_header(body)
            if not 0 < size <= MAX_RESOURCE or coded_offset >= len(body):
                raise ValueError("Yz2 unpacked size/header exceeds policy")
            input_digest = self.store.put(body)
            with tempfile.NamedTemporaryFile(dir=self.store.blobs, delete=False) as stream:
                destination = Path(stream.name)
            try:
                decode = native(self.tool, "decode", self.store.blobs/input_digest, destination)
                raw_body = destination.read_bytes()
            finally:
                destination.unlink(missing_ok=True)
            if len(raw_body) != size:
                raise ValueError("Yz2 length mismatch")
            decode.update(input_sha256=input_digest, output_sha256=sha(raw_body),
                          header_packed=packed, coded_offset=coded_offset, oracle="not run")
            # all compressed resources by default in the reproducibility run;
            # disabling it is visibly recorded, never reported as verified.
            if self.oracle_all:
                reference, ref_packed, consumed = self.modules["yz2"].decode(body)
                if raw_body != reference or packed != ref_packed or decode["consumed"] != consumed + coded_offset:
                    raise ValueError("original Python Yz2/native decoder disagreement")
                decode.update(oracle="byte-identical", oracle_consumed=consumed)
            self.case("decode", input_digest, dict(size=len(raw_body), sha256=sha(raw_body), consumed=decode["consumed"]))
            body = raw_body + bytes((-len(raw_body)) & 31)
        body_digest = self.store.put(body)
        result = self.body(body, body_digest)
        result.update(format="DAS" if compressed else "DRS", compressed=compressed,
                      signature_hex=data[:32].hex(), records=[list(r) for r in table], decode=decode)
        # Reuse Drs, including bounded embedded REL relocations and SoundBank.
        if result["rel_offset"]:
            formats.validate_rel(body[result["rel_offset"]:])
        normalized = self.modules["drs"].write_records(data[:32], [
            (0,len(body),0,0x400,0,0,0,0),
            (sound[0],sound[1],0,0x400+len(body),0,0,0,0)]) + body + data[sound[3]:sound[3]+sound[1]]
        reference = self.modules["drs"].Drs(normalized)
        if reference.pack() != normalized:
            raise ValueError("Drs roundtrip disagrees")
        result["drs_roundtrip"] = "byte-identical decompressed container" if compressed else "byte-identical original container"
        if sound[1]:
            result["sound"] = [dict(index=i,type=r[0],offset=r[3],size=r[1],parameters=list(r[4:6]),
                                    sha256=sha(data[sound[3]+r[3]:sound[3]+r[3]+r[1]]),status="opaque")
                               for i,r in enumerate(formats.records(data,sound[3]))]
        return result

    def entries(self, data, entries, depth=0):
        if depth > 8:
            raise ValueError("nested archive depth exceeds policy")
        result = []
        for entry in entries:
            entry = dict(entry)
            off, size = entry["offset"], entry["size"]
            bounded(len(data), off, size)
            blob = data[off:off+size]
            entry["sha256"] = sha(blob)
            entry["validation"] = formats.semantic(entry["tag"], blob, self.modules)
            if entry["tag"] == "ETM" and blob:
                try:
                    children = formats.etm_index(blob, self.modules["archive"])
                    entry["children"] = self.entries(blob, children, depth+1)
                    entry["validation"] = dict(status="indexed", parser="tools/motion/archive.py:parse_etm")
                except (ValueError, AssertionError, IndexError, struct.error) as error:
                    entry["validation"] = dict(status="open",detail=str(error))
            elif entry["tag"] == "SMD" and blob:
                try:
                    bounded(len(blob), 0, 16)
                    offset = formats.u32(blob, 12)
                    bounded(len(blob), offset, 0)
                    if offset != len(blob):
                        first = formats.u32(blob, offset)
                        if not first or first % 4:
                            raise ValueError("invalid SMD motion table")
                        bounded(len(blob),offset,first)
                        offsets = [formats.u32(blob,offset+i*4) for i in range(first//4)]
                        previous = first
                        for ref in offsets:
                            if ref < previous: raise ValueError("unordered SMD motions")
                            bounded(len(blob),offset+ref,0)
                            previous = ref
                    motions = self.modules["archive"].smd_motions(blob)
                    entry["validation"] = dict(status="indexed", motion_count=len(motions), scope="motion table; geometry remains opaque")
                except (ValueError, AssertionError, IndexError, struct.error) as error:
                    entry["validation"] = dict(status="open",detail=str(error))
            result.append(entry)
        return result


@contextmanager
def atomic_text(path, newline=None):
    # Replace the directory entry rather than following an existing file symlink.
    with tempfile.NamedTemporaryFile(dir=path.parent,mode="w",encoding="utf-8",newline=newline,delete=False) as stream:
        temporary=Path(stream.name)
        try:
            yield stream
        except BaseException:
            stream.close()
            temporary.unlink(missing_ok=True)
            raise
    temporary.replace(path)


def write_json(path, data):
    with atomic_text(path) as stream:
        stream.write(json.dumps(data,ensure_ascii=True,sort_keys=True,indent=2)+"\n")


def walk_entries(entries, prefix=""):
    for entry in entries:
        identity = prefix + str(entry["index"])
        yield identity, entry
        yield from walk_entries(entry.get("children", []), identity + "/")


def run(root, output, images, tool, oracle_all):
    root = Path(root).resolve()
    output = external_root(output, root, images)
    importer = Importer(root, output, tool, oracle_all)
    discs, files = [], []
    for image in images:
        disc = Disc(image, root)
        if disc.info["game_id"] != "G4BE08" or not disc.info["build_date_evidence"]:
            raise ValueError("image does not supply required G4BE08/Nov 25 2004 evidence")
        number = disc.info["disc_number"]
        if number in [d["disc_number"] for d in discs]:
            raise ValueError("duplicate disc number")
        print(f"disc {number}: hashing image and extracting {len(disc.files)} FST files",flush=True)
        before = disc.hash_image()
        info = dict(disc.info, image_sha256=before, source_path=str(disc.path), entries=disc.entries)
        info["system_blobs"] = dict(fst=importer.store.put(disc.fst), dol=importer.store.put(disc.dol),
                                     boot=importer.store.put(disc.header), bi2=importer.store.put(disc.read(0x440,0x2000)))
        app_header = disc.read(0x2440,32)
        app_size = 32 + formats.u32(app_header,20) + formats.u32(app_header,24)
        bounded(disc.dol_offset,0x2440,app_size,"apploader")
        info["system_blobs"]["apploader"] = importer.store.put(disc.read(0x2440,app_size))
        for index, entry in enumerate(disc.files):
            digest = importer.store.extract(disc, entry)
            file = dict(entry, id=f"disc{number}:fst{entry['index']}",disc_number=number,sha256=digest)
            file["analysis"] = importer.analyse(file)
            files.append(file)
            if index % 25 == 0:
                print(f"disc {number}: {index+1}/{len(disc.files)} {entry['path']}: {file['analysis']['status']}",flush=True)
        after = disc.hash_image()
        if before != after:
            raise ValueError("source image changed during import")
        info["source_unchanged_sha256"] = after
        discs.append(info)
    if set(d["disc_number"] for d in discs) != {0,1}:
        raise ValueError("expected original disc numbers 0 and 1")
    source = references(root, files)
    groups, variants = defaultdict(list), defaultdict(list)
    for file in files:
        groups[file["sha256"]].append(file["id"])
        variants[file["path"].casefold()].append(dict(id=file["id"],sha256=file["sha256"]))
    tools = ["tools/extract_orig.py","tools/drs.py","tools/motion/archive.py","tools/motion/yz2.py",
             "tools/motion/fcv.py","tools/motion/modelbin.py","tools/motion/meshbin.py","tools/motion/gxtex.py"]
    manifest = dict(schema=1, byte_order="big", reference_language="eng", discs=discs, files=files,
                    duplicates={k:v for k,v in groups.items() if len(v)>1},
                    path_variants={k:v for k,v in variants.items() if len({e['sha256'] for e in v})>1},
                    parser_sources={p:sha((root/p).read_bytes().replace(b"\r\n",b"\n")) for p in tools})
    write_json(output/"manifest.json",manifest)
    write_json(output/"source-references.json",source)
    cases = {json.dumps(c,sort_keys=True):c for c in importer.native_cases}
    write_json(output/"native-cases.json",list(cases.values()))
    rooms = []
    for file in files:
        match = re.fullmatch(r"st([0-9])/r([0-9a-f]{3})\.das",file["path"],re.I)
        if not match:
            continue
        room = dict(id=match[2].lower(),file_id=file["id"],disc=file["disc_number"],path=file["path"],
                    archive_sha256=file["sha256"],analysis=file["analysis"],
                    static_source=source["rooms"].get(match[2].lower()),
                    room_table=[r for r in source["room_table"] if r["room_id"]==match[2].lower()],
                    event_candidates=[f["id"] for f in files if f["disc_number"]==file["disc_number"] and re.fullmatch(rf"evd/r{match[2]}s[0-9a-f]+\.evd",f["path"],re.I)],
                    auxiliary_candidates=[f["id"] for f in files if f["disc_number"]==file["disc_number"] and re.fullmatch(rf"st{match[1]}/r{match[2]}_.*",f["path"],re.I)])
        rooms.append(room)
    write_json(output/"rooms.json",rooms)
    coverage = Counter()
    with atomic_text(output/"resource-coverage.csv",newline="") as stream:
        writer = csv.writer(stream);writer.writerow(["disc","file","entry","tag","bytes","sha256","status","detail"])
        for file in files:
            analysis = file["analysis"]
            writer.writerow([file["disc_number"],file["path"],"",analysis["format"],file["size"],file["sha256"],analysis["status"],analysis.get("detail","")])
            for identity,entry in walk_entries(analysis.get("entries",[])):
                check = entry["validation"]
                coverage[(entry["tag"],check["status"])] += 1
                writer.writerow([file["disc_number"],file["path"],identity,entry["tag"],entry["size"],entry["sha256"],check["status"],check.get("detail","")])
    with atomic_text(output/"room-coverage.csv",newline="") as stream:
        writer=csv.writer(stream);writer.writerow(["disc","room","path","status","entries","tags","open","opaque","events","auxiliary"])
        for room in rooms:
            entries=list(walk_entries(room["analysis"].get("entries",[])))
            writer.writerow([room["disc"],room["id"],room["path"],room["analysis"]["status"],len(entries),
                             json.dumps(dict(Counter(e["tag"] for _,e in entries)),sort_keys=True),
                             sum(e["validation"]["status"]=="open" for _,e in entries),
                             sum(e["validation"]["status"]=="opaque" for _,e in entries),len(room["event_candidates"]),len(room["auxiliary_candidates"])])
    language = dict(reference="eng", evidence=["src/game/main.cpp:systemWorkInit language=1",
                   "src/game/read.cpp:OptionDataRead", "src/game/mes.cpp:MessageControl::setLanguage/getAddr/loadCommonFont",
                   "src/Sscrn: SS/___ runtime paths; src/game/event.cpp:EVD named bins"],directories={},messages=[],
                   limitations=["Structural message presence does not prove complete translation or correctness of every control code.",
                                "Shared SFD and sound banks are preserved; spoken language is not established by filenames or binary headers.",
                                "Other message-language indices are retained without assuming a language count from MDT word zero."])
    for file in files:
        parts=file["path"].split("/")
        if len(parts)>2 and parts[0].casefold() in ("ss","etc"):
            language["directories"].setdefault(parts[1],[]).append(file["id"])
        for identity,entry in walk_entries(file["analysis"].get("entries",[])):
            if entry["tag"]=="MDT":
                language["messages"].append(dict(file_id=file["id"],entry=identity,validation=entry["validation"]))
    write_json(output/"languages.json",language)
    summary = dict(files=len(files),discs=[{k:v for k,v in d.items() if k not in ("entries","system_blobs")} for d in discs],
                   unique_file_contents=len(groups),duplicate_groups=sum(len(v)>1 for v in groups.values()),
                   file_status=dict(Counter(f["analysis"]["status"] for f in files)),
                   room_instances=len(rooms),unique_rooms=len({r["id"] for r in rooms}),
                   room_status=dict(Counter(r["analysis"]["status"] for r in rooms)),
                   entry_coverage=[dict(tag=t,status=s,count=c) for (t,s),c in sorted(coverage.items())],
                   native_cases=len(cases),oracle_all=oracle_all)
    write_json(output/"summary.json",summary)
    from .reports import enrich
    enrich(output)
    summary=json.loads((output/"summary.json").read_text())
    return summary
