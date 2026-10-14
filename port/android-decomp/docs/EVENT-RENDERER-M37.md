# M37: Source event presentation and special effect passes

## First-house handgun

`wep0200` in the supplied `r100s03.evd` is the normal handgun from
`em/pl00/wep02/wep0200.bin`, attached to `pl0000` part10. Its supplied FCV is
intentionally empty. It therefore retains its static BIN pose and follows the
authored parent matrix. It is not a knife or a fallback actor.

`cModel::isTrans()` tests `be_flag & 2`: that bit enables rendering.
`Evt_R100S03_Func` enables the handgun on cuts0–4 and13–20, and clears that bit
on cuts5–12. The previous M36 presentation interpreted the bit in reverse.
`Obj18CmfOn(mod,5)` sets common flag0x04000000, which exempts this model from
`ControlTransFlag` motion/parent display propagation. The explicit override
therefore applies even though the handgun has an empty FCV.

The attached local transform uses the original `obj18SetOya` concatenation and
native column normalization. It retains the translated parent pose while
removing inherited basis scale. Native part scale and rotation setters refresh
the real motion64 hierarchy without advancing motion or Hokan.

## Opening model and sprite effects

The actual `r120s00`/`r120s01` EFF banks contain94/192 EFM records. ID0xFC
resolves the first SMD placement whose `work` equals `Tex_id`, as
`SmdGetGroupObjPtr` does. Each distinct source group is decoded and uploaded
once. Source `cObj04` positions, native rotation, 0.005 model scale, RGBA and
light mask0x10 place every live instance; these models never become textured
billboard substitutes.

`cObj04::move` selects the original opaque/translucent OT at `Col_a<250`.
The opaque model pass precedes actors; translucent model instances follow
actors in camera depth order. The immutable group mesh remains shared by all
records that use it.

The event billboard shader supports original Esp08 repeat/scroll UVs,
screen-part512×448 coordinates, alpha compare0x80 for flag0x100000 and depth
writes for0x200000. Source flag0x400000 uses a separate pass before opaque
world/model geometry. All these source coordinates stretch over the physical
Android viewport; the fixed console letterbox is omitted.

The original Esp18 bank uses one EFB copy and eight layers. Each layer samples
the immutable captured frame RGB, the actual ANM alpha, the source circular
offsets and source alpha envelope. Its configured indirect stage does not
issue an indirect TEV command, so the ANM RGB does not provide a displacement
map. The capture reuses the native GX copy filter and half-resolution resolve
already ported for water, including in rooms without water. FBO, viewport,
texture, sampler and render state are restored. The supplied opening record
has `blur_rate==0`; the world projective branch retains its exact source
offset behavior.

Source fades draw an alpha0–255 black overlay across the complete physical
frame after scene/effects/subtitles. Scissor, stencil, partial viewport and
channel masks cannot leave an uncovered edge; all caller state is restored.

The rear-view mirror uses the separate `event_mirror_gl` source pass from
`EvtTexRenderCamTrans`, `TexRender.cpp` and `mirror.cpp`. The clone models are
excluded from the ordinary main actor pass while that pass is active. Its
source target and projective surface implementation are documented alongside
that module's dedicated actual-data regression.

## Trap line and lens effects

Esp09 consumes its original ring-buffer points, source sixth-pixel width and
per-point alpha. Portable triangles reproduce that width in the source
512×448 coordinate system on GLES drivers whose native line width is limited
to one pixel. This raster adaptation preserves source point order, width,
depth test, blend and color; it does not claim bit-identical GX edge coverage.

Esp0e projects the original attached world position, ANM pivot, rotation,
size and source center/distance alpha. Its visibility pass copies the actual
completed depth buffer, evaluates the original twelve-point ring against
previous screen position/depth+150, quantizes comparison to the original
24-bit depth and applies `hide_alpha += (target-hide_alpha)*0.6`. Rendering the
same30Hz source age twice does not advance that feedback. The source visibility
history survives EGL name loss; new room ownership clears it. The fixed
console56-row letterbox margin is omitted for the full Android viewport.

## Source alpha glow and masked room effects

Filter03 consumes the real source effect alpha channel. The room frame clears
alpha to0; ordinary geometry preserves it. Common effects enable alpha writes
for Tool0x8000, or Tool0x800000 while the original AlphaDraw2 status is set.
Tool0x200000 keeps its depth write and destination-alpha override0 without
changing the alpha used for RGB blending or alpha testing.

Glow request order, locked priority255 and the persistent AlphaDraw2 status
follow `Filter03SetParam`. That function changes `loop=level+2` while retaining
the room-initialized level1: each loop therefore uses the original two
horizontal offsets -0.008/+0.008. The GL pass copies alpha through the source
256×224,128×112 and64×56 A8 stages, performs the two initial scale2 passes and
the original alpha128 feedback draws, then adds the requested color over the
complete physical viewport. The immutable capture also resolves multisampled
Android framebuffers. A8 quantization, filtering, blend and render-state
ownership are explicit; this is an alpha mask filter, not a synthetic bloom
texture or bright-color threshold.
GLES uses an8-bit framebuffer rather than the console's RGBA6 EFB storage;
its texture interpolation/blend rounding and MSAA coverage remain GPU
adaptations. The source pass sequence and A8 intermediate precision are
preserved, but bit-identical console raster output is not asserted.

Masked room layers have a separate GPU owner and source area/display state.
The mask's independent ANM frame preserves stage0 RGB and multiplies only
alpha. Actual Esp08 Mask_type1 spans the complete sprite independently of
primary texture repeats, scroll and flips. Common Esp00 keeps the source
extra mask-stage scale4 for Tool0x20000; Esp08 does not apply that extra scale.
Source alpha cutoff0x80, depth writes and before-world ordering remain active.
The actual SST4 Esp08 shimmer resolves an immutable EFB RGB copy through the
original indirect ANM A/B components, signed S0.10 matrix truncation and
exponent1 coordinate shift. Its unsigned type1 offsets retain source power255
and the independent stretched mask; ANM RGB is not drawn over the room.
This pass explicitly writes the source indirect texture command, unlike the
Esp18 eight-layer effect whose source never issues that command.

Original room MDT text uses the actual font and glyph queue, including English
room messages. `Message::WidthCk` measures the entire current page before
incremental reveal; default alignment centers the widest line and keeps the
text stationary as each glyph appears. The full original alignment attributes,
font dimensions, spacing and Japanese one-line offset drive the source layout.

## Regression scope

`test_m36_event_playback` verifies every actual first-house frame against the
source visibility table and normalized parent basis, without substituting
motions. English `Event::MesSet` omits the MDT lookup entirely; the separate
actual Japanese GLES timeline verifies the original glyph resources.

`test_event_actor_gl` renders all original s03/s20 cuts, verifies independent
CPU/GPU skinning, visible handgun pixels, shape uploads, subtitles, pause and
actual EGL recreation. Its special-pass fixtures prove Esp18 ANM-RGB
independence, eight immutable feedback layers, source alpha cutoff and before-
world routing, full-frame fades, Esp09 width/alpha and Esp0e depth occlusion
with source0.6 feedback. Opening checks prepare the real r120 SMD group banks
and draw every original source cut with EFM and sprite effects. Host EGL
framebuffers are verification artifacts, not Android device screenshots.
Additional fixtures verify Filter03 priority, alpha group selection, constant
alpha-field feedback and independent mask coordinates/destination alpha.
The shimmer fixture verifies A/B component selection, R/G independence and
the source power255 matrix offset against an analytically positioned EFB sample.

Primary source files: `model.cpp`, `obj18.cpp`, `r100.cpp`, `r120.cpp`,
`esp_efm.cpp`, `obj04.cpp`, `esp.cpp`, `esp_sub.cpp`, `esp08.cpp`, `esp09.cpp`,
`esp0e.cpp`, `esp18.cpp`, `filter03.cpp`, `mes.cpp`, `TexRender.cpp`, `mirror.cpp`
`GXBump.c` and `mtx44.c`.

## Native r100 radio screen

Event GPU teardown releases its own actor and EFM mesh entries. It preserves
the independent caches for persistent room actors, forest actors and radio
models; actual-asset framebuffer checks exercise this ownership boundary.

`radio_term` loads the original terminal ID pack, both `ss_oc101/102` partner
archives and the `op01.das` archive at its authored0x400 offset. Leon has the
five `tel00ModelInit` model layers; Hunnigan retains all six source body/face
layers. Original idle/talking FCVs advance through Motion64, with10-frame
blends when the stream becomes ready or B enters manual mode. The fourth
model-info face on each actor receives the original shape data; Hunnigan's
shape flag0x8 multiplies the mouth weight by1.37 in the original operation
order. The scene uses the source camera(0,0,2000),50-degree perspective and
both source character placements, plus the three shared screen lights and
CORE cut9 ambient channels.

The terminal keeps the source30-tick delay, asynchronous stream-ready wait,
DSP block1 calls0x8c/0x8d, automatic OpeSeq frames and manual A/B stepping.
START closes the call after the initial wait, including while a requested
stream is not ready. Call0 has10 entries and its final sentinel at810;
call1 has7 entries and its final sentinel at430. The MDT words retain the
source speaker18 command, instant glyph reveal, timed waits, page changes,
line/color commands and WaitEnd behavior. English codec captions remain
visible. Entry fades black-to-clear over5 source ticks; the r100 owner handles
its separate3/10-tick system/scenario black-to-clear fades after closing.

The original ID units keep their independent four Hermite timers, reverse,
loop, two-control-point authored paths, parent matrices and byte-truncated
parent color multiplication. Source mark IDs12/13/14 control complete widget
groups, rather than being confused with their unit numbers. Shared/CORE/term
texture ownership and native texture pattern clocks stay separate from the
30Hz call sequence. Art, models and captions stretch to the physical display.
Source textured/negative/indirect UI passes retain immutable frame copies,
blend tables and independent masks; negative TEV output is `RASC*(1-CPREV)`.
Both source English language values select `Eng` art and MDT language1.
Other source language paths remain exact, but their language archives are
absent from the supplied US disc and have not been tested. One limitation
remains explicit: the
recovered `IdShimmerTrans` source reads an uninitialized local vector for its
indirect depth divisor. This backend uses its defined1500 lower bound;
identical strength to the original binary cannot be asserted until that
instruction/data path is recovered. GX/GLES framebuffer precision and edge
coverage differences also remain the documented platform adaptations.

The `m37_radio_gl_check` target uses the actual source assets and DSP streams.
Its checks cover both call tables, all11 models, visible original art/morphs,
subtitles, stream-ready delay, manual stepping, START, pause, aspect changes,
entry fade and GPU recreation without resetting the CPU call position.

The final frozen native build passed1,370 radio checks and1,525,061 event
GLES checks against the supplied disc assets. Both radio calls end at their
source810/430 sentinels and display6/4 MDT pages; both source English enum
values produce identical art, text and pixels. The event proof includes the
persistent actor teardown regression, all21/5 first-house/post-house cuts,
all9/15 opening cuts, original model effects and the specialized effect
passes described above. These host GLES results do not replace physical
Android device validation or remove the documented GX/GLES precision and
recovered-source shimmer limitations.
