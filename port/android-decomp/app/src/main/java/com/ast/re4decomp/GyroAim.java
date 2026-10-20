package com.ast.re4decomp;

import android.app.Activity;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Handler;
import android.os.SystemClock;
import android.util.Log;

/**
 * Gyro aiming of the core mode (wish of the project owner, 19.10.2026: as in
 * the first Android port, ControllerActivity / leon_preview.cpp aimTick): the
 * gyroscope's turn rates go to the core (CoreBridge.gyro), which turns the aim
 * by them only while Leon aims a gun, not before (port/whole/runtime). The
 * axes (GyroAxes): pitch about the screen's horizontal axis, yaw about its
 * vertical axis (turning the device like a camera; until 20.10.2026 the axis
 * through the screen, a steering wheel). The filter is the first port's:
 * smoothed (35 % of each new reading), below 0.006 rad/s nothing, a reading
 * older than 150 ms or not a number: nothing. Switched by the menu ("Use
 * Gyro-Aiming"); off while the app is not in front.
 */
final class GyroAim implements SensorEventListener {
  private final Activity activity;
  private final SensorManager sensors;
  private final Sensor gyroscope;
  private final Handler handler;
  private boolean enabled, listening, resumed;
  private float yaw, pitch;

  GyroAim(Activity activity, Handler handler) {
    this.activity = activity;
    this.handler = handler;
    sensors = (SensorManager) activity.getSystemService(Context.SENSOR_SERVICE);
    gyroscope = sensors == null ? null : sensors.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
  }

  /** The device has a gyroscope. */
  static boolean available(Context context) {
    SensorManager s = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
    return s != null && s.getDefaultSensor(Sensor.TYPE_GYROSCOPE) != null;
  }

  /** The menu's switch. */
  void setEnabled(boolean on) {
    enabled = on && gyroscope != null;
    Log.i(CoreActivity.TAG, "gyro aiming " + (enabled ? "on" : gyroscope == null ? "off (no gyroscope)" : "off"));
    update();
  }

  void resume() {
    resumed = true;
    update();
  }

  void pause() {
    resumed = false;
    update();
  }

  private void update() {
    boolean want = enabled && resumed;
    if (want == listening) return;
    listening = want;
    if (want) {
      sensors.registerListener(this, gyroscope, SensorManager.SENSOR_DELAY_GAME, handler);
    } else {
      sensors.unregisterListener(this);
      still();
    }
  }

  private void still() {
    yaw = pitch = 0;
    CoreBridge.gyro(0, 0);
  }

  @Override public void onAccuracyChanged(Sensor sensor, int accuracy) {}

  // Float.isFinite needs Android 7.0; the app runs from 6.0
  private static boolean finite(float v) { return !Float.isNaN(v) && !Float.isInfinite(v); }

  @Override public void onSensorChanged(SensorEvent event) {
    if (!activity.hasWindowFocus()) {
      still();
      return;
    }
    // the gyroscope's axes are the device's natural orientation's, the aim's the screen's
    int rotation = activity.getWindowManager().getDefaultDisplay().getRotation();
    float p = GyroAxes.pitch(rotation, event.values);
    float y = GyroAxes.yaw(rotation, event.values);
    if (SystemClock.elapsedRealtimeNanos() - event.timestamp > 150_000_000L || !finite(y) || !finite(p)) {
      still();
      return;
    }
    yaw += .35f * (y - yaw);
    pitch += .35f * (p - pitch);
    CoreBridge.gyro(Math.abs(yaw) < .006f ? 0 : yaw, Math.abs(pitch) < .006f ? 0 : pitch);
  }
}
