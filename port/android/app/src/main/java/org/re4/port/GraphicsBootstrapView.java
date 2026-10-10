package org.re4.port;

import android.content.Context;
import android.util.Log;
import android.view.Choreographer;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/** Real clear-only Vulkan surface. No GX draw or playable-room claim. */
public final class GraphicsBootstrapView extends SurfaceView implements SurfaceHolder.Callback, Choreographer.FrameCallback {
    private long handle;
    private boolean active;
    private int frames;
    private String report = "Vulkan surface not attached";
    public GraphicsBootstrapView(Context context) {
        super(context); getHolder().addCallback(this);
        System.loadLibrary("re4_port_jni");
    }
    @Override public void surfaceCreated(SurfaceHolder holder) {
        try { handle = nativeOpen(holder.getSurface()); active = true; Choreographer.getInstance().postFrameCallback(this); }
        catch (IllegalStateException error) { report = error.getMessage(); Log.e("RE4Graphics",report); }
    }
    @Override public void surfaceChanged(SurfaceHolder holder,int format,int width,int height) {
        // Swapchain OUT_OF_DATE is handled by the native owner on its next frame.
    }
    @Override public void surfaceDestroyed(SurfaceHolder holder) {
        active = false; Choreographer.getInstance().removeFrameCallback(this);
        if (handle != 0) { nativeClose(handle); handle = 0; }
    }
    @Override public void doFrame(long timeNanos) {
        if (!active || handle == 0) return;
        try { report = nativeFrame(handle,false); frames++; }
        catch (IllegalStateException error) { report = error.getMessage(); active = false; Log.e("RE4Graphics",report); }
        if (active) Choreographer.getInstance().postFrameCallback(this);
    }
    public boolean ready() { return handle != 0 && active && frames > 0; }
    public String report() { return report; }
    public String verifyOriginalRuntime() {
        if (!ready()) throw new IllegalStateException(report);
        Choreographer.getInstance().removeFrameCallback(this);
        try {
            // This invokes actual GameTask/R120Event bodies against the existing
            // runtime fixtures; asset/model verification uses the separate real-data CLI.
            String result = nativeOriginalProbe(handle,getContext().getCacheDir()+"/graphics-original-probe");
            nativeFrame(handle,true); nativeFrame(handle,false);
            Log.i("RE4Graphics",result); return result;
        } finally { if (active) Choreographer.getInstance().postFrameCallback(this); }
    }
    private static native long nativeOpen(Surface surface);
    private static native void nativeClose(long handle);
    private static native String nativeFrame(long handle,boolean black);
    private static native String nativeOriginalProbe(long handle,String directory);
}
