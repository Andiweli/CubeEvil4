package com.ast.re4decomp;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.view.KeyEvent;
import android.os.SystemClock;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import java.util.ArrayList;

/** Source Sscrn flows: case placement, item commands, map and collected files.
 * All changes are native transactions; Canvas never grants or consumes items. */
public final class InventoryDialog extends GameScreenDialog {
  public static final int EQUIP = 1, USE = 2, DISCARD = 3, MOVE = 4, COMBINE = 5, DETACH = 6;
  public static final int KEYS = 0, CASE = 1, MAP = 2, FILES = 3, EXIT = 4, OPTIONS = 5;
  public interface ActionHandler { void onAction(int operation, int slot, int first, int second, int third); }
  public interface OptionHandler { void onOption(int operation); }
  public interface PreviewHandler { void onPreview(int id); }
  private static final class Item {
    int slot, id, num, loaded, x, y, w, h, rotation, kind, weaponId, attachedTo = -1, commandType;
    boolean equipped, equippable, usable, space, held;
    String name, description;
    boolean inCase() { return x >= 0 && y >= 0 && !space && !held; }
    boolean onBoard(){return x>=0 && y>=0 && !held;}
  }
  private static final class Command {
    final String title; final Runnable action;
    Command(String title, Runnable action) { this.title = title; this.action = action; }
  }
  private final ActionHandler handler;
  private final OptionHandler options;
  private final PreviewHandler preview;
  private final ArrayList<Item> items = new ArrayList<>();
  private final ArrayList<Command> commands = new ArrayList<>();
  private int columns = 10, rows = 6, pesetas, selected = -1, combineFrom = -1;
  private int caseCommandType;
  private RectF[] commandBounds=new RectF[0];
  private int tab, commandSelected, optionSelected, documentSelected, documentPage;
  private int moveX, moveY, moveRotation;
  private int caseCursorX,caseCursorY;
  private boolean activeSpace,moveSpace,leavePrompt;
  private boolean backendReady;
  private Runnable leaveAction;
  private final Activity owner;
  private boolean moving, pending, discardConfirm, discardYes, examining, reading, controls;
  private boolean exitOptions;
  private boolean pickupOrdering;
  private String message = "";
  private StartupArt cockpitArt;
  private final Bitmap cockpitImage=Bitmap.createBitmap(640,480,Bitmap.Config.ARGB_8888);
  private final Canvas cockpitCanvas=new Canvas(cockpitImage);
  void setCockpitArt(StartupArt value){cockpitArt=value;refreshScreen();}
  @Override float sourceHeight(){return 360;}
  @Override float sourceTop(){return 60;}
  private JSONObject world = new JSONObject();
  private JSONArray files = new JSONArray();
  private float mapZoom = 1;
  private float mapPanX, mapPanZ;
  private boolean mapItems = true;
  private GameMapCamera mapCamera=new GameMapCamera();
  private final ArrayList<JSONObject> orderedMapModels=new ArrayList<>();
  private final float[] mapProjected=new float[6];
  private final GameMenuArt art = new GameMenuArt(this::refreshScreen);

  public InventoryDialog(Activity activity, String snapshot, String worldSnapshot, int firstTab,
                         ActionHandler handler, OptionHandler options, PreviewHandler preview) {
    super(activity);owner=activity; this.handler = handler; this.options = options; this.preview = preview;
    tab = Math.max(KEYS, Math.min(OPTIONS, firstTab));
    setSnapshot(snapshot); setWorldSnapshot(worldSnapshot);
  }
  @Override protected void onStart() {
    super.onStart();
    if (tab <= FILES) NativeBridge.menuSound(0x1E);
  }
  void setPickupOrdering() { pickupOrdering = true; tab = CASE; selectFirst(); refreshScreen(); }
  public void setWorldSnapshot(String snapshot) {
    try {
      if (snapshot != null && !snapshot.isEmpty()) {
        world = new JSONObject(snapshot);
        orderedMapModels.clear();mapCamera=new GameMapCamera();
        JSONObject map=world.optJSONObject("map");JSONArray paths=map==null?null:map.optJSONArray("paths");
        if(map!=null && "BIN".equals(map.optString("geometry")) && paths!=null) {
          for(int n=0;n<paths.length();++n){JSONObject path=paths.optJSONObject(n);if(path!=null)orderedMapModels.add(path);}
          java.util.Collections.sort(orderedMapModels,(a,b)->Double.compare(mapHeight(a),mapHeight(b)));
        }
        JSONArray supplied = world.optJSONArray("files");
        if (supplied != null) files = supplied;
      }
    } catch (JSONException e) { message = "Could not read map."; }
    rebuildFiles(); refreshScreen();
  }
  /** UI-thread publication only, after the native GL-owned transaction. */
  public void setSnapshot(String snapshot) {
    try {
      JSONObject state = new JSONObject(snapshot);
      if(state.optInt("casePuzzleVersion",0)!=1 || !state.has("held") ||
          state.optInt("spaceWidth",0)!=6 || state.optInt("spaceHeight",0)!=12)
        throw new JSONException("Inventory Java/native versions differ. Clean and rebuild Fix18.");
      int nextColumns = state.getInt("width"), nextRows = state.getInt("height");
      if (nextColumns < 1 || nextColumns > 15 || nextRows < 1 || nextRows > 8)
        throw new JSONException("Invalid case size");
      JSONArray values = state.getJSONArray("items");
      if (values.length() > 384) throw new JSONException("Too many items");
      ArrayList<Item> next = new ArrayList<>();
      for (int n = 0; n < values.length(); ++n) {
        JSONObject v = values.getJSONObject(n); Item i = new Item();
        i.slot = v.getInt("slot"); i.id = v.getInt("id"); i.num = v.getInt("num");
        i.loaded = v.getInt("loaded"); i.x = v.getInt("x"); i.y = v.getInt("y");
        i.w = v.getInt("w"); i.h = v.getInt("h"); i.rotation = v.getInt("rotation");
        i.kind = v.getInt("kind"); i.equipped = v.getBoolean("equipped"); i.name = v.getString("name");
        i.space=v.optBoolean("space",false);i.held=v.optBoolean("held",false);
        i.commandType=v.getInt("commandType");
        i.weaponId=v.optInt("weaponId",i.id);i.attachedTo=v.optInt("attachedTo",-1);
        if(i.attachedTo>=0)i.name+=" (attached)";
        i.description = v.optString("description", "");
        i.equippable = v.optBoolean("equippable", i.id == 0x21 || i.id == 0x23 || i.id == 0x2c);
        i.usable = v.optBoolean("usable", i.kind == 6 && i.id != 0x19 && i.id != 0x1c && i.id != 0xa8);
        if (i.onBoard() && !GameMenuNavigation.fits(i.space?6:nextColumns, i.space?12:nextRows, i.x, i.y, i.w, i.h))
          throw new JSONException("Item is outside the case");
        next.add(i);
      }
      if(!backendReady)android.util.Log.i("RE4Inventory","Fix18: original case commands 13/14; native puzzle v1; temporary board 6x12");
      backendReady=true;
      columns = nextColumns; rows = nextRows; pesetas = state.getInt("pesetas");
      items.clear(); items.addAll(next); message = state.optString("message", "");
      pending = false; commands.clear(); discardConfirm = false;
      if(activeSpace && !hasSpace() && state.optInt("held",-1)<0){activeSpace=false;selected=-1;caseCursorX=caseCursorY=0;}
      if (!visible(find(selected))) selectFirst();
      else if(tab==CASE){Item selectedItem=find(selected);caseCursorX=selectedItem.x;caseCursorY=selectedItem.y;}
      int heldSlot=state.optInt("held",-1);Item hand=find(heldSlot);moving=hand!=null;
      if(moving){selected=heldSlot;moveX=hand.x;moveY=hand.y;moveRotation=hand.rotation;activeSpace=moveSpace=hand.space;}
      if(leaveAction!=null && !hasSpace() && !moving){Runnable action=leaveAction;leaveAction=null;leavePrompt=false;action.run();return;}
      rebuildFiles(); refreshScreen();
    } catch (JSONException e) {
      backendReady=false;pending=false;moving=false;items.clear();commands.clear();
      message="Could not read inventory: " + e.getMessage();
      android.util.Log.e("RE4Inventory",message);refreshScreen();
    }
  }
  private Item find(int slot) { for (Item i : items) if (i.slot == slot) return i; return null; }
  private boolean visible(Item i) {
    return i != null && (tab == CASE ? i.onBoard() && i.space==activeSpace : tab == KEYS && GameMenuNavigation.keyOrTreasure(i.x, i.y, i.kind));
  }
  private void selectFirst() { selected = -1; for (Item i : items) if (visible(i)) { selected = i.slot;if(tab==CASE){caseCursorX=i.x;caseCursorY=i.y;}break; } }
  private int[] caseOccupants(){int[] board=new int[boardColumns()*boardRows()];java.util.Arrays.fill(board,-1);
    for(Item item:items)if(item.onBoard() && item.space==activeSpace)for(int y=item.y;y<item.y+item.h;++y)for(int x=item.x;x<item.x+item.w;++x)board[y*boardColumns()+x]=item.slot;
    return board;
  }
  private boolean hasSpace(){for(Item item:items)if(item.space && !item.held && item.onBoard())return true;return false;}
  private int boardColumns(){return activeSpace?6:columns;}
  private int boardRows(){return activeSpace?12:rows;}
  private boolean puzzleVisible(){return moving || hasSpace();}
  private float[] itemQuad(Item item){return item.space?layout.spaceQuad(columns,rows,item.x,item.y,item.w,item.h):layout.caseQuad(columns,rows,item.x,item.y,item.w,item.h);}
  private void requestLeave(Runnable action){leavePrompt=true;discardYes=false;leaveAction=action;NativeBridge.menuSound(0x2A);refreshScreen();}
  private void rebuildFiles() {
    // Only owned original file IDs appear. Never populate a campaign reader with fabricated text.
    JSONArray collected = new JSONArray();
    for (Item i : items) if (i.kind == 10) {
      JSONObject file = null;
      for (int n = 0; n < files.length(); ++n) {
        JSONObject candidate = files.optJSONObject(n);
        if (candidate != null && candidate.optInt("id", -1) == i.id) { file = candidate; break; }
      }
      if (file == null) {
        file = new JSONObject();
        try { file.put("id", i.id); file.put("title", i.name); } catch (JSONException ignored) { }
      }
      collected.put(file);
    }
    files = collected;
    documentSelected = Math.max(0, Math.min(Math.max(0, files.length() - 1), documentSelected));
  }
  private void switchTab(int value) {
    if (pending || pickupOrdering) return;
    if(moving)return;
    if(hasSpace()){requestLeave(()->switchTab(value));return;}
    activeSpace=false;
    preview.onPreview(-1); tab = (value + 5) % 5; moving = reading = controls = examining = false;
    if(tab==MAP)mapCamera=new GameMapCamera();
    commands.clear(); combineFrom = -1; discardConfirm = false; message = ""; selectFirst();
    NativeBridge.menuSound(0xA);
    if (tab <= FILES) NativeBridge.menuSound(0x1E);
    refreshScreen();
  }
  private void submit(int operation, int slot, int a, int b, int c) {
    if (pending) return;
    pending = true; moving = false; combineFrom = -1; commands.clear(); discardConfirm = false;
    message = ""; refreshScreen(); handler.onAction(operation, slot, a, b, c);
  }
  private void startMove() {
    if(!backendReady)return;
    Item i=find(selected);if(pending || i==null || !i.onBoard())return;
    submit(7,i.slot,0,0,0);
  }
  private void rotate(int rotationStep) {
    if (!moving) startMove();
    if (!moving) return;
    moveRotation = (moveRotation + rotationStep) & 3; clampMove(); NativeBridge.menuSound(0xB); refreshScreen();
  }
  private int moveWidth(Item i) { return (moveRotation & 1) != (i.rotation & 1) ? i.h : i.w; }
  private int moveHeight(Item i) { return (moveRotation & 1) != (i.rotation & 1) ? i.w : i.h; }
  private void clampMove() {
    Item i = find(selected); if (i == null) return;
    moveX = GameMenuNavigation.clampPosition(moveX, moveWidth(i), activeSpace?6:columns);
    moveY = GameMenuNavigation.clampPosition(moveY, moveHeight(i), activeSpace?12:rows);
  }
  private boolean fitsMove() {
    Item i = find(selected); if (i == null) return false;
    int w = moveWidth(i), h = moveHeight(i);
    if (!GameMenuNavigation.fits(moveSpace?6:columns, moveSpace?12:rows, moveX, moveY, w, h)) return false;
    int overlaps=0;
    for (Item other : items) if (other.slot != selected && other.onBoard() && other.space==moveSpace &&
        GameMenuNavigation.overlaps(moveX, moveY, w, h, other.x, other.y, other.w, other.h))++overlaps;
    return overlaps<=1;
  }
  private void startCombine() {
    Item i = find(selected); if (pending || i == null) return;
    combineFrom = i.slot; moving = false; commands.clear(); message = "";
    refreshScreen();
  }
  private void examine() {
    Item i = find(selected); if (i == null) return;
    commands.clear(); examining = true; NativeBridge.menuSound(0x1A); preview.onPreview(i.id); refreshScreen();
  }
  private void openCommands() {
    Item i = find(selected); if (i == null || pending) return;
    commands.clear(); commandSelected = 0;
    if(tab==CASE){
      caseCommandType=i.commandType;
      for(int action:CaseMenuRules.actions(caseCommandType)){
        if(action==0)commands.add(new Command("Equip",()->submit(EQUIP,i.slot,0,0,0)));
        else if(action==2)commands.add(new Command("Combine",this::startCombine));
        else if(action==3)commands.add(new Command("Remove",()->submit(DETACH,i.slot,0,0,0)));
        else if(action==4)commands.add(new Command("Use",()->submit(USE,i.slot,0,0,0)));
        else if(action==5)commands.add(new Command("Examine",this::examine));
        else if(action==6)commands.add(new Command("Discard",()->{discardConfirm=true;discardYes=false;NativeBridge.menuSound(0x2A);refreshScreen();}));
      }
    } else {
    if (i.equippable && !pickupOrdering) commands.add(new Command("Equip", () -> submit(EQUIP, i.slot, 0, 0, 0)));
    if (i.usable && !pickupOrdering) commands.add(new Command("Use", () -> submit(USE, i.slot, 0, 0, 0)));
    commands.add(new Command("Examine", this::examine));
    commands.add(new Command("Combine", this::startCombine));
    if(i.attachedTo>=0)commands.add(new Command("Remove", () -> submit(DETACH,i.slot,0,0,0)));
    if (i.inCase()) {
      commands.add(new Command("Move", this::startMove));
      if (!i.equipped) commands.add(new Command("Discard", () -> { commands.clear(); discardConfirm = true; discardYes = false; NativeBridge.menuSound(0x2A); refreshScreen(); }));
    }
    }
    NativeBridge.menuSound(4); refreshScreen();
  }
  @Override void confirm() {
    if (pending || !backendReady) return;
    if(leavePrompt){if(discardYes){leavePrompt=false;submit(10,-1,0,0,0);}else{leavePrompt=false;leaveAction=null;refreshScreen();}return;}
    if (discardConfirm) {
      Item i = find(selected);
      if (discardYes && i != null) submit(DISCARD, i.slot, 0, 0, 0);
      else { discardConfirm = false;commands.clear(); refreshScreen(); }
    } else if (!commands.isEmpty()) commands.get(commandSelected).action.run();
    else if (examining || controls) back();
    else if (tab == CASE || tab == KEYS) {
      Item i = find(selected); if (i == null) return;
      if (moving) {
        submit(8,i.slot,moveX,moveY,moveRotation|(moveSpace?4:0));
      } else if (combineFrom >= 0) submit(COMBINE, combineFrom, i.slot, 0, 0);
      else openCommands();
    } else if(tab==MAP) {
      if(mapCamera.initialized())mapCamera.toggle(SystemClock.uptimeMillis());
      NativeBridge.menuSound(mapCamera.entire?5:4);refreshScreen();
    } else if (tab == FILES && files.length() > 0) {
      reading = true; documentPage = 0; refreshScreen();
    } else if (tab == EXIT) { if (exitOptions) showOptions(); else dismiss(); }
    else if (tab == OPTIONS) {
      if (optionSelected == 0) { controls = true; refreshScreen(); }
      else { final int action = optionSelected; dismiss(); options.onOption(action); }
    }
  }
  @Override void back() {
    if (pending) return;
    if(leavePrompt){leavePrompt=false;leaveAction=null;refreshScreen();return;}
    if (examining) { examining = false; preview.onPreview(-1); }
    else if (reading) reading = false;
    else if (controls) controls = false;
    else if (discardConfirm) discardConfirm = false;
    else if (!commands.isEmpty()) commands.clear();
    else if(combineFrom>=0){selected=combineFrom;combineFrom=-1;openCommands();return;}
    else if (moving) { submit(9,selected,0,0,0);return; }
    else if(tab==MAP && mapCamera.entire) {mapCamera.toggle(SystemClock.uptimeMillis());NativeBridge.menuSound(4);refreshScreen();return;}
    else if (tab == OPTIONS) { switchTab(CASE); return; }
    else { dismiss(); return; }
    NativeBridge.menuSound(5); refreshScreen();
  }
  @Override void auxiliary(int key) {
    if (pending || leavePrompt || !commands.isEmpty() || discardConfirm || examining || controls) return;
    if(moving && tab==CASE){
      if(key==KeyEvent.KEYCODE_BUTTON_X)confirm();
      else if(key==KeyEvent.KEYCODE_BUTTON_L1)rotate(-1);
      else if(key==KeyEvent.KEYCODE_BUTTON_R1)rotate(1);
      return;
    }
    if (key == KeyEvent.KEYCODE_BUTTON_SELECT) switchTab(tab + 1);
    else if (key == KeyEvent.KEYCODE_BUTTON_L1) switchTab(tab - 1);
    else if (key == KeyEvent.KEYCODE_BUTTON_R1) switchTab(tab + 1);
    else if (key == KeyEvent.KEYCODE_BUTTON_X) {
      if(tab==CASE)startMove();
      else if(tab==MAP){mapItems=!mapItems;refreshScreen();}
    }
  }

  @Override void turnPage(int step) {
    if(tab==CASE && moving){rotate(step);return;}
    if (tab == MAP) { mapCamera.zoom(step);refreshScreen(); }
    else if (reading) {
      JSONObject file = files.optJSONObject(documentSelected);
      JSONArray pages = file == null ? null : file.optJSONArray("pages");
      int count = file == null ? 0 : file.optInt("pageCount", pages == null ? 0 : pages.length());
      documentPage = Math.max(0, Math.min(Math.max(0, count - 1), documentPage + step)); refreshScreen();
    }
  }
  @Override boolean continuousPageInput(){return tab==MAP && !examining && !controls;}
  @Override boolean hardwareDrawing(){return tab==MAP && !examining && !controls;}
  @Override void rightStick(float x, float y) {
    if (examining) NativeBridge.input(0, 0, 0, x, y, 0, 0);
  }
  @Override void direction(int dx, int dy) {
    if (pending) return;
    if (discardConfirm || leavePrompt) { if(dx!=0){discardYes = dx < 0;NativeBridge.menuSound(0xA);refreshScreen();}return; }
    if (!commands.isEmpty()) { if(dy!=0){commandSelected = (commandSelected + dy + commands.size()) % commands.size();NativeBridge.menuSound(0xA);refreshScreen();}return; }
    if (examining || controls) return;
    if (reading) { turnPage(dx == 0 ? dy : dx); return; }
    if (tab == FILES) { documentSelected = Math.max(0, Math.min(Math.max(0, files.length() - 1), documentSelected + (dy == 0 ? dx : dy))); refreshScreen(); return; }
    if (tab == OPTIONS) { optionSelected = Math.max(0, Math.min(2, optionSelected + (dy == 0 ? dx : dy))); refreshScreen(); return; }
    if (tab == EXIT) { exitOptions = dx > 0 || dy > 0; refreshScreen(); return; }
    if (tab == MAP) { mapCamera.pan(dx*mapCamera.position[2]*.03,dy*mapCamera.position[2]*.03);refreshScreen();return; }
    if (moving) {
      int maxX=boardColumns()-moveWidth(find(selected));
      if(dx!=0 && (moveX+dx<0 || moveX+dx>maxX)){
        activeSpace=moveSpace=!activeSpace;moveY+=activeSpace?2:-2;
        Item held=find(selected);if(moveWidth(held)>boardColumns())moveRotation=(moveRotation+1)&3;
        moveX=dx>0?0:boardColumns()-moveWidth(held);
      }else{moveX+=dx;moveY+=dy;}
      clampMove();NativeBridge.menuSound(0xA);refreshScreen();return;
    }
    if(tab==CASE){
      int oldCell=caseCursorY*boardColumns()+caseCursorX;int[] board=caseOccupants();
      int cell=GameMenuNavigation.caseCursor(board,boardColumns(),boardRows(),oldCell,dx,dy);
      if(cell==oldCell && dx!=0 && hasSpace()){
        activeSpace=!activeSpace;caseCursorY=Math.max(0,Math.min(boardRows()-1,caseCursorY+(activeSpace?2:-2)));caseCursorX=dx>0?0:boardColumns()-1;board=caseOccupants();cell=caseCursorY*boardColumns()+caseCursorX;
      }
      caseCursorX=cell%boardColumns();caseCursorY=cell/boardColumns();selected=board[cell];NativeBridge.menuSound(6);refreshScreen();return;
    }
    Item current = find(selected); if (current == null) { selectFirst(); refreshScreen(); return; }
    Item closest = null; double score = Double.MAX_VALUE;
    if (tab == KEYS) {
      int column = keyColumn(current);
      ArrayList<Item> list = keyItems(column); int at = list.indexOf(current);
      if (dx != 0) { column = dx < 0 ? 0 : 1; list = keyItems(column); }
      else at += dy;
      int to = Math.max(0, Math.min(list.size() - 1, at));
      if (to >= 0 && to < list.size()) selected = list.get(to).slot;
    } else for (Item i : items) {
      if (!visible(i) || i.slot == selected) continue;
      double x = i.x + i.w * .5 - (current.x + current.w * .5), y = i.y + i.h * .5 - (current.y + current.h * .5);
      if (x * dx + y * dy <= .01) continue;
      double cross = Math.abs(x * dy - y * dx), candidate = x * x + y * y + cross * cross * 3;
      if (candidate < score) { closest = i; score = candidate; }
    }
    if (closest != null) selected = closest.slot;
    NativeBridge.menuSound(0xA); refreshScreen();
  }
  private int keyColumn(Item item) { return item.kind == 5 || item.kind == 12 ? 1 : 0; }
  private ArrayList<Item> keyItems(int column) {
    ArrayList<Item> values = new ArrayList<>();
    for (Item i : items) if (GameMenuNavigation.keyOrTreasure(i.x, i.y, i.kind) && keyColumn(i) == column) values.add(i);
    return values;
  }

  void abandon(){art.close();super.dismiss();}
  @Override public void dismiss() {
    if(!owner.isFinishing() && !owner.isDestroyed()){
      if(pending)return;
      if(moving){submit(9,selected,0,0,0);return;}
      if(hasSpace()){requestLeave(this::dismiss);return;}
    }
    art.close();super.dismiss();
  }
  @Override void drawScreen(Canvas c) {
    if(!backendReady){centered(c,message,layout.width*.5f,layout.height*.5f,14,PAPER);return;}
    layout.puzzleActive=tab==CASE && puzzleVisible();
    art.viewport(layout);
    int[] ids=new int[items.size()];for(int n=0;n<ids.length;++n)ids[n]=items.get(n).id;
    if(!art.prepareInventory(columns,rows,ids,world)) {
      c.drawColor(Color.BLACK);return;
    }
    art.background(c, paint, tab);
    if(tab!=CASE && tab!=MAP || examining || controls)art.frame(c,paint);
    // Tabs, labels and selection plate are original ID data, not Android
    // buttons. Their controller-independent hit regions follow that layout.
    if (!pickupOrdering && !leavePrompt && !moving) {
      for(int n=0;n<5;++n) { final int index=n; float[] bounds=layout.tabBounds(n);hotspot(new RectF(bounds[0],bounds[1],bounds[2],bounds[3]),()->switchTab(index)); }
    }
    if (examining) drawExamine(c);
    else if (controls) drawControls(c);
    else if (tab == CASE) drawCase(c);
    else if (tab == KEYS) drawKeys(c);
    else if (tab == MAP) drawMap(c);
    else if (tab == FILES) drawFiles(c);
    else if (tab == EXIT) drawExit(c);
    else drawOptions(c);
    if (!commands.isEmpty()) drawCommands(c);
    if (discardConfirm) drawDiscard(c);
    if(leavePrompt)drawLeavePrompt(c);
    if (!examining && !controls && !reading && tab != OPTIONS) art.header(c,paint,tab,pesetas);
    if(!puzzleVisible() && cockpitArt!=null && !examining && !controls && !reading && tab!=OPTIONS) {
      cockpitImage.eraseColor(Color.TRANSPARENT);cockpitArt.beginFrame(cockpitImage);
      cockpitArt.cockpit(cockpitCanvas,paint,NativeBridge.cockpitState(),false);
      paint.setAlpha(255);paint.setColorFilter(null);paint.setXfermode(null);
      c.drawBitmap(cockpitImage,new Rect(440,270,640,420),new RectF(layout.right(440),layout.bottom(270),layout.right(640),layout.bottom(420)),paint);
    }
    if(!examining && !controls && !reading && tab!=OPTIONS)refreshScreenOnAnimation();
  }
  private void outline(Canvas c,float[] q,int color,float width) {
    Path path=new Path();path.moveTo(q[0],q[1]);
    for(int j=1;j<4;++j)path.lineTo(q[j*2],q[j*2+1]);path.close();
    paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(width);paint.setColor(color);c.drawPath(path,paint);paint.setStyle(Paint.Style.FILL);
  }
  private int equippedId() { for(Item i:items)if(i.equipped)return i.weaponId;return 0xffff; }
  private void drawCase(Canvas c) {
    art.frame(c,paint);
    art.caseBoard(c,paint,columns,rows);
    if(!puzzleVisible())art.portrait(c,paint,equippedId());
    else art.spaceBoard(c,paint,columns,rows);
    // Original board grid colours and 100-unit cell size, projected through
    // the authored -0.57-radian case tilt and 20-degree subscreen camera.
    paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(.6f);paint.setColor(Color.rgb(64,64,64));
    for(int x=0;x<=columns;++x){float[] a=layout.casePoint(columns,rows,x,0),b=layout.casePoint(columns,rows,x,rows);c.drawLine(a[0],a[1],b[0],b[1],paint);}
    for(int y=0;y<=rows;++y){float[] a=layout.casePoint(columns,rows,0,y),b=layout.casePoint(columns,rows,columns,y);c.drawLine(a[0],a[1],b[0],b[1],paint);}
    if(puzzleVisible()){
      for(int x=0;x<=6;++x){float[] q=layout.spaceQuad(columns,rows,x,0,0,12);c.drawLine(q[0],q[1],q[6],q[7],paint);}
      for(int y=0;y<=12;++y){float[] q=layout.spaceQuad(columns,rows,0,y,6,0);c.drawLine(q[0],q[1],q[2],q[3],paint);}
    }
    for(Item i:items)if(i.onBoard()) {
      float[] q=itemQuad(i);
      art.caseItem(c,paint,i.id,q,i.rotation);
      if(i.slot==selected || i.slot==combineFrom)outline(c,q,Color.rgb(112,112,64),2);
      if(i.equipped)art.sourceTexture(c,paint,12,0,new RectF(q[0]-1,q[1]-14,q[0]+10,q[1]));
      if(i.kind==1 || i.num>1)art.number(c,paint,i.kind==1?i.loaded:i.num,q[4]-2,q[5]-2);
      if(!moving && !leavePrompt && commands.isEmpty() && !discardConfirm)hotspot(q,()->{boolean same=selected==i.slot;selected=i.slot;activeSpace=i.space;caseCursorX=i.x;caseCursorY=i.y;if(same || combineFrom>=0)confirm();else refreshScreen();});
    }
    Item i=find(selected);
    if(!moving && i==null)outline(c,(activeSpace?layout.spaceQuad(columns,rows,caseCursorX,caseCursorY,1,1):layout.caseQuad(columns,rows,caseCursorX,caseCursorY,1,1)),Color.rgb(112,112,64),2);
    if(moving && i!=null) {
      float[] q=moveSpace?layout.spaceQuad(columns,rows,moveX,moveY,moveWidth(i),moveHeight(i)):layout.caseQuad(columns,rows,moveX,moveY,moveWidth(i),moveHeight(i));
      art.caseItem(c,paint,i.id,q,moveRotation);outline(c,q,fitsMove()?Color.rgb(112,112,64):Color.rgb(180,60,40),2.5f);
    }
    art.caseHud(c,paint);
    if(i!=null && !art.itemName(c,paint,i.id,new RectF(34,layout.bottom(358),layout.right(415),layout.bottom(382)),false))text(c,i.name,34,layout.bottom(378),18,PAPER);
  }
  @Override void touchBackground(float x,float y){
    if(pending || tab!=CASE || leavePrompt || !commands.isEmpty() || examining)return;
    int[] cell=puzzleVisible()?layout.spaceCell(columns,rows,x,y):null;boolean space=cell!=null;
    if(cell==null)cell=layout.caseCell(columns,rows,x,y);if(cell==null)return;
    if(moving){activeSpace=moveSpace=space;moveX=cell[0];moveY=cell[1];Item held=find(selected);if(moveWidth(held)>boardColumns())moveRotation=(moveRotation+1)&3;clampMove();}
    else{activeSpace=space;caseCursorX=cell[0];caseCursorY=cell[1];selected=caseOccupants()[caseCursorY*boardColumns()+caseCursorX];}
    refreshScreen();
  }
  private void drawKeys(Canvas c) {
    art.keyCaptions(c,paint);
    int visibleRows=layout.listRows(143,layout.bottom(385),34,32);
    for (int column = 0; column < 2; ++column) {
      ArrayList<Item> values = keyItems(column); int at = 0;
      for (int n = 0; n < values.size(); ++n) if (values.get(n).slot == selected) at = n;
      int start = Math.max(0, Math.min(Math.max(0, values.size() - visibleRows), at - visibleRows/2));
      float left = layout.keyColumnLeft(column), width=layout.keyColumnWidth();
      if (values.isEmpty()) continue;
      for (int n = start; n < values.size() && n < start + visibleRows; ++n) {
        Item i = values.get(n); float y = 143 + (n - start) * 34;
        RectF r = new RectF(left, y, left + width, y + 31);
        if(i.slot==selected)art.listSelection(c,paint,r);
        art.item(c, paint, i.id, new RectF(left + 4, y + 1, left + 44, y + 30), 0);
        if (!art.itemName(c, paint, i.id, new RectF(left + 50, y + 6, left + width-53, y + 25), false))
          text(c, shorten(i.name, width-103, 13), left + 50, y + 21, 13, PAPER);
        if (i.num > 1) art.number(c,paint,i.num,left+width-9,y+21);
        hotspot(r, () -> { boolean same = selected == i.slot; selected = i.slot; if (same || combineFrom >= 0) confirm(); else refreshScreen(); });
      }
    }
    Item i = find(selected);
    if (i != null && !art.itemName(c, paint, i.id, new RectF(29, layout.bottom(389), layout.width-27, layout.bottom(415)), false))
      text(c, shorten(i.name, layout.width-56, 17), 29, layout.bottom(411), 17, PAPER);
  }
  private void drawLeavePrompt(Canvas c){
    RectF[] choices=art.caseLeaveMessage(c,paint,discardYes);
    for(int n=0;n<choices.length;++n){final boolean yes=n==0;hotspot(choices[n],()->{discardYes=yes;confirm();});}
  }
  private void drawCommands(Canvas c) {
    if(tab==CASE){
      Item item=find(selected);if(item==null)return;
      float[] q=itemQuad(item);
      float x=item.space?Math.max(100,Math.min(layout.width-90,q[0])):Math.min(layout.width-242,Math.max(12,q[2]));
      float y=item.y<rows/2?q[5]:q[1]-CaseMenuRules.height(caseCommandType);
      y=Math.max(110,Math.min(layout.bottom(375)-CaseMenuRules.height(caseCommandType),y));
      commandBounds=art.caseCommands(c,paint,caseCommandType,commandSelected,x,y,item.space);
      for(int n=0;!discardConfirm && n<commandBounds.length && n<commands.size();++n){final int index=n;hotspot(commandBounds[n],()->{commandSelected=index;confirm();});}
      if(discardConfirm && commandBounds.length>0){RectF at=commandBounds[Math.min(commandSelected,commandBounds.length-1)];RectF[] choices=art.caseDiscard(c,paint,discardYes,at.left,at.top,item.space);
        for(int n=0;n<choices.length;++n){final boolean yes=n==0;hotspot(choices[n],()->{discardYes=yes;confirm();});}}
      return;
    }

    RectF box = centeredBounds(199, 128, 441, 144 + commands.size() * 35);
    panel(c, box, false);
    for (int n = 0; n < commands.size(); ++n) {
      final int index = n; float y = 137 + n * 35;
      button(c, commands.get(n).title, centeredBounds(209, y, 431, y + 31), n == commandSelected, true,
          () -> { commandSelected = index; confirm(); });
    }
  }
  private void drawDiscard(Canvas c) {
    if(tab==CASE)return;
    panel(c, centeredBounds(128, 168, 512, 321), false);
    centered(c, "Discard this item?", layout.centerX(), 201+layout.extraY*.5f, 21, PAPER);
    Item i = find(selected);
    if (i != null && !art.itemName(c, paint, i.id, centeredBounds(148, 217, 492, 243), true))
      centered(c, shorten(i.name, 345, 15), layout.centerX(), 235+layout.extraY*.5f, 15, GOLD);
    button(c, "Yes", centeredBounds(158, 263, 306, 303), discardYes, true, () -> { discardYes = true; confirm(); });
    button(c, "No", centeredBounds(334, 263, 482, 303), !discardYes, true, () -> { discardYes = false; confirm(); });
  }
  private void drawExamine(Canvas c) {
    Item i = find(selected); if (i == null) return;
    float[] bounds=layout.modelBounds();modelWindow(c,new RectF(bounds[0],bounds[1],bounds[2],bounds[3]));
    if (!art.itemName(c, paint, i.id, new RectF(45, layout.bottom(377), layout.width-45, layout.bottom(403)), true))
      centered(c, i.name, layout.centerX(), layout.bottom(397), 22, PAPER);

  }
  private void drawMap(Canvas c) {
    JSONObject map = world.optJSONObject("map");
    RectF viewport = new RectF(27, 143, layout.width-27, layout.bottom(383));
    if (map == null) {art.frame(c,paint);return;}
    JSONArray paths = map.optJSONArray("paths");
    boolean modelGeometry="BIN".equals(map.optString("geometry"));
    JSONArray sourceViewport=map.optJSONArray("viewport");
    if(modelGeometry && sourceViewport!=null) {
      float cx=(float)sourceViewport.optDouble(0)+layout.extraX*.5f;
      float cy=(float)sourceViewport.optDouble(1)+layout.extraY*.5f;
      float width=(float)sourceViewport.optDouble(2)+layout.extraX;
      float height=(float)sourceViewport.optDouble(3)+layout.extraY;
      viewport=new RectF(cx-width*.5f,cy-height*.5f,cx+width*.5f,cy+height*.5f);
    }
    double minX = Double.POSITIVE_INFINITY, maxX = Double.NEGATIVE_INFINITY;
    double minZ = Double.POSITIVE_INFINITY, maxZ = Double.NEGATIVE_INFINITY;
    if (!modelGeometry && paths != null) for (int n = 0; n < paths.length(); ++n) {
      JSONObject path = paths.optJSONObject(n);
      JSONArray points = path == null ? paths.optJSONArray(n) : path.optJSONArray("points");
      if (points == null) continue;
      for (int p = 0; p < points.length(); ++p) {
        JSONArray point = points.optJSONArray(p); if (point == null || point.length() < 2) continue;
        double x = point.optDouble(0), z = point.optDouble(1);
        if (Double.isNaN(x) || Double.isInfinite(x) || Double.isNaN(z) || Double.isInfinite(z)) continue;
        minX = Math.min(minX, x); maxX = Math.max(maxX, x); minZ = Math.min(minZ, z); maxZ = Math.max(maxZ, z);
      }
    }
    if (minX == Double.POSITIVE_INFINITY) { minX = map.optDouble("minX", 0); maxX = map.optDouble("maxX", 1); minZ = map.optDouble("minZ", 0); maxZ = map.optDouble("maxZ", 1); }
    double spanX = Math.max(1, maxX - minX), spanZ = Math.max(1, maxZ - minZ);
    float scale = (float)Math.min((viewport.width() - 28) / spanX, (viewport.height() - 28) / spanZ) * mapZoom;
    double centerX = (minX + maxX) * .5, centerZ = (minZ + maxZ) * .5;
    JSONArray camera=map.optJSONArray("camera");
    double cameraHeight=10000,focal=viewport.height()/(2*Math.tan(Math.toRadians(55)*.5));
    if(modelGeometry && camera!=null) {
      centerX=camera.optDouble(0);centerZ=camera.optDouble(1);cameraHeight=camera.optDouble(2);
      focal=viewport.height()/(2*Math.tan(Math.toRadians(camera.optDouble(3,55))*.5));
      if(!mapCamera.initialized()) {
        JSONArray focus=map.optJSONArray("focus"),limits=map.optJSONArray("zoomLimits");
        mapCamera.reset(new double[]{centerX,centerZ,cameraHeight},focus==null?new double[]{centerX,centerZ,cameraHeight}:
            new double[]{focus.optDouble(0),focus.optDouble(1),focus.optDouble(2)},
            limits==null?4000/Math.tan(Math.toRadians(55)*.5):limits.optDouble(0),SystemClock.uptimeMillis());
      }
      if(mapCamera.advance(SystemClock.uptimeMillis()))refreshScreenAfter(33);
      centerX=mapCamera.position[0];centerZ=mapCamera.position[1];cameraHeight=mapCamera.position[2];
      scale=(float)(focal/cameraHeight);
    }
    JSONArray originalMarker = map.optJSONArray("player");
    boolean original = "original".equals(map.optString("source"));
    JSONObject player = world.optJSONObject("player");
    boolean markerValid = original ? map.optBoolean("markerValid", false) && originalMarker != null : player != null;
    double playerX = original && originalMarker != null ? originalMarker.optDouble(0) : player == null ? centerX : player.optDouble("x", centerX);
    double playerZ = original && originalMarker != null ? originalMarker.optDouble(1) : player == null ? centerZ : player.optDouble("z", centerZ);
    double playerYaw = original && originalMarker != null ? originalMarker.optDouble(2) : player == null ? 0 : player.optDouble("yaw", 0);
    if (!modelGeometry && mapZoom > 1 && markerValid) { centerX = playerX; centerZ = playerZ; }
    centerX += mapPanX / scale; centerZ += mapPanZ / scale;
    int save = c.save(); c.clipRect(viewport);
    c.clipRect(27,110,layout.width-27,layout.height-91);
    c.translate(viewport.centerX(), viewport.centerY());
    int currentFirst = -1, currentCount = 0;
    JSONArray rooms = map.optJSONArray("rooms");
    if (rooms != null) for (int n = 0; n < rooms.length(); ++n) {
      JSONObject room = rooms.optJSONObject(n);
      if (room != null && room.optBoolean("current", false)) {
        currentFirst = Math.max(-1, room.optInt("firstPath", -1));
        currentCount = Math.max(0, room.optInt("pathCount", 0)); break;
      }
    }
    int count=modelGeometry?orderedMapModels.size():paths==null?0:paths.length();
    for (int n = 0; n < count; ++n) {
      JSONObject path = modelGeometry?orderedMapModels.get(n):paths.optJSONObject(n);
      JSONArray points = path == null ? paths.optJSONArray(n) : path.optJSONArray("points"); if (points == null || points.length() < 2) continue;
      if(modelGeometry) {
        float[] projected=mapProjected;boolean visible=points.length()==3;
        for(int p=0;p<3 && visible;++p){JSONArray point=points.optJSONArray(p);if(point==null){visible=false;break;}
          double depth=cameraHeight-point.optDouble(2,0);
          if(depth<=1){visible=false;break;}
          projected[p*2]=(float)((point.optDouble(0)-centerX)*focal/depth);
          projected[p*2+1]=(float)((point.optDouble(1)-centerZ)*focal/depth);
        }
        if(visible)art.mapTriangle(c,paint,path,projected);
        continue;
      }
      Path trace = new Path(); boolean first = true;
      for (int p = 0; p < points.length(); ++p) {
        JSONArray point = points.optJSONArray(p); if (point == null || point.length() < 2) continue;
        float x = (float)(point.optDouble(0) - centerX) * scale, z = (float)(point.optDouble(1) - centerZ) * scale;
        if (first) { trace.moveTo(x, z); first = false; } else trace.lineTo(x, z);
      }
      boolean floor = path == null || "floor".equals(path.optString("kind"));
      if (floor) trace.close();
      paint.setStyle(floor ? Paint.Style.FILL : Paint.Style.STROKE); paint.setStrokeWidth(1.1f);
      boolean current = currentFirst >= 0 && n >= currentFirst && n - currentFirst < currentCount;
      paint.setColor(floor ? current ? Color.rgb(109, 96, 63) : Color.rgb(60, 68, 55) : Color.rgb(147, 153, 127));
      c.drawPath(trace, paint);
    }
    mapMarks(c, map.optJSONArray("doors"), centerX, centerZ, scale, Color.rgb(128, 170, 211), 4);
    mapMarks(c, map.optJSONArray("ladders"), centerX, centerZ, scale, GOLD, 3);
    if (mapItems) mapMarks(c, map.optJSONArray("items"), centerX, centerZ, scale, Color.rgb(160, 190, 117), 3);
    if (markerValid) {
      float x = (float)(playerX - centerX) * scale, z = (float)(playerZ - centerZ) * scale;
      if(modelGeometry) {
        double depth=cameraHeight-originalMarker.optDouble(3,0);
        if(depth>1)art.mapPlayer(c,paint,(float)((playerX-centerX)*focal/depth),(float)((playerZ-centerZ)*focal/depth),(float)playerYaw);
      } else {
      int marker = c.save(); c.translate(x, z); c.rotate(-(float)Math.toDegrees(playerYaw));
      Path arrow = new Path(); arrow.moveTo(0, 8); arrow.lineTo(-5, -5); arrow.lineTo(0, -2); arrow.lineTo(5, -5); arrow.close();
      paint.setStyle(Paint.Style.FILL); paint.setColor(Color.rgb(221, 85, 65)); c.drawPath(arrow, paint); c.restoreToCount(marker);
      }
    }
    if(modelGeometry) {
      JSONArray goal=map.optJSONArray("goal");
      if(goal!=null && cameraHeight-goal.optDouble(2)>1) {
        double depth=cameraHeight-goal.optDouble(2);
        art.mapGoal(c,paint,(float)((goal.optDouble(0)-centerX)*focal/depth),(float)((goal.optDouble(1)-centerZ)*focal/depth));
      }
    }
    c.restoreToCount(save);
    if(modelGeometry){art.frame(c,paint);art.mapFrame(c,paint,map.optInt("stage"),mapCamera.entire);return;}

  }
  private static double mapHeight(JSONObject path) {
    JSONArray points=path.optJSONArray("points");double height=0;
    if(points!=null)for(int n=0;n<points.length();++n){JSONArray p=points.optJSONArray(n);if(p!=null)height+=p.optDouble(2,0)/points.length();}
    return height;
  }
  private boolean hasMarks(JSONObject map, String key) {
    JSONArray marks = map.optJSONArray(key); return marks != null && marks.length() > 0;
  }
  private void mapMarks(Canvas c, JSONArray marks, double cx, double cz, float scale, int color, float radius) {
    if (marks == null) return;
    paint.setStyle(Paint.Style.FILL); paint.setColor(color);
    for (int n = 0; n < marks.length(); ++n) {
      JSONObject mark = marks.optJSONObject(n); if (mark == null) continue;
      float x = (float)(mark.optDouble("x", 0) - cx) * scale, z = (float)(mark.optDouble("z", 0) - cz) * scale;
      c.drawCircle(x, z, radius, paint);
    }
  }
  private void drawFiles(Canvas c) {
    if (files.length() == 0) return;
    if (reading) {
      JSONObject file = files.optJSONObject(documentSelected); if (file == null) return;
      if (!art.itemName(c, paint, file.optInt("id", -1), new RectF(37, 83, layout.width-37, 110), true))
        centered(c, file.optString("title", "Document"), layout.centerX(), 105, 20, PAPER);
      JSONArray pages = file.optJSONArray("pages");
      int pageCount = file.optInt("pageCount", pages == null ? 0 : pages.length());
      panel(c, new RectF(37, 122, layout.width-37, layout.bottom(404)), false);
      if (pageCount > 0) {
        documentPage = Math.max(0, Math.min(pageCount - 1, documentPage));
        boolean original = art.filePage(c, paint, file.optInt("id"), documentPage, new RectF(43, 128, layout.width-43, layout.bottom(398)));
        if (!original && pages != null && documentPage < pages.length())
          paragraph(c, pages.optString(documentPage, ""), 54, 148, layout.width-108, 16, layout.bottom(385), PAPER);
        
        rightText(c, (documentPage + 1) + " / " + pageCount, layout.width-40, layout.bottom(414), 12, GOLD);
      }
      return;
    }

    int visibleRows=layout.listRows(143,layout.bottom(385),39,32);
    int start = Math.max(0, Math.min(Math.max(0, files.length() - visibleRows), documentSelected - visibleRows/2));
    for (int n = start; n < files.length() && n < start + visibleRows; ++n) {
      JSONObject file = files.optJSONObject(n); if (file == null) continue;
      final int selected = n; float y = 143 + (n - start) * 39;
      RectF bounds = new RectF(30, y, layout.width-30, y + 35);
      if(n==documentSelected)art.listSelection(c,paint,bounds);
      if (!art.itemName(c, paint, file.optInt("id", -1), new RectF(48, y + 5, layout.width-48, y + 30), false))
        text(c, shorten(file.optString("title", "Document"), layout.width-96, 15), 48, y + 23, 15, PAPER);
      hotspot(bounds, () -> { documentSelected = selected; confirm(); });
    }
  }
  private void showOptions() {
    if (pending) return;
    if(moving)return;
    if(hasSpace()){requestLeave(this::showOptions);return;}
    activeSpace=false;
    preview.onPreview(-1); tab = OPTIONS; examining = reading = moving = false;
    commands.clear(); discardConfirm = false; combineFrom = -1; refreshScreen();
  }
  private void drawExit(Canvas c) {
    button(c, "Resume game", centeredBounds(144, 194, 496, 244), !exitOptions, true, this::dismiss);
    button(c, "Options", centeredBounds(144, 268, 496, 318), exitOptions, true, this::showOptions);
  }
  private void drawOptions(Canvas c) {
    String[] labels = { "Controls", "Tests and diagnostics", "Back to room selection" };
    for (int n = 0; n < labels.length; ++n) {
      final int index = n; float y = 144 + n * 62;
      button(c, labels[n], centeredBounds(136, y, 504, y + 47), n == optionSelected, true,
          () -> { optionSelected = index; confirm(); });
    }
  }
  private void drawControls(Canvas c) {
    String[] labels = {
      "Left stick   Move / Turn", "Right stick   Look / Aim",
      "L2 / LT   Aim handgun / shotgun", "L1 / LB   Draw knife",
      "R2 / RT   Fire / Knife attack", "A / Cross   Action / Pick up item",
      "B / Circle   Run / Cancel", "Y / Triangle   Case", "Start   In-game menu", "View / Select   Map"
    };
    for (int n = 0; n < labels.length; ++n) text(c, labels[n], 58+layout.extraX*.5f, 112+layout.extraY*.5f + n * 28, 17, PAPER);
  }
}
