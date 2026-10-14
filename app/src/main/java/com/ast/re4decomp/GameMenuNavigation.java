package com.ast.re4decomp;

/** Menu stick repeat and case geometry, independent of the Android event loop. */
public final class GameMenuNavigation {
  /** ss_pzzl::caseModelMove / puzzlePos2screenPos, settled source camera. */
  public static float[] casePoint(int columns, float x, float y) {
    double focal = 240 / Math.tan(Math.toRadians(10));
    double screen = 5000 / focal;
    double localX = (x - columns * .5) * 100, localY = -y * 100;
    double worldX = -95 * screen + localX;
    double worldY = 98 * screen + Math.cos(-.57) * localY;
    double worldZ = -300 + Math.sin(-.57) * localY;
    return new float[]{(float)(320 + worldX * focal / (5000 - worldZ)),
                       (float)(240 - worldY * focal / (5000 - worldZ))};
  }
  public static float[] caseQuad(int columns, int x, int y, int width, int height) {
    float[] result = new float[8];
    float[][] points = {casePoint(columns,x,y),casePoint(columns,x+width,y),
        casePoint(columns,x+width,y+height),casePoint(columns,x,y+height)};
    for (int i=0;i<4;++i) { result[i*2]=points[i][0]; result[i*2+1]=points[i][1]; }
    return result;
  }
  private int held;
  private long next;
  public void reset() { held = 0; next = 0; }
  /** Returns -1/+1 horizontally, -2/+2 vertically, or zero until repeat time. */
  public int sample(float x, float y, long now) {
    if (Float.isNaN(x) || Float.isInfinite(x) || Float.isNaN(y) || Float.isInfinite(y)) { reset(); return 0; }
    int direction = 0;
    if (Math.max(Math.abs(x), Math.abs(y)) >= .55f)
      direction = Math.abs(x) > Math.abs(y) ? (x < 0 ? -1 : 1) : (y < 0 ? -2 : 2);
    if (direction == 0) { reset(); return 0; }
    if (direction != held) { held = direction; next = now + 320; return direction; }
    if (now < next) return 0;
    next = now + 130;
    return direction;
  }
  /** game/puzzle.cpp::pzlPlayer::selPiece: leave the current piece along
   * the requested row/column; an empty cell moves exactly one cell. */
  public static int caseCursor(int[] board,int columns,int rows,int cell,int dx,int dy) {
    if(dx==0 && dy==0)return cell;
    int x=cell%columns,y=cell/columns,original=board[cell];
    for(;;){int nx=x+dx,ny=y+dy;
      if(nx<0 || nx>=columns || ny<0 || ny>=rows)return cell;
      x=nx;y=ny;int next=y*columns+x;
      if(original<0 || board[next]!=original)return next;
    }
  }
  public static boolean fits(int columns, int rows, int x, int y, int w, int h) {
    return w > 0 && h > 0 && x >= 0 && y >= 0 && x <= columns - w && y <= rows - h;
  }
  public static boolean overlaps(int x, int y, int w, int h, int bx, int by, int bw, int bh) {
    return x < bx + bw && x + w > bx && y < by + bh && y + h > by;
  }
  public static int clampPosition(int position, int extent, int limit) {
    return Math.max(0, Math.min(Math.max(0, limit - extent), position));
  }
  /** Source itemInfo.type: type 10 files have their own reader, not key-item slots. */
  public static boolean keyOrTreasure(int x, int y, int kind) {
    return x < 0 && y < 0 && (kind == 0 || kind == 7 || kind == 5 || kind == 12);
  }
}
