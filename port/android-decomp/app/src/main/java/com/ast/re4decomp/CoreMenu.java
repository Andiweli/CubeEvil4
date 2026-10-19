package com.ast.re4decomp;

import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Typeface;
import android.net.Uri;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.UnderlineSpan;
import android.util.TypedValue;
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
 * display, the touch controls' and gyro aiming's switches (19.10.2026), the memory card as .gci
 * (export for Dolphin or a GameCube, import from there) and the error log; centred headings in small
 * capitals between lines (wishes of the project owner, 17.10.2026). The game
 * stands while it is open (CoreActivity pauses the core).
 * Run with the controller: D-pad between the entries, A chooses, B / START /
 * SELECT close it; by touch like any Android dialog ("Back to the game"
 * closes it). English (decision of the project owner, 17.10.2026).
 */
final class CoreMenu extends Dialog {
  interface Host {
    /** Picture options changed (the preferences hold them). */
    void graphicsChanged();
    void exportAll();
    void importFiles();
    void shareReport();
    void saveReport();
    void logChanged(boolean on);
    void toolsChanged(boolean on);
    void fpsChanged(boolean on);
    /** The switch "Use Touch Controls" (the preferences hold it). */
    void touchChanged(boolean on);
    /** The switch "Use Gyro-Aiming" (the preferences hold it). */
    void gyroChanged(boolean on);
    /** The switch "Alternate Controls" (the preferences hold it). */
    void alternateChanged(boolean on);
  }

  private final Host host;
  private final SharedPreferences graphics;
  private final boolean tools;
  private final int screenWidth, screenHeight;
  private final LinearLayout body;
  private RadioGroup firstChoice;
  private TextView graphicsLine, cardLine;
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

  /** A check box taking `weight` shares of its row. */
  private CheckBox checkBox(ViewGroup parent, String label, boolean checked, float weight,
                            android.widget.CompoundButton.OnCheckedChangeListener changed) {
    CheckBox c = new CheckBox(getContext());
    c.setText(label);
    c.setChecked(checked);
    c.setOnCheckedChangeListener(changed);
    parent.addView(c, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, weight));
    return c;
  }

  private CheckBox checkBox(ViewGroup parent, String label, boolean checked,
                            android.widget.CompoundButton.OnCheckedChangeListener changed) {
    return checkBox(parent, label, checked, 1, changed);
  }

  private Button button(ViewGroup parent, String label, View.OnClickListener click) {
    Button b = new Button(getContext());
    b.setText(label);
    b.setAllCaps(false);
    b.setOnClickListener(click);
    parent.addView(b);
    return b;
  }

  /** A button taking `weight` shares of its row. */
  private Button button(ViewGroup parent, String label, float weight, View.OnClickListener click) {
    Button b = new Button(getContext());
    b.setText(label);
    b.setAllCaps(false);
    b.setOnClickListener(click);
    parent.addView(b, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, weight));
    return b;
  }

  /**
   * A row like the choices' rows: `title` in the label column, then the returned box, whose
   * children share the width as the five columns of the choices do (weight 1 each: a child of
   * weight 2 at the start spans the first two columns, the next one starts under the third).
   */
  private LinearLayout labelledRow(String title) {
    LinearLayout row = new LinearLayout(getContext());
    row.setGravity(Gravity.CENTER_VERTICAL);
    row.addView(text(title, 14, false), new LinearLayout.LayoutParams(dp(170), ViewGroup.LayoutParams.WRAP_CONTENT));
    LinearLayout box = new LinearLayout(getContext());
    box.setGravity(Gravity.CENTER_VERTICAL);
    box.setWeightSum(5);
    row.addView(box, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
    body.addView(row);
    return box;
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
    // the title and the line under it on the left, "Back to the game" on the right of the same row
    LinearLayout top = new LinearLayout(getContext());
    top.setGravity(Gravity.CENTER_VERTICAL);
    LinearLayout titles = new LinearLayout(getContext());
    titles.setOrientation(LinearLayout.VERTICAL);
    titles.addView(text("CubeEvil4 Options", 22, true));
    titles.addView(text("SELECT (short-press) = this Menu  ·  SELECT (long-press) = the Game's Debug Menu", 13, false));
    top.addView(titles, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
    button(top, "Back to the game", v -> dismiss());
    body.addView(top, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
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
    // the check boxes in the choices' columns: the first under "16:9", the second under the third
    LinearLayout other = labelledRow("Other options");
    checkBox(other, "Depth of field", graphics.getBoolean("dof", true), 2,
        (b, on) -> {
          graphics.edit().putBoolean("dof", on).apply();
          CoreBridge.setDof(on);
        });
    checkBox(other, "FPS display bottom left", graphics.getBoolean("fps", false), 3,
        (b, on) -> {
          graphics.edit().putBoolean("fps", on).apply();
          host.fpsChanged(on);
        });
    graphicsLine = text("", 12, false);
    body.addView(graphicsLine);
    showGraphics();

    heading("Controls");
    // in the columns of the check boxes above
    SharedPreferences input = getContext().getSharedPreferences(CoreActivity.INPUT_PREFS, Context.MODE_PRIVATE);
    LinearLayout methods = labelledRow("Select methods");
    checkBox(methods, "Use Touch Controls", input.getBoolean(CoreActivity.TOUCH_ALWAYS, false), 2,
        (b, on) -> host.touchChanged(on));
    boolean gyro = GyroAim.available(getContext());
    CheckBox gyroBox = checkBox(methods, gyro ? "Use Gyro-Aiming" : "Use Gyro-Aiming (no gyroscope)",
        gyro && input.getBoolean(CoreActivity.GYRO_AIM, false), 3, (b, on) -> host.gyroChanged(on));
    gyroBox.setEnabled(gyro);
    body.addView(text("Touch Controls: They appear when no controller is detected, use checkbox to manually enable "
        + "it. The menu-icon opens the configuration (MENU = SELECT, L and R stay pressed until tapped again).",
        12, false));
    // the third switch: a row of its own (three do not fit side by side), under "Use Touch Controls"
    LinearLayout layout = labelledRow("Button layout");
    checkBox(layout, "Alternate Controls", input.getBoolean(CoreActivity.ALT_CONTROLS, false), 5,
        (b, on) -> host.alternateChanged(on));
    body.addView(text("Alternate Controls: L1 = Knife, L2 = Aim, R1 = Inventory (Weapons/Recovery), "
        + "R2 = Fire / use the knife, Y = Map. A, B, X (Ashley) and START as before.", 12, false));

    heading("Memory card (slot A)");
    body.addView(text("Export and Import Savegames as .gci (for Dolphin or GameCube).", 12, false));
    // only when the card cannot be read now (in use, still loading, none)
    cardLine = text("", 12, false);
    body.addView(cardLine);
    LinearLayout cardButtons = new LinearLayout(getContext());
    exportAll = button(cardButtons, "Export Savegames (.gci)", 1, v -> host.exportAll());
    button(cardButtons, "Import Savegames (.gci)", 1, v -> host.importFiles());
    body.addView(cardButtons);
    refreshCard();

    heading("Error log");
    body.addView(text("Records an app report (also when crashed or it hangs). The report holds log of this run and the "
        + "one before.", 12, false));
    // four equal parts: the two check boxes, then the two buttons
    LinearLayout log = new LinearLayout(getContext());
    log.setGravity(Gravity.CENTER_VERTICAL);
    checkBox(log, "Record the error log", CoreLog.enabled(getContext()),
        (b, on) -> host.logChanged(on));
    checkBox(log, "R3 = Graphics capture", tools,
        (b, on) -> host.toolsChanged(on));
    button(log, "Send report …", 1, v -> host.shareReport());
    button(log, "Save report …", 1, v -> host.saveReport());
    body.addView(log);

    heading("Source");
    // the address opens in the browser (A on it, or a tap)
    TextView source = text("", 12, false);
    SpannableStringBuilder link = new SpannableStringBuilder("More on this port at ");
    int at = link.length();
    link.append(SOURCE_URL);
    link.setSpan(new UnderlineSpan(), at, link.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    link.setSpan(new ForegroundColorSpan(source.getLinkTextColors().getDefaultColor()), at, link.length(),
        Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    source.setText(link);
    source.setGravity(Gravity.CENTER_HORIZONTAL);
    source.setFocusable(true);
    source.setClickable(true);
    TypedValue ripple = new TypedValue();
    getContext().getTheme().resolveAttribute(android.R.attr.selectableItemBackground, ripple, true);
    source.setBackgroundResource(ripple.resourceId);
    source.setPadding(dp(8), dp(6), dp(8), dp(6));
    source.setOnClickListener(v -> openSource());
    body.addView(source, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
        ViewGroup.LayoutParams.WRAP_CONTENT));
  }

  /** The port's page (wish of the project owner, 19.10.2026). */
  static final String SOURCE_URL = "https://github.com/Andiweli/CubeEvil4-Android";

  private void openSource() {
    try {
      getContext().startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(SOURCE_URL)));
    } catch (ActivityNotFoundException e) {
      android.widget.Toast.makeText(getContext(), "No browser on this device", android.widget.Toast.LENGTH_SHORT).show();
    }
  }

  /**
   * Auto (as core_mode.cpp chooses it): one level below the one whose 336 picture rows come
   * closest to the screen's rows (wish of the project owner, 19.10.2026: 1080 rows take 2).
   */
  static int autoScale(int screenWidth, int screenHeight) {
    int rows = Math.min(screenHeight, screenWidth * 9 / 16);
    return Math.max(1, Math.min(3, (rows + 168) / 336) - 1);
  }

  /** The resolution the chosen level gives. */
  private void showGraphics() {
    int chosen = graphics.getInt("scale", 0), scale = chosen;
    if (scale < 1 || scale > 4) scale = autoScale(screenWidth, screenHeight);
    boolean original = graphics.getInt("aspect", 0) == 1;
    graphicsLine.setText(String.format(Locale.ROOT, "Resolution: %d × %d pixels (level %d%s) · screen %d × %d · "
        + "applies when the menu closes. Higher levels need more power.", 640 * scale, (original ? 448 : 336) * scale, scale, chosen == 0 ? ", Auto" : "",
        screenWidth, screenHeight));
  }

  /** Whether the card can be read now (the game stands): the export only then, else why not. */
  void refreshCard() {
    String[] rows = CoreBridge.cardList();
    if (rows == null) {
      boolean busy = "busy".equals(CoreBridge.cardNote());
      cardLine.setText(busy ? "The card is in use (the game saves or loads) or the game is still loading – close the "
          + "menu and open it again in a moment."
          : "No memory card.");
      cardLine.setVisibility(View.VISIBLE);
      exportAll.setEnabled(false);
      return;
    }
    cardLine.setVisibility(View.GONE);
    exportAll.setEnabled(rows.length > 0);
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
