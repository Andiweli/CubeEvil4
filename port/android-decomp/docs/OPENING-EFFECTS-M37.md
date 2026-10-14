# M37: original opening effects

The actual Disc1 EFF banks of `r120s00`, `r120s01` and `r100s40` are evaluated, including their cut identifiers, record order, explicit generators and pre-run markers. The event EFF files contain no local texture table. Their cards reference the room EFF bank; lookup follows `eff_sys.cpp::espTexRegist` registration order (CORE, room, event; the first existing owner wins).

| Original path | M37 behavior | Source |
|---|---|---|
| `FC` effect models | All 94 driving and 192 exit records retain their model-group identity. `Efm04` uses scale `0.005`, native `DEG2RAD`, immediate first `move`, authored physics/colour/lifetime and the actual original model matrix routines. The model index addresses the room SMD group. | `esp_efm.cpp::EfmSeqSet/EfmSetObj04`, `obj04.cpp::cObj04::move` |
| `00` opening cards | Original billboard/oriented cards, including the authored 44,788.5625-unit foliage card. Flags `100000`, `200000` and `400000` retain alpha-test, depth-write and draw-order meaning. | `esp.cpp`, `esp_sub.cpp::EspCommonTrans` |
| `08` | Original tiled repeats and integer scroll velocities. | `esp08.cpp` |
| `0A`, type 0 | Up to fifty source-path copies, frozen speed/size increments/colour envelopes, original texture animation and source `Work8[0]` lifetime (110 in these assets). The temporary predictor advances with the source `CommonMove` envelope. | `esp0a.cpp::SetFreeWork` |
| `11`, type 1 | Live CORE cut 8 light 0 requests, parent pose, radius `size × 10` and the original colour multiplier; the effect has no billboard draw and no `AnmMove`. | `esp11.cpp` |
| `18` | Original direct quad geometry with local Z=1, texture-only flips, raw floating-point alpha and eight framebuffer layers. | `esp18.cpp` |
| `1B` | Actual bilinear patch from `GetPosXY`, subdivided `Work8[0]+4` along each axis. This source function is bilinear, despite the original “spline” name. | `esp1b.cpp`, `esp_sub.cpp::Esp1b_SpTrans/GetPosXY` |
| `48` | Original random initial phase, phase increment `0.01`, per-axis sine amplitude/frequency, and subtraction of the previously added angle before each update. Uses the original fdlibm sine implementation. | `esp48.cpp` |
| Parts `FD` | Original 512×448 screen coordinates and screen-specific flips; the actual three banks have identity sequence matrices. | `esp_sub.cpp::EspSeqSet/EspCommonTrans` |
| Generator `02` | Authored CORE path, source interval/burst/lifetime/seed, random path coordinate and explicit path scale/rotation. | `espgen02.cpp` |

Models and effect lights are retained in the CPU event state and published through separate model/light requests. They never become substitute textured billboards. Cut changes clear the event-owned effect state; UI pause does not advance it. GPU context recreation preserves the CPU sequence clock.

`tools/test_m37_opening_effects.cpp` evaluates all 5,505 opening frames. It independently reconstructs actual `FC` record movement, source angles, scale and colour history, verifies exact sine offsets and their history, checks trail lifetimes, and exercises screen quads, original light references and bilinear patches. `tools/test_m37_opening_event.cpp` separately checks the complete authored cameras, actor motion, EVD callbacks, fades, source speech, pause and skip behavior. GPU rendering is checked separately; these CPU checks do not establish physical-device rendering or performance.

The actual-asset CPU run passed5,329,415 checks:61,900 model poses,8,280 sine observations,22,000 trail copies,675 light observations,84,483 screen quads and37,440 bilinear patches.
