# M37: r100 original room effects

The actual Disc1 r100 EFF contains17 SST banks and236 records; its EAR contains14 volumes. All236 immutable300-byte records are retained and enabled. Registration follows the source CORE-before-room texture ownership. No special profile is replaced with a guessed billboard.

| Source | Actual records | CPU and draw path |
|---|---:|---|
| Esp00 |113| Original CommonMove/ANM, channel/depth/blend/geometry flags and independent mask ANM |
| Esp04 |1| Original screen jitter consumes two global Rnd calls; source s16 tile grid uses542/sx+2 and448/sy+2, whole texture per tile |
| Esp06 |4| Actual CORE path10, signed speed/random spread, original path scale/rotation, reverse cached interpolation, loop waits and accumulating byte StopFrame randomness |
| Esp08 |8| Authored tiling/scroll; mask31 independent texture stage; actual SST4 record26 uses CommonShimmer type1/power255 and original framebuffer indirect path |
| Esp0A |3| Original fifty predicted id0 copies; these assets specify Life0 and retain their source infinite lifetime |
| Esp0B |3| Source camera-facing offsets and global random draws, including zero-amplitude draws |
| Esp15 |1| Original camera-relative weather volume, three extra initialization draws, indoor fade, depth wrap and source Work8 deletion distances |
| Esp46 |1| Source Filter03 request, never a sprite substitute |
| Esp48 |99| Original fdlibm sine phase, per-axis amplitudes/frequencies and history subtraction |
| Esp4E |3| Source Cloth::Set120 settle moves, pinned grid, original spring/normal/wind update and shared global Rnd; independent cloth mesh renderer |

`RoomAreaEffects` implements `EffAreaUpdate`, `EffSetAreaState`, `SetSstDispFlag` and `SstSet`. Player coordinates receive the source+100 height offset; the exact source area test retains its own+100 lower-bound allowance. Area exits delete their owned particles/controllers; globals survive. Clearing a display flag prevents subsequent creation without deleting existing effects, matching the original. The script's17/18 switches therefore affect exactly their original SST owners.

All controllers advance before all sprites, matching `EspgenMove` then `EspMove`. Every instance shares the room's visual Rnd stream and512-particle pool budget. Global effects receive the original200-frame warmup. Android preparation supplies a destination-local follow camera rather than using the old room's active camera or the origin. Texture pixels and immutable room data stay on the loader path; normal ticks do no file reads. Mask state advances incrementally rather than replaying its complete history each frame.

The r100 pond remains the separate original `setTexRender` path:64x64 EST0 target, RepeatType1 and SMD works0/1 with alpha240, shader2, refraction power30 and ratio128. The new SST path does not duplicate that target.

`tools/test_m37_room_area_effects.cpp` checks the actual raw inventory and area boundaries, source17/18 lifecycle, exact screen jitter/grid, all area transitions, masks/shimmer/cloth and pond parameters. Its independent44-follower source oracle checks1500 frames of exact coordinates and global random draws, including reverse path lookup, loop waits and byte overflow. The latest actual-asset run passed7,918,830 checks with387,308 quads,73,920 follower observations and138,000 weather observations. An additional500-frame moving-camera oracle checks exact weather wrapping, depth alpha and native angular updates. Cloth CPU and GPU rendering have separate source-oracle checks.

These checks establish the listed actual source paths and asset compatibility. They do not establish physical Android-device rendering, timings, or every possible combination of unrelated pool-slot reuse. The general camera adapter remains the existing bounded follow-camera implementation; this patch does not replace the complete original camera system.
