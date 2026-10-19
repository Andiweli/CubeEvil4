package com.ast.re4decomp;

/**
 * GPU step c (FramePacing): the display's rate from its vsyncs, and at which
 * vsyncs the picture is drawn: at each on 60 Hz, at every second on 120 Hz,
 * changed only after two measurements in a row agree, late callbacks ignored.
 */
public final class FramePacingTests {
  static int checks;

  static void check(boolean value) {
    checks++;
    if (!value)
      throw new AssertionError("check " + checks);
  }

  /** `n` vsyncs of a display of `hz` from `at` (ns); returns how often every() changed. */
  static int run(FramePacing p, double hz, int n, long[] at) {
    int changes = 0;
    final double period = 1e9 / hz;
    for (int i = 0; i < n; i++) {
      at[0] += Math.round(period);
      if (p.vsync(at[0])) changes++;
    }
    return changes;
  }

  public static void main(String[] args) {
    // the rule: the most vsyncs per picture that still give 59.94 pictures a second
    check(FramePacing.everyFor(59.94) == 1 && FramePacing.everyFor(60) == 1 && FramePacing.everyFor(50) == 1);
    check(FramePacing.everyFor(90) == 1);  // 45 a second would be too few
    check(FramePacing.everyFor(119.88) == 2 && FramePacing.everyFor(120) == 2 && FramePacing.everyFor(144) == 2);
    check(FramePacing.everyFor(165) == 2 && FramePacing.everyFor(180) == 3 && FramePacing.everyFor(240) == 4);
    check(FramePacing.everyFor(0) == 1);

    // the Retroid: 60 Hz, nothing changes
    long[] at = {1_000_000_000L};
    FramePacing p = new FramePacing();
    check(p.hz() == 0 && p.every() == 1);
    check(run(p, 60, 200, at) == 0 && p.every() == 1 && Math.abs(p.hz() - 60) < 0.5);

    // 120 Hz: every second vsync, after two measurements (the first vsync starts the first interval)
    p = new FramePacing();
    at[0] = 5_000_000_000L;
    run(p, 120, FramePacing.WINDOW, at);
    check(p.hz() == 0);
    run(p, 120, 1, at);
    check(p.every() == 1 && Math.abs(p.hz() - 120) < 0.5);
    check(run(p, 120, FramePacing.WINDOW, at) == 1 && p.every() == 2);
    check(run(p, 120, 500, at) == 0 && p.every() == 2);

    // late callbacks (a vsync missed now and then: twice the interval) do not move the median
    for (int i = 0; i < 400; i++) {
      at[0] += Math.round(1e9 / 120 * (i % 5 == 0 ? 2 : 1));
      check(!p.vsync(at[0]));
    }
    check(p.every() == 2 && Math.abs(p.hz() - 120) < 0.5);

    // the screen goes to 60 Hz (the request took effect): back to each vsync after two measurements
    run(p, 60, FramePacing.WINDOW, at);
    check(p.every() == 2);
    check(run(p, 60, FramePacing.WINDOW, at) == 1 && p.every() == 1);

    // one odd measurement between agreeing ones changes nothing
    run(p, 120, FramePacing.WINDOW, at);
    run(p, 60, FramePacing.WINDOW, at);
    run(p, 120, FramePacing.WINDOW, at);
    run(p, 60, FramePacing.WINDOW, at);
    check(p.every() == 1);

    // pause: the time away is no interval; a clock that goes back is skipped
    p = new FramePacing();
    at[0] = 0;
    run(p, 120, 10, at);
    p.restart();
    at[0] += 3_000_000_000L;
    run(p, 120, 2 * FramePacing.WINDOW + 1, at);
    check(p.every() == 2 && Math.abs(p.hz() - 120) < 0.5);
    check(!p.vsync(at[0] - 5_000_000L));
    run(p, 120, 2 * FramePacing.WINDOW, at);
    check(p.every() == 2 && Math.abs(p.hz() - 120) < 0.5);

    // 144 Hz: every second (72 a second), 240 Hz: every fourth
    p = new FramePacing();
    run(p, 144, 3 * FramePacing.WINDOW, at);
    check(p.every() == 2);
    p = new FramePacing();
    run(p, 240, 3 * FramePacing.WINDOW, at);
    check(p.every() == 4);
    System.out.println("FramePacingTests: " + checks + " checks passed");
  }
}
