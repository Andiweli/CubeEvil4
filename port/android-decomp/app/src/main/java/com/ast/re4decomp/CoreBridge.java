package com.ast.re4decomp;

/**
 * Core mode (armeabi-v7a): the original game core (libre4_main.so and its
 * libraries, port/whole) runs the game; native side core_mode.cpp.
 */
public final class CoreBridge {
  private CoreBridge() {}
  private static boolean loaded;
  /** Loads the app library; its JNI_OnLoad loads the core and reserves MEM1. */
  public static synchronized void load() {
    if (!loaded) {
      System.loadLibrary("re4decomp");
      loaded = true;
    }
  }
  /**
   * Starts the game once per process; "" when it runs, else the reason.
   * settings: "NAME=value;NAME=value" for the core's test switches (the same
   * environment variables as the Linux checks, e.g. RE4_START_ROOM=0x100,
   * RE4_PAD_SCRIPT=..., RE4_PAD_UNTIL_ROOM=1), set before the game starts.
   */
  public static native String start(String gameRoot, String settings);
  /** GameCube controller: SDK button bits, sticks about -100..100, triggers 0..255. */
  public static native void pad(int buttons, int stickX, int stickY, int substickX,
                                int substickY, int triggerL, int triggerR);
  /** The room the game is in (stage << 8 | room), -1 outside a room. */
  public static native int wantedRoom();
  /** The decoded room archive for a room (RoomRepository), or the error. */
  public static native void roomPrepared(int room, String decodedPath, String error);
  /** Diagnostic text: core state, frame, room, models, last log lines. */
  public static native String status();
  /** Saves the graphics data of the next picture into `folder` (diagnosis; R3). */
  public static native void gxDump(String folder);
  public static native void surfaceCreated();
  public static native void resize(int width, int height);
  public static native void draw();
}
