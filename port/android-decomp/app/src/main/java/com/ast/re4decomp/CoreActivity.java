package com.ast.re4decomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.graphics.Color;
import android.graphics.Typeface;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
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
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Core mode: the original game core runs the game (started once per process);
 * this Activity draws its frames, passes the controller as GameCube pad 0 and
 * prepares the room archive the game enters. SELECT shows/hides the
 * diagnostic overlay (hidden at the start unless the core did not start); Back ends the game (the core cannot be restarted inside
 * the same process).
 */
public final class CoreActivity extends Activity {
  // GameCube pad bits (PAD_BUTTON_*)
  static final int GC_LEFT = 0x0001, GC_RIGHT = 0x0002, GC_DOWN = 0x0004, GC_UP = 0x0008,
      GC_Z = 0x0010, GC_R = 0x0020, GC_L = 0x0040, GC_A = 0x0100, GC_B = 0x0200, GC_X = 0x0400,
      GC_Y = 0x0800, GC_START = 0x1000;

  private final Handler main = new Handler(Looper.getMainLooper());
  private final ExecutorService rooms = Executors.newSingleThreadExecutor();
  private GLSurfaceView surface;
  private TextView overlay;
  private String startProblem = "";
  private int buttons, hats;
  private float lx, ly, rx, ry, lt, rt;
  private volatile boolean destroyed;
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
    startProblem = imports.root == null ? "No data folder"
        : CoreBridge.start(imports.root.getAbsolutePath(), settings);

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
    String text = CoreBridge.status();
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

  private static boolean controller(android.view.InputEvent e) {
    return (e.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
        || (e.getSource() & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
        || (e.getSource() & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD;
  }

  /** Android gamepad keys to GameCube buttons (milestone 7 refines the layout). */
  static int gcButton(int keyCode) {
    switch (keyCode) {
      case KeyEvent.KEYCODE_BUTTON_A: return GC_A;
      case KeyEvent.KEYCODE_BUTTON_B: return GC_B;
      case KeyEvent.KEYCODE_BUTTON_X: return GC_X;
      case KeyEvent.KEYCODE_BUTTON_Y: return GC_Y;
      case KeyEvent.KEYCODE_BUTTON_L1: return GC_L;
      case KeyEvent.KEYCODE_BUTTON_R1: return GC_Z;
      case KeyEvent.KEYCODE_BUTTON_L2: return GC_L;
      case KeyEvent.KEYCODE_BUTTON_R2: return GC_R;
      case KeyEvent.KEYCODE_BUTTON_START: return GC_START;
      case KeyEvent.KEYCODE_DPAD_UP: return GC_UP;
      case KeyEvent.KEYCODE_DPAD_DOWN: return GC_DOWN;
      case KeyEvent.KEYCODE_DPAD_LEFT: return GC_LEFT;
      case KeyEvent.KEYCODE_DPAD_RIGHT: return GC_RIGHT;
      default: return 0;
    }
  }

  /** Stick -1..1 to the GameCube range (a real pad reaches about +-100). */
  static int gcStick(float v) {
    return Math.round(Math.max(-1f, Math.min(1f, v)) * 100f);
  }

  private void send() {
    int b = buttons | hats;
    // analog triggers also press the digital L/R at their end stop, as on the GameCube
    if (lt > 0.85f) b |= GC_L;
    if (rt > 0.85f) b |= GC_R;
    CoreBridge.pad(b, gcStick(lx), gcStick(-ly), gcStick(rx), gcStick(-ry),
        Math.round(Math.max(lt, (buttons & GC_L) != 0 ? 1f : 0f) * 255f),
        Math.round(Math.max(rt, (buttons & GC_R) != 0 ? 1f : 0f) * 255f));
  }

  @Override
  public boolean dispatchKeyEvent(KeyEvent e) {
    if (e.getKeyCode() == KeyEvent.KEYCODE_BUTTON_SELECT) {
      if (e.getAction() == KeyEvent.ACTION_DOWN && e.getRepeatCount() == 0)
        overlay.setVisibility(overlay.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE);
      return true;
    }
    // R3 (right stick pressed): the graphics data of the next picture for the
    // diagnosis in the cloud (files/gx-dumps, test-core-app.ps1 / pull-gx-dumps.ps1 fetch them)
    if (e.getKeyCode() == KeyEvent.KEYCODE_BUTTON_THUMBR) {
      if (e.getAction() == KeyEvent.ACTION_DOWN && e.getRepeatCount() == 0) {
        File dir = getExternalFilesDir(null);
        if (dir != null) {
          CoreBridge.gxDump(new File(dir, "gx-dumps").getAbsolutePath());
          Toast.makeText(this, "Grafik-Aufnahme gespeichert", Toast.LENGTH_SHORT).show();
        }
      }
      return true;
    }
    int bit = gcButton(e.getKeyCode());
    if (bit != 0 && controller(e)) {
      if (e.getAction() == KeyEvent.ACTION_DOWN) buttons |= bit;
      else if (e.getAction() == KeyEvent.ACTION_UP) buttons &= ~bit;
      send();
      return true;
    }
    return super.dispatchKeyEvent(e);
  }

  private static float axis(MotionEvent e, int a) {
    float v = e.getAxisValue(a);
    return Float.isNaN(v) || Float.isInfinite(v) ? 0 : v;
  }

  @Override
  public boolean onGenericMotionEvent(MotionEvent e) {
    if (e.getAction() == MotionEvent.ACTION_MOVE && controller(e)) {
      InputDevice d = e.getDevice();
      boolean zr = d != null && d.getMotionRange(MotionEvent.AXIS_Z, e.getSource()) != null
          && d.getMotionRange(MotionEvent.AXIS_RZ, e.getSource()) != null;
      lx = axis(e, MotionEvent.AXIS_X);
      ly = axis(e, MotionEvent.AXIS_Y);
      rx = axis(e, zr ? MotionEvent.AXIS_Z : MotionEvent.AXIS_RX);
      ry = axis(e, zr ? MotionEvent.AXIS_RZ : MotionEvent.AXIS_RY);
      lt = Math.max(axis(e, MotionEvent.AXIS_LTRIGGER), axis(e, MotionEvent.AXIS_BRAKE));
      rt = Math.max(axis(e, MotionEvent.AXIS_RTRIGGER), axis(e, MotionEvent.AXIS_GAS));
      float hx = axis(e, MotionEvent.AXIS_HAT_X), hy = axis(e, MotionEvent.AXIS_HAT_Y);
      hats = (hy < -0.5f ? GC_UP : 0) | (hy > 0.5f ? GC_DOWN : 0) | (hx < -0.5f ? GC_LEFT : 0)
          | (hx > 0.5f ? GC_RIGHT : 0);
      send();
      return true;
    }
    return super.onGenericMotionEvent(e);
  }

  @Override
  protected void onResume() {
    super.onResume();
    hideSystemUi();
    surface.onResume();
  }

  @Override
  protected void onPause() {
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
