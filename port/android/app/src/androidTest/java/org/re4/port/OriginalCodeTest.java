package org.re4.port;

import org.junit.Test;
import androidx.test.platform.app.InstrumentationRegistry;
import static org.junit.Assert.assertTrue;

/** Executes the same compiled original code inside the device's app process. */
public final class OriginalCodeTest {
    private void check(String group) {
        NativeChecks.Result result = NativeChecks.runGroup(group);
        assertTrue(result.report, result.passed());
    }

    @Test public void nativeAbi() {
        NativeChecks.Result result = NativeChecks.runGroup("abi");
        assertTrue(result.report, result.passed());
        String bits = InstrumentationRegistry.getArguments().getString("re4.expectedPointerBits");
        if (bits != null) {
            assertTrue("Wrong process ABI: " + result.report,
                result.report.contains("pointer=" + bits + " bits;"));
        }
    }
    @Test public void originalRng() { check("rng"); }
    @Test public void originalFlags() { check("flags"); }
    @Test public void originalMath() { check("math"); }
    @Test public void gameCubeByteOrder() { check("endian"); }
    @Test public void stackfulScheduler() { check("scheduler_stack"); }
    @Test public void schedulerOrder() { check("scheduler_order"); }
    @Test public void schedulerLifecycle() { check("scheduler_lifecycle"); }
    @Test public void schedulerModuleLifetimes() { check("scheduler_modules"); }
}
