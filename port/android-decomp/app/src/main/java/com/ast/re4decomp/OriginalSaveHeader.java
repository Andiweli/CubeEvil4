package com.ast.re4decomp;

/** Original cCard::SaveInfo and MSB-first CRCCalc, independent of JNI. */
final class OriginalSaveHeader {
  static final int FILE_BYTES=0x10000, IMAGE_BYTES=0xeafc;
  final int room, mode, gameMode, chapter, saveCount, round, seconds;
  final boolean corrupt;
  final long calendarOrder;
  OriginalSaveHeader(byte[] data) {
    if(data==null || (data.length!=IMAGE_BYTES && data.length!=FILE_BYTES))
      throw new IllegalArgumentException("Invalid save file size");
    if(word(data,0x2004)!=0x116 || word(data,0x2000)!=crc(data,0x2004,0x1fc) ||
        word(data,0xeaf8)!=crc(data,0,0xeaf8))
      throw new IllegalArgumentException("Save file is corrupted");
    corrupt=false;mode=word(data,0x2030); gameMode=data[0x203d]&255; chapter=data[0x203e]&255;
    saveCount=half(data,0x2040);round=half(data,0x2042);seconds=word(data,0x2048);room=half(data,0x2050);
    // SaveInfo::time is OSCalendarTime, month zero-based. Its six fields
    // order files exactly like CardID's newest-timestamp initial selection.
    int sec=word(data,0x2008),min=word(data,0x200c),hour=word(data,0x2010),day=word(data,0x2014),month=word(data,0x2018),year=word(data,0x201c);
    if(sec<0||sec>60||min<0||min>59||hour<0||hour>23||day<1||day>31||month<0||month>11||year<0||year>9999)
      throw new IllegalArgumentException("Invalid save calendar");
    calendarOrder=(((((long)year*12+month)*32+day)*24+hour)*60+min)*60+sec;
  }
  private OriginalSaveHeader(){room=mode=gameMode=chapter=saveCount=round=seconds=0;calendarOrder=0;corrupt=true;}
  static OriginalSaveHeader broken(){return new OriginalSaveHeader();}
  static int half(byte[] data,int offset){return (data[offset]&255)<<8 | data[offset+1]&255;}
  static int word(byte[] data,int offset){return (data[offset]&255)<<24 | (data[offset+1]&255)<<16 | (data[offset+2]&255)<<8 | data[offset+3]&255;}
  static int crc(byte[] data,int offset,int length) {
    int crc=0;
    for(int end=offset+length;offset<end;++offset){crc^=(data[offset]&255)<<24;
      for(int bit=0;bit<8;++bit)crc=(crc<<1)^((crc<0)?0x04c11db7:0);}
    return crc;
  }
}
