# M36 water seam diagnosis and source copy filter

## Reproduction and cause

The uploaded `linien-in-wasser.png` was reproduced with the original r10a SMD/SMX/LIT/EFF and CORE EFF in GLES, without Leon or diagnostic geometry. The newest uploaded log contains r100 only; the earlier 20:44 log includes r10a and matching shallow-water contacts. Removing specular color, shimmer, and finally the entire Esp42 water grid did not remove the line.

A diagnostic magenta framebuffer clear identified the individual uncovered pixels in the original terrain. Object84/work84 uses a coarse boundary at `y=-10000`, `x+z=115000`. Object163/work254 shares the endpoints but has finer BIN vertices such as `(20310,-10000,94685)`, where `x+z=114995`. Both original placements use scale 20 and BIN shift 2. The five-unit difference comes from the actual quarter-unit BIN coordinate quantization. It is not a water triangle-strip gap, a UV wrap seam, a mip derivative, Leon's laser, or a depth-precision overlay.

The source `gx_sub.cpp::bio4_AddBgColor` fills the background with the fog color while fog is active, so changing r10a's active-fog background to black would not reproduce original behavior. The original SMD coordinates and raster gaps remain unchanged.

The final source audit also corrected the independent no-fog case in `scene_gl.cpp`: `Fog.Type==0` clears black even when the inactive fog record retains a nonblack color. This occurs in 45 of the 127 original Cut0 records. The regression checks the same uncovered original terrain samples with a deliberately nonblack disabled fog color, and requires black RGB with alpha 255.

## Corrected water copy

The previous GLES water pass copied the completed room framebuffer at full resolution without the original copy filter. This made the narrow bright fog-colored gaps sharp in the refraction image.

The source behavior is shared by `trans.cpp::GetEfbTex`, `Espgen42.cpp::Espgen42_TransSub`, and `espgen45.cpp::Espgen45_TransSub`:

- `Render_before` installs `Bg_brightness_set(64)` before room rendering, with coefficients `{8,8,10,12,10,8,8}`.
- The seven GX copy coefficients cover three EFB rows: 0/1 above, 2/3/4 current, 5/6 below. Their sums are 16/32/16. RGB is truncated with an integer right shift by 6; alpha is copied from the middle sample.
- `GXSetTexCopyDst(...,mipmap=1)` enables a half-size copy in both axes. The sampled water texture does not use a mip chain.
- The console's fixed 56-row crop and texture-coordinate matrix remove its letterbox. The GLES adapter retains the existing full-phone-viewport mapping and resolves it at half width/height; it does not introduce console letterboxing into the phone view.

The row mapping and integer truncation are corroborated by the primary, hardware-tested [Dolphin EFB copy implementation](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/TextureCacheBase.cpp) and [texture conversion shader](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/TextureConversionShader.cpp). The implementation here uses the frozen game source's coefficients and its own GLES adapter; no proprietary pointers or copied emulator routine are used.

`CaptureEnvironmentScreen` now copies the active draw framebuffer, independently of a caller's unrelated read-framebuffer binding, and renders the filtered half-size image into its own framebuffer. It restores both caller framebuffers, program, VAO, viewport, color mask, all modified enables, and texture/sampler state. As before, its result remains bound on texture unit 2 and the material pass resumes on unit 0. Lost-context names are discarded by `ForgetEnvironmentGl`; the capture resources are recreated lazily. Existing deletion handles the new resources with a current context. No bridge/API change is required.

M36 also corrects `display_brightness_gl.cpp` from seven separate pixel rows to the same three GX rows, with integer truncation and alpha preservation. The CPU coefficient oracle remains unchanged. Default brightness 64 still skips the final display pass, preserving the established default scene framebuffer. The water copy always uses the source pre-render filter 64; the user's configured brightness applies at frame end.

## Verification and remaining limits

`tools/test_m36_water_seams.cpp` passes 221 checks against the real room and source assets. Its independent box-average/three-row pixel oracle checks all RGBA channels, with a one-byte allowance for GLES linear interpolation rounding. It also checks nondefault GLES state, separate read/draw framebuffers, paused pixel equality, fresh-context pixel equality, and the disabled-fog background. The real-data regression identifies 34 uncovered terrain pixels along the boundary and requires their contrast in the water image to remain below 8, with mean below 5. The measured filtered mean is 2.59 and maximum 3.33.

The optional `--1080p` mode passes 222 checks at the uploaded screenshot's 1920×1080 resolution. It identifies 278 original terrain-gap pixels; filtered mean contrast is 2.97 and maximum 5.67. The copy oracle, disabled-fog background, paused frame and fresh-context checks also pass at this resolution.

A side-by-side run on the same 34 corresponding raster-gap samples measured unfiltered mean contrast 30.46/max 33.17 and filtered mean 2.59/max 3.33: about 91.5% less contrast. The same camera and unchanged original terrain were used. A negative control linked against the previous unfiltered GPU implementation fails the real-data seam contrast assertion. The bright dashed line is therefore strongly suppressed by the missing source copy stage; it is not hidden by changing floor geometry or depth offsets.

`tools/test_m35_display_brightness.cpp` passes 69,382 checks across every supported value 34..114, now with exact three-row integer pixel expectations. `tools/check_brightness_reference.py` still passes 567 coefficients against the unchanged original `Bg_brightness_set`.

After the separate event-parent ABI changes, the final integrated 127-room water catalogue still passes 1,216,930 checks, including 127 Cut0 lighting records, 20 original startup grids, zero environment warnings, actual r10a/r10b authored-water-start rendering, animation, pause and fresh-context equality. The four existing unsupported room model formats are r313/r314/r322/r323; the lighting claim is Cut0 data coverage, not full gameplay camera-cut/shadow parity. The unchanged original wave routines match 3,687,300 float comparisons with maximum error 0; original CommonMove/ColorUpdate/AnmMove match 399,768 checks with maximum error 0.

The original tiny dry-terrain gaps can still be visible without water. This patch deliberately retains those original coordinates. It does not claim full console raster sample positions, RGBA6 EFB precision, interlacing, VI scanout, or the default 64 display filter. GLES linear resolve can differ by one byte from a hardware EFB copy. The M34 wave simulation, authored grid activation, normals, mesh topology, and snapshots are unchanged by this GPU correction; separate M36 event-parent support extends the CPU sprite state independently.
