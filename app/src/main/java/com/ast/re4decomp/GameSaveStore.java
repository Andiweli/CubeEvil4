package com.ast.re4decomp;

import java.io.*;
import java.util.Locale;

/** Twenty original bh4_data files, replaced only after a complete validated write. */
final class GameSaveStore {
  private final File directory;
  private static final java.util.Map<String,Integer> lastSelections=new java.util.HashMap<>();
  GameSaveStore(File directory){this.directory=directory;}
  private File file(int slot){if(slot<0||slot>=20)throw new IllegalArgumentException("Save slot");return new File(directory,String.format(Locale.ROOT,"bh4_data%02d",slot));}
  byte[] read(int slot)throws IOException {
    File source=file(slot);long size=source.length();
    if(size!=OriginalSaveHeader.IMAGE_BYTES && size!=OriginalSaveHeader.FILE_BYTES)throw new IOException("Save file is corrupted");
    for(int retry=0;;++retry) {
      byte[] data=new byte[(int)size];try(DataInputStream input=new DataInputStream(new FileInputStream(source))){input.readFully(data);if(input.read()!=-1)throw new IOException("Save file size changed");}
      try{new OriginalSaveHeader(data);return data;}
      catch(IllegalArgumentException e){if(retry==3)throw new IOException(e.getMessage(),e);}
    }
  }
  OriginalSaveHeader[] headers(){OriginalSaveHeader[]result=new OriginalSaveHeader[20];for(int slot=0;slot<20;++slot)if(file(slot).isFile())try{result[slot]=new OriginalSaveHeader(read(slot));}catch(IOException invalid){result[slot]=OriginalSaveHeader.broken();}return result;}
  boolean exists(int slot){return file(slot).isFile();}
  int initialSelection(OriginalSaveHeader[] headers) {
    synchronized(lastSelections){Integer last=lastSelections.get(directory.getAbsolutePath());if(last!=null)return last;}
    int newest=0;long time=-1;
    for(int n=0;n<20;++n)if(headers[n]!=null&&!headers[n].corrupt&&headers[n].calendarOrder>time){time=headers[n].calendarOrder;newest=n;}
    return newest;
  }
  void rememberSelection(int slot){file(slot);synchronized(lastSelections){lastSelections.put(directory.getAbsolutePath(),slot);}}
  void delete(int slot)throws IOException {File source=file(slot);if(source.exists()&&!source.delete())throw new IOException("Could not delete save file");}
  void write(int slot,byte[] data)throws IOException {
    try{new OriginalSaveHeader(data);}catch(IllegalArgumentException e){throw new IOException(e.getMessage(),e);}
    File target=file(slot);if(!directory.isDirectory()&&!directory.mkdirs())throw new IOException("Could not create save directory");
    File pending=new File(directory,target.getName()+".pending");
    try{try(FileOutputStream output=new FileOutputStream(pending)){output.write(data);output.getFD().sync();}
      if(!pending.renameTo(target))throw new IOException("Could not replace save file");
    }finally{if(pending.exists())pending.delete();}
  }
}
