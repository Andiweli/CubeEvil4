package com.ast.re4decomp;

/**
 * Device selection rules, independent of Android so they can be host-tested.
 */
public final class ControllerPolicy {
  private int active = -1, activeRank = -1;
  private boolean selected;
  public static int rank(boolean virtual, boolean sticks) {
    return sticks ? (virtual ? 2 : 3) : (virtual ? 0 : 1);
  }
  public boolean accept(int id, int rank, boolean physicalSticksAvailable) {
    // Synthetic navigation must not lock out a connected real game controller.
    if (rank == 0 && physicalSticksAvailable)
      return false;
    if (selected && active == id)
      return true;
    if (activeRank >= rank)
      return false;
    active = id;
    activeRank = rank;
    selected = true;
    return true;
  }
  public int active() { return active; }
  public void reset() {
    active = -1;
    activeRank = -1;
    selected = false;
  }
}
