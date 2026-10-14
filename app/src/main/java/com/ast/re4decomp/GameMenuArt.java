package com.ast.re4decomp;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.RectF;
import android.graphics.Path;
import android.os.Handler;
import android.os.Looper;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import java.util.HashMap;
import java.util.HashSet;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.Map;

/** Original SS archive artwork decoded natively, cached for the life of a menu. */
final class GameMenuArt {
  // Frozen Sscrn/ss_item.cpp::itemTexNo. These values select a frame of the
  // layout-authored icon texture; they are not item IDs used as texture IDs.
  private static final int[] KEY_ICONS = {
    0xff,0xa2,0x3b,0x3c,0x3d,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,
    0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x6e,0x6f,0x70,0x77,0x82,0x83,0x84,0x85,
    0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8f,0x90,0x91,0xd2,0x93,0xd3,0xd4,0x96,
    0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,0x98,0xa1,0x74,0xa3,0xa4,0xa5,0xa6,0x0f,0xa7,0xb8,
    0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,0xc0,0xc1,0xc2,0xc3,0xc4,0x92,0x1d,0x1e,0x1f,
    0x39,0x80,0x3a,0x69,0x7a,0x7b,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
    0xd0,0xd1,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb
  };
  private static final LinkedHashMap<String, Bitmap> images = new LinkedHashMap<>(16, .75f, true);
  private static final HashMap<String, JSONObject> layouts = new HashMap<>();
  private final HashMap<JSONObject,MapTriangle> mapTriangles=new HashMap<>();
  private final HashSet<String> unavailable = new HashSet<>();
  private static final ExecutorService LOADER = Executors.newSingleThreadExecutor(r -> {
    Thread thread = new Thread(r, "RE4-menu-art"); thread.setDaemon(true); return thread;
  });
  private final Handler ui = new Handler(Looper.getMainLooper());
  private final HashSet<String> requested = new HashSet<>();
  private final Runnable changed;
  private volatile boolean closed;
  private int keyTexture = -2;
  GameMenuArt(Runnable changed) { this.changed = changed; }
  void close() { closed = true; mapTriangles.clear(); requested.clear(); backgroundImage=null; frameImage=null; portraitImage=null; }
  private Bitmap portraitImage;
  private int portraitWeapon=-1;
  private boolean portraitRequested;

  private static long pixels;
  static void resetCache() { images.clear();layouts.clear();pixels=0; }
  private GameMenuLayout viewport = new GameMenuLayout(640,480);
  private Bitmap backgroundImage,frameImage;
  private int backgroundVersion, backgroundPainted = -1;
  void viewport(GameMenuLayout next) { viewport = next; }
  private int activeTab = -1, money = -1, itemCount = -1;
  private int mapStage;
  private boolean mapEntire;
  private int mapMarkerId;
  private boolean mapMarkerOnly;
  private final HashMap<String,Integer> retries = new HashMap<>();

  // Prime all main pages before publishing the inventory. Native archive
  // decoding stays on LOADER; publication and shared cache access stay on UI.
  private int preparationPhase;
  boolean prepareInventory(int columns,int rows,int[] ids,JSONObject world) {
    if(preparationPhase==0) {
      for(String pack:new String[]{"common","case","casechoices","item","file","map"})layout(pack);
      preparationPhase=1;
    }
    if(!requested.isEmpty())return false;
    if(preparationPhase==2)return true;
    preparationPhase=2;
    for(String pack:new String[]{"common","case","casechoices","item","file","map"}) {
      JSONObject data=layout(pack);if(data==null)continue;
      JSONArray quads=data.optJSONArray("quads");if(quads==null)continue;
      for(int n=0;n<quads.length();++n) {
        JSONObject q=quads.optJSONObject(n);if(q==null || q.optInt("kind")==1)continue;
        int texture=q.optInt("texture",255),mask=q.optInt("mask",255);
        if(texture!=255)image(pack,texture,0);if(mask!=255)image(pack,mask,0);
        if(pack.equals("common") && (q.optInt("group")==7 || q.optInt("group")==8) && texture==7)
          for(int digit=1;digit<10;++digit)image(pack,texture,digit);
      }
    }
    image("common",66,0);image("common",2,0);image("casemessage",0,0);image("casemessage",0,1);image("caseboard",columns,rows);
    for(int id:ids){itemImage(id);image("itemname",id,0);}
    JSONObject map=world.optJSONObject("map");JSONArray paths=map==null?null:map.optJSONArray("paths");
    if(paths!=null)for(int n=0;n<paths.length();++n) {
      JSONObject path=paths.optJSONObject(n);if(path!=null && path.has("texture"))image("map",path.optInt("texture"),0);JSONArray triangles=path==null?null:path.optJSONArray("triangles");
      if(triangles!=null)for(int k=0;k<triangles.length();++k) {
        JSONObject triangle=triangles.optJSONObject(k);if(triangle!=null)image("map",triangle.optInt("texture"),0);
      }
    }
    JSONArray files=world.optJSONArray("files");
    if(files!=null)for(int n=0;n<files.length();++n){JSONObject file=files.optJSONObject(n);if(file!=null)image("itemname",file.optInt("id"),0);}
    return requested.isEmpty();
  }
  private JSONObject layout(String pack) {
    if (layouts.containsKey(pack)) return layouts.get(pack);
    String key = "layout:" + pack;
    if (closed || unavailable.contains(key) || !requested.add(key)) return null;
    LOADER.execute(() -> {
      if (closed) return;
      JSONObject result = null;
      try {
        String json = NativeBridge.subscreenLayout(pack);
        if (json != null && !json.isEmpty()) result = new JSONObject(json);
      } catch (JSONException ignored) { }
      final JSONObject loaded = result;
      ui.post(() -> { if (closed) return; requested.remove(key);
        if (loaded == null) unavailable.add(key); else layouts.put(pack, loaded);
        if(pack.equals("common"))++backgroundVersion;
        changed.run(); });
    });
    return null;
  }
  private Bitmap image(String pack, int id, int frame) {
    String key = pack + ":" + id + ":" + frame;
    if (images.containsKey(key)) return images.get(key);
    if (closed || unavailable.contains(key) || !requested.add(key)) return null;
    LOADER.execute(() -> {
      if (closed) return;
      Bitmap result = null;
      int[] data = NativeBridge.subscreenTexture(pack, id, frame);
      if (data != null && data.length >= 3 && data[0] >= 1 && data[1] >= 1 && data[0] <= 2048 && data[1] <= 2048 &&
          (long)data[0]*data[1] == data.length-2 && (long)data[0]*data[1] <= 8L*1024*1024)
        result = Bitmap.createBitmap(data, 2, data[0], data[0], data[1], Bitmap.Config.ARGB_8888);
      final Bitmap loaded = result;
      ui.post(() -> {
        if (closed) return; requested.remove(key);
        if (loaded == null) {
          int retry = retries.containsKey(key) ? retries.get(key) : 0;
          if (pack.equals("portrait") && retry < 4) { retries.put(key,retry+1); ui.postDelayed(changed,100); }
          else unavailable.add(key);
        } else {
          long count = (long)loaded.getWidth()*loaded.getHeight(), budget = 8L*1024*1024;
          while (pixels + count > budget && !images.isEmpty()) {
            Map.Entry<String,Bitmap> eldest = images.entrySet().iterator().next();
            pixels -= (long)eldest.getValue().getWidth()*eldest.getValue().getHeight(); images.remove(eldest.getKey());
          }
          Bitmap previous=images.put(key,loaded);
          if(previous!=null)pixels-=(long)previous.getWidth()*previous.getHeight();
          pixels += count;
          if(pack.equals("common"))++backgroundVersion;
        }
        changed.run();
      });
    });
    return null;
  }
  private void bitmap(Canvas canvas, Paint paint, String pack, int id, int frame, RectF bounds) {
    Bitmap bitmap = image(pack, id, frame); if (bitmap == null) return;
    paint.setColorFilter(null); paint.setAlpha(255); paint.setFilterBitmap(true);
    paint.setStyle(Paint.Style.FILL); canvas.drawBitmap(bitmap, null, bounds, paint);
  }
  void portrait(Canvas canvas, Paint paint, int weaponId) {
    if(closed)return;
    if(portraitWeapon!=weaponId){portraitWeapon=weaponId;portraitImage=null;}
    if(!portraitRequested) {
      portraitRequested=true;
      {
        int[] data=NativeBridge.subscreenTexture("portrait",weaponId,0);
        {
          portraitRequested=false;
          if(closed || portraitWeapon!=weaponId)return;
          if(data!=null && data.length>=3 && data[0]>0 && data[1]>0 && data[0]<=2048 && data[1]<=2048 && (long)data[0]*data[1]==data.length-2) {
            if(portraitImage==null || portraitImage.getWidth()!=data[0] || portraitImage.getHeight()!=data[1])
              portraitImage=Bitmap.createBitmap(data[0],data[1],Bitmap.Config.ARGB_8888);
            portraitImage.setPixels(data,2,data[0],0,0,data[0],data[1]);
          }
        }
      }
    }
    if(portraitImage==null)return;
    paint.setColorFilter(null);paint.setAlpha(255);paint.setFilterBitmap(true);paint.setStyle(Paint.Style.FILL);
    canvas.drawBitmap(portraitImage,null,new RectF(viewport.extraX,viewport.extraY,viewport.width,viewport.height),paint);
  }
  void caseBoard(Canvas canvas, Paint paint, int columns, int rows) {
    float[] transform=viewport.caseTransform(columns,rows);int saved=canvas.save();
    canvas.clipRect(0,110,viewport.width,viewport.bottom(348));
    canvas.translate(transform[1],transform[2]);canvas.scale(transform[0],transform[0]);
    bitmap(canvas,paint,"caseboard",columns,rows,new RectF(0,0,640,480));
    canvas.restoreToCount(saved);
  }
  void spaceBoard(Canvas canvas,Paint paint,int columns,int rows){
    float[] q=viewport.spaceQuad(columns,rows,0,0,6,12);float left=q[0],right=q[0],top=q[1],bottom=q[1];
    for(int n=1;n<4;++n){left=Math.min(left,q[n*2]);right=Math.max(right,q[n*2]);top=Math.min(top,q[n*2+1]);bottom=Math.max(bottom,q[n*2+1]);}
    bitmap(canvas,paint,"case",98,0,new RectF(left-12,top-8,viewport.width,bottom+8));
  }
  RectF[] caseLeaveMessage(Canvas canvas,Paint paint,boolean yes){
    // The 512-wide message layer must retain its native size in the
    // inventory; do not enlarge it like the 640-wide pickup presentation.
    final float zoom=512f/640f,cx=viewport.centerX(),cy=240+viewport.extraY*.5f;
    int saved=canvas.save();canvas.translate(cx,cy);canvas.scale(zoom,zoom);canvas.translate(-320,-240);
    JSONObject common=layout("common");if(common!=null)drawLayout(canvas,paint,"common",common,10);
    bitmap(canvas,paint,"casemessage",0,yes?0:1,new RectF(0,0,640,480));
    canvas.restoreToCount(saved);
    JSONObject choices=layout("casechoices");JSONArray units=choices==null?null:choices.optJSONArray("quads");if(units==null)return new RectF[0];
    RectF[] result=new RectF[units.length()];
    for(int n=0;n<result.length;++n){JSONArray v=units.optJSONObject(n).optJSONArray("vertices");result[n]=new RectF(cx+((float)v.optDouble(0)*1.25f-320)*zoom,cy+(-(float)v.optDouble(1)*1.25f-240)*zoom,cx+((float)v.optDouble(2)*1.25f-320)*zoom,cy+(-(float)v.optDouble(5)*1.25f-240)*zoom);}
    return result;
  }
  void sourceTexture(Canvas canvas, Paint paint, int id, int frame, RectF bounds) {
    bitmap(canvas,paint,"common",id,frame,bounds);
  }
  void pickupMessage(Canvas canvas,Paint paint,int id,int message,int count,int selection) {
    final float top=viewport.extraY*.5f-GameMenuLayout.PICKUP_TOP;
    bitmap(canvas,paint,"pickuptext",id|(message<<8),count*4+selection,
        new RectF(viewport.extraX*.5f,top,640+viewport.extraX*.5f,480+top));
  }
  void number(Canvas canvas,Paint paint,int value,float x,float y) {
    JSONObject common=layout("common");if(common==null)return;
    itemCount=Math.max(0,Math.min(999,value));int save=canvas.save();
    canvas.translate(x-320,y-240);drawLayout(canvas,paint,"common",common,8);
    canvas.restoreToCount(save);itemCount=-1;
  }
  void background(Canvas canvas, Paint paint, int tab) {
    activeTab = tab;
    canvas.drawColor(Color.BLACK);
    JSONObject common = layout("common");
    if (common == null) return;
    if(backgroundImage==null) {
      backgroundImage=Bitmap.createBitmap(640,480,Bitmap.Config.ARGB_8888);
      frameImage=Bitmap.createBitmap(640,480,Bitmap.Config.ARGB_8888);
    }
    if(backgroundPainted!=backgroundVersion) {
      backgroundImage.eraseColor(Color.BLACK);Canvas source=new Canvas(backgroundImage);
      // The source511x291 backing plate ends before the right surround.
      // Extend its real artwork beneath the flexible frame, so transparent
      // areas and expanded pages never reveal unpainted viewport strips.
      Bitmap plate=image("common",66,0);
      if(plate!=null) {
        paint.setAlpha(255);paint.setXfermode(null);paint.setFilterBitmap(true);
        paint.setColorFilter(new ColorMatrixColorFilter(new ColorMatrix(new float[]{
          225/255f,0,0,0,0,0,225/255f,0,0,0,0,0,225/255f,0,0,0,0,0,1,0})));
        source.drawBitmap(plate,null,new RectF(0,60,640,420),paint);paint.setColorFilter(null);
      }
      drawLayout(source,paint,"common",common,9,true);
      frameImage.eraseColor(Color.TRANSPARENT);
      drawLayout(new Canvas(frameImage),paint,"common",common,12);
      backgroundPainted=backgroundVersion;
    }
    stretchFrame(canvas,paint,backgroundImage);
  }
  // Original common0xC is installed by item/map/file/puzzle init in OT0xC.
  // The puzzle's cModel ot_type3 is later: the plate stays behind Leon.
  void frame(Canvas canvas,Paint paint) {
    if(frameImage!=null)stretchFrame(canvas,paint,frameImage);
  }
  private void stretchFrame(Canvas canvas,Paint paint,Bitmap image) {
    paint.setColorFilter(null);paint.setXfermode(null);paint.setAlpha(255);paint.setFilterBitmap(true);
    for(float[] slice:viewport.frameSlices())
      canvas.drawBitmap(image,new android.graphics.Rect((int)slice[0],(int)slice[1],(int)slice[2],(int)slice[3]),new RectF(slice[4],slice[5],slice[6],slice[7]),paint);
  }
  void header(Canvas canvas, Paint paint, int tab, int pesetas) {
    activeTab = tab; money = pesetas;
    JSONObject common = layout("common");
    if (common != null) { drawLayout(canvas,paint,"common",common,11); drawLayout(canvas,paint,"common",common,7); }
    money = -1;
  }
  private static final class MapTriangle {
    final android.graphics.BitmapShader shader;
    final ColorMatrixColorFilter color;
    final Matrix matrix=new Matrix();final Path path=new Path();
    final float[] source=new float[6];final int alpha;
    MapTriangle(Bitmap texture,JSONArray uv,JSONArray rgba) {
      for(int n=0;n<3;++n){JSONArray p=uv.optJSONArray(n);
        source[n*2]=(float)p.optDouble(0)*texture.getWidth();source[n*2+1]=(float)p.optDouble(1)*texture.getHeight();}
      shader=new android.graphics.BitmapShader(texture,android.graphics.Shader.TileMode.REPEAT,android.graphics.Shader.TileMode.REPEAT);
      color=new ColorMatrixColorFilter(new float[]{(float)rgba.optDouble(0)/255,0,0,0,0,
          0,(float)rgba.optDouble(1)/255,0,0,0,0,0,(float)rgba.optDouble(2)/255,0,0,0,0,0,1,0});
      alpha=rgba.optInt(3);
    }
  }
  void mapTriangle(Canvas canvas,Paint paint,JSONObject triangle,float[] points) {
    MapTriangle cached=mapTriangles.get(triangle);
    if(cached==null) {
      Bitmap texture=image("map",triangle.optInt("texture"),0);
      JSONArray uv=triangle.optJSONArray("uv"),rgba=triangle.optJSONArray("color");
      if(texture==null || uv==null || uv.length()!=3 || rgba==null || rgba.optInt(3)<=0)return;
      for(int n=0;n<3;++n)if(uv.optJSONArray(n)==null)return;
      cached=new MapTriangle(texture,uv,rgba);mapTriangles.put(triangle,cached);
    }
    if(!cached.matrix.setPolyToPoly(cached.source,0,points,0,3))return;
    cached.shader.setLocalMatrix(cached.matrix);paint.setShader(cached.shader);paint.setFilterBitmap(true);
    paint.setColorFilter(cached.color);paint.setAlpha(cached.alpha);paint.setStyle(Paint.Style.FILL);paint.setAntiAlias(false);
    cached.path.rewind();cached.path.moveTo(points[0],points[1]);cached.path.lineTo(points[2],points[3]);cached.path.lineTo(points[4],points[5]);cached.path.close();
    canvas.drawPath(cached.path,paint);paint.setShader(null);paint.setColorFilter(null);paint.setAlpha(255);paint.setAntiAlias(true);
  }
  void mapFrame(Canvas c,Paint p,int stage,boolean entire) {
    mapEntire=entire;
    mapStage=Math.max(1,Math.min(3,stage));JSONObject map=layout("map");
    if(map!=null){drawLayout(c,p,"map",map,7);drawLayout(c,p,"map",map,11);}
  }
  void mapPlayer(Canvas c,Paint p,float x,float y,float heading) {
    mapMarkerId=0;
    JSONObject map=layout("map");if(map==null)return;
    int saved=c.save();c.translate(x,y);c.rotate(-(float)Math.toDegrees(heading));
    c.translate(-320,-240);mapMarkerOnly=true;drawLayout(c,p,"map",map,6);
    mapMarkerOnly=false;c.restoreToCount(saved);
  }
  void mapGoal(Canvas c,Paint p,float x,float y) {
    JSONObject map=layout("map");if(map==null)return;
    int saved=c.save();c.translate(x-320,y-240);mapMarkerOnly=true;mapMarkerId=3;
    drawLayout(c,p,"map",map,6);mapMarkerOnly=false;c.restoreToCount(saved);
  }
  void keyCaptions(Canvas canvas,Paint paint) {
    // Authored ss_item group6 textures203/125: "Key Item" / "Treasure".
    // Use the source glyph artwork in the port's two responsive columns.
    float left=viewport.keyColumnLeft(0),right=viewport.keyColumnLeft(1);
    bitmap(canvas,paint,"item",203,0,new RectF(left+1,112,left+81,112+80f*19/86));
    bitmap(canvas,paint,"item",125,0,new RectF(right+1,112,right+81,112+80f*16/84));
  }
  void caseHud(Canvas canvas, Paint paint) {
    JSONObject common = layout("common");
    if (common != null) {
      drawLayout(canvas,paint,"common",common,16);
    }
  }
  void moveSelection(Canvas canvas,Paint paint) {
    // Original ss_cmmn caption; replace only its baked GC X glyph with the
    // actual universal Y mapping used by this port's move transaction.
    Bitmap caption=image("common",2,0);if(caption==null)return;
    paint.setAlpha(255);paint.setColorFilter(null);paint.setXfermode(null);
    float x=174,y=viewport.bottom(390),h=28.235294f;
    int glyph=Math.round(caption.getWidth()*22f/176);
    canvas.drawBitmap(caption,new android.graphics.Rect(glyph,0,caption.getWidth(),caption.getHeight()),new RectF(x+22,y,x+176,y+h),paint);
    paint.setColor(Color.rgb(235,235,225));paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(1.2f);
    canvas.drawOval(new RectF(x+2,y+5,x+20,y+23),paint);
    paint.setStyle(Paint.Style.FILL);paint.setTextAlign(Paint.Align.CENTER);paint.setTextSize(12);
    canvas.drawText("Y",x+11,y+19,paint);paint.setTextAlign(Paint.Align.LEFT);
  }
  /** Source file-list selection unit: ss_file entry7, mark1. It is an
   * additive texture25 gradient, rather than an Android rectangle button. */
  void listSelection(Canvas canvas,Paint paint,RectF bounds) {
    JSONObject file=layout("file");if(file==null)return;
    JSONArray quads=file.optJSONArray("quads");if(quads==null)return;
    for(int n=0;n<quads.length();++n) {
      JSONObject q=quads.optJSONObject(n);
      if(q==null || q.optInt("group")!=7 || q.optInt("mark")!=1 || q.optInt("kind")==1)continue;
      Bitmap tex=image("file",q.optInt("texture"),0);if(tex==null)return;
      JSONArray rgba=q.optJSONArray("color");if(rgba==null)return;
      paint.setStyle(Paint.Style.FILL);paint.setFilterBitmap(true);paint.setAlpha(rgba.optInt(3,255));
      paint.setColorFilter(new ColorMatrixColorFilter(new ColorMatrix(new float[]{
        (float)rgba.optDouble(0,255)/255,0,0,0,0,0,(float)rgba.optDouble(1,255)/255,0,0,0,
        0,0,(float)rgba.optDouble(2,255)/255,0,0,0,0,0,1,0})));
      paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.ADD));
      canvas.drawBitmap(tex,null,bounds,paint);
      paint.setXfermode(null);paint.setColorFilter(null);paint.setAlpha(255);return;
    }
  }

  RectF[] caseCommands(Canvas canvas,Paint paint,int type,int selected,float x,float y,boolean space) {
    int count=CaseMenuRules.actions(type).length,base=CaseMenuRules.BASE[Math.max(0,Math.min(9,type))];
    return caseCommandUnits(canvas,paint,base,count,selected,x,y,false,space);
  }
  RectF[] caseDiscard(Canvas canvas,Paint paint,boolean yes,float x,float y) {
    return caseDiscard(canvas,paint,yes,x,y,false);
  }
  RectF[] caseDiscard(Canvas canvas,Paint paint,boolean yes,float x,float y,boolean space){
    return caseCommandUnits(canvas,paint,0x80,2,yes?0:1,x,y,true,space);
  }
  private RectF[] caseCommandUnits(Canvas canvas,Paint paint,int base,int count,int selected,float x,float y,boolean submenu,boolean space) {
    JSONObject common=layout("common");if(common==null)return new RectF[0];
    JSONArray all=common.optJSONArray("quads"),chosen=new JSONArray();if(all==null)return new RectF[0];
    RectF[] rows=new RectF[count];
    try {
      for(int n=0;n<all.length();++n){JSONObject q=all.optJSONObject(n);if(q==null || q.optInt("group")!=(space?14:13))continue;
        int mark=q.optInt("mark")-base,limit=submenu?11:count*2+6;
        if(mark<0 || mark>=limit)continue;
        int first=submenu?5:4,last=first+count*2;
        if(mark>=first && mark<last && (mark-first)%2==0 && (mark-first)/2!=selected)continue;
        if(mark==0){q=new JSONObject(q.toString());q.put("x",x-320);q.put("y",240-y);}
        chosen.put(q);
        int labelStart=submenu?6:5;
        if(mark>=labelStart && mark<labelStart+count*2 && (mark-labelStart)%2==0){
          JSONArray v=q.optJSONArray("vertices");float left=x+(float)q.optDouble("x"),top=y-(float)q.optDouble("y");
          rows[(mark-labelStart)/2]=new RectF(left+(float)v.optDouble(0),top-(float)v.optDouble(1),left+(float)v.optDouble(2),top-(float)v.optDouble(5));
        }
      }
      JSONObject filtered=new JSONObject();filtered.put("quads",chosen);drawLayout(canvas,paint,"common",filtered,space?14:13);
    } catch(JSONException invalid){return new RectF[0];}
    for(int n=0;n<count;++n)if(rows[n]==null)rows[n]=new RectF(x+2,y+5+n*18,x+80,y+23+n*18);
    return rows;
  }
  private void drawLayout(Canvas canvas, Paint paint, String pack, JSONObject layout, int group) {
    drawLayout(canvas,paint,pack,layout,group,false);
  }
  private void drawLayout(Canvas canvas,Paint paint,String pack,JSONObject layout,int group,boolean expandedPlate) {
    JSONArray quads = layout.optJSONArray("quads"); if (quads == null) return;
    // IDSystem::trans queues in allocation order; AddOtDirect inserts at the
    // head of each shared OT bucket. Execute the authored units in reverse.
    for (int n = quads.length() - 1; n >= 0; --n) {
      JSONObject q = quads.optJSONObject(n);
      if (q == null || q.optInt("group") != group || q.optInt("kind") == 1) continue;
      if(expandedPlate && group==9 && q.optInt("texture")==66)continue;
      // IDC_SSCRN_MAIN_MENU: the five marked highlight groups are mutually
      // exclusive (ss_main::idMainMenu), while all unmarked tab captions stay.
      if (group == 12 && q.optInt("mark") != 255) continue;
      int frame = 0;
      if(group==8 && itemCount>=0) {
        int digit=q.optInt("mark",255),place=1;
        if(digit>=17 && digit<=19)digit-=16;
        if(digit<1 || digit>3)continue;
        for(int nDigit=1;nDigit<digit;++nDigit)place*=10;
        if(itemCount<place && digit>1)continue;
        frame=q.optInt("texture")==7?itemCount/place%10:0;
      }
      if (group == 7 && money >= 0) {
        int mark = q.optInt("mark",255);
        if (mark >= 9 && mark <= 10) continue;
        if (mark >= 1 && mark <= 8) {
          int place = 1; for (int digit=1;digit<mark;++digit) place *= 10;
          if (money < place && mark > 1) continue;
          frame = money / place % 10;
        }
      }
      JSONArray vertices = q.optJSONArray("vertices"), colors = q.optJSONArray("color");
      if (vertices == null || vertices.length() != 8) continue;
      int alpha = colors == null ? 255 : colors.optInt(3, 255); if (alpha <= 0) continue;
      boolean solid = q.optInt("texture") == 255;
      Bitmap bitmap = solid ? null : image(pack, q.optInt("texture"), frame);
      if (!solid && bitmap == null) continue;
      ArrayList<JSONObject> parents = new ArrayList<>();
      int parent = q.optInt("parent", 255), depth = 0;
      while (parent != 255 && depth++ < 16) {
        JSONObject p = null;
        for (int j = 0; j < quads.length(); ++j) {
          JSONObject candidate = quads.optJSONObject(j);
          if (candidate != null && candidate.optInt("group") == group && candidate.optInt("unit") == parent) { p = candidate; break; }
        }
        if (p == null || p == q) break;
        parents.add(p); parent = p.optInt("parent", 255);
      }
      boolean visible = true;
      if (pack.equals("common") && group == 11) for (JSONObject parentUnit : parents) {
        int mark = parentUnit.optInt("mark",255);
        if (!GameMenuLayout.tabParentVisible(mark,activeTab)) { visible = false; break; }
      }
      if (!visible) continue;
      if(pack.equals("map")) {
        int mark=q.optInt("mark",255);
        if(group==7 && (mark==0xfe || mark>=0x31 && mark<=0x33 && mark!=0x30+mapStage))continue;
        // MapFocus/MapRead enables Overall (mark0). The other two
        // captions occupy the same source position in different widgets.
        if(group==11 && !GameMenuLayout.mapCaptionVisible(mark,mapEntire))continue;
        boolean mapVisible=true,marker=mark==mapMarkerId;
        for(JSONObject parentUnit:parents) {
          int m=parentUnit.optInt("mark",255);
          if(group==7 && (m==0 || m==0x10))mapVisible=false;
          // r100 has no partner, merchant or treasure-map legend entries.
          if(group==11 && (!GameMenuLayout.mapCaptionVisible(m,mapEntire) || m==0x12 || m==0x13 || m==0x14))mapVisible=false;
          if(m==mapMarkerId)marker=true;
        }
        if(!mapVisible || group==6 && mapMarkerOnly && !marker)continue;
      }
      float red = colors == null ? 255 : (float)colors.optDouble(0, 255);
      float green = colors == null ? 255 : (float)colors.optDouble(1, 255);
      float blue = colors == null ? 255 : (float)colors.optDouble(2, 255);
      for (JSONObject p : parents) {
        JSONArray rgba = p.optJSONArray("color"); if (rgba == null) continue;
        red = (int)(red * rgba.optDouble(0, 255) / 255);
        green = (int)(green * rgba.optDouble(1, 255) / 255);
        blue = (int)(blue * rgba.optDouble(2, 255) / 255);
        alpha = (int)(alpha * rgba.optDouble(3, 255) / 255);
      }
      if (alpha <= 0) continue;
      // IdCommonTrans blend3 is SRC*DST_COLOR + DST, so a black source
      // contributes zero. SRC_OVER would paint the map's black gradient
      // over the lower half of the screen instead of preserving the EFB.
      if(q.optInt("blend")==3 && red==0 && green==0 && blue==0)continue;
      float[] destination = new float[8];
      for (int j = 0; j < 4; ++j) {
        destination[j * 2] = (float)vertices.optDouble(j * 2);
        destination[j * 2 + 1] = -(float)vertices.optDouble(j * 2 + 1);
      }
      if(group==16 && q.optInt("texture")==96)
        for(int j=0;j<4;++j)if(j==1 || j==2)destination[j*2]+=viewport.extraX;
      boolean flexibleMapFrame=pack.equals("map") && group==7 && q.optInt("unit")==7;
      if(flexibleMapFrame)for(int j=0;j<4;++j) {
        if(j==1 || j==2)destination[j*2]+=viewport.extraX;
        if(j>=2)destination[j*2+1]+=viewport.extraY;
      }
      Matrix transform = new Matrix();
      if (!solid && !transform.setPolyToPoly(new float[]{0,0,bitmap.getWidth(),0,bitmap.getWidth(),bitmap.getHeight(),0,bitmap.getHeight()}, 0, destination, 0, 4)) continue;
      int save = canvas.save(); canvas.translate(320, 240);
      if(pack.equals("common") && group==7)canvas.translate(viewport.extraX,0);
      else if(pack.equals("common") && group==16)canvas.translate(0,viewport.extraY);
      else if(pack.equals("map") && group==11)canvas.translate(viewport.extraX*.5f,viewport.extraY);
      else if(pack.equals("map") && group==7)canvas.translate(viewport.extraX*.5f,viewport.extraY*.5f);
      else if(pack.equals("common") && group==11) {
        int tab=-1;
        for(JSONObject p:parents) {
          int unit=p.optInt("unit",255);
          if(unit==1 || unit==14)tab=0;else if(unit==0 || unit==12)tab=1;
          else if(unit==19 || unit==22)tab=2;else if(unit==28 || unit==31)tab=3;
          else if(unit==37 || unit==40)tab=4;
        }
        if(tab>=0) {
          canvas.translate(viewport.tabShift(tab),0);
          // Uniformly enlarge the complete tab (caption and highlight)
          // around its original centre. Parent matrices retain source data.
          float factor=viewport.captionScale(),cx=viewport.tabCenter(tab)-320;
          canvas.translate(cx,-158);canvas.scale(factor,factor);canvas.translate(-cx,158);
        }
      }
      if(flexibleMapFrame)canvas.translate(-viewport.extraX*.5f,-viewport.extraY*.5f);
      // idSysMove03 stores angles in degrees and concatenates group-parent
      // matrices. ParentNo addresses the authored unit number, not markNo.
      int inherited = 0;
      while (inherited < parents.size() && parents.get(inherited).optInt("kind") == 1) ++inherited;
      for (int j = inherited - 1; j >= 0; --j) {
        JSONObject p = parents.get(j);
        canvas.translate((float)p.optDouble("x"), -(float)p.optDouble("y"));
        canvas.rotate(-(float)p.optDouble("rotation"));
      }
      canvas.translate((float)q.optDouble("x"), -(float)q.optDouble("y"));
      canvas.rotate(-(float)q.optDouble("rotation"));
      paint.setStyle(Paint.Style.FILL); paint.setAlpha(alpha); paint.setFilterBitmap(true);
      // IdCommonTrans blend[1] is SRC_ALPHA + ONE, used by the original
      // selection glows. Normal SRC_OVER would obscure the caption beneath.
      paint.setXfermode(q.optInt("blend") == 1 || q.optInt("blend") == 2
          ? new PorterDuffXfermode(PorterDuff.Mode.ADD) : null);
      paint.setColorFilter(new ColorMatrixColorFilter(new ColorMatrix(new float[]{
          red/255,0,0,0,0, 0,green/255,0,0,0, 0,0,blue/255,0,0, 0,0,0,1,0})));
      Bitmap mask = q.optInt("mask", 255) == 255 ? null : image(pack, q.optInt("mask"), 0);
      int layer = -1;
      if (mask != null) {
        RectF bounds = new RectF(destination[0], destination[1], destination[0], destination[1]);
        for (int j = 1; j < 4; ++j) bounds.union(destination[j*2], destination[j*2+1]);
        layer = canvas.saveLayer(bounds, null);
      }
      if (solid) {
        paint.setColorFilter(null); paint.setColor(Color.argb(alpha,(int)red,(int)green,(int)blue));
        Path shape = new Path(); shape.moveTo(destination[0],destination[1]);
        for (int j=1;j<4;++j) shape.lineTo(destination[j*2],destination[j*2+1]); shape.close(); canvas.drawPath(shape,paint);
      } else canvas.drawBitmap(bitmap, transform, paint);
      if (mask != null) {
        Matrix maskTransform = new Matrix();
        maskTransform.setPolyToPoly(new float[]{0,0,mask.getWidth(),0,mask.getWidth(),mask.getHeight(),0,mask.getHeight()}, 0, destination, 0, 4);
        paint.setColorFilter(null); paint.setAlpha(255);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.DST_IN));
        canvas.drawBitmap(mask, maskTransform, paint); paint.setXfermode(null);
        canvas.restoreToCount(layer);
      }
      paint.setXfermode(null); paint.setColorFilter(null); paint.setAlpha(255); canvas.restoreToCount(save);
    }
  }
  private Bitmap itemImage(int id) {
    int frame = -1;
    for (int n = 1; n < KEY_ICONS.length; ++n) if (KEY_ICONS[n] == id) { frame = n; break; }
    if (frame >= 0) {
      if (keyTexture == -2) {
        JSONObject keys = layout("item");
        if (keys == null) return image("itemicon", id, 0);
        keyTexture = -1;
        JSONArray quads = keys == null ? null : keys.optJSONArray("quads");
        if (quads != null) for (int n = 0; n < quads.length(); ++n) {
          JSONObject q = quads.optJSONObject(n);
          if (q != null && q.optInt("group") == 7 && q.optInt("mark") >= 0x41 && q.optInt("mark") <= 0x58) { keyTexture = q.optInt("texture"); break; }
        }
      }
      if (keyTexture >= 0) return image("item", keyTexture, frame);
    }
    return image("itemicon", id, 0);
  }
  void caseItem(Canvas canvas, Paint paint, int id, float[] corners, int rotation) {
    Bitmap bitmap = itemImage(id); if (bitmap == null || corners.length != 8) return;
    float w=bitmap.getWidth(),h=bitmap.getHeight();
    float[] source={0,0,w,0,w,h,0,h},rotated=new float[8];
    for (int j=0;j<4;++j) { int from=(j-(rotation&3)+4)%4; rotated[j*2]=source[from*2]; rotated[j*2+1]=source[from*2+1]; }
    Matrix transform = new Matrix(); if (!transform.setPolyToPoly(rotated,0,corners,0,4)) return;
    paint.setColorFilter(null);paint.setAlpha(255);paint.setFilterBitmap(true);paint.setStyle(Paint.Style.FILL);
    canvas.drawBitmap(bitmap,transform,paint);
  }
  boolean hasItem(int id) { return itemImage(id) != null; }
  void item(Canvas canvas, Paint paint, int id, RectF bounds, int rotation) {
    Bitmap bitmap = itemImage(id); if (bitmap == null) return;
    int save = canvas.save(); canvas.clipRect(bounds); canvas.translate(bounds.centerX(), bounds.centerY());
    canvas.rotate((rotation & 3) * 90);
    float width = (rotation & 1) == 0 ? bounds.width() : bounds.height();
    float height = (rotation & 1) == 0 ? bounds.height() : bounds.width();
    float scale = Math.min((width - 4) / bitmap.getWidth(), (height - 4) / bitmap.getHeight());
    float w = bitmap.getWidth() * scale, h = bitmap.getHeight() * scale;
    paint.setFilterBitmap(true); paint.setColorFilter(null); paint.setAlpha(255);
    canvas.drawBitmap(bitmap, null, new RectF(-w/2,-h/2,w/2,h/2), paint); canvas.restoreToCount(save);
  }
  boolean filePage(Canvas canvas, Paint paint, int id, int page, RectF bounds) {
    Bitmap bitmap = image("filepage", id, page); if (bitmap == null) return false;
    paint.setColorFilter(null); paint.setAlpha(255); paint.setFilterBitmap(true);
    float scale = Math.min(bounds.width()/bitmap.getWidth(), bounds.height()/bitmap.getHeight());
    float w = bitmap.getWidth()*scale, h = bitmap.getHeight()*scale;
    canvas.drawBitmap(bitmap,null,new RectF(bounds.centerX()-w/2,bounds.centerY()-h/2,bounds.centerX()+w/2,bounds.centerY()+h/2),paint);
    return true;
  }
  /** MesData slot 3 contains original item titles as font glyphs, not UTF text. */
  boolean itemName(Canvas canvas, Paint paint, int id, RectF bounds, boolean centered) {
    if (id < 0 || id > 255 || bounds.width() <= 0 || bounds.height() <= 0) return false;
    Bitmap bitmap = image("itemname", id, 0); if (bitmap == null) return false;
    paint.setStyle(Paint.Style.FILL); paint.setColorFilter(null); paint.setAlpha(255); paint.setFilterBitmap(true);
    float scale = Math.min(bounds.width()/bitmap.getWidth(), bounds.height()/bitmap.getHeight());
    float width = bitmap.getWidth()*scale, height = bitmap.getHeight()*scale;
    float x = centered ? bounds.centerX()-width/2 : bounds.left;
    canvas.drawBitmap(bitmap, null, new RectF(x,bounds.centerY()-height/2,x+width,bounds.centerY()+height/2),paint);
    return true;
  }
}
