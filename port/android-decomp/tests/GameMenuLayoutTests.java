import com.ast.re4decomp.GameMenuLayout;
import com.ast.re4decomp.GameMenuNavigation;

/** Aspect changes exercise actual panel/viewport geometry and touch inverse,
 * including rotated piece corners; no Android/JNI or copied game assets. */
public final class GameMenuLayoutTests {
  private static int checks;
  private static void check(boolean value) { ++checks;if(!value)throw new AssertionError("layout check "+checks); }
  private static void near(float a,float b) { check(Math.abs(a-b)<.015f); }
  private static void reject(float width,float height) {
    boolean caught=false;try { new GameMenuLayout(width,height); }catch(IllegalArgumentException expected) { caught=true; }check(caught);
  }
  public static void main(String[] args) {
    reject(0,480);reject(640,Float.NaN);reject(Float.POSITIVE_INFINITY,480);reject(640,-1);
    // Screenshot's original active frame is640x352. Frame/model/font share
    // this scale; source font y330 maps inside the separate bottom window.
    for(int[] display:new int[][]{{1331,571},{1920,1080},{2400,1080},{640,480},{480,640}}) {
      GameMenuLayout pickup=new GameMenuLayout(display[0],display[1],GameMenuLayout.PICKUP_HEIGHT);
      near(pickup.width*pickup.scale,display[0]);near(pickup.height*pickup.scale,display[1]);
      float sourceTop=pickup.extraY*.5f-GameMenuLayout.PICKUP_TOP;
      float messageTop=298+sourceTop,messageBottom=408+sourceTop,choicesTop=330+sourceTop;
      check(messageTop>=0 && messageBottom<=pickup.height && choicesTop>=messageTop && choicesTop+47<=messageBottom);
      near((240+sourceTop)*pickup.scale,display[1]*.5f);
      if(display[0]==1331) {
        near((pickup.extraX*.5f)*pickup.scale,146.4091f);
        near(messageTop*pickup.scale,379.5852f);
      }
    }
    int[][] displays={{640,480},{1280,720},{1920,1080},{2400,1080},{2560,1080},{1024,1024},{1080,1920},{480,640}};
    for(int[] display:displays) {
      GameMenuLayout ui=new GameMenuLayout(display[0],display[1]);
      // Canvas starts at(0,0), its far corners reach the actual display: no
      // old4:3 letterbox offsets. A circle/model unit has one uniform scale.
      near(ui.width*ui.scale,display[0]);near(ui.height*ui.scale,display[1]);
      check(ui.width>=639.99f && ui.height>=479.99f);
      near(ui.right(640),ui.width);near(ui.bottom(480),ui.height);
      float area=0;
      float[][] slices=ui.frameSlices();check(slices.length==9);
      for(int n=0;n<slices.length;++n) {
        float[] slice=slices[n];
        check(slice[0]>=0 && slice[2]<=640 && slice[1]>=60 && slice[3]<=420);
        check(slice[2]>slice[0] && slice[3]>slice[1] && slice[6]>slice[4] && slice[7]>slice[5]);
        check(slice[4]>=0 && slice[5]>=0 && slice[6]<=ui.width && slice[7]<=ui.height);
        area+=(slice[6]-slice[4])*(slice[7]-slice[5]);
        if(n%3>0)near(slice[4],slices[n-1][6]);
        if(n>=3)near(slice[5],slices[n-3][7]);
      }
      near(area/ui.width,ui.height);near(slices[0][4],0);near(slices[0][5],0);
      near(slices[8][6],ui.width);near(slices[8][7],ui.height);
      for(int tab=0;tab<5;++tab)for(int mark=0;mark<5;++mark)
        check(GameMenuLayout.tabParentVisible(mark,tab)==(mark==tab));
      float previous=80;
      for(int tab=0;tab<5;++tab) {
        float[] hit=ui.tabBounds(tab);near(hit[0],previous);previous=hit[2];
        check(hit[0]>=0 && hit[2]<=ui.width && hit[0]<hit[2]);
        check(hit[1]>=0 && hit[3]<=ui.height);
      }
      near(previous,ui.width-83);
      float first=ui.keyColumnLeft(0),second=ui.keyColumnLeft(1),columnWidth=ui.keyColumnWidth();
      near(first,28);near(second-first-columnWidth,24);near(second+columnWidth,ui.width-28);
      check(columnWidth>=279.99f);
      check(ui.listRows(119,ui.bottom(397),34,32)>=8);
      check(ui.listRows(119,ui.bottom(397),39,32)>=7);
      float[] bounds=ui.modelBounds(),normalized=ui.normalized(bounds);
      check(bounds[2]>bounds[0] && bounds[3]>bounds[1]);
      for(float value:normalized)check(Float.isFinite(value) && value>0 && value<1);
      near(normalized[0]*ui.width,bounds[0]);near(normalized[1]*ui.height,bounds[1]);
      near((normalized[0]+normalized[2])*ui.width,bounds[2]);near((normalized[1]+normalized[3])*ui.height,bounds[3]);
      // The portrait is right/bottom anchored with its original640x480
      // source rectangle, never stretched to the flexible full panel.
      near(ui.width-ui.extraX,640);near(ui.height-ui.extraY,480);
      for(int[] size:new int[][]{{10,6},{11,7},{12,8},{15,8}}) {
        int columns=size[0],rows=size[1];float[] transform=ui.caseTransform(columns,rows);
        check(Float.isFinite(transform[0]) && transform[0]>0);
        float[] source=GameMenuNavigation.caseQuad(columns,0,0,columns,rows),drawn=ui.caseQuad(columns,rows,0,0,columns,rows);
        for(int n=0;n<4;++n) {
          near(drawn[n*2],source[n*2]*transform[0]+transform[1]);near(drawn[n*2+1],source[n*2+1]*transform[0]+transform[2]);
          check(drawn[n*2]>=0 && drawn[n*2]<=ui.width && drawn[n*2+1]>=107 && drawn[n*2+1]<ui.bottom(389));
        }
        for(int row=0;row<rows;++row)for(int column=0;column<columns;++column) {
          float[] point=ui.casePoint(columns,rows,column+.5f,row+.5f),cell=ui.caseQuad(columns,rows,column,row,1,1);
          check(GameMenuLayout.insideQuad(cell,point[0],point[1]));
          int[] picked=ui.caseCell(columns,rows,point[0],point[1]);check(picked!=null && picked[0]==column && picked[1]==row);
        }
        check(ui.caseCell(columns,rows,-1,-1)==null);
        check(!GameMenuLayout.insideQuad(drawn,ui.width+1,drawn[1]));
        float[] neighboringA=ui.caseQuad(columns,rows,0,0,2,3),neighboringB=ui.caseQuad(columns,rows,2,0,3,2);
        near(neighboringA[2],neighboringB[0]);near(neighboringA[3],neighboringB[1]);
      }
    }
    GameMenuLayout source=new GameMenuLayout(640,480);
    for(int[] display:displays) {
      GameMenuLayout inventory=new GameMenuLayout(display[0],display[1],360,60);
      near(inventory.visibleHeight*inventory.scale,display[1]);near(inventory.top,60);
      float area=0;for(float[] slice:inventory.frameSlices()) {
        check(slice[5]>=inventory.top && slice[7]<=inventory.top+inventory.visibleHeight);
        area+=(slice[6]-slice[4])*(slice[7]-slice[5]);
      }
      near(area/inventory.width,inventory.visibleHeight);
      float[] bounds=inventory.modelBounds(),uv=inventory.normalized(bounds);
      near(uv[1]*inventory.visibleHeight+inventory.top,bounds[1]);
      near((uv[1]+uv[3])*inventory.visibleHeight+inventory.top,bounds[3]);
      for(int row=0;row<6;++row)for(int column=0;column<10;++column) {
        float[] point=inventory.casePoint(10,6,column+.5f,row+.5f);
        // Same conversion as Android's Canvas and touch event, including crop.
        float physicalY=(point[1]-inventory.top)*inventory.scale;
        int[] picked=inventory.caseCell(10,6,point[0],physicalY/inventory.scale+inventory.top);
        check(picked!=null && picked[0]==column && picked[1]==row);
      }
    }
    GameMenuLayout nativeInventory=new GameMenuLayout(1280,720,360,60);
    near(nativeInventory.scale,2);near(nativeInventory.width,640);near(nativeInventory.height,480);
    near(nativeInventory.captionScale(),1);near(nativeInventory.caseTransform(10,6)[0],1);
    for(float[] slice:nativeInventory.frameSlices())for(int n=0;n<4;++n)near(slice[n],slice[n+4]);
    near(source.captionScale(),1);
    near(new GameMenuLayout(1920,1080).captionScale(),4f/3);
    near(source.caseTransform(10,6)[0],1);near(source.caseTransform(10,6)[1],0);near(source.caseTransform(10,6)[2],0);
    check(!GameMenuLayout.insideQuad(new float[]{0,0,0,0,0,0,0,0},0,0));
    System.out.println("Full viewport, isotropic model/piece layout, flexible lists and touch inverse: "+checks+" checks PASS");
  }
}
