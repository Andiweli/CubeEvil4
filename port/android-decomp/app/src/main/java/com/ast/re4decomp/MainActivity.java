package com.ast.re4decomp;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.*;
import android.widget.*;
import com.ast.re4decomp.disc.DiscInstaller;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;

/** One action prepares the verified room bundle and opens gameplay. */
public final class MainActivity extends Activity implements ImportManager.Listener {
  private final ExecutorService worker = Executors.newSingleThreadExecutor();
  private final Handler main = new Handler(Looper.getMainLooper());
  private volatile boolean destroyed;
  private boolean loading, startupShown;
  /** Core flavor: the original game core runs the game (CoreActivity). */
  private boolean core;
  private ImportManager imports;
  private TextView status, discs, roomNote;
  private Button disc1, disc2, cancel, open;
  private LinearLayout importPanel, roomBrowser;
  private Spinner category, rooms, mode;
  private RoomCatalog catalog;
  private List<RoomCatalog.Entry> shown = Collections.emptyList();
  @Override public void onCreate(Bundle state) {
    super.onCreate(state); startupShown=state!=null&&state.getBoolean("startupShown",false); imports = ((Re4Application)getApplication()).imports;
    core = getResources().getBoolean(R.bool.core_mode);
    LinearLayout body = new LinearLayout(this); body.setOrientation(LinearLayout.VERTICAL);
    int p = dp(20); body.setPadding(p, p, p, p);
    TextView title = new TextView(this); title.setText(R.string.app_title); title.setTextSize(25); body.addView(title);
    TextView note = new TextView(this); note.setText(core ? R.string.core_notice : R.string.browser_notice);
    note.setPadding(0, dp(8), 0, dp(12)); body.addView(note);
    button(body,"Start game",this::startOriginalTitle);
    if (core) {
      // the controller of the core mode (CoreActivity, CoreInput): the layout and its one switch
      TextView controls = new TextView(this); controls.setText(R.string.core_controls); controls.setPadding(0, dp(8), 0, 0); body.addView(controls);
      final android.content.SharedPreferences input = getSharedPreferences(CoreActivity.INPUT_PREFS, MODE_PRIVATE);
      CheckBox swap = new CheckBox(this); swap.setText(R.string.core_swap_face);
      swap.setChecked(input.getBoolean(CoreActivity.SWAP_FACE, false));
      swap.setOnCheckedChangeListener((v, on) -> input.edit().putBoolean(CoreActivity.SWAP_FACE, on).apply());
      body.addView(swap);
    }
    // The room browser opens the app's own game modules; the core mode has none.
    Button browse = button(body, getString(R.string.launch_rooms), () -> {
      roomBrowser.setVisibility(roomBrowser.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE);
      if (roomBrowser.getVisibility() == View.VISIBLE) rooms.requestFocus();
    });
    if (core) browse.setVisibility(View.GONE);
    roomBrowser = new LinearLayout(this); roomBrowser.setOrientation(LinearLayout.VERTICAL); roomBrowser.setVisibility(View.GONE); body.addView(roomBrowser);
    category = spinner(roomBrowser, Arrays.asList(getString(R.string.category_campaign), getString(R.string.category_extras), getString(R.string.category_all)));
    rooms = spinner(roomBrowser, Collections.emptyList()); mode = spinner(roomBrowser, Collections.singletonList(getString(R.string.mode_explore)));
    roomNote = new TextView(this); roomNote.setPadding(0, dp(8), 0, dp(8)); roomBrowser.addView(roomNote);
    open = button(roomBrowser, getString(R.string.room_open_selected), this::openSelected);
    status = new TextView(this); status.setPadding(0, dp(8), 0, dp(8)); body.addView(status);
    button(body, getString(R.string.browser_menu), this::showMenu);
    importPanel = new LinearLayout(this); importPanel.setOrientation(LinearLayout.VERTICAL); importPanel.setVisibility(View.GONE); body.addView(importPanel);
    discs = new TextView(this); importPanel.addView(discs);
    disc1 = button(importPanel, getString(R.string.import_disc1), () -> choose(1));
    disc2 = button(importPanel, getString(R.string.import_disc2), () -> choose(2));
    cancel = button(importPanel, getString(R.string.cancel_import), imports::cancel);
    try (Reader reader = new InputStreamReader(getAssets().open("rooms.tsv"), StandardCharsets.UTF_8)) { catalog = new RoomCatalog(reader); }
    catch (IOException e) { status.setText(e.getMessage()); }
    category.setOnItemSelectedListener(selected(this::fillRooms)); rooms.setOnItemSelectedListener(selected(this::roomChanged));
    mode.setOnItemSelectedListener(selected(this::updateOpenState)); fillRooms();
    ScrollView scroll = new ScrollView(this); scroll.addView(body); setContentView(scroll);
    findViewById(android.R.id.content).setOnApplyWindowInsetsListener((v, insets) -> {
      v.setPadding(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(), insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom()); return insets;
    });
  }
  private void startOriginalTitle(){if(!loading&&!imports.busy&&discReady(1)){startupShown=true;Intent start=new Intent(this,core?CoreActivity.class:StartupActivity.class);if(core)start.putExtra("coreSettings",getIntent().getStringExtra("coreSettings"));startActivity(start);}else importPanel.setVisibility(View.VISIBLE);}
  @Override protected void onSaveInstanceState(Bundle state){state.putBoolean("startupShown",startupShown);super.onSaveInstanceState(state);}
  private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
  private Button button(LinearLayout body, String label, Runnable action) {
    Button b = new Button(this); b.setText(label); b.setOnClickListener(v -> action.run()); body.addView(b); return b;
  }
  private Spinner spinner(LinearLayout body, List<String> labels) {
    Spinner s = new Spinner(this); body.addView(s); setChoices(s, labels); return s;
  }
  private void setChoices(Spinner spinner, List<String> labels) {
    ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, labels);
    adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item); spinner.setAdapter(adapter);
  }
  private AdapterView.OnItemSelectedListener selected(Runnable action) {
    return new AdapterView.OnItemSelectedListener() {
      @Override public void onItemSelected(AdapterView<?> p, View v, int position, long id) { action.run(); }
      @Override public void onNothingSelected(AdapterView<?> p) { updateOpenState(); }
    };
  }
  private RoomCatalog.Entry selectedRoom() {
    int index = rooms.getSelectedItemPosition(); return index >= 0 && index < shown.size() ? shown.get(index) : null;
  }
  private void fillRooms() {
    if (catalog == null) { updateOpenState(); return; }
    String last = getPreferences(MODE_PRIVATE).getString("room", "r100");
    shown = catalog.filter(Math.max(0, category.getSelectedItemPosition())); List<String> labels = new ArrayList<>(); int selected = 0;
    for (int i = 0; i < shown.size(); ++i) { labels.add(shown.get(i).label()); if (shown.get(i).id.equals(last)) selected = i; }
    setChoices(rooms, labels); rooms.setSelection(selected); roomChanged();
  }
  private void roomChanged() {
    RoomCatalog.Entry entry = selectedRoom();
    setChoices(mode, entry != null && entry.combatTest() ? Arrays.asList(getString(R.string.mode_explore), getString(R.string.mode_combat_test))
        : Collections.singletonList(getString(entry != null && !entry.walkable() && entry.explorable() ? R.string.mode_scene_only : R.string.mode_explore)));
    updateOpenState();
  }
  private boolean discReady(int number) {
    if (imports.root == null) return false;
    File disc = new File(imports.root, "disc" + number);
    // the core mounts the disc from sys/fst.bin: older imports must be repeated
    return core ? DiscInstaller.coreReady(disc, number) : DiscInstaller.ready(disc, number);
  }
  private void updateOpenState() {
    if (open == null) return;
    RoomCatalog.Entry entry = selectedRoom();
    boolean available = entry != null && entry.explorable() && discReady(1) && (discReady(entry.disc) ||
        (discReady(2) && new File(imports.root, "disc2/files/" + entry.path).isFile()));
    roomNote.setText(entry == null ? "" : !entry.explorable() ? getString(R.string.browser_room_unsupported)
        : !discReady(1) ? getString(R.string.browser_missing_disc, 1)
        : !available ? getString(R.string.browser_missing_disc, entry.disc)
        : !entry.walkable() ? getString(R.string.browser_room_dynamic_floor)
        : getString(R.string.browser_room_note, entry.area(), entry.id));
    open.setEnabled(available && !loading && !imports.busy); category.setEnabled(!loading && !imports.busy); rooms.setEnabled(!loading && !imports.busy);
    mode.setEnabled(!loading && !imports.busy && entry != null && entry.combatTest());
  }
  private void openSelected() {
    RoomCatalog.Entry entry = selectedRoom(); if (entry == null || !entry.explorable() || loading || imports.busy) return;
    openRoom(entry, mode.getSelectedItemPosition() != 1 || !entry.combatTest(), 0);
  }
  private void openTest(String id, boolean explore, int profile) {
    if (catalog == null || loading || imports.busy || !discReady(1)) return;
    for (RoomCatalog.Entry entry : catalog.filter(2)) if (id.equals(entry.id)) { openRoom(entry, explore, profile); return; }
    status.setText(getString(R.string.browser_load_error, "Test room is missing from the catalog"));
  }
  private void openRoom(RoomCatalog.Entry entry, final boolean explore, final int profile) {
    final File root = imports.root;
    loading = true; updateOpenState(); status.setText(getString(R.string.browser_loading, entry.id));
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON); getPreferences(MODE_PRIVATE).edit().putString("room", entry.id).apply();
    worker.execute(() -> {
      File file = null; String error = null;
      try (Reader rows = new InputStreamReader(getAssets().open("rooms.tsv"), StandardCharsets.UTF_8);
           Reader blocks = new InputStreamReader(getAssets().open("blocks.tsv"), StandardCharsets.UTF_8)) {
        RoomRepository repository = new RoomRepository(root, getCacheDir(), rows, blocks,
            (source, output) -> NativeBridge.decodeRoom(source.getAbsolutePath(), output.getAbsolutePath()));
        file = repository.prepare(entry.id);
        if (Thread.currentThread().isInterrupted()) throw new IOException("Loading canceled");
        NativeBridge.inspectScene(file.getAbsolutePath());
      } catch (Exception e) { error = e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage(); }
      final File prepared = file; final String failure = error;
      main.post(() -> {
        if (destroyed) return; loading = false; getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON); changed();
        if (failure != null || prepared == null) { status.setText(getString(R.string.browser_load_error, failure)); return; }
        status.setText(""); startActivity(new Intent(this, ControllerActivity.class).putExtra("previewScene", prepared.getAbsolutePath())
            .putExtra("sceneRoom", entry.id).putExtra("exploreMode", explore).putExtra("testProfile", profile));
      });
    });
  }
  private void showMenu() {
    if (core) {  // only the import: the diagnostics run the app's own modules
      importPanel.setVisibility(importPanel.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE);
      if (importPanel.getVisibility() == View.VISIBLE) disc1.requestFocus();
      return;
    }
    new AlertDialog.Builder(this).setTitle(R.string.browser_menu).setItems(new String[]{getString(R.string.browser_data), getString(R.string.browser_diagnostics)}, (dialog, which) -> {
      if (which == 0) { importPanel.setVisibility(importPanel.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE);
        if (importPanel.getVisibility() == View.VISIBLE) disc1.requestFocus(); }
      else new AlertDialog.Builder(this).setTitle(R.string.browser_diagnostics).setItems(new String[]{getString(R.string.open_test), getString(R.string.room_test)},
          (d, test) -> startActivity(new Intent(this, test == 0 ? ControllerActivity.class : RoomCheckActivity.class))).show();
    }).show();
  }
  private void choose(int disc) {
    if (loading) return; Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT); i.addCategory(Intent.CATEGORY_OPENABLE); i.setType("*/*");
    i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION); startActivityForResult(i, disc);
  }
  @Override protected void onActivityResult(int request, int result, Intent data) {
    super.onActivityResult(request, result, data);
    if ((request == 1 || request == 2) && result == RESULT_OK && data != null && data.getData() != null) imports.start(data.getData(), request);
  }
  @Override protected void onResume() { super.onResume(); imports.attach(this); }
  @Override protected void onPause() { imports.detach(this); super.onPause(); }
  @Override public void changed() {
    if (!loading) status.setText(imports.status);
    discs.setText(getString(R.string.disc_status, getString(discReady(1) ? R.string.ready : R.string.missing), getString(discReady(2) ? R.string.ready : R.string.missing)));
    disc1.setEnabled(imports.root != null && !imports.busy && !loading); disc2.setEnabled(imports.root != null && !imports.busy && !loading); cancel.setEnabled(imports.busy);
    if (!discReady(1) || imports.busy) importPanel.setVisibility(View.VISIBLE);
    if (imports.busy || loading) getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON); else getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    updateOpenState();
    if(!startupShown&&discReady(1)&&!imports.busy&&!loading)startOriginalTitle();
  }
  @Override public boolean dispatchKeyEvent(KeyEvent e) {
    if ((e.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD &&
        (e.getKeyCode() == KeyEvent.KEYCODE_BUTTON_A || e.getKeyCode() == KeyEvent.KEYCODE_BUTTON_B))
      return super.dispatchKeyEvent(new KeyEvent(e.getDownTime(), e.getEventTime(), e.getAction(),
          e.getKeyCode() == KeyEvent.KEYCODE_BUTTON_A ? KeyEvent.KEYCODE_DPAD_CENTER : KeyEvent.KEYCODE_BACK,
          e.getRepeatCount(), e.getMetaState(), e.getDeviceId(), e.getScanCode(), e.getFlags(), InputDevice.SOURCE_DPAD));
    return super.dispatchKeyEvent(e);
  }
  @Override protected void onDestroy() { destroyed = true; worker.shutdownNow(); main.removeCallbacksAndMessages(null); super.onDestroy(); }
}
