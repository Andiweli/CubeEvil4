package com.ast.re4decomp;

/**
 * The sound output's latency tuning (LatencyTuner): small at first, a burst more after new
 * underruns, never beyond the capacity; the mixer burst at the track's 32 kHz.
 */
public final class LatencyTunerTests {
  static int checks;

  static void check(boolean value) {
    checks++;
    if (!value)
      throw new AssertionError("check " + checks);
  }

  public static void main(String[] args) {
    // 240 frames at 48 kHz = 160 at 32 kHz; unknown: 256 at 48 kHz = 171
    check(LatencyTuner.burst("240", "48000", 32000) == 160);
    check(LatencyTuner.burst(null, null, 32000) == 171);
    check(LatencyTuner.burst("x", "48000", 32000) == 171);
    check(LatencyTuner.burst("0", "0", 32000) == 171);

    // the Retroid's speaker: capacity 2568, burst 160, writes of 256: starts at 1024 (32 ms)
    LatencyTuner t = new LatencyTuner(2568, 160, 256, 0);
    check(t.size() == 1024);
    check(!t.check(0) && t.size() == 1024);  // no underruns: stays
    check(t.check(1) && t.size() == 1184);   // new ones: a burst more
    check(!t.check(1) && t.size() == 1184);  // the same count: stays
    check(t.check(5) && t.size() == 1344 && t.grown() == 2);

    // Bluetooth: capacity 8988; it grows until the underruns stop, at most to the capacity
    t = new LatencyTuner(8988, 160, 256, 0);
    int underruns = 0;
    for (int i = 0; i < 200; i++) t.check(++underruns);
    check(t.size() == 8988);
    check(!t.check(++underruns) && t.size() == 8988);

    // a capacity smaller than the start: the capacity
    t = new LatencyTuner(300, 160, 256, 0);
    check(t.size() == 300 && !t.check(3));
    // a new track on the same output starts where the last one ended
    t = new LatencyTuner(8988, 160, 256, 2400);
    check(t.size() == 2400);
    t = new LatencyTuner(8988, 160, 256, 100);  // below the start: the start
    check(t.size() == 1024);
    System.out.println("LatencyTunerTests: " + checks + " checks passed");
  }
}
