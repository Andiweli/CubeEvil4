# Native runtime boundaries (planned, not implemented)

The approved architecture is original game code plus a compatible native
runtime. Phase 1 implements only fixed-width primitive types, scalar endian
decoding, a small original-code target and its platform verification harness.
`include/re4/port/contracts.h` declares proposed abstract boundaries; there is
no implementation, factory, success-returning stub or invocation of them yet.

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
callback declaration/use. The proposed `TaskEntry(void*, function)` boundary is
explicitly a native boundary, not a binary TASK replica.

Implement a tested stackful context mechanism for each supported ARM ABI (or a
deterministic thread handshake first). `TaskSleep/SceSleep` must preserve local
variables and nested calls. Preserve priorities, cancellation, TaskChain,
suspend, stop flags and deterministic per-tick ordering. One logical game token
must prevent simultaneous mutation of original globals. Native stack sizes need
measurement; the original small stacks must not be copied blindly. Android
suspend/Surface destruction is not equivalent to killing the game's tasks.

No `TaskExec`, `TaskSleep`, `OSCreateThread` or other substitute currently exists
in the native library. Implementing a sleeping loop or returning success would
not satisfy this contract.

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
with original completion/error semantics. No asynchronous loader is simulated.
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
