package com.ast.re4decomp;

/** Original 30 Hz frames measured against monotonic time, including render cost. */
final class OriginalFrameClock {
  private long previous;
  private double remainder;
  void reset(){previous=0;remainder=0;}
  int advance(long now){
    if(previous==0){previous=now;return 0;}
    long elapsed=Math.max(0,Math.min(250_000_000L,now-previous));previous=now;
    remainder+=elapsed*30/1_000_000_000.0;
    int frames=(int)(remainder+1e-9);remainder-=frames;return frames;
  }
  long delayMillis(){return Math.max(1,(long)Math.ceil((1-remainder)*1000/30));}
}
