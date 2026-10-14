package com.ast.re4decomp;

/** User-requested stretch: authored active picture fills all screen ratios. */
final class StartupViewport {
  final float scaleX,scaleY,top,height;
  StartupViewport(float width,float viewHeight,StartupFlow.Phase phase){top=phase==StartupFlow.Phase.OPTIONS?46:60;height=phase==StartupFlow.Phase.OPTIONS?388:360;scaleX=width/640;scaleY=viewHeight/height;}
  float sourceX(float x){return x/scaleX;}
  float sourceY(float y){return y/scaleY+top;}
}
