# M36 original r100 event runtime

`OriginalEvent` executes the provided Disc1 `Evd/r100s03.evd` and
`Evd/r100s20.evd`. It does not replace those sequences with a video. The room
loader prepares their original BIN/TPL bodies, SetParts attachments, FCV
motions, morph deltas, camera curves, LIT files, MDT words, EFF records and DSP
stream audio before the event can start. A tick is one original 30 Hz frame;
pause never samples or advances a clock and a tick performs no file reads.

| Actual resource | r100s03 | r100s20 |
| --- | ---: | ---: |
| EVD bytes | 1,341,504 | 689,632 |
| Named assets | 125 | 29 |
| Packets | 128 | 34 |
| Camera cuts | 21 | 5 |
| Source frames | 1,465 | 570 |
| Bodies and attached parts | 16 | 8 |
| Nonempty EST banks | 5 | 3 |

## Checked data and source behavior

The 80-byte header, 64-byte asset directory, signed packet sizes and BE
offsets stay immutable. The reader checks directory/packet overlap, alignment,
budgets, names, asset extents, chronological packet order, EndPac, all camera
cuts and every executable packet reference during construction. Missing
models, motions, morph entries, light files, message tables, glyph resources,
stream entries or unsupported executable commands fail on the loader worker.
No PPC address is interpreted as a native pointer.

Camera and morph channels use the original `motion64::HermiteInterpolation`
after checked endian conversion. Bodies use the existing original FCV motion
runtime and actual weighted model corners. SetParts borrows the owner's
skeleton, including the original head model's rest-offset substitution.
Shape movement applies the original signed integer deltas to original BIN
positions; uploads occur only when those positions change. The empty handgun
FCV keeps its source rest skin: its original root centre is exactly zero,
rest translation cancels inverse bind, and the authored parts-10 attachment
places the unchanged weapon vertices. M37 corrects the earlier interpretation
of the r100s03 callback: `cModel::isTrans()` returns true for `be_flag & 2`, so
`Evt_R100S03_Func` draws the handgun on cuts 0–4 and 13–20 and hides it on
cuts 5–12. `Obj18CmfOn(mod,5)` makes this explicit display state independent
of ControlTransFlag's motion state and of the attached parent's display.

Every original LIT packet installs its supplied light environment without
substituting a test-map profile. Actual parented lights in these files are
all ParentType1/ParentNo0 and refer to the frozen gameplay Leon, rather than
the independent SetOm `pl0000` event body. Original SetObj18 lighting masks
are Leon/weapon1, Ganado2 and OBM/truck4; model light centres and radii come
from original BIN bounds and rootparts, with attached models inheriting the
owner's light information.

`finish()` processes the remaining actual packets to their final authored
poses. The natural and skipped r100s03 player root and heading match. The
observable `endPlayerPlacement()` is the source root/heading before the SAT
floor ray; r100s03's scenario then sets its explicit post-event placement.
r100s20 has source PlPosNoSet and returns no replacement player placement.
The bridge owns the scenario flags, post-event floor/placement, fade,
ambush enemies, stream mixing and input restoration.

## Original EST effects

EventCutEstSet converts decimal cut indices to BCD identifiers. A new camera
cut deletes the prior event cut's sprites/controllers and starts its authored
EST. Parent_no addresses only SetOm registration order. Effect physics stays
in parts-local coordinates while attached; CommonMove applies the current
original parts matrix when Release_time reaches its authored age. The draw
snapshot converts attached positions to world coordinates without modifying
their local physics or advancing their clock.

All EST records present in these two files execute: source Esp00 dust/smoke,
Esp01 strips, Esp12 five-point world-position history ribbon, generator00
burst/interval/lifetime rules, source generator02 path emissions and the
generatorFF five-frame prerun marker. The s20 generator02 uses original CORE
PATH7's 30 unweighted vertices, cumulative lengths, random ratio0–100,
authored scale(47,1,51) and yaw186/256 turns. Path selection consumes its seed
before the sprite's original random parameters, as in espgen02_Update.
Textures/ANM are the actual CORE records30/31/82/226. Actual blends are0/1;
none of these event quads tile. The Esp12 record has Shimmer_type1, but its
dedicated original renderer binds its ordinary texture and CommonStateSet;
it does not call EspCommonTransShimmer. Its history strip and UV staggering
are retained instead of substituting a framebuffer ripple.

These are the two integrated r100 event profiles. Other EVD names are rejected;
SetPl, explicit Esp/Func/Fade/Fog/Focus commands absent from these profiles are
not claimed as a complete general event engine. No unknown executable command
is silently ignored. Original handlers that are themselves unused remain
explicit no-ops.

## Audio and subtitle resources

SHD `bgm/bio4str.hed` selects real DSP entries: block1 uses `bio4evt.sbb` and
block0 uses `bio4bgm.sbb`. s03 begins voice1/2 and starts BGM0/0 at cut14;
s20 begins voice1/9. Streams are decoded on the loader worker. Packet Se uses
the source `SndCall(u16 val.no,u16 val.arg)` block/number order.

The supplied source suppresses event captions in language1, so normal English
playback retains MDT metadata/timers without invented visible text. Japanese
language0 uses the original event1_j font with 28-pixel cells. The supplied
NTSC MDT blocks2–5 contain identical non-Western placeholder glyph data that
does not fit their supplied Western font; construction explicitly rejects
that unsupported caption resource combination. They are not German subtitles.

## Verification

`tools/test_m36_event_playback.cpp` plays all 2,035 source frames from both
actual EVDs. Its independent raw BE first-key oracle checks eye, target, roll
and FOV on all26 camera cuts; it checks every actual actor pose, facial mesh
revision, stream, subtitle timer, effect texture/quad, ribbon/path emission,
cut effect cleanup, source model light masks/volumes and pause clock. Natural
versus skipped player pose, mid-cut cancellation, malformed packet/directory
boundaries and unsupported caption-resource rejection are checked. The standalone Werror run passed
24,122,025 checks: s03 had219 subtitle timer frames,722 facial revisions and
124 visible EST frames; s20 had182,236 and377 respectively.

The production GLES skinning, lighting, caption, dynamic upload, effect and
context restoration checks are documented separately in
`EVENT-RENDERER-M36.md`. Host verification establishes these authored resource
paths; Android device behavior still requires the release APK/device run.
