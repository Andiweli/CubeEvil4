package com.ast.re4decomp;

import android.content.Context;
import android.graphics.*;
import android.view.View;

/** r101_Event30_TitleCall: original UWF/EFF from bell frame1100 until exit. */
final class EventTitleView extends View {
  private StartupArt artwork;
  private final Paint paint=new Paint(Paint.FILTER_BITMAP_FLAG);
  private final Bitmap picture=Bitmap.createBitmap(640,480,Bitmap.Config.ARGB_8888);
  private final Canvas source=new Canvas(picture);
  EventTitleView(Context context){super(context);setClickable(false);setFocusable(false);}
  void artwork(StartupArt value){artwork=value;invalidate();}
  @Override protected void onDraw(Canvas canvas) {
    super.onDraw(canvas);
    int[] state=NativeBridge.eventTitleState();
    // The view is already transparent/active during the bell. Its first
    // title frame then follows onDraw, not the slower 100ms HUD poll.
    if(artwork!=null && state[0]!=0) {
      picture.eraseColor(Color.TRANSPARENT);artwork.beginFrame(picture);
      artwork.draw(source,paint,8000,state[1],0,1);
      float scale=Math.min(getWidth()/640f,getHeight()/360f),width=640*scale,height=360*scale;
      float left=(getWidth()-width)*.5f,top=(getHeight()-height)*.5f;
      canvas.drawBitmap(picture,new Rect(0,60,640,420),new RectF(left,top,left+width,top+height),paint);
    }
    if(getVisibility()==VISIBLE)postInvalidateOnAnimation();
  }
}
