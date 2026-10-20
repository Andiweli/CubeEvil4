package com.ast.re4decomp;

/** The game's four channels in Android's 5.1 order (SurroundPcm): front, silent centre and LFE, rear. */
public final class SurroundPcmTests {
  static int checks;

  static void check(boolean value) {
    checks++;
    if (!value)
      throw new AssertionError("check " + checks);
  }

  public static void main(String[] args) {
    short[] four = {1, 2, 3, 4, -5, -6, -7, -8, 9, 9, 9, 9};
    short[] six = new short[18];
    java.util.Arrays.fill(six, (short) 77);
    SurroundPcm.fivePointOne(four, six, 2);
    check(six[0] == 1 && six[1] == 2 && six[2] == 0 && six[3] == 0 && six[4] == 3 && six[5] == 4);
    check(six[6] == -5 && six[7] == -6 && six[8] == 0 && six[9] == 0 && six[10] == -7 && six[11] == -8);
    check(six[12] == 77);  // only `frames` frames
    System.out.println("SurroundPcmTests: " + checks + " checks passed");
  }
}
