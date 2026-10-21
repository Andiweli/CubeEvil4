package com.ast.re4decomp;

/**
 * When the touch controls show (TouchControls): while no controller is connected, and always with
 * the menu's switch "Use Touch Controls" (decisions of the project owner, 19.10.2026). Since
 * 21.10.2026 (decision of the project owner, variant A) also after a finger touched the screen
 * while they were hidden, until a controller's button or stick is used again: a Redmi 15 5G
 * reported a controller nobody had connected, the controls stayed hidden and with them the
 * menu (SELECT). Pure Java (tests/TouchShowRuleTests.java).
 */
final class TouchShowRule {
  /** A stick, trigger or hat moved this far (of 1) is a deliberate use, not a resting stick's noise. */
  static final float MOVED = 0.5f;

  private boolean touched;

  /** Shown: the switch is on, no controller is connected, or the screen was touched since its last use. */
  boolean shown(boolean always, boolean controller) { return always || !controller || touched; }

  /** The screen was touched while the controls were hidden; true when that changes the rule. */
  boolean touch() {
    boolean was = touched;
    touched = true;
    return !was;
  }

  /** A controller's button or stick was used; true when that changes the rule. */
  boolean controller() {
    boolean was = touched;
    touched = false;
    return was;
  }

  /** The screen was touched since the controller's last use. */
  boolean touched() { return touched; }

  /** Any of the axes (sticks and hat -1..1, triggers 0..1) moved deliberately. */
  static boolean moved(float... axes) {
    for (float a : axes) {
      if (Math.abs(a) > MOVED) return true;
    }
    return false;
  }
}
