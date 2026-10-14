import com.ast.re4decomp.GameMapCamera;
import com.ast.re4decomp.GameMenuLayout;
public final class GameMapCameraTests {
  private static int checks;
  private static void check(boolean pass,String why) {++checks;if(!pass)throw new AssertionError(why);}
  public static void main(String[] args) {
    GameMapCamera map=new GameMapCamera();
    map.reset(new double[]{0,0,10000},new double[]{1000,-2000,8000},7000,100);
    check(map.advance(267) && Math.abs(map.position[0]-501)<1.e-8 && Math.abs(map.position[1]+1002)<1.e-8 && Math.abs(map.position[2]-8998)<1.e-8,"Camera interpolates continuously within the source ten-tick transition");
    double before=map.position[0];map.advance(275);check(map.position[0]>before && map.position[0]<600,"Display refreshes between source ticks visibly advance the map");
    before=map.position[0];map.pan(99,99);check(map.position[0]==before,"Scrolling waits for zoom transition");
    check(!map.advance(434) && map.position[0]==1000 && !map.entire,"Focus reaches source camera");
    map.pan(100,-50);check(map.position[0]==1100 && map.position[1]==-2050,"Read pans without returning to player every draw");
    for(int i=0;i<100;++i)map.zoom(1);check(map.position[2]==7000,"L/R zoom obeys source lower limit");
    map.toggle(500);check(map.entire,"A selects whole map and swaps footer immediately");
    map.advance(834);check(map.position[0]==0 && map.position[1]==0 && map.position[2]==10000,"Whole view uses source stage camera");
    map.toggle(900);map.advance(1234);check(!map.entire && map.position[0]==1000 && map.position[2]==8000,"Whole map A/B returns to focused camera");
    for(boolean entire:new boolean[]{false,true}) {
      int visible=0;for(int mark=0;mark<3;++mark)if(GameMenuLayout.mapCaptionVisible(mark,entire))++visible;
      check(visible==1,"Exactly one mutually exclusive footer caption");
    }
    System.out.println("Map camera/footer PASS: "+checks);
  }
}
