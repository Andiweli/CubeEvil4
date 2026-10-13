"""Validate spans before calling the repository's existing, unchanged parsers."""
import importlib
import struct
import sys
from collections import Counter
from pathlib import Path
from .gcm import bounded


def readers(root):
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(Path(root) / "tools/motion"))
    sys.path.insert(0, str(Path(root) / "tools"))
    return {name: importlib.import_module(name) for name in ("drs", "archive", "yz2", "fcv", "modelbin", "meshbin")}


def u32(data, offset):
    bounded(len(data), offset, 4)
    return struct.unpack_from(">I", data, offset)[0]


def records(data, base=0):
    bounded(len(data), base, 0x400, "container table")
    result = []
    for p in range(base + 0x20, base + 0x400, 32):
        r = struct.unpack_from(">8I", data, p)
        if r[0] == 0xffffffff:
            if any(r[1:]) or any(data[p + 32:base + 0x400]):
                raise ValueError("invalid record table padding/terminator")
            return result
        bounded(len(data) - base, r[3], r[1], "container record")
        result.append(r)
    raise ValueError("missing record terminator")


def body_index(data):
    bounded(len(data), 0, 16, "body header")
    count, rel, pad0, pad1 = struct.unpack_from(">4I", data)
    if not 1 <= count <= (len(data) - 16) // 8 or pad0 or pad1:
        raise ValueError("invalid/unknown body header")
    header_end = 16 + count * 8
    first = (header_end + 31) & ~31
    offsets = struct.unpack_from(f">{count}I", data, 16)
    if offsets[0] != first or first > len(data) or any(data[header_end:first]):
        raise ValueError("invalid body first offset/padding")
    end = rel or len(data)
    if end > len(data) or (rel and rel % 32):
        raise ValueError("invalid REL offset")
    result, occurrences = [], Counter()
    for i, start in enumerate(offsets):
        stop = offsets[i + 1] if i + 1 < count else end
        if start < first or start % 32 or stop < start or stop > end:
            raise ValueError("invalid body entry range")
        tag = bytes(data[16 + count * 4 + i * 4:20 + count * 4 + i * 4])
        result.append(dict(index=i, table_word_index=i+4, offset=start, size=stop-start, tag_hex=tag.hex(),
                           tag=tag.rstrip(b"\0").decode("ascii", "backslashreplace"),
                           occurrence=occurrences[tag[:3]]))
        occurrences[tag[:3]] += 1
    return result, rel


def validate_rel(data):
    bounded(len(data), 0, 0x40, "REL header")
    section_count, section_offset = u32(data, 12), u32(data, 16)
    bounded(len(data), section_offset, section_count * 8, "REL sections")
    for i in range(section_count):
        off, size = struct.unpack_from(">II", data, section_offset + 8 * i)
        if off & ~1:
            bounded(len(data), off & ~1, size, "REL section")
    imp_offset, imp_size = u32(data, 0x28), u32(data, 0x2c)
    if imp_size % 8:
        raise ValueError("misaligned REL import table")
    bounded(len(data), imp_offset, imp_size, "REL imports")
    imports, end = [], 0
    for p in range(imp_offset, imp_offset + imp_size, 8):
        module, cursor = struct.unpack_from(">II", data, p)
        imports.append(module)
        while True:
            bounded(len(data), cursor, 8, "REL relocation")
            kind = data[cursor + 2]
            cursor += 8
            if kind == 203:  # tools/relfile.py:R_DOLPHIN_END
                break
        end = max(end, cursor)
    return dict(module_id=u32(data, 0), imports=imports, exact_size=end, sections=section_count)


def event_index(data, reference):
    if data[:5] != b"event":
        raise ValueError("unknown EVD header")
    count = u32(data, 0x48)
    table = u32(data, 0x4c)
    bounded(len(data), table, count * 64, "EVD bin table")
    result, previous = [], table + count * 64
    for i in range(count):
        p = table + i * 64
        name = data[p:p + 48].split(b"\0")[0].decode("ascii")
        off, size = struct.unpack_from(">II", data, p + 48)
        bounded(len(data), off, size, "EVD bin")
        if off < previous or any(data[p + 56:p + 64]):
            raise ValueError("invalid EVD bin ordering/padding")
        previous = off + size
        result.append(dict(index=i, offset=off, size=size, name=name, tag=reference.ext_tag(name)))
    original = reference.parse_event(data)
    if [(e["name"], data[e["offset"]:e["offset"]+e["size"]]) for e in result] != original:
        raise ValueError("EVD parser disagreement")
    return result


def etm_index(data, reference):
    count = u32(data, 0)
    if count > (len(data) - 32) // 64:
        raise ValueError("ETM count outside buffer")
    result, offset = [], 32
    for i in range(count):
        bounded(len(data), offset, 64, "ETM header")
        size = u32(data, offset)
        if size < 64 or size % 32:
            raise ValueError("invalid ETM record size")
        bounded(len(data), offset, size, "ETM record")
        name = data[offset+32:offset+64].split(b"\0")[0].decode("ascii")
        result.append(dict(index=i, name=name, offset=offset+64, size=size-64, tag=reference.ext_tag(name)))
        offset += size
    if offset != len(data):
        raise ValueError("unaccounted ETM data")
    if [(e["name"], data[e["offset"]:e["offset"]+e["size"]]) for e in result] != reference.parse_etm(data):
        raise ValueError("ETM parser disagreement")
    return result


def english_messages(data):
    # mes.cpp:getAddr explicitly uses tbl[language+1], English language=1.
    # Do not infer the number of other languages from unknown header word 0.
    offset = u32(data, 8)
    bounded(len(data), offset, 8, "English message block")
    count = u32(data, offset + 4)
    bounded(len(data), offset + 8, count * 4, "English message offsets")
    refs = [u32(data, offset + 8 + i * 4) for i in range(count)]
    for ref in refs:
        bounded(len(data), offset + ref, 2, "English u16 message")
        if ref < 8 + count * 4 or ref % 2:
            raise ValueError("invalid English message reference")
    return dict(language_index=1, offset=offset, message_count=count,
                distinct_message_offsets=len(set(refs)),
                status="indexed" if count else "empty", scope="offsets, not text/control-code interpretation")


def semantic(tag, data, modules):
    try:
        if not data:
            return dict(status="empty")
        if tag == "MDT":
            return dict(status="indexed", english=english_messages(data))
        if tag == "FCV":
            motion = modules["fcv"].parse(data)
            exact = modules["fcv"].serialise(motion) == data
            return dict(status="roundtrip" if exact else "open", frames=motion.max_frame,
                        joints=len(motion.joints), detail=None if exact else "FCV serialisation differs")
        if tag == "SEQ":
            seq = modules["fcv"].parse_seq(data)
            exact = modules["fcv"].serialise_seq(seq) == data
            return dict(status="roundtrip" if exact else "open", detail=None if exact else "not a MotionSeqKey table")
        if tag == "BIN":
            bounded(len(data), 0, 0x40, "model header")
            bounded(len(data), u32(data, 0), data[0x19]*16, "model parts")
            if u32(data,0x3c)==0x20030818:
                blend=u32(data,0x40)
                if blend:
                    bounded(len(data),blend+4,u32(data,blend)*8,"model blend table")
            model = modules["modelbin"].parse(data)
            mesh = modules["meshbin"].parse(data)
            exact = modules["meshbin"].serialise(mesh) == data
            return dict(status="roundtrip" if exact else "open", version=f"{model.version:08x}",
                        parts=model.n_parts, detail=None if exact else "BIN serialisation differs")
        if tag == "TPL":
            return tpl_index(data)
    except (ValueError, AssertionError, IndexError, struct.error, KeyError, OverflowError) as error:
        return dict(status="open", detail=str(error) or type(error).__name__)
    return dict(status="opaque", detail="preserved; semantic parser not yet integrated")


def tpl_index(data):
    # include/tpl.h and tools/motion/gxtex.py:parse_tpl/base_size, no pixel conversion.
    if u32(data, 0) != 0x0020af30:
        raise ValueError("unknown TPL version (may be a placeholder)")
    count, table = u32(data, 4), u32(data, 8)
    bounded(len(data), table, count * 8, "TPL descriptors")
    textures = []
    for i in range(count):
        header, clut = struct.unpack_from(">II", data, table + i * 8)
        bounded(len(data), header, 36, "TPL texture header")
        h, w, fmt, off = struct.unpack_from(">HHII", data, header)
        if not w or not h or clut or data[header+35]:
            raise ValueError("unsupported TPL CLUT/relocation/dimensions")
        if fmt in (0, 14):
            size = ((w+7)//8) * ((h+7)//8) * 32
        elif fmt == 3:
            size = ((w+3)//4) * ((h+3)//4) * 32
        else:
            raise ValueError(f"TPL format {fmt} not supported by original gxtex reader")
        bounded(len(data), off, size, "TPL base pixels")
        textures.append(dict(index=i, width=w, height=h, format=fmt, offset=off, base_bytes=size,
                             min_lod=data[header+33], max_lod=data[header+34]))
    return dict(status="indexed", textures=textures, scope="headers/base pixels; mip-chain validation remains open")
