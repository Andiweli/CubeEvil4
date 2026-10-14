package com.ast.re4decomp;

/** GX title TEV/indirect operations; RGB channels use an opaque EFB destination. */
final class StartupPixels {
  private StartupPixels(){}
  static float[] tint(float[]color,float[]color1,float[][]curve,float frame,boolean loop){float[]out=color.clone();if(curve.length==0)return out;float a=StartupMath.curve(curve,frame,0,loop);if(color1[0]+color1[1]+color1[2]+color1[3]!=0){for(int k=0;k<4;++k)out[k]=Math.max(0,Math.min(255,(1-a)*color[k]+a*color1[k]));}else out[3]=a;return out;}
  static int shimmerCoefficient(int alpha,int sub){ // IdShimmerTrans clamps the unresolved decompiled depth to1500.
    return (int)(1024*(alpha/255f)*.04f*1000/1500*(1+sub/32f));
  }
  static float shimmerOffset(int coefficient,int channel,boolean signed){return ((coefficient*(channel-(signed?128:0)))>>2)/128f;}
  static int negative(int rgb,int pow){int scale=1<<Math.min(2,Math.max(0,pow));return 0xff000000|((255-Math.min(255,((rgb>>16)&255)*scale))<<16)|((255-Math.min(255,((rgb>>8)&255)*scale))<<8)|(255-Math.min(255,(rgb&255)*scale));}
  static int tintRgb(int rgb,float[]tint){return 0xff000000|(clamp(((rgb>>16)&255)*tint[0]/255)<<16)|(clamp(((rgb>>8)&255)*tint[1]/255)<<8)|clamp((rgb&255)*tint[2]/255);}
  static int blend(int src,int dst,int alpha,int mode){if(alpha<=1)return dst;int r=blendChannel((src>>16)&255,(dst>>16)&255,alpha,mode),g=blendChannel((src>>8)&255,(dst>>8)&255,alpha,mode),b=blendChannel(src&255,dst&255,alpha,mode);return 0xff000000|(r<<16)|(g<<8)|b;}
  private static int blendChannel(int src,int dst,int alpha,int mode){switch(mode){case 1:return clamp(src*(alpha/255f)+dst);case 2:return clamp(src+dst);case 3:return clamp(src*(dst/255f)+dst);case 4:return clamp(src*(dst/255f));default:return clamp(src*(alpha/255f)+dst*(1-alpha/255f));}}
  private static int clamp(float v){return Math.max(0,Math.min(255,Math.round(v)));}
  static int sample(int[]pixels,int width,int height,float x,float y){x=Math.max(0,Math.min(width-1,x));y=Math.max(0,Math.min(height-1,y));int ix=(int)x,iy=(int)y,jx=Math.min(width-1,ix+1),jy=Math.min(height-1,iy+1);float fx=x-ix,fy=y-iy;int a=pixels[iy*width+ix],b=pixels[iy*width+jx],c=pixels[jy*width+ix],d=pixels[jy*width+jx],out=0;for(int shift=0;shift<=24;shift+=8){float upper=((a>>shift)&255)*(1-fx)+((b>>shift)&255)*fx,lower=((c>>shift)&255)*(1-fx)+((d>>shift)&255)*fx;out|=clamp(upper*(1-fy)+lower*fy)<<shift;}return out;}
}
