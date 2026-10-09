# In-game graphics settings: binding rendering-phase requirements

This file is architecture and acceptance criteria only. Phase 1 implements no
graphics settings menu, persistence, GPU capabilities, MSAA, AF or depth-of-field
effect. A diagnostic app build must not imply those functions exist.

## User-visible settings

| Setting | Required choices | Meaning |
| --- | --- | --- |
| MSAA | Off / 2x / 4x | Multisampling for the active scene attachments |
| Anisotropic filtering | 2x / 4x / 8x | Requested filtering level on eligible texture samplers |
| Depth of field | On / Off | Focus nearby or aimed-at objects, with appropriate background blur |

All settings must be available during play and persist across app restarts.
Changes must not alter original AI, event timing, collision or input decisions.
Vulkan may be primary, but UI, storage and validation policy must be independent
of the renderer API and work with another backend.

## Separate requested, supported and active state

The eventual settings service owns user requests and persistence. A renderer
reports capabilities for the actual device, formats and current render path.
It validates a request, applies it transactionally, then returns the effective
state or an error. Only acknowledged effective state is displayed as active.
Names in this paragraph describe planned components, not implemented classes.

Changing saved values is not evidence that the GPU used them. If apply fails,
retain the previously active state and explain the failure. Unsupported choices
are disabled or clearly unavailable. Do not silently label a fallback as the
requested value. Pending changes are visibly pending until applied.

Revalidate at startup, device/backend change, Surface/render-target recreation
and restored settings. Never trust cached capabilities or a previous session's
effective state on a different device. Preserve the user's saved request so it
can be reconsidered when hardware supports it.

## Hardware validation

- **MSAA:** validate the requested count against the intersection of supported
  color and depth attachment sample counts for the actual formats and render
  path, including resolve, EFB-copy and postprocess requirements. Global support
  for 4x alone is insufficient. Off is internally one sample. Fall back to a
  supported count with the actual count shown; if none above one is usable,
  display Off. Recreate attachments safely before acknowledging activation.
- **AF:** query anisotropy support and its maximum. Enable only supported 2x,
  4x and 8x choices, never round above the hardware limit. If support is below
  2x, show the setting unavailable; internal 1x fallback must not appear as 2x.
  Apply to appropriate samplers while respecting original special-purpose
  nearest/filter states used by effects. Revalidate sampler creation results.
- **DoF:** validate the depth sampling/resolve and postprocess path needed for
  that backend. Unsupported depth input means DoF is unavailable, not On with
  no effect. Focus input must use verified camera/aim/world-position data from
  the original engine. Prefer a valid aimed-at object; otherwise use a nearby
  eligible focus subject. Define distances in the original world units after
  auditing the source. Do not invent target APIs or use gameplay mutations to
  drive a visual effect. Add temporal smoothing; preserve HUD readability and
  define behavior for cutscenes, scopes and menus.

## Persistence and tests

Store all requests using a versioned, backend-neutral schema in Android private
storage with atomic replacement. Do not serialize native C++ object memory or
ABI-sized enums/pointers. Defaults must be explicit: MSAA Off, DoF Off; request
AF 2x subject to validation. Persisted requests and last-applied values must be
distinguishable, and stored active values must be revalidated before display.

Rendering-phase acceptance must include mocked capability-policy tests plus
real device validation: unsupported options are never shown active; settings
survive restart; apply failure preserves the previous active configuration;
Surface recreation reapplies valid settings; ARM32 and ARM64 read identical
settings; requested sample counts/samplers are verified in the actual backend;
DoF visibly focuses a nearby/aimed subject and respects its Off state.
