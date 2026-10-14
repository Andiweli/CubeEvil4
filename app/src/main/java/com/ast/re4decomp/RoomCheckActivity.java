package com.ast.re4decomp;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.WindowManager;
import android.widget.*;
import java.io.*;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Real native room decode, checked against original PowerPC output
 * fingerprints.
 */
public final class RoomCheckActivity extends Activity {
  private final ExecutorService worker = Executors.newSingleThreadExecutor();
  private final Handler main = new Handler(Looper.getMainLooper());
  private volatile boolean destroyed;
  private TextView report;
  private Button start, preview, scene, scene100;
  private File sceneFile, scene100File;
  private Spinner roomPicker;
  private Button loadRoom, openRoom;
  private final List<String[]> roomCatalog = new ArrayList<>();
  private File selectedRoomFile;
  private String loadedRoom;
  private int loadedCatalogIndex = -1;
  private File previewFile;
  @Override
  public void onCreate(Bundle state) {
    super.onCreate(state);
    LinearLayout body = new LinearLayout(this);
    body.setOrientation(LinearLayout.VERTICAL);
    int pad = (int)(20 * getResources().getDisplayMetrics().density);
    body.setPadding(pad, pad, pad, pad);
    TextView title = new TextView(this);
    title.setText(R.string.room_test);
    title.setTextSize(24);
    body.addView(title);
    TextView note = new TextView(this);
    note.setText(R.string.room_note);
    body.addView(note);
    start = new Button(this);
    start.setText(R.string.room_start);
    body.addView(start);
    previewFile = new File(getCacheDir(), "room-preview-r100.tpl");
    preview = new Button(this);
    preview.setText(R.string.room_preview);
    preview.setEnabled(false);
    body.addView(preview);
    preview.setOnClickListener(
        v
        -> startActivity(
            new Intent(this, ControllerActivity.class)
                .putExtra("previewTpl", previewFile.getAbsolutePath())
                .putExtra("previewIndex", 6)));
    sceneFile = new File(getCacheDir(), "room-preview-r300.dat");
    scene = new Button(this);
    scene.setText(R.string.scene_preview);
    scene.setEnabled(false);
    body.addView(scene);
    scene.setOnClickListener(
        v
        -> startActivity(
            new Intent(this, ControllerActivity.class)
                .putExtra("previewScene", sceneFile.getAbsolutePath())));
    scene100File = new File(getCacheDir(), "room-preview-r100.dat");
    scene100 = new Button(this);
    scene100.setText(R.string.scene_preview_r100);
    scene100.setEnabled(false);
    body.addView(scene100);
    scene100.setOnClickListener(v -> startActivity(
        new Intent(this, ControllerActivity.class)
            .putExtra("previewScene", scene100File.getAbsolutePath())
            .putExtra("sceneRoom", "r100")));
    report = new TextView(this);
    report.setTextIsSelectable(true);
    body.addView(report);
    TextView roomLabel = new TextView(this);
    roomLabel.setText(R.string.room_choose);
    body.addView(roomLabel);
    roomPicker = new Spinner(this);
    body.addView(roomPicker);
    loadRoom = new Button(this);
    loadRoom.setText(R.string.room_load_selected);
    body.addView(loadRoom);
    openRoom = new Button(this);
    openRoom.setText(R.string.room_open_selected);
    openRoom.setEnabled(false);
    body.addView(openRoom);
    selectedRoomFile = new File(getCacheDir(), "selected-room.dat");
    List<String> labels = new ArrayList<>();
    try (BufferedReader refs = new BufferedReader(new InputStreamReader(getAssets().open("rooms.tsv"), "UTF-8"))) {
      String line;
      while ((line = refs.readLine()) != null) {
        if (line.startsWith("#")) continue;
        String[] row = line.split("\t");
        if (row.length != 5) throw new IOException("Invalid room catalog");
        roomCatalog.add(row);
        labels.add("Disc " + row[0] + " · " + row[1]);
      }
      ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, labels);
      adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
      roomPicker.setAdapter(adapter);
      loadRoom.setEnabled(!roomCatalog.isEmpty());
    } catch (IOException e) { loadRoom.setEnabled(false); report.setText(e.getMessage()); }
    roomPicker.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
      @Override public void onItemSelected(AdapterView<?> parent, android.view.View view, int position, long id) {
        openRoom.setEnabled(position == loadedCatalogIndex && selectedRoomFile.isFile());
      }
      @Override public void onNothingSelected(AdapterView<?> parent) { openRoom.setEnabled(false); }
    });
    loadRoom.setOnClickListener(v -> loadSelectedRoom());
    openRoom.setOnClickListener(v -> startActivity(new Intent(this, ControllerActivity.class)
        .putExtra("previewScene", selectedRoomFile.getAbsolutePath()).putExtra("sceneRoom", loadedRoom)));
    ScrollView scroll = new ScrollView(this);
    scroll.addView(body);
    setContentView(scroll);
    findViewById(android.R.id.content)
        .setOnApplyWindowInsetsListener((v, i) -> {
          v.setPadding(
              i.getSystemWindowInsetLeft(), i.getSystemWindowInsetTop(),
              i.getSystemWindowInsetRight(), i.getSystemWindowInsetBottom());
          return i;
        });
    start.setOnClickListener(v -> runCheck());
  }
  private static String sha256(File file) throws Exception {
    MessageDigest digest = MessageDigest.getInstance("SHA-256");
    byte[] bytes = new byte[65536];
    try (InputStream in = new FileInputStream(file)) {
      int count;
      while ((count = in.read(bytes)) != -1)
        digest.update(bytes, 0, count);
    }
    StringBuilder hex = new StringBuilder();
    for (byte b : digest.digest()) {
      hex.append(Character.forDigit((b & 255) >> 4, 16));
      hex.append(Character.forDigit(b & 15, 16));
    }
    return hex.toString();
  }
  private RoomRepository repository(File root) throws IOException {
    try (Reader rooms = new InputStreamReader(getAssets().open("rooms.tsv"), "UTF-8");
         Reader blocks = new InputStreamReader(getAssets().open("blocks.tsv"), "UTF-8")) {
      return new RoomRepository(root, getCacheDir(), rooms, blocks,
          (source, output) -> NativeBridge.decodeRoom(source.getAbsolutePath(),
                                                     output.getAbsolutePath()));
    }
  }
  private void runCheck() {
    start.setEnabled(false);
    loadRoom.setEnabled(false);
    roomPicker.setEnabled(false);
    openRoom.setEnabled(false);
    preview.setEnabled(false);
    scene.setEnabled(false);
    scene100.setEnabled(false);
    report.setText(R.string.room_busy);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    // Application-owned root; never modify or overwrite installed disc data.
    File root = ((Re4Application)getApplication()).imports.root;
    worker.execute(() -> {
      StringBuilder text = new StringBuilder();
      StringBuilder details = new StringBuilder();
      boolean previewReady = false, sceneReady = false, scene100Ready = false;
      try (BufferedReader refs = new BufferedReader(
               new InputStreamReader(getAssets().open("rooms.tsv"), "UTF-8"))) {
        String line;
        while ((line = refs.readLine()) != null) {
          if (Thread.currentThread().isInterrupted())
            break;
          if (line.startsWith("#"))
            continue;
          String[] f = line.split("\t");
          if (f.length != 5)
            throw new IOException("Invalid room verification catalog");
          if (!((f[0].equals("1") && f[1].equals("St1/r100.das")) ||
                (f[0].equals("2") && f[1].equals("St3/r300.das"))))
            continue;
          text.append("Disc ").append(f[0]).append(" · ").append(f[1]).append(
              '\n');
          File temp = null;
          try {
            if (root == null)
              throw new IOException("App storage is unavailable");
            File source = new File(root, "disc" + f[0] + "/files/" + f[1]);
            if (!source.isFile())
              throw new IOException(
                  "File missing. Please import this disc.");
            if (!sha256(source).equals(f[4]))
              throw new IOException(
                  "Source file does not match the verified disc.");
            temp = File.createTempFile("yz2-", ".dat", getCacheDir());
            long time = android.os.SystemClock.elapsedRealtime();
            String info = NativeBridge.decodeRoom(source.getAbsolutePath(),
                                                  temp.getAbsolutePath());
            long ms = android.os.SystemClock.elapsedRealtime() - time;
            String actual = sha256(temp);
            if (temp.length() != Long.parseLong(f[2]) || !actual.equals(f[3]))
              throw new IOException("Output differs from the original PowerPC decoder: " +
                                    actual);
            String archiveInfo = NativeBridge.inspectArchive(
                temp.getAbsolutePath(),
                f[0].equals("1") ? previewFile.getAbsolutePath() : "", 6);
            if (f[0].equals("2")) {
              cacheScene(temp, sceneFile);
              sceneReady = true;
            }
            if (f[0].equals("1")) {
              // Direct r100 opens need the same verified sibling block
              // bundle as a gameplay room transition. A preview-only
              // filename would otherwise hide those dependencies from JNI.
              scene100File = repository(root).prepare("r100");
              previewReady = true;
              scene100Ready = true;
            }
            Log.i("RE4Archive", "PASS " + f[1] + " SHA256=" + actual);
            for (String row : archiveInfo.split("\n"))
              Log.i("RE4Archive", row);
            text.append("PASS · ")
                .append(info)
                .append('\n')
                .append(ms)
                .append(" ms including file access\nSHA-256: ")
                .append(actual)
                .append("\nOriginal comparison passed.\n")
                .append(archiveInfo.split("\n", 2)[0])
                .append("\n\n");
            details.append("Disc ")
                .append(f[0])
                .append(" · ")
                .append(f[1])
                .append("\n")
                .append(archiveInfo)
                .append("\n");
          } catch (Exception e) {
            Log.e("RE4Archive", "ERROR " + f[1], e);
            text.append("ERROR · ").append(e.getMessage()).append("\n\n");
          } finally {
            if (temp != null && !temp.delete())
              temp.deleteOnExit();
          }
        }
      } catch (Exception e) {
        text.append("Verification failed: ").append(e.getMessage());
      }
      String result =
          text.append("\nArchive details\n").append(details).toString();
      final boolean ready = previewReady;
      final boolean sceneOk = sceneReady;
      final boolean scene100Ok = scene100Ready;
      main.post(() -> {
        if (destroyed)
          return;
        report.setText(result);
        start.setEnabled(true);
        loadRoom.setEnabled(!roomCatalog.isEmpty());
        roomPicker.setEnabled(true);
        preview.setEnabled(ready && previewFile.isFile());
        scene.setEnabled(sceneOk && sceneFile.isFile());
        scene100.setEnabled(scene100Ok && scene100File.isFile());
        openRoom.setEnabled(loadedCatalogIndex == roomPicker.getSelectedItemPosition() && loadedCatalogIndex >= 0 && selectedRoomFile.isFile());
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
      });
    });
  }
  private void loadSelectedRoom() {
    int selected = roomPicker.getSelectedItemPosition();
    if (selected < 0 || selected >= roomCatalog.size()) return;
    String[] row = roomCatalog.get(selected).clone();
    final boolean oldPreview = preview.isEnabled(), oldScene = scene.isEnabled(), oldScene100 = scene100.isEnabled();
    start.setEnabled(false); loadRoom.setEnabled(false); roomPicker.setEnabled(false);
    openRoom.setEnabled(false); preview.setEnabled(false); scene.setEnabled(false); scene100.setEnabled(false);
    report.setText(R.string.room_selected_loading);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    File root = ((Re4Application)getApplication()).imports.root;
    worker.execute(() -> {
      File prepared = null; boolean success = false; String message;
      try {
        if (root == null) throw new IOException("App storage is unavailable");
        String roomId = row[1].substring(row[1].lastIndexOf('/') + 1).replace(".das", "");
        prepared = repository(root).prepare(roomId);
        if (Thread.currentThread().isInterrupted()) throw new IOException("Loading canceled");
        String details = NativeBridge.inspectScene(prepared.getAbsolutePath());
        message = row[1] + "\n" + details;
        success = true;
        Log.i("RE4Room", "PASS " + row[1] + " SHA256=" + row[3] + "\n" + details);
      } catch (Exception e) {
        message = row[1] + "\nRoom view not available yet: " + e.getMessage();
        Log.i("RE4Room", message);
      }
      final boolean ready = success;
      final String result = message;
      final File resultFile = prepared;
      main.post(() -> {
        if (destroyed) return;
        if (ready) selectedRoomFile = resultFile;
        report.setText(result);
        loadedCatalogIndex = ready ? selected : -1;
        preview.setEnabled(oldPreview); scene.setEnabled(oldScene); scene100.setEnabled(oldScene100);
        loadedRoom = ready ? row[1].substring(row[1].lastIndexOf('/')+1).replace(".das", "") : null;
        start.setEnabled(true); loadRoom.setEnabled(true); roomPicker.setEnabled(true);
        openRoom.setEnabled(ready && selectedRoomFile.isFile());
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
      });
    });
  }
  private void cacheScene(File decoded, File target) throws IOException {
    File stage = File.createTempFile("scene-stage-", ".dat", getCacheDir());
    try {
      try (InputStream in = new FileInputStream(decoded);
           OutputStream out = new FileOutputStream(stage)) {
        byte[] buffer = new byte[65536];
        int n;
        while ((n = in.read(buffer)) != -1) {
          if (Thread.currentThread().isInterrupted())
            throw new IOException("Verification canceled");
          out.write(buffer, 0, n);
        }
      }
      if (destroyed || Thread.currentThread().isInterrupted())
        throw new IOException("Loading canceled");
      if (!stage.renameTo(target))
        throw new IOException("Could not save 3D preview");
    } finally {
      if (stage.exists() && !stage.delete())
        stage.deleteOnExit();
    }
  }
  @Override
  protected void onDestroy() {
    destroyed = true;
    worker.shutdownNow();
    main.removeCallbacksAndMessages(null);
    super.onDestroy();
  }
}
