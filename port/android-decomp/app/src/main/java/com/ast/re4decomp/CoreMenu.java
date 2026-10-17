package com.ast.re4decomp;

import android.app.Dialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Typeface;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.TextView;
import java.util.Locale;

/**
 * The core mode's menu (M11; SELECT pressed, decision of the project owner,
 * 17.10.2026): picture options, the memory card's files as .gci (export for
 * Dolphin or a GameCube, import from there), the error log and the end of
 * the game. The game stands while it is open (CoreActivity pauses the core).
 * Run with the controller: D-pad between the entries, A chooses, B / START /
 * SELECT close it. German on a German device, English otherwise.
 */
final class CoreMenu extends Dialog {
  interface Host {
    /** Picture options changed (the preferences hold them). */
    void graphicsChanged();
    void exportFile(int index, String fileName);
    void exportAll();
    void importFiles();
    void shareReport();
    void saveReport();
    void logChanged(boolean on);
    void toolsChanged(boolean on);
    void quit();
  }

  static boolean german() {
    return "de".equals(Locale.getDefault().getLanguage());
  }

  static String t(String de, String en) {
    return german() ? de : en;
  }

  private final Host host;
  private final SharedPreferences graphics;
  private final boolean tools;
  private final LinearLayout body;
  private TextView graphicsLine, cardLine;
  private LinearLayout cardFiles;
  private Button exportAll;

  CoreMenu(Context context, Host host, boolean tools) {
    super(context);
    this.host = host;
    this.tools = tools;
    graphics = context.getSharedPreferences(CoreActivity.GRAPHICS_PREFS, Context.MODE_PRIVATE);
    requestWindowFeature(android.view.Window.FEATURE_NO_TITLE);
    body = new LinearLayout(context);
    body.setOrientation(LinearLayout.VERTICAL);
    int pad = dp(16);
    body.setPadding(pad, pad, pad, pad);
    build();
    ScrollView scroll = new ScrollView(context);
    scroll.addView(body);
    setContentView(scroll);
    setCanceledOnTouchOutside(false);
  }

  private int dp(int value) {
    return Math.round(value * getContext().getResources().getDisplayMetrics().density);
  }

  private TextView text(String s, int size, boolean bold) {
    TextView v = new TextView(getContext());
    v.setText(s);
    v.setTextSize(size);
    if (bold) v.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
    return v;
  }

  private void heading(String s) {
    TextView v = text(s, 17, true);
    v.setPadding(0, dp(14), 0, dp(4));
    body.addView(v);
  }

  private Button button(ViewGroup parent, String label, View.OnClickListener click) {
    Button b = new Button(getContext());
    b.setText(label);
    b.setAllCaps(false);
    b.setOnClickListener(click);
    parent.addView(b);
    return b;
  }

  /** A row of choices for preference `key` (values in `values`), applied at once. */
  private RadioGroup choices(String title, String key, String[] labels, int[] values, int fallback, int limit) {
    LinearLayout row = new LinearLayout(getContext());
    row.setGravity(Gravity.CENTER_VERTICAL);
    TextView label = text(title, 14, false);
    row.addView(label, new LinearLayout.LayoutParams(dp(170), ViewGroup.LayoutParams.WRAP_CONTENT));
    RadioGroup group = new RadioGroup(getContext());
    group.setOrientation(LinearLayout.HORIZONTAL);
    int current = graphics.getInt(key, fallback);
    for (int i = 0; i < labels.length; i++) {
      RadioButton b = new RadioButton(getContext());
      b.setId(View.generateViewId());
      b.setText(labels[i]);
      b.setTextSize(14);
      b.setTag(values[i]);
      b.setEnabled(limit <= 0 || values[i] <= limit);
      group.addView(b, new RadioGroup.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
      if (values[i] == current) group.check(b.getId());
    }
    for (int i = labels.length; i < 5; i++) group.addView(new View(getContext()), new RadioGroup.LayoutParams(0, 0, 1));
    group.setOnCheckedChangeListener((g, id) -> {
      if (id == -1) return;
      RadioButton b = g.findViewById(id);
      graphics.edit().putInt(key, (Integer) b.getTag()).apply();
      host.graphicsChanged();
      showGraphics();
    });
    row.addView(group, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
    body.addView(row);
    return group;
  }

  private void build() {
    body.addView(text(t("RE4 Kern – Menü", "RE4 Core – Menu"), 22, true));
    body.addView(text(t("SELECT: dieses Menü  ·  SELECT 1 Sekunde halten: Debug-Menü des Spiels",
        "SELECT: this menu  ·  hold SELECT for a second: the game's debug menu"), 13, false));
    Button resume = button(body, t("Weiterspielen", "Continue"), v -> dismiss());
    resume.requestFocus();

    heading(t("Grafik", "Graphics"));
    int[] info = CoreBridge.graphicsInfo();
    int maxSamples = info != null ? info[0] : 0, maxAnisotropy = info != null ? info[1] : 0;
    choices(t("Auflösung", "Resolution"), "scale", new String[] {"Auto", "1×", "2×", "3×", "4×"},
        new int[] {0, 1, 2, 3, 4}, 0, 0);
    choices(t("Kantenglättung", "Anti-aliasing"), "samples", new String[] {t("Aus", "Off"), "2× MSAA", "4× MSAA"},
        new int[] {1, 2, 4}, 1, maxSamples);
    choices(t("Texturfilter", "Texture filter"), "anisotropy", new String[] {t("Aus", "Off"), "2×", "4×", "8×", "16×"},
        new int[] {1, 2, 4, 8, 16}, 1, maxAnisotropy);
    choices(t("Bildformat", "Picture"), "aspect", new String[] {"16:9", t("4:3 (Original)", "4:3 (original)")},
        new int[] {0, 1}, 0, 0);
    graphicsLine = text("", 12, false);
    body.addView(graphicsLine);
    showGraphics();

    heading(t("Speicherkarte (Steckplatz A)", "Memory card (slot A)"));
    cardLine = text("", 13, false);
    body.addView(cardLine);
    body.addView(text(t("Spielstände als .gci – für Dolphin und (mit einem Kopierprogramm wie GCMM) für den GameCube.",
        "Saves as .gci – for Dolphin and (with a copy tool such as GCMM) for the GameCube."), 12, false));
    cardFiles = new LinearLayout(getContext());
    cardFiles.setOrientation(LinearLayout.VERTICAL);
    body.addView(cardFiles);
    LinearLayout cardButtons = new LinearLayout(getContext());
    exportAll = button(cardButtons, t("Alle exportieren …", "Export all …"), v -> host.exportAll());
    button(cardButtons, t("Importieren (.gci) …", "Import (.gci) …"), v -> host.importFiles());
    body.addView(cardButtons);
    refreshCard();

    heading(t("Fehlerprotokoll", "Error log"));
    body.addView(text(t("Zeichnet auf, was die App meldet (auch bei einem Absturz). Der Bericht enthält das Protokoll "
        + "dieses und des letzten Laufs und Angaben zum Gerät, keine Spielstände.",
        "Records what the app reports (also when it crashes). The report holds the log of this run and the one "
        + "before and the device's data, no saves."), 12, false));
    CheckBox record = new CheckBox(getContext());
    record.setText(t("Fehlerprotokoll aufzeichnen", "Record the error log"));
    record.setChecked(CoreLog.enabled(getContext()));
    record.setOnCheckedChangeListener((b, on) -> host.logChanged(on));
    body.addView(record);
    LinearLayout reportButtons = new LinearLayout(getContext());
    button(reportButtons, t("Bericht senden …", "Send report …"), v -> host.shareReport());
    button(reportButtons, t("Bericht speichern …", "Save report …"), v -> host.saveReport());
    body.addView(reportButtons);
    CheckBox capture = new CheckBox(getContext());
    capture.setText(t("R3: Grafik-Aufnahme für die Fehlersuche", "R3: graphics capture for the diagnosis"));
    capture.setChecked(tools);
    capture.setOnCheckedChangeListener((b, on) -> host.toolsChanged(on));
    body.addView(capture);

    heading(t("Spiel", "Game"));
    button(body, t("Spiel beenden", "Quit the game"), v -> host.quit());
  }

  private void showGraphics() {
    int[] info = CoreBridge.graphicsInfo();
    String now = info == null ? "" : String.format(Locale.ROOT, t("Jetzt: Stufe %d (%d×%d), %s, Texturfilter %s. ",
        "Now: level %d (%d×%d), %s, texture filter %s. "), info[2], 640 * info[2], 528 * info[2],
        info[3] > 1 ? info[3] + "× MSAA" : t("ohne Kantenglättung", "no anti-aliasing"),
        info[4] > 1 ? info[4] + "×" : t("aus", "off"));
    graphicsLine.setText(now + t("Änderungen gelten nach dem Schließen des Menüs. Auto: passend zum Bildschirm "
        + "(höchstens 3×). Höhere Stufen brauchen mehr Leistung – ruckelt es, eine Stufe zurück.",
        "Changes apply when the menu closes. Auto: to the screen (at most 3×). Higher levels need more power – "
        + "if it stutters, go one level back."));
  }

  /** The card's files (the game stands, so the card can be read). */
  void refreshCard() {
    cardFiles.removeAllViews();
    String[] rows = CoreBridge.cardList();
    if (rows == null) {
      boolean busy = "busy".equals(CoreBridge.cardNote());
      cardLine.setText(busy ? t("Die Karte ist gerade in Benutzung (Speichern/Laden des Spiels) oder das Spiel lädt "
          + "noch – das Menü schließen und gleich noch einmal öffnen.", "The card is in use (the game saves or loads) "
          + "or the game is still loading – close the menu and open it again in a moment.")
          : t("Keine Speicherkarte.", "No memory card."));
      exportAll.setEnabled(false);
      return;
    }
    int free = CoreBridge.cardFreeBlocks();
    cardLine.setText(String.format(Locale.ROOT, t("%d Dateien, %d von 251 Blöcken frei.", "%d files, %d of 251 blocks free."),
        rows.length, Math.max(0, free)));
    exportAll.setEnabled(rows.length > 0);
    for (String row : rows) {
      final String[] f = row.split("\t", -1);
      if (f.length < 16) continue;
      final int index = Integer.parseInt(f[0]);
      button(cardFiles, describe(f) + "  –  " + t("exportieren", "export"), v -> {
        String name = CoreBridge.cardFileName(index);
        host.exportFile(index, name != null ? name : f[1] + ".gci");
      });
    }
  }

  /** "Datei 01 · r100 · Spielzeit 0:12:34 · 17.10.2026 00:48" for a save, the system file by name. */
  static String describe(String[] f) {
    String name = f[1];
    boolean intact = "1".equals(f[4]);
    if ("bh4_system".equals(name)) {
      return t("Systemdatei (Optionen)", "System file (options)") + (intact ? "" : t(" – beschädigt", " – damaged"));
    }
    if ("1".equals(f[3])) {
      int no = 0;
      try {
        no = Integer.parseInt(name.substring(8)) + 1;
      } catch (RuntimeException e) {
        no = 0;
      }
      int room = Integer.parseInt(f[5]), play = Integer.parseInt(f[8]);
      String date = german()
          ? String.format(Locale.ROOT, "%02d.%02d.%04d %02d:%02d", Integer.parseInt(f[11]), Integer.parseInt(f[10]),
              Integer.parseInt(f[9]), Integer.parseInt(f[12]), Integer.parseInt(f[13]))
          : String.format(Locale.ROOT, "%04d-%02d-%02d %02d:%02d", Integer.parseInt(f[9]), Integer.parseInt(f[10]),
              Integer.parseInt(f[11]), Integer.parseInt(f[12]), Integer.parseInt(f[13]));
      return String.format(Locale.ROOT, t("Datei %02d · r%03x · Spielzeit %d:%02d:%02d · %s%s",
          "File %02d · r%03x · play time %d:%02d:%02d · %s%s"), no, room, play / 3600, play / 60 % 60, play % 60, date,
          intact ? "" : t(" – beschädigt", " – damaged"));
    }
    return name + " (" + f[2] + t(" Blöcke)", " blocks)");
  }

  @Override protected void onStart() {
    super.onStart();
    if (getWindow() != null) {
      int available = getContext().getResources().getDisplayMetrics().widthPixels;
      getWindow().setLayout(Math.min(dp(860), Math.round(available * .96f)), ViewGroup.LayoutParams.WRAP_CONTENT);
    }
  }

  @Override public boolean dispatchKeyEvent(KeyEvent event) {
    int key = event.getKeyCode();
    if (key == KeyEvent.KEYCODE_BUTTON_SELECT || key == KeyEvent.KEYCODE_BUTTON_START
        || key == KeyEvent.KEYCODE_BUTTON_B) {
      if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) dismiss();
      return true;
    }
    if (key == KeyEvent.KEYCODE_BUTTON_A) {
      return super.dispatchKeyEvent(new KeyEvent(event.getDownTime(), event.getEventTime(), event.getAction(),
          KeyEvent.KEYCODE_DPAD_CENTER, event.getRepeatCount()));
    }
    return super.dispatchKeyEvent(event);
  }
}
