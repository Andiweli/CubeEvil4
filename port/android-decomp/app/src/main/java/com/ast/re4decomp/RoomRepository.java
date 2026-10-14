package com.ast.re4decomp;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.Reader;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Checked original room inputs and decoded cache, independent of Android/JNI.
 */
public final class RoomRepository {
  @FunctionalInterface
  public interface Decoder {
    void decode(File source, File output) throws IOException;
  }

  private static final long MAX_BYTES = 64L * 1024 * 1024;
  private static final int MAX_ROWS = 4096, MAX_LINE = 512,
                           MAX_CATALOG_CHARS = 512 * 1024;
  private static final Pattern ROOM = Pattern.compile("r[0-9a-f]{3}");
  private static final Pattern PATH =
      Pattern.compile("St([0-9a-f])/(r([0-9a-f])[0-9a-f]{2})\\.das");
  private static final Pattern BLOCK_PATH =
      Pattern.compile("St([0-9a-f])/(r([0-9a-f])[0-9a-f]{2}_[0-9a-f]{2})\\.dat");
  private static final Pattern SHA256 = Pattern.compile("[0-9a-f]{64}");
  private static final long ROOM_INFO_SIZE = 10790;
  // Extracted independently from the FST records on both verified original
  // GCMs.
  private static final String ROOM_INFO_SHA256 =
      "ee61ecc29ab8c21e1089774f65cc69db074d1cb02bd7937d5adf1f92b555f0b2";

  private static final class Entry {
    final int disc;
    final String path, decodedHash, sourceHash;
    final long size;
    Entry(int disc, String path, long size, String decodedHash,
          String sourceHash) {
      this.disc = disc;
      this.path = path;
      this.size = size;
      this.decodedHash = decodedHash;
      this.sourceHash = sourceHash;
    }
    boolean sameData(Entry other) {
      return path.equals(other.path) && size == other.size &&
          decodedHash.equals(other.decodedHash) &&
          sourceHash.equals(other.sourceHash);
    }
  }

  private final File root, cache;
  private final Decoder decoder;
  private final Map<String, List<Entry>> rooms = new HashMap<>();
  private final Map<String, List<Entry>> blocks = new TreeMap<>();

  public RoomRepository(File gameRoot, File cacheDir, Reader roomsTsv,
                        Decoder decoder) throws IOException {
    this(gameRoot, cacheDir, roomsTsv, null, decoder);
  }
  public RoomRepository(File gameRoot, File cacheDir, Reader roomsTsv,
                        Reader blocksTsv, Decoder decoder) throws IOException {
    if (gameRoot == null || cacheDir == null || roomsTsv == null ||
        decoder == null)
      throw new IOException("Room data: Missing configuration");
    root = gameRoot.getCanonicalFile();
    cache = cacheDir.getCanonicalFile();
    this.decoder = decoder;
    if ((!cache.isDirectory() && !cache.mkdirs()) || !cache.isDirectory())
      throw new IOException("Room cache is unavailable");
    readCatalog(roomsTsv, rooms, PATH);
    if (blocksTsv != null) readCatalog(blocksTsv, blocks, BLOCK_PATH);
  }

  private void readCatalog(Reader source, Map<String, List<Entry>> catalog,
                           Pattern paths) throws IOException {
    int[] count = {0};
    int rows = 0;
    try (BufferedReader reader = new BufferedReader(source)) {
      String line;
      while ((line = boundedLine(reader, count)) != null) {
        interrupted();
        if (line.isEmpty() || line.startsWith("#"))
          continue;
        if (++rows > MAX_ROWS)
          throw new IOException("Room catalog exceeds the limit");
        String[] fields = line.split("\\t", -1);
        if (fields.length != 5 ||
            !(fields[0].equals("1") || fields[0].equals("2")) ||
            !SHA256.matcher(fields[3]).matches() ||
            !SHA256.matcher(fields[4]).matches() ||
            !fields[2].matches("[1-9][0-9]{0,8}"))
          throw new IOException("Invalid room catalog");
        Matcher path = paths.matcher(fields[1]);
        if (!path.matches() || !path.group(1).equals(path.group(3)))
          throw new IOException("Invalid room path in catalog");
        long size;
        try {
          size = Long.parseLong(fields[2]);
        } catch (NumberFormatException e) {
          throw new IOException("Invalid room size", e);
        }
        if (size < 16 || size > MAX_BYTES)
          throw new IOException("Room size exceeds the limit");
        Entry entry = new Entry(Integer.parseInt(fields[0]), fields[1], size,
                                fields[3], fields[4]);
        // GameCube .dat block archives are already decompressed. Both
        // catalog hashes must describe the same unmodified bytes.
        if (paths == BLOCK_PATH && !entry.decodedHash.equals(entry.sourceHash))
          throw new IOException("Room block differs from the original");
        String id = path.group(2);
        List<Entry> alternatives = catalog.get(id);
        if (alternatives == null) {
          alternatives = new ArrayList<>();
          catalog.put(id, alternatives);
        }
        for (Entry previous : alternatives)
          if (previous.disc == entry.disc || !previous.sameData(entry))
            throw new IOException("Inconsistent room catalog: " + id);
        alternatives.add(entry);
      }
    }
    if (catalog.isEmpty())
      throw new IOException("Empty room catalog");
  }

  private static String boundedLine(BufferedReader reader, int[] total)
      throws IOException {
    StringBuilder line = new StringBuilder();
    int c;
    while ((c = reader.read()) != -1) {
      if (++total[0] > MAX_CATALOG_CHARS)
        throw new IOException("Room catalog exceeds the limit");
      if (c == '\n')
        break;
      if (line.length() >= MAX_LINE)
        throw new IOException("Room catalog row exceeds the limit");
      line.append((char)c);
    }
    if (c == -1 && line.length() == 0)
      return null;
    if (line.length() > 0 && line.charAt(line.length() - 1) == '\r')
      line.setLength(line.length() - 1);
    return line.toString();
  }

  /**
   * Called by a worker; returns only a size/hash-checked original decoded
   * room.
   */
  public synchronized File prepare(String roomId) throws IOException {
    interrupted();
    if (roomId == null || !ROOM.matcher(roomId).matches())
      throw new IOException("Invalid room ID");
    List<Entry> entries = rooms.get(roomId);
    if (entries == null)
      throw new IOException("Room missing from verified catalog: " + roomId);
    File room = prepareChecked(roomId, entries);
    // A caller sees the bundle only after every known source/decoded block
    // has passed its independent SHA-256, including on cached room opens.
    for (Map.Entry<String, List<Entry>> block : blocks.entrySet())
      if (block.getKey().startsWith(roomId + "_"))
        prepareChecked(block.getKey(), block.getValue());
    return room;
  }
  private File prepareChecked(String roomId, List<Entry> entries) throws IOException {
    Entry expected = entries.get(0);
    File target = cacheFile("room-" + roomId + ".dat");
    if (valid(target, expected.size, expected.decodedHash))
      return target;
    File source = checkedSource(entries);
    File stage = File.createTempFile("room-" + roomId + "-", ".tmp", cache);
    try {
      checkedChild(cache, stage.getName());
      if (roomId.indexOf('_') >= 0)
        copyBlock(source, stage, expected.size);
      else
        decoder.decode(source, stage);
      interrupted();
      if (!stage.getCanonicalFile().equals(stage.getAbsoluteFile()) ||
          !valid(stage, expected.size, expected.decodedHash))
        throw new IOException("Decoded room data differs from the original");
      // Flush the verified staging file before the same-filesystem rename. File
      // rename uses Linux rename(2) on API21; an unsuccessful rename keeps the
      // previous cache untouched. No delete-before-rename window is introduced.
      try (FileOutputStream out = new FileOutputStream(stage, true)) {
        out.getFD().sync();
      }
      interrupted();
      cacheFile(target.getName());
      if (!stage.renameTo(target))
        throw new IOException("Could not save room cache");
      return target;
    } finally {
      if (stage.exists() && !stage.delete())
        stage.deleteOnExit();
    }
  }

  private static void copyBlock(File source, File output, long expectedSize)
      throws IOException {
    long total = 0;
    byte[] buffer = new byte[65536];
    try (InputStream input = new FileInputStream(source);
         FileOutputStream out = new FileOutputStream(output)) {
      int count;
      while ((count = input.read(buffer)) != -1) {
        interrupted();
        total += count;
        if (total > expectedSize)
          throw new IOException("Room block changed while loading");
        out.write(buffer, 0, count);
      }
    }
    if (total != expectedSize)
      throw new IOException("Room block is truncated");
  }

  private File checkedSource(List<Entry> entries) throws IOException {
    IOException last = null;
    for (Entry entry : entries) {
      try {
        File source =
            checkedChild(root, "disc" + entry.disc + "/files/" + entry.path);
        if (!source.isFile())
          throw new IOException("Please import Disc " + entry.disc + " before opening this room");
        long size = source.length();
        if (size <= 0 || size > MAX_BYTES ||
            !digest(source, size).equals(entry.sourceHash))
          throw new IOException(
              "Room source does not match the verified disc");
        return source;
      } catch (IOException e) {
        interrupted();
        last = e;
      }
    }
    throw last == null ? new IOException("Room source is missing") : last;
  }

  /**
   * Prefer Disc 2, accepting Disc 1 only after the same original size/hash
   * check.
   */
  public static File checkedRoomInfo(File gameRoot) throws IOException {
    if (gameRoot == null)
      throw new IOException("Room start data: Import directory is missing");
    File root = gameRoot.getCanonicalFile();
    IOException last = null;
    for (int disc : new int[] {2, 1}) {
      try {
        File file =
            checkedChild(root, "disc" + disc + "/files/Debug/roominfo.dat");
        if (valid(file, ROOM_INFO_SIZE, ROOM_INFO_SHA256))
          return file;
        throw new IOException(
            "Original room start data is missing or modified");
      } catch (IOException e) {
        interrupted();
        last = e;
      }
    }
    throw last;
  }

  private File cacheFile(String name) throws IOException {
    File file = checkedChild(cache, name);
    if (!file.getCanonicalFile().equals(file.getAbsoluteFile()))
      throw new IOException("Invalid room cache path");
    return file;
  }

  private static File checkedChild(File directory, String relative)
      throws IOException {
    if (!directory.getCanonicalFile().equals(directory.getAbsoluteFile()))
      throw new IOException(
          "Room data directory changed while loading");
    File file = new File(directory, relative).getAbsoluteFile();
    String prefix = directory.getAbsolutePath() + File.separator;
    if (!file.getCanonicalPath().startsWith(prefix))
      throw new IOException("Room data path escapes the import/cache directory");
    return file;
  }

  private static boolean valid(File file, long size, String hash)
      throws IOException {
    interrupted();
    return file.isFile() && file.length() == size &&
        digest(file, size).equals(hash);
  }

  private static String digest(File file, long expectedSize)
      throws IOException {
    if (expectedSize < 0 || expectedSize > MAX_BYTES)
      throw new IOException("Room data exceeds the limit");
    MessageDigest hash;
    try {
      hash = MessageDigest.getInstance("SHA-256");
    } catch (NoSuchAlgorithmException e) {
      throw new IOException("SHA-256 is unavailable", e);
    }
    long total = 0;
    byte[] buffer = new byte[65536];
    try (InputStream input = new FileInputStream(file)) {
      int size;
      while ((size = input.read(buffer)) != -1) {
        interrupted();
        total += size;
        if (total > expectedSize)
          throw new IOException(
              "Room data changed during verification");
        hash.update(buffer, 0, size);
      }
    }
    if (total != expectedSize)
      throw new IOException("Room data changed during verification");
    StringBuilder text = new StringBuilder(64);
    for (byte value : hash.digest()) {
      int v = value & 255;
      text.append("0123456789abcdef".charAt(v >>> 4));
      text.append("0123456789abcdef".charAt(v & 15));
    }
    return text.toString();
  }

  private static void interrupted() throws IOException {
    if (Thread.currentThread().isInterrupted())
      throw new IOException("Room loading canceled");
  }
}
