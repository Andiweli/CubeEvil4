"""Synthetic malformed-file/adapter tests. Contains no original disc assets."""
import struct
import sys
import unittest
import tempfile
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"port/tools"))
from original_import.gcm import bounded, parse_fst
from original_import.formats import body_index, records, event_index, etm_index, english_messages, readers, validate_rel
from original_import.pipeline import external_root, Importer
from original_import.reports import message_blocks, esl, block


def fst(name=b"room.das",offset=128,size=32):
    return struct.pack(">6I",0x01000000,0,2,0,offset,size)+name+b"\0"


def body():
    data=bytearray(96)
    struct.pack_into(">7I",data,0,3,0,0,0,64,64,96)
    data[28:40]=b"MDT\0MDT!XYZ\0"
    return data


class ImportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.modules=readers(ROOT)

    def test_fst_preserves_ids_names_offsets(self):
        entry=parse_fst(fst(),512)[0]
        self.assertEqual((entry["index"],entry["path"],entry["offset"],entry["size"]),(1,"room.das",128,32))

    def test_fst_short_and_large_count(self):
        for data in (b"",fst()[:11],struct.pack(">3I",0x01000000,0,0xffffffff)):
            with self.assertRaises(ValueError):parse_fst(data,512)

    def test_fst_file_bounds_and_name(self):
        for data in (fst(offset=500),fst(name=b"../bad"),fst(name=b".."),fst(name=b"x:y"),fst()[:-1]):
            with self.assertRaises(ValueError):parse_fst(data,512)

    def test_fst_directory_parent(self):
        good=struct.pack(">9I",0x01000000,0,3,0x01000000,0,3,2,128,32)+b"d\0f\0"
        self.assertEqual(parse_fst(good,512)[1]["path"],"d/f")
        bad=bytearray(good);struct.pack_into(">I",bad,16,2)
        with self.assertRaises(ValueError):parse_fst(bad,512)

    def test_fst_case_collision(self):
        data=struct.pack(">9I",0x01000000,0,3,0,128,32,2,160,32)+b"A\0a\0"
        with self.assertRaises(ValueError):parse_fst(data,512)

    def test_offset_arithmetic(self):
        for size,offset,length in ((32,31,2),(32,-1,1),(32,0,-1),(32,2**64,1)):
            with self.assertRaises(ValueError):bounded(size,offset,length)
        bounded(32,32,0)

    def test_body_original_getdataext_occurrence(self):
        entries,rel=body_index(body())
        self.assertEqual(rel,0)
        self.assertEqual([e["occurrence"] for e in entries],[0,1,0])
        self.assertEqual([e["size"] for e in entries],[0,32,0])

    def test_body_bad_offsets_count_endianness(self):
        for offset,value in ((0,0xffffffff),(16,0xffffffe0),(20,32),(24,128),(4,32),(8,1)):
            data=body();struct.pack_into(">I",data,offset,value)
            with self.assertRaises(ValueError):body_index(data)
        data=body();struct.pack_into("<I",data,0,3)
        with self.assertRaises(ValueError):body_index(data)

    def test_records_terminator_and_bounds(self):
        data=bytearray(0x420);struct.pack_into(">8I",data,32,0,32,0,0x400,0,0,0,0)
        struct.pack_into(">I",data,64,0xffffffff)
        self.assertEqual(len(records(data)),1)
        data[64:68]=bytes(4)
        with self.assertRaises(ValueError):records(data)
        struct.pack_into(">I",data,64,0xffffffff);struct.pack_into(">I",data,36,0xffffffff)
        with self.assertRaises(ValueError):records(data)

    def test_event_untrusted_count(self):
        data=bytearray(0x60);data[:5]=b"event";struct.pack_into(">II",data,0x48,0xffffffff,0x50)
        with self.assertRaises(ValueError):event_index(data,self.modules["archive"])

    def test_etm_untrusted_count_and_record(self):
        for count in (0xffffffff,1):
            data=bytearray(96);struct.pack_into(">I",data,0,count)
            with self.assertRaises(ValueError):etm_index(data,self.modules["archive"])

    def test_english_message_offsets(self):
        data=bytearray(32);struct.pack_into(">I",data,8,16);struct.pack_into(">II",data,20,1,12)
        self.assertEqual(english_messages(data)["message_count"],1)
        struct.pack_into(">I",data,24,0xfffffffe)
        with self.assertRaises(ValueError):english_messages(data)

    def test_rel_missing_end(self):
        data=bytearray(80);struct.pack_into(">II",data,0x28,64,8);struct.pack_into(">II",data,64,0,72)
        with self.assertRaises(ValueError):validate_rel(data)
        data[74]=203
        self.assertEqual(validate_rel(data)["exact_size"],80)

    def test_private_output_guard(self):
        with self.assertRaises(ValueError):external_root(ROOT/"port/build/assets",ROOT,[])
        with self.assertRaises(ValueError):external_root(ROOT.parent,ROOT,[ROOT.parent/"disc.gcm"])
        self.assertEqual(external_root(ROOT.parent/"private-import",ROOT,[]),ROOT.parent/"private-import")

    def test_message_does_not_escape_language_block(self):
        data=bytearray(192)
        for i in range(6):
            offset=32+i*24
            struct.pack_into(">I",data,4*(i+1),offset)
            struct.pack_into(">III",data,offset,0,1,12)
        self.assertTrue(all(b["status"]=="present" for b in message_blocks(data)["blocks"]))
        struct.pack_into(">I",data,64,40) # English message crosses the next block.
        self.assertEqual(message_blocks(data)["blocks"][1]["status"],"open")

    def test_esl_wire_width_and_record_identity(self):
        data=bytearray(8192);data[1]=0x10;struct.pack_into(">H",data,24,0x100)
        entries=esl(data)
        self.assertEqual((entries[0]["index"],entries[0]["room_id"],entries[0]["enemy_id"]),(0,"100",16))
        self.assertEqual(len(esl(data[:-32])),255)
        with self.assertRaises(ValueError):esl(data[:-1])

    def test_block_reference_bounds(self):
        data=bytearray(64);data[:4]=b"BLK\0";struct.pack_into(">H",data,4,0x100);data[7]=1
        struct.pack_into(">III",data,12,32,64,64);data[36:44]=bytes([255])*8
        self.assertEqual(len(block(data)["blocks"]),1)
        data[36]=1
        with self.assertRaises(ValueError):block(data)
        empty=bytearray(32);empty[:4]=b"BLK\0";struct.pack_into(">H",empty,4,0x100)
        struct.pack_into(">III",empty,12,20,20,0xcdcdcdcd)
        self.assertEqual(block(empty)["blocks"],[])

    def test_identical_bytes_do_not_alias_resource_types(self):
        # Isolated importer test with synthetic bytes; no native tool invocation.
        temporary_root=ROOT/"port/build/import-unit-tests"
        temporary_root.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temporary_root) as directory:
            self.assertEqual(Path(directory).resolve().parent,temporary_root.resolve())
            importer=Importer(ROOT,Path(directory),Path(sys.executable))
            digest=importer.store.put(bytes(32))
            fcv=importer.analyse(dict(path="test.fcv",sha256=digest,size=32))
            tpl=importer.analyse(dict(path="test.tpl",sha256=digest,size=32))
            self.assertEqual(fcv["entries"][0]["tag"],"FCV")
            self.assertEqual(tpl["entries"][0]["tag"],"TPL")


if __name__=="__main__":unittest.main(verbosity=2)
