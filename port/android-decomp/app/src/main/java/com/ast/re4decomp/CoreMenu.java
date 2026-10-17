package com.ast.re4decomp;

import android.app.Dialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Typeface;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.RelativeSizeSpan;
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
 * 17.10.2026): picture options with the resolution they give, the FPS
 * display, the memory card's files as .gci (export for Dolphin or a
 * GameCube, import from there) and the error log; centred headings in small
 * capitals between lines (wishes of the project owner, 17.10.2026). The game
 * stands while it is open (CoreActivity pauses the core).
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
    void fpsChanged(boolean on);
  }

  private final Host host;
  private final SharedPreferences graphics;
  private final boolean tools;
  private final int screenWidth, screenHeight;
  private final LinearLayout body;
  private RadioGroup firstChoice;
  private TextView graphicsLine, cardLine;
  private LinearLayout cardFiles;
  private Button exportAll;

  /** screenWidth/Height: the game's picture on the screen (Auto chooses the resolution from it). */
  CoreMenu(Context context, Host host, boolean tools, int screenWidth, int screenHeight) {
    super(context);
    this.host = host;
    this.tools = tools;
    this.screenWidth = screenWidth;
    this.screenHeight = screenHeight;
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

  /** Small capitals: the lower-case letters as smaller capitals, the capitals as they are. */
  static CharSequence smallCaps(String s) {
    SpannableStringBuilder b = new SpannableStringBuilder();
    for (int i = 0; i < s.length(); i++) {
      char c = s.charAt(i);
      int at = b.length();
      b.append(Character.toUpperCase(c));
      if (Character.isLowerCase(c)) b.setSpan(new RelativeSizeSpan(0.78f), at, at + 1, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    }
    return b;
  }

  /** A thin line in the text colour, faint. */
  private View line(TextView colourOf) {
    View v = new View(getContext());
    v.setBackgroundColor((colourOf.getCurrentTextColor() & 0x00FFFFFF) | 0x60000000);
    return v;
  }

  /** A centred heading in small capitals with a line left and right of it. */
  private void heading(String s) {
    LinearLayout row = new LinearLayout(getContext());
    row.setGravity(Gravity.CENTER_VERTICAL);
    TextView v = text("", 16, true);
    v.setText(smallCaps(s));
    v.setLetterSpacing(0.08f);
    v.setPadding(dp(12), 0, dp(12), 0);
    int h = Math.max(1, dp(1));
    row.addView(line(v), new LinearLayout.LayoutParams(0, h, 1));
    row.addView(v, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    row.addView(line(v), new LinearLayout.LayoutParams(0, h, 1));
    LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
        ViewGroup.LayoutParams.WRAP_CONTENT);
    lp.setMargins(0, dp(14), 0, dp(6));
    body.addView(row, lp);
  }

  private CheckBox checkBox(ViewGroup parent, String label, boolean checked,
                            android.widget.CompoundButton.OnCheckedChangeListener changed) {
    CheckBox c = new CheckBox(getContext());
    c.setText(label);
    c.setChecked(checked);
    c.setOnCheckedChangeListener(changed);
    parent.addView(c, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
    return c;
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
    TextView title = text("Resident Evil 4 Options", 22, true);
    title.setGravity(Gravity.CENTER_HORIZONTAL);
    body.addView(title, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
        ViewGroup.LayoutParams.WRAP_CONTENT));
    TextView intro = text("SELECT: this menu  ·  hold SELECT for a second: the game's debug menu  ·  B: back to the game",
        13, false);
    intro.setGravity(Gravity.CENTER_HORIZONTAL);
    body.addView(intro, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
        ViewGroup.LayoutParams.WRAP_CONTENT));

    heading("Graphics");
    int[] info = CoreBridge.graphicsInfo();
    int maxSamples = info != null ? info[0] : 0, maxAnisotropy = info != null ? info[1] : 0;
    firstChoice = choices("Resolution", "scale", new String[] {"Auto", "1×", "2×", "3×", "4×"},
        new int[] {0, 1, 2, 3, 4}, 0, 0);
    choices("Anti-aliasing", "samples", new String[] {"Off", "2× MSAA", "4× MSAA"},
        new int[] {1, 2, 4}, 1, maxSamples);
    choices("Texture filter", "anisotropy", new String[] {"Off", "2×", "4×", "8×", "16×"},
        new int[] {1, 2, 4, 8, 16}, 1, maxAnisotropy);
    choices("Picture", "aspect", new String[] {"16:9", "4:3 (original)"},
        new int[] {0, 1}, 0, 0);
    LinearLayout fpsRow = new LinearLayout(getContext());
    checkBox(fpsRow, "FPS display bottom left", graphics.getBoolean("fps", false),
        (b, on) -> {
          graphics.edit().putBoolean("fps", on).apply();
          host.fpsChanged(on);
        });
    checkBox(fpsRow, "Depth of field", graphics.getBoolean("dof", true),
        (b, on) -> {
          graphics.edit().putBoolean("dof", on).apply();
          CoreBridge.setDof(on);
        });
    body.addView(fpsRow);
    graphicsLine = text("", 12, false);
    body.addView(graphicsLine);
    showGraphics();

    heading("Memory card (slot A)");
    cardLine = text("", 13, false);
    body.addView(cardLine);
    body.addView(text("Saves as .gci – for Dolphin and (with a copy tool such as GCMM) for the GameCube.", 12, false));
    cardFiles = new LinearLayout(getContext());
    cardFiles.setOrientation(LinearLayout.VERTICAL);
    body.addView(cardFiles);
    LinearLayout cardButtons = new LinearLayout(getContext());
    exportAll = button(cardButtons, "Export all …", v -> host.exportAll());
    button(cardButtons, "Import (.gci) …", v -> host.importFiles());
    body.addView(cardButtons);
    refreshCard();

    heading("Error log");
    body.addView(text("Records what the app reports (also when it crashes or hangs). The report holds the log of this "
        + "run and the one before and the device's data, no saves.", 12, false));
    LinearLayout checks = new LinearLayout(getContext());
    checkBox(checks, "Record the error log", CoreLog.enabled(getContext()),
        (b, on) -> host.logChanged(on));
    checkBox(checks, "R3: graphics capture (diagnosis)", tools,
        (b, on) -> host.toolsChanged(on));
    body.addView(checks);
    LinearLayout reportButtons = new LinearLayout(getContext());
    button(reportButtons, "Send report …", v -> host.shareReport());
    button(reportButtons, "Save report …", v -> host.saveReport());
    body.addView(reportButtons);
  }

  /** The resolution the chosen level gives (Auto: as core_mode.cpp chooses it from the screen). */
  private void showGraphics() {
    int chosen = graphics.getInt("scale", 0), scale = chosen;
    if (scale < 1 || scale > 4) {
      int rows = Math.min(screenHeight, screenWidth * 9 / 16);
      scale = Math.max(1, Math.min(3, (rows + 168) / 336));
    }
    boolean original = graphics.getInt("aspect", 0) == 1;
    graphicsLine.setText(String.format(Locale.ROOT, "Resolution: %d × %d pixels (level %d%s) · screen %d × %d · "
        + "applies when the menu closes. Higher levels need more power.", 640 * scale, (original ? 448 : 336) * scale, scale, chosen == 0 ? ", Auto" : "",
        screenWidth, screenHeight));
  }

  /** The card's files (the game stands, so the card can be read). */
  void refreshCard() {
    cardFiles.removeAllViews();
    String[] rows = CoreBridge.cardList();
    if (rows == null) {
      boolean busy = "busy".equals(CoreBridge.cardNote());
      cardLine.setText(busy ? "The card is in use (the game saves or loads) or the game is still loading – close the "
          + "menu and open it again in a moment."
          : "No memory card.");
      exportAll.setEnabled(false);
      return;
    }
    int free = CoreBridge.cardFreeBlocks();
    cardLine.setText(String.format(Locale.ROOT, "%d files, %d of 251 blocks free.",
        rows.length, Math.max(0, free)));
    exportAll.setEnabled(rows.length > 0);
    for (String row : rows) {
      final String[] f = row.split("\t", -1);
      if (f.length < 16) continue;
      final int index = Integer.parseInt(f[0]);
      button(cardFiles, describe(f) + "  –  " + "export", v -> {
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
      return "System file (options)" + (intact ? "" : " – damaged");
    }
    if ("1".equals(f[3])) {
      int no = 0;
      try {
        no = Integer.parseInt(name.substring(8)) + 1;
      } catch (RuntimeException e) {
        no = 0;
      }
      int room = Integer.parseInt(f[5]), play = Integer.parseInt(f[8]);
      String date = String.format(Locale.ROOT, "%04d-%02d-%02d %02d:%02d", Integer.parseInt(f[9]),
          Integer.parseInt(f[10]), Integer.parseInt(f[11]), Integer.parseInt(f[12]), Integer.parseInt(f[13]));
      return String.format(Locale.ROOT, "File %02d · r%03x · play time %d:%02d:%02d · %s%s", no, room, play / 3600, play / 60 % 60, play % 60, date,
          intact ? "" : " – damaged");
    }
    return name + " (" + f[2] + " blocks)";
  }

  @Override protected void onStart() {
    super.onStart();
    // the first choice has the focus (the controller starts there)
    if (firstChoice != null) {
      View checked = firstChoice.findViewById(firstChoice.getCheckedRadioButtonId());
      if (checked != null) checked.requestFocus();
    }
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
