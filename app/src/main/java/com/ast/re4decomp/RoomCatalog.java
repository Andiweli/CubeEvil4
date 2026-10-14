package com.ast.re4decomp;
import java.io.*;
import java.util.*;

/** Verified disc rows, cross-checked against st1/st2/st3/st4.cpp setTbl(). */
public final class RoomCatalog {
  public static final int CAMPAIGN = 0, EXTRAS = 1, ALL = 2;
  public static final class Entry {
    public final String id, path;
    public final int disc, stage;
    private Entry(String[] row) throws IOException {
      path = row[1];
      if (!path.matches("St[1-4]/r[1-4][0-9a-f]{2}\\.das")) throw new IOException("Invalid room path");
      id = path.substring(4, 8); stage = Integer.parseInt(id.substring(1, 2)); disc = Integer.parseInt(row[0]);
      if (path.charAt(2) != id.charAt(1)) throw new IOException("Invalid room mapping");
    }
    public boolean unused() { return id.equals("r313") || id.equals("r314") || id.equals("r322") || id.equals("r323") || id.equals("r324"); }
    public boolean campaign() { return stage >= 1 && stage <= 3 && !unused(); }
    public boolean explorable() { return !id.equals("r313") && !id.equals("r314") && !id.equals("r322") && !id.equals("r323"); }
    // r30f's bulldozer scenario installs its SAT area parts dynamically;
    // the initial file part alone provides no player floor in this runtime.
    public boolean walkable() { return explorable() && !id.equals("r30f"); }
    public boolean combatTest() { return id.equals("r400"); }
    public String area() { return stage == 1 ? "Village" : stage == 2 ? "Castle" : stage == 3 ? "Island" : "Extra modes"; }
    public String label() {
      String name = id.equals("r100") ? " · Forest path / First house" : id.equals("r101") ? " · Village square"
          : id.equals("r400") ? " · Mercenaries village" : "";
      return (campaign() ? "★ Campaign · " : unused() ? "Unused room data · " : "Extra mode · ") + area() + " · " + id + name;
    }
  }
  private final List<Entry> entries;
  public RoomCatalog(Reader source) throws IOException {
    Map<String, Entry> unique = new TreeMap<>();
    BufferedReader reader = new BufferedReader(source); String line;
    while ((line = reader.readLine()) != null) {
      if (line.isEmpty() || line.startsWith("#")) continue;
      String[] row = line.split("\t", -1);
      if (row.length != 5 || !row[0].matches("[12]") || !row[2].matches("[0-9]+") ||
          !row[3].matches("[a-f0-9]{64}") || !row[4].matches("[a-f0-9]{64}")) throw new IOException("Invalid room catalog");
      Entry entry = new Entry(row), previous = unique.get(entry.id);
      // Identical room resources on both discs: prefer Disc 1 like RoomRepository.
      if (previous == null || entry.disc < previous.disc) unique.put(entry.id, entry);
    }
    if (unique.isEmpty()) throw new IOException("Room catalog is empty");
    entries = Collections.unmodifiableList(new ArrayList<>(unique.values()));
  }
  public List<Entry> filter(int category) {
    if (category < CAMPAIGN || category > ALL) throw new IllegalArgumentException("Room filter");
    List<Entry> result = new ArrayList<>();
    for (Entry entry : entries) if (category == ALL || entry.campaign() == (category == CAMPAIGN)) result.add(entry);
    return result;
  }
}
