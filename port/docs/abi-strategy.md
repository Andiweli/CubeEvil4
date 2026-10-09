# ARM32 and ARM64 are both supported

Both `armeabi-v7a` and `arm64-v8a` are required build targets; ARM64 is mandatory.
The core code must not require that pointers fit in `u32`, nor require 64-bit
pointers where 32-bit pointers suffice. The same original source selection and
verification suite build for both. Optional host builds may also use either width.

## Rules already applied

- Original integer names have exact widths: `u32/s32` stay 32-bit regardless of
  `long` or pointer size. IEEE `f32/f64` widths are checked.
- Native addresses use pointers/`uintptr_t`, sizes use `size_t`. File offsets
  remain fixed-width values and are validated before addressing buffers.
- The big-endian reader is bounded, unaligned-safe and independent of host byte
  order. Offset overflow is rejected without forming an invalid pointer.
- No native class is claimed to match a serialized GameCube structure.
- No globally forced packing, low-address allocation or fake fixed GameCube
  memory map is introduced.
- Android ABI selection happens in the toolchain/Gradle build, not source casts.

## Does ARM32 make the port easier?

**Yes, in limited areas:** original pointer fields were four bytes. ARM32 keeps
that width, so some pointer-containing records and pool sizes may require fewer
changes. This is an implementation hypothesis until each layout is checked.
Primitive-only original functions do not gain a special ARM32 advantage.

**It does not remove the shared blockers:** both Android ABIs are little-endian;
neither uses the old GameCube C++ ABI, its virtual-table placement, PowerPC
instructions, GX MMIO, ARAM, 0x80000000-tagged pointers or original OS scheduler.
An ARM32 pointer also need not lie inside the GameCube address range. File
offsets and runtime pointers remain different concepts even if both are 32-bit.

Authority in this repository: `include/types.h`, `include/cManager.h`,
`include/em.h`, `include/room_data.h`, `src/game/model.cpp:calcModelAddr`,
`src/game/sce_sys.cpp:scenarioGetOtAddr`, `src/game/scheduler.cpp:TaskExec_hook`.
Android ABI rules: https://developer.android.com/ndk/guides/abis.

## Decision

Keep one portable engine/data/runtime design and two small platform backends
where architecture-specific work is unavoidable (implemented context switching and
SIMD kernels). Do not use ARM32 as a prerequisite or a shortcut that blocks the
ARM64 build. Start with scalar reference kernels and typed native data views.

Future allocations must use actual native `sizeof` and `alignof`, recompute
derived actor/work pool strides and guard sizes/overflow on both ABIs. Native
64-bit state values need explicit thread ownership or synchronization on ARM32;
64-bit loads/stores must not be assumed atomic. ARM32 has a smaller process
address space, so streaming/memory budgets must be measured separately.

A passing dual-ABI crossbuild establishes compilation/linking, not execution
on either CPU. Device tests must record process ABI/pointer width; forcing the
32-bit test build or running the 32-bit executable is required to test ARM32.

Phase 2 uses one scheduler policy and ABI-specific Boost.Context assembly.
ARM32 softfp needs d8-d15 saved despite its integer-register FP calling convention;
the assembly-only macro override enables that. Both ARM backends passed actual
Retroid Android 13 execution, including register probes and identical task traces.
Task/module generations are explicit uint64 values with single-thread ownership;
native stacks use size_t and the device's real page size. TASK's wire offsets
and the scenario OT's tagged 32-bit addresses are not copied into native state.
