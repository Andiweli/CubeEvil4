package com.ast.re4decomp;

/** Regression: no native work may block SurfaceView's first frame or focus. */
public final class SceneLoadGateTests {
  private static int checks;
  private static void check(boolean condition) {
    ++checks;
    if (!condition) throw new AssertionError("Scene load check " + checks);
  }
  public static void main(String[] args) {
    SceneLoadGate gate = new SceneLoadGate();
    long first = gate.newContext();
    gate.resume(true); gate.focus(true);
    check(!gate.start());
    check(gate.firstFrame() == first);
    check(!gate.start()); // Handler must run after surfaceChanged has returned.
    gate.confirmFrame(first);
    gate.focus(false); check(!gate.start());
    gate.focus(true); gate.resume(false); check(!gate.start());
    gate.resume(true); check(gate.start()); check(!gate.start());
    check(gate.busy()); check(!gate.ready());
    check(gate.complete(first)); check(gate.ready()); check(!gate.busy());
    long restored = gate.newContext();
    gate.confirmFrame(first); check(!gate.start());
    check(gate.firstFrame() == restored); gate.confirmFrame(restored);
    check(gate.start()); check(!gate.complete(first)); check(!gate.ready());
    check(!gate.cancel()); // Defer exit until the native owner is finished.
    check(gate.cancelled()); check(gate.complete(restored)); check(!gate.busy());
    check(!gate.start());
    SceneLoadGate beforeStart = new SceneLoadGate();
    long pending = beforeStart.newContext(); beforeStart.resume(true); beforeStart.focus(true);
    check(beforeStart.cancel()); beforeStart.firstFrame(); beforeStart.confirmFrame(pending);
    check(!beforeStart.start());
    System.out.println("SceneLoadGate: " + checks + " checks passed");
  }
}
