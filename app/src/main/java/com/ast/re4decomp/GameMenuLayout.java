package com.ast.re4decomp;

/** Full viewport menu geometry. Artwork/content scale uniformly; spare space
 * belongs to flexible panels and gaps instead of stretching Leon or items. */
public final class GameMenuLayout {
  // CORE_EXAMINE_ID's horizontal borders are at source y=+/-176.
  public static final float PICKUP_HEIGHT=352;
  public static final float PICKUP_TOP=(480-PICKUP_HEIGHT)*.5f;
  public boolean puzzleActive;
  public final float scale, width, height, extraX, extraY, top, visibleHeight;
  public GameMenuLayout(float pixelsWide, float pixelsHigh) {
    this(pixelsWide,pixelsHigh,480);
  }
  public GameMenuLayout(float pixelsWide, float pixelsHigh,float sourceHeight) {
    this(pixelsWide,pixelsHigh,sourceHeight,0);
  }
  public GameMenuLayout(float pixelsWide,float pixelsHigh,float sourceHeight,float sourceTop) {
    if (!Float.isFinite(pixelsWide) || !Float.isFinite(pixelsHigh) || pixelsWide <= 0 || pixelsHigh <= 0)
      throw new IllegalArgumentException("Menu viewport");
    if(!Float.isFinite(sourceHeight) || sourceHeight<=0)throw new IllegalArgumentException("Menu source height");
    if(!Float.isFinite(sourceTop) || sourceTop<0)throw new IllegalArgumentException("Menu source top");
    scale = Math.min(pixelsWide / 640, pixelsHigh / sourceHeight);
    top=sourceTop;visibleHeight=pixelsHigh/scale;
    width = pixelsWide / scale; height = visibleHeight+2*top;
    extraX = Math.max(0, width - 640); extraY = Math.max(0, visibleHeight - sourceHeight);
  }
  public float centerX() { return width * .5f; }
  public float right(float sourceX) { return sourceX + extraX; }
  public float bottom(float sourceY) { return sourceY + extraY; }
  /** ss_cmmn entry0xC is a640x360 frame at source y60..420. Only
   * its decorative centre expands; unused source480-line margins do not. */
  public float[][] frameSlices() {
    final float[] sx={0,300,430,640},sy={60,100,384,420};
    final float[] dx={0,300,width-210,width},dy=top==60?
        new float[]{60,100,384+extraY,420+extraY}:new float[]{0,110,height-91,height};
    float[][] slices=new float[9][8];int n=0;
    for(int y=0;y<3;++y)for(int x=0;x<3;++x)
      slices[n++]=new float[]{sx[x],sy[y],sx[x+1],sy[y+1],dx[x],dy[y],dx[x+1],dy[y+1]};
    return slices;
  }
  /** Original main-menu marked parent units enable exactly one highlight. */
  public static boolean tabParentVisible(int mark,int activeTab) {
    return mark<0 || mark>4 || mark==activeTab;
  }
  public static boolean mapCaptionVisible(int mark,boolean entire) {
    return mark==0?!entire:mark==1?entire:mark!=2;
  }
  public float tabShift(int tab) { return extraX * Math.max(0, Math.min(4, tab)) / 4; }
  /** Captions are bitmap ID units, not a replacement font. The original
   * visible frame is640x360; wide displays can use its vertical scale while
   * a narrow display still constrains the row to its authored640 width. */
  public float captionScale() { return Math.min(width / 640, height / 360); }
  public float tabCenter(int tab) {
    final float[] centers={144,266,360,437,515};
    return centers[Math.max(0,Math.min(4,tab))];
  }
  public float[] tabBounds(int tab) {
    final float[] centers = {144, 266, 360, 437, 515};
    float left = tab == 0 ? 80 : (centers[tab - 1] + tabShift(tab - 1) + centers[tab] + tabShift(tab)) * .5f;
    float right = tab == 4 ? width - 83 : (centers[tab] + tabShift(tab) + centers[tab + 1] + tabShift(tab + 1)) * .5f;
    return new float[]{left,57,right,107};
  }
  public float keyColumnWidth() { return (width - 80) * .5f; }
  public float keyColumnLeft(int column) { return 28 + Math.max(0, Math.min(1, column)) * (keyColumnWidth() + 24); }
  public int listRows(float top, float bottom, float step, int maximum) {
    return Math.max(1, Math.min(maximum, (int)Math.floor((bottom - top) / step)));
  }
  public float[] modelBounds() { return new float[]{100,110,width-100,height-110}; }
  public float[] normalized(float[] bounds) {
    return new float[]{bounds[0]/width,(bounds[1]-top)/visibleHeight,(bounds[2]-bounds[0])/width,(bounds[3]-bounds[1])/visibleHeight};
  }
  /** Same scale for x and y, with the case centre inside the flexible left
   * panel. The fixed portrait column stays at the right/bottom screen edge. */
  public float[] caseTransform(int columns, int rows) {
    float[] q=GameMenuNavigation.caseQuad(columns,0,0,columns,rows);
    float lowX=q[0],highX=q[0],lowY=q[1],highY=q[1];
    for(int j=1;j<4;++j) { lowX=Math.min(lowX,q[j*2]); highX=Math.max(highX,q[j*2]); lowY=Math.min(lowY,q[j*2+1]); highY=Math.max(highY,q[j*2+1]); }
    float availableWidth=width-240,availableHeight=height-230;
    float fit=Math.min(availableWidth/(highX-lowX+36),availableHeight/(highY-lowY+36));
    float preferred=1+Math.min(.30f,extraX/640)+Math.min(.30f,extraY/480);
    float zoom=Math.max(.5f,Math.min(preferred,fit));
    if(extraX<.01f && extraY<.01f) zoom=1; // exact source framing at4:3
    float sourceX=(lowX+highX)*.5f,sourceY=(lowY+highY)*.5f;
    return new float[]{zoom,sourceX+extraX*.5f-sourceX*zoom,sourceY+extraY*.5f-sourceY*zoom};
  }
  public float[] casePoint(int columns,int rows,float x,float y) {
    float[] point=GameMenuNavigation.casePoint(columns,x,y),transform=caseTransform(columns,rows);
    return new float[]{point[0]*transform[0]+transform[1],point[1]*transform[0]+transform[2]};
  }
  public float[] caseQuad(int columns,int rows,int x,int y,int w,int h) {
    float[] q=GameMenuNavigation.caseQuad(columns,x,y,w,h),transform=caseTransform(columns,rows);
    for(int j=0;j<4;++j) { q[j*2]=q[j*2]*transform[0]+transform[1]; q[j*2+1]=q[j*2+1]*transform[0]+transform[2]; }
    return q;
  }
  /** Inverse of the source camera and the uniform panel transform; avoids
   * ambiguous bounding rectangles along the tilted grid's sloping edges. */
  public int[] caseCell(int columns,int rows,float x,float y) {
    if(!Float.isFinite(x) || !Float.isFinite(y))return null;
    float[] transform=caseTransform(columns,rows);
    double sourceX=(x-transform[1])/transform[0],sourceY=(y-transform[2])/transform[0];
    double focal=240/Math.tan(Math.toRadians(10)),screen=5000/focal;
    double vertical=240-sourceY,sine=Math.sin(-.57),cosine=Math.cos(-.57);
    double localY=(vertical*5300-focal*98*screen)/(vertical*sine+focal*cosine);
    double depth=5300-sine*localY;
    double column=((sourceX-320)*depth/focal+95*screen)/100+columns*.5;
    double row=-localY/100;
    if(!Double.isFinite(column) || !Double.isFinite(row) || column<0 || row<0 || column>=columns || row>=rows)return null;
    return new int[]{(int)Math.floor(column),(int)Math.floor(row)};
  }
  /** caseModelMove gives the space its own x=140 screen-unit anchor;
   * its y/z are the case board plus two rows. Never refit the case for it. */
  public float[] spaceQuad(int columns,int rows,int x,int y,int w,int h){
    float[] t=caseTransform(columns,rows),result=new float[8];
    double focal=240/Math.tan(Math.toRadians(10)),screen=5000/focal;
    int[] xs={x,x+w,x+w,x},ys={y,y,y+h,y+h};
    // Pin the source board to the right edge on wider displays. Preserve
    // the original source-screen clipping instead of shrinking both boards.
    float shift=extraX-t[0]*640+640;
    for(int n=0;n<4;++n){double localY=(2-ys[n])*100;
      double depth=5300-Math.sin(-.57)*localY;
      result[n*2]=(float)(320+(140*screen+xs[n]*100)*focal/depth)*t[0]+shift;
      result[n*2+1]=(float)(240-(98*screen+Math.cos(-.57)*localY)*focal/depth)*t[0]+t[2];
    }return result;
  }
  public int[] spaceCell(int columns,int rows,float x,float y){
    for(int sy=0;sy<12;++sy)for(int sx=0;sx<6;++sx)if(insideQuad(spaceQuad(columns,rows,sx,sy,1,1),x,y))return new int[]{sx,sy};
    return null;
  }
  public static boolean insideQuad(float[] q,float x,float y) {
    if(q==null || q.length!=8 || !Float.isFinite(x) || !Float.isFinite(y))return false;
    boolean positive=false,negative=false;
    for(int i=0;i<4;++i) {
      int j=(i+1)%4;
      float cross=(q[j*2]-q[i*2])*(y-q[i*2+1])-(q[j*2+1]-q[i*2+1])*(x-q[i*2]);
      if(cross>.001f)positive=true;else if(cross<-.001f)negative=true;
      if(positive && negative)return false;
    }
    return positive || negative;
  }
}
