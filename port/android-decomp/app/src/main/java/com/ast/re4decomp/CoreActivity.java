package com.ast.re4decomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.hardware.input.InputManager;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.os.VibrationAttributes;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.util.Log;
import android.view.Choreographer;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.widget.Toast;
import java.io.File;
import java.io.InputStreamReader;
import java.io.Reader;
import java.nio.charset.StandardCharsets;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Core mode: the original game core runs the game (started once per process);
 * this Activity draws its frames, passes the controller as GameCube pad 0
 * (CoreInput: layout, raw values), plays the game's sound (CoreAudio), drives
 * the vibrator from the game's rumble and prepares the room archive the game
 * enters. SELECT shows/hides the
 * diagnostic overlay (hidden at the start unless the core did not start), R3
 * saves graphics data for the diagnosis; Back ends the game (the core cannot
 * be restarted inside the same process).
 */
public final class CoreActivity extends Activity implements InputManager.InputDeviceListener {
  static final String TAG = "RE4Input";
  /** Preferences of the core mode's controller (MainActivity sets the swap). */
  static final String INPUT_PREFS = "core_input", SWAP_FACE = "swapFace";

  private final Handler main = new Handler(Looper.getMainLooper());
  private final ExecutorService rooms = Executors.newSingleThreadExecutor();
  private final CoreInput input = new CoreInput();
  private final CoreAudio audio = new CoreAudio();
  private final Set<Integer> seenDevices = new HashSet<>();
  private GLSurfaceView surface;
  private TextView overlay;
  private String startProblem = "";
  private String lastKey = "-";
  private int[] lastPad = new int[7];
  private volatile boolean destroyed;
  private boolean resumed;
  // rumble: the game's motor (CoreBridge.rumble) on the device's vibrator
  private Vibrator vibrator;
  private boolean amplitudeControl, vibrating;
  private int rumbleFrames = -1, rumbleLevel;
  private long rumbleSeen, rumbleUntil;
  private volatile int preparedRoom = -1;
  private boolean roomBusy;

  @Override
  public void onCreate(Bundle state) {
    super.onCreate(state);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    hideSystemUi();
    ImportManager imports = ((Re4Application) getApplication()).imports;
    // test switches of the core (port/scripts/test-core-app.ps1 passes them)
    String settings = getIntent().getStringExtra("coreSettings");
    // linked shader programs kept between runs (no compile stutter the second time)
    String programs = "RE4_GX_PROGRAM_CACHE=" + new File(getCacheDir(), "gx-programs").getAbsolutePath();
    settings = settings == null || settings.isEmpty() ? programs : settings + ";" + programs;
    // the app's own folder on the shared storage, where test switches write (RE4_AUDIO_WAV)
    getExternalFilesDir(null);
    startProblem = imports.root == null ? "No data folder"
        : CoreBridge.start(imports.root.getAbsolutePath(), settings);
    input.setSwapFace(getSharedPreferences(INPUT_PREFS, MODE_PRIVATE).getBoolean(SWAP_FACE, false));
    Log.i(TAG, "layout: A B X Y by label" + (input.swapFace() ? ", A/B and X/Y swapped" : "")
        + ", R2 = R, L2 = L, R1 = Z, SELECT overlay, R3 graphics capture");
    setUpVibrator();
    // the volume buttons set the game's volume
    setVolumeControlStream(AudioManager.STREAM_MUSIC);

    FrameLayout root = new FrameLayout(this);
    surface = new GLSurfaceView(this);
    surface.setEGLContextClientVersion(3);
    surface.setPreserveEGLContextOnPause(true);
    surface.setRenderer(new GLSurfaceView.Renderer() {
      @Override public void onSurfaceCreated(GL10 gl, EGLConfig config) { CoreBridge.surfaceCreated(); }
      @Override public void onSurfaceChanged(GL10 gl, int w, int h) { CoreBridge.resize(w, h); }
      @Override public void onDrawFrame(GL10 gl) { CoreBridge.draw(); }
    });
    root.addView(surface);
    overlay = new TextView(this);
    overlay.setTypeface(Typeface.MONOSPACE);
    overlay.setTextSize(11);
    overlay.setTextColor(Color.WHITE);
    overlay.setBackgroundColor(0x80000000);
    overlay.setPadding(12, 8, 12, 8);
    FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
        FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.TOP | Gravity.START);
    root.addView(overlay, lp);
    // hidden while the game runs (SELECT shows it); shown when the core did not start
    overlay.setVisibility(startProblem.isEmpty() ? View.GONE : View.VISIBLE);
    setContentView(root);
    surface.setFocusable(true);
    surface.requestFocus();
    if (Build.VERSION.SDK_INT >= 33)
      getOnBackInvokedDispatcher().registerOnBackInvokedCallback(0, this::askExit);
    main.post(this::tick);
  }

  private void hideSystemUi() {
    getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
  }

  /** Twice a second: overlay text and the room the game wants. */
  private void tick() {
    if (destroyed) return;
    String text = padText() + "\n" + CoreBridge.status();
    overlay.setText(startProblem.isEmpty() ? text : "Core not started: " + startProblem + "\n" + text);
    int room = CoreBridge.wantedRoom();
    if (room >= 0 && room != preparedRoom && !roomBusy) prepareRoom(room);
    main.postDelayed(this::tick, 500);
  }

  /** Decodes the room archive the game entered with the app's room loader. */
  private void prepareRoom(final int room) {
    roomBusy = true;
    final File root = ((Re4Application) getApplication()).imports.root;
    final File cache = getCacheDir();
    rooms.execute(() -> {
      String path = "", error = "";
      String id = String.format(Locale.ROOT, "r%03x", room);
      try (Reader rows = new InputStreamReader(getAssets().open("rooms.tsv"), StandardCharsets.UTF_8);
           Reader blocks = new InputStreamReader(getAssets().open("blocks.tsv"), StandardCharsets.UTF_8)) {
        RoomRepository repository = new RoomRepository(root, cache, rows, blocks,
            (source, output) -> NativeBridge.decodeRoom(source.getAbsolutePath(), output.getAbsolutePath()));
        path = repository.prepare(id).getAbsolutePath();
      } catch (Exception e) {
        error = id + ": " + (e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage());
      }
      CoreBridge.roomPrepared(room, path, error);
      preparedRoom = room;
      main.post(() -> roomBusy = false);
    });
  }

  private void askExit() {
    new AlertDialog.Builder(this).setMessage("End the game? (The core starts anew with the next launch.)")
        .setPositiveButton("End", (d, w) -> {
          finishAndRemoveTask();
          // the core cannot be restarted inside this process
          main.postDelayed(() -> System.exit(0), 300);
        })
        .setNegativeButton("Continue", null).show();
  }

  @Override
  @SuppressWarnings("deprecation")
  public void onBackPressed() {
    askExit();
  }

  /** The controller as the overlay shows it: last key, buttons, raw sticks and triggers. */
  private String padText() {
    int[] p = lastPad;
    return String.format(Locale.ROOT, "pad: last key %s | %s | stick %d,%d C %d,%d | L %d R %d%s",
        lastKey, CoreInput.names(p[0]), p[1], p[2], p[3], p[4], p[5], p[6],
        input.swapFace() ? " | A/B X/Y swapped" : "");
  }

  private static boolean controller(android.view.InputEvent e) {
    return (e.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
        || (e.getSource() & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
        || (e.getSource() & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD;
  }

  private void send() {
    int[] p = input.pad();
    lastPad = p;
    CoreBridge.pad(p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
  }

  /** Once per device: what it reports (name, sources, axes), for the device test's log. */
  private void noteDevice(InputDevice d) {
    if (d == null || !seenDevices.add(d.getId())) return;
    StringBuilder axes = new StringBuilder();
    for (InputDevice.MotionRange r : d.getMotionRanges()) {
      axes.append(' ').append(MotionEvent.axisToString(r.getAxis()))
          .append(String.format(Locale.ROOT, "(%.2f..%.2f flat %.3f)", r.getMin(), r.getMax(), r.getFlat()));
    }
    Log.i(TAG, String.format(Locale.ROOT, "device %d \"%s\" vendor %04x product %04x sources 0x%x%s axes:%s",
        d.getId(), d.getName(), d.getVendorId(), d.getProductId(), d.getSources(),
        d.isVirtual() ? " virtual" : "", axes.length() == 0 ? " none" : axes.toString()));
  }

  @Override
  public boolean dispatchKeyEvent(KeyEvent e) {
    int code = e.getKeyCode();
    boolean down = e.getAction() == KeyEvent.ACTION_DOWN;
    boolean first = down && e.getRepeatCount() == 0;
    int function = CoreInput.appFunction(code);
    if (function != CoreInput.APP_NONE && first) {
      String what = function == CoreInput.APP_OVERLAY ? "overlay" : "graphics capture";
      lastKey = KeyEvent.keyCodeToString(code).replace("KEYCODE_", "") + " = " + what;
      Log.i(TAG, "key " + KeyEvent.keyCodeToString(code) + " (" + code + ") -> app " + what);
    }
    switch (function) {
      case CoreInput.APP_OVERLAY:
        if (first) overlay.setVisibility(overlay.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE);
        return true;
      case CoreInput.APP_GX_DUMP:
        // R3 (right stick pressed): the graphics data of the next pictures for the
        // diagnosis in the cloud (files/gx-dumps, test-core-app.ps1 / pull-gx-dumps.ps1 fetch them)
        if (first) {
          File dir = getExternalFilesDir(null);
          if (dir != null) {
            CoreBridge.gxDump(new File(dir, "gx-dumps").getAbsolutePath());
            Toast.makeText(this, "Grafik-Aufnahme gespeichert", Toast.LENGTH_SHORT).show();
          }
        }
        return true;
      default:
        break;
    }
    if (controller(e) || KeyEvent.isGamepadButton(code)) {
      if (e.getAction() != KeyEvent.ACTION_DOWN && e.getAction() != KeyEvent.ACTION_UP) return true;
      if (e.getRepeatCount() > 0) return true;  // held: nothing changes
      if (input.key(e.getDeviceId(), code, down)) {
        if (down) {
          noteDevice(e.getDevice());
          lastKey = KeyEvent.keyCodeToString(code).replace("KEYCODE_", "") + " = "
              + CoreInput.names(input.gcButton(code));
          Log.i(TAG, "key " + KeyEvent.keyCodeToString(code) + " (" + code + ", scan " + e.getScanCode()
              + ", device " + e.getDeviceId() + ") -> GC " + CoreInput.names(input.gcButton(code)));
        }
        send();
        return true;
      }
      if (CoreInput.unused(code)) {
        if (down) {
          lastKey = KeyEvent.keyCodeToString(code).replace("KEYCODE_", "") + " = -";
          Log.i(TAG, "key " + KeyEvent.keyCodeToString(code) + " (" + code + ") -> no function");
        }
        return true;
      }
    }
    return super.dispatchKeyEvent(e);
  }

  /** An axis of `e` in -1..1 (sticks, hat) or 0..1 (triggers); 0 when the device lacks it. */
  private static float axis(MotionEvent e, InputDevice d, int a, boolean trigger) {
    InputDevice.MotionRange range = d == null ? null : d.getMotionRange(a, e.getSource());
    if (range == null || range.getRange() <= 0) return 0;
    float v = e.getAxisValue(a);
    if (Float.isNaN(v) || Float.isInfinite(v)) return 0;
    return trigger ? (v - range.getMin()) / range.getRange()
        : 2 * (v - range.getMin()) / range.getRange() - 1;
  }

  @Override
  public boolean onGenericMotionEvent(MotionEvent e) {
    if (e.getAction() == MotionEvent.ACTION_MOVE && controller(e)) {
      InputDevice d = e.getDevice();
      noteDevice(d);
      // the right stick: Z/RZ on most Android controllers, RX/RY on others
      boolean zr = d != null && d.getMotionRange(MotionEvent.AXIS_Z, e.getSource()) != null
          && d.getMotionRange(MotionEvent.AXIS_RZ, e.getSource()) != null;
      input.motion(e.getDeviceId(),
          axis(e, d, MotionEvent.AXIS_X, false), axis(e, d, MotionEvent.AXIS_Y, false),
          axis(e, d, zr ? MotionEvent.AXIS_Z : MotionEvent.AXIS_RX, false),
          axis(e, d, zr ? MotionEvent.AXIS_RZ : MotionEvent.AXIS_RY, false),
          Math.max(axis(e, d, MotionEvent.AXIS_LTRIGGER, true), axis(e, d, MotionEvent.AXIS_BRAKE, true)),
          Math.max(axis(e, d, MotionEvent.AXIS_RTRIGGER, true), axis(e, d, MotionEvent.AXIS_GAS, true)),
          axis(e, d, MotionEvent.AXIS_HAT_X, false), axis(e, d, MotionEvent.AXIS_HAT_Y, false));
      send();
      return true;
    }
    return super.onGenericMotionEvent(e);
  }

  @Override public void onInputDeviceAdded(int id) {}
  @Override public void onInputDeviceChanged(int id) {
    input.remove(id);
    seenDevices.remove(id);
    send();
  }
  @Override public void onInputDeviceRemoved(int id) {
    input.remove(id);
    send();
  }

  /** Nothing held while the game is not in front (no stuck keys after a dialog or the home screen). */
  private void releaseAll() {
    input.clear();
    send();
  }

  @Override
  public void onWindowFocusChanged(boolean focus) {
    super.onWindowFocusChanged(focus);
    if (!focus) releaseAll();
  }

  @SuppressWarnings("deprecation")
  private void setUpVibrator() {
    if (Build.VERSION.SDK_INT >= 31) {
      VibratorManager m = (VibratorManager) getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
      vibrator = m == null ? null : m.getDefaultVibrator();
    } else {
      vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
    }
    if (vibrator != null && !vibrator.hasVibrator()) vibrator = null;
    amplitudeControl = vibrator != null && Build.VERSION.SDK_INT >= 26 && vibrator.hasAmplitudeControl();
    Log.i(TAG, vibrator == null ? "rumble: no vibrator"
        : "rumble: vibrator" + (amplitudeControl ? " with amplitude control" : " (on/off only)"));
  }

  /**
   * About every 16 ms while the game is in front: the motor of the last four
   * game frames (the game pulses it for weaker rumble) as the vibrator's
   * strength; off when the game stands still.
   */
  private final Runnable rumbleTick = new Runnable() {
    @Override public void run() {
      if (destroyed || !resumed) return;
      int r = CoreBridge.rumble();
      long now = SystemClock.uptimeMillis();
      int frames = r >>> 8;
      if (frames != rumbleFrames) {
        rumbleFrames = frames;
        rumbleSeen = now;
      }
      int level = now - rumbleSeen < 200 ? Integer.bitCount(r & 0xF) : 0;
      if (level == 0) {
        stopVibrator();
      } else if (!vibrating || level != rumbleLevel || now >= rumbleUntil - 40) {
        vibrate(level);
        rumbleLevel = level;
        rumbleUntil = now + 120;
        vibrating = true;
      }
      main.postDelayed(this, 16);
    }
  };

  @SuppressWarnings("deprecation")
  private void vibrate(int level) {
    if (vibrator == null) return;
    try {
      if (Build.VERSION.SDK_INT >= 26) {
        int strength = amplitudeControl ? 70 + 185 * level / 4 : VibrationEffect.DEFAULT_AMPLITUDE;
        VibrationEffect effect = VibrationEffect.createOneShot(120, strength);
        if (Build.VERSION.SDK_INT >= 33) {
          vibrator.vibrate(effect, VibrationAttributes.createForUsage(VibrationAttributes.USAGE_MEDIA));
        } else {
          vibrator.vibrate(effect, new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).build());
        }
      } else {
        vibrator.vibrate(120);
      }
    } catch (RuntimeException e) {
      Log.w(TAG, "rumble: " + e);
      vibrator = null;
    }
  }

  private void stopVibrator() {
    if (vibrating && vibrator != null) vibrator.cancel();
    vibrating = false;
    rumbleLevel = 0;
  }

  // every vsync of the display while the game is shown: the core's retraces
  // follow it (runtime/vi.cpp), so the pictures do not drift against 60 Hz
  private final Choreographer.FrameCallback vsyncTick = new Choreographer.FrameCallback() {
    @Override public void doFrame(long frameTimeNanos) {
      if (!resumed) return;
      CoreBridge.vsync(frameTimeNanos);
      Choreographer.getInstance().postFrameCallback(this);
    }
  };

  @Override
  protected void onResume() {
    super.onResume();
    hideSystemUi();
    surface.onResume();
    resumed = true;
    Choreographer.getInstance().removeFrameCallback(vsyncTick);
    Choreographer.getInstance().postFrameCallback(vsyncTick);
    InputManager im = (InputManager) getSystemService(Context.INPUT_SERVICE);
    if (im != null) im.registerInputDeviceListener(this, main);
    main.removeCallbacks(rumbleTick);
    main.post(rumbleTick);
    audio.start();
  }

  @Override
  protected void onPause() {
    resumed = false;
    Choreographer.getInstance().removeFrameCallback(vsyncTick);
    // the sound pauses with the app (the core skips what it played meanwhile)
    audio.stop();
    main.removeCallbacks(rumbleTick);
    stopVibrator();
    InputManager im = (InputManager) getSystemService(Context.INPUT_SERVICE);
    if (im != null) im.unregisterInputDeviceListener(this);
    releaseAll();
    surface.onPause();
    super.onPause();
  }

  @Override
  protected void onDestroy() {
    destroyed = true;
    main.removeCallbacksAndMessages(null);
    rooms.shutdownNow();
    super.onDestroy();
  }
}
