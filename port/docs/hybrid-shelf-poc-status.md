# Original shelf callbacks: verified Vulkan presentation proof

Baseline: `0653b9295a96932be9179f8ee5f71b72b87f9188`, `android-port`.
Verified on 12 October 2026. This is the bounded callback proof, not completion
of H1, a playable room, `ScenarioRoomInit`, `SceSetItemEvent` or `gameMainLoop`.

## Decision about the GLES donor

The read-only donor is `E:\Development\Android\RE4\RE4-Android-Decomp`.
Its `app/src/main/cpp/actor_gl.cpp::CreateGenericActorGl/DrawGenericActorGl`
is a useful presentation boundary, but expects `StaticScene`, `SkinCorner`,
`Matrix34`, bind centers, `GenericActorStyle` and its own shader/lighting/texture
helpers. It owns GL programs, buffers and textures in global owner maps; Android
integration additionally needs EGL/context-loss handling. Its current CMake
configuration rejects ARM32.

No donor implementation was copied in this proof. There is no measured GLES
performance comparison. Introducing this second backend did not offer a
demonstrated benefit for two already supported original models. The authorized
Vulkan alternative was used: one existing GPU owner, existing GX/TPL/material
transport and a fenced pose-buffer update. Vulkan was not replaced.

## Original functions and native adaptations

`generate_shelves.py` extracts both complete bodies from the unchanged sources:

- `src/st1/r103.cpp:128`, `r103_openShelf_main(SHELF_SMD*, int)`.
- `src/st1/r106.cpp:268`, `r106_openShelf_main(int, int)`.
- `src/game/scroll.cpp:357`, `SmdGetObjPtr(u32)`, through the existing scroll
  generator and native scroll pool.

The manifests fingerprint both callback sources, `scroll.cpp`, `scroll.h` and
`model.h`. Generation preserves the loop, floating-point constants, branch
conditions, `be_flag |= 0x20`, parameter types and call ordering. PPC `r10`
binding becomes a native pointer local; `pa2 = pa` remains. The compiler-only
empty asm is removed. No pointer is passed through `int`.

The three `SHELF_SMD` parameter records are extracted from r103. The r106 work
IDs are extracted from its actual switch cases. Original selections are:

| Callback | Selection | Original SMD work IDs |
|---|---:|---|
| r103 | 0 / 1 / 2 | 0x57/0x58, 0x59/0x5a, 0x5b/0x5c |
| r106 | 0 / 1 | 0x29/0x2a, 0x2b/0x2c |

The SMD getter retains its original body. Native pool ownership replaces the
GameCube address-range test. Its two diagnostic debug flags are native scalar
views, defaulting to normal non-tool mode. Group/invalid-ID errors remain errors.

`ShelfCallbackSession` uses the existing imported-file loader, original SMD
registration, CNS-sized pools, original model initialization and native parts.
Callbacks mutate those actual owned parts. Their parameter and model borrows
live on the session/frame owner, not on abruptly cancellable task stacks.
Native module-generation tokens quiesce callbacks before the borrows and pools
are retired. The tests reject early model release and reuse retired callbacks.

`SceSleep` calls the already compiled original wrapper and native stackful
scheduler. The matrix update occurs on the CPU tick using the existing original
model/parts routines. Snapshot presentation does not advance the callback.
The existing skinning/GX decoder reads the current matrices and original BIN/TPL
bytes. Both selected leaves produce 44 triangles in total.

The callbacks request `SndCall(6, 0x1a, 0, 0, 0, 0)`. Here that platform boundary
captures the typed request into explicit diagnostic evidence. **The original
SndCall body, audio mixing and audible playback were not implemented or claimed.**
There is no fabricated playback handle or completion result.

## Observable execution

The animated branch performs 30 increments and 30 original `SceSleep(1)` calls.
It returns on scheduler visit 31. A host/native suspend test adds three paused
visits and returns at visit 34 without changing the frozen geometry.

All five selections end at approximately `(-1.91986167, +1.91986167)` radians.
The already-opened original branch directly stores approximately
`(-1.91986215, +1.91986215)`. The difference, about `4.76837e-7` radians, is the
original repeated float-addition versus literal-store behavior, not an ARM error.
The Python reference rounds each addition to float32. Host x86/x64 and Android
ARM32/ARM64 callback/geometry reports agree after excluding pointer-width labels.

Each data-verification case runs animated, already-opened, cancellation after
seven visits, and a fresh complete animated session. Forty final case runs
across four platforms exercise 160 sessions and 640 C++ scheduler/lifetime
assertions, plus Python source-state checks. All pools/stacks retire cleanly.

## GPU proof and its limits

This is an explicitly labeled **texture-only DEBUG presentation of two original
model leaves**. It uses a fixed diagnostic camera derived from their original
geometry bounds. It does not display the cabinet body/full room or claim an
original room camera, LIT/SAR lighting, item interaction or running GameTask.
No global brightness multiplier or replacement light was introduced.

Native diagnostic playback is paced at most 30 scheduler visits per second,
one visit per available presentation frame. Slow rendering stretches diagnostic
wall time; this is not the production Android/gameplay frame-clock contract.

The new `updateSceneGeometry` preserves buffers, textures and descriptors while
uploading changed native vertex/material data. It rejects topology, resource,
sampler/mipmap or descriptor-binding changes. `vkDeviceWaitIdle` protects these
small diagnostic uploads; this conservative synchronization is not a final
high-performance animation renderer.

Retroid `dbdcacc`, Android 13/API 33, Adreno 650:

| Model pair | CPU visits | Closed -> open changed pixels | EFB |
|---|---:|---:|---|
| r103 selection 0 | 31 | 40,322 | 512 x 448 RGBA |
| r106 selection 0 | 31 | 33,178 | 512 x 448 RGBA |

Two GPU runs per room and ABI captured visits 0, 15, 30 and 31: 32 actual GPU
readbacks plus eight device screenshots. Corresponding readbacks are byte-identical
across repeats and ARM32/ARM64. Reopening each room's Activity in the same process
proves surface teardown/recreation. Logs report `models=0 stacks=0` on actual
native close; `graphics_domains=1` during presentation.

Khronos validation is enabled: zero errors, one warning per surface concerning
identity pre-transform versus the device's rotated surface. PID-filtered vendor
logs still contain `AdrenoUtils: Unknown Format 0` / `Memory Layout input parameter
validation failed!` on both ABIs. These are not reported as clean driver logs;
their cause remains open despite valid, matching rendered output and no crashes.

Private evidence is outside Git:
`E:\Development\Android\RE4-Data\hybrid-shelf-poc`.
Examples: `gpu-arm64-v8a-r103-1-tick0.png`, `...-tick30.png`,
`gpu-arm64-v8a-r106-1-tick15.png`, corresponding JSON/RGBA files,
`display-*.png`, `log-*.txt`, source hashes and `result.json`.

## Verification actually executed

| Test | Final result |
|---|---|
| Windows x86/x64 CMake builds | Passed; VS compiler environment initialized |
| Existing CTest suites | 27/27 on each host |
| Original-data callback cases | 10 per host/Android ABI; all five selections repeated twice |
| Android native tools and debug/test APKs | ARM32/ARM64 built; offline Gradle and lint passed |
| Retroid native core, two runs each ABI | ARM32 13,386 / ARM64 13,387 checks, zero failures; semantic traces equal |
| Retroid instrumentation | 20/20 on each ABI, including controller/JNI, scheduler, modules and Vulkan surfaces |
| Original-data GameRuntime regression | Both hosts and Android ABIs; repeat 2, both initial-disc inputs, new-game/load-save branches, EVD/model/motion/weapon/cloth-init/action/collision/light/save regressions passed |
| Callback GPU output | Both rooms, two runs each ABI, matching readbacks and verified native close |
| Existing r120 GX/Vulkan path | Host primitive/material/camera/TPL oracle passed; two GPU runs per ABI, 229,376 non-clear pixels each and matching readbacks |

All hardware runners acquire `Global\Retroid_ADB_Exclusive` through the existing
PowerShell `System.Threading.Mutex` broker, with timeout and finally-release.
Installation, start, tests, logs, screenshots and package cleanup stay under the
same acquired lock. Every ADB call uses `-s dbdcacc`. No global ADB reset or device
restart was issued.

Initial failures were corrected and rerun: absent VS compiler environment,
missing cached `/EHsc` after compiler rediscovery, premature OT capture before
the diagnostic camera existed, and a test incorrectly requiring the callback's
activation flag on the pre-callback tick-0 image. Original game states were not
altered to satisfy these checks.

The original GameTask still stops at `Rno0=2` / unbound `gameRoomInit`. H1 still
stops at the missing Leon cloth-simulation owner; this proof does not complete
`startUp` or player ticks.

## Reproduction

After the standard host/Android build, run:

```powershell
python port/tools/test_shelf_callbacks.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/hybrid-shelf-poc --host-exe port/build/host-ninja/re4_gx_tool.exe --repeat 2
python port/tools/test_shelf_callbacks.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/hybrid-shelf-poc --host-exe port/build/host-x86/re4_gx_tool.exe --repeat 2
python port/tools/test_shelf_callbacks.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/hybrid-shelf-poc --adb C:/Users/andiw/AppData/Local/Android/Sdk/platform-tools/adb.exe --serial dbdcacc --repeat 2
```

For build/driver evidence see ignored `port/build/shelf-*.log`. Source files and
implementation changes are confined to `port/`; no original assets, donor edits,
master changes, merges, rebases or pushes are part of this work.

Changed complete project files:

- `port/CMakeLists.txt`, `port/config/shelf-sources.json`.
- `port/include/re4/port/shelf_callbacks.h`, `port/src/shelf_callbacks.cpp`,
  `port/src/shelf_original.h`, `port/src/scroll_original.h`.
- `port/tools/generate_shelves.py`, `port/tools/generate_scroll.py`,
  `port/tools/test_shelf_callbacks.py`, `port/tests/gx_tool.cpp`.
- `port/platform/android/graphics_jni.cpp`, `vulkan_bootstrap.cpp`,
  `vulkan_bootstrap.h` in the same directory.
- `port/android/app/src/main/java/org/re4/port/GeometryActivity.java`,
  `GraphicsBootstrapView.java` in the same directory.
- This report. Build-generated original bodies/manifests are reproducible ignored
  outputs; copyrighted resource and GPU evidence files remain in the private data
  directories.

## Technical gain and next boundary

The proof establishes a reusable boundary: compiled original callbacks -> owned
native model/parts state -> original matrix/skinning/GX transport -> existing GPU
resources. It does not require handwritten frame counters or recreated shelf
animations. Other callbacks can use this boundary after their actual object,
actor, camera and resource consumers exist. Texture/material topology changes
require an explicit resource reload rather than being silently ignored.

For this scope, keeping Vulkan avoided a second GPU/context/texture owner and
donor gameplay structures. The additional work is the guarded callback binding,
session lifetime/diagnostic request boundary, presentation snapshots, fenced pose
uploads and verification. A faster GLES renderer or faster completion of the
whole game has not been demonstrated. No next hybrid/gameplay phase was begun.
