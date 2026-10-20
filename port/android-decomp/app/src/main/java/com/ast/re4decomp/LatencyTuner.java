package com.ast.re4decomp;

/**
 * How much of an AudioTrack's buffer the sound output keeps filled (CoreAudio; the latency
 * tuning of Oboe's LatencyTuner): it starts small, two bursts of the output's mixer (at least four
 * writes: from two the Retroid's speaker had six underruns in the first two seconds before it
 * settled at 40 ms, 20.10.2026), and grows by a burst whenever the track counts new underruns, up
 * to the buffer's capacity. Android asks for a large buffer on Bluetooth (about 280 ms for AirPods on the
 * Retroid, 20.10.2026: the shots were heard late); kept full it added all of it to the
 * headphones' own delay. Independent of Android so the host tests can check it.
 */
final class LatencyTuner {
  private final int capacity, step;
  private int size, lastUnderruns, grown;

  /**
   * capacity: the track's buffer (frames); burst: the output mixer's period in the track's frames;
   * write: the frames of one write; learned: what an earlier track on the same output grew to
   * (0: none), the start then.
   */
  LatencyTuner(int capacity, int burst, int write, int learned) {
    this.capacity = Math.max(1, capacity);
    this.step = Math.max(64, burst);
    size = Math.min(this.capacity, Math.max(Math.max(2 * this.step, 4 * write), learned));
  }

  /** The frames to keep filled now. */
  int size() { return size; }

  /** How often it grew so far. */
  int grown() { return grown; }

  /** The track's underruns so far: true when the size grew (call setBufferSizeInFrames then). */
  boolean check(int underruns) {
    final boolean more = underruns > lastUnderruns;
    lastUnderruns = underruns;
    if (!more || size >= capacity) return false;
    size = Math.min(capacity, size + step);
    grown++;
    return true;
  }

  /** The output mixer's period (frames at its rate, AudioManager's PROPERTY_OUTPUT_FRAMES_PER_BUFFER)
   *  in frames at `rate`; 256 at 48 kHz when unknown. */
  static int burst(String framesPerBuffer, String outputRate, int rate) {
    int frames = 256, at = 48000;
    try {
      if (framesPerBuffer != null) frames = Integer.parseInt(framesPerBuffer.trim());
      if (outputRate != null) at = Integer.parseInt(outputRate.trim());
    } catch (NumberFormatException ignored) {
      // the defaults
    }
    if (frames <= 0 || at <= 0) {
      frames = 256;
      at = 48000;
    }
    return (int) ((frames * (long) rate + at - 1) / at);
  }
}
