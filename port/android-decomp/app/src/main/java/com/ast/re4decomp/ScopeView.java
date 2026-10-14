package com.ast.re4decomp;

import android.content.Context;
import android.graphics.*;
import android.view.View;
import java.util.Map;

/** CameraScope/IdScope artwork from the equipped original weapon archive. */
final class ScopeView extends View {
  private Map<Integer,StartupArt> artwork;
  private final Paint paint=new Paint(Paint.FILTER_BITMAP_FLAG);
  private final Bitmap picture=Bitmap.createBitmap(640,480,Bitmap.Config.ARGB_8888);
  private final Canvas source=new Canvas(picture);
  ScopeView(Context context){super(context);setClickable(false);setFocusable(false);}
  void artwork(Map<Integer,StartupArt> value){artwork=value;invalidate();}
  @Override protected void onDraw(Canvas canvas) {
    super.onDraw(canvas);
    int[] state=NativeBridge.scopeState();
    StartupArt art=artwork==null||state[0]==0?null:artwork.get(state[0]);
    if(art!=null) {
      picture.eraseColor(Color.TRANSPARENT);art.beginFrame(picture);art.scope(source,paint,state);
      float scale=Math.min(getWidth()/640f,getHeight()/360f),width=640*scale,height=360*scale;
      float left=(getWidth()-width)*.5f,top=(getHeight()-height)*.5f;
      canvas.drawBitmap(picture,new Rect(0,60,640,420),new RectF(left,top,left+width,top+height),paint);
    }
    if(getVisibility()==VISIBLE)postInvalidateOnAnimation();
  }
}
