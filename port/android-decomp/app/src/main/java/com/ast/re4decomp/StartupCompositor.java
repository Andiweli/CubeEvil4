package com.ast.re4decomp;

import android.graphics.*;
import java.util.IdentityHashMap;

/** The original special units sample the already drawn EFB, never their map as visible artwork. */
final class StartupCompositor {
  private static final int WIDTH=640,HEIGHT=480;
  private final int[]framePixels=new int[WIDTH*HEIGHT];
  private final IdentityHashMap<Bitmap,int[]>decoded=new IdentityHashMap<>();
  private Bitmap target;
  private float sampleTop,sampleBottom=HEIGHT;
  void begin(Bitmap picture){begin(picture,0,HEIGHT);}
  void begin(Bitmap picture,float top,float height){if(picture.getWidth()!=WIDTH||picture.getHeight()!=HEIGHT)throw new IllegalArgumentException("Original title framebuffer");target=picture;sampleTop=top;sampleBottom=top+height;}
  private int[] pixels(Bitmap texture){int[]p=decoded.get(texture);if(p==null){p=new int[texture.getWidth()*texture.getHeight()];texture.getPixels(p,0,texture.getWidth(),0,0,texture.getWidth(),texture.getHeight());decoded.put(texture,p);}return p;}
  @SuppressWarnings("deprecation") // Source Canvas matrix supplies the authored quad's inverse coordinates.
  void draw(Canvas canvas,Bitmap texture,Bitmap mask,RectF bounds,float[]rgba,int alpha,int type,int sub,int blend){if(target==null)throw new IllegalStateException("Missing title framebuffer");Matrix inverse=new Matrix();Matrix matrix=canvas.getMatrix();if(!matrix.invert(inverse))return;float[]m=new float[9];inverse.getValues(m);RectF affected=new RectF(bounds);matrix.mapRect(affected);int x0=Math.max(0,(int)Math.floor(affected.left)),y0=Math.max(0,(int)Math.floor(affected.top)),x1=Math.min(WIDTH,(int)Math.ceil(affected.right)),y1=Math.min(HEIGHT,(int)Math.ceil(affected.bottom));if(x0>=x1||y0>=y1)return;
    int[]texturePixels=pixels(texture),maskPixels=mask==null?null:pixels(mask);
    float[]parameters={texture.getWidth(),texture.getHeight(),mask==null?0:mask.getWidth(),mask==null?0:mask.getHeight(),m[0],m[1],m[2],m[3],m[4],m[5],bounds.left,bounds.top,bounds.right,bounds.bottom,rgba[0],rgba[1],rgba[2],alpha,type,sub,blend,x0,y0,x1,y1,0,sampleTop,WIDTH,sampleBottom};
    // Ordinary special blends read only their destination rectangle. EFB
    // distortions can sample outside it and still need the complete image.
    if(type==0)target.getPixels(framePixels,y0*WIDTH+x0,WIDTH,x0,y0,x1-x0,y1-y0);
    else target.getPixels(framePixels,0,WIDTH,0,0,WIDTH,HEIGHT);
    NativeBridge.startupCompose(framePixels,texturePixels,maskPixels,parameters);
    target.setPixels(framePixels,y0*WIDTH+x0,WIDTH,x0,y0,x1-x0,y1-y0);
  }
}
