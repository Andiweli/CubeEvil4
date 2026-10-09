package org.re4.port;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/** Native original-code and scheduler diagnostics. No assets or gameplay loop. */
public final class FoundationActivity extends Activity {
    private TextView reportView;

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
}
