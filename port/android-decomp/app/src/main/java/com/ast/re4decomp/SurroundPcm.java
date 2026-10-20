package com.ast.re4decomp;

/**
 * The game's four channels (front left, front right, rear left, rear right; CoreBridge.audio4) as
 * Android's 5.1 layout (AudioFormat.CHANNEL_OUT_5POINT1: front left, front right, centre, low
 * frequency, back left, back right). The game has no centre and no subwoofer channel: both stay
 * silent. Independent of Android so the host tests can check it (tests/SurroundPcmTests.java).
 */
final class SurroundPcm {
  private SurroundPcm() {}

  /** `frames` frames of `four` (4 values each) into `six` (6 values each). */
  static void fivePointOne(short[] four, short[] six, int frames) {
    for (int i = 0; i < frames; i++) {
      six[i * 6] = four[i * 4];
      six[i * 6 + 1] = four[i * 4 + 1];
      six[i * 6 + 2] = 0;
      six[i * 6 + 3] = 0;
      six[i * 6 + 4] = four[i * 4 + 2];
      six[i * 6 + 5] = four[i * 4 + 3];
    }
  }
}
