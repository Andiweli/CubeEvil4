package com.ast.re4decomp;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.*;
import android.os.*;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;

/** Original authored title artwork/flow plus playback of the user's disc movies. */
public final class StartupActivity extends Activity {
  private boolean openingCanceled;
  private final ExecutorService worker=Executors.newSingleThreadExecutor();
  private final Handler main=new Handler(Looper.getMainLooper());
  private final StartupFlow flow=new StartupFlow();
  private final HashMap<String,File> movies=new HashMap<>();
  private StartupArt art;
  private FrameLayout root;
  private TitleView screen;
  private StartupMovieView video;
  private boolean resumed,destroyed,preparing,moviePreparing;
  private long lastTime;
  private double remainder;
  private String error="",currentMovie="";
  private int moviePosition;
  private final CardFlow loadFlow=new CardFlow(false);
  private boolean restoreLoadFlow;
  private int loadStickDirection,loadStickTicks;
  private OriginalSaveHeader[] saveHeaders=new OriginalSaveHeader[20];
  private GameSaveStore saveStore;
  private byte[] selectedSave;
  private final StartupOptions options=new StartupOptions();
  private boolean titleCue;
  private SharedPreferences settings;
  private final Runnable tick=new Runnable(){@Override public void run(){if(!resumed||destroyed)return;
    long now=System.nanoTime();double elapsed=lastTime==0?0:Math.min(.25,(now-lastTime)/1e9);lastTime=now;
    boolean advanced=false;
    if(art!=null && currentMovie.isEmpty() && !preparing){remainder+=elapsed*30;while(remainder>=1){advanced=true;remainder-=1;flow.tick();if(flow.phase==StartupFlow.Phase.LOAD){loadFlow.tick();if(loadStickDirection!=0&&++loadStickTicks>=10){move(loadStickDirection);loadStickTicks=5;}}}}
    if(art!=null&&flow.logoSound&&!titleCue){titleCue=true;NativeBridge.startupCue(6,4);}
    if(art!=null&&flow.phase==StartupFlow.Phase.NEW_GAME_FADE)
      flow.startGameVoiceEnded(NativeBridge.startupVoiceEnded()&&GameAudio.startupVoiceDrained());
    if(flow.phase==StartupFlow.Phase.LOAD&&loadFlow.phase==CardFlow.Phase.CLOSED){if(selectedSave!=null)flow.phase=StartupFlow.Phase.LAUNCH;else flow.back();}
    if(art!=null&&flow.phase==StartupFlow.Phase.OPENING)playMovie("opening");
    else if(art!=null&&flow.phase==StartupFlow.Phase.DEMO)playMovie(flow.demoNumber==1?"demo0":"demo1");
    else if(art!=null&&flow.phase==StartupFlow.Phase.LAUNCH&&!preparing&&(selectedSave==null||GameAudio.startupVoiceDrained()))launchCampaign();
    if(advanced)screen.invalidate();main.postDelayed(this,16);
  }};
  @Override public void onCreate(Bundle state){super.onCreate(state);settings=getSharedPreferences("game-options",MODE_PRIVATE);readOptions();
    saveStore=new GameSaveStore(new File(getFilesDir(),"saves"));
    if(state!=null){flow.phase=StartupFlow.Phase.valueOf(state.getString("phase","NINTENDO"));flow.frame=state.getInt("frame");flow.cursor=state.getInt("cursor",0);flow.idle=state.getInt("idle");flow.demoNumber=state.getInt("demo");flow.menuFrame=state.getInt("menuFrame");flow.menuFadeFrame=state.getInt("menuFadeFrame",flow.menuFrame);flow.cursorFrame=state.getInt("cursorFrame");flow.titleTicks=state.getInt("titleTicks");flow.logoSound=state.getBoolean("logoSound");flow.menuFadeIn=state.getBoolean("menuFadeIn");titleCue=state.getBoolean("titleCue",flow.logoSound);openingCanceled=state.getBoolean("openingCanceled");moviePosition=state.getInt("moviePosition");options.page=StartupOptions.Page.valueOf(state.getString("optionPage","TOP"));options.top=state.getInt("optionTop",1);options.row=state.getInt("optionRow");options.pendingReverse=state.getBoolean("pendingReverse",options.reverse);options.pendingVibration=state.getBoolean("pendingVibration",options.vibration);options.pendingKnife=state.getBoolean("pendingKnife",options.knife);restoreLoadFlow=true;loadFlow.cursor=state.getInt("loadCursor");loadFlow.choice=state.getInt("loadChoice",1);
      loadFlow.phase=CardFlow.Phase.valueOf(state.getString("cardPhase","SELECT"));loadFlow.message=state.getInt("cardMessage");loadFlow.exitFrame=state.getInt("cardExitFrame");selectedSave=state.getByteArray("selectedSave");loadFlow.presentationFrame=state.getInt("cardPresentationFrame",30);loadFlow.operationFrame=state.getInt("cardOperationFrame");
      if(loadFlow.busy()){loadFlow.phase=CardFlow.Phase.CONFIRM;loadFlow.message=18;loadFlow.choice=1;}}
    root=new FrameLayout(this);root.setBackgroundColor(Color.BLACK);screen=new TitleView();root.addView(screen,new FrameLayout.LayoutParams(-1,-1));setContentView(root);screen.setFocusableInTouchMode(true);screen.requestFocus();
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);fullscreen();
    if(Build.VERSION.SDK_INT>=33)getOnBackInvokedDispatcher().registerOnBackInvokedCallback(android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT,this::onBackPressed);
    final File disc=((Re4Application)getApplication()).imports.root;
    worker.execute(()->{StartupArt loaded=null;String failure="";HashMap<String,File>prepared=new HashMap<>();try{if(disc==null)throw new IOException("Disc 1 is missing");NativeBridge.startupConfigure(new File(disc,"disc1/files").getAbsolutePath(),1);loaded=StartupArt.load();for(String name:new String[]{"opening","demo0","demo1"})prepared.put(name,copyMovie(name));}catch(Exception e){failure=e.getMessage()==null?e.toString():e.getMessage();}
      final OriginalSaveHeader[] headers=saveStore.headers();final StartupArt complete=loaded;final String message=failure;main.post(()->{if(destroyed)return;saveHeaders=headers;art=complete;if(art!=null&&flow.phase==StartupFlow.Phase.LOAD){if(restoreLoadFlow){loadFlow.refresh(headers);loadFlow.entryDuration=art.cardDuration(10);loadFlow.entryFrame=loadFlow.entryDuration;loadFlow.scrollDuration=Math.max(art.cardDuration(8),art.cardDuration(9));restoreLoadFlow=false;}else openLoad();}error=message;movies.putAll(prepared);if(art!=null&&resumed&&currentMovie.isEmpty()&&!preparing&&flow.phase!=StartupFlow.Phase.OPENING&&flow.phase!=StartupFlow.Phase.DEMO&&(flow.phase!=StartupFlow.Phase.LAUNCH || (selectedSave!=null&&!GameAudio.startupVoiceDrained())))titleAudio(true);screen.invalidate();});});
  }
  private File copyMovie(String name)throws IOException{
    String asset="movies/"+name+".mp4";File dest=new File(getCacheDir(),"startup-m35-dualaudio-"+name+".mp4");
    long size;try(android.content.res.AssetFileDescriptor d=getAssets().openFd(asset)){size=d.getLength();}
    if(dest.isFile()&&dest.length()==size)return dest;
    File pending=new File(getCacheDir(),dest.getName()+".tmp");try(InputStream in=getAssets().open(asset);FileOutputStream out=new FileOutputStream(pending)){byte[]buf=new byte[65536];int n;while((n=in.read(buf))!=-1){if(Thread.currentThread().isInterrupted())throw new IOException("Movie preparation canceled");out.write(buf,0,n);}out.getFD().sync();}
    if(pending.length()!=size||!pending.renameTo(dest))throw new IOException("Could not prepare original movie");return dest;
  }
  private void fullscreen(){if(Build.VERSION.SDK_INT>=30)getWindow().setDecorFitsSystemWindows(false);if(Build.VERSION.SDK_INT>=28){WindowManager.LayoutParams p=getWindow().getAttributes();p.layoutInDisplayCutoutMode=Build.VERSION.SDK_INT>=30?WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS:WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;getWindow().setAttributes(p);}getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE|View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_FULLSCREEN|View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);}
  private void titleAudio(boolean active){NativeBridge.startupAudio(active);GameAudio.setActive(active);}
  private void playMovie(String name){if(!currentMovie.isEmpty()||moviePreparing||!resumed||!error.isEmpty())return;File file=movies.get(name);if(file==null){error="Original movie missing: "+name;return;}moviePreparing=true;currentMovie=name;titleAudio(false);
    // TextureView participates in normal window composition. Its identity transform
    // deliberately stretches the original picture to the whole display.
    final StartupMovieView[] holder=new StartupMovieView[1];
    StartupMovieView current=new StartupMovieView(this,file,moviePosition,options.soundMode,settings.getInt("volume",100)/100f,new StartupMovieView.Listener(){
      @Override public void prepared(){if(destroyed||video!=holder[0])return;moviePreparing=false;}
      @Override public void completed(){if(video==holder[0])movieFinished();}
      @Override public void failed(String message){if(video!=holder[0])return;error=message;stopMovie();if(flow.phase==StartupFlow.Phase.DEMO){flow.movieComplete();if(resumed)titleAudio(true);}screen.invalidate();}
    });
    holder[0]=current;video=current;screen.setVisibility(View.INVISIBLE);root.addView(current,new FrameLayout.LayoutParams(-1,-1));
    current.setOnTouchListener((view,event)->{if(event.getAction()==MotionEvent.ACTION_UP)movieFinished(true);return true;});current.resumeMovie();
  }
  private void stopMovie(){StartupMovieView current=video;video=null;if(current!=null){current.closeMovie();root.removeView(current);}currentMovie="";moviePreparing=false;moviePosition=0;screen.setVisibility(View.VISIBLE);screen.resetPresentation();}
  private void movieFinished(){movieFinished(false);}
  private void movieFinished(boolean skipped){if(skipped&&"opening".equals(currentMovie))openingCanceled=true;stopMovie();flow.movieComplete();lastTime=0;remainder=0;if(resumed&&(flow.phase!=StartupFlow.Phase.LAUNCH || (selectedSave!=null&&!GameAudio.startupVoiceDrained())))titleAudio(true);screen.invalidate();}
  private void launchCampaign(){final String entryRoom=selectedSave!=null?String.format(Locale.ROOT,"r%03x",new OriginalSaveHeader(selectedSave).room):openingCanceled?"r100":"r120";preparing=true;titleAudio(false);final File game=((Re4Application)getApplication()).imports.root;
    worker.execute(()->{File room=null;String failure="";try(Reader rows=new InputStreamReader(getAssets().open("rooms.tsv"),StandardCharsets.UTF_8);Reader blocks=new InputStreamReader(getAssets().open("blocks.tsv"),StandardCharsets.UTF_8)){RoomRepository repository=new RoomRepository(game,getCacheDir(),rows,blocks,(source,output)->NativeBridge.decodeRoom(source.getAbsolutePath(),output.getAbsolutePath()));room=repository.prepare(entryRoom);}catch(Exception e){failure=e.getMessage()==null?e.toString():e.getMessage();}
      final File ready=room;final String message=failure;main.post(()->{if(destroyed)return;if(ready==null||!message.isEmpty()){error=message;preparing=false;flow.phase=StartupFlow.Phase.MENU;if(resumed)titleAudio(true);return;}
        startActivity(new Intent(this,ControllerActivity.class).putExtra("previewScene",ready.getAbsolutePath()).putExtra("sceneRoom",entryRoom).putExtra("openingCanceled",openingCanceled).putExtra("savedGame",selectedSave).putExtra("exploreMode",false).putExtra("testProfile",0).putExtra("campaignMode",true).putExtra("campaignDifficulty",selectedSave==null?0:new OriginalSaveHeader(selectedSave).gameMode==6?2:0));overridePendingTransition(0,0);finish();});});
  }
  private void readOptions(){options.open(settings.getBoolean("invertAimY",false),settings.getBoolean("vibration",true),settings.getBoolean("knifeMode",false),settings.getInt("brightness",64),settings.getInt("soundMode",1));applyAudioOptions();}
  private void applyAudioOptions(){GameAudio.setVolume(settings.getInt("volume",100)/100f);GameAudio.setOutputMode(options.soundMode);NativeBridge.configureDisplayBrightness(options.brightness);}
  private void saveOptions(){settings.edit().putBoolean("invertAimY",options.reverse).putBoolean("vibration",options.vibration).putBoolean("knifeMode",options.knife).putInt("brightness",options.brightness).putInt("soundMode",options.soundMode).apply();applyAudioOptions();}
  private void confirm(){if(art==null||preparing)return;if(!currentMovie.isEmpty()){movieFinished(true);return;}
    if(flow.phase==StartupFlow.Phase.OPTIONS){StartupOptions.Page old=options.page;boolean close=options.confirm();saveOptions();NativeBridge.startupCue(0,close?0x39:old==StartupOptions.Page.TOP?0x36:options.page==StartupOptions.Page.TOP?0x39:0x3a);if(close)flow.back();return;}
    if(flow.phase==StartupFlow.Phase.LOAD){confirmLoad();return;}
    if(flow.phase==StartupFlow.Phase.MENU){int chosen=flow.cursor;error="";flow.confirm();if(chosen==2)readOptions();if(chosen==1)openLoad();selectedSave=null;NativeBridge.startupCue(chosen==0?6:0,chosen==0?2:chosen==2?0x33:4);if(chosen==0)GameAudio.awaitStartupVoice();}else flow.skip();}
  private void openLoad() {
    loadFlow.open(saveHeaders,saveStore.initialSelection(saveHeaders),art.cardDuration(10),Math.max(art.cardDuration(8),art.cardDuration(9)));
    loadStickDirection=loadStickTicks=0;error="";
  }
  private void confirmLoad() {
    if(preparing)return;CardFlow.Phase before=loadFlow.phase;CardFlow.Operation operation=loadFlow.confirm();
    if(loadFlow.phase==CardFlow.Phase.CLOSED){flow.back();NativeBridge.startupCue(0,0x39);screen.invalidate();return;}
    if(operation==CardFlow.Operation.NONE){if(before!=loadFlow.phase)NativeBridge.startupCue(0,loadFlow.selecting()?5:4);screen.invalidate();return;}
    final int slot=loadFlow.cursor;saveStore.rememberSelection(slot);NativeBridge.startupCue(0,operation==CardFlow.Operation.LOAD?0x38:4);
    if(operation==CardFlow.Operation.LOAD)GameAudio.awaitStartupVoice();
    worker.execute(()->{
      byte[] image=null;String failure="";boolean unsupported=false;
      try {
        if(operation==CardFlow.Operation.DELETE)saveStore.delete(slot);
        else {image=saveStore.read(slot);String problem=NativeBridge.savedGameProblem(image);if(problem==null||!problem.isEmpty()){unsupported=true;failure=problem==null?"Save verification failed":problem;image=null;}}
      }catch(IOException exception){failure=exception.getMessage()==null?exception.toString():exception.getMessage();}
      final byte[] data=image;final String problem=failure;final boolean profileError=unsupported;
      final OriginalSaveHeader[] rows=saveStore.headers();
      main.post(()->{if(destroyed)return;saveHeaders=rows;loadFlow.refresh(rows);
        if(profileError)loadFlow.unsupported();else loadFlow.completed(problem.isEmpty());
        if(!problem.isEmpty()){android.util.Log.e("RE4Load",problem);NativeBridge.startupCue(0,0x2a);}
        else if(operation==CardFlow.Operation.LOAD)selectedSave=data;
        else NativeBridge.startupCue(0,4);
        screen.invalidate();
      });
    });screen.invalidate();
  }
  @Override public boolean onGenericMotionEvent(MotionEvent event) {
    if(flow.phase!=StartupFlow.Phase.LOAD||(event.getSource()&InputDevice.SOURCE_CLASS_JOYSTICK)==0)return super.onGenericMotionEvent(event);
    boolean horizontal=loadHorizontalChoices();
    int hat=horizontal?MotionEvent.AXIS_HAT_X:MotionEvent.AXIS_HAT_Y;
    int stick=horizontal?MotionEvent.AXIS_X:MotionEvent.AXIS_Y;
    float axis=event.getAxisValue(hat);if(Math.abs(axis)<.5f)axis=event.getAxisValue(stick);
    int direction=axis<-.5f?-1:axis>.5f?1:0;if(direction!=loadStickDirection){loadStickDirection=direction;loadStickTicks=0;if(direction!=0)move(direction);}return true;
  }
  private boolean loadHorizontalChoices(){return flow.phase==StartupFlow.Phase.LOAD&&loadFlow.choices()&&art!=null&&art.cardHorizontalChoices(loadFlow.message);}
  private void move(int direction){if(flow.phase==StartupFlow.Phase.OPTIONS){int old=options.page==StartupOptions.Page.TOP?options.top:options.row;options.move(direction);if(old!=(options.page==StartupOptions.Page.TOP?options.top:options.row))NativeBridge.startupCue(0,0x34);}
    else if(flow.phase==StartupFlow.Phase.LOAD){if(loadFlow.move(direction))NativeBridge.startupCue(0,loadFlow.choices()?0xe:direction<0?0x2f:0x2c);}
    else{int old=flow.cursor;flow.move(direction);if(old!=flow.cursor)NativeBridge.startupCue(0,10);}screen.invalidate();}
  private void changeOption(int direction){options.adjust(direction);saveOptions();NativeBridge.startupCue(0,options.page==StartupOptions.Page.BRIGHTNESS?0x3b:0x35);screen.invalidate();}
  @Override public void onBackPressed(){if(!currentMovie.isEmpty()){movieFinished(true);return;}if(flow.phase==StartupFlow.Phase.OPTIONS){if(options.back())flow.back();NativeBridge.startupCue(0,0x39);screen.invalidate();return;}
    if(flow.phase==StartupFlow.Phase.LOAD){loadFlow.back();if(loadFlow.phase==CardFlow.Phase.CLOSED)flow.back();NativeBridge.startupCue(0,5);screen.invalidate();return;}finish();}
  @Override public boolean dispatchKeyEvent(KeyEvent e){int key=e.getKeyCode();boolean startup=flow.phase==StartupFlow.Phase.NINTENDO||flow.phase==StartupFlow.Phase.TIMELINE;boolean relevant=key==KeyEvent.KEYCODE_BUTTON_A||key==KeyEvent.KEYCODE_BUTTON_B||key==KeyEvent.KEYCODE_BUTTON_START||(startup&&key==KeyEvent.KEYCODE_BUTTON_X)||key==KeyEvent.KEYCODE_DPAD_CENTER||key==KeyEvent.KEYCODE_ENTER||key==KeyEvent.KEYCODE_DPAD_UP||key==KeyEvent.KEYCODE_DPAD_DOWN||key==KeyEvent.KEYCODE_DPAD_LEFT||key==KeyEvent.KEYCODE_DPAD_RIGHT||key==KeyEvent.KEYCODE_BUTTON_SELECT;
    // Consume the triggering key without confirming LOAD or START GAME in
    // the same event. Android's repeat events cannot restart the entry fade.
    if(startup&&(key==KeyEvent.KEYCODE_BUTTON_START||key==KeyEvent.KEYCODE_BUTTON_A||key==KeyEvent.KEYCODE_BUTTON_X||key==KeyEvent.KEYCODE_DPAD_CENTER||key==KeyEvent.KEYCODE_ENTER)){if(e.getAction()==KeyEvent.ACTION_DOWN&&e.getRepeatCount()==0){flow.skip();screen.invalidate();}return true;}
    if(!relevant)return super.dispatchKeyEvent(e);if(e.getAction()==KeyEvent.ACTION_DOWN){if(key==KeyEvent.KEYCODE_BUTTON_SELECT){finish();return true;}if(key==KeyEvent.KEYCODE_BUTTON_B){if(flow.phase==StartupFlow.Phase.OPTIONS||flow.phase==StartupFlow.Phase.LOAD||!currentMovie.isEmpty())onBackPressed();}else if(key==KeyEvent.KEYCODE_DPAD_UP){if(!loadHorizontalChoices())move(-1);}else if(key==KeyEvent.KEYCODE_DPAD_DOWN){if(!loadHorizontalChoices())move(1);}else if(key==KeyEvent.KEYCODE_DPAD_LEFT){if(flow.phase==StartupFlow.Phase.OPTIONS)changeOption(-1);else if(loadHorizontalChoices())move(-1);}else if(key==KeyEvent.KEYCODE_DPAD_RIGHT){if(flow.phase==StartupFlow.Phase.OPTIONS)changeOption(1);else if(loadHorizontalChoices())move(1);}else if(e.getRepeatCount()==0){if(key==KeyEvent.KEYCODE_BUTTON_START){if(flow.phase==StartupFlow.Phase.LOAD){if(loadFlow.phase==CardFlow.Phase.ERROR||loadFlow.phase==CardFlow.Phase.DONE)confirmLoad();}else if(flow.phase==StartupFlow.Phase.OPTIONS)flow.back();else if(!currentMovie.isEmpty())movieFinished(true);else flow.skip();}else if(flow.phase!=StartupFlow.Phase.NINTENDO&&flow.phase!=StartupFlow.Phase.TIMELINE)confirm();}}return true;}
  @Override protected void onResume(){super.onResume();resumed=true;lastTime=0;fullscreen();if(video!=null)video.resumeMovie();else if(art!=null&&!preparing&&flow.phase!=StartupFlow.Phase.OPENING&&flow.phase!=StartupFlow.Phase.DEMO&&(flow.phase!=StartupFlow.Phase.LAUNCH || (selectedSave!=null&&!GameAudio.startupVoiceDrained())))titleAudio(true);main.post(tick);}
  @Override protected void onPause(){resumed=false;main.removeCallbacks(tick);titleAudio(false);if(video!=null){moviePosition=video.position();video.pauseMovie();}super.onPause();}
  @Override protected void onSaveInstanceState(Bundle state){state.putString("phase",flow.phase.name());state.putInt("frame",flow.frame);state.putInt("cursor",flow.cursor);state.putInt("idle",flow.idle);state.putInt("demo",flow.demoNumber);state.putInt("menuFrame",flow.menuFrame);state.putInt("menuFadeFrame",flow.menuFadeFrame);state.putInt("cursorFrame",flow.cursorFrame);state.putInt("titleTicks",flow.titleTicks);state.putBoolean("logoSound",flow.logoSound);state.putBoolean("menuFadeIn",flow.menuFadeIn);state.putBoolean("titleCue",titleCue);state.putBoolean("openingCanceled",openingCanceled);state.putInt("moviePosition",video==null?moviePosition:video.position());state.putString("optionPage",options.page.name());state.putInt("optionTop",options.top);state.putInt("optionRow",options.row);state.putBoolean("pendingReverse",options.pendingReverse);state.putBoolean("pendingVibration",options.pendingVibration);state.putBoolean("pendingKnife",options.pendingKnife);state.putInt("loadCursor",loadFlow.cursor);state.putInt("loadChoice",loadFlow.choice);state.putString("cardPhase",loadFlow.phase.name());state.putInt("cardMessage",loadFlow.message);state.putInt("cardExitFrame",loadFlow.exitFrame);state.putByteArray("selectedSave",selectedSave);state.putInt("cardPresentationFrame",loadFlow.presentationFrame);state.putInt("cardOperationFrame",loadFlow.operationFrame);super.onSaveInstanceState(state);}
  @Override protected void onDestroy(){destroyed=true;main.removeCallbacksAndMessages(null);stopMovie();worker.shutdownNow();super.onDestroy();}
  private final class TitleView extends View {
    final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG),displayPaint=new Paint(Paint.FILTER_BITMAP_FLAG);
    Bitmap picture=Bitmap.createBitmap(640,480,Bitmap.Config.ARGB_8888);
    Canvas pictureCanvas=new Canvas(picture);
    StartupViewport viewport;long rendered=Long.MIN_VALUE;
    TitleView(){super(StartupActivity.this);
      // Android draws its default focus highlight after onDraw, across this
      // entire fullscreen View. A DPAD key after the movie leaves touch mode
      // and can activate that overlay. The original menu draws its own cursor.
      // Opt out before onCreate makes this View focusable or requests focus.
      if(Build.VERSION.SDK_INT>=26)setDefaultFocusHighlightEnabled(false);
    }
    void resetPresentation(){paint.reset();paint.setAntiAlias(true);displayPaint.reset();displayPaint.setFilterBitmap(true);
      // Sofdec returns to a clean EFB. Give Android a new bitmap/Canvas too,
      // so the old display list and its texture cannot survive the movie.
      picture=Bitmap.createBitmap(640,480,Bitmap.Config.ARGB_8888);pictureCanvas=new Canvas(picture);
      picture.eraseColor(Color.BLACK);rendered=Long.MIN_VALUE;invalidate();}
    @Override protected void onDraw(Canvas destination){super.onDraw(destination);destination.drawColor(Color.BLACK,PorterDuff.Mode.SRC);viewport=new StartupViewport(getWidth(),getHeight(),flow.phase);
      // R120Event holds VI black while the first engine event is prepared.
      // The title picture must not reappear between the SFD and that event.
      if(flow.phase==StartupFlow.Phase.LAUNCH || (preparing && selectedSave!=null))return;
      // Source simulation runs at30Hz. Reuse its picture between display refreshes.
      long signature=Objects.hash(flow.phase,flow.frame,flow.cursor,flow.menuFrame,flow.cursorFrame,flow.titleTicks,flow.fade(),options.page,options.top,options.row,options.reverse,options.vibration,options.knife,options.pendingReverse,options.pendingVibration,options.pendingKnife,options.brightness,options.soundMode,loadFlow.cursor,loadFlow.phase,loadFlow.message,loadFlow.choice,loadFlow.entryFrame,loadFlow.scrollFrame,loadFlow.scrollDirection,loadFlow.operationFrame,loadFlow.exitFrame,preparing,error,art,saveHeaders);
      if(signature!=rendered){rendered=signature;
      // Rebuild a clean source framebuffer; no movie/filter/blend state survives return.
      picture.eraseColor(Color.BLACK);paint.reset();paint.setAntiAlias(true);displayPaint.reset();displayPaint.setFilterBitmap(true);Canvas c=pictureCanvas;
      if(art==null){text(c,error.isEmpty()?"Loading original data …":error,320,240,17);}
      else{
        art.beginFrame(picture,new StartupViewport(640,480,flow.phase));art.animationFrame=flow.titleTicks;
        if(flow.phase==StartupFlow.Phase.LOAD){art.animationFrame=loadFlow.presentationFrame;art.card(c,paint,loadFlow,saveHeaders);}
        else if(flow.phase==StartupFlow.Phase.OPTIONS){art.draw(c,paint,13,0,0,1);art.draw(c,paint,1009,90,0,1);art.draw(c,paint,options.group(),flow.frame,0,1,options);art.help(c,paint,options.message());}
        else if(flow.phase==StartupFlow.Phase.NINTENDO)art.draw(c,paint,11,flow.frame,0,flow.fade());
        else{art.draw(c,paint,6,flow.titleFrame(),0,flow.fade());if(flow.phase==StartupFlow.Phase.MENU)art.menu(c,paint,flow.menuFrame,flow.cursorFrame,flow.cursor);}
        if(flow.menuEntryFade()<1){paint.setColorFilter(null);paint.setXfermode(null);paint.setStyle(Paint.Style.FILL);paint.setColor(Color.BLACK);paint.setAlpha(Math.round(255*(1-flow.menuEntryFade())));c.drawRect(0,0,640,480,paint);paint.setAlpha(255);}
        if(!error.isEmpty())text(c,error,320,400,13);
      }
      }
      displayPaint.reset();displayPaint.setFilterBitmap(true);
      if(options.brightness!=64){float gain=options.brightness/64f;displayPaint.setColorFilter(new ColorMatrixColorFilter(new float[]{gain,0,0,0,0,0,gain,0,0,0,0,0,gain,0,0,0,0,0,1,0}));}
      int save=destination.save();destination.scale(viewport.scaleX,viewport.scaleY);destination.translate(0,-viewport.top);destination.clipRect(0,viewport.top,640,viewport.top+viewport.height);destination.drawBitmap(picture,0,0,displayPaint);destination.restoreToCount(save);
    }
    private void text(Canvas c,String s,float x,float y,int size){paint.setColor(Color.WHITE);paint.setTextSize(size);paint.setTextAlign(Paint.Align.CENTER);c.drawText(s,x,y,paint);}
    @Override public boolean onTouchEvent(MotionEvent e){if(e.getAction()!=MotionEvent.ACTION_UP)return true;if(viewport==null)return true;float x=viewport.sourceX(e.getX()),y=viewport.sourceY(e.getY());
      if(flow.phase==StartupFlow.Phase.MENU&&y>=310&&y<=365){flow.cursor=Math.max(0,Math.min(2,Math.round((y-318)/20)));confirm();}
      else if(flow.phase==StartupFlow.Phase.OPTIONS){if(options.page==StartupOptions.Page.TOP&&y>=145&&y<=295){options.top=Math.max(1,Math.min(4,Math.round((y-160)/30)));confirm();}
        else if(options.page==StartupOptions.Page.CONTROLLER&&y>=145&&y<=295){options.row=y>250?3:y>214?2:y>180?1:0;if(options.row<3)changeOption(x<410?-1:1);confirm();}
        else if(options.page==StartupOptions.Page.BRIGHTNESS){if(y>260){options.row=1;confirm();}else{options.row=0;changeOption(x<320?-1:1);}}
        else if(options.page==StartupOptions.Page.AUDIO&&y>=145&&y<=295){options.row=y>265?3:y>215?2:y>175?1:0;confirm();}}
      else if(flow.phase==StartupFlow.Phase.LOAD){if(loadFlow.choices()){
        android.graphics.RectF[] choices=art.cardChoiceBounds(loadFlow.message);
        for(int n=0;n<choices.length;++n)if(choices[n].contains(x,y)){loadFlow.choice=n;confirm();break;}
      }else if(!loadFlow.selecting())confirm();else if(y>340)onBackPressed();else if(y>=215&&y<=265)confirm();else move(y<240?-1:1);}
      else confirm();return true;}

  }
}
