# Phase 1 verification record

Date: 2026-10-09. Working branch: `android-port`.
Original master reference: `9b76ff003081d6ece8b838453fb8310743a972c7`.
Only new files under `port/` are introduced. This verified foundation is secured
in a local commit before Phase 2. No pushes were made.
Original tracked game sources, headers, configuration, tools and documentation
remain identical to master. No GameCube assets or binaries were imported.

## First milestone reached

Original RNG code and selected original flag/math functions build into a native
verification library on ARM32, ARM64 and independent Windows host targets.
Both host pointer widths execute the shared suite successfully. A dual-ABI
Android diagnostic APK and instrumentation APK are produced. This is an
original-code build/test milestone, not a playable RE4 engine.

## Results

| Check | Actual result | Limit |
| --- | --- | --- |
| Windows x64 host build | Passed, MSVC 19.51, Ninja | Host execution, not Android |
| Windows x86 host build | Passed, MSVC 19.51, Ninja | Host execution, not ARM32 |
| CTest on each host | 7/7 passed | Includes source guard and rejection probe |
| Shared suite on each host | 12,412 checks, zero failures | Source-derived vectors, no PPC reference trace |
| NDK armeabi-v7a | Build passed; 12,412 checks passed on Retroid | Source-derived vectors, no PPC trace |
| NDK arm64-v8a | Build passed; 12,412 checks passed on Retroid | Source-derived vectors, no PPC trace |
| ELF inspection | ARM ELF32 and AArch64 ELF64, both little-endian | Architecture inspection only |
| Gradle assembleDebug | Passed; installed on Retroid | Diagnostic application, not gameplay |
| Gradle assembleDebugAndroidTest | Passed; instrumentation 5/5 on Retroid | Dual-ABI APK selects ARM64; ARM32 tested by native executable |
| Gradle lintDebug | Passed: 0 errors, 8 warnings | See warning discussion below |
| adb devices -l | Retroid Pocket Flip 2, serial dbdcacc | Android 13 / API 33; supports arm64-v8a, armeabi-v7a, armeabi |
| APK contents | Both JNI libraries; no assets/ entries | No original disc data involved |
| Original source identity | Fingerprints and master comparison pass | Covers reused sources and unchanged tracked tree |

Host suite groups: ABI 9, RNG 12,364, flags 15, math 14, endian 10 checks.
Successful CTest cases: `original_abi`, `original_rng`, `original_flags`,
`original_math`, `original_endian`, `original_source_guard`,
`original_source_guard_rejects_change`.

Native functions included: `RndInit`, `Rnd`, `fRand0_1`, `fRand1_1`,
`fRandSeed0_1`, `fRandSeed1_1`, `FlagBitChk`, `hermite`, `IPOW`.
Their verified execution is on both hosts and on actual ARM32 and ARM64 Android
processes. The Retroid was connected after the initial Phase-1 report. Tests and
builds were repeated before the local baseline commit. Native test logs are in
ignored `port/build/device-armeabi-v7a.txt` and `device-arm64-v8a.txt`.

## Toolchain and resolved local difficulties

- AGP 8.13.2 / Gradle 8.13 / NDK r28c 28.2.13676358 / SDK 35 /
  Build Tools 35.0.0 / SDK CMake 4.1.2 / Ninja 1.12.1.
- The locally used build JDK is Android Studio's JBR 21.0.8. JDK 11 on the
  ordinary PATH is insufficient for AGP 8; explicit `JAVA_HOME` resolves this.
- The checkout is NTFS case-sensitive. MSBuild path lowercasing generated
  invalid dependency paths. Host builds use Ninja instead.
- SDK CMake 3.22.1's Ninja 1.10.2 falsely reported an existing generated source
  missing in `.cxx/Debug`. The same build's dry-run succeeds with Ninja 1.12.1.
  CMake/Ninja 4.1.2/1.12.1 are therefore pinned for Android, not the older bundle.
- Gradle and adb local process connections, Gradle cache locks and MSVC's PDB
  helper required approved execution outside the sandbox. These were build
  environment constraints, not worked around with fake compiler results.
- AndroidX runner 1.6.2 was absent from the offline cache; the online build
  fetched it from the configured repositories and succeeded.
- Installed mixed SDK metadata emits CXX5304 XML-version warnings, but both
  real NDK builds and packaging complete. No warning was treated as proof of
  runtime compatibility.

Lint's eight warnings concern older pinned target/compile/tool/test versions,
missing ChromeOS x86_64 (outside the requested Android ABI scope), backup-policy
metadata, a diagnostic launcher icon and a hardcoded diagnostic label. Lint
checks were not disabled or baselined. Release UI, target SDK and backup policy
must be settled before distribution; this foundation APK is not a release.

## All new project files

Paths are relative to the repository root. Build products and caches are ignored.

```text
port/.gitignore
port/CMakeLists.txt
port/CMakePresets.json
port/README.md
port/android/settings.gradle.kts
port/android/build.gradle.kts
port/android/gradle.properties
port/android/gradlew
port/android/gradlew.bat
port/android/gradle/wrapper/gradle-wrapper.jar
port/android/gradle/wrapper/gradle-wrapper.properties
port/android/app/build.gradle.kts
port/android/app/src/main/AndroidManifest.xml
port/android/app/src/main/java/org/re4/port/FoundationActivity.java
port/android/app/src/main/java/org/re4/port/NativeChecks.java
port/android/app/src/androidTest/java/org/re4/port/OriginalCodeTest.java
port/cmake/OriginalSources.cmake
port/compat/include/types.h
port/include/re4/port/byte_order.h
port/include/re4/port/contracts.h
port/include/re4/port/original_math.h
port/include/re4/port/verification.h
port/src/verification.cpp
port/platform/android/jni.cpp
port/tests/test_main.cpp
port/tests/source_guard.cmake
port/tests/source_guard_negative.cmake
port/scripts/build-android.ps1
port/scripts/test-host.ps1
port/scripts/test-device.ps1
port/docs/abi-strategy.md
port/docs/runtime-boundaries.md
port/docs/graphics-settings.md
port/docs/phase1-status.md
```

## Outstanding work and next milestone

Implement the stackful scheduler and typed callback adapters from the
original declarations/call sites, with retained local-variable/nested-call state,
slot ordering, sleep, chain and cancellation tests for both ABIs. Resolve pool
layouts and pointer-bearing native views before compiling a larger engine unit.
Do not add manual room behavior. Original data import remains gated on obtaining
the matching two-disc data set outside the public repository.

Graphics settings requirements are recorded in `graphics-settings.md`: runtime
menu, MSAA Off/2x/4x, AF 2x/4x/8x, nearby/aimed-object DoF On/Off, persistence,
backend-neutral requested/effective state and actual capability validation.
No settings UI or effects are claimed implemented.
