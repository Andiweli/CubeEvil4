package com.ast.re4decomp;

import android.app.Dialog;
import android.content.Context;
import android.graphics.*;
import android.os.Bundle;
import android.view.*;

/** Original option.dat DEAD/CONTINUE ID files over the completed game frame. */
final class ContinueDialog extends Dialog {
  interface Listener { void completed(boolean retry); }
  private final StartupArt art;
  private final Bitmap backdrop;
  private final ContinueFlow flow;
  private final Listener listener;
  private Screen screen;
  private boolean active=true,completed;
  private long previous;
  private double accumulator;
  private int axis;
  ContinueDialog(Context context,StartupArt art,int[] picture,ContinueFlow flow,Listener listener) {
    super(context,android.R.style.Theme_Black_NoTitleBar_Fullscreen);
    this.art=art;this.flow=flow;this.listener=listener;
    // The int[] factory produces an immutable bitmap. The confirmed
    // YES/NO fade replaces this frame, so allocate writable pixels.
    backdrop=Bitmap.createBitmap(picture[0],picture[1],Bitmap.Config.ARGB_8888);
    backdrop.setPixels(picture,2,picture[0],0,0,picture[0],picture[1]);
    setCancelable(false);
  }
  @Override protected void onCreate(Bundle state) {
    super.onCreate(state);screen=new Screen();setContentView(screen);
    screen.setFocusableInTouchMode(true);screen.requestFocus();
    Window window=getWindow();if(window!=null) {
      window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN|WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
      window.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
      window.getDecorView().setSystemUiVisibility(5894);
    }
  }
  void active(boolean value){active=value;previous=0;if(screen!=null)screen.invalidate();}
  private void cue(int number){if(number>=0){NativeBridge.continueSound(number);if(number==5||number==8)NativeBridge.continueConfirmed();}if(screen!=null)screen.invalidate();}
  @Override public void onBackPressed() { /* Original requires YES/NO confirmation. */ }
  @Override public boolean dispatchKeyEvent(KeyEvent event) {
    int key=event.getKeyCode();boolean confirm=key==KeyEvent.KEYCODE_BUTTON_A||key==KeyEvent.KEYCODE_ENTER||key==KeyEvent.KEYCODE_DPAD_CENTER;
    boolean left=key==KeyEvent.KEYCODE_DPAD_LEFT,right=key==KeyEvent.KEYCODE_DPAD_RIGHT;
    if(!confirm&&!left&&!right)return super.dispatchKeyEvent(event);
    if(active&&event.getAction()==KeyEvent.ACTION_DOWN&&event.getRepeatCount()==0)
      cue(confirm?flow.confirm():flow.move(left));
    return true;
  }
  @Override public boolean onGenericMotionEvent(MotionEvent event) {
    if((event.getSource()&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK) {
      float x=event.getAxisValue(MotionEvent.AXIS_HAT_X);if(Math.abs(x)<.5f)x=event.getAxisValue(MotionEvent.AXIS_X);
      int next=x<-.5f?-1:x>.5f?1:0;
      if(active&&next!=0&&next!=axis)cue(flow.move(next<0));axis=next;return true;
    }
    return super.onGenericMotionEvent(event);
  }
  private final class Screen extends View {
    private final Bitmap picture=Bitmap.createBitmap(640,480,Bitmap.Config.ARGB_8888);
    private final Canvas canvas=new Canvas(picture);
    private final Paint paint=new Paint(Paint.FILTER_BITMAP_FLAG);
    private boolean exitBackdrop;
    Screen(){super(ContinueDialog.this.getContext());}
    @Override protected void onDraw(Canvas c) {
      long now=System.nanoTime();if(active&&previous!=0)accumulator+=Math.min(.1,(now-previous)/1e9)*30;previous=now;
      while(active&&accumulator>=1){flow.tick();accumulator-=1;}
      if(flow.leaving()&&!exitBackdrop) {
        int[] pixels=NativeBridge.deathBackdrop();
        if(pixels!=null&&pixels.length==2+640*480){backdrop.setPixels(pixels,2,640,0,0,640,480);exitBackdrop=true;}
      }
      canvas.drawBitmap(backdrop,null,new RectF(0,0,640,480),paint);art.beginFrame(picture);
      art.death(canvas,paint,flow);
      c.drawBitmap(picture,null,new RectF(0,0,getWidth(),getHeight()),paint);
      if(active&&flow.phase==ContinueFlow.Phase.DONE&&!completed){completed=true;post(()->{dismiss();listener.completed(flow.yes);});}
      else if(active)postInvalidateOnAnimation();
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
      if(active&&event.getAction()==MotionEvent.ACTION_UP) {
        if(flow.phase==ContinueFlow.Phase.CHOICE)cue(flow.move(event.getX()<getWidth()/2f));
        cue(flow.confirm());
      }
      return true;
    }
  }
}
