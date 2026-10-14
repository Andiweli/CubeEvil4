package com.ast.re4decomp;

import android.graphics.*;
import java.util.*;
import org.json.*;

/** Immutable decoded original artwork; all native I/O runs before publication. */
final class StartupArt {
  final HashMap<Integer,Bitmap> textures = new HashMap<>();
  final HashMap<Integer,Bitmap[]> animated=new HashMap<>();
  int animationFrame;
  private final CockpitState cockpit=new CockpitState();
  private OriginalSaveHeader slotHeader;
  private CardFlow cardFlow;
  private ContinueFlow continueFlow;
  private final StartupCompositor compositor=new StartupCompositor();
  private float binocularLeft,binocularRight,binocularGaugeY,binocularGaugeHeight,binocularPreviousFov=45;
  private int binocularPreviousTick=-1;
  private boolean binocularArt;
  void beginFrame(Bitmap picture){compositor.begin(picture);}
  void beginFrame(Bitmap picture,StartupViewport viewport){compositor.begin(picture,viewport.top,viewport.height);}
  final HashMap<Integer,List<Unit>> groups = new HashMap<>();
  final HashMap<Integer,int[]>orders=new HashMap<>();
  final HashMap<Integer,int[][]> messages=new HashMap<>();
  int[][] bearings;
  Bitmap[] font;
  static final class Unit {
    int mark, number, parent, kind, flags, texture, mask, loop, scaleType, rotAxis, anchor, blend, order,textureFlags,textureFrames,textureAnimationFrames,transType,transSub;
    float x,y,width,height,rotationX,rotationY,rotation;
    int selectedFrame=-1;
    float uvLeft=0,uvTop=0,uvRight=1,uvBottom=1;
    float[] color=new float[4], color1=new float[4];
    float[][][] curves=new float[4][][];
    float[][] coefficients;
    Unit(JSONObject j) throws JSONException {
      mark=j.getInt("mark"); number=j.getInt("unit");parent=j.getInt("parent");kind=j.getInt("kind");flags=j.getInt("flags");
      texture=j.getInt("texture");mask=j.getInt("mask");loop=j.getInt("loop");scaleType=j.getInt("scaleType");rotAxis=j.getInt("rotAxis");anchor=j.getInt("anchor");blend=j.getInt("blend");order=j.getInt("order");
      textureFlags=j.getInt("textureFlags");textureFrames=j.getInt("textureFrames");transType=j.getInt("transType");transSub=j.getInt("transSub");
      textureAnimationFrames=j.optInt("textureAnimationFrames",1);
      x=(float)j.getDouble("x");y=(float)j.getDouble("y");width=(float)j.getDouble("width");height=(float)j.getDouble("height");rotation=(float)j.getDouble("rotation");
      rotationX=(float)j.optDouble("rotationX",0);rotationY=(float)j.optDouble("rotationY",0);
      for(int k=0;k<4;++k){color[k]=(float)j.getJSONArray("color").getDouble(k);color1[k]=(float)j.getJSONArray("color1").getDouble(k);curves[k]=rows(j.getJSONArray("curves").getJSONArray(k),4);}
      coefficients=StartupMath.coefficients(rows(j.getJSONArray("path"),3),order);
    }
    float sample(int channel,float frame,float fallback){return StartupMath.curve(curves[channel],frame,fallback,(loop&(1<<channel))!=0);}
    float[] tint(float frame){return StartupPixels.tint(color,color1,curves[2],frame,(loop&4)!=0);}
    float[] transform(float frame){float px=x,py=y;if(coefficients.length>0){float t=sample(0,frame,0);float[]p=StartupMath.position(coefficients,order,t),zero=StartupMath.position(coefficients,order,0);px+=p[0]-zero[0];py+=p[1]-zero[1];}return StartupMath.idTransform(px,py,rotAxis==0?sample(3,frame,rotationX):rotationX,rotAxis==1?sample(3,frame,rotationY):rotationY,rotAxis==2?sample(3,frame,rotation):rotation);}
    Matrix matrix(float frame){Matrix m=new Matrix();m.setValues(StartupMath.screenMatrix(transform(frame)));return m;}
  }
  private static float[][] rows(JSONArray j,int columns)throws JSONException {float[][]r=new float[j.length()][columns];for(int n=0;n<j.length();++n)for(int k=0;k<columns;++k)r[n][k]=(float)j.getJSONArray(n).getDouble(k);return r;}
  static StartupArt load() throws JSONException {
    StartupArt art=new StartupArt();HashSet<Integer> ids=new HashSet<>();HashMap<Integer,Integer>frames=new HashMap<>();
    for(int group:new int[]{6,8,10,11,13,1009,1010,1012,1013,1014,2005,2006,2007,2008,2009}){
      List<Unit> units=new ArrayList<>();JSONArray list=new JSONObject(NativeBridge.startupLayout(group)).getJSONArray("units");
      int[][]hierarchy=new int[list.length()][3];
      for(int n=0;n<list.length();++n){Unit unit=new Unit(list.getJSONObject(n));units.add(unit);hierarchy[n]=new int[]{unit.number,unit.parent,unit.kind};if(unit.texture!=255){ids.add(unit.texture);frames.put(unit.texture,unit.textureFrames);}if(unit.mask!=255)ids.add(unit.mask);}art.groups.put(group,units);art.orders.put(group,StartupMath.drawOrder(hierarchy));
    }
    for(int id:ids){int[]p=NativeBridge.startupTexture(id,0);if(p!=null&&p.length>=3&&p[0]>0&&p[1]>0&&(long)p[0]*p[1]==p.length-2)art.textures.put(id,Bitmap.createBitmap(p,2,p[0],p[0],p[1],Bitmap.Config.ARGB_8888));}
    for(int id:frames.keySet())if(frames.get(id)>1){Bitmap[]images=new Bitmap[frames.get(id)];images[0]=art.textures.get(id);for(int frame=1;frame<images.length;++frame){int[]p=NativeBridge.startupTexture(id,frame);if(p!=null&&p.length>2)images[frame]=Bitmap.createBitmap(p,2,p[0],p[0],p[1],Bitmap.Config.ARGB_8888);}art.animated.put(id,images);}
    for(int frame=0;frame<10;++frame){int[]p=NativeBridge.startupTexture(263,frame);if(p!=null&&p.length>2)art.textures.put(10000+frame,Bitmap.createBitmap(p,2,p[0],p[0],p[1],Bitmap.Config.ARGB_8888));}
    for(int frame=0;frame<10;++frame){int[]p=NativeBridge.startupTexture(513,frame);if(p!=null&&p.length>2)art.textures.put(10020+frame,Bitmap.createBitmap(p,2,p[0],p[0],p[1],Bitmap.Config.ARGB_8888));}
    JSONObject font=new JSONObject(NativeBridge.startupLayout(3000));art.bearings=intRows(font.getJSONArray("bearings"),2);
    JSONArray sheets=font.getJSONArray("sheets");art.font=new Bitmap[sheets.length()];
    for(int i=0;i<sheets.length();++i){int[]p=NativeBridge.startupTexture(sheets.getInt(i),0);if(p!=null&&p.length>2)art.font[i]=Bitmap.createBitmap(p,2,p[0],p[0],p[1],Bitmap.Config.ARGB_8888);}
    for(int message:new int[]{0x81,0x82,0x83,0x84,0x85,0x86,0x8b,0x8c,0x92,0x8d,0x8e})art.messages.put(message,intRows(new JSONObject(NativeBridge.startupLayout(4000+message)).getJSONArray("glyphs"),3));
    for(int message:new int[]{1,11,12,13,14,15,16,17,18,19,20,22,30,31,32,36,38})art.messages.put(5000+message,intRows(new JSONObject(NativeBridge.startupLayout(5000+message)).getJSONArray("glyphs"),3));
    return art;
  }
  static StartupArt loadCockpit() throws JSONException {
    StartupArt art=new StartupArt();HashSet<Integer> ids=new HashSet<>();
    HashMap<Integer,Integer> frames=new HashMap<>();
    for(int group:new int[]{6031,6032,6038}) {
      JSONArray list=new JSONObject(NativeBridge.startupLayout(group)).getJSONArray("units");
      List<Unit> units=new ArrayList<>();int[][] hierarchy=new int[list.length()][3];
      for(int n=0;n<list.length();++n) {
        Unit u=new Unit(list.getJSONObject(n));units.add(u);
        hierarchy[n]=new int[]{u.number,u.parent,u.kind};
        if(u.texture!=255){ids.add(u.texture);frames.put(u.texture,u.textureFrames);}
        if(u.mask!=255)ids.add(u.mask);
      }
      art.groups.put(group,units);art.orders.put(group,StartupMath.drawOrder(hierarchy));
    }
    for(int id:ids) {
      int count=frames.containsKey(id)?Math.max(1,frames.get(id)):1;Bitmap[] animation=new Bitmap[count];
      for(int frame=0;frame<count;++frame) {
        int[] p=NativeBridge.startupTexture(id,frame);
        if(p==null || p.length<3 || p[0]<1 || p[1]<1 || (long)p[0]*p[1]!=p.length-2)
          throw new JSONException("Original cockpit texture missing: "+id);
        animation[frame]=Bitmap.createBitmap(p,2,p[0],p[0],p[1],Bitmap.Config.ARGB_8888);
      }
      art.textures.put(id,animation[0]);if(count>1)art.animated.put(id,animation);
    }
    JSONObject font=new JSONObject(NativeBridge.startupLayout(3000));art.bearings=intRows(font.getJSONArray("bearings"),2);
    JSONArray sheets=font.getJSONArray("sheets");art.font=new Bitmap[sheets.length()];
    for(int n=0;n<sheets.length();++n){int[]p=NativeBridge.startupTexture(sheets.getInt(n),0);
      if(p!=null && p.length>2)art.font[n]=Bitmap.createBitmap(p,2,p[0],p[0],p[1],Bitmap.Config.ARGB_8888);}
    for(int kind=0;kind<=47;++kind)art.messages.put(0x16+kind,
        intRows(new JSONObject(NativeBridge.startupLayout(4000+0x16+kind)).getJSONArray("glyphs"),3));
    return art;
  }
  static int[][]intRows(JSONArray array,int columns)throws JSONException{int[][]rows=new int[array.length()][columns];for(int i=0;i<rows.length;++i)for(int j=0;j<columns;++j)rows[i][j]=array.getJSONArray(i).getInt(j);return rows;}
  static StartupArt loadDeath() throws JSONException {
    StartupArt art=new StartupArt();HashMap<Integer,Integer> frames=new HashMap<>();
    for(int group:new int[]{7005,7006,7007}) {
      JSONArray list=new JSONObject(NativeBridge.startupLayout(group)).getJSONArray("units");
      List<Unit> units=new ArrayList<>();int[][] hierarchy=new int[list.length()][3];
      for(int n=0;n<list.length();++n){Unit u=new Unit(list.getJSONObject(n));units.add(u);hierarchy[n]=new int[]{u.number,u.parent,u.kind};
        if(u.texture!=255)frames.put(u.texture,Math.max(1,u.textureFrames));if(u.mask!=255&&!frames.containsKey(u.mask))frames.put(u.mask,1);}
      art.groups.put(group,units);art.orders.put(group,StartupMath.drawOrder(hierarchy));
    }
    for(int id:frames.keySet()) {
      Bitmap[] images=new Bitmap[frames.get(id)];
      for(int frame=0;frame<images.length;++frame){int[] p=NativeBridge.startupTexture(id,frame);
        if(p==null||p.length<3||p[0]<1||p[1]<1||(long)p[0]*p[1]!=p.length-2)throw new JSONException("Original death texture missing: "+id);
        images[frame]=Bitmap.createBitmap(p,2,p[0],p[0],p[1],Bitmap.Config.ARGB_8888);}
      art.textures.put(id,images[0]);if(images.length>1)art.animated.put(id,images);
    }
    return art;
  }
  static StartupArt loadBinocular() throws JSONException {
    StartupArt art=loadOriginalView("binocular");
    List<Unit> units=art.groups.get(8000);
    Unit left=marked(units,1),right=marked(units,3),gauge=marked(units,0x35);
    if(left==null||right==null||gauge==null)throw new JSONException("Original binocular layout incomplete");
    art.binocularLeft=left.x;art.binocularRight=right.x;art.binocularGaugeY=gauge.y;art.binocularGaugeHeight=gauge.height;
    JSONObject font=new JSONObject(NativeBridge.startupLayout(3000));art.bearings=intRows(font.getJSONArray("bearings"),2);
    JSONArray sheets=font.getJSONArray("sheets");art.font=new Bitmap[sheets.length()];
    for(int n=0;n<sheets.length();++n){int[]p=NativeBridge.startupTexture(sheets.getInt(n),0);if(p!=null&&p.length>2)art.font[n]=Bitmap.createBitmap(p,2,p[0],p[0],p[1],Bitmap.Config.ARGB_8888);}
    art.messages.put(0x81,intRows(new JSONObject(NativeBridge.startupLayout(4000+0x81)).getJSONArray("glyphs"),3));
    return art;
  }
  static StartupArt loadOriginalView(String pack) throws JSONException {
    StartupArt art=new StartupArt();HashMap<Integer,Integer> frames=new HashMap<>();
    art.binocularArt=pack.equals("binocular");
    JSONArray list=new JSONObject(NativeBridge.subscreenLayout(pack)).getJSONArray("units");
    List<Unit> units=new ArrayList<>();int[][] hierarchy=new int[list.length()][3];
    for(int n=0;n<list.length();++n){Unit u=new Unit(list.getJSONObject(n));units.add(u);hierarchy[n]=new int[]{u.number,u.parent,u.kind};
      if(u.texture!=255)frames.put(u.texture,Math.max(1,u.textureFrames));if(u.mask!=255&&!frames.containsKey(u.mask))frames.put(u.mask,1);}
    art.groups.put(8000,units);art.orders.put(8000,StartupMath.drawOrder(hierarchy));
    for(int id:frames.keySet()) {
      Bitmap[] images=new Bitmap[frames.get(id)];
      for(int frame=0;frame<images.length;++frame){int[] p=NativeBridge.subscreenTexture(pack,id,frame);
        if(p==null||p.length<3||p[0]<1||p[1]<1||(long)p[0]*p[1]!=p.length-2)throw new JSONException("Original view texture missing: "+pack+" / "+id);
        images[frame]=Bitmap.createBitmap(p,2,p[0],p[0],p[1],Bitmap.Config.ARGB_8888);}
      art.textures.put(id,images[0]);if(images.length>1)art.animated.put(id,images);
    }
    return art;
  }
  void scope(Canvas canvas,Paint paint,int[] state){
    if(state[0]==0xab){ // IdScope::move, the Mine Thrower's zoom rings.
      List<Unit> units=groups.get(8000);float zoom=state[3]/1000000f;
      Unit a=marked(units,1),b=marked(units,2);
      if(a!=null){a.curves[3]=new float[0][];a.rotAxis=2;a.rotationX=a.rotationY=0;a.rotation=270*(zoom+.08f*(float)Math.sin(state[1]*2*Math.PI/45))-90;}
      if(b!=null){b.curves[3]=new float[0][];b.rotAxis=2;b.rotationX=b.rotationY=0;b.rotation=-180*(zoom+.05f*(float)Math.cos(state[1]*2*Math.PI/60))+90;}
    }
    draw(canvas,paint,8000,state[1],0,1);
  }
  void binocular(Canvas c,Paint paint,int[] state) {
    if(state==null||state.length<4||state[0]==0)return;
    List<Unit> units=groups.get(8000);if(units==null)return;
    float fov=state[1]/1000f,angle=(4.712389f-state[2]/1000000f)/(float)Math.PI,lo=angle-.5f,hi=angle+.5f;
    Unit compass=marked(units,0);if(compass!=null){compass.uvLeft=angle;compass.uvRight=angle+1;}
    int count=0;
    for(int k=0;k<=4;++k){float heading=k*.5f;if(lo<=heading&&hi>=heading){Unit u=marked(units,++count);if(u!=null){u.selectedFrame=k==0||k==4?3:k-1;u.x=(heading-lo)*(binocularRight-binocularLeft)+binocularLeft;visible(units,count,true);}}}
    for(int k=count+1;k<=3;++k)visible(units,k,false);
    int distance=(int)(15*(fov-45)/(3-45)+1),digits=distance*10;
    for(int k=0;k<4;++k){Unit u=marked(units,0x20+k);if(u!=null)u.selectedFrame=digits%10;digits/=10;}
    float ratio=1-(distance-1)/15f;Unit gauge=marked(units,0x35);
    if(gauge!=null){gauge.y=binocularGaugeY-binocularGaugeHeight*ratio*.5f;gauge.height=binocularGaugeHeight*(1-ratio);gauge.uvTop=ratio;}
    if(state[3]!=binocularPreviousTick) {
      if(state[3]<binocularPreviousTick)binocularPreviousFov=45;
      visible(units,0x1c,fov<binocularPreviousFov);visible(units,0x1d,fov>binocularPreviousFov);
      binocularPreviousFov=fov;binocularPreviousTick=state[3];
    }
    // HoldBinocular constructs IdBinocular with timer 0; its cutin(150)
    // helper is not called by r101 Event00.
    draw(c,paint,8000,state[3],0,1);
    Unit exit=marked(units,0x36);if(exit!=null)glyphs(c,paint,messages.get(0x81),(exit.x+320)*.8f,(240-exit.y)*.8f-10,16,20,0,20,false);
  }
  void death(Canvas c,Paint paint,ContinueFlow flow) {
    continueFlow=flow;animationFrame=flow.deadFrame;visible(groups.get(7005),0,false);
    draw(c,paint,7005,flow.deadFrame,0,1);
    if(flow.choicesVisible()) {
      visible(groups.get(7006),0,flow.yes);visible(groups.get(7006),1,!flow.yes);
      draw(c,paint,7006,flow.choiceFrame,0,1);
    }
    continueFlow=null;
  }
  private float deathFrame(int group,Unit unit,Map<Integer,Unit> byNumber,float frame) {
    if(group!=7006||continueFlow==null)return frame;
    Unit u=unit;
    for(int depth=0;u!=null&&depth<32;++depth) {
      if(u.mark==0x30||u.mark==0x40)
        return continueFlow.leaving() && (u.mark==0x40)==continueFlow.yes?continueFlow.exitFrame:0;
      u=byNumber.get(u.parent);
    }
    return unit.mark==0||unit.mark==1?continueFlow.cursorFrame:frame;
  }
  private float cardFrame(int group,Unit unit,float frame) {
    // LifeMeter::disp(1) freezes the root at time0; its8-frame path is
    // the slide OUT, while the children retain their own pulse timers.
    if(group==6031 && unit.mark==0)return 0;
    return cardFlow==null?frame:cardFlow.animationFrame(group,unit.mark,frame);
  }
  boolean cardWritingCue(int frame) {
    Unit paper=marked(groups.get(2005),1);if(paper==null)return false;
    for(int i=4;i<paper.curves[0].length;++i)if((int)paper.curves[0][i][0]==frame)return true;
    return false;
  }
  private float[] cardTransform(int group,Unit unit,float frame) {
    if(cardFlow!=null&&group==2007&&unit.mark==0&&cardFlow.animating()) {
      Unit source=marked(groups.get(2007),cardFlow.entryFrame<cardFlow.entryDuration?10:cardFlow.scrollDirection>0?8:9);
      if(source!=null)return source.transform(cardFlow.entryFrame<cardFlow.entryDuration?cardFlow.entryFrame:cardFlow.scrollFrame);
    }
    return unit.transform(frame);
  }
  void draw(Canvas c,Paint paint,int group,float frame,int cursor,float fade) {
    draw(c,paint,group,frame,cursor,fade,null);
  }
  void menu(Canvas c,Paint paint,float entryFrame,float cursorFrame,int cursor){draw(c,paint,8,entryFrame,cursor,1,null,cursorFrame);}
  void draw(Canvas c,Paint paint,int group,float frame,int cursor,float fade,StartupOptions options){draw(c,paint,group,frame,cursor,fade,options,frame);}
  private void draw(Canvas c,Paint paint,int group,float frame,int cursor,float fade,StartupOptions options,float cursorFrame) {
    List<Unit>units=groups.get(group);if(units==null)return;
    HashMap<Integer,Unit>byNumber=new HashMap<>();for(Unit u:units)byNumber.put(u.number,u);
    for(int index:orders.get(group)){
      Unit u=units.get(index);if(u.kind==1||u.texture==255||(u.flags&8)==0||group==2006&&!saveVisible(u.mark))continue;
      if(group==8&&!StartupMath.menuVisible(u.mark,cursor))continue;
      if(cardFlow!=null&&!cardFlow.unitVisible(group,u.mark))continue;
      if(group==2007&&cardFlow!=null&&cardFlow.scrollDirection<0&&u.mark==1)continue;
      if(options!=null&&!optionVisible(u,options))continue;
      float unitFrame=group==8&&(u.mark&1)==1&&u.mark!=255?cursorFrame:frame;
      unitFrame=deathFrame(group,u,byNumber,unitFrame);
      unitFrame=cardFrame(group,u,unitFrame);
      float[]rgba=u.tint(unitFrame);if(options!=null)rgba=optionTint(u,options,units,frame,rgba);
      float[] transform=cardTransform(group,u,unitFrame);if(options!=null&&group==1013&&u.mark==1){Unit a=marked(units,9),b=marked(units,16);if(a!=null&&b!=null){float rate=(options.brightness-34)/80f;transform=StartupMath.idTransform(a.x+(b.x-a.x)*rate,a.y+(b.y-a.y)*rate,0,0,0);}}
      int parent=u.parent,depth=0;boolean hidden=false;
      while(parent!=255 && depth++<32){Unit p=byNumber.get(parent);if(p==null)break;if((p.flags&8)==0||(group==2006&&!saveVisible(p.mark))||(cardFlow!=null&&!cardFlow.unitVisible(group,p.mark))){hidden=true;break;}float parentFrame=cardFrame(group,p,deathFrame(group,p,byNumber,frame));float[]pc=p.tint(parentFrame);for(int k=0;k<4;++k)rgba[k]=((int)(rgba[k]*Math.max(0,Math.min(255,pc[k]))/255));if(p.kind==1&&!(group==2006&&p.mark==21))transform=StartupMath.concat(cardTransform(group,p,parentFrame),transform);parent=p.parent;}if(hidden)continue;
      Matrix matrix=new Matrix();matrix.setValues(StartupMath.screenMatrix(transform));
      int alpha=Math.round(Math.max(0,Math.min(255,rgba[3]))*fade);if(alpha<=0)continue;
      Bitmap bitmap=options!=null&&group==1013&&(u.mark==2||u.mark==3)?textures.get(10000+(u.mark==2?Math.abs(options.brightness-64)/10:Math.abs(options.brightness-64)%10)):textures.get(u.texture);if(bitmap==null)continue;
      Bitmap[]animation=animated.get(u.texture);if(animation!=null&&(u.textureFlags&2)==0&&!(group==1013&&(u.mark==2||u.mark==3))&&group!=2006&&!(group==2008&&u.mark==2))bitmap=animation[Math.floorMod(animationFrame,Math.max(1,Math.min(u.textureAnimationFrames,animation.length)))];
      if(group==8000&&animation!=null&&u.selectedFrame>=0)bitmap=animation[Math.min(u.selectedFrame,animation.length-1)];
      if(cardFlow!=null&&(group==2005||group==2008)&&u.mark==2&&animation!=null)
        bitmap=animation[cardFlow.scrollDirection<0&&cardFlow.scrollFrame<5?cardFlow.scrollFrame:0];
      if(group==6031 && animation!=null && (u.mark==0xa || u.mark==0xb || u.mark==0x17)) {
        int place=u.mark==0xb?1:u.mark==0xa?10:100;bitmap=animation[cockpitAmmo/place%10];
      }
      if(group==2006&&(u.mark==1||u.mark==2))bitmap=textures.get(10020+(u.mark==1?(cursor+1)/10:(cursor+1)%10));if(bitmap==null)continue;
      if(group==2006&&slotHeader!=null&&u.mark>=3&&u.mark<=20&&u.mark!=4&&u.mark!=6&&u.mark!=7){int digit=saveDigit(u.mark);Bitmap[] digits=animated.get(u.texture);if(digits!=null&&digit<digits.length)bitmap=digits[digit];}
      float scale=u.sample(1,unitFrame,1),w=u.width,h=u.height;
      if((u.scaleType&0x10)!=0)w*=scale;else if((u.scaleType&0x20)!=0)h*=scale;else{w*=scale;h*=scale;}
      float flipX=Math.copySign(1,w),flipY=Math.copySign(1,h);w=Math.abs(w);h=Math.abs(h);
      float left=-w*.5f,top=-h*.5f;if(u.anchor==1){left=0;top=0;}else if(u.anchor==2){left=-w;top=0;}else if(u.anchor==3){left=-w;top=-h;}else if(u.anchor==4){left=0;top=-h;}
      int save=c.save();c.translate(320,240);c.concat(matrix);c.scale(flipX,flipY);paint.setAlpha(alpha);paint.setFilterBitmap(true);
      paint.setColorFilter(new ColorMatrixColorFilter(new float[]{rgba[0]/255,0,0,0,0,0,rgba[1]/255,0,0,0,0,0,rgba[2]/255,0,0,0,0,0,1,0}));
      paint.setXfermode(u.blend==1||u.blend==2?new PorterDuffXfermode(PorterDuff.Mode.ADD):u.blend==4?new PorterDuffXfermode(PorterDuff.Mode.MULTIPLY):null);
      RectF bounds=new RectF(left,top,left+w,top+h);Bitmap mask=textures.get(u.mask);
      int type=u.transType>=1&&u.transType<=3?u.transType:0;
      if(binocularArt&&group==8000&&u.mark==0){int clip=c.save();c.clipRect(bounds);float start=(float)(u.uvLeft-Math.floor(u.uvLeft));float span=bounds.width();for(int tile=-1;tile<=1;++tile)c.drawBitmap(bitmap,null,new RectF(bounds.left+(tile-start)*span,bounds.top,bounds.right+(tile-start)*span,bounds.bottom),paint);c.restoreToCount(clip);}
      else if(binocularArt&&group==8000&&u.mark==0x35)c.drawBitmap(bitmap,new Rect(0,Math.round(bitmap.getHeight()*u.uvTop),bitmap.getWidth(),bitmap.getHeight()),bounds,paint);
      else if(type!=0||u.blend>=2)compositor.draw(c,bitmap,mask,bounds,rgba,alpha,type,u.transSub,u.blend);
      else{int layer=-1;if(mask!=null)layer=c.saveLayer(bounds,null);c.drawBitmap(bitmap,null,bounds,paint);
        if(mask!=null){paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.DST_IN));paint.setColorFilter(null);paint.setAlpha(255);c.drawBitmap(mask,null,bounds,paint);c.restoreToCount(layer);}}
      paint.setXfermode(null);paint.setColorFilter(null);paint.setAlpha(255);c.restoreToCount(save);
    }
  }
  private static Unit marked(List<Unit>units,int mark){for(Unit u:units)if(u.mark==mark)return u;return null;}
  private int cockpitAmmo;
  private static void visible(List<Unit> units,int mark,boolean show){if(units==null)return;Unit u=marked(units,mark);if(u!=null)u.flags=show?u.flags|8:u.flags&~8;}
  void cockpit(Canvas c,Paint paint,int[] state) {
    cockpit(c,paint,state,true);
  }
  void cockpit(Canvas c,Paint paint,int[] state,boolean showAction) {
    if(state==null || state.length<9)return;
    List<Unit> life=groups.get(6031),icons=groups.get(6038),action=groups.get(6032);
    if(life==null || icons==null || action==null)return;
    Unit template=marked(life,new int[]{0x11,0x10,0xf}[CockpitState.lifeLevel(state[0])]);
    if(template==null)return;
    boolean ashley=CockpitState.ashleyPresent(state);
    int subLevel=state.length>=14?CockpitState.partnerLevel(state[10],state[11]):0;
    Unit subTemplate=marked(life,new int[]{0x11,0x10,0xf}[subLevel]);
    if(subTemplate==null)return;
    cockpit.update(state,template.color,template.color1,subTemplate.color,subTemplate.color1);animationFrame=state[7];
    visible(life,1,ashley);visible(life,3,ashley);
    int playerType=state.length>=14?state[13]:0;
    for(int mark=0x40;mark<=0x45;++mark)visible(life,mark,mark==0x40+playerType && playerType<=2);
    Unit maximum=marked(life,0xfe);if(maximum!=null)maximum.rotation=CockpitState.maximumRotation(state[1]);
    for(int mark:new int[]{7,8,9}){Unit u=marked(life,mark);if(u!=null){visible(life,mark,cockpit.segmentVisible(mark));u.rotation=cockpit.rotation(mark);}}
    Unit color=marked(life,0x12);
    if(color!=null){for(int k=0;k<3;++k){color.color[k]=(int)cockpit.color0[k];color.color1[k]=(int)cockpit.color1[k];}
      color.color[3]=color.color1[3]=255;color.curves[2]=template.curves[2];color.loop|=4;}
    visible(life,0x13,cockpit.level==1);visible(life,0x14,cockpit.level==2);
    Unit subMax=marked(life,2);if(subMax!=null)subMax.rotation=CockpitState.partnerMaximumRotation(state.length>=14?state[11]:600);
    for(int mark:new int[]{4,5}){Unit u=marked(life,mark);if(u!=null){visible(life,mark,ashley && cockpit.partnerSegmentVisible(mark));u.rotation=cockpit.partnerRotation(mark);}}
    Unit subColor=marked(life,3);
    if(subColor!=null){for(int k=0;k<3;++k){subColor.color[k]=(int)cockpit.partnerColor0[k];subColor.color1[k]=(int)cockpit.partnerColor1[k];}
      subColor.color[3]=subColor.color1[3]=255;subColor.curves[2]=subTemplate.curves[2];subColor.loop|=4;}
    visible(life,0x18,ashley && subLevel==1);visible(life,0x19,ashley && subLevel==2);
    for(int mark=0x1a;mark<=0x1e;++mark)visible(life,mark,ashley && CockpitState.partnerIconVisible(mark,state[12]));
    cockpitAmmo=Math.max(0,Math.min(999,state[2]));boolean empty=cockpitAmmo==0 && state[3]==0;
    visible(life,0xa,!empty);visible(life,0xb,!empty);visible(life,0x17,!empty && cockpitAmmo>=100);
    visible(life,0x3f,empty);
    draw(c,paint,6031,animationFrame,0,1);
    int bullet=state[4]==0x2c?0x37:0x31;
    for(Unit u:icons)if(u.mark!=255)u.flags=u.mark==bullet?u.flags|8:u.flags&~8;
    Unit root=marked(life,0),parent=marked(life,0x30);int saved=c.save();
    if(root!=null)c.concat(root.matrix(0));if(parent!=null)c.concat(parent.matrix(0));
    draw(c,paint,6038,animationFrame,0,1);c.restoreToCount(saved);
    if(showAction && state[5]>=0) {
      int button=state[6]==4?4:state[6]==0x10?0xe:0;
      for(Unit u:action)if(u.parent==255 && u.mark!=255)u.flags=u.mark==1 || u.mark==0xf0 || u.mark==button?u.flags|8:u.flags&~8;
      draw(c,paint,6032,animationFrame,0,1);
      Unit anchor=marked(action,state[6]==1 || state[6]==0x10?1:0xf0);
      if(anchor!=null)glyphs(c,paint,messages.get(state[5]+0x16),(anchor.x+320)*.8f,
          (240-anchor.y)*.8f-16,23,32,0,25,false);
    }
  }
  private static boolean optionVisible(Unit u,StartupOptions o){int m=u.mark;if(m==8||m==14)return false;
    if(o.page==StartupOptions.Page.CONTROLLER){if(m==10||m==11)return m==(o.reverse?10:11);if(m==12||m==13)return m==(o.vibration?12:13);if(m==20||m==21)return m==(o.knife?21:20);}
    if(o.page==StartupOptions.Page.BRIGHTNESS){if(m==4)return o.brightness<64;if(m==5)return o.brightness>64;if(m==9||m==16)return false;}
    if(o.page==StartupOptions.Page.AUDIO&&m>=10&&m<=12)return m==(o.soundMode==1?10:o.soundMode==0?11:12);return true;}
  private static float[]optionTint(Unit u,StartupOptions o,List<Unit>units,float frame,float[]defaultTint){int m=u.mark;Unit base=marked(units,8),off=marked(units,14);float[]highlight=base==null?new float[]{255,255,0,255}:base.tint(frame);float[]white={255,255,255,255},grey=off==null?new float[]{125,125,125,255}:off.color.clone();
    if(o.page==StartupOptions.Page.TOP&&m>=2&&m<=6)return m==2?new float[]{64,64,64,255}:m==o.top+2?highlight:white;
    if(o.page==StartupOptions.Page.CONTROLLER){int[]labels={2,5,17,9};for(int i=0;i<labels.length;++i)if(m==labels[i])return i==o.row?highlight:white;
      int[]left={3,6,18},right={4,7,19};boolean[]pending={o.pendingReverse,o.pendingVibration,!o.pendingKnife};for(int i=0;i<3;++i){if(m==left[i]||m==right[i])return m==(pending[i]?left[i]:right[i])?(i==o.row?highlight:white):grey;}}
    if(o.page==StartupOptions.Page.BRIGHTNESS){if(m>=2&&m<=5)return o.row==0?highlight:white;if(m==6)return o.row==1?highlight:white;}
    if(o.page==StartupOptions.Page.AUDIO&&m>=2&&m<=5)return m==o.row+2?highlight:m==5||m==(o.soundMode==1?2:o.soundMode==0?3:4)?white:grey;return defaultTint;}
  void help(Canvas c,Paint paint,int message){glyphs(c,paint,messages.get(message),0,245,22,32,-1,25,true);}
  void load(Canvas c,Paint paint,int selected,boolean exitPrompt,int choice){card(c,paint,selected,null,false,exitPrompt?17:0,choice);}
  int cardDuration(int mark) {
    List<Unit> frame=groups.get(2007);Unit unit=frame==null?null:marked(frame,mark);
    if(unit==null)return 0;float last=0;
    for(float[][] curve:unit.curves)if(curve.length>0)last=Math.max(last,curve[curve.length-1][0]);
    return Math.max(0,(int)Math.ceil(last));
  }
  void card(Canvas c,Paint paint,int selected,OriginalSaveHeader[]slots,boolean saving,int message,int choice){
    CardFlow flow=new CardFlow(saving);flow.open(slots==null?new OriginalSaveHeader[20]:slots,selected,0,0);flow.message=message;flow.choice=choice;
    if(message!=0&&choice>=0)flow.phase=CardFlow.Phase.CONFIRM;
    card(c,paint,flow,slots);
  }
  void card(Canvas c,Paint paint,CardFlow flow,OriginalSaveHeader[]slots) {
    cardFlow=flow;
    // EFB-reading blends must see the same framebuffer as the ordinary IDs.
    // A Canvas saveLayer would make the compositor read a different target.
    int fade=c.save();
    draw(c,paint,2007,90,0,1);
    Matrix movement=new Matrix();List<Unit> frame=groups.get(2007);
    if(frame!=null&&flow.animating()) {
      Unit base=marked(frame,0),source=marked(frame,flow.entryFrame<flow.entryDuration?10:flow.scrollDirection>0?8:9);
      if(base!=null&&source!=null) {
        Matrix inverse=new Matrix();base.matrix(90).invert(inverse);
        movement.setConcat(source.matrix(flow.entryFrame<flow.entryDuration?flow.entryFrame:flow.scrollFrame),inverse);
      }
    }
    float[]ys={132,96,60,24,-13,-49,-86};
    int rows=c.save();c.translate(320,240);c.concat(movement);c.translate(-320,-240);
    for(int row=0;row<7;++row){int save=c.save();c.translate(85,-ys[row]);int slot=Math.floorMod(flow.cursor-3+row,20);
      if(flow.scrollDirection<0 && row==3){c.restoreToCount(save);continue;}
      slotHeader=slots==null?null:slots[slot];draw(c,paint,2006,flow.presentationFrame,slot,1);c.restoreToCount(save);}
    c.restoreToCount(rows);slotHeader=null;
    // card.cpp queues the list in OT0xC and these plates in OT0xF.
    // The near plates cover the spare rows, rather than being overpainted
    // by their numbers and chapter labels after they have been drawn.
    draw(c,paint,flow.saving?2005:2008,flow.busy()?flow.operationFrame:90,0,1);
    draw(c,paint,2009,90,0,1);
    if(flow.message!=0)cardMessage(c,paint,flow.message,flow.choices()?flow.choice:-1);
    c.restoreToCount(fade);cardFlow=null;
    if(flow.phase==CardFlow.Phase.LEAVE||flow.phase==CardFlow.Phase.CLOSED) {
      paint.setColorFilter(null);paint.setXfermode(null);paint.setStyle(Paint.Style.FILL);
      paint.setColor(Color.BLACK);paint.setAlpha(Math.min(255,flow.exitFrame*255/10));
      c.drawRect(0,0,640,480,paint);paint.setAlpha(255);
    }
  }
  private static final int[][] CARD_MESSAGE_POS={
    {100,110},{100,210},{100,90},{100,110},{100,110},{100,110},{100,110},{100,110},
    {100,130},{100,110},{100,150},{100,210},{100,210},{100,210},{100,210},{200,110},
    {100,210},{200,110},{100,210},{100,210},{100,210},{100,140},{100,110},{100,70},
    {100,70},{100,70},{100,70},{100,70},{100,110},{100,90},{100,130},{100,150},
    {100,110},{100,90},{100,90},{100,90},{100,90},{100,90},{100,210},{100,110},
    {100,150},{100,150},{100,110},{100,130},{100,110}
  };
  float cardChoiceY(int message,int option) {
    RectF[] bounds=cardChoiceBounds(message);
    if(option>=0&&option<bounds.length)return bounds[option].centerY();
    return option==0?280:320;
  }
  private CardMessageLayout cardLayout(int message) {
    int[][] glyphs=messages.get(5000+message);
    return glyphs==null||bearings==null?null:new CardMessageLayout(glyphs,bearings,
        CARD_MESSAGE_POS[message][0],CARD_MESSAGE_POS[message][1]);
  }
  RectF[] cardChoiceBounds(int message) {
    CardMessageLayout layout=cardLayout(message);if(layout==null)return new RectF[0];
    RectF[] bounds=new RectF[layout.choices.length];
    for(int i=0;i<bounds.length;++i){float[]b=layout.choices[i];bounds[i]=new RectF(b[0]*1.25f,b[1]*1.25f,b[2]*1.25f,b[3]*1.25f);}
    return bounds;
  }
  boolean cardHorizontalChoices(int message) {
    CardMessageLayout layout=cardLayout(message);return layout!=null&&layout.horizontalChoices();
  }
  void cardMessage(Canvas c,Paint paint,int message,int choice) {
    int[][] original=messages.get(5000+message);if(original==null||message>=CARD_MESSAGE_POS.length)return;
    int[][] cursor=new int[original.length][];int n=0;
    for(int i=0;i<original.length;++i){int[]g=original[i];cursor[i]=g[2]==99?new int[]{n++==choice?1:0,g[1],99}:g;}
    CardMessageLayout layout=cardLayout(message);
    glyphs(c,paint,cursor,layout.x,layout.y,21,25,0,20,false);
  }
  private boolean saveVisible(int mark){if(mark==22)return slotHeader!=null;if(slotHeader==null)return true;if(slotHeader.corrupt){if(mark==32)return true;if(mark>=3&&mark<=20||mark==23||mark==24||mark==25||mark==33||mark==34)return false;return true;}if(mark==32||mark==34)return false;if(mark==7)return slotHeader.mode==1&&slotHeader.chapter!=18;if(mark==6)return slotHeader.mode==2;if(mark==33)return slotHeader.mode==3;if(mark==25)return slotHeader.mode==1&&slotHeader.chapter==18;if(mark==23)return slotHeader.gameMode!=6;if(mark==24)return slotHeader.gameMode==6;if(mark>=3&&mark<=5)return slotHeader.mode!=3&&!(slotHeader.mode==1&&slotHeader.chapter==18);return true;}
  private int saveDigit(int mark){int chapter=slotHeader.chapter-(slotHeader.mode==2?1:0);int[]chapters={1,1,1,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,6};int[]sections={1,2,3,1,2,3,1,2,3,4,1,2,3,4,1,2,3,4,1};if(mark==3)return chapter>=0&&chapter<chapters.length?chapters[chapter]:1;if(mark==5)return chapter>=0&&chapter<sections.length?sections[chapter]:1;int value=0,digit=0;if(mark>=8&&mark<=10){value=slotHeader.saveCount;digit=10-mark;}else if(mark>=11&&mark<=12){value=slotHeader.seconds/3600;digit=12-mark;}else if(mark==13||mark==16)return 11;else if(mark>=14&&mark<=15){value=slotHeader.seconds/60%60;digit=15-mark;}else if(mark>=17&&mark<=18){value=slotHeader.seconds%60;digit=18-mark;}else if(mark>=19&&mark<=20){value=slotHeader.round+(slotHeader.mode==3?0:1);digit=20-mark;}return value/(digit==2?100:digit==1?10:1)%10;}
  private void glyphs(Canvas c,Paint paint,int[][]glyphs,float x,float y,float fw,float fh,int gap,int lineGap,boolean center){if(glyphs==null||font==null)return;
    float ratio=fw/32;int maxLine=0;for(int[]g:glyphs)maxLine=Math.max(maxLine,g[1]);int[]width=new int[maxLine+1];for(int[]g:glyphs){int id=g[0];if(id<bearings.length)width[g[1]]+=id==0&&g[2]!=99?(int)(8*ratio):(int)((bearings[g[2]==99?1:id][1]-bearings[g[2]==99?1:id][0])*ratio)+gap;}int max=0;for(int w:width)max=Math.max(max,w);if(center)x=(512-max)/2;
    paint.setColorFilter(null);paint.setXfermode(null);paint.setAlpha(240);paint.setFilterBitmap(true);float[]positions=new float[maxLine+1];Arrays.fill(positions,x);
    for(int[]g:glyphs){int id=g[0],line=g[1];if(id>=bearings.length)continue;if(id==0&&g[2]!=99){positions[line]+=(int)(8*ratio);continue;}int cell=id;Bitmap sheet=null;for(Bitmap candidate:font){if(candidate==null)continue;int count=candidate.getWidth()/32*(candidate.getHeight()/32);if(cell<count){sheet=candidate;break;}cell-=count;}if(sheet==null)continue;
      int columns=sheet.getWidth()/32,sx=cell%columns*32,sy=cell/columns*32;float left=positions[line]*1.25f,top=(y+line*lineGap)*1.25f;int bearing=g[2]==99?1:id;int widthGlyph=(int)((bearings[bearing][1]-bearings[bearing][0])*ratio);
      paint.setColorFilter(new ColorMatrixColorFilter(new float[]{229f/255,0,0,0,0,0,217f/255,0,0,0,0,0,207f/255,0,0,0,0,0,1,0}));c.drawBitmap(sheet,new Rect(sx+bearings[bearing][0],sy,sx+bearings[bearing][1],sy+32),new RectF(left,top,left+widthGlyph*1.25f,top+fh*1.25f),paint);positions[line]+=widthGlyph+gap;
    }paint.setColorFilter(null);paint.setAlpha(255);
  }
}
