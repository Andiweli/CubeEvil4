package com.ast.re4decomp;

/** Coordinates native scene creation without holding Android's first frame. */
final class SceneLoadGate {
  private long generation;
  private boolean firstFrame, confirmedFrame, focused, resumed, started, ready, cancelled;

  synchronized long newContext() {
    ++generation;
    firstFrame = confirmedFrame = started = ready = false;
    return generation;
  }
  synchronized long firstFrame() {
    if (firstFrame) return 0;
    firstFrame = true;
    return generation;
  }
  synchronized void confirmFrame(long context) {
    if (context == generation && firstFrame) confirmedFrame = true;
  }
  synchronized void focus(boolean value) { focused = value; }
  synchronized void resume(boolean value) { resumed = value; }
  synchronized boolean start() {
    if (cancelled || started || !confirmedFrame || !focused || !resumed) return false;
    started = true;
    return true;
  }
  synchronized boolean complete(long context) {
    if (context != generation || !started) return false;
    ready = true;
    return true;
  }
  synchronized boolean ready() { return ready; }
  synchronized boolean busy() { return started && !ready; }
  synchronized long generation() { return generation; }
  synchronized boolean cancel() { cancelled = true; return !busy(); }
  synchronized boolean cancelled() { return cancelled; }
}
