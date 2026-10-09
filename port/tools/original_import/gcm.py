"""Bounded counterpart of tools/extract_orig.py:Gcm. Images are always opened rb."""
import ast
import hashlib
import re
import struct
from pathlib import Path


def bounded(size, offset, length, what="span"):
    if offset < 0 or length < 0 or offset > size or length > size - offset:
        raise ValueError(f"{what} outside file: {offset:#x}+{length:#x}/{size:#x}")


def parse_fst(data, image_size):
    bounded(len(data), 0, 12, "FST root")
    flags, parent, count = struct.unpack_from(">III", data)
    if flags != 0x01000000 or parent != 0 or not 1 <= count <= len(data) // 12:
        raise ValueError("invalid FST root/count")
    names = data[12 * count:]
    stack = [("", count, 0)]
    result, seen = [], set()
    for i in range(1, count):
        while i >= stack[-1][1]:
            stack.pop()
        flag_name, a, b = struct.unpack_from(">III", data, 12 * i)
        flag, name_offset = flag_name >> 24, flag_name & 0xffffff
        if flag not in (0, 1) or name_offset >= len(names):
            raise ValueError("invalid FST type/name offset")
        end = names.find(b"\0", name_offset)
        if end < 0:
            raise ValueError("unterminated FST name")
        raw = names[name_offset:end]
        name = raw.decode("shift_jis")
        if not name or name in (".", "..") or any(c in name for c in "/\\:\0"):
            raise ValueError("unsafe FST component")
        path = stack[-1][0] + ("/" if stack[-1][0] else "") + name
        if path.casefold() in seen:
            raise ValueError("ambiguous case-insensitive FST path")
        seen.add(path.casefold())
        item = dict(index=i, parent_index=stack[-1][2], path=path,
                    name_hex=raw.hex(), kind="directory" if flag else "file")
        if flag:
            if a != stack[-1][2] or not i < b <= stack[-1][1]:
                raise ValueError("invalid FST directory ancestry/range")
            item["end_index"] = b
            stack.append((path, b, i))
        else:
            bounded(image_size, a, b, path)
            item.update(offset=a, size=b)
        result.append(item)
    return result


def original_gcm_class(root):
    # Reuse the actual class verbatim without extract_orig's CLI-only PyYAML/DOL
    # dependencies or its output directories. No shim or rewritten reader.
    path = Path(root) / "tools/extract_orig.py"
    tree = ast.parse(path.read_text(encoding="utf-8"))
    node = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "Gcm")
    namespace = {"struct": struct}
    exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec"), namespace)
    return namespace["Gcm"]


class Disc:
    def __init__(self, path, root=None):
        self.path = Path(path).resolve(strict=True)
        self.size = self.path.stat().st_size
        self.header = self.read(0, 0x440)
        if self.header[0x1c:0x20] != bytes.fromhex("c2339f3d"):
            raise ValueError("not a GameCube disc header")
        self.dol_offset, self.fst_offset, self.fst_size = struct.unpack_from(">III", self.header, 0x420)
        if not 12 <= self.fst_size <= 64 * 1024 * 1024:
            raise ValueError("FST exceeds import policy")
        self.fst = self.read(self.fst_offset, self.fst_size)
        self.entries = parse_fst(self.fst, self.size)
        self.files = [e for e in self.entries if e["kind"] == "file"]
        dol_header = self.read(self.dol_offset, 0x100)
        offsets = struct.unpack_from(">18I", dol_header, 0)
        sizes = struct.unpack_from(">18I", dol_header, 0x90)
        for off, size in zip(offsets, sizes):
            if size and off < 0x100:
                raise ValueError("DOL section overlaps header")
            bounded(self.size, self.dol_offset + off, size, "DOL section")
        self.dol_size = max(0x100, *(o + s for o, s in zip(offsets, sizes)))
        self.dol = self.read(self.dol_offset, self.dol_size)
        self.info = dict(game_id=self.header[:6].decode("ascii"),
                         disc_number=self.header[6], revision=self.header[7],
                         streaming=self.header[8], streaming_buffer=self.header[9],
                         title=self.header[0x20:0x400].split(b"\0")[0].decode("shift_jis"),
                         image_size=self.size, fst_offset=self.fst_offset, fst_size=self.fst_size,
                         fst_sha256=hashlib.sha256(self.fst).hexdigest(),
                         dol_offset=self.dol_offset, dol_size=self.dol_size,
                         dol_sha256=hashlib.sha256(self.dol).hexdigest(),
                         build_date_evidence=[dict(offset=m.start(), value=m.group().decode("ascii"))
                             for m in re.finditer(rb"Nov\s+25\s+2004\x00[0-9:]{8}", self.dol)])
        if root:
            oracle = original_gcm_class(root)(str(self.path))
            try:
                expected = {e["path"].lower(): (e["path"], e["offset"], e["size"]) for e in self.files}
                if oracle.files != expected or oracle.read("sys/main.dol") != self.dol:
                    raise ValueError("tools/extract_orig.py:Gcm disagrees")
            finally:
                oracle.f.close()
            self.info["original_gcm_reference"] = "pass"

    def read(self, offset, size):
        bounded(self.size, offset, size)
        with self.path.open("rb") as stream:
            stream.seek(offset)
            data = stream.read(size)
        if len(data) != size:
            raise ValueError("short disc read")
        return data

    def hash_image(self):
        digest = hashlib.sha256()
        with self.path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(4 * 1024 * 1024), b""):
                digest.update(chunk)
        return digest.hexdigest()
