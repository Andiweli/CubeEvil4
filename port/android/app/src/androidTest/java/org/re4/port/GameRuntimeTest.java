package org.re4.port;

import androidx.test.platform.app.InstrumentationRegistry;
import android.view.KeyEvent;
import org.json.JSONObject;
import org.junit.Test;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

/** Original functions plus clearly isolated native/Java adapter probes. Physical
 * Retroid presses are captured separately by the Activity's event log. */
public final class GameRuntimeTest {
    @Test public void originalRuntimeInActualAppAbi() throws Exception {
        File root = new File(InstrumentationRegistry.getInstrumentation().getTargetContext().getFilesDir(),"game-verification");
        JSONObject result = new JSONObject(NativeChecks.runGameRuntime(root.getPath()));
        int bits = Integer.parseInt(InstrumentationRegistry.getArguments().getString("re4.expectedPointerBits"));
        assertEquals(bits,result.getInt("pointer_bits")); assertEquals(0,result.getInt("failed"));
        assertTrue(result.getInt("checks") >= 609);
        assertTrue(result.getJSONArray("trace").toString().contains("original-init-TexRenderInit"));
        assertTrue(result.getJSONArray("trace").toString().contains("original-modelInit-joints-matrices-bounds"));
        assertTrue(result.getJSONArray("trace").toString().contains("transitions:20-initialized-cross-stage"));
        assertTrue(result.getJSONArray("trace").toString().contains("event-stack:original-ExeFunc"));
    }
    @Test public void androidRangeNormalization() {
        assertEquals(0,ControllerLoop.normalize(0.1f,-1,1,0.2f,false),0);
        assertEquals(-1,ControllerLoop.normalize(0,0,65534,0,false),0);
        assertEquals(0,ControllerLoop.normalize(32767,0,65534,0,false),0);
        assertEquals(1,ControllerLoop.normalize(65534,0,65534,0,false),0);
        assertEquals(0.5f,ControllerLoop.normalize(0.5f,0,1,0,true),0);
        assertEquals(0,ControllerLoop.normalize(Float.NaN,-1,1,0,false),0);
    }
    @Test public void centralNativeControllerAndLifecycle() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            try (ControllerLoop loop = new ControllerLoop(InstrumentationRegistry.getInstrumentation().getTargetContext())) {
                loop.connectForTest(123456,0); loop.start();
                loop.frameForTest(0); loop.frameForTest(33_333_334L);
                assertTrue(loop.buttonForTest(123456,KeyEvent.KEYCODE_BUTTON_A,true));
                loop.frameForTest(66_666_668L);
                try {
                    JSONObject held = new JSONObject(loop.snapshot());
                    assertEquals(256,held.getInt("joy_on")); assertEquals(256,held.getInt("joy_trg"));
                    assertTrue((held.getLong("key_on") & 0x80000000L) != 0);
                    loop.stop(); loop.start(); loop.frameForTest(5_000_000_000L); loop.frameForTest(5_033_333_334L);
                    assertEquals(0,new JSONObject(loop.snapshot()).getLong("key_on"));
                    loop.disconnectForTest(123456); loop.frameForTest(5_066_666_668L);
                    assertEquals(-1,new JSONObject(loop.snapshot()).getInt("pad_error"));
                } catch (Exception failure) { throw new AssertionError(failure); }
            }
        });
    }
    @Test public void controllerUsesSharedGameRuntime() throws Exception {
        // Isolated catalog adapter fixture. Boot and PadRead need no original assets.
        // ImportedFiles deliberately rejects catalogs with no entries.
        File root = new File(InstrumentationRegistry.getInstrumentation().getTargetContext().getFilesDir(),"runtime-catalog-adapter");
        assertTrue(root.isDirectory() || root.mkdirs());
        File catalog = new File(root,"catalog.tsv");
        try (FileOutputStream file = new FileOutputStream(catalog)) {
            file.write(("RE4-IMPORT-CATALOG-1\n0\t1\t0\tfixture-only/data.bin\t" +
                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n").getBytes(StandardCharsets.UTF_8));
        }
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            try (ControllerLoop loop = new ControllerLoop(InstrumentationRegistry.getInstrumentation().getTargetContext());
                 ControllerLoop second = new ControllerLoop(InstrumentationRegistry.getInstrumentation().getTargetContext())) {
                loop.configureGame(root.getPath(),catalog.getPath(),0);
                boolean refused = false;
                try { second.configureGame(root.getPath(),catalog.getPath(),0); }
                catch (IllegalStateException expected) { refused = true; }
                assertTrue(refused);
                loop.connectForTest(123456,0); loop.start();
                loop.frameForTest(0); loop.frameForTest(33_333_334L);
                assertTrue(loop.buttonForTest(123456,KeyEvent.KEYCODE_BUTTON_A,true));
                loop.frameForTest(66_666_668L);
                try {
                    JSONObject held = new JSONObject(loop.snapshot());
                    assertEquals("shared-game-runtime",held.getString("mode"));
                    assertEquals(256,held.getInt("joy_on"));
                    loop.stop(); loop.start();
                    loop.frameForTest(5_000_000_000L); loop.frameForTest(5_033_333_334L);
                    assertEquals(0,new JSONObject(loop.snapshot()).getLong("key_on"));
                } catch (Exception failure) { throw new AssertionError(failure); }
            }
        });
    }
}
