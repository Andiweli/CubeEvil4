package com.ast.re4decomp;
import android.opengl.GLSurfaceView;
import javax.microedition.khronos.egl.*;
/** Keep the window single-sampled; selectable MSAA belongs to the native scene FBO. */
final class SingleSampleConfigChooser implements GLSurfaceView.EGLConfigChooser {
  @Override public EGLConfig chooseConfig(EGL10 egl,EGLDisplay display) {
    int[] attrs={EGL10.EGL_RED_SIZE,8,EGL10.EGL_GREEN_SIZE,8,EGL10.EGL_BLUE_SIZE,8,
      EGL10.EGL_ALPHA_SIZE,8,EGL10.EGL_DEPTH_SIZE,24,EGL10.EGL_RENDERABLE_TYPE,4,EGL10.EGL_NONE};
    int[] count=new int[1];
    if(!egl.eglChooseConfig(display,attrs,null,0,count) || count[0]==0)throw new IllegalArgumentException("No RGBA8 depth24 EGL configuration");
    EGLConfig[] configs=new EGLConfig[count[0]];
    if(!egl.eglChooseConfig(display,attrs,configs,configs.length,count))throw new IllegalArgumentException("Cannot enumerate EGL configurations");
    int[] value=new int[1];
    for(EGLConfig config:configs) {
      if(!egl.eglGetConfigAttrib(display,config,EGL10.EGL_SAMPLES,value) || value[0]!=0)continue;
      boolean exact=true;
      for(int channel:new int[]{EGL10.EGL_RED_SIZE,EGL10.EGL_GREEN_SIZE,EGL10.EGL_BLUE_SIZE,EGL10.EGL_ALPHA_SIZE}) {
        if(!egl.eglGetConfigAttrib(display,config,channel,value) || value[0]!=8){exact=false;break;}
      }
      if(exact)return config;
    }
    throw new IllegalArgumentException("No single-sample RGBA8 EGL configuration");
  }
}
