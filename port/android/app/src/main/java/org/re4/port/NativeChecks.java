package org.re4.port;

import java.util.Objects;

public final class NativeChecks {
    static {
        System.loadLibrary("re4_port_jni");
    }

    private NativeChecks() {}

    public static final class Result {
        public final int checks;
        public final int failures;
        public final String report;

        public Result(int checks, int failures, String report) {
            this.checks = checks;
            this.failures = failures;
            this.report = Objects.requireNonNull(report);
        }

        public boolean passed() {
            return checks > 0 && failures == 0;
        }
    }

    public static Result runAll() {
        return nativeRun("");
    }

    public static Result runGroup(String group) {
        return nativeRun(Objects.requireNonNull(group));
    }

    private static native Result nativeRun(String group);
    public static String runModules(String directory) {
        return nativeModules(Objects.requireNonNull(directory));
    }
    private static native String nativeModules(String directory);
}
