package com.ast.re4decomp;

/** ss_map MapFocus/Read/Entire: source camera and ten 30Hz interpolation ticks. */
public final class GameMapCamera {
  private final double[] whole=new double[3],focus=new double[3],from=new double[3],to=new double[3];
  public final double[] position=new double[3];
  private double minimum;
  private long start;
  private boolean initialized,animating;
  public boolean entire;
  public void reset(double[] wholeView,double[] focusedView,double minHeight,long now) {
    if(wholeView.length!=3 || focusedView.length!=3 || !(minHeight>0))throw new IllegalArgumentException("Map camera");
    for(int i=0;i<3;++i) {
      if(!Double.isFinite(wholeView[i]) || !Double.isFinite(focusedView[i]))throw new IllegalArgumentException("Map coordinate");
      whole[i]=wholeView[i];focus[i]=focusedView[i];position[i]=whole[i];
    }
    minimum=minHeight;initialized=true;entire=false;begin(focus,now);
  }
  public boolean initialized() { return initialized; }
  private void begin(double[] destination,long now) {
    System.arraycopy(position,0,from,0,3);System.arraycopy(destination,0,to,0,3);
    start=now;animating=true;
  }
  public void toggle(long now) { advance(now);entire=!entire;begin(entire?whole:focus,now); }
  public boolean advance(long now) {
    if(!animating)return false;
    double amount=Math.min(10,Math.max(0,(now-start)*30/1000.))/10.;
    for(int i=0;i<3;++i)position[i]=from[i]*(1-amount)+to[i]*amount;
    animating=amount<1;return animating;
  }
  public void pan(double x,double z) { if(!entire && !animating){position[0]+=x;position[1]+=z;} }
  public void zoom(int step) {
    // ss_map map_dbg_cam1[0]: one held L/R tick changes height by1000,
    // independent of the current zoom distance.
    if(!entire && !animating)position[2]=Math.max(minimum,Math.min(whole[2],position[2]-step*1000));
  }
}
