# Native runtime boundaries

The current resource boundary is Phase 3c: original DC commands/conditions run
against bounded native MRAM and CPU-backed logical ARAM. Resource handles and
leases preserve ownership across relocation/clear. Block data can be cached and
restored; `BLOCK_CREATE` and full heap/model/REL/input integration are still
pending. See [Phase-3c status](phase3c-status.md) and
[next integration contracts](phase4-boundaries.md). Earlier phase descriptions
below explain the progression and do not override these current limits.

The approved architecture is original game code plus a compatible native
runtime. Phase 1 implements only fixed-width primitive types, scalar endian
decoding, a small original-code target and its platform verification harness.
Phase 2 implements `NativeScheduler` in `include/re4/port/scheduler.h`; Phase 2b
adds explicit slot-4 slices, owned Yz2 decode jobs and a single file-I/O worker.
`contracts.h` retains proposed abstract module/asset boundaries. Neither those
systems nor their success-returning substitutes are implemented.

## Module registry

The original `ROOM_DATA` in `include/room_data.h` contains `u16 rel_file` and
`void (*)()` Init/Main callbacks. `src/st1/st1.cpp:setTbl` registers these;
`src/game/roomdata.cpp:execInitFunc/execMainFunc` dispatch them. Thus native room
callbacks preserve these exact function signatures. `rel_file` is a FileTbl
index, distinct from an OS REL module ID and an enemy's module identifier.

Generate registration from `config/G4BE08/modules.py`, per-REL configuration,
room tables and the actual import graph. Preserve repeated source variants and
real cross-room dependencies, such as `r100` using `r120::EventCarInit`. Resolve
duplicate entry/helper symbols per their actual scopes before linking modules.
No manually authored room behavior or replacement enemy placement is needed.

Initial implementation should keep compiled native code resident. Activation,
constructor registration, function ownership, reset and state preservation still
need implementation. `stopRelData/restartRelData` preserve original module BSS;
native objects cannot be preserved by blindly copying arbitrary ELF BSS. Do not
double-run C++ constructors. Missing modules and unresolved imports must fail.
Current abstract methods report activation failure by an exception; absence of
a room callback is represented by an empty optional, not a dummy function.

## Stackful scheduler

`include/scheduler.h` defines 18 slots, ISR slot 4, sleep/suspend state and
`void (*pFunc)(int)`. `TaskFunc` itself is untyped `void*`; `TaskExec_hook` narrows
its argument to `int`. Those are original ABI hazards, not native signatures to
copy with reinterpret_cast. Typed trampolines must be derived from each real
callback declaration/use. `bindTask` captures the exact callback and argument
types into a native `TaskEntry`; there is no function-pointer reinterpret_cast.
For example, `CardMainTask(int)` (`card.cpp:2157`) and
`gameDiedemo(DIEDEMO_WORK*)` (`game.cpp:1075`) need distinct typed bindings.
Neither entire game unit is linked by this milestone.

`NativeScheduler` uses one owner thread and separate retained stacks. Android
uses pinned Boost.Context assembly, Windows hosts use OS Fibers. Main slots
0..3, scenario reverse OT/LIFO/head restart, sleep visit counts, counted suspend,
chain, exit and abrupt cancellation are tested on both ARM ABIs and hosts.
Main callbacks must explicitly exit/chain or loop; unexpected return is an error.
Scenario return performs original OT cleanup. Generation handles replace unsafe
long-lived TASK*/SCE_TASK* identities. A module lifetime cannot be invalidated
while its code is on an active task/caller chain; idle tasks are cancelled first.
Full callback ownership and transitive module dependencies still need the real
registry. Keep native code resident until that integration is proven.

Native metadata does not emulate OSThread's binary layout. Original pure control
functions and SceSleep/SceExit are compiled verbatim against narrow adapters.
There are no pretend OSCreateThread, GX or semaphore functions. ISR slot 4 uses
execBackground/runBackground/backgroundCheckpoint; ordinary exec/visit still
reject it. Gameplay order is unchanged. Retrace callbacks request pause
atomically; the full VI/Surface frame bridge remains open. See
background-design.md and phase2b-status.md for proof and limits.

Native stacks default to 256 KiB and are configurable, with Android guard pages;
full-engine budgets still need measurement. Cancellation, chain and explicit exit
discard the old stack without invoking automatic C++ destructors, matching the
original abrupt OS control transfers. Port-owned entry captures are released on
the caller stack. Do not rely on automatic RAII cleanup across those operations.
Android suspend/Surface destruction is not equivalent to killing the game's tasks.

The verification mutex protects only concurrent test invocations of the
original global RNG; it is not a game scheduler implementation.

## Original data import and runtime loading

`OriginalAssetSource` is an import-time raw-file boundary only. Missing files
must produce an error; an empty vector is valid only for a real zero-byte file.
It specifies no DAS/DRS/ESL/EVD schema. Derive schemas from actual source readers
and verified original bytes, using existing tools as references.

Authority: `src/game/read.cpp:ReadAreaData/GetDataExt/readEmData`,
`src/game/block.cpp:roomInit/getBlockWork`, `src/game/datactrl.cpp`,
`tools/extract_orig.py`, `tools/drs.py`, `tools/motion/archive.py` and `yz2.py`.
Keep provenance for both discs, original paths, IDs, tag occurrences and archive
indices. Preserve original bytes and explicitly list unsupported formats.

Separate wire layouts from native views. `BigEndianView` reads bounded scalars;
it performs no pointer relocation and does not claim to parse any RE4 format.
Runtime DVD queues and MRAM/ARAM transitions require separate implementations
with original completion/error semantics. AsyncFileReader performs real bounded
filesystem reads on one worker; startFileReadPump polls it without blocking
frames. Its job owns buffers through cancellation acknowledgement. Yz2 setup
is adapted from original C; the PPC kernel is translated and reference-checked.
Neither component claims full decodeData/cDvd/archive/asset import integration.
No original binaries or assets belong in tracked files or APK inputs here.

## Android application boundary

Phase 1 uses a small ordinary Activity to show real JNI verification results.
It supplies no dummy render loop or gameplay input. The eventual game lifecycle,
Surface and controller adapter can use GameActivity without changing core
targets; that integration follows the scheduler milestone. Native renderer and
audio backends are not linked yet. The present diagnostic Activity must not be
represented as a playable game or the future graphics menu.

Phase 1 uses `c++_static` because each app process loads one native library and
the standalone tests are separate executables. Keep one compatible C++ runtime
for the eventual engine. If multiple native DSOs exchange C++ objects, allocations
or exceptions, use one shared libc++ runtime with one toolchain/ABI configuration
instead of independent static runtimes. This decision precedes module activation.
