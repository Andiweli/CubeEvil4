package org.re4.port;

import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Choreographer;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

/** One owner-thread input domain. Android events update raw state; original PadRead
 * samples it once per rational 30-Hz gameplay tick. No Android key auto-repeat logic. */
public final class ControllerLoop implements AutoCloseable, Choreographer.FrameCallback,
        InputManager.InputDeviceListener {
    static { System.loadLibrary("re4_port_jni"); }
    private final InputManager manager;
    private final Map<Integer, Integer> ports = new HashMap<>();
    private long handle;
    private boolean running;
    private String previous = "";
    public interface Observer { void accept(String value); }
    private Observer observer;

    public ControllerLoop(Context context) {
        owner();
        manager = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
        handle = nativeOpen();
        manager.registerInputDeviceListener(this, new Handler(Looper.getMainLooper()));
        for (int id : InputDevice.getDeviceIds()) add(id);
    }
    private static void owner() {
        if (Looper.myLooper() != Looper.getMainLooper()) throw new IllegalStateException("Controller main owner thread required");
    }
    private void live() { owner(); if (handle == 0) throw new IllegalStateException("Closed controller"); }
    private static boolean controller(InputDevice device) {
        return device != null && !device.isVirtual() &&
            (device.supportsSource(InputDevice.SOURCE_GAMEPAD) || device.supportsSource(InputDevice.SOURCE_JOYSTICK));
    }
    private void add(int id) {
        InputDevice device = InputDevice.getDevice(id);
        if (!controller(device) || ports.containsKey(id)) return;
        int port = 0;
        while (port < 4 && ports.containsValue(port)) ++port;
        if (port == 4) return;
        ports.put(id,port); connectDevice(device,port);
        Log.i("RE4ControllerDevice", "id=" + id + " port=" + port + " name=" + device.getName() +
            " descriptor=" + device.getDescriptor() + " sources=" + device.getSources());
        for (InputDevice.MotionRange range : device.getMotionRanges())
            Log.i("RE4ControllerDevice", "axis=" + MotionEvent.axisToString(range.getAxis()) +
                " min=" + range.getMin() + " max=" + range.getMax() + " flat=" + range.getFlat());
    }
    private void connectDevice(InputDevice device,int port) {
        boolean analogLeft = device.getMotionRange(MotionEvent.AXIS_LTRIGGER) != null || device.getMotionRange(MotionEvent.AXIS_BRAKE) != null;
        boolean analogRight = device.getMotionRange(MotionEvent.AXIS_RTRIGGER) != null || device.getMotionRange(MotionEvent.AXIS_GAS) != null;
        nativeConnect(handle,device.getId(),port,analogLeft,analogRight);
    }
    @Override public void onInputDeviceAdded(int id) { live(); add(id); }
    @Override public void onInputDeviceRemoved(int id) {
        live(); if (ports.remove(id) != null) nativeDisconnect(handle,id);
    }
    @Override public void onInputDeviceChanged(int id) {
        live(); Integer old = ports.get(id);
        InputDevice device = InputDevice.getDevice(id);
        if (old != null && controller(device)) connectDevice(device,old);
        else { onInputDeviceRemoved(id); add(id); }
    }
    public void setObserver(Observer value) { live(); observer = value; }
    public boolean key(KeyEvent event) {
        live();
        if (!running || !ports.containsKey(event.getDeviceId()) ||
            !(event.isFromSource(InputDevice.SOURCE_GAMEPAD) || event.isFromSource(InputDevice.SOURCE_DPAD))) return false;
        if (event.getAction() != KeyEvent.ACTION_DOWN && event.getAction() != KeyEvent.ACTION_UP) return false;
        boolean used = nativeButton(handle,event.getDeviceId(),event.getKeyCode(),event.getAction() == KeyEvent.ACTION_DOWN);
        if (used && event.getRepeatCount() == 0)
            Log.i("RE4ControllerRaw", "device=" + event.getDeviceId() + " key=" + KeyEvent.keyCodeToString(event.getKeyCode()) +
                " action=" + event.getAction() + " scan=" + event.getScanCode());
        return used;
    }
    static float normalize(float value, float min, float max, float flat, boolean trigger) {
        if (!finite(value) || !finite(min) || !finite(max) || max <= min || !finite(flat)) return 0;
        float center = trigger ? min : (min + max) / 2;
        if (Math.abs(value-center) <= Math.max(0,flat)) return 0;
        float extent = trigger ? max-min : (value >= center ? max-center : center-min);
        return Math.max(trigger ? 0 : -1,Math.min(1,(value-center)/extent));
    }
    private static boolean finite(float value) { return !Float.isNaN(value) && !Float.isInfinite(value); }
    private static float axis(MotionEvent event,InputDevice device,int axis,boolean trigger) {
        InputDevice.MotionRange range = device.getMotionRange(axis,event.getSource());
        if (range == null) range = device.getMotionRange(axis);
        if (range == null) return 0;
        return normalize(event.getAxisValue(axis),range.getMin(),range.getMax(),range.getFlat(),trigger);
    }
    public boolean motion(MotionEvent event) {
        live();
        if (!running || event.getActionMasked() != MotionEvent.ACTION_MOVE ||
            !event.isFromSource(InputDevice.SOURCE_JOYSTICK) || !ports.containsKey(event.getDeviceId())) return false;
        InputDevice device = event.getDevice(); if (!controller(device)) return false;
        int rightX = device.getMotionRange(MotionEvent.AXIS_Z) != null ? MotionEvent.AXIS_Z : MotionEvent.AXIS_RX;
        int rightY = device.getMotionRange(MotionEvent.AXIS_RZ) != null ? MotionEvent.AXIS_RZ : MotionEvent.AXIS_RY;
        float x=axis(event,device,MotionEvent.AXIS_X,false), y=axis(event,device,MotionEvent.AXIS_Y,false);
        float z=axis(event,device,rightX,false), rz=axis(event,device,rightY,false);
        float hx=axis(event,device,MotionEvent.AXIS_HAT_X,false), hy=axis(event,device,MotionEvent.AXIS_HAT_Y,false);
        float lt=Math.max(axis(event,device,MotionEvent.AXIS_LTRIGGER,true),axis(event,device,MotionEvent.AXIS_BRAKE,true));
        float rt=Math.max(axis(event,device,MotionEvent.AXIS_RTRIGGER,true),axis(event,device,MotionEvent.AXIS_GAS,true));
        boolean used = nativeAxes(handle,event.getDeviceId(),x,y,z,rz,hx,hy,lt,rt);
        if (used) Log.i("RE4ControllerRaw", "device=" + event.getDeviceId() + " axes=" + x + "," + y + "," + z + "," + rz + "," + hx + "," + hy + "," + lt + "," + rt);
        return used;
    }
    public void start() {
        live(); if (running) return;
        nativeRunning(handle,true); running = true; previous = "";
        Choreographer.getInstance().postFrameCallback(this);
    }
    public void stop() {
        live(); running = false; Choreographer.getInstance().removeFrameCallback(this);
        nativeRunning(handle,false);
    }
    @Override public void doFrame(long ns) {
        live(); if (!running) return;
        if (nativeFrame(handle,ns) != 0) {
            String snapshot = nativeSnapshot(handle);
            if (!snapshot.equals(previous)) {
                previous = snapshot; Log.i("RE4ControllerNative",snapshot);
                if (observer != null) observer.accept(snapshot);
            }
        }
        Choreographer.getInstance().postFrameCallback(this);
    }
    public String snapshot() { live(); return nativeSnapshot(handle); }
    public void configureGame(String root,String catalog,int disc) {
        live(); if (running) throw new IllegalStateException("Stop controller before configuring game runtime");
        nativeConfigureGame(handle,Objects.requireNonNull(root),Objects.requireNonNull(catalog),disc);
    }
    // Isolated native adapter tests, explicitly not physical input evidence.
    int frameForTest(long ns) { live(); return nativeFrame(handle,ns); }
    void connectForTest(int id,int port) { live(); nativeConnect(handle,id,port,true,true); }
    boolean buttonForTest(int id,int key,boolean down) { live(); return nativeButton(handle,id,key,down); }
    void disconnectForTest(int id) { live(); nativeDisconnect(handle,id); }
    @Override public void close() {
        live(); stop(); manager.unregisterInputDeviceListener(this); nativeClose(handle); handle = 0; ports.clear();
    }
    private static native long nativeOpen();
    private static native void nativeClose(long handle);
    private static native void nativeConnect(long handle,int device,int port,boolean analogLeft,boolean analogRight);
    private static native void nativeDisconnect(long handle,int device);
    private static native boolean nativeButton(long handle,int device,int key,boolean down);
    private static native boolean nativeAxes(long handle,int device,float x,float y,float z,float rz,float hx,float hy,float lt,float rt);
    private static native void nativeRunning(long handle,boolean running);
    private static native int nativeFrame(long handle,long ns);
    private static native String nativeSnapshot(long handle);
    private static native void nativeConfigureGame(long handle,String root,String catalog,int disc);
}
