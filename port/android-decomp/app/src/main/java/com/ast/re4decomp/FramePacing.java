package com.ast.re4decomp;

import java.util.Arrays;

/**
 * GPU step c (19.10.2026): the game makes at most 60 pictures a second (59.94
 * fields); a screen of 90 Hz or more had each of them drawn anew at every
 * vsync (the tester's POCO X7 Pro: 120 presents a second). CoreActivity asks
 * Android for 60 Hz; where the screen still runs faster, the picture is drawn
 * at every n-th vsync only (`every()`: 120 Hz 2, 144 Hz 2, 240 Hz 4), still
 * at least 60 times a second. The rate is the display's, measured from its
 * vsyncs (Choreographer), not from the pictures drawn: a slow GPU does not
 * change it. Independent of Android so the host tests can check it
 * (tests/FramePacingTests.java).
 */
final class FramePacing {
  /** Vsync intervals per measurement; their median is the display's period (a late callback does not count). */
  static final int WINDOW = 32;

  private final long[] deltas = new long[WINDOW];
  private int count;
  private long last = Long.MIN_VALUE;
  private int every = 1, candidate = 1;
  private double hz;

  /** The pictures for a display of `hz`: every n-th vsync, n the most that still gives 59.94 a second. */
  static int everyFor(double hz) {
    return Math.max(1, (int) Math.floor(hz / 59.94 + 0.03));
  }

  /**
   * A vsync of the display at `nanos` (Choreographer's frame time). True when
   * every() changed: the same new value from two measurements in a row (a
   * screen that changes its rate settles first).
   */
  boolean vsync(long nanos) {
    final long previous = last;
    last = nanos;
    if (previous == Long.MIN_VALUE || nanos <= previous) return false;
    deltas[count++] = nanos - previous;
    if (count < WINDOW) return false;
    count = 0;
    final long[] sorted = deltas.clone();
    Arrays.sort(sorted);
    hz = 1e9 / sorted[WINDOW / 2];
    final int want = everyFor(hz);
    if (want == every) {
      candidate = want;
      return false;
    }
    if (want != candidate) {
      candidate = want;
      return false;
    }
    every = want;
    return true;
  }

  /** Draw at every n-th vsync (1: at each). */
  int every() { return every; }

  /** The display's rate from the last measurement, 0 before the first. */
  double hz() { return hz; }

  /** The app went away (pause): the next vsync starts a new interval. */
  void restart() {
    last = Long.MIN_VALUE;
    count = 0;
  }
}
