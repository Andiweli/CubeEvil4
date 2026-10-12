package org.re4.port;

import android.app.Activity;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import org.json.JSONObject;

/** Isolated original-data rendering proof. No GameTask state is advanced or bypassed. */
public final class GeometryActivity extends Activity {
    private ControllerLoop controllers;
    private boolean resumed;
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        LinearLayout layout=new LinearLayout(this);layout.setOrientation(LinearLayout.VERTICAL);
        TextView label=new TextView(this);label.setText("Original data / isolated GX diagnostic");layout.addView(label);
        GraphicsBootstrapView graphics=new GraphicsBootstrapView(this);
        final int debugView=getIntent().getIntExtra("debug_view",0);
        graphics.configureDebugView(debugView);
        String model=getIntent().getStringExtra("model");graphics.configureEventModel(model==null?"":model);
        graphics.configureEventCut(getIntent().getIntExtra("cut",-1));
        final int shelf=getIntent().getIntExtra("shelf_selection",-1);
        if(shelf>=0)graphics.configureShelf(shelf);
        String root=getIntent().getStringExtra("root"),catalog=getIntent().getStringExtra("catalog");
        if(root==null || catalog==null)throw new IllegalArgumentException("Private imported root and catalog required");
        int room=getIntent().getIntExtra("room",0x120),frame=getIntent().getIntExtra("frame",0);
        String event=getIntent().getStringExtra("event");if(event==null)event="Evd/r120s00.evd";
        String requestedSession=getIntent().getStringExtra("capture");
        final String session=requestedSession==null?"phase5a":requestedSession;
        graphics.configureEvidenceSession(session);
        label.setText(String.format(Locale.ROOT,"Isolated original-data rendering: r%03x, frame %d",room,frame));
        graphics.configureScene(root,catalog,room,event,frame,report -> {
            try(FileOutputStream out=openFileOutput(session+"-graphics.json",MODE_PRIVATE)) {
                JSONObject evidence=new JSONObject(report);
                JSONObject scene=evidence.getJSONObject("scene");
                label.setText(shelf>=0?String.format(Locale.ROOT,"DEBUG original shelf callback r%03x / selection %d\nScheduler ticks %d / returned %s / texture-only diagnostic",
                    room,shelf,evidence.getJSONObject("shelf_callback").getInt("visits"),evidence.getJSONObject("shelf_callback").getBoolean("returned")):String.format(Locale.ROOT,
                    "DEBUG view %d / isolated render: r%03x | frame %d | model %s\nCamera: %s",
                    debugView,room,scene.getInt("frame"),scene.optString("event_model",""),scene.getString("camera")));
                out.write(evidence.put("evidence_session",session).toString().getBytes(StandardCharsets.UTF_8));
            }catch(java.io.IOException|org.json.JSONException e){android.util.Log.e("RE4Graphics","Cannot save evidence: "+report,e);}
        });
        layout.addView(graphics,new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,0,1));
        controllers=new ControllerLoop(this);setContentView(layout);
    }
    @Override protected void onResume(){super.onResume();resumed=true;if(controllers!=null&&hasWindowFocus())controllers.start();}
    @Override protected void onPause(){resumed=false;if(controllers!=null)controllers.stop();super.onPause();}
    @Override public void onWindowFocusChanged(boolean focus){super.onWindowFocusChanged(focus);if(controllers!=null){if(focus&&resumed)controllers.start();else controllers.stop();}}
    @Override public boolean dispatchKeyEvent(KeyEvent e){return controllers!=null&&controllers.key(e)||super.dispatchKeyEvent(e);}
    @Override public boolean dispatchGenericMotionEvent(MotionEvent e){return controllers!=null&&controllers.motion(e)||super.dispatchGenericMotionEvent(e);}
    @Override protected void onDestroy(){if(controllers!=null)controllers.close();super.onDestroy();}
}
