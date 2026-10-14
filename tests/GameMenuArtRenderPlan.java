import com.ast.re4decomp.GameMenuLayout;

/** Export the production slice/selection geometry for original-data pixel QA.
 * This intentionally does not substitute a copied set of layout constants. */
public final class GameMenuArtRenderPlan {
  private static String array(float[] values) {
    StringBuilder result=new StringBuilder("[");
    for(int n=0;n<values.length;++n) { if(n>0)result.append(',');result.append(values[n]); }
    return result.append(']').toString();
  }
  public static void main(String[] args) {
    GameMenuLayout ui=new GameMenuLayout(Integer.parseInt(args[0]),Integer.parseInt(args[1]));
    System.out.print("{\"width\":"+ui.width+",\"height\":"+ui.height+",\"scale\":"+ui.scale+",\"slices\":[");
    float[][] slices=ui.frameSlices();
    for(int n=0;n<slices.length;++n) { if(n>0)System.out.print(',');System.out.print(array(slices[n])); }
    System.out.print("],\"case\":"+array(ui.caseTransform(10,6))+",\"caseQuad\":"+array(ui.caseQuad(10,6,0,0,3,2))+",\"visible\":[");
    for(int tab=0;tab<5;++tab) {
      if(tab>0)System.out.print(',');System.out.print('[');
      for(int mark=0;mark<5;++mark) { if(mark>0)System.out.print(',');System.out.print(GameMenuLayout.tabParentVisible(mark,tab)); }
      System.out.print(']');
    }
    System.out.print("],\"grid\":[");
    for(int x=0;x<=10;++x) {
      if(x>0)System.out.print(',');float[] a=ui.casePoint(10,6,x,0),b=ui.casePoint(10,6,x,6);
      System.out.print(array(new float[]{a[0],a[1],b[0],b[1]}));
    }
    for(int y=0;y<=6;++y) {
      float[] a=ui.casePoint(10,6,0,y),b=ui.casePoint(10,6,10,y);
      System.out.print(','+array(new float[]{a[0],a[1],b[0],b[1]}));
    }
    System.out.println("]}");
  }
}
