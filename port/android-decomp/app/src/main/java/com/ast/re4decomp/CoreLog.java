package com.ast.re4decomp;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.util.Log;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/**
 * The error log of the core mode for testers (M11): without a PC. While the
 * game runs, the app's own log (an app reads only its own lines) goes into
 * logs/log.txt in the app's folder on the shared storage
 * (Android/data/<app>/files/logs), two files of 4 MB at most; a crash
 * writes where it happened into logs/crash.txt (core_mode.cpp). At the
 * next start the files of the run before move to logs/previous/, with what the
 * system says about how that run ended (ApplicationExitInfo, Android 11 and
 * later) and its crash report lines. The texts are English (wish of the
 * project owner, 17.10.2026: everything the app adds in English). The menu packs all of it with the
 * device's data into a zip to save or to send (CoreActivity, ReportProvider).
 * On by default; the menu switches the recording off.
 */
final class CoreLog {
  static final String PREFS = "core_log", RECORD = "record";
  private static final String TAG = "RE4Log";
  private static Process recorder;
  private static boolean started;  // once per process (the Activity may be made anew, the core runs on)

  private CoreLog() {}

  static File folder(Context c) {
    File base = c.getExternalFilesDir(null);
    File dir = new File(base != null ? base : c.getFilesDir(), "logs");
    if (!dir.isDirectory() && !dir.mkdirs()) Log.w(TAG, "cannot make " + dir);
    return dir;
  }

  static boolean enabled(Context c) {
    return c.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getBoolean(RECORD, true);
  }

  static void setEnabled(Context c, boolean on) {
    c.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putBoolean(RECORD, on).apply();
    if (on) startRecorder(c); else stop();
  }

  /** At the start of the core: the run before kept, then the recording of this one. */
  static synchronized void start(Context c, String settings) {
    if (started) return;
    started = true;
    File dir = folder(c);
    File before = new File(dir, "previous");
    deleteTree(before);
    if (!before.mkdirs()) Log.w(TAG, "cannot make " + before);
    File[] old = dir.listFiles();
    if (old != null) {
      for (File f : old) {
        if (f.isFile() && !f.renameTo(new File(before, f.getName()))) Log.w(TAG, "cannot keep " + f);
      }
    }
    write(new File(before, "exit.txt"), lastExits(c));
    String crash = run("logcat", "-d", "-b", "crash", "-v", "threadtime");
    if (!crash.trim().isEmpty()) write(new File(before, "crash-system.txt"), crash);
    write(new File(dir, "device.txt"), deviceText(c, settings));
    CoreBridge.setCrashFile(new File(dir, "crash.txt").getAbsolutePath());
    if (enabled(c)) startRecorder(c);
  }

  private static synchronized void startRecorder(Context c) {
    if (recorder != null) return;
    File file = new File(folder(c), "log.txt");
    try {
      // from now on (-T 1), rotated: 2 files of 4 MB
      recorder = new ProcessBuilder("logcat", "-v", "threadtime", "-b", "main", "-b", "system", "-b", "crash",
          "-T", "1", "-f", file.getAbsolutePath(), "-r", "4096", "-n", "2").redirectErrorStream(true).start();
      Log.i(TAG, "error log: recording into " + file);
    } catch (IOException e) {
      Log.w(TAG, "error log: logcat does not start: " + e);
      recorder = null;
    }
  }

  static synchronized void stop() {
    if (recorder != null) {
      recorder.destroy();
      recorder = null;
    }
  }

  /** The device, the app and the switches, for the report. */
  static String deviceText(Context c, String settings) {
    StringBuilder b = new StringBuilder();
    b.append("RE4 core, error report\n");
    b.append("Time: ").append(new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.ROOT).format(new Date())).append('\n');
    try {
      android.content.pm.PackageInfo p = c.getPackageManager().getPackageInfo(c.getPackageName(), 0);
      b.append("App: ").append(c.getPackageName()).append(' ').append(p.versionName).append('\n');
    } catch (Exception e) {
      b.append("App: ").append(c.getPackageName()).append('\n');
    }
    b.append("Device: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
        .append(" (").append(Build.DEVICE).append(")\n");
    b.append("Android: ").append(Build.VERSION.RELEASE).append(" (API ").append(Build.VERSION.SDK_INT).append(")\n");
    b.append("ABIs:");
    for (String abi : Build.SUPPORTED_ABIS) b.append(' ').append(abi);
    b.append('\n');
    ActivityManager am = (ActivityManager) c.getSystemService(Context.ACTIVITY_SERVICE);
    if (am != null) {
      ActivityManager.MemoryInfo m = new ActivityManager.MemoryInfo();
      am.getMemoryInfo(m);
      b.append("Memory: ").append(m.totalMem >> 20).append(" MB, free ").append(m.availMem >> 20).append(" MB\n");
    }
    SharedPreferences g = c.getSharedPreferences(CoreActivity.GRAPHICS_PREFS, Context.MODE_PRIVATE);
    b.append("Graphics: ").append(g.getAll()).append('\n');
    int[] info = CoreBridge.graphicsInfo();
    if (info != null) {
      b.append(String.format(Locale.ROOT, "Graphics in use: level %d, %d samples (up to %d), anisotropy %d (up to %d)\n",
          info[2], info[3], info[0], info[4], info[1]));
    }
    b.append("Switches: ").append(settings == null || settings.isEmpty() ? "-" : settings).append('\n');
    return b.toString();
  }

  /** What the system says about how the last runs ended (Android 11 and later). */
  private static String lastExits(Context c) {
    return Build.VERSION.SDK_INT >= 30 ? exitReasons(c) : "(from Android 11 on)\n";
  }

  @android.annotation.TargetApi(30)
  private static String exitReasons(Context c) {
    ActivityManager am = (ActivityManager) c.getSystemService(Context.ACTIVITY_SERVICE);
    if (am == null) return "";
    StringBuilder b = new StringBuilder();
    List<ApplicationExitInfo> exits = am.getHistoricalProcessExitReasons(null, 0, 3);
    SimpleDateFormat when = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.ROOT);
    for (ApplicationExitInfo e : exits) {
      b.append(when.format(new Date(e.getTimestamp()))).append("  ").append(reason(e.getReason()))
          .append(", Status ").append(e.getStatus()).append(", ").append(e.getDescription()).append('\n');
    }
    return b.toString();
  }

  @android.annotation.TargetApi(30)
  private static String reason(int r) {
    switch (r) {
      case ApplicationExitInfo.REASON_CRASH: return "crash (Java)";
      case ApplicationExitInfo.REASON_CRASH_NATIVE: return "crash (native)";
      case ApplicationExitInfo.REASON_ANR: return "not responding (ANR)";
      case ApplicationExitInfo.REASON_LOW_MEMORY: return "low memory";
      case ApplicationExitInfo.REASON_EXIT_SELF: return "ended (app)";
      case ApplicationExitInfo.REASON_USER_REQUESTED: return "ended (user)";
      case ApplicationExitInfo.REASON_SIGNALED: return "signal";
      default: return "reason " + r;
    }
  }

  /** The report: the log folder (this run, the one before) and `extra` files (name, text). */
  static void writeReport(Context c, OutputStream out, String settings, String[][] extra) throws IOException {
    try (ZipOutputStream zip = new ZipOutputStream(out)) {
      put(zip, "device-now.txt", deviceText(c, settings).getBytes(StandardCharsets.UTF_8));
      if (extra != null) {
        for (String[] e : extra) put(zip, e[0], e[1].getBytes(StandardCharsets.UTF_8));
      }
      addTree(zip, folder(c), "logs/");
    }
  }

  private static void addTree(ZipOutputStream zip, File dir, String prefix) throws IOException {
    File[] files = dir.listFiles();
    if (files == null) return;
    for (File f : files) {
      if (f.isDirectory()) {
        addTree(zip, f, prefix + f.getName() + "/");
      } else {
        try (InputStream in = new FileInputStream(f)) {
          put(zip, prefix + f.getName(), readAll(in));
        }
      }
    }
  }

  private static void put(ZipOutputStream zip, String name, byte[] data) throws IOException {
    zip.putNextEntry(new ZipEntry(name));
    zip.write(data);
    zip.closeEntry();
  }

  static byte[] readAll(InputStream in) throws IOException {
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    byte[] buffer = new byte[65536];
    for (int n; (n = in.read(buffer)) > 0; ) out.write(buffer, 0, n);
    return out.toByteArray();
  }

  private static String run(String... command) {
    try {
      Process p = new ProcessBuilder(command).redirectErrorStream(true).start();
      String text = new String(readAll(p.getInputStream()), StandardCharsets.UTF_8);
      p.waitFor();
      return text;
    } catch (IOException e) {
      return "";
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
      return "";
    }
  }

  private static void write(File f, String text) {
    try (OutputStream out = new java.io.FileOutputStream(f)) {
      out.write(text.getBytes(StandardCharsets.UTF_8));
    } catch (IOException e) {
      Log.w(TAG, "cannot write " + f + ": " + e);
    }
  }

  private static void deleteTree(File f) {
    File[] files = f.listFiles();
    if (files != null) for (File x : files) deleteTree(x);
    if (f.exists() && !f.delete()) Log.w(TAG, "cannot delete " + f);
  }
}
