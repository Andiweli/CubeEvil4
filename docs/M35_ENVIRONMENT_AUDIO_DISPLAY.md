# M35 environment audio and display brightness

## Foot, running and landing sounds

The bridge consumes actual original motion SEQ transitions emitted by LeonPreview. There is no walking timer or inferred landing frame. `SourceFootSoundCue(Se, Free)` follows `upstream/re4/src/game/pl_class.cpp::cPlayer::seqSeCtrl` and preserves the clothing cue before the main positional cue.

| Source cue number (`Se−1`) | Main block | World part | Optional clothing cue when `Free&7 == 0` |
| --- | --- | --- | --- |
| 0, 2, 13 | 5 | 0x15 | block1/0x2a for0; block1/0x2f for2/13 |
| 1, 3, 14, 20 | 5 | 0x19 | block1/0x2a for1; block1/0x2f for3/14/20 |
| 4, 5 | 5 | root0 | none |
| Other numbers | 1 | root0 | none |

`Free&7` values1/2/3 override the main block to1/6/2. Values0/4/5/6/7 preserve the main block. Clothing cues always use the source root position.

`FootstepSounds` reads the room's original FSE0x103 records with the original132-byte stride. It retains source record order, activation, group, quad/cylinder boundaries and the original `point.y + 300` area test. As in `FlrAtInit`, an absent, old or zero-filled FSE block leaves the default sound variant0. Fourteen original rooms contain zero placeholders.

The block5 dispatch matches `snd.cpp::footSeCheck`:

- Numbers16..19 query floor areas with `use_kind&2`; they do not take the player water branch.
- Other numbers test water first, then floor areas with `use_kind&1`.
- A floor area's sound variant adds `se_type*30`; ordinary foot effects are selected only for base numbers0..3.
- Water adds30 only under the source's strict condition `point.y < waterHeight - 90.0f`. Walk/run sounds0..3 become30..33; knee/body landing sounds4/5 become34/35.
- Original cue randomization and existence checks remain in SoundBank/SoundMixer.

`QueryEnvironmentWaterContact` follows `Espgen42.cpp::GetWaterHeight`: generator42 first, bounded45 maximum height, then an unbounded45 plane override. It uses the authored transformed plane and inclusive local grid bounds; it does not sample animated wave height. Generator42's collision inverse is captured before its height rate, generator45's after. The room Init `NoWater` overrides for102/112/222/303/31a/31b disable physical contact while keeping decorative water visible.

The real r10a test searches the original SAT floor rather than selecting an arbitrary point under a visual plane. It finds23 wet floor centroids, of which21 permit the original scenery-sphere WalkProbe placement. One verified contact is polygon1 at `(66666.7,−10000,110833)`, water height`−9200.08`, immersion`799.921`. Original r10a.das block5 cues30..35 all exist and produce audible PCM through the production mixer. Points in the air or at the exact90-unit boundary stay dry.

The original r10a water-entry effect EST0x24 contains six Esp40 graphics records and no sound record. Esp40 has no direct sound call. An extra sound at a guessed water crossing would therefore double or invent an event. Audible landing uses the original landing SEQ and block5 surface selection.

Existing draw, knife and reload direct sound calls remain necessary. Handgun draw uses direct1/0x28 at frame2 and its original SEQ independently requests2/9 at frame4. Shotgun draw SEQ21 is silent; source ready10 directly requests1/0x28 and2/9. Knife slash frame3 is a direct1/3 call and its current original slash FCVs omit SEQ. Reload and weapon pump sounds also remain direct calls.

This change does not port `AddWaterPower` foot disturbances, the full EST0x24 splash graphics, dynamic FSE activation/group changes, or future scripted water enable/disable changes. It makes the source-selected audio audible at actual SEQ transitions using the currently active authored room environment.

## Display brightness

`DisplayBrightnessFilter` implements `main_sub.cpp::Bg_brightness_set` for the source UI range34..114, default64. Base coefficients are`{8,8,10,12,10,8,8}`. The source scales each by`brightness/64`, truncates to a byte, then distributes the remaining integer units by looking up the priority in`order={5,1,3,0,4,2,6}`. The resulting increment order is tap indices`3,1,5,2,4,0,6`.

`ApplyDisplayBrightnessGl(width,height,value)` copies the completed framebuffer and applies the seven coefficients to their three GX rows: coefficients0/1 above,2/3/4 current,5/6 below. M36 corrected the earlier seven-pixel approximation. The default coefficient row sums are16/32/16; RGB uses the hardware integer right shift by6 and color clamp, with unchanged alpha. The default64 skips the post pass to preserve the existing default framebuffer exactly. GL program, VAO, texture/sampler bindings, read framebuffer, viewport, color mask and modified render enables are restored. `ForgetDisplayBrightnessGl` abandons lost-context names; resources rebuild lazily. `DestroyDisplayBrightnessGl` deletes them with a current context.

The coefficients match original source exactly. The three-row mapping and integer truncation are corroborated by Dolphin's hardware-tested EFB copy implementation (`TextureCacheBase::GetRAMCopyFilterCoefficients`, `TextureConversionShader.cpp::WriteSampleFunction`). The GLES post pass is not a complete emulation of console scanout, EFB channel precision, sample positions, interlacing or the base default64 filter.

## Verification

- `tools/test_m35_water_sounds.cpp`:194 checks, all127 original room FSE blocks/1927 authored records, physical r10a wet SAT contacts, strict immersion/airborne conditions, and six original wet cues rendered to nonzero PCM.
- `tools/check_foot_sound_reference.py`:1,286,419 comparisons against unchanged extracted original `seqSeCtrl`, area tests, `FlrAtCheck`, `EspPlWaterCall` and `footSeCheck`; all65,536 Se/Free byte combinations and all127 room floor-boundary samples. Device, random selection and particle side effects are isolated stubs; production mixer tests cover original cue playback separately.
- `tools/check_brightness_reference.py`:567 coefficients match unchanged `Bg_brightness_set` across every supported value34..114.
- `tools/test_m35_display_brightness.cpp`:69,382 checks; every supported nondefault value, framebuffer taps/clamp/alpha, default pixel/state preservation, altered GLES state restoration, destroy/recreate and fresh EGL context pixel equality.
- All new production C++ files compiled under host`-Wall -Wextra -Werror -ffp-contract=off`.

The M34 grid simulation and render path are unchanged by M35; water contact is an additional read-only query.
