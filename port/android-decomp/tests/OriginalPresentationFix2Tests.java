package com.ast.re4decomp;

import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/** The unchanged original UWF geometry is exported by k17_fix2_check. */
public final class OriginalPresentationFix2Tests {
  private static int checks;
  private static void check(boolean pass,String message){++checks;if(!pass)throw new AssertionError(message);}
  private static float[] transform(float[] u,Map<Integer,float[]> units){
    float[] result=StartupMath.idTransform(u[6],u[7],u[10],u[11],u[12]);int parent=(int)u[2];
    for(int depth=0;parent!=255&&depth<32;++depth){float[] p=units.get(parent);check(p!=null,"Source scope parent exists");if(p[3]==1)result=StartupMath.concat(StartupMath.idTransform(p[6],p[7],p[10],p[11],p[12]),result);parent=(int)p[2];}
    return StartupMath.screenMatrix(result);
  }
  public static void main(String[] args)throws Exception {
    // Variable paint/handler cost must not slow down the source 30Hz clock.
    for(long paint:new long[]{0,8_000_000,25_000_000}) {
      OriginalFrameClock clock=new OriginalFrameClock();long now=1_000_000_000L,start=now;int frames=clock.advance(now);
      while(now-start<10_000_000_000L){now+=clock.delayMillis()*1_000_000L+paint;frames+=clock.advance(now);}
      check(frames==(int)((now-start)*30/1_000_000_000L),"30Hz keeps pace with actual time including paint cost");
      clock.reset();check(clock.advance(now+30_000_000_000L)==0,"Resume resets the suspended UI clock");
      check(clock.advance(now+30_100_000_000L)==3,"Three delayed source frames are advanced together");
    }
    float[] half=StartupMath.idTransform(0,0,90,0,0),full=StartupMath.screenMatrix(StartupMath.concat(half,half));
    check(Math.abs(full[4]+1)<.0001f,"Nested X rotations preserve Z until the full chain is composed");
    if(args.length==1) {
      Map<Integer,ArrayList<float[]>> scopes=new HashMap<>();
      for(String line:Files.readAllLines(Paths.get(args[0]))){String[] fields=line.split(",");float[] u=new float[fields.length];for(int k=0;k<u.length;++k)u[k]=Float.parseFloat(fields[k]);scopes.computeIfAbsent((int)u[0],key->new ArrayList<>()).add(u);}
      check(scopes.size()==10,"Every rifle variant,3 launchers and scoped mine UWF checked");
      for(int id:new int[]{0x2e,0x2f,0x6b,0x6c,0x99,0x51}) {
        Map<Integer,float[]> units=new HashMap<>();for(float[] u:scopes.get(id))units.put((int)u[1],u);
        ArrayList<float[]> centers=new ArrayList<>();
        for(float[] u:scopes.get(id))if(u[4]>=16 && u[4]<=19) {
          float[] m=transform(u,units);float w=Math.abs(u[8]),h=Math.abs(u[9]);
          float x=-w/2,y=-h/2;if(u[13]==1){x=0;y=0;}else if(u[13]==2){x=-w;y=0;}else if(u[13]==3){x=-w;y=-h;}else if(u[13]==4){x=0;y=-h;}
          x=(x+w/2)*Math.signum(u[8]);y=(y+h/2)*Math.signum(u[9]);
          centers.add(new float[]{320+m[0]*x+m[1]*y+m[2],240+m[3]*x+m[4]*y+m[5]});
        }
        check(centers.size()==4,"Four original rifle mask plates");float mx=0,my=0;for(float[] c:centers){mx+=c[0]/4;my+=c[1]/4;}
        check(Math.abs(mx-320)<.01,"Scope masks are horizontally centred");int quadrants=0;
        for(float[] c:centers){check(Math.abs(Math.abs(c[0]-mx)-160)<.01 && Math.abs(Math.abs(c[1]-my)-112)<.01,"Each original plate occupies its full-sized quadrant");quadrants|=1<<((c[0]>mx?1:0)+(c[1]>my?2:0));}
        check(quadrants==15,"Mask covers all four quadrants instead of top-right only");
      }
      for(int id:new int[]{0x35,0x17,0x6d,0xab}) {
        boolean signed=false;for(float[] u:scopes.get(id))if(u[8]<0 || u[9]<0)signed=true;
        check(signed,"Launcher/mine source retains signed mirror quads");
      }
    }
    System.out.println("Fix2 original 30Hz UI clock,3D ID transforms and10 scope layouts PASS: "+checks+" checks");
  }
}
