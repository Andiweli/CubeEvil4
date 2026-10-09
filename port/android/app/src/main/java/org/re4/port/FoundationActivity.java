package org.re4.port;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.view.KeyEvent;
import android.view.MotionEvent;

/** Original-code diagnostics and live controller values. No renderer or playable room. */
public final class FoundationActivity extends Activity {
    private TextView reportView;
    private ControllerLoop controllers;
    private boolean resumed;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        int padding = Math.round(16 * getResources().getDisplayMetrics().density);
        layout.setPadding(padding, padding, padding, padding);

        TextView title = new TextView(this);
        title.setText("RE4 Native Foundation\nOriginal code verification; no game assets loaded.");
        layout.addView(title);
        TextView input = new TextView(this);
        input.setText("Controller: waiting for native gameplay tick");
        input.setTextIsSelectable(true);
        layout.addView(input);
        controllers = new ControllerLoop(this);
        controllers.setObserver(input::setText);
        Button run = new Button(this);
        run.setText("Run native checks");
        run.setOnClickListener(view -> runChecks());
        layout.addView(run);
        reportView = new TextView(this);
        reportView.setTextIsSelectable(true);
        ScrollView scroll = new ScrollView(this);
        scroll.addView(reportView);
        layout.addView(scroll, new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, 0, 1));
        setContentView(layout);
        runChecks();
    }

    private void runChecks() {
        NativeChecks.Result result = NativeChecks.runAll();
        reportView.setText(result.report);
        Log.println(result.passed() ? Log.INFO : Log.ERROR, "RE4Foundation", result.report);
    }
    @Override protected void onResume() {
        super.onResume(); resumed = true;
        if (controllers != null && hasWindowFocus()) controllers.start();
    }
    @Override protected void onPause() {
        resumed = false; if (controllers != null) controllers.stop(); super.onPause();
    }
    @Override public void onWindowFocusChanged(boolean focus) {
        super.onWindowFocusChanged(focus);
        if (controllers != null) { if (focus && resumed) controllers.start(); else controllers.stop(); }
    }
    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        return controllers != null && controllers.key(event) || super.dispatchKeyEvent(event);
    }
    @Override public boolean dispatchGenericMotionEvent(MotionEvent event) {
        return controllers != null && controllers.motion(event) || super.dispatchGenericMotionEvent(event);
    }
    @Override protected void onDestroy() {
        if (controllers != null) { controllers.close(); controllers = null; } super.onDestroy();
    }
}
