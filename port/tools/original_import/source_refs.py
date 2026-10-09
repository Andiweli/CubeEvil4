"""Evidence-backed static references; dynamic expressions remain explicitly unresolved."""
import hashlib
import re
from pathlib import Path


def references(root, files):
    root = Path(root)
    known = {}
    for file in files:
        known.setdefault(file["path"].casefold(), []).append(file["id"])
    references, rooms, sources, archive_lookups = [], {}, {}, []
    literal = re.compile(r'"([^"\n]*\.(?:das|drs|dat|evd|fcv|eff|bin|tpl|fnt|sfd|esl|rel|hed|sbb))"', re.I)
    for path in sorted((root / "src").rglob("*.cpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        relative = path.relative_to(root).as_posix()
        room = re.fullmatch(r"r([0-9a-f]{3})\.cpp", path.name, re.I)
        room_id = room[1].lower() if room else None
        if room_id:
            rooms[room_id] = dict(source=relative, references=[])
        for match in literal.finditer(text):
            name = match[1]
            lookup, rule = name.casefold(), "exact path"
            if lookup.startswith("event/evd/"):
                lookup, rule = lookup[6:], "event.cpp:EvtRead strstr(evd/)"
            if lookup.startswith("ss/___/"):
                lookup, rule = lookup.replace("/___/","/eng/",1), "option.cpp:setLangExt3 English=1"
            line_start=text.rfind("\n",0,match.start())+1
            if "sscrnDataFilename" in text[line_start:match.start()] and "/" not in lookup:
                lookup,rule="ss/eng/"+lookup,"sscrn.cpp:sscrnSetLanguage/sscrnDataFilename English=1"
            hits = known.get(lookup, [])
            # A .das -> .drs is NOT a blanket alias. Only note a candidate:
            # read.cpp:SET_DRS_NAME is conditional on a nonzero module DLL.
            candidate = known.get(lookup[:-4] + ".drs", []) if lookup.endswith(".das") else []
            item = dict(source=relative, line=text.count("\n", 0, match.start()) + 1,
                        literal=name, resolved=hits, rule=rule,
                        conditional_drs_candidate=candidate,
                        state="resolved" if hits else "dynamic" if "%" in name or "___" in name else "unresolved")
            references.append(item)
            if room_id:
                rooms[room_id]["references"].append(len(references)-1)
        for match in re.finditer(r'GetDataExt\s*\(([^,\n]+),\s*"([A-Za-z0-9]{3})",\s*(0x[0-9a-fA-F]+|\d+)\s*\)',text):
            archive_lookups.append(dict(source=relative,line=text.count("\n",0,match.start())+1,
                                       buffer=match[1].strip(),tag=match[2],occurrence=int(match[3],0),
                                       scope="static lookup, possibly nullable; runtime reachability unresolved"))
        if room_id or relative in ("src/game/read.cpp", "src/game/mes.cpp", "src/game/main.cpp", "src/game/event.cpp"):
            sources[relative] = hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()
    dvd = (root / "src/game/dvd.cpp").read_text(encoding="utf-8")
    table = re.search(r"FileTblEntry FileTbl\[\] = \{(.*?)\n\};", dvd, re.S)[1]
    file_table = [dict(index=i, original_name=m[1], resolved=known.get(m[1].casefold(), []),
                       conditional_drs_candidate=known.get(m[1][:-4].casefold()+".drs", []) if m[1].endswith(".das") else [])
                  for i, m in enumerate(re.finditer(r'\{"([^"\n]+)",\s*0\}', table))]
    room_data = (root / "src/game/roomdata.cpp").read_text(encoding="utf-8")
    room_table = []
    for table in re.finditer(r"ROOM_DATA St(\d)_data_tbl\[\d+\] = \{(.*?)\n\};", room_data, re.S):
        for i, entry in enumerate(re.finditer(r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\}", table[2])):
            values = [int(v) for v in entry.groups()]
            rel_no = values[2]
            room_table.append(dict(room_id=f"{int(table[1])*256+i:03x}", raw_fields=values,
                                   module_file_index=rel_no,
                                   module_name=file_table[rel_no]["original_name"] if rel_no < len(file_table) else None))
    return dict(literals=references, rooms=rooms, file_table=file_table, room_table=room_table,
                archive_lookups=archive_lookups,
                source_sha256=sources,
                limitations=["Static string occurrences include debug and unused code; they are not a complete executed dependency graph.",
                              "Computed indices, module calls and runtime event dependencies require Phase 3b resolution.",
                              "Conditional .das/.drs candidates do not automatically rewrite original FileTbl entries."])
