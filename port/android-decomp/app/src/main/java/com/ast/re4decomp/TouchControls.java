package com.ast.re4decomp;

import android.content.Context;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import com.ast.retrotouch.RetroTouchAdapter;
import com.ast.retrotouch.RetroTouchColors;
import com.ast.retrotouch.RetroTouchControl;
import com.ast.retrotouch.RetroTouchControllers;
import com.ast.retrotouch.RetroTouchLayout;
import com.ast.retrotouch.RetroTouchMode;
import com.ast.retrotouch.RetroTouchView;
import java.util.ArrayList;
import java.util.List;

/**
 * RetroTouch (app/libs/retrotouch.aar; an update replaces the file) above the
 * game's picture: the touch controls of the core mode, mapped by CoreTouch.
 * Shown while no controller is connected (RetroTouchControllers: built-in,
 * USB and Bluetooth controllers), and always when the menu's switch "Use
 * Touch Controls" is on (decisions of the project owner, 19.10.2026).
 * RetroTouch's gear at the top right opens its editor (move, resize, save the
 * layout); the game stands meanwhile. Everything here runs on the UI thread.
 */
final class TouchControls implements CoreTouch.Host {
  interface Host {
    /** MENU went down or up: the app's SELECT; `finger` false when RetroTouch let it go itself (no menu then). */
    void select(boolean down, boolean finger);
    /** RetroTouch's editor opened or closed. */
    void editing(boolean on);
    /** The controller changed: to the core. */
    void send();
  }

  /** The key of the saved layout (RetroTouch keeps the player's layout under it). */
  static final String LAYOUT_ID = "cubeevil4";

  private final RetroTouchView view;
  private final CoreTouch touch;
  private final Host host;
  private final int[] where = new int[2];
  private boolean always, shown, editing, quiet, first = true;

  /** always: the menu's switch (shown even with a controller). */
  TouchControls(Context context, CoreInput input, boolean always, Host host) {
    this.host = host;
    this.always = always;
    touch = new CoreTouch(input, this);
    view = new RetroTouchView(context);
    // the game's surface keeps the focus (the controller's keys)
    view.setFocusable(false);
    // shown by the controllers present, not hidden for a while by their keys
    view.setAutoHideOnController(false);
    for (int i = 0; i < CoreTouch.ACTIONS.length; i++) {
      if (CoreTouch.COLOURS[i] == 0) view.registerAction(CoreTouch.ACTIONS[i], CoreTouch.LABELS[i]);
      else view.registerAction(CoreTouch.ACTIONS[i], CoreTouch.LABELS[i], RetroTouchColors.of(CoreTouch.COLOURS[i]));
    }
    view.setGameplayLayout(layout());
    view.setMode(RetroTouchMode.OFF);  // hidden until update() decides
    view.setListener(new RetroTouchAdapter() {
      @Override public void onAction(String action, boolean pressed) {
        touch.action(action, pressed);
        host.send();
      }

      @Override public void onMove(float x, float y) {
        touch.move(x, y);
        host.send();
      }

      @Override public void onLook(float dx, float dy) {
        touch.look(dx, dy);
        host.send();
      }

      @Override public void onEditorStateChanged(boolean on) {
        editing = on;
        touch.reset();
        Log.i(CoreActivity.TAG, "touch controls: editor " + (on ? "open, the game stands" : "closed"));
        host.editing(on);
        host.send();
      }
    });
    update();
  }

  private static RetroTouchLayout layout() {
    List<RetroTouchControl> controls = new ArrayList<>();
    for (CoreTouch.Control c : CoreTouch.LAYOUT) {
      switch (c.kind) {
        case CoreTouch.LOOK:
          controls.add(RetroTouchControl.lookZone(c.id, c.x, c.y, c.width, c.height));
          break;
        case CoreTouch.STICK:
          controls.add(RetroTouchControl.moveStick(c.id, c.x, c.y, c.width));
          break;
        case CoreTouch.DPAD:
          controls.add(RetroTouchControl.dPad(c.id, c.x, c.y, c.width));
          break;
        default:
          controls.add(RetroTouchControl.button(c.id, c.action, CoreTouch.label(c.action), c.x, c.y, c.width));
          break;
      }
    }
    return new RetroTouchLayout(LAYOUT_ID, controls);
  }

  View view() { return view; }

  boolean shown() { return shown; }

  /** A gamepad or joystick is connected (built-in, USB, Bluetooth). */
  static boolean controllerConnected() { return RetroTouchControllers.isControllerConnected(); }

  /** The menu's switch: shown always (on) or only while no controller is connected (off). */
  void setAlways(boolean on) {
    always = on;
    update();
  }

  /** The menu's switch "Latch L and R": a tap keeps L or R pressed until the next tap (off: held while touched). */
  void setLatch(boolean on) { touch.setLatch(on); }

  /** Shown or hidden by the controllers present and the switch (twice a second and when a device changes). */
  void update() {
    boolean controller = controllerConnected();
    boolean want = always || !controller;
    if (want == shown && !first) return;
    first = false;
    shown = want;
    quietly(() -> view.setMode(want ? RetroTouchMode.GAMEPLAY : RetroTouchMode.OFF));
    touch.reset();
    Log.i(CoreActivity.TAG, "touch controls " + (want ? "shown" : "hidden")
        + (controller ? " (a controller is connected" + (always ? ", the menu's switch is on)" : ")") : " (no controller)"));
    host.send();
  }

  /** Nothing held (the app's menu, focus lost, pause): RetroTouch forgets its fingers too. */
  void reset() {
    touch.reset();
    quietly(view::resetInputState);
  }

  private void quietly(Runnable r) {
    quiet = true;
    try {
      r.run();
    } finally {
      quiet = false;
    }
  }

  @Override public void select(boolean down) { host.select(down, !quiet); }

  @Override public void latched(String action, boolean on) { view.setActionLatched(action, on); }

  /** Before the window's touch event goes to the views (CoreActivity.dispatchTouchEvent). */
  void before(MotionEvent e) {
    if (shown) touch.beginEvent();
  }

  /** After it went through them: which finger looks, which stopped. */
  void after(MotionEvent e) {
    if (!shown) return;
    int i = e.getActionIndex();
    switch (e.getActionMasked()) {
      case MotionEvent.ACTION_DOWN:
      case MotionEvent.ACTION_POINTER_DOWN:
        touch.fingerDown(e.getPointerId(i), inLookZone(e.getX(i), e.getY(i)));
        break;
      case MotionEvent.ACTION_UP:
      case MotionEvent.ACTION_POINTER_UP:
        boolean looked = touch.looking();
        touch.fingerUp(e.getPointerId(i));
        if (looked) host.send();
        break;
      case MotionEvent.ACTION_CANCEL:
        touch.cancel();
        host.send();
        break;
      default:
        break;
    }
  }

  /** A point of the window inside a look zone of the current layout (as RetroTouch tests it). */
  private boolean inLookZone(float x, float y) {
    if (editing || view.getWidth() <= 0) return false;
    view.getLocationInWindow(where);
    x -= where[0];
    y -= where[1];
    int w = view.getWidth(), h = view.getHeight();
    for (RetroTouchControl c : view.getCurrentLayout().getControls()) {
      if (c.getType() != RetroTouchControl.Type.LOOK_ZONE) continue;
      float halfW = c.getWidth() * w * 0.5f, halfH = c.getHeight() * h * 0.5f;
      float cx = c.getX() * w, cy = c.getY() * h;
      if (x >= cx - halfW && x <= cx + halfW && y >= cy - halfH && y <= cy + halfH) return true;
    }
    return false;
  }
}
