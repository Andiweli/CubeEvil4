import com.ast.re4decomp.RoomRepository;
import java.io.File;
import java.io.IOException;
import java.io.StringReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.Arrays;

/** Host-only repository checks; no Android or JNI dependency. */
public final class RoomRepositoryTest {
  private static int checks;
  private interface Attempt {
    void run() throws Exception;
  }
  private static void check(boolean value, String label) {
    if (!value)
      throw new AssertionError(label);
    checks++;
  }
  private static void rejected(Attempt action, String label) throws Exception {
    try {
      action.run();
    } catch (IOException expected) {
      checks++;
      return;
    }
    throw new AssertionError(label);
  }
  private static String hash(byte[] bytes) throws Exception {
    StringBuilder text = new StringBuilder();
    for (byte b : MessageDigest.getInstance("SHA-256").digest(bytes)) {
      int v = b & 255;
      text.append("0123456789abcdef".charAt(v >>> 4));
      text.append("0123456789abcdef".charAt(v & 15));
    }
    return text.toString();
  }
  private static String row(int disc, String path, byte[] source,
                            byte[] decoded) throws Exception {
    return disc + "\t" + path + "\t" + decoded.length + "\t" + hash(decoded) +
        "\t" + hash(source) + "\n";
  }
  private static File write(File root, String path, byte[] data)
      throws Exception {
    File file = new File(root, path);
    Files.createDirectories(file.getParentFile().toPath());
    Files.write(file.toPath(), data);
    return file;
  }
  private static RoomRepository repository(File root, File cache, String rows,
                                           RoomRepository.Decoder decoder)
      throws IOException {
    return new RoomRepository(root, cache, new StringReader(rows), decoder);
  }
  private static void delete(Path file) throws Exception {
    if (Files.isDirectory(file) && !Files.isSymbolicLink(file)) {
      try (java.nio.file.DirectoryStream<Path> children =
               Files.newDirectoryStream(file)) {
        for (Path child : children)
          delete(child);
      }
    }
    Files.deleteIfExists(file);
  }
  private static void noStages(File cache) {
    String[] names = cache.list();
    check(names != null, "cache directory available");
    for (String name : names)
      check(!name.endsWith(".tmp"), "staging files cleaned");
  }

  public static void main(String[] args) throws Exception {
    Path temp = Files.createTempDirectory("re4-room-repository-");
    try {
      File root = temp.resolve("game").toFile();
      File cache = temp.resolve("cache").toFile();
      byte[] source =
          "original-compressed-room".getBytes(StandardCharsets.US_ASCII);
      byte[] decoded =
          "checked-decoded-original-room".getBytes(StandardCharsets.US_ASCII);
      byte[] other =
          "another-checked-original-room".getBytes(StandardCharsets.US_ASCII);
      String r100 = row(1, "St1/r100.das", source, decoded);
      String r101 = row(1, "St1/r101.das", source, other);
      File input = write(root, "disc1/files/St1/r100.das", source);
      write(root, "disc1/files/St1/r101.das", source);
      int[] calls = {0};
      RoomRepository repo =
          repository(root, cache, "# catalog\r\n" + r100 + r101, (src, out) -> {
            check(src.getCanonicalFile().equals(input.getCanonicalFile()),
                  "checked canonical source passed to decoder");
            calls[0]++;
            Files.write(out.toPath(), decoded);
          });
      File current = repo.prepare("r100");
      check(calls[0] == 1 &&
                Arrays.equals(Files.readAllBytes(current.toPath()), decoded),
            "verified initial decode");
      check(repo.prepare("r100").equals(current) && calls[0] == 1,
            "verified cache hit skips decoder");
      // A main-room cache hit must still prepare all BLK dependencies. Raw
      // .dat blocks never go through the Yz2 decoder used for .das rooms.
      File blockRoot = temp.resolve("block-game").toFile();
      File blockCache = temp.resolve("block-cache").toFile();
      write(blockRoot, "disc1/files/St1/r100.das", source);
      byte[] block0 = "checked-original-raw-block0".getBytes(StandardCharsets.US_ASCII);
      byte[] block1 = "checked-original-raw-block1".getBytes(StandardCharsets.US_ASCII);
      String blockRows = row(1, "St1/r100_00.dat", block0, block0) +
                         row(1, "St1/r100_01.dat", block1, block1);
      write(blockRoot, "disc1/files/St1/r100_00.dat", block0);
      int[] blockCalls = {0};
      RoomRepository bundle = new RoomRepository(blockRoot, blockCache,
          new StringReader(r100), new StringReader(blockRows), (src, out) -> {
            blockCalls[0]++;
            check(src.getName().endsWith(".das"), "raw block bypasses Yz2 decoder");
            Files.write(out.toPath(), decoded);
          });
      rejected(() -> bundle.prepare("r100"), "missing block rejects complete room activation");
      check(blockCalls[0] == 1 && new File(blockCache, "room-r100.dat").isFile(),
            "verified main cache can be reused after incomplete bundle");
      File secondBlock = write(blockRoot, "disc1/files/St1/r100_01.dat", block1);
      bundle.prepare("r100");
      check(blockCalls[0] == 1 && Arrays.equals(Files.readAllBytes(
          new File(blockCache, "room-r100_01.dat").toPath()), block1),
            "cached room still publishes its missing checked sibling block");
      Files.write(new File(blockCache, "room-r100_01.dat").toPath(), block0);
      Files.write(secondBlock.toPath(), block0);
      rejected(() -> bundle.prepare("r100"), "wrong same-sized block hash rejected");
      Files.write(secondBlock.toPath(), block1);
      bundle.prepare("r100");
      check(Arrays.equals(Files.readAllBytes(new File(blockCache,
          "room-r100_01.dat").toPath()), block1), "corrupt block cache repairs atomically");
      noStages(blockCache);
      rejected(() -> new RoomRepository(blockRoot, blockCache,
          new StringReader(r100), new StringReader(row(1,
          "St1/r100_00.dat", block0, block1)), (src, out) -> {}),
          "raw block catalog cannot request a decoding transformation");
      File blockOutside = write(temp.toFile(), "block-outside.dat", block1);
      Files.delete(new File(blockCache, "room-r100_01.dat").toPath());
      Files.createSymbolicLink(new File(blockCache, "room-r100_01.dat").toPath(),
          blockOutside.toPath());
      rejected(() -> bundle.prepare("r100"), "block cache symlink escape rejected");
      Files.write(input.toPath(), new byte[] {1});
      check(repo.prepare("r100").equals(current) && calls[0] == 1,
            "known-original cache remains useful independent of source");
      Files.write(input.toPath(), source);
      for (String id : new String[] {"r999", "R100", "r10A", "r100/../r101",
                                     "../r100", "r100.das", "r100\n", ""})
        rejected(() -> repo.prepare(id), "invalid or unknown room id rejected");
      rejected(() -> repo.prepare(null), "null room id rejected");
      RoomRepository badDecode =
          repository(root, cache, r100 + r101,
                     (src, out) -> Files.write(out.toPath(), decoded));
      rejected(() -> badDecode.prepare("r101"), "wrong decoded hash rejected");
      check(Arrays.equals(Files.readAllBytes(current.toPath()), decoded),
            "failed next-room decode preserves current room");
      check(!new File(cache, "room-r101.dat").exists(),
            "bad output never published");
      noStages(cache);
      RoomRepository thrownDecode =
          repository(root, cache, r100 + r101, (src, out) -> {
            Files.write(out.toPath(), other);
            throw new IOException("decode failed");
          });
      rejected(
          () -> thrownDecode.prepare("r101"), "decoder failure propagated");
      check(Arrays.equals(Files.readAllBytes(current.toPath()), decoded),
            "decoder exception preserves current room");
      noStages(cache);
      RoomRepository wrongSize =
          repository(root, cache, r100 + r101,
                     (src, out) -> Files.write(out.toPath(), new byte[] {0}));
      rejected(() -> wrongSize.prepare("r101"), "wrong decoded size rejected");
      noStages(cache);
      RoomRepository cancelledDecode =
          repository(root, cache, r100 + r101, (src, out) -> {
            Files.write(out.toPath(), other);
            Thread.currentThread().interrupt();
          });
      try {
        rejected(()
                     -> cancelledDecode.prepare("r101"),
                 "cancellation after decode prevents publication");
      } finally {
        Thread.interrupted();
      }
      check(!new File(cache, "room-r101.dat").exists(),
            "cancelled output never published");
      noStages(cache);
      File previous = write(cache, "room-r101.dat", decoded);
      rejected(()
                   -> thrownDecode.prepare("r101"),
               "bad replacement decode rejected");
      check(Arrays.equals(Files.readAllBytes(previous.toPath()), decoded),
            "existing destination preserved before successful commit");
      RoomRepository good101 =
          repository(root, cache, r100 + r101,
                     (src, out) -> Files.write(out.toPath(), other));
      check(Arrays.equals(Files.readAllBytes(good101.prepare("r101").toPath()),
                          other),
            "verified replacement committed by rename");
      Files.write(current.toPath(), new byte[decoded.length]);
      repo.prepare("r100");
      check(calls[0] == 2, "cache content corruption forces checked re-decode");
      Files.delete(current.toPath());
      Files.write(input.toPath(), new byte[] {1});
      rejected(() -> repo.prepare("r100"), "source hash mismatch rejected");
      check(calls[0] == 2, "invalid source never reaches decoder");
      Files.write(input.toPath(), source);
      Thread.currentThread().interrupt();
      try {
        rejected(() -> repo.prepare("r100"), "interrupted work rejected");
      } finally {
        Thread.interrupted();
      }

      RoomRepository.Decoder unused = (src, out) -> {
        throw new AssertionError("unexpected decoder call");
      };
      String h = hash(source), d = hash(decoded);
      String[] malformed = {"",
                            "1\tSt1/r100.das\t16\t" + d + "\n",
                            "3\tSt1/r100.das\t16\t" + d + "\t" + h,
                            "1\t../St1/r100.das\t16\t" + d + "\t" + h,
                            "1\t/St1/r100.das\t16\t" + d + "\t" + h,
                            "1\tSt1\\r100.das\t16\t" + d + "\t" + h,
                            "1\tSt2/r100.das\t16\t" + d + "\t" + h,
                            "1\tSt1/r10A.das\t16\t" + d + "\t" + h,
                            "1\tSt1/r100.das\t-16\t" + d + "\t" + h,
                            "1\tSt1/r100.das\t67108865\t" + d + "\t" + h,
                            "1\tSt1/r100.das\t16\t" + d.toUpperCase() + "\t" +
                                h,
                            "1\tSt1/r100.das\t16\t" + d + "\tbad",
                            r100 + r100,
                            r100 + row(2, "St1/r100.das", source, other),
                            "#"
                                + "x".repeat(513),
                            "\n".repeat(512 * 1024 + 1)};
      for (String rows : malformed)
        rejected(()
                     -> repository(root, cache, rows, unused),
                 "malformed catalog rejected");
      repository(root, cache, r100 + row(2, "St1/r100.das", source, decoded),
                 unused);
      checks++;
      File altRoot = temp.resolve("alternative-game").toFile();
      write(altRoot, "disc2/files/St1/r100.das", source);
      RoomRepository alternative = repository(
          altRoot, temp.resolve("alternative-cache").toFile(),
          r100 + row(2, "St1/r100.das", source, decoded), (src, out) -> {
            check(src.getPath().contains("disc2"),
                  "available duplicate disc selected");
            Files.write(out.toPath(), decoded);
          });
      alternative.prepare("r100");
      checks++;
      write(altRoot, "disc1/files/St1/r100.das", new byte[] {0});
      File alternativeCache = temp.resolve("alternative-cache").toFile();
      Files.delete(new File(alternativeCache, "room-r100.dat").toPath());
      alternative.prepare("r100");
      checks++;

      File outside = temp.resolve("outside").toFile();
      Files.createDirectories(outside.toPath());
      write(outside, "r100.das", source);
      File escapedRoot = temp.resolve("escaped-game").toFile();
      Files.createDirectories(new File(escapedRoot, "disc1/files").toPath());
      Files.createSymbolicLink(
          new File(escapedRoot, "disc1/files/St1").toPath(), outside.toPath());
      RoomRepository sourceEscape = repository(
          escapedRoot, temp.resolve("escaped-cache").toFile(), r100, unused);
      rejected(
          () -> sourceEscape.prepare("r100"), "source symlink escape rejected");
      File replacedRoot = temp.resolve("replaced-game").toFile();
      Files.createDirectories(replacedRoot.toPath());
      RoomRepository replaced = repository(
          replacedRoot, temp.resolve("replaced-cache").toFile(), r100, unused);
      Files.delete(replacedRoot.toPath());
      Files.createSymbolicLink(replacedRoot.toPath(), altRoot.toPath());
      rejected(()
                   -> replaced.prepare("r100"),
               "replacement of import root with symlink rejected");
      File outsideCache = write(outside, "room-cache.dat", decoded);
      File cacheLink = new File(cache, "room-r100.dat");
      Files.createSymbolicLink(cacheLink.toPath(), outsideCache.toPath());
      rejected(() -> repo.prepare("r100"), "cache symlink escape rejected");
      check(Arrays.equals(Files.readAllBytes(outsideCache.toPath()), decoded),
            "outside cache file untouched");

      rejected(()
                   -> RoomRepository.checkedRoomInfo(root),
               "missing roominfo rejected");
      write(root, "disc2/files/Debug/roominfo.dat", new byte[10790]);
      rejected(()
                   -> RoomRepository.checkedRoomInfo(root),
               "same-sized wrong roominfo rejected");
      if (args.length >= 1) {
        byte[] roomInfo = Files.readAllBytes(new File(args[0]).toPath());
        File first = write(root, "disc1/files/Debug/roominfo.dat", roomInfo);
        check(RoomRepository.checkedRoomInfo(root).equals(first),
              "verified disc1 roominfo fallback");
        File second = write(root, "disc2/files/Debug/roominfo.dat", roomInfo);
        check(RoomRepository.checkedRoomInfo(root).equals(second),
              "verified disc2 roominfo preferred");
        Files.write(second.toPath(), new byte[10790]);
        check(RoomRepository.checkedRoomInfo(root).equals(first),
              "corrupt disc2 roominfo uses verified fallback");
      }
      if (args.length >= 2) {
        try (java.io.Reader reader =
                 Files.newBufferedReader(new File(args[1]).toPath())) {
          new RoomRepository(root, cache, reader, unused);
        }
        checks++;
      }
      if (args.length >= 3) {
        try (java.io.Reader roomsReader = Files.newBufferedReader(new File(args[1]).toPath());
             java.io.Reader blocksReader = Files.newBufferedReader(new File(args[2]).toPath())) {
          new RoomRepository(root, temp.resolve("real-catalog-cache").toFile(),
                             roomsReader, blocksReader, (src, out) -> {});
        }
        checks++;
      }
      System.out.println("Java RoomRepository tests: " + checks +
                         " checks PASS");
    } finally {
      delete(temp);
    }
  }
}
