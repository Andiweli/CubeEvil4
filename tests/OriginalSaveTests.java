package com.ast.re4decomp;
import java.io.*;
import java.nio.file.*;
import java.util.*;
public final class OriginalSaveTests {
  private static int checks;
  private static void check(boolean value,String message){++checks;if(!value)throw new AssertionError(message);}
  private static void word(byte[] data,int at,int value){for(int n=0;n<4;++n)data[at+n]=(byte)(value>>>(24-8*n));}
  private static OriginalSaveHeader year(byte[] original,int year){byte[]data=original.clone();word(data,0x201c,year);word(data,0x2000,OriginalSaveHeader.crc(data,0x2004,0x1fc));word(data,0xeaf8,OriginalSaveHeader.crc(data,0,0xeaf8));return new OriginalSaveHeader(data);}
  public static void main(String[]args)throws Exception {
    if(args.length!=1)throw new IllegalArgumentException("Original native card fixture required");
    byte[]fixture=Files.readAllBytes(Paths.get(args[0]));OriginalSaveHeader header=new OriginalSaveHeader(fixture);
    check(header.room==0x100&&header.saveCount==9&&header.seconds==3723&&header.gameMode==5,"Independent Java parser reads frozen original card.cpp fixture");
    for(int offset:new int[]{0,0x2004,0x2040,0x21ff,0x572c,0xeaf7,0xeaf8}) {
      byte[]broken=fixture.clone();broken[offset]^=1;boolean rejected=false;try{new OriginalSaveHeader(broken);}catch(IllegalArgumentException expected){rejected=true;}check(rejected,"Whole-image/header corruption rejected at "+offset);
    }
    Path directory=Files.createTempDirectory("re4-card-test-");try {
      GameSaveStore store=new GameSaveStore(directory.toFile());check(Arrays.stream(store.headers()).allMatch(Objects::isNull),"No invented initial saves");
      OriginalSaveHeader[] dates=new OriginalSaveHeader[20];dates[2]=year(fixture,2025);dates[19]=year(fixture,2027);dates[0]=OriginalSaveHeader.broken();
      check(store.initialSelection(dates)==19,"New card selects newest original OSCalendarTime among valid files");
      store.rememberSelection(2);check(new GameSaveStore(directory.toFile()).initialSelection(dates)==2,"Same virtual card keeps last cursor as original saved serial selection");
      for(int slot=0;slot<20;++slot){store.write(slot,fixture);check(Arrays.equals(fixture,store.read(slot)),"Exact source image retained in slot "+slot);check(Files.exists(directory.resolve(String.format(Locale.ROOT,"bh4_data%02d",slot))),"Original filename "+slot);}
      byte[]invalid=fixture.clone();invalid[0x5000]^=1;boolean rejected=false;try{store.write(3,invalid);}catch(IOException expected){rejected=true;}check(rejected&&Arrays.equals(fixture,store.read(3)),"Invalid replacement leaves previous valid file intact");
      Path blocked=directory.resolve("bh4_data03.pending");Files.createDirectory(blocked);rejected=false;
      try{store.write(3,fixture);}catch(IOException expected){rejected=true;}
      check(rejected&&Arrays.equals(fixture,store.read(3)),"Actual filesystem write failure preserves existing valid save");
      Files.write(directory.resolve("bh4_data03"),invalid);check(store.headers()[3].corrupt,"Corrupt file remains visibly distinct from empty slot");
      boolean badSlot=false;try{store.read(20);}catch(IllegalArgumentException expected){badSlot=true;}check(badSlot,"Exactly twenty slots");
      check(Files.list(directory).noneMatch(p->p.getFileName().toString().endsWith(".pending")),"No unfinished write after successful atomic save");
    }finally {try(java.util.stream.Stream<Path>files=Files.walk(directory)){files.sorted(Comparator.reverseOrder()).forEach(p->{try{Files.delete(p);}catch(IOException e){throw new UncheckedIOException(e);}});}}
    System.out.println("Original save Java checks passed: "+checks);
  }
}
