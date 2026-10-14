package com.ast.re4decomp;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.*;
import android.os.*;
import android.view.*;
import java.io.IOException;

/** Original card screen over the paused typewriter scene. */
final class SaveGameDialog extends Dialog {
  interface Listener {void save(int slot);void delete(int slot);void closed();}
  private final StartupArt art;
  private final GameSaveStore store;
  private final Listener listener;
  private final CardFlow flow=new CardFlow(true);
  private final Handler handler=new Handler(Looper.getMainLooper());
  private OriginalSaveHeader[] headers;
  private boolean closed;
  private Screen screen;
  private int stickDirection,stickTicks;
  private final OriginalFrameClock clock=new OriginalFrameClock();
  private final Runnable tick=new Runnable(){@Override public void run(){
    if(closed)return;int frames=clock.advance(System.nanoTime());
    for(int n=0;n<frames;++n){flow.tick();if(flow.phase==CardFlow.Phase.CLOSED){dismiss();return;}
    if(flow.operation==CardFlow.Operation.SAVE&&(flow.busy()||flow.phase==CardFlow.Phase.DONE)&&art.cardWritingCue(flow.operationFrame))NativeBridge.menuSound(0x2e);
    if(stickDirection!=0&&++stickTicks>=10){move(stickDirection);stickTicks=5;}
    }
    if(frames>0&&screen!=null)screen.invalidate();handler.postDelayed(this,clock.delayMillis());
  }};
  SaveGameDialog(Activity activity,StartupArt art,GameSaveStore store,OriginalSaveHeader[] rows,Listener listener){
    super(activity,android.R.style.Theme_Material_NoActionBar_Fullscreen);this.art=art;this.store=store;this.listener=listener;
    headers=rows;flow.open(headers,store.initialSelection(headers),art.cardDuration(10),Math.max(art.cardDuration(8),art.cardDuration(9)));setCancelable(false);
  }
  @Override protected void onCreate(Bundle state){
    super.onCreate(state);screen=new Screen();setContentView(screen);screen.setFocusableInTouchMode(true);screen.requestFocus();
    Window w=getWindow();if(w!=null){w.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN|WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);w.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);w.getDecorView().setSystemUiVisibility(5894);}
    if(Build.VERSION.SDK_INT>=33)getOnBackInvokedDispatcher().registerOnBackInvokedCallback(android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT,this::onBackPressed);
  }
  @Override protected void onStart(){super.onStart();Window w=getWindow();if(w!=null)w.setLayout(ViewGroup.LayoutParams.MATCH_PARENT,ViewGroup.LayoutParams.MATCH_PARENT);clock.reset();handler.removeCallbacks(tick);handler.post(tick);}
  private void move(int direction){if(flow.move(direction)){NativeBridge.menuSound(flow.choices()?0xe:direction<0?0x2f:0x2c);screen.invalidate();}}
  private boolean horizontalChoices(){return flow.choices()&&art.cardHorizontalChoices(flow.message);}
  private void confirm(){
    CardFlow.Phase before=flow.phase;CardFlow.Operation operation=flow.confirm();
    if(operation!=CardFlow.Operation.NONE){store.rememberSelection(flow.cursor);NativeBridge.menuSound(4);if(operation==CardFlow.Operation.SAVE)NativeBridge.menuSound(0x2d);screen.invalidate();if(operation==CardFlow.Operation.DELETE)listener.delete(flow.cursor);else listener.save(flow.cursor);return;}
    if(flow.phase==CardFlow.Phase.CLOSED){NativeBridge.menuSound(5);dismiss();return;}
    if(flow.phase!=before){NativeBridge.menuSound(flow.phase==CardFlow.Phase.SELECT?5:4);screen.invalidate();}
  }
  void completed(OriginalSaveHeader[] rows,IOException failure){
    if(closed)return;headers=rows;flow.refresh(headers);flow.completed(failure==null);
    NativeBridge.menuSound(failure==null?4:0x2a);screen.invalidate();
  }
  @Override public void onBackPressed(){if(flow.busy())return;flow.back();if(flow.phase==CardFlow.Phase.CLOSED)dismiss();else{NativeBridge.menuSound(5);screen.invalidate();}}
  @Override public void dismiss(){handler.removeCallbacks(tick);stickDirection=0;super.dismiss();if(!closed){closed=true;listener.closed();}}
  @Override public boolean dispatchKeyEvent(KeyEvent e){
    int key=e.getKeyCode();boolean handled=key==KeyEvent.KEYCODE_DPAD_UP||key==KeyEvent.KEYCODE_DPAD_DOWN||key==KeyEvent.KEYCODE_DPAD_LEFT||key==KeyEvent.KEYCODE_DPAD_RIGHT||key==KeyEvent.KEYCODE_BUTTON_A||key==KeyEvent.KEYCODE_BUTTON_B||key==KeyEvent.KEYCODE_ENTER||key==KeyEvent.KEYCODE_DPAD_CENTER||key==KeyEvent.KEYCODE_BUTTON_START||key==KeyEvent.KEYCODE_BACK;
    if(!handled)return super.dispatchKeyEvent(e);
    if(e.getAction()==KeyEvent.ACTION_DOWN){if(key==KeyEvent.KEYCODE_DPAD_UP){if(!horizontalChoices())move(-1);}else if(key==KeyEvent.KEYCODE_DPAD_DOWN){if(!horizontalChoices())move(1);}else if(key==KeyEvent.KEYCODE_DPAD_LEFT){if(horizontalChoices())move(-1);}else if(key==KeyEvent.KEYCODE_DPAD_RIGHT){if(horizontalChoices())move(1);}else if(key==KeyEvent.KEYCODE_BUTTON_B||key==KeyEvent.KEYCODE_BACK)onBackPressed();else if(e.getRepeatCount()==0)confirm();}return true;
  }
  @Override public boolean onGenericMotionEvent(MotionEvent e){
    if((e.getSource()&InputDevice.SOURCE_CLASS_JOYSTICK)==0)return super.onGenericMotionEvent(e);
    boolean horizontal=horizontalChoices();float axis=e.getAxisValue(horizontal?MotionEvent.AXIS_HAT_X:MotionEvent.AXIS_HAT_Y);
    if(Math.abs(axis)<.5f)axis=e.getAxisValue(horizontal?MotionEvent.AXIS_X:MotionEvent.AXIS_Y);
    int next=axis<-.5f?-1:axis>.5f?1:0;if(next!=stickDirection){stickDirection=next;stickTicks=0;if(next!=0)move(next);}return true;
  }
  private final class Screen extends View {
    final Bitmap source=Bitmap.createBitmap(640,480,Bitmap.Config.ARGB_8888);
    final Canvas canvas=new Canvas(source);final Paint paint=new Paint(Paint.FILTER_BITMAP_FLAG);
    StartupViewport viewport;
    Screen(){super(SaveGameDialog.this.getContext());}
    @Override protected void onDraw(Canvas c){viewport=new StartupViewport(getWidth(),getHeight(),StartupFlow.Phase.LOAD);source.eraseColor(Color.BLACK);art.beginFrame(source,viewport);art.animationFrame=flow.presentationFrame;art.card(canvas,paint,flow,headers);c.drawBitmap(source,new Rect(0,(int)viewport.top,640,(int)(viewport.top+viewport.height)),new RectF(0,0,getWidth(),getHeight()),paint);}
    @Override public boolean onTouchEvent(MotionEvent e){
      if(e.getAction()!=MotionEvent.ACTION_UP||viewport==null)return true;float y=viewport.sourceY(e.getY());
      if(flow.choices()){float x=viewport.sourceX(e.getX());RectF[]choices=art.cardChoiceBounds(flow.message);
        for(int i=0;i<choices.length;++i)if(choices[i].contains(x,y)){flow.choice=i;confirm();break;}}
      else if(!flow.selecting())confirm();else if(y>340)onBackPressed();else if(y<210)move(-1);else if(y>270)move(1);else confirm();return true;
    }
  }
}
