# Original startup and title adapter, M35

M36 corrects the device startup presentation defects on top of this adapter.

The Android entry now runs the supplied disc's authored Nintendo screen, warning,
CAPCOM, Sofdec and Dolby screens, followed by the original three-entry title menu.
`title.dat` supplies the artwork and ID layouts. `StartupFlow` advances the source
30 Hz timings, cancel thresholds, 15-frame logo fades, 90-frame new-game fade and
600-frame alternating attract demos. The fresh title starts its cursor on LOAD;
it does not manufacture unlocked modes or difficulty choices.

`TitleData` retains original Hermite keys and path points. A title colour curve
starts at frame **−15**; this is legitimate source animation data. The adapter's
bounds/finite/monotonic validator therefore replaces the static subscreen curve
validator for these layouts. Drawing follows the original hierarchy-aware OT
order: reversed roots, then reversed direct children recursively. Texture frames
advance at 30 Hz according to the authored texture flags independently of a
skipped logo's curve time.

## Resources and controls

| Screen | Original resources | Behaviour |
|---|---|---|
| Nintendo | `ss/Eng/title.dat`, entry 11 | Authored image, source fade/wait |
| Intro logos/title | Entries 4 and 6 | Authored textures, paths and colour/scale/rotation keys |
| Title menu | Entry 8 | START, LOAD, OPTIONS; clamped up/down; A selects |
| Options | `ss/Eng/option.dat`, texture entry 8, layouts 9/10/12/13/14; CORE cockpit textures | Original controller, brightness, audio and exit labels/widgets |
| Option help | `etc/core.das` body entry 0x0A; `font/common_p.fnt` | Original glyphs, source line breaks, widest-line centering and caption preset |
| Empty Load | `ss/Cmn/save_e.dat`, entries 4/6/7/8 | Seven visible blank metal rows across 20 wrapping slots; A on empty does nothing |
| Load exit question | `etc/memcard.das` body entry 5, message 17 | Original glyph question/Yes/No; initial choice No |

Controller settings stage left/right changes and commit with A, as the source
does. Preferences use `game-options`: `invertAimY`, `vibration`, `knifeMode`,
`brightness` (34–114, default 64), and `soundMode` (0 mono, 1 stereo, 2 DPLII;
fresh Android default stereo). Brightness changes immediately. Native PCM output
implements mono downmix and stereo. The authored DPLII choice persists, but a
Dolby Pro Logic II encoder is not implemented.

The title's **640×448** source setting is the EFB/viewport resolution, not the
authored ID coordinate range. `IDSystem::move` places its screen plane at
`−240/tan(Fovy/2)` and the camera's 4:3 perspective projects coordinates
`x=±320`, `y=±240` to its edges. The orthographic globals use the same bounds.
`IdCommonTrans` restores that camera projection. The adapter therefore correctly
uses a logical **640×480** canvas; source rasterization maps its normalized result
to the 448-pixel EFB height.

The supplied `opening.sfd`, `demo0.sfd` and `demo1.sfd` are played from derived
H.264/AAC MP4 assets using Android's MediaPlayer, rather than a replacement image
or invented movie. The movie preparation script and manifest record source and
output hashes. Stereo and mono audio tracks support the source output-mode
selection. This conversion changes the source MPEG-1/ADX encoding; AAC is lossy.
The movie output now uses a `TextureView` and a decoder-owned `MediaPlayer`
worker. There is no opaque black View background above a SurfaceView underlay.
Prepare, audio-track selection, seek, pause, position queries and release stay
off the UI thread. Each surface has a generation token; stale callbacks cannot
resume a replaced surface or closed movie. Logs distinguish decoder rendering
from an actual texture-update callback, and a missing first texture update fails
visibly instead of playing black output indefinitely.

Movie preparation and room preparation run on a worker. Asset copies use a
versioned cache name, size check, fsync and atomic rename. Lifecycle pause/resume
retains movie position and stops native title audio while the movie plays.

## Explicit remaining differences

This adapter does **not** establish complete original startup or campaign parity.

- The original new-game path plays the opening movie and then runs the r120
  police-car EVD scenes before jumping to r100. The Android adapter currently
  proceeds to the r100 campaign entry after the movie. Those car scenes and a
  general original event runtime remain unported.
- There is no original CardSave/CardLoad backend, CARD adapter, save block
  serialization/restoration, CRC verification or system-save unlock persistence.
  LOAD consequently exposes the actual original empty-list behaviour; it does
  not claim to restore saved games. No fake “no saves” message is drawn.
- Authored ID artwork, curves, paths and texture animations are retained, but
  M36 implements the authored negative and indirect shimmer shaders from
  `IdNegativeTrans` / `IdShimmerTrans` against the accumulated framebuffer,
  including separate GX blend factors0–4. The original title does contain
  shimmer transitions; drawing distortion texture8 itself as visible artwork
  was incorrect. The portable compositor uses the source half-resolution
  320×224 screen copy, authored quad UVs, GX A/B displacement channels,
  signed bias for type3, S0.10 coefficient truncation and clamp/bilinear copy
  sampling. The decompiled shimmer depth expression reads uninitialized matrix
  storage; its original intended value is unresolved, so this implementation
  uses the source minimum1500. GX copy/filter precision, projection and the full
  original rasterizer are not reproduced bit for bit. Title brightness still
  uses the brightness gain; room GLES applies the original seven-tap vertical
  display filter.
- Original list entrance/scroll animation timing and every card backdrop state
  are not emulated by the empty-list view. The list uses the authored final
  geometry and original digits.
- GameCube second-controller boot shortcuts, unlocked Ada/Mercenaries branches,
  original difficulty/system-save unlock gates and remaining title event tasks
  are not implemented through this fresh-title adapter.
- RUMBLE and QUICK KNIFE TYPE II choices and their original check/selection
  artwork persist in preferences. The actual controller rumble output and full
  original in-game Quick Knife Type II behaviour remain unported. Persistence
  and menu interaction do not establish runtime parity for these two settings.

## M36 presentation changes

The original ID coordinates remain640×480. At the user's explicit request,
Android now fills every display by cropping the authored active title/logo
picture(y60–420) and stretching each axis independently. Options use their
active background(y46–434); Load uses the full480-unit viewport. Touch mapping
uses the inverse of the same transform. Movies fill the parent TextureView with
an identity transform, removing the old VideoView aspect-ratio measurement.
This stretch deliberately changes the original4:3 display presentation.

Cursor movement resets only the selected highlight timers, as
`TITLE_MENU_MOVE` does. The root entry fade and even base labels retain their
clocks and visibility; selecting another row does not replay the whole menu
fade. Returning from a demo restores the settled title at frame615 with the
menu entry clock past its fade. Every rendered source frame starts with an
opaque black bitmap and reset paints. Movie removal resets the display paint,
colour filter, blend and cached picture before title drawing resumes.

Special-unit composition is bounded to640×480 and performed by an optimized,
lock-free native helper. Source pictures are reused between display refreshes
at the30Hz title tick. This avoids the slower Java pixel loop; measured host
throughput is not an Android frame-rate guarantee.

## Validation

- Actual supplied title, option, save, CORE, memcard and font archives load through
  the strict native parser. Every retained decoded texture has valid dimensions
  and the exact expected RGBA buffer length; referenced layout textures exist.
- 18 pure-Java title/curve/path/draw-order contracts and 17 original option
  navigation/commit/bounds contracts pass with JDK17. M36 additionally passes
  86 presentation, independent menu timer, viewport/touch, movie-generation,
  GX colour/indirect/blend and sampling contracts, plus27 native compositor
  coverage/mask/blend/validation contracts.
- Every Android Java source compiles against Android API 35.
- Ten offline frames from the decoded original assets were rendered and visually
  inspected: Nintendo, warning, CAPCOM, Sofdec, Dolby, title, and four option
  screens. They demonstrate the actual source artwork/layout and identified two
  fixed defects (flat OT order and wrong brightness digit texture). They are
  **offline asset renders**, not Android/device screenshots or full fidelity
  certification. Device movie playback, lifecycle and controller validation
  remain necessary.

The supplied M36 device log contains four `MEDIA_INFO_VIDEO_RENDERING_START`
events, including the opening, with no accompanying fatal decoder error. This
supports a composition diagnosis; it does not establish that visible pixels
were displayed. The M36 fix has been compiled and checked on the host; no device
is attached here. Actual logo transitions, both attract demos, opening,
mono/stereo playback, film skip/return brightness and pause/resume still require
validation on the user's device. Existing M35 offline frames do not validate
M36 Android composition.
