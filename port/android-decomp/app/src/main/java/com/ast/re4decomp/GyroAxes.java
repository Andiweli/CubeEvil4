package com.ast.re4decomp;

/**
 * The two turn rates of the gyro aiming (GyroAim) from a gyroscope reading. The gyroscope's
 * axes are the device's natural orientation's (x right, y up, z out of the screen; rad/s,
 * counter-clockwise positive); the aim's are the screen's as it is shown (Display.getRotation,
 * Surface.ROTATION_0..270 = 0..3; 1: the device turned a quarter counter-clockwise, its screen's
 * right is the device's -y and its up the device's x).
 *
 * <ul>
 *   <li>yaw: about the screen's vertical axis, positive turns left: the device turned like a
 *       camera, as most shooters do (a tester, 20.10.2026; before, the axis through the screen
 *       as in the first port: tilting like a steering wheel, "as if it's a racing game")
 *   <li>pitch: about the screen's horizontal axis, positive looks up (as before)
 * </ul>
 *
 * Tilting about the axis through the screen does nothing.
 */
final class GyroAxes {
  private GyroAxes() {}

  /** Turning left (rad/s) at the display's rotation; v: x, y, z of the reading. */
  static float yaw(int rotation, float[] v) {
    switch (rotation & 3) {
      case 1: return v[0];
      case 2: return -v[1];
      case 3: return -v[0];
      default: return v[1];
    }
  }

  /** Looking up (rad/s) at the display's rotation; v: x, y, z of the reading. */
  static float pitch(int rotation, float[] v) {
    switch (rotation & 3) {
      case 1: return -v[1];
      case 2: return -v[0];
      case 3: return v[1];
      default: return v[0];
    }
  }
}
