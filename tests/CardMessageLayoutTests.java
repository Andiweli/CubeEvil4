package com.ast.re4decomp;

import java.nio.file.*;
import java.util.*;
import java.util.regex.*;

/** Actual disc font bearings and MES5015/5017/5019 confirmation geometry. */
public final class CardMessageLayoutTests {
  private static int checks;
  private static void check(boolean value,String text){++checks;if(!value)throw new AssertionError(text);}
  private static int[][] rows(String json,int fields){
    ArrayList<int[]> result=new ArrayList<>();String part="(-?\\d+)";
    StringBuilder regex=new StringBuilder("\\[");for(int i=0;i<fields;++i){if(i>0)regex.append(',');regex.append(part);}regex.append("\\]");
    Matcher m=Pattern.compile(regex.toString()).matcher(json);while(m.find()){int[] row=new int[fields];for(int i=0;i<fields;++i)row[i]=Integer.parseInt(m.group(i+1));result.add(row);}return result.toArray(new int[0][]);
  }
  public static void main(String[] args)throws Exception {
    if(args.length!=4)throw new IllegalArgumentException("FONT3000 MES5015 MES5017 MES5019 JSON fixtures required");
    String font=Files.readString(Paths.get(args[0]));int[][] bearings=rows(font.substring(font.indexOf("\"bearings\""),font.indexOf("\"sheets\"")),2);
    for(int index=1;index<=3;++index){
      int[][] glyphs=rows(Files.readString(Paths.get(args[index])),3);
      CardMessageLayout layout=new CardMessageLayout(glyphs,bearings,100,210);
      check(layout.x+layout.width/2==256&&layout.y+layout.height/2==192,"Original glyph block centered");
      check(layout.width==(index==3?141:48)&&layout.height==(index==3?45:85),"Actual source bearings and blank lines preserved");
      check(layout.choices.length==2,"Actual message has precisely two choices");
      check(layout.horizontalChoices()==(index==3),"Save/load exit vertical, overwrite horizontal");
      float[] yes=layout.choices[0],no=layout.choices[1];
      check(yes[2]>yes[0]&&no[2]>no[0],"Hit regions cover complete Yes and No words");
      check(index==3?yes[2]<=no[0]:yes[3]<=no[1],"Choice hit regions do not overlap");
      for(int[] screen:new int[][]{{640,480},{1920,1080},{2400,1080}}){
        StartupViewport viewport=new StartupViewport(screen[0],screen[1],StartupFlow.Phase.LOAD);
        float centerX=(layout.x+layout.width/2)*1.25f;
        float centerY=(layout.y+layout.height/2)*1.25f;
        check(centerX*viewport.scaleX==screen[0]/2f&&(centerY-viewport.top)*viewport.scaleY==screen[1]/2f,
            "Original confirmation remains centered in the active card picture");
        for(float[] choice:layout.choices){
          float x=(choice[0]+choice[2])*.625f,y=(choice[1]+choice[3])*.625f;
          float hitX=viewport.sourceX(x*viewport.scaleX)/1.25f;
          float hitY=viewport.sourceY((y-viewport.top)*viewport.scaleY)/1.25f;
          check(hitX>=choice[0]&&hitX<=choice[2]&&hitY>=choice[1]&&hitY<=choice[3],
              "Displayed original choice maps back into its touch region after the card crop");
        }
      }
    }
    int[][] info={{43,0,0},{88,0,0}};CardMessageLayout nonChoice=new CardMessageLayout(info,bearings,100,210);
    check(nonChoice.x==100&&nonChoice.y==210&&nonChoice.choices.length==0,"Nonconfirmation messages keep authored position");
    System.out.println("Original card confirmation geometry: "+checks+" checks PASS");
  }
}
