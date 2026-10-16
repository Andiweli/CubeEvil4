package com.ast.re4decomp;

import java.util.HashMap;
import java.util.Map;

/**
 * The controller of the core mode: Android keys and axes become GameCube
 * controller 0 of the original game (port/whole/runtime/pad.cpp). The values
 * are raw like the hardware's; the game applies the SDK's dead zones and
 * ranges itself (PADClamp: sticks 15, triggers 30). Independent of Android so
 * the host tests can check it (tests/CoreInputTests.java).
 *
 * Layout (decision of the project owner, 16.10.2026): A, B, X, Y by their
 * labels (a switch swaps A/B and X/Y for controllers that send the other
 * codes), D-pad, START; R2 = R (ready weapon, analog), L2 = L (knife, analog),
 * R1 = Z (map), L1 free; left stick = control stick, right stick = C stick.
 * SELECT (diagnostic overlay) and R3 (graphics capture) belong to the app.
 */
public final class CoreInput {
  // GameCube pad bits (PAD_BUTTON_*)
  public static final int GC_LEFT = 0x0001, GC_RIGHT = 0x0002, GC_DOWN = 0x0004, GC_UP = 0x0008,
      GC_Z = 0x0010, GC_R = 0x0020, GC_L = 0x0040, GC_A = 0x0100, GC_B = 0x0200, GC_X = 0x0400,
      GC_Y = 0x0800, GC_START = 0x1000;
  // what a key does for the app instead of the game
  public static final int APP_NONE = 0, APP_OVERLAY = 1, APP_GX_DUMP = 2;

  // android.view.KeyEvent key codes
  static final int KEYCODE_DPAD_UP = 19, KEYCODE_DPAD_DOWN = 20, KEYCODE_DPAD_LEFT = 21,
      KEYCODE_DPAD_RIGHT = 22, KEYCODE_BUTTON_A = 96, KEYCODE_BUTTON_B = 97,
      KEYCODE_BUTTON_X = 99, KEYCODE_BUTTON_Y = 100, KEYCODE_BUTTON_L1 = 102,
      KEYCODE_BUTTON_R1 = 103, KEYCODE_BUTTON_L2 = 104, KEYCODE_BUTTON_R2 = 105,
      KEYCODE_BUTTON_THUMBL = 106, KEYCODE_BUTTON_THUMBR = 107, KEYCODE_BUTTON_START = 108,
      KEYCODE_BUTTON_SELECT = 109;

  /**
   * Deflection of a genuine controller at its gate in raw units: main stick
   * about 101, C stick about 92 (averages over real controllers, 0.794 and
   * 0.722 of 127). Full deflection then reaches the clamp (72 / 59) as on the
   * GameCube.
   */
  static final float MAIN_RAW = 101f, C_RAW = 92f;
  /** Where an analog trigger also presses the digital L/R (the click of a GameCube trigger). */
  static final float TRIGGER_CLICK = 0.85f;

  /** One input device (the built-in controller may report keys and axes separately). */
  private static final class Device {
    int buttons;            // GameCube bits held through keys
    boolean l2Key, r2Key;   // L2/R2 as keys (triggers without an axis)
    int hats;               // D-pad from the hat axes
    float lx, ly, rx, ry;   // sticks -1..1, y down as Android reports it
    float lt, rt;           // analog triggers 0..1
  }

  private final Map<Integer, Device> devices = new HashMap<>();
  private boolean swapFace;

  /** Swaps A/B and X/Y (controllers that send the codes of the other layout). */
  public void setSwapFace(boolean swap) {
    if (swap != swapFace) clear();
    swapFace = swap;
  }

  public boolean swapFace() { return swapFace; }

  /** The GameCube button of an Android key, 0 for none (L2/R2: the triggers). */
  public int gcButton(int keyCode) {
    switch (keyCode) {
      case KEYCODE_BUTTON_A: return swapFace ? GC_B : GC_A;
      case KEYCODE_BUTTON_B: return swapFace ? GC_A : GC_B;
      case KEYCODE_BUTTON_X: return swapFace ? GC_Y : GC_X;
      case KEYCODE_BUTTON_Y: return swapFace ? GC_X : GC_Y;
      case KEYCODE_BUTTON_R1: return GC_Z;
      case KEYCODE_BUTTON_L2: return GC_L;
      case KEYCODE_BUTTON_R2: return GC_R;
      case KEYCODE_BUTTON_START: return GC_START;
      case KEYCODE_DPAD_UP: return GC_UP;
      case KEYCODE_DPAD_DOWN: return GC_DOWN;
      case KEYCODE_DPAD_LEFT: return GC_LEFT;
      case KEYCODE_DPAD_RIGHT: return GC_RIGHT;
      default: return 0;
    }
  }

  /** What a key does for the app (overlay, graphics capture), APP_NONE for the rest. */
  public static int appFunction(int keyCode) {
    switch (keyCode) {
      case KEYCODE_BUTTON_SELECT: return APP_OVERLAY;
      case KEYCODE_BUTTON_THUMBR: return APP_GX_DUMP;
      default: return APP_NONE;
    }
  }

  /**
   * Keys the game controller takes without a function (L1, L3): consumed so
   * Android does not act on them.
   */
  public static boolean unused(int keyCode) {
    return keyCode == KEYCODE_BUTTON_L1 || keyCode == KEYCODE_BUTTON_THUMBL;
  }

  private Device device(int id) {
    Device d = devices.get(id);
    if (d == null) {
      d = new Device();
      devices.put(id, d);
    }
    return d;
  }

  /** A key of `device` went down or up; false when it is no game button. */
  public boolean key(int device, int keyCode, boolean down) {
    Device d = device(device);
    if (keyCode == KEYCODE_BUTTON_L2) {
      d.l2Key = down;
      return true;
    }
    if (keyCode == KEYCODE_BUTTON_R2) {
      d.r2Key = down;
      return true;
    }
    int bit = gcButton(keyCode);
    if (bit == 0) return false;
    d.buttons = down ? d.buttons | bit : d.buttons & ~bit;
    return true;
  }

  /**
   * The axes of `device`: sticks -1..1 (y down, as Android reports it),
   * triggers 0..1, hat -1..1; 0 for axes the device does not have.
   */
  public void motion(int device, float lx, float ly, float rx, float ry, float lt, float rt,
      float hatX, float hatY) {
    Device d = device(device);
    d.lx = clamp(lx, -1, 1);
    d.ly = clamp(ly, -1, 1);
    d.rx = clamp(rx, -1, 1);
    d.ry = clamp(ry, -1, 1);
    d.lt = clamp(lt, 0, 1);
    d.rt = clamp(rt, 0, 1);
    d.hats = (hatY < -0.5f ? GC_UP : 0) | (hatY > 0.5f ? GC_DOWN : 0)
        | (hatX < -0.5f ? GC_LEFT : 0) | (hatX > 0.5f ? GC_RIGHT : 0);
  }

  /** A device went away: its keys count as released. */
  public void remove(int device) { devices.remove(device); }

  /** Everything released (the app lost the focus, the layout changed). */
  public void clear() { devices.clear(); }

  /**
   * The GameCube controller for the core: buttons, main stick x/y, C stick
   * x/y (raw, up positive), triggers L/R (0..255). Buttons of all devices
   * count; of each stick and trigger the device that moves it furthest.
   */
  public int[] pad() {
    int buttons = 0;
    float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
    for (Device d : devices.values()) {
      buttons |= d.buttons | d.hats;
      if (d.lx * d.lx + d.ly * d.ly > lx * lx + ly * ly) {
        lx = d.lx;
        ly = d.ly;
      }
      if (d.rx * d.rx + d.ry * d.ry > rx * rx + ry * ry) {
        rx = d.rx;
        ry = d.ry;
      }
      lt = Math.max(lt, d.l2Key ? 1f : d.lt);
      rt = Math.max(rt, d.r2Key ? 1f : d.rt);
    }
    // the triggers' digital L/R: only at the end stop, the game reads the analog
    // value (PadRead: any value past the dead zone counts as L/R)
    if (lt >= TRIGGER_CLICK) buttons |= GC_L;
    if (rt >= TRIGGER_CLICK) buttons |= GC_R;
    return new int[] {buttons, raw(lx, MAIN_RAW), raw(-ly, MAIN_RAW), raw(rx, C_RAW),
        raw(-ry, C_RAW), Math.round(lt * 255f), Math.round(rt * 255f)};
  }

  private static int raw(float v, float range) {
    return Math.round(v * range);
  }

  private static float clamp(float v, float min, float max) {
    if (Float.isNaN(v) || Float.isInfinite(v)) return 0;
    return Math.max(min, Math.min(max, v));
  }

  /** GameCube buttons as text ("A+B"), for the overlay and the log. */
  public static String names(int bits) {
    StringBuilder s = new StringBuilder();
    String[] names = {"LEFT", "RIGHT", "DOWN", "UP", "Z", "R", "L", "", "A", "B", "X", "Y", "START"};
    for (int i = 0; i < names.length; i++) {
      if ((bits & (1 << i)) != 0 && !names[i].isEmpty()) {
        if (s.length() > 0) s.append('+');
        s.append(names[i]);
      }
    }
    return s.length() == 0 ? "-" : s.toString();
  }
}
