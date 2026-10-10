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
        String model=getIntent().getStringExtra("model");graphics.configureEventModel(model==null?"":model);
        graphics.configureEventCut(getIntent().getIntExtra("cut",-1));
        String root=getIntent().getStringExtra("root"),catalog=getIntent().getStringExtra("catalog");
        if(root==null || catalog==null)throw new IllegalArgumentException("Private imported root and catalog required");
        int room=getIntent().getIntExtra("room",0x120),frame=getIntent().getIntExtra("frame",0);
        String event=getIntent().getStringExtra("event");if(event==null)event="Evd/r120s00.evd";
        String requestedSession=getIntent().getStringExtra("capture");
        final String session=requestedSession==null?"phase5a":requestedSession;
        graphics.configureEvidenceSession(session);
        graphics.configureScene(root,catalog,room,event,frame,report -> {
            label.setText("Original data / isolated GX OT/TEV/LIT diagnostic\n"+report.substring(0,Math.min(160,report.length())));
            try(FileOutputStream out=openFileOutput(session+"-graphics.json",MODE_PRIVATE)) {
                out.write(new JSONObject(report).put("evidence_session",session).toString().getBytes(StandardCharsets.UTF_8));
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
