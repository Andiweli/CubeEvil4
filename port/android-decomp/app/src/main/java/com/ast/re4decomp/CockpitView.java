package com.ast.re4decomp;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.View;

/** Original CORE ID artwork anchored to the Android screen's safe lower edge. */
final class CockpitView extends View {
  private StartupArt art;
  private final Paint paint=new Paint(Paint.FILTER_BITMAP_FLAG);
  private final Bitmap picture=Bitmap.createBitmap(640,480,Bitmap.Config.ARGB_8888);
  private final Canvas source=new Canvas(picture);
  CockpitView(Context context){super(context);setClickable(false);setFocusable(false);}
  void artwork(StartupArt value){art=value;invalidate();}
  @Override protected void onDraw(Canvas c) {
    super.onDraw(c);if(art==null)return;
    picture.eraseColor(Color.TRANSPARENT);art.beginFrame(picture);
    art.cockpit(source,paint,NativeBridge.cockpitState());
    float scale=Math.min((getWidth()-getPaddingLeft()-getPaddingRight())/640f,(getHeight()-getPaddingTop()-getPaddingBottom())/360f);
    float width=640*scale,height=360*scale;
    c.drawBitmap(picture,new Rect(0,60,640,420),new RectF(getWidth()-getPaddingRight()-width,getHeight()-getPaddingBottom()-height,getWidth()-getPaddingRight(),getHeight()-getPaddingBottom()),paint);
    if(getVisibility()==VISIBLE)postInvalidateOnAnimation();
  }
}
