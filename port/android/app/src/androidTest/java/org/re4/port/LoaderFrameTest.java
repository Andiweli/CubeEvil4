package org.re4.port;

import android.content.Context;
import androidx.test.platform.app.InstrumentationRegistry;
import org.junit.Test;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicReference;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

/** Isolated frame/VI adapter tests. Real copyrighted room data is tested by the native CLI. */
public final class LoaderFrameTest {
    private File catalog() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        File root = new File(context.getFilesDir(), "loader-adapter-test");
        assertTrue(root.isDirectory() || root.mkdirs());
        File file = new File(root, "catalog.tsv");
        String text = "RE4-IMPORT-CATALOG-1\n0\t1\t3\tfixture.dat\t" +
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n";
        try (FileOutputStream out = new FileOutputStream(file)) { out.write(text.getBytes(StandardCharsets.UTF_8)); }
        return file;
    }
    @Test public void displayAndGameplayClocksAreSeparate() throws Exception {
        File catalog = catalog();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            try (LoaderFrameLoop loop = new LoaderFrameLoop(catalog.getParent(), catalog.getPath(), 0)) {
                for (int i = 0; i <= 120; ++i) loop.frameForTest(1_000_000_000L * i / 120);
                assertEquals(60, loop.timing()[0]); assertEquals(30, loop.timing()[1]);
                loop.stop(); loop.frameForTest(10_000_000_000L);
                assertEquals(30, loop.timing()[1]);
            }
        });
    }
    @Test public void actualChoreographerFeedsNativeClock() throws Exception {
        File catalog = catalog();
        AtomicReference<LoaderFrameLoop> loop = new AtomicReference<>();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            loop.set(new LoaderFrameLoop(catalog.getParent(), catalog.getPath(), 0)); loop.get().start();
        });
        try {
            Thread.sleep(250);
            InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
                assertTrue(loop.get().timing()[0] > 0); assertTrue(loop.get().timing()[1] > 0);
            });
        } finally { InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> loop.get().close()); }
    }
}
