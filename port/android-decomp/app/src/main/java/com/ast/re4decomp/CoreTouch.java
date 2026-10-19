package com.ast.re4decomp;

import java.util.HashSet;
import java.util.Set;

/**
 * The touch controls of the core mode (RetroTouch, decision of the project
 * owner, 19.10.2026): its callbacks become GameCube controller 0, a device of
 * CoreInput of its own. Independent of Android and RetroTouch so the host
 * tests can check it (tests/CoreTouchTests.java); TouchControls puts RetroTouch
 * on the screen and calls this class.
 *
 * One button per controller function with its label (A, B, X, Y, Z, L, R,
 * START), the D-pad (also for the game's menus), the move stick as the control
 * stick and the right look zone as the C stick (decision of the project owner,
 * 19.10.2026); MENU is the app's SELECT (pressed: the app's menu, held for a
 * second: the game's debug menu). L and R stay pressed after a tap until the
 * next tap: one thumb cannot hold R (ready the weapon) and tap A (fire).
 *
 * The look zone reports how far a finger moved, not where it is. The C stick
 * is the way the finger went since it went down (LOOK_RANGE of the screen's
 * shorter side tilts it fully; a longer way moves the centre along) and goes
 * back when the finger lifts. RetroTouch does not say which finger looks, so
 * TouchControls names the fingers that went down inside the look zone without
 * pressing a control (fingerDown).
 */
final class CoreTouch {
  // the actions registered with RetroTouch (the layout's contract: lower case, never renamed)
  static final String A = "a", B = "b", X = "x", Y = "y", Z = "z", L = "l", R = "r",
      START = "start", MENU = "menu";
  // the D-pad's directions (RetroTouchNavigation)
  static final String UP = "nav_up", DOWN = "nav_down", LEFT = "nav_left", RIGHT = "nav_right";
  /** This way of a looking finger (share of the screen's shorter side) tilts the C stick fully. */
  static final float LOOK_RANGE = 0.15f;

  /** The buttons in the editor's order: action, label (short: it has to fit the button), colour (0: RetroTouch's grey). */
  static final String[] ACTIONS = {A, B, X, Y, Z, L, R, START, MENU};
  static final String[] LABELS = {"A", "B", "X", "Y", "Z", "L", "R", "START", "MENU"};
  // the GameCube's official colours (the project owner's palette, 19.10.2026): A green, B red,
  // Z purple, the others grey (RetroTouch writes dark labels on the light grey)
  static final int GC_GREEN = rgb(0x10, 0x9B, 0x61), GC_RED = rgb(0xF6, 0x39, 0x2C), GC_PURPLE = rgb(0x6B, 0x5C, 0xB1),
      GC_GREY = rgb(0xDD, 0xDD, 0xDD);
  static final int[] COLOURS = {GC_GREEN, GC_RED, GC_GREY, GC_GREY, GC_PURPLE, GC_GREY, GC_GREY, GC_GREY, GC_GREY};

  // the kinds of RetroTouchControl
  static final int BUTTON = 0, STICK = 1, LOOK = 2, DPAD = 3;

  /** A control of the default layout: centre and size in shares of the screen (RetroTouchControl). */
  static final class Control {
    final int kind;
    final String id, action;
    final float x, y, width, height;

    Control(int kind, String id, String action, float x, float y, float width, float height) {
      this.kind = kind;
      this.id = id;
      this.action = action;
      this.x = x;
      this.y = y;
      this.width = width;
      this.height = height;
    }

    static Control button(String action, float x, float y, float size) {
      return new Control(BUTTON, action, action, x, y, size, size);
    }
  }

  /**
   * The default layout, for landscape screens of 4:3 to 20:9: left the D-pad
   * above the move stick and L; right the face buttons as on the GameCube (A
   * large, B below left, X right, Y above), Z and R above them; MENU and START
   * at the top centre (the subtitles are at the bottom). The look zone is the
   * right half; the buttons in it come first. RetroTouch's gear (top right)
   * opens its editor, where the player moves, resizes and saves the layout.
   */
  static final Control[] LAYOUT = {
      new Control(LOOK, "look", "", 0.75f, 0.50f, 0.50f, 1.00f),
      new Control(STICK, "stick", "", 0.14f, 0.70f, 0.30f, 0.30f),
      new Control(DPAD, "dpad", "", 0.11f, 0.36f, 0.20f, 0.20f),
      Control.button(L, 0.14f, 0.12f, 0.13f),
      Control.button(R, 0.84f, 0.12f, 0.13f),
      Control.button(Z, 0.93f, 0.30f, 0.09f),
      Control.button(Y, 0.80f, 0.46f, 0.11f),
      Control.button(X, 0.955f, 0.50f, 0.11f),
      Control.button(A, 0.875f, 0.68f, 0.17f),
      Control.button(B, 0.76f, 0.86f, 0.11f),
      Control.button(MENU, 0.42f, 0.08f, 0.085f),
      Control.button(START, 0.58f, 0.08f, 0.085f),
  };

  interface Host {
    /** MENU went down or up: the app's SELECT. */
    void select(boolean down);
    /** A tap latched L or R or let it go (RetroTouch shows a latched button stronger). */
    void latched(String action, boolean on);
  }

  private final CoreInput input;
  private final Host host;
  private boolean l, r;
  // the way the looking fingers went since the first of them went down (shares of the shorter side)
  private float lookX, lookY;
  private final Set<Integer> lookFingers = new HashSet<>();
  // a control answered the finger of the touch event being dispatched (beginEvent .. fingerDown)
  private boolean answered;

  CoreTouch(CoreInput input, Host host) {
    this.input = input;
    this.host = host;
  }

  static int rgb(int red, int green, int blue) {
    return 0xFF000000 | red << 16 | green << 8 | blue;
  }

  /** The label of an action, "" for none. */
  static String label(String action) {
    for (int i = 0; i < ACTIONS.length; i++) if (ACTIONS[i].equals(action)) return LABELS[i];
    return "";
  }

  /** The GameCube button of an action (the D-pad's directions too), 0 for none. */
  static int gcButton(String action) {
    switch (action) {
      case A: return CoreInput.GC_A;
      case B: return CoreInput.GC_B;
      case X: return CoreInput.GC_X;
      case Y: return CoreInput.GC_Y;
      case Z: return CoreInput.GC_Z;
      case L: return CoreInput.GC_L;
      case R: return CoreInput.GC_R;
      case START: return CoreInput.GC_START;
      case UP: return CoreInput.GC_UP;
      case DOWN: return CoreInput.GC_DOWN;
      case LEFT: return CoreInput.GC_LEFT;
      case RIGHT: return CoreInput.GC_RIGHT;
      default: return 0;
    }
  }

  /** RetroTouch's onAction: a button or a direction of the D-pad went down or up. */
  void action(String action, boolean down) {
    if (down) answered = true;
    if (MENU.equals(action)) {
      host.select(down);
    } else if (L.equals(action) || R.equals(action)) {
      if (!down) return;
      boolean on = L.equals(action) ? (l = !l) : (r = !r);
      input.touchButton(gcButton(action), on);
      host.latched(action, on);
    } else {
      int bit = gcButton(action);
      if (bit != 0) input.touchButton(bit, down);
    }
  }

  /** RetroTouch's onMove: the move stick, -1..1 with y down. */
  void move(float x, float y) {
    answered = true;
    input.touchStick(false, x, y);
  }

  /** RetroTouch's onLook: a looking finger moved by dx/dy (shares of the shorter side). */
  void look(float dx, float dy) {
    if (lookFingers.isEmpty()) return;  // not a finger of the look zone (see fingerDown)
    lookX += dx;
    lookY += dy;
    float way = (float) Math.sqrt(lookX * lookX + lookY * lookY);
    if (way > LOOK_RANGE) {
      lookX *= LOOK_RANGE / way;
      lookY *= LOOK_RANGE / way;
    }
    input.touchStick(true, lookX / LOOK_RANGE, lookY / LOOK_RANGE);
  }

  /** Before RetroTouch gets a touch event. */
  void beginEvent() { answered = false; }

  /**
   * After RetroTouch had the event in which `finger` went down: a finger
   * inside the look zone that no control answered looks.
   */
  void fingerDown(int finger, boolean inLookZone) {
    if (inLookZone && !answered) lookFingers.add(finger);
  }

  /** A finger lifted: the C stick goes back when it was the last looking one. */
  void fingerUp(int finger) {
    if (lookFingers.remove(finger) && lookFingers.isEmpty()) centreLook();
  }

  /** Android took the fingers away (another window): nobody looks any more. */
  void cancel() {
    lookFingers.clear();
    centreLook();
  }

  boolean looking() { return !lookFingers.isEmpty(); }

  boolean latched(String action) { return L.equals(action) ? l : R.equals(action) && r; }

  private void centreLook() {
    lookX = 0;
    lookY = 0;
    input.touchStick(true, 0, 0);
  }

  /** Everything let go (pause, the controls hidden, the editor), L and R too. */
  void reset() {
    lookFingers.clear();
    lookX = 0;
    lookY = 0;
    if (l) host.latched(L, false);
    if (r) host.latched(R, false);
    l = false;
    r = false;
    input.touchClear();
  }
}
