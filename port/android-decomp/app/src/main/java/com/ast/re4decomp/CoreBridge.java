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
  /**
   * GameCube controller, raw like the hardware (the game applies the dead
   * zones itself): SDK button bits, main stick about -100..100, C stick about
   * -90..90, triggers 0..255 (CoreInput).
   */
  public static native void pad(int buttons, int stickX, int stickY, int substickX,
                                int substickY, int triggerL, int triggerR);
  /**
   * The game's rumble motor: bits 0-3 = on in the last four game frames
   * (bit 0 the newest), bits 8-31 = game frames so far; 0 without the core.
   */
  public static native int rumble();
  /**
   * The game's sound: `frames` stereo frames (left, right) at 32 kHz into
   * `pcm` (silence while there is none); returns the frames from the game.
   */
  public static native int audio(short[] pcm, int frames);
  /**
   * A vsync of the display (the Choreographer's frame time, System.nanoTime):
   * the game's video retraces follow the display (no drift against 60 Hz).
   */
  public static native void vsync(long frameTimeNanos);
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

  // --- the app's menu (M11; cores since M11, hasMenu says whether this one has it) ---

  /** The core has the menu's functions (pause, memory card). */
  public static native boolean hasMenu();
  /**
   * on: the game stops at its next frame and waits until pause(false);
   * waits up to waitMs for that. True when the game stands.
   */
  public static native boolean pause(boolean on, int waitMs);
  /** The game's debug menu at its next frame (SELECT held, patch 0061). */
  public static native void debugMenu();
  /** The game's depth of field (patch 0065) and, where the game asks for none while the player is free, the
   *  gameplay autofocus of the first Android port; on unless switched off. */
  public static native void setDof(boolean on);
  /**
   * The memory card's files, one tab-separated row each (index, name,
   * blocks, save, intact, room, chapter, times saved, play time, year,
   * month, day, hour, minute, comment 1, comment 2); null when the game
   * uses the card or there is none (cardNote: "busy" / "nocard"). Only
   * while the game stands (pause).
   */
  public static native String[] cardList();
  /** Free blocks of the card, negative when it cannot be read now. */
  public static native int cardFreeBlocks();
  /** The name Dolphin gives the file's .gci ("08-G4BE-bh4_data00.gci"). */
  public static native String cardFileName(int index);
  /** The .gci of a file (GameCube byte order), null on an error (cardNote). */
  public static native byte[] cardExport(int index);
  /** A .gci onto the card: CARD_OK or an error (cardNote). */
  public static native int cardImport(byte[] gci, boolean replace);
  /** What the last card function did or why not. */
  public static native String cardNote();
  /** Results of cardImport (runtime/re4_card_files.h). */
  public static final int CARD_OK = 0, CARD_NOCARD = -1, CARD_EXISTS = -2, CARD_NOSPACE = -3, CARD_BAD = -4,
      CARD_BUSY = -5;
  /**
   * The picture options: scale 0 (automatic) or 1-4 (EFB resolution
   * multiple), samples 1/2/4 (multisampling), anisotropy 1-16, aspect 0
   * (16:9) or 1 (4:3); applied at the next picture while the game runs.
   */
  public static native void setGraphics(int scale, int samples, int anisotropy, int aspect);
  /** {most samples, strongest anisotropy, scale, samples, anisotropy in use}; null before the first picture. */
  public static native int[] graphicsInfo();
  /** Where a crash writes where it happened (the error log's folder). */
  public static native void setCrashFile(String path);
  /** Game frames so far (one per controller read), -1 without the counter. */
  public static native long gameFrames();
  /** The game stands without a pause for `seconds`: where its thread is, into the error log. */
  public static native void reportHang(int seconds);
}
