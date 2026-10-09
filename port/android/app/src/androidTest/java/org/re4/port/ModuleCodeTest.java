package org.re4.port;

import androidx.test.platform.app.InstrumentationRegistry;
import org.json.JSONObject;
import org.junit.Test;
import java.io.File;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

/** Original stage registration / two original Init/Main pairs; lifecycle probes are isolated adapters. */
public final class ModuleCodeTest {
    @Test public void originalModulesExecuteInActualAppAbi() throws Exception {
        File root = new File(InstrumentationRegistry.getInstrumentation().getTargetContext().getFilesDir(), "module-verification");
        JSONObject result = new JSONObject(NativeChecks.runModules(root.getPath()));
        int expectedBits = Integer.parseInt(InstrumentationRegistry.getArguments().getString("re4.expectedPointerBits"));
        assertEquals(expectedBits, result.getInt("pointer_bits"));
        assertEquals(0, result.getInt("failed"));
        assertTrue(result.getInt("checks") >= 210);
        assertEquals(14, result.getInt("original_registration_components"));
        assertEquals(2, result.getInt("original_init_main_pairs"));
    }
}
