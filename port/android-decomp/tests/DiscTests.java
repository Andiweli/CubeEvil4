import com.ast.re4decomp.disc.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.*;

public final class DiscTests {
  private static int checks;
  private static void check(boolean ok, String m) {
    if (!ok)
      throw new AssertionError(m);
    checks++;
  }
  private static void put32(byte[] b, int o, long v) {
    for (int i = 0; i < 4; i++)
      b[o + i] = (byte)(v >> (24 - 8 * i));
  }
  private static byte[] synthetic(String name) {
    byte[] b = new byte[0x1000];
    System.arraycopy("G4BE08".getBytes(StandardCharsets.US_ASCII), 0, b, 0, 6);
    put32(b, 0x1c, 0xc2339f3dL);
    put32(b, 0x420, 0x600);
    put32(b, 0x424, 0x500);
    put32(b, 0x428, 64);
    b[0x500] = 1;
    put32(b, 0x508, 2);
    put32(b, 0x510, 0x900);
    put32(b, 0x514, 1);
    byte[] n = name.getBytes(StandardCharsets.US_ASCII);
    System.arraycopy(n, 0, b, 0x518, n.length);
    return b;
  }
  private static void rejected(File dir, byte[] b, String label)
      throws Exception {
    File f = new File(dir, "bad.gcm");
    Files.write(f.toPath(), b);
    boolean fail = false;
    try (GameCubeDisc ignored = new GameCubeDisc(f)) {
    } catch (IOException e) {
      fail = true;
    }
    check(fail, label);
  }
  public static void main(String[] args) throws Exception {
    File tmp = Files.createTempDirectory("re4-disc-tests").toFile();
    try {
      File good = new File(tmp, "good.gcm");
      Files.write(good.toPath(), synthetic("a"));
      try (GameCubeDisc d = new GameCubeDisc(good)) {
        check(d.discNumber == 1 && d.files.size() == 2, "synthetic parser");
      }
      rejected(tmp, synthetic(".."), "path traversal rejected");
      byte[] b = synthetic("a");
      put32(b, 0x508, 100000);
      rejected(tmp, b, "bad entry count");
      b = synthetic("a");
      put32(b, 0x510, 0xffffff00L);
      rejected(tmp, b, "file out of bounds");
      b = synthetic("a");
      put32(b, 0x424, 0xffffff00L);
      rejected(tmp, b, "FST out of bounds");
      b = synthetic("a");
      b[0] = 'P';
      rejected(tmp, b, "wrong disc id");
      rejected(tmp, new byte[100], "short file");
      try (GameCubeDisc d = new GameCubeDisc(good)) {
        File dst = new File(tmp, "cancelled");
        boolean fail = false;
        try {
          d.extract(dst, (phase, done, total) -> {
            throw new IOException("cancel");
          });
        } catch (IOException e) {
          fail = true;
        }
        check(fail && !new File(dst, "ready.properties").exists(),
              "cancelled extraction never ready");
      }
      if (args.length == 4) {
        ReferenceCatalog refs =
            new ReferenceCatalog(new FileInputStream(args[0]));
        File root = new File(args[3]);
        DiscInstaller.recover(root);
        for (int n = 1; n <= 2; n++) {
          File source = new File(args[n]);
          int expectedFiles;
          try (GameCubeDisc d = new GameCubeDisc(source)) {
            d.validate(refs, (phase, done, total) -> {});
            check(d.discNumber == n, "real disc identity");
            // Both supplied images have exactly fourteen separate Japanese
            // fonts and five unused CARD language archives.
            expectedFiles = d.files.size() - 19;
          }
          DiscInstaller.install(source, root, n, refs,
                                (phase, done, total) -> {});
          check(DiscInstaller.ready(new File(root, "disc" + n), n),
                "real extraction ready");
          verifyManifest(new File(root, "disc" + n), expectedFiles);
          verifyEnglishAssets(new File(root, "disc" + n));
          System.out.println("Disc " + n +
                             (": reference validation, English extraction and " +
                              "SHA-256 manifest PASS"));
        }
        File current = new File(root, "disc1"),
             old = new File(root, ".old-disc1");
        check(current.renameTo(old), "simulate interrupted commit");
        DiscInstaller.recover(root);
        check(DiscInstaller.ready(current, 1), "old disc recovered");
        boolean aborted = false;
        try {
          DiscInstaller.install(new File(args[1]), root, 1, refs,
                                (phase, done, total) -> {
                                  if (phase.contains("Extracting"))
                                    throw new IOException("cancel");
                                });
        } catch (IOException e) {
          aborted = true;
        }
        check(aborted && DiscInstaller.ready(current, 1) &&
                  !new File(root, ".work-disc1").exists(),
              "cancel preserves installed disc");
        boolean wrong = false;
        try {
          DiscInstaller.install(new File(args[2]), root, 1, refs,
                                (phase, done, total) -> {});
        } catch (IOException e) {
          wrong = true;
        }
        check(wrong && DiscInstaller.ready(current, 1),
              "wrong disc preserves installed data");
      }
      System.out.println("Java disc tests: " + checks + " checks PASS");
    } finally {
      DiscInstaller.delete(tmp);
    }
  }
  private static void verifyEnglishAssets(File disc) {
    check(new File(disc, "files/ss/Cmn/save_e.dat").isFile(), "English CARD archive retained");
    check(new File(disc, "files/font/common_p.fnt").isFile(), "English font retained");
    check(new File(disc, "files/etc/core.das").isFile(), "Shared CORE archive retained");
    for (String language : new String[]{"j", "g", "f", "s", "i"})
      check(!new File(disc, "files/ss/Cmn/save_" + language + ".dat").exists(), "Unused CARD archive excluded");
    for (String name : new String[]{"common", "dvd", "event1", "event2", "event3", "event4",
                                   "ss_file", "ss_term", "sscrn", "stage1", "stage2", "stage3", "stage4", "system"})
      check(!new File(disc, "files/font/" + name + "_j.fnt").exists(), "Japanese font excluded");
    for (String name : new String[]{"tel0000_101_f.fcv", "tel0100_101_f.fcv"})
      check(new File(disc, "files/ss/Eng/" + name).isFile(), "English animation suffix retained");
  }
  private static void verifyManifest(File disc, int expectedFiles) throws Exception {
    try (BufferedReader r = new BufferedReader(
             new FileReader(new File(disc, "manifest.tsv")))) {
      String line;
      int n = 0;
      byte[] buffer = new byte[1024 * 1024];
      while ((line = r.readLine()) != null) {
        if (line.startsWith("#"))
          continue;
        String[] s = line.split("\t");
        File f = new File(disc, s[2]);
        check(f.length() == Long.parseLong(s[1]), "extracted size");
        java.security.MessageDigest md = GameCubeDisc.newDigest("SHA-256");
        try (InputStream input = new FileInputStream(f)) {
          int size;
          while ((size = input.read(buffer)) >= 0)
            if (size > 0)
              md.update(buffer, 0, size);
        }
        check(GameCubeDisc.hex(md.digest()).equals(s[0]), "extracted digest");
        n++;
      }
      check(n == expectedFiles, "Only the nineteen unused language assets excluded");
    }
  }
}
