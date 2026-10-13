package org.re4.port;

import android.os.Looper;
import android.view.Choreographer;
import java.util.Objects;

/** Owner-thread loader/VI bridge. Data and catalog must live in app-readable private storage.
 * No assets, room reconstruction, renderer or audio implementation is supplied by this class. */
public final class LoaderFrameLoop implements AutoCloseable, Choreographer.FrameCallback {
    static { System.loadLibrary("re4_port_jni"); }
    private long handle;
    private boolean running;

    public LoaderFrameLoop(String dataRoot, String catalog, int disc) {
        owner();
        handle = nativeOpen(Objects.requireNonNull(dataRoot), Objects.requireNonNull(catalog), disc);
    }
    private static void owner() {
        if (Looper.myLooper() != Looper.getMainLooper()) {
            throw new IllegalStateException("Loader and Choreographer require the main owner thread");
        }
    }
    private void live() { owner(); if (handle == 0) throw new IllegalStateException("Closed loader"); }
    public void start() {
        live();
        if (!running) { running = true; Choreographer.getInstance().postFrameCallback(this); }
    }
    public void stop() {
        live(); running = false;
        Choreographer.getInstance().removeFrameCallback(this);
        nativeReset(handle);
    }
    @Override public void doFrame(long frameTimeNanos) {
        live();
        if (!running) return;
        nativeFrame(handle, frameTimeNanos);
        Choreographer.getInstance().postFrameCallback(this);
    }
    public boolean loadRoom(int roomId) { live(); return nativeLoadRoom(handle, roomId); }
    public long[] timing() { live(); return nativeTiming(handle); }
    // Deterministic adapter verification uses the same entry point as Choreographer.
    int frameForTest(long timeNanos) { live(); return nativeFrame(handle, timeNanos); }
    @Override public void close() {
        live(); stop(); nativeClose(handle); handle = 0;
    }
    private static native long nativeOpen(String root, String catalog, int disc);
    private static native void nativeClose(long handle);
    private static native void nativeReset(long handle);
    private static native int nativeFrame(long handle, long timeNanos);
    private static native boolean nativeLoadRoom(long handle, int room);
    private static native long[] nativeTiming(long handle);
}
