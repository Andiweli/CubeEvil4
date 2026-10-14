package com.ast.re4decomp;

import android.content.Context;
import android.graphics.*;
import android.view.View;

/** r101's original ROOM_ARC27/28 ID artwork, over the live binocular camera. */
final class BinocularView extends View {
  private StartupArt art;
  private final Paint paint=new Paint(Paint.FILTER_BITMAP_FLAG);
  private final Bitmap picture=Bitmap.createBitmap(640,480,Bitmap.Config.ARGB_8888);
  private final Canvas source=new Canvas(picture);
  BinocularView(Context context){super(context);setClickable(false);setFocusable(false);}
  void artwork(StartupArt value){art=value;invalidate();}
  @Override protected void onDraw(Canvas canvas) {
    super.onDraw(canvas);if(art==null)return;
    picture.eraseColor(Color.TRANSPARENT);art.beginFrame(picture);art.binocular(source,paint,NativeBridge.binocularState());
    float scale=Math.min(getWidth()/640f,getHeight()/360f),width=640*scale,height=360*scale;
    float left=(getWidth()-width)*.5f,top=(getHeight()-height)*.5f;
    canvas.drawBitmap(picture,new Rect(0,60,640,420),new RectF(left,top,left+width,top+height),paint);
    if(getVisibility()==VISIBLE)postInvalidateOnAnimation();
  }
}
