import com.ast.re4decomp.GameMenuNavigation;

/** Input contracts: menu opening does not repeat immediately or hide rotated pieces. */
public final class GameMenuNavigationTests {
  private static int checks;
  private static void check(boolean value) { ++checks; if (!value) throw new AssertionError("check " + checks); }
  public static void main(String[] args) {
    GameMenuNavigation menu = new GameMenuNavigation();
    check(menu.sample(0, 0, 0) == 0);
    check(menu.sample(.3f, .1f, 10) == 0);
    check(menu.sample(.7f, .1f, 20) == 1);
    check(menu.sample(.7f, .1f, 339) == 0);
    check(menu.sample(.7f, .1f, 340) == 1);
    check(menu.sample(.7f, .1f, 469) == 0);
    check(menu.sample(.7f, .1f, 470) == 1);
    check(menu.sample(-.7f, .1f, 480) == -1);
    check(menu.sample(.1f, -.8f, 490) == -2);
    check(menu.sample(.1f, .8f, 500) == 2);
    check(menu.sample(Float.NaN, 0, 510) == 0);
    check(menu.sample(.7f, 0, 511) == 1);
    menu.reset(); check(menu.sample(.7f, 0, 512) == 1);
    check(menu.sample(0, 0, 520) == 0);
    check(menu.sample(.7f, 0, 521) == 1);
    check(GameMenuNavigation.fits(10, 6, 7, 4, 3, 2));
    check(!GameMenuNavigation.fits(10, 6, 8, 4, 3, 2));
    check(!GameMenuNavigation.fits(10, 6, 7, 5, 3, 2));
    check(!GameMenuNavigation.fits(10, 6, -1, 0, 3, 2));
    check(!GameMenuNavigation.fits(10, 6, 0, 0, 0, 2));
    check(GameMenuNavigation.clampPosition(9, 8, 10) == 2);
    check(GameMenuNavigation.clampPosition(5, 8, 6) == 0);
    check(GameMenuNavigation.clampPosition(-1, 3, 10) == 0);
    check(GameMenuNavigation.overlaps(0, 0, 3, 2, 2, 1, 2, 1));
    check(!GameMenuNavigation.overlaps(0, 0, 3, 2, 3, 0, 2, 1));
    check(!GameMenuNavigation.overlaps(0, 0, 3, 2, 0, 2, 2, 1));
    check(GameMenuNavigation.keyOrTreasure(-1, -1, 7));
    check(GameMenuNavigation.keyOrTreasure(-1, -1, 5));
    check(GameMenuNavigation.keyOrTreasure(-1, -1, 12));
    check(!GameMenuNavigation.keyOrTreasure(-1, -1, 10));
    check(!GameMenuNavigation.keyOrTreasure(-1, -1, 11));
    check(!GameMenuNavigation.keyOrTreasure(0, 0, 6));
    // Source case board stays inside its Disc model opening. All four retail
    // sizes are navigable without changing the source FOV or perspective.
    for(int[] size:new int[][]{{10,6},{11,7},{12,8},{15,8}}) {
      float[] quad=GameMenuNavigation.caseQuad(size[0],0,0,size[0],size[1]);
      check(quad[0]<quad[2] && quad[6]<quad[4]);
      check(quad[1]<quad[7] && quad[3]<quad[5]);
      for(float coordinate:quad)check(Float.isFinite(coordinate));
      float[] zero=GameMenuNavigation.casePoint(size[0],0,0);
      float[] bottom=GameMenuNavigation.casePoint(size[0],0,size[1]);
      check(Math.abs(zero[0]-quad[0])<.001f && Math.abs(bottom[1]-quad[7])<.001f);
      // The perspective widens toward the near edge of the tilted board.
      check(quad[4]-quad[6]>quad[2]-quad[0]);
    }
    System.out.println("Menu repeat, rotated placement, collision and source categories: " + checks + " checks PASS");
  }
}
