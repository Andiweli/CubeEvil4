package com.ast.re4decomp;

/**
 * Gyro aiming's axes (GyroAxes): at each display rotation, turning the device about the screen's
 * vertical axis is yaw (positive to the left), about its horizontal axis pitch (positive up, as
 * before 20.10.2026), and tilting about the axis through the screen (the steering wheel the
 * aim used before) does nothing. The screen's axes in the device's coordinates are Android's
 * (as SensorManager.remapCoordinateSystem / the accelerometer sample).
 */
public final class GyroAxesTests {
  static int checks;

  static void check(boolean value) {
    checks++;
    if (!value)
      throw new AssertionError("check " + checks);
  }

  static boolean near(float a, float b) { return Math.abs(a - b) < 1e-6f; }

  static float[] scale(float[] a, float s) { return new float[] {a[0] * s, a[1] * s, a[2] * s}; }

  static float[] cross(float[] a, float[] b) {
    return new float[] {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
  }

  static float dot(float[] a, float[] b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

  public static void main(String[] args) {
    // the screen's right and up in the device's coordinates, for ROTATION_0, 90, 180, 270
    final float[][] right = {{1, 0, 0}, {0, -1, 0}, {-1, 0, 0}, {0, 1, 0}};
    final float[][] up = {{0, 1, 0}, {1, 0, 0}, {0, -1, 0}, {-1, 0, 0}};
    final float[] normal = {0, 0, 1};
    final float[] view = {0, 0, -1};  // the direction the device looks into, through its back
    for (int r = 0; r < 4; r++) {
      check(near(dot(cross(right[r], up[r]), normal), 1));  // a right-handed screen
      // a counter-clockwise turn about the screen's up moves the view to the screen's left,
      // about its right up: the signs of the aim
      check(dot(cross(up[r], view), right[r]) < 0);
      check(dot(cross(right[r], view), up[r]) > 0);
      for (float rate : new float[] {0.5f, -1.25f}) {
        float[] turn = scale(up[r], rate);
        check(near(GyroAxes.yaw(r, turn), rate) && near(GyroAxes.pitch(r, turn), 0));
        float[] nod = scale(right[r], rate);
        check(near(GyroAxes.pitch(r, nod), rate) && near(GyroAxes.yaw(r, nod), 0));
        float[] wheel = scale(normal, rate);
        check(near(GyroAxes.yaw(r, wheel), 0) && near(GyroAxes.pitch(r, wheel), 0));
      }
      // pitch as before 20.10.2026
      float[] v = {0.3f, -0.7f, 0.2f};
      float before = r == 1 ? -v[1] : r == 3 ? v[1] : r == 2 ? -v[0] : v[0];
      check(near(GyroAxes.pitch(r, v), before));
    }
    System.out.println("GyroAxesTests: " + checks + " checks passed");
  }
}
