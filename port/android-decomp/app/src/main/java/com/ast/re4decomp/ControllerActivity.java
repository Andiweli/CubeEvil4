package com.ast.re4decomp;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.hardware.input.InputManager;
import android.opengl.GLSurfaceView;
import android.opengl.GLES30;
import android.os.*;
import android.view.*;
import android.widget.*;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import org.json.JSONObject;
import org.json.JSONException;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Native render surface with a reversible in-Activity test/controls menu.
 */
public final class ControllerActivity
    extends Activity implements InputManager.InputDeviceListener, android.hardware.SensorEventListener {
  private android.hardware.SensorManager sensors;
  private android.hardware.Sensor gyroscope;
  private float gyroYaw,gyroPitch;
  @Override public void onAccuracyChanged(android.hardware.Sensor sensor,int accuracy) {}
  @Override public void onSensorChanged(android.hardware.SensorEvent event) {
    if(!activityResumed || !rendering || roomLoading || openingSubscreen || inventoryDialog!=null || cheatDialog!=null || pickupDialog!=null || saveDialog!=null || deathDialog!=null || !hasWindowFocus()) {
      gyroYaw=gyroPitch=0;NativeBridge.gyroRate(0,0);return;
    }
    // Gyroscope axes are fixed to the device's natural orientation.
    int rotation=getWindowManager().getDefaultDisplay().getRotation();
    float pitch=rotation==Surface.ROTATION_90?-event.values[1]:rotation==Surface.ROTATION_270?event.values[1]:rotation==Surface.ROTATION_180?-event.values[0]:event.values[0];
    float yaw=event.values[2];
    if(SystemClock.elapsedRealtimeNanos()-event.timestamp>150000000L || !Float.isFinite(yaw) || !Float.isFinite(pitch)) {
      gyroYaw=gyroPitch=0;NativeBridge.gyroRate(0,0);return;
    }
    gyroYaw+=.35f*(yaw-gyroYaw);gyroPitch+=.35f*(pitch-gyroPitch);
    NativeBridge.gyroRate(Math.abs(gyroYaw)<.006f?0:gyroYaw,Math.abs(gyroPitch)<.006f?0:gyroPitch);
  }
  private GLSurfaceView surface;
  private TextView stats, diagnosticsText, hud, inventoryText;
  private CockpitView cockpit;
  private BinocularView binocular;
  private ScopeView scope;
  private EventTitleView eventTitle;
  private StartupArt cockpitArt;
  private FrameLayout viewport;
  private LinearLayout loadingView;
  private TextView loadingText;
  private final SceneLoadGate sceneLoad = new SceneLoadGate();
  private final ExecutorService surfaceWorker = Executors.newSingleThreadExecutor();
  private boolean roomLoading;
  private volatile boolean exitAfterLoading;
  private ScrollView menu;
  private ImageButton back;
  private Button continueButton;
  private LinearLayout debugGroup;
  private boolean rendering = true, diagnostics, roomScene;
  private boolean activityResumed, surfaceRunning;
  private int safeLeft, safeTop, safeRight, safeBottom;
  private InputManager devices;
  private final Handler handler = new Handler(Looper.getMainLooper());
  private int activeDevice = -1, buttons, hats;
  private float lx, ly, rx, ry, lt, rt;
  private boolean swapFace;
  private final ControllerPolicy policy = new ControllerPolicy();
  private long sceneSession = System.nanoTime();
  private final ExecutorService roomWorker =
      Executors.newSingleThreadExecutor();
  private volatile boolean destroyed;
  private GameMenuArt inventoryWarmup;
  private void warmInventoryArt() {
    if(destroyed || inventoryWarmup==null)return;
    try {
      JSONObject snapshot=new JSONObject(NativeBridge.caseSnapshot());
      org.json.JSONArray items=snapshot.optJSONArray("items");int[] ids=new int[items==null?0:items.length()];
      for(int n=0;n<ids.length;++n)ids[n]=items.getJSONObject(n).getInt("id");
      inventoryWarmup.prepareInventory(snapshot.getInt("width"),snapshot.getInt("height"),ids,new JSONObject());
    } catch(JSONException unavailable) { inventoryWarmup.prepareInventory(10,6,new int[0],new JSONObject()); }
  }
  private InventoryDialog inventoryDialog;
  private PickupDialog pickupDialog;
  private SaveGameDialog saveDialog;
  private boolean preparingSave;
  private StartupArt saveArt;
  private StartupArt deathArt;
  private ContinueDialog deathDialog;
  private ContinueFlow deathFlow;
  private boolean preparingDeath,retrying;
  private CheatDialog cheatDialog;
  private boolean openingSubscreen, orderingPickup, restorePickupOnResume;
  private int pickupItemId = -1;
  private long pickupDialogToken;
  // Completed GL frames, independent of the original 30 Hz simulation.
  // GL owns the window; only the published value crosses to the HUD thread.
  private long fpsWindowStart;
  private int fpsWindowFrames;
  private volatile int renderedFps;
  private RoomRepository roomRepository;
  private long loadingToken;
  private final Runnable refresh = new Runnable() {
    @Override
    public void run() {
      if (destroyed) return;
      // Never enter scene/menu JNI while the first scene is being built. The
      // loading window must draw and receive focus independently of GL work.
      if (!sceneLoad.ready()) { handler.postDelayed(this, 100); return; }
      if(roomScene && inventoryWarmup==null){inventoryWarmup=new GameMenuArt(ControllerActivity.this::warmInventoryArt);warmInventoryArt();}
      pollRoomRequest();
      pollDeath();
      pollPickup();
      pollSave();
      InputDevice device = InputDevice.getDevice(activeDevice);
      String status = getString(
          R.string.controller_status,
          device == null ? getString(R.string.controller_prompt)
                         : getString(R.string.controller_name, device.getName(),
                                     device.getId(),
                                     device.isVirtual() ? " · virtual" : ""),
          NativeBridge.status());
      stats.setText(status);
      if (diagnostics)
        diagnosticsText.setText(status);
      if (roomScene) {
        hud.setText("Room: " + NativeBridge.roomId() + " · FPS " + renderedFps);
        boolean campaign=getIntent().getBooleanExtra("campaignMode",false);
        boolean event = NativeBridge.eventActive() || NativeBridge.deathRequested();
        cockpit.setVisibility(rendering && !roomLoading && !event ? View.VISIBLE : View.GONE);
        binocular.setVisibility(rendering && !roomLoading && NativeBridge.binocularState()[0]!=0 ? View.VISIBLE : View.GONE);
        scope.setVisibility(rendering && !roomLoading && NativeBridge.scopeState()[0]!=0 ? View.VISIBLE : View.GONE);
        eventTitle.setVisibility(rendering && !roomLoading && NativeBridge.eventTitleState()[2]!=0 ? View.VISIBLE : View.GONE);
        hud.setVisibility(rendering && !roomLoading && !event && NativeBridge.roomFpsVisible() && hud.length() != 0 ? View.VISIBLE
                                                         : View.GONE);
        back.setVisibility(rendering && !roomLoading && !event ? View.VISIBLE : View.GONE);
        if (!rendering && inventoryText != null)
          inventoryText.setText(NativeBridge.inventory());
      }
      handler.postDelayed(this, 100);
    }
  };
  @Override
  public void onCreate(Bundle state) {
    super.onCreate(state);
    GameMenuArt.resetCache();
    sensors=(android.hardware.SensorManager)getSystemService(Context.SENSOR_SERVICE);
    gyroscope=sensors==null?null:sensors.getDefaultSensor(android.hardware.Sensor.TYPE_GYROSCOPE);
    NativeBridge.gyroAvailable(gyroscope!=null);
    if (state != null) {
      sceneSession = state.getLong("sceneSession", sceneSession);
      rendering = state.getBoolean("rendering", true);
      diagnostics = state.getBoolean("diagnostics", false);
      if(state.containsKey("deathPhase")) {
        deathFlow=new ContinueFlow();deathFlow.phase=ContinueFlow.Phase.valueOf(state.getString("deathPhase"));
        deathFlow.yes=state.getBoolean("deathYes",true);deathFlow.deadFrame=state.getInt("deathFrame");
        deathFlow.choiceFrame=state.getInt("deathChoice");deathFlow.cursorFrame=state.getInt("deathCursor");
        deathFlow.exitFrame=state.getInt("deathExit");deathFlow.gate=state.getInt("deathGate");deathFlow.remaining=state.getInt("deathRemaining");
      }
    }
    roomScene = getIntent().hasExtra("previewScene");
    devices = (InputManager)getSystemService(Context.INPUT_SERVICE);
    viewport = new FrameLayout(this);
    LinearLayout root = new LinearLayout(this);
    root.setOrientation(LinearLayout.VERTICAL);
    int padding = dp(20);
    root.setPadding(padding, padding, padding, padding);
    TextView title = new TextView(this);
    title.setText(
        getIntent().hasExtra("previewScene")
            ? ("r100".equals(getIntent().getStringExtra("sceneRoom"))
                   ? R.string.scene_title_r100
                   : R.string.scene_title)
        : getIntent().hasExtra("previewTpl") ? R.string.room_preview_title
                                             : R.string.test_title);
    if (getIntent().hasExtra("previewScene") &&
        getIntent().hasExtra("sceneRoom"))
      title.setText(getString(R.string.scene_title_selected,
                              getIntent().getStringExtra("sceneRoom")));
    if (getIntent().hasExtra("previewScene"))
      title.setText(R.string.runtime_title);
    title.setTextSize(20);
    root.addView(title);
    continueButton = new Button(this);
    continueButton.setText(R.string.render_continue);
    continueButton.setOnClickListener(v -> showRender());
    root.addView(continueButton);
    stats = new TextView(this);
    stats.setMinLines(getIntent().hasExtra("previewScene") ? 3 : 6);
    stats.setTextIsSelectable(true);
    CheckBox swap = new CheckBox(this);
    swap.setText(R.string.swap_layout);
    if (getIntent().hasExtra("previewScene"))
      swap.setVisibility(android.view.View.GONE);
    root.addView(swap);
    swap.setOnCheckedChangeListener((v, on) -> {
      swapFace = on;
      clearInput();
    });
    TextView help = new TextView(this);
    help.setText(getIntent().hasExtra("previewScene")
                     ? R.string.scene_controls_short
                     : R.string.controller_help);
    if (getIntent().hasExtra("previewScene"))
      help.setOnClickListener(
          v
          -> new android.app.AlertDialog.Builder(this)
                 .setTitle(R.string.controls_title)
                 .setMessage(R.string.scene_controls)
                 .setPositiveButton(android.R.string.ok, null)
                 .show());
    root.addView(help);
    debugGroup = new LinearLayout(this);
    debugGroup.setOrientation(LinearLayout.VERTICAL);
    debugGroup.setVisibility(roomScene ? android.view.View.GONE : android.view.View.VISIBLE);
    surface = new GLSurfaceView(this);
    surface.setEGLContextClientVersion(3);
    surface.setEGLConfigChooser(new SingleSampleConfigChooser());
    surface.setPreserveEGLContextOnPause(true);
    File game = ((Re4Application)getApplication()).imports.root;
    if (getIntent().hasExtra("previewScene")) {
      try (Reader rooms = new InputStreamReader(getAssets().open("rooms.tsv"),
                                                StandardCharsets.UTF_8);
           Reader blocks = new InputStreamReader(getAssets().open("blocks.tsv"),
                                                 StandardCharsets.UTF_8)) {
        roomRepository = new RoomRepository(
            game, getCacheDir(), rooms, blocks,
            (source, output)
                -> NativeBridge.decodeRoom(source.getAbsolutePath(),
                                           output.getAbsolutePath()));
      } catch (IOException e) {
        android.util.Log.w("RE4Flow", "Room catalog is unavailable", e);
      }
      Button doorTest = new Button(this);
      doorTest.setText(R.string.door_test);
      doorTest.setOnClickListener(v -> {
        surface.queueEvent(NativeBridge::requestDoorTest);
        showRender();
      });
      debugGroup.addView(doorTest);
      Button caseButton = new Button(this);
      caseButton.setText(R.string.open_case);
      caseButton.setOnClickListener(v -> showInventory(false));
      root.addView(caseButton);
      Button inspector = new Button(this);
      inspector.setText(R.string.model_inspector);
      inspector.setOnClickListener(v -> {
        NativeBridge.requestInspector();
        showRender();
      });
      debugGroup.addView(inspector);
    }
    CheckBox diagnosticToggle = new CheckBox(this);
    diagnosticToggle.setText(R.string.render_diagnostics);
    diagnosticToggle.setChecked(diagnostics);
    diagnosticToggle.setOnCheckedChangeListener((v, checked) -> {
      diagnostics = checked;
      updatePresentation();
    });
    debugGroup.addView(diagnosticToggle);
    debugGroup.addView(stats);
    Button selection = new Button(this);
    selection.setText(R.string.render_selection);
    selection.setOnClickListener(v -> finish());
    if (roomScene) {
      Button debugMenu = new Button(this);
      debugMenu.setText(R.string.browser_diagnostics);
      debugMenu.setOnClickListener(v -> debugGroup.setVisibility(
          debugGroup.getVisibility() == android.view.View.VISIBLE ? android.view.View.GONE : android.view.View.VISIBLE));
      root.addView(debugMenu);
    }
    root.addView(debugGroup);
    root.addView(selection);
    String path =
        game == null
            ? ""
            : new File(game, "disc1/files/etc/pl010a.tpl").getAbsolutePath();
    String requested = getIntent().getStringExtra("previewTpl");
    if (requested != null) {
      try {
        File preview = new File(requested).getCanonicalFile();
        if (!preview.getParentFile().equals(getCacheDir().getCanonicalFile()) ||
            !preview.isFile())
          throw new java.io.IOException("Invalid preview");
        path = preview.getAbsolutePath();
      } catch (java.io.IOException e) {
        path = "";
      }
    }
    String scenePath = "";
    String sceneRequested = getIntent().getStringExtra("previewScene");
    if (sceneRequested != null) {
      try {
        File file = new File(sceneRequested).getCanonicalFile();
        if (!file.getParentFile().equals(getCacheDir().getCanonicalFile()) ||
            !file.isFile())
          throw new java.io.IOException("Invalid 3D preview");
        scenePath = file.getAbsolutePath();
      } catch (java.io.IOException e) {
        scenePath = "";
      }
    }
    final String decodedScene = scenePath;
    final boolean sceneMode = sceneRequested != null;
    final String texturePath = path;
    final String initialRoom = getIntent().getStringExtra("sceneRoom") == null
                                   ? "r300"
                                   : getIntent().getStringExtra("sceneRoom");
    surface.setRenderer(new GLSurfaceView.Renderer() {
      @Override
      public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        int[] samples=new int[1];
        GLES30.glGetIntegerv(GLES30.GL_SAMPLES,samples,0);
        android.util.Log.i("RE4Graphics","Window samples: "+samples[0]+"; selectable MSAA uses the scene framebuffer");
        sceneLoad.newContext();
        GLES30.glClearColor(0.035f, 0.04f, 0.045f, 1);
        handler.post(() -> {
          if (destroyed) return;
          if (exitAfterLoading || sceneLoad.cancelled()) { finish(); return; }
          showLoading(initialRoom);
        });
      }
      private void createNativeScene() {
        if (sceneMode) {
          String player = "", weapon = "", roomInfo = "", core = "";
          try {
            if (game == null)
              throw new IOException("Disc 1 is missing");
            player =
                checkedActor(new File(game, "disc1/files/em/pl00.drs"), 2215392,
                             "5ea520d7026c48e33e12256ef82173140e72cf401975816"
                                 + "7a7d5aa5b6e356090");
            weapon =
                checkedActor(new File(game, "disc1/files/em/wep01.drs"), 437536,
                             "fb838948d4161ee04da306ff0910980ff1b9e3b5701b439"
                                 + "83a36e1d21bca3689");
          } catch (IOException e) {
            player = weapon = "";
            android.util.Log.w("RE4Player",
                               "Leon data unavailable: " + e.getMessage());
          }
          try {
            roomInfo = RoomRepository.checkedRoomInfo(game).getAbsolutePath();
          } catch (IOException e) {
            android.util.Log.w("RE4Flow", "Original room starts missing: " +
                                              e.getMessage());
          }
          try {
            if (game != null)
              core = checkedActor(new File(game, "disc1/files/etc/core.das"),
                                  2525120,
                                  "018c6ddd4aa65a3f6325e2fbda4fecc1afd2b7fbb2" +
                                  "695146a828f3c641abfaad");
          } catch (IOException e) {
            android.util.Log.w("RE4Environment",
                               "Water CORE missing: " + e.getMessage());
          }
          String chicken = "", chickenList = "";
          try {
            if (game == null) throw new IOException("Disc 1 is missing");
            chicken = checkedActor(new File(game, "disc1/files/em/em28.drs"), 228928,
                "28968cfd709f670c6c17c7fdbbffc9b27240763ad0fb51fd62e28058a7594823");
            chickenList = checkedActor(new File(game, "disc1/files/etc/omake01.esl"), 8160,
                "25b63cfffc1868afa1605bf697aef452806aaa92d6cff42e494b09371836b9ed");
          } catch (IOException e) {
            chicken = chickenList = "";
            android.util.Log.w("RE4Animals", "r400 chicken data missing: " + e.getMessage());
          }
          NativeBridge.configureAmbient(chicken, chickenList);
          NativeBridge.environmentData(core);
          String enemy = "", enemyList = "";
          try {
            if (game == null)
              throw new IOException("Disc 1 is missing");
            enemy =
                checkedActor(new File(game, "disc1/files/em/em12.drs"), 5664736,
                             "6468f2d05d496b27d193ad79b3afeb20f40edcd0f54d3c2"
                                 + "d16da7a3c03c16e7b");
            enemyList = checkedActor(
                new File(game, "disc1/files/etc/emleon00.esl"), 8160,
                "65cd90d8979caab88476677aeec3bc33138c8542f106cbdc09290dc71f04"
                    + "5126");
          } catch (IOException e) {
            enemy = enemyList = "";
            android.util.Log.w("RE4Enemy",
                               "Ganado data missing: " + e.getMessage());
          }
          // EM12 also owns Leon's original grab and kick curves. Configure
          // those assets before creating the player, including catalog rooms
          // opened directly without either combat-test button.
          NativeBridge.configureEnemy(enemy, enemyList);
          String handgun = "", shotgun = "";
          try {
            if (game == null) throw new IOException("Disc 1 is missing");
            handgun = checkedActor(new File(game,"disc1/files/em/wep02.drs"),442880,
                "86121ae13a7b82d2eb11c94a705f94a9a9d183fcd569077c68bd2c06b90955e2");
            shotgun = checkedActor(new File(game,"disc1/files/em/wep07.drs"),490848,
                "1b7807e6d5839acccb17cf3e6abff70b0779f93a09265a8d16be3932570ebe2a");
          } catch (IOException e) { android.util.Log.e("RE4Weapon","Additional weapons missing: " + e.getMessage()); }
          NativeBridge.configureWeapons(handgun,shotgun);
          NativeBridge.configureMusic(game == null ? "" : new File(game,"disc1/files/bgm").getAbsolutePath());
          NativeBridge.configureSubscreen(game == null ? "" : new File(game,"disc1/files").getAbsolutePath(), 1);
          NativeBridge.configurePlayMode(getIntent().getBooleanExtra("exploreMode", true), sceneSession);
          NativeBridge.configureCampaignMode(getIntent().getBooleanExtra("campaignMode", false),
              getIntent().getIntExtra("campaignDifficulty", 0), sceneSession);
          NativeBridge.configureOpeningCanceled(getIntent().getBooleanExtra("openingCanceled", false));
          byte[] savedGame=getIntent().getByteArrayExtra("savedGame");
          if(savedGame!=null)NativeBridge.configureSavedGame(savedGame);
          NativeBridge.configureAimInversion(getSharedPreferences("game-options", MODE_PRIVATE)
              .getBoolean("invertAimY", false));
          NativeBridge.configureDisplayBrightness(getSharedPreferences("game-options", MODE_PRIVATE)
              .getInt("brightness", 64));
          NativeBridge.configureTestProfile(getIntent().getIntExtra("testProfile", 0), sceneSession);
          NativeBridge.createScene(decodedScene, player, weapon, roomInfo,
                                   initialRoom, sceneSession);
          IngameSettings.restore(ControllerActivity.this);
        } else
          NativeBridge.create(texturePath,
                              getIntent().getIntExtra("previewIndex", 0));
      }
      @Override
      public void onSurfaceChanged(GL10 gl, int w, int h) {
        NativeBridge.resize(w, h);
        fpsWindowStart = 0;
        fpsWindowFrames = renderedFps = 0;
      }
      @Override
      public void onDrawFrame(GL10 gl) {
        if (!sceneLoad.ready()) {
          GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT | GLES30.GL_DEPTH_BUFFER_BIT);
          final long first = sceneLoad.firstFrame();
          if (first != 0) handler.post(() -> sceneLoad.confirmFrame(first));
          // The confirmation runs on main only after SurfaceView's initial
          // surfaceChanged wait has returned. Focus is a separate requirement.
          if (sceneLoad.start()) {
            final long context = sceneLoad.generation();
            try { createNativeScene(); }
            catch (RuntimeException failure) { android.util.Log.e("RE4Scene", "Initialization canceled", failure); }
            finally {
              if (sceneLoad.complete(context)) handler.post(() -> {
                if (destroyed || !sceneLoad.ready() || sceneLoad.generation() != context) return;
                if (exitAfterLoading || sceneLoad.cancelled()) { finish(); return; }
                hideLoading();
                if (!rendering && inventoryDialog == null && pickupDialog == null) pauseSurface();
                GameAudio.setActive(activityResumed && rendering && roomScene);
              });
            }
          }
          return;
        }
        if (sceneLoad.cancelled()) {
          GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT | GLES30.GL_DEPTH_BUFFER_BIT);
          return;
        }
        NativeBridge.draw();
        long now = System.nanoTime();
        if (fpsWindowStart == 0 || now - fpsWindowStart > 2_000_000_000L) {
          fpsWindowStart = now;
          fpsWindowFrames = 0;
          renderedFps = 0;
        } else {
          ++fpsWindowFrames;
          long elapsed = now - fpsWindowStart;
          if (elapsed >= 500_000_000L) {
            renderedFps = (int)Math.round(fpsWindowFrames * 1_000_000_000.0 / elapsed);
            fpsWindowStart = now;
            fpsWindowFrames = 0;
          }
        }
      }
    });
    viewport.addView(surface, new FrameLayout.LayoutParams(-1, -1));
    cockpit=new CockpitView(this);viewport.addView(cockpit,new FrameLayout.LayoutParams(-1,-1));
    cockpit.setVisibility(View.GONE);
    binocular=new BinocularView(this);viewport.addView(binocular,new FrameLayout.LayoutParams(-1,-1));binocular.setVisibility(View.GONE);
    scope=new ScopeView(this);viewport.addView(scope,new FrameLayout.LayoutParams(-1,-1));scope.setVisibility(View.GONE);
    eventTitle=new EventTitleView(this);viewport.addView(eventTitle,new FrameLayout.LayoutParams(-1,-1));eventTitle.setVisibility(View.GONE);
    if(roomScene)roomWorker.execute(()->{
      try {
        File hudGameRoot=((Re4Application)getApplication()).imports.root;
        if(hudGameRoot==null)throw new IOException("Disc 1 is missing");
        NativeBridge.startupConfigure(new File(hudGameRoot,"disc1/files").getAbsolutePath(),1);
        final StartupArt artwork=StartupArt.loadCockpit();
        final StartupArt dead=StartupArt.loadDeath();
        handler.post(()->{if(!destroyed)deathArt=dead;});
        handler.post(()->{if(!destroyed){cockpitArt=artwork;cockpit.artwork(artwork);if(inventoryDialog!=null)inventoryDialog.setCockpitArt(artwork);}});
        NativeBridge.configureSubscreen(new File(hudGameRoot,"disc1/files").getAbsolutePath(),1);
        final StartupArt binocularArt=StartupArt.loadBinocular();
        handler.post(()->{if(!destroyed)binocular.artwork(binocularArt);});
        final java.util.Map<Integer,StartupArt> rifleArt=new java.util.HashMap<>();
        for(int id:new int[]{0x2e,0x2f,0x6b,0x6c,0x99,0x51,0x35,0x17,0x6d,0xab})rifleArt.put(id,StartupArt.loadOriginalView("scope_"+id));
        handler.post(()->{if(!destroyed)scope.artwork(rifleArt);});
        final StartupArt chapterTitle=StartupArt.loadOriginalView("village_title");
        handler.post(()->{if(!destroyed)eventTitle.artwork(chapterTitle);});
      } catch(Exception error){android.util.Log.e("RE4Cockpit","Could not load original HUD",error);}
    });
    hud = new TextView(this);
    hud.setTextColor(Color.WHITE);
    hud.setTextSize(10);
    hud.setMaxLines(4);
    hud.setPadding(dp(6), dp(3), dp(6), dp(3));
    hud.setBackgroundColor(Color.argb(150, 0, 0, 0));
    viewport.addView(hud, new FrameLayout.LayoutParams(
                              -2, -2, Gravity.BOTTOM | Gravity.START));
    diagnosticsText = new TextView(this);
    diagnosticsText.setTextColor(Color.WHITE);
    diagnosticsText.setTextSize(11);
    diagnosticsText.setMaxLines(8);
    diagnosticsText.setPadding(dp(8), dp(6), dp(8), dp(6));
    diagnosticsText.setBackgroundColor(Color.argb(185, 0, 0, 0));
    viewport.addView(diagnosticsText, new FrameLayout.LayoutParams(
                                          -1, -2, Gravity.TOP | Gravity.START));
    back = new ImageButton(this);
    back.setImageResource(R.drawable.ic_arrow_back);
    back.setContentDescription(getString(R.string.render_back));
    back.setPadding(dp(12), dp(12), dp(12), dp(12));
    back.setFocusable(false);
    GradientDrawable background = new GradientDrawable();
    background.setShape(GradientDrawable.OVAL);
    background.setColor(Color.argb(150, 0, 0, 0));
    back.setBackground(background);
    back.setOnClickListener(v -> showMenu());
    viewport.addView(back, new FrameLayout.LayoutParams(
                               dp(48), dp(48), Gravity.TOP | Gravity.START));
    menu = new ScrollView(this);
    menu.setFillViewport(true);
    menu.setBackgroundColor(Color.rgb(24, 26, 28));
    menu.addView(root);
    viewport.addView(menu, new FrameLayout.LayoutParams(-1, -1));
    loadingView = new LinearLayout(this);
    loadingView.setOrientation(LinearLayout.VERTICAL);
    loadingView.setGravity(Gravity.CENTER);
    loadingView.setBackgroundColor(Color.rgb(9, 10, 11));
    loadingView.setFocusableInTouchMode(true);
    loadingView.addView(new ProgressBar(this), new LinearLayout.LayoutParams(dp(56), dp(56)));
    loadingText = new TextView(this);
    loadingText.setTextColor(Color.WHITE); loadingText.setTextSize(18);
    loadingText.setPadding(dp(20), dp(20), dp(20), dp(20));
    loadingView.addView(loadingText);
    Button cancelLoad = new Button(this);
    cancelLoad.setText(android.R.string.cancel);
    cancelLoad.setOnClickListener(v -> cancelRoomLoad());
    loadingView.addView(cancelLoad);
    viewport.addView(loadingView, new FrameLayout.LayoutParams(-1, -1));
    showLoading(initialRoom);
    setContentView(viewport);
    viewport.setOnApplyWindowInsetsListener((v, insets) -> {
      safeLeft = insets.getSystemWindowInsetLeft();
      safeTop = insets.getSystemWindowInsetTop();
      safeRight = insets.getSystemWindowInsetRight();
      safeBottom = insets.getSystemWindowInsetBottom();
      if (Build.VERSION.SDK_INT >= 28 && insets.getDisplayCutout() != null) {
        DisplayCutout cutout = insets.getDisplayCutout();
        safeLeft = Math.max(safeLeft, cutout.getSafeInsetLeft());
        safeTop = Math.max(safeTop, cutout.getSafeInsetTop());
        safeRight = Math.max(safeRight, cutout.getSafeInsetRight());
        safeBottom = Math.max(safeBottom, cutout.getSafeInsetBottom());
      }
      applyInsets();
      return insets;
    });
    updatePresentation();
    if (!rendering)
      continueButton.requestFocus();
    if (Build.VERSION.SDK_INT >= 33)
      getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
          android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT,
          this::onBackPressed);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
  }
  private void showLoading(String room) {
    if (loadingView == null) return;
    // Original campaign events fade to black and hold that frame during
    // room preparation. Keep the explicit progress controls in the room
    // inspector, where the user deliberately selects diagnostic assets.
    boolean campaign = getIntent().getBooleanExtra("campaignMode", false);
    loadingView.setBackgroundColor(campaign ? Color.BLACK : Color.rgb(9, 10, 11));
    for (int i = 0; i < loadingView.getChildCount(); ++i)
      loadingView.getChildAt(i).setVisibility(campaign ? View.GONE : View.VISIBLE);
    loadingText.setText(getString(R.string.browser_loading, room));
    loadingView.setVisibility(View.VISIBLE);
    loadingView.requestFocus();
    hud.setVisibility(View.GONE); back.setVisibility(View.GONE);
  }
  private void hideLoading() {
    if (loadingView != null) loadingView.setVisibility(View.GONE);
    updatePresentation();
    if (!rendering) continueButton.requestFocus();
  }
  private void cancelRoomLoad() {
    if (destroyed || exitAfterLoading) return;
    exitAfterLoading = true;
    clearInput();
    loadingText.setText("Finishing loading …");
    if (!sceneLoad.ready()) {
      if (sceneLoad.cancel()) finish();
    } else if (roomLoading && loadingToken > 0) {
      // Finish the mailbox as a failure. The worker checks exitAfterLoading
      // before publishing success, and the current room remains intact.
      NativeBridge.finishRoomRequest(loadingToken, "", "Loading canceled");
    } else finish();
  }
  private int dp(int value) {
    return Math.round(value * getResources().getDisplayMetrics().density);
  }
  private void applyInsets() {
    // Insets affect overlays/menu only; the GL viewport occupies the window.
    menu.setPadding(safeLeft, safeTop, safeRight, safeBottom);
    FrameLayout.LayoutParams arrow =
        (FrameLayout.LayoutParams)back.getLayoutParams();
    arrow.leftMargin = safeLeft + dp(8);
    arrow.topMargin = safeTop + dp(8);
    back.setLayoutParams(arrow);
    FrameLayout.LayoutParams debug =
        (FrameLayout.LayoutParams)diagnosticsText.getLayoutParams();
    debug.leftMargin = safeLeft + dp(8);
    debug.rightMargin = safeRight + dp(8);
    debug.topMargin = safeTop + dp(64);
    diagnosticsText.setLayoutParams(debug);
    FrameLayout.LayoutParams playerHud =
        (FrameLayout.LayoutParams)hud.getLayoutParams();
    playerHud.leftMargin = safeLeft + dp(12);
    playerHud.rightMargin = safeRight + dp(12);
    playerHud.bottomMargin = playerHud.leftMargin;
    hud.setLayoutParams(playerHud);
    cockpit.setPadding(safeLeft,safeTop,safeRight,safeBottom);
  }
  private void updatePresentation() {
    if (menu == null)
      return;
    menu.setVisibility(rendering ? View.GONE : View.VISIBLE);
    boolean loading = !sceneLoad.ready() || roomLoading;
    boolean event = roomScene && (NativeBridge.eventActive() || NativeBridge.deathRequested());
    back.setVisibility(rendering && !loading && !event ? View.VISIBLE : View.GONE);
    stats.setVisibility(diagnostics ? View.VISIBLE : View.GONE);
    diagnosticsText.setVisibility(rendering && diagnostics ? View.VISIBLE
                                                           : View.GONE);
    boolean campaign=getIntent().getBooleanExtra("campaignMode",false);
    cockpit.setVisibility(rendering && !loading && !event && roomScene ? View.VISIBLE : View.GONE);
    binocular.setVisibility(rendering && !loading && roomScene && NativeBridge.binocularState()[0]!=0 ? View.VISIBLE : View.GONE);
    scope.setVisibility(rendering && !loading && roomScene && NativeBridge.scopeState()[0]!=0 ? View.VISIBLE : View.GONE);
    eventTitle.setVisibility(rendering && !loading && roomScene && NativeBridge.eventTitleState()[2]!=0 ? View.VISIBLE : View.GONE);
    hud.setVisibility(rendering && !loading && !event && roomScene && NativeBridge.roomFpsVisible() && hud.length() != 0 ? View.VISIBLE
                                                                  : View.GONE);
    getWindow().getDecorView().setSystemUiVisibility(
        rendering ? View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                        View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                  : View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    viewport.requestApplyInsets();
  }
  private void showRender() {
    clearInput();
    rendering = true;
    updatePresentation();
    if (activityResumed)
      resumeSurface();
  }
  private void showMenu() {
    if (skipEvent()) return;
    if (roomScene) showInventory(true, InventoryDialog.CASE);
    else showDiagnosticsMenu();
  }
  private void showDiagnosticsMenu() {
    clearInput();
    rendering = false;
    pauseSurface();
    if (roomScene && debugGroup != null) debugGroup.setVisibility(View.VISIBLE);
    updatePresentation();
    continueButton.requestFocus();
  }
  private void showCheats() {
    if(!roomScene || destroyed || !sceneLoad.ready() || roomLoading || openingSubscreen || cheatDialog!=null || inventoryDialog!=null || pickupDialog!=null || saveDialog!=null || NativeBridge.eventActive() || NativeBridge.deathRequested())return;
    openingSubscreen=true;clearInput();
    surface.queueEvent(()->{
      if(NativeBridge.eventActive() || NativeBridge.deathRequested()){handler.post(()->openingSubscreen=false);return;}
      NativeBridge.setSubscreenOpen(true);
      final int[] graphicsOptions=NativeBridge.graphicsOptions();
      handler.post(()->{
        openingSubscreen=false;
        if(destroyed){surface.queueEvent(()->NativeBridge.setSubscreenOpen(false));return;}
        NativeBridge.subscreenAudio(true);
        cheatDialog=new CheatDialog(this,NativeBridge.cheatFlags(),graphicsOptions,new CheatDialog.Listener(){
          @Override public void flags(int flags){surface.queueEvent(()->{NativeBridge.setCheatFlags(flags);IngameSettings.cheats(ControllerActivity.this,flags);});}
          @Override public void weapon(int id){surface.queueEvent(()->{String message=NativeBridge.giveTestWeapon(id);handler.post(()->{if(cheatDialog!=null)cheatDialog.result(message);});});}
        },(msaa,af,mipmapping,roomFps,dof,gyro)->surface.queueEvent(()->{
          String result=NativeBridge.configureGraphics(msaa,af,mipmapping,roomFps,gyro,dof);
          IngameSettings.graphics(ControllerActivity.this,msaa,af,mipmapping,roomFps,gyro,dof);
          int[] applied=NativeBridge.graphicsOptions();
          handler.post(()->{if(cheatDialog!=null)cheatDialog.graphicsResult(applied,result);});
        }));
        cheatDialog.setOnDismissListener(dialog->{cheatDialog=null;clearInput();surface.queueEvent(()->NativeBridge.setSubscreenOpen(false));NativeBridge.subscreenAudio(false);});
        cheatDialog.show();
      });
    });
  }
  private void showInventory(boolean fromGame) { showInventory(fromGame, InventoryDialog.CASE); }
  private void showInventory(boolean fromGame, int firstTab) {
    if (!roomScene || destroyed || !sceneLoad.ready() || roomLoading || openingSubscreen || cheatDialog != null || inventoryDialog != null || pickupDialog != null || saveDialog != null || preparingSave || NativeBridge.eventActive() || NativeBridge.deathRequested()) return;
    openingSubscreen = true;
    clearInput();
    // The GL thread keeps servicing native menu transactions and original item
    // model previews while setSubscreenOpen freezes the gameplay simulation.
    if (activityResumed && !surfaceRunning) resumeSurface();
    surface.queueEvent(() -> {
      // Recheck on the GL owner: an event can begin between the UI guard and this command.
      if (NativeBridge.eventActive()) { handler.post(() -> openingSubscreen = false); return; }
      NativeBridge.setSubscreenOpen(true);
      final String snapshot = NativeBridge.caseSnapshot();
      final String world = NativeBridge.subscreenSnapshot();
      handler.post(() -> {
        openingSubscreen = false;
        if (destroyed || !activityResumed || snapshot.isEmpty()) {
          surface.queueEvent(() -> NativeBridge.setSubscreenOpen(false));
          if (!destroyed && snapshot.isEmpty()) android.util.Log.w("RE4Menu", "Inventory is not available yet");
          return;
        }
        NativeBridge.subscreenAudio(true);
        GameAudio.setActive(activityResumed && roomScene);
        NativeBridge.menuSound(2);
        inventoryDialog = createInventoryDialog(snapshot, world, firstTab);
        inventoryDialog.setOnDismissListener(d -> {
          inventoryDialog = null; clearInput(); NativeBridge.menuSound(3);
          surface.queueEvent(() -> { NativeBridge.subscreenItem(-1, false); NativeBridge.setSubscreenOpen(false); });
          NativeBridge.subscreenAudio(false);
          if (!destroyed && fromGame) showRender();
          else if (!destroyed && !rendering) pauseSurface();
          GameAudio.setActive(!destroyed && activityResumed && rendering && roomScene);
        });
        inventoryDialog.show();
      });
    });
  }
  private InventoryDialog createInventoryDialog(String snapshot, String world, int firstTab) {
    InventoryDialog dialog=new InventoryDialog(this, snapshot, world, firstTab,
        (op, slot, a, b, c) -> surface.queueEvent(() -> {
          NativeBridge.caseAction(op, slot, a, b, c);
          final String nextSnapshot = NativeBridge.caseSnapshot();
          final String nextWorld = NativeBridge.subscreenSnapshot();
          handler.post(() -> {
            if (!destroyed && inventoryDialog != null) {
              inventoryDialog.setSnapshot(nextSnapshot); inventoryDialog.setWorldSnapshot(nextWorld);
            }
          });
        }),
        operation -> {
          if (operation == 1) showDiagnosticsMenu();
          else if (operation == 2) finish();
        }, id -> { NativeBridge.resetInput(); surface.queueEvent(() -> NativeBridge.subscreenItem(id, true)); });
    dialog.setCockpitArt(cockpitArt);return dialog;
  }
  private void orderPickupCase() {
    if (destroyed || pickupDialog == null || inventoryDialog != null || openingSubscreen) return;
    openingSubscreen = true; orderingPickup = true; pickupDialog.hide(); clearInput();
    surface.queueEvent(() -> {
      // Keep the same pause and item-get request. The item is still outside the
      // inventory until its single final Yes/No answer has been committed.
      NativeBridge.subscreenItem(-1, false);
      final String snapshot = NativeBridge.caseSnapshot();
      final String world = NativeBridge.subscreenSnapshot();
      handler.post(() -> {
        openingSubscreen = false;
        if (destroyed || pickupDialog == null) return;
        if (!activityResumed || snapshot.isEmpty()) { restorePickupScreen(); return; }
        inventoryDialog = createInventoryDialog(snapshot, world, InventoryDialog.CASE);
        inventoryDialog.setPickupOrdering();
        inventoryDialog.setOnDismissListener(d -> {
          inventoryDialog = null; clearInput(); NativeBridge.menuSound(5);
          if (!destroyed) restorePickupScreen();
        });
        inventoryDialog.show();
      });
    });
  }
  private void restorePickupScreen() {
    orderingPickup = false; clearInput();
    if (destroyed || pickupDialog == null) return;
    surface.queueEvent(() -> {
      NativeBridge.subscreenItem(pickupItemId, false);
      final String pending = NativeBridge.pendingPickup();
      handler.post(() -> {
        if (destroyed || pickupDialog == null) return;
        try {
          JSONObject item = new JSONObject(pending);
          if (item.getLong("token") != pickupDialogToken) { pickupDialog.abandon(); return; }
          pickupDialog.setFits(item.getBoolean("fits"));
        } catch (JSONException invalid) { pickupDialog.abandon(); return; }
        if (activityResumed) { restorePickupOnResume = false; pickupDialog.show(); }
        else restorePickupOnResume = true;
      });
    });
  }
  private void pollSave() {
    if(!roomScene||!rendering||!activityResumed||destroyed||!sceneLoad.ready()||roomLoading||openingSubscreen||pickupDialog!=null||inventoryDialog!=null||saveDialog!=null||preparingSave||!NativeBridge.saveRequested())return;
    preparingSave=true;clearInput();NativeBridge.subscreenAudio(true);
    final GameSaveStore store=new GameSaveStore(new File(getFilesDir(),"saves"));
    roomWorker.execute(()->{StartupArt ready=saveArt;String failure="";try{if(ready==null){File game=((Re4Application)getApplication()).imports.root;if(game==null)throw new IOException("Disc 1 is missing");NativeBridge.startupConfigure(new File(game,"disc1/files").getAbsolutePath(),1);ready=StartupArt.load();}}catch(Exception e){failure=e.getMessage();}
      final OriginalSaveHeader[] headers=store.headers();
      final StartupArt artwork=ready;final String error=failure;handler.post(()->{preparingSave=false;if(destroyed)return;if(artwork==null){surface.queueEvent(NativeBridge::finishSave);NativeBridge.subscreenAudio(false);Toast.makeText(this,error,Toast.LENGTH_LONG).show();return;}saveArt=artwork;
        saveDialog=new SaveGameDialog(this,artwork,store,headers,new SaveGameDialog.Listener(){
          @Override public void save(int slot){java.util.Calendar c=java.util.Calendar.getInstance();int[]clock={c.get(java.util.Calendar.SECOND),c.get(java.util.Calendar.MINUTE),c.get(java.util.Calendar.HOUR_OF_DAY),c.get(java.util.Calendar.DAY_OF_MONTH),c.get(java.util.Calendar.MONTH),c.get(java.util.Calendar.YEAR),c.get(java.util.Calendar.DAY_OF_WEEK)-1,c.get(java.util.Calendar.DAY_OF_YEAR)-1,0,0};
            surface.queueEvent(()->{
              byte[]image=NativeBridge.saveGame(slot,clock);long transaction=NativeBridge.saveWriteId();
              roomWorker.execute(()->{
                IOException failure=null;
                try{if(image==null || transaction==0)throw new IOException("Could not create save file");store.write(slot,image);}
                catch(IOException e){failure=e;}
                final IOException writeFailure=failure;final OriginalSaveHeader[] rows=store.headers();
                surface.queueEvent(()->{
                  boolean committed=NativeBridge.completeSaveWrite(transaction,writeFailure==null);
                  final IOException result=writeFailure!=null?writeFailure:committed?null:new IOException("Save operation is no longer active");
                  handler.post(()->{if(!destroyed&&saveDialog!=null)saveDialog.completed(rows,result);});
                });
              });
            });}
          @Override public void delete(int slot){roomWorker.execute(()->{
            IOException failure=null;try{store.delete(slot);}catch(IOException e){failure=e;}
            final IOException result=failure;final OriginalSaveHeader[] rows=store.headers();handler.post(()->{if(!destroyed&&saveDialog!=null)saveDialog.completed(rows,result);});
          });}
          @Override public void closed(){saveDialog=null;clearInput();surface.queueEvent(NativeBridge::finishSave);NativeBridge.subscreenAudio(false);}
        });saveDialog.show();});});
  }
  private void pollDeath() {
    if(!roomScene||destroyed||!sceneLoad.ready())return;
    if(!NativeBridge.deathRequested()){retrying=false;deathFlow=null;return;}
    // The GL command publishes the room request asynchronously. Keep the
    // accepted YES latched until the native death state clears or loading fails.
    if(retrying)return;
    if(!rendering||!activityResumed||roomLoading||preparingDeath||deathDialog!=null||pickupDialog!=null||inventoryDialog!=null||saveDialog!=null)return;
    int[] picture=NativeBridge.deathBackdrop();
    if(picture==null||picture.length!=2+640*480)return;
    clearInput();
    if(deathArt==null) {
      preparingDeath=true;roomWorker.execute(()->{
        try {final StartupArt artwork=StartupArt.loadDeath();handler.post(()->{preparingDeath=false;if(!destroyed)deathArt=artwork;});}
        catch(Exception error){handler.post(()->preparingDeath=false);android.util.Log.e("RE4Continue","Original artwork is missing",error);}
      });return;
    }
    if(deathFlow==null)deathFlow=new ContinueFlow();
    deathDialog=new ContinueDialog(this,deathArt,picture,deathFlow,retry->{
      if(destroyed)return;
      deathDialog=null;clearInput();
      if(!retry){startActivity(new Intent(this,StartupActivity.class));finish();return;}
      retrying=true;rendering=true;sceneLoad.focus(true);
      showLoading("Checkpoint");updatePresentation();
      if(activityResumed)resumeSurface();
      surface.queueEvent(()->{
        boolean accepted=NativeBridge.continueGame();
        handler.post(()->{if(!accepted){retrying=false;deathFlow=null;hideLoading();android.util.Log.e("RE4Continue","Could not load checkpoint");}});
      });
    });
    deathDialog.show();sceneLoad.focus(true);updatePresentation();
  }
  private void pollPickup() {
    if (!roomScene || !rendering || !activityResumed || destroyed || !sceneLoad.ready() || roomLoading || openingSubscreen || pickupDialog != null || inventoryDialog != null || saveDialog != null || preparingSave) return;
    String snapshot = NativeBridge.pendingPickup();
    if (snapshot.isEmpty()) { pickupDialogToken = 0; return; }
    try {
      JSONObject item = new JSONObject(snapshot);
      final long token = item.getLong("token");
      if (token <= 0 || token == pickupDialogToken) return;
      pickupDialogToken = token;
      long money = item.getLong("money");
      long count = money > 0 ? money : item.getInt("count");
      int itemId = item.optInt("id", -1); pickupItemId = itemId;
      clearInput();
      surface.queueEvent(() -> { NativeBridge.setSubscreenOpen(true); NativeBridge.subscreenItem(itemId, false); });
      NativeBridge.subscreenAudio(true);
      GameAudio.setActive(activityResumed && roomScene);
      pickupDialog = new PickupDialog(this,itemId,count,item.getBoolean("fits"),item.getInt("message"),item.getInt("choices"),
          choice -> {
            NativeBridge.menuSound(choice==2 ? 5 : 4);
            surface.queueEvent(() -> NativeBridge.answerPickupChoice(token,choice));
          }, this::orderPickupCase);
      pickupDialog.setOnDismissListener(d -> {
        pickupDialog = null; orderingPickup = restorePickupOnResume = false; pickupItemId = -1; clearInput();
        surface.queueEvent(() -> { NativeBridge.subscreenItem(-1, false); NativeBridge.setSubscreenOpen(false); });
        NativeBridge.subscreenAudio(false);
        GameAudio.setActive(!destroyed && activityResumed && rendering && roomScene);
      });
      pickupDialog.show();
    } catch (JSONException e) { android.util.Log.e("RE4Items", "Could not read pickup dialog", e); }
  }
  private void resumeSurface() {
    android.content.SharedPreferences options = getSharedPreferences("game-options", MODE_PRIVATE);
    GameAudio.setVolume(options.getInt("volume", 100) / 100f);
    GameAudio.setOutputMode(options.getInt("soundMode", 1));
    if (!surfaceRunning) {
      surfaceRunning = true;
      // GLSurfaceView's lifecycle methods can wait on its GL thread. Keep
      // those waits off main while scene archives or GPU resources are loaded.
      surfaceWorker.execute(surface::onResume);
    }
    GameAudio.setActive(roomScene && sceneLoad.ready() && !roomLoading);
  }
  private void pauseSurface() {
    GameAudio.setActive(false);
    if (surfaceRunning) {
      surfaceRunning = false;
      surfaceWorker.execute(surface::onPause);
    }
  }
  @Override
  public void onBackPressed() {
    if (!sceneLoad.ready() || roomLoading) { cancelRoomLoad(); return; }
    if(rendering && roomScene && (NativeBridge.messageActive() || NativeBridge.binocularState()[0]!=0)) {
      surface.queueEvent(() -> {
        NativeBridge.input(1 << 1,0,0,0,0,0,0);
        NativeBridge.input(0,0,0,0,0,0,0);
      });
      return;
    }
    if (rendering)
      showMenu();
    else
      finish();
  }
  private boolean skipEvent() {
    if (!rendering || !roomScene || !NativeBridge.eventActive()) return false;
    clearInput();
    surface.queueEvent(() -> {
      if (!NativeBridge.eventActive()) return;
      // InputBuffer latches even a complete press/release between render frames.
      NativeBridge.input(1 << 6, 0, 0, 0, 0, 0, 0);
      NativeBridge.input(0, 0, 0, 0, 0, 0, 0);
    });
    return true;
  }
  private void pollRoomRequest() {
    if (destroyed || surface == null)
      return;
    String request = NativeBridge.pendingRoomRequest();
    if (request.isEmpty()) {
      loadingToken = 0;
      if (roomLoading) {
        roomLoading = false;
        if (exitAfterLoading) { finish(); return; }
        hideLoading();
        if(retrying && NativeBridge.deathRequested()){retrying=false;deathFlow=null;}
        GameAudio.setActive(activityResumed && roomScene && rendering);
      }
      return;
    }
    String[] fields = request.split("\t", -1);
    if (fields.length != 2)
      return;
    final long token;
    try {
      token = Long.parseLong(fields[0]);
    } catch (NumberFormatException e) {
      return;
    }
    if (token <= 0 || loadingToken == token)
      return;
    loadingToken = token;
    roomLoading = true;
    GameAudio.setActive(false);
    clearInput(); showLoading(fields[1]);
    roomWorker.execute(() -> {
      String path = "", error = "";
      try {
        if (roomRepository == null)
          throw new IOException("Verified room catalog is missing");
        path = roomRepository.prepare(fields[1]).getAbsolutePath();
      } catch (IOException | RuntimeException e) {
        error = e.getMessage() == null ? "Could not load room"
                                       : e.getMessage();
        android.util.Log.w("RE4Flow", "Loading " + fields[1] + " canceled",
                           e);
      }
      final String preparedPath = path, failure = error;
      handler.post(() -> {
        if (destroyed || exitAfterLoading) return;
        // The mailbox call only publishes a CPU result. Serial main dispatch
        // prevents a late worker success from overwriting Cancel's failure.
        // draw() owns the later GPU commit, including after a resumed Activity.
        NativeBridge.finishRoomRequest(token, preparedPath, failure);
      });
    });
  }
  @Override
  protected void onDestroy() {
    destroyed = true;
    if(inventoryWarmup!=null)inventoryWarmup.close();
    GameMenuArt.resetCache();
    sceneLoad.resume(false); sceneLoad.focus(false);
    GameAudio.stop();
    if (inventoryDialog != null) inventoryDialog.abandon();
    if (cheatDialog != null) cheatDialog.dismiss();
    if (pickupDialog != null) pickupDialog.abandon();
    if (saveDialog != null) saveDialog.dismiss();
    if (deathDialog != null) deathDialog.dismiss();
    handler.removeCallbacks(refresh);
    roomWorker.shutdownNow();
    surfaceWorker.shutdown();
    super.onDestroy();
  }
  @Override
  protected void onSaveInstanceState(Bundle state) {
    state.putLong("sceneSession", sceneSession);
    state.putBoolean("rendering", rendering);
    state.putBoolean("diagnostics", diagnostics);
    if(deathFlow!=null) {
      state.putString("deathPhase",deathFlow.phase.name());state.putBoolean("deathYes",deathFlow.yes);
      state.putInt("deathFrame",deathFlow.deadFrame);state.putInt("deathChoice",deathFlow.choiceFrame);
      state.putInt("deathCursor",deathFlow.cursorFrame);state.putInt("deathExit",deathFlow.exitFrame);
      state.putInt("deathGate",deathFlow.gate);state.putInt("deathRemaining",deathFlow.remaining);
    }
    super.onSaveInstanceState(state);
  }
  private static String checkedActor(File file, long size, String expected)
      throws IOException {
    if (file.length() != size)
      throw new IOException(file.getName() + ": Size mismatch");
    try {
      MessageDigest hash = MessageDigest.getInstance("SHA-256");
      byte[] bytes = new byte[16384];
      try (FileInputStream in = new FileInputStream(file)) {
        int n;
        while ((n = in.read(bytes)) != -1)
          hash.update(bytes, 0, n);
      }
      StringBuilder hex = new StringBuilder();
      for (byte b : hash.digest())
        hex.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
      if (!hex.toString().equals(expected))
        throw new IOException(file.getName() + ": SHA-256 mismatch");
      return file.getCanonicalPath();
    } catch (NoSuchAlgorithmException e) {
      throw new IOException("SHA-256 is missing", e);
    }
  }
  @Override
  protected void onResume() {
    super.onResume();
    activityResumed = true;
    if(gyroscope!=null)sensors.registerListener(this,gyroscope,android.hardware.SensorManager.SENSOR_DELAY_GAME,handler);
    if(deathDialog!=null)deathDialog.active(true);
    sceneLoad.resume(true);
    if (inventoryDialog != null || pickupDialog != null || saveDialog != null || deathDialog != null || cheatDialog != null) sceneLoad.focus(true);
    clearInput();
    if (!sceneLoad.ready() || rendering || inventoryDialog != null || pickupDialog != null || saveDialog != null || openingSubscreen)
      resumeSurface();
    if (pickupDialog != null && inventoryDialog == null && !openingSubscreen && (orderingPickup || restorePickupOnResume)) restorePickupScreen();
    devices.registerInputDeviceListener(this, handler);
    handler.post(refresh);
  }
  @Override
  protected void onPause() {
    activityResumed = false;
    if(sensors!=null)sensors.unregisterListener(this);
    if(deathDialog!=null)deathDialog.active(false);
    sceneLoad.resume(false);
    handler.removeCallbacks(refresh);
    devices.unregisterInputDeviceListener(this);
    clearInput();
    pauseSurface();
    super.onPause();
  }
  @Override
  public void onWindowFocusChanged(boolean focus) {
    super.onWindowFocusChanged(focus);
    sceneLoad.focus(focus || (activityResumed && (inventoryDialog != null || pickupDialog != null || saveDialog != null || deathDialog != null || cheatDialog != null)));
    if (!focus)
      clearInput();
    else
      updatePresentation();
  }
  private void clearInput() {
    buttons = hats = 0;
    lx = ly = rx = ry = lt = rt = 0;
    activeDevice = -1;
    policy.reset();
    gyroYaw=gyroPitch=0;NativeBridge.gyroRate(0,0);
    NativeBridge.resetInput();
  }
  private static boolean controller(InputEvent e) {
    return (e.getSource() & InputDevice.SOURCE_GAMEPAD) ==
        InputDevice.SOURCE_GAMEPAD ||
        (e.getSource() & InputDevice.SOURCE_JOYSTICK) ==
            InputDevice.SOURCE_JOYSTICK;
  }
  private static boolean sticks(InputDevice d) {
    return d != null && d.getMotionRange(MotionEvent.AXIS_X) != null &&
        d.getMotionRange(MotionEvent.AXIS_Y) != null;
  }
  private boolean physicalSticksAvailable() {
    for (int id : InputDevice.getDeviceIds()) {
      InputDevice d = InputDevice.getDevice(id);
      if (d != null && !d.isVirtual() && sticks(d) &&
          (d.getSources() & InputDevice.SOURCE_JOYSTICK) ==
              InputDevice.SOURCE_JOYSTICK)
        return true;
    }
    return false;
  }
  private boolean select(InputEvent e) {
    InputDevice d = e.getDevice();
    if (!controller(e) || d == null)
      return false;
    if (!policy.accept(e.getDeviceId(),
                       ControllerPolicy.rank(d.isVirtual(), sticks(d)),
                       physicalSticksAvailable()))
      return false;
    if (activeDevice != policy.active()) {
      // A device with actual sticks can replace a weaker navigation device.
      buttons = hats = 0;
      lx = ly = rx = ry = lt = rt = 0;
      NativeBridge.resetInput();
      activeDevice = policy.active();
    }
    return true;
  }
  private int bit(int code) {
    switch (code) {
    case KeyEvent.KEYCODE_BUTTON_A:
      return 1 << (swapFace ? 1 : 0);
    case KeyEvent.KEYCODE_BUTTON_B:
      return 1 << (swapFace ? 0 : 1);
    case KeyEvent.KEYCODE_BUTTON_X:
      return 1 << (swapFace ? 3 : 2);
    case KeyEvent.KEYCODE_BUTTON_Y:
      return 1 << (swapFace ? 2 : 3);
    case KeyEvent.KEYCODE_BUTTON_L1:
      return 1 << 4;
    case KeyEvent.KEYCODE_BUTTON_R1:
      return 1 << 5;
    case KeyEvent.KEYCODE_BUTTON_START:
      return 1 << 6;
    case KeyEvent.KEYCODE_BUTTON_SELECT:
      return 1 << 7;
    case KeyEvent.KEYCODE_DPAD_UP:
      return 1 << 8;
    case KeyEvent.KEYCODE_DPAD_DOWN:
      return 1 << 9;
    case KeyEvent.KEYCODE_DPAD_LEFT:
      return 1 << 10;
    case KeyEvent.KEYCODE_DPAD_RIGHT:
      return 1 << 11;
    case KeyEvent.KEYCODE_BUTTON_L2:
      return 1 << 12;
    case KeyEvent.KEYCODE_BUTTON_R2:
      return 1 << 13;
    default:
      return 0;
    }
  }
  private void send() {
    NativeBridge.input(buttons | hats, lx, ly, rx, ry, lt, rt);
  }
  @Override
  public boolean dispatchKeyEvent(KeyEvent e) {
    if ((!sceneLoad.ready() || roomLoading) && controller(e)) {
      if (e.getAction() == KeyEvent.ACTION_DOWN && e.getRepeatCount() == 0 &&
          (e.getKeyCode() == KeyEvent.KEYCODE_BUTTON_B || e.getKeyCode() == KeyEvent.KEYCODE_BACK)) cancelRoomLoad();
      return true;
    }
    if (openingSubscreen && controller(e)) return true;
    if (!rendering && controller(e) && (e.getKeyCode() == KeyEvent.KEYCODE_BUTTON_B || e.getKeyCode() == KeyEvent.KEYCODE_BUTTON_START)) {
      if (e.getAction() == KeyEvent.ACTION_DOWN && e.getRepeatCount() == 0) showRender();
      return true;
    }
    if (!rendering && controller(e) &&
        e.getKeyCode() == KeyEvent.KEYCODE_BUTTON_A) {
      // Menu confirmation is ordinary Android focus navigation. Never submit
      // this edge as a gameplay action when Continue resumes the surface.
      return super.dispatchKeyEvent(
          new KeyEvent(e.getDownTime(), e.getEventTime(), e.getAction(),
                       KeyEvent.KEYCODE_DPAD_CENTER, e.getRepeatCount(),
                       e.getMetaState(), e.getDeviceId(), e.getScanCode(),
                       e.getFlags(), InputDevice.SOURCE_DPAD));
    }
    int b = bit(e.getKeyCode());
    if (rendering && b != 0 && controller(e)) {
      if (!select(e))
        return true;
      if (roomScene && b == (1 << 3)) {
        // Use the logical mapping so swapped face-button layouts still open
        // inventory with their mapped Inventory action. Consume repeat/up
        // edges instead of forwarding them to the SAT diagnostic toggle.
        if (e.getAction() == KeyEvent.ACTION_DOWN && e.getRepeatCount() == 0)
          showInventory(true);
        return true;
      }
      if (roomScene && b == (1 << 6)) {
        if (NativeBridge.eventActive() || (buttons & b) != 0) {
          if (e.getAction() == KeyEvent.ACTION_DOWN) buttons |= b;
          else if (e.getAction() == KeyEvent.ACTION_UP) buttons &= ~b;
          send();
          return true;
        }
        if (e.getAction() == KeyEvent.ACTION_DOWN && e.getRepeatCount() == 0) showInventory(true, InventoryDialog.CASE);
        return true;
      }
      if (roomScene && b == (1 << 7)) {
        if (e.getAction() == KeyEvent.ACTION_DOWN && e.getRepeatCount() == 0) showCheats();
        return true;
      }
      if (e.getAction() == KeyEvent.ACTION_DOWN)
        buttons |= b;
      else if (e.getAction() == KeyEvent.ACTION_UP)
        buttons &= ~b;
      send();
      return true;
    }
    return super.dispatchKeyEvent(e);
  }
  private float axis(MotionEvent e, int a, int history, boolean trigger) {
    InputDevice d = e.getDevice();
    if (d == null)
      return 0;
    InputDevice.MotionRange range = d.getMotionRange(a, e.getSource());
    if (range == null || range.getRange() <= 0)
      return 0;
    float v =
        history < 0 ? e.getAxisValue(a) : e.getHistoricalAxisValue(a, history);
    if (Float.isNaN(v) || Float.isInfinite(v))
      return 0;
    return trigger
        ? Math.max(0, Math.min(1, (v - range.getMin()) / range.getRange()))
        : Math.max(
              -1, Math.min(1, 2 * (v - range.getMin()) / range.getRange() - 1));
  }
  private void motion(MotionEvent e, int h) {
    InputDevice d = e.getDevice();
    lx = axis(e, MotionEvent.AXIS_X, h, false);
    ly = -axis(e, MotionEvent.AXIS_Y, h, false);
    boolean zr = d != null &&
                 d.getMotionRange(MotionEvent.AXIS_Z, e.getSource()) != null &&
                 d.getMotionRange(MotionEvent.AXIS_RZ, e.getSource()) != null;
    rx = axis(e, zr ? MotionEvent.AXIS_Z : MotionEvent.AXIS_RX, h, false);
    ry = -axis(e, zr ? MotionEvent.AXIS_RZ : MotionEvent.AXIS_RY, h, false);
    lt = Math.max(axis(e, MotionEvent.AXIS_LTRIGGER, h, true),
                  axis(e, MotionEvent.AXIS_BRAKE, h, true));
    rt = Math.max(axis(e, MotionEvent.AXIS_RTRIGGER, h, true),
                  axis(e, MotionEvent.AXIS_GAS, h, true));
    float x = e.getAxisValue(MotionEvent.AXIS_HAT_X),
          y = e.getAxisValue(MotionEvent.AXIS_HAT_Y);
    hats = 0;
    if (y < -0.5f)
      hats |= 1 << 8;
    if (y > 0.5f)
      hats |= 1 << 9;
    if (x < -0.5f)
      hats |= 1 << 10;
    if (x > 0.5f)
      hats |= 1 << 11;
    send();
  }
  @Override
  public boolean onGenericMotionEvent(MotionEvent e) {
    if ((!sceneLoad.ready() || roomLoading) && controller(e)) return true;
    if (openingSubscreen && controller(e)) return true;
    if (rendering && e.getAction() == MotionEvent.ACTION_MOVE &&
        controller(e)) {
      if (!select(e))
        return true;
      motion(e, -1);
      return true;
    }
    return super.onGenericMotionEvent(e);
  }
  @Override
  public void onInputDeviceAdded(int id) {}
  @Override
  public void onInputDeviceChanged(int id) {
    if (id == activeDevice)
      clearInput();
  }
  @Override
  public void onInputDeviceRemoved(int id) {
    if (id == activeDevice)
      clearInput();
  }
}
