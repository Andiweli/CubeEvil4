package org.re4.port;

import android.content.Context;
import android.util.Log;
import android.view.Choreographer;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/** Vulkan clears and isolated original-data geometry diagnostics; no playable-room claim. */
public final class GraphicsBootstrapView extends SurfaceView implements SurfaceHolder.Callback, Choreographer.FrameCallback {
    private long handle;
    private boolean active;
    private int frames;
    private String report = "Vulkan surface not attached";
    private String sceneRoot, sceneCatalog, sceneEvent;
    private String sceneModel="";
    private String evidenceSession="phase5a";
    private int sceneRoom=0x120,sceneFrame;
    private int sceneCut=-1;
    public interface SceneObserver { void accept(String report); }
    private SceneObserver sceneObserver;
    public void configureEvidenceSession(String session) {
        if(handle!=0)throw new IllegalStateException("Configure before attaching surface");
        if(session==null||!session.matches("[A-Za-z0-9_-]{1,80}"))
            throw new IllegalArgumentException("Evidence session must be a safe file component");
        evidenceSession=session;
    }
    public void configureScene(String root,String catalog,String event,SceneObserver observer) {
        configureScene(root,catalog,0x120,event,0,observer);
    }
    public void configureScene(String root,String catalog,int room,String event,int frame,SceneObserver observer) {
        if(handle!=0) throw new IllegalStateException("Configure before attaching surface");
        if(room<0||room>0xffff||frame<0)throw new IllegalArgumentException("Original room/frame range");
        sceneRoom=room;sceneFrame=frame;
        sceneRoot=root;sceneCatalog=catalog;sceneEvent=event;sceneObserver=observer;
    }
    public GraphicsBootstrapView(Context context) {
        super(context); getHolder().addCallback(this);
        System.loadLibrary("re4_port_jni");
    }
    public void configureEventModel(String name) {
        if(handle!=0)throw new IllegalStateException("Configure before attaching surface");
        if(name==null||!name.matches("[A-Za-z0-9_]{0,11}"))throw new IllegalArgumentException("Original event model name");
        sceneModel=name;
    }
    public void configureEventCut(int cut) {
        if(handle!=0||cut< -1||cut>32767)throw new IllegalArgumentException("Original event cut");sceneCut=cut;
    }
    @Override public void surfaceCreated(SurfaceHolder holder) {
        try { frames=0;handle = nativeOpen(holder.getSurface());
            if(sceneRoot!=null) nativeLoadScene(handle,sceneRoot,sceneCatalog,sceneRoom,sceneEvent,sceneFrame,sceneModel,sceneCut);
            active = true; Choreographer.getInstance().postFrameCallback(this); }
        catch (IllegalStateException error) { report = error.getMessage(); Log.e("RE4Graphics",report); if(sceneObserver!=null)sceneObserver.accept(report); }
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
        try { report = sceneRoot==null?nativeFrame(handle,false):nativeSceneFrame(handle,
            frames==2?getContext().getFilesDir()+"/"+evidenceSession+"-gpu.rgba":null); frames++;
            if(sceneObserver!=null)sceneObserver.accept(report);
        }
        catch (IllegalStateException error) { report = error.getMessage(); active = false; Log.e("RE4Graphics",report); if(sceneObserver!=null)sceneObserver.accept(report); }
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
    private static native void nativeLoadScene(long handle,String root,String catalog,int room,String event,int frame,String model,int cut);
    private static native String nativeSceneFrame(long handle,String readback);
}
