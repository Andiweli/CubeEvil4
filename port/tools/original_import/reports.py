"""Additional source-derived dependency and language reports, without decoding again."""
import csv
import hashlib
import json
import struct
from collections import Counter,defaultdict
from pathlib import Path
from .gcm import bounded
from .formats import u32
from .pipeline import write_json, walk_entries, atomic_text
from .source_refs import references


def message_blocks(data):
    # These six indices come from MessageControl::setLanguage, not guesses about
    # word zero. The actual directory spellings come from OptionDataRead.
    names={0:"jpn",1:"eng",2:"ger",3:"fra",4:"esp",5:"ita"}
    result=[]
    offsets=[u32(data,4*(i+1)) for i in names]
    for i,name in names.items():
        try:
            off=offsets[i]
            end=min([p for p in offsets if p>off]+[len(data)])
            bounded(len(data),off,8,"message block")
            count=u32(data,off+4)
            bounded(end,off+8,count*4,"message table")
            refs=[u32(data,off+8+j*4) for j in range(count)]
            payloads=[]
            for ref in refs:
                bounded(end,off+ref,2,"message payload")
                if ref<8+count*4 or ref%2:raise ValueError("invalid message offset")
                stop=min([r for r in refs if r>ref]+[end-off])
                payloads.append(hashlib.sha256(data[off+ref:off+stop]).hexdigest())
            result.append(dict(language=name,index=i,offset=off,extent=end-off,count=count,
                               distinct_payloads=len(set(payloads)),payload_sha256=payloads,
                               status="present" if count else "empty",
                               detail="bounded payload spans; no inferred text terminator or control-code conversion"))
        except (ValueError,IndexError,struct.error) as error:
            result.append(dict(language=name,index=i,status="open",detail=str(error)))
    return dict(header_word=u32(data,0),blocks=result,
                identical_language_payloads=[[a["language"],b["language"]]
                    for n,a in enumerate(result) for b in result[n+1:]
                    if a.get("payload_sha256") and a.get("payload_sha256")==b.get("payload_sha256")])


def esl(data):
    # stage.cpp reads the actual file length into a 256-record-capacity buffer.
    # The supplied originals have 255 records. Never fabricate record 256.
    if not data or len(data)>0x2000 or len(data)%32:raise ValueError("ESL exceeds capacity or has a partial 0x20-byte record")
    result=[]
    for i in range(len(data)//32):
        p=i*32
        result.append(dict(index=i,be_flag=data[p],enemy_id=data[p+1],type=data[p+2],set=data[p+3],
                           flags=u32(data,p+4),hp=struct.unpack_from(">H",data,p+8)[0],
                           character=data[p+11],position_raw=list(struct.unpack_from(">3h",data,p+12)),
                           rotation_raw=list(struct.unpack_from(">3h",data,p+18)),
                           room_id=f"{struct.unpack_from('>H',data,p+24)[0]:03x}",
                           guard_radius_raw=struct.unpack_from(">h",data,p+26)[0],
                           padding_hex=data[p+28:p+32].hex()))
    return result


def block(data):
    bounded(len(data),0,24,"BLK header")
    if data[:4]!=b"BLK\0" or struct.unpack_from(">H",data,4)[0]!=0x100:
        raise ValueError("unknown BLK tag/version")
    count=data[7];areas,connects=struct.unpack_from(">HH",data,8)
    link,area,connect=struct.unpack_from(">III",data,12)
    for off,n,stride in ((link,count,12),(area,areas,56),(connect,connects,20)):
        # Empty original BLKs have unused ofsConnect=0xCDCDCDCD.
        # roomInit returns for nBlock=0 without dereferencing that pointer.
        if n:bounded(len(data),off,n*stride,"BLK table")
    refs=[]
    for i in range(count):
        neighbours=list(struct.unpack_from(">8b",data,link+i*12+4))
        if any(n != -1 and not 0<=n<count for n in neighbours):raise ValueError("invalid BLK neighbour")
        refs.append(dict(index=i,flags=data[link+i*12],neighbours=neighbours))
    connected=[]
    for i in range(connects):
        p=connect+i*20; mram=list(struct.unpack_from(">8b",data,p+4));aram=list(struct.unpack_from(">8b",data,p+12))
        if data[p]&1 and (data[p+1]>=count or any(n!=-1 and not 0<=n<count for n in mram+aram)):
            raise ValueError("invalid active BLK connect reference")
        connected.append(dict(index=i,flags=data[p],block=data[p+1],mram=mram,aram=aram))
    return dict(status="indexed",blocks=refs,area_count=areas,connects=connected,
                scope="streaming dependencies; AREA_HIT_DATA geometry not interpreted")


def enrich(output):
    output=Path(output)
    manifest=json.loads((output/"manifest.json").read_text())
    rooms=json.loads((output/"rooms.json").read_text())
    files=manifest["files"];by_id={f["id"]:f for f in files}
    source=references(Path(__file__).resolve().parents[3],files)
    write_json(output/"source-references.json",source)
    messages,enemy_lists,block_dependencies=[],[],[]
    cache={}
    for file in files:
        analysis=file["analysis"]
        digest=analysis.get("body_sha256",file["sha256"])
        if not analysis.get("entries") and not file["path"].lower().endswith(".esl"):continue
        if digest not in cache:
            if len(cache)>=8:cache.clear()
            cache[digest]=(output/"blobs"/digest).read_bytes()
            if hashlib.sha256(cache[digest]).hexdigest()!=digest:raise ValueError("report input SHA-256 mismatch")
        data=cache[digest]
        if file["path"].lower().endswith(".esl"):
            try:
                entries=esl(data)
                analysis.update(status="indexed",format="ESL",enemy_records=entries)
                enemy_lists.append(dict(file_id=file["id"],records=entries))
            except ValueError as error:analysis.update(status="open",detail=str(error))
        def visit(entries,buffer,prefix=""):
            for entry in entries:
                off,size=entry["offset"],entry["size"]
                bounded(len(buffer),off,size,"report resource")
                blob=buffer[off:off+size];identity=prefix+str(entry["index"])
                if not prefix and analysis.get("body_sha256"):
                    entry["table_word_index"]=entry["index"]+4
                if entry["tag"]=="MDT" and blob:
                    try:
                        info=message_blocks(blob)
                        messages.append(dict(file_id=file["id"],entry=identity,**info))
                        english=next(b for b in info["blocks"] if b["index"]==1)
                        entry["validation"].update(languages=info,status="indexed" if english["status"]!="open" else "open")
                    except ValueError as error:
                        entry["validation"]=dict(status="open",detail=str(error))
                if entry["tag"]=="BLK" and blob:
                    try:
                        entry["validation"]=block(blob)
                        for record in entry["validation"]["blocks"]:
                            if record["flags"]&1:
                                room_path=Path(file["path"])
                                expected=f"{room_path.parent.as_posix()}/{room_path.stem}_{record['index']:02x}.dat"
                                found=[f["id"] for f in files if f["disc_number"]==file["disc_number"] and f["path"].casefold()==expected.casefold()]
                                block_dependencies.append(dict(file_id=file["id"],entry=identity,block=record["index"],path=expected,resolved=found,
                                                                evidence="block.cpp:getBlockWork st%x/r%03x_%02x.dat"))
                    except ValueError as error:entry["validation"]=dict(status="open",detail=str(error))
                visit(entry.get("children",[]),blob,identity+"/")
        visit(analysis.get("entries",[]),data)
    for room in rooms:
        room["analysis"]=by_id[room["file_id"]]["analysis"]
        room["static_source"]=source["rooms"].get(room["id"])
        room["static_literals"]=[source["literals"][i] for i in room["static_source"]["references"]] if room["static_source"] else []
        room["original_room_lookups"]=[dict(lookup=l,present=any(e["tag"][:3]==l["tag"] and e.get("occurrence")==l["occurrence"]
                                                              for e in room["analysis"].get("entries",[])))
                                        for l in source["archive_lookups"] if "pG->pRoom" in l["buffer"]]
        room["block_dependencies"]=[b for b in block_dependencies if b["file_id"]==room["file_id"]]
        room["enemy_list_candidates"]=[dict(file_id=l["file_id"],indices=[e["index"] for e in l["records"] if e["room_id"]==room["id"] and e["enemy_id"]])
                                       for l in enemy_lists if by_id[l["file_id"]]["disc_number"]==room["disc"] and any(e["room_id"]==room["id"] and e["enemy_id"] for e in l["records"])]
    language=json.loads((output/"languages.json").read_text());language["message_blocks"]=messages
    counts=Counter((b["language"],b["status"]) for m in messages for b in m["blocks"])
    language["block_coverage"]=[dict(language=l,status=s,count=c) for (l,s),c in sorted(counts.items())]
    language["evidence"]=sorted(set(language["evidence"]+["src/game/option.cpp:setLangExt3","src/game/sscrn.cpp:sscrnDataFilename"]))
    language["limitations"]=sorted(set(language["limitations"]+["Same-sized, repeated or byte-identical payloads are placeholder candidates; they do not prove missing translations."]))
    write_json(output/"languages.json",language)
    write_json(output/"manifest.json",manifest)
    write_json(output/"rooms.json",rooms)
    write_json(output/"data-dependencies.json",dict(enemy_lists=enemy_lists,blocks=block_dependencies,
                unresolved_blocks=[b for b in block_dependencies if not b["resolved"]],
                limitations=["Enemy list selection depends on stage.cpp:checkEmListNo and story/mode flags; candidates do not invent a default selection.",
                             "No reconstruction of gameplay positions, event logic, or room modules."]))
    coverage=Counter()
    with atomic_text(output/"resource-coverage.csv",newline="") as stream:
        writer=csv.writer(stream);writer.writerow(["disc","file","entry","tag","bytes","sha256","status","detail"])
        for file in files:
            a=file["analysis"];writer.writerow([file["disc_number"],file["path"],"",a["format"],file["size"],file["sha256"],a["status"],a.get("detail","")])
            for identity,e in walk_entries(a.get("entries",[])):
                v=e["validation"];coverage[(e["tag"],v["status"])]+=1
                writer.writerow([file["disc_number"],file["path"],identity,e["tag"],e["size"],e["sha256"],v["status"],v.get("detail","")])
    with atomic_text(output/"room-coverage.csv",newline="") as stream:
        writer=csv.writer(stream);writer.writerow(["disc","room","status","entries","tags","open","opaque","events","blocks","missing_blocks","enemy_lists"])
        for room in rooms:
            entries=list(walk_entries(room["analysis"].get("entries",[])))
            writer.writerow([room["disc"],room["id"],room["analysis"]["status"],len(entries),json.dumps(dict(Counter(e["tag"] for _,e in entries)),sort_keys=True),
                             sum(e["validation"]["status"]=="open" for _,e in entries),sum(e["validation"]["status"]=="opaque" for _,e in entries),
                             len(room["event_candidates"]),len(room["block_dependencies"]),sum(not b["resolved"] for b in room["block_dependencies"]),len(room["enemy_list_candidates"])])
    summary=json.loads((output/"summary.json").read_text());summary["entry_coverage"]=[dict(tag=t,status=s,count=c) for (t,s),c in sorted(coverage.items())]
    summary["file_status"]=dict(Counter(f["analysis"]["status"] for f in files))
    summary["language_block_coverage"]=language["block_coverage"];summary["missing_block_dependencies"]=sum(not b["resolved"] for b in block_dependencies)
    summary["esl_file_instances"]=len(enemy_lists)
    summary["esl_record_counts"]=dict(Counter(len(l["records"]) for l in enemy_lists))
    write_json(output/"summary.json",summary)
    with atomic_text(output/"fst-inventory.csv",newline="") as stream:
        writer=csv.writer(stream);writer.writerow(["disc","fst_index","parent_index","kind","path","offset","bytes","sha256","identical_file_ids"])
        for disc in manifest["discs"]:
            for item in disc["entries"]:
                file=by_id.get(f"disc{disc['disc_number']}:fst{item['index']}")
                digest=file["sha256"] if file else ""
                writer.writerow([disc["disc_number"],item["index"],item["parent_index"],item["kind"],item["path"],item.get("offset",""),item.get("size",""),digest,
                                 json.dumps(manifest["duplicates"].get(digest,[]))])
    room_dir=output/"rooms"
    room_dir.mkdir(exist_ok=True)
    if room_dir.resolve().parent!=output.resolve():raise ValueError("room report directory escapes private data root")
    for room in rooms:write_json(room_dir/f"r{room['id']}-disc{room['disc']}.json",room)
    lines=["# G4BE08: automatisch erzeugter Importbericht", "",
           f"{len(files)} FST-Dateien, {summary['unique_file_contents']} unterschiedliche Dateiinhalte; {len(rooms)} Raumarchive / {summary['unique_rooms']} Raum-IDs.","",
           "## Disc-Nachweise","","| Disc-ID | Dateien | Image-SHA-256 | DOL-SHA-256 |","|---|---:|---|---|"]
    for disc in manifest["discs"]:
        lines.append(f"| {disc['disc_number']} | {sum(f['disc_number']==disc['disc_number'] for f in files)} | {disc['image_sha256']} | {disc['dol_sha256']} |")
    lines += ["", "Image-Hashes vor und nach dem Import sind gleich. Kennung, FST und Datumsmarker sind aus den tatsächlichen Images gelesen; eine externe authentifizierte Referenzprüfsumme liegt nicht vor.",
              "", "## Sprachblöcke", "", "| Sprache | vorhanden | leer | offen |", "|---|---:|---:|---:|"]
    for lang in ("eng","jpn","ger","fra","esp","ita"):
        lines.append(f"| {lang} | {counts[(lang,'present')]} | {counts[(lang,'empty')]} | {counts[(lang,'open')]} |")
    lines += ["", "Sprachblockzahlen zählen Disc-/Ressourceninstanzen, einschließlich gemeinsamer Duplikate. Vorhandene Offsets beweisen keine vollständige Übersetzung. SS/Eng und SS/Cmn werden aus Dateisystem und Original-Ladefunktionen zugeordnet; Item bezeichnet eine Ressourcengruppe, keine Sprache.",
              "", "## Ressourcenabdeckung", "", "| Tag | Status | Anzahl |", "|---|---|---:|"]
    lines += [f"| {tag or '(leer)'} | {status} | {count} |" for (tag,status),count in sorted(coverage.items())]
    lines += ["", "indexed = Container/Referenzen geprüft; roundtrip = vorhandener Parser serialisiert bytegleich; opaque = original erhalten ohne semantische Interpretation; open = Parserlücke, Variante oder beschädigter/platzhalterartiger Originaleintrag. Keine dieser Angaben bedeutet bereits ausführbares Gameplay.",
              "", "## Raumabdeckung", "", "| Disc | Raum | Status | Top-Level-Einträge | Ereigniskandidaten | Blockdateien |", "|---:|---|---|---:|---:|---:|"]
    for room in rooms:
        lines.append(f"| {room['disc']} | r{room['id']} | {room['analysis']['status']} | {len(room['analysis'].get('entries',[]))} | {len(room['event_candidates'])} | {len(room['block_dependencies'])} |")
    lines += ["", "Vollständige FST: fst-inventory.csv / manifest.json. Alle Ressourcen: resource-coverage.csv. Alle Räume: room-coverage.csv / rooms/*.json. Sprachverwendung: languages.json. Nachgewiesene Referenzen: source-references.json und data-dependencies.json.",
              "", "Native Prüfresultate stehen separat in host-x86-import-tests.json, host-x64-import-tests.json und android-import-tests.json; sie werden nur durch tatsächlich ausgeführte Tests erzeugt.",
              "", "Offen bleiben insbesondere Ereignis-Paketinterpretation, weitere Kollisions-/Kamera-/Licht-/Effektformate, Audio-/Videowiedergabe, native REL-Registrierung und die vollständige cDvd/MRAM/ARAM/Frame-Anbindung.",""]
    with atomic_text(output/"inventory.md") as stream:stream.write("\n".join(lines))
    print(json.dumps({k:v for k,v in summary.items() if k not in ("discs","entry_coverage")},indent=2),flush=True)
