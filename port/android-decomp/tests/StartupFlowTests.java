package com.ast.re4decomp;
public final class StartupFlowTests {
  private static int checks;
  private static void check(boolean pass) { ++checks; if (!pass) throw new AssertionError("Startup " + checks); }
  public static void main(String[] args) {
    StartupFlow f = new StartupFlow(); for (int i=0;i<78;++i) f.tick(); check(f.phase==StartupFlow.Phase.TIMELINE && f.frame==0);
    for(int i=0;i<585;++i)f.tick();check(f.phase==StartupFlow.Phase.MENU && f.cursor==0 && f.logoSound);
    for(int tick=0;tick<78+585;++tick){StartupFlow skipped=new StartupFlow();for(int i=0;i<tick;++i)skipped.tick();skipped.skip();check(skipped.phase==StartupFlow.Phase.MENU&&skipped.frame==585&&skipped.cursor==0&&skipped.menuFrame==0&&skipped.cursorFrame==0&&skipped.logoSound);skipped.tick();skipped.skip();check(skipped.phase==StartupFlow.Phase.MENU&&skipped.menuFrame==1&&skipped.frame==586);}
    StartupFlow skipped=new StartupFlow();skipped.skip();check(skipped.menuEntryFade()==0 && skipped.titleFrame()==585);
    for(int n=1;n<=15;++n){skipped.tick();check(Math.abs(skipped.menuEntryFade()-n/15f)<.0001f);check(skipped.titleFrame()==585+n);}
    skipped.move(1);check(skipped.menuEntryFade()==1 && skipped.menuFrame==15);skipped.skip();check(skipped.menuFrame==15);
    skipped.confirm();skipped.back();check(skipped.menuEntryFade()==1 && skipped.titleFrame()==600);
    check(!f.menuFadeIn && f.menuEntryFade()==1); // Natural intro already supplied its fade.
    f.move(-1);f.move(-1);check(f.cursor==0);f.confirm();check(f.phase==StartupFlow.Phase.NEW_GAME_FADE);for(int i=0;i<90;++i)f.tick();check(f.phase==StartupFlow.Phase.NEW_GAME_FADE&&f.frame==90&&f.fade()==0);f.startGameVoiceEnded(true);check(f.phase==StartupFlow.Phase.OPENING);f.movieComplete();check(f.phase==StartupFlow.Phase.LAUNCH);
    f=new StartupFlow();for(int i=0;i<78+585+600;++i)f.tick();check(f.phase==StartupFlow.Phase.DEMO && f.demoNumber==1);f.movieComplete();for(int i=0;i<600;++i)f.tick();check(f.phase==StartupFlow.Phase.DEMO && f.demoNumber==0);
    float[][] curve={{0,0,0,0},{10,10,0,0}};check(StartupMath.curve(curve,5,-1,false)==5);check(StartupMath.curve(curve,15,-1,false)==10);check(StartupMath.curve(curve,15,-1,true)==5);
    float[][] p={{0,0,0},{10,10,0},{20,0,0},{30,10,0}};float[][] c=StartupMath.coefficients(p,3);for(int i=0;i<p.length;++i){float[]actual=StartupMath.position(c,3,i);check(Math.abs(actual[0]-p[i][0])<.001 && Math.abs(actual[1]-p[i][1])<.001);}
    int[][] tree={{0,255,0},{19,255,1},{167,19,1},{46,19,0},{168,167,0},{169,167,0}};
    check(java.util.Arrays.equals(StartupMath.drawOrder(tree),new int[]{1,3,2,5,4,0}));
    System.out.println("Startup source timeline/curve/path: "+checks+" checks PASS");
  }
}
