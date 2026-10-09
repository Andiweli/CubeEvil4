package org.re4.port;

import org.junit.Test;
import static org.junit.Assert.assertTrue;

/** Executes the same compiled original code inside the device's app process. */
public final class OriginalCodeTest {
    private void check(String group) {
        NativeChecks.Result result = NativeChecks.runGroup(group);
        assertTrue(result.report, result.passed());
    }

    @Test public void nativeAbi() { check("abi"); }
    @Test public void originalRng() { check("rng"); }
    @Test public void originalFlags() { check("flags"); }
    @Test public void originalMath() { check("math"); }
    @Test public void gameCubeByteOrder() { check("endian"); }
}
