# RE4 native foundation

Strategy A: compile original RE4 code with a separate native compatibility layer.
This directory contains the verified build foundation and Phase-2 stackful
scheduler. It is not a playable game. It loads
no GameCube assets and implements no room, actor, event, renderer or SDK stubs.
The original `src/`, `include/`, configurations and master history are unchanged.

## Layout

- `CMakeLists.txt`: shared host/Android build.
- `compat/include/types.h`: exact-width original primitive names on both ABIs.
- `cmake/OriginalSources.cmake`: reviewed source fingerprints and verbatim extraction.
- `include/re4/port/`: byte-order helpers, verification API and future boundaries.
- `src/verification.cpp`, `tests/`: the shared checks and source-identity tests.
- `src/scheduler.cpp`, `src/stack_context.cpp`: native task policy and real stacks.
- `third_party/boost_context/`: pinned, licensed ARM context assembly.
- `platform/android/`: JNI diagnostics bridge.
- `android/`: pinned Gradle project, verification Activity and device tests.
- `scripts/`: checked build/test commands for Windows.
- `docs/`: decisions, graphics requirements and recorded verification status.

## Code actually reused

`src/game/rnd.cpp` is compiled directly, unchanged: `RndInit`, `Rnd`, `fRand0_1`,
`fRand1_1`, `fRandSeed0_1`, `fRandSeed1_1`. `include/flag.h` and `rnd.h` are copied
unchanged into the build directory. Only `FlagBitChk` is exercised; the writing
flag macros still truncate native pointers and are intentionally unsupported.

`hermite(f32*, f32*, f32)` and `IPOW(f32, int)` are extracted from
`src/game/math_sub.cpp` at configure time, preserving the complete definitions
and source locations. Their expressions are not reimplemented. The extractor
is deliberately restricted to these reviewed functions. Source fingerprints
normalize CRLF to LF so Linux and Windows checkouts agree. Changing an approved
source causes configuration to fail until its fingerprint is explicitly reviewed.

Phase 2 also extracts `GetStackSize`, `TaskSuspend`, `TaskSignal`, `TaskStatus`,
`SetTaskModelPtr`, `SceSleep`, `SceExit`, `cSceSys::checkCTaskRange` and the six
TASK kind accessors verbatim. These use native control metadata and actual stack
transfers, not pretend Dolphin threads. `TaskSchedulerMain`'s policy and scenario
OT ordering are adapted in `NativeScheduler`; their entire engine units are not
claimed compiled. See [Phase-2 evidence and limits](docs/phase2-status.md).

Only selected original headers are exposed. The legacy include tree must not
shadow Bionic, the host CRT or the modern C++ standard library. Original RNG
type-punning is built with strict-aliasing optimization disabled on GCC/Clang;
fast-math and implicit FMA contraction are disabled. These tests do not establish
general PowerPC floating-point equivalence.

## Prerequisites and pinned Android toolchain

| Component | Version |
| --- | --- |
| Gradle wrapper | 8.13 |
| Android Gradle Plugin | 8.13.2 |
| JDK | 17 recommended; Gradle 8.13 also supports 21 used locally |
| Android SDK platform / build tools | 35 / 35.0.0 |
| Android NDK | 28.2.13676358 (r28c) |
| SDK CMake / Ninja | 4.1.2 / 1.12.1 |
| Native standard | C++17 |
| Android minimum API | 21 |
| Packaged native ABIs | armeabi-v7a AND arm64-v8a |

Install missing SDK components with Android Studio's SDK Manager. The build
script does not install packages or accept licenses automatically. Set
`ANDROID_HOME` and `JAVA_HOME` or pass paths explicitly. The ordinary Java on
PATH may differ from the configured build JDK.

The official Gradle 8.13 distribution SHA-256 is pinned in wrapper properties;
the wrapper JAR SHA-256 is
`81a82aaea5abcc8ff68b3dfcb58b3c3c429378efd98e7433460610fecd7ae45f`.
Sources: [AGP compatibility](https://developer.android.com/build/releases/agp-8-13-0-release-notes),
[Gradle checksums](https://gradle.org/release-checksums/).
Tool versions are pinned for repeatable inputs; byte-for-byte APK reproducibility
across machines is not claimed (debug signing and packaging metadata can differ).

## Host build and tests

On a host with a working compiler and Ninja:

```sh
cd port
cmake --preset host
cmake --build --preset host
ctest --preset host
./build/host/re4_core_tests
```

On Windows, initialize an x64 or x86 Visual Studio Developer Command Prompt
before using Ninja. This checkout has NTFS case sensitivity enabled; Ninja
avoids MSBuild's observed path-lowercasing problems. From the repository root:

```powershell
.\port\scripts\test-host.ps1
```

Do not force a 64-bit pointer layout: 32-bit hosts are also permitted when their
compiler/toolchain is installed. The shared types always keep `u32` at 4 bytes.
On compatible GCC/Clang hosts, configure a separate build with
`-DRE4_ENABLE_SANITIZERS=ON` to enable address/undefined-behavior sanitizers.

## Android build

From the repository root on Windows:

```powershell
.\port\scripts\build-android.ps1 -SdkRoot 'C:\path\to\Android\Sdk' -JavaHome 'C:\path\to\jdk'
```

The script first builds native test executables and the JNI library for **both**
ABIs. It then builds the debug APK, instrumentation APK and Android lint report.
`-NativeOnly` needs neither Java nor Gradle. `-Offline` applies to Gradle and only
works when all dependencies are already cached. No hard-coded local paths are
stored in tracked files.

On another host, configure each ABI with the NDK toolchain directly:

```sh
cmake -S port -B port/build/android-arm64-v8a -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_HOME/ndk/28.2.13676358/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 \
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
cmake --build port/build/android-arm64-v8a
# Repeat in a different build directory with -DANDROID_ABI=armeabi-v7a.
cd port/android
./gradlew :app:assembleDebug :app:assembleDebugAndroidTest :app:lintDebug
```

On Unix checkouts where the wrapper is not executable, use `sh ./gradlew`.
Outputs: `port/build/android-<abi>/re4_core_tests`,
`port/build/android-<abi>/libre4_port_jni.so`, and
`port/android/app/build/outputs/apk/debug/app-debug.apk`.

## Actual device execution

Cross-compilation is not device execution. The same suite can run as a standalone
NDK executable, as the Activity's JNI diagnostics, or as instrumented tests.

```powershell
.\port\scripts\test-device.ps1 -Adb 'C:\path\to\adb.exe' -Serial 'device-serial'
.\port\scripts\test-app-device.ps1 -Adb 'C:\path\to\adb.exe' -Serial 'device-serial'
```

The standalone script requires a device that supports both requested ABIs;
unsupported ABIs fail explicitly, never count as passes. For a device with only
one ABI, select it explicitly and record the missing ABI separately. It records
device/version, binary hash, actual pointer width and compares scheduler traces.
The app script installs the default dual-ABI APK with `adb install --abi`, checks
`primaryCpuAbi` and passes an expected pointer width to the nine JUnit tests.
It fails on missing/unsupported ABI or a failed instrumentation result even
when adb returns zero. It leaves the last requested ABI installed (default ARM64).

For JNI/instrumentation validation, build/install each ABI separately:

```sh
cd port/android
./gradlew -Pre4.testAbi=arm64-v8a :app:connectedDebugAndroidTest
./gradlew -Pre4.testAbi=armeabi-v7a :app:connectedDebugAndroidTest
```

`re4.testAbi` restricts a test build only; the default APK contains both ABIs.
Use `ANDROID_SERIAL` when more than one device is connected. A dual-ABI APK
normally executes ARM64 on a dual-ABI device, so that alone does not test ARM32.

## What the checks prove

Five foundation groups cover primitive widths/native addresses, fixed RNG byte and
float vectors, 32-bit seed overflow, replay/ranges, original top-bit flag
numbering, exact Hermite/power cases and bounded unaligned big-endian decoding.
CTest also checks source identity and deliberately changes a build-directory
copies to prove rejection of changed RNG, scheduler, scenario and TASK methods.
Four scheduler groups prove nested stack retention, exact sleep visits, main/OT
ordering, counted suspend/signal, chain/exit, cancellation, generation handles,
module quiescence and FP state. Android additionally pins d8-d15 in two tasks
over 64 yields each. Main callback return without TaskExit/TaskChain fails
explicitly; scenario callback return removes its OT entry, as in the original.
There is no cooperative replacement for interrupt-preempted slot 4: requesting
it fails explicitly. ASan/UBSan require context-switch hooks and are unavailable.
Supported host context backends are Windows x86/x64; no unverified Unix host
fallback is claimed. RNG vectors are derived from the source recurrence;
they are not independently captured GameCube traces. Existing GameCube data and
a reference run are still needed for broader differential validation.

See [runtime boundaries](docs/runtime-boundaries.md),
[ABI assessment](docs/abi-strategy.md),
[graphics requirements](docs/graphics-settings.md) and
[recorded Phase-1 results](docs/phase1-status.md) and
[Phase-2 report](docs/phase2-status.md).
