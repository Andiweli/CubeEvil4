package com.ast.re4decomp;

/**
 * When the touch controls show (TouchShowRule, variant A of 21.10.2026): without a controller and
 * with the menu's switch always; with a controller (or a device that only says it is one, as on
 * a Redmi 15 5G) after a touch of the screen, until the controller's next button or stick.
 */
public final class TouchShowRuleTests {
  static int checks;

  static void check(boolean value) {
    checks++;
    if (!value)
      throw new AssertionError("check " + checks);
  }

  public static void main(String[] args) {
    TouchShowRule rule = new TouchShowRule();
    // as before 21.10.2026: no controller or the switch shows them, a controller hides them
    check(rule.shown(false, false));
    check(rule.shown(true, false));
    check(rule.shown(true, true));
    check(!rule.shown(false, true));
    check(!rule.touched());

    // a touch while a controller hides them: shown, once
    check(rule.touch());
    check(rule.touched());
    check(rule.shown(false, true));
    check(!rule.touch());
    check(rule.shown(false, true));
    // the switch and "no controller" keep showing them as before
    check(rule.shown(true, true));
    check(rule.shown(false, false));

    // the controller's button or stick: hidden again, once
    check(rule.controller());
    check(!rule.touched());
    check(!rule.shown(false, true));
    check(!rule.controller());
    check(!rule.shown(false, true));
    // without a controller still shown, with the switch too
    check(rule.shown(false, false));
    check(rule.shown(true, true));

    // a controller used while nothing was touched changes nothing
    TouchShowRule fresh = new TouchShowRule();
    check(!fresh.controller());
    check(!fresh.shown(false, true));
    check(fresh.touch());
    check(fresh.shown(false, true));

    // deliberate moves: past half the way, in either direction; a resting stick's noise is not one
    check(!TouchShowRule.moved());
    check(!TouchShowRule.moved(0, 0, 0, 0, 0, 0, 0, 0));
    check(!TouchShowRule.moved(0.05f, -0.04f, 0.1f, -0.2f, 0.3f, 0.1f, 0, 0));
    check(!TouchShowRule.moved(TouchShowRule.MOVED, -TouchShowRule.MOVED));
    check(TouchShowRule.moved(0.51f));
    check(TouchShowRule.moved(0, -0.8f));
    check(TouchShowRule.moved(0, 0, 0, 0, 1, 0, 0, 0));      // a trigger pulled
    check(TouchShowRule.moved(0, 0, 0, 0, 0, 0, -1, 0));     // the hat left
    check(TouchShowRule.moved(0, 0, 0, 0, 0, 0, 0, 1));      // the hat down
    check(!TouchShowRule.moved(Float.NaN));                  // CoreActivity.axis gives 0 for NaN anyway

    System.out.println("TouchShowRuleTests: " + checks + " checks passed");
  }
}
