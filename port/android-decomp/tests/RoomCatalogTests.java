import com.ast.re4decomp.RoomCatalog;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

/** Exercises the real browser catalog without requiring Android classes. */
public final class RoomCatalogTests {
  private static int checks;
  private static void check(boolean ok,String message) {
    if(!ok)throw new AssertionError(message);++checks;
  }
  private static String row(int disc,String path) {
    return disc+"\t"+path+"\t32\t"+String.join("",Collections.nCopies(64,"0"))+"\t"+
        String.join("",Collections.nCopies(64,"1"))+"\n";
  }
  private static void rejects(String rows) throws Exception {
    boolean rejected=false;
    try {new RoomCatalog(new StringReader(rows));}catch(IOException e){rejected=true;}
    check(rejected,"Untrusted or incomplete room rows are rejected");
  }
  public static void main(String[] args) throws Exception {
    if(args.length!=1)throw new IllegalArgumentException("RoomCatalogTests rooms.tsv");
    RoomCatalog catalog;
    try(Reader reader=Files.newBufferedReader(Path.of(args[0]),StandardCharsets.UTF_8)){catalog=new RoomCatalog(reader);}
    List<RoomCatalog.Entry> all=catalog.filter(RoomCatalog.ALL),campaign=catalog.filter(RoomCatalog.CAMPAIGN),extras=catalog.filter(RoomCatalog.EXTRAS);
    check(all.size()==127&&campaign.size()==108&&extras.size()==19,"Real disc rows distinguish 108 registered campaign rooms and 19 additional/unused rooms");
    Set<String> ids=new HashSet<>();String previous="";int combat=0;
    for(RoomCatalog.Entry e:all) {
      check(ids.add(e.id)&&e.id.compareTo(previous)>0,"Browser order stable with no duplicate disc entries");previous=e.id;
      check(e.label().contains(e.id),"Every visible label identifies its actual room");
      if(e.combatTest()){++combat;check(e.id.equals("r400")&&!e.campaign(),"Combat label limited to the implemented Mercenaries test");}
      if(e.id.equals("r100"))check(e.campaign()&&e.disc==1&&e.label().startsWith("★"),"Campaign start is highlighted and prefers available Disc 1");
      if(e.id.equals("r300"))check(e.area().equals("Island")&&e.campaign(),"Stage 3 is the campaign island");
    }
    check(combat==1,"No fictitious original-enemy mode offered for unsupported rooms");
    for(RoomCatalog.Entry e:campaign)check(e.campaign()&&e.stage<=3,"Campaign filter preserves source stage membership");
    for(RoomCatalog.Entry e:extras)check(!e.campaign()&&(e.stage==4||e.unused()),"Additional modes and unregistered room data have separate selection");
    int unavailable=0,unused=0,walkable=0;
    for(RoomCatalog.Entry e:all){if(!e.explorable())++unavailable;if(e.walkable())++walkable;if(e.id.equals("r30f"))check(e.campaign()&&e.explorable()&&!e.walkable(),"Original bulldozer scenario is campaign geometry but not a ready walking level");if(e.unused()){++unused;check(!e.label().startsWith("★"),"Unregistered island assets are not advertised as campaign levels");}}
    check(unavailable==4&&unused==5,"Four legacy model archives remain explicitly disabled, five original unregistered rooms marked");
    check(walkable==122,"Catalog does not advertise a false player floor in the dynamic vehicle scenario");
    RoomCatalog duplicates=new RoomCatalog(new StringReader(row(2,"St1/r100.das")+row(1,"St1/r100.das")));
    check(duplicates.filter(RoomCatalog.ALL).size()==1&&duplicates.filter(RoomCatalog.ALL).get(0).disc==1,"Disc preference independent of input ordering");
    rejects("");rejects(row(1,"../St1/r100.das"));rejects(row(1,"St2/r100.das"));
    rejects(row(1,"St1/r100.DAS"));rejects(row(3,"St1/r100.das"));rejects("1\tSt1/r100.das\t32\tbad\tbad\n");
    System.out.println("M31 room catalog PASS: "+checks);
  }
}
