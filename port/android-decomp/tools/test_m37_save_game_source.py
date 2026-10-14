#!/usr/bin/env python3
"""Compile frozen card.cpp::makeSaveData/CRCInit/CRCCalc, emulate GC scalar stores.

Usage: test_m37_save_game_source.py CXX UPSTREAM FIXTURE
The fixture is emitted by test_m37_save_game; no copyrighted asset is checked in.
"""
import pathlib
import re
import subprocess
import sys
import tempfile


def function(source, signature):
    start = source.index(signature + "\n{")
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[start:end]


def main():
    cxx, upstream, fixture = sys.argv[1:]
    upstream = pathlib.Path(upstream)
    source = (upstream / "src/game/card.cpp").read_text()
    crc_init = function(source, "void CRCInit()")
    crc_calc = function(source, "u32 CRCCalc(u8* bufPtr, u32 nByte)").replace("CRCCalc(", "SourceCRCCalc(", 1)
    make = function(source, "void cCard::makeSaveData()")
    # Sole pointer-platform adaptation: retain the original offset expression
    # without truncating the host pointer to the GameCube's32-bit address.
    make = make.replace("(u32) pSubData", "(std::uintptr_t) pSubData")
    defs = "\n".join(re.findall(r"^#define (?:SAVE_\w+|ICON_NUM) .*", source, re.M))
    room_source = (upstream / "src/game/roomdata.cpp").read_text()
    ids = []
    for stage in range(5):
        table = re.search(r"ROOM_DATA St%d_data_tbl\[\d+\] = \{(.*?)\n\};" % stage, room_source, re.S)[1]
        ids.extend(stage * 256 + i for i, value in enumerate(re.findall(r"\{(\d+),[^}]*\}", table)) if value == "1")
    code = r'''
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using u8=std::uint8_t;using u32=std::uint32_t;using s32=std::int32_t;
struct OSCalendarTime{int sec,min,hour,mday,mon,year,wday,yday,msec,usec;};
struct TEXHeader{u8*data;};struct CLUTHeader{u8*data;};
struct TEXDescriptor{TEXHeader*textureHeader;struct CLUTHeader*CLUTHeader;};
struct TEXPalette{};struct CardArc{u32 ofs[1];};
u32 CRCTable[256];std::vector<u8> input,output(0x10000),global,inventory,rooms,subscreen,merchant;
std::array<TEXHeader,3> textures;std::array<CLUTHeader,3> palettes;std::array<TEXDescriptor,3> descriptors;
TEXDescriptor* TEXGet(TEXPalette*,int i){return &descriptors.at(i);}
struct G{std::uint16_t save_cnt;u32 CardStatus=0;}g;G*pG=&g;
struct Sys{int eff_country=1;}sys;Sys*pSys=&sys;
struct Sections{u8*p8,*pC,*p10,*p14,*p18;}sections;Sections*SD=&sections;
void*pSaveData=nullptr;
void set16(u8*p,unsigned n){p[0]=n>>8;p[1]=n;}
void set32(u8*p,u32 n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
u32 get32(const u8*p){return(u32(p[0])<<24)|(u32(p[1])<<16)|(u32(p[2])<<8)|p[3];}
void scalarStore(u8*p){u32 n;memcpy(&n,p,4);set32(p,n);}
int gameTimeCalls=0;void SetGameTime(){++gameTimeCalls;}
struct Save{void save(void*,u32 mode){set16(global.data()+12,pG->save_cnt);set32(global.data()+0x33d8,mode);}}GameSave;
struct Rm{unsigned getSaveDataSize(){return rooms.size();}}RoomData;
unsigned SscrnDataSize(){return4;}unsigned MerchantDataSize(){return0x304;}
std::uint64_t OSGetTime(){return0;}
void OSTicksToCalendarTime(std::uint64_t,OSCalendarTime*c){*c={21,37,18,6,9,2026,2,278,123,456};}
void DCFlushRange(void*,u32){}
class cCard{public:u8*pSaveBuf=output.data();CardArc arc{{0}};CardArc*pSubData=&arc;int m_SaveNo=19;void makeSaveData();};
'''.replace("return4", "return 4").replace("return0x304", "return 0x304").replace("return0;", "return 0;")
    code += defs + "\n" + crc_init + "\n" + crc_calc + r'''
// The original function writes native scalars. Before each source CRC sees
// those bytes, adapt only that just-written host scalar to a GC big-endian
// store. All copied asset/save bytes remain untouched.
u32 CRCCalc(u8*p,u32 n){
  if(p==output.data()+0x2004){
    scalarStore(output.data()+0x2004);scalarStore(output.data()+0x2030);
    for(unsigned i=0;i<10;++i)scalarStore(output.data()+0x2008+i*4);
  }else if(p==output.data()&&n==0xeaf8)scalarStore(output.data()+0x2000);
  return SourceCRCCalc(p,n);
}
''' + make + r'''
int main(int argc,char**argv){
  if(argc!=2)return2;
  std::ifstream file(argv[1],std::ios::binary|std::ios::ate);auto size=file.tellg();input.resize(size);file.seekg(0);file.read((char*)input.data(),size);
  if(input.size()!=0x10000)return3;
  auto copy=[](unsigned a,unsigned n){return std::vector<u8>(input.begin()+a,input.begin()+a+n);};
  global=copy(0x2034,0x36f8);inventory=copy(0x572c,0x1204);rooms=copy(0x6930,get32(input.data()+0x6930));subscreen=copy(0xe7d0,4);merchant=copy(0xe7d4,0x304);
  g.save_cnt=(unsigned(global[12])<<8|global[13])-1;
  sections={global.data(),inventory.data(),rooms.data(),subscreen.data(),merchant.data()};
  for(unsigned i=0;i<3;++i){textures[i].data=input.data()+(i==2?0x1840:0x40);palettes[i].data=input.data()+0x1c40;descriptors[i]={&textures[i],&palettes[i]};}
  CRCInit();cCard c;c.makeSaveData();scalarStore(output.data()+0xeaf8);
  unsigned checks=0;
  for(unsigned i=0;i<0x10000;++i){++checks;if(output[i]!=input[i]){std::cerr<<"source card mismatch "<<std::hex<<i<<" "<<unsigned(output[i])<<" "<<unsigned(input[i])<<'\n';return4;}}
  if(gameTimeCalls!=1)return5;
  ++checks;
  const std::vector<unsigned> ids={ROOM_IDS};
  if(get32(rooms.data()+4)!=ids.size())return6;
  for(unsigned i=0;i<ids.size();++i){++checks;if((unsigned(rooms[16+i*0xd8])<<8|rooms[17+i*0xd8])!=ids[i])return7;}
  const u8 known[]={'1','2','3','4','5','6','7','8','9'};
  if(SourceCRCCalc(const_cast<u8*>(known),9)!=0x89a1897f)return8;
  std::cout<<"Original makeSaveData/CRC/RoomData: "<<checks+1<<" byte/source checks PASS\n";
}
'''.replace("ROOM_IDS", ",".join(str(x) for x in ids))
    code = re.sub(r"return([2-8]);", r"return \1;", code)
    with tempfile.TemporaryDirectory(prefix="m37-card-source-") as work:
        work = pathlib.Path(work)
        cpp, exe = work / "oracle.cpp", work / "oracle"
        cpp.write_text(code)
        subprocess.run([cxx, "-std=c++17", "-O2", "-fno-strict-aliasing", "-fpermissive", str(cpp), "-o", str(exe)], check=True)
        subprocess.run([str(exe), str(pathlib.Path(fixture).resolve())], check=True)


if __name__ == "__main__":
    main()
