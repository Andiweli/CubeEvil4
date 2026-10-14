# M37: original new-game engine scenes

The M36 title opening MP4 was followed directly by controllable Leon in r100. The original `st1/r120.cpp::R120Event` instead runs a chain of authored engine scenes after `movie/opening.sfd`:

| Step | Original source | Disc 1 event | Frames / cuts | Completion |
| --- | --- | --- | --- | --- |
| Drive to the village | `Evt_R120S00_Func` | `Evd/r120s00.evd` | 1,970 / 9 | Continue to s01 |
| Car stops; Leon exits | `Evt_R120S01_Func` | `Evd/r120s01.evd` | 2,360 / 15 | Room jump r100 at (-109450, -515, 820), yaw 0 |
| Officers at the ravine/car | `r100_StartEvent`, `Evt_R100S40_Func` | `Evd/r100s40.evd` | 1,175 / 8 | Place Leon (-99685, -484, -1343), yaw 2.246 |

The durations above count the original inclusive FCV end frame. They are not invented cinematic delays. The scene player uses the existing 30 Hz native clock and each real EVD camera/model/face animation, original LIT and DSP stream. The entry events have 94/120/89 embedded assets and 124/142/102 executable packets. Original voice streams are block 1, numbers 30/31/15 respectively.

The original MSB-first scenario flag `SCF_R120_EVENT_CANCEL` is index 59 (`Scenario_flg[1]` mask 0x10). R120 clears it for a new opening. Skipping the FMV or either car scene sets it and skips every subsequent entry scene; it does not replay the skipped audio or expose a test player. R100 sets room-save flag 13 before scheduling its entry task and never replays it on ordinary re-entry.

`OriginalEvent` now applies the r120 room callbacks as well as the event packet stream. `EventCarInit` selects the authored window light masks and depth mode 1, and sets the original car parts 0x12–0x17 and 0x0f to scale zero. `pl0010` and `evm0000` select light mask 0x20. The special glass model's alpha is 192 in s01 cut 6 and restored to 255 in cut 7. Original event-model display flags are applied with their actual meaning: `be_flag |= 2` makes a model visible. Replaced SMD models and the original `DPF_SCR` background state are exposed to SceneGL.

Fades use the original integer alpha interpolation: s00 holds black until cut 0/frame 120, fades in over 60 frames, then fades out from cut 8/frame 50 over 60 frames. S01 fades in over 150 frames and out from cut 14/frame 370 over 215 frames. The r100 entry fades in over 30 frames. The presentation expands these fades to the Android viewport.

The two actual s01 `Focus` packets are decoded as the original `DatFocus` layout: near and far 64-key fixed Hermite curves at offsets 0 and 0x404, with levels at 0x808/0x80c. The native evaluator preserves the original float operation order and requests are cleared every frame as `Filter01Trans` does. A camera cut clears the focus data pointer. Focus parameters alone are not proof of a matching depth-of-field GPU pass.

Validation is provided by `tools/test_m37_opening_event.cpp`: all 5,505 authored frames and 32 cuts, actual speech/MDT data, finite camera and original poses, source car-part scales/light masks/display flags, integer fade oracle, independent raw-key focus oracle, pause/reset and cancellation. The integrated Android/JNI sequence and actual EFM/particle rendering have separate checks recorded in the M37 validation report.

The driving light callback attaches the first alive kind-1 light to `obm3000c` in cuts 1 and 8. Its original light-local position and part number are resolved against the actual animated parent skeleton every frame before light selection; the regression compares those world coordinates with an independent raw-LIT oracle. The original car mirror uses `TexRenderModAddOtMirror`/`TexRenderModAddOt` in selected cuts. That framebuffer path must be judged against the final renderer implementation; displaying the original models by itself does not establish complete GameCube rendering parity. All game regions, costume substitutions and later chapters are outside this opening regression's claim.
