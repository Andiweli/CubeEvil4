import com.ast.re4decomp.GameMenuLayout;
public final class TestCaseSpaceLayout {
 static int checks;
 static void check(boolean v,String s){++checks;if(!v)throw new AssertionError(s);}
 public static void main(String[] args){
  int[][] sizes={{10,6},{11,7},{12,8},{15,8}},screens={{640,360},{1280,720},{1920,1080},{1920,1200},{2560,1080},{640,480}};
  for(int[] screen:screens)for(int[] size:sizes){GameMenuLayout l=new GameMenuLayout(screen[0],screen[1],360,60);float[] before=l.caseQuad(size[0],size[1],0,0,size[0],size[1]);l.puzzleActive=true;float[] after=l.caseQuad(size[0],size[1],0,0,size[0],size[1]);for(int n=0;n<8;++n)check(before[n]==after[n],"Case never changes size/position when space opens");
   for(int y=0;y<12;++y)for(int x=0;x<6;++x){float[] q=l.spaceQuad(size[0],size[1],x,y,1,1);float cx=0,cy=0;for(int n=0;n<4;++n){cx+=q[n*2]/4;cy+=q[n*2+1]/4;}int[] at=l.spaceCell(size[0],size[1],cx,cy);check(at!=null && at[0]==x && at[1]==y,"Source-space touch selects correct cell");check(cx<l.width && cx>0,"Space cells remain accessible");}
   for(int y=0;y<size[1];++y)for(int x=0;x<size[0];++x){float[] q=l.caseQuad(size[0],size[1],x,y,1,1);float cx=0,cy=0;for(int n=0;n<4;++n){cx+=q[n*2]/4;cy+=q[n*2+1]/4;}int[] at=l.caseCell(size[0],size[1],cx,cy);check(at!=null && at[0]==x && at[1]==y,"Case touch retains original grid");}
  }
  System.out.println("Fix18 original layout PASS: "+checks+" geometry/touch checks");
 }
}
